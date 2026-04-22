/*
 * memoria_compartida.c
 * Universidad Santiago de Cali, 2026A
 * Lab #2 - Modulo 2A: IPC con Memoria Compartida (shmget/shmat)
 *
 * Patron productor-consumidor con 3 procesos:
 *   - Productor: genera N pares (a, b)
 *   - Consumidor1: calcula a * b
 *   - Consumidor2: calcula a + b
 * Sincronizacion manual via campo 'listo' (semaforo simulado).
 *
 * Compilar: gcc -Wall -std=c99 memoria_compartida.c -o shm_demo
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/wait.h>

#define SHM_KEY 1234
#define N 5

/*
 * Estructura en memoria compartida.
 * listo: 0 = libre, 1 = productor escribio (cons1 puede leer),
 *        2 = cons1 proceso (cons2 puede leer)
 */
typedef struct {
    int a;
    int b;
    int resultado_mul;
    int resultado_sum;
    int listo;
} DatosCompartidos;

int main() {
    printf("=== MODULO 2A: IPC Memoria Compartida ===\n\n");

    /* Crear segmento de memoria compartida */
    int shm_id = shmget(SHM_KEY, sizeof(DatosCompartidos), IPC_CREAT | 0666);
    if (shm_id < 0) { perror("shmget"); exit(1); }

    DatosCompartidos *mem = (DatosCompartidos *) shmat(shm_id, NULL, 0);
    if (mem == (DatosCompartidos *) -1) { perror("shmat"); exit(1); }

    mem->listo = 0;

    /* ---- PRODUCTOR ---- */
    pid_t pid_prod = fork();
    if (pid_prod < 0) { perror("fork productor"); exit(1); }

    if (pid_prod == 0) {
        for (int i = 1; i <= N; i++) {
            /* esperar a que los consumidores liberen el slot */
            while (mem->listo != 0) usleep(50000);

            mem->a = i;
            mem->b = i + 2;
            mem->listo = 1;
            printf("[Productor] Par generado: a=%d  b=%d\n", mem->a, mem->b);
            sleep(1);
        }
        exit(0);
    }

    /* ---- CONSUMIDOR 1: multiplicacion ---- */
    pid_t pid_c1 = fork();
    if (pid_c1 < 0) { perror("fork cons1"); exit(1); }

    if (pid_c1 == 0) {
        int *resultados = malloc(N * sizeof(int));
        if (!resultados) { perror("malloc cons1"); exit(1); }

        for (int i = 0; i < N; i++) {
            while (mem->listo != 1) usleep(50000);

            resultados[i] = mem->a * mem->b;
            mem->resultado_mul = resultados[i];
            printf("[Consumidor1] %d * %d = %d\n", mem->a, mem->b, resultados[i]);
            mem->listo = 2; /* ceder turno a cons2 */
        }

        printf("\n[Consumidor1] Resultados finales (mul):\n");
        for (int i = 0; i < N; i++) printf("  [%d] = %d\n", i, resultados[i]);

        free(resultados);
        exit(0);
    }

    /* ---- CONSUMIDOR 2: suma ---- */
    pid_t pid_c2 = fork();
    if (pid_c2 < 0) { perror("fork cons2"); exit(1); }

    if (pid_c2 == 0) {
        int *resultados = malloc(N * sizeof(int));
        if (!resultados) { perror("malloc cons2"); exit(1); }

        for (int i = 0; i < N; i++) {
            while (mem->listo != 2) usleep(50000);

            resultados[i] = mem->a + mem->b;
            mem->resultado_sum = resultados[i];
            printf("[Consumidor2] %d + %d = %d\n", mem->a, mem->b, resultados[i]);
            mem->listo = 0; /* liberar para productor */
        }

        printf("\n[Consumidor2] Resultados finales (sum):\n");
        for (int i = 0; i < N; i++) printf("  [%d] = %d\n", i, resultados[i]);

        free(resultados);
        exit(0);
    }

    /* ---- Padre: espera los 3 hijos y libera recursos ---- */
    wait(NULL);
    wait(NULL);
    wait(NULL);

    shmdt(mem);
    shmctl(shm_id, IPC_RMID, NULL);
    printf("\n[Padre] Memoria compartida liberada. Fin.\n");
    return 0;
}
