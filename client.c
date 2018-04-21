//
//  client.c
//  Project_1B
//
//  Created by Zegao on 4/21/18.
//  Copyright © 2018 pro. All rights reserved.
//

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <strings.h>
#include <unistd.h>
#include <poll.h>
#include <errno.h>

void error(char *msg)
{
    perror(msg);
    exit(0);
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
        error("ERROR opening socket");
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
        error("ERROR connecting");
    bzero(buffer,256);
    struct pollfd pollFdGroup[2];
    pollFdGroup[0].fd = 0;
    pollFdGroup[1].fd = sockfd;
    pollFdGroup[0].events = POLLIN | POLLHUP | POLLERR;
    pollFdGroup[1].events = POLLIN | POLLHUP | POLLERR;
    while(1){
        int returnValue = poll(pollFdGroup, 3, 0);
        if (returnValue < 0) {
            fprintf(stderr, "Error:%s.\n", strerror(errno));
            exit(1);
        }
        if ((pollFdGroup[0].revents & POLLIN)){
            int count = read(0,buffer,255);
            write(sockfd,buffer,count);
        }
        if ((pollFdGroup[1].revents & POLLIN)){
            int count = read(sockfd,buffer,255);
            write(1,buffer,count);
        }
    }
}
