#include <stdlib.h>

int main(void)
{                                
   int* x = realloc(NULL, 800);  
   int* y __attribute__((unused)); 

   x = realloc(x, 800);          

   x = realloc(x, 400);          

   x = realloc(x, 1200);         

   y = realloc(x+10, 1600);      

   x = realloc(x, 0);            
                                 
   return 0;
}
