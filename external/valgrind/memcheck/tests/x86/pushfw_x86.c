

#include <stdio.h>

int arr[3];

int main ( void )
{
   
   __asm__ __volatile__(
      "movl %%esp,0(%%eax)\n\t"
      "pushfw\n\t"
      "movl %%esp,4(%%eax)\n\t"
      "popfw\n\t"
      "movl %%esp,8(%%eax)\n"
      :  :  "a"(&arr) :  "memory","cc"
   );

   printf("%x %x %x\n", arr[0]-arr[0], arr[0]-arr[1], arr[0]-arr[2]);
   return 0;
}
