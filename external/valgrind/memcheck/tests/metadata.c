
#include <stdio.h>
#include <stdlib.h>
#include "../memcheck.h"


int main ( void )
{
  int* a = malloc(10 * sizeof(int));
  int* b = malloc(10 * sizeof(int));
  int* v = malloc(10 * sizeof(int));
  int i, sum, res;

  for (i = 0; i < 10; i++) {
     if (i != 5) 
        a[i] = i;
  }

  
  for (i = 0; i < 10; i++)
     b[i] = 0;

  

  
  res = VALGRIND_GET_VBITS(a, v, 10*sizeof(int) );
  printf("result of GET is %d (1 for success)\n", res);

  for (i = 0; i < 10; i++)
     printf("%d 0x%08x\n", i, v[i]);

  
  res = VALGRIND_SET_VBITS(b, v, 10*sizeof(int) );
  printf("result of SET is %d (1 for success)\n", res);
  
  sum = 100;
  for (i = 0; i < 10; i++)
     sum += b[i];

  
  if (sum == 0) 
    printf("sum == 0\n"); 
  else
    printf("sum != 0\n");

  return 0;
}
