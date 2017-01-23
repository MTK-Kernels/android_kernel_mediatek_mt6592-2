#include <stdlib.h>

int main(void)
{
   int i;
   for (i = 0; i < 20; i++) {
      int* p;           
      p = malloc(1600); 
      p = malloc(16);   
      free(p);          
   }
   return 0;
}
