#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/pstat.h"
int main()
{
    struct pstat p;
    getpinfo(&p);
    printf("PID\t| In Use | inQ | Tickets(Original) | Tickets(Current) | Time Slices\n");
    printf("-------------------------------------------------------------------------\n");
    
    for (int i = 0; i < NPROC; i++) {
      //if (pinfo.inuse[i]) {
        printf("%d\t   %d      %d           %d                %d                 %d\n",
               p.pid[i],
               p.inuse[i],
               p.inQ[i],
               p.tickets_original[i],
               p.tickets_current[i],
               p.time_slices[i]);
      //}
    }
}