
#include <stdlib.h>
#include <strings.h>
#include <stdio.h>
#include <math.h>


int stack_fan_out = 15;
int stack_depth = 4; 

int malloc_fan = 4;

int malloc_depth = 2; 

int malloc_data = 5;

int free_every_n = 2;

int leak_every_n = 250;



struct Chunk {
   struct Chunk* child;
   char   s[];
};

struct Chunk** topblocks;
int freetop = 0;

long total_malloced = 0;
int blocknr = 0;
int blockfreed = 0;
int blockleaked = 0;
int total_stacks = 0;
int releaseop = 0;

void free_chunks (struct Chunk ** mem)
{
   if (*mem == 0)
      return;

   free_chunks ((&(*mem)->child));

   blockfreed++;
   free (*mem);
   *mem = 0; 
}

void release (struct Chunk ** mem)
{
   releaseop++;

   if (releaseop % leak_every_n == 0) {
      blockleaked++;
      *mem = 0; 
   } else {
      free_chunks (mem);
   }
}

void call_stack (int level)
{
   int call_fan_out = 1;

   if (level == stack_depth) {  
      int sz = sizeof(struct Chunk*) + malloc_data;
      int d;
      int f;

      for (f = 0; f < malloc_fan; f++) {
         struct Chunk *new  = NULL;    
         struct Chunk *prev = NULL;

         for (d = 0; d < malloc_depth; d++) {
            new = malloc (sz);
            total_malloced += sz;
            blocknr++;
            new->child = prev;
            prev = new;
         }
         topblocks[freetop] = new;

         if (freetop % free_every_n == 0) {
               release (&topblocks[freetop]);
         }
         freetop++;
      }

      total_stacks++;

   } else {
      call_stack (level + 1);
      if (call_fan_out == stack_fan_out) return;
      call_fan_out++;

      call_stack (level + 1);
      if (call_fan_out == stack_fan_out) return;
      call_fan_out++;

      call_stack (level + 1);
      if (call_fan_out == stack_fan_out) return;
      call_fan_out++;

      call_stack (level + 1);
      if (call_fan_out == stack_fan_out) return;
      call_fan_out++;

      call_stack (level + 1);
      if (call_fan_out == stack_fan_out) return;
      call_fan_out++;

      call_stack (level + 1);
      if (call_fan_out == stack_fan_out) return;
      call_fan_out++;

      call_stack (level + 1);
      if (call_fan_out == stack_fan_out) return;
      call_fan_out++;

      call_stack (level + 1);
      if (call_fan_out == stack_fan_out) return;
      call_fan_out++;

      call_stack (level + 1);
      if (call_fan_out == stack_fan_out) return;
      call_fan_out++;

      call_stack (level + 1);
      if (call_fan_out == stack_fan_out) return;
      call_fan_out++;

      call_stack (level + 1);
      if (call_fan_out == stack_fan_out) return;
      call_fan_out++;

      call_stack (level + 1);
      if (call_fan_out == stack_fan_out) return;
      call_fan_out++;

      call_stack (level + 1);
      if (call_fan_out == stack_fan_out) return;
      call_fan_out++;

      call_stack (level + 1);
      if (call_fan_out == stack_fan_out) return;
      call_fan_out++;

      call_stack (level + 1);
      if (call_fan_out == stack_fan_out) return;
      call_fan_out++;

      call_stack (level + 1);
      if (call_fan_out == stack_fan_out) return;
      call_fan_out++;

      call_stack (level + 1);
      if (call_fan_out == stack_fan_out) return;
      call_fan_out++;

      call_stack (level + 1);
      if (call_fan_out == stack_fan_out) return;
      call_fan_out++;

      call_stack (level + 1);
      if (call_fan_out == stack_fan_out) return;
      call_fan_out++;

      call_stack (level + 1);
      if (call_fan_out == stack_fan_out) return;
      call_fan_out++;

      printf ("maximum stack_fan_out exceeded\n");
   }
}

int main()
{
   int d;
   int stacks = 1;
   for (d = 0; d < stack_depth; d++)
      stacks *= stack_fan_out;
   printf ("will generate %d different stacks\n", stacks);
   topblocks = malloc(sizeof(struct Chunk*) * stacks * malloc_fan);
   call_stack (0);
   printf ("total stacks %d\n", total_stacks);
   printf ("total bytes malloc-ed: %ld\n", total_malloced);
   printf ("total blocks malloc-ed: %d\n", blocknr);
   printf ("total blocks free-ed: %d\n", blockfreed);
   printf ("total blocks leak-ed: %d\n", blockleaked);
   return 0;
}
