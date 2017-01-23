#include "scalar.h"

int main(void)
{
   
   long* px  = malloc(sizeof(long));
   long  x0  = px[0];
   int   res __attribute__((unused));

   
   
   
   GO(__NR_exit_group, "1s 0m");
   SY(__NR_exit_group, x0);

   return(0);
}

