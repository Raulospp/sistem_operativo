/* perifericos.c
 * Simulacion minima de perifericos de E/S:
 *   - teclado : lectura linea por linea desde stdin (con strip de '\n').
 *   - pantalla: escritura formateada a stdout.
 *   - impresora: cola FIFO con flush diferido (modela un device queue
 *                con scheduler propio del SO).
 */
#include <stdio.h>
#include <string.h>
#include "tipos.h"

static TrabajoImpresion cola_impresora[MAX_IMPRESORA];
static int cola_imp_count = 0;

void init_perifericos(void) {
    int i;
    for (i = 0; i < MAX_IMPRESORA; i++) cola_impresora[i].usado = 0;
    cola_imp_count = 0;
}

void teclado_leer(char *buffer, int max) {
    size_t n;
    if (!fgets(buffer, max, stdin)) { buffer[0] = '\0'; return; }
    n = strlen(buffer);
    if (n > 0 && buffer[n - 1] == '\n') buffer[n - 1] = '\0';
}

void pantalla_escribir(const char *texto) {
    printf("[PANTALLA] %s\n", texto);
}

void impresora_imprimir(int pid, const char *texto) {
    if (cola_imp_count >= MAX_IMPRESORA) {
        printf("[ERR] cola de impresora llena (%d)\n", MAX_IMPRESORA);
        return;
    }
    cola_impresora[cola_imp_count].pid = pid;
    strncpy(cola_impresora[cola_imp_count].texto, texto, 127);
    cola_impresora[cola_imp_count].texto[127] = '\0';
    cola_impresora[cola_imp_count].usado = 1;
    cola_imp_count++;
    printf("[IMPRESORA] PID %d encolo trabajo (%d en cola)\n",
           pid, cola_imp_count);
}

void impresora_flush(void) {
    int i;
    if (cola_imp_count == 0) return;
    printf("\n--- Impresora: ejecutando %d trabajos ---\n", cola_imp_count);
    for (i = 0; i < cola_imp_count; i++) {
        printf("  [IMP %d/%d] PID %d -> %s\n", i + 1, cola_imp_count,
               cola_impresora[i].pid, cola_impresora[i].texto);
        cola_impresora[i].usado = 0;
    }
    cola_imp_count = 0;
    printf("--- Impresora: cola vacia ---\n\n");
}

int impresora_pendientes(void) { return cola_imp_count; }
