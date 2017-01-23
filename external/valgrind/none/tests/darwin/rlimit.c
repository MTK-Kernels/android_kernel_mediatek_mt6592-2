
#include <stdio.h>
#include <sys/resource.h>

int main(void)
{
   struct rlimit rlp;
   getrlimit(RLIMIT_NOFILE, &rlp);
   fprintf(stderr, "RLIMIT_NOFILE is %lld\n", (long long)rlp.rlim_cur);
   return 0;
}

