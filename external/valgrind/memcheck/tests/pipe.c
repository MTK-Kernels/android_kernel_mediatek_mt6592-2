
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
   int *filedes = malloc(2 * sizeof(int));

   pipe(filedes);

   return 0;
}
