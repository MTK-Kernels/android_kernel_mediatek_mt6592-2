

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

#ifndef __PUB_TOOL_LIBCBASE_H
#define __PUB_TOOL_LIBCBASE_H

#include "pub_tool_basics.h"   


extern Bool  VG_(isspace) ( HChar c );
extern Bool  VG_(isdigit) ( HChar c );
extern HChar VG_(tolower) ( HChar c );


extern Long  VG_(strtoll10) ( const HChar* str, HChar** endptr );
extern Long  VG_(strtoll16) ( const HChar* str, HChar** endptr );
extern ULong  VG_(strtoull10) ( const HChar* str, HChar** endptr );
extern ULong  VG_(strtoull16) ( const HChar* str, HChar** endptr );

extern double VG_(strtod)  ( const HChar* str, HChar** endptr );


#define VG_STREQ(s1,s2) ( (s1 != NULL && s2 != NULL \
                           && VG_(strcmp)((s1),(s2))==0) ? True : False )
#define VG_STREQN(n,s1,s2) ( (s1 != NULL && s2 != NULL \
                             && VG_(strncmp)((s1),(s2),(n))==0) ? True : False )

extern SizeT  VG_(strlen)         ( const HChar* str );
extern HChar* VG_(strcat)         ( HChar* dest, const HChar* src );
extern HChar* VG_(strncat)        ( HChar* dest, const HChar* src, SizeT n );
extern HChar* VG_(strpbrk)        ( const HChar* s, const HChar* accpt );
extern HChar* VG_(strcpy)         ( HChar* dest, const HChar* src );
extern HChar* VG_(strncpy)        ( HChar* dest, const HChar* src, SizeT ndest );
extern Int    VG_(strcmp)         ( const HChar* s1, const HChar* s2 );
extern Int    VG_(strcasecmp)     ( const HChar* s1, const HChar* s2 );
extern Int    VG_(strncmp)        ( const HChar* s1, const HChar* s2, SizeT nmax );
extern Int    VG_(strncasecmp)    ( const HChar* s1, const HChar* s2, SizeT nmax );
extern HChar* VG_(strstr)         ( const HChar* haystack, const HChar* needle );
extern HChar* VG_(strcasestr)     ( const HChar* haystack, const HChar* needle );
extern HChar* VG_(strchr)         ( const HChar* s, HChar c );
extern HChar* VG_(strrchr)        ( const HChar* s, HChar c );
extern SizeT  VG_(strspn)         ( const HChar* s, const HChar* accpt );
extern SizeT  VG_(strcspn)        ( const HChar* s, const HChar* reject );

extern HChar* VG_(strtok_r)       (HChar* s, const HChar* delim, HChar** saveptr);
extern HChar* VG_(strtok)         (HChar* s, const HChar* delim);

extern Bool VG_(parse_Addr) ( const HChar** ppc, Addr* result );

extern Bool VG_(parse_enum_set) ( const HChar *tokens,
                                  Bool  allow_all,
                                  const HChar *input,
                                  UInt *enum_set);


extern void* VG_(memcpy) ( void *d, const void *s, SizeT sz );
extern void* VG_(memmove)( void *d, const void *s, SizeT sz );
extern void* VG_(memset) ( void *s, Int c, SizeT sz );
extern Int   VG_(memcmp) ( const void* s1, const void* s2, SizeT n );

inline __attribute__((always_inline))
static void VG_(bzero_inline) ( void* s, SizeT sz )
{
   if (LIKELY(0 == (((Addr)sz) & (Addr)(sizeof(UWord)-1)))
       && LIKELY(0 == (((Addr)s) & (Addr)(sizeof(UWord)-1)))) {
      UWord* p = (UWord*)s;
      switch (sz / (SizeT)sizeof(UWord)) {
          case 12: p[0] = p[1] = p[2] = p[3]
                  = p[4] = p[5] = p[6] = p[7] 
                  = p[8] = p[9] = p[10] = p[11] = 0UL; return;
          case 11: p[0] = p[1] = p[2] = p[3]
                  = p[4] = p[5] = p[6] = p[7] 
                  = p[8] = p[9] = p[10] = 0UL; return;
          case 10: p[0] = p[1] = p[2] = p[3]
                  = p[4] = p[5] = p[6] = p[7] 
                  = p[8] = p[9] = 0UL; return;
          case 9: p[0] = p[1] = p[2] = p[3]
                  = p[4] = p[5] = p[6] = p[7] 
                  = p[8] = 0UL; return;
          case 8: p[0] = p[1] = p[2] = p[3]
                  = p[4] = p[5] = p[6] = p[7] = 0UL; return;
          case 7: p[0] = p[1] = p[2] = p[3]
                  = p[4] = p[5] = p[6] = 0UL; return;
          case 6: p[0] = p[1] = p[2] = p[3]
                  = p[4] = p[5] = 0UL; return;
          case 5: p[0] = p[1] = p[2] = p[3] = p[4] = 0UL; return;
          case 4: p[0] = p[1] = p[2] = p[3] = 0UL; return;
          case 3: p[0] = p[1] = p[2] = 0UL; return;
          case 2: p[0] = p[1] = 0UL; return;
          case 1: p[0] = 0UL; return;
          case 0: return;
          default: break;
      }
   }
   VG_(memset)(s, 0, sz);
}



#define VG_IS_2_ALIGNED(aaa_p)    (0 == (((Addr)(aaa_p)) & ((Addr)0x1)))
#define VG_IS_4_ALIGNED(aaa_p)    (0 == (((Addr)(aaa_p)) & ((Addr)0x3)))
#define VG_IS_8_ALIGNED(aaa_p)    (0 == (((Addr)(aaa_p)) & ((Addr)0x7)))
#define VG_IS_16_ALIGNED(aaa_p)   (0 == (((Addr)(aaa_p)) & ((Addr)0xf)))
#define VG_IS_32_ALIGNED(aaa_p)   (0 == (((Addr)(aaa_p)) & ((Addr)0x1f)))
#define VG_IS_WORD_ALIGNED(aaa_p) (0 == (((Addr)(aaa_p)) & ((Addr)(sizeof(Addr)-1))))
#define VG_IS_PAGE_ALIGNED(aaa_p) (0 == (((Addr)(aaa_p)) & ((Addr)(VKI_PAGE_SIZE-1))))

#define VG_ROUNDDN(p, a)   ((Addr)(p) & ~((Addr)(a)-1))
#define VG_ROUNDUP(p, a)   VG_ROUNDDN((p)+(a)-1, (a))
#define VG_PGROUNDDN(p)    VG_ROUNDDN(p, VKI_PAGE_SIZE)
#define VG_PGROUNDUP(p)    VG_ROUNDUP(p, VKI_PAGE_SIZE)


extern void VG_(ssort)( void* base, SizeT nmemb, SizeT size,
                        Int (*compar)(const void*, const void*) );

extern Int VG_(log2) ( UInt x );

extern Int VG_(log2_64)( ULong x );

extern UInt VG_(random) ( UInt* pSeed );

extern UInt VG_(adler32)( UInt adler, const UChar* buf, UInt len);

#endif   

