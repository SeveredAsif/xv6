struct syscall_stat {
    char syscall_name[16];
    int count;
    int accum_time;
  };
  
extern struct syscall_stat syscall_stats[];