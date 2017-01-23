#include <stdio.h>
#include "opcodes.h"

int
main(void)
{
   printf("before\n");
   __asm__ volatile ( CEGBRA(1,0,0,0) : : : "cc", "memory");
   __asm__ volatile ( CEFBRA(3,0,0,0) : : : "cc", "memory");
   __asm__ volatile ( CDGBRA(4,0,0,0) : : : "cc", "memory");

   __asm__ volatile ( CEFBRA(5,0,0,0) : : : "cc", "memory");

   printf("after\n");
   return 0;
}
