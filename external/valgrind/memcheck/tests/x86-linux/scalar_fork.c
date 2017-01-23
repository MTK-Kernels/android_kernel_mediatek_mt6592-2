#include "scalar.h"

int main(void)
{
   int res __attribute__((unused));
   
   
   
   
   GO(__NR_fork, "0e");
   SY(__NR_fork);

   return(0);
}

