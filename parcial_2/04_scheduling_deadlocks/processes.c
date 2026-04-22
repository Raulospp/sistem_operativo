/*
 * processes.c
 * Universidad Santiago de Cali, 2026A
 * Laboratorio #2 Sistemas Operativos
 * Modulo 4: Planificacion de Procesos y Gestion de Interbloqueos
 *
 * Algoritmos implementados:
 *   - FCFS (First Come First Served)
 *   - Round Robin con quantum configurable
 *   - SPN / SJF (Shortest Process Next)
 *   - CFS simulado con min-heap por vruntime (simula arbol Rojo-Negro)
 *   - MLFQ (Multilevel Feedback Queue, 3 colas)
 *   - Algoritmo del Banquero (evitacion de deadlock)
 *   - Deteccion de ciclos con DFS (deteccion de deadlock)
 *
 * Compilar: gcc -Wall -std=c99 processes.c -o scheduler_lab
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define MAX_PROCESOS 10
#define QUANTUM      2
#define NUM_COLAS    3   /* MLFQ: 0=alta prioridad, 2=batch */
#define N_PROC       4   /* Banquero: numero de procesos */
#define N_REC        3   /* Banquero: tipos de recursos (A, B, C) */
#define N_NODOS      6   /* Deadlock DFS: 3 procesos + 3 recursos */

/* ================================================================
 * 1. PCB — Bloque de Control de Proceso
 * ================================================================ */
typedef enum { NUEVO, LISTO, EJECUCION, BLOQUEADO, TERMINADO } EstadoProceso;

typedef struct {
    int  pid;
    char nombre[32];
    int  prioridad;         /* menor valor = mayor prioridad */
    int  rafaga_total;      /* CPU total necesaria */
    int  rafaga_restante;   /* CPU restante (para RR, SRTF, CFS) */
    int  tiempo_llegada;
    int  tiempo_fin;
    int  tiempo_respuesta;  /* -1 hasta que ejecuta por primera vez */
    EstadoProceso estado;
    /* CFS */
    long vruntime;
    int  nice;              /* -20..19; menor = mas prioridad */
} PCB;

/* ================================================================
 * 2. Cola circular de listos (FIFO)
 * ================================================================ */
typedef struct {
    PCB *procesos[MAX_PROCESOS];
    int  frente, final, cantidad;
} ReadyQueue;

static void init_queue(ReadyQueue *q) {
    q->frente = q->final = q->cantidad = 0;
    memset(q->procesos, 0, sizeof(q->procesos));
}

static bool queue_vacia(ReadyQueue *q) { return q->cantidad == 0; }
static bool queue_llena(ReadyQueue *q) { return q->cantidad == MAX_PROCESOS; }

static void encolar(ReadyQueue *q, PCB *p) {
    if (queue_llena(q)) return;
    q->procesos[q->final] = p;
    q->final = (q->final + 1) % MAX_PROCESOS;
    q->cantidad++;
}

static PCB *desencolar(ReadyQueue *q) {
    if (queue_vacia(q)) return NULL;
    PCB *p = q->procesos[q->frente];
    q->frente = (q->frente + 1) % MAX_PROCESOS;
    q->cantidad--;
    return p;
}

/* ================================================================
 * 3A. FCFS — First Come First Served (no preemptivo)
 * ================================================================ */
void planificar_fcfs(ReadyQueue *q) {
    printf("\n=== FCFS (First Come First Served) ===\n");
    int t = 0, tat_total = 0, wt_total = 0;
    int total = q->cantidad;

    while (!queue_vacia(q)) {
        PCB *p = desencolar(q);
        t += p->rafaga_total;
        p->tiempo_fin = t;
        p->estado     = TERMINADO;

        int tat = p->tiempo_fin - p->tiempo_llegada;
        int wt  = tat - p->rafaga_total;
        printf("  %-12s | fin=%3d | TAT=%3d | WT=%3d\n",
               p->nombre, p->tiempo_fin, tat, wt);
        tat_total += tat;
        wt_total  += wt;
    }
    if (total > 0)
        printf("  Promedio          TAT=%.1f  WT=%.1f\n",
               (float)tat_total / total, (float)wt_total / total);
}

/* ================================================================
 * 3B. Round Robin — preemptivo con quantum fijo
 * ================================================================ */
void planificar_rr(ReadyQueue *q, int quantum) {
    printf("\n=== ROUND ROBIN (quantum=%d) ===\n", quantum);

    ReadyQueue tmp;
    init_queue(&tmp);
    int total = q->cantidad;
    while (!queue_vacia(q)) encolar(&tmp, desencolar(q));

    int t = 0, tat_total = 0, wt_total = 0;

    while (!queue_vacia(&tmp)) {
        PCB *p = desencolar(&tmp);

        /* primera vez que corre: registrar tiempo de respuesta */
        if (p->tiempo_respuesta == -1)
            p->tiempo_respuesta = t - p->tiempo_llegada;

        if (p->rafaga_restante <= quantum) {
            t += p->rafaga_restante;
            p->rafaga_restante = 0;
            p->tiempo_fin      = t;
            p->estado          = TERMINADO;

            int tat = p->tiempo_fin - p->tiempo_llegada;
            int wt  = tat - p->rafaga_total;
            printf("  [t=%3d] %-12s terminado | TAT=%3d | WT=%3d | RT=%3d\n",
                   t, p->nombre, tat, wt, p->tiempo_respuesta);
            tat_total += tat;
            wt_total  += wt;
        } else {
            t += quantum;
            p->rafaga_restante -= quantum;
            printf("  [t=%3d] %-12s ejecuta   | restante=%d\n",
                   t, p->nombre, p->rafaga_restante);
            encolar(&tmp, p); /* vuelve al final */
        }
    }
    if (total > 0)
        printf("  Promedio          TAT=%.1f  WT=%.1f\n",
               (float)tat_total / total, (float)wt_total / total);
}

/* ================================================================
 * 3C. SPN / SJF — Shortest Process Next (no preemptivo)
 * ================================================================ */
static int cmp_rafaga(const void *a, const void *b) {
    return (*(PCB **)a)->rafaga_total - (*(PCB **)b)->rafaga_total;
}

void planificar_spn(ReadyQueue *q) {
    printf("\n=== SPN / SJF (Shortest Process Next) ===\n");

    PCB *lista[MAX_PROCESOS];
    int  n = 0;
    while (!queue_vacia(q)) lista[n++] = desencolar(q);
    qsort(lista, n, sizeof(PCB *), cmp_rafaga);

    int t = 0, tat_total = 0, wt_total = 0;
    for (int i = 0; i < n; i++) {
        t += lista[i]->rafaga_total;
        lista[i]->tiempo_fin = t;
        lista[i]->estado     = TERMINADO;

        int tat = t - lista[i]->tiempo_llegada;
        int wt  = tat - lista[i]->rafaga_total;
        printf("  %-12s | fin=%3d | TAT=%3d | WT=%3d\n",
               lista[i]->nombre, t, tat, wt);
        tat_total += tat;
        wt_total  += wt;
    }
    if (n > 0)
        printf("  Promedio          TAT=%.1f  WT=%.1f\n",
               (float)tat_total / n, (float)wt_total / n);
}

/* ================================================================
 * 3D. CFS — Completely Fair Scheduler (min-heap por vruntime)
 *
 * Simula el arbol Rojo-Negro de Linux usando un min-heap binario.
 * El proceso con menor vruntime (= nodo mas a la izquierda del RBT)
 * siempre se selecciona primero. El time slice depende del nice:
 *   nice < 0  -> slice 4  (mayor prioridad, mas CPU)
 *   nice = 0  -> slice 2
 *   nice > 0  -> slice 1  (menor prioridad)
 * ================================================================ */
static PCB *cfs_heap[MAX_PROCESOS];
static int  cfs_heap_sz = 0;

static void heap_push(PCB *p) {
    cfs_heap[cfs_heap_sz] = p;
    int i = cfs_heap_sz++;
    while (i > 0) {
        int par = (i - 1) / 2;
        if (cfs_heap[par]->vruntime > cfs_heap[i]->vruntime) {
            PCB *tmp       = cfs_heap[par];
            cfs_heap[par]  = cfs_heap[i];
            cfs_heap[i]    = tmp;
            i = par;
        } else break;
    }
}

static PCB *heap_pop() {
    PCB *top       = cfs_heap[0];
    cfs_heap[0]    = cfs_heap[--cfs_heap_sz];
    int i = 0;
    while (1) {
        int l = 2*i+1, r = 2*i+2, mn = i;
        if (l < cfs_heap_sz && cfs_heap[l]->vruntime < cfs_heap[mn]->vruntime) mn = l;
        if (r < cfs_heap_sz && cfs_heap[r]->vruntime < cfs_heap[mn]->vruntime) mn = r;
        if (mn == i) break;
        PCB *tmp      = cfs_heap[i];
        cfs_heap[i]   = cfs_heap[mn];
        cfs_heap[mn]  = tmp;
        i = mn;
    }
    return top;
}

void planificar_cfs(ReadyQueue *q) {
    printf("\n=== CFS (min-heap por vruntime, simula arbol R-N) ===\n");

    cfs_heap_sz = 0;
    int total = q->cantidad;
    while (!queue_vacia(q)) heap_push(desencolar(q));

    int t = 0, tat_total = 0, wt_total = 0;

    while (cfs_heap_sz > 0) {
        PCB *p    = heap_pop();
        int slice = (p->nice < 0) ? 4 : (p->nice > 0) ? 1 : 2;

        if (p->rafaga_restante <= slice) {
            int ejecutado      = p->rafaga_restante;
            t                 += ejecutado;
            p->vruntime       += ejecutado;
            p->rafaga_restante = 0;
            p->tiempo_fin      = t;
            p->estado          = TERMINADO;

            int tat = t - p->tiempo_llegada;
            int wt  = tat - p->rafaga_total;
            printf("  [t=%3d] %-12s terminado | vruntime=%4ld | TAT=%3d | WT=%3d\n",
                   t, p->nombre, p->vruntime, tat, wt);
            tat_total += tat;
            wt_total  += wt;
        } else {
            t                 += slice;
            p->rafaga_restante -= slice;
            p->vruntime       += slice;
            printf("  [t=%3d] %-12s slice=%d   | vruntime=%4ld | restante=%d\n",
                   t, p->nombre, slice, p->vruntime, p->rafaga_restante);
            heap_push(p);
        }
    }
    if (total > 0)
        printf("  Promedio          TAT=%.1f  WT=%.1f\n",
               (float)tat_total / total, (float)wt_total / total);
}

/* ================================================================
 * 3E. MLFQ — Multilevel Feedback Queue (3 colas)
 *
 * Cola 0: quantum=2 (alta prioridad, interactivos)
 * Cola 1: quantum=4
 * Cola 2: quantum=8 (batch, CPU-bound)
 * Regla: proceso que usa todo su quantum degrada a la siguiente cola.
 *        Siempre se atiende la cola de mayor prioridad no vacia.
 * ================================================================ */
void planificar_mlfq(ReadyQueue *q) {
    printf("\n=== MLFQ (3 colas con degradacion) ===\n");

    ReadyQueue colas[NUM_COLAS];
    int quantums[NUM_COLAS] = {2, 4, 8};
    for (int i = 0; i < NUM_COLAS; i++) init_queue(&colas[i]);

    int total = q->cantidad;
    while (!queue_vacia(q)) encolar(&colas[0], desencolar(q));

    int t = 0, tat_total = 0, wt_total = 0, terminados = 0;

    while (terminados < total) {
        /* seleccionar la cola de mayor prioridad no vacia */
        int cola_activa = -1;
        for (int i = 0; i < NUM_COLAS; i++) {
            if (!queue_vacia(&colas[i])) { cola_activa = i; break; }
        }
        if (cola_activa == -1) break;

        PCB *p   = desencolar(&colas[cola_activa]);
        int  qt  = quantums[cola_activa];

        if (p->rafaga_restante <= qt) {
            t                 += p->rafaga_restante;
            p->rafaga_restante = 0;
            p->tiempo_fin      = t;
            p->estado          = TERMINADO;

            int tat = t - p->tiempo_llegada;
            int wt  = tat - p->rafaga_total;
            printf("  [t=%3d] Cola%d | %-12s terminado | TAT=%3d | WT=%3d\n",
                   t, cola_activa, p->nombre, tat, wt);
            tat_total += tat;
            wt_total  += wt;
            terminados++;
        } else {
            t                 += qt;
            p->rafaga_restante -= qt;
            int next = (cola_activa < NUM_COLAS - 1) ? cola_activa + 1 : cola_activa;
            printf("  [t=%3d] Cola%d | %-12s (restante=%d) -> Cola%d\n",
                   t, cola_activa, p->nombre, p->rafaga_restante, next);
            encolar(&colas[next], p);
        }
    }
    if (total > 0)
        printf("  Promedio          TAT=%.1f  WT=%.1f\n",
               (float)tat_total / total, (float)wt_total / total);
}

/* ================================================================
 * 4. ALGORITMO DEL BANQUERO — Evitacion de Deadlock (Dijkstra)
 *
 * Dados: Available, Max, Allocation -> Need = Max - Allocation
 * Busca una secuencia segura de finalizacion.
 * Si no existe -> estado inseguro -> rechazar asignacion.
 *
 * Ejemplo clasico con 4 procesos y 3 recursos (A, B, C):
 * ================================================================ */
void algoritmo_banquero() {
    printf("\n=== ALGORITMO DEL BANQUERO (Evitacion de Deadlock) ===\n");

    int available[N_REC]            = {3, 3, 2};
    int max_demand[N_PROC][N_REC]   = {{7,5,3},{3,2,2},{9,0,2},{2,2,2}};
    int allocation[N_PROC][N_REC]   = {{0,1,0},{2,0,0},{3,0,2},{2,1,1}};
    int need[N_PROC][N_REC];

    printf("  Available: [A=%d B=%d C=%d]\n",
           available[0], available[1], available[2]);
    printf("  %-6s  Max       Alloc     Need\n", "Proc");

    for (int i = 0; i < N_PROC; i++) {
        for (int j = 0; j < N_REC; j++)
            need[i][j] = max_demand[i][j] - allocation[i][j];
        printf("  P%-5d [%d %d %d]   [%d %d %d]   [%d %d %d]\n", i,
               max_demand[i][0], max_demand[i][1], max_demand[i][2],
               allocation[i][0], allocation[i][1], allocation[i][2],
               need[i][0],       need[i][1],       need[i][2]);
    }

    /* Algoritmo de seguridad */
    int  work[N_REC];
    bool finish[N_PROC];
    int  seq[N_PROC];
    int  seq_idx = 0;

    for (int j = 0; j < N_REC; j++) work[j]   = available[j];
    for (int i = 0; i < N_PROC; i++) finish[i] = false;

    for (int count = 0; count < N_PROC; count++) {
        for (int i = 0; i < N_PROC; i++) {
            if (finish[i]) continue;
            bool puede = true;
            for (int j = 0; j < N_REC; j++) {
                if (need[i][j] > work[j]) { puede = false; break; }
            }
            if (puede) {
                for (int j = 0; j < N_REC; j++) work[j] += allocation[i][j];
                finish[i]      = true;
                seq[seq_idx++] = i;
                break;
            }
        }
    }

    bool seguro = true;
    for (int i = 0; i < N_PROC; i++) if (!finish[i]) { seguro = false; break; }

    if (seguro) {
        printf("\n  ESTADO SEGURO. Secuencia de finalizacion: ");
        for (int i = 0; i < seq_idx; i++) printf("P%d ", seq[i]);
        printf("\n");
    } else {
        printf("\n  ESTADO INSEGURO — posible DEADLOCK detectado.\n");
    }
}

/* ================================================================
 * 5. DETECCION DE DEADLOCK — DFS sobre grafo de asignacion
 *
 * Nodos 0..2 = procesos P0, P1, P2
 * Nodos 3..5 = recursos R0, R1, R2
 * solicita[pi][rj] = 1  -> proceso pi solicita recurso rj
 * asignado[rj][pi] = 1  -> recurso rj asignado a proceso pi
 *
 * Un ciclo en el grafo => deadlock.
 * ================================================================ */
static int  solicita_g[3][3];
static int  asignado_g[3][3];
static bool vis[N_NODOS];
static bool pila[N_NODOS];

static bool dfs(int nodo) {
    vis[nodo] = pila[nodo] = true;

    if (nodo < 3) {
        /* proceso: aristas hacia los recursos que solicita */
        for (int r = 0; r < 3; r++) {
            if (!solicita_g[nodo][r]) continue;
            int dst = r + 3;
            if (!vis[dst] && dfs(dst)) return true;
            if (pila[dst]) return true;
        }
    } else {
        /* recurso: aristas hacia los procesos a los que esta asignado */
        int r = nodo - 3;
        for (int p = 0; p < 3; p++) {
            if (!asignado_g[r][p]) continue;
            if (!vis[p] && dfs(p)) return true;
            if (pila[p]) return true;
        }
    }

    pila[nodo] = false;
    return false;
}

void detectar_deadlock() {
    printf("\n=== DETECCION DE DEADLOCK (DFS en grafo de asignacion) ===\n");

    /*
     * Escenario del tablero (clase):
     *   P1 solicita R2  |  R2 asignado a P2
     *   P2 solicita R1  |  R1 asignado a P3
     *   P3 solicita R2  -> ciclo: P1->R2->P2->R1->P3->R2 = DEADLOCK
     *
     * Indices internos: proceso Pi = nodo (i-1), recurso Rj = nodo (j-1)+3
     */
    memset(solicita_g, 0, sizeof(solicita_g));
    memset(asignado_g, 0, sizeof(asignado_g));

    solicita_g[0][1] = 1;  /* P1 solicita R2 */
    solicita_g[1][0] = 1;  /* P2 solicita R1 */
    solicita_g[2][1] = 1;  /* P3 solicita R2 */

    asignado_g[1][1] = 1;  /* R2 asignado a P2 */
    asignado_g[0][2] = 1;  /* R1 asignado a P3 */

    printf("  Grafo (tablero): P1->R2->P2->R1->P3->R2 (ciclo esperado)\n");

    memset(vis,  false, sizeof(vis));
    memset(pila, false, sizeof(pila));
    bool deadlock = false;

    for (int i = 0; i < N_NODOS; i++) {
        if (!vis[i] && dfs(i)) { deadlock = true; break; }
    }

    printf("  Resultado: %s\n",
           deadlock ? "DEADLOCK DETECTADO (ciclo en grafo)" : "Sin deadlock");
}

/* ================================================================
 * Utilidades: crear / liberar PCB y cargar cola de prueba
 * ================================================================ */
static PCB *crear_proceso(int pid, const char *nombre,
                           int rafaga, int llegada, int nice, int prioridad) {
    PCB *p = (PCB *) malloc(sizeof(PCB));
    p->pid              = pid;
    strncpy(p->nombre, nombre, 31);
    p->rafaga_total     = rafaga;
    p->rafaga_restante  = rafaga;
    p->tiempo_llegada   = llegada;
    p->tiempo_fin       = 0;
    p->tiempo_respuesta = -1;
    p->nice             = nice;
    p->vruntime         = 0;
    p->prioridad        = prioridad;
    p->estado           = LISTO;
    return p;
}

static void liberar_cola(ReadyQueue *q) {
    while (!queue_vacia(q)) free(desencolar(q));
}

/* Carga los mismos 4 procesos de ejemplo en la cola */
static void cargar_procesos(ReadyQueue *q) {
    init_queue(q);
    /*                  pid  nombre         rafaga  llegada  nice  prioridad */
    encolar(q, crear_proceso(1, "EditorTexto", 8,    0,       0,    3));
    encolar(q, crear_proceso(2, "Compilador",  4,    0,      -5,    1));
    encolar(q, crear_proceso(3, "Navegador",  12,    0,       5,    4));
    encolar(q, crear_proceso(4, "Servidor",    6,    0,      -2,    2));
}

/* ================================================================
 * MAIN
 * ================================================================ */
int main() {
    printf("=========================================\n");
    printf(" Lab #2 - Sistemas Operativos - USC 2026A\n");
    printf(" Modulo 4: Scheduling + Deadlocks\n");
    printf("=========================================\n");

    ReadyQueue cola;

    /* --- Algoritmos de planificacion --- */
    cargar_procesos(&cola);  planificar_fcfs(&cola);
    liberar_cola(&cola);

    cargar_procesos(&cola);  planificar_rr(&cola, QUANTUM);
    liberar_cola(&cola);

    cargar_procesos(&cola);  planificar_spn(&cola);
    liberar_cola(&cola);

    cargar_procesos(&cola);  planificar_cfs(&cola);
    liberar_cola(&cola);

    cargar_procesos(&cola);  planificar_mlfq(&cola);
    liberar_cola(&cola);

    /* --- Gestion de interbloqueos --- */
    algoritmo_banquero();
    detectar_deadlock();

    printf("\n=========================================\n");
    printf(" Fin del laboratorio\n");
    printf("=========================================\n");
    return 0;
}
