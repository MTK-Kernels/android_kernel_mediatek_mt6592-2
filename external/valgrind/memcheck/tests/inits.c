
#include <stdio.h>


int        g;
static int gs;

int main(void)
{
   int        l;
   static int ls;
   
   if (gs == 0xCAFEBABE) printf("1!\n");
   if (g  == 0xCAFEBABE) printf("2!\n");
   if (ls == 0xCAFEBABE) printf("3!\n");
   if (l  == 0xCAFEBABE) printf("4!\n");  
   
   return 0;
}
