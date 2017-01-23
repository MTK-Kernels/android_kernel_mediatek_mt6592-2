#include <sys/socket.h>
#include <stdlib.h>
#include <stdio.h>

int main(void)
{
   struct sockaddr name;
   int res1, res2, res3;
   unsigned len = 10;

   res1 = socket(PF_UNIX, SOCK_STREAM, 0);
   if (res1 == 0) {
      fprintf(stderr, "socket() failed\n");
      exit(1);
   }

   
   res2 = getsockname(res1, NULL,  &len);    
   res3 = getsockname(res1, &name, NULL);    
   if (res2 == -1) {
      fprintf(stderr, "getsockname(1) failed\n");
   }
   if (res3 == -1) {
      fprintf(stderr, "getsockname(2) failed\n");
   }
   
   return 0;
}

