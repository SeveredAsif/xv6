#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/pstat.h"
int main(int argc, char* argv){
    struct pstat p;
    
    settickets(5);
    getpinfo(&p);
    // for(int i=0;i<1e9;i++){
    //     fork();
    //     fork();
    //     fork();
    // }
}