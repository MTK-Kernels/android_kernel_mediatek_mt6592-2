

#include <stdio.h>
#include <string.h>
#include <assert.h>

typedef  unsigned short int      UShort;
typedef  unsigned int            UInt;
typedef  double                  Double;
typedef  unsigned long long int  ULong;

typedef  struct { Double arg; Double st0; Double st1; UShort fpusw; }  Res;

#define SHIFT_C3   14
#define SHIFT_C2   10
#define SHIFT_C1   9
#define SHIFT_C0   8


#define my_offsetof(type,memb) ((int)(unsigned long int)&((type*)0)->memb)

void do_fsin ( Res* r, double d )
{
   assert(my_offsetof(Res,arg) == 0);
   assert(my_offsetof(Res,st0) == 8);
   assert(my_offsetof(Res,st1) == 16);
   assert(my_offsetof(Res,fpusw) == 24);
   memset(r, 0, sizeof(*r));
   r->arg = d;
   __asm__ __volatile__(
     "finit"              "\n\t"
     "fldpi"              "\n\t"
     "fldl 0(%0)"         "\n\t" 
     "fsin"               "\n\t"
     "fstsw %%ax"         "\n\t"
     "fstpl 8(%0)"        "\n\t" 
     "fstpl 16(%0)"       "\n\t" 
     "movw %%ax, 24(%0)"  "\n\t" 
     "finit"              "\n"
     : : "r"(r) : "eax","cc","memory"
   );
}

void do_fcos ( Res* r, double d )
{
   assert(my_offsetof(Res,arg) == 0);
   assert(my_offsetof(Res,st0) == 8);
   assert(my_offsetof(Res,st1) == 16);
   assert(my_offsetof(Res,fpusw) == 24);
   memset(r, 0, sizeof(*r));
   r->arg = d;
   __asm__ __volatile__(
     "finit"              "\n\t"
     "fldpi"              "\n\t"
     "fldl 0(%0)"         "\n\t" 
     "fcos"               "\n\t"
     "fstsw %%ax"         "\n\t"
     "fstpl 8(%0)"        "\n\t" 
     "fstpl 16(%0)"       "\n\t" 
     "movw %%ax, 24(%0)"  "\n\t" 
     "finit"              "\n"
     : : "r"(r) : "eax","cc","memory"
   );
}

void do_fsincos ( Res* r, double d )
{
   assert(my_offsetof(Res,arg) == 0);
   assert(my_offsetof(Res,st0) == 8);
   assert(my_offsetof(Res,st1) == 16);
   assert(my_offsetof(Res,fpusw) == 24);
   memset(r, 0, sizeof(*r));
   r->arg = d;
   __asm__ __volatile__(
     "finit"              "\n\t"
     "fldpi"              "\n\t"
     "fldl 0(%0)"         "\n\t" 
     "fsincos"            "\n\t"
     "fstsw %%ax"         "\n\t"
     "fstpl 8(%0)"        "\n\t" 
     "fstpl 16(%0)"       "\n\t" 
     "movw %%ax, 24(%0)"  "\n\t" 
     "finit"              "\n"
     : : "r"(r) : "eax","cc","memory"
   );
}

void do_fptan ( Res* r, double d )
{
   assert(my_offsetof(Res,arg) == 0);
   assert(my_offsetof(Res,st0) == 8);
   assert(my_offsetof(Res,st1) == 16);
   assert(my_offsetof(Res,fpusw) == 24);
   memset(r, 0, sizeof(*r));
   r->arg = d;
   __asm__ __volatile__(
     "finit"              "\n\t"
     "fldpi"              "\n\t"
     "fldl 0(%0)"         "\n\t" 
     "fptan"              "\n\t"
     "fstsw %%ax"         "\n\t"
     "fstpl 8(%0)"        "\n\t" 
     "fstpl 16(%0)"       "\n\t" 
     "movw %%ax, 24(%0)"  "\n\t" 
     "finit"              "\n"
     : : "r"(r) : "eax","cc","memory"
   );
}


void try ( char* name, void(*fn)(Res*,double), double d )
{
   Res r;
   fn(&r, d);
   
   r.fpusw &= (1 << SHIFT_C2);
   printf("%s  %16e --> %16e %16e %04x\n",
          name, r.arg, r.st0, r.st1, (UInt)r.fpusw);
}

int main ( void )
{
   Double limit = 9223372036854775808.0; 

   char* names[4] = { "fsin   ", "fcos   ", "fsincos", "fptan  " };
   void(*fns[4])(Res*,double) = { do_fsin, do_fcos, do_fsincos, do_fptan };

   int i;
   for (i = 0; i < 4; i++) {
      char* name = names[i];
      void (*fn)(Res*,double) = fns[i];

      try( name, fn,   0.0   );
      try( name, fn,   0.123 );
      try( name, fn,  -0.456 );
      try( name, fn,  37.0   );
      try( name, fn, -53.0   );
      printf("\n");

      try( name, fn, limit * 0.900000 );
      try( name, fn, limit * 0.999999 );
      try( name, fn, limit * 1.000000 );
      try( name, fn, limit * 1.000001 );
      try( name, fn, limit * 1.100000 );
      printf("\n");

      try( name, fn, -limit * 0.900000 );
      try( name, fn, -limit * 0.999999 );
      try( name, fn, -limit * 1.000000 );
      try( name, fn, -limit * 1.000001 );
      try( name, fn, -limit * 1.100000 );
      printf("\n");
   }

   return 0;
}
