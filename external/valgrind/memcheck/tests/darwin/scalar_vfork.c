#include "scalar.h"

int main(void)
{
   int res;
   
   
   GO(__NR_vfork, 66, "0e");
   SY(__NR_vfork);

   return(0);
}

