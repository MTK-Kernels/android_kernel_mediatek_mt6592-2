#include <stdlib.h>
#include <string.h>


int main(int argc, char* argv[])
{
   char *s, *a __attribute__((unused)), *b __attribute__((unused));
   s = malloc(sizeof(char));

   
   
   a = strchr (s, '1');
   b = strrchr(s, '1');
   return 0;
}
