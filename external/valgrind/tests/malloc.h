
#include <stdlib.h>
#if defined(VGO_darwin)
#  include <malloc/malloc.h>
#else
#  include <malloc.h>
#endif

#include <assert.h>

__attribute__((unused))
static void* memalign16(size_t szB)
{
   void* x;
#if defined(VGO_darwin)
   
   x = malloc(szB);
#else
   x = memalign(16, szB);
#endif
   assert(x);
   assert(0 == ((16-1) & (unsigned long)x));
   return x;
} 

