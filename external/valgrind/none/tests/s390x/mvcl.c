#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>

typedef struct {
  uint64_t addr1;
  uint32_t len1;
  uint64_t addr2;
  uint32_t len2;
  uint8_t  pad;
  uint32_t cc;
} mvcl_t;

typedef struct {
  uint64_t r1;
  uint64_t r1p1;
  uint64_t r2;
  uint64_t r2p1;
  uint64_t cc;
} mvcl_regs;


static mvcl_regs
do_mvcl(uint64_t r1, uint64_t r1p1, uint64_t r2, uint64_t r2p1)
{
  mvcl_regs regs;

  register uint64_t a1 asm ("2") = r1;
  register uint64_t l1 asm ("3") = r1p1;
  register uint64_t a2 asm ("4") = r2;
  register uint64_t l2 asm ("5") = r2p1;
  register uint32_t cc asm ("7");

  asm volatile( "mvcl %1,%3\n\t"
                "ipm %0\n\t"
                "srl %0,28\n\t"
                :"=d"(cc), "+d"(a1), "+d"(l1), "+d"(a2), "+d"(l2)
                :
                : "memory", "cc");

  regs.r1   = a1;
  regs.r1p1 = l1;
  regs.r2   = a2;
  regs.r2p1 = l2;
  regs.cc   = cc;

  return regs;
}

mvcl_t
result_from_regs(mvcl_regs regs)
{
  mvcl_t result;

  result.addr1 = regs.r1;
  result.len1  = regs.r1p1 & 0xFFFFFF;
  result.addr2 = regs.r2;
  result.len2  = regs.r2p1 & 0xFFFFFF;
  result.pad   = (regs.r2p1 & 0xFF000000u) >> 24;
  result.cc    = regs.cc;

  return result;
}

static mvcl_t
mvcl(void *addr1, uint32_t len1, 
     void *addr2, uint32_t len2, uint32_t pad)
{
  mvcl_t result1, result2;
  mvcl_regs regs;
  uint64_t r1, r1p1, r2, r2p1;

  
  assert((pad & 0xFF) == pad);  
  assert((len1 & 0xFFFFFF) == len1);
  assert((len2 & 0xFFFFFF) == len2);

  
  void *copy = memcpy(malloc(len1), addr1, len1);

  
  r1   = (uint64_t)addr1;
  r1p1 = len1;
  r2   = (uint64_t)addr2;
  r2p1 = len2 | (pad << 24);

  
  regs = do_mvcl(r1, r1p1, r2, r2p1);
  result1 = result_from_regs(regs);

  
  if ((regs.r1p1 >> 24) != 0)
    printf("FAIL: r1[0:39] modified (unused bits 0)\n");
  if ((regs.r2p1 >> 32) != 0)
    printf("FAIL: r2[0:31] modified (unused bits 0)\n");

  
  if (result1.pad != pad)
    printf("FAIL: pad byte modified (unused bits 0)\n");

  
  memcpy(addr1, copy, len1);
  r1p1 |= 0xFFFFFFFFFFULL << 24;
  r2p1 |= ((uint64_t)0xFFFFFFFF) << 32;

  
  regs = do_mvcl(r1, r1p1, r2, r2p1);
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
print_buf(const char *prefix, char *buf, uint32_t len)
{
  uint32_t i;

  if (len > 0) {
    printf("%s |", prefix);
    for (i = 0; i < len; ++i)
      putchar(buf[i]);
    printf("|\n");
  }
}

void 
run_test(void *dst, uint32_t dst_len, void *src, uint32_t src_len, uint32_t pad)
{
  mvcl_t result;

  result = mvcl(dst, dst_len, src, src_len, pad);

  printf("cc: %"PRIu32", len1: %"PRIu32", len2: %"PRIu32
         ", addr1 diff: %"PRId64", addr2 diff: %"PRId64"\n", result.cc,
         result.len1, result.len2, (int64_t)result.addr1 - (int64_t)dst,
         (int64_t)result.addr2 - (int64_t)src);
  print_buf("dst buffer:", dst, dst_len);
}

int main()
{
  uint8_t byte, buf[10], small[5], i;
  uint32_t dst_offset, dst_len, src_offset, src_len;

  
  printf("--- test 1 ---\n");
  run_test(NULL, 0, NULL, 0, 0x00);
  run_test(NULL, 0, NULL, 0, 0xFF);
  run_test(NULL, 0, NULL, 5, 0x00);
  run_test(NULL, 0, NULL, 5, 0xFF);
  run_test(NULL, 0, buf,  sizeof buf, 0x00);
  run_test(NULL, 0, buf,  sizeof buf, 0xFF);

  
  printf("--- test 2 ---\n");
  run_test(&byte, 1, NULL, 0, 'a');
  memset(buf, 'x', sizeof buf);
  run_test(buf, sizeof buf, NULL, 0, 'a');

  

  
  printf("--- test 3 ---\n");
  byte = 'x';
  run_test(&byte, 1, &byte, 1, 'a');
  memset(buf, 'x', sizeof buf);
  for (i = 0; i <= sizeof buf; ++i)
    run_test(buf, i, buf, sizeof buf, 'a');

  
  printf("--- test 4 ---\n");
  memset(buf, 'b', sizeof buf);
  memset(small, 's', sizeof small);
  run_test(buf, sizeof buf, small, sizeof small, 'a');

  
  printf("--- test 5 ---\n");
  memset(buf, 'b', sizeof buf);
  memset(small, 's', sizeof small);
  run_test(small, sizeof small, buf, sizeof buf, 'a');

  
  printf("--- test 6 ---\n");
  memcpy(buf, "0123456789", 10);
  run_test(buf, sizeof buf, buf + 5, 5, 'x');

  
  printf("--- test 7 ---\n");
  memcpy(buf, "0123456789", 10);
  run_test(buf, 5, buf + 4, 3, 'x');

  printf("--- test 8 ---\n");
  memcpy(buf, "0123456789", 10);
  run_test(buf + 3, 1, buf, 10, 'x');   
  memcpy(buf, "0123456789", 10);
  run_test(buf + 3, 2, buf, 10, 'x');   
  memcpy(buf, "0123456789", 10);
  run_test(buf + 3, 3, buf, 10, 'x');   
  memcpy(buf, "0123456789", 10);
  run_test(buf + 3, 4, buf, 10, 'x');   
  memcpy(buf, "0123456789", 10);
  run_test(buf + 3, 5, buf, 10, 'x');   
  memcpy(buf, "0123456789", 10);
  run_test(buf + 3, 6, buf, 10, 'x');   
  memcpy(buf, "0123456789", 10);
  run_test(buf + 3, 7, buf, 10, 'x');   

  printf("--- test 9 ---\n");
  memcpy(buf, "0123456789", 10);
  run_test(buf + 3, 1, buf, 0, 'x');   
  memcpy(buf, "0123456789", 10);
  run_test(buf + 3, 2, buf, 0, 'x');   
  memcpy(buf, "0123456789", 10);
  run_test(buf + 3, 3, buf, 0, 'x');   
  memcpy(buf, "0123456789", 10);
  run_test(buf + 3, 4, buf, 0, 'x');   
  memcpy(buf, "0123456789", 10);
  run_test(buf + 3, 4, buf, 0, 'x');   
  memcpy(buf, "0123456789", 10);
  run_test(buf + 3, 5, buf, 0, 'x');   
  memcpy(buf, "0123456789", 10);
  run_test(buf + 3, 6, buf, 0, 'x');   
  memcpy(buf, "0123456789", 10);
  run_test(buf + 3, 7, buf, 0, 'x');   

  
  printf("--- test 9 ---\n");
  for (dst_offset = 0; dst_offset < sizeof buf; ++dst_offset)
    for (dst_len = 0; dst_len <= sizeof buf - dst_offset; ++dst_len)
      for (src_offset = 0; src_offset < sizeof buf; ++src_offset)
        for (src_len = 0; src_len <= sizeof buf - src_offset; ++src_len)
          run_test(buf + dst_offset, dst_len, buf + src_offset, src_len, 'x');
  
  return 0;
}

