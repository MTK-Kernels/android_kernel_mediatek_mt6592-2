#include <assert.h>
#include <stdio.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>


int main(void)
{
   int i;
   void* orig_ds = sbrk(0);
   void* ds = orig_ds;
   void* vals[10];
   void* res __attribute__((unused));
#define EOL ((void*)( ~(long)0 ))
   vals[0] = (void*)0;
   vals[1] = (void*)1;
   vals[2] = ds - 0x1;          
   vals[3] = ds;
   vals[4] = ds + 0x1000;       
   vals[5] = ds + 0x40000000;   
   vals[6] = ds + 0x500;        
   vals[7] = ds - 0x1;          
   vals[8] = EOL;

   for (i = 0; EOL != vals[i]; i++) {
      res = (void*)syscall(__NR_brk, vals[i]);
   }

   assert( 0 == brk(orig_ds) );  

   for (i = 0; EOL != vals[i]; i++) {
      res = (void*)(long)brk(vals[i]);
   }

   return 0;
}
