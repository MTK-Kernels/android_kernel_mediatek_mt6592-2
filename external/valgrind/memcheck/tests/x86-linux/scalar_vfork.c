#include "scalar.h"

int main(void)
{
   int res __attribute__((unused));
   
   
   
   
   GO(__NR_vfork, "0e");
   SY(__NR_vfork);

   return(0);
}

