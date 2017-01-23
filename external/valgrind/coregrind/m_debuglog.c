

/*
   This file is part of Valgrind, a dynamic binary instrumentation
   framework.

   Copyright (C) 2000-2013 Julian Seward 
      jseward@acm.org

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307, USA.

   The GNU General Public License is contained in the file COPYING.
*/





#include "pub_core_basics.h"     
#include "pub_core_vkiscnums.h"  
#include "pub_core_debuglog.h"   
#include "pub_core_clreq.h"      

static Bool clo_xml;

void VG_(debugLog_setXml)(Bool xml)
{
   clo_xml = xml;
}



#if defined(VGP_x86_linux)

static UInt local_sys_write_stderr ( const HChar* buf, Int n )
{
   Int result;

   __asm__ volatile (
      "pushl %%ebx\n"
      "movl  $"VG_STRINGIFY(__NR_write)", %%eax\n" 
      "movl  $2, %%ebx\n"       
      "int   $0x80\n"           
      "popl %%ebx\n"
      :     "=a" (result)
      :     "c" (buf), "d" (n)
      :  "edi", "memory", "cc"
   );

   return result >= 0 ? result : -1;
}

static UInt local_sys_getpid ( void )
{
   UInt __res;
   __asm__ volatile (
      "movl $"VG_STRINGIFY(__NR_getpid)", %%eax\n" 
      "int  $0x80\n"       
      "movl %%eax, %0\n"   
      : "=mr" (__res)
      :
      : "eax" );
   return __res;
}

#elif defined(VGP_amd64_linux)

__attribute__((noinline))
static UInt local_sys_write_stderr ( const HChar* buf, Int n )
{
   volatile Long block[2];
   block[0] = (Long)buf;
   block[1] = n;
   __asm__ volatile (
      "subq  $256, %%rsp\n"     
      "pushq %%r15\n"           
      "movq  %0, %%r15\n"       
      "pushq %%r15\n"           
      "movq  $"VG_STRINGIFY(__NR_write)", %%rax\n" 
      "movq  $2, %%rdi\n"       
      "movq  0(%%r15), %%rsi\n" 
      "movq  8(%%r15), %%rdx\n" 
      "syscall\n"               
      "popq  %%r15\n"           
      "movq  %%rax, 0(%%r15)\n" 
      "popq  %%r15\n"           
      "addq  $256, %%rsp\n"     
      : 
      :     "r" (block)
      :  "rax", "rdi", "rsi", "rdx", "memory", "cc"
   );
   if (block[0] < 0) 
      block[0] = -1;
   return (UInt)block[0];
}

static UInt local_sys_getpid ( void )
{
   UInt __res;
   __asm__ volatile (
      "movq $"VG_STRINGIFY(__NR_getpid)", %%rax\n" 
      "syscall\n"          
      "movl %%eax, %0\n"   
      : "=mr" (__res)
      :
      : "rax" );
   return __res;
}

#elif defined(VGP_ppc32_linux)

static UInt local_sys_write_stderr ( const HChar* buf, Int n )
{
   volatile Int block[2];
   block[0] = (Int)buf;
   block[1] = n;
   __asm__ volatile (
      "addi 1,1,-256\n\t"
      "mr   5,%0\n\t"     
      "stw  5,0(1)\n\t"   
      "li   0,"VG_STRINGIFY(__NR_write)"\n\t" 
      "li   3,2\n\t"      
      "lwz  4,0(5)\n\t"   
      "lwz  5,4(5)\n\t"   
      "sc\n\t"            
      "lwz  5,0(1)\n\t"
      "addi 1,1,256\n\t"
      "stw  3,0(5)\n"     
      :
      : "b" (block)
      : "cc","memory","cr0","ctr",
        "r0","r2","r3","r4","r5","r6","r7","r8","r9","r10","r11","r12"
   );
   if (block[0] < 0)
      block[0] = -1;
   return (UInt)block[0];
}

static UInt local_sys_getpid ( void )
{
   register UInt __res __asm__ ("r3");
   __asm__ volatile ( 
      "li 0, %1\n\t"
      "sc"
      : "=&r" (__res)
      : "i" (__NR_getpid)
      : "cc","memory","cr0","ctr",
        "r0","r2","r4","r5","r6","r7","r8","r9","r10","r11","r12"
   );
   return __res;
}

#elif defined(VGP_ppc64be_linux) || defined(VGP_ppc64le_linux)

static UInt local_sys_write_stderr ( const HChar* buf, Int n )
{
   volatile Long block[2];
   block[0] = (Long)buf;
   block[1] = (Long)n;
   __asm__ volatile (
      "addi 1,1,-256\n\t"
      "mr   5,%0\n\t"     
      "std  5,0(1)\n\t"   
      "li   0,"VG_STRINGIFY(__NR_write)"\n\t" 
      "li   3,2\n\t"      
      "ld   4,0(5)\n\t"   
      "ld   5,8(5)\n\t"   
      "sc\n\t"            
      "ld   5,0(1)\n\t"
      "addi 1,1,256\n\t"
      "std  3,0(5)\n"     
      :
      : "b" (block)
      : "cc","memory","cr0","ctr",
        "r0","r2","r3","r4","r5","r6","r7","r8","r9","r10","r11","r12"
   );
   if (block[0] < 0)
      block[0] = -1;
   return (UInt)(Int)block[0];
}

static UInt local_sys_getpid ( void )
{
   register ULong __res __asm__ ("r3");
   __asm__ volatile ( 
      "li 0, %1\n\t"
      "sc"
      : "=&r" (__res)
      : "i" (__NR_getpid)
      : "cc","memory","cr0","ctr",
        "r0","r2","r4","r5","r6","r7","r8","r9","r10","r11","r12"
   );
   return (UInt)__res;
}

#elif defined(VGP_arm_linux)

static UInt local_sys_write_stderr ( const HChar* buf, Int n )
{
   volatile Int block[2];
   block[0] = (Int)buf;
   block[1] = n;
   __asm__ volatile (
      "mov  r0, #2\n\t"        
      "ldr  r1, [%0]\n\t"      
      "ldr  r2, [%0, #4]\n\t"  
      "push {r6,r7}\n\t"
      "mov  r7, #"VG_STRINGIFY(__NR_write)"\n\t"
      "svc  0x0\n"          
      "pop  {r6,r7}\n\t"
      "str  r0, [%0]\n\t"
      :
      : "r" (block)
      : "r0","r1","r2"
   );
   if (block[0] < 0)
      block[0] = -1;
   return (UInt)block[0];
}

static UInt local_sys_getpid ( void )
{
   UInt __res;
   __asm__ volatile (
      "push {r6,r7}\n\t"
      "mov  r7, #"VG_STRINGIFY(__NR_getpid)"\n\t"
      "svc  0x0\n\t"      
      "pop  {r6,r7}\n\t"
      "mov  %0, r0\n\t"
      : "=r" (__res)
      :
      : "r0" );
   return __res;
}

#elif defined(VGP_arm64_linux)

static UInt local_sys_write_stderr ( const HChar* buf, Int n )
{
   volatile ULong block[2];
   block[0] = (ULong)buf;
   block[1] = (ULong)n;
   __asm__ volatile (
      "mov  x0, #2\n\t"        
      "ldr  x1, [%0]\n\t"      
      "ldr  x2, [%0, #8]\n\t"  
      "mov  x8, #"VG_STRINGIFY(__NR_write)"\n\t"
      "svc  0x0\n"          
      "str  x0, [%0]\n\t"
      :
      : "r" (block)
      : "x0","x1","x2","x7"
   );
   if (block[0] < 0)
      block[0] = -1;
   return (UInt)block[0];
}

static UInt local_sys_getpid ( void )
{
   UInt __res;
   __asm__ volatile (
      "mov  x8, #"VG_STRINGIFY(__NR_getpid)"\n"
      "svc  0x0\n"      
      "mov  %0, x0\n"
      : "=r" (__res)
      :
      : "x0", "x8" );
   return (UInt)__res;
}

#elif defined(VGP_x86_darwin)

__attribute__((noinline))
static UInt local_sys_write_stderr ( const HChar* buf, Int n )
{
   UInt __res;
   __asm__ volatile (
      "movl  %2, %%eax\n"    
      "pushl %%eax\n"
      "movl  %1, %%eax\n"    
      "pushl %%eax\n"
      "movl  $2, %%eax\n"    
      "pushl %%eax\n"
      "movl  $"VG_STRINGIFY(VG_DARWIN_SYSNO_INDEX(__NR_write_nocancel))
             ", %%eax\n"
      "pushl %%eax\n"        
      "int   $0x80\n"        
      "jnc   1f\n"           
      "movl  $-1, %%eax\n"   
      "1: "
      "movl  %%eax, %0\n"    
      "addl  $16, %%esp\n"   
      : "=mr" (__res)
      : "g" (buf), "g" (n)
      : "eax", "edx", "cc"
   );
   return __res;
}

static UInt local_sys_getpid ( void )
{
   UInt __res;
   __asm__ volatile (
      "movl $"VG_STRINGIFY(VG_DARWIN_SYSNO_INDEX(__NR_getpid))", %%eax\n"
      "int  $0x80\n"       
      "movl %%eax, %0\n"   
      : "=mr" (__res)
      :
      : "eax", "cc" );
   return __res;
}

#elif defined(VGP_amd64_darwin)

__attribute__((noinline))
static UInt local_sys_write_stderr ( const HChar* buf, Int n )
{
   UInt __res;
   __asm__ volatile (
      "movq  $2, %%rdi\n"    
      "movq  %1, %%rsi\n"    
      "movl  %2, %%edx\n"    
      "movl  $"VG_STRINGIFY(VG_DARWIN_SYSNO_FOR_KERNEL(__NR_write_nocancel))
             ", %%eax\n"
      "syscall\n"            
      "jnc   1f\n"           
      "movq  $-1, %%rax\n"   
      "1: "
      "movl  %%eax, %0\n"    
      : "=mr" (__res)
      : "g" (buf), "g" (n)
      : "rdi", "rsi", "rdx", "rcx", "rax", "cc" );
   return __res;
}

static UInt local_sys_getpid ( void )
{
   UInt __res;
   __asm__ volatile (
      "movl $"VG_STRINGIFY(VG_DARWIN_SYSNO_FOR_KERNEL(__NR_getpid))", %%eax\n"
      "syscall\n"          
      "movl %%eax, %0\n"   
      : "=mr" (__res)
      :
      : "rax", "rcx", "cc" );
   return __res;
}

#elif defined(VGP_s390x_linux)

static UInt local_sys_write_stderr ( const HChar* buf, Int n )
{
   register Int          r2     asm("2") = 2;      
   register const HChar* r3     asm("3") = buf;
   register ULong        r4     asm("4") = n;
   register ULong        r2_res asm("2");
   ULong __res;

   __asm__ __volatile__ (
      "svc %b1\n"
      : "=d" (r2_res)
      : "i" (__NR_write),
        "0" (r2),
        "d" (r3),
        "d" (r4)
      : "cc", "memory");
   __res = r2_res;

   if (__res >= (ULong)(-125))
      __res = -1;
   return (UInt)(__res);
}

static UInt local_sys_getpid ( void )
{
   register ULong r2 asm("2");
   ULong __res;

   __asm__ __volatile__ (
      "svc %b1\n"
      : "=d" (r2)
      : "i" (__NR_getpid)
      : "cc", "memory");
   __res = r2;

   if (__res >= (ULong)(-125))
      __res = -1;
   return (UInt)(__res);
}

#elif defined(VGP_mips32_linux)

static UInt local_sys_write_stderr ( const HChar* buf, Int n )
{
   volatile Int block[2];
   block[0] = (Int)buf;
   block[1] = n;
   __asm__ volatile (
      "li   $4, 2\n\t"        
      "lw   $5, 0(%0)\n\t"    
      "lw   $6, 4(%0)\n\t"    
      "move $7, $0\n\t"
      "li   $2, %1\n\t"       
      "syscall\n\t"           
      "nop\n\t"
      :
      : "r" (block), "n" (__NR_write)
      : "2", "4", "5", "6", "7"
   );
   if (block[0] < 0)
      block[0] = -1;
   return (UInt)block[0];
}

static UInt local_sys_getpid ( void )
{
   UInt __res;
   __asm__ volatile (
      "li   $2, %1\n\t"       
      "syscall\n\t"      
      "nop\n\t"
      "move  %0, $2\n"
      : "=r" (__res)
      : "n" (__NR_getpid)
      : "$2" );
   return __res;
}

#elif defined(VGP_mips64_linux)

static UInt local_sys_write_stderr ( const HChar* buf, Int n )
{
   volatile Long block[2];
   block[0] = (Long)buf;
   block[1] = n;
   __asm__ volatile (
      "li   $4, 2\n\t"      
      "ld   $5, 0(%0)\n\t"  
      "ld   $6, 8(%0)\n\t"  
      "move $7, $0\n\t"
      "li   $2, %1\n\t"     
      "\tsyscall\n"
      "\tnop\n"
      : 
      :   "r" (block), "n" (__NR_write)
      : "2", "4", "5", "6", "7"
   );
   if (block[0] < 0)
      block[0] = -1;
   return (UInt)(Int)block[0];
}
 
static UInt local_sys_getpid ( void )
{
   ULong __res;
   __asm__ volatile (
      "li   $2, %1\n\t"  
      "syscall\n\t"      
      "nop\n\t"
      "move  %0, $2\n"
      : "=r" (__res)
      : "n" (__NR_getpid)
      : "$2" );
   return (UInt)(__res);
}

#elif defined(VGP_tilegx_linux)

static UInt local_sys_write_stderr ( const HChar* buf, Int n )
{
   volatile Long block[2];
   block[0] = (Long)buf;
   block[1] = n;
   Long __res = 0;
   __asm__ volatile (
      "movei  r0,  2    \n\t"    
      "move   r1,  %1   \n\t"    
      "move   r2,  %2   \n\t"    
      "move   r3,  zero \n\t"
      "moveli r10, %3   \n\t"    
      "swint1           \n\t"    
      "nop              \n\t"
      "move   %0, r0    \n\t"    
      : "=r"(__res)
      : "r" (block[0]), "r"(block[1]), "n" (__NR_write)
      : "r0", "r1", "r2", "r3", "r4", "r5");
   if (__res < 0)
      __res = -1;
   return (UInt)__res;
}

static UInt local_sys_getpid ( void )
{
   UInt __res, __err;
   __res = 0;
   __err = 0;
   __asm__ volatile (
      "moveli r10, %2\n\t"    
      "swint1\n\t"            
      "nop\n\t"
      "move  %0, r0\n"
      "move  %1, r1\n"
      : "=r" (__res), "=r"(__err)
      : "n" (__NR_getpid)
      : "r0", "r1", "r2", "r3", "r4",
        "r5", "r6", "r7", "r8", "r9",
        "r10", "r11", "r12", "r13", "r14",
        "r15", "r16", "r17", "r18", "r19",
        "r20", "r21", "r22", "r23", "r24",
        "r25", "r26", "r27", "r28", "r29");
  return __res;
}

#else
# error Unknown platform
#endif



static Int local_strlen ( const HChar* str )
{
   Int i = 0;
   while (str[i] != 0) i++;
   return i;
}

static HChar local_toupper ( HChar c )
{
   if (c >= 'a' && c <= 'z')
      return c + ('A' - 'a'); 
   else
      return c;
}

static void emit ( const HChar* buf, Int n )
{
   if (n >= 1)
      (void)local_sys_write_stderr(buf, n);
}



/* -----------------------------------------------
   Distantly derived from:

      vprintf replacement for Checker.
      Copyright 1993, 1994, 1995 Tristan Gingold
      Written September 1993 Tristan Gingold
      Tristan Gingold, 8 rue Parmentier, F-91120 PALAISEAU, FRANCE

   (Checker itself was GPL'd.)
   ----------------------------------------------- */

#define VG_MSG_SIGNED    1 
#define VG_MSG_ZJUSTIFY  2 
#define VG_MSG_LJUSTIFY  4 
#define VG_MSG_PAREN     8 
#define VG_MSG_COMMA    16 
#define VG_MSG_ALTFORMAT 32 

static 
UInt myvprintf_str ( void(*send)(HChar,void*),
                     void* send_arg2,
                     Int flags, 
                     Int width, 
                     const HChar* str, 
                     Bool capitalise )
{
#  define MAYBE_TOUPPER(ch) (capitalise ? local_toupper(ch) : (ch))
   UInt ret = 0;
   Int i, extra;
   Int len = local_strlen(str);

   if (width == 0) {
      ret += len;
      for (i = 0; i < len; i++)
          send(MAYBE_TOUPPER(str[i]), send_arg2);
      return ret;
   }

   if (len > width) {
      ret += width;
      for (i = 0; i < width; i++)
         send(MAYBE_TOUPPER(str[i]), send_arg2);
      return ret;
   }

   extra = width - len;
   if (! (flags & VG_MSG_LJUSTIFY)) {
      ret += extra;
      for (i = 0; i < extra; i++)
         send(' ', send_arg2);
   }
   ret += len;
   for (i = 0; i < len; i++)
      send(MAYBE_TOUPPER(str[i]), send_arg2);
   if (flags & VG_MSG_LJUSTIFY) {
      ret += extra;
      for (i = 0; i < extra; i++)
         send(' ', send_arg2);
   }

#  undef MAYBE_TOUPPER
   return ret;
}


static 
UInt myvprintf_str_XML_simplistic ( void(*send)(HChar,void*),
                                    void* send_arg2,
                                    const HChar* str )
{
   UInt   ret = 0;
   Int    i;
   Int    len = local_strlen(str);
   const HChar* alt;

   for (i = 0; i < len; i++) {
      switch (str[i]) {
         case '&': alt = "&amp;"; break;
         case '<': alt = "&lt;"; break;
         case '>': alt = "&gt;"; break;
         default:  alt = NULL;
      }

      if (alt) {
         while (*alt) {
            send(*alt, send_arg2);
            ret++;
            alt++;
         }
      } else {
         send(str[i], send_arg2);
         ret++;
      }
   }

   return ret;
}


static 
UInt myvprintf_int64 ( void(*send)(HChar,void*), 
                       void* send_arg2,
                       Int flags, 
                       Int base, 
                       Int width, 
                       Bool capitalised,
                       ULong p )
{
   HChar  buf[width + 1 > 90 ? width + 1 : 90];
   Int    ind = 0;
   Int    i, nc = 0;
   Bool   neg = False;
   const HChar* digits = capitalised ? "0123456789ABCDEF" : "0123456789abcdef";
   UInt   ret = 0;

   if (base < 2 || base > 16)
      return ret;
 
   if ((flags & VG_MSG_SIGNED) && (Long)p < 0) {
      p   = - (Long)p;
      neg = True;
   }

   if (p == 0)
      buf[ind++] = '0';
   else {
      while (p > 0) {
         if (flags & VG_MSG_COMMA && 10 == base &&
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
         buf[ind] = (flags & VG_MSG_ZJUSTIFY) ? '0': ' ';
      }
   }

   
   ret += ind;
   for (i = ind -1; i >= 0; i--) {
      send(buf[i], send_arg2);
   }
   if (width > 0 && (flags & VG_MSG_LJUSTIFY)) {
      for(; ind < width; ind++) {
         ret++;
         
         send(' ', send_arg2);
      }
   }
   return ret;
}


UInt
VG_(debugLog_vprintf) ( 
   void(*send)(HChar,void*), 
   void* send_arg2,
   const HChar* format, 
   va_list vargs
)
{
   UInt ret = 0;
   Int  i;
   Int  flags;
   Int  width, precision;
   Int  n_ls = 0;
   Bool is_long, is_sizet, caps;


   for (i = 0; format[i] != 0; i++) {
      if (format[i] != '%') {
         send(format[i], send_arg2);
         ret++;
         continue;
      }
      i++;
      
      if (format[i] == 0)
         break;
      if (format[i] == '%') {
         
         send('%', send_arg2);
         ret++;
         continue;
      }
      flags = 0;
      n_ls  = 0;
      width = 0; 
      precision = -1;  
      while (1) {
         switch (format[i]) {
         case '(':
            flags |= VG_MSG_PAREN;
            break;
         case ',':
         case '\'':
            
            flags |= VG_MSG_COMMA;
            break;
         case '-':
            
            flags |= VG_MSG_LJUSTIFY;
            break;
         case '0':
            
            flags |= VG_MSG_ZJUSTIFY;
            break;
         case '#':
            
            flags |= VG_MSG_ALTFORMAT;
            break;
         default:
            goto parse_fieldwidth;
         }
         i++;
      }
     parse_fieldwidth:
      
      if (format[i] == '*') {
         width = va_arg(vargs, Int);
         ++i;
      } else {
         while (format[i] >= '0' && format[i] <= '9') {
            width *= 10;
            width += format[i++] - '0';
         }
      }
      if (format[i] == '.') {
         ++i;
         if (format[i] == '*') {
            precision = va_arg(vargs, Int);
            ++i;
         } else {
            precision = 0;
            while (format[i] >= '0' && format[i] <= '9') {
               precision *= 10;
               precision += format[i++] - '0';
            }
         }
      }

      is_sizet = False;
      if (format[i] == 'z') {
         is_sizet = True;
         ++i;
      } else {
         while (format[i] == 'l') {
            i++;
            n_ls++;
         }
      }

      
      
      
      if      (0 == n_ls) { is_long = False; }
      else if (1 == n_ls) { is_long = ( sizeof(void*) == sizeof(Long) ); }
      else                { is_long = True; }

      switch (format[i]) {
         case 'o': 
            if (flags & VG_MSG_ALTFORMAT) {
               ret += 2;
               send('0',send_arg2);
            }
            if (is_sizet)
               ret += myvprintf_int64(send, send_arg2, flags, 8, width, False,
                                      (ULong)(va_arg (vargs, SizeT)));
            else if (is_long)
               ret += myvprintf_int64(send, send_arg2, flags, 8, width, False,
                                      (ULong)(va_arg (vargs, ULong)));
            else
               ret += myvprintf_int64(send, send_arg2, flags, 8, width, False,
                                      (ULong)(va_arg (vargs, UInt)));
            break;
         case 'd': 
            flags |= VG_MSG_SIGNED;
            if (is_long)
               ret += myvprintf_int64(send, send_arg2, flags, 10, width, False,
                                      (ULong)(va_arg (vargs, Long)));
            else
               ret += myvprintf_int64(send, send_arg2, flags, 10, width, False,
                                      (ULong)(va_arg (vargs, Int)));
            break;
         case 'u': 
            if (is_sizet)
               ret += myvprintf_int64(send, send_arg2, flags, 10, width, False,
                                      (ULong)(va_arg (vargs, SizeT)));
            else if (is_long)
               ret += myvprintf_int64(send, send_arg2, flags, 10, width, False,
                                      (ULong)(va_arg (vargs, ULong)));
            else
               ret += myvprintf_int64(send, send_arg2, flags, 10, width, False,
                                      (ULong)(va_arg (vargs, UInt)));
            break;
         case 'p':
            if (format[i+1] == 'S') {
               i++;
               
               
               const HChar *str = va_arg (vargs, HChar *);
               if (str == NULL)
                  str = "(null)";
               ret += myvprintf_str_XML_simplistic(send, send_arg2, str);
            } else if (format[i+1] == 's') {
               i++;
               
               const HChar *str = va_arg (vargs, HChar *);
               if (str == NULL)
                  str = "(null)";
               if (clo_xml)
                  ret += myvprintf_str_XML_simplistic(send, send_arg2, str);
               else
                  ret += myvprintf_str(send, send_arg2, flags, width, str,
                                       False);
            } else {
               
               ret += 2;
               send('0',send_arg2);
               send('x',send_arg2);
               ret += myvprintf_int64(send, send_arg2, flags, 16, width, True,
                                      (ULong)((UWord)va_arg (vargs, void *)));
            }
            break;
         case 'x': 
         case 'X': 
            caps = toBool(format[i] == 'X');
            if (flags & VG_MSG_ALTFORMAT) {
               ret += 2;
               send('0',send_arg2);
               send('x',send_arg2);
            }
            if (is_sizet)
               ret += myvprintf_int64(send, send_arg2, flags, 16, width, False,
                                      (ULong)(va_arg (vargs, SizeT)));
            else if (is_long)
               ret += myvprintf_int64(send, send_arg2, flags, 16, width, caps,
                                      (ULong)(va_arg (vargs, ULong)));
            else
               ret += myvprintf_int64(send, send_arg2, flags, 16, width, caps,
                                      (ULong)(va_arg (vargs, UInt)));
            break;
         case 'c': 
            ret++;
            send(va_arg (vargs, int), send_arg2);
            break;
         case 's': case 'S': { 
            const HChar *str = va_arg (vargs, HChar *);
            if (str == NULL) str = "(null)";
            ret += myvprintf_str(send, send_arg2, 
                                 flags, width, str, format[i]=='S');
            break;
         }
         case 'f': {
            Double val = va_arg (vargs, Double);
            Bool is_negative = False;
            Int cnt;

            if (val < 0.0) {
               is_negative = True;
               val = - val;
            }
            if (val > (Double)(~0ULL)) {
               if (width == 0) width = 6;    
               for (cnt = 0; cnt < width; ++cnt)
                  send('*', send_arg2);
               ret += width;
               break;
            }
            ULong ipval = val;
            Double frac = val - ipval;

            if (precision == -1) precision = 6;   

            
            if (precision > 10) precision = 10;

            
            ULong factor = 1;
            for (cnt = 0; cnt < precision; ++cnt)
               factor *= 10;
            ULong frval = frac * factor;
            if ((frac * factor - frval) > 0.5)    
               frval += 1;
            
            if (frval == factor)
               ipval += 1;
            frval %= factor;

            

            
            UInt ipwidth, num_digit = 1;   
            ULong x, old_x = 0;
            for (x = 10; ; old_x = x, x *= 10, ++num_digit) {
               if (x <= old_x) break;    
               if (ipval < x) break;
            }
            ipwidth = num_digit;   
            if (is_negative) ++num_digit;
            if (precision != 0)
               num_digit += 1 + precision;

            

            
            if (num_digit < width && (flags & VG_MSG_LJUSTIFY) == 0) {
               for (cnt = 0; cnt < width - num_digit; ++cnt)
                  send(' ', send_arg2);
               ret += width - num_digit;
            }
            
            if (is_negative) {
               send('-', send_arg2);
               ret += 1;
            }
            
            ret += myvprintf_int64(send, send_arg2, 0, 10, ipwidth, False,
                                   ipval);
            
            if (precision != 0) {
               send('.', send_arg2);
               ret += 1;

               ret += myvprintf_int64(send, send_arg2, VG_MSG_ZJUSTIFY, 10,
                                      precision, False, frval);
            }
            
            if (num_digit < width && (flags & VG_MSG_LJUSTIFY) != 0) {
               for (cnt = 0; cnt < width - num_digit; ++cnt)
                  send(' ', send_arg2);
               ret += width - num_digit;
            }
            break;
         }

         default:
            break;
      }
   }
   return ret;
}




static Int loglevel = 0;

void VG_(debugLog_startup) ( Int level, const HChar* who )
{
   if (level < 0)  level = 0;
   if (level > 10) level = 10;
   loglevel = level;
   VG_(debugLog)(1, "debuglog", 
                 "DebugLog system started by %s, "
                 "level %d logging requested\n", 
                 who, loglevel);
}

Int VG_(debugLog_getLevel) ( void )
{
   return loglevel;
}



typedef 
   struct {
      HChar buf[100];
      Int   n;
   } 
   printf_buf;

static void add_to_buf ( HChar c, void* p )
{
   printf_buf* buf = (printf_buf*)p;

   if (buf->n >= 100-10  ) {
      emit( buf->buf, local_strlen(buf->buf) );
      buf->n = 0;
      buf->buf[buf->n] = 0;      
   }
   buf->buf[buf->n++] = c;
   buf->buf[buf->n] = 0;
}

void VG_(debugLog) ( Int level, const HChar* modulename,
                                const HChar* format, ... )
{
   UInt pid;
   Int indent, depth, i;
   va_list vargs;
   printf_buf buf;

   if (level > loglevel)
      return;

   indent = 2*level - 1;
   if (indent < 1) indent = 1;

   buf.n = 0;
   buf.buf[0] = 0;
   pid = local_sys_getpid();

   
   
   depth = RUNNING_ON_VALGRIND;
   for (i = 0; i < depth; i++) {
      (void)myvprintf_str ( add_to_buf, &buf, 0, 1, ">", False );
   }
   
   (void)myvprintf_str ( add_to_buf, &buf, 0, 2, "--", False );
   (void)myvprintf_int64 ( add_to_buf, &buf, 0, 10, 1, False, (ULong)pid );
   (void)myvprintf_str ( add_to_buf, &buf, 0, 1, ":", False );
   (void)myvprintf_int64 ( add_to_buf, &buf, 0, 10, 1, False, (ULong)level );
   (void)myvprintf_str ( add_to_buf, &buf, 0, 1, ":", False );
   (void)myvprintf_str ( add_to_buf, &buf, 0, 8, modulename, False );
   (void)myvprintf_str ( add_to_buf, &buf, 0, indent, "", False );

   va_start(vargs,format);
   
   (void) VG_(debugLog_vprintf) ( add_to_buf, &buf, format, vargs );

   if (buf.n > 0) {
      emit( buf.buf, local_strlen(buf.buf) );
   }

   va_end(vargs);
}



