#include <unistd.h>

volatile void *ptr;

int main()
{
   int i;
   for (i=0; i < 10; ++i) {
      ptr = sbrk(1024*1024);
   }
   return 0;
}
