/*
 * particion_dinamica.c
 * Universidad Santiago de Cali, 2026A
 * Lab #2 - Modulo 3B: Particionamiento Dinamico de Memoria
 *
 * Implementa tres estrategias de asignacion:
 *   - First Fit: primer bloque libre suficiente
 *   - Best Fit : bloque libre mas pequeno que cabe
 *   - Worst Fit: bloque libre mas grande disponible
 * Incluye compactacion de memoria para reducir fragmentacion externa.
 *
 * Compilar: gcc -Wall -std=c99 particion_dinamica.c -o pdinamica
 */

#include <stdio.h>
#include <stdbool.h>
#include <string.h>

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
    block_count = 1;
}

void print_memory() {
    printf("\nEstado de memoria:\n");
    for (int i = 0; i < block_count; i++) {
        if (blocks[i].free)
            printf("  Bloque %d | inicio=%d | tam=%d | LIBRE\n",
                   i, blocks[i].start, blocks[i].size);
        else
            printf("  Bloque %d | inicio=%d | tam=%d | Proceso %d\n",
                   i, blocks[i].start, blocks[i].size, blocks[i].process_id);
    }
}

/* Divide el bloque i, asignando 'size' bytes al proceso pid */
static void split_and_assign(int i, int pid, int size) {
    if (blocks[i].size > size) {
        /* desplazar bloques hacia adelante para insertar el fragmento libre */
        for (int j = block_count; j > i + 1; j--)
            blocks[j] = blocks[j - 1];

        blocks[i + 1].start      = blocks[i].start + size;
        blocks[i + 1].size       = blocks[i].size - size;
        blocks[i + 1].free       = true;
        blocks[i + 1].process_id = -1;
        blocks[i].size = size;
        block_count++;
    }
    blocks[i].free       = false;
    blocks[i].process_id = pid;
}

/* First Fit */
void allocate_first_fit(int pid, int size) {
    for (int i = 0; i < block_count; i++) {
        if (blocks[i].free && blocks[i].size >= size) {
            split_and_assign(i, pid, size);
            printf("[FirstFit] Proceso %d -> bloque %d (inicio=%d)\n", pid, i, blocks[i].start);
            return;
        }
    }
    printf("[FirstFit] Sin espacio para proceso %d (%d bytes)\n", pid, size);
}

/* Best Fit */
void allocate_best_fit(int pid, int size) {
    int best = -1;
    for (int i = 0; i < block_count; i++) {
        if (blocks[i].free && blocks[i].size >= size) {
            if (best == -1 || blocks[i].size < blocks[best].size)
                best = i;
        }
    }
    if (best == -1) {
        printf("[BestFit] Sin espacio para proceso %d (%d bytes)\n", pid, size);
        return;
    }
    split_and_assign(best, pid, size);
    printf("[BestFit] Proceso %d -> bloque %d (inicio=%d)\n", pid, best, blocks[best].start);
}

/* Worst Fit */
void allocate_worst_fit(int pid, int size) {
    int worst = -1;
    for (int i = 0; i < block_count; i++) {
        if (blocks[i].free && blocks[i].size >= size) {
            if (worst == -1 || blocks[i].size > blocks[worst].size)
                worst = i;
        }
    }
    if (worst == -1) {
        printf("[WorstFit] Sin espacio para proceso %d (%d bytes)\n", pid, size);
        return;
    }
    split_and_assign(worst, pid, size);
    printf("[WorstFit] Proceso %d -> bloque %d (inicio=%d)\n", pid, worst, blocks[worst].start);
}

void free_block(int pid) {
    for (int i = 0; i < block_count; i++) {
        if (blocks[i].process_id == pid) {
            blocks[i].free       = true;
            blocks[i].process_id = -1;
            printf("[Free] Proceso %d liberado\n", pid);
        }
    }
}

void compact_memory() {
    int new_start = 0;
    int index     = 0;

    /* mover bloques ocupados al frente */
    for (int i = 0; i < block_count; i++) {
        if (!blocks[i].free) {
            blocks[index]       = blocks[i];
            blocks[index].start = new_start;
            new_start += blocks[index].size;
            index++;
        }
    }

    /* un unico bloque libre al final */
    blocks[index].start      = new_start;
    blocks[index].size       = MEMORY_SIZE - new_start;
    blocks[index].free       = true;
    blocks[index].process_id = -1;
    block_count = index + 1;

    printf("[Compact] Memoria compactada. Libre desde %d (%d bytes)\n",
           new_start, MEMORY_SIZE - new_start);
}

int main() {
    printf("=== MODULO 3B: Particionamiento Dinamico ===\n");

    /* --- First Fit demo --- */
    printf("\n-- First Fit --\n");
    initialize_memory();
    allocate_first_fit(1, 200);
    allocate_first_fit(2, 300);
    allocate_first_fit(3, 100);
    print_memory();
    free_block(2);
    print_memory();
    compact_memory();
    print_memory();

    /* --- Best Fit demo --- */
    printf("\n-- Best Fit --\n");
    initialize_memory();
    allocate_best_fit(1, 200);
    allocate_best_fit(2, 300);
    allocate_best_fit(3, 100);
    print_memory();
    free_block(1);
    allocate_best_fit(4, 150);
    print_memory();

    /* --- Worst Fit demo --- */
    printf("\n-- Worst Fit --\n");
    initialize_memory();
    allocate_worst_fit(1, 200);
    allocate_worst_fit(2, 300);
    allocate_worst_fit(3, 100);
    print_memory();
    free_block(1);
    allocate_worst_fit(4, 150);
    print_memory();

    return 0;
}
