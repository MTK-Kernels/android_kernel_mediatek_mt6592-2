#include <stdlib.h>


int main(void)
{
   
   int i;
   for (i = 0; i < 10; i++) {
      malloc(10 * 1024 * 1024);
   }
   
   return 0;
}
