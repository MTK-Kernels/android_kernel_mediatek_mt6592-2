
#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

void* child1 ( void* arg )
{
   pthread_barrier_wait( (pthread_barrier_t*)arg );
   return NULL;
}

int main ( void )
{
  pthread_barrier_t *bar1, *bar2, *bar3, *bar4, *bar5;
  pthread_t thr1, thr2;
  int r;

  











  
  fprintf(stderr, "\ninitialise a barrier with zero count\n");
  bar1 = malloc(sizeof(pthread_barrier_t));
  pthread_barrier_init(bar1, NULL, 0);

  
  fprintf(stderr, "\ninitialise a barrier twice\n");
  bar2 = malloc(sizeof(pthread_barrier_t));
  pthread_barrier_init(bar2, NULL, 1);
  pthread_barrier_init(bar2, NULL, 1);

  fprintf(stderr, "\ninitialise a barrier which has threads waiting on it\n");
  bar3 = malloc(sizeof(pthread_barrier_t));
  pthread_barrier_init(bar3, NULL, 2);
  
  pthread_create(&thr1, NULL, child1, (void*)bar3);
  
  sleep(1);
  
  pthread_barrier_init(bar3, NULL, 3);

  
  fprintf(stderr, "\ndestroy a barrier that has waiting threads\n");
  
  bar4 = malloc(sizeof(pthread_barrier_t));
  pthread_barrier_init(bar4, NULL, 2);
  
  pthread_create(&thr2, NULL, child1, (void*)bar4);
  
  sleep(1);
  
  pthread_barrier_destroy(bar4);

  fprintf(stderr, "\ndestroy a barrier that was never initialised\n");
  bar5 = malloc(sizeof(pthread_barrier_t));
  assert(bar5);
  memset(bar5, 0, sizeof(*bar5));
  pthread_barrier_destroy(bar5);

  
  r= pthread_cancel(thr1); assert(!r);
  r= pthread_cancel(thr2); assert(!r);

  free(bar1); free(bar2); free(bar3); free(bar4); free(bar5);

  return 0;
}
