#include <string.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#define DEST_IP "127.0.0.1"
#define DEST_PORT 2000

#define SRC_IP "127.0.0.1"
#define SRC_PORT 3000

int main() {
    int sockfd;
    struct sockaddr_in dest_addr,src_addr; // will hold the destination addr
    sockfd = socket(AF_INET, SOCK_DGRAM, 0); // do some error checking!
    dest_addr.sin_family = AF_INET; // host byte order
    dest_addr.sin_port = htons(DEST_PORT); // short, network byte order
    dest_addr.sin_addr.s_addr = inet_addr(DEST_IP);
    memset(&(dest_addr.sin_zero), '\0', 8); // zero the rest of the struct
    // don’t forget to error check the connect()!
    src_addr.sin_family = AF_INET; // host byte order
    src_addr.sin_port = htons(SRC_PORT); // short, network byte order
    src_addr.sin_addr.s_addr = inet_addr(SRC_IP);
    memset(&(src_addr.sin_zero), '\0', 8); // zero the rest of the struct
    bind(sockfd, (struct sockaddr*)&src_addr, sizeof(struct sockaddr));
    char* msg="Hello, shoewashers!!";
    unsigned int addr_len=sizeof(struct sockaddr);
    sendto(sockfd, msg, 20, 0, (struct sockaddr*)&dest_addr, addr_len);
    close(sockfd);
}