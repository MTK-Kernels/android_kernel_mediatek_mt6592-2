
#include <stdio.h>

extern int fooble ( int, int );

int main ( void )
{
   int arr[2];
   arr[0] = 3;
   
   printf("fooble: result is %d\n", fooble(arr[0], arr[1]));
   return 0;
}
