

/*
   This file is part of Valgrind, a dynamic binary instrumentation
   framework.

   Copyright (C) 2004-2013 OpenWorks LLP
      info@open-works.net

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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   The GNU General Public License is contained in the file COPYING.

   Neither the names of the U.S. Department of Energy nor the
   University of California nor the names of its contributors may be
   used to endorse or promote products derived from this software
   without prior written permission.
*/

#include "libvex_basictypes.h"
#include "libvex.h"

#include "main_globals.h"
#include "main_util.h"




#define N_TEMPORARY_BYTES 5000000

static HChar  temporary[N_TEMPORARY_BYTES] __attribute__((aligned(REQ_ALIGN)));
static HChar* temporary_first = &temporary[0];
static HChar* temporary_curr  = &temporary[0];
static HChar* temporary_last  = &temporary[N_TEMPORARY_BYTES-1];

static ULong  temporary_bytes_allocd_TOT = 0;

#define N_PERMANENT_BYTES 10000

static HChar  permanent[N_PERMANENT_BYTES] __attribute__((aligned(REQ_ALIGN)));
static HChar* permanent_first = &permanent[0];
static HChar* permanent_curr  = &permanent[0];
static HChar* permanent_last  = &permanent[N_PERMANENT_BYTES-1];

HChar* private_LibVEX_alloc_first = &temporary[0];
HChar* private_LibVEX_alloc_curr  = &temporary[0];
HChar* private_LibVEX_alloc_last  = &temporary[N_TEMPORARY_BYTES-1];


static VexAllocMode mode = VexAllocModeTEMP;

void vexAllocSanityCheck ( void )
{
   vassert(temporary_first == &temporary[0]);
   vassert(temporary_last  == &temporary[N_TEMPORARY_BYTES-1]);
   vassert(permanent_first == &permanent[0]);
   vassert(permanent_last  == &permanent[N_PERMANENT_BYTES-1]);
   vassert(temporary_first <= temporary_curr);
   vassert(temporary_curr  <= temporary_last);
   vassert(permanent_first <= permanent_curr);
   vassert(permanent_curr  <= permanent_last);
   vassert(private_LibVEX_alloc_first <= private_LibVEX_alloc_curr);
   vassert(private_LibVEX_alloc_curr  <= private_LibVEX_alloc_last);
   if (mode == VexAllocModeTEMP){
      vassert(private_LibVEX_alloc_first == temporary_first);
      vassert(private_LibVEX_alloc_last  == temporary_last);
   } 
   else
   if (mode == VexAllocModePERM) {
      vassert(private_LibVEX_alloc_first == permanent_first);
      vassert(private_LibVEX_alloc_last  == permanent_last);
   }
   else 
      vassert(0);

#  define IS_WORD_ALIGNED(p)   (0 == (((HWord)p) & (sizeof(HWord)-1)))
   vassert(sizeof(HWord) == 4 || sizeof(HWord) == 8);
   vassert(IS_WORD_ALIGNED(temporary_first));
   vassert(IS_WORD_ALIGNED(temporary_curr));
   vassert(IS_WORD_ALIGNED(temporary_last+1));
   vassert(IS_WORD_ALIGNED(permanent_first));
   vassert(IS_WORD_ALIGNED(permanent_curr));
   vassert(IS_WORD_ALIGNED(permanent_last+1));
   vassert(IS_WORD_ALIGNED(private_LibVEX_alloc_first));
   vassert(IS_WORD_ALIGNED(private_LibVEX_alloc_curr));
   vassert(IS_WORD_ALIGNED(private_LibVEX_alloc_last+1));
#  undef IS_WORD_ALIGNED
}


void vexSetAllocMode ( VexAllocMode m )
{
   vexAllocSanityCheck();

   
   if (mode == VexAllocModeTEMP){
      temporary_curr = private_LibVEX_alloc_curr;
   } 
   else
   if (mode == VexAllocModePERM) {
      permanent_curr = private_LibVEX_alloc_curr;
   }
   else 
      vassert(0);

   
   vexAllocSanityCheck();

   if (m == VexAllocModeTEMP){
      private_LibVEX_alloc_first = temporary_first;
      private_LibVEX_alloc_curr  = temporary_curr;
      private_LibVEX_alloc_last  = temporary_last;
   } 
   else
   if (m == VexAllocModePERM) {
      private_LibVEX_alloc_first = permanent_first;
      private_LibVEX_alloc_curr  = permanent_curr;
      private_LibVEX_alloc_last  = permanent_last;
   }
   else 
      vassert(0);

   mode = m;
}

VexAllocMode vexGetAllocMode ( void )
{
   return mode;
}

__attribute__((noreturn))
void private_LibVEX_alloc_OOM(void)
{
   const HChar* pool = "???";
   if (private_LibVEX_alloc_first == &temporary[0]) pool = "TEMP";
   if (private_LibVEX_alloc_first == &permanent[0]) pool = "PERM";
   vex_printf("VEX temporary storage exhausted.\n");
   vex_printf("Pool = %s,  start %p curr %p end %p (size %lld)\n",
              pool, 
              private_LibVEX_alloc_first,
              private_LibVEX_alloc_curr,
              private_LibVEX_alloc_last,
              (Long)(private_LibVEX_alloc_last + 1 - private_LibVEX_alloc_first));
   vpanic("VEX temporary storage exhausted.\n"
          "Increase N_{TEMPORARY,PERMANENT}_BYTES and recompile.");
}

void vexSetAllocModeTEMP_and_clear ( void )
{
    
   temporary_bytes_allocd_TOT 
      += (ULong)(private_LibVEX_alloc_curr - private_LibVEX_alloc_first);

   mode = VexAllocModeTEMP;
   temporary_curr            = &temporary[0];
   private_LibVEX_alloc_curr = &temporary[0];

   if (0) {
      Int i;
      for (i = 0; i < N_TEMPORARY_BYTES; i++)
         temporary[i] = 0x00;
   }

   vexAllocSanityCheck();
}



void LibVEX_ShowAllocStats ( void )
{
   vex_printf("vex storage: T total %lld bytes allocated\n",
              (Long)temporary_bytes_allocd_TOT );
   vex_printf("vex storage: P total %lld bytes allocated\n",
              (Long)(permanent_curr - permanent_first) );
}

void *LibVEX_Alloc ( SizeT nbytes )
{
   return LibVEX_Alloc_inline(nbytes);
}


__attribute__ ((noreturn))
void vex_assert_fail ( const HChar* expr,
                       const HChar* file, Int line, const HChar* fn )
{
   vex_printf( "\nvex: %s:%d (%s): Assertion `%s' failed.\n",
               file, line, fn, expr );
   (*vex_failure_exit)();
}

__attribute__ ((noreturn))
void vpanic ( const HChar* str )
{
   vex_printf("\nvex: the `impossible' happened:\n   %s\n", str);
   (*vex_failure_exit)();
}



#include <stdarg.h>

SizeT vex_strlen ( const HChar* str )
{
   SizeT i = 0;
   while (str[i] != 0) i++;
   return i;
}

Bool vex_streq ( const HChar* s1, const HChar* s2 )
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

void vex_bzero ( void* sV, SizeT n )
{
   SizeT i;
   UChar* s = (UChar*)sV;
   for (i = 0; i < n; i++) s[i] = 0;
}


static
void convert_int ( HChar* buf, Long n0, 
                   Int base, Bool syned, Bool hexcaps )
{
   ULong u0;
   HChar c;
   Bool minus = False;
   Int i, j, bufi = 0;
   buf[bufi] = 0;

   if (syned) {
      if (n0 < 0) {
         minus = True;
         u0 = (ULong)(-n0);
      } else {
         u0 = (ULong)(n0);
      }
   } else {
      u0 = (ULong)n0;
   }

   while (1) {
     buf[bufi++] = toHChar('0' + toUInt(u0 % base));
     u0 /= base;
     if (u0 == 0) break;
   }
   if (minus)
      buf[bufi++] = '-';

   buf[bufi] = 0;
   for (i = 0; i < bufi; i++)
      if (buf[i] > '9') 
         buf[i] = toHChar(buf[i] + (hexcaps ? 'A' : 'a') - '9' - 1);

   i = 0;
   j = bufi-1;
   while (i <= j) {
      c = buf[i];
      buf[i] = buf[j];
      buf[j] = c;
      i++;
      j--;
   }
}


static
UInt vprintf_wrk ( void(*sink)(HChar),
                   const HChar* format,
                   va_list ap )
{
#  define PUT(_ch)  \
      do { sink(_ch); nout++; } \
      while (0)

#  define PAD(_n) \
      do { Int _qq = (_n); for (; _qq > 0; _qq--) PUT(padchar); } \
      while (0)

#  define PUTSTR(_str) \
      do { const HChar* _qq = _str; for (; *_qq; _qq++) PUT(*_qq); } \
      while (0)

   const HChar* saved_format;
   Bool   longlong, ljustify, is_sizet;
   HChar  padchar;
   Int    fwidth, nout, len1, len3;
   SizeT  len2;
   HChar  intbuf[100];  

   nout = 0;
   while (1) {

      if (!format)
         break;
      if (*format == 0) 
         break;

      if (*format != '%') {
         PUT(*format); 
         format++;
         continue;
      }

      saved_format = format;
      longlong = is_sizet = False;
      ljustify = False;
      padchar = ' ';
      fwidth = 0;
      format++;

      if (*format == '-') {
         format++;
         ljustify = True;
      }
      if (*format == '0') {
         format++;
         padchar = '0';
      }
      if (*format == '*') {
         fwidth = va_arg(ap, Int);
         vassert(fwidth >= 0);
         format++;
      } else {
         while (*format >= '0' && *format <= '9') {
            fwidth = fwidth * 10 + (*format - '0');
            format++;
         }
      }
      if (*format == 'l') {
         format++;
         if (*format == 'l') {
            format++;
            longlong = True;
         }
      } else if (*format == 'z') {
         format++;
         is_sizet = True;
      }

      switch (*format) {
         case 's': {
            const HChar* str = va_arg(ap, HChar*);
            if (str == NULL)
               str = "(null)";
            len1 = len3 = 0;
            len2 = vex_strlen(str);
            if (fwidth > len2) { len1 = ljustify ? 0 : fwidth-len2;
                                 len3 = ljustify ? fwidth-len2 : 0; }
            PAD(len1); PUTSTR(str); PAD(len3);
            break;
         }
         case 'c': {
            HChar c = (HChar)va_arg(ap, int);
            HChar str[2];
            str[0] = c;
            str[1] = 0;
            len1 = len3 = 0;
            len2 = vex_strlen(str);
            if (fwidth > len2) { len1 = ljustify ? 0 : fwidth-len2;
                                 len3 = ljustify ? fwidth-len2 : 0; }
            PAD(len1); PUTSTR(str); PAD(len3);
            break;
         }
         case 'd': {
            Long l;
            vassert(is_sizet == False); 
            if (longlong) {
               l = va_arg(ap, Long);
            } else {
               l = (Long)va_arg(ap, Int);
            }
            convert_int(intbuf, l, 10, True,
                                False);
            len1 = len3 = 0;
            len2 = vex_strlen(intbuf);
            if (fwidth > len2) { len1 = ljustify ? 0 : fwidth-len2;
                                 len3 = ljustify ? fwidth-len2 : 0; }
            PAD(len1); PUTSTR(intbuf); PAD(len3);
            break;
         }
         case 'u': 
         case 'x': 
         case 'X': {
            Int   base = *format == 'u' ? 10 : 16;
            Bool  hexcaps = True; 
            ULong l;
            if (is_sizet) {
               l = (ULong)va_arg(ap, SizeT);
            } else if (longlong) {
               l = va_arg(ap, ULong);
            } else {
               l = (ULong)va_arg(ap, UInt);
            }
            convert_int(intbuf, l, base, False, hexcaps);
            len1 = len3 = 0;
            len2 = vex_strlen(intbuf);
            if (fwidth > len2) { len1 = ljustify ? 0 : fwidth-len2;
                                 len3 = ljustify ? fwidth-len2 : 0; }
            PAD(len1); PUTSTR(intbuf); PAD(len3);
            break;
         }
         case 'p': 
         case 'P': {
            Bool hexcaps = toBool(*format == 'P');
            ULong l = (Addr)va_arg(ap, void*);
            convert_int(intbuf, l, 16, False, hexcaps);
            len1 = len3 = 0;
            len2 = vex_strlen(intbuf)+2;
            if (fwidth > len2) { len1 = ljustify ? 0 : fwidth-len2;
                                 len3 = ljustify ? fwidth-len2 : 0; }
            PAD(len1); PUT('0'); PUT('x'); PUTSTR(intbuf); PAD(len3);
            break;
         }
         case '%': {
            PUT('%');
            break;
         }
         default:
            while (saved_format <= format) {
               PUT(*saved_format);
               saved_format++;
            }
            break;
      }

      format++;

   }

   return nout;

#  undef PUT
#  undef PAD
#  undef PUTSTR
}


static HChar myprintf_buf[1000];
static Int   n_myprintf_buf;

static void add_to_myprintf_buf ( HChar c )
{
   Bool emit = toBool(c == '\n' || n_myprintf_buf >= 1000-10 );
   myprintf_buf[n_myprintf_buf++] = c;
   myprintf_buf[n_myprintf_buf] = 0;
   if (emit) {
      (*vex_log_bytes)( myprintf_buf, vex_strlen(myprintf_buf) );
      n_myprintf_buf = 0;
      myprintf_buf[n_myprintf_buf] = 0;
   }
}

static UInt vex_vprintf ( const HChar* format, va_list vargs )
{
   UInt ret;
   
   n_myprintf_buf = 0;
   myprintf_buf[n_myprintf_buf] = 0;      
   ret = vprintf_wrk ( add_to_myprintf_buf, format, vargs );

   if (n_myprintf_buf > 0) {
      (*vex_log_bytes)( myprintf_buf, n_myprintf_buf );
   }

   return ret;
}

UInt vex_printf ( const HChar* format, ... )
{
   UInt ret;
   va_list vargs;
   va_start(vargs, format);
   ret = vex_vprintf(format, vargs);
   va_end(vargs);

   return ret;
}

__attribute__ ((noreturn))
void vfatal ( const HChar* format, ... )
{
   va_list vargs;
   va_start(vargs, format);
   vex_vprintf( format, vargs );
   va_end(vargs);
   vex_printf("Cannot continue. Good-bye\n\n");

   (*vex_failure_exit)();
}


static HChar *vg_sprintf_ptr;

static void add_to_vg_sprintf_buf ( HChar c )
{
   *vg_sprintf_ptr++ = c;
}

UInt vex_sprintf ( HChar* buf, const HChar *format, ... )
{
   Int ret;
   va_list vargs;

   vg_sprintf_ptr = buf;

   va_start(vargs,format);

   ret = vprintf_wrk ( add_to_vg_sprintf_buf, format, vargs );
   add_to_vg_sprintf_buf(0);

   va_end(vargs);

   vassert(vex_strlen(buf) == ret);
   return ret;
}


