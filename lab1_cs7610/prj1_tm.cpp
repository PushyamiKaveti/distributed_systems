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
#include <map>
#include <queue>



#define MAXBUFLEN 100
using namespace std;

map <uint32_t , DataMessage*> mid_message_map;
map <uint32_t , bool > mid_delivery_status_map;
multimap <uint32_t , AckMessage> ack_q;
priority_queue <Mesg_pq, vector<Mesg_pq>, CompareMessage> final_mesg_q;

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

void send_mesg(int sock_fd , void* m , uint32_t ty){
    int s = 0 ;
    switch (ty){
        case 1 :
        {
            s = sizeof (DataMessage);
            break;
        }
        case 2 :
        {

            s = sizeof (AckMessage);
            cout<<"sending ack of size"<<s <<"\n";
            break;
        }
    }

    if (send(sock_fd, m, s, 0) == -1) {
        perror("send");
    }
}

void multicast_mesg(int fdmax , fd_set writefds , int receive_fd , void* m, uint32_t ty){
    int s=0;
    //send messages in a loop to all the hosts
    for (int i=0 ; i <=fdmax ;i++)
    {
        if (FD_ISSET(i, &writefds) && i != receive_fd)
        {
            //char mesg[MAXBUFLEN]="hellow";
            //send(i,mesg, strlen(mesg), 0)
            switch (ty){
                case 1 :
                {
                    s = sizeof (DataMessage);
                    break;
                }
                case 3 :
                {
                    s = sizeof (SeqMessage);
                    break;
                }

            }
            if (send(i, m, s, 0) == -1) {
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

bool check_acks(map<uint32_t, int> pid_sock_map, uint32_t msg_id){

    int num_acks = ack_q.count(msg_id);
    if (num_acks == pid_sock_map.size()){
        return true;
    }
    return false;

}

std::priority_queue<Mesg_pq, std::vector<Mesg_pq>, CompareMessage> reorder_queue(SeqMessage* b){
    priority_queue <Mesg_pq, vector<Mesg_pq>, CompareMessage> tmp_q;
    while (!final_mesg_q.empty()) {
        Mesg_pq p = final_mesg_q.top();
        if (p.msg_id == b->msg_id ){
            Mesg_pq m_pq {p.msg_id , b->final_seq, true};
            tmp_q.push(m_pq);
        }
        else{
            tmp_q.push(p);
        }
        final_mesg_q.pop();

    }
    final_mesg_q = tmp_q;
    return tmp_q;
}
// function to handle the received messages
void handle_messages(uint32_t ty ,uint32_t pid, map<uint32_t , int> pid_sock_map, queue<uint32_t > mid_q, int fdmax, fd_set writefds, int receive_fd, int& a_seq, int& p_seq, char* buf){

    printf("listener: packet contains type \"%d  \"\n", ty);
    switch(ty){
        case 1:
        {

            //handle datamessages
            DataMessage* b = (DataMessage *)buf;
            mid_q.push(b->msg_id);
            mid_message_map.insert(pair <uint32_t, DataMessage*> (b->msg_id , b));
            mid_delivery_status_map.insert(pair <uint32_t, bool>(b->msg_id, false));
            //ideally we dont have to give a large sequence number. we can have the proposed sequence number
            // the queue will be reordered when we change the sequnce number
            if (p_seq > a_seq)
                p_seq = p_seq+1;
            else
                p_seq = a_seq+1;

            Mesg_pq m_pq{b->msg_id, b->sender, (uint32_t )(p_seq), false};
            final_mesg_q.push(m_pq);

            AckMessage m {2,b->sender,b->msg_id, (uint32_t )(p_seq), pid };
            //send Ack message tpo the sender of the datamessage
            int sock_fd = pid_sock_map.find(b->sender)->second;
            cout<<"sending ack to sender :"<< pid_sock_map.find(b->sender)->second<<"\n";
            send_mesg( sock_fd , &m , 2);
            break;
        }
        case 2:
        {
            //handle ack messages
            AckMessage* b = (AckMessage *)buf;
            cout<<"received ACK for msg:"<<b->msg_id<<"\n";
            ack_q.insert( pair <uint32_t , AckMessage> (b->msg_id , *b));
            if (check_acks(pid_sock_map, b->msg_id)) {
                //find the maximum of the sequence numbers proposed
                pair<multimap<uint32_t, AckMessage>::iterator, multimap<uint32_t, AckMessage>::iterator> ret;
                ret = ack_q.equal_range(b->msg_id);
                uint32_t max_seq = 0;
                uint32_t max_seq_proposer = 0;
                for (std::multimap<uint32_t, AckMessage>::iterator it = ret.first; it != ret.second; ++it) {
                    AckMessage am = it->second;
                    int a_seq = am.proposed_seq;
                    if (a_seq > max_seq) {
                        max_seq = a_seq;
                        max_seq_proposer = am.proposer;
                    }
                }
                cout << "received all ACKS\n";
                SeqMessage seq_m{3, b->sender, b->msg_id, max_seq, max_seq_proposer};
                multicast_mesg(fdmax, writefds, receive_fd, &seq_m, 3);

                // and multicast the final  seq numbner
            }
            break;
        }

        case 3:
        {
            //handle sequence messages
            SeqMessage* b = (SeqMessage *)buf;
            mid_delivery_status_map[b->msg_id] = true;
            if (b->final_seq > a_seq)
                a_seq = b->final_seq;

            //reorder the queue
            priority_queue <Mesg_pq, vector<Mesg_pq>, CompareMessage> tmp_q;
            while (!final_mesg_q.empty()) {
                Mesg_pq p = final_mesg_q.top();
                if (p.msg_id == b->msg_id ){
                    Mesg_pq m_pq {p.msg_id , b->final_seq, true};
                    tmp_q.push(m_pq);
                }
                else{
                    tmp_q.push(p);
                }
                final_mesg_q.pop();

            }
            final_mesg_q = tmp_q;
            //deliver the low sequence and deliverable messages
            cout << final_mesg_q.size()<<"\n";

            while (!final_mesg_q.empty()){
                Mesg_pq p = final_mesg_q.top();
                if(p.deliver){
                    final_mesg_q.pop();
                    cout<<pid<<" : Processed message :"<<p.msg_id<<"from sender :"<<p.sender<<" with seq :("<<p.final_seq<<","<<b->final_seq_proposer<<")\n";
                }
                else
                    break;
            }
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
    tv.tv_usec = 5000;
    char* port = argv[1];
    vector <string> hostnames;
    char buf[MAXBUFLEN];
    int numbytes;
    struct sockaddr_storage their_addr;
    socklen_t addr_len;
    int num_mesgs = 1;
    char host[256];
    char remote_host[256];
    map <uint32_t , int> pid_sock_map;
    queue <uint32_t > mid_q;
    int agreed_seq = 0;
    int proposed_seq = 0;


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
            c=c+1;
            // add the pid , socked fd pairs to the map
            pid_sock_map.insert(pair <uint32_t , int> (c , sock_fd));

            if (strcmp(host, i.c_str()) == 0){
                pid = c;
                cout<<"process id :"<<pid<<"\n";
            }

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
                multicast_mesg(fdmax , writefds, receive_fd, &m , 1);
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
                        handle_messages(b1 ,pid,pid_sock_map, mid_q, fdmax, writefds, receive_fd ,agreed_seq,proposed_seq,buf);


                    }

                }

            }


        }
    }
    timer_.join();
    //
    return 0;
}
