
#define _GNU_SOURCE 1
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>

#if !defined(__APPLE__)

#if !defined(__GLIBC_PREREQ)
# error "This program needs __GLIBC_PREREQ (in /usr/include/features.h)"
#endif

short unprotected = 0;

void* lazy_child ( void* v ) {
   assert(0); 
}

void* racy_child ( void* v ) {
   unprotected = 1234;
   return NULL;
}

int main ( void )
{
   int r;
   
   
   pthread_mutexattr_t mxa, mxa2;
   pthread_mutex_t mx, mx2, mx3, mx4;
   pthread_cond_t cv;
   struct timespec abstime;
   pthread_rwlock_t rwl;
   pthread_rwlock_t rwl2;
   pthread_rwlock_t rwl3;
   sem_t s1;

#  if __GLIBC_PREREQ(2,4)
   fprintf(stderr, 
           "\n\n------ This is output for >= glibc 2.4 ------\n");
#  else
   fprintf(stderr,
           "\n\n------ This is output for < glibc 2.4 ------\n");
#  endif

   

   fprintf(stderr,
   "\n---------------- pthread_create/join ----------------\n\n");

   
   { pthread_t child;
     r= pthread_create( &child, NULL, racy_child, NULL ); assert(!r);
     sleep(1); 
     unprotected = 5678;
     r= pthread_join( child, NULL ); assert(!r);
   }

   
   r= pthread_join( pthread_self(), NULL ); assert(r);

   

   fprintf(stderr,
   "\n---------------- pthread_mutex_lock et al ----------------\n\n");

   
   memset( &mxa, 0xFF, sizeof(mxa) );
   r= pthread_mutex_init( &mx, &mxa );
#  if __GLIBC_PREREQ(2,4)
   assert(r); 
#  else
   assert(!r); 
#  endif

   
   r= pthread_mutex_init( &mx2, NULL ); assert(!r);
   r= pthread_mutex_lock( &mx2 ); assert(!r);
   r= pthread_mutex_destroy( &mx2 );

#  if __GLIBC_PREREQ(2,4)
   memset( &mx3, 0xFF, sizeof(mx3) );
   r= pthread_mutex_lock( &mx3 ); assert(r);
#  else
   fprintf(stderr, "\nmake pthread_mutex_lock fail: "
                   "skipped on glibc < 2.4\n\n");
#  endif

   
   memset( &mx3, 0xFF, sizeof(mx3) );
   r= pthread_mutex_trylock( &mx3 ); assert(r);

   
   memset( &abstime, 0, sizeof(abstime) );
   memset( &mx3, 0xFF, sizeof(mx3) );
   r= pthread_mutex_timedlock( &mx3, &abstime ); assert(r);

   
   memset( &mx3, 0xFF, sizeof(mx3) );
   r= pthread_mutex_unlock( &mx3 );
#  if __GLIBC_PREREQ(2,4)
   assert(r);
#  else
   assert(!r);
#  endif

   

   fprintf(stderr,
   "\n---------------- pthread_cond_wait et al ----------------\n\n");

   r= pthread_mutexattr_init( &mxa2 ); assert(!r);
   r= pthread_mutexattr_settype( &mxa2, PTHREAD_MUTEX_ERRORCHECK );
      assert(!r);
   r= pthread_mutex_init( &mx4, &mxa2 ); assert(!r);
   r= pthread_cond_init( &cv, NULL ); assert(!r);
   r= pthread_cond_wait( &cv, &mx4 ); assert(r);
   r= pthread_mutexattr_destroy( &mxa2 ); assert(!r);

   r= pthread_cond_signal( &cv ); assert(!r);
   fprintf(stderr, "\nFIXME: can't figure out how to "
                   "verify wrap of pthread_cond_signal\n\n");

   r= pthread_cond_broadcast( &cv ); assert(!r);
   fprintf(stderr, "\nFIXME: can't figure out how to "
                   "verify wrap of pthread_broadcast_signal\n\n");

   
   memset( &abstime, 0, sizeof(abstime) );
   abstime.tv_nsec = 1000000000 + 1;
   r= pthread_cond_timedwait( &cv, &mx4, &abstime ); assert(r);

   

   fprintf(stderr,
   "\n---------------- pthread_rwlock_* ----------------\n\n");

   
   r= pthread_rwlock_init( &rwl, NULL ); assert(!r);
   r= pthread_rwlock_unlock( &rwl ); 
   

   r= pthread_rwlock_init( &rwl2, NULL ); assert(!r);

   
   fprintf(stderr, "(1) no error on next line\n");
   r= pthread_rwlock_wrlock( &rwl2 ); assert(!r);
   
   fprintf(stderr, "(2) no error on next line\n");
   r= pthread_rwlock_unlock( &rwl2 ); assert(!r);
   
   fprintf(stderr, "(3)    ERROR on next line\n");
   r= pthread_rwlock_unlock( &rwl2 ); assert(!r);

   
   r= pthread_rwlock_init( &rwl2, NULL ); assert(!r);
   
   fprintf(stderr, "(4) no error on next line\n");
   r= pthread_rwlock_rdlock( &rwl2 ); assert(!r);
   fprintf(stderr, "(5) no error on next line\n");
   r= pthread_rwlock_rdlock( &rwl2 ); assert(!r);
   
   fprintf(stderr, "(6) no error on next line\n");
   r= pthread_rwlock_unlock( &rwl2 ); assert(!r);
   fprintf(stderr, "(7) no error on next line\n");
   r= pthread_rwlock_unlock( &rwl2 ); assert(!r);
   
   fprintf(stderr, "(8)    ERROR on next line\n");
   r= pthread_rwlock_unlock( &rwl2 ); assert(!r);

   r= pthread_rwlock_init( &rwl3, NULL ); assert(!r);
   r= pthread_rwlock_rdlock( &rwl3 ); assert(!r);

   

   

   fprintf(stderr,
   "\n---------------- sem_* ----------------\n\n");

   
   
   r= sem_init(&s1, 0, ~0); assert(r);

   
   r= sem_init(&s1, 0, 0);

   fprintf(stderr, "\nFIXME: can't figure out how to verify wrap of "
                   "sem_destroy\n\n");

   
   memset(&s1, 0x55, sizeof(s1));
   r= sem_wait(&s1); 

   
   r= sem_post(&s1);
   fprintf(stderr, "\nFIXME: can't figure out how to verify wrap of "
                   "sem_post\n\n");

   sem_destroy(&s1);

   

   fprintf(stderr,
   "\n------------ dealloc of mem holding locks ------------\n\n");


   return 0;
}

#else 
int main ( void )
{
   fprintf(stderr, "This program does not work on Mac OS X.\n");
   return 0;
}
#endif
