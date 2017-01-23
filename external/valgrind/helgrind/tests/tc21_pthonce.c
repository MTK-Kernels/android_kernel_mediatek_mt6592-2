



#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <assert.h>

#include <pthread.h>

#define  NUM_THREADS 2

static pthread_once_t welcome_once_block = PTHREAD_ONCE_INIT;

static int unprotected1 = 0;
static int unprotected2 = 0;

void maybe_stall ( int myid )
{
   assert(myid >= 0 && myid < NUM_THREADS);
   if (myid > 0)
      sleep(1);
}

void welcome(void) {
   printf("welcome: Welcome\n");
   unprotected1++; 
}

void* child ( void* argV ) { 
   int r; 
   maybe_stall( *(int*)argV );
   r= pthread_once(&welcome_once_block, welcome); assert(!r);
   printf("child: Hi, I'm thread %d\n", *(int*)argV);
   unprotected2++; 
   return NULL;
}

int main ( void ) {
   int       *id_arg, i, r;
   pthread_t threads[NUM_THREADS];

   id_arg = (int *)malloc(NUM_THREADS*sizeof(int));

   printf("main: Hello\n");
   for (i = 0; i < NUM_THREADS; i++) {
      id_arg[i] = i;
      r= pthread_create(&threads[i], NULL, child, &id_arg[i]);
      assert(!r);
   }

   for (i = 0; i < NUM_THREADS; i++) {
      pthread_join(threads[i], NULL);
      
   }
   printf("main: Goodbye\n");
   return 0;
}
