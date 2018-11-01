# distributed_systems cs7610 lab2
Name : Pushyami Kaveti
NUID ": 001950607

This code contains four parts for Membership protocol. The details of the code and running instructions are mentioned below.
The code requires the terminals of the machines mentioned in the README to be loggedin and opened.

For eaxmple in the hostnames.txt provided you should open three terminals and execute
ssh -t pushyami@login.ccs.neu.edu ssh  pushyami@vdi-linux-031.ccs.neu.edu

ssh -t pushyami@login.ccs.neu.edu ssh  pushyami@vdi-linux-032.ccs.neu.edu

ssh -t pushyami@login.ccs.neu.edu ssh  pushyami@vdi-linux-033.ccs.neu.edu

General Instructions

* execute the following commands in terminal
  cd lab2_cs7610/
  mkdir build
  cd build
  cmake ..
  make

* This would have created an executable prj2_mp
* To run enter
  ./prj2_mp -p <portnumber> -h <hostfile-path> -f <pid-at-failure> -l <pid-for-request-loss> -o <oper>
   
  example without leader failure:
  ./prj2_mp -h ../hostnames.txt -p 22222

  example with leader failure and pending:
  ./prj2_mp -h ../hostnames.txt -p 22222 -f 4 -l 2 -o 2

This means that the leader failure will be simulated when process 4 (-f 4) will be deleted (-o 2) and request message loss will be simulated at process id 2. The -o can be either 1 = ADD or 2 = DEL.  


1. PART1 : 
  * This is a membership addition. Make sure to run the leader process which is the first host in hostnames.txt first before running others.
  TESTCASE 1:
  * To add members one by one. run the corresponding host processes one by one and you should see the view ID changing along with memberlist in stdout
  

2.PART2 
  * This is the failure detection part where the processes multicat heartbeat messages to others.
  * This is implemented in the process already. Once the prj2_mp process is runs you should see the heartbeat messages.
  TESTCASE 2:
  * start the leader and add few other processes one by one. Then crash one process. You should see "Peer <pid> not reachable" in stdout for all the processes.


3. PART 3
 * This is part is the member delation phase. 
 * The code is alsready present in the main executable prj2_mp.
 TESTCASE 3:
 * To test this start crashing the processes one by one and you should see the view ID updates across all the remaining processes. 
 * The results are printed to stdout

4. PART 4
 * This is the leader failure
 * The code is implemented in the main executable prj2_mp
 * to test this 
	
    TEST CASE 4-1:
      - start the leader process ./prj2_mp -h ../hostnames.txt -p 22222 
      - add any members by running the code.
      - crash the leader. The next lowest PID process should become the leader and initiate the NEW leader protocol. If there are any pending operations they will be reinitiated.
    TEST CASE 4-2:
      - To simulated pending operations execute the code as mentioned above in "example with leader failure> for the leader process. 
      ./prj2_mp -h ../hostnames.txt -p 22222 -f 4 -l 2 -o 2

      - run other processes normally ./prj2_mp -h ../hostnames.txt -p 22222 
      - To run TESTCASE 4  as mentioned in assignment
        run ./prj2_mp -h ../hostnames.txt -p 22222 -f <some pid> -l 2 -o 1
         


