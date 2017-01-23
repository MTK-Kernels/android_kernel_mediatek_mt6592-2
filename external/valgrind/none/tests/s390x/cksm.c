#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include "test.h"

uint32_t data[64];

typedef struct {
  uint64_t addr;
  uint64_t len;
  uint32_t sum;
  char     cc;
} cksm_t;


static __attribute__((noinline)) cksm_t
cksm_by_insn(const uint32_t *buff, uint64_t len, uint32_t sum)
{
  const uint32_t *init_addr = buff;
  uint64_t init_length = len;
  uint64_t addr;
  char cc;
  cksm_t result;
  register uint64_t reg2 asm("2") = (uint64_t) buff;
  register uint64_t reg3 asm("3") = len;

  asm volatile( "       lhi     4,42\n\t"
                "       xr      4,4\n\t"        
                "0:	cksm	%0,%1\n\t"	
		"	jo	0b\n\t"
		: "+d" (sum), "+d" (reg2), "+d" (reg3) : : "cc", "memory");

  cc   = get_cc();
  len  = reg3;
  addr = reg2;

  
  if(addr != (uint64_t)init_addr + init_length)
    printf("FAIL: address not updated properly\n");

  if(len != 0)
    printf("FAIL: length not zero\n");

  if (cc != 0)
    printf("FAIL: condition code not zero\n");

  result.addr = addr;
  result.len  = len;
  result.cc   = cc;
  result.sum  = sum;

  return result;
}


static __attribute__((noinline)) cksm_t
cksm_by_hand(const uint32_t *buff, uint64_t len, uint32_t sum)
{
  cksm_t result;
  unsigned int n;
  uint64_t v64;
  uint32_t final;

  for (n=0; n < len/4; n++) {
    v64 = sum;
    v64 += buff[n];
    
    if (v64 >> 32)
      sum = sum + buff[n] + 1;
    else
      sum = sum + buff[n];
  }

  if (len != 0) {
    switch (len % 4) {
    case 0:
      final = 0;  
      
      break;

    case 1:
      final = buff[n] & 0xFF000000;
      break;

    case 2:
      final = buff[n] & 0xFFFF0000;
      break;

    case 3:
      final = buff[n] & 0xFFFFFF00;
      break;
    }

    if (len % 4) {
      v64 = sum;
      v64 += final;
      
      if (v64 >> 32)
        sum = sum + final + 1;
      else
        sum = sum + final;
    }    
  }    

  result.addr = (uint64_t)buff + len;
  result.len  = 0;
  result.cc   = 0;
  result.sum  = sum;

  return result;
}

int
compare_results(cksm_t by_hand, cksm_t by_insn, uint32_t expected_sum)
{
  int rc = 0;

  if (by_hand.sum != by_insn.sum) {
    ++rc;
    printf("FAIL: sum:   by-hand %"PRIx32"  by-insn %"PRIx32"\n",
           by_hand.sum, by_insn.sum);
  }

  if (by_hand.addr != by_insn.addr) {
    ++rc;
    printf("FAIL: addr:  by-hand %"PRIx64"  by-insn %"PRIx64"\n",
           by_hand.addr, by_insn.addr);
  }

  if (by_hand.len != by_insn.len) {
    ++rc;
    printf("FAIL: len:   by-hand %"PRIx64"  by-insn %"PRIx64"\n",
           by_hand.len, by_insn.len);
  }

  if (by_hand.cc != by_insn.cc) {
    ++rc;
    printf("FAIL: cc:    by-hand %d  by-insn %d\n",
           by_hand.cc, by_insn.cc);
  }

  if (by_insn.sum != expected_sum) {
    ++rc;
    printf("FAIL: sum:   by-insn %"PRIx32"  expected %"PRIx32"\n",
           by_insn.sum, expected_sum);
  }

  if (by_hand.sum != expected_sum) {
    ++rc;
    printf("FAIL: sum:   by-hand %"PRIx32"  expected %"PRIx32"\n",
           by_hand.sum, expected_sum);
  }

  return rc;
}

void
run_test(const char *name, const uint32_t *buff, uint64_t len, uint32_t sum,
         uint32_t expected_sum)
{
  cksm_t by_hand, by_insn;

  by_hand = cksm_by_hand(buff, len, sum);
  by_insn = cksm_by_insn(buff, len, sum);
  if (compare_results(by_hand, by_insn, expected_sum) != 0) {
    printf("%s failed\n", name);
  }
}

int main ()
{
  uint32_t sum, expected_sum;
  uint64_t len;

  
  
  sum = 2;
  data[0] = 1;
  len = 4;
  expected_sum = 3;
  run_test("test1", data, len, sum, expected_sum);

  
  
  sum = 1;
  data[0] = 0xffffffff;
  len = 4;
  expected_sum = 1;
  run_test("test2", data, len, sum, expected_sum);

  
  
  sum      = 0x1;
  data[0]  = 0x4;
  data[1]  = 0x10;
  data[2]  = 0x40;
  data[3]  = 0x100;
  data[4]  = 0x400;
  data[5]  = 0x1000;
  data[6]  = 0x4000;
  data[7]  = 0x10000;
  data[8]  = 0x40000;
  data[9]  = 0x100000;
  data[10] = 0x400000;
  data[11] = 0x1000000;
  data[12] = 0x4000000;
  data[13] = 0x10000000;
  data[14] = 0x40000000;
  len = 60;
  expected_sum = 0x55555555;
  run_test("test3", data, len, sum, expected_sum);

  
  sum      = 0xff000000;
  data[0]  = 0x80000000;   
  data[1]  = 0x85000000;   
  data[2]  = 0xff000000;   
  data[3]  = 0xff000000;   
  data[4]  = 0xff000000;   
  data[5]  = 0xff000000;   
  len = 24;
  expected_sum = 0x00000006;
  run_test("test4", data, len, sum, expected_sum);

  
  len = 0;
  sum = 42;
  expected_sum = sum;
  run_test("test5", NULL, len, sum, expected_sum);

  
  
  sum = 0x02000000;
  len = 1;
  data[0] = 0x7fffffff;
  expected_sum = 0x81000000;
  run_test("test6", data, len, sum, expected_sum);

  
  
  sum = 0x02000000;
  len = 1;
  data[0] = 0xffffffff;
  expected_sum = 0x01000001;
  run_test("test7", data, len, sum, expected_sum);

  
  
  sum = 0x00020000;
  len = 2;
  data[0] = 0x7fffffff;
  expected_sum = 0x80010000;
  run_test("test8", data, len, sum, expected_sum);

  
  
  sum = 0x00020000;
  len = 2;
  data[0] = 0xffffffff;
  expected_sum = 0x00010001;
  run_test("test9", data, len, sum, expected_sum);

  
  
  sum = 0x00000200;
  len = 3;
  data[0] = 0x7fffffff;
  expected_sum = 0x80000100;
  run_test("test10", data, len, sum, expected_sum);

  
  
  sum = 0x00000200;
  len = 3;
  data[0] = 0xffffffff;
  expected_sum = 0x00000101;
  run_test("test11", data, len, sum, expected_sum);

  return 0;
}
