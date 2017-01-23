#include <stdlib.h>

int main(void)
{
   int* x = malloc(5);
   int* y = malloc(10);
   int* z = malloc(2);
   int a, b, c;

   x = realloc(x, 5);   
   y = realloc(y, 5);   
   z = realloc(z, 5);   

   a = (x[5] == 0xdeadbeef ? 1 : 0);
   b = (y[5] == 0xdeadbeef ? 1 : 0);
   c = (z[5] == 0xdeadbeef ? 1 : 0);

   return a + b + c;
}
