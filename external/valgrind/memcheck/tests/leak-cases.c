#include <stdio.h>
#include <stdlib.h>
#include "leak.h"
#include "../memcheck.h"



typedef
   struct _Node {
      struct _Node* next;
      
      
      char padding[8 - sizeof(struct _Node*)];
   } Node;

Node* mk(Node* next)
{
   
   
   Node* x = malloc(2 * sizeof(Node));
   x->next = next;
   return x;
}

Node* p1;
Node* p2;
Node* p3;
Node* p4;
Node* p5;
Node* p6;
Node* p7;
Node* p8;
Node* p9;

void f(void)
{
   p1 = mk(NULL);       

   p2 = mk(mk(NULL));   
                                
   (void)mk(NULL);      

   (void)mk(mk(NULL));  
                                
   p5 = mk(NULL);       
   p5++;

   p6 = mk(mk(NULL));   
   (p6->next)++;                

   p7 = mk(mk(NULL));   
   p7++;                        

   p8 = mk(mk(NULL));   
   (p8->next)++;                
   p8++;

   p9 = mk(mk(NULL));   
   (p9->next)++;                
   p9 = NULL;
}

int main(void)
{
   DECLARE_LEAK_COUNTERS;

   GET_INITIAL_LEAK_COUNTS;

   
   
   
   
   
   f();

   CLEAR_CALLER_SAVED_REGS;
   GET_FINAL_LEAK_COUNTS;

   PRINT_LEAK_COUNTS(stderr);

   return 0;
}
