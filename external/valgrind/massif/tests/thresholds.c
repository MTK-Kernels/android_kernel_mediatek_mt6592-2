
#include <stdlib.h>

void my_malloc1(int n)
{
   malloc(n);
}

void my_malloc2(int n)
{
   malloc(n);
}

void my_malloc3(int n)
{
   malloc(n);
}

void a7550(void)
{
   my_malloc1(48000);
   my_malloc2( 7200);
}

void a450(void)
{
   my_malloc2(2400);
   my_malloc1( 800);
   my_malloc2( 800);
   my_malloc1( 400);
}

int main(void)
{
   a7550(); 
   a450(); 
   my_malloc1(4000);       
   malloc(16000);
   my_malloc3(400);
   return 0;
}
