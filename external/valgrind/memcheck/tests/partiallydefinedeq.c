
#include <stdio.h>
#include <stdlib.h>


__attribute__((noinline)) 
void foo ( int* p1, int* p2, unsigned int * hack )
{
  *hack = 0x80808080;
  if (*p1 == *p2)
    printf("foo\n");
  else
    printf("bar\n");
}

static void bar ( void );
int main ( void )
{

  unsigned int hack;

  int* junk1 = malloc(sizeof(int));
  int* junk2 = malloc(sizeof(int));

  short* ps1 = (short*)junk1;
  short* ps2 = (short*)junk2;

  int*   pi1 = (int*)junk1;
  int*   pi2 = (int*)junk2;
  bar();
  
  foo(pi1,pi2, &hack);

  
  
  
  
  *ps1 = 41;
  *ps2 = 42;
  foo(pi1,pi2, &hack);

  
  
  
  *ps1 = 42;
  *ps2 = 42;
  foo(pi1,pi2, &hack);

  return 0;
}

static __attribute__((noinline)) void bar ( void )
{
#if defined(__powerpc__) || defined(__powerpc64__) || defined(__arm__)
  fprintf(stderr, "Currently running on ppc32/64/arm: this test should give 3 errors, not 2.\n");
#endif
#if defined(__s390__)
  fprintf(stderr, "On s390 we might see 2 or 3 errors.\n");
#endif
}
