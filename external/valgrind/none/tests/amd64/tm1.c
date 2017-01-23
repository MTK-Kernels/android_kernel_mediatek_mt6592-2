
#include <stdio.h>
#include <stdlib.h>



__attribute__((noinline))
static int transactionally_apply ( void(*f)(void*), void* arg )
{
  register int ok;
  __asm__ __volatile__(
     "  xbegin .Lzzqqfail" );
  f(arg);
  __asm__ __volatile__( 
     "  xend           \n\t"  
     "  movl $1,%0     \n\t"  
     "  jmp .Lzzqqout  \n\t"  
     ".Lzzqqfail:      \n\t"  
     "  movl $0,%0     \n\t"  
     ".Lzzqqout:       \n\t"  
     : "=r"(ok) : : "cc", "rax"
  );
  return ok;
}

void testfn ( void* arg )
{
}

int main ( void )
{
  long long int ok = transactionally_apply ( testfn, NULL );
  printf("transactionally_apply: ok = %lld (expected %d)\n", ok, 0);

  __asm__ __volatile__(
    "movq $0, %%rax  \n\t"
    "xtest           \n\t"   
    "setz %%al       \n\t"
    "movq %%rax, %0  \n\t"
    : "=r"(ok) : : "cc","rax"
  );
  printf("xtest: rflags.Z = %lld (expected %d)\n", ok, 1);


  __asm__ __volatile__( "xabort $0x1" );
  printf("xabort: outside transaction is nop.\n");

  return 0;
}
