#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>

int main(void)
{
   
   int* pi  = malloc(sizeof(int));

   
   char** pc  = malloc(sizeof(char*));
   
   
   
   
   
   
   syscall(pi[0]+__NR_write, pi[0], pc[0], pi[0]+1);

   return 0;
}

