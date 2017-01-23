
/* This test case was originally written by Nicholas Nethercote. */

// immediately get written with stuff, so there's no significant possibility

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include "tests/sys_mman.h"
#include <unistd.h>
#include "../memcheck.h"

int x = 0;

int main(void)
{
   assert(1 == sizeof(char));
   assert(2 == sizeof(short));
   assert(4 == sizeof(int));
   assert(8 == sizeof(long long));

   
   
   

   
   {
      volatile int undef_stack_int;
      fprintf(stderr, "\nUndef 1 of 8 (stack, 32 bit)\n");
      x += (undef_stack_int == 0x12345678 ? 10 : 21);
   }
   
   
   
   
   {
      volatile int undef_stack_int;
      register int modified_undef_stack_int;
      fprintf(stderr, "\nUndef 2 of 8 (stack, 32 bit)\n");
      modified_undef_stack_int = undef_stack_int;
      modified_undef_stack_int++;
      x += (modified_undef_stack_int == 0x1234 ? 11 : 22);
   }
   
   
   {
      volatile long long undef_stack_longlong;
      fprintf(stderr, "\nUndef 3 of 8 (stack, 64 bit)\n");
      x += (undef_stack_longlong == 0x1234567812345678LL ? 11 : 22);
   }
   
   
   {
      int* ptr_to_undef_malloc_int = malloc(sizeof(int));
      int  undef_malloc_int = *ptr_to_undef_malloc_int;
      fprintf(stderr, "\nUndef 4 of 8 (mallocd, 32-bit)\n");
      x += (undef_malloc_int == 0x12345678 ? 12 : 23);
   }

   
   {
      int* ptr_to_undef_malloc_int2 = malloc(sizeof(int));
         
      int* ptr_to_undef_realloc_int = realloc(ptr_to_undef_malloc_int2, 4096);
         
         
      int  undef_realloc_int = *(ptr_to_undef_realloc_int+1);
      fprintf(stderr, "\nUndef 5 of 8 (realloc)\n");
      x += (undef_realloc_int == 0x12345678 ? 13 : 24);
   }

   
   {
      int  undef_custom_alloc_int;
      VALGRIND_MALLOCLIKE_BLOCK(&undef_custom_alloc_int, sizeof(int),
                                0, 0);
      fprintf(stderr, "\nUndef 6 of 8 (MALLOCLIKE_BLOCK)\n");
      x += (undef_custom_alloc_int == 0x12345678 ? 14 : 25);
   }

   
   
   
   
   
   
   
   
   
   
   
      fprintf(stderr, "\nUndef 7 of 8 (brk)\n");
      fprintf(stderr, "\n(currently disabled)\n");

   
   {
      int  undef_user_int = 0;
      (void) VALGRIND_MAKE_MEM_UNDEFINED(&undef_user_int, sizeof(int));
      fprintf(stderr, "\nUndef 8 of 8 (MAKE_MEM_UNDEFINED)\n");
      x += (undef_user_int == 0x12345678 ? 16 : 27);
   }

   
   
   

   
   {
      int* ptr_to_def_calloc_int = calloc(1, sizeof(int));
      int  def_calloc_int = *ptr_to_def_calloc_int;
      fprintf(stderr, "\nDef 1 of 3\n");
      x += (def_calloc_int == 0x12345678 ? 17 : 28);
   }

   
   {
      int  def_custom_alloc_int = 0;
      fprintf(stderr, "\nDef 2 of 3\n");
      VALGRIND_MALLOCLIKE_BLOCK(&def_custom_alloc_int, sizeof(int),
                                0, 1);
      x += (def_custom_alloc_int == 0x12345678 ? 18 : 29);
   }

   
   {
      int* ptr_to_def_mmap_int =
               mmap(0, 4096, PROT_READ, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
      int def_mmap_int = *ptr_to_def_mmap_int;
      fprintf(stderr, "\nDef 3 of 3\n");
      x += (def_mmap_int == 0x12345678 ? 19 : 30);
   }

   return x;
}
