#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <pthread.h>
#include <string.h>

#define MAX_PROC 1000
#define NAME_SIZE 100


int main(int argc, char *argv[], char *environ[])
{
    int count=0;
    int pcount;
    int child_status;
    pid_t pid[MAX_PROC],rpid;
    char a1[NAME_SIZE] = {};
    char code[NAME_SIZE]= {};
    char name[NAME_SIZE] = {};
    int i;

    sem_t *init[MAX_PROC], *go[MAX_PROC], *term[MAX_PROC];

    pcount=atoi(argv[1]);  // Get the number of processes


    for(i=0;i < pcount; i++){

        sprintf(name,"semaphore_mpi_init%d",i);
        init[i] = sem_open(name, O_CREAT, 0600, 0);
        sem_init(init[i],1,0);

        sprintf(name,"semaphore_go%d",i);
        go[i] = sem_open(name, O_CREAT, 0600, 0);
        sem_init(go[i],1,0);

        sprintf(name,"semaphore_termin%d",i);
        term[i] = sem_open(name, O_CREAT, 0600, 0);
        sem_init(term[i],1,0);

    }


    while(count < pcount)
    {
        if(pid[count]=fork() == 0){   //fork a new process
            strcpy(code,argv[2]);
            argv[0]=argv[1];         // Set nproc
            sprintf(a1,"%d",count);
            argv[1]=a1;              // Set rank
            argv[2]=0;
            execve(code,argv,environ);   // Run MPI process
        }
        count++;
    }


    for(i=0;i < pcount; i++)
        sem_wait(init[i]);            // Wait for each process to call MPI_Init

    for(i=0;i < pcount; i++)
        sem_post(go[i]);              // Wait for each process to complete its MPI_Init tasks (for safe initialization of data structures, semaphores, etc.)

    for(i=0;i < pcount; i++)
        sem_wait(term[i]);           // Wait for each process to call MPI_Finalize


    for(i=0;i < pcount; i++){
        rpid=waitpid(pid[i],&child_status,WUNTRACED);   // Reap the processes one by one
    }



    return 0;
}
