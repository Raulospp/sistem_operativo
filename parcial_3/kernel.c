/* kernel.c
 * Mini-Sistema Operativo - nucleo coordinador.
 * Parcial 3 - Sistemas Operativos USC 2026A
 *
 * Modos de operacion:
 *   ./kernel              -> menu interactivo por stdin (terminal)
 *   ./kernel --remote     -> polling de comandos.txt (controlado por dashboard)
 *
 * Compilacion:
 *   gcc -std=c99 -Wall -o kernel kernel.c memory.c processes.c files.c perifericos.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tipos.h"

#ifdef _WIN32
  #include <windows.h>
  #define sleep_ms(x) Sleep(x)
#else
  #include <unistd.h>
  #define sleep_ms(x) usleep((x) * 1000)
#endif

#define CMD_FILE "comandos.txt"

/* === Inicializacion y finalizacion === */
static void inicializar_sistema(void) {
    init_memoria();
    init_procesos();
    init_filesystem();
    init_perifericos();
    printf("\n========= Mini-SO inicializado =========\n");
    printf("  Memoria total:    %d B\n", MEM_TOTAL);
    printf("  Max procesos:     %d\n", MAX_PROCESOS);
    printf("  Max archivos:     %d\n", MAX_FILES);
    printf("========================================\n");
}

static void finalizar_sistema(void) {
    int i;
    printf("\n========= Finalizando sistema =========\n");
    impresora_flush();
    for (i = 0; i < MAX_PROCESOS; i++) {
        if (procesos[i].estado != LIBRE)
            terminar_proceso(procesos[i].pid);
    }
    printf("\n[Resumen final]\n");
    printf("  Memoria libre: %d / %d B\n", memoria_libre(), MEM_TOTAL);
    printf("  Archivos en uso: %d\n", contar_archivos_usados());
    dump_estado_json();
    printf("Sistema apagado limpiamente.\n");
}

/* === Estado global y dashboard === */
void mostrar_estado_global(void) {
    printf("\n=== Estado global del sistema ===\n");
    printf("  Procesos:  LISTO=%d  EJEC=%d  BLOQ=%d  TERM=%d\n",
           contar_procesos_estado(LISTO),
           contar_procesos_estado(EJECUCION),
           contar_procesos_estado(BLOQUEADO),
           contar_procesos_estado(TERMINADO));
    printf("  Memoria:   usada=%d B   libre=%d B   (total %d B)\n",
           memoria_usada(), memoria_libre(), MEM_TOTAL);
    printf("  Archivos:  en_uso=%d / %d\n",
           contar_archivos_usados(), MAX_FILES);
    printf("  Impresora: %d trabajos pendientes\n\n",
           impresora_pendientes());
}

void dump_estado_json(void) {
    FILE *f;
    int i, k, primero;
    f = fopen("estado.json", "w");
    if (!f) return;

    fprintf(f, "{\n");
    fprintf(f, "  \"mem_total\": %d,\n", MEM_TOTAL);
    fprintf(f, "  \"mem_libre\": %d,\n", memoria_libre());
    fprintf(f, "  \"mem_usada\": %d,\n", memoria_usada());

    fprintf(f, "  \"bloques\": [\n");
    for (i = 0; i < bloque_count; i++) {
        fprintf(f,
            "    {\"i\":%d,\"start\":%d,\"size\":%d,\"libre\":%d,\"pid\":%d}%s\n",
            i, bloques[i].start, bloques[i].size,
            bloques[i].libre, bloques[i].pid_dueno,
            (i + 1 < bloque_count) ? "," : "");
    }
    fprintf(f, "  ],\n");

    fprintf(f, "  \"procesos\": [\n");
    primero = 1;
    for (i = 0; i < MAX_PROCESOS; i++) {
        if (procesos[i].estado == LIBRE) continue;
        if (!primero) fprintf(f, ",\n");
        fprintf(f,
            "    {\"pid\":%d,\"nombre\":\"%s\",\"estado\":\"%s\","
            "\"prioridad\":%d,\"memoria\":%d,\"espera_archivo\":%d}",
            procesos[i].pid, procesos[i].nombre,
            nombre_estado(procesos[i].estado),
            procesos[i].prioridad, procesos[i].memoria_asignada,
            procesos[i].archivo_esperado);
        primero = 0;
    }
    fprintf(f, "\n  ],\n");

    fprintf(f, "  \"archivos\": [\n");
    primero = 1;
    for (i = 0; i < MAX_FILES; i++) {
        if (!disco[i].usado) continue;
        if (!primero) fprintf(f, ",\n");
        fprintf(f,
            "    {\"i\":%d,\"nombre\":\"%s\",\"tam\":%d,\"dueno\":%d,\"cola\":[",
            i, disco[i].nombre, disco[i].tamanio, disco[i].pid_dueno);
        for (k = 0; k < disco[i].cola_count; k++)
            fprintf(f, "%d%s",
                    disco[i].cola_espera[k],
                    (k + 1 < disco[i].cola_count) ? "," : "");
        fprintf(f, "]}");
        primero = 0;
    }
    fprintf(f, "\n  ],\n");

    fprintf(f, "  \"impresora_pendientes\": %d\n", impresora_pendientes());
    fprintf(f, "}\n");
    fclose(f);
}

/* ============================================================
 *  MODO REMOTO: lee comandos desde comandos.txt
 *
 *  Formato por linea (tab separado, terminada en \n):
 *     VERBO\targ1\targ2\targ3
 *
 *  Catalogo de verbos:
 *     CREAR_PROC     nombre prioridad memoria
 *     TERMINAR_PROC  pid
 *     CREAR_ARCH     nombre
 *     ABRIR_ARCH     pid nombre
 *     LEER_ARCH      pid nombre
 *     ESCRIBIR_ARCH  pid nombre texto
 *     CERRAR_ARCH    pid nombre
 *     ELIMINAR_ARCH  pid nombre
 *     IMPRIMIR       pid texto
 *     FLUSH_IMP
 *     PLAN_FCFS
 *     PLAN_RR
 *     ESTADO
 *     APAGAR
 *
 *  Implementacion:
 *     - polling cada 200 ms sobre comandos.txt
 *     - offset por bytes -> no se reprocesan lineas viejas
 *     - el archivo se borra al arrancar el modo remoto
 *
 *  El puente HTTP (serve.py) escribe lineas a este archivo cuando
 *  el dashboard envia POST /cmd. Ver README.md seccion 8.
 * ============================================================ */

static int procesar_linea_remota(char *linea) {
    char *verb, *a1, *a2, *a3;
    /* strip CR/LF */
    size_t n = strlen(linea);
    while (n > 0 && (linea[n-1] == '\n' || linea[n-1] == '\r'))
        linea[--n] = '\0';
    if (n == 0) return 0;

    printf("[REMOTO] >> %s\n", linea);

    verb = strtok(linea, "\t");
    a1   = strtok(NULL,  "\t");
    a2   = strtok(NULL,  "\t");
    a3   = strtok(NULL,  "\t");

    if (!verb) return 0;

    if (strcmp(verb, "CREAR_PROC") == 0) {
        if (!a1 || !a2 || !a3) { printf("[ERR] args\n"); return 1; }
        crear_proceso(a1, atoi(a2), atoi(a3));
    }
    else if (strcmp(verb, "TERMINAR_PROC") == 0) {
        if (!a1) { printf("[ERR] args\n"); return 1; }
        terminar_proceso(atoi(a1));
    }
    else if (strcmp(verb, "CREAR_ARCH") == 0) {
        if (!a1) { printf("[ERR] args\n"); return 1; }
        crear_archivo(a1);
    }
    else if (strcmp(verb, "ABRIR_ARCH") == 0) {
        if (!a1 || !a2) { printf("[ERR] args\n"); return 1; }
        abrir_archivo(atoi(a1), a2);
    }
    else if (strcmp(verb, "LEER_ARCH") == 0) {
        if (!a1 || !a2) { printf("[ERR] args\n"); return 1; }
        leer_archivo(atoi(a1), a2);
    }
    else if (strcmp(verb, "ESCRIBIR_ARCH") == 0) {
        if (!a1 || !a2 || !a3) { printf("[ERR] args\n"); return 1; }
        escribir_archivo(atoi(a1), a2, a3);
    }
    else if (strcmp(verb, "CERRAR_ARCH") == 0) {
        if (!a1 || !a2) { printf("[ERR] args\n"); return 1; }
        cerrar_archivo(atoi(a1), a2);
    }
    else if (strcmp(verb, "ELIMINAR_ARCH") == 0) {
        if (!a1 || !a2) { printf("[ERR] args\n"); return 1; }
        eliminar_archivo(atoi(a1), a2);
    }
    else if (strcmp(verb, "IMPRIMIR") == 0) {
        if (!a1 || !a2) { printf("[ERR] args\n"); return 1; }
        impresora_imprimir(atoi(a1), a2);
    }
    else if (strcmp(verb, "FLUSH_IMP") == 0) {
        impresora_flush();
    }
    else if (strcmp(verb, "PLAN_FCFS") == 0) {
        planificar_fcfs();
    }
    else if (strcmp(verb, "PLAN_RR") == 0) {
        planificar_rr(QUANTUM_DEFAULT);
    }
    else if (strcmp(verb, "ESTADO") == 0) {
        mostrar_estado_global();
    }
    else if (strcmp(verb, "APAGAR") == 0) {
        return -1;
    }
    else {
        printf("[REMOTO] verbo desconocido: %s\n", verb);
        return 1;
    }
    return 1;
}

/* Lee comandos.txt usando offset por bytes (no reprocesa lineas viejas) */
static int leer_y_procesar_comandos(long *ultimo_offset) {
    FILE *f;
    long size;
    char linea[512];
    int cambios = 0, salir = 0;

    f = fopen(CMD_FILE, "rb");
    if (!f) return 0;

    fseek(f, 0, SEEK_END);
    size = ftell(f);
    if (size < *ultimo_offset) *ultimo_offset = 0;  /* archivo truncado/recreado */
    if (size <= *ultimo_offset) { fclose(f); return 0; }

    fseek(f, *ultimo_offset, SEEK_SET);
    while (fgets(linea, sizeof(linea), f)) {
        int r = procesar_linea_remota(linea);
        if (r != 0) cambios++;
        if (r < 0) { salir = 1; break; }
    }
    *ultimo_offset = ftell(f);
    fclose(f);

    return salir ? -1 : cambios;
}

static int correr_modo_remoto(void) {
    long ultimo_offset = 0;
    remove(CMD_FILE);  /* limpiar comandos viejos al arrancar */

    printf("\n[REMOTO] Escuchando %s. Use el dashboard en http://localhost:8000/dashboard.html\n",
           CMD_FILE);
    printf("[REMOTO] Ctrl+C en esta terminal para forzar salida.\n\n");

    while (1) {
        int r = leer_y_procesar_comandos(&ultimo_offset);
        if (r < 0) {
            printf("\n[REMOTO] Comando APAGAR recibido.\n");
            break;
        }
        if (r > 0) dump_estado_json();
        sleep_ms(200);
    }

    finalizar_sistema();
    return 0;
}

/* ============================================================
 *  MODO INTERACTIVO: menu por stdin (mismo de antes)
 * ============================================================ */

static void mostrar_menu(void) {
    printf("\n========== Mini-SO Menu ==========\n");
    printf("  1. Crear proceso          8. Escribir archivo\n");
    printf("  2. Terminar proceso       9. Cerrar archivo\n");
    printf("  3. Listar procesos       10. Eliminar archivo\n");
    printf("  4. Mostrar memoria       11. Listar archivos\n");
    printf("  5. Crear archivo         12. Imprimir (periferico)\n");
    printf("  6. Abrir archivo         13. Estado global\n");
    printf("  7. Leer archivo          14. Planificar (FCFS/RR)\n");
    printf("  0. Salir\n");
    printf("Seleccione: ");
}

static int leer_entero(const char *prompt) {
    char buf[64];
    if (prompt && *prompt) printf("%s", prompt);
    teclado_leer(buf, sizeof(buf));
    return atoi(buf);
}

static void leer_cadena(const char *prompt, char *buf, int max) {
    printf("%s", prompt);
    teclado_leer(buf, max);
}

static int correr_modo_interactivo(void) {
    int opcion;
    char nombre[64];
    char texto[256];

    dump_estado_json();

    do {
        mostrar_menu();
        opcion = leer_entero("");

        switch (opcion) {
            case 1: {
                int prio, mem;
                leer_cadena("  Nombre proceso: ", nombre, sizeof(nombre));
                prio = leer_entero("  Prioridad (1=alta): ");
                mem  = leer_entero("  Memoria (B): ");
                crear_proceso(nombre, prio, mem);
                break;
            }
            case 2: {
                int pid = leer_entero("  PID a terminar: ");
                terminar_proceso(pid);
                break;
            }
            case 3: mostrar_procesos(); break;
            case 4: mostrar_memoria();  break;
            case 5:
                leer_cadena("  Nombre archivo: ", nombre, sizeof(nombre));
                crear_archivo(nombre);
                break;
            case 6: {
                int pid = leer_entero("  PID: ");
                leer_cadena("  Nombre archivo: ", nombre, sizeof(nombre));
                abrir_archivo(pid, nombre);
                break;
            }
            case 7: {
                int pid = leer_entero("  PID: ");
                leer_cadena("  Nombre archivo: ", nombre, sizeof(nombre));
                leer_archivo(pid, nombre);
                break;
            }
            case 8: {
                int pid = leer_entero("  PID: ");
                leer_cadena("  Nombre archivo: ", nombre, sizeof(nombre));
                leer_cadena("  Texto: ", texto, sizeof(texto));
                escribir_archivo(pid, nombre, texto);
                break;
            }
            case 9: {
                int pid = leer_entero("  PID: ");
                leer_cadena("  Nombre archivo: ", nombre, sizeof(nombre));
                cerrar_archivo(pid, nombre);
                break;
            }
            case 10: {
                int pid = leer_entero("  PID: ");
                leer_cadena("  Nombre archivo: ", nombre, sizeof(nombre));
                eliminar_archivo(pid, nombre);
                break;
            }
            case 11: listar_archivos(); break;
            case 12: {
                int pid = leer_entero("  PID: ");
                leer_cadena("  Texto a imprimir: ", texto, sizeof(texto));
                impresora_imprimir(pid, texto);
                break;
            }
            case 13: mostrar_estado_global(); break;
            case 14: {
                int alg = leer_entero("  Algoritmo (0=FCFS, 1=RR): ");
                if (alg == 1) planificar_rr(QUANTUM_DEFAULT);
                else          planificar_fcfs();
                break;
            }
            case 0: break;
            default: printf("Opcion invalida\n");
        }

        dump_estado_json();
    } while (opcion != 0);

    finalizar_sistema();
    return 0;
}

int main(int argc, char **argv) {
    int remoto = 0, i;
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--remote") == 0 || strcmp(argv[i], "-r") == 0)
            remoto = 1;
    }
    inicializar_sistema();
    if (remoto) return correr_modo_remoto();
    return correr_modo_interactivo();
}
