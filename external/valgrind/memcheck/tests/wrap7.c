
#include <stdio.h>
#include "valgrind.h"


extern void actual ( void );

void I_WRAP_SONAME_FNNAME_ZU(wrap7soZdso,actual) ( void )
{
   OrigFn fn;
   VALGRIND_GET_ORIG_FN(fn);
   printf("wrapper-pre\n");
   CALL_FN_v_v(fn);
   printf("wrapper-post\n");
}


int main ( void )
{
   printf("starting\n");
   actual();
   return 0;
}
