#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/stats.h"

int main(int argc, char *argv[]){
    struct syscall_stat s;
    if (argc != 2) {
        history(-1,&s);
        return 0;
    }
    int num = atoi(argv[1]);
    int success = history(num,&s);
    if(success<0){
        printf("history: syscall failed\n");
        exit(1);
    }
    printf("Syscall: %s, Count: %d\n", s.syscall_name, s.count);
    return 0;
}