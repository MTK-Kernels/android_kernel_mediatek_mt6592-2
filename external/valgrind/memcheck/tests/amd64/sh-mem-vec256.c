

#define VECTOR_BYTES 32

static __attribute__((noinline))
void vector_copy ( void* dst, void* src )
{
  __asm__ __volatile__(
     "vmovupd (%1), %%ymm7 ; vmovupd %%ymm7, (%0)"
     :  :  "r"(dst), "r"(src) : "memory","xmm7"
  );
}

#include "../common/sh-mem-vec128.tmpl.c"
