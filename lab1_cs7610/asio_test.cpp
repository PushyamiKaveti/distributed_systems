#include <iostream>
#include <thread>
#include <time.h>
using namespace std;

void timer_thread(bool& s, int& interval)
{

     //while(1) {
         this_thread::sleep_for(chrono::seconds(interval));
         cout<<"is this printing?";
         s = true;
    // }
}


int main()
{
    bool send_m = false;
    int interval=5;
    int i=20;

    thread t(timer_thread , std::ref(send_m), std::ref(interval));

    t.detach();
    while(1){

        if (send_m){
            cout<<"tick"<<"\n";
            send_m=false;
       }

    }

    return 0;
}