
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
   pthread_t child;

   x++; 

   if (pthread_create(&child, NULL, child_fn, NULL)) {
      perror("pthread_create");
      exit(1);
   }

   if (pthread_join(child, NULL)) {
      perror("pthread join");
      exit(1);
   }

   
   x++;

   return 0;
}
