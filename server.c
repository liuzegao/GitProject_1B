//
//  server.c
//  Project_1B
//  CS 111
//  Created by Zegao on 4/12/18.
//  Copyright © 2018 pro. All rights reserved.
//

#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

char isThereShell = 'N';

int fpid;

void catch(){
    kill(fpid,13);
    fprintf (stderr, "Error: receive SIGPIPE.\r\n");
    _exit(4);
}

void closeWithError(int pipe)
{
    int RC = close(pipe);
    if (RC == -1){
        fprintf(stderr, "Error:%s.\r\n", strerror(errno));
        exit(1);
    }
}

void dup2WithError(int pipe, int fd)
{
    int RC = dup2(pipe,fd);
    if (RC == -1){
        fprintf(stderr, "Error:%s.\r\n", strerror(errno));
        exit(1);
    }
}

int readWithError(int fd, char* buffer, int size)
{
    int RC = read(fd, buffer, size);
    if (RC == -1){
        fprintf(stderr, "Error:%s.\r\n", strerror(errno));
        exit(1);
    }
    return RC;
}

int writeWithError(int fd, char* buffer, int size)
{
    int RC = write(fd, buffer, size);
    if (RC == -1){
        fprintf(stderr, "Error:%s.\r\n", strerror(errno));
        exit(1);
    }
    return RC;
}

int main(int argc, char * argv[]) {
    
    if (signal(SIGPIPE, catch) == SIG_ERR ){
        fprintf(stderr, "Error:%s.\r\n", strerror(errno));
        exit(1);
    }
    
    int toChildPip[2],toParentPip[3];
    if(pipe(toChildPip) == -1){
        fprintf(stderr, "Error:%s.\r\n", strerror(errno));
        exit(1);
    }
    if(pipe(toParentPip) == -1){
        fprintf(stderr, "Error:%s.\r\n", strerror(errno));
        exit(1);
    }
    //Create a new procee with fork()
    fpid =fork();
    if(fpid < 0){ //fails fork()
        fprintf(stderr, "Error:%s.\r\n", strerror(errno));
        exit(1);
    }else if (fpid == 0){//Child process
        closeWithError(toChildPip[1]);
        closeWithError(toParentPip[0]);
        dup2WithError(toChildPip[0],0);
        closeWithError(toChildPip[0]);
        dup2WithError(toParentPip[1],1);
        dup2WithError(toParentPip[1],2);
        closeWithError(toParentPip[1]);
        char *execvp_argv[2];
        char execvp_filename[] = "/bin/bash";
        execvp_argv[0] = execvp_filename;
        execvp_argv[1] = NULL;
        if (execvp(execvp_filename,execvp_argv)<0){
            fprintf(stderr, "Error:%s.\r\n", strerror(errno));
            exit(1);
        }
    }else{//parent process
        //Close useless fd
        closeWithError(toChildPip[0]);
        closeWithError(toParentPip[1]);
        char parentbuffer;
        char newbuffer[2048] = "\0";
        int returnValue;
        struct pollfd pollFdGroup[2];
        pollFdGroup[0].fd = 0;
        pollFdGroup[1].fd = toParentPip[0];
        pollFdGroup[0].events = POLLIN | POLLHUP | POLLERR;
        pollFdGroup[1].events = POLLIN | POLLHUP | POLLERR;
        //Poll
        while (1) {
            // do a poll and check for errors
            returnValue = poll(pollFdGroup, 2, 0);
            if (returnValue < 0) {
                fprintf(stderr, "Error:%s.\r\n", strerror(errno));
                exit(1);
            }
            if ((pollFdGroup[0].revents & POLLIN)){
                readWithError(0,&parentbuffer, 1);
                if(parentbuffer == '\04'){
                    fprintf(stderr,"ANC \r\n");
                    close(toChildPip[1]);
                }else if(parentbuffer == '\03'){
                    fprintf(stderr,"ANC \r\n");
                    kill(fpid, SIGINT);
                }else{
                    writeWithError(toChildPip[1], &parentbuffer, sizeof(parentbuffer));
                }
            }
            if ((pollFdGroup[1].revents & POLLIN)) {
                int count = readWithError(toParentPip[0], newbuffer, 2048); // read from shell pipe
                writeWithError(1,newbuffer,count);
            }
            if ((pollFdGroup[1].revents & (POLLHUP | POLLERR))) {
                fprintf(stderr,"ANC \r\n");
                exit(0);
            }
        }
    }
    exit(0);
}


