

#include <stdio.h>
#include <stdlib.h>

#include "../memcheck.h"

int main ( void )
{
  volatile int* volatile mem
     = (volatile int* volatile)malloc(1000 * sizeof(int));
  free((void*)mem);

  
  fprintf(stderr, "\nDoing invalid access.  Expect complaint.\n\n");
  mem[123] = 0;

  
  fprintf(stderr, "\nDisabling address error reporting for the range.\n\n");
  VALGRIND_DISABLE_ADDR_ERROR_REPORTING_IN_RANGE(mem, 1000 * sizeof(int));

  
  fprintf(stderr, "\nDoing invalid another access.  Expect no complaint.\n\n");
  mem[456] = 0;
 
  
  fprintf(stderr, "\nPartially reenabling address error reporting.\n\n");
  VALGRIND_ENABLE_ADDR_ERROR_REPORTING_IN_RANGE(&mem[789], 1);

  
  fprintf(stderr, "\nDoing a third access.  Expect complaint.\n\n");
  mem[789] = 0;

  
  fprintf(stderr, "\nExiting.  Expect warnings of 2 remaining ranges.\n\n");

  return 0;
}
