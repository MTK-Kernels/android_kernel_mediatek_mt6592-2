#include <unistd.h>

char base[] ="0123456789012345678901234567890123456789";

void
stmg_no_wrap(void)
{
   char buf[24];

   
   asm volatile( "lg   5,  0(%1)\n\t"
                 "lg   6,  8(%1)\n\t"
                 "lg   7, 16(%1)\n\t"
                 "stmg 5, 7, %0\n\t"
                 :"=m" (buf)
                 : "a" (base)
                 : "5", "6", "7");
   
   asm volatile( "lghi 2, 1\n\t"   
                 "lgr  3, %0\n\t"  
                 "lghi 4, 24\n\t"  
                 "svc  4\n\t"
                 : : "a" (buf)
                 : "2", "3", "4");
}

void
stmg_wrap(void)
{
   char buf[64];

   
   asm volatile( "lg   15,  0(%1)\n\t"
                 "lg    0,  8(%1)\n\t"
                 "lg    1, 16(%1)\n\t"
                 "lg    2, 24(%1)\n\t"
                 "stmg 15, 2, %0\n\t"
                 :"=m" (buf)
                 : "a" (base)
                 : "15", "0", "1", "2");
   
   asm volatile( "lghi 2, 1\n\t"   
                 "lgr  3, %0\n\t"  
                 "lghi 4, 32\n\t"  
                 "svc  4\n\t"
                 : : "a" (buf)
                 : "2", "3", "4");
}


int main(void)
{
   stmg_no_wrap();
   write(1, "\n", 1);
   stmg_wrap();
   write(1, "\n", 1);

   return 0;
}
