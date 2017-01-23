
#include "../../memcheck.h"

#include <stdio.h>
#include <assert.h>

typedef unsigned int UInt;

static int ctz(UInt x)
{
   assert(sizeof(UInt) == 4);
   int result=8*sizeof(UInt);
   
   asm("bsfl %1,%0" : "=r" (result) : "r" (x), "0" (result) : "cc");
   return result;
}

static void set_vbits(UInt *addr, UInt vbits)
{
   (void)VALGRIND_SET_VBITS(addr, &vbits, sizeof(unsigned));
}

static void doit(unsigned vbits, unsigned val)
{
   unsigned val_copy = val;

   
   set_vbits(&val, vbits);

   __asm__ ("" : "=r" (val) : "0" (val));

   int result = ctz(val);

   if (result < ctz(vbits) || vbits == 0) {
      printf("vbits=0x%08x ctz(0x%08x)=%d\n", vbits, val_copy, result);
   }
   else {
       fprintf(stderr, "0x%08x: Invalid value is %d\n", val_copy,
               ctz(val_copy));
   }
}

int main(int argc, char *argv[])
{
   doit(0x00000000, 0x00000000);
   doit(0x00000000, 0x00000001);
   doit(0x00000001, 0x00000000);
   doit(0x00000001, 0x00000001);

   
   doit(0x00000090, 0x00000040);
   doit(0x00000040, 0x00000090);

   
   doit(0x00000500, 0x00000a00);
   doit(0x00000a00, 0x00000500);

   doit(0x000f0000, 0x001e0000);
   doit(0x001e0000, 0x000f0000);

   doit(0xffffffff, 0xffffffff);
   doit(0xfffffffe, 0xffffffff);
   doit(0xffffffff, 0xfffffffe);
   doit(0xfffffffe, 0xfffffffe);

   return 0;
}
