/*
 * process_mgmt.c
 * Universidad Santiago de Cali, 2026A
 * Lab #2 - Modulo 1: Anatomia de Proceso
 *
 * Demuestra: PCB (struct), Process Image (segmentos texto/datos/pila),
 *            creacion de procesos con fork(), sincronizacion con wait().
 *
 * Compilar: gcc -Wall -std=c99 process_mgmt.c -o process_mgmt
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

/* ========== PCB: Bloque de Control de Proceso ========== */
typedef enum {
    NUEVO, LISTO, EJECUCION, BLOQUEADO, TERMINADO
} EstadoProceso;

typedef struct {
    int   pid;
    int   ppid;             /* PID del padre */
    char  nombre[32];
    EstadoProceso estado;
    int   prioridad;
    int   tiempo_cpu;       /* tiempo de CPU consumido (simulado) */
    /* Contexto del procesador (guardado en context-switch) */
    int   reg_pc;           /* program counter simulado */
    int   reg_sp;           /* stack pointer simulado */
    int   reg_ax;           /* registro de proposito general */
} PCB;

/* ========== Imagen del Proceso (simulacion de segmentos) ========== */
typedef struct {
    char  segmento_texto[64];  /* codigo del programa */
    int   datos_globales[8];   /* segmento de datos */
    int   pila[16];            /* stack de llamadas */
    int   tope_pila;
} ImagenProceso;

/* ========== Funciones auxiliares ========== */
PCB crear_pcb(int pid, int ppid, const char *nombre, int prioridad) {
    PCB pcb;
    pcb.pid       = pid;
    pcb.ppid      = ppid;
    pcb.prioridad = prioridad;
    pcb.estado    = NUEVO;
    pcb.tiempo_cpu = 0;
    pcb.reg_pc    = 0;
    pcb.reg_sp    = 0;
    pcb.reg_ax    = 0;
    strncpy(pcb.nombre, nombre, 31);
    return pcb;
}

void imprimir_pcb(PCB *pcb) {
    const char *estados[] = {"NUEVO","LISTO","EJECUCION","BLOQUEADO","TERMINADO"};
    printf("  PCB[pid=%d] padre=%d | nombre=%-12s | estado=%-10s | prioridad=%d | cpu=%d\n",
        pcb->pid, pcb->ppid, pcb->nombre, estados[pcb->estado], pcb->prioridad, pcb->tiempo_cpu);
}

void simular_ejecucion(PCB *pcb, int ciclos) {
    pcb->estado    = EJECUCION;
    pcb->tiempo_cpu += ciclos;
    pcb->reg_pc   += ciclos * 4;  /* avanza PC (simulado) */
}

/* ========== MAIN ========== */
int main() {
    printf("=== MODULO 1: Anatomia de Proceso ===\n\n");

    /* --- Crear PCB del proceso padre --- */
    PCB pcb_padre = crear_pcb(getpid(), getppid(), "Padre", 5);
    pcb_padre.estado = EJECUCION;

    printf("[Padre] PCB inicial:\n");
    imprimir_pcb(&pcb_padre);

    /* --- Imagen del proceso: segmentos --- */
    ImagenProceso imagen;
    strncpy(imagen.segmento_texto, "main() { fork(); fork(); wait(); }", 63);
    imagen.datos_globales[0] = 100;
    imagen.datos_globales[1] = 200;
    imagen.tope_pila = 0;
    imagen.pila[imagen.tope_pila++] = 0xDEAD; /* direccion de retorno simulada */

    printf("\n[Padre] Imagen del proceso:\n");
    printf("  Texto  : %s\n", imagen.segmento_texto);
    printf("  Datos  : [%d, %d, ...]\n", imagen.datos_globales[0], imagen.datos_globales[1]);
    printf("  Pila   : tope=%d, ret=0x%X\n\n", imagen.tope_pila, imagen.pila[0]);

    /* ---- fork() hijo 1 ---- */
    pid_t pid_h1 = fork();

    if (pid_h1 < 0) {
        perror("Error fork hijo1");
        exit(1);
    }

    if (pid_h1 == 0) {
        /* Proceso Hijo 1 */
        PCB pcb_h1 = crear_pcb(getpid(), getppid(), "Hijo1", 3);
        simular_ejecucion(&pcb_h1, 5);
        printf("[Hijo1 pid=%d] ejecutando...\n", getpid());
        imprimir_pcb(&pcb_h1);
        pcb_h1.estado = TERMINADO;
        printf("[Hijo1] terminando.\n");
        exit(0);
    }

    /* ---- fork() hijo 2 (solo el padre lo crea) ---- */
    pid_t pid_h2 = fork();

    if (pid_h2 < 0) {
        perror("Error fork hijo2");
        exit(1);
    }

    if (pid_h2 == 0) {
        /* Proceso Hijo 2 */
        PCB pcb_h2 = crear_pcb(getpid(), getppid(), "Hijo2", 2);
        simular_ejecucion(&pcb_h2, 8);
        printf("[Hijo2 pid=%d] ejecutando...\n", getpid());
        imprimir_pcb(&pcb_h2);
        pcb_h2.estado = TERMINADO;
        printf("[Hijo2] terminando.\n");
        exit(0);
    }

    /* ---- Padre espera a ambos hijos ---- */
    int status;
    pid_t terminado;

    while ((terminado = wait(&status)) > 0) {
        if (WIFEXITED(status))
            printf("[Padre] Hijo pid=%d termino con codigo %d\n", terminado, WEXITSTATUS(status));
    }

    simular_ejecucion(&pcb_padre, 2);
    pcb_padre.estado = TERMINADO;
    printf("\n[Padre] PCB final:\n");
    imprimir_pcb(&pcb_padre);
    printf("\n[Padre] Fin del programa.\n");

    return 0;
}
