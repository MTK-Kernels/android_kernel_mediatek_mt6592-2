

#include <errno.h>     
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>    
#include <string.h>    
#include <sys/time.h>  
#include <time.h>      
#include <fcntl.h>     
#include <unistd.h>
#include "../../config.h"


#define PTH_CALL(expr)                                  \
  do                                                    \
  {                                                     \
    int err = (expr);                                   \
    if (! s_quiet && err)				\
    {                                                   \
      fprintf(stderr,                                   \
              "%s:%d %s returned error code %d (%s)\n", \
              __FILE__,                                 \
              __LINE__,                                 \
              #expr,                                    \
              err,                                      \
              strerror(err));                           \
    }                                                   \
  } while (0)


static pthread_cond_t  s_cond;
static pthread_mutex_t s_mutex1;
static pthread_mutex_t s_mutex2;
static sem_t*          s_sem;
static int             s_quiet;


static sem_t* create_semaphore(const char* const name)
{
#ifdef VGO_darwin
  char name_and_pid[32];
  snprintf(name_and_pid, sizeof(name_and_pid), "%s-%d", name, getpid());
  sem_t* p = sem_open(name_and_pid, O_CREAT | O_EXCL, 0600, 0);
  if (p == SEM_FAILED) {
    perror("sem_open");
    return NULL;
  }
  return p;
#else
  sem_t* p = malloc(sizeof(*p));
  if (p)
    sem_init(p, 0, 0);
  return p;
#endif
}

static void destroy_semaphore(const char* const name, sem_t* p)
{
#ifdef VGO_darwin
  sem_close(p);
  sem_unlink(name);
#else
  sem_destroy(p);
  free(p);
#endif
}

static void* thread_func(void* mutex)
{
  struct timeval now;
  struct timespec deadline;

  PTH_CALL(pthread_mutex_lock(mutex));
  sem_post(s_sem);
  gettimeofday(&now, 0);
  memset(&deadline, 0, sizeof(deadline));
  deadline.tv_sec  = now.tv_sec + 2;
  deadline.tv_nsec = now.tv_usec * 1000;
  PTH_CALL(pthread_cond_timedwait(&s_cond, mutex, &deadline));
  PTH_CALL(pthread_mutex_unlock(mutex));
  return 0;
}

int main(int argc, char** argv)
{
  char semaphore_name[32];
  int optchar;
  pthread_t tid1;
  pthread_t tid2;

  while ((optchar = getopt(argc, argv, "q")) != EOF)
  {
    switch (optchar)
    {
    case 'q': s_quiet = 1; break;
    default:
      fprintf(stderr, "Error: unknown option '%c'.\n", optchar);
      return 1;
    }
  }

  
  snprintf(semaphore_name, sizeof(semaphore_name), "semaphore-%d", getpid());
  s_sem = create_semaphore(semaphore_name);
  PTH_CALL(pthread_cond_init(&s_cond, 0));
  PTH_CALL(pthread_mutex_init(&s_mutex1, 0));
  PTH_CALL(pthread_mutex_init(&s_mutex2, 0));

  
  PTH_CALL(pthread_create(&tid1, 0, &thread_func, &s_mutex1));
  PTH_CALL(pthread_create(&tid2, 0, &thread_func, &s_mutex2));

  
  sem_wait(s_sem);
  sem_wait(s_sem);
  destroy_semaphore(semaphore_name, s_sem);
  s_sem = 0;

  
  PTH_CALL(pthread_mutex_lock(&s_mutex1));
  PTH_CALL(pthread_mutex_lock(&s_mutex2));
  PTH_CALL(pthread_mutex_unlock(&s_mutex2));
  PTH_CALL(pthread_mutex_unlock(&s_mutex1));

  
  PTH_CALL(pthread_cond_signal(&s_cond));
  PTH_CALL(pthread_cond_signal(&s_cond));

  
  PTH_CALL(pthread_join(tid1, 0));
  PTH_CALL(pthread_join(tid2, 0));

  return 0;
}
