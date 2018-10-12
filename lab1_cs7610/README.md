# distributed_systems cs7610 lab1
Name : Pushyami Kaveti
NUID ": 001950607

This code contains two parts for Total Ordered Multicast and Distributed Snapshot. The details of the code and running instructions are mentioned below

1. Reliable Total Ordered Multicast 
* execute the following commands in terminal
  cd part1/
  mkdir build
  cd build
  cmake ..
  make

* This would have created an executable prj1_tm
* To run enter
  ./prj1_tm -p <portnumber> -h <hostfile-path> -c <No Messages>  -l <loss-details-file-path>

  example:
  ./prj1_tm -p 22222 -h ../../hostnames.txt -c 1 -l ../../file.txt
  


2. Distributed Snapshot capture
* execute the following commands in terminal
  cd part2/
  mkdir build
  cd build
  cmake ..
  make

* This would have created an executable prj1_tm
* To run enter
  ./prj1_ds -p <portnumber> -h <hostfile-path> -c <No Messages> -s <No-Messages-snapshot> -l <loss-details-file-path>
  
  example:
  ./prj1_ds -p 22222 -h ../../hostnames.txt -c 1-s 1 -l ../../file.txt

* here -s is the number of messages after which the snap shot should be instantiated and the value musst be less than -c argument.

---------------------------------------------------------
LOSS FILE DETAILS

---------------------------------------------------------

The losses for the messages can be simulated by creating a file with format as follows

<msg_id> <process_id>
<msg_id> <process_id>
.
.
.

In the code I have set my bounded delay value as 30 seconds. So, if a message gets delayed beyond 30 seconds its considered lost and resent. 

