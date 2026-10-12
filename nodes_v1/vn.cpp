#include <iostream>
#include <string>
#include <cstring>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
using namespace std;

int main(int argc,char** argv) {
    if(argc!=2||argc!=3) return -1;
    string ip_on=string(argv[0]);
    int port_on=stoi(argv[1]);
    int port_udp=stoi(argv[2]);

    int sd_udp=socket(AF_INET,SOCK_DGRAM,0);
    sockaddr_in sock_udp;
    sock_udp.sin_family=AF_INET;
    sock_udp.sin_port=port_udp;
    sock_udp.sin_addr.s_addr=inet_addr("127.0.0.1");
    memset(&sock_udp.sin_zero,'\0',8);
    bind(sd_udp,(sockaddr*)&sock_udp,sizeof(sockaddr));

    int sd_tcp=socket(AF_INET,SOCK_STREAM,0);
    sockaddr_in sock_tcp;
    sockaddr_in sock_on;
    sock_on.sin_family=AF_INET;
    sock_on.sin_port=port_on;
    sock_on.sin_addr.s_addr=inet_addr(argv[0]);
    memset(&sock_udp.sin_zero,'\0',8);
    connect(sd_tcp,(sockaddr*)&sock_on,sizeof(sockaddr));
    unsigned int temp=sizeof(sockaddr);
    getsockname(sd_tcp,(sockaddr *)&sock_tcp,&temp);

    int idx[3];
    int i=0,j=0;
    for(int i=0;i<ip_on.length();i++) {
        if(j==3) break;
        if(ip_on[i]=='.') {
            idx[j]=i;
            j++;
        }
        i++;
    }
    int a,b,c,d;
    a=stoi(substr(0,idx[0]));
    b=stoi(substr(idx[0]+1,idx[1]-idx[0]-1));
    c=stoi(substr(idx[1]+1,idx[2]-idx[1]-1));
    d=stoi(substr(idx[2]+1,ip_on.length()-idx[2]-1));
    long long msg=port_udp+(100000*(d+256*(c+256*(b+256*a))));
    send(sd_tcp,(void*)msg,sizeof(long long),0);
}