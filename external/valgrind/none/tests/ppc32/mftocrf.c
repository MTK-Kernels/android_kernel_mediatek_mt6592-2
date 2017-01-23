
#include <stdio.h>
#include <config.h>

static
int try_mtocrf ( int x )
{
  int base = 0x31415927;
  int res;
#ifdef HAVE_AS_PPC_MFTOCRF
  
  __asm__ __volatile__(
     "mtcr %0"
     :  :  "b"(base) : "cc" );

  
  __asm__ __volatile__(
     "mtocrf 4, %0"
     :  :  "b"(x) : "cc" );

  
  __asm__ __volatile__(
     "mfcr %0"
     : "=b"(res) :  );
#else
  res = 42;
#endif
  return res;
}

static
int try_mfocrf ( int x ) 
{
   int res;
#ifdef HAVE_AS_PPC_MFTOCRF
   
   __asm__ __volatile__(
     "mtcr %0"
     :  :  "b"(x) : "cc" );

  
  __asm__ __volatile__(
     "li %0,0\n\t"
     "mfocrf %0,64"
     : "=b"(res) :  );
#else
  res = 42;
#endif
  return res;
}


int main ( void )
{
  int i, j;
  for (i = 0; i < 32; i++) {
    printf("0x%08x\n", try_mtocrf( 1<<i ));
  }
  printf("\n");
  j = 1;
  for (i = 0; i < 32; i++) {
    printf("0x%08x\n", try_mfocrf( j ));
    j *= 3;
  }

  return 0;
}
