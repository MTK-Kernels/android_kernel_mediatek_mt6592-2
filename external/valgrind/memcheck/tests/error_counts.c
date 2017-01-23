#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "../memcheck.h"
#include "leak.h"

int main(void)
{
   int  x;
   int  y = 0;
   int* reachable __attribute__((unused));
   int* dubious;
   int* leaked __attribute__((unused));
   DECLARE_LEAK_COUNTERS;

   
   assert(sizeof(long) == sizeof(void*));

   
   fprintf(stderr, "errors: %d\n\n", VALGRIND_COUNT_ERRORS);

   if (x == 0) {
      y++;
   } else {
      y--;
   }

   fprintf(stderr, "errors: %d\n\n", VALGRIND_COUNT_ERRORS);

   
   
   GET_INITIAL_LEAK_COUNTS;

   fprintf(stderr, "errors: %d\n\n", VALGRIND_COUNT_ERRORS);

   
   GET_FINAL_LEAK_COUNTS;
   PRINT_LEAK_COUNTS(stderr);
   fprintf(stderr, "\n");

   fprintf(stderr, "errors: %d\n\n", VALGRIND_COUNT_ERRORS);

   leaked = malloc(77);
   leaked = 0;

   dubious = malloc(88);
   dubious += 10;

   reachable = malloc(99);

   GET_FINAL_LEAK_COUNTS;
   PRINT_LEAK_COUNTS(stderr);
   fprintf(stderr, "\n");

   fprintf(stderr, "errors: %d\n", VALGRIND_COUNT_ERRORS);

   return 0;
}
