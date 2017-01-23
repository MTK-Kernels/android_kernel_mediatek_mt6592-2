#include <unistd.h>

volatile void *ptr;

int main()
{
   ptr = sbrk(9*1024*1024);

   return 0;
}
