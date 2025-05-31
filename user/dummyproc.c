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
    
    
    for(int i=0;i<6;i++){
        fork();
    }
    
    //printf("done\n");    
}