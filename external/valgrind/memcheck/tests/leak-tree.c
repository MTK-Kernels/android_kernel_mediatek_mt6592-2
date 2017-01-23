#include <stdio.h>
#include <stdlib.h>
#include "leak.h"
#include "../memcheck.h"


typedef
   struct _Node {
      struct _Node *l;
      struct _Node *r;
      
      
      char padding[16 - 2*sizeof(struct _Node*)];
   } Node;

Node* mk(void)
{
   Node *x = malloc(sizeof(Node));
   x->l = NULL;
   x->r = NULL;
   return x;
}

Node* t;

void f(void)
{
   
   
   t       = mk();   
   t->l    = mk();   
   t->r    = mk();   
   t->l->l = mk();   
   t->l->r = mk();   
   t->r->l = mk();   
   t->r->r = mk();   

   
   t->l->l = NULL;
 
   
   t->r = NULL;
}

int main(void)
{
   DECLARE_LEAK_COUNTERS;

   GET_INITIAL_LEAK_COUNTS;

   
   f();

   GET_FINAL_LEAK_COUNTS;

   PRINT_LEAK_COUNTS(stderr);

   return 0;
}

