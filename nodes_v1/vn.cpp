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
    vector<pair<int,int>> adj;
    int seq;
};


vector<neighbour> parse_on(const uint8_t* p, ssize_t n) {
    vector<neighbour> v;
    for (size_t off=0; off+11<=(size_t)n; off+=11) {
        neighbour nb;
        nb.id = p[off]-'A';

        char ipstr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, p+off+1, ipstr, sizeof(ipstr));
        nb.ip = ipstr;

        uint16_t port_n;
        memcpy(&port_n, p+off+5, 2);
        nb.port = ntohs(port_n);

        uint32_t cost_n;
        memcpy(&cost_n, p+off+7, 4);
        nb.cost = (int32_t)ntohl(cost_n);

        memset(&nb.sock, 0, sizeof(nb.sock));
        nb.sock.sin_family = AF_INET;
        nb.sock.sin_port = htons(nb.port);
        inet_pton(AF_INET, nb.ip.c_str(), &nb.sock.sin_addr);

        v.push_back(nb);
    }
    return v;
}

nodeinfo to_nodeinfo(const vector<neighbour>& v, int self) {
    nodeinfo ni;
    ni.id = self;
    ni.seq = 0;
    for (const auto& nb : v) {
        if (nb.id != self && nb.cost >= 0)
            ni.adj.push_back({nb.id, nb.cost});
    }
    return ni;
}

string make_lsp(const nodeinfo& v) {
    string out;
    out.push_back((char)('A' + v.id));

    uint32_t seq_n = htonl((uint32_t)v.seq);
    out.append((const char*)&seq_n, 4);

    out.push_back((char)v.adj.size());

    for (const auto& e : v.adj) {
        out.push_back((char)('A' + e.first));
        uint32_t cost_n = htonl((uint32_t)e.second);
        out.append((const char*)&cost_n, 4);
    }
    return out;
}

nodeinfo parse_lsp(const uint8_t* p, ssize_t n) {
    nodeinfo ls;
    ls.id = -1;           // -1 means malformed
    ls.seq = -1;
    if (n < 6) return ls;

    ls.id = p[0] - 'A';

    uint32_t seq_n;
    memcpy(&seq_n, p + 1, 4);
    ls.seq = (int)ntohl(seq_n);

    size_t count = p[5];
    size_t off = 6;
    for (size_t i = 0; i < count && off + 5 <= (size_t)n; i++, off += 5) {
        int nid = p[off] - 'A';
        uint32_t cost_n;
        memcpy(&cost_n, p + off + 1, 4);
        ls.adj.push_back({nid, (int)(int32_t)ntohl(cost_n)});
    }
    return ls;
}

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
    if (::bind(sd_udp, reinterpret_cast<sockaddr*>(&sock_udp),
               sizeof(sock_udp)) < 0) {
        perror("UDP bind");
        close(sd_udp);
        return 1;
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
    uint8_t reg[6];
    inet_pton(AF_INET, ip_vn.c_str(), reg);
    uint16_t p = htons((uint16_t)port_udp);
    memcpy(reg + 4, &p, 2);
    send(sd_tcp, reg, sizeof(reg), 0);

    vector<neighbour> adj;
    map<int,nodeinfo> graph;
    uint8_t recv_msg[1024];
    ssize_t siz = recv(sd_tcp,recv_msg,1024,0);
    fcntl(sd_tcp, F_SETFL, O_NONBLOCK);
    if(siz<=0) {cout << "Oracle Node closed the TCP connection.\n"; return -1;}
    adj=parse_on(recv_msg,siz);
    int self=-1;
    int seq=-1;
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
    seq++;
    graph.insert({self,to_nodeinfo(adj,self)});
    graph[self].seq=seq;

    string temp=make_lsp(graph[self]);
    const char* msg=temp.c_str();
    for(auto& neigh:adj) {
        if(neigh.id!=self) sendto(sd_udp,msg,temp.length(),0,(sockaddr*)&neigh.sock,sizeof(sockaddr));
    }

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
            uint8_t recv_msg[1024];
            ssize_t siz = recv(sd_tcp,recv_msg,1024,0);
            cout<<recv_msg<<'\n';
            vector<neighbour> new_local=parse_on(recv_msg, siz);
            for(auto &old:adj) {
                for(auto neww:new_local) {
                    if(neww.id==old.id) old.cost=neww.cost;
                }
            }
            seq++;
            graph[self]=to_nodeinfo(adj,self);
            graph[self].seq=seq;

            string temp=make_lsp(graph[self]);
            const char* msg=temp.c_str();
            for(auto& neigh:adj) {
                if(neigh.id!=self) sendto(sd_udp,msg,temp.length(),0,(sockaddr*)&neigh.sock,sizeof(sockaddr));
            }
        }

        if(FD_ISSET(sd_udp,&read_fds)) {
            uint8_t s[1024];
            unsigned int len=sizeof(sockaddr);

            sockaddr_in sock;
            ssize_t ret=recvfrom(sd_udp,s,1024,0,(sockaddr*)&sock,&len);

            if(ret>0) {
                cout<<s<<'\n';
                nodeinfo ls=parse_lsp(s,ret);
                if(graph.find(ls.id)==graph.end()) {
                    graph.insert({ls.id,ls});

                    // string temp(s);
                    // const char* msg=temp.c_str();
                    for(auto& neigh:adj) {
                        if(neigh.id!=self) sendto(sd_udp,s,ret,0,(sockaddr*)&neigh.sock,sizeof(sockaddr));
                    }
                }

                else if(ls.seq>graph[ls.id].seq) {
                    graph[ls.id]=ls;

                    // string temp(s);
                    // const char* msg=temp.c_str();
                    for(auto& neigh:adj) {
                        if(neigh.id!=self) sendto(sd_udp,s,ret,0,(sockaddr*)&neigh.sock,sizeof(sockaddr));
                    }
                }
            }
        }

        if(steady_clock::now()>=next_lsp) {
            next_lsp+=seconds(10);
            seq++;
            graph[self].seq=seq;
            string temp=make_lsp(graph[self]);
            const char* msg=temp.c_str();
            for(auto& neigh:adj) {
                if(neigh.id!=self) sendto(sd_udp,msg,temp.length(),0,(sockaddr*)&neigh.sock,sizeof(sockaddr));
            }
            start=steady_clock::now();
        }

    }

}
