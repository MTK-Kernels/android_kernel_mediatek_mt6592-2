

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

typedef 
struct __attribute__ ((__packed__)) {
  char before[2];
  unsigned long long int w64;
  char after[6];
}
T;

void foo (T* t, unsigned long long int w)
{
  __asm__ __volatile__(
     "stdx %0,%1,%2"
     : : "b"(w), "b"(t), "b"(2) : "memory"
  );
}

int main ( void )
{
  T* t;
  unsigned char* p;
  int i;
  assert(sizeof(T) == 16);
  t = calloc(sizeof(T),1);
  assert(t);
  assert(0 == (((unsigned long)t) & 7));
  foo(t, 0x1122334455667788);
  p = (unsigned char*)t;
  for (i = 0; i < 16; i++)
    if (p[i] == 0)
      printf(".."); 
    else
      printf("%02x", (int)p[i]);
  printf("\n");
  return 0;
}
