#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/pstat.h"
int main(int argc, char* argv[]){
    if(argc==1){
        settickets(-1);
    }
    else{
        settickets(atoi(argv[1]));
    }
    
    
    for(int i=0;i<100;i++){
        fork();
    }
    
    //printf("done\n");    
}

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
