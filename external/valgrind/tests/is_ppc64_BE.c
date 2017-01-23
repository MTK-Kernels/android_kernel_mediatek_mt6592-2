#include <stdio.h>
#include <stdlib.h>
#include <string.h>


int main(void)
{
#if defined(VGP_ppc64be_linux)
   return 0;
#else
   return 1;
#endif
}
