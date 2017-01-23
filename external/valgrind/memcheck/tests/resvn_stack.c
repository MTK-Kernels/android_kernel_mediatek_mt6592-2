#include <stdio.h>

__attribute__((noinline)) void big(void)
{
   /* The below ensures the stack grows a lot. However, we hope the stack
      extension is not done yet, as no memory has been read/written. */
   volatile char c[200000];

   
   if (c[200000 - 1])
      fprintf(stderr, "Accessing fresh %s\n", "stack");
   else
      fprintf(stderr, "Accessing %s stack\n", "fresh");

}

int main(void )
{
   big();
   return 0;
}
