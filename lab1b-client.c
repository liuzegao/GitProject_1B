//
//  client.c
//  Project_1B
//
//  Created by Zegao on 4/21/18.
//  Copyright © 2018 pro. All rights reserved.
//

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <strings.h>
#include <unistd.h>
#include <poll.h>
#include <errno.h>
#include <termios.h>
#include <string.h>
#include <getopt.h>
#include <sys/stat.h>
#include <fcntl.h>


static struct termios saved_attributes;

char isTherePort = 'N';

char isThereLog = 'N';

int portNumber;

char* itoa(int val, int base){
    
    static char buf[32] = {0};
    
    int i = 30;
    
    for(; val && i ; --i, val /= base)
        
        buf[i] = "0123456789abcdef"[val % base];
    
    return &buf[i+1];
    
}

void closeWithError(int pipe)
{
    int RC = close(pipe);
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

int writeToLogSent(int fd, char* buffer, int size)
{
    if(isThereLog == 'N'){
        return -1;
    }
    //write(fd, "SENT ", 5);
    char numberbuffer[32] = "\0";
    //char *number = NULL;
    //number = itoa(size,10);
    sprintf(numberbuffer,"SENT %d bytes: ", size);
    write(fd, numberbuffer, 14);
    //write(fd, " bytes:", 7);
    write(fd, buffer, size);
    int RC = write(fd, "\n", 1);
    return RC;
}

int writeToLogReceive(int fd, char* buffer, int size)
{
    if(isThereLog == 'N'){
        return -1;
    }
    //write(fd, "RECEIVED ", 9);
    char numberbuffer[32] = "\0";
    //char *number = NULL;
    //number = itoa(size,10);
    sprintf(numberbuffer,"RECEIVED %d bytes: ", size);
    write(fd, numberbuffer, 18);
    //write(fd, " bytes:", 7);
    write(fd, buffer, size);
    int RC = write(fd, "\n", 1);
    return RC;
}


//GNU FUNCTION, EXAM!
void restoreInputMode(void)
{
    tcsetattr (0, TCSANOW, &saved_attributes);
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

int main(int argc, char *argv[])
{
    static struct option long_options[] = {
        {"port", required_argument, 0, 'P'},{"log", required_argument, 0, 'L'},
    };
    
    int ch;
    char *outputFile = NULL;
    while((ch = getopt_long(argc, argv,"",long_options,NULL))!=-1){
        switch(ch){
            case 'P':
                isTherePort = 'Y';
                portNumber = atoi(optarg);
                break;
            case 'L':
                isThereLog = 'Y';
                outputFile = optarg;
                break;
            default:  exit(1); break;
        }
    }
    if(isTherePort == 'N'){
        fprintf(stderr, "Error: No port number .\n");
        exit(1);
    }
    
    
    int sockfd, portno;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    
    char buffer[256];
    
    portno = portNumber;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        fprintf(stderr,"ERROR opening socket \r\n");
    server = gethostbyname("localhost");
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host \r\n");
        exit(0);
    }
    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    memcpy((char *)server->h_addr,
          (char *)&serv_addr.sin_addr.s_addr,
          server->h_length);
    serv_addr.sin_port = htons(portno);
    if (connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0)
        fprintf(stderr,"ERROR connecting \r\n");
    memset(buffer,0,256);
    struct pollfd pollFdGroup[2];
    pollFdGroup[0].fd = 0;
    pollFdGroup[1].fd = sockfd;
    pollFdGroup[0].events = POLLIN | POLLHUP | POLLERR;
    pollFdGroup[1].events = POLLIN | POLLHUP | POLLERR;
    int fd_output = 0;
    if(isThereLog == 'Y')
    {
        fd_output = creat(outputFile,S_IRWXU);
    }

    changeInputMode();
    while(1){
        int returnValue = poll(pollFdGroup, 3, 0);
        if (returnValue < 0) {
            fprintf(stderr, "Error:%s.\r\n", strerror(errno));
            exit(1);
        }
        if ((pollFdGroup[0].revents & POLLIN)){
            char stdinBuffer = '\0';
            readWithError(0,&stdinBuffer, 1);
            if(stdinBuffer == '\03'){
                //kill(fpid, SIGINT);
                exit(0);
            }else if(stdinBuffer== '\r' || stdinBuffer == '\n'){
                char tempBuffer = {'\n'};
                writeWithError(sockfd,&tempBuffer,sizeof(tempBuffer));
                tempBuffer = '\r';
                writeToLogSent(fd_output,&tempBuffer,sizeof(tempBuffer));
                char templineBuffer[2] = {'\r','\n'};
                writeWithError(1,templineBuffer,sizeof(templineBuffer));
            }else{
                writeWithError(sockfd, &stdinBuffer, sizeof(stdinBuffer));
                writeToLogSent(fd_output,&stdinBuffer, sizeof(stdinBuffer));
                writeWithError(1,&stdinBuffer,sizeof(stdinBuffer));
            }
        }
        if ((pollFdGroup[1].revents & POLLIN)){
            char sockBuffer[2048] = "\0";
            int count = readWithError(sockfd, sockBuffer, 2048); // read from shell pipe
            if(count == 0){
                fprintf(stderr, "received EOF from server \r\n");
                exit(0);
            }
            writeToLogReceive(fd_output,sockBuffer,count);
            int i;
            for(i = 0;i<count;i++){
                if(sockBuffer[i] == '\n'){
                    char tempshellBuffer[2] = {'\r','\n'};
                    writeWithError(1,tempshellBuffer,sizeof(tempshellBuffer));
                    
                }else{
                    writeWithError(1,&sockBuffer[i],1);
                }
            }
        }
        if ((pollFdGroup[1].revents & (POLLHUP | POLLERR))) {
            fprintf(stderr, "received POLLUP/POLLERR from server \r\n");
            exit(0);
        }
    }
}
