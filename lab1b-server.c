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
#include <string.h>

char isThereShell = 'N';

int fpid;

void catch(){
    kill(fpid,13);
    fprintf (stderr, "Error: receive SIGPIPE.\n");
    _exit(0);
}

void closeWithError(int pipe)
{
    int RC = close(pipe);
    if (RC == -1){
        fprintf(stderr, "Error:%s.\n", strerror(errno));
        exit(1);
    }
}

void dup2WithError(int pipe, int fd)
{
    int RC = dup2(pipe,fd);
    if (RC == -1){
        fprintf(stderr, "Error:%s.\n", strerror(errno));
        exit(1);
    }
}

int readWithError(int fd, char* buffer, int size)
{
    int RC = read(fd, buffer, size);
    if (RC == -1){
        fprintf(stderr, "Error:%s.\n", strerror(errno));
        exit(1);
    }
    return RC;
}

int writeWithError(int fd, char* buffer, int size)
{
    int RC = write(fd, buffer, size);
    if (RC == -1){
        fprintf(stderr, "Error:%s.\n", strerror(errno));
        exit(1);
    }
    return RC;
}

int main(int argc, char * argv[]) {
    
    if (signal(SIGPIPE, catch) == SIG_ERR ){
        fprintf(stderr, "Error:%s.\n", strerror(errno));
        exit(1);
    }
    
    int toChildPip[2],toParentPip[3];
    if(pipe(toChildPip) == -1){
        fprintf(stderr, "Error:%s.\n", strerror(errno));
        exit(1);
    }
    if(pipe(toParentPip) == -1){
        fprintf(stderr, "Error:%s.\n", strerror(errno));
        exit(1);
    }
    //Create a new procee with fork()
    fpid =fork();
    if(fpid < 0){ //fails fork()
        fprintf(stderr, "Error:%s.\n", strerror(errno));
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
            fprintf(stderr, "Error:%s.\n", strerror(errno));
            exit(1);
        }
    }else{//parent process
        //Close useless fd
        int sockfd, newsockfd, portno;
        socklen_t* restrict clilen;
        struct sockaddr_in serv_addr, cli_addr;
        if (argc < 2) {
            fprintf(stderr,"ERROR, no port provided\n");
            exit(1);
        }
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0)
            fprintf(stderr,"ERROR opening socket");
        memset((char *) &serv_addr, 0, sizeof(serv_addr));
        portno = atoi(argv[1]);
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr.s_addr = INADDR_ANY;
        serv_addr.sin_port = htons(portno);
        if (bind(sockfd, (struct sockaddr *) &serv_addr,
                 sizeof(serv_addr)) < 0)
            fprintf(stderr,"ERROR on binding");
        listen(sockfd,5);
        //clilen = sizeof(cli_addr);
        socklen_t client_addr_size = sizeof(cli_addr);
        clilen = (socklen_t* restrict) &client_addr_size;
        newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, clilen);
        if (newsockfd < 0)
            fprintf(stderr,"ERROR on accept");
        
        closeWithError(toChildPip[0]);
        closeWithError(toParentPip[1]);
        char newbuffer[2048] = "\0";
        int returnValue;
        struct pollfd pollFdGroup[3];
        pollFdGroup[0].fd = 0;
        pollFdGroup[1].fd = toParentPip[0];
        pollFdGroup[2].fd = newsockfd;
        pollFdGroup[0].events = POLLIN | POLLHUP | POLLERR;
        pollFdGroup[1].events = POLLIN | POLLHUP | POLLERR;
        pollFdGroup[2].events = POLLIN | POLLHUP | POLLERR;
        //Poll
        while (1) {
            // do a poll and check for errors
            returnValue = poll(pollFdGroup, 3, 0);
            if (returnValue < 0) {
                fprintf(stderr, "Error:%s.\n", strerror(errno));
                exit(1);
            }
            if ((pollFdGroup[0].revents & POLLIN)){
                char buffer[256];
                memset(buffer,0,256);
                int count = readWithError(0,buffer, 256);
                writeWithError(toChildPip[1], buffer, count);
            }
            if ((pollFdGroup[1].revents & POLLIN)) {
                int count = readWithError(toParentPip[0], newbuffer, 2048); // read from shell pipe
                writeWithError(newsockfd,newbuffer,count);
            }
            if ((pollFdGroup[1].revents & (POLLHUP | POLLERR))) {
                fprintf(stderr,"ANC \n");
                exit(0);
            }
            if ((pollFdGroup[2].revents & POLLIN)){
                //fprintf(stderr,"BREAK 1 \n");
                char buffer[256];
                memset(buffer,0,256);
                int count = readWithError(newsockfd,buffer,255);
                if(count == 0){
                    fprintf(stderr,"received EOF from client \n");
                    close(toChildPip[1]);
                    int childstatus;
                    if (waitpid(fpid, &childstatus, 0) == -1) {
                        fprintf(stderr, "error: waitpid failed");
                        exit(EXIT_FAILURE);
                    }
                    const int higher = WEXITSTATUS(childstatus);
                    const int lower = WTERMSIG(childstatus);
                    fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\n", lower, higher);
                    exit(0);
                }
                writeWithError(toChildPip[1],buffer,count);
            }
            if ((pollFdGroup[2].revents & (POLLHUP | POLLERR))) {
                fprintf(stderr,"client socket disconnected \n");
                close(sockfd);
                exit(0);
            }
        }
    }
    exit(0);
}


