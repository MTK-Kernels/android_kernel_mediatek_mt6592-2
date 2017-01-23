
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>


int x = 0;

void* child_fn ( void* arg )
{
   
   x++;
   return NULL;
}

int main ( void )
{
   const struct timespec delay = { 0, 100 * 1000 * 1000 };
   pthread_t child;
   if (pthread_create(&child, NULL, child_fn, NULL)) {
      perror("pthread_create");
      exit(1);
   }
   nanosleep(&delay, 0);
   
   x++;

   if (pthread_join(child, NULL)) {
      perror("pthread join");
      exit(1);
   }

   return 0;
}
