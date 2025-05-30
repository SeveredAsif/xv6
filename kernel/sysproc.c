#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "stats.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  if(n < 0)
    n = 0;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

// struct syscall_stat {
//   char syscall_name[16];
//   int count;
//   int accum_time;
// };


extern char *syscall_names[]; 

extern struct syscall_stat syscall_stats[];


uint64 sys_history(void){
  int syscall_number;
  uint64 user_stat_struct_pointer;

  argint(0, &syscall_number);


  if(syscall_number<0){

    return -1;
    // argaddr(1, &user_stat_struct_array_pointer);

    // if(user_stat_struct_array_pointer<0){
    //   return -1;
    // }


    // for(int i = 0; i < 23; i++){


    //   if (syscall_names[i]) {
    //     safestrcpy(syscall_stats[i].syscall_name, syscall_names[i], 16);
    //   }


    //   if (copyout(myproc()->pagetable, (user_stat_struct_pointer+8*i), (char *)&syscall_stats[i], sizeof(syscall_stats[i])) < 0){
    //     return -1;
    //   }


    //   //printf("%d: syscall: %s, #: %d\n", i, syscall_stats[i].syscall_name, syscall_stats[i].count);
    // }


  }


  if (syscall_names[syscall_number]) {
    safestrcpy(syscall_stats[syscall_number].syscall_name, syscall_names[syscall_number], 16);
  }


  argaddr(1, &user_stat_struct_pointer);
  if(user_stat_struct_pointer<0){
    return -1;
  }


  if (copyout(myproc()->pagetable, user_stat_struct_pointer, (char *)&syscall_stats[syscall_number], sizeof(syscall_stats[syscall_number])) < 0){
    return -1;
  }
 
  return 0;


}
