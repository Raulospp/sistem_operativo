/*
 * particion_fija.c
 * Universidad Santiago de Cali, 2026A
 * Lab #2 - Modulo 3A: Particionamiento Fijo de Memoria
 *
 * Divide MEMORY_SIZE bytes en PARTITIONS bloques iguales.
 * Asignacion: first-fit secuencial.
 *
 * Compilar: gcc -Wall -std=c99 particion_fija.c -o pfija
 */

#include <stdio.h>
#include <stdbool.h>

#define MEMORY_SIZE 1024
#define PARTITIONS  4
#define PART_SIZE   (MEMORY_SIZE / PARTITIONS)  /* 256 bytes por particion */

typedef struct {
    int  process_id;
    int  size;
    bool occupied;
} Partition;

Partition memory[PARTITIONS];

void initalize_mem() {
    for (int i = 0; i < PARTITIONS; i++) {
        memory[i].process_id = -1;
        memory[i].size       = 0;
        memory[i].occupied   = false;
    }
}

void print_mem() {
    printf("\nEstado de memoria (%d particiones x %d bytes):\n", PARTITIONS, PART_SIZE);
    for (int i = 0; i < PARTITIONS; i++) {
        if (memory[i].occupied)
            printf("  Particion %d | Proceso %d | %d bytes\n",
                   i, memory[i].process_id, memory[i].size);
        else
            printf("  Particion %d | LIBRE\n", i);
    }
}

void allocate_mem(int pid, int size) {
    if (size > PART_SIZE) {
        printf("\n[ERROR] El proceso %d solicita %d bytes > tamano de particion (%d)\n",
               pid, size, PART_SIZE);
        return;
    }

    for (int i = 0; i < PARTITIONS; i++) {
        if (!memory[i].occupied) {
            memory[i].process_id = pid;
            memory[i].size       = size;
            memory[i].occupied   = true;
            printf("[Alloc] Proceso %d asignado a particion %d (%d bytes)\n", pid, i, size);
            return;
        }
    }

    printf("[ERROR] No hay particiones libres para el proceso %d\n", pid);
}

void free_mem(int pid) {
    for (int i = 0; i < PARTITIONS; i++) {
        if (memory[i].process_id == pid) {  /* fix: condicion correcta */
            memory[i].process_id = -1;
            memory[i].size       = 0;
            memory[i].occupied   = false;
            printf("[Free] Proceso %d liberado de particion %d\n", pid, i);
            return;
        }
    }
    printf("[WARN] Proceso %d no encontrado en memoria\n", pid);
}

int main() {
    printf("=== MODULO 3A: Particionamiento Fijo ===\n");

    initalize_mem();

    allocate_mem(1, 200);
    allocate_mem(2, 150);
    allocate_mem(3, 250);

    print_mem();

    free_mem(2);

    print_mem();

    allocate_mem(4, 100);

    print_mem();

    return 0;
}
