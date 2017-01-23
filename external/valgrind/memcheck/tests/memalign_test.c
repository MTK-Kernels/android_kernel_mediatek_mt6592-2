
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>

int main ( void )
{
  void* a[10];
  int i;
  unsigned long pszB = sysconf(_SC_PAGE_SIZE);
  assert(sizeof(long) == sizeof(void*));
  assert(pszB == 4096 || pszB == 16384 || pszB == 65536);

  for (i = 0; i < 10; i++) {
    a[i] = valloc(11111 * (i+1));
    
    assert( (((unsigned long)(a[i])) % pszB) == 0 );
    
  }
  for (i = 0; i < 10; i++) {
    
    free(a[i]);
  }
  free(a[9]);
  return 0;
}
