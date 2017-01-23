#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>


char bytes[10];

void* child_fn ( void* arg )
{
   int i;
   for (i = 0; i < 5; i++)
      bytes[2*i + 0] ++; 
   return NULL;
}

int main ( void )
{
   const struct timespec delay = { 0, 100 * 1000 * 1000 };
   int i;
   pthread_t child;
   if (pthread_create(&child, NULL, child_fn, NULL)) {
      perror("pthread_create");
      exit(1);
   }
   nanosleep(&delay, 0);
   for (i = 0; i < 5; i++)
      bytes[2*i + 1] ++; 

   
   for (i = 0; i < 3; i++)
      bytes[3*i + 1] ++; 

   if (pthread_join(child, NULL)) {
      perror("pthread join");
      exit(1);
   }

   return 0;
}
