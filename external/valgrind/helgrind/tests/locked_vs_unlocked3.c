
#define _GNU_SOURCE 1

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>


pthread_mutex_t mx;

int x = 0;

void* child_fn1 ( void* arg )
{
   int r;
   r= pthread_mutex_lock(&mx);  assert(!r);
   r= pthread_mutex_lock(&mx);  assert(!r);
   x = 1;
   r= pthread_mutex_unlock(&mx);  assert(!r);
   r= pthread_mutex_unlock(&mx);  assert(!r);
   sleep(1);
   return NULL;
}

void* child_fn2 ( void* arg )
{
   sleep(1);
   x = 1;
   return NULL;
}

int main ( int argc, char** argv )
{
   int r;
   pthread_t child1, child2;
   pthread_mutexattr_t attr;
   r = pthread_mutexattr_init( &attr );
   assert(!r);
   r = pthread_mutexattr_settype( &attr, PTHREAD_MUTEX_RECURSIVE );
   assert(!r);
   r= pthread_mutex_init(&mx, &attr);  assert(!r);

   r= pthread_create(&child2, NULL, child_fn2, NULL);  assert(!r);
   r= pthread_create(&child1, NULL, child_fn1, NULL);  assert(!r);

   r= pthread_join(child1, NULL);  assert(!r);
   r= pthread_join(child2, NULL);  assert(!r);

   r= pthread_mutex_destroy(&mx);  assert(!r);

   return 0;
}
