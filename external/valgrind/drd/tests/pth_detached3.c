
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>

static void* thread_func(void* arg)
{
  return 0;
}

int main(int argc, char** argv)
{
  pthread_t thread;

  pthread_create(&thread, NULL, thread_func, NULL);
  pthread_join(thread, NULL);

  
  pthread_detach(thread);

  
  pthread_detach(thread + 8);

  fprintf(stderr, "Finished.\n");

  return 0;
}
