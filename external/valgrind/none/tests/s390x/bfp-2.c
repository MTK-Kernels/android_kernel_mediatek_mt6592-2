#include <stdio.h>


void sqebr(float in)
{
   float out;

   __asm__ volatile("sqebr %[out],%[in]" : [out]"=f"(out) : [in]"f"(in));
   printf("sqebr  %f  -> %f\n", in, out);
}

void sqdbr(double in)
{
   double out;

   __asm__ volatile("sqdbr %[out],%[in]" : [out]"=f"(out) : [in]"f"(in));
   printf("sqdbr  %f  -> %f\n", in, out);
}

void lnebr(float in)
{
   float out;

   __asm__ volatile("lnebr %[out],%[in]" : [out]"=f"(out) : [in]"f"(in));
   printf("lnebr  %f  -> %f\n", in, out);
}

void lndbr(double in)
{
   double out;

   __asm__ volatile("lndbr %[out],%[in]" : [out]"=f"(out) : [in]"f"(in));
   printf("lndbr  %f  -> %f\n", in, out);
}

void lpebr(float in)
{
   float out;

   __asm__ volatile("lpebr %[out],%[in]" : [out]"=f"(out) : [in]"f"(in));
   printf("lpebr  %f  -> %f\n", in, out);
}

void lpdbr(double in)
{
   double out;

   __asm__ volatile("lpdbr %[out],%[in]" : [out]"=f"(out) : [in]"f"(in));
   printf("lpdbr  %f  -> %f\n", in, out);
}

void lcebr(float in)
{
   float out;

   __asm__ volatile("lcebr %[out],%[in]" : [out]"=f"(out) : [in]"f"(in));
   printf("lcebr  %f  -> %f\n", in, out);
}

void lcdbr(double in)
{
   double out;

   __asm__ volatile("lcdbr %[out],%[in]" : [out]"=f"(out) : [in]"f"(in));
   printf("lcdbr  %f  -> %f\n", in, out);
}

int main(void)
{
   
   sqebr(121.0f);  
   sqdbr(144.0);   

   
   lnebr(-2.5f);   
   lnebr(12.5f);   

   lndbr(-0.5);    
   lndbr(42.5);    

   
   lpebr(-2.5f);   
   lpebr(12.5f);   

   lpdbr(-0.5);    
   lpdbr(42.5);    

   
   lcebr(-23.5f);  
   lcebr(123.5f);  

   lcdbr(-17.5);   
   lcdbr(234.5);   

   return 0;
}
