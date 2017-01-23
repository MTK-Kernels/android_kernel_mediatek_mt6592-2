
#include <stdlib.h>
#include <stdio.h>

#include "../memcheck.h"


int main ( void )
{
   char* a;

   fprintf(stderr, "\n---- part defined, address error at end ----\n\n");
   a = malloc(8);
   a[0] = a[1] = a[2] = a[3] = a[6] = a[7] = 'x';
   (void) VALGRIND_CHECK_MEM_IS_DEFINED(a, 9);
   free(a);

   fprintf(stderr, "\n---- part defined, address error at start ----\n\n");
   a = malloc(8);
   a[0] = a[1] = a[2] = a[3] = a[6] = a[7] = 'x';
   (void) VALGRIND_CHECK_MEM_IS_DEFINED(a-1, 9);
   free(a);

   fprintf(stderr, "\n---- fully defined, address error at end ----\n\n");
   a = malloc(8);
   a[0] = a[1] = a[2] = a[3] = a[4] = a[5] = a[6] = a[7] = 'x';
   (void) VALGRIND_CHECK_MEM_IS_DEFINED(a, 9);
   free(a);

   fprintf(stderr, "\n---- fully defined, address error at start ----\n\n");
   a = malloc(8);
   a[0] = a[1] = a[2] = a[3] = a[4] = a[5] = a[6] = a[7] = 'x';
   (void) VALGRIND_CHECK_MEM_IS_DEFINED(a-1, 9);
   free(a);

   return 0;
}
