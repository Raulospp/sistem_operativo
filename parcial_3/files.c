/* files.c
 * Sistema de archivos simulado en RAM.
 *
 * Caracteristicas:
 *   - Array global disco[MAX_FILES] (no toca el disco real del host).
 *   - Tabla de archivos abiertos por proceso via campo pid_dueno.
 *   - Bloqueo por archivo en uso: solo un proceso puede tener un archivo
 *     abierto a la vez para escritura. Los demas se encolan en cola_espera
 *     y pasan al estado BLOQUEADO. Al cerrar, el siguiente se entrega
 *     automaticamente y vuelve a LISTO.
 */
#include <stdio.h>
#include <string.h>
#include "tipos.h"

Archivo disco[MAX_FILES];

void init_filesystem(void) {
    int i;
    for (i = 0; i < MAX_FILES; i++) {
        disco[i].usado        = 0;
        disco[i].pid_dueno    = -1;
        disco[i].tamanio      = 0;
        disco[i].cola_count   = 0;
        disco[i].nombre[0]    = '\0';
        disco[i].contenido[0] = '\0';
    }
}

static int buscar_idx(const char *nombre) {
    int i;
    for (i = 0; i < MAX_FILES; i++)
        if (disco[i].usado && strcmp(disco[i].nombre, nombre) == 0) return i;
    return -1;
}

int crear_archivo(const char *nombre) {
    int i;
    if (buscar_idx(nombre) >= 0) {
        printf("[ERR] archivo '%s' ya existe\n", nombre);
        return -1;
    }
    for (i = 0; i < MAX_FILES; i++) {
        if (!disco[i].usado) {
            disco[i].usado      = 1;
            disco[i].pid_dueno  = -1;
            disco[i].tamanio    = 0;
            disco[i].cola_count = 0;
            strncpy(disco[i].nombre, nombre, MAX_NAME - 1);
            disco[i].nombre[MAX_NAME - 1] = '\0';
            disco[i].contenido[0] = '\0';
            printf("[OK] archivo '%s' creado en slot %d\n", nombre, i);
            return i;
        }
    }
    printf("[ERR] disco lleno (%d archivos)\n", MAX_FILES);
    return -1;
}

int abrir_archivo(int pid, const char *nombre) {
    int i, k;
    PCB *p;
    i = buscar_idx(nombre);
    if (i < 0) { printf("[ERR] archivo '%s' no existe\n", nombre); return -1; }
    p = buscar_proceso(pid);
    if (!p)    { printf("[ERR] PID %d no existe\n", pid); return -1; }

    if (disco[i].pid_dueno == -1) {
        disco[i].pid_dueno = pid;
        printf("[OK] PID %d abrio '%s'\n", pid, nombre);
        return 0;
    }
    if (disco[i].pid_dueno == pid) {
        printf("[INFO] PID %d ya tiene '%s' abierto\n", pid, nombre);
        return 0;
    }
    /* En uso: encolar y bloquear */
    if (disco[i].cola_count >= MAX_ESPERA) {
        printf("[ERR] cola de espera de '%s' llena\n", nombre);
        return -1;
    }
    for (k = 0; k < disco[i].cola_count; k++) {
        if (disco[i].cola_espera[k] == pid) {
            printf("[INFO] PID %d ya esta esperando '%s'\n", pid, nombre);
            return 1;
        }
    }
    disco[i].cola_espera[disco[i].cola_count++] = pid;
    bloquear_proceso(pid, i);
    printf("[BLOQUEADO] PID %d en espera de '%s' (dueno PID %d)\n",
           pid, nombre, disco[i].pid_dueno);
    return 1;
}

int escribir_archivo(int pid, const char *nombre, const char *texto) {
    int i = buscar_idx(nombre);
    if (i < 0) { printf("[ERR] archivo '%s' no existe\n", nombre); return -1; }
    if (disco[i].pid_dueno != pid) {
        printf("[ERR] PID %d no es dueno de '%s' (dueno=%d)\n",
               pid, nombre, disco[i].pid_dueno);
        return -1;
    }
    strncpy(disco[i].contenido, texto, MAX_CONTENT - 1);
    disco[i].contenido[MAX_CONTENT - 1] = '\0';
    disco[i].tamanio = (int)strlen(disco[i].contenido);
    printf("[OK] PID %d escribio %d B en '%s'\n", pid, disco[i].tamanio, nombre);
    return 0;
}

int leer_archivo(int pid, const char *nombre) {
    int i = buscar_idx(nombre);
    if (i < 0) { printf("[ERR] archivo '%s' no existe\n", nombre); return -1; }
    printf("[LECTURA] PID %d lee '%s' (%d B, dueno=%d):\n  >>> %s\n",
           pid, nombre, disco[i].tamanio, disco[i].pid_dueno, disco[i].contenido);
    return 0;
}

int cerrar_archivo(int pid, const char *nombre) {
    int i, k, nuevo;
    i = buscar_idx(nombre);
    if (i < 0) { printf("[ERR] archivo '%s' no existe\n", nombre); return -1; }
    if (disco[i].pid_dueno != pid) {
        printf("[ERR] PID %d no es dueno de '%s'\n", pid, nombre);
        return -1;
    }
    if (disco[i].cola_count > 0) {
        nuevo = disco[i].cola_espera[0];
        for (k = 1; k < disco[i].cola_count; k++)
            disco[i].cola_espera[k - 1] = disco[i].cola_espera[k];
        disco[i].cola_count--;
        disco[i].pid_dueno = nuevo;
        desbloquear_proceso(nuevo);
        printf("[OK] PID %d cerro '%s' -> entregado a PID %d\n",
               pid, nombre, nuevo);
    } else {
        disco[i].pid_dueno = -1;
        printf("[OK] PID %d cerro '%s'\n", pid, nombre);
    }
    return 0;
}

int eliminar_archivo(int pid, const char *nombre) {
    int i = buscar_idx(nombre);
    if (i < 0) { printf("[ERR] archivo '%s' no existe\n", nombre); return -1; }
    if (disco[i].pid_dueno != -1) {
        printf("[ERR] no se puede eliminar '%s': abierto por PID %d\n",
               nombre, disco[i].pid_dueno);
        return -1;
    }
    if (disco[i].cola_count != 0) {
        printf("[ERR] no se puede eliminar '%s': %d procesos en cola\n",
               nombre, disco[i].cola_count);
        return -1;
    }
    disco[i].usado     = 0;
    disco[i].nombre[0] = '\0';
    disco[i].contenido[0] = '\0';
    disco[i].tamanio   = 0;
    printf("[OK] PID %d elimino '%s'\n", pid, nombre);
    return 0;
}

/* Cierra todos los archivos del pid (usado al terminar el proceso). */
void liberar_archivos_de(int pid) {
    int i, k, nuevo, nueva_cola;
    for (i = 0; i < MAX_FILES; i++) {
        if (!disco[i].usado) continue;

        if (disco[i].pid_dueno == pid) {
            if (disco[i].cola_count > 0) {
                nuevo = disco[i].cola_espera[0];
                for (k = 1; k < disco[i].cola_count; k++)
                    disco[i].cola_espera[k - 1] = disco[i].cola_espera[k];
                disco[i].cola_count--;
                disco[i].pid_dueno = nuevo;
                desbloquear_proceso(nuevo);
                printf("[CLEANUP] '%s' transferido a PID %d (PID %d termino)\n",
                       disco[i].nombre, nuevo, pid);
            } else {
                disco[i].pid_dueno = -1;
            }
        }
        /* Sacar pid de la cola de espera de cualquier archivo */
        nueva_cola = 0;
        for (k = 0; k < disco[i].cola_count; k++) {
            if (disco[i].cola_espera[k] != pid)
                disco[i].cola_espera[nueva_cola++] = disco[i].cola_espera[k];
        }
        disco[i].cola_count = nueva_cola;
    }
}

void listar_archivos(void) {
    int i, k, n = 0;
    printf("\n--- Sistema de archivos ---\n");
    printf("  #   Nombre                Tam   Dueno   Cola_espera\n");
    for (i = 0; i < MAX_FILES; i++) {
        if (!disco[i].usado) continue;
        n++;
        printf("  %-3d %-22s %-5d %-7d ",
               i, disco[i].nombre, disco[i].tamanio, disco[i].pid_dueno);
        if (disco[i].cola_count == 0) printf("(ninguno)");
        else for (k = 0; k < disco[i].cola_count; k++)
            printf("%d ", disco[i].cola_espera[k]);
        printf("\n");
    }
    if (!n) printf("  (sin archivos)\n");
    printf("\n");
}

int contar_archivos_usados(void) {
    int i, c = 0;
    for (i = 0; i < MAX_FILES; i++) if (disco[i].usado) c++;
    return c;
}
