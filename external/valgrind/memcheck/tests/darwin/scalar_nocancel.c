#include "../../memcheck.h"
#include "scalar.h"
#include <unistd.h>
#include <sched.h>
#include <signal.h>
#include <sys/shm.h>


int main(void)
{
   
   long* px  = malloc(sizeof(long));
   long  x0  = px[0];
   long  res;

   VALGRIND_MAKE_MEM_NOACCESS(0, 0x1000);

   
   
   
   

   
   
   
   
   
   
   
   
   
   
   
   
   
   
   

   GO(__NR_sigsuspend_nocancel, 410, "ignore");
   

   
   
   
   
   
   
   
   
   

   
   GO(__NR_sem_wait_nocancel, 420, "1s 0m");
   SY(__NR_sem_wait_nocancel, x0); FAIL;

   
   
   

   return 0;
}

