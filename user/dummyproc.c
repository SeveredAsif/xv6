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
        //printf("dummyproc PID %d - Iteration %d\n", pid, i);
        sleep(2); // Yield the CPU briefly
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
