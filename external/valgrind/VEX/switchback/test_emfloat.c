
#include "../pub/libvex_basictypes.h"

static HWord (*serviceFn)(HWord,HWord) = 0;



static char* my_strcpy ( char* dest, const char* src )
{
   char* dest_orig = dest;
   while (*src) *dest++ = *src++;
   *dest = 0;
   return dest_orig;
}

static void* my_memcpy ( void *dest, const void *src, int sz )
{
   const char *s = (const char *)src;
   char *d = (char *)dest;

   while (sz--)
      *d++ = *s++;

   return dest;
}

static void* my_memmove( void *dst, const void *src, unsigned int len )
{
    register char *d;
    register char *s;
    if ( dst > src ) {
        d = (char *)dst + len - 1;
        s = (char *)src + len - 1;
        while ( len >= 4 ) {
            *d-- = *s--;
            *d-- = *s--;
            *d-- = *s--;
            *d-- = *s--;
            len -= 4;
        }
        while ( len-- ) {
            *d-- = *s--;
        }
    } else if ( dst < src ) {
        d = (char *)dst;
        s = (char *)src;
        while ( len >= 4 ) {
            *d++ = *s++;
            *d++ = *s++;
            *d++ = *s++;
            *d++ = *s++;
            len -= 4;
        }
        while ( len-- ) {
            *d++ = *s++;
        }
    }
    return dst;
}


static void vexxx_log_bytes ( char* p, int n )
{
   int i;
   for (i = 0; i < n; i++)
      (*serviceFn)( 1, (int)p[i] );
}


#include <stdarg.h>

static HChar vexxx_toupper ( HChar c )
{
   if (c >= 'a' && c <= 'z')
      return toHChar(c + ('A' - 'a'));
   else
      return c;
}

static Int vexxx_strlen ( const HChar* str )
{
   Int i = 0;
   while (str[i] != 0) i++;
   return i;
}

Bool vexxx_streq ( const HChar* s1, const HChar* s2 )
{
   while (True) {
      if (*s1 == 0 && *s2 == 0)
         return True;
      if (*s1 != *s2)
         return False;
      s1++;
      s2++;
   }
}

#define VG_MSG_SIGNED    1 
#define VG_MSG_ZJUSTIFY  2 
#define VG_MSG_LJUSTIFY  4 
#define VG_MSG_PAREN     8 
#define VG_MSG_COMMA    16 

static UInt
myvprintf_str ( void(*send)(HChar), Int flags, Int width, HChar* str, 
                Bool capitalise )
{
#  define MAYBE_TOUPPER(ch) toHChar(capitalise ? vexxx_toupper(ch) : (ch))
   UInt ret = 0;
   Int i, extra;
   Int len = vexxx_strlen(str);

   if (width == 0) {
      ret += len;
      for (i = 0; i < len; i++)
         send(MAYBE_TOUPPER(str[i]));
      return ret;
   }

   if (len > width) {
      ret += width;
      for (i = 0; i < width; i++)
         send(MAYBE_TOUPPER(str[i]));
      return ret;
   }

   extra = width - len;
   if (flags & VG_MSG_LJUSTIFY) {
      ret += extra;
      for (i = 0; i < extra; i++)
         send(' ');
   }
   ret += len;
   for (i = 0; i < len; i++)
      send(MAYBE_TOUPPER(str[i]));
   if (!(flags & VG_MSG_LJUSTIFY)) {
      ret += extra;
      for (i = 0; i < extra; i++)
         send(' ');
   }

#  undef MAYBE_TOUPPER

   return ret;
}

static UInt
myvprintf_int64 ( void(*send)(HChar), Int flags, Int base, Int width, ULong pL)
{
   HChar buf[40];
   Int   ind = 0;
   Int   i, nc = 0;
   Bool  neg = False;
   HChar *digits = "0123456789ABCDEF";
   UInt  ret = 0;
   UInt  p = (UInt)pL;

   if (base < 2 || base > 16)
      return ret;
 
   if ((flags & VG_MSG_SIGNED) && (Int)p < 0) {
      p   = - (Int)p;
      neg = True;
   }

   if (p == 0)
      buf[ind++] = '0';
   else {
      while (p > 0) {
         if ((flags & VG_MSG_COMMA) && 10 == base &&
             0 == (ind-nc) % 3 && 0 != ind) 
         {
            buf[ind++] = ',';
            nc++;
         }
         buf[ind++] = digits[p % base];
         p /= base;
      }
   }

   if (neg)
      buf[ind++] = '-';

   if (width > 0 && !(flags & VG_MSG_LJUSTIFY)) {
      for(; ind < width; ind++) {
	
         buf[ind] = toHChar((flags & VG_MSG_ZJUSTIFY) ? '0': ' ');
      }
   }

   
   ret += ind;
   for (i = ind -1; i >= 0; i--) {
      send(buf[i]);
   }
   if (width > 0 && (flags & VG_MSG_LJUSTIFY)) {
      for(; ind < width; ind++) {
	 ret++;
         send(' ');  
      }
   }
   return ret;
}


static 
UInt vprintf_wrk ( void(*send)(HChar), const HChar *format, va_list vargs )
{
   UInt ret = 0;
   int i;
   int flags;
   int width;
   Bool is_long;


   for (i = 0; format[i] != 0; i++) {
      if (format[i] != '%') {
         send(format[i]);
	 ret++;
         continue;
      }
      i++;
      
      if (format[i] == 0)
         break;
      if (format[i] == '%') {
         
         send('%');
	 ret++;
         continue;
      }
      flags = 0;
      is_long = False;
      width = 0; 
      if (format[i] == '(') {
	 flags |= VG_MSG_PAREN;
	 i++;
      }
      
      if (format[i] == ',') {
         flags |= VG_MSG_COMMA;
         i++;
      }
      
      if (format[i] == '-') {
         flags |= VG_MSG_LJUSTIFY;
         i++;
      }
      
      if (format[i] == '0') {
         flags |= VG_MSG_ZJUSTIFY;
         i++;
      }
      
      while (format[i] >= '0' && format[i] <= '9') {
         width *= 10;
         width += format[i++] - '0';
      }
      while (format[i] == 'l') {
         i++;
         is_long = True;
      }

      switch (format[i]) {
         case 'd': 
            flags |= VG_MSG_SIGNED;
            if (is_long)
               ret += myvprintf_int64(send, flags, 10, width, 
				      (ULong)(va_arg (vargs, Long)));
            else
               ret += myvprintf_int64(send, flags, 10, width, 
				      (ULong)(va_arg (vargs, Int)));
            break;
         case 'u': 
            if (is_long)
               ret += myvprintf_int64(send, flags, 10, width, 
				      (ULong)(va_arg (vargs, ULong)));
            else
               ret += myvprintf_int64(send, flags, 10, width, 
				      (ULong)(va_arg (vargs, UInt)));
            break;
         case 'p': 
	    ret += 2;
            send('0');
            send('x');
            ret += myvprintf_int64(send, flags, 16, width, 
				   (ULong)((HWord)va_arg (vargs, void *)));
            break;
         case 'x': 
            if (is_long)
               ret += myvprintf_int64(send, flags, 16, width, 
				      (ULong)(va_arg (vargs, ULong)));
            else
               ret += myvprintf_int64(send, flags, 16, width, 
				      (ULong)(va_arg (vargs, UInt)));
            break;
         case 'c': 
	    ret++;
            send(toHChar(va_arg (vargs, int)));
            break;
         case 's': case 'S': { 
            char *str = va_arg (vargs, char *);
            if (str == (char*) 0) str = "(null)";
            ret += myvprintf_str(send, flags, width, str, 
                                 toBool(format[i]=='S'));
            break;
	 }
#        if 0
	 case 'y': { 
	    Addr a = va_arg(vargs, Addr);

            HChar *name;
	    if (VG_(get_fnname_w_offset)(a, &name)) {
               HChar buf[1 + VG_strlen(name) + 1 + 1];
	       if (flags & VG_MSG_PAREN) {
                  VG_(sprintf)(str, "(%s)", name):
	       } else {
                  VG_(sprintf)(str, "%s", name):
               }
	       ret += myvprintf_str(send, flags, width, buf, 0);
	    }
	    break;
	 }
#        endif
         default:
            break;
      }
   }
   return ret;
}


static HChar myprintf_buf[1000];
static Int   n_myprintf_buf;

static void add_to_myprintf_buf ( HChar c )
{
   if (c == '\n' || n_myprintf_buf >= 1000-10  ) {
      (*vexxx_log_bytes)( myprintf_buf, vexxx_strlen(myprintf_buf) );
      n_myprintf_buf = 0;
      myprintf_buf[n_myprintf_buf] = 0;      
   }
   myprintf_buf[n_myprintf_buf++] = c;
   myprintf_buf[n_myprintf_buf] = 0;
}

static UInt vexxx_printf ( const char *format, ... )
{
   UInt ret;
   va_list vargs;
   va_start(vargs,format);
   
   n_myprintf_buf = 0;
   myprintf_buf[n_myprintf_buf] = 0;      
   ret = vprintf_wrk ( add_to_myprintf_buf, format, vargs );

   if (n_myprintf_buf > 0) {
      (*vexxx_log_bytes)( myprintf_buf, n_myprintf_buf );
   }

   va_end(vargs);

   return ret;
}





typedef unsigned char uchar;
typedef unsigned int uint;
typedef unsigned short ushort;
typedef unsigned long ulong;
typedef int int32;              

#define INTERNAL_FPF_PRECISION 4
#define CPUEMFLOATLOOPMAX 500000L
#define EMFARRAYSIZE 3000L

typedef struct {
        int adjust;             
        ulong request_secs;     
        ulong arraysize;        
        ulong loops;            
        double emflops;         
} EmFloatStruct;




#define u8 unsigned char
#define u16 unsigned short
#ifdef LONG64
#define u32 unsigned int
#else
#define u32 unsigned long
#endif
#define uchar unsigned char
#define ulong unsigned long

#define MAX_EXP 32767L
#define MIN_EXP (-32767L)

#define IFPF_IS_ZERO 0
#define IFPF_IS_SUBNORMAL 1
#define IFPF_IS_NORMAL 2
#define IFPF_IS_INFINITY 3
#define IFPF_IS_NAN 4
#define IFPF_TYPE_COUNT 5

#define ZERO_ZERO                       0
#define ZERO_SUBNORMAL                  1
#define ZERO_NORMAL                     2
#define ZERO_INFINITY                   3
#define ZERO_NAN                        4

#define SUBNORMAL_ZERO                  5
#define SUBNORMAL_SUBNORMAL             6
#define SUBNORMAL_NORMAL                7
#define SUBNORMAL_INFINITY              8
#define SUBNORMAL_NAN                   9

#define NORMAL_ZERO                     10
#define NORMAL_SUBNORMAL                11
#define NORMAL_NORMAL                   12
#define NORMAL_INFINITY                 13
#define NORMAL_NAN                      14

#define INFINITY_ZERO                   15
#define INFINITY_SUBNORMAL              16
#define INFINITY_NORMAL                 17
#define INFINITY_INFINITY               18
#define INFINITY_NAN                    19

#define NAN_ZERO                        20
#define NAN_SUBNORMAL                   21
#define NAN_NORMAL                      22
#define NAN_INFINITY                    23
#define NAN_NAN                         24
#define OPERAND_ZERO                    0
#define OPERAND_SUBNORMAL               1
#define OPERAND_NORMAL                  2
#define OPERAND_INFINITY                3
#define OPERAND_NAN                     4

typedef struct
{
        u8 type;        
        u8 sign;        
        short exp;      
        u16 mantissa[INTERNAL_FPF_PRECISION];
} InternalFPF;

static
void SetupCPUEmFloatArrays(InternalFPF *abase,
        InternalFPF *bbase, InternalFPF *cbase, ulong arraysize);
static
ulong DoEmFloatIteration(InternalFPF *abase,
        InternalFPF *bbase, InternalFPF *cbase,
        ulong arraysize, ulong loops);

static void SetInternalFPFZero(InternalFPF *dest,
                        uchar sign);
static void SetInternalFPFInfinity(InternalFPF *dest,
                        uchar sign);
static void SetInternalFPFNaN(InternalFPF *dest);
static int IsMantissaZero(u16 *mant);
static void Add16Bits(u16 *carry,u16 *a,u16 b,u16 c);
static void Sub16Bits(u16 *borrow,u16 *a,u16 b,u16 c);
static void ShiftMantLeft1(u16 *carry,u16 *mantissa);
static void ShiftMantRight1(u16 *carry,u16 *mantissa);
static void StickyShiftRightMant(InternalFPF *ptr,int amount);
static void normalize(InternalFPF *ptr);
static void denormalize(InternalFPF *ptr,int minimum_exponent);
static void RoundInternalFPF(InternalFPF *ptr);
static void choose_nan(InternalFPF *x,InternalFPF *y,InternalFPF *z,
                int intel_flag);
static void AddSubInternalFPF(uchar operation,InternalFPF *x,
                InternalFPF *y,InternalFPF *z);
static void MultiplyInternalFPF(InternalFPF *x,InternalFPF *y,
                        InternalFPF *z);
static void DivideInternalFPF(InternalFPF *x,InternalFPF *y, 
                        InternalFPF *z);

static void Int32ToInternalFPF(int32 mylong,
                InternalFPF *dest);
static int InternalFPFToString(char *dest,
                InternalFPF *src);

static int32 randnum(int32 lngval);

static int32 randwc(int32 num)
{
	return(randnum((int32)0)%num);
}

static int32 randw[2] = { (int32)13 , (int32)117 };
static int32 randnum(int32 lngval)
{
	register int32 interm;

	if (lngval!=(int32)0)
	{	randw[0]=(int32)13; randw[1]=(int32)117; }

	interm=(randw[0]*(int32)254754+randw[1]*(int32)529562)%(int32)999563;
	randw[1]=randw[0];
	randw[0]=interm;
	return(interm);
}


static 
void SetupCPUEmFloatArrays(InternalFPF *abase,
                InternalFPF *bbase,
                InternalFPF *cbase,
                ulong arraysize)
{
ulong i;
InternalFPF locFPF1,locFPF2;

randnum((int32)13);

for(i=0;i<arraysize;i++)
{
        Int32ToInternalFPF(randwc((int32)50000),&locFPF1);
 
        Int32ToInternalFPF(randwc((int32)50000)+(int32)1,&locFPF2);
        DivideInternalFPF(&locFPF1,&locFPF2,abase+i);
 
        Int32ToInternalFPF(randwc((int32)50000)+(int32)1,&locFPF2);
        DivideInternalFPF(&locFPF1,&locFPF2,bbase+i);
}
return;
}


static char* str1 = "loops %d\n";
static 
ulong DoEmFloatIteration(InternalFPF *abase,
                InternalFPF *bbase,
                InternalFPF *cbase,
                ulong arraysize, ulong loops)
{
static uchar jtable[16] = {0,0,0,0,1,1,1,1,2,2,2,2,2,3,3,3};
ulong i;
int number_of_loops;
 loops = 100;
number_of_loops=loops-1; 

vexxx_printf(str1, (int)loops);


{
        for(i=0;i<arraysize;i++)
                switch(jtable[i % 16])
                {
                        case 0: 
                                AddSubInternalFPF(0,abase+i,
                                  bbase+i,
                                  cbase+i);
                                break;
                        case 1: 
                                AddSubInternalFPF(1,abase+i,
                                  bbase+i,
                                  cbase+i);
                                break;
                        case 2: 
                                MultiplyInternalFPF(abase+i,
                                  bbase+i,
                                  cbase+i);
                                break;
                        case 3: 
                                DivideInternalFPF(abase+i,
                                  bbase+i,
                                  cbase+i);
                                break;
                }
{
  ulong j[8];   
  int k;
  ulong i;
  char buffer[1024];
  if (100==loops) 
    {
      j[0]=(ulong)2;
      j[1]=(ulong)6;
      j[2]=(ulong)10;
      j[3]=(ulong)14;
      j[4]=(ulong)(arraysize-14);
      j[5]=(ulong)(arraysize-10);
      j[6]=(ulong)(arraysize-6);
      j[7]=(ulong)(arraysize-2);
      for(k=0;k<8;k++){
	i=j[k];
	InternalFPFToString(buffer,abase+i);
	vexxx_printf("%6d: (%s) ",i,buffer);
	switch(jtable[i % 16])
	  {
	  case 0: my_strcpy(buffer,"+"); break;
	  case 1: my_strcpy(buffer,"-"); break;
	  case 2: my_strcpy(buffer,"*"); break;
	  case 3: my_strcpy(buffer,"/"); break;
	  }
	vexxx_printf("%s ",buffer);
	InternalFPFToString(buffer,bbase+i);
	vexxx_printf("(%s) = ",buffer);
	InternalFPFToString(buffer,cbase+i);
	vexxx_printf("%s\n",buffer);
      }
return 0;
    }
}
}
return 0;
}

static void SetInternalFPFZero(InternalFPF *dest,
                        uchar sign)
{
int i;          

dest->type=IFPF_IS_ZERO;
dest->sign=sign;
dest->exp=MIN_EXP;
for(i=0;i<INTERNAL_FPF_PRECISION;i++)
        dest->mantissa[i]=0;
return;
}

static void SetInternalFPFInfinity(InternalFPF *dest,
                        uchar sign)
{
int i;          

dest->type=IFPF_IS_INFINITY;
dest->sign=sign;
dest->exp=MIN_EXP;
for(i=0;i<INTERNAL_FPF_PRECISION;i++)
        dest->mantissa[i]=0;
return;
}

static void SetInternalFPFNaN(InternalFPF *dest)
{
int i;          

dest->type=IFPF_IS_NAN;
dest->exp=MAX_EXP;
dest->sign=1;
dest->mantissa[0]=0x4000;
for(i=1;i<INTERNAL_FPF_PRECISION;i++)
        dest->mantissa[i]=0;

return;
}

static int IsMantissaZero(u16 *mant)
{
int i;          
int n;          

n=0;
for(i=0;i<INTERNAL_FPF_PRECISION;i++)
        n|=mant[i];

return(!n);
}

static void Add16Bits(u16 *carry,
                u16 *a,
                u16 b,
                u16 c)
{
u32 accum;              

accum=(u32)b;
accum+=(u32)c;
accum+=(u32)*carry;
*carry=(u16)((accum & 0x00010000) ? 1 : 0);     
*a=(u16)(accum & 0xFFFF);       
return;
}

static void Sub16Bits(u16 *borrow,
                u16 *a,
                u16 b,
                u16 c)
{
u32 accum;              

accum=(u32)b;
accum-=(u32)c;
accum-=(u32)*borrow;
*borrow=(u32)((accum & 0x00010000) ? 1 : 0);    
*a=(u16)(accum & 0xFFFF);
return;
}

static void ShiftMantLeft1(u16 *carry,
                        u16 *mantissa)
{
int i;          
int new_carry;
u16 accum;      

for(i=INTERNAL_FPF_PRECISION-1;i>=0;i--)
{       accum=mantissa[i];
        new_carry=accum & 0x8000;       
        accum=accum<<1;                 
        if(*carry)
                accum|=1;               
        *carry=new_carry;
        mantissa[i]=accum;              
}
return;
}

static void ShiftMantRight1(u16 *carry,
                        u16 *mantissa)
{
int i;          
int new_carry;
u16 accum;

for(i=0;i<INTERNAL_FPF_PRECISION;i++)
{       accum=mantissa[i];
        new_carry=accum & 1;            
        accum=accum>>1;
        if(*carry)
                accum|=0x8000;
        *carry=new_carry;
        mantissa[i]=accum;
}
return;
}


static void StickyShiftRightMant(InternalFPF *ptr,
                        int amount)
{
int i;          
u16 carry;      
u16 *mantissa;

mantissa=ptr->mantissa;

if(ptr->type!=IFPF_IS_ZERO)     
{
        if(amount>=INTERNAL_FPF_PRECISION * 16)
        {
                for(i=0;i<INTERNAL_FPF_PRECISION-1;i++)
                        mantissa[i]=0;
                mantissa[INTERNAL_FPF_PRECISION-1]=1;
        }
        else
                for(i=0;i<amount;i++)
                {
                        carry=0;
                        ShiftMantRight1(&carry,mantissa);
                        if(carry)
                                mantissa[INTERNAL_FPF_PRECISION-1] |= 1;
                }
}
return;
}



static void normalize(InternalFPF *ptr)
{
u16     carry;

while ((ptr->mantissa[0] & 0x8000) == 0)
{
        carry = 0;
        ShiftMantLeft1(&carry, ptr->mantissa);
        ptr->exp--;
}
return;
}

static void denormalize(InternalFPF *ptr,
                int minimum_exponent)
{
long exponent_difference;

if (IsMantissaZero(ptr->mantissa))
{
        vexxx_printf("Error:  zero significand in denormalize\n");
}

exponent_difference = ptr->exp-minimum_exponent;
if (exponent_difference < 0)
{
        exponent_difference = -exponent_difference;
        if (exponent_difference >= (INTERNAL_FPF_PRECISION * 16))
        {
                
                SetInternalFPFZero(ptr, ptr->sign);
        }
        else
        {
                ptr->exp+=exponent_difference;
                StickyShiftRightMant(ptr, exponent_difference);
        }
}
return;
}


void RoundInternalFPF(InternalFPF *ptr)
{

if (ptr->type == IFPF_IS_NORMAL ||
        ptr->type == IFPF_IS_SUBNORMAL)
{
        denormalize(ptr, MIN_EXP);
        if (ptr->type != IFPF_IS_ZERO)
        {

                
                ptr->mantissa[3] &= 0xfff8;
        }
}
return;
}


static void choose_nan(InternalFPF *x,
                InternalFPF *y,
                InternalFPF *z,
                int intel_flag)
{
int i;

for (i=0; i<INTERNAL_FPF_PRECISION; i++)
{
        if (x->mantissa[i] > y->mantissa[i])
        {
                my_memmove((void *)x,(void *)z,sizeof(InternalFPF));
                return;
        }
        if (x->mantissa[i] < y->mantissa[i])
        {
                my_memmove((void *)y,(void *)z,sizeof(InternalFPF));
                return;
        }
}

if (!intel_flag)
        
        my_memmove((void *)x,(void *)z,sizeof(InternalFPF));
else
        
        my_memmove((void *)y,(void *)z,sizeof(InternalFPF));
return;
}


static void AddSubInternalFPF(uchar operation,
                InternalFPF *x,
                InternalFPF *y,
                InternalFPF *z)
{
int exponent_difference;
u16 borrow;
u16 carry;
int i;
InternalFPF locx,locy;  

switch ((x->type * IFPF_TYPE_COUNT) + y->type)
{
case ZERO_ZERO:
        my_memmove((void *)x,(void *)z,sizeof(InternalFPF));
        if (x->sign ^ y->sign ^ operation)
        {
                z->sign = 0; 
        }
        break;

case NAN_ZERO:
case NAN_SUBNORMAL:
case NAN_NORMAL:
case NAN_INFINITY:
case SUBNORMAL_ZERO:
case NORMAL_ZERO:
case INFINITY_ZERO:
case INFINITY_SUBNORMAL:
case INFINITY_NORMAL:
        my_memmove((void *)x,(void *)z,sizeof(InternalFPF));
        break;


case ZERO_NAN:
case SUBNORMAL_NAN:
case NORMAL_NAN:
case INFINITY_NAN:
        my_memmove((void *)y,(void *)z,sizeof(InternalFPF));
        break;

case ZERO_SUBNORMAL:
case ZERO_NORMAL:
case ZERO_INFINITY:
case SUBNORMAL_INFINITY:
case NORMAL_INFINITY:
        my_memmove((void *)y,(void *)z,sizeof(InternalFPF));
        z->sign ^= operation;
        break;

case SUBNORMAL_SUBNORMAL:
case SUBNORMAL_NORMAL:
case NORMAL_SUBNORMAL:
case NORMAL_NORMAL:
        my_memmove((void *)&locx,(void *)x,sizeof(InternalFPF));
        my_memmove((void *)&locy,(void *)y,sizeof(InternalFPF));

        
        exponent_difference = locx.exp-locy.exp;
        if (exponent_difference == 0)
        {
                if (locx.type == IFPF_IS_SUBNORMAL ||
                  locy.type == IFPF_IS_SUBNORMAL)
                        z->type = IFPF_IS_SUBNORMAL;
                else
                        z->type = IFPF_IS_NORMAL;

                z->sign = locx.sign;
                z->exp= locx.exp;
        }
        else
                if (exponent_difference > 0)
                {
                        StickyShiftRightMant(&locy,
                                 exponent_difference);
                        z->type = locx.type;
                        z->sign = locx.sign;
                        z->exp = locx.exp;
                }
                else    
                {
                        StickyShiftRightMant(&locx,
                                -exponent_difference);
                        z->type = locy.type;
                        z->sign = locy.sign ^ operation;
                        z->exp = locy.exp;
                }

                if (locx.sign ^ locy.sign ^ operation)
                {
                        borrow = 0;
                        for (i=(INTERNAL_FPF_PRECISION-1); i>=0; i--)
                                Sub16Bits(&borrow,
                                        &z->mantissa[i],
                                        locx.mantissa[i],
                                        locy.mantissa[i]);

                        if (borrow)
                        {
                                z->sign = locy.sign ^ operation;
                                borrow = 0;
                                for (i=(INTERNAL_FPF_PRECISION-1); i>=0; i--)
                                {
                                        Sub16Bits(&borrow,
                                                &z->mantissa[i],
                                                0,
                                                z->mantissa[i]);
                                }
                        }
                        else
                        {
                        }

                        if (IsMantissaZero(z->mantissa))
                        {
                                z->type = IFPF_IS_ZERO;
                                z->sign = 0; 
                        }
                        else
                                if (locx.type == IFPF_IS_NORMAL ||
                                         locy.type == IFPF_IS_NORMAL)
                                {
                                        normalize(z);
                                }
                }
                else
                {
                        
                        carry = 0;
                        for (i=(INTERNAL_FPF_PRECISION-1); i>=0; i--)
                        {
                                Add16Bits(&carry,
                                        &z->mantissa[i],
                                        locx.mantissa[i],
                                        locy.mantissa[i]);
                        }

                        if (carry)
                        {
                                z->exp++;
                                carry=0;
                                ShiftMantRight1(&carry,z->mantissa);
                                z->mantissa[0] |= 0x8000;
                                z->type = IFPF_IS_NORMAL;
                        }
                        else
                                if (z->mantissa[0] & 0x8000)
                                        z->type = IFPF_IS_NORMAL;
        }
        break;

case INFINITY_INFINITY:
        SetInternalFPFNaN(z);
        break;

case NAN_NAN:
        choose_nan(x, y, z, 1);
        break;
}

RoundInternalFPF(z);
return;
}


static void MultiplyInternalFPF(InternalFPF *x,
                        InternalFPF *y,
                        InternalFPF *z)
{
int i;
int j;
u16 carry;
u16 extra_bits[INTERNAL_FPF_PRECISION];
InternalFPF locy;       
switch ((x->type * IFPF_TYPE_COUNT) + y->type)
{
case INFINITY_SUBNORMAL:
case INFINITY_NORMAL:
case INFINITY_INFINITY:
case ZERO_ZERO:
case ZERO_SUBNORMAL:
case ZERO_NORMAL:
        my_memmove((void *)x,(void *)z,sizeof(InternalFPF));
        z->sign ^= y->sign;
        break;

case SUBNORMAL_INFINITY:
case NORMAL_INFINITY:
case SUBNORMAL_ZERO:
case NORMAL_ZERO:
        my_memmove((void *)y,(void *)z,sizeof(InternalFPF));
        z->sign ^= x->sign;
        break;

case ZERO_INFINITY:
case INFINITY_ZERO:
        SetInternalFPFNaN(z);
        break;

case NAN_ZERO:
case NAN_SUBNORMAL:
case NAN_NORMAL:
case NAN_INFINITY:
        my_memmove((void *)x,(void *)z,sizeof(InternalFPF));
        break;

case ZERO_NAN:
case SUBNORMAL_NAN:
case NORMAL_NAN:
case INFINITY_NAN:
        my_memmove((void *)y,(void *)z,sizeof(InternalFPF));
        break;


case SUBNORMAL_SUBNORMAL:
case SUBNORMAL_NORMAL:
case NORMAL_SUBNORMAL:
case NORMAL_NORMAL:
        my_memmove((void *)&locy,(void *)y,sizeof(InternalFPF));

        if (IsMantissaZero(x->mantissa) || IsMantissaZero(y->mantissa))
                SetInternalFPFInfinity(z, 0);

        if (x->type == IFPF_IS_SUBNORMAL ||
            y->type == IFPF_IS_SUBNORMAL)
                z->type = IFPF_IS_SUBNORMAL;
        else
                z->type = IFPF_IS_NORMAL;

        z->sign = x->sign ^ y->sign;
        z->exp = x->exp + y->exp ;
        for (i=0; i<INTERNAL_FPF_PRECISION; i++)
        {
                z->mantissa[i] = 0;
                extra_bits[i] = 0;
        }

        for (i=0; i<(INTERNAL_FPF_PRECISION*16); i++)
        {
                carry = 0;
                ShiftMantRight1(&carry, locy.mantissa);
                if (carry)
                {
                        carry = 0;
                        for (j=(INTERNAL_FPF_PRECISION-1); j>=0; j--)
                                Add16Bits(&carry,
                                        &z->mantissa[j],
                                        z->mantissa[j],
                                        x->mantissa[j]);
                }
                else
                {
                        carry = 0;
                }

                ShiftMantRight1(&carry, z->mantissa);
                ShiftMantRight1(&carry, extra_bits);
        }

        while ((z->mantissa[0] & 0x8000) == 0)
        {
                carry = 0;
                ShiftMantLeft1(&carry, extra_bits);
                ShiftMantLeft1(&carry, z->mantissa);
                z->exp--;
        }

        if (IsMantissaZero(extra_bits))
        {
                z->mantissa[INTERNAL_FPF_PRECISION-1] |= 1;
        }
        break;

case NAN_NAN:
        choose_nan(x, y, z, 0);
        break;
}

RoundInternalFPF(z);
return;
}


static void DivideInternalFPF(InternalFPF *x,
                        InternalFPF *y,
                        InternalFPF *z)
{
int i;
int j;
u16 carry;
u16 extra_bits[INTERNAL_FPF_PRECISION];
InternalFPF locx;       

switch ((x->type * IFPF_TYPE_COUNT) + y->type)
{
case ZERO_ZERO:
case INFINITY_INFINITY:
        SetInternalFPFNaN(z);
        break;

case ZERO_SUBNORMAL:
case ZERO_NORMAL:
        if (IsMantissaZero(y->mantissa))
        {
                SetInternalFPFNaN(z);
                break;
        }

case ZERO_INFINITY:
case SUBNORMAL_INFINITY:
case NORMAL_INFINITY:
        SetInternalFPFZero(z, x->sign ^ y->sign);
        break;

case SUBNORMAL_ZERO:
case NORMAL_ZERO:
        if (IsMantissaZero(x->mantissa))
        {
                SetInternalFPFNaN(z);
                break;
        }

case INFINITY_ZERO:
case INFINITY_SUBNORMAL:
case INFINITY_NORMAL:
        SetInternalFPFInfinity(z, 0);
        z->sign = x->sign ^ y->sign;
        break;

case NAN_ZERO:
case NAN_SUBNORMAL:
case NAN_NORMAL:
case NAN_INFINITY:
        my_memmove((void *)x,(void *)z,sizeof(InternalFPF));
        break;

case ZERO_NAN:
case SUBNORMAL_NAN:
case NORMAL_NAN:
case INFINITY_NAN:
        my_memmove((void *)y,(void *)z,sizeof(InternalFPF));
        break;

case SUBNORMAL_SUBNORMAL:
case NORMAL_SUBNORMAL:
case SUBNORMAL_NORMAL:
case NORMAL_NORMAL:
        my_memmove((void *)&locx,(void *)x,sizeof(InternalFPF));

        if (IsMantissaZero(locx.mantissa))
        {
                if (IsMantissaZero(y->mantissa))
                        SetInternalFPFNaN(z);
                else
                        SetInternalFPFZero(z, 0);
                break;
        }
        if (IsMantissaZero(y->mantissa))
        {
                SetInternalFPFInfinity(z, 0);
                break;
        }

        z->type = x->type;
        z->sign = x->sign ^ y->sign;
        z->exp = x->exp - y->exp +
                        ((INTERNAL_FPF_PRECISION * 16 * 2));
        for (i=0; i<INTERNAL_FPF_PRECISION; i++)
        {
                z->mantissa[i] = 0;
                extra_bits[i] = 0;
        }

        while ((z->mantissa[0] & 0x8000) == 0)
        {
                carry = 0;
                ShiftMantLeft1(&carry, locx.mantissa);
                ShiftMantLeft1(&carry, extra_bits);

                if (carry == 0)
                        for (j=0; j<INTERNAL_FPF_PRECISION; j++)
                        {
                                if (y->mantissa[j] > extra_bits[j])
                                {
                                        carry = 0;
                                        goto no_subtract;
                                }
                                if (y->mantissa[j] < extra_bits[j])
                                        break;
                        }
                carry = 0;
                for (j=(INTERNAL_FPF_PRECISION-1); j>=0; j--)
                        Sub16Bits(&carry,
                                &extra_bits[j],
                                extra_bits[j],
                                y->mantissa[j]);
                carry = 1;      
        no_subtract:
                ShiftMantLeft1(&carry, z->mantissa);
                z->exp--;
        }
        break;

case NAN_NAN:
        choose_nan(x, y, z, 0);
        break;
}

RoundInternalFPF(z);
}

static void Int32ToInternalFPF(int32 mylong,
                InternalFPF *dest)
{
int i;          
u16 myword;     
if(mylong<(int32)0)
{       dest->sign=1;
        mylong=(int32)0-mylong;
}
else
        dest->sign=0;
dest->type=IFPF_IS_NORMAL;
for(i=0;i<INTERNAL_FPF_PRECISION;i++)
        dest->mantissa[i]=0;

if(mylong==0)
{       dest->type=IFPF_IS_ZERO;
        dest->exp=0;
        return;
}

dest->exp=32;
myword=(u16)((mylong >> 16) & 0xFFFFL);
dest->mantissa[0]=myword;
myword=(u16)(mylong & 0xFFFFL);
dest->mantissa[1]=myword;
normalize(dest);
return;
}

#if 1
static int InternalFPFToString(char *dest,
                InternalFPF *src)
{
InternalFPF locFPFNum;          
InternalFPF IFPF10;             
InternalFPF IFPFComp;           
int msign;                      
int expcount;                   
int ccount;                     
int i,j,k;                      
u16 carryaccum;                 
u16 mycarry;                    

switch(src->type)
{
        case IFPF_IS_NAN:
                my_memcpy(dest,"NaN",3);
                return(3);

        case IFPF_IS_INFINITY:
                if(src->sign==0)
                        my_memcpy(dest,"+Inf",4);
                else
                        my_memcpy(dest,"-Inf",4);
                return(4);

        case IFPF_IS_ZERO:
                if(src->sign==0)
                        my_memcpy(dest,"+0",2);
                else
                        my_memcpy(dest,"-0",2);
                return(2);
}

my_memcpy((void *)&locFPFNum,(void *)src,sizeof(InternalFPF));

Int32ToInternalFPF((int32)10,&IFPF10);

msign=src->sign;

 
(&locFPFNum)->sign=0;

expcount=0;             


while(1)
{       AddSubInternalFPF(1,&locFPFNum,&IFPF10,&IFPFComp);
        if(IFPFComp.sign==0) break;
        MultiplyInternalFPF(&locFPFNum,&IFPF10,&IFPFComp);
        expcount--;
        my_memcpy((void *)&locFPFNum,(void *)&IFPFComp,sizeof(InternalFPF));
}

while(1)
{
        AddSubInternalFPF(1,&locFPFNum,&IFPF10,&IFPFComp);
        if(IFPFComp.sign!=0) break;
        DivideInternalFPF(&locFPFNum,&IFPF10,&IFPFComp);
        expcount++;
        my_memcpy((void *)&locFPFNum,(void *)&IFPFComp,sizeof(InternalFPF));
}

ccount=1;               
if(msign==0)
        *dest++='+';
else
        *dest++='-';

carryaccum=0;
while(locFPFNum.exp>0)
{
        mycarry=0;
        ShiftMantLeft1(&mycarry,locFPFNum.mantissa);
        carryaccum=(carryaccum<<1);
        if(mycarry) carryaccum++;
        locFPFNum.exp--;
}

while(locFPFNum.exp<0)
{
        mycarry=0;
        ShiftMantRight1(&mycarry,locFPFNum.mantissa);
        locFPFNum.exp++;
}

for(i=0;i<6;i++)
        if(i==1)
        {       
                *dest++='.';
                ccount++;
        }
        else
        {       
                *dest++=('0'+carryaccum);
                ccount++;

                carryaccum=0;
                my_memcpy((void *)&IFPF10,
                        (void *)&locFPFNum,
                        sizeof(InternalFPF));

                
                for(j=0;j<9;j++)
                {
                        mycarry=0;
                        for(k=(INTERNAL_FPF_PRECISION-1);k>=0;k--)
                                Add16Bits(&mycarry,&(IFPFComp.mantissa[k]),
                                        locFPFNum.mantissa[k],
                                        IFPF10.mantissa[k]);
                        carryaccum+=mycarry ? 1 : 0;
                        my_memcpy((void *)&locFPFNum,
                                (void *)&IFPFComp,
                                sizeof(InternalFPF));
                }
        }

*dest++='E';

if (expcount < 0) {
     *dest++ = '-';
     expcount =- expcount;
}
else *dest++ = ' ';

*dest++ = (char)(expcount + '0');
*dest++ = 0;

ccount += 3;
return(ccount);

}

#endif



static 
void* AllocateMemory ( unsigned long n, int* p )
{
  *p = 0;
  void* r = (void*) (*serviceFn)(2,n);
  return r;
}
static 
void FreeMemory ( void* p, int* zz )
{
  *zz = 0;
  
}



static 
void DoEmFloat(void)
{
EmFloatStruct *locemfloatstruct;        
InternalFPF *abase;             
InternalFPF *bbase;             
InternalFPF *cbase;             
ulong tickcount;                
char *errorcontext;             
int systemerror;                
ulong loops;                    

EmFloatStruct global_emfloatstruct;
 global_emfloatstruct.adjust = 0;
 global_emfloatstruct.request_secs = 0;
 global_emfloatstruct.arraysize = 100;
 global_emfloatstruct.loops = 1;
 global_emfloatstruct.emflops = 0.0;
locemfloatstruct=&global_emfloatstruct;

errorcontext="CPU:Floating Emulation";


abase=(InternalFPF *)AllocateMemory(locemfloatstruct->arraysize*sizeof(InternalFPF),
		&systemerror);

bbase=(InternalFPF *)AllocateMemory(locemfloatstruct->arraysize*sizeof(InternalFPF),
		&systemerror);

cbase=(InternalFPF *)AllocateMemory(locemfloatstruct->arraysize*sizeof(InternalFPF),
		&systemerror);

SetupCPUEmFloatArrays(abase,bbase,cbase,locemfloatstruct->arraysize);

 loops=100;
	       tickcount=DoEmFloatIteration(abase,bbase,cbase,
			locemfloatstruct->arraysize,
			loops);

FreeMemory((void *)abase,&systemerror);
FreeMemory((void *)bbase,&systemerror);
FreeMemory((void *)cbase,&systemerror);

return;
}

void entry ( HWord(*f)(HWord,HWord) )
{
  serviceFn = f;
  vexxx_printf("starting\n");
  DoEmFloat();
  (*serviceFn)(0,0);
}
