#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* ============================================================
   PARTICIONAMIENTO FIJO
   ============================================================ */
#define NUM_PARTITIONS  4

typedef struct {
    int start;
    int size;
    int process_id;
    bool free;
} FixedBlock;

FixedBlock fixed_blocks[NUM_PARTITIONS];

/* Particiones de tamaño diferente: 128, 256, 512, 128 = 1024 bytes total */
int partition_sizes[NUM_PARTITIONS] = {128, 256, 512, 128};

void initialize_fixed_memory() {
    int offset = 0;
    for (int i = 0; i < NUM_PARTITIONS; i++) {
        fixed_blocks[i].start      = offset;
        fixed_blocks[i].size       = partition_sizes[i];
        fixed_blocks[i].free       = true;
        fixed_blocks[i].process_id = -1;
        offset += partition_sizes[i];
    }
}

void print_fixed_memory() {
    printf("\nEstado de memoria (Particionamiento Fijo):\n");
    for (int i = 0; i < NUM_PARTITIONS; i++) {
        if (fixed_blocks[i].free)
            printf("Particion %d | Inicio: %d | Tamanio: %d | Libre\n",
                   i, fixed_blocks[i].start, fixed_blocks[i].size);
        else {
            int fragmentacion = fixed_blocks[i].size - fixed_blocks[i].process_id;
            /* Guardamos el tamaño del proceso por separado, usamos campo extra */
            printf("Particion %d | Inicio: %d | Tamanio: %d | Ocupado por Proceso %d\n",
                   i, fixed_blocks[i].start, fixed_blocks[i].size, fixed_blocks[i].process_id);
            (void)fragmentacion;
        }
    }
}

int process_sizes[NUM_PARTITIONS]; /* guarda el tamaño real de cada proceso asignado */

void allocate_fixed(int pid, int size) {
    for (int i = 0; i < NUM_PARTITIONS; i++) {
        if (fixed_blocks[i].free && fixed_blocks[i].size >= size) {
            fixed_blocks[i].free       = false;
            fixed_blocks[i].process_id = pid;
            process_sizes[i]           = size;
            int desperdicio            = fixed_blocks[i].size - size;
            printf("Proceso %d (%d bytes) -> Particion %d (%d bytes) | Fragmentacion interna: %d bytes\n",
                   pid, size, i, fixed_blocks[i].size, desperdicio);
            return;
        }
    }
    printf("No hay particion disponible para el proceso %d (tamanio %d bytes).\n", pid, size);
}

void free_fixed_block(int pid) {
    for (int i = 0; i < NUM_PARTITIONS; i++) {
        if (fixed_blocks[i].process_id == pid) {
            fixed_blocks[i].free       = true;
            fixed_blocks[i].process_id = -1;
            process_sizes[i]           = 0;
            printf("Proceso %d liberado de Particion %d.\n", pid, i);
            return;
        }
    }
    printf("Proceso %d no encontrado.\n", pid);
}

void run_particionamiento_fijo() {
    initialize_fixed_memory();

    printf("\n[Memoria inicial - particiones: 128 | 256 | 512 | 128 bytes]\n");
    print_fixed_memory();

    printf("\n[Asignando procesos...]\n");
    allocate_fixed(1, 100);   /* entra en particion 0 (128), desperdicia 28  */
    allocate_fixed(2, 200);   /* entra en particion 1 (256), desperdicia 56  */
    allocate_fixed(3, 450);   /* entra en particion 2 (512), desperdicia 62  */
    allocate_fixed(4, 130);   /* no cabe en particion 3 (128), falla          */
    print_fixed_memory();

    printf("\n[Liberando proceso 2...]\n");
    free_fixed_block(2);
    print_fixed_memory();
}

/* ============================================================
   PARTICIONAMIENTO DINAMICO - ESTRUCTURAS Y FUNCIONES COMUNES
   ============================================================ */
#define MEMORY_SIZE 1024
#define MAX_BLOCKS  20

typedef struct {
    int  start;
    int  size;
    int  process_id;
    bool free;
} Block;

Block blocks[MAX_BLOCKS];
int   block_count = 1;

void initialize_memory() {
    blocks[0].start      = 0;
    blocks[0].size       = MEMORY_SIZE;
    blocks[0].free       = true;
    blocks[0].process_id = -1;
    block_count          = 1;
}

void print_memory() {
    printf("\nEstado de memoria:\n");
    for (int i = 0; i < block_count; i++) {
        if (blocks[i].free)
            printf("Bloque %d | Inicio: %d | Tamanio: %d | Libre\n",
                   i, blocks[i].start, blocks[i].size);
        else
            printf("Bloque %d | Inicio: %d | Tamanio: %d | Ocupado por Proceso %d\n",
                   i, blocks[i].start, blocks[i].size, blocks[i].process_id);
    }
}

void free_block(int pid) {
    for (int i = 0; i < block_count; i++)
        if (blocks[i].process_id == pid) {
            blocks[i].free       = true;
            blocks[i].process_id = -1;
        }
}

void compact_memory() {
    int new_start = 0;
    for (int i = 0; i < block_count; i++)
        if (!blocks[i].free) { blocks[i].start = new_start; new_start += blocks[i].size; }

    int index = 0;
    for (int i = 0; i < block_count; i++)
        if (!blocks[i].free) blocks[index++] = blocks[i];

    blocks[index].start      = new_start;
    blocks[index].size       = MEMORY_SIZE - new_start;
    blocks[index].free       = true;
    blocks[index].process_id = -1;
    block_count              = index + 1;
    printf("\nMemoria compactada.\n");
}

/* ============================================================
   FIRST FIT
   ============================================================ */
void allocate_first_fit(int pid, int size) {
    for (int i = 0; i < block_count; i++) {
        if (blocks[i].free && blocks[i].size >= size) {
            if (blocks[i].size > size) {
                for (int j = block_count; j > i; j--) blocks[j] = blocks[j - 1];
                blocks[i + 1].start      = blocks[i].start + size;
                blocks[i + 1].size       = blocks[i].size  - size;
                blocks[i + 1].free       = true;
                blocks[i + 1].process_id = -1;
                blocks[i].size           = size;
                block_count++;
            }
            blocks[i].free       = false;
            blocks[i].process_id = pid;
            return;
        }
    }
    printf("No hay espacio suficiente.\n");
}

void run_first_fit() {
    initialize_memory();
    allocate_first_fit(1, 200);
    allocate_first_fit(2, 300);
    allocate_first_fit(3, 100);
    print_memory();
    free_block(2);
    print_memory();
    compact_memory();
    print_memory();
}

/* ============================================================
   WORST FIT
   ============================================================ */
void allocate_worst_fit(int pid, int size) {
    int worst_index = -1, worst_size = -1;
    for (int i = 0; i < block_count; i++)
        if (blocks[i].free && blocks[i].size >= size && blocks[i].size > worst_size) {
            worst_size = blocks[i].size; worst_index = i;
        }
    if (worst_index == -1) { printf("No hay espacio suficiente.\n"); return; }
    if (blocks[worst_index].size > size) {
        for (int j = block_count; j > worst_index; j--) blocks[j] = blocks[j - 1];
        blocks[worst_index + 1].start      = blocks[worst_index].start + size;
        blocks[worst_index + 1].size       = blocks[worst_index].size  - size;
        blocks[worst_index + 1].free       = true;
        blocks[worst_index + 1].process_id = -1;
        blocks[worst_index].size           = size;
        block_count++;
    }
    blocks[worst_index].free       = false;
    blocks[worst_index].process_id = pid;
}

void run_worst_fit() {
    initialize_memory();
    allocate_worst_fit(1, 200);
    allocate_worst_fit(2, 300);
    allocate_worst_fit(3, 100);
    print_memory();
}

/* ============================================================
   BEST FIT
   ============================================================ */
void allocate_best_fit(int pid, int size) {
    int best_index = -1, best_size = MEMORY_SIZE + 1;
    for (int i = 0; i < block_count; i++)
        if (blocks[i].free && blocks[i].size >= size && blocks[i].size < best_size) {
            best_size = blocks[i].size; best_index = i;
        }
    if (best_index == -1) { printf("No hay espacio suficiente.\n"); return; }
    if (blocks[best_index].size > size) {
        for (int j = block_count; j > best_index; j--) blocks[j] = blocks[j - 1];
        blocks[best_index + 1].start      = blocks[best_index].start + size;
        blocks[best_index + 1].size       = blocks[best_index].size  - size;
        blocks[best_index + 1].free       = true;
        blocks[best_index + 1].process_id = -1;
        blocks[best_index].size           = size;
        block_count++;
    }
    blocks[best_index].free       = false;
    blocks[best_index].process_id = pid;
}

void run_best_fit() {
    initialize_memory();
    allocate_best_fit(1, 200);
    allocate_best_fit(2, 300);
    allocate_best_fit(3, 100);
    print_memory();
}

/* ============================================================
   SEGMENTACION
   ============================================================ */
#define MAX_SEGMENTS  8
#define MEM_SIZE      65536
#define SEG_READ      0x01
#define SEG_WRITE     0x02
#define SEG_EXEC      0x04

typedef struct {
    unsigned long base;
    unsigned long limit;
    int           valid;
    int           perms;
    char          name[16];
} SegmentEntry;

SegmentEntry seg_table[MAX_SEGMENTS];
int num_segments   = 0;
int total_accesses = 0;
int seg_faults     = 0;
int prot_errors    = 0;

void init_seg_table(void) {
    int i;
    for (i = 0; i < MAX_SEGMENTS; i++) {
        seg_table[i].valid = 0; seg_table[i].base = 0;
        seg_table[i].limit = 0; seg_table[i].perms = 0;
        seg_table[i].name[0] = '\0';
    }
    num_segments = 0; total_accesses = 0; seg_faults = 0; prot_errors = 0;
}

int add_segment(unsigned long base, unsigned long limit, int perms, const char *name) {
    int idx;
    if (num_segments >= MAX_SEGMENTS) { printf("[ERROR] Tabla de segmentos llena.\n"); return -1; }
    if (limit == 0 || (base + limit) > MEM_SIZE) {
        printf("[ERROR] Parametros invalidos para segmento '%s'.\n", name); return -1;
    }
    idx = num_segments++;
    seg_table[idx].base = base; seg_table[idx].limit = limit;
    seg_table[idx].perms = perms; seg_table[idx].valid = 1;
    snprintf(seg_table[idx].name, sizeof(seg_table[idx].name), "%s", name);
    printf("[SEG] Seg %d '%s': base=0x%05lX, lim=0x%05lX, perms=%c%c%c\n",
           idx, name, base, limit,
           (perms & SEG_READ) ? 'R' : '-',
           (perms & SEG_WRITE) ? 'W' : '-',
           (perms & SEG_EXEC) ? 'X' : '-');
    return idx;
}

long translate_seg(int seg_num, unsigned long offset, int access) {
    long physical_addr;
    total_accesses++;
    printf("\n[TRADUCCION] Seg=%d, Offset=0x%05lX, Acceso=%s\n",
           seg_num, offset,
           (access == SEG_READ) ? "LECTURA" :
           (access == SEG_WRITE) ? "ESCRITURA" : "EJECUCION");
    if (seg_num < 0 || seg_num >= num_segments || !seg_table[seg_num].valid) {
        printf("[SEG FAULT] Segmento %d no existe o es invalido.\n", seg_num);
        seg_faults++; return -1;
    }
    if (offset > seg_table[seg_num].limit) {
        printf("[SEG FAULT] Offset 0x%lX > Limite 0x%lX en seg %d ('%s').\n",
               offset, seg_table[seg_num].limit, seg_num, seg_table[seg_num].name);
        seg_faults++; return -1;
    }
    if ((seg_table[seg_num].perms & access) == 0) {
        printf("[PROTECCION] Acceso denegado en segmento '%s'.\n", seg_table[seg_num].name);
        prot_errors++; return -1;
    }
    physical_addr = (long)(seg_table[seg_num].base + offset);
    printf("[OK] Dir. fisica: 0x%05lX (0x%05lX + 0x%05lX)\n",
           physical_addr, seg_table[seg_num].base, offset);
    return physical_addr;
}

void print_seg_table(void) {
    int i;
    printf("\n+-----+----------+----------+----------+--------+\n");
    printf("| Seg | Nombre   | Base     | Limite   | Perms  |\n");
    printf("+-----+----------+----------+----------+--------+\n");
    for (i = 0; i < num_segments; i++)
        printf("|  %d  | %-8s | 0x%05lX  | 0x%05lX  |  %c%c%c   |\n",
               i, seg_table[i].name, seg_table[i].base, seg_table[i].limit,
               (seg_table[i].perms & SEG_READ)  ? 'R' : '-',
               (seg_table[i].perms & SEG_WRITE) ? 'W' : '-',
               (seg_table[i].perms & SEG_EXEC)  ? 'X' : '-');
    printf("+-----+----------+----------+----------+--------+\n");
}

void print_stats(void) {
    printf("\n=== ESTADISTICAS ===\n");
    printf("Accesos totales   : %d\n", total_accesses);
    printf("Seg faults        : %d\n", seg_faults);
    printf("Errores proteccion: %d\n", prot_errors);
    printf("Accesos validos   : %d\n", total_accesses - seg_faults - prot_errors);
}

void run_segmentacion() {
    printf("=== Laboratorio: Segmentacion - Sistemas Operativos ===\n");
    init_seg_table();
    add_segment(0x4000, 0x0FFF, SEG_READ | SEG_EXEC,  "codigo");
    add_segment(0x1000, 0x07FF, SEG_READ | SEG_WRITE, "datos");
    add_segment(0x9000, 0x0FFF, SEG_READ | SEG_WRITE, "pila");
    print_seg_table();
    translate_seg(0, 0x0100, SEG_READ);
    translate_seg(1, 0x0200, SEG_WRITE);
    translate_seg(1, 0x0900, SEG_READ);
    translate_seg(0, 0x0050, SEG_WRITE);
    translate_seg(5, 0x0010, SEG_READ);
    translate_seg(0, 0x0200, SEG_EXEC);
    translate_seg(2, 0x0100, SEG_READ);
    translate_seg(2, 0x0200, SEG_WRITE);
    translate_seg(2, 0x2000, SEG_WRITE);
    add_segment(0xA000, 0x0FFF, SEG_READ | SEG_WRITE, "heap");
    translate_seg(3, 0x0100, SEG_WRITE);
    print_stats();
}

/* ============================================================
   MENU PRINCIPAL
   ============================================================ */
int main() {
    int opcion;
    do {
        printf("\n========================================\n");
        printf("   SIMULADOR DE GESTION DE MEMORIA\n");
        printf("========================================\n");
        printf("1. Particionamiento Fijo\n");
        printf("2. Particionamiento Dinamico - First Fit\n");
        printf("3. Particionamiento Dinamico - Worst Fit\n");
        printf("4. Particionamiento Dinamico - Best Fit\n");
        printf("5. Segmentacion\n");
        printf("0. Salir\n");
        printf("Opcion: ");
        scanf("%d", &opcion);

        switch (opcion) {
            case 1:
                printf("\nSeleccionaste: Particionamiento Fijo\n");
                printf("----------------------------------------\n");
                run_particionamiento_fijo();
                printf("\n----------------------------------------\n");
                printf("Presiona Enter para volver al menu principal...");
                getchar(); getchar();
                break;
            case 2:
                printf("\nSeleccionaste: Particionamiento Dinamico - First Fit\n");
                printf("----------------------------------------\n");
                run_first_fit();
                printf("\n----------------------------------------\n");
                printf("Presiona Enter para volver al menu principal...");
                getchar(); getchar();
                break;
            case 3:
                printf("\nSeleccionaste: Particionamiento Dinamico - Worst Fit\n");
                printf("----------------------------------------\n");
                run_worst_fit();
                printf("\n----------------------------------------\n");
                printf("Presiona Enter para volver al menu principal...");
                getchar(); getchar();
                break;
            case 4:
                printf("\nSeleccionaste: Particionamiento Dinamico - Best Fit\n");
                printf("----------------------------------------\n");
                run_best_fit();
                printf("\n----------------------------------------\n");
                printf("Presiona Enter para volver al menu principal...");
                getchar(); getchar();
                break;
            case 5:
                printf("\nSeleccionaste: Segmentacion\n");
                printf("----------------------------------------\n");
                run_segmentacion();
                printf("\n----------------------------------------\n");
                printf("Presiona Enter para volver al menu principal...");
                getchar(); getchar();
                break;
            case 0: printf("Hasta luego.\n"); break;
            default: printf("Opcion invalida.\n");
        }
    } while (opcion != 0);

    return 0;
}