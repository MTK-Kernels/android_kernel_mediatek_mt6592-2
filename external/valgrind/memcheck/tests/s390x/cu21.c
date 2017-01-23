#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "../../../none/tests/s390x/opcodes.h"


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


void
do_cu21(uint8_t *dst, uint64_t dst_len, uint16_t *src, uint64_t src_len)
{
   
   register uint16_t *source     asm("4") = src;
   register uint64_t  source_len asm("5") = src_len;
   register uint8_t  *dest       asm("2") = dst;
   register uint64_t  dest_len   asm("3") = dst_len;

   asm volatile(
                CU21(0,2,4)
                : "+d"(dest), "+d"(source), "+d"(source_len), "+d"(dest_len)
                :
                : "memory", "cc");
   return;
}

int main()
{
   
   
   

   
   do_cu21(malloc(1), 10, pattern2, 2);             

   
   do_cu21(malloc(2), 10, pattern2, 2);             

   
   do_cu21(malloc(1), 10, pattern3, 2);             

   
   do_cu21(malloc(2), 10, pattern3, 2);             

   
   do_cu21(malloc(3), 10, pattern3, 2);             

   
   do_cu21(malloc(1), 10, pattern4, 4);             

   
   do_cu21(malloc(2), 10, pattern4, 4);             

   
   do_cu21(malloc(3), 10, pattern4, 4);             

   
   do_cu21(malloc(4), 10, pattern4, 4);             

   
   
   
   uint8_t *input = malloc(10);

   
   do_cu21(malloc(4), 4, (void *)input, 2);         
   
   
   input = malloc(10);
   input[1] = 0x0;
   do_cu21(malloc(4), 4, (void *)input, 2);          

   
   input = malloc(10);
   input[0] = 0x0;
   do_cu21(malloc(4), 4, (void *)input, 2);          
   
   
   input = malloc(10);
   input[0] = input[1] = 0x0;
   do_cu21(malloc(4), 4, (void *)input, 2);          
   
   input = malloc(10);
   input[0] = input[1] = 0x0;
   do_cu21(malloc(4), 4, (void *)input, 4);          
   
   
   

   return 0;
}
