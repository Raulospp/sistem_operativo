#include <stdio.h>
#include <stdbool.h>
#define MEMORY_SYZE 1824
#define PARTITIONS 4
#define PART_SIZE MEMORY_SIZE / PARTITIONS 



typedef struct
{
    int process_id;
    int size;
    bool occupied;

} Partition;

Partition memory[PARTITIONS];

void initialize_mem()
{
    for ( int i = 0; i < PARTITIONS; i++)
    {
        memory[i].process_id =1;
        memory[i]. size=0;
        memory[i].occupied = false;
    }
}
void print_mem()
{
    printf ("\nEstado de asignacion de memoria:");
    for ( int i = 0; i < PARTITIONS; i++)
    {

        if(memory[i].occupied)
        {
            printf("particion %d, proceso %d, (%d bytes)", 
                i, memory[i].process_id,memory[i].size);
        }
        else
        {
            printf("particion %d,esta libre",i);
        }

    }
}
void allocate_mem(int pid, int size)
{
    if(size > PART_SIZE)
    {
        printf("\n No puede reservarse ese tamaño de memoria");
        return;
    }
    for(int i = 0; i< PARTITIONS; i++)
       
        {
        if (!memory [i].occupied)
        {
            memory[i].process_id = pid;
            memory[i].size;
            memory[i].occupied = true;
            return;

        }
}
    return;
}
void free_mem ()
{
    ;
}



int main()


{
   
    //prueba
    //inicializar
    //alojar 3 procesos
    // imprimir
    //
}
