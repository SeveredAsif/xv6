// #include "kernel/types.h"
// #include "kernel/stat.h"
// #include "user/user.h"

// int main(int argc, char *argv[]){
//     if (argc < 3)
//     {
//         fprintf(2, "Usage: %s ticket_number iterations\n", argv[0]);
//         exit(1);
//     }

//     int num_tickets = atoi(argv[1]);

//     if(settickets(num_tickets) == -1){
//         printf("settickets %s failed\n", argv[1]);
//     }

//     int num_iterations = atoi(argv[2]);
//     // int num_iterations = 1000000000;

//     int NUM_CHILDREN = 3;

//     int indicator;

//     // testing forked processes
//     for(int i=0; i<NUM_CHILDREN; i++){
//         indicator = fork();
//         if(indicator == 0){
//             // if it is already a child process, no need to fork from that again
//             break;
//         }
//     }

//     int SLEEP_AFTER = num_iterations/10;
//     int SLEEP_FOR = 5;

//     // PARENT
//     if(indicator != 0){
//         // in case of parent
//         // do dummy calculation in a continuos loop
//         int a = 0;
//         for(int i=0; i<num_iterations; i++){
//             a = !a; // dummy calculation
//         }
//     } else{
//         // in case of child
//         // do sleep after certain iterations
//         int a = 0;
//         for(int i=0; i<num_iterations; i++){
//             a = !a; // dummy calculation
//             if(i%SLEEP_AFTER == 0){
//                 sleep(SLEEP_FOR);
//             }
//         }
//     }
// }

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/pstat.h"

int main(int argc, char* argv[]) {
    int tickets = -1;

    if (argc == 2) {
        tickets = atoi(argv[1]);
    }

    // Set tickets for this process
    settickets(tickets);

    int pid = getpid();
    printf("Running dummyproc with %d tickets | PID: %d\n", tickets, pid);

    // Simulate some CPU activity to consume time slices
    for (int i = 0; i < 50; i++) {
        // Do some computation
        volatile int x = 0;
        for (int j = 0; j < 1000000; j++) {
            x += j % 3;
        }
        //fork();
        //printf("dummyproc PID %d - Iteration %d\n", pid, i);
        sleep(20); // Yield the CPU briefly
    }

    printf("dummyproc PID %d finished\n", pid);
    exit(0);
}


// #include "kernel/types.h"
// #include "kernel/stat.h"
// #include "user/user.h"
// #include "kernel/pstat.h"
// int main(int argc, char* argv[]){
//     printf("Running dummyproc %d , PID %d\n",atoi(argv[1]) ,getpid());
//     if(argc==1){
//         settickets(-1);
//     }
//     else{
//         settickets(atoi(argv[1]));
//     }
    
    
//     for(int i=0;;i++){
//         //printf("Running dummyproc %d as in loop %d , PID %d\n",atoi(argv[1]),i ,getpid());
//         fork();
//     }
    
//     //printf("done\n");    
// }

// #include "kernel/types.h"
// #include "kernel/stat.h"
// #include "user/user.h"
// #include "kernel/pstat.h"

// int main(int argc, char* argv[]){
//     int tickets = -1;
//     if(argc > 1){
//         tickets = atoi(argv[1]);
//     }

//     // Apply settickets BEFORE fork so parent has tickets
//     settickets(tickets);

//     for(int i = 0; i < 100; i++) {
//         int pid = fork();
//         if(pid == 0){
//             // In child process
//             settickets(tickets);  // each child gets proper tickets too
//             //while(1) {}           // dummy infinite loop to consume CPU
//         }
//     }

//     // Let parent also run
//     //while(1) {}
//     exit(0);
// }
