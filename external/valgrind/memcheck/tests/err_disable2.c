

#include <stdlib.h>
#include <stdio.h>

#include "../include/valgrind.h"

char* block = NULL;

__attribute__((noinline)) void usechar ( char c )
{
   
   
   
   __asm__ __volatile__("" : : "r"(c) : "memory","cc");
}

__attribute__((noinline)) void err ( void )
{
   usechar( block[5] );
}

int main ( void )
{
  block = malloc(10);
  free(block);

  fprintf(stderr, "\n--------- enabled (expect 1) ---------\n\n");

  err();

  fprintf(stderr, "\n--------- disabled (expect 0) ---------\n\n");
  VALGRIND_DISABLE_ERROR_REPORTING;

  err();

  fprintf(stderr, "\n--------- exiting (expect complaint) ---------\n\n");

  return 0;
}
