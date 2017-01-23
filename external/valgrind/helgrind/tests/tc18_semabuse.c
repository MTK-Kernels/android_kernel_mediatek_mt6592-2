

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>
#include <semaphore.h>
#include <string.h>
void start_watchdog ( void );
int main ( void )
{
  int r __attribute__((unused));
  sem_t s1;
  start_watchdog();
  
  r= sem_init(&s1, 0, ~0);

  
  r= sem_init(&s1, 0, 0);


  memset(&s1, 0x55, sizeof(s1));
  r= sem_wait(&s1); 

  
  r= sem_post(&s1);

  sem_destroy(&s1);

  return 0;
}

void* watchdog ( void* v )
{
  sleep(10);
  fprintf(stderr, "watchdog timer expired - not a good sign\n");
  exit(0);
}

void start_watchdog ( void )
{
  pthread_t t;
  int r;
  r= pthread_create(&t, NULL, watchdog, NULL);
  assert(!r);
}
