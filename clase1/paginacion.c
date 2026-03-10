/**
 * lab_segmentacion.c
 * Laboratorio: Simulación de Memoria Virtual con Segmentación
 *
 * Compilar: gcc -ansi -Wall -o lab_seg lab_segmentacion.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_SEGMENTS   8
#define MEM_SIZE   65536

#define SEG_READ    0x01
#define SEG_WRITE   0x02
#define SEG_EXEC    0x04

typedef struct {
    unsigned long base;
    unsigned long limit;
    int valid;
    int perms;
    char name[16];
} SegmentEntry;

SegmentEntry seg_table[MAX_SEGMENTS];
int num_segments = 0;
int total_accesses = 0;
int seg_faults = 0;
int prot_errors = 0;

void init_seg_table(void);
int add_segment(unsigned long base, unsigned long limit, int perms, const char *name);
long translate_seg(int seg_num, unsigned long offset, int access);
void print_seg_table(void);
void print_stats(void);

void init_seg_table(void) {
    int i;
    for (i = 0; i < MAX_SEGMENTS; i++) {
        seg_table[i].valid = 0;
        seg_table[i].base = 0;
        seg_table[i].limit = 0;
        seg_table[i].perms = 0;
        seg_table[i].name[0] = '\0';
    }
    num_segments = 0;
}

int add_segment(unsigned long base, unsigned long limit, int perms, const char *name) {

    int idx;

    if (num_segments >= MAX_SEGMENTS) {
        printf("[ERROR] Tabla de segmentos llena.\n");
        return -1;
    }

    if (limit == 0 || (base + limit) > MEM_SIZE) {
        printf("[ERROR] Parámetros inválidos para segmento '%s'.\n", name);
        return -1;
    }

    idx = num_segments++;

    seg_table[idx].base = base;
    seg_table[idx].limit = limit;
    seg_table[idx].perms = perms;
    seg_table[idx].valid = 1;

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
           seg_num,
           offset,
           (access == SEG_READ) ? "LECTURA" :
           (access == SEG_WRITE) ? "ESCRITURA" : "EJECUCION");

    if (seg_num < 0 || seg_num >= num_segments || !seg_table[seg_num].valid) {
        printf("[SEG FAULT] Segmento %d no existe o es invalido.\n", seg_num);
        seg_faults++;
        return -1;
    }

    if (offset > seg_table[seg_num].limit) {
        printf("[SEG FAULT] Offset 0x%lX > Limite 0x%lX en seg %d ('%s').\n",
               offset,
               seg_table[seg_num].limit,
               seg_num,
               seg_table[seg_num].name);
        seg_faults++;
        return -1;
    }

    if ((seg_table[seg_num].perms & access) == 0) {
        printf("[PROTECCION] Acceso denegado en segmento '%s'.\n",
               seg_table[seg_num].name);
        prot_errors++;
        return -1;
    }

    physical_addr = (long)(seg_table[seg_num].base + offset);

    printf("[OK] Dir. fisica: 0x%05lX (0x%05lX + 0x%05lX)\n",
           physical_addr,
           seg_table[seg_num].base,
           offset);

    return physical_addr;
}

void print_seg_table(void) {

    int i;

    printf("\n+-----+----------+----------+----------+--------+\n");
    printf("| Seg | Nombre   | Base     | Limite   | Perms  |\n");
    printf("+-----+----------+----------+----------+--------+\n");

    for (i = 0; i < num_segments; i++) {

        printf("|  %d  | %-8s | 0x%05lX  | 0x%05lX  |  %c%c%c   |\n",
               i,
               seg_table[i].name,
               seg_table[i].base,
               seg_table[i].limit,
               (seg_table[i].perms & SEG_READ) ? 'R' : '-',
               (seg_table[i].perms & SEG_WRITE) ? 'W' : '-',
               (seg_table[i].perms & SEG_EXEC) ? 'X' : '-');
    }

    printf("+-----+----------+----------+----------+--------+\n");
}

void print_stats(void) {

    printf("\n=== ESTADISTICAS ===\n");

    printf("Accesos totales   : %d\n", total_accesses);
    printf("Seg faults        : %d\n", seg_faults);
    printf("Errores proteccion: %d\n", prot_errors);
    printf("Accesos validos   : %d\n",
           total_accesses - seg_faults - prot_errors);
}

int main(void) {

    printf("=== Laboratorio: Segmentacion - Sistemas Operativos ===\n");

    init_seg_table();

    add_segment(0x4000, 0x0FFF, SEG_READ | SEG_EXEC, "codigo");
    add_segment(0x1000, 0x07FF, SEG_READ | SEG_WRITE, "datos");
    add_segment(0x9000, 0x0FFF, SEG_READ | SEG_WRITE, "pila");

    print_seg_table();

    translate_seg(0, 0x0100, SEG_READ);
    translate_seg(1, 0x0200, SEG_WRITE);
    translate_seg(1, 0x0900, SEG_READ);
    translate_seg(0, 0x0050, SEG_WRITE);
    translate_seg(5, 0x0010, SEG_READ);

    /* Accesos adicionales */

    translate_seg(0, 0x0200, SEG_EXEC);

    translate_seg(2, 0x0100, SEG_READ);
    translate_seg(2, 0x0200, SEG_WRITE);

    translate_seg(2, 0x2000, SEG_WRITE);

    add_segment(0xA000, 0x0FFF, SEG_READ | SEG_WRITE, "heap");

    translate_seg(3, 0x0100, SEG_WRITE);

    print_stats();

    return 0;
}