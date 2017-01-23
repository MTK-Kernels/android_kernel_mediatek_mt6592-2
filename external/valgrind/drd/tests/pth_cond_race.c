
#include <assert.h>
#include <stdio.h>      
#include <pthread.h>
#include <unistd.h>    



static void* thread_func(void* thread_arg);



static pthread_mutex_t s_mutex;
static pthread_cond_t  s_cond;
static int             s_use_mutex = 0;



int main(int argc, char** argv)
{
  int optchar;
  pthread_t threadid;

  while ((optchar = getopt(argc, argv, "m")) != EOF)
  {
    switch (optchar)
    {
    case 'm':
      s_use_mutex = 1;
      break;
    default:
      assert(0);
    }
  }

  pthread_cond_init(&s_cond, 0);
  pthread_mutex_init(&s_mutex, 0);
  pthread_mutex_lock(&s_mutex);

  pthread_create(&threadid, 0, thread_func, 0);

  pthread_cond_wait(&s_cond, &s_mutex);
  pthread_mutex_unlock(&s_mutex);

  pthread_join(threadid, 0);

  pthread_mutex_destroy(&s_mutex);
  pthread_cond_destroy(&s_cond);

  return 0;
}

static void* thread_func(void* thread_arg)
{
  
  pthread_mutex_lock(&s_mutex);
  pthread_mutex_unlock(&s_mutex);

  
  if (s_use_mutex) pthread_mutex_lock(&s_mutex);
  pthread_cond_signal(&s_cond);
  if (s_use_mutex) pthread_mutex_unlock(&s_mutex);

  return 0;
}
