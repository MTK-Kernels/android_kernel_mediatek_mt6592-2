

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <sys/syscall.h>
#include <unistd.h>

#define VG_STRINGIFZ(__str)  #__str
#define VG_STRINGIFY(__str)  VG_STRINGIFZ(__str)

#define __NR_READLINK        VG_STRINGIFY(__NR_readlink)

extern long my_readlink ( const char* path );
asm(
".text\n"
".globl my_readlink\n"
"my_readlink:\n"
"\tsubq    $0x1008,%rsp\n"
"\tmovq    %rdi,%rdi\n"              
"\tmovq    %rsp,%rsi\n"              
"\tmovl    $0x1000,%edx\n"           
"\tmovl    $"__NR_READLINK",%eax\n"  
"\tsyscall\n"
"\taddq    $0x1008,%rsp\n"
"\tret\n"
".previous\n"
);

long recurse ( const char* path, long count )
{
   if (count <= 0) {
      return my_readlink(path);
   } else { 
      long r = recurse(path, count-1);
      return r;
   }
}

int main ( void )
{
   long i, r;
   for (i = 0; i < 2000; i++) {
      printf("depth %ld: ", i );
      r = recurse( "/proc/self", i );
      if (r > 1) r = 1; 
      assert(r >= 1);
      printf("r = %ld\n", r);
   }
   return 0;
}
