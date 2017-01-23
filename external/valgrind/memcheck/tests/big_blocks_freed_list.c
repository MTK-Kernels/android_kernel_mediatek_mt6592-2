#include <stdlib.h>
static void jumped(void)
{
   ;
}
int main(int argc, char *argv[])
{
   char *semi_big = NULL;
   char *big = NULL;
   char *small = NULL;
   char *other_small = NULL;
   int i;
   int j;

   semi_big = malloc (900000);
   big = malloc (1000015);
   free(semi_big);
   free(big);
   if (big[1000] > 0x0) jumped();
   if (semi_big[1000] > 0x0) jumped();

   small = malloc (10000);
   free(small);

   if (big[2000] > 0x0) jumped();
   if (semi_big[2000] > 0x0) jumped();

   big = NULL;

   {
      big = malloc (1000015);
      free(big);
      if (small[10] > 0x0) jumped();
      
      if (big[10] > 0x0) jumped();
   }
   
   
   for (i = 0; i < 100; i++) {
      other_small = malloc(10000);
      for (j = 0; j < 10000; j++)
         other_small[j] = 0x1;
   }
   if (small[10] > 0x0) jumped();
   return 0;
}
