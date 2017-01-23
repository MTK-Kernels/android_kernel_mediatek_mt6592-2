
#include <assert.h>
#include <time.h>

#define REPS   1000*1000*10

__attribute__((noinline))
int f(int i)
{
   
   
   char big_array[500];
   big_array[  0] = 12;
   big_array[ 23] = 34;
   big_array[256] = 56;
   big_array[434] = 78;
   assert( 480 == (&big_array[490] - &big_array[10]) );
   return big_array[i];
}

int main(void)
{
   int i, sum = 0;

   struct timespec req __attribute__((unused));
   req.tv_sec  = 0;
   req.tv_nsec = 100*1000*1000;   

   
   
   
   
   for (i = 0; i < REPS; i++) {
      sum += f(i & 0xff);
   }
   return ( sum == 0xdeadbeef ? 1 : 0 );
}

