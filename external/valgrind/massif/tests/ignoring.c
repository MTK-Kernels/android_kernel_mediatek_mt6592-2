

#include <stdlib.h>

int main(void)
{
   int i;
   
   
   int* x = malloc(1024);
   free(x);

   
   malloc(512);

   
   
   
   
   

   for (i = 0; i < 350; i++) {
      int* y = malloc(256);
      free(y);
   }
   
   return 0;
}
