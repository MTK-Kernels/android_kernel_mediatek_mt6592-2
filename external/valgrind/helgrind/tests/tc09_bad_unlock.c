

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

void* child_fn ( void* arg )
{
   pthread_mutex_unlock( (pthread_mutex_t*)arg ); 
   return NULL;
}

void nearly_main ( void )
{
   pthread_t child;
   pthread_mutex_t mx1, mx2;
   int bogus[100], i;
   
   for (i = 0; i < 100; i++) bogus[i] = 0xFFFFFFFF;
   
   pthread_mutex_init( &mx1, NULL );
   pthread_mutex_lock( &mx1 );
   pthread_mutex_unlock( &mx1 );

   pthread_mutex_unlock( &mx1 ); 

   

   pthread_mutex_init( &mx2, NULL );
   pthread_mutex_lock( &mx2 );
   

   pthread_create( &child, NULL, child_fn, (void*)&mx2 );
   pthread_join(child, NULL );

   
   pthread_mutex_unlock( (pthread_mutex_t*) &bogus[50] ); 

}

int main ( void )
{
   nearly_main(); fprintf(stderr, "---------------------\n" );
   nearly_main();
   return 0;
}
