#include <stdio.h>
#include <stdlib.h>
int main()
{
   __attribute__((unused)) char *p = malloc (1);
   char *b1 = malloc (128);
   char *b2 = malloc (128);
   fprintf (stderr, "b1 %p b2 %p\n", b1, b2);

   
   
   
   
   
   b1[127 + 70] = 'a';
   return 0;
}
