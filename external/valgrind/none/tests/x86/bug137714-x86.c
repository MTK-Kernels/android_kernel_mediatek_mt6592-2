
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

typedef  unsigned char  UChar;
typedef  unsigned int   UInt;

static UInt randomUInt ( void )
{
   static UInt n = 0;
   
   n = 1664525UL * n + 1013904223UL;
   return n >> 17;
}

void maskmovq_mmx ( UChar* regL, UChar* regR )
{
   int i;
   UChar* dst = malloc(8);
   assert(dst);
   for (i = 0; i < 8; i++)
      dst[i] = 17 * (i+1);
   __asm__ __volatile__(
      "emms\n\t"
      "movq (%0), %%mm1\n\t"
      "movq (%1), %%mm2\n\t"
      "movl %2, %%edi\n\t"
      "maskmovq %%mm1,%%mm2"
      :  
      :  "r"(regL), "r"(regR), "r"(&dst[0])
      :  "edi", "memory", "cc"
   );
   for (i = 0; i < 8; i++)
      printf("%02x", dst[i]);
   free(dst);
}

void maskmovdqu_sse ( UChar* regL, UChar* regR )
{
   int i;
   UChar* dst = malloc(16);
   assert(dst);
   for (i = 0; i < 16; i++)
      dst[i] = i;
   __asm__ __volatile__(
      "movups (%0), %%xmm1\n\t"
      "movups (%1), %%xmm2\n\t"
      "movl %2, %%edi\n\t"
      "maskmovdqu %%xmm2,%%xmm1\n\t"
      "sfence"
      :  
      :  "r"(regL), "r"(regR), "r"(dst)
      :  "edi", "memory", "cc"
   );
   for (i = 0; i < 16; i++)
      printf("%02x", dst[i]);
   free(dst);
}

int main ( int argc, char** argv )
{
   int i, j;

   
   {
      UChar* regL = malloc(8);
      UChar* regR = malloc(8);
      assert(regL);
      assert(regR);
      for (i = 0; i < 10; i++) {
         for (j = 0; j < 8; j++) {
            regL[j] = (UChar)randomUInt();
            printf("%02x", regL[j]);
         }
         printf(" ");
         for (j = 0; j < 8; j++) {
            regR[j] = (UChar)randomUInt();
            printf("%02x", regR[j]);
         }
         printf(" ");
         maskmovq_mmx( regR, regL );
         printf("\n");
      }
   }

   
   {
      UChar* regL = malloc(16);
      UChar* regR = malloc(16);
      assert(regL);
      assert(regR);
      for (i = 0; i < 10; i++) {
         for (j = 0; j < 16; j++) {
            regL[j] = (UChar)randomUInt();
            printf("%02x", regL[j]);
         }
         printf(" ");
         for (j = 0; j < 16; j++) {
            regR[j] = (UChar)randomUInt();
            printf("%02x", regR[j]);
         }
         printf(" ");
         maskmovdqu_sse( regR, regL );
         printf("\n");
      }
   }

   return 0;
}
