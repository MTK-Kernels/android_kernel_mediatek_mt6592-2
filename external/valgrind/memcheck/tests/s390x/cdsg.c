#include <stdint.h>
#include <stdio.h>

typedef struct {
   uint64_t high;
   uint64_t low;
} __attribute__((aligned(16))) quad_word;


quad_word _op1, _op2, _op3;

void
test(quad_word op1_init, quad_word op2_init, quad_word op3_init)
{
   int cc; 
   _op1 = op1_init;
   _op2 = op2_init;
   _op3 = op3_init;

   __asm__ volatile (
                     "lmg     %%r0,%%r1,%1\n\t"
                     "lmg     %%r2,%%r3,%3\n\t"
                     "cdsg    %%r0,%%r2,%2\n\t"  
                     "stmg    %%r0,%%r1,%1\n"    
                     "stmg    %%r2,%%r3,%3\n"    
                     : "=d"(cc), "+QS" (_op1), "+QS" (_op2), "+QS" (_op3)
                     :
                     : "r0", "r1", "r2", "r3", "cc");
}

void op1_undefined(void)
{
   quad_word op1, op2, op3;

   
   op2.high = op2.low = 42;
   op3.high = op3.low = 0xdeadbeefdeadbabeull;
   test(op1, op2, op3);  
}

void op2_undefined(void)
{
   quad_word op1, op2, op3;

   op1.high = op1.low = 42;
   
   op3.high = op3.low = 0xdeadbeefdeadbabeull;
   test(op1, op2, op3);  
}

void op3_undefined(void)
{
   quad_word op1, op2, op3;

   op1.high = op1.low = 42;
   op2 = op1;
   
   test(op1, op2, op3);  
}

int main ()
{
   op1_undefined();
   op2_undefined();
   op3_undefined();

   return 0;
}
