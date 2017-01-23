#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
/* This code is derived from
   gcc-4.3-20071012/libgomp/config/posix/bar.c, which is

   Copyright (C) 2005 Free Software Foundation, Inc.
   Contributed by Richard Henderson <rth@redhat.com>.

   and available under version 2.1 or later of the GNU Lesser General
   Public License.

   Relative to the libgomp sources, the gomp_barrier_t type here has
   an extra semaphore field, xxx.  This is not functionally useful,
   but it is used to create enough extra inter-thread dependencies
   that the barrier-like behaviour of gomp_barrier_t is evident to
   Thrcheck.  There is no other purpose for the .xxx field. */
static sem_t* my_sem_init(char*, int, unsigned);
static int my_sem_destroy(sem_t*);
static int my_sem_wait(sem_t*); static int my_sem_post(sem_t*);
typedef struct
{
  pthread_mutex_t mutex1;
  pthread_mutex_t mutex2;
  sem_t* sem1;
  sem_t* sem2;
  unsigned total;
  unsigned arrived;
  sem_t* xxx;
} gomp_barrier_t;

typedef long bool;

void
gomp_barrier_init (gomp_barrier_t *bar, unsigned count)
{
  pthread_mutex_init (&bar->mutex1, NULL);
  pthread_mutex_init (&bar->mutex2, NULL);
  bar->sem1 = my_sem_init ("sem1", 0, 0);
  bar->sem2 = my_sem_init ("sem2", 0, 0);
  bar->xxx  = my_sem_init ("xxx",  0, 0);
  bar->total = count;
  bar->arrived = 0;
}

void
gomp_barrier_destroy (gomp_barrier_t *bar)
{
  
  pthread_mutex_lock (&bar->mutex1);
  pthread_mutex_unlock (&bar->mutex1);

  pthread_mutex_destroy (&bar->mutex1);
  pthread_mutex_destroy (&bar->mutex2);
  my_sem_destroy(bar->sem1);
  my_sem_destroy(bar->sem2);
  my_sem_destroy(bar->xxx);
}

void
gomp_barrier_reinit (gomp_barrier_t *bar, unsigned count)
{
  pthread_mutex_lock (&bar->mutex1);
  bar->total = count;
  pthread_mutex_unlock (&bar->mutex1);
}

void
gomp_barrier_wait (gomp_barrier_t *bar)
{
  unsigned int n;
  pthread_mutex_lock (&bar->mutex1);

  ++bar->arrived;

  if (bar->arrived == bar->total)
    {
      bar->arrived--;
      n = bar->arrived;
      if (n > 0) 
        {
          { unsigned int i;
            for (i = 0; i < n; i++)
              my_sem_wait(bar->xxx); 
              
          }
          
          
          
          do
            my_sem_post (bar->sem1); 
          while (--n != 0);
          
          my_sem_wait (bar->sem2); 
        }
      pthread_mutex_unlock (&bar->mutex1);
    }
  else
    {
      pthread_mutex_unlock (&bar->mutex1);
      my_sem_post(bar->xxx);
      
      my_sem_wait (bar->sem1); 

      pthread_mutex_lock (&bar->mutex2);
      n = --bar->arrived; 
      pthread_mutex_unlock (&bar->mutex2);

      if (n == 0)
        my_sem_post (bar->sem2); 
    }
}



static gomp_barrier_t bar;

static volatile long unprotected = 0;

void* child ( void* argV )
{
   long myid = (long)argV;
   

   
   gomp_barrier_wait( &bar );

   if (myid == 4) {
      unprotected = 99;
   }

   
   gomp_barrier_wait( &bar );

   if (myid == 3) {
      unprotected = 88;
   }

   
   gomp_barrier_wait( &bar );
   return NULL;
}


int main (int argc, char *argv[])
{
   long i; int res;
   pthread_t thr[4];
   fprintf(stderr, "starting\n");

   gomp_barrier_init( &bar, 4 );

   for (i = 0; i < 4; i++) {
      res = pthread_create( &thr[i], NULL, child, (void*)(i+2) );
      assert(!res);
   }

   for (i = 0; i < 4; i++) {
      res = pthread_join( thr[i], NULL );
      assert(!res);
   }

   gomp_barrier_destroy( &bar );

   fprintf(stderr, "done, result is %ld, should be 88\n", unprotected);

   return 0;
}







static sem_t* my_sem_init (char* identity, int pshared, unsigned count)
{
   sem_t* s;

#if defined(VGO_linux)
   s = malloc(sizeof(*s));
   if (s) {
      if (sem_init(s, pshared, count) < 0) {
	 perror("sem_init");
	 free(s);
	 s = NULL;
      }
   }
#elif defined(VGO_darwin)
   char name[100];
   sprintf(name, "anonsem_%s_pid%d", identity, (int)getpid());
   name[ sizeof(name)-1 ] = 0;
   if (0) printf("name = %s\n", name);
   s = sem_open(name, O_CREAT | O_EXCL, 0600, count);
   if (s == SEM_FAILED) {
      perror("sem_open");
      s = NULL;
   }
#else
#  error "Unsupported OS"
#endif

   return s;
}

static int my_sem_destroy ( sem_t* s )
{
   return sem_destroy(s);
}

static int my_sem_wait(sem_t* s)
{
  return sem_wait(s);
}

static int my_sem_post(sem_t* s)
{
  return sem_post(s);
}
