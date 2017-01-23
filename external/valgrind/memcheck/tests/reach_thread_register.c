#define _GNU_SOURCE 

#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <config.h>


pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
int cont = 1;

void* helper(void* v_bar) {
  pthread_barrier_t* bar = (pthread_barrier_t*)v_bar;
  register int* i = malloc(sizeof(*i));
  
  *i = 3;
  pthread_barrier_wait(bar);
  pthread_mutex_lock(&mu);
  while (cont) {
#if defined(VGA_x86) || defined(VGA_amd64)
     
     asm volatile("test %0, %0" : : "S"(i));
#else
     
     if (*i) 
        
#endif
     pthread_mutex_unlock(&mu);
     sched_yield();
     pthread_mutex_lock(&mu);
  }
  pthread_mutex_unlock(&mu);
  free((void *)i);
  fprintf(stderr, "Quitting the helper.\n");
  return NULL;
}

int main() {
  pthread_barrier_t bar;
  pthread_barrier_init(&bar, NULL, 2);
  pthread_t thr;
  pthread_create(&thr, NULL, &helper, &bar);
  pthread_barrier_wait(&bar);
  pthread_barrier_destroy(&bar);
  fprintf(stderr, "Abandoning the helper.\n");
  pthread_detach(thr);
  return 0;
}
