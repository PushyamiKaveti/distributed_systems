//
// Created by pushyamik on 10/7/18.
//

#include <cstdint>
#include <queue>
#include <vector>

#ifndef LAB2_CS7610_MESSAGES_H
#define LAB2_CS7610_MESSAGES_H

#endif //LAB1_CS7610_MESSAGES_H

#define ADD 1
#define DEL 2
#define MAX_PROCESSES 20

using namespace std;

typedef struct {
    uint32_t type; // must be equal to 1
    uint32_t request_id; // the request id
    uint32_t cur_view_id; // the identifier current view at leader
    uint32_t oper_type; // type of operation ADD or DEL
    uint32_t pid;  // process id of the peer to be added or deleted
} REQ_MESG;

typedef struct {
    uint32_t type; // must be equal to 2
    uint32_t request_id; // the request id
    uint32_t cur_view_id; // the identifier current view at header from the leader
    uint32_t pid;  // process id of the OK sender
// the process id of the proposer
} OK_MESG;


typedef struct {
    uint32_t type; // must be equal to 3
    uint32_t newview_id; // the new view id evaluiated by leader
    uint32_t no_members;
    uint32_t member_list[MAX_PROCESSES]; //list of pids of the members of the new view
} NEWVIEW_MESG;

typedef struct {
    uint32_t type; // must be equal to 4
    uint32_t pid; //PID of the process sending heart beat
} HEARTBEAT;




