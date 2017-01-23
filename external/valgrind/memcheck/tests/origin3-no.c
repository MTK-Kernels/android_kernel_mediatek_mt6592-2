
/* This test case was originally written by Nicholas Nethercote. */


#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include "../memcheck.h"

int x = 0;

__attribute__((noinline)) int t1(void);
__attribute__((noinline)) int t2(void);
__attribute__((noinline)) int t3(void);
__attribute__((noinline)) int t4(void);
__attribute__((noinline)) int t5(void);
__attribute__((noinline)) int t6(void);

int main(void)
{
   assert(4 == sizeof(int));

   x += t1();
   x += t2();
   x += t3();
   x += t4();
   x += t5();
   x += t6();

   return x & 255;
}

__attribute__((noinline)) int t1(void)
{
   
   
   char* ptr_to_undef_char = malloc(sizeof(char));
   char  undef_char = *ptr_to_undef_char;
   fprintf(stderr, "\nUndef 1 of 8 (8 bit undef)\n");
   return (undef_char == 0x12 ? 11 : 22);
}

__attribute__((noinline)) int t2(void)
{
   
   
   int undef_stack_int;
   register char undef_stack_char = (char)undef_stack_int;
   fprintf(stderr, "\nUndef 2 of 8 (8 bits of 32 undef)\n");
   return (undef_stack_char == 0x12 ? 11 : 22);
}

__attribute__((noinline)) int t3(void)
{
   
   
   int* ptr_to_undef_int = malloc(sizeof(int));
   int  undef_int = *ptr_to_undef_int;
   fprintf(stderr, "\nUndef 3 of 8 (32 bit undef)\n");
   return (undef_int == 0x12345678 ? 13 : 24);
}

__attribute__((noinline)) int t4(void)
{
   
   int* ptr_to_undef_int = malloc(sizeof(int) + 1);
   int  undef_unaligned_int = *(int*)((long)ptr_to_undef_int + 1);
   fprintf(stderr, "\nUndef 4 of 8 (32 bit undef, unaligned)\n");
   return (undef_unaligned_int == 0x12345678 ? 14 : 25);
}

__attribute__((noinline)) int t5(void)
{
   
   int* ptr_to_undef_int3 = malloc(sizeof(int));
   int  modified_undef_int = *ptr_to_undef_int3;
   fprintf(stderr, "\nUndef 5 of 8 (32 bit undef, modified)\n");
   modified_undef_int++;
   return (modified_undef_int == 0x12345678 ? 15 : 26);
}

__attribute__((noinline)) int t6(void)
{
   int y = 0;
   
   
   
   
   
   
   
   
   
   
   
   {
      int* ptr_to_3_undef_ints = calloc(3, sizeof(int));
      int* ptr_to_middle       = (int*)((long)ptr_to_3_undef_ints + 6);
      (void) VALGRIND_MAKE_MEM_UNDEFINED(ptr_to_3_undef_ints, 6);
      (void) VALGRIND_MAKE_MEM_UNDEFINED(ptr_to_middle,       6);
      fprintf(stderr, "\nUndef 6 of 8 (32 bit undef, unaligned, strange, #1)\n");
      y += (*(ptr_to_3_undef_ints + 0)  == 0x12345678 ? 16 : 27);
      fprintf(stderr, "\nUndef 7 of 8 (32 bit undef, unaligned, strange, #2)\n");
      y += (*(ptr_to_3_undef_ints + 1)  == 0x12345678 ? 17 : 28);
      fprintf(stderr, "\nUndef 8 of 8 (32 bit undef, unaligned, strange, #3)\n");
      y += (*(ptr_to_3_undef_ints + 2)  == 0x12345678 ? 18 : 29);
      return y;
   }

   return x;
}
