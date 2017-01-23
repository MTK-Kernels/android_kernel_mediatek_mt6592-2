
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>


int y = 0, v = 0;
pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;

void* child_fn ( void* arg )
{
   
   pthread_mutex_lock( &mu );
   v = v + 1;
   pthread_mutex_unlock( &mu );
   y = y + 1;
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
   
   y = y + 1;
   pthread_mutex_lock( &mu );
   v = v + 1;
   pthread_mutex_unlock( &mu );

   if (pthread_join(child, NULL)) {
      perror("pthread join");
      exit(1);
   }

   return 0;
}
