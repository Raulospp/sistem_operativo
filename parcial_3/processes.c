/* processes.c
 * Administrador de procesos: tabla PCB, cola de listos, scheduler FCFS/RR.
 * Adaptado de parcial_2/04_scheduling_deadlocks/processes.c
 *
 * Decisiones de diseno:
 *   - Tabla estatica de MAX_PROCESOS slots (sin malloc).
 *   - Cola circular indexada por PID (no por puntero).
 *   - terminar_proceso() libera tambien archivos y memoria en cascada.
 *   - El scheduler es ilustrativo: no consume CPU real, muestra el orden.
 */
#include <stdio.h>
#include <string.h>
#include "tipos.h"

PCB procesos[MAX_PROCESOS];
static int next_pid = 1;

/* Cola circular de listos (almacena PIDs) */
static int ready_q[MAX_PROCESOS];
static int rq_frente, rq_final, rq_count;

static void rq_init(void)        { rq_frente = rq_final = rq_count = 0; }
static int  rq_vacia(void)       { return rq_count == 0; }
static void rq_encolar(int pid) {
    if (rq_count >= MAX_PROCESOS) return;
    ready_q[rq_final] = pid;
    rq_final = (rq_final + 1) % MAX_PROCESOS;
    rq_count++;
}
static int  rq_desencolar(void) {
    int pid;
    if (rq_vacia()) return -1;
    pid = ready_q[rq_frente];
    rq_frente = (rq_frente + 1) % MAX_PROCESOS;
    rq_count--;
    return pid;
}
static void rq_remover(int pid) {
    int k, n;
    if (rq_vacia()) return;
    n = rq_count;
    for (k = 0; k < n; k++) {
        int actual = rq_desencolar();
        if (actual != pid) rq_encolar(actual);
    }
}

void init_procesos(void) {
    int i;
    for (i = 0; i < MAX_PROCESOS; i++) {
        procesos[i].pid              = -1;
        procesos[i].estado           = LIBRE;
        procesos[i].nombre[0]        = '\0';
        procesos[i].memoria_asignada = 0;
        procesos[i].archivo_esperado = -1;
        procesos[i].rafaga_total     = 0;
        procesos[i].rafaga_restante  = 0;
        procesos[i].prioridad        = 0;
    }
    next_pid = 1;
    rq_init();
}

PCB *buscar_proceso(int pid) {
    int i;
    for (i = 0; i < MAX_PROCESOS; i++)
        if (procesos[i].pid == pid && procesos[i].estado != LIBRE)
            return &procesos[i];
    return NULL;
}

int crear_proceso(const char *nombre, int prioridad, int mem_solicitada) {
    int slot = -1, i, pid;
    PCB *p;
    for (i = 0; i < MAX_PROCESOS; i++)
        if (procesos[i].estado == LIBRE) { slot = i; break; }
    if (slot < 0) {
        printf("[ERR] tabla de procesos llena\n");
        return -1;
    }
    pid = next_pid++;
    if (mem_solicitada > 0 && asignar_memoria(pid, mem_solicitada) < 0) {
        printf("[ERR] sin memoria para %s (%d B)\n", nombre, mem_solicitada);
        return -1;
    }
    p = &procesos[slot];
    p->pid = pid;
    strncpy(p->nombre, nombre, 31);
    p->nombre[31] = '\0';
    p->estado = LISTO;
    p->prioridad = prioridad;
    p->rafaga_total = 5;
    p->rafaga_restante = 5;
    p->memoria_asignada = mem_solicitada;
    p->archivo_esperado = -1;
    rq_encolar(pid);
    printf("[OK] proceso '%s' creado con PID %d (%d B, prio=%d)\n",
           nombre, pid, mem_solicitada, prioridad);
    return pid;
}

int terminar_proceso(int pid) {
    PCB *p = buscar_proceso(pid);
    if (!p) {
        printf("[ERR] PID %d no existe\n", pid);
        return -1;
    }
    liberar_archivos_de(pid);
    liberar_memoria(pid);
    rq_remover(pid);
    printf("[OK] PID %d (%s) terminado\n", pid, p->nombre);
    p->pid    = -1;
    p->estado = LIBRE;
    p->nombre[0] = '\0';
    p->memoria_asignada = 0;
    p->archivo_esperado = -1;
    return 0;
}

void bloquear_proceso(int pid, int archivo_idx) {
    PCB *p = buscar_proceso(pid);
    if (!p) return;
    p->estado = BLOQUEADO;
    p->archivo_esperado = archivo_idx;
    rq_remover(pid);
}

void desbloquear_proceso(int pid) {
    PCB *p = buscar_proceso(pid);
    if (!p) return;
    p->estado = LISTO;
    p->archivo_esperado = -1;
    rq_encolar(pid);
}

const char *nombre_estado(EstadoProceso e) {
    switch (e) {
        case NUEVO:     return "NUEVO";
        case LISTO:     return "LISTO";
        case EJECUCION: return "EJECUCION";
        case BLOQUEADO: return "BLOQUEADO";
        case TERMINADO: return "TERMINADO";
        default:        return "LIBRE";
    }
}

void mostrar_procesos(void) {
    int i, activos = 0, n, f, k;
    printf("\n--- Tabla de procesos ---\n");
    printf("  PID  Nombre              Estado     Prio  Mem   EsperaArch\n");
    for (i = 0; i < MAX_PROCESOS; i++) {
        PCB *p = &procesos[i];
        if (p->estado == LIBRE) continue;
        activos++;
        printf("  %-4d %-18s %-10s %-5d %-5d %d\n",
               p->pid, p->nombre, nombre_estado(p->estado),
               p->prioridad, p->memoria_asignada, p->archivo_esperado);
    }
    if (!activos) printf("  (sin procesos)\n");
    printf("  Cola listos: ");
    n = rq_count;
    f = rq_frente;
    for (k = 0; k < n; k++) {
        printf("%d ", ready_q[f]);
        f = (f + 1) % MAX_PROCESOS;
    }
    if (n == 0) printf("(vacia)");
    printf("\n\n");
}

int contar_procesos_estado(EstadoProceso e) {
    int i, c = 0;
    for (i = 0; i < MAX_PROCESOS; i++)
        if (procesos[i].estado == e) c++;
    return c;
}

/* Scheduler FCFS ilustrativo: muestra orden de servicio sin terminar procesos. */
void planificar_fcfs(void) {
    int copia[MAX_PROCESOS], n = 0, t = 0, i;
    printf("\n=== Planificacion FCFS ===\n");
    if (rq_vacia()) { printf("  (cola vacia)\n\n"); return; }
    while (!rq_vacia()) copia[n++] = rq_desencolar();
    for (i = 0; i < n; i++) {
        PCB *p = buscar_proceso(copia[i]);
        if (!p) continue;
        printf("  t=%2d  PID %d (%s) ejecuta %d unidades -> fin=%d\n",
               t, p->pid, p->nombre, p->rafaga_total, t + p->rafaga_total);
        t += p->rafaga_total;
        rq_encolar(copia[i]);
    }
    printf("  Fin FCFS (tiempo simulado=%d, procesos=%d)\n\n", t, n);
}

/* Round Robin ilustrativo. Itera hasta 3 vueltas para mantener la cola estable. */
void planificar_rr(int quantum) {
    int copia[MAX_PROCESOS], rest[MAX_PROCESOS], n = 0, t = 0, vueltas = 0, i;
    printf("\n=== Planificacion Round Robin (q=%d) ===\n", quantum);
    if (rq_vacia()) { printf("  (cola vacia)\n\n"); return; }
    while (!rq_vacia()) copia[n++] = rq_desencolar();
    for (i = 0; i < n; i++) {
        PCB *p = buscar_proceso(copia[i]);
        rest[i] = p ? p->rafaga_total : 0;
    }
    while (vueltas < 3 * n) {
        int activos = 0;
        for (i = 0; i < n; i++) {
            int slice;
            PCB *p;
            if (rest[i] <= 0) continue;
            activos++;
            p = buscar_proceso(copia[i]);
            if (!p) continue;
            slice = (rest[i] < quantum) ? rest[i] : quantum;
            printf("  t=%2d  PID %d (%s) slice=%d  restante=%d\n",
                   t, p->pid, p->nombre, slice, rest[i] - slice);
            t += slice;
            rest[i] -= slice;
            vueltas++;
        }
        if (!activos) break;
    }
    for (i = 0; i < n; i++) rq_encolar(copia[i]);
    printf("  Fin RR (tiempo simulado=%d)\n\n", t);
}
