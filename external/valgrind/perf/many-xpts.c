#include <stdlib.h>

#define nth_bit(x, n)   ((x >> n) & 1)
#define Fn(N, Np1) \
   void* a##N(int x) { return ( nth_bit(x, N) ? a##Np1(x) : a##Np1(x) ); }


void* a999(int x)
{
   return malloc(100);
}

Fn(17, 999)
Fn(16, 17)
Fn(15, 16)
Fn(14, 15)
Fn(13, 14)
Fn(12, 13)
Fn(11, 12)
Fn(10, 11)
Fn( 9, 10)
Fn( 8, 9)
Fn( 7, 8)
Fn( 6, 7)
Fn( 5, 6)
Fn( 4, 5)
Fn( 3, 4)
Fn( 2, 3)
Fn( 1, 2)
Fn( 0, 1)

int main(void)
{
   int i;

   
   for (i = 0; i < (1 << 18); i++)
      a0(i);

   
   for (i = 0; i < 100000; i++) {
      free(a1(234));
      free(a2(111));
   }

   return 0;
}
