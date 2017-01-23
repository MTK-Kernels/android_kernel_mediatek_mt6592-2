#include "tests/asm.h"
#include <stdio.h>


int shift_ndep( void )
{
  char shift_amt = 0;
  int x = -2;
  asm (
       "stc"                    "\n\t"
       "inc %[x]"               "\n\t"
       "shl %[shift_amt], %[x]" "\n\t"
       "adc $0, %[x]"           "\n\t"
       : [x] "+r" (x) : [shift_amt] "c" (shift_amt));
  return x;
}

int main ( void )
{
  int r = shift_ndep();
  if (r == 0)
    printf("Passed (%d).\n", r);
  else
    printf("Failed (%d).\n", r);
  return 0;
}
