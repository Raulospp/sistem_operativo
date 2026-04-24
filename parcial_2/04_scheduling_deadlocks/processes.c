/*
 * processes.c
 * Universidad Santiago de Cali, 2026A
 * Laboratorio #2 - Sistemas Operativos
 *
 * Estudiantes:
 *   Juan Mateo Salas Arturo      - 1004235543
 *   Raul Guillermo Orobio Ospina - 1110286143
 *   Jean Paul Aragon Diaz        - 1108559610
 *
 * Docente: Diego Fernando Loaiza Buitrago
 *
 * Compilar: gcc -Wall -std=c99 processes.c -o scheduler_lab
 *
 * Contenido:
 *   1. Estructuras de datos: PCB, ImagenProceso, ReadyQueue
 *   2. ciclo_de_vida()       - simula transiciones de estado del proceso
 *   3. llamar_planificador() - dispatcher por id de algoritmo
 *   4. planificar_rr()       - Round Robin (preemptivo) [prevencion deadlock]
 *   5. planificar_sjf()      - Shortest Job First       [prevencion deadlock]
 *   6. planificar_prioridad()- Prioridad simple         [evitacion deadlock]
 *   7. planificar_cfs()      - CFS con min-heap/R-N     [BONUS]
 *   8. algoritmo_banquero()  - evitacion de deadlock
 *   9. detectar_deadlock()   - deteccion con DFS
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* ================================================================
 * CONSTANTES
 * ================================================================ */
#define MAX_PROCESOS  10
#define QUANTUM        2    /* quantum Round Robin */
#define NUM_COLAS      3    /* colas MLFQ */
#define N_REC          3    /* recursos Banquero: A, B, C */
#define N_PROC_BAN     4    /* procesos Banquero */
#define N_NODOS        6    /* nodos grafo deadlock: 3 proc + 3 rec */

/* IDs de algoritmo para llamar_planificador() */
#define ALG_RR         1
#define ALG_SJF        2
#define ALG_PRIORIDAD  3
#define ALG_CFS        4
#define ALG_FCFS       0

/* ================================================================
 * 1. ESTRUCTURAS DE DATOS
 * ================================================================ */

/* --- 1A. Ciclo de vida del proceso --- */
typedef enum {
    NUEVO,       /* proceso creado, aun no admitido */
    LISTO,       /* en cola de listos, esperando CPU */
    EJECUCION,   /* actualmente en la CPU */
    BLOQUEADO,   /* esperando E/S u otro evento */
    TERMINADO    /* finalizo ejecucion */
} EstadoProceso;

/* --- 1B. PCB — Bloque de Control de Proceso ---
 * Es la estructura central del SO para gestionar un proceso.
 * Contiene identificadores, estado del procesador y parametros
 * de planificacion. Permite suspender y reanudar un proceso
 * exactamente en el mismo punto (context switch). */
typedef struct {
    /* Identificadores */
    int  pid;
    char nombre[32];

    /* Informacion de planificacion */
    int  prioridad;        /* menor valor = mayor prioridad */
    int  rafaga_total;     /* CPU total requerida */
    int  rafaga_restante;  /* CPU restante (RR, CFS) */
    int  tiempo_llegada;   /* instante de arribo al sistema */
    int  tiempo_fin;       /* instante en que termino */
    int  tiempo_respuesta; /* -1 hasta primera ejecucion */

    /* Estado del proceso */
    EstadoProceso estado;

    /* Campos CFS (Completely Fair Scheduler) */
    long vruntime;         /* tiempo virtual acumulado */
    int  nice;             /* rango -20..19; menor = mas CPU */

    /* Recursos simulados para evitacion de deadlock */
    int  recursos_max[N_REC];    /* maximo que puede pedir */
    int  recursos_asig[N_REC];   /* actualmente asignados */
} PCB;

/* --- 1C. Imagen del Proceso (Process Image / Core Image) ---
 * Coleccion completa de segmentos necesarios para la ejecucion:
 * texto (codigo), datos globales/estaticos y pila de llamadas. */
typedef struct {
    char segmento_texto[64];  /* codigo del programa */
    int  datos_globales[8];   /* variables globales y estaticas */
    int  pila[16];            /* stack de llamadas a funciones */
    int  tope_pila;
    int  registro_pc;         /* program counter simulado */
    int  registro_sp;         /* stack pointer simulado */
} ImagenProceso;

/* --- 1D. Cola circular de listos (FIFO) --- */
typedef struct {
    PCB *procesos[MAX_PROCESOS];
    int  frente, final, cantidad;
} ReadyQueue;

/* ================================================================
 * 2. FUNCIONES AUXILIARES DE COLA
 * ================================================================ */
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
 * 3. CICLO DE VIDA DEL PROCESO
 *
 * Simula las transiciones de estado de un proceso desde su
 * creacion hasta su finalizacion. Si el proceso tiene baja
 * prioridad (> 3), se simula un bloqueo por E/S intermedio.
 * ================================================================ */
void ciclo_de_vida(PCB *p) {
    const char *nombres_estado[] = {
        "NUEVO", "LISTO", "EJECUCION", "BLOQUEADO", "TERMINADO"
    };

    printf("  [Ciclo PID=%d %-12s] ", p->pid, p->nombre);

    /* NUEVO: proceso recien creado */
    p->estado = NUEVO;
    printf("%s", nombres_estado[p->estado]);

    /* NUEVO -> LISTO: admitido por el SO, entra a la cola */
    p->estado = LISTO;
    printf(" -> %s", nombres_estado[p->estado]);

    /* LISTO -> EJECUCION: dispatcher lo selecciona */
    p->estado = EJECUCION;
    printf(" -> %s", nombres_estado[p->estado]);

    /* Simulacion de bloqueo por E/S para procesos de baja prioridad */
    if (p->prioridad > 3) {
        p->estado = BLOQUEADO;
        printf(" -> %s", nombres_estado[p->estado]);
        p->estado = LISTO;
        printf(" -> %s", nombres_estado[p->estado]);
        p->estado = EJECUCION;
        printf(" -> %s", nombres_estado[p->estado]);
    }

    /* EJECUCION -> TERMINADO: proceso completo */
    p->estado = TERMINADO;
    printf(" -> %s\n", nombres_estado[p->estado]);
}

/* ================================================================
 * 4. DISPATCHER — llamar_planificador()
 *
 * Selecciona y ejecuta el algoritmo de planificacion indicado
 * por su ID. Es el punto de entrada unico para el scheduler,
 * equivalente al dispatcher del nucleo del SO.
 *
 * Parametros:
 *   q         - cola de procesos listos
 *   algoritmo - ALG_RR | ALG_SJF | ALG_PRIORIDAD | ALG_CFS | ALG_FCFS
 * ================================================================ */
void planificar_fcfs(ReadyQueue *q);
void planificar_rr(ReadyQueue *q, int quantum);
void planificar_sjf(ReadyQueue *q);
void planificar_prioridad(ReadyQueue *q);
void planificar_cfs(ReadyQueue *q);

void llamar_planificador(ReadyQueue *q, int algoritmo) {
    printf("\n[Dispatcher] Algoritmo seleccionado: ");
    switch (algoritmo) {
        case ALG_RR:        printf("Round Robin\n");       planificar_rr(q, QUANTUM);  break;
        case ALG_SJF:       printf("SJF\n");               planificar_sjf(q);          break;
        case ALG_PRIORIDAD: printf("Prioridad Simple\n");  planificar_prioridad(q);    break;
        case ALG_CFS:       printf("CFS\n");               planificar_cfs(q);          break;
        default:            printf("FCFS\n");              planificar_fcfs(q);         break;
    }
}

/* ================================================================
 * 5A. FCFS — First Come First Served
 *
 * No preemptivo. Atiende en orden de llegada.
 * Deadlock: no preemption condition activa -> depende del
 * Algoritmo del Banquero externo para garantizar estado seguro.
 * ================================================================ */
void planificar_fcfs(ReadyQueue *q) {
    printf("\n=== FCFS (First Come First Served) ===\n");
    int t = 0, tat_total = 0, wt_total = 0;
    int total = q->cantidad;

    while (!queue_vacia(q)) {
        PCB *p = desencolar(q);
        p->estado = EJECUCION;
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
        printf("  Promedio TAT=%.1f  WT=%.1f\n",
               (float)tat_total/total, (float)wt_total/total);
}

/* ================================================================
 * 5B. Round Robin — preemptivo con quantum fijo
 *
 * Equidad: cada proceso recibe el mismo tiempo de CPU (quantum).
 * Rendimiento: tiempo de respuesta predecible O(1/n por ciclo).
 * Deadlock (PREVENCION): la preempcion forzada elimina la
 * condicion de Coffman "sin preempcion" -> ningun proceso puede
 * retener CPU indefinidamente, rompiendo la espera circular.
 * ================================================================ */
void planificar_rr(ReadyQueue *q, int quantum) {
    printf("\n=== ROUND ROBIN (quantum=%d) [Prevencion deadlock: preempcion] ===\n", quantum);

    ReadyQueue tmp;
    init_queue(&tmp);
    int total = q->cantidad;
    while (!queue_vacia(q)) encolar(&tmp, desencolar(q));

    int t = 0, tat_total = 0, wt_total = 0;

    while (!queue_vacia(&tmp)) {
        PCB *p = desencolar(&tmp);
        p->estado = EJECUCION;

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
            p->estado = LISTO;  /* preemptado: vuelve a LISTO */
            printf("  [t=%3d] %-12s preemptado | restante=%d\n",
                   t, p->nombre, p->rafaga_restante);
            encolar(&tmp, p);
        }
    }
    if (total > 0)
        printf("  Promedio TAT=%.1f  WT=%.1f\n",
               (float)tat_total/total, (float)wt_total/total);
}

/* ================================================================
 * 5C. SJF — Shortest Job First (no preemptivo)
 *
 * Rendimiento: minimiza el tiempo de espera promedio (optimo
 *              para procesos con rafagas conocidas).
 * Equidad: puede causar inanicion en procesos largos.
 * Deadlock (PREVENCION): al ejecutar los procesos mas cortos
 * primero, los recursos se liberan rapidamente, reduciendo
 * el tiempo de retencion y la probabilidad de espera circular.
 * ================================================================ */
static int cmp_rafaga(const void *a, const void *b) {
    return (*(PCB **)a)->rafaga_total - (*(PCB **)b)->rafaga_total;
}

void planificar_sjf(ReadyQueue *q) {
    printf("\n=== SJF (Shortest Job First) [Prevencion deadlock: recursos liberados antes] ===\n");

    PCB *lista[MAX_PROCESOS];
    int  n = 0;
    while (!queue_vacia(q)) lista[n++] = desencolar(q);
    qsort(lista, n, sizeof(PCB *), cmp_rafaga);

    int t = 0, tat_total = 0, wt_total = 0;
    for (int i = 0; i < n; i++) {
        lista[i]->estado = EJECUCION;
        t += lista[i]->rafaga_total;
        lista[i]->tiempo_fin = t;
        lista[i]->estado     = TERMINADO;

        int tat = t - lista[i]->tiempo_llegada;
        int wt  = tat - lista[i]->rafaga_total;
        printf("  %-12s | rafaga=%2d | fin=%3d | TAT=%3d | WT=%3d\n",
               lista[i]->nombre, lista[i]->rafaga_total, t, tat, wt);
        tat_total += tat;
        wt_total  += wt;
    }
    if (n > 0)
        printf("  Promedio TAT=%.1f  WT=%.1f\n",
               (float)tat_total/n, (float)wt_total/n);
}

/* ================================================================
 * 5D. PRIORIDAD SIMPLE — no preemptivo
 *
 * Ejecuta en orden de prioridad (menor numero = mayor prioridad).
 * Equidad: NO garantiza equidad (procesos de baja prioridad
 *          pueden sufrir inanicion).
 * Deadlock (EVITACION): usa jerarquia de prioridades para ordenar
 *   la asignacion de recursos. Procesos de alta prioridad obtienen
 *   recursos primero, reduciendo la probabilidad de espera circular.
 *   Se complementa con el Algoritmo del Banquero para verificar
 *   que el estado permanezca seguro antes de asignar.
 * Mejora posible: Priority Inheritance para evitar inversion de
 *   prioridad (un proceso de alta prioridad bloqueado hereda su
 *   prioridad al proceso que retiene el recurso necesario).
 * ================================================================ */
static int cmp_prioridad(const void *a, const void *b) {
    return (*(PCB **)a)->prioridad - (*(PCB **)b)->prioridad;
}

void planificar_prioridad(ReadyQueue *q) {
    printf("\n=== PRIORIDAD SIMPLE [Evitacion deadlock: jerarquia de prioridades] ===\n");

    PCB *lista[MAX_PROCESOS];
    int  n = 0;
    while (!queue_vacia(q)) lista[n++] = desencolar(q);
    qsort(lista, n, sizeof(PCB *), cmp_prioridad);

    int t = 0, tat_total = 0, wt_total = 0;
    for (int i = 0; i < n; i++) {
        lista[i]->estado = EJECUCION;
        t += lista[i]->rafaga_total;
        lista[i]->tiempo_fin = t;
        lista[i]->estado     = TERMINADO;

        int tat = t - lista[i]->tiempo_llegada;
        int wt  = tat - lista[i]->rafaga_total;
        printf("  %-12s | prioridad=%d | fin=%3d | TAT=%3d | WT=%3d\n",
               lista[i]->nombre, lista[i]->prioridad, t, tat, wt);
        tat_total += tat;
        wt_total  += wt;
    }
    if (n > 0)
        printf("  Promedio TAT=%.1f  WT=%.1f\n",
               (float)tat_total/n, (float)wt_total/n);
}

/* ================================================================
 * 5E. CFS — Completely Fair Scheduler (BONUS)
 *
 * Simula el scheduler de Linux con un min-heap binario que
 * representa el arbol Rojo-Negro. El proceso con menor vruntime
 * (nodo mas a la izquierda del RBT) siempre se selecciona.
 *
 * Time slice segun nice:
 *   nice < 0 -> 4 unidades  (mayor prioridad)
 *   nice = 0 -> 2 unidades  (normal)
 *   nice > 0 -> 1 unidad    (menor prioridad)
 *
 * Equidad: ningún proceso acumula mas vruntime que otro de forma
 *          injusta -> elimina inanicion.
 * Deadlock (PREVENCION): la preempcion por vruntime impide que
 *   un proceso monopolice la CPU, rompiendo la condicion
 *   "sin preempcion" de Coffman.
 * ================================================================ */
static PCB *cfs_heap[MAX_PROCESOS];
static int  cfs_heap_sz = 0;

static void heap_push(PCB *p) {
    cfs_heap[cfs_heap_sz] = p;
    int i = cfs_heap_sz++;
    while (i > 0) {
        int par = (i - 1) / 2;
        if (cfs_heap[par]->vruntime > cfs_heap[i]->vruntime) {
            PCB *tmp      = cfs_heap[par];
            cfs_heap[par] = cfs_heap[i];
            cfs_heap[i]   = tmp;
            i = par;
        } else break;
    }
}

static PCB *heap_pop() {
    PCB *top      = cfs_heap[0];
    cfs_heap[0]   = cfs_heap[--cfs_heap_sz];
    int i = 0;
    while (1) {
        int l = 2*i+1, r = 2*i+2, mn = i;
        if (l < cfs_heap_sz && cfs_heap[l]->vruntime < cfs_heap[mn]->vruntime) mn = l;
        if (r < cfs_heap_sz && cfs_heap[r]->vruntime < cfs_heap[mn]->vruntime) mn = r;
        if (mn == i) break;
        PCB *tmp    = cfs_heap[i];
        cfs_heap[i] = cfs_heap[mn];
        cfs_heap[mn]= tmp;
        i = mn;
    }
    return top;
}

void planificar_cfs(ReadyQueue *q) {
    printf("\n=== CFS - BONUS (min-heap simula arbol Rojo-Negro) ===\n");

    cfs_heap_sz = 0;
    int total = q->cantidad;
    while (!queue_vacia(q)) heap_push(desencolar(q));

    int t = 0, tat_total = 0, wt_total = 0;

    while (cfs_heap_sz > 0) {
        PCB *p    = heap_pop();
        int slice = (p->nice < 0) ? 4 : (p->nice > 0) ? 1 : 2;
        p->estado = EJECUCION;

        if (p->rafaga_restante <= slice) {
            int ej         = p->rafaga_restante;
            t             += ej;
            p->vruntime   += ej;
            p->rafaga_restante = 0;
            p->tiempo_fin  = t;
            p->estado      = TERMINADO;

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
            p->estado          = LISTO;
            printf("  [t=%3d] %-12s slice=%d   | vruntime=%4ld | restante=%d\n",
                   t, p->nombre, slice, p->vruntime, p->rafaga_restante);
            heap_push(p);
        }
    }
    if (total > 0)
        printf("  Promedio TAT=%.1f  WT=%.1f\n",
               (float)tat_total/total, (float)wt_total/total);
}

/* ================================================================
 * 6. ALGORITMO DEL BANQUERO — Evitacion de Deadlock
 *
 * Verifica si el sistema esta en estado seguro antes de asignar
 * recursos. Si existe una secuencia de finalizacion garantizada
 * -> estado seguro -> se otorga la asignacion.
 * Implementa las matrices: Max, Allocation, Need, Available.
 * ================================================================ */
void algoritmo_banquero() {
    printf("\n=== ALGORITMO DEL BANQUERO (Evitacion de Deadlock) ===\n");

    int available[N_REC]               = {3, 3, 2};
    int max_demand[N_PROC_BAN][N_REC]  = {{7,5,3},{3,2,2},{9,0,2},{2,2,2}};
    int allocation[N_PROC_BAN][N_REC]  = {{0,1,0},{2,0,0},{3,0,2},{2,1,1}};
    int need[N_PROC_BAN][N_REC];

    printf("  Available: [A=%d B=%d C=%d]\n",
           available[0], available[1], available[2]);
    printf("  %-6s  Max       Alloc     Need\n", "Proc");

    for (int i = 0; i < N_PROC_BAN; i++) {
        for (int j = 0; j < N_REC; j++)
            need[i][j] = max_demand[i][j] - allocation[i][j];
        printf("  P%-5d [%d %d %d]   [%d %d %d]   [%d %d %d]\n", i,
               max_demand[i][0], max_demand[i][1], max_demand[i][2],
               allocation[i][0], allocation[i][1], allocation[i][2],
               need[i][0], need[i][1], need[i][2]);
    }

    int  work[N_REC];
    bool finish[N_PROC_BAN];
    int  seq[N_PROC_BAN];
    int  seq_idx = 0;

    for (int j = 0; j < N_REC; j++)          work[j]   = available[j];
    for (int i = 0; i < N_PROC_BAN; i++)     finish[i] = false;

    for (int count = 0; count < N_PROC_BAN; count++) {
        for (int i = 0; i < N_PROC_BAN; i++) {
            if (finish[i]) continue;
            bool puede = true;
            for (int j = 0; j < N_REC; j++)
                if (need[i][j] > work[j]) { puede = false; break; }
            if (puede) {
                for (int j = 0; j < N_REC; j++) work[j] += allocation[i][j];
                finish[i]      = true;
                seq[seq_idx++] = i;
                break;
            }
        }
    }

    bool seguro = true;
    for (int i = 0; i < N_PROC_BAN; i++) if (!finish[i]) { seguro = false; break; }

    if (seguro) {
        printf("\n  ESTADO SEGURO. Secuencia: ");
        for (int i = 0; i < seq_idx; i++) printf("P%d ", seq[i]);
        printf("\n");
    } else {
        printf("\n  ESTADO INSEGURO — asignacion rechazada (posible DEADLOCK).\n");
    }
}

/* ================================================================
 * 7. DETECCION DE DEADLOCK — DFS sobre grafo de asignacion
 *
 * Modela el grafo de asignacion de recursos y busca ciclos
 * mediante DFS con deteccion de back-edges (arco de retorno).
 * Un ciclo en el grafo implica espera circular => DEADLOCK.
 *
 * Escenario del tablero de clase:
 *   P1->R2->P2->R1->P3->R2  (ciclo cerrado)
 * ================================================================ */
static int  solicita_g[3][3];
static int  asignado_g[3][3];
static bool vis_g[N_NODOS];
static bool pila_g[N_NODOS];

static bool dfs(int nodo) {
    vis_g[nodo] = pila_g[nodo] = true;
    if (nodo < 3) {
        for (int r = 0; r < 3; r++) {
            if (!solicita_g[nodo][r]) continue;
            int dst = r + 3;
            if (!vis_g[dst] && dfs(dst)) return true;
            if (pila_g[dst])             return true;
        }
    } else {
        int r = nodo - 3;
        for (int p = 0; p < 3; p++) {
            if (!asignado_g[r][p]) continue;
            if (!vis_g[p] && dfs(p)) return true;
            if (pila_g[p])           return true;
        }
    }
    pila_g[nodo] = false;
    return false;
}

void detectar_deadlock() {
    printf("\n=== DETECCION DE DEADLOCK (DFS en grafo de asignacion) ===\n");

    memset(solicita_g, 0, sizeof(solicita_g));
    memset(asignado_g, 0, sizeof(asignado_g));

    solicita_g[0][1] = 1;  /* P1 solicita R2 */
    solicita_g[1][0] = 1;  /* P2 solicita R1 */
    solicita_g[2][1] = 1;  /* P3 solicita R2 */
    asignado_g[1][1] = 1;  /* R2 asignado a P2 */
    asignado_g[0][2] = 1;  /* R1 asignado a P3 */

    printf("  Grafo: P1->R2->P2->R1->P3->R2\n");

    memset(vis_g,  false, sizeof(vis_g));
    memset(pila_g, false, sizeof(pila_g));
    bool deadlock = false;
    for (int i = 0; i < N_NODOS; i++)
        if (!vis_g[i] && dfs(i)) { deadlock = true; break; }

    printf("  Resultado: %s\n",
           deadlock ? "DEADLOCK DETECTADO (ciclo en grafo)"
                    : "Sin deadlock");
}

/* ================================================================
 * UTILIDADES
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
    p->estado           = NUEVO;
    memset(p->recursos_max,  0, sizeof(p->recursos_max));
    memset(p->recursos_asig, 0, sizeof(p->recursos_asig));
    return p;
}

static void liberar_cola(ReadyQueue *q) {
    while (!queue_vacia(q)) free(desencolar(q));
}

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
    printf(" Parcial 2 - Sistemas Operativos USC 2026A\n");
    printf("=========================================\n");

    ReadyQueue cola;

    /* --- Ciclo de vida de cada proceso --- */
    printf("\n=== CICLO DE VIDA DE LOS PROCESOS ===\n");
    cargar_procesos(&cola);
    ReadyQueue tmp_vida; init_queue(&tmp_vida);
    while (!queue_vacia(&cola)) {
        PCB *p = desencolar(&cola);
        ciclo_de_vida(p);
        encolar(&tmp_vida, p);
    }

    /* --- Dispatcher: ejecutar cada algoritmo via llamar_planificador --- */
    cargar_procesos(&cola);  llamar_planificador(&cola, ALG_FCFS);
    liberar_cola(&cola);

    cargar_procesos(&cola);  llamar_planificador(&cola, ALG_RR);
    liberar_cola(&cola);

    cargar_procesos(&cola);  llamar_planificador(&cola, ALG_SJF);
    liberar_cola(&cola);

    cargar_procesos(&cola);  llamar_planificador(&cola, ALG_PRIORIDAD);
    liberar_cola(&cola);

    cargar_procesos(&cola);  llamar_planificador(&cola, ALG_CFS);
    liberar_cola(&cola);

    /* --- Gestion de interbloqueos --- */
    algoritmo_banquero();
    detectar_deadlock();

    /* Liberar ciclo de vida */
    liberar_cola(&tmp_vida);

    printf("\n=========================================\n");
    printf(" Fin del laboratorio\n");
    printf("=========================================\n");
    return 0;
}
