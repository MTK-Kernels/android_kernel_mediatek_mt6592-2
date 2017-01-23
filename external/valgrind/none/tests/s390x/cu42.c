#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include "opcodes.h"

typedef struct {
   uint64_t addr1;  
   uint64_t len1;
   uint64_t addr2;  
   uint64_t len2;
   uint32_t cc;
} cu42_t;


uint32_t pattern2[] = {
   0x0000, 0xd7ff,    
   0xdc00, 0xffff,    
   0xabba, 0xf00d, 0xd00f, 0x1234 
};

uint32_t pattern4[] = {
   0x00010000, 0x0010ffff,    
   0x00010123, 0x00023456, 0x000789ab, 0x00100000  
};

uint32_t invalid[] = {
   0x0000d800, 0x0000dbff,   
   0x00110000, 0xffffffff,   
   0x0000daad, 0x0000d901, 0x0000d8ff, 
   0x00110011, 0x01000000, 0x10000000, 0xdeadbeef  
};

uint32_t mixed[] = {
   0x00000078 ,
   0x0000d000 ,
   0x00033333 ,
   0x00040404 ,
   0x0000abcd ,
};

uint16_t buff[1000];  

void write_and_check(uint32_t *, unsigned, unsigned);


static cu42_t
do_cu42(uint16_t *dst, uint64_t dst_len, uint32_t *src, uint64_t src_len)
{
   int cc = 42;
   cu42_t regs;

   
   register uint32_t *source     asm("4") = src;
   register uint64_t  source_len asm("5") = src_len;
   register uint16_t *dest       asm("2") = dst;
   register uint64_t  dest_len   asm("3") = dst_len;

   asm volatile(
                CU42(2,4)
                "ipm %2\n\t"
                "srl %2,28\n\t"
                : "+d"(dest), "+d"(source), "=d"(cc),
                  "+d"(source_len), "+d"(dest_len)
                :
                : "memory", "cc");

   
   regs.addr1 = (uint64_t)dest;
   regs.len1  = dest_len;
   regs.addr2 = (uint64_t)source;
   regs.len2  = source_len;
   regs.cc = cc;
   
   return regs;
}

void
run_test(uint16_t *dst, uint64_t dst_len, uint32_t *src, uint64_t src_len)
{
   int i;
   cu42_t result;

   result = do_cu42(dst, dst_len, src, src_len);

   
   printf("UTF16: ");
   if (dst_len - result.len1 == 0)
      printf(" <none>");
   else
      assert((dst_len - result.len1) % 2 == 0);
      for (i = 0; i < (dst_len - result.len1) / 2; ++i) {
         printf(" %04x", dst[i]);
      }
   printf("\n");

   printf("  cc = %d\n", result.cc);
   if (dst != NULL)
      printf("  dst address difference: %"PRId64, result.addr1 - (uint64_t)dst);
   printf("  dst len: %"PRId64"\n", result.len1);

   if (src != NULL)
      printf("  src address difference: %"PRId64, result.addr2 - (uint64_t)src);
   printf("  src len: %"PRId64"\n", result.len2);
}

int main()
{
   int i;

   /* Length == 0, no memory should be read or written */
   printf("\n------------- test1 ----------------\n");
   run_test(NULL, 0, NULL, 0);

   
   printf("\n------------- test2.1 ----------------\n");

   /* No character will be written to BUFF, i.e. loop in jitted code
      is not iterated */
   run_test(buff, sizeof buff, NULL,     0);
   run_test(buff, sizeof buff, NULL,     1);
   run_test(buff, sizeof buff, NULL,     2);
   run_test(buff, sizeof buff, NULL,     3);
   run_test(buff, sizeof buff, pattern2, 0);
   run_test(buff, sizeof buff, pattern2, 1);
   run_test(buff, sizeof buff, pattern2, 2);
   run_test(buff, sizeof buff, pattern2, 3);

   printf("\n------------- test2.2 ----------------\n");
   /* At least one character will be written to BUFF, i.e. loop in jitted
      code is iterated */
   run_test(buff, sizeof buff, pattern2, 4);  
   run_test(buff, sizeof buff, pattern2, 10); 
   run_test(buff, sizeof buff, pattern4, 5);  
   run_test(buff, sizeof buff, pattern4, 11); 
   run_test(buff, sizeof buff, pattern4, 18); 

   
   printf("\n------------- test3.1 ----------------\n");

   /* No character will be written to BUFF, i.e. loop in jitted code
      is not iterated */

   
   run_test(NULL, 0, pattern2, sizeof pattern2);

   
   run_test(NULL, 0, pattern2, sizeof pattern2);
   run_test(NULL, 1, pattern2, sizeof pattern2);

   
   run_test(NULL, 0, pattern4, sizeof pattern4);
   run_test(NULL, 1, pattern4, sizeof pattern4);
   run_test(NULL, 2, pattern4, sizeof pattern4);
   run_test(NULL, 3, pattern4, sizeof pattern4);

   printf("\n------------- test4 ----------------\n");
   run_test(buff, 4, pattern2, 8);

   

   
   
   

   printf("\n------------- test5 ----------------\n");
   for (i = 0; i < sizeof invalid / 4; ++i) {
      run_test(buff, sizeof buff, invalid + i, 4);
   }
   run_test(buff, 0, invalid, sizeof invalid);  
   run_test(buff, 100, invalid, sizeof invalid);

   
   printf("\n------------- test6 ----------------\n");
   run_test(buff, sizeof buff, pattern2, sizeof pattern2);
   run_test(buff, sizeof buff, pattern4, sizeof pattern4);
   run_test(buff, sizeof buff, mixed,    sizeof mixed);

   

   
   printf("\n------------- test7.1 ----------------\n");
   write_and_check(pattern2 + 3, 4, 2);

   
   printf("\n------------- test7.2 ----------------\n");
   write_and_check(pattern4 + 5, 4, 4);

   return 0;
}


void
write_and_check_aux(uint32_t *input, unsigned num_input_bytes,
                    unsigned num_expected_output_bytes,
                    unsigned fill_byte)
{
   int num_errors, i;

   
   memset(buff, fill_byte, sizeof buff);

   
   run_test(buff, sizeof buff, input, num_input_bytes);

   
   num_errors = 0;
   for (i = num_expected_output_bytes; i < sizeof buff; ++i)
      if (((unsigned char *)buff)[i] != fill_byte) ++num_errors;
   if (num_errors)
      fprintf(stderr, "*** wrote more than %d bytes\n",
              num_expected_output_bytes);
}

void
write_and_check(uint32_t *input, unsigned num_input_bytes,
                unsigned num_expected_output_bytes)
{
   write_and_check_aux(input, num_input_bytes, num_expected_output_bytes, 0x0);

   write_and_check_aux(input, num_input_bytes, num_expected_output_bytes, 0xFF);
}
