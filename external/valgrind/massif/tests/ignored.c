#include <stdlib.h>


int* ignore1(void)
{
   
   int* ignored_x1 = malloc(400);
   int* ignored_x2 = malloc(400);
   free(ignored_x2);
   return ignored_x1;
}

void ignore2(int* x, int* ignored_x)
{
   
   x = realloc(x, 800);   
   x = realloc(x, 400);   

   
   ignored_x = realloc(ignored_x, 800);   
   ignored_x = realloc(ignored_x, 400);   
}

int main(void)
{
   int* x;
   int* ignored_x;

   
   x = malloc(400);

   
   ignored_x = ignore1();

   
   
   x = realloc(x, 800);
   x = realloc(x, 400);

   
   ignored_x = realloc(ignored_x, 800);
   ignored_x = realloc(ignored_x, 400);

   ignore2(x, ignored_x);

   x = realloc(ignored_x, 0);    

   return 0;
}

