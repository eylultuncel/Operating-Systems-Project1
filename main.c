#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <semaphore.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>


sem_t mutex;
sem_t *full,*empty;
int BOUNDED_BUFFER_SIZE =10;
int RANK,SIZE;
sem_t  *init, *go, *term;
int shm_fd;



int MPI_Init(int *argc, char ***argv){
    char name[20]= {};
    char shm_name[50]= {};
    const int mem_size = 4096;
    void* pointer;

    // initialize global variables SIZE and RANK with given arguments
    SIZE=atoi((*argv)[0]);
    RANK=atoi((*argv)[1]);

    sprintf(name,"semaphore_mpi_init%d",RANK);
    init = sem_open(name, 0);

    sprintf(name,"semaphore_go%d",RANK);
    go = sem_open(name, 0);

    sprintf(name,"semaphore_termin%d",RANK);
    term = sem_open(name, 0);

    sem_post(init);

    // open a shared memory special to the process
    sprintf(shm_name,"eylul_shared_memory%d",RANK);
    shm_fd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
    if(shm_fd < 0){
        printf("Error opening shared memoryy\n");
        exit(0);}
    ftruncate(shm_fd, mem_size);
    pointer = mmap(NULL, mem_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd,0);
    if( MAP_FAILED == pointer){
        printf("Error shared memory pointer\n");
        exit(0);}


    // create mutex for critical operations
    sem_init(&mutex,1,1);

    // create semaphores full and empty for process
    sprintf(name,"semaphore_full%d",RANK);
    full = sem_open(name, O_CREAT, 0600, 0);
    sem_init(full,1,0);

    sprintf(name,"semaphore_empty%d",RANK);
    empty = sem_open(name, O_CREAT, 0600, 0);
    sem_init(empty,1,BOUNDED_BUFFER_SIZE);

    sem_wait(go);

}

int MPI_Comm_size(int *size){
    *size = SIZE;
    return  *size;
}

int MPI_Comm_rank(int *rank){
    *rank = RANK;
    return *rank;
}

int MPI_Recv(void *buf, int count, int datatype, int source, int tag){
    void* pointer;
    const int mem_size = 4096;
    int shm_fd;
    char shm_name[50]= {};
    char name[20]= {};
    buf = (int*)buf;

    // creating pointers for accessing source process semaphores
    sem_t *full_source,*empty_source;

    // open source process semaphores
    sprintf(name,"semaphore_full%d",source);
    full_source = sem_open(name, O_CREAT, 0600, 0);
    sprintf(name,"semaphore_empty%d",source);
    empty_source = sem_open(name, O_CREAT, 0600, 0);


    // open the shared memory of the process
    sprintf(shm_name,"eylul_shared_memory%d",RANK);
    shm_fd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
    if(shm_fd < 0){
        printf("Error opening shared memory\n");
        exit(0);}
    ftruncate(shm_fd, mem_size);
    pointer = mmap(NULL, mem_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd,0);
    if( MAP_FAILED == pointer){
        printf("Error shared memory pointer\n");
        exit(0);}


    // first signal the sender process for start their execution
    sem_wait(empty_source);
    sem_post(full_source);

    // wait for sender process to send message
    sem_wait(full);
    //for critical operations mutex must be waited
    sem_wait(&mutex);

    // read message from shared memory
    memcpy(buf, pointer, strlen(pointer)+1);
    pointer += count*datatype;

    // if tag value (in the shared memory) is equal to the tag of the receiver
    if(tag == atoi(pointer)){

        // then send signal to the sender process for let it continue and finish
        sem_post(&mutex);
        sem_post(empty);
        sem_post(full_source);

    }

}

int MPI_Send(const void *buf, int count, int datatype, int dest, int tag){

    void* pointer;
    const int mem_size = 4096;
    int shm_fd;
    char name[20]= {};
    char shm_name[50]= {};

    sem_t *full_dest,*empty_dest;

    sprintf(name,"semaphore_full%d",dest);
    full_dest = sem_open(name, O_CREAT, 0600, 0);
    sprintf(name,"semaphore_empty%d",dest);
    empty_dest = sem_open(name, O_CREAT, 0600, 0);


    sprintf(shm_name,"eylul_shared_memory%d",dest);
    shm_fd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
    if(shm_fd < 0){
        printf("Error opening shared memory\n");
        exit(0);}
    ftruncate(shm_fd, mem_size);
    pointer = mmap(NULL, mem_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd,0);
    if( MAP_FAILED == pointer){
        printf("Error shared memory pointer\n");
        exit(0);}


    // first wait for the receiver process
    sem_wait(full);
    sem_post(empty);

    // when receiver process starts execution, sender can continue to produce message
    sem_wait(empty_dest);
    //for critical operations mutex must be waited
    sem_wait(&mutex);

    // write message to the destination shared memory
    memcpy(pointer, buf, strlen(buf)+1);
    pointer += count*datatype;
    // write tag value to the destination shared memory
    sprintf(pointer,"%d",tag);
    pointer += sizeof(int);

    // then send signal to the receiver process for let it continue and finish
    sem_post(&mutex);
    sem_post(full_dest);

    // wait for receiver to finish its execution, and then sender can be finished
    sem_wait(full);

}

int MPI_Finalize(){

    sem_destroy(&mutex);
    sem_destroy(empty);
    sem_destroy(full);

    shm_unlink("shared_memory");

    sem_post(term);
    exit(0);

}



int main(int argc, char *argv[], char *envp[]) {

    int npes,myrank,number,i;
    MPI_Init(&argc, &argv);
    MPI_Comm_size(&npes);
    MPI_Comm_rank(&myrank);

    /*
    if(myrank%2 == 0){
        MPI_Recv(&number,1,sizeof(int),(myrank+1)%npes,0);
        printf("From process %d data= %d, RECEIVED!\n",RANK,number);
        MPI_Send(&number,1,sizeof(int),(myrank+1)%npes,0);
    } else {
        number=myrank;
        MPI_Send(&number,1,sizeof(int),(myrank-1+npes)%npes,0);
        MPI_Recv(&number,1,sizeof(int),(myrank-1+npes)%npes,0);
        printf("From process %d data= %d, RECEIVED!\n",RANK,number);
    }

*/


     if(myrank == 0)
     {
         for(i = 1; i < npes ; i++)
         {
             MPI_Recv(&number, 1, sizeof(int), i, 0);
             printf("From process %d data= %d, RECEIVED!\n",i,number);
         }
     }
     else
     {
         number=myrank;
         MPI_Send(&number, 1, sizeof(int), 0, 0);
     }

     if(myrank == 0)
     {
         for(i = 1; i < npes ; i++)
         {
             MPI_Send(&i, 1, sizeof(int), i, 0);
         }
     }
     else
     {
         MPI_Recv(&number, 1, sizeof(int), 0, 0);
         printf("RECEIVED from %d data= %d, pid=%d \n",myrank,number,getpid());
     }



    for(i = 0; i < 100 ; i++)
    {
        if(myrank%2 == 0)
        {
            MPI_Recv(&number, 1,sizeof(int), (myrank+1)%npes, 0);
            MPI_Send(&number, 1,sizeof(int), (myrank+1)%npes, 0);
        } else {
            MPI_Send(&number, 1,sizeof(int), (myrank-1+npes)%npes, 0);
            MPI_Recv(&number, 1,sizeof(int), (myrank-1+npes)%npes, 0);
        }
    }


/*

    if(myrank == 0)
    {
        MPI_Recv(&number, 1, sizeof(int), 1, 0);
        printf("RECEIVED!\n");
    }
    else
    {
        MPI_Send(&myrank, 1, sizeof(int), 0, 0);
        printf("SENT!\n");
    }
    printf("STAGE 2!\n");
    if(myrank == 0)
    {
        MPI_Recv(&number, 1, sizeof(int), 1, 1);
        printf("RECEIVED >>> 2\n");
    }
    else
    {
        MPI_Send(&myrank, 1, sizeof(int), 0, 0);
        printf("SENT >>> 2\n");
    }
*/

    printf("FINISHED%d\n",myrank);
    MPI_Finalize();

    return 0;
}