#include <iostream>
#include <string>
#include <cstring>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <map>
#include <chrono>
using namespace std;
using namespace std::chrono;

struct neighbour {
    int id;
    string ip;
    int port;
    int cost;
    sockaddr_in sock;
};
struct nodeinfo {
    int id;
    vector<int> adj;
    int seq;
};

vector<neighbour> parse_on(const char* c);
nodeinfo to_nodeinfo(const vector<neighbour>& v);
string make_lsp(const vector<neighbour>& v);
nodeinfo parse_lsp(const char* c);

int main(int argc,char** argv) {
    int port_on;
    int port_udp;
    if(argc==3) {
        port_on=5000;
        port_udp=stoi(argv[2]);
    }
    else if(argc==4) {
        port_on=stoi(argv[2]);
        port_udp=stoi(argv[3]);
    }
    else {
        cerr<<"Incorrect arguments\n";
        return -1;
    }
    string ip_on=string(argv[1]);
    string ip_vn="127.0.0.1";

    int sd_udp=socket(AF_INET,SOCK_DGRAM,0);
    if(sd_udp<0) {
        cerr<<"Socket allocation failed\n";
        return -1;
    }
    fcntl(sd_udp,F_SETFL,O_NONBLOCK);
    sockaddr_in sock_udp;
    sock_udp.sin_family=AF_INET;
    sock_udp.sin_port=htons(port_udp);
    const char* ip_vn_c=ip_vn.c_str();
    sock_udp.sin_addr.s_addr=inet_addr(ip_vn_c);
    memset(&sock_udp.sin_zero,'\0',8);
    if(bind(sd_udp,(sockaddr*)&sock_udp,sizeof(sockaddr))<0) {
        cerr<<"Binding failed\n";
        return -1;
    }

    int sd_tcp=socket(AF_INET,SOCK_STREAM,0);
    if(sd_tcp<0) {
        cerr<<"Socket allocation failed\n";
        return -1;
    }
    sockaddr_in sock_tcp;
    sockaddr_in sock_on;
    sock_on.sin_family=AF_INET;
    sock_on.sin_port=htons(port_on);
    sock_on.sin_addr.s_addr=inet_addr(argv[1]);
    memset(&sock_on.sin_zero,'\0',8);
    if(connect(sd_tcp,(sockaddr*)&sock_on,sizeof(sockaddr))<0) {
        cerr<<"Connection failed\n";
        return -1;
    }
    fcntl(sd_tcp, F_SETFL, O_NONBLOCK);
    unsigned int sock_sz=sizeof(sockaddr);
    getsockname(sd_tcp,(sockaddr *)&sock_tcp,&sock_sz);

    // int idx[3];
    // int i=0,j=0;
    // for(int i=0;i<ip_on.length();i++) {
    //     if(j==3) break;
    //     if(ip_on[i]=='.') {
    //         idx[j]=i;
    //         j++;
    //     }
    // }
    // int a,b,c,d;
    // a=stoi(ip_on.substr(0,idx[0]));
    // b=stoi(ip_on.substr(idx[0]+1,idx[1]-idx[0]-1));
    // c=stoi(ip_on.substr(idx[1]+1,idx[2]-idx[1]-1));
    // d=stoi(ip_on.substr(idx[2]+1,ip_on.length()-idx[2]-1));
    // long long msg=port_udp+(100000*(d+256*(c+256*(b+256*a))));
    string s=ip_vn+" "+to_string(port_udp);
    const char* send_msg=s.c_str();

    send(sd_tcp,send_msg,s.length(),0);

    vector<neighbour> adj;
    map<int,nodeinfo> graph;
    char* recv_msg=new char[100];
    recv(sd_tcp,recv_msg,100,0);
    cout<<recv_msg<<'\n';
    adj=parse_on(recv_msg);
    int self=-1;
    int seq=0;
    auto start=steady_clock::now();

    for(auto& neigh:adj) {
        if(neigh.cost==0) {
            self=neigh.id;
        }
        else {
            sockaddr_in sock;
            sock.sin_family=AF_INET;
            sock.sin_port=htons(neigh.port);
            const char* ip_c=neigh.ip.c_str();
            sock.sin_addr.s_addr=inet_addr(ip_c);
            memset(&sock.sin_zero,'\0',8);
            neigh.sock=sock;
        }
    }
    graph.insert({self,to_nodeinfo(adj)});

    string temp=make_lsp(adj);
    const char* msg=temp.c_str();
    for(auto& neigh:adj) {
        if(neigh.id!=self) sendto(sd_udp,msg,temp.length(),0,(sockaddr*)&neigh.sock,sizeof(sockaddr));
    }
    seq++;

    fd_set read_fds;
    auto next_lsp=steady_clock::now()+seconds(10);
    while(1) {

        FD_ZERO(&read_fds);;
        FD_SET(sd_tcp,&read_fds);
        FD_SET(sd_udp,&read_fds);
        int max_fd=max(sd_tcp,sd_udp);
        auto left=max<long long>(0,duration_cast<microseconds>(next_lsp - steady_clock::now()).count());
        timeval tv{ (time_t)(left / 1000000), (suseconds_t)(left % 1000000) };
        if(select(max_fd+1,&read_fds,nullptr,nullptr,&tv)<0) {
            cerr<<"Select error\n";
            return -1;
        }

        if(FD_ISSET(sd_tcp,&read_fds)) {
            char* recv_msg=new char[100];
            recv(sd_tcp,recv_msg,100,0);
            cout<<recv_msg<<'\n';
            vector<neighbour> new_local=parse_on(recv_msg);
            for(auto &old:adj) {
                for(auto neww:new_local) {
                    if(neww.id==old.id) old.cost=neww.cost;
                }
            }
            graph[self]=to_nodeinfo(adj);

            string temp=make_lsp(adj);
            const char* msg=temp.c_str();
            for(auto& neigh:adj) {
                if(neigh.id!=self) sendto(sd_udp,msg,temp.length(),0,(sockaddr*)&neigh.sock,sizeof(sockaddr));
            }
            seq++;
        }

        if(FD_ISSET(sd_udp,&read_fds)) {
            char* s=new char[100];
            unsigned int len=sizeof(sockaddr);

            for(int i=0;i<adj.size();i++) {
                auto sock=adj[i].sock;
                auto ret=recvfrom(sd_udp,s,100,0,(sockaddr*)&sock,&len);

                if(ret>0) {
                    cout<<s<<'\n';
                    nodeinfo ls=parse_lsp(s);
                    if(graph.find(ls.id)==graph.end()) {
                        graph.insert({ls.id,ls});

                        string temp(s);
                        const char* msg=temp.c_str();
                        for(auto& neigh:adj) {
                            if(neigh.id!=self) sendto(sd_udp,msg,temp.length(),0,(sockaddr*)&neigh.sock,sizeof(sockaddr));
                        }
                    }

                    else if(ls.seq>graph[ls.id].seq) {
                        graph[ls.id]=ls;

                        string temp(s);
                        const char* msg=temp.c_str();
                        for(auto& neigh:adj) {
                            if(neigh.id!=self) sendto(sd_udp,msg,temp.length(),0,(sockaddr*)&neigh.sock,sizeof(sockaddr));
                        }
                    }
                    break;
                }
            }
        }

        if (steady_clock::now() >= next_lsp) {
            next_lsp+=seconds(10);
            string temp=make_lsp(adj);
            const char* msg=temp.c_str();
            for(auto& neigh:adj) {
                if(neigh.id!=self) sendto(sd_udp,msg,temp.length(),0,(sockaddr*)&neigh.sock,sizeof(sockaddr));
            }
            seq++;
            start=steady_clock::now();
        }

    }

}
