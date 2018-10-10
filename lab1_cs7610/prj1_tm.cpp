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

//map for process ids and socket descriptors
map <uint32_t , int> pid_sock_map;
//map for storing data messages in case we would like to display the messahes - CAN BE REMOVED
map <uint32_t , DataMessage*> mid_message_map;
//map for message delivery status _ CAN BE REMOVED. already stored in the structure finalmesgq
map <uint32_t , bool > mid_delivery_status_map;
//map of messages and ack to keep track of receibed acks
multimap <uint32_t , AckMessage> ack_q;
//resend map
map <uint32_t , pair <bool, fd_set > > resend_map;
int bounded_delay = 30;

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
       // cout<<"hostnames:\n"<<endl;
        while (getline(f , line))
        {
            hostnames->push_back(line);
            //cout<<line<<"\n";
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

            break;
        }
    }

    if (send(sock_fd, m, s, 0) == -1) {
        perror("send");
    }
}

void multicast_mesg(int fdmax , fd_set writefds , int receive_fd , void* m, uint32_t ty, uint32_t loss_pid){
    int s=0;
    //send messages in a loop to all the hosts
    int loss_fd = -1;
    map<uint32_t , int>::iterator it = pid_sock_map.find(loss_pid);
    if (it != pid_sock_map.end())
        loss_fd= it->second;
    cout<<"simulating messages loss for pid :"<<loss_pid<<"\n";
    for (int i=0 ; i <=fdmax ;i++)
    {
        if (FD_ISSET(i, &writefds) && i != receive_fd)
        {

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
            if (i == loss_fd) {
                cout << "loss for " << loss_pid<<"\n";

            }
            else {
                cout << "sent message : " << ty << "\n";
                if (send(i, m, s, 0) == -1) {
                    perror("send");
                }
            }
        }
    }
}

//a timer
void periodic_timer_thread(bool& s, int& interval)
{

    while(1) {
        this_thread::sleep_for(chrono::seconds(interval));
        s = true;
    }
}


bool check_acks( uint32_t msg_id){

    int num_acks = ack_q.count(msg_id);
    cout<<"checking if all acks are received\n";
    if (num_acks == pid_sock_map.size()){
        return true;
    }
    return false;

}

void get_missing_acks(uint32_t mid){
    pair<multimap<uint32_t, AckMessage>::iterator, multimap<uint32_t, AckMessage>::iterator> ret;
    map<uint32_t , int > ::iterator pid_itr;
    ret = ack_q.equal_range(mid);
    bool found, resend_ack=false;
    fd_set resend_fds;
    // go through all the pids and check if that process sent an ack
    for (pid_itr = pid_sock_map.begin() ; pid_itr != pid_sock_map.end(); ++pid_itr){
        found=false;
        for (std::multimap<uint32_t, AckMessage>::iterator it = ret.first; it != ret.second; ++it) {
            AckMessage am = it->second;
            if(am.proposer == pid_itr->first){
                found = true;
                break;
            }

        }
        if (!found){
            cout<<"ack not received from pid:"<<pid_itr->first<<"\n";
            resend_ack = true;
            FD_SET(pid_itr->second, &resend_fds);
        }

    }
    pair<bool, fd_set> ack_pair (resend_ack , resend_fds);
    map<uint32_t , pair<bool,fd_set>>::iterator itr ;
    //set the rsend boolean to true and fill the fd-set with socket fds of pids to be used for resend
    resend_map.find(mid)->second =ack_pair;
    for (itr = resend_map.begin() ; itr != resend_map.end(); ++itr) {
        uint32_t  msg_id = itr->first;
        pair<bool, fd_set> resend_pair = itr->second;
        cout << "checking resend map for message :" << msg_id << "\n";
        cout<<resend_pair.first<<"\n";
    }
}

void timeout_thread(uint32_t mid)
{

    this_thread::sleep_for(chrono::seconds(bounded_delay));
    map<uint32_t , pair <bool, fd_set>> ::iterator it = resend_map.find(mid);
    if (it != resend_map.end()){
        cout<<"timeout for acks started for message:"<<mid<<"\n";
        if (!check_acks(mid)){
            cout<<"getting missing acks\n";
            get_missing_acks(mid);
        }
        else
            cout<<"NO missing ACKS\n";
    }


}

void check_resend(uint32_t pid, int fdmax, int receive_fd){
    map<uint32_t , pair<bool,fd_set>>::iterator itr ;
    uint32_t msg_id;
    for (itr = resend_map.begin() ; itr != resend_map.end(); ++itr){
        msg_id = itr->first;
        pair<bool, fd_set> resend_pair = itr->second;
        //cout<<"checking resend for message :"<<msg_id<<"\n";
        // check if the resend flag is set
        if (resend_pair.first){
            cout<<pid <<" : resending message :"<<msg_id;
            //create Data message
           // DataMessage m {1,pid,msg_id,1};

            //multicast the message to the group with socket descriptors ( writefds)
            //multicast_mesg(fdmax , itr->second.second, receive_fd, &m , 1);
            //cout<<pid<<" : resent message: "<<msg_id<<"\n";
            fd_set resend_fds;
            FD_ZERO(&resend_fds);
            pair<bool, fd_set> ack_pair (false , resend_fds);
            itr->second =ack_pair;
            //create a timeout thread abd detach to run independently. When the timeout happens and all acks are not received it updates the resend map
            //thread t(timeout_thread , msg_id);
            //t.detach();
        }
    }
    //cout<<"exiting check resend\n";
}


std::priority_queue<Mesg_pq, std::vector<Mesg_pq>, CompareMessage> reorder_queue(SeqMessage* b, priority_queue <Mesg_pq, vector<Mesg_pq>, CompareMessage>& final_mesg_q){
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
void handle_messages(uint32_t ty ,uint32_t pid, queue<uint32_t > mid_q, int fdmax, fd_set writefds, int receive_fd, int& a_seq, int& p_seq, priority_queue <Mesg_pq, vector<Mesg_pq>, CompareMessage>& final_mesg_q, char* buf){

    printf(" with type : \"%d  \"\n", ty);
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

            Mesg_pq m_pq{b->msg_id, b->sender, (uint32_t )(p_seq), pid, false};
            final_mesg_q.push(m_pq);

            AckMessage m {2,b->sender,b->msg_id, (uint32_t )(p_seq), pid };
            //send Ack message tpo the sender of the datamessage
            int sock_fd = pid_sock_map.find(b->sender)->second;
            //cout<<"sending ack to sender :"<< pid_sock_map.find(b->sender)->first<<"\n";
            send_mesg( sock_fd , &m , 2);
            break;
        }
        case 2:
        {
            //handle ack messages
            AckMessage* b = (AckMessage *)buf;
            //cout<<"received ACK for msg:"<<b->msg_id<<"\n";
            ack_q.insert( pair <uint32_t , AckMessage> (b->msg_id , *b));
            if (check_acks(b->msg_id)) {
                //find the maximum of the sequence numbers proposed
                pair<multimap<uint32_t, AckMessage>::iterator, multimap<uint32_t, AckMessage>::iterator> ret;
                ret = ack_q.equal_range(b->msg_id);
                uint32_t max_seq = 0;
                uint32_t max_seq_proposer = 0;
                for (std::multimap<uint32_t, AckMessage>::iterator it = ret.first; it != ret.second; ++it) {
                    AckMessage am = it->second;
                    //cout<<"ack details: datamesg sender :"<<am.sender <<" proposer :"<<am.proposer<<"\n";
                    if (am.proposed_seq > max_seq) {
                        max_seq = am.proposed_seq;
                        max_seq_proposer = am.proposer;
                    }
                }
                cout << "received all ACKS\n";
                SeqMessage seq_m{3, b->sender, b->msg_id, max_seq, max_seq_proposer};
                multicast_mesg(fdmax, writefds, receive_fd, &seq_m, 3, 0);

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
                if (p.msg_id == b->msg_id && p.sender == b->sender){
                    Mesg_pq m_pq {p.msg_id ,b->sender, b->final_seq, b->final_seq_proposer,true};
                    tmp_q.push(m_pq);
                }
                else{
                    tmp_q.push(p);
                }
                final_mesg_q.pop();


            }
            final_mesg_q = tmp_q;
            //deliver the low sequence and deliverable messages
            //cout << final_mesg_q.size()<<"\n";

            while (!final_mesg_q.empty()){
                Mesg_pq p = final_mesg_q.top();
                if(p.deliver){

                    cout<<pid<<" : Processed message :"<<p.msg_id<<"from sender :"<<p.sender<<" with seq :("<<p.final_seq<<","<<p.final_seq_proposer<<")\n";
                    final_mesg_q.pop();
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
    tv.tv_usec = 50000;
    char* port = argv[1];
    vector <string> hostnames;
    char buf[MAXBUFLEN];
    int numbytes;
    struct sockaddr_storage their_addr;
    socklen_t addr_len;
    int num_mesgs = 1;
    char host[256];
    char remote_host[256];
    queue <uint32_t > mid_q;
    int agreed_seq = 0;
    int proposed_seq = 0;
    priority_queue <Mesg_pq, vector<Mesg_pq>, CompareMessage> final_mesg_q;
    uint32_t loss_pid=0;
    if(argc == 4){
        loss_pid = (uint32_t ) atoi(argv[3]) ;
        cout<<loss_pid<<","<<argv[2]<<"\n";
    }


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
    cout<<"hosts:\n";
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

            inet_ntop(p->ai_family, get_in_addr(p->ai_addr), s_tmp, INET6_ADDRSTRLEN);
            getnameinfo(p->ai_addr, p->ai_addrlen, remote_host, sizeof (remote_host), NULL, 0, NI_NUMERICHOST);
            //puts(host);

            if ((sock_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
                perror("remote: socket");
                continue;
            }
            cout<<i<<":";
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
    //sleep for 5 seconds so that we can setup the other processes
    this_thread::sleep_for(chrono::seconds(5));
    //boolean for sending messages
    bool send_m = true;
    //time interval between sending consecutive messages
    int interval=10;
    //timer for sending messages at regular time intervals
    thread timer_(periodic_timer_thread , std::ref(send_m) , std::ref(interval));
    //message counter
    int counter=1;

    // select loop to send and receive messages
    while(1)
    {
        // if the timer goes off send the next message
        if (send_m){
            if (counter <= num_mesgs){
                //create Data message
                DataMessage m {1,pid,(uint32_t )counter,1};

                cout<<pid<<" : sent message: "<<counter<<"\n";
               // fd_set resend_fds;
               // FD_ZERO(&resend_fds);
                //resend_map.insert(pair <uint32_t, pair<bool,fd_set>>((uint32_t)counter, pair<bool, fd_set> (false,resend_fds)));
                //create a timeout thread abd detach to run independently. When the timeout happens and all acks are not received it updates the resend map
                //thread t(timeout_thread , counter);
                //t.detach();
                counter=counter+1;
                //multicast the message to the group with socket descriptors ( writefds)
                send_m = false;
            }

        }


        readfds = original;
        rv = select(fdmax+1, &readfds, NULL, NULL, &tv);

        if (rv == -1) {
            perror("select"); // error occurred in select()
            exit(1);
        } else if (rv == 0) {
           //printf("Timeout occurred!  No data after 5 seconds.\n");
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

                        //printf("got packet from %s", inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof s));

                        buf[numbytes] = '\0';
                        //check the first few bytes and check the type of the message
                        uint32_t b1;
                        memcpy(&b1 , &buf, sizeof(uint32_t));
                        //handle the message
                        handle_messages(b1 ,pid, mid_q, fdmax, writefds, receive_fd ,agreed_seq,proposed_seq, final_mesg_q , buf);


                    }

                }

            }


        }

        //cout<<"select loop"<<"\n";
        //check if the resend map is set and resend the data messages which are lost
        //check_resend(pid, fdmax, receive_fd );
    }
    timer_.join();
    //
    return 0;
}
