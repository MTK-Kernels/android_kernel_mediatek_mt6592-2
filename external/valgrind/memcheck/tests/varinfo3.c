
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "memcheck/memcheck.h"
void croak ( void* aV )
{
  char* a = (char*)aV;
  char* undefp = malloc(1);
  char saved = *a;
  assert(undefp);
  *a = *undefp;
  (void) VALGRIND_CHECK_MEM_IS_DEFINED(a, 1);
  *a = saved;
  free(undefp);
}

#include <stdio.h>

static char static_global_def[10]    = {0,0,0,0,0, 0,0,0,0,0};
       char nonstatic_global_def[10] = {0,0,0,0,0, 0,0,0,0,0};
static char static_global_undef[10];
       char nonstatic_global_undef[10];

void bar ( char* p1, char* p2, char* p3, char* p4 )
{
   croak(p1);
   croak(p2);
   croak(p3);
   croak(p4);
}

void foo ( void )
{
   static char static_local_def[10]    = {0,0,0,0,0, 0,0,0,0,0};
          char nonstatic_local_def[10] = {0,0,0,0,0, 0,0,0,0,0};
   static char static_local_undef[10];
          char nonstatic_local_undef[10];
   croak ( 1 + (char*)&static_global_def );
   croak ( 2 + (char*)&nonstatic_global_def );
   croak ( 3 + (char*)&static_global_undef );
   croak ( 4 + (char*)&nonstatic_global_undef );
   bar( 5 + (char*)&static_local_def,
        6 + (char*)&nonstatic_local_def,
        7 + (char*)&static_local_undef,
        8 + (char*)&nonstatic_local_undef );
}

int main ( void )
{
  foo();
  return 0;
}
