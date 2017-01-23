

#include <stdio.h>

typedef unsigned int Addr;
typedef unsigned char UChar;

void q ( int n )
{
   printf("in q %d\n", n);
}

void p ( int n )
{
   printf("in p %d\n", n);
}

static UChar code[10];

void set_dest ( Addr dest )
{
   code[0] = 0x68; 
   code[1] = (dest & 0xFF);
   code[2] = ((dest >> 8) & 0xFF);
   code[3] = ((dest >> 16) & 0xFF);
   code[4] = ((dest >> 24) & 0xFF);
   code[5] = 0xC3;
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
   for (i = 0; i < 10; i += 2) {
      set_dest ( (Addr)&p );
      
      aa(i);
      set_dest ( (Addr)&q );
      
      aa(i+1);
   }
   return 0;
}
