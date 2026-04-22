/*
 * fifo_shm.c
 * Universidad Santiago de Cali, 2026A
 * Lab #2 - Modulo 2B: IPC combinando FIFO (named pipe) + Shared Memory
 *
 * Canal de datos: FIFO (el productor escribe pares "a b" como strings)
 * Almacenamiento de resultados: Shared Memory
 * Los dos consumidores leen del FIFO y calculan mul/suma respectivamente.
 *
 * NOTA: ambos consumidores abren el mismo FIFO. En un sistema real
 * competirian por los datos (race condition en el canal). Esta es una
 * simplificacion educativa aceptada por el enunciado.
 *
 * Compilar: gcc -Wall -std=c99 fifo_shm.c -o fifo_demo
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/wait.h>

#define FIFO_FILE "canal_fifo"
#define SHM_KEY   5678
#define BUF_SIZE  128
#define N         5

typedef struct {
    int a;
    int b;
    int resultado_mul;
    int resultado_sum;
} DatosCompartidos;

int main() {
    printf("=== MODULO 2B: IPC FIFO + Shared Memory ===\n\n");

    /* Crear memoria compartida */
    int shm_id = shmget(SHM_KEY, sizeof(DatosCompartidos), IPC_CREAT | 0666);
    if (shm_id < 0) { perror("shmget"); exit(1); }

    DatosCompartidos *mem = (DatosCompartidos *) shmat(shm_id, NULL, 0);
    if (mem == (DatosCompartidos *) -1) { perror("shmat"); exit(1); }

    /* Crear FIFO en el sistema de archivos */
    mkfifo(FIFO_FILE, 0666);

    /* ---- PRODUCTOR ---- */
    pid_t pid_prod = fork();
    if (pid_prod < 0) { perror("fork productor"); exit(1); }

    if (pid_prod == 0) {
        int fd = open(FIFO_FILE, O_WRONLY);
        if (fd < 0) { perror("[Prod] open FIFO"); exit(1); }

        for (int i = 1; i <= N; i++) {
            char buf[BUF_SIZE];
            int a = i, b = i + 2;
            snprintf(buf, BUF_SIZE, "%d %d", a, b);
            write(fd, buf, strlen(buf) + 1);
            printf("[Productor] Enviado: a=%d  b=%d\n", a, b);
            sleep(1);
        }

        close(fd);
        exit(0);
    }

    /* ---- CONSUMIDOR 1: multiplicacion ---- */
    pid_t pid_c1 = fork();
    if (pid_c1 < 0) { perror("fork cons1"); exit(1); }

    if (pid_c1 == 0) {
        int fd = open(FIFO_FILE, O_RDONLY);
        if (fd < 0) { perror("[Cons1] open FIFO"); exit(1); }

        int *resultados = malloc(N * sizeof(int));
        if (!resultados) { perror("malloc cons1"); exit(1); }

        for (int i = 0; i < N; i++) {
            char buf[BUF_SIZE];
            int bytes = read(fd, buf, BUF_SIZE);
            if (bytes <= 0) break;

            int a, b;
            sscanf(buf, "%d %d", &a, &b);

            mem->a = a;
            mem->b = b;
            resultados[i] = a * b;
            mem->resultado_mul = resultados[i];
            printf("[Consumidor1] %d * %d = %d\n", a, b, resultados[i]);
        }

        printf("\n[Consumidor1] Array final (multiplicacion):\n");
        for (int i = 0; i < N; i++) printf("  [%d] = %d\n", i, resultados[i]);

        free(resultados);
        close(fd);
        exit(0);
    }

    /* ---- CONSUMIDOR 2: suma ---- */
    pid_t pid_c2 = fork();
    if (pid_c2 < 0) { perror("fork cons2"); exit(1); }

    if (pid_c2 == 0) {
        int fd = open(FIFO_FILE, O_RDONLY);
        if (fd < 0) { perror("[Cons2] open FIFO"); exit(1); }

        int *resultados = malloc(N * sizeof(int));
        if (!resultados) { perror("malloc cons2"); exit(1); }

        for (int i = 0; i < N; i++) {
            char buf[BUF_SIZE];
            int bytes = read(fd, buf, BUF_SIZE);
            if (bytes <= 0) break;

            int a, b;
            sscanf(buf, "%d %d", &a, &b);

            resultados[i] = a + b;
            mem->resultado_sum = resultados[i];
            printf("[Consumidor2] %d + %d = %d\n", a, b, resultados[i]);
        }

        printf("\n[Consumidor2] Array final (suma):\n");
        for (int i = 0; i < N; i++) printf("  [%d] = %d\n", i, resultados[i]);

        free(resultados);
        close(fd);
        exit(0);
    }

    /* ---- Padre: espera y libera recursos ---- */
    wait(NULL);
    wait(NULL);
    wait(NULL);

    shmdt(mem);
    shmctl(shm_id, IPC_RMID, NULL);
    unlink(FIFO_FILE);

    printf("\n[Padre] Todos los procesos finalizaron. Recursos liberados.\n");
    return 0;
}
