#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/stats.h"

int main(int argc, char *argv[]){
    struct syscall_stat s;
    if (argc != 2) {
        for(int i=0;i<23;i++){
            int success = history(i,&s);
            if(success<0){
                printf("history: syscall failed\n");
                exit(1);
            }
            printf("%d: syscall: %s, #: %d, time: %d\n", i, s.syscall_name, s.count,s.accum_time);  
        }
        return 0;
    }
    int num = atoi(argv[1]);
    int success = history(num,&s);
    if(success<0){
        printf("history: syscall failed\n");
        exit(1);
    }
    printf("%d: syscall: %s, #: %d, time: %d\n", num, s.syscall_name, s.count,s.accum_time);
    return 0;
}