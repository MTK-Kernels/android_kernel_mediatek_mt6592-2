#include <assert.h>
#include <stdlib.h>
#include <sys/poll.h>


int main(void)
{
   
   struct pollfd* ufds = malloc(2 * sizeof(struct pollfd) - 1);
   assert(ufds);

   ufds[0].fd = 0;
   ufds[0].events = 0;
   
   ufds[1].events = 0;

   
   
   poll(ufds, 2, 200);

   return 0;
}
