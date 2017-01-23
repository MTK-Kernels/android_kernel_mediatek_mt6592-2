
#include <pthread.h>
#include <unistd.h>
#include <assert.h>
#include <signal.h>


pthread_mutex_t mxC1  = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mxC2  = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mxC2b = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mxP   = PTHREAD_MUTEX_INITIALIZER;

void* child_fn1 ( void* arg )
{
  int r= pthread_mutex_lock( &mxC1 ); assert(!r);
  return NULL;
}

void* child_fn2 ( void* arg )
{
  int r;
  r= pthread_mutex_lock( &mxC2 ); assert(!r);
  r= pthread_mutex_lock( &mxC2b ); assert(!r);
  r= pthread_detach( pthread_self() ); assert(!r);
  return NULL;
}

int main ( void )
{
   int r;
   pthread_t child1, child2;

   r= pthread_create(&child2, NULL, child_fn2, NULL); assert(!r);
   sleep(1);

   r= pthread_create(&child1, NULL, child_fn1, NULL); assert(!r);
   r= pthread_join(child1, NULL); assert(!r);
   sleep(1);

   r= pthread_mutex_lock( &mxP );
 
   kill( getpid(), SIGABRT );
   return 0;
}
