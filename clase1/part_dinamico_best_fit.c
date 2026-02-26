#include <stdio.h>
#include <stdbool.h>

#define MEMORY_SIZE 1024
#define MAX_BLOCKS 20

typedef struct {
    int start;
    int size;
    int process_id;
    bool free;
} Block;

Block blocks[MAX_BLOCKS];
int block_count = 1;

void initialize_memory() {
    blocks[0].start = 0;
    blocks[0].size = MEMORY_SIZE;
    blocks[0].free = true;
    blocks[0].process_id = -1;
}

void print_memory() {
    printf("\nEstado de memoria:\n");
    for (int i = 0; i < block_count; i++) {
        printf("Bloque %d | Inicio: %d | Tamaño: %d | %s\n",
               i,
               blocks[i].start,
               blocks[i].size,
               blocks[i].free ? "Libre" : "Ocupado");
    }
}

void allocate_best_fit(int pid, int size) {
    int best_index = -1;
    int best_size = MEMORY_SIZE + 1;

    for (int i = 0; i < block_count; i++) {
        if (blocks[i].free && blocks[i].size >= size) {
            if (blocks[i].size < best_size) {
                best_size = blocks[i].size;
                best_index = i;
            }
        }
    }

    if (best_index == -1) {
        printf("No hay espacio suficiente.\n");
        return;
    }

    if (blocks[best_index].size > size) {
        for (int j = block_count; j > best_index; j--)
            blocks[j] = blocks[j - 1];

        blocks[best_index + 1].start = blocks[best_index].start + size;
        blocks[best_index + 1].size = blocks[best_index].size - size;
        blocks[best_index + 1].free = true;
        blocks[best_index + 1].process_id = -1;

        blocks[best_index].size = size;
        block_count++;
    }

    blocks[best_index].free = false;
    blocks[best_index].process_id = pid;
}

int main() {
    initialize_memory();

    allocate_best_fit(1, 200);
    allocate_best_fit(2, 300);
    allocate_best_fit(3, 100);

    print_memory();

    return 0;
}