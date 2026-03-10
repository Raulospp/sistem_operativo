#include <stdio.h>
#include <stdbool.h>

#define MEMORY_SIZE 1024
#define PARTITIONS 4
#define PART_SIZE MEMORY_SIZE / PARTITIONS /* CALCULA EL TAMAÑO DE CADA PARTICIÓN */

typedef struct
{
    int process_id;
    int size;
    bool occupied;

} Partition;

//Representación lógica del particionamiento de memoria:
Partition memory[PARTITIONS];

void initalize_mem()
{
    for(int i = 0; i < PARTITIONS; i++)
    {
        memory[i].process_id = -1;
        memory[i].size = 0;
        memory[i].occupied = false;
    }

}

void print_mem()
{
    printf("\nEstado de asignación de memoria:");
    for(int i = 0; i < PARTITIONS; i++)
    {
        if(memory[i].occupied)
        {
            printf("Partición %d, proceso %d (%d bytes)",
                i, memory[i].process_id,memory[i].size

            );
        }
        else
        {
            printf("Partición %d está libre",i);
        }
    }

}

void allocate_mem(int pid, int size)
{
    if(size > PART_SIZE)
    {
        printf("\nNo puede reservarse ese tamaño de memoria");
        return;
    }

    //BÚSQUEDA DE UN BLOQUE DE MEMORIA LIBRE - SECUENCIAL:
    for(int i = 0; i < PARTITIONS; i++)
    {
        if(! memory[i].occupied)
        {
            memory[i].process_id = pid;
            memory[i].size = size;
            memory[i].occupied = true;
            return;
        }
    }

    //SI AL FINAL DEL RECORRIDO NO ENCUENTRA BLOQUE LIBRE, RETORNA
    return;

}

void free_mem(int pid)
{
    for(int i = 0; i < PARTITIONS; i++)
    {
        if(! memory[i].process_id == pid)
        {
            memory[i].process_id = -1;
            memory[i].size = 0;
            memory[i].occupied = false;
            return;
        }
    }

}


int main()
{
    
    //PRUEBA:
    //inicializamos:

    //alojamos 3 procesos:

    //imprimimos:

    //liberamos un proceso:

    //imprimimos de nuevo:

    return 0;
}