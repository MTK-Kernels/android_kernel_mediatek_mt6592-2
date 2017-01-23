#include "tests/sys_mman.h"

int main ( void )
{
#if defined(__powerpc64__) && _CALL_ELF != 2
   unsigned long long int p[3];
   p[0] = (unsigned long long int)get_unmapped_page();
   p[1] = 0;
   p[2] = 0;
#else
   char* p = get_unmapped_page();
#endif
   return ((int(*)(void)) p) ();
}

