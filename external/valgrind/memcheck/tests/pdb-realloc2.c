

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include "memcheck/memcheck.h"

typedef unsigned char UChar;
typedef unsigned int  UInt;


static UInt seed = 0;
static inline UInt myrand ( UInt size )
{
   
   seed = 1664525UL * seed + 1013904223UL;
   return seed % size;
}

static void barf ( int size, int offset )
{
   printf("pdb-realloc2: fail: size %d, offset %d\n", size,offset);
   exit(1);
}

void do_test ( int size )
{
   int i,j,r;
   UChar* v;
   UChar* p = malloc(size);
   assert(p);
   
   seed = 0;
   for (i = 0; i < size; i++) {

      j = myrand( 256 * 25 );
      
      if (j >= 256 * 13) {
         
         p[i] = 0xFF;
      } else 
      if (j >= 256 && j < 256*13) {
         
         p[i] = 0;
      } else {
         
         p[i] &= (UChar)j;
      }

   }

   
   for (i = 1; i <= 100; i++) {
      p = realloc(p, size+i);
      assert(p);
   }

   
   v = malloc(size+100);
   assert(v);
   r = VALGRIND_GET_VBITS(p,v, size+100);
   assert(r == 1);

   
   
   

   seed = 0;
   for (i = 0; i < size; i++) {

      j = myrand( 256 * 25 );

      if (j >= 256) {
         
         if (v[i] != 0)
            barf(size, i);
      } else {
         
         if (v[i] != (UChar)j)
            barf(size,i);
      }

   }

   
   for (i = 0; i < 100; i++) {
      if (v[size+i] != 0xFF)
         barf(size, i);
   }

   free(v);
   free(p);
}

int main ( void )
{
  int z;
  for (z = 0; z < 100; z++) {
     printf("pdb_realloc: z = %d\n", z);
     do_test(z);
     do_test(z + 173);
     do_test(z + 1731);
  }
  printf("pdb-realloc2: done\n");
  return 0;
}
