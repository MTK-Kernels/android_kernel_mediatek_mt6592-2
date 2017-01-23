
#include <stdlib.h>

int main(void)
{
   int i, t = 0;
   char* x = malloc(1000);

   
   for (i = 0; i < 1000; i++)
      x[i] &= (i & 0xff);

   
   x = realloc(x, 10000);

   
   for (i = 0; i < 1000; i++)
      t += x[i];
   
   __asm__ __volatile__ ("" :: "r"(t));

   return 0;
}

