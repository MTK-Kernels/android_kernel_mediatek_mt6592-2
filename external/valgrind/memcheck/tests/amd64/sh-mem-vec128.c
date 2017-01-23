

#define VECTOR_BYTES 16

static __attribute__((noinline))
void vector_copy ( void* dst, void* src )
{
  __asm__ __volatile__(
     "movups (%1), %%xmm7 ; movups %%xmm7, (%0)"
     :  :  "r"(dst), "r"(src) : "memory","xmm7"
  );
}

#include "../common/sh-mem-vec128.tmpl.c"
