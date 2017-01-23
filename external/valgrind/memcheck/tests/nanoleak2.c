

#include <stdlib.h>

int* a;

int main ( void )
{
  a = malloc(1000);  
  a[0] = 0;
  return a[0];
}
