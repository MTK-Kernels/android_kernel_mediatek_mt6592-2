
#include "../../memcheck.h"

#include <stdio.h>

typedef unsigned long ULong;

typedef struct {
   ULong w64[2];  
} V128;

static int getMSBs16x8(V128 v)
{
   int result;
   __asm__("movups %1,%%xmm6\n"
           "\tpmovmskb %%xmm6,%0\n"
           : "=r" (result) : "m" (v) : "xmm6");
   return result;
}

static void set_vbits(V128 *addr, V128 vbits)
{
   int i;
   for (i=0 ; i<2 ; ++i) {
      (void)VALGRIND_SET_VBITS(&addr->w64[i], &vbits.w64[i], sizeof(vbits.w64[i]));
   }
}

static void print(V128 vbits, V128 val, int bit, int result)
{
   printf("vbits=0x%016lx%016lx val=0x%016lx%016lx bit=%d result=%d\n",
          vbits.w64[1], vbits.w64[0], val.w64[1], val.w64[0],
          bit, result);
}

static void use(int index, int invalid)
{
   __asm__ ("" : "=r" (invalid) : "0" (invalid));

   fprintf(stderr, "%d: Invalid value is %s\n",
           index, invalid ? "true" : "false");
}

static void doit(ULong vbits_hi, ULong vbits_lo, ULong val_hi, ULong val_lo)
{
   V128 vbits = { { vbits_lo, vbits_hi } };
   V128 val = { { val_lo, val_hi } };

   V128 val_copy = val;

   set_vbits(&val, vbits);

   int result = getMSBs16x8(val);

   int vbits_mask = getMSBs16x8(vbits);

   int bit = 0; ULong mask = (1UL << bit);
   if ((vbits_mask & mask) == 0) print(vbits, val_copy, bit, result & mask);
   else use(bit, result & mask);

   bit = 1; mask = (1UL << bit);
   if ((vbits_mask & mask) == 0) print(vbits, val_copy, bit, result & mask);
   else use(bit, result & mask);

   bit = 2; mask = (1UL << bit);
   if ((vbits_mask & mask) == 0) print(vbits, val_copy, bit, result & mask);
   else use(bit, result & mask);

   bit = 3; mask = (1UL << bit);
   if ((vbits_mask & mask) == 0) print(vbits, val_copy, bit, result & mask);
   else use(bit, result & mask);

   bit = 4; mask = (1UL << bit);
   if ((vbits_mask & mask) == 0) print(vbits, val_copy, bit, result & mask);
   else use(bit, result & mask);

   bit = 5; mask = (1UL << bit);
   if ((vbits_mask & mask) == 0) print(vbits, val_copy, bit, result & mask);
   else use(bit, result & mask);

   bit = 6 ; mask = (1UL << bit);
   if ((vbits_mask & mask) == 0) print(vbits, val_copy, bit, result & mask);
   else use(bit, result & mask);

   bit = 7 ; mask = (1UL << bit);
   if ((vbits_mask & mask) == 0) print(vbits, val_copy, bit, result & mask);
   else use(bit, result & mask);

   bit = 8 ; mask = (1UL << bit);
   if ((vbits_mask & mask) == 0) print(vbits, val_copy, bit, result & mask);
   else use(bit, result & mask);

   bit = 9 ; mask = (1UL << bit);
   if ((vbits_mask & mask) == 0) print(vbits, val_copy, bit, result & mask);
   else use(bit, result & mask);

   bit = 10 ; mask = (1UL << bit);
   if ((vbits_mask & mask) == 0) print(vbits, val_copy, bit, result & mask);
   else use(bit, result & mask);

   bit = 11 ; mask = (1UL << bit);
   if ((vbits_mask & mask) == 0) print(vbits, val_copy, bit, result & mask);
   else use(bit, result & mask);

   bit = 12 ; mask = (1UL << bit);
   if ((vbits_mask & mask) == 0) print(vbits, val_copy, bit, result & mask);
   else use(bit, result & mask);

   bit = 13 ; mask = (1UL << bit);
   if ((vbits_mask & mask) == 0) print(vbits, val_copy, bit, result & mask);
   else use(bit, result & mask);

   bit = 14 ; mask = (1UL << bit);
   if ((vbits_mask & mask) == 0) print(vbits, val_copy, bit, result & mask);
   else use(bit, result & mask);

   bit = 15 ; mask = (1UL << bit);
   if ((vbits_mask & mask) == 0) print(vbits, val_copy, bit, result & mask);
   else use(bit, result & mask);
}

int main(int argc, char *argv[])
{
   doit(0x0000000000000000, 0x0000000000000000,
        0x0000000000000000, 0x0000000000000000);

   doit(0x0707070707070707, 0x0707070707070707,
        0x0000000000000000, 0x0000000000000000);

   doit(0x8080808080808080, 0x8080808080808080,
        0x0000000000000000, 0x0000000000000000);

   doit(0x13579BDF02468ACE, 0xFEDCBA9876543210,
        0xFEEDFACEDEADBEEF, 0xFEE1DEADDABBAD00);

   return 0;
}
