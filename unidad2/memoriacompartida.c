#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/wait.h>

#define SHM_KEY 1234
#define N 5   // cantidad de pares

typedef struct {
    int a;
    int b;
    int resultado_mul;
    int resultado_sum;
    int listo; // 0: no listo, 1: listo para consumir
} DatosCompartidos;

int main() {
    int shm_id;
    DatosCompartidos *mem;
    pid_t pid_prod, pid_cons1, pid_cons2;

    // Crear memoria compartida
    shm_id = shmget(SHM_KEY, sizeof(DatosCompartidos), IPC_CREAT | 0666);
    if (shm_id < 0) {
        perror("Error creando memoria compartida");
        exit(1);
    }

    mem = (DatosCompartidos *) shmat(shm_id, NULL, 0);
    if (mem == (DatosCompartidos *) -1) {
        perror("Error asociando memoria compartida");
        exit(1);
    }

    mem->listo = 0; // inicializar estado

    pid_prod = fork();

    if (pid_prod == 0) {
        // Productor
        for (int i = 1; i <= N; i++) {
            mem->a = i;
            mem->b = i + 2;
            mem->listo = 1; // indicar que hay datos
            printf("[Productor] Generado par: %d %d\n", mem->a, mem->b);
            sleep(1); // simular tiempo de producción
            while (mem->listo == 1) {
                // esperar a que consumidores procesen
                usleep(100000);
            }
        }
        exit(0);
    } else {
        // Consumidor 1: multiplicación
        pid_cons1 = fork();
        if (pid_cons1 == 0) {
            int *resultados = malloc(N * sizeof(int));
            int idx = 0;

            while (idx < N) {
                if (mem->listo == 1) {
                    resultados[idx] = mem->a * mem->b;
                    mem->resultado_mul = resultados[idx];
                    printf("[Consumidor1] %d * %d = %d\n", mem->a, mem->b, resultados[idx]);
                    idx++;
                    // marcar como procesado solo cuando ambos hayan leído
                    mem->listo = 2; // estado intermedio
                }
                usleep(100000);
            }

            printf("[Consumidor1] Resultados finales:\n");
            for (int i = 0; i < idx; i++) printf("  %d\n", resultados[i]);

            free(resultados);
            exit(0);
        } else {
            // Consumidor 2: suma
            pid_cons2 = fork();
            if (pid_cons2 == 0) {
                int *resultados = malloc(N * sizeof(int));
                int idx = 0;

                while (idx < N) {
                    if (mem->listo == 2) {
                        resultados[idx] = mem->a + mem->b;
                        mem->resultado_sum = resultados[idx];
                        printf("[Consumidor2] %d + %d = %d\n", mem->a, mem->b, resultados[idx]);
                        idx++;
                        mem->listo = 0; // liberar para siguiente par
                    }
                    usleep(100000);
                }

                printf("[Consumidor2] Resultados finales:\n");
                for (int i = 0; i < idx; i++) printf("  %d\n", resultados[i]);

                free(resultados);
                exit(0);
            }
        }
    }

    // Padre espera a todos
    wait(NULL);
    wait(NULL);
    wait(NULL);

    // Liberar recursos
    shmdt(mem);
    shmctl(shm_id, IPC_RMID, NULL);

    printf("[Padre] Memoria compartida liberada. Fin del programa.\n");
    return 0;
}
