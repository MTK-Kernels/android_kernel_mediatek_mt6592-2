#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include "opcodes.h"

#define srnmb(b,d) \
   ({ \
      __asm__ volatile ( "lghi 8," #b "\n\t" \
                         SRNMB(8,d) \
                         ::: "8"); \
   })


#define srnmb0(d) \
   ({ \
      __asm__ volatile ( SRNMB(0,d) \
                         ::: "0"); \
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

   
   srnmb(1,002);  
   printf("rounding mode = %u\n", get_rounding_mode());

   srnmb(2,000);
   printf("rounding mode = %u\n", get_rounding_mode());

   srnmb(0,001);
   printf("rounding mode = %u\n", get_rounding_mode());

   srnmb(0,000);
   printf("rounding mode = %u\n", get_rounding_mode());

#if 0
   
   srnmb(7,000);  
   printf("rounding mode = %u\n", get_rounding_mode());

   srnmb(0,000);  
   printf("rounding mode = %u\n", get_rounding_mode());

   srnmb(0,007);  
   printf("rounding mode = %u\n", get_rounding_mode());
#endif

   srnmb(0,001);
   printf("rounding mode = %u\n", get_rounding_mode());

   srnmb0(004);    
   printf("rounding mode = %u\n", get_rounding_mode());

   return 0;
}
