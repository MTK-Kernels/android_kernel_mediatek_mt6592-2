#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static void breakme(int line)
{
   fprintf(stderr, "breakme function called from line %d\n", line);
   fflush(stderr);
}
static char undefined[10] = "undefined";
int main (int argc, char *argv[])
{


   breakme(__LINE__); 

   
   fprintf(stderr, "before reading 0/4/8\n");
   if (undefined[0] == 'u')
      fprintf(stderr, "u: Expected value at 0\n");
   else
      fprintf(stderr, "u: Unexpected value at 0\n");
   
   if (undefined[4] == 'f')
      fprintf(stderr, "f: Expected value at 4\n");
   else
      fprintf(stderr, "f: Unexpected value at 4\n");
   
   if (undefined[8] == 'd')
      fprintf(stderr, "d: Expected value at 8\n");
   else
      fprintf(stderr, "d: Unexpected value at 8\n");


   
   fprintf(stderr, "before writing 0\n");
   undefined[0] = 'U';

   fprintf(stderr, "before writing 4\n");
   undefined[4] = 'F';

   fprintf(stderr, "before writing 8\n");
   undefined[8] = 'D';

   fprintf(stderr, "after writing 8\n");

   fprintf(stderr, "value %s\n", undefined);

   fprintf(stderr, "before rewriting 0\n");
   undefined[0] = '0';

   fprintf(stderr, "before rewriting 4\n");
   undefined[4] = '4';

   fprintf(stderr, "before rewriting 8\n");
   undefined[8] = '8';
   
   fprintf(stderr, "value %s\n", undefined);

   {
      char *k50 = malloc(50000);
      memset (k50, 'p', 50000);
      fprintf(stderr, "value of k50[1000] %c\n", k50[1000]);
      free(k50); 
   }

   exit(0);
}
