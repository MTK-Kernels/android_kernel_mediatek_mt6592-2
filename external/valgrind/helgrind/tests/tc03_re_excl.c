
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>


static void use ( int x ) {
   __asm__ __volatile__( "" : : "r"(x) : "cc","memory" );
}

static void* worker_thread ( void* argV )
{
  int* arg = (int*)argV;
  use(arg[5]); 
  return NULL;
}

int main ( void )
{
   pthread_t thread_id;
   volatile int* x = malloc(10 * sizeof(int));
   x[5] = 1;
   

   pthread_create(&thread_id, 0, worker_thread, (void*)x);

   use(x[5]); 

   pthread_join(thread_id, 0);

   x[5] = 0; 

   return x[5];
}
