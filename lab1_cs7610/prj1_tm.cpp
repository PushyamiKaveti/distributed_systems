//
// Created by pushyamik on 10/3/18.
// TODO : add Gflags for options
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



#define MAXBUFLEN 100
using namespace std;



// preparing the sockets from host names file
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

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
        cout<<"hostnames:\n"<<endl;
        while (getline(f , line))
        {
            hostnames->push_back(line);
            cout<<line<<"\n";
        }
        f.close();
    }

}

void multicast_mesg(int fdmax , fd_set writefds , int receive_fd , void* m){
    //send messages in a loop to all the hosts
    for (int i=0 ; i <=fdmax ;i++)
    {
        if (FD_ISSET(i, &writefds) && i != receive_fd)
        {
            //char mesg[MAXBUFLEN]="hellow";
            //send(i,mesg, strlen(mesg), 0)
            if (send(i,&m, sizeof m, 0) == -1) {
                perror("send");
            }
        }
    }
}

//a timer
void timer_thread(bool& s, int& interval)
{

    while(1) {
        this_thread::sleep_for(chrono::seconds(interval));
        s = true;
    }
}

// function to handle the received messages
void handle_messages(uint32_t ty ,uint32_t pid, int seq , int fdmax, fd_set writefds, int receive_fd, char* buf){

    printf("listener: packet contains \"%d  \"\n", ty);
    switch(ty){
        case 1:
        {
            //handle datamessages
            DataMessage* b = (DataMessage *)buf;
            AckMessage m {2,b->sender,b->msg_id, (uint32_t )(seq+1), pid };

            multicast_mesg(fdmax , writefds, receive_fd, &m);
            break;
        }
        case 2:
        {
            //handle ack messages
            AckMessage* b = (AckMessage *)buf;
            break;
        }

        case 3:
        {
            //handle sequence messages
            SeqMessage* b = (SeqMessage *)buf;
            break;
        }

    }
}

int main(int argc, char *argv[])
{
    struct timeval tv;
    fd_set readfds;
    fd_set writefds;
    fd_set original;
    int fdmax;
    uint32_t pid;
    int receive_fd, sock_fd;
    char s[INET6_ADDRSTRLEN];
    char s_tmp[INET6_ADDRSTRLEN];
    int rv;
    tv.tv_sec = 0;
    tv.tv_usec = 500000;
    char* port = argv[1];
    vector <string> hostnames;
    char buf[MAXBUFLEN];
    int numbytes;
    struct sockaddr_storage their_addr;
    socklen_t addr_len;
    int num_mesgs = 5;
    char host[256];
    char remote_host[256];

    FD_ZERO(&writefds);    // clear the write and temp sets
    FD_ZERO(&readfds);
    FD_ZERO(&original);

    //read the file and get all hostnames
    get_hostnames(argv[2] , &hostnames);
    // setup sockets for each of the hosts ion specified port number
    //for each hostname get addrssinfo
    struct addrinfo hints, *servinfo, *p;
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

        if ((receive_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("listener: socket");
            continue;
        }


        printf("listener: %s\n",inet_ntop(p->ai_family, get_in_addr(p->ai_addr), s, INET6_ADDRSTRLEN));

        if (bind(receive_fd, p->ai_addr, p->ai_addrlen) == -1) {
            close(receive_fd);
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
    FD_SET(receive_fd , &readfds);
    FD_SET(receive_fd , &original);
    fdmax = receive_fd;
    gethostname(host , sizeof (host));
    puts(host);

    //loop through the hostnames
    int c=0;
    for (auto &i : hostnames)
    {

        //for each hostname get addrssinfo
        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_UNSPEC; // set to AF_INET to force IPv4
        hints.ai_socktype = SOCK_DGRAM;
        hints.ai_flags = AI_PASSIVE; // use my IP
        cout<<i<<"\n";
        if ((rv = getaddrinfo( i.c_str(), port, &hints, &servinfo)) != 0) {
            fprintf(stderr, "gaddrinfo: %s\n", gai_strerror(rv));
            return 1;
        }

        // loop through all the results and bind to the first we can
        for(p = servinfo; p != NULL; p = p->ai_next) {

            inet_ntop(p->ai_family, get_in_addr(p->ai_addr), s_tmp, INET6_ADDRSTRLEN);
            getnameinfo(p->ai_addr, p->ai_addrlen, remote_host, sizeof (remote_host), NULL, 0, NI_NUMERICHOST);
            //puts(host);

            if ((sock_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
                perror("remote: socket");
                continue;
            }

            printf("remote : %s\n",s_tmp);

            int res = connect(sock_fd, p->ai_addr, p->ai_addrlen);
            if (res <0)
            {
                perror("remote: unable to connect()");
                continue;
            }
            //add the socket desc to the list of writefds, add to readfds as well
            //store all the socket descriptors in fd sets
            FD_SET(sock_fd , &writefds);
            if (fdmax < sock_fd){
                fdmax = sock_fd;
            }


            if (strcmp(host, i.c_str()) == 0){
                pid = c+1;
                cout<<"process id :"<<pid<<"\n";
            }
            c=c+1;
            break;
        }

        if (p == NULL) {
            fprintf(stderr, "remote: failed to create socket\n");
            return 2;
        }

        freeaddrinfo(servinfo);
    }


    bool send_m = true;
    int interval=10;
    thread timer_(timer_thread , std::ref(send_m) , std::ref(interval));
    c=1;
    // select loop to send and receive messages
    while(1)
    {

        if (send_m){
            if (c <= num_mesgs){
                DataMessage m {1,pid,c,1};
                c=c+1;
                multicast_mesg(fdmax , writefds, receive_fd, &m);
                send_m = false;
            }

        }


        readfds = original;
        rv = select(fdmax+1, &readfds, NULL, NULL, &tv);

        if (rv == -1) {
            perror("select"); // error occurred in select()
            exit(1);
        } else if (rv == 0) {
           // printf("Timeout occurred!  No data after 5 seconds.\n");
        } else {
            // one of the descriptors have data
            for (int i =0; i<=fdmax ; i++){

                if (FD_ISSET(i, &readfds)) {
                    if(i == receive_fd){
                        // received message
                        addr_len = sizeof their_addr;
                        if ((numbytes = recvfrom(i, buf, MAXBUFLEN-1 , 0,
                                                 (struct sockaddr *)&their_addr, &addr_len)) == -1) {
                            perror("recvfrom");
                            exit(1);
                        }

                        printf("listener: got packet from %s\n", inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof s));
                        printf("listener: packet is %d bytes long\n", numbytes);
                        buf[numbytes] = '\0';
                        //check the first few bytes and check the type of the message
                        uint32_t b1;
                        memcpy(&b1 , &buf, sizeof(uint32_t));
                        //handle the message
                        handle_messages(b1 ,pid, 0 ,fdmax, writefds, receive_fd, buf);


                    }

                }

            }


        }
    }
    timer_.join();
    //
    return 0;
}
