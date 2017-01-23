
#define _XOPEN_SOURCE 600 

#include "../../memcheck.h"

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

int aligned_strlen(const char *const s)
{
   assert(((unsigned long)s & 0x0F) == 0);

   const char *p = s;

   __asm__ __volatile__ ("\n1:\n"
                         "\tmovdqa (%0),%%xmm6\n"
                         "\tpcmpistri $0x3a,%%xmm6,%%xmm6\n"
                         "\tjc 2f\n"
                         "\tadd $0x10,%0\n"
                         "\tjmp 1b\n"
                         "2:\n"
                         "\tadd %%rcx,%0\n"
                         : "=p" (p) : "0" (p) : "xmm6", "rcx", "cc", "memory");

   return p-s;
}

int test_strlen(const char *const s, int valid)
{
   
   const size_t len = strlen(s) + 1;
   const size_t roundup = ((len+15)/16)*16;
   int result = -1;

   void *space;
   posix_memalign(&space, 16, roundup);
   memset(space, 'x', roundup);
   memcpy(space, s, len);

   const char *const s_copy = space;
   const unsigned char ff = 0xFF;
   if (valid) {
      
      size_t i;
      for (i=len ; i < roundup ; ++i)
         (void)VALGRIND_SET_VBITS(&s_copy[i], &ff, 1);
   }
   else {
      
      assert(len > 0);
      (void)VALGRIND_SET_VBITS(&s_copy[len-1], &ff, 1);
   }

   result = aligned_strlen(s_copy);

   free(space);

   return result;
}

void doit(const char *const s)
{
   printf("strlen(\"%s\")=%d\n", s, test_strlen(s, 1));

   fprintf(stderr, "strlen(\"%s\")=%s\n", s,
           test_strlen(s, 0) ? "true" : "false");
}

int main(int argc, char *argv[])
{
   doit("");
   doit("a");
   doit("ab");
   doit("abc");
   doit("abcd");
   doit("abcde");
   doit("abcdef");
   doit("abcdefg");
   
   doit("abcdefgh");
   doit("abcdefghi");
   doit("abcdefghij");
   doit("abcdefghijk");
   doit("abcdefghijkl");
   doit("abcdefghijklm");
   doit("abcdefghijklmn");
   doit("abcdefghijklmno");
   
   doit("abcdefghijklmnop");
   doit("abcdefghijklmnopq");
   doit("abcdefghijklmnopqr");
   doit("abcdefghijklmnopqrs");
   doit("abcdefghijklmnopqrst");
   doit("abcdefghijklmnopqrstu");
   doit("abcdefghijklmnopqrstuv");
   doit("abcdefghijklmnopqrstuvw");
   doit("abcdefghijklmnopqrstuwvx");
   doit("abcdefghijklmnopqrstuwvxy");
   doit("abcdefghijklmnopqrstuwvxyz");
   
   doit("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
   
   doit("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
   return 0;
}
