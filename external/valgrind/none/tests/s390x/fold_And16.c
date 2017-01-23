#include <stdio.h>

int main()
{
   unsigned long p;
   register unsigned long *msg = &p;

   
   __asm__ volatile ( "iihl %[p],0x0a00\n\t" 
                      "iihh %[p],0x6869\n\t" 
                      "nihh %[p],0x6868\n\t" : [p] "+d" (p) : : "cc");

   
   printf("%s", (char *)msg);

   return 0;
}
