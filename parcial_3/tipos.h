/* tipos.h
 * Declaraciones compartidas entre los modulos del Mini-SO.
 * Parcial 3 - Sistemas Operativos USC 2026A
 */
#ifndef TIPOS_H
#define TIPOS_H

#define MAX_PROCESOS     16
#define MAX_BLOQUES      32
#define MEM_TOTAL        1024
#define MAX_FILES        20
#define MAX_NAME         50
#define MAX_CONTENT      256
#define MAX_ESPERA       8
#define MAX_IMPRESORA    16
#define QUANTUM_DEFAULT  2

/* === Estados del proceso === */
typedef enum {
    LIBRE = 0,   /* slot vacio en la tabla de procesos */
    NUEVO,
    LISTO,
    EJECUCION,
    BLOQUEADO,
    TERMINADO
} EstadoProceso;

/* === PCB simplificado === */
typedef struct {
    int  pid;
    char nombre[32];
    EstadoProceso estado;
    int  prioridad;          /* menor valor = mayor prioridad */
    int  rafaga_total;       /* CPU simulada */
    int  rafaga_restante;
    int  memoria_asignada;   /* bytes solicitados, 0 si no */
    int  archivo_esperado;   /* indice en disco[] o -1 */
} PCB;

/* === Bloque de memoria (particionamiento dinamico) === */
typedef struct {
    int start;
    int size;
    int pid_dueno;           /* -1 si libre */
    int libre;               /* 1 libre, 0 ocupado */
} Bloque;

/* === Archivo en RAM === */
typedef struct {
    char nombre[MAX_NAME];
    char contenido[MAX_CONTENT];
    int  tamanio;
    int  usado;                       /* slot ocupado */
    int  pid_dueno;                   /* -1 si nadie lo tiene abierto */
    int  cola_espera[MAX_ESPERA];     /* PIDs en espera */
    int  cola_count;
} Archivo;

/* === Trabajo de impresion === */
typedef struct {
    int  pid;
    char texto[128];
    int  usado;
} TrabajoImpresion;

/* === memory.c === */
void init_memoria(void);
int  asignar_memoria(int pid, int size);   /* devuelve indice o -1 */
void liberar_memoria(int pid);
void mostrar_memoria(void);
int  memoria_libre(void);
int  memoria_usada(void);
extern Bloque bloques[];
extern int    bloque_count;

/* === processes.c === */
void init_procesos(void);
int  crear_proceso(const char *nombre, int prioridad, int mem_solicitada);
int  terminar_proceso(int pid);
void bloquear_proceso(int pid, int archivo_idx);
void desbloquear_proceso(int pid);
PCB *buscar_proceso(int pid);
void mostrar_procesos(void);
void planificar_fcfs(void);
void planificar_rr(int quantum);
int  contar_procesos_estado(EstadoProceso e);
const char *nombre_estado(EstadoProceso e);
extern PCB procesos[];

/* === files.c === */
void init_filesystem(void);
int  crear_archivo(const char *nombre);
int  abrir_archivo(int pid, const char *nombre);
int  escribir_archivo(int pid, const char *nombre, const char *texto);
int  leer_archivo(int pid, const char *nombre);
int  cerrar_archivo(int pid, const char *nombre);
int  eliminar_archivo(int pid, const char *nombre);
void liberar_archivos_de(int pid);
void listar_archivos(void);
int  contar_archivos_usados(void);
extern Archivo disco[];

/* === perifericos.c === */
void init_perifericos(void);
void teclado_leer(char *buffer, int max);
void pantalla_escribir(const char *texto);
void impresora_imprimir(int pid, const char *texto);
void impresora_flush(void);
int  impresora_pendientes(void);

/* === kernel.c === */
void dump_estado_json(void);

#endif
