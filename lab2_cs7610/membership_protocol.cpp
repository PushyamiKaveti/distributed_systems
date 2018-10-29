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
#include <typeinfo>


#define MAXBUFLEN 100
#define LEADER 1
#define TIMEOUT 1

using namespace std;

//map for process ids and socket descriptors of members
map <uint32_t , int> pid_sock_membermap; // only require by leader to check for the members ids and theior corrsponidng sockets.
// Will be needed when removing a peer because we have to delete the socket from the list as well.
// Alternatively we can each time go through the memeship list and get the socket ids from the map, send while doing multicast instead of using writefds
map< uint32_t , pair<uint32_t, int >> request_map; // mapping between request id and (pid, socket)

REQ_MESG pending_request; // This only for peers and not for leader

uint32_t view_id = 0;
vector<uint32_t> membership_list;
multimap <uint32_t , OK_MESG> OK_q;

int num_hosts = 0;
bool connection_established = false;

//function to send a message

/*void send_ReqMesgs(void* m){
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
}*/

//a timer thread to send heartbeats
void periodic_timer_thread(bool& s)
{

    while(1) {
        this_thread::sleep_for(chrono::seconds(TIMEOUT));
        s = true;
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
                case 2:
                {
                    //sent only in case of peers
                    s = sizeof(OK_MESG);
                    break;
                }
                case 3 :
                {
                    NEWVIEW_MESG* b = (NEWVIEW_MESG *)m;
                    s = sizeof (NEWVIEW_MESG) + (b->no_members * sizeof(uint32_t));
                    break;
                }
                case 4 :
                {
                    s = sizeof(HEARTBEAT);
                    break;
                }

            }

            cout << "sent message of type : " << ty << "\n";
            if (send(i, m, s, 0) == -1) {
                    perror("sent message");
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
    else if (sa->sa_family == AF_INET6){
        cout<<sa->sa_family<<"\n";
        return &(((struct sockaddr_in6*)sa)->sin6_addr);
    }
    else
        cout<<"ERROR in accept. BAD ADDRESS\n";

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
    struct addrinfo hints, *servinfo, *p;
    char s[256];
    int rv;
    for (int i = 1; i < hostnames.size(); i++){
        //for each hostname get addrssinfo
        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_UNSPEC; // set to AF_INET to force IPv4
        hints.ai_socktype = SOCK_DGRAM;

        if ((rv = getaddrinfo( hostnames[i].c_str(), "22222", &hints, &servinfo)) != 0) {
            fprintf(stderr, "gaddrinfo: %s\n", gai_strerror(rv));
            return 1;
        }

        // loop through all the results and bind to the first we can


        getnameinfo(servinfo->ai_addr, servinfo->ai_addrlen, s, sizeof (s), NULL, 0, NI_NUMERICHOST);
        cout<<"host : "<<s<<"\n";
        if (strcmp(remote_host, s) == 0) {
            cout << "host is present at index " << i;
            found = true;
            return (i+1);
        }
    }


    if (!found) {
        std::cout << "Unknown host trying to contact";
        return -1;
    }
}


int initialize_udp_sockets(char* port,vector<string> hostnames , fd_set& udp_readfds, fd_set& original, fd_set& udp_writefds,int& udp_receive_fd , int& fdmax){
    //---------------------------//
    // INITIALIZE THE UDP SOCKETS//
    //---------------------------//
    char host[256];
    struct addrinfo hints, *servinfo, *p;
    int rv, sock_fd;

    gethostname(host , sizeof (host));
    cout<<"My HOST:";
    cout<<host<<"\n";

    // setup sockets for each of the hosts in specified port number
    //for each hostname get addrssinfo

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // set to AF_INET to force IPv4
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {

        if ((udp_receive_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("listener: socket");
            continue;
        }

        //printf("listener: %s\n",inet_ntop(p->ai_family, get_in_addr(p->ai_addr), s, INET6_ADDRSTRLEN));

        if (bind(udp_receive_fd, p->ai_addr, p->ai_addrlen) == -1) {
            close(udp_receive_fd);
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

    //add the socket to the list of read fds, add to write fds as well
    FD_SET(udp_receive_fd , &udp_readfds);
    FD_SET(udp_receive_fd , &original);
    if (fdmax < udp_receive_fd)
        fdmax = udp_receive_fd;

    //loop through the hostnames

    cout<<"All Hosts:\n";
    cout<<"-----------------\n";
    for (auto &i : hostnames)
    {

        //for each hostname get addrssinfo
        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_UNSPEC; // set to AF_INET to force IPv4
        hints.ai_socktype = SOCK_DGRAM;
        hints.ai_flags = AI_PASSIVE; // use my IP

        if ((rv = getaddrinfo( i.c_str(), port, &hints, &servinfo)) != 0) {
            fprintf(stderr, "gaddrinfo: %s\n", gai_strerror(rv));
            return 1;
        }

        // loop through all the results and bind to the first we can
        for(p = servinfo; p != NULL; p = p->ai_next) {

            //inet_ntop(p->ai_family, get_in_addr(p->ai_addr), s_tmp, INET6_ADDRSTRLEN);
           // getnameinfo(p->ai_addr, p->ai_addrlen, remote_host, sizeof (remote_host), NULL, 0, NI_NUMERICHOST);
            //puts(host);

            if ((sock_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
                perror("remote: socket");
                continue;
            }
            cout<<i<<":";

            int res = connect(sock_fd, p->ai_addr, p->ai_addrlen);
            if (res <0)
            {
                perror("remote: unable to connect()");
                continue;
            }
            //add the socket desc to the list of writefds, add to readfds as well
            //store all the socket descriptors in fd sets
            FD_SET(sock_fd , &udp_writefds);
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
    if (listen(tcp_receive_fd, 10) == -1) {
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
                cout<<"connecting to leader at :"<< leader<<"\n";
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

    int sock_fd, rv;
    struct sockaddr* sa = (struct sockaddr *) &their_addr;
    addr_len = sizeof their_addr;
    char remote_host[256];

    cout<<"safamily : "<<sa->sa_family<<"\n";
    cout<<"addr_len : "<< addr_len;
    cout<<"port : "<< atoi(port);

    struct addrinfo hints, *servinfo, *p;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // set to AF_INET to force IPv4
    hints.ai_socktype = SOCK_STREAM;
    getnameinfo( (struct sockaddr *) &their_addr, addr_len, remote_host, sizeof (remote_host), NULL, 0, NI_NUMERICHOST);
    puts(remote_host);
    if ((rv = getaddrinfo( remote_host, port, &hints, &servinfo)) != 0) {
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

        int res = connect(sock_fd, p->ai_addr, p->ai_addrlen);
        if (res < 0) {
            perror("remote: unable to connect()");
            continue;
        }

        if (fdmax < sock_fd) {
            fdmax = sock_fd;
        }
        break;
    }
    if (p == NULL) {
        fprintf(stderr, "remote: failed to create socket\n");
        return 2;
    }

    freeaddrinfo(servinfo);
   /* if ((sock_fd = socket(sa->sa_family, SOCK_STREAM, 0)) == -1) {
            perror("remote: socket");
            return -1;

    }
    //cout<<i<<":";
    //printf("remote : %s\n",s_tmp);
    // connect to the new accepted peer.
    int res = -1;
    if (sa->sa_family == AF_INET) {

        struct sockaddr_in* addr = (struct sockaddr_in*)sa;
        addr->sin_port = atoi(port);
        res = connect(sock_fd, (struct sockaddr *) addr, addr_len);
    }
    else if (sa->sa_family == AF_INET6){
        struct sockaddr_in6* addr = (struct sockaddr_in6*)sa;
        addr->sin6_port = atoi(port);
        res = connect(sock_fd,  (struct sockaddr *)addr, addr_len);
    }

    if (res <0)
    {
        perror("remote: unable to connect()");
        return -1;
    } */

    //add the socket desc to the list of writefds,
    //FD_SET(sock_fd , &tcp_write_fds);



    return sock_fd;
}

bool check_oks( uint32_t request_id){

    int num_acks = OK_q.count(request_id);
    //cout<<"checking if all acks are received\n";
    //check from all memebers except from the leader so -1
    if (num_acks == (membership_list.size() -1)){
        return true;
    }
    return false;

}

void handle_messages(char* buf, uint32_t ty, fd_set tcp_writefds , int fdmax, uint32_t pid) {

    printf(" Received message with type : \"%d  \"\n", ty);
    switch (ty) {
        case 1: {

            //handle datamessages
            REQ_MESG *b = (REQ_MESG *) buf;

            // save the data into a local message buffer ( buffer because there might be cases where the Req has not yet processed, but we received another Request to add)
            // But for this project it is just peers adding one by one . so May be just a variable might suffice
            pending_request = *b;

            // send an OK message
            OK_MESG m{2, b->request_id, b->cur_view_id, pid};
            // we can use multi cast because a peer has only leader in the writefds
            multicast_mesgs( &m, tcp_writefds, fdmax, 2);
            break;
        }
        case 2: {
             // ADD check cndition if this is the lader process or not
             if (pid != 1){
                 cout<<"OK message received inside a peer. SOMETHING IS WRONG \n";
                 return;
             }
            //handle OK messages
            OK_MESG *b = (OK_MESG *) buf;
            //cout<<"received ACK for msg:"<<b->msg_id<<"\n";
            OK_q.insert(pair<uint32_t, OK_MESG>(b->request_id, *b));
            if (check_oks(b->request_id)) {
              //change the view and communicate it to all the peers
                view_id++;
                map < uint32_t , pair <uint32_t , int>>::iterator it = request_map.find(b->request_id);
                int new_sock = -1;
                uint32_t new_pid = 0;
                if (it != request_map.end()) {
                    new_pid = it->second.first;
                    new_sock = it->second.second;
                }
                else{
                    cout<<"No request pending to add the peer. SOMETHING WRONG\n";
                    return;
                }
                FD_SET(new_sock, &tcp_writefds);
                membership_list.push_back(new_pid);
                pid_sock_membermap.insert(pair<uint32_t, int>(new_pid, new_sock));
                request_map.erase(it);

                NEWVIEW_MESG m{3, view_id , (uint32_t ) membership_list.size() , &membership_list[0]};
                char* b1= (char *) calloc((sizeof(NEWVIEW_MESG)+ m.no_members* sizeof(uint32_t)), sizeof(char));
                memcpy( b1, &m, (sizeof(NEWVIEW_MESG)+ m.no_members* sizeof(uint32_t)));
                multicast_mesgs(b1 , tcp_writefds, fdmax, 3);
                //TODO: When a leader updates its view add the new members to the heartbeat timeout map and remove the
                // TODO : deleted members from the map and start the timeout thread and reset it everytime you receuived a heartbeat
            }
            break;
        }
        case 3:{

            NEWVIEW_MESG* b1 = (NEWVIEW_MESG *) buf;
            NEWVIEW_MESG b;

            memcpy(&b, &buf, (sizeof(NEWVIEW_MESG)+ b1->no_members* sizeof(uint32_t)));
            //print the new view
            cout<< "NEW VIEW_ID: "<<b->newview_id<<'\n';
            cout<<"No of members in new view : "<<b->no_members<<"\n";

            for (uint32_t *i = b->member_list; *i ; ++i){
                cout<< *i <<" , ";
            }
            cout<<"\n";
            view_id = b->newview_id;
            cout<<"here"<<"\n";
            membership_list.assign( b->member_list , b->member_list+ b->no_members);

            //TODO: When a peer updates its view add the new members to the heartbeat timeout map and remove the
            // TODO : deleted members from the map and start the timeout thread and reset it everytime you receuived a heartbeat
            break;
        }

    }
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

    fd_set udp_readfds;
    fd_set udp_writefds;


    int fdmax;
    uint32_t pid;
    int udp_receive_fd, sock_fd, tcp_receive_fd;
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

    FD_ZERO(&udp_writefds);    // clear the write and temp sets
    FD_ZERO(&udp_readfds);


    // Initialize UDP sockets
    initialize_udp_sockets(port, hostnames , udp_readfds, original, udp_writefds, udp_receive_fd , fdmax);

    if (pid == 1){
        // This is the leader. Initialize the membership
        membership_list.push_back(pid);
    }
    //sleep for 5 seconds so that we can setup the other processes
    this_thread::sleep_for(chrono::seconds(5));
    uint32_t request_id = 0;
    int new_sock = -1;

    bool send_HB = true;
    //timer for sending messages at regular time intervals. MIGHT HAV ETO START AFTER CONNECTION ESTABLISHED> ANY PROBLEM WITH THIS??
    thread timer_(periodic_timer_thread , std::ref(send_HB));


    // select loop to send and receive messages
    while(1)
    {
        //if (send_HB && connection_established){
            //send the heartbeat message
        //    HEARTBEAT h{4, pid};
       //     multicast_mesgs(&h, udp_writefds, fdmax, 4);
        //    send_HB = false;
       // }

        //TODO: check for timer gone off
        //TODO: If so print out the peer has gone down and mark the bool false in the map. Here the timer thread exits

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
                        //VERY IMPORTANT LINE OF CODE TO GET HOST ADDRESS
                        addr_len = sizeof their_addr;
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
                             connection_established = true;
                            // if this process is the leader it additionally should call connect() because
                            // this remote process connection leader has accepted just came up and is listening now on port.
                            // after calling connect the leader can use that sock fd to send messages to this new peer
                            if( pid == 1){
                                // connect to the neew member to sent messages and get the socket.
                                new_sock = connect_to_new_member(their_addr, port, addr_len, fdmax );
                                // Initiate the 2PC to add the new member
                                //get the name of the new peer
                                getnameinfo( (struct sockaddr *) &their_addr, addr_len, remote_host, sizeof (remote_host), NULL, 0, NI_NUMERICHOST);
                                cout<<"Remote host who is trying to connect is : "<<remote_host<<"\n";
                                // look up the hostnames to get the pid of the peer
                                int new_pid = get_pidofhost( hostnames, remote_host);
                                cout<<"\nhost PID is :"<<new_pid<<"\n";
                                if (new_pid < 0){
                                    cout<<"Unknown peer trying to connect\n";
                                    continue;
                                }

                                // check if there are other memebers in the membershio list
                                // if so compose a request message
                                if (check_membership() != 0){
                                    // if there are other members in the group send the Req to all those members
                                    REQ_MESG m {1, request_id, view_id, ADD, (uint32_t )new_pid};
                                    // This can be changed as  a multicast to all write fds. Initially it will be empty
                                    //send_ReqMesgs(&m, &tcp_writefds);
                                    //insert the request -> (pid, socket) mapping inside leader map.
                                    pair<uint32_t, int> req_pair(new_pid, new_sock);
                                    request_map.insert(pair< uint32_t , pair<uint32_t, int>> (request_id, req_pair));
                                    multicast_mesgs(&m , tcp_writefds, fdmax, 1);
                                }
                                else{
                                    // if there are no memebers in the group then difectly send NEWVIEW Message to the new member
                                    view_id++;
                                    //get the name of the new peer
                                    //getnameinfo( (struct sockaddr *) &their_addr, addr_len, remote_host, sizeof (remote_host), NULL, 0, NI_NUMERICHOST);

                                    // look up the hostnames to get the pid of the peer
                                    //int res = get_pidofhost( hostnames, remote_host);

                                    //if (res < 0){
                                    //    cout<<"Unknown peer trying to connect\n";
                                    //    continue;
                                    //}

                                    FD_SET(new_sock, &tcp_writefds);
                                    membership_list.push_back(new_pid);
                                    pid_sock_membermap.insert(pair<uint32_t, int>(new_pid, new_sock));
                                    NEWVIEW_MESG m{3, view_id , (uint32_t ) membership_list.size() , &membership_list[0]};

                                    char* b1= (char *) calloc((sizeof(NEWVIEW_MESG)+ m.no_members * sizeof(uint32_t)), sizeof(char));
                                    memcpy( b1, &m, (sizeof(NEWVIEW_MESG)+ m.no_members* sizeof(uint32_t)));
                                    multicast_mesgs(b1 , tcp_writefds, fdmax, 3);
                                    // TODO: When a leader updates its view add the new members to the heartbeat timeout map and remove the
                                    // TODO : deleted members from the map and start the timeout thread and reset it everytime you receuived a heartbeat

                                }


                            }


                        }

                    }
                    else if (i == udp_receive_fd){
                        // received message
                        addr_len = sizeof their_addr;
                        if ((numbytes = recvfrom(i, buf, MAXBUFLEN - 1, 0, (struct sockaddr *) &their_addr, &addr_len)) == -1) {
                            perror("recvfrom");
                            exit(1);
                        }

                        //printf("got packet from %s", inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof s));

                        buf[numbytes] = '\0';
                        //check the first few bytes and check the type of the message
                        uint32_t b1;
                        memcpy(&b1, &buf, sizeof(uint32_t));
                        //This must be heartbeat message
                         //TODO:
                        //handle the message
                        // TODO: check if the member is not timed out from the map pid->timeout bool
                        // TODO: if not timed out reset the timer if its there
                        //TODO: else print unexpected behavior already time dout but received heart beat

                    }
                    else {
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
                            uint32_t typ;
                            memcpy(&typ, &buf, sizeof(uint32_t));
                            //handle the message
                            handle_messages(buf, typ, tcp_writefds , fdmax, pid);

                        }

                    }
                }


            }


        }


    }

    //
    return 0;
}
