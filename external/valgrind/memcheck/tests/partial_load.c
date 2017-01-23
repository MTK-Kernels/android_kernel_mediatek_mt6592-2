
#include <stdlib.h>
#include <assert.h>

int main ( void )
{
  long  w;
  int   i;
  char* p;

  assert(sizeof(long) == sizeof(void*));

  
  p = calloc( sizeof(long)-1, 1 );
  assert(p);
  w = *(long*)p;
  free(p);

  
  p = calloc( sizeof(long), 1 );
  assert(p);
  p++;
  w += *(long*)p;
  p--;
  free(p);

  
  p = calloc( sizeof(short)-1, 1 );
  assert(p);
  w += (long)( *(short*)p );
  free(p);

  
  p = calloc( sizeof(long), 1 );
  assert(p);
  free(p);
  w += *(long*)p;

  
  for (i = 0; i < 64; i++)
     w <<= 1;

  return (int)w;
}

