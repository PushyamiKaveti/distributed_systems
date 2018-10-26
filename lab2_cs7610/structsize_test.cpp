//
// Created by pushyamik on 10/26/18.
//

#include <cstring>
#include "messages.h"
#include <iostream>

using namespace std;

int main(int argc, char *argv[]) {
    vector<uint32_t> members;
    members.push_back(11);
    members.push_back(22);
    vector<uint32_t> members2;
    members2.push_back(11);

    NEWVIEW_MESG m{3, 1, 2, &members[0]};

    char* b1= (char *) calloc((sizeof(NEWVIEW_MESG)+ m.no_members* sizeof(uint32_t)), sizeof(char));

    memcpy( b1, &m, (sizeof(NEWVIEW_MESG)+ m.no_members* sizeof(uint32_t)));
    NEWVIEW_MESG* b = (NEWVIEW_MESG *)b1;
    cout<< b->type<<'\n';
    cout<< b->newview_id<<'\n';
    cout << b->no_members << "\n";
    for (uint32_t *i = b->member_list; *i ; ++i){
        cout<< *i <<"\n";
    }

    members2.assign( b->member_list , b->member_list+ b->no_members);

    for (auto &i : members2){
        cout<<i<<"\n";
    }
}