

#include <stdio.h>
#include <stdlib.h>
static void* return_arg(void* q);
int main ( void )
{
   void* p = (void*)0x87654321;
   int q[] = { 1, 2, 3 };
   
   
   free(p);

   
   free(return_arg(q));

   return 0;
}

static void* return_arg(void* q)
{
   return q;
}

