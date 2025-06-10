#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "pstat.h"


//source = https://en.wikipedia.org/wiki/Xorshift#xorshift+
struct xorshift128p_state {
  uint64_t x[2];
};

/* The state must be seeded so that it is not all zero */
uint64_t xorshift128p(struct xorshift128p_state *state)
{   
uint64_t t = state->x[0];
uint64_t const s = state->x[1];
state->x[0] = s;
t ^= t << 23;		// a
t ^= t >> 18;		// b -- Again, the shifts and the multipliers are tunable
t ^= s ^ (s >> 5);	// c
state->x[1] = t;
return t + s;
}

struct cpu cpus[NCPU];

struct proc proc[NPROC];

struct proc *initproc;

int nextpid = 1;
struct spinlock pid_lock;

extern void forkret(void);
static void freeproc(struct proc *p);

extern char trampoline[]; // trampoline.S

// helps ensure that wakeups of wait()ing
// parents are not lost. helps obey the
// memory model when using p->parent.
// must be acquired before any p->lock.
struct spinlock wait_lock;

// Allocate a page for each process's kernel stack.
// Map it high in memory, followed by an invalid
// guard page.
void
proc_mapstacks(pagetable_t kpgtbl)
{
  struct proc *p;
  
  for(p = proc; p < &proc[NPROC]; p++) {
    char *pa = kalloc();
    if(pa == 0)
      panic("kalloc");
    uint64 va = KSTACK((int) (p - proc));
    kvmmap(kpgtbl, va, (uint64)pa, PGSIZE, PTE_R | PTE_W);
  }
}

// initialize the proc table.
void
procinit(void)
{
  struct proc *p;
  
  initlock(&pid_lock, "nextpid");
  initlock(&wait_lock, "wait_lock");
  for(p = proc; p < &proc[NPROC]; p++) {
      initlock(&p->lock, "proc");
      p->state = UNUSED;
      p->kstack = KSTACK((int) (p - proc));
  }
}

// Must be called with interrupts disabled,
// to prevent race with process being moved
// to a different CPU.
int
cpuid()
{
  int id = r_tp();
  return id;
}

// Return this CPU's cpu struct.
// Interrupts must be disabled.
struct cpu*
mycpu(void)
{
  int id = cpuid();
  struct cpu *c = &cpus[id];
  return c;
}

// Return the current struct proc *, or zero if none.
struct proc*
myproc(void)
{
  push_off();
  struct cpu *c = mycpu();
  struct proc *p = c->proc;
  pop_off();
  return p;
}

int
allocpid()
{
  int pid;
  
  acquire(&pid_lock);
  pid = nextpid;
  nextpid = nextpid + 1;
  release(&pid_lock);

  return pid;
}

// Look in the process table for an UNUSED proc.
// If found, initialize state required to run in the kernel,
// and return with p->lock held.
// If there are no free procs, or a memory allocation fails, return 0.
extern struct pstat global_stat;
static struct proc*
allocproc(void)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->state == UNUSED) {
      goto found;
    } else {
      release(&p->lock);
    }
  }
  return 0;

found:
  p->pid = allocpid();
  p->state = USED;

  // p->original_tickets = DEFAULT_TICKET_COUNT;
  // p->remaining_tickets = DEFAULT_TICKET_COUNT;
  // p->inq = 0;
  // p->runtime = 0;

  // Allocate a trapframe page.
  if((p->trapframe = (struct trapframe *)kalloc()) == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // An empty user page table.
  p->pagetable = proc_pagetable(p);
  if(p->pagetable == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // Set up new context to start executing at forkret,
  // which returns to user space.
  memset(&p->context, 0, sizeof(p->context));
  p->context.ra = (uint64)forkret;
  p->context.sp = p->kstack + PGSIZE;

  if(p->original_tickets==0){

    p->original_tickets = DEFAULT_TICKET_COUNT;
    p->remaining_tickets = DEFAULT_TICKET_COUNT;
    
  
    global_stat.tickets_original[p - proc] = DEFAULT_TICKET_COUNT;
    global_stat.tickets_current[p - proc] = DEFAULT_TICKET_COUNT;

  }

  return p;
}

// free a proc structure and the data hanging from it,
// including user pages.
// p->lock must be held.
static void
freeproc(struct proc *p)
{
  if(p->trapframe)
    kfree((void*)p->trapframe);
  p->trapframe = 0;
  if(p->pagetable)
    proc_freepagetable(p->pagetable, p->sz);
  p->pagetable = 0;
  p->sz = 0;
  p->pid = 0;
  p->parent = 0;
  p->name[0] = 0;
  p->chan = 0;
  p->killed = 0;
  p->xstate = 0;
  p->state = UNUSED;

  p->original_tickets = 0;
  p->remaining_tickets = 0;
  p->runtime = 0;

  
  for (int i = 0; i < NPROC; i++) {
    if (global_stat.pid[i] == p->pid) {
      global_stat.inuse[i] = 0;
      global_stat.pid[i] = 0;
      global_stat.tickets_original[i] = 0;
      global_stat.tickets_current[i] = 0;
      global_stat.time_slices[i] = 0;
      break;
    }
  }
}

// Create a user page table for a given process, with no user memory,
// but with trampoline and trapframe pages.
pagetable_t
proc_pagetable(struct proc *p)
{
  pagetable_t pagetable;

  // An empty page table.
  pagetable = uvmcreate();
  if(pagetable == 0)
    return 0;

  // map the trampoline code (for system call return)
  // at the highest user virtual address.
  // only the supervisor uses it, on the way
  // to/from user space, so not PTE_U.
  if(mappages(pagetable, TRAMPOLINE, PGSIZE,
              (uint64)trampoline, PTE_R | PTE_X) < 0){
    uvmfree(pagetable, 0);
    return 0;
  }

  // map the trapframe page just below the trampoline page, for
  // trampoline.S.
  if(mappages(pagetable, TRAPFRAME, PGSIZE,
              (uint64)(p->trapframe), PTE_R | PTE_W) < 0){
    uvmunmap(pagetable, TRAMPOLINE, 1, 0);
    uvmfree(pagetable, 0);
    return 0;
  }

  return pagetable;
}

// Free a process's page table, and free the
// physical memory it refers to.
void
proc_freepagetable(pagetable_t pagetable, uint64 sz)
{
  uvmunmap(pagetable, TRAMPOLINE, 1, 0);
  uvmunmap(pagetable, TRAPFRAME, 1, 0);
  uvmfree(pagetable, sz);
}

// a user program that calls exec("/init")
// assembled from ../user/initcode.S
// od -t xC ../user/initcode
uchar initcode[] = {
  0x17, 0x05, 0x00, 0x00, 0x13, 0x05, 0x45, 0x02,
  0x97, 0x05, 0x00, 0x00, 0x93, 0x85, 0x35, 0x02,
  0x93, 0x08, 0x70, 0x00, 0x73, 0x00, 0x00, 0x00,
  0x93, 0x08, 0x20, 0x00, 0x73, 0x00, 0x00, 0x00,
  0xef, 0xf0, 0x9f, 0xff, 0x2f, 0x69, 0x6e, 0x69,
  0x74, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00
};

// Set up first user process.
void
userinit(void)
{
  struct proc *p;

  p = allocproc();
  initproc = p;
  
  // allocate one user page and copy initcode's instructions
  // and data into it.
  uvmfirst(p->pagetable, initcode, sizeof(initcode));
  p->sz = PGSIZE;

  // prepare for the very first "return" from kernel to user.
  p->trapframe->epc = 0;      // user program counter
  p->trapframe->sp = PGSIZE;  // user stack pointer

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  p->state = RUNNABLE;

  release(&p->lock);
}

// Grow or shrink user memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint64 sz;
  struct proc *p = myproc();

  sz = p->sz;
  if(n > 0){
    if((sz = uvmalloc(p->pagetable, sz, sz + n, PTE_W)) == 0) {
      return -1;
    }
  } else if(n < 0){
    sz = uvmdealloc(p->pagetable, sz, sz + n);
  }
  p->sz = sz;
  return 0;
}

// Create a new process, copying the parent.
// Sets up child kernel stack to return as if from fork() system call.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *p = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy user memory from parent to child.
  if(uvmcopy(p->pagetable, np->pagetable, p->sz) < 0){
    freeproc(np);
    release(&np->lock);
    return -1;
  }
  np->sz = p->sz;

  // copy saved user registers.
  *(np->trapframe) = *(p->trapframe);

  // Cause fork to return 0 in the child.
  np->trapframe->a0 = 0;

  // increment reference counts on open file descriptors.
  for(i = 0; i < NOFILE; i++)
    if(p->ofile[i])
      np->ofile[i] = filedup(p->ofile[i]);
  np->cwd = idup(p->cwd);

  safestrcpy(np->name, p->name, sizeof(p->name));

  pid = np->pid;

  release(&np->lock);

  acquire(&wait_lock);
  np->parent = p;
  release(&wait_lock);

  acquire(&np->lock);
  np->state = RUNNABLE;
  release(&np->lock);
  np->original_tickets = p->original_tickets;
  np->remaining_tickets = p->original_tickets;
  return pid;
}

// Pass p's abandoned children to init.
// Caller must hold wait_lock.
void
reparent(struct proc *p)
{
  struct proc *pp;

  for(pp = proc; pp < &proc[NPROC]; pp++){
    if(pp->parent == p){
      pp->parent = initproc;
      wakeup(initproc);
    }
  }
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait().
void
exit(int status)
{
  struct proc *p = myproc();

  if(p == initproc)
    panic("init exiting");

  // Close all open files.
  for(int fd = 0; fd < NOFILE; fd++){
    if(p->ofile[fd]){
      struct file *f = p->ofile[fd];
      fileclose(f);
      p->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(p->cwd);
  end_op();
  p->cwd = 0;

  acquire(&wait_lock);

  // Give any children to init.
  reparent(p);

  // Parent might be sleeping in wait().
  wakeup(p->parent);
  
  acquire(&p->lock);

  p->xstate = status;
  p->state = ZOMBIE;
  p->original_tickets = 0;
  p->remaining_tickets = 0;
  p->runtime = 0;

  
  for (int i = 0; i < NPROC; i++) {
    if (global_stat.inuse[i] && global_stat.pid[i] == p->pid) {
      global_stat.inuse[i] = 0;
      global_stat.pid[i] = 0;
      global_stat.tickets_original[i] = 0;
      global_stat.tickets_current[i] = 0;
      global_stat.time_slices[i] = 0;
      break;
    }
  }
  

  release(&wait_lock);

  // Jump into the scheduler, never to return.
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(uint64 addr)
{
  struct proc *pp;
  int havekids, pid;
  struct proc *p = myproc();

  acquire(&wait_lock);

  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(pp = proc; pp < &proc[NPROC]; pp++){
      if(pp->parent == p){
        // make sure the child isn't still in exit() or swtch().
        acquire(&pp->lock);

        havekids = 1;
        if(pp->state == ZOMBIE){
          // Found one.
          pid = pp->pid;
          if(addr != 0 && copyout(p->pagetable, addr, (char *)&pp->xstate,
                                  sizeof(pp->xstate)) < 0) {
            release(&pp->lock);
            release(&wait_lock);
            return -1;
          }
          freeproc(pp);
          release(&pp->lock);
          release(&wait_lock);
          return pid;
        }
        release(&pp->lock);
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || killed(p)){
      release(&wait_lock);
      return -1;
    }
    
    // Wait for a child to exit.
    sleep(p, &wait_lock);  //DOC: wait-sleep
  }
}

// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run.
//  - swtch to start running that process.
//  - eventually that process transfers control
//    via swtch back to the scheduler.

void
scheduler(void)
{
  //printf("scheduler reaching\n");
  int startTime = ticks;
  struct proc *p;
  struct cpu *c = mycpu();

  c->proc = 0;
  for(;;){
    
    //priority boosting
    int currTime = ticks;
    
    if(currTime-startTime>BOOST_INTERVAL){
      for(p=proc;p<&proc[NPROC];p++){
        acquire(&p->lock);
        p->inq = 0;
        int i = p-proc;
        global_stat.inQ[i] = 0;
        release(&p->lock);
      }
      startTime = ticks;
    }

    // The most recent process to run may have had interrupts
    // turned off; enable them to avoid a deadlock if all
    // processes are waiting.
    intr_on();
    int found = 0;

    //LOTTERY
  while(1){
      // printf("scheduler reaching lottery at time: %d\n",currTime);
      //first choose a random number 
      struct xorshift128p_state seed;
      seed.x[0] = 12345 + ticks;
      seed.x[1] = (ticks << 1) ^ 98765;
      uint64_t random_number =  xorshift128p(&seed);
      //make an array with proccesses that are inq = 0 and runnable and tickets > 0 
      struct proc *lotteryPool[NPROC]; 
      int currInd = 0;
      int total_remaining_tickets = 0;
      int lotteryProcesses = 0;
      for(p = proc; p < &proc[NPROC]; p++){
        acquire(&p->lock);
        if(p->inq==0 && p->state==RUNNABLE){
          lotteryProcesses++;
          if(p->remaining_tickets>0){
            lotteryPool[currInd] = p;
            currInd++;
            total_remaining_tickets += p->remaining_tickets;
          }
        }
        release(&p->lock);
      }

      if(lotteryProcesses == 0) {
        //reinitialize all tickets to original ones
        // for(p = proc;p<&proc[NPROC];p++){
        //   acquire(&p->lock);
        //   p->remaining_tickets = p->original_tickets;
        //   //have to check this logic
        //   //p->inq = 1;
        //   int i = p-proc;
        //   global_stat.tickets_current[i] = p->original_tickets;
        //   global_stat.tickets_original[i] = p->original_tickets;
        //   //global_stat.inQ[i] = 1;
        //   release(&p->lock);
        // }
        if(PRINT_SCHEDULING==1){
          printf("breaking to ROUND ROBIN because no process in Lottery are RUNNABLE (or there are no processes in Lottery)\n");
        }
        //printf("breaking\n");
        break; //jump to level 2 
      } 
      if(total_remaining_tickets==0){
        //reinitialize all tickets to original ones if there are processes in lottery, but no one has tickets
        if(PRINT_SCHEDULING==1){
            printf("reinitialize all tickets to original ones if there are processes in lottery, but no one has tickets");
        }
        
        for(p = proc;p<&proc[NPROC];p++){
          acquire(&p->lock);
          p->remaining_tickets = p->original_tickets;
          //have to check this logic
          //p->inq = 1;
          int i = p-proc;
          global_stat.tickets_current[i] = p->original_tickets;
          global_stat.tickets_original[i] = p->original_tickets;
          //global_stat.inQ[i] = 1;
          release(&p->lock);
        }
        continue; //continue with lottery
      }
      if(PRINT_SCHEDULING==1){
        printf("scheduler reaching tickets: %d\n",total_remaining_tickets);
      }
      //printf("scheduler reaching tickets: %d\n",total_remaining_tickets);
      int chosenTicketNumber = random_number % total_remaining_tickets;
      struct proc* chosenProc = 0;
      //int currentBest = __INT_MAX__; 
      int ticketSum = 0;
      

      //choosing the proc 
      //if i got 15 as random number, and i have processes with 10,20,30 remaining tickets, i will choose the second process 
      // for(int i=0; i<currInd; i++){
      //   p = lotteryPool[i];
      //   acquire(&p->lock);
      //   if(p->remaining_tickets>chosenTicketNumber && p->remaining_tickets<currentBest){
      //     currentBest = p->remaining_tickets;
      //     chosenProc = p;
      //     printf("chosen lottery: %d\n",p->pid);
      //   }
      //   release(&p->lock);
      // }

      for(int i = 0; i < currInd; i++) {
        p = lotteryPool[i];
        acquire(&p->lock);
      
        ticketSum += p->remaining_tickets;
      
        if(ticketSum > chosenTicketNumber) {
          //if(p->remaining_tickets < currentBest) {
            
            chosenProc = p;
            release(&p->lock);
            break;
          
        }
      
        release(&p->lock);
      }

      if(chosenProc==0){break;}
      acquire(&chosenProc->lock); 
      int index = chosenProc - proc;
      if(PRINT_SCHEDULING==1){
        printf("chosen index: %d for pid %d using lottery\n",index,chosenProc->pid);
      }
      //printf("chosen index: %d for pid %d\n",index,chosenProc->pid);
      global_stat.inuse[index] = 1;
      global_stat.pid[index] = chosenProc->pid;

      
      //reducing tickets because i scheduled this process by choosing it 

      chosenProc->remaining_tickets -= 1;

      //setting the global stats for printing
      global_stat.tickets_current[index] = chosenProc->remaining_tickets;
      global_stat.tickets_original[index] = chosenProc->original_tickets;
      

      if(TICKET_DEBUG){
        if(global_stat.tickets_current[index]<0){
          printf("ALERT!!pid:%d has negative tickets!\n",chosenProc->pid);
        }
      }


      while(chosenProc->state == RUNNABLE && chosenProc->runtime < TIME_LIMIT_1){
        // Switch to chosen process.  It is the process's job
        // to release its lock and then reacquire it
        // before jumping back to us.
        //printf("chosen pid %d for chosen ticket number:%d ,remaining tickets: %d , for random number:%lld \n",chosenProc->pid,chosenTicketNumber,chosenProc->remaining_tickets,random_number);
        //acquire(&chosenProc->lock);
        chosenProc->state = RUNNING;
        c->proc = chosenProc;
        swtch(&c->context, &chosenProc->context);

        // Process is done running for now.
        // It should have changed its p->state before coming back.
        c->proc = 0;
        found = 1;
        chosenProc->runtime += 1;
        global_stat.time_slices[index]++; //i chose it, so another time slice increase for it
        //printf("remaining tickets: %d\n",chosenProc->remaining_tickets);
        //release(&chosenProc->lock);
    }
      
    
    
      //p->inuse = 0;
      if(chosenProc->runtime >= TIME_LIMIT_1){ //(should make ==) if i used more time than allocated, go to round robin
          
          global_stat.inQ[index] = 1;
          chosenProc->inq = 1;
          if(PRINT_SCHEDULING==1){
            printf("demoting process pid:%d because it is taking more time than TIME_LIMIT_1\n",chosenProc->pid);
          }
        }

        else{
          chosenProc->runtime = 0; //(recheck logic) process run done, so runtime resets, otherwise, when going to round robin, we dont make the runtime 0
          global_stat.inuse[index] = 0; //recheck this logic 
        }

      
      
      release(&chosenProc->lock);
  }

   
    //round robin 
    if(PRINT_SCHEDULING==1){
      printf("reaching RR\n");
    }
    //printf("reaching RR\n");
    for(p = proc; p < &proc[NPROC]; p++) {
      acquire(&p->lock);
      int index = p-proc;
      global_stat.pid[index] = p->pid;
      
      if(p->state == RUNNABLE && p->inq==1) {
        while(p->state == RUNNABLE && p->runtime < TIME_LIMIT_2){
            // Switch to chosen process.  It is the process's job
            // to release its lock and then reacquire it
            // before jumping back to us.
            global_stat.inuse[index] = 1; // i am choosing it to run, so inuse = 1 
            if(PRINT_SCHEDULING==1){
              printf("RR pid: %d\n",p->pid);
            }
            //printf("RR pid: %d\n",p->pid);
            p->state = RUNNING;
            c->proc = p;
            swtch(&c->context, &p->context);

            // Process is done running for now.
            // It should have changed its p->state before coming back.
            c->proc = 0;
            found = 1;
            //printf("reaching RR ,process name: %s\n",p->name);
            p->runtime += 1;
            global_stat.time_slices[index]++; //i chose it for running 
        }
          
        
        
        //p->inuse = 0;
        if(p->runtime < TIME_LIMIT_2){
            
            global_stat.inQ[index] = 0;
            p->inq = 0;
            if(PRINT_SCHEDULING==1){
              printf("promoting pid:%d to lottery because it took less time than TIME_LIMIT_2\n",p->pid);
            }
          }
          else{
            p->runtime = 0; //still in RR, not went to lottery, so runtime made 0, run done
            global_stat.inuse[index] = 0; //check the logic
          }

        
        

      }
      release(&p->lock);
    }


    if(found == 0) {
      // nothing to run; stop running on this core until an interrupt.
      intr_on();
      asm volatile("wfi");
    }
  }
}

// Switch to scheduler.  Must hold only p->lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->noff, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&p->lock))
    panic("sched p->lock");
  if(mycpu()->noff != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(intr_get())
    panic("sched interruptible");

  intena = mycpu()->intena;
  swtch(&p->context, &mycpu()->context);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  struct proc *p = myproc();
  acquire(&p->lock);
  p->state = RUNNABLE;
  sched();
  release(&p->lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch to forkret.
void
forkret(void)
{
  static int first = 1;

  // Still holding p->lock from scheduler.
  release(&myproc()->lock);

  if (first) {
    // File system initialization must be run in the context of a
    // regular process (e.g., because it calls sleep), and thus cannot
    // be run from main().
    fsinit(ROOTDEV);

    first = 0;
    // ensure other cores see first=0.
    __sync_synchronize();
  }

  usertrapret();
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  // Must acquire p->lock in order to
  // change p->state and then call sched.
  // Once we hold p->lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup locks p->lock),
  // so it's okay to release lk.

  acquire(&p->lock);  //DOC: sleeplock1
  release(lk);

  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  release(&p->lock);
  acquire(lk);
}

// Wake up all processes sleeping on chan.
// Must be called without any p->lock.
void
wakeup(void *chan)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    if(p != myproc()){
      acquire(&p->lock);
      if(p->state == SLEEPING && p->chan == chan) {
        p->state = RUNNABLE;
      }
      release(&p->lock);
    }
  }
}

// Kill the process with the given pid.
// The victim won't exit until it tries to return
// to user space (see usertrap() in trap.c).
int
kill(int pid)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++){
    acquire(&p->lock);
    if(p->pid == pid){
      p->killed = 1;
      if(p->state == SLEEPING){
        // Wake process from sleep().
        p->state = RUNNABLE;
      }
      release(&p->lock);
      return 0;
    }
    release(&p->lock);
  }
  return -1;
}

void
setkilled(struct proc *p)
{
  acquire(&p->lock);
  p->killed = 1;
  release(&p->lock);
}

int
killed(struct proc *p)
{
  int k;
  
  acquire(&p->lock);
  k = p->killed;
  release(&p->lock);
  return k;
}

// Copy to either a user address, or kernel address,
// depending on usr_dst.
// Returns 0 on success, -1 on error.
int
either_copyout(int user_dst, uint64 dst, void *src, uint64 len)
{
  struct proc *p = myproc();
  if(user_dst){
    return copyout(p->pagetable, dst, src, len);
  } else {
    memmove((char *)dst, src, len);
    return 0;
  }
}

// Copy from either a user address, or kernel address,
// depending on usr_src.
// Returns 0 on success, -1 on error.
int
either_copyin(void *dst, int user_src, uint64 src, uint64 len)
{
  struct proc *p = myproc();
  if(user_src){
    return copyin(p->pagetable, dst, src, len);
  } else {
    memmove(dst, (char*)src, len);
    return 0;
  }
}

// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [USED]      "used",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  struct proc *p;
  char *state;

  printf("\n");
  for(p = proc; p < &proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    printf("%d %s %s", p->pid, state, p->name);
    printf("\n");
  }
}
