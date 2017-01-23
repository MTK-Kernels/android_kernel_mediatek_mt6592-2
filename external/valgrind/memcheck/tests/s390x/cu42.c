#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "../../../none/tests/s390x/opcodes.h"


uint32_t pattern2[] = {
   0x0000, 0xd7ff,    
   0xdc00, 0xffff,    
   0xabba, 0xf00d, 0xd00f, 0x1234 
};

uint32_t pattern4[] = {
   0x00010000, 0x0010ffff,    
   0x00010123, 0x00023456, 0x000789ab, 0x00100000  
};

static void
do_cu42(uint16_t *dst, uint64_t dst_len, uint32_t *src, uint64_t src_len)
{
   
   register uint32_t *source     asm("4") = src;
   register uint64_t  source_len asm("5") = src_len;
   register uint16_t *dest       asm("2") = dst;
   register uint64_t  dest_len   asm("3") = dst_len;

   asm volatile(
                CU42(2,4)
                : "+d"(dest), "+d"(source), "+d"(source_len), "+d"(dest_len)
                :
                : "memory", "cc");
}

int main()
{
   
   
   

   
   do_cu42(malloc(1), 10, pattern2, 4);             

   
   do_cu42(malloc(2), 10, pattern2, 4);             

   
   do_cu42(malloc(1), 10, pattern4, 4);             

   
   do_cu42(malloc(2), 10, pattern4, 4);             

   
   do_cu42(malloc(3), 10, pattern4, 4);             

   
   do_cu42(malloc(4), 10, pattern4, 4);             

   
   
   
   uint16_t buf[100];
   uint8_t *input;

   
   input = malloc(10);
   do_cu42(buf, sizeof buf, (void *)input, 4);         
   
   
   input = malloc(10);
   input[1] = input[2] = input[3] = 0x0;
   do_cu42(buf, sizeof buf, (void *)input, 4);          

   
   input = malloc(10);
   input[0] = input[2] = input[3] = 0x0;
   do_cu42(buf, sizeof buf, (void *)input, 4);          
   
   
   input = malloc(10);
   input[0] = input[1] = input[3] = 0x0;
   do_cu42(buf, sizeof buf, (void *)input, 4);          
   
   
   input = malloc(10);
   input[0] = input[1] = input[2] = 0x0;
   do_cu42(buf, sizeof buf, (void *)input, 4);          
   
   
   input = malloc(10);
   memset(input, 0, 4);
   do_cu42(buf, sizeof buf, (void *)input, 4);          

   input = malloc(10);
   memset(input, 0, 4);
   do_cu42(buf, sizeof buf, (void *)input, 8);          
   
   
   
   

   return 0;
}
