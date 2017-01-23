
#include <stdlib.h>

int main(void)
{
   int i, *x1, *x2, *x3, *x4;
   for (i = 0; i < 1500; i++) {
      x1 = malloc( 800 * 1000);
      x2 = malloc(1100 * 1000);
      free(x1);
      x3 = malloc(1200 * 1000);
      free(x2);
      free(x3);
      x4 = malloc( 900 * 1000);
      free(x4);
   }
   return 0;
}
