
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>


typedef struct { int stuff[2000]; 
                 pthread_mutex_t lock; int morestuff[2000]; } XX;

void bar ( void );
void foo ( void );

int main ( void )
{
   XX* xx = malloc(sizeof(XX));
   assert(xx);

   pthread_mutex_init( &xx->lock, NULL );

   pthread_mutex_lock( &xx->lock );

   free(xx);

   bar();
   foo();
   bar();

   return 0;
}

void bar ( void )
{
  pthread_mutex_t mx = PTHREAD_MUTEX_INITIALIZER;
  
  pthread_mutex_lock( &mx );
  
}

void foo ( void )
{
  pthread_mutex_t mx;
  pthread_mutex_init( &mx, NULL );
  pthread_mutex_lock( &mx );
  
}

