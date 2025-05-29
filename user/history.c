#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/stats.h"

int main(){
    struct syscall_stat s;
    history(5,&s);
    printf("Syscall: %s, Count: %d\n", s.syscall_name, s.count);
    return 0;
}