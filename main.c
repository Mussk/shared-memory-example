#define _POSIX_SOURCE
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <time.h>
#include <sys/msg.h>
#include <sys/sem.h>


#define NPIDS 20
// key value of shared memory
#define KEY 0x687524
#define MSKEY 0x123456
#define SEM_ID 250

//struct for sm
struct memory {
    char buff[100];
    int pids[NPIDS];
};

//struct for message queue
struct mesg_buffer {
    long mesg_type;
    char mesg_text[100];
} message;

struct memory* shmptr;

// handler function to print message received from process
int messages_counter = 0;
int index_pid = 0;
int msgid;
int sem_set_id; // ID of the semaphore set

void sem_lock(int sem_set_id) {
    // structure for semaphore operations.
    struct sembuf sem_op;

    // wait on the semaphore, unless it's value is non-negative.
    sem_op.sem_num = 0;
    sem_op.sem_op = -1;
    sem_op.sem_flg = 0;
    semop(sem_set_id, &sem_op, 1);
}


void sem_unlock(int sem_set_id) {
    // structure for semaphore operations.
    struct sembuf sem_op;

    // signal the semaphore - increase its value by one.
    sem_op.sem_num = 0;
    sem_op.sem_op = 1;
    sem_op.sem_flg = 0;
    semop(sem_set_id, &sem_op, 1);
}

void handler(int signum) {
    // if signum is SIGUSR1, then user 1 is receiving a message from process
    if (signum == SIGUSR1) {
        write(1,"Recieved: ",11);
        sem_lock(sem_set_id);
        char *str = shmptr->buff;
        write(1,str,50);
        write(1,"\n",1);
        sem_unlock(sem_set_id);
        messages_counter++;
        if(messages_counter == 3) {
            write(1,"Got 3 messages, change my pid to 0\n", 50);
            sem_lock(sem_set_id);
            shmptr->pids[index_pid] = 0;
            sem_unlock(sem_set_id);
        }
        //for message queue
    } else if (signum == SIGUSR2) {
        write(1,"Got signal from message queue, writing data: \n",52);
        sprintf(message.mesg_text,"my pid: %d, messages recieved %d\n", getpid(),messages_counter); //?

        //send message
        msgsnd(msgid, &message, sizeof(message), 0);

    }

}

//get amount of actually alive processes
int get_pids_size() {

    int size = 0;
    sem_lock(sem_set_id);
    for (int i = 0; i < NPIDS; ++i) {
        if(shmptr->pids[i] != 0)
            size++;
    }
    sem_unlock(sem_set_id);
    return size;
}

/** returns a massive of indexes of pids without index of pid of current process
 * needed for randoming process to send signal **/
int* get_indexes(int index_pid, int size) {

    static int* pids_indexes;
    pids_indexes = malloc((size-1)*sizeof(int));
    int k = 0;
    for (int i = 0; i < size-1; ++i) {
        if(i == index_pid) {
            k++;
            pids_indexes[i] = i+k;
        }else
            pids_indexes[i] = i+k;
    }
    printf("Avalible processess to send them message: ");
    for (int j = 0; j < size-1; ++j) {
        printf("%d ",pids_indexes[j]);
        fflush(stdout);
    }
    write(1,"\n",1);

    return pids_indexes;
}





int main() {
    // process id of user
    int pid = getpid();
    int shmid;
    int rc;

    union semun {
        int val;
        struct semid_ds *buf;
        short * array;
    } sem_val;


    printf("My pid %d\n",pid);
    fflush(stdout);

    srand(time(NULL));

    // semaphore create
    sem_set_id = semget(SEM_ID, 1, IPC_CREAT | 0600);
    if (sem_set_id == -1) {
        perror("semaphore");
        exit(1);
    }

    // intialize the first semaphore in our set to 1
    sem_val.val = 1;
    rc = semctl(sem_set_id, 0, SETVAL, sem_val);
    if (rc == -1) {
        perror("semaphore rc");
        exit(1);
    }

    // shared memory create
   if((shmid = shmget(KEY, sizeof(struct memory), IPC_CREAT | IPC_EXCL | 0666)) == -1){
       printf("Shared memory segment already exists\n");
       //client like
       if((shmid = shmget(KEY, sizeof(struct memory), 0)) == -1) {
           perror("shmget");
           exit(1);
       }
   }else {
       printf("Creating new shared memory segment\n");
   }

    // attaching the shared memory
    shmptr = shmat(shmid, 0, 0);

    // store the process id of user in shared memory
    for (int i = 0; i < NPIDS; ++i) {
        if (shmptr->pids[i] == 0){
            shmptr->pids[i] = pid;
            index_pid = i;

            break;
        }
    }

    /** MESSAGE QUEUE SECTION **/
    msgid = msgget(MSKEY, 0666 | IPC_CREAT);
    message.mesg_type = 1;


    struct sigaction act;
    // calling the signal function using signal type SIGUSER1
    act.sa_handler = &handler;
    act.sa_flags = 0;
    sigaction(SIGUSR1, &act, NULL);
    sigaction(SIGUSR2,&act,NULL);


    while (messages_counter < 3) {

        sleep(10);

        //if last process causes floating point exception and terminates
        int size = get_pids_size();

        if (size == 1) {
            printf("I am last one, terminating...\n");
            fflush(stdout);
            shmdt((void*)shmptr);
            semctl(sem_set_id, 0, IPC_RMID,sem_val);
            shmctl(shmid, IPC_RMID, NULL);
            //msgctl(msgid, IPC_RMID, NULL);
            exit(0);
        }

        // write to shm
        printf("I send to shm\n");
        fflush(stdout);

        //we don't want send signal to ourselves
        int* indexes = get_indexes(index_pid, size);
        int random = indexes[rand() % (size-1)];
        printf("Generated index of pid: ");
        printf("%d\n", random);
        fflush(stdout);

        sem_lock(sem_set_id);
        sprintf(shmptr->buff,"message %d,from pid: %d, to pid: %d",messages_counter, pid, shmptr->pids[random]);
        fflush(stdout);
        sem_unlock(sem_set_id);
        // sending the message to user2 using kill function
        kill(shmptr->pids[random], SIGUSR1);


    }
    shmdt((void*)shmptr);

    return 0;
}