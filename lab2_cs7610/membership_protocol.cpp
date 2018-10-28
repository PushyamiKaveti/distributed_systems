//
// Created by pushyamik on 10/23/18.
//


#include <cstdint>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>
#include <sys/types.h>

#include <iostream>
#include <fstream>
#include <vector>
#include <ctime>
#include <thread>
#include "messages.h"
#include <map>

#include <sstream>


#define MAXBUFLEN 100
#define LEADER 1

using namespace std;

//map for process ids and socket descriptors of members
map <uint32_t , int> pid_sock_map;
map <uint32_t , int> pid_sock_read_map;
uint32_t view_id = 0;
vector<uint32_t> membership_list;
int num_hosts = 0;

//function to send a message

void send_ReqMesgs(void* m){
    int sock_fd=0;
    for (int i = 0; i < membership_list.size(); i++)
    {
        if (membership_list[i] != LEADER){
            sock_fd = pid_sock_map.find(i)->second;
            if (send(i, m, sizeof (REQ_MESG), 0) == -1) {
                perror("send");
            }
            cout << "sent Req \n";
        }



    }
}

void multicast_mesgs(void* m, fd_set writefds, int fdmax, uint32_t  ty){
    int s = 0;
    for (int i=0 ; i <=fdmax ;i++)
    {
        if (FD_ISSET(i, &writefds))
        {

            switch (ty){
                case 1 :
                {
                    s = sizeof (REQ_MESG);
                    break;
                }
                case 3 :
                {
                    NEWVIEW_MESG* b = (NEWVIEW_MESG *)m;
                    s = sizeof (NEWVIEW_MESG) + (b->no_members * sizeof(uint32_t));
                    break;
                }

            }

            //cout << "sent message : " << ty << "\n";
            if (send(i, m, s, 0) == -1) {
                    perror("send");
            }
        }
    }
}

int check_membership(){
    int c =0;
    for (int i = 0; i < membership_list.size(); i++)
    {
        if (membership_list[i] != LEADER)
            c++;
    }
    return c;
}
//function to multicast a message


//function to handle the incoming messages

/********************************************************************************************************
 *
 *
 * The MEMBERSHIP PROTOCOL CODE
 *
 *******************************************************************************************************/

// preparing the sockets from host names file
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    cout<<sa->sa_family<<"\n";
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void get_hostnames(char* hostfile, vector <string>* hostnames)
{
    ifstream f (hostfile);
    string line;
    // vector <string> hostnames;
    int i=0;
    if (f.is_open())
    {
        // cout<<"hostnames:\n"<<endl;
        while (getline(f , line))
        {
            hostnames->push_back(line);
            i++;
            //cout<<line<<"\n";
        }
        f.close();
    }
    num_hosts = i;

}

int get_pidofhost( vector<string>& hostnames, char* remote_host){
    bool found = false;
    for (int i = 0; i < hostnames.size(); i++)
        if (strcmp(remote_host, hostnames[i].c_str()) == 0) {
            std::cout << "host is present at index " << i;
            found = true;
            return (i+1);
        }

    if (!found) {
        std::cout << "Unknown host trying to contact";
        return -1;
    }
}

int initialize_sockets(char* port, vector <string> hostnames, fd_set& tcp_fds, fd_set& tcp_original, fd_set& tcp_write_fds, int& tcp_receive_fd, int& fdmax, uint32_t& pid){

    //establish tcp connections beyween processes for snapshot algorithm
    struct addrinfo hints, *servinfo, *p;
    char host[256];
    int rv, sock_fd;
    int yes=1;
    gethostname(host , sizeof (host));
    puts(host);

    // setup socket for listening from other hosts in specified port number
    //for each hostname get addrssinfo

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // set to AF_INET to force IPv4
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, port , &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {

        if ((tcp_receive_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("listener: socket");
            continue;
        }


        setsockopt(tcp_receive_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        if (bind(tcp_receive_fd, p->ai_addr, p->ai_addrlen) == -1) {
            close(tcp_receive_fd);
            perror("listener: bind");
            continue;
        }
        break;
    }

    if (p == NULL) {
        fprintf(stderr, "listener: failed to bind socket\n");
        return 2;
    }

    freeaddrinfo(servinfo);

    // listen
    if (listen(tcp_receive_fd, 15) == -1) {
        perror("listen");
        exit(3);
    }

    fdmax = tcp_receive_fd;
    // add the listener to the master set
    FD_SET(tcp_receive_fd, &tcp_original);
    FD_SET(tcp_receive_fd, &tcp_fds);

    //loop through the hostnames to connect() which means that the port should be up for listening on the remote hosts. which mean bind(), listen() should already be running
    // so let this sleep()
    this_thread::sleep_for(chrono::seconds(5));
    int c=0;
    string leader = hostnames.at(0);
    cout<<"hosts:\n";
    cout<<"-----------------\n";
    for (auto &i : hostnames)
    {
        c=c+1;
        if (strcmp(host, i.c_str()) == 0){
            pid = c;
            cout<<"process id :"<<pid<<"\n";
            if (pid == 1)
                return 0;
            else
            {
                // the current process is not the leader , so try and contact the leader
                //for each hostname get addrssinfo
                memset(&hints, 0, sizeof hints);
                hints.ai_family = AF_UNSPEC; // set to AF_INET to force IPv4
                hints.ai_socktype = SOCK_STREAM;

                if ((rv = getaddrinfo( leader.c_str(), port, &hints, &servinfo)) != 0) {
                    fprintf(stderr, "gaddrinfo: %s\n", gai_strerror(rv));
                    return 1;
                }

                // loop through all the results and bind to the first we can
                for(p = servinfo; p != NULL; p = p->ai_next) {


                    if ((sock_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
                        perror("remote: socket");
                        continue;
                    }
                    //cout<<i<<":";
                    //printf("remote : %s\n",s_tmp);
                    // connect to the leader. Make sure that the leaderis already bound and lilstening to the port
                    int res = connect(sock_fd, p->ai_addr, p->ai_addrlen);
                    if (res <0)
                    {
                        perror("remote: unable to connect()");
                        continue;
                    }
                    //add the socket desc to the list of writefds,
                    FD_SET(sock_fd , &tcp_write_fds);
                    if (fdmax < sock_fd){
                        fdmax = sock_fd;
                    }

                    break;
                }

                if (p == NULL) {
                    fprintf(stderr, "remote: failed to create socket\n");
                    return 2;
                }

                freeaddrinfo(servinfo);
            }
            // exit the initialization as the process has been correctly initialized
            break;
        }


    }
}

int connect_to_new_member(struct sockaddr_storage their_addr, char* port, socklen_t addr_len, int& fdmax){

    int sock_fd;

    if ((sock_fd = socket(AF_UNSPEC, SOCK_STREAM, 0)) == -1) {
            perror("remote: socket");
            return -1;

    }
    //cout<<i<<":";
    //printf("remote : %s\n",s_tmp);
    // connect to the new accepted peer.
    int res = connect(sock_fd,  (struct sockaddr *) &their_addr, addr_len);
    if (res <0)
    {
        perror("remote: unable to connect()");
        return -1;
    }
    //add the socket desc to the list of writefds,
    //FD_SET(sock_fd , &tcp_write_fds);

    if (fdmax < sock_fd){
        fdmax = sock_fd;
    }

    return sock_fd;
}

void handle_messages(uint32_t ty, char* buf){

}


int main(int argc, char *argv[])
{

    // Parse command line arguments
    // -p port -h hostfile -c number of messages

    int cmd_arg;

    int num_mesgs = 1;

    char* port;
    char* hostfile;
    bool simulate_loss=false;
    char* lossfile;


    bool args_provided = false;

    while ((cmd_arg = getopt (argc, argv, "p:h:")) != -1){
        args_provided=true;
        switch (cmd_arg)
        {
            cout<<cmd_arg<<"\n";
            case 'p':
            {
                port = optarg;
                break;
            }

            case 'h':
            {
                hostfile = optarg;
                break;
            }

            case '?':
                if (optopt == 'c' || optopt == 'p'||optopt == 'h')
                    fprintf (stderr, "Option -%c requires an argument.\n", optopt);
                else if (isprint (optopt))
                    fprintf (stderr, "Unknown option `-%c'.\n", optopt);
                else
                    fprintf (stderr, "Unknown option character `\\x%x'.\n", optopt);
                return 1;
            default:
                cout<<"bad argument";
                return 1;
        }
    }
    if(!args_provided || argc <=1){
        fprintf (stderr,"command options not provided\n");
        return 1;
    }

    // All the command line arguments
    //Required variables
    vector <string> hostnames;
    char host[256];
    char remote_host[256];

    struct timeval tv;

    fd_set original;

    fd_set tcp_readfds;
    fd_set tcp_writefds;


    int fdmax;
    uint32_t pid;
    int receive_fd, sock_fd, tcp_receive_fd;
    char s[INET6_ADDRSTRLEN];
    char s_tmp[INET6_ADDRSTRLEN];
    int rv;
    tv.tv_sec = 0;
    tv.tv_usec = 50000;


    char buf[MAXBUFLEN];
    int numbytes;
    struct sockaddr_storage their_addr;
    socklen_t addr_len;


    //---------------------------//
    // INITIALIZE THE TCP SOCKETS//
    //---------------------------//

    //read the file and get all hostnames
    get_hostnames(hostfile , &hostnames);

    // print the current hostname
    gethostname(host , sizeof (host));
    cout<<"My HOST:";
    cout<<host<<"\n";


    FD_ZERO(&tcp_writefds);    // clear the write and temp sets
    FD_ZERO(&tcp_readfds);
    FD_ZERO(&original);

    //Initialize tcp sockets and also updates the process id (pid)
    initialize_sockets(port, hostnames , tcp_readfds, original, tcp_writefds, tcp_receive_fd , fdmax, pid);
    if (pid == 1){
        // This is the leader. Initialize the membership
        membership_list.push_back(pid);
    }
    //sleep for 5 seconds so that we can setup the other processes
    this_thread::sleep_for(chrono::seconds(5));
    uint32_t request_id = 0;
    int new_sock = 0;

    // select loop to send and receive messages
    while(1)
    {

        tcp_readfds = original;
        rv = select(fdmax+1, &tcp_readfds, NULL, NULL, &tv);

        if (rv == -1) {
            perror("select"); // error occurred in select()
            exit(1);
        } else if (rv == 0) {
            //printf("Timeout occurred!  No data after 5 seconds.\n");
        } else {

            // one of the descriptors have data
            for (int i =0; i<=fdmax ; i++){

                if (FD_ISSET(i, &tcp_readfds)) {
                    if (i == tcp_receive_fd) {
                        // handle new connections
                        //accept the connection
                        sock_fd = accept(tcp_receive_fd, (struct sockaddr *) &their_addr, &addr_len);

                        if (sock_fd == -1) {
                            perror("accept");
                        } else {
                            //add the socket to the originak read set
                            FD_SET(sock_fd, &original); // add to master set
                            if (sock_fd > fdmax) {    // keep track of the max
                                fdmax = sock_fd;
                            }
                            printf(" new connection from %s on socket %d\n",
                                   inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *) &their_addr), s,
                                             INET6_ADDRSTRLEN), sock_fd);

                            // if this process is the leader it additionally should call connect() because
                            // this remote process connection leader has accepted just came up and is listening now on port.
                            // after calling connect the leader can use that sock fd to send messages to this new peer
                            if( pid == 1){
                                // connect to the neew member to sent messages and get the socket.
                                new_sock = connect_to_new_member(their_addr, port, addr_len, fdmax );
                                // Initiate the 2PC to add the new member

                                // check if there are other memebers in the membershio list
                                // if so compose a request message
                                if (check_membership() != 0){
                                    // if there are other members in the group send the Req to all those members
                                    REQ_MESG m {1, request_id, view_id, ADD, pid};
                                    // This can be changed as  a multicast to all write fds. Initially it will be empty
                                    //send_ReqMesgs(&m, &tcp_writefds);
                                    multicast_mesgs(&m , tcp_writefds, fdmax, 1);
                                }
                                else{
                                    // if there are no memebers in the group then difectly send NEWVIEW Message to the new member
                                    view_id++;
                                    //get the name of the new peer
                                    getnameinfo( (struct sockaddr *) &their_addr, addr_len, remote_host, sizeof (remote_host), NULL, 0, NI_NUMERICHOST);
                                    // look up the hostnames to get the pid of the peer
                                    int res = get_pidofhost( hostnames, remote_host);
                                    if (res < 0){
                                        cout<<"Unknown peer trying to connect\n";
                                        continue;
                                    }
                                    FD_SET(new_sock, &tcp_writefds);
                                    membership_list.push_back(res);
                                    NEWVIEW_MESG m{3, view_id , (uint32_t ) membership_list.size() , &membership_list[0]};
                                    char* b1= (char *) calloc((sizeof(NEWVIEW_MESG)+ m.no_members* sizeof(uint32_t)), sizeof(char));
                                    memcpy( b1, &m, (sizeof(NEWVIEW_MESG)+ m.no_members* sizeof(uint32_t)));
                                    multicast_mesgs(b1 , tcp_writefds, fdmax, 3);

                                }


                            }


                        }

                    } else {
                        // receiving in any other sockets means it is getting messages from peers (here it is just the leader for membership protocol) connected to this
                        // handle data from a remote host
                        if ((numbytes = recv(i, buf, sizeof buf, 0)) <= 0) {
                            // got error or connection closed by client
                            if (numbytes == 0) {
                                // connection closed
                                //printf("selectserver: socket %d hung up\n", i);
                            } else {
                                perror("recv");
                            }

                        } else {
                            // we got data on tcp connection different types of handling messages depending on leader or not
                            buf[numbytes] = '\0';
                            //check the first few bytes and check the type of the message
                            uint32_t b1;
                            memcpy(&b1, &buf, sizeof(uint32_t));
                            //handle the message
                            //handle_messages(buf, b1);

                        }

                    }
                }


            }


        }


    }

    //
    return 0;
}
