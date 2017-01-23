#include <assert.h>
#include "tests/malloc.h"
#include <stdlib.h>
#include <stdio.h>

int main(void)
{
#  if !defined(VGO_darwin)
   
   
   int* x = malloc(99);

   
   assert(99 == malloc_usable_size(x));
   assert( 0 == malloc_usable_size(NULL));
   assert( 0 == malloc_usable_size((void*)0xdeadbeef));
#  endif

   return 0;
}
