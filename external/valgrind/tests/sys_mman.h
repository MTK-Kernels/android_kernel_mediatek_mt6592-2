
#include <sys/mman.h>

#if defined(VGO_darwin)
#  define MAP_ANONYMOUS MAP_ANON
#endif


#include <assert.h>
#include <unistd.h>


__attribute__((unused))
static void* get_unmapped_page(void)
{
   void* ptr;
   int r;
   long pagesz = sysconf(_SC_PAGE_SIZE);
   assert(pagesz == 4096 || pagesz == 16384 || pagesz == 65536);
   ptr = mmap(0, pagesz, PROT_READ, MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);
   assert(ptr != (void*)-1);
   r = munmap(ptr, pagesz);
   assert(r == 0);
   return ptr;
}

