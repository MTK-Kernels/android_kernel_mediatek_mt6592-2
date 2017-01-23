#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/syslimits.h>


int main(int argc, char *argv[], char *envp[], char *apple[])
{
   char *pargv = calloc((PATH_MAX+1), sizeof(char)),
        *pappl = calloc((PATH_MAX+1), sizeof(char));
   int i;

   for (i = 0; envp[i]; i++)
      ;

   
   assert(envp[i+1] == apple[0]);

   
   
   realpath(argv[0], pargv);
   realpath(apple[0], pappl);
   assert(0 == strcmp(pargv, pappl));

   return 0;
}

