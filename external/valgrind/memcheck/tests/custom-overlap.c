
#include <stdlib.h>
#include "valgrind.h"

int main(void)
{
   char* x;
   
   
   
   x = malloc(1000);
   VALGRIND_MALLOCLIKE_BLOCK(x,      16, 0, 0);
   VALGRIND_MALLOCLIKE_BLOCK(x+100,  32, 0, 0);
   VALGRIND_MALLOCLIKE_BLOCK(x+200,  64, 0, 0);
   VALGRIND_MALLOCLIKE_BLOCK(x+300, 128, 0, 0);

   
   
   x = malloc(1000);
   VALGRIND_MALLOCLIKE_BLOCK(x+100,  32, 0, 0);
   VALGRIND_MALLOCLIKE_BLOCK(x+200,  64, 0, 0);
   VALGRIND_MALLOCLIKE_BLOCK(x+300, 128, 0, 0);

   return 0;
}
