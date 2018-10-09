//
// Created by pushyamik on 10/7/18.
//
#include <iostream>
#include <queue>

#include "messages.h"
using namespace std;

int main()
{
    priority_queue<DataMessage, vector<DataMessage>, CompareDataMessage> Q;


    for (int i = 0; i < 5; ++i) {
        DataMessage m{1,1,(uint32_t)i,1};
        Q.push(m);

        // insert an object in priority_queue by using
        // the Person strucure constructor
    }

    while (!Q.empty()) {
        DataMessage p = Q.top();
        Q.pop();
        cout << p.msg_id << " " << p.type << "\n";
    }
    return 0;
}
