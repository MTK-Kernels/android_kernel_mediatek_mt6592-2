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
} cu14_t;


uint8_t pattern1[] = {
   0x00, 0x01, 0x02, 0x03
};

uint8_t pattern2[] = {
   0xc2, 0x80,
   0xc2, 0x81,
   0xc2, 0x82,
   0xc2, 0x83,
};

uint8_t pattern3[] = {
   0xe1, 0x80, 0x80,
   0xe1, 0x80, 0x81,
   0xe1, 0x80, 0x82,
   0xe1, 0x80, 0x83,
};

uint8_t pattern4[] = {
   0xf4, 0x80, 0x80, 0x80,
   0xf4, 0x80, 0x80, 0x81,
   0xf4, 0x80, 0x80, 0x82,
   0xf4, 0x80, 0x80, 0x83,
};


uint8_t mixed[] = {
   0x01,                    
   0xc3, 0x80,              
   0x12,                    
   0xe1, 0x90, 0x93,        
   0x23,                    
   0xf4, 0x80, 0x90, 0x8a,  
   0x34,                    
   0xc4, 0x8c,              
   0xe1, 0x91, 0x94,        
   0xc5, 0x8a,              
   0xf4, 0x80, 0x90, 0x8a,  
   0xc5, 0x8a,              
   0xe1, 0x91, 0x94,        
   0xf4, 0x80, 0x90, 0x8a,  
   0xe1, 0x91, 0x94,        
};

uint32_t buff[500];  


static cu14_t
do_cu14(uint32_t *dst, uint64_t dst_len, uint8_t *src, uint64_t src_len)
{
   int cc = 42;
   cu14_t regs;

   
   register uint8_t  *source     asm("4") = src;
   register uint64_t  source_len asm("5") = src_len;
   register uint32_t *dest       asm("2") = dst;
   register uint64_t  dest_len   asm("3") = dst_len;

   asm volatile(
                CU14(M3,2,4)
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
run_test(uint32_t *dst, uint64_t dst_len, uint8_t *src, uint64_t src_len)
{
   int i;
   cu14_t result;

   printf("UTF8:  ");
   if (src_len == 0) 
      printf(" <none>");
   else {
      for(i = 0; i < src_len; ++i)
         printf(" %02x", src[i]);
   }
   printf("\n");
      
   result = do_cu14(dst, dst_len, src, src_len);

   
   printf("UTF32: ");
   if (dst_len - result.len1 == 0)
      printf(" <none>");
   else {
      uint64_t num_bytes = dst_len - result.len1;

      /* The number of bytes that were written must be divisible by 4 */
      if (num_bytes % 4 != 0)
         fprintf(stderr, "*** number of bytes is not a multiple of 4\n");

      for (i = 0; i < num_bytes / 4; i++) {
         printf(" %08x", dst[i]);
      }
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

void convert_1_byte(void)
{
   int i;

   printf("===== Conversion of a one-byte character =====\n");

   printf("\n----- Valid characters -----\n");
   uint8_t valid[] = { 
      0x00, 0x7f,              
      0x01, 0x10, 0x7e, 0x5d   
   };
   run_test(buff, sizeof buff, valid, sizeof valid);

   
   
   
   
   printf("\n----- Invalid characters -----\n");
   uint8_t always_invalid[] = {
      0x80, 0xbf,              
      0xf8, 0xff,              
      0x81, 0xbe, 0x95, 0xab   
   };
   for (i = 0; i < sizeof always_invalid; ++i) {
      uint8_t invalid_char[1];
      invalid_char[0] = always_invalid[i];
      run_test(buff, sizeof buff, invalid_char, sizeof invalid_char);
   }

   
   printf("\n----- Invalid characters if m3 == 1 -----\n");
   uint8_t invalid_if_m3[] = {  
      0xc0, 0xc1,
      0xf5, 0xf6, 0xf7
   };
   for (i = 0; i < sizeof invalid_if_m3; ++i) {
      uint8_t invalid_char[1];
      invalid_char[0] = invalid_if_m3[i];
      run_test(buff, sizeof buff, invalid_char, sizeof invalid_char);
   }

   printf("\n----- 1st char valid, 2nd char invalid -----\n");
   uint8_t valid_invalid[] = {
      0x10, 
      0xaa  
   };
   run_test(buff, sizeof buff, valid_invalid, sizeof valid_invalid);
}

void convert_2_bytes(void)
{
   int i;

   printf("\n===== Conversion of a two-byte character =====\n");

   printf("\n----- Valid characters -----\n");
   uint8_t valid[] = { 
      0xc2, 0x80,             
      0xc2, 0xbf,             
      0xdf, 0x80,             
      0xdf, 0xbf,             
      0xc3, 0xbe, 0xda, 0xbc  
   };
   run_test(buff, sizeof buff, valid, sizeof valid);

   printf("\n----- Valid characters if m3 == 0 -----\n");
   
   uint8_t valid_if_not_m3[] = {
      0xc0, 0x80,
      0xc0, 0xbf,
      0xc1, 0x80,
      0xc0, 0xbf
   };
   run_test(buff, sizeof buff, valid_if_not_m3, sizeof valid_if_not_m3);

   
   

   
   
   
   
   printf("\n----- Invalid characters if m3 == 1 -----\n");
   uint8_t always_invalid[] = {
      0xc2, 0x00,
      0xc2, 0x7f,
      0xc2, 0xc0,
      0xc2, 0xff
   };
   for (i = 0; i < sizeof always_invalid; i += 2) {
      uint8_t invalid_char[2];
      invalid_char[0] = always_invalid[i];
      invalid_char[1] = always_invalid[i+1];
      run_test(buff, sizeof buff, invalid_char, sizeof invalid_char);
   }


   printf("\n----- 1st char valid, 2nd char invalid -----\n");
   uint8_t valid_invalid[] = {
      0xc3, 0x81, 
      0xc4, 0x00  
   };
   run_test(buff, sizeof buff, valid_invalid, sizeof valid_invalid);
}

void
convert_3_bytes(void)
{
   int i;

   printf("\n===== Conversion of a three-byte character =====\n");

   printf("\n----- Valid characters -----\n");
   uint8_t e0[] = { 
      0xe0, 0xa0, 0x80,
      0xe0, 0xbf, 0x80,
      0xe0, 0xa0, 0xbf,
      0xe0, 0xbf, 0xbf,
      0xe0, 0xaa, 0xbb,   
   };
   run_test(buff, sizeof buff, e0, sizeof e0);

   uint8_t ed[] = { 
      0xed, 0x80, 0x80,
      0xed, 0x9f, 0x80,
      0xed, 0x80, 0xbf,
      0xed, 0x9f, 0xbf,
      0xed, 0x8a, 0xbb,   
   };
   run_test(buff, sizeof buff, ed, sizeof ed);

   for (i = 0; i <= 0xf; ++i) {
      uint8_t exxx_1[3] = { 0x0, 0x80, 0x80 };
      uint8_t exxx_2[3] = { 0x0, 0xbf, 0x80 };
      uint8_t exxx_3[3] = { 0x0, 0x80, 0xbf };
      uint8_t exxx_4[3] = { 0x0, 0xbf, 0xbf };

      if (i == 0x00) continue;   
      if (i == 0x0d) continue;   

      exxx_1[0] = 0xe0 | i;
      exxx_2[0] = 0xe0 | i;
      exxx_3[0] = 0xe0 | i;
      exxx_4[0] = 0xe0 | i;
      run_test(buff, sizeof buff, exxx_1, sizeof exxx_1);
      run_test(buff, sizeof buff, exxx_2, sizeof exxx_2);
      run_test(buff, sizeof buff, exxx_3, sizeof exxx_3);
      run_test(buff, sizeof buff, exxx_4, sizeof exxx_4);
   };

   printf("\n----- Invalid characters (2nd byte is invalid) -----\n");
   
   

   
   
   

   e0[0] = 0xe0;  
   e0[1] = 0x9f;  
   e0[2] = 0x80;  
   run_test(buff, sizeof buff, e0, sizeof e0);
   e0[1] = 0xc0;  
   run_test(buff, sizeof buff, e0, sizeof e0);

   ed[0] = 0xed;  
   ed[1] = 0x7f;  
   ed[2] = 0x80;  
   run_test(buff, sizeof buff, ed, sizeof ed);
   ed[1] = 0xa0;  
   run_test(buff, sizeof buff, ed, sizeof ed);

   for (i = 0; i <= 0xf; ++i) {
      uint8_t exxx_1[3] = { 0x0, 0x7f, 0x80 };
      uint8_t exxx_2[3] = { 0x0, 0xc0, 0x80 };

      if (i == 0x00) continue;   
      if (i == 0x0d) continue;   

      exxx_1[0] = 0xe0 | i;
      exxx_2[0] = 0xe0 | i;
      run_test(buff, sizeof buff, exxx_1, sizeof exxx_1);
      run_test(buff, sizeof buff, exxx_2, sizeof exxx_2);
   };

   printf("\n----- Invalid characters (3rd byte is invalid) -----\n");
   
   
   for (i = 0; i <= 0xf; ++i) {
      uint8_t exxx_1[3] = { 0x0, 0xab, 0x7f };
      uint8_t exxx_2[3] = { 0x0, 0xab, 0xc0 };

      exxx_1[0] = 0xe0 | i;
      exxx_2[0] = 0xe0 | i;
      run_test(buff, sizeof buff, exxx_1, sizeof exxx_1);
      run_test(buff, sizeof buff, exxx_2, sizeof exxx_2);
   };

   printf("\n----- Invalid 2nd char AND output exhausted -----\n");
   uint8_t pat1[] = {
      0xe0, 0x00, 0x80
   };
   run_test(buff, 1, pat1, 3);

   printf("\n----- Invalid 3rd char AND output exhausted -----\n");
   uint8_t pat2[] = {
      0xe4, 0x84, 0x00
   };
   run_test(buff, 1, pat2, 3);

   printf("\n----- 1st char valid, 2nd char invalid -----\n");
   uint8_t valid_invalid[] = {
      0xe1, 0x90, 0x90, 
      0xe1, 0x00, 0x90  
   };
   run_test(buff, sizeof buff, valid_invalid, sizeof valid_invalid);
}

void
convert_4_bytes(void)
{
   int i, j;

   printf("\n===== Conversion of a four-byte character =====\n");

   printf("\n----- Valid characters -----\n");
   for (i = 0; i <= 4; ++i) {
      uint8_t valid[4];

      valid[0] = 0xf0 | i;

      for (j = 0; j <= 1; ++j) {
         
         if (i == 0) {
            valid[1] = j == 0 ? 0x90 : 0xbf;    
         } else if (i == 4) {
            valid[1] = j == 0 ? 0x80 : 0x8f;    
         } else {
            valid[1] = j == 0 ? 0x80 : 0xbf;    
         }
         
         valid[2] = 0x80;
         valid[3] = 0x80;
         run_test(buff, sizeof buff, valid, sizeof valid);
         valid[2] = 0x80;
         valid[3] = 0xbf;
         run_test(buff, sizeof buff, valid, sizeof valid);
         valid[2] = 0xbf;
         valid[3] = 0x80;
         run_test(buff, sizeof buff, valid, sizeof valid);
         valid[2] = 0xbf;
         valid[3] = 0xbf;
         run_test(buff, sizeof buff, valid, sizeof valid);
      }
   }

   printf("\n----- Valid characters if m3 == 0 -----\n");
   
   uint8_t valid_if_not_m3[] = {
      0xf5, 0x00, 0x00, 0x00,
      0xf6, 0x11, 0x22, 0x33,
      0xf7, 0x44, 0x55, 0x66,
   };
   run_test(buff, sizeof buff, valid_if_not_m3, sizeof valid_if_not_m3);

   
   
   

   printf("\n----- Invalid characters (2nd byte is invalid) -----\n");
   
   
   uint8_t f0[4], f4[4];

   f0[0] = 0xf0;  
   f0[1] = 0x8f;  
   f0[2] = 0x80;  
   f0[3] = 0x80;  
   run_test(buff, sizeof buff, f0, sizeof f0);
   f0[1] = 0xc0;  
   run_test(buff, sizeof buff, f0, sizeof f0);

   f4[0] = 0xf4;  
   f4[1] = 0x7f;  
   f4[2] = 0x80;  
   f4[3] = 0x80;  
   run_test(buff, sizeof buff, f4, sizeof f4);
   f4[1] = 0x90;  
   run_test(buff, sizeof buff, f4, sizeof f4);

   for (i = 0; i <= 0x4; ++i) {
      uint8_t fxxx_1[4] = { 0x0, 0x7f, 0x80, 0x80 };
      uint8_t fxxx_2[4] = { 0x0, 0xc0, 0x80, 0x80 };

      if (i == 0) continue;   
      if (i == 4) continue;   

      fxxx_1[0] = 0xf0 | i;
      fxxx_2[0] = 0xf0 | i;
      run_test(buff, sizeof buff, fxxx_1, sizeof fxxx_1);
      run_test(buff, sizeof buff, fxxx_2, sizeof fxxx_2);
   };

   printf("\n----- Invalid characters (3rd byte is invalid) -----\n");
   
   
   for (i = 0; i <= 0x4; ++i) {
      uint8_t fxxx[4] = { 0x0, 0x0, 0x0, 0x80 };

      fxxx[0] = 0xf0 | i;
      fxxx[1] = (i == 0) ? 0x94 : 0x84;
      fxxx[2] = 0x7f;
      run_test(buff, sizeof buff, fxxx, sizeof fxxx);
      fxxx[2] = 0xc0;
      run_test(buff, sizeof buff, fxxx, sizeof fxxx);
   };

   printf("\n----- Invalid characters (4th byte is invalid) -----\n");
   
   
   for (i = 0; i <= 0x4; ++i) {
      uint8_t fxxx[4] = { 0x0, 0x0, 0x80, 0x0 };

      fxxx[0] = 0xf0 | i;
      fxxx[1] = (i == 0) ? 0x94 : 0x84;
      fxxx[3] = 0x7f;
      run_test(buff, sizeof buff, fxxx, sizeof fxxx);
      fxxx[3] = 0xc0;
      run_test(buff, sizeof buff, fxxx, sizeof fxxx);
   };

   printf("\n----- Invalid 2nd char AND output exhausted -----\n");
   uint8_t pat1[] = {
      0xf0, 0x00, 0x80, 0x80
   };
   run_test(buff, 1, pat1, 4);

   printf("\n----- Invalid 3rd char AND output exhausted -----\n");
   uint8_t pat2[] = {
      0xf0, 0xaa, 0x00, 0x80
   };
   run_test(buff, 3, pat2, 4);

   printf("\n----- Invalid 4th char AND output exhausted -----\n");
   uint8_t pat3[] = {
      0xf0, 0xaa, 0xaa, 0x00
   };
   run_test(buff, 3, pat3, 4);

   printf("\n----- 1st char valid, 2nd char invalid -----\n");
   uint8_t valid_invalid[] = {
      0xf0, 0xaa, 0xaa, 0xaa, 
      0xf0, 0x00, 0x00, 0x00  
   };
   run_test(buff, sizeof buff, valid_invalid, sizeof valid_invalid);
}


int main()
{
   convert_1_byte();
   convert_2_bytes();
   convert_3_bytes();
   convert_4_bytes();

   /* Length == 0, no memory should be read or written */
   printf("\n------------- test1 ----------------\n");
   run_test(NULL, 0, NULL, 0);

   
   printf("\n------------- test2.1 ----------------\n");

   /* No character will be written to BUFF, i.e. loop in jitted code
      is not iterated */
   run_test(buff, sizeof buff, NULL,     0);
   run_test(buff, sizeof buff, pattern1, 0);
   run_test(buff, sizeof buff, pattern2, 0);
   run_test(buff, sizeof buff, pattern2, 1);
   run_test(buff, sizeof buff, pattern3, 0);
   run_test(buff, sizeof buff, pattern3, 1);
   run_test(buff, sizeof buff, pattern3, 2);
   run_test(buff, sizeof buff, pattern4, 0);
   run_test(buff, sizeof buff, pattern4, 1);
   run_test(buff, sizeof buff, pattern4, 2);
   run_test(buff, sizeof buff, pattern4, 3);

   printf("\n------------- test2.2 ----------------\n");
   /* At least one character will be written to BUFF, i.e. loop in jitted
      code is iterated */
   run_test(buff, sizeof buff, pattern1, 2);
   run_test(buff, sizeof buff, pattern2, 5);
   run_test(buff, sizeof buff, pattern3, 6);
   run_test(buff, sizeof buff, pattern4, 9);

   
   printf("\n------------- test3.1 ----------------\n");

   /* No character will be written to BUFF, i.e. loop in jitted code
      is not iterated */

   
   run_test(NULL, 0, pattern1, sizeof pattern1);  
   run_test(NULL, 0, pattern2, sizeof pattern2);  
   run_test(NULL, 1, pattern2, sizeof pattern2);  
   run_test(NULL, 0, pattern3, sizeof pattern3);  
   run_test(NULL, 1, pattern3, sizeof pattern3);  
   run_test(NULL, 0, pattern4, sizeof pattern4);  
   run_test(NULL, 1, pattern4, sizeof pattern4);  
   run_test(NULL, 2, pattern4, sizeof pattern4);  
   run_test(NULL, 3, pattern4, sizeof pattern4);  

   printf("\n------------- test3.2 ----------------\n");
   /* At least one character will be written to BUFF, i.e. loop in jitted
      code is iterated */
   run_test(buff, 4, pattern1, sizeof pattern1);
   run_test(buff, 5, pattern1, sizeof pattern2);
   run_test(buff, 6, pattern1, sizeof pattern3);
   run_test(buff, 7, pattern1, sizeof pattern4);

   
   printf("\n------------- test4 ----------------\n");
   run_test(buff, sizeof buff, mixed, sizeof mixed);

   return 0;
}
