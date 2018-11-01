# distributed_systems cs7610 lab2
Name : Pushyami Kaveti
NUID ": 001950607

This code contains four parts for Membership protocol. The details of the code and running instructions are mentioned below.
The code requires the terminals of the machines mentioned in the README to be loggedin and opened.

For eaxmple in the hostnames.txt provided you should open three terminals and execute
ssh -t pushyami@login.ccs.neu.edu ssh  pushyami@vdi-linux-030.ccs.neu.edu

ssh -t pushyami@login.ccs.neu.edu ssh  pushyami@vdi-linux-030.ccs.neu.edu

ssh -t pushyami@login.ccs.neu.edu ssh  pushyami@vdi-linux-030.ccs.neu.edu

General Instructions

* execute the following commands in terminal
  cd lab2_cs7610/
  mkdir build
  cd build
  cmake ..
  make

* This would have created an executable prj2_mp
* To run enter
  ./prj2_mp -p <portnumber> -h <hostfile-path> 

  example:
  ./prj1_tm -p 22222 -h ../../hostnames.txt 


1. PART1 : 
  * This is a membership addition. Make sure to run the leader process which is the fisrt host in hostnames.txt first before running others.
  * to add members one by one. run the corresponding host processes one by one and you should see the view ID changing along with memberlist in stdout
  

2.PART2 
  * This is the failure detection part where the processes multicat heartbeat messages to others.
  * This is implemented in the process already. Once the prj2_mp process is runs you should see the heartbeat messages


3. PART 3
 * This is part is the member delation phase. 
 * The code is alsready present in the main executable prj2_mp.
 * To test this start crashing the processes one by one and you should see the view ID updates across all the remaining processes. 
 * The results are printed to stdout

4. PART 4
 * This is the leader failure
 * The code is implemented in the main executable prj2_mp
 * to test this 
	- start the leader process
    - add any members, atleast 2 more for demonstration
    - then crash the leader. The nest lowest PID process should become the leader and initiate the NEW leader protocol


