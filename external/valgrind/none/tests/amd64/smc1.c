

#include <stdio.h>
#include <assert.h>
#include "tests/sys_mman.h"

typedef unsigned long long int Addr;
typedef unsigned char UChar;

void q ( int n )
{
   printf("in q %d\n", n);
}

void p ( int n )
{
   printf("in p %d\n", n);
}

static UChar* code;

void set_dest ( Addr dest )
{
   assert(sizeof(Addr) == 8);

   
   code[0] = 0x48;
   code[1] = 0xB8;
   code[2] = (dest & 0xFF);
   code[3] = ((dest >>  8) & 0xFF);
   code[4] = ((dest >> 16) & 0xFF);
   code[5] = ((dest >> 24) & 0xFF);
   code[6] = ((dest >> 32) & 0xFF);
   code[7] = ((dest >> 40) & 0xFF);
   code[8] = ((dest >> 48) & 0xFF);
   code[9] = ((dest >> 56) & 0xFF);

   
   code[10] = 0x50;

   
   code[11] = 0xC3;
}


__attribute__((noinline))
void dd ( int x, void (*f)(int) ) { f(x); }

__attribute__((noinline))
void cc ( int x ) { dd(x, (void(*)(int)) &code[0]); }

__attribute__((noinline))
void bb ( int x ) { cc(x); }

__attribute__((noinline))
void aa ( int x ) { bb(x); }

__attribute__((noinline))
void diversion ( void ) { }

int main ( void )
{
   int i;
   code = mmap(NULL, 20, PROT_READ|PROT_WRITE|PROT_EXEC,
               MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
   assert(code != MAP_FAILED);
   for (i = 0; i < 10; i += 2) {
      set_dest ( (Addr)&p );
      
      aa(i);
      set_dest ( (Addr)&q );
      
      aa(i+1);
   }
   munmap(code, 20);
   return 0;
}
