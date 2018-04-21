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

static struct termios saved_attributes;

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
    int sockfd, portno;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    
    char buffer[256];
    if (argc < 3) {
        fprintf(stderr,"usage %s hostname port\n", argv[0]);
        exit(0);
    }
    portno = atoi(argv[2]);
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        fprintf(stderr,"ERROR opening socket");
    server = gethostbyname(argv[1]);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,
          (char *)&serv_addr.sin_addr.s_addr,
          server->h_length);
    serv_addr.sin_port = htons(portno);
    if (connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0)
        fprintf(stderr,"ERROR connecting");
    bzero(buffer,256);
    struct pollfd pollFdGroup[2];
    pollFdGroup[0].fd = 0;
    pollFdGroup[1].fd = sockfd;
    pollFdGroup[0].events = POLLIN | POLLHUP | POLLERR;
    pollFdGroup[1].events = POLLIN | POLLHUP | POLLERR;
    changeInputMode();
    while(1){
        int returnValue = poll(pollFdGroup, 3, 0);
        if (returnValue < 0) {
            fprintf(stderr, "Error:%s.\n", strerror(errno));
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
                char templineBuffer[2] = {'\r','\n'};
                writeWithError(1,templineBuffer,sizeof(templineBuffer));
            }else{
                writeWithError(sockfd, &stdinBuffer, sizeof(stdinBuffer));
                writeWithError(1,&stdinBuffer,sizeof(stdinBuffer));
            }
        }
        if ((pollFdGroup[1].revents & POLLIN)){
            char sockBuffer[2048] = "\0";
            int count = readWithError(sockfd, sockBuffer, 2048); // read from shell pipe
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
    }
}
