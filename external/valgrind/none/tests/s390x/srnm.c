#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include "opcodes.h"

#define srnm(b,d) \
   ({ \
      __asm__ volatile ( "lghi 8," #b "\n\t" \
                         "srnm " #d "(8)\n\t" ::: "8"); \
   })

unsigned
get_rounding_mode(void)
{
   unsigned fpc;

   __asm__ volatile ("stfpc  %0\n\t" : "=m"(fpc));

   return fpc & 0x7;
}

int main(void)
{
   printf("initial rounding mode = %u\n", get_rounding_mode());

   
   srnm(1,2);  
   printf("rounding mode = %u\n", get_rounding_mode());

   srnm(2,0);
   printf("rounding mode = %u\n", get_rounding_mode());

   srnm(0,1);
   printf("rounding mode = %u\n", get_rounding_mode());

   srnm(0,0);
   printf("rounding mode = %u\n", get_rounding_mode());

   
   srnm(0xff,0);  
   printf("rounding mode = %u\n", get_rounding_mode());

   srnm(0,0xfe);  
   printf("rounding mode = %u\n", get_rounding_mode());

   srnm(0xf0,0x0f);  
   printf("rounding mode = %u\n", get_rounding_mode());

   return 0;
}
