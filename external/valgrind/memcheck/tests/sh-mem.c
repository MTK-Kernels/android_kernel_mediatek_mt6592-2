
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "memcheck/memcheck.h"


typedef unsigned char        U1;
typedef unsigned short       U2;
typedef unsigned int         U4;
typedef unsigned long long   U8;

typedef float                F4;
typedef double               F8;

#define SZB_OF_a    64

U8 a [SZB_OF_a / 8];    
U8 b [SZB_OF_a / 8];    


U8 build(int size, U1 byte)
{
   int i;
   U8 mask = 0;
   U8 shres;
   U8 res = 0xffffffffffffffffULL, res2;
   (void)VALGRIND_MAKE_MEM_UNDEFINED(&res, 8);
   assert(1 == size || 2 == size || 4 == size || 8 == size);

   for (i = 0; i < size; i++) {
      mask <<= 8;
      mask |= (U8)byte;
   }

   res &= mask;      
   
   
   
   
   (void)VALGRIND_GET_VBITS(&res, &shres, 8);
   res2 = res;
   (void)VALGRIND_MAKE_MEM_DEFINED(&res2, 8);  
   assert(res2 == shres);
   return res;
}

void check_all(U4 x, U4 y, U1 expected_byte, U1 expected_byte_alt, 
                           char* str, int offset)
{
   U1 sh[SZB_OF_a];     
   int i;

   (void)VALGRIND_GET_VBITS(a, sh, sizeof(a));
   for (i = x; i < y; i++) {
      if ( expected_byte != sh[i] && expected_byte_alt != sh[i] ) {
         fprintf(stderr, "\n\nFAILURE: %s, offset %d, byte %d -- "
                         "is 0x%x, should be 0x%x or 0x%x\n\n",
                         str, offset, i, sh[i], expected_byte, 
                         expected_byte_alt);
         exit(1);
      }
   }
}

int main(void)
{
   int h, i, j;
   U1 *undefA, expected_byte, expected_byte_alt;

   if (0 == RUNNING_ON_VALGRIND) {
      fprintf(stderr,
              "error: this program only works when run under Valgrind\n");
      exit(1);
   }

   
   
   
   assert( 0 == (long)a % 8);
   if (sizeof(void*) == 8) {
      assert( ((U1*)(&a[0])) < ((U1*)(32ULL * 1024*1024*1024)) );
   }

   
   assert(1 == sizeof(U1));
   assert(2 == sizeof(U2));
   assert(4 == sizeof(U4));
   assert(8 == sizeof(U8));

   
   
   
   
   
   
   
   undefA = calloc(1, 256);         
   (void)VALGRIND_MAKE_MEM_UNDEFINED(undefA, 256);
   for (i = 0; i < 256; i++) {
      undefA[i] &= i; 
   }

   
   
   
   
   
   
   
   
   

#define DO(NNN, Ty, ITy, isF4) \
   fprintf(stderr, "-- NNN: %d %s %s ------------------------\n", \
           NNN, #Ty, #ITy); \
    \
    \
   for (h = 0; h < NNN; h++) { \
      \
      size_t n  = sizeof(a); \
      size_t nN = n / sizeof(Ty); \
      Ty* aN    = (Ty*)a; \
      Ty* bN    = (Ty*)b; \
      Ty* aNb   = (Ty*)(((U1*)aN) + h);  \
      Ty* bNb   = (Ty*)(((U1*)bN) + h);  \
      \
      fprintf(stderr, "h = %d (checking %d..%d)   ", h, h, (int)(n-NNN+h)); \
      \
       \
      for (j = 0; j < 256; j++) { \
          \
         U8  tmp        = build(NNN, j); \
         ITy undefN_ITy = (ITy)tmp; \
         Ty* undefN_Ty; \
         {  \
            \
            U8  tmpDef     = tmp; \
            ITy undefN_ITyDef = undefN_ITy; \
            (void)VALGRIND_MAKE_MEM_DEFINED(&tmpDef,        8  );       \
            (void)VALGRIND_MAKE_MEM_DEFINED(&undefN_ITyDef, NNN);       \
            assert(tmpDef == (U8)undefN_ITyDef); \
         } \
         \
 \
         undefN_Ty = (Ty*)&undefN_ITy; \
         if (0 == j % 32) fprintf(stderr, "%d...", j);  \
         \
 \
         if (isF4) { \
            expected_byte = j; \
            expected_byte_alt = 0 != j ? 0xFF : j; \
         } else { \
            expected_byte = j; \
            expected_byte_alt = j; \
         } \
         \
 \
         for (i = 0; i < nN-1; i++) { aNb[i] = undefN_Ty[0]; } \
         check_all(h, n-NNN+h, expected_byte, expected_byte_alt, \
                   "STOREVn", h); \
         \
 \
         for (i = 0; i < nN-1; i++) { bNb[i] = aNb[i]; } \
         for (i = 0; i < nN-1; i++) { aNb[i] = bNb[i]; } \
         check_all(h, n-NNN+h, expected_byte, expected_byte_alt, "LOADVn", h); \
      } \
      fprintf(stderr, "\n"); \
   }

   
   
   
   
   DO(1, U1, U1, 0);
   DO(2, U2, U2, 0);
   DO(4, U4, U4, 0);
   DO(4, F4, U4, 1);
   DO(8, U8, U8, 0);
   DO(8, F8, U8, 0);
   
   return 0;
}
