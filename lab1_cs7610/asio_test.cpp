#include <iostream>
#include <thread>
#include <time.h>
using namespace std;

void timer_thread(bool& s, int& interval)
{

     while(1) {
         this_thread::sleep_for(chrono::seconds(interval));
         s = true;
     }
}

int main()
{
    bool send_m = false;
    int interval=1;

    thread t(timer_thread , std::ref(send_m) , std::ref(interval));

    while(1){
        if (send_m){
            cout<<"tick"<<"\n";
            send_m=false;
        }
    }
    t.join();
    return 0;
}