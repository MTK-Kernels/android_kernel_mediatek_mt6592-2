
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

typedef unsigned long long int ULong;
typedef unsigned int   UInt;
typedef unsigned short UShort;
typedef unsigned char  UChar;

typedef signed int    Int;
typedef signed short  Short;

typedef signed long int  Word;


ULong btsq_mem ( char* base, Word bitno )
{
   UChar res;
   __asm__ 
   __volatile__("btsq\t%2, %0\n\t"
                "setc\t%1" 
                : "=m" (*base), "=q" (res)
                : "r" (bitno));
   return res;
}

ULong btrq_mem ( char* base, Word bitno )
{
   UChar res;
   __asm__ 
   __volatile__("btrq\t%2, %0\n\t"
                "setc\t%1" 
                : "=m" (*base), "=q" (res)
                : "r" (bitno));
   return res;
}

ULong btcq_mem ( char* base, Word bitno )
{
   UChar res;
   __asm__ 
   __volatile__("btcq\t%2, %0\n\t"
                "setc\t%1" 
                : "=m" (*base), "=q" (res)
                : "r" (bitno));
   return res;
}

ULong btq_mem ( char* base, Word bitno )
{
   UChar res;
   __asm__ 
   __volatile__("btq\t%2, %0\n\t"
                "setc\t%1" 
                : "=m" (*base), "=q" (res)
                : "r" (bitno)
                : "cc", "memory");
   return res;
}



ULong btsl_mem ( char* base, Word bitno )
{
   UChar res;
   __asm__ 
   __volatile__("btsl\t%2, %0\n\t"
                "setc\t%1" 
                : "=m" (*base), "=q" (res)
                : "r" ((Int)bitno));
   return res;
}

ULong btrl_mem ( char* base, Word bitno )
{
   UChar res;
   __asm__ 
   __volatile__("btrl\t%2, %0\n\t"
                "setc\t%1" 
                : "=m" (*base), "=q" (res)
                : "r" ((Int)bitno));
   return res;
}

ULong btcl_mem ( char* base, Word bitno )
{
   UChar res;
   __asm__ 
   __volatile__("btcl\t%2, %0\n\t"
                "setc\t%1" 
                : "=m" (*base), "=q" (res)
                : "r" ((Int)bitno));
   return res;
}

ULong btl_mem ( char* base, Word bitno )
{
   UChar res;
   __asm__ 
   __volatile__("btl\t%2, %0\n\t"
                "setc\t%1" 
                : "=m" (*base), "=q" (res)
                : "r" ((Int)bitno)
                : "cc", "memory");
   return res;
}




ULong btsw_mem ( char* base, Word bitno )
{
   UChar res;
   __asm__ 
   __volatile__("btsw\t%2, %0\n\t"
                "setc\t%1" 
                : "=m" (*base), "=q" (res)
                : "r" ((Short)bitno));
   return res;
}

ULong btrw_mem ( char* base, Word bitno )
{
   UChar res;
   __asm__ 
   __volatile__("btrw\t%2, %0\n\t"
                "setc\t%1" 
                : "=m" (*base), "=q" (res)
                : "r" ((Short)bitno));
   return res;
}

ULong btcw_mem ( char* base, Word bitno )
{
   UChar res;
   __asm__ 
   __volatile__("btcw\t%2, %0\n\t"
                "setc\t%1" 
                : "=m" (*base), "=q" (res)
                : "r" ((Short)bitno));
   return res;
}

ULong btw_mem ( char* base, Word bitno )
{
   UChar res;
   __asm__ 
   __volatile__("btw\t%2, %0\n\t"
                "setc\t%1" 
                : "=m" (*base), "=q" (res)
                : "r" ((Short)bitno)
                : "cc", "memory");
   return res;
}




ULong btsq_reg ( ULong reg_in, Word bitno, 
                        ULong* reg_out_p )
{
   UChar res;
   ULong reg_out;
   __asm__ 
   __volatile__("movq\t%3, %%rax\n\t"
                "btsq\t%2, %%rax\n\t"
                "movq\t%%rax, %1\n\t"
                "setc\t%0" 
                : "=q" (res), "=r" (reg_out)
                : "r" (bitno), "r" (reg_in)
                : "cc", "eax");
   *reg_out_p = reg_out;
   return res;
}


ULong btrq_reg ( ULong reg_in, Word bitno, 
                        ULong* reg_out_p )
{
   UChar res;
   ULong reg_out;
   __asm__ 
   __volatile__("movq\t%3, %%rax\n\t"
                "btrq\t%2, %%rax\n\t"
                "movq\t%%rax, %1\n\t"
                "setc\t%0" 
                : "=q" (res), "=r" (reg_out)
                : "r" (bitno), "r" (reg_in)
                : "cc", "eax");
   *reg_out_p = reg_out;
   return res;
}


ULong btcq_reg ( ULong reg_in, Word bitno, 
                        ULong* reg_out_p )
{
   UChar res;
   ULong reg_out;
   __asm__ 
   __volatile__("movq\t%3, %%rax\n\t"
                "btcq\t%2, %%rax\n\t"
                "movq\t%%rax, %1\n\t"
                "setc\t%0" 
                : "=q" (res), "=r" (reg_out)
                : "r" (bitno), "r" (reg_in)
                : "cc", "eax");
   *reg_out_p = reg_out;
   return res;
}


ULong btq_reg ( ULong reg_in, Word bitno, 
                       ULong* reg_out_p )
{
   UChar res;
   ULong reg_out;
   __asm__ 
   __volatile__("movq\t%3, %%rax\n\t"
                "btq\t%2, %%rax\n\t"
                "movq\t%%rax, %1\n\t"
                "setc\t%0" 
                : "=q" (res), "=r" (reg_out)
                : "r" (bitno), "r" (reg_in)
                : "cc", "eax");
   *reg_out_p = reg_out;
   return res;
}




ULong btsl_reg ( ULong reg_in, Word bitno, 
                        ULong* reg_out_p )
{
   UChar res;
   ULong reg_out;
   __asm__ 
   __volatile__("movq\t%3, %%rax\n\t"
                "btsl\t%2, %%eax\n\t"
                "movq\t%%rax, %1\n\t"
                "setc\t%0" 
                : "=q" (res), "=r" (reg_out)
                : "r" ((Int)bitno), "r" (reg_in)
                : "cc", "eax");
   *reg_out_p = reg_out;
   return res;
}


ULong btrl_reg ( ULong reg_in, Word bitno, 
                        ULong* reg_out_p )
{
   UChar res;
   ULong reg_out;
   __asm__ 
   __volatile__("movq\t%3, %%rax\n\t"
                "btrl\t%2, %%eax\n\t"
                "movq\t%%rax, %1\n\t"
                "setc\t%0" 
                : "=q" (res), "=r" (reg_out)
                : "r" ((Int)bitno), "r" (reg_in)
                : "cc", "eax");
   *reg_out_p = reg_out;
   return res;
}


ULong btcl_reg ( ULong reg_in, Word bitno, 
                        ULong* reg_out_p )
{
   UChar res;
   ULong reg_out;
   __asm__ 
   __volatile__("movq\t%3, %%rax\n\t"
                "btcl\t%2, %%eax\n\t"
                "movq\t%%rax, %1\n\t"
                "setc\t%0" 
                : "=q" (res), "=r" (reg_out)
                : "r" ((Int)bitno), "r" (reg_in)
                : "cc", "eax");
   *reg_out_p = reg_out;
   return res;
}


ULong btl_reg ( ULong reg_in, Word bitno, 
                       ULong* reg_out_p )
{
   UChar res;
   ULong reg_out;
   __asm__ 
   __volatile__("movq\t%3, %%rax\n\t"
                "btl\t%2, %%eax\n\t"
                "movq\t%%rax, %1\n\t"
                "setc\t%0" 
                : "=q" (res), "=r" (reg_out)
                : "r" ((Int)bitno), "r" (reg_in)
                : "cc", "eax");
   *reg_out_p = reg_out;
   return res;
}




ULong btsw_reg ( ULong reg_in, Word bitno, 
                        ULong* reg_out_p )
{
   UChar res;
   ULong reg_out;
   __asm__ 
   __volatile__("movq\t%3, %%rax\n\t"
                "btsw\t%2, %%ax\n\t"
                "movq\t%%rax, %1\n\t"
                "setc\t%0" 
                : "=q" (res), "=r" (reg_out)
                : "r" ((Short)bitno), "r" (reg_in)
                : "cc", "eax");
   *reg_out_p = reg_out;
   return res;
}


ULong btrw_reg ( ULong reg_in, Word bitno, 
                        ULong* reg_out_p )
{
   UChar res;
   ULong reg_out;
   __asm__ 
   __volatile__("movq\t%3, %%rax\n\t"
                "btrw\t%2, %%ax\n\t"
                "movq\t%%rax, %1\n\t"
                "setc\t%0" 
                : "=q" (res), "=r" (reg_out)
                : "r" ((Short)bitno), "r" (reg_in)
                : "cc", "eax");
   *reg_out_p = reg_out;
   return res;
}


ULong btcw_reg ( ULong reg_in, Word bitno, 
                        ULong* reg_out_p )
{
   UChar res;
   ULong reg_out;
   __asm__ 
   __volatile__("movq\t%3, %%rax\n\t"
                "btcw\t%2, %%ax\n\t"
                "movq\t%%rax, %1\n\t"
                "setc\t%0" 
                : "=q" (res), "=r" (reg_out)
                : "r" ((Short)bitno), "r" (reg_in)
                : "cc", "eax");
   *reg_out_p = reg_out;
   return res;
}


ULong btw_reg ( ULong reg_in, Word bitno, 
                       ULong* reg_out_p )
{
   UChar res;
   ULong reg_out;
   __asm__ 
   __volatile__("movq\t%3, %%rax\n\t"
                "btw\t%2, %%ax\n\t"
                "movq\t%%rax, %1\n\t"
                "setc\t%0" 
                : "=q" (res), "=r" (reg_out)
                : "r" ((Short)bitno), "r" (reg_in)
                : "cc", "eax");
   *reg_out_p = reg_out;
   return res;
}







ULong rol1 ( ULong x )
{
  return (x << 1) | (x >> 63);
}

int main ( void )
{
   UInt   n, op;
   ULong  carrydep, c, res;
   char*  block;
   ULong  reg;
   Word   bitoff;

   

   carrydep = 0;
   block = calloc(200,1);
   block += 100;
   

   for (n = 0; n < 10000; n++) {
      bitoff = (random() % 1600) - 800;
      op = random() % 12;
      c = 2;
      switch (op) {
         case  0: c = btsl_mem(block, bitoff); break;
         case  1: c = btrl_mem(block, bitoff); break;
         case  2: c = btcl_mem(block, bitoff); break;
         case  3: c =  btl_mem(block, bitoff); break;
         case  4: c = btsq_mem(block, bitoff); break;
         case  5: c = btrq_mem(block, bitoff); break;
         case  6: c = btcq_mem(block, bitoff); break;
         case  7: c =  btq_mem(block, bitoff); break;
         case  8: c = btsw_mem(block, bitoff); break;
         case  9: c = btrw_mem(block, bitoff); break;
         case 10: c = btcw_mem(block, bitoff); break;
         case 11: c =  btw_mem(block, bitoff); break;
         default: assert(0);
      }
      assert(c == 0 || c == 1);
      carrydep = c ? (rol1(carrydep) ^ bitoff) : carrydep;
   }

   
   block -= 100;   
   res = 0;
   for (n = 0; n < 200; n++) {
      UChar ch = block[n];
      
      res = rol1(res) ^ (UInt)ch;
   }

   printf("MEM-L: final res 0x%llx, carrydep 0x%llx\n", res, carrydep);

   

   carrydep = 0;
   reg = 0;

   for (n = 0; n < 1000; n++) {
      bitoff = (random() % 100) - 50;
      op = random() % 12;
      c = 2;
      switch (op) {
         case  0: c = btsl_reg(reg, bitoff, &reg); break;
         case  1: c = btrl_reg(reg, bitoff, &reg); break;
         case  2: c = btcl_reg(reg, bitoff, &reg); break;
         case  3: c =  btl_reg(reg, bitoff, &reg); break;
         case  4: c = btsq_reg(reg, bitoff, &reg); break;
         case  5: c = btrq_reg(reg, bitoff, &reg); break;
         case  6: c = btcq_reg(reg, bitoff, &reg); break;
         case  7: c =  btq_reg(reg, bitoff, &reg); break;
         case  8: c = btsw_reg(reg, bitoff, &reg); break;
         case  9: c = btrw_reg(reg, bitoff, &reg); break;
         case 10: c = btcw_reg(reg, bitoff, &reg); break;
         case 11: c =  btw_reg(reg, bitoff, &reg); break;
         default: assert(0);
      }
      assert(c == 0 || c == 1);
      carrydep = c ? (rol1(carrydep) ^ bitoff) : carrydep;
   }

   printf("REG-L: final res 0x%llx, carrydep 0x%llx\n", reg, carrydep);

   block += 100;

   
   
   
   

   block -= 100;
   free(block);

   return 0;
}

