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

static struct termios saved_attributes;

void restoreInputMode(void)
{
    
    tcsetattr (0, TCSANOW, &saved_attributes);
    if (isThereShell == 'Y') {
        int childstatus;
        if (waitpid(fpid, &childstatus, 0) == -1) {
            fprintf(stderr, "error: waitpid failed");
            exit(EXIT_FAILURE);
        }
        if (WIFEXITED(childstatus)) {
            //exit(0);
        }
        
        const int higher = WEXITSTATUS(childstatus);
        const int lower = WTERMSIG(childstatus);
        fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\n", lower, higher);
    }
}

void catch(){
    kill(fpid,13);
    fprintf (stderr, "Error: receive SIGPIPE.\r\n");
    restoreInputMode();
    _exit(4);
}

void changeInputMode(void)
{
    //Make sure stdin is in terminal mode
    if (!isatty(0))
    {
        fprintf (stderr, "Error: stdin is not in terminal mode.\n");
        exit(1);
    }
    
    //Save the original terminal mode
    tcgetattr(0, &saved_attributes);
    atexit(restoreInputMode);
    
    struct termios tattr;
    //Change to new terminal mode
    tcgetattr (0, &tattr);
    tattr.c_iflag = ISTRIP;
    tattr.c_oflag = 0;
    tattr.c_lflag = 0; //Vmin is by default 1, Vtime is by default 0
    tcsetattr (0, TCSANOW, &tattr);
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
    static struct option long_options[] = {
        {"shell", no_argument, 0, 'S'},
    };
    
    int ch;
    while((ch = getopt_long(argc, argv,"",long_options,NULL))!=-1){
        switch(ch){
            case 'S': isThereShell = 'Y'; break;
            default:  exit(1); break;
        }
    }
    
    if(isThereShell == 'N'){
        changeInputMode();
        //Read and write
        char buffer = '\0';
        char temp_buffer[2] = "\0";
        while((buffer != '\03')&&(buffer != '\04')){
            readWithError(0,&buffer, sizeof(char));
            //Deal with the line change problem
            if(buffer == '\r' || buffer == '\n'){
                temp_buffer[0] = '\r';
                temp_buffer[1] = '\n';
                writeWithError(1,temp_buffer,sizeof(temp_buffer));
            }else{
                writeWithError(1,&buffer,sizeof(char));
            }
        }
    }else{//Shell Mode
        //Create two fd
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
            changeInputMode();
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
                        close(toChildPip[1]);
                        //exit(0);
                    }else if(parentbuffer == '\03'){
                        kill(fpid, SIGINT);
                        //exit(0);
                    }else if(parentbuffer == '\r' || parentbuffer  == '\n'){
                        char tempBuffer = {'\n'};
                        writeWithError(toChildPip[1],&tempBuffer,sizeof(tempBuffer));
                        char templineBuffer[2] = {'\r','\n'};
                        writeWithError(1,templineBuffer,sizeof(templineBuffer));
                    }else{
                        writeWithError(toChildPip[1], &parentbuffer, sizeof(parentbuffer));
                        writeWithError(1,&parentbuffer,sizeof(parentbuffer));
                    }
                }
                if ((pollFdGroup[1].revents & POLLIN)) {
                    int count = readWithError(toParentPip[0], newbuffer, 2048); // read from shell pipe
                    int i;
                    for(i = 0;i<count;i++){
                        if(newbuffer[i] == '\n'){
                            char tempshellBuffer[2] = {'\r','\n'};
                            writeWithError(1,tempshellBuffer,sizeof(tempshellBuffer));
                        }else{
                            writeWithError(1,&newbuffer[i],1);
                        }
                    }
                }
                if ((pollFdGroup[1].revents & (POLLHUP | POLLERR))) {
                    //fprintf(stderr,"ANC \r\n");
                    exit(0);
                }
            }
        }
    }
    exit(0);
}
