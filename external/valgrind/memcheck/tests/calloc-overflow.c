#include <stdlib.h>
#include <stdio.h>
#include "pub_tool_basics.h"

int main(void)
{
   
   
   
   
   int* x;
#if VG_WORDSIZE == 8
   size_t szB = 0x1000000010000001ULL;
#else
   size_t szB = 0x10000001UL;
#endif
   x = calloc(szB, 0x10);
   fprintf(stderr, "x = %#lx\n", (long)x);
   return 0;
}
