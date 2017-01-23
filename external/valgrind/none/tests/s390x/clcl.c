#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <assert.h>


typedef struct {
  uint64_t addr1;
  uint32_t len1;
  uint64_t addr2;
  uint32_t len2;
  uint8_t  pad;
  uint32_t cc;
} clcl_t;

typedef struct {
  uint64_t r1;
  uint64_t r1p1;
  uint64_t r2;
  uint64_t r2p1;
  uint64_t cc;
} clcl_regs;

static clcl_regs
do_clcl(uint64_t r1, uint64_t r1p1, uint64_t r2, uint64_t r2p1)
{
  clcl_regs regs;

  register uint64_t a1 asm ("2") = r1;
  register uint64_t l1 asm ("3") = r1p1;
  register uint64_t a2 asm ("4") = r2;
  register uint64_t l2 asm ("5") = r2p1;
  register uint32_t cc asm ("7");

  asm volatile(	"0: clcl 2,4\n\t"
                "jo 0b\n\t"
                "ipm %0\n\t"
                "srl %0,28\n\t"
                :"=d" (cc), "+d" (a1),"+d" (l1), "+d" (a2), "+d" (l2)
                : : "memory", "cc");

  regs.r1   = a1;
  regs.r1p1 = l1;
  regs.r2   = a2;
  regs.r2p1 = l2;
  regs.cc   = cc;

  return regs;
}

clcl_t
result_from_regs(clcl_regs regs)
{
  clcl_t result;

  result.addr1 = regs.r1;
  result.len1  = regs.r1p1 & 0xFFFFFF;
  result.addr2 = regs.r2;
  result.len2  = regs.r2p1 & 0xFFFFFF;
  result.pad   = (regs.r2p1 & 0xFF000000u) >> 24;
  result.cc    = regs.cc;

  return result;
}

static clcl_t
clcl(void *addr1, uint32_t len1, 
     void *addr2, uint32_t len2, uint32_t pad)
{
  clcl_t result1, result2;
  clcl_regs regs;
  uint64_t r1, r1p1, r2, r2p1;

  
  assert((pad & 0xFF) == pad);  
  assert((len1 & 0xFFFFFF) == len1);
  assert((len2 & 0xFFFFFF) == len2);

  
  r1   = (uint64_t)addr1;
  r1p1 = len1;
  r2   = (uint64_t)addr2;
  r2p1 = len2 | (pad << 24);

  
  regs = do_clcl(r1, r1p1, r2, r2p1);
  result1 = result_from_regs(regs);

  
  if ((regs.r1p1 >> 24) != 0)
    printf("FAIL: r1[0:39] modified (unused bits 0)\n");
  if ((regs.r2p1 >> 32) != 0)
    printf("FAIL: r2[0:31] modified (unused bits 0)\n");

  
  if (result1.pad != pad)
    printf("FAIL: pad byte modified (unused bits 0)\n");

  
  r1p1 |= 0xFFFFFFFFFFull << 24;
  r2p1 |= ((uint64_t)0xFFFFFFFF) << 32;

  
  regs = do_clcl(r1, r1p1, r2, r2p1);
  result2 = result_from_regs(regs);

  
  if ((regs.r1p1 >> 24) != 0xFFFFFFFFFFull)
    printf("FAIL: r1[0:39] modified (unused bits 1)\n");
  if ((regs.r2p1 >> 32) != 0xFFFFFFFFu)
    printf("FAIL: r2[0:31] modified (unused bits 1)\n");

  
  if (result2.pad != pad)
    printf("FAIL: pad byte modified (unused bits 1)\n");

  
  if (result1.addr1 != result2.addr1)
    printf("FAIL: addr1 result is different\n");
  if (result1.addr2 != result2.addr2)
    printf("FAIL: addr2 result is different\n");
  if (result1.len1 != result2.len1)
    printf("FAIL: len1 result is different\n");
  if (result1.len2 != result2.len2)
    printf("FAIL: len2 result is different\n");
  if (result1.pad != result2.pad)
    printf("FAIL: pad result is different\n");
  if (result1.cc != result2.cc)
    printf("FAIL: cc result is different\n");

  return result1;
}

void 
run_test(void *addr1, uint32_t len1, void *addr2, uint32_t len2, uint32_t pad)
{
  clcl_t result;

  result = clcl(addr1, len1, addr2, len2, pad);

  printf("cc: %"PRIu32", len1: %"PRIu32", len2: %"PRIu32
         ", addr1 diff: %"PRId64", addr2 diff: %"PRId64"\n", result.cc,
         result.len1, result.len2, (int64_t)result.addr1 - (int64_t)addr1,
         (int64_t)result.addr2 - (int64_t)addr2);
}

int main()
{
  uint8_t byte, byte1, byte2;

  
  printf("--- test 1 ---\n");
  run_test(NULL, 0, NULL, 0, 0x00);
  run_test(NULL, 0, NULL, 0, 0xff);

  
  printf("--- test 2 ---\n");
  byte1 = 10;
  byte2 = 20;
  run_test(&byte1, 1, &byte2, 1, 0x00);  
  run_test(&byte1, 1, &byte1, 1, 0x00);  
  run_test(&byte2, 1, &byte1, 1, 0x00);  
  run_test(&byte1, 1, &byte2, 1, 0xFF);  
  run_test(&byte1, 1, &byte1, 1, 0xFF);  
  run_test(&byte2, 1, &byte1, 1, 0xFF);  

  
  printf("--- test 3 ---\n");
  byte = 10;
  run_test(NULL, 0, &byte, 1, 10);  
  run_test(NULL, 0, &byte, 1,  9);  
  run_test(NULL, 0, &byte, 1, 11);  
  
  run_test(&byte, 1, NULL, 0, 10);  
  run_test(&byte, 1, NULL, 0,  9);  
  run_test(&byte, 1, NULL, 0, 11);  

  
  printf("--- test 4 ---\n");
  byte = 10;
  run_test(&byte, 1, NULL, 0, 0xFF);  
  byte = 0xFF;
  run_test(&byte, 1, NULL, 0, 0xFF);  

  
  printf("--- test 5 ---\n");
  uint8_t buf1[4] = "yyyy";
  run_test(buf1, 4, NULL, 0, 'y');    
  run_test(buf1, 4, NULL, 0, 'x');    
  run_test(buf1, 4, NULL, 0, 'z');    

  
  {
  printf("--- test 6 ---\n");
  uint8_t x[5] = "pqrst";
  uint8_t y[5] = "abcde";
  uint8_t z[5] = "abcde";
  run_test(x, 5, y, 5, 'a');   
  run_test(y, 5, x, 5, 'a');   
  run_test(y, 5, z, 5, 'q');   
  }

  
  {
  printf("--- test 7 ---\n");
  uint8_t x[5] = "abcdd";
  uint8_t y[5] = "abcde";
  uint8_t z[5] = "abcdf";
  run_test(x, 5, y, 5, 'a');   
  run_test(z, 5, z, 5, 'a');   
  }

  {
  printf("--- test 8 ---\n");
  uint8_t x[5] = "abcde";
  uint8_t y[7] = "abcdeff";
  run_test(x, 5, y, 7, 0);   
  run_test(y, 7, x, 5, 0);   
  run_test(x, 5, y, 7, 'f'); 
  run_test(y, 7, x, 5, 'f'); 
  }

  {
  printf("--- test 9 ---\n");
  uint8_t x[5] = "abcab";
  uint8_t y[7] = "abcdeff";
  run_test(x, 5, y, 7, 0);   
  run_test(y, 7, x, 5, 0);   
  run_test(x, 5, y, 7, 'f'); 
  run_test(y, 7, x, 5, 'f'); 
  }

  return 0;
}
