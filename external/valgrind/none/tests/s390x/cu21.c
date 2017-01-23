#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "opcodes.h"

#ifndef M3
#define M3 0
#endif

typedef struct {
   uint64_t addr1;  
   uint64_t len1;
   uint64_t addr2;  
   uint64_t len2;
   uint32_t cc;
} cu21_t;


uint16_t pattern1[] = {
   0x0000, 0x007f,    
   0x0047, 0x0056, 0x0045, 0x0021, 0x007b, 0x003a 
};

uint16_t pattern2[] = {
   0x0080, 0x07ff,    
   0x07df, 0x008f, 0x0100, 0x017f, 0x052f, 0x0600, 0x06ff 
};

uint16_t pattern3[] = {
   0x0800, 0xd7ff,    
   0xdc00, 0xffff,    
   0x083f, 0x1a21, 0x1b10, 0x2200, 0x225e, 0x22c9, 0xe001  
};

uint16_t pattern4[] = {
   0xd800, 0xdc00,    
   0xdbff, 0xdfff,    
   0xdada, 0xdddd, 0xdeaf, 0xdcdc  
};

uint16_t invalid[] = { 0xd801, 0x0098 };

uint16_t mixed[] = {
   0x0078 ,
   0x0200 ,
   0xffff ,
   0xd800, 0xdc01 
};

uint8_t buff[1000];  

void write_and_check(uint16_t *, unsigned, unsigned);


static cu21_t
do_cu21(uint8_t *dst, uint64_t dst_len, uint16_t *src, uint64_t src_len)
{
   int cc = 42;
   cu21_t regs;

   
   register uint16_t *source     asm("4") = src;
   register uint64_t  source_len asm("5") = src_len;
   register uint8_t  *dest       asm("2") = dst;
   register uint64_t  dest_len   asm("3") = dst_len;

   asm volatile(
                CU21(M3,2,4)
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
run_test(uint8_t *dst, uint64_t dst_len, uint16_t *src, uint64_t src_len)
{
   int i;
   cu21_t result;

   result = do_cu21(dst, dst_len, src, src_len);

   
   printf("UTF8: ");
   if (dst_len - result.len1 == 0)
      printf(" <none>");
   else
      for (i = 0; i < dst_len - result.len1; i++) {
         printf(" %02x", dst[i]);
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
   /* Length == 0, no memory should be read or written */
   printf("\n------------- test1 ----------------\n");
   run_test(NULL, 0, NULL, 0);

   
   printf("\n------------- test2.1 ----------------\n");

   /* No character will be written to BUFF, i.e. loop in jitted code
      is not iterated */
   run_test(buff, sizeof buff, NULL,     1);
   run_test(buff, sizeof buff, pattern1, 1);
   run_test(buff, sizeof buff, pattern2, 1);
   run_test(buff, sizeof buff, pattern3, 1);
   run_test(buff, sizeof buff, pattern4, 1);
   run_test(buff, sizeof buff, pattern4, 2);
   run_test(buff, sizeof buff, pattern4, 3);

   printf("\n------------- test2.2 ----------------\n");
   /* At least one character will be written to BUFF, i.e. loop in jitted
      code is iterated */
   run_test(buff, sizeof buff, pattern1, 3);
   run_test(buff, sizeof buff, pattern2, 5);
   run_test(buff, sizeof buff, pattern3, 7);
   run_test(buff, sizeof buff, pattern4, 9);
   run_test(buff, sizeof buff, pattern4, 10);
   run_test(buff, sizeof buff, pattern4, 11);

   
   printf("\n------------- test3.1 ----------------\n");

   /* No character will be written to BUFF, i.e. loop in jitted code
      is not iterated */

   
   run_test(NULL, 0, pattern1, sizeof pattern1);

   
   run_test(NULL, 0, pattern2, sizeof pattern2);
   run_test(NULL, 1, pattern2, sizeof pattern2);

   
   run_test(NULL, 0, pattern3, sizeof pattern3);
   run_test(NULL, 1, pattern3, sizeof pattern3);
   run_test(NULL, 2, pattern3, sizeof pattern3);

   
   run_test(NULL, 0, pattern4, sizeof pattern4);
   run_test(NULL, 1, pattern4, sizeof pattern4);
   run_test(NULL, 2, pattern4, sizeof pattern4);
   run_test(NULL, 3, pattern4, sizeof pattern4);

   printf("\n------------- test3.2 ----------------\n");
   /* At least one character will be written to BUFF, i.e. loop in jitted
      code is iterated */
   run_test(buff, 3, pattern1, sizeof pattern1);

   run_test(buff, 5, pattern2, sizeof pattern2);

   run_test(buff, 7, pattern3, sizeof pattern3);
   run_test(buff, 8, pattern3, sizeof pattern3);

   run_test(buff,  9, pattern4, sizeof pattern4);
   run_test(buff, 10, pattern4, sizeof pattern4);
   run_test(buff, 11, pattern4, sizeof pattern4);

   printf("\n------------- test4 ----------------\n");
   run_test(buff, 6, pattern1, 6);

   
   printf("\n------------- test5 ----------------\n");
   run_test(buff, sizeof buff, invalid, sizeof invalid);
   run_test(buff, 0, invalid, sizeof invalid);

   
   printf("\n------------- test6 ----------------\n");
   run_test(buff, sizeof buff, pattern1, sizeof pattern1);
   run_test(buff, sizeof buff, pattern2, sizeof pattern2);
   run_test(buff, sizeof buff, pattern3, sizeof pattern3);
   run_test(buff, sizeof buff, pattern4, sizeof pattern4);
   run_test(buff, sizeof buff, mixed,    sizeof mixed);

   
   uint16_t pat[2];

   
   printf("\n------------- test7.1 ----------------\n");
   pat[0] = 0x10;
   write_and_check(pat, 2, 1);

   
   printf("\n------------- test7.2 ----------------\n");
   pat[0] = 0x8f;
   write_and_check(pat, 2, 2);

   
   printf("\n------------- test7.3 ----------------\n");
   pat[0] = 0x842;
   write_and_check(pat, 2, 3);

   
   printf("\n------------- test7.4 ----------------\n");
   pat[0] = 0xd842;
   pat[1] = 0xdc42;
   write_and_check(pat, 2, 4);

   return 0;
}


void
write_and_check_aux(uint16_t *input, unsigned num_input_bytes,
                    unsigned num_expected_output_bytes,
                    unsigned fill_byte)
{
   int num_errors, i;

   
   memset(buff, fill_byte, sizeof buff);

   
   run_test(buff, sizeof buff, input, num_input_bytes);

   
   num_errors = 0;
   for (i = num_expected_output_bytes; i < sizeof buff; ++i)
      if (buff[i] != fill_byte) ++num_errors;
   if (num_errors)
      fprintf(stderr, "*** wrote more than %u bytes\n",
              num_expected_output_bytes);
}

void
write_and_check(uint16_t *input, unsigned num_input_bytes,
                unsigned num_expected_output_bytes)
{
   write_and_check_aux(input, num_input_bytes, num_expected_output_bytes, 0x0);

   write_and_check_aux(input, num_input_bytes, num_expected_output_bytes, 0xFF);
}
