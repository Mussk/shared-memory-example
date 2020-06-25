#define _POSIX_SOURCE
#include <signal.h>
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <zconf.h>
#include <stdlib.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
#define MSKEY 0x123456

int msgid_r;

struct mesg_bufferr {
    long mesg_type;
    char mesg_text[100];
} mesgBufferrmessage;

void handler_m(int sig){
    printf("Got signal\n");
    msgctl(msgid_r, IPC_RMID, NULL);
    exit(0);
}

int main() {

    signal(SIGINT,handler_m);
    while (1) {
        int pid_to_send;
        printf("Write a pid of process: ");
        fflush(stdout);
        scanf("%d", &pid_to_send);

        //send signal to process
        printf("send signal to process with pid %d\n", pid_to_send);
        fflush(stdout);
        kill(pid_to_send, SIGUSR2);
        sleep(3);

        msgid_r = msgget(MSKEY, IPC_CREAT);

        // msgrcv to receive message
        msgrcv(msgid_r, &mesgBufferrmessage, sizeof(mesgBufferrmessage), 1L, 0);

        // display the message
        printf("Data Received is: ");
        fflush(stdout);
        write(1,mesgBufferrmessage.mesg_text,65);


    }

    return 0;
};

#pragma clang diagnostic pop