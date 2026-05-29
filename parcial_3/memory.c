/* memory.c
 * Administrador de memoria por particionamiento dinamico con first-fit.
 * Adaptado de parcial_2/03_memoria/particion_dinamica.c
 *
 * Decisiones de diseno:
 *   - First-fit: O(n), simple y suficientemente rapido para 32 bloques.
 *   - Split al asignar (si el bloque libre es mayor a lo pedido se parte).
 *   - Fusion de bloques libres adyacentes al liberar (evita fragmentacion).
 */
#include <stdio.h>
#include <string.h>
#include "tipos.h"

Bloque bloques[MAX_BLOQUES];
int    bloque_count = 0;

void init_memoria(void) {
    int i;
    memset(bloques, 0, sizeof(bloques));
    bloques[0].start     = 0;
    bloques[0].size      = MEM_TOTAL;
    bloques[0].libre     = 1;
    bloques[0].pid_dueno = -1;
    for (i = 1; i < MAX_BLOQUES; i++) bloques[i].pid_dueno = -1;
    bloque_count = 1;
}

/* Divide el bloque i para asignar 'size' al proceso pid */
static void split_y_asignar(int i, int pid, int size) {
    if (bloques[i].size > size && bloque_count < MAX_BLOQUES) {
        int j;
        for (j = bloque_count; j > i + 1; j--) bloques[j] = bloques[j - 1];
        bloques[i + 1].start     = bloques[i].start + size;
        bloques[i + 1].size      = bloques[i].size - size;
        bloques[i + 1].libre     = 1;
        bloques[i + 1].pid_dueno = -1;
        bloques[i].size = size;
        bloque_count++;
    }
    bloques[i].libre     = 0;
    bloques[i].pid_dueno = pid;
}

int asignar_memoria(int pid, int size) {
    int i;
    if (size <= 0) return -1;
    for (i = 0; i < bloque_count; i++) {
        if (bloques[i].libre && bloques[i].size >= size) {
            split_y_asignar(i, pid, size);
            printf("[MEM] PID %d -> bloque %d (inicio=%d tam=%d)\n",
                   pid, i, bloques[i].start, size);
            return i;
        }
    }
    printf("[MEM] sin espacio para PID %d (%d B)\n", pid, size);
    return -1;
}

static void fusionar_libres(void) {
    int i = 0;
    while (i < bloque_count - 1) {
        if (bloques[i].libre && bloques[i + 1].libre) {
            int j;
            bloques[i].size += bloques[i + 1].size;
            for (j = i + 1; j < bloque_count - 1; j++) bloques[j] = bloques[j + 1];
            bloque_count--;
        } else {
            i++;
        }
    }
}

void liberar_memoria(int pid) {
    int i, encontrado = 0;
    for (i = 0; i < bloque_count; i++) {
        if (bloques[i].pid_dueno == pid) {
            bloques[i].libre     = 1;
            bloques[i].pid_dueno = -1;
            encontrado = 1;
        }
    }
    if (encontrado) {
        fusionar_libres();
        printf("[MEM] memoria liberada para PID %d\n", pid);
    }
}

void mostrar_memoria(void) {
    int i;
    printf("\n--- Mapa de memoria (total %d B) ---\n", MEM_TOTAL);
    printf("  #   inicio    tam   estado    PID\n");
    for (i = 0; i < bloque_count; i++) {
        printf("  %-3d %6d  %5d   %-7s   %d\n", i,
               bloques[i].start, bloques[i].size,
               bloques[i].libre ? "LIBRE" : "OCUPADO",
               bloques[i].pid_dueno);
    }
    printf("  Libre: %d B | Usada: %d B\n\n", memoria_libre(), memoria_usada());
}

int memoria_libre(void) {
    int i, sum = 0;
    for (i = 0; i < bloque_count; i++) if (bloques[i].libre) sum += bloques[i].size;
    return sum;
}

int memoria_usada(void) { return MEM_TOTAL - memoria_libre(); }
