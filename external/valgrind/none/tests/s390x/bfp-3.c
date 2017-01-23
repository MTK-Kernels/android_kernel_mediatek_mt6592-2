#include <stdio.h>


void maebr(float v1, float v2, float v3)
{
   float r1 = v1;

   __asm__ volatile("maebr %[r1],%[r3],%[r2]"
                    : [r1]"+f"(r1) : [r2]"f"(v2), [r3]"f"(v3));
   printf("maebr  %f * %f + %f  -> %f\n", v2, v3, v1, r1);
}

void madbr(double v1, double v2, double v3)
{
   double r1 = v1;

   __asm__ volatile("madbr %[r1],%[r3],%[r2]"
                    : [r1]"+f"(r1) : [r2]"f"(v2), [r3]"f"(v3));
   printf("madbr  %f * %f + %f  -> %f\n", v2, v3, v1, r1);
}

void msebr(float v1, float v2, float v3)
{
   float r1 = v1;

   __asm__ volatile("msebr %[r1],%[r3],%[r2]"
                    : [r1]"+f"(r1) : [r2]"f"(v2), [r3]"f"(v3));
   printf("msebr  %f * %f - %f  -> %f\n", v2, v3, v1, r1);
}

void msdbr(double v1, double v2, double v3)
{
   double r1 = v1;

   __asm__ volatile("msdbr %[r1],%[r3],%[r2]"
                    : [r1]"+f"(r1) : [r2]"f"(v2), [r3]"f"(v3));
   printf("msdbr  %f * %f - %f  -> %f\n", v2, v3, v1, r1);
}

int main(void)
{
   
   maebr(10.5f, 20.25, 3.0);  
   madbr(-10.5, 42.75, -2.0); 

   
   msebr(10.5f, 20.25, 3.0);  
   msdbr(-10.5, 42.75, -2.0); 

   return 0;
}
