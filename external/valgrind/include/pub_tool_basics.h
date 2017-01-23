

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

#ifndef __PUB_TOOL_BASICS_H
#define __PUB_TOOL_BASICS_H



#include "libvex_basictypes.h"

#include <stdarg.h>


 

#define VGAPPEND(str1,str2) str1##str2

#define VG_(str)    VGAPPEND(vgPlain_,          str)
#define ML_(str)    VGAPPEND(vgModuleLocal_,    str)



typedef unsigned long          UWord;     
typedef   signed long           Word;     

typedef  Word                 SSizeT;     

typedef  Word                 PtrdiffT;   

#if defined(VGO_linux)
typedef Word                   OffT;      
#elif defined(VGO_darwin)
typedef Long                   OffT;      
#else
#  error Unknown OS
#endif
typedef Long                 Off64T;      

#if !defined(NULL)
#  define NULL ((void*)0)
#endif

typedef  struct { UWord uw1; UWord uw2; }  UWordPair;




typedef UInt ThreadId;

#if defined(VGO_linux)
typedef
   struct {
      Bool  _isError;
      UWord _val;
#if defined(VGA_mips64) || defined(VGA_mips32)
      UWord _valEx;
#endif
   }
   SysRes;
#elif defined(VGO_darwin)
typedef
   enum { 
      SysRes_MACH=40,  
      SysRes_MDEP,     
      SysRes_UNIX_OK,  
      SysRes_UNIX_ERR  
   }
   SysResMode;
typedef
   struct {
      UWord _wLO;
      UWord _wHI;
      SysResMode _mode;
   }
   SysRes;
#else
#  error "Unknown OS"
#endif



#if defined(VGO_linux)

static inline Bool sr_isError ( SysRes sr ) {
   return sr._isError;
}
static inline UWord sr_Res ( SysRes sr ) {
   return sr._isError ? 0 : sr._val;
}
#if defined(VGA_mips64) || defined(VGA_mips32)
static inline UWord sr_ResEx ( SysRes sr ) {
   return sr._isError ? 0 : sr._valEx;
}
#endif
static inline UWord sr_Err ( SysRes sr ) {
   return sr._isError ? sr._val : 0;
}
static inline Bool sr_EQ ( SysRes sr1, SysRes sr2 ) {
   return sr1._val == sr2._val
       && sr1._isError == sr2._isError;
}

#elif defined(VGO_darwin)

static inline Bool sr_isError ( SysRes sr ) {
   switch (sr._mode) {
      case SysRes_UNIX_ERR:
         return True;
      
      case SysRes_MACH:
      case SysRes_MDEP:
      case SysRes_UNIX_OK:
      default:
         return False;
   }
}

static inline UWord sr_Res ( SysRes sr ) {
   switch (sr._mode) {
      case SysRes_MACH:
      case SysRes_MDEP:
      case SysRes_UNIX_OK:
         return sr._wLO;
      
      case SysRes_UNIX_ERR:
      default:
         return 0;
   }
}

static inline UWord sr_ResHI ( SysRes sr ) {
   switch (sr._mode) {
      case SysRes_UNIX_OK:
         return sr._wHI;
      
      case SysRes_MACH:
      case SysRes_MDEP:
      case SysRes_UNIX_ERR:
      default:
         return 0;
   }
}

static inline UWord sr_Err ( SysRes sr ) {
   switch (sr._mode) {
      case SysRes_UNIX_ERR:
         return sr._wLO;
      
      case SysRes_MACH:
      case SysRes_MDEP:
      case SysRes_UNIX_OK:
      default:
         return 0;
   }
}

static inline Bool sr_EQ ( SysRes sr1, SysRes sr2 ) {
   return sr1._mode == sr2._mode
          && sr1._wLO == sr2._wLO && sr1._wHI == sr2._wHI;
}

#else
#  error "Unknown OS"
#endif



#define VG_WORDSIZE VEX_HOST_WORDSIZE

#undef VG_BIGENDIAN
#undef VG_LITTLEENDIAN

#if defined(VGA_x86) || defined(VGA_amd64) || defined (VGA_arm) \
    || ((defined(VGA_mips32) || defined(VGA_mips64)) && defined (_MIPSEL)) \
    || defined(VGA_arm64)  || defined(VGA_ppc64le) || defined(VGA_tilegx)
#  define VG_LITTLEENDIAN 1
#elif defined(VGA_ppc32) || defined(VGA_ppc64be) || defined(VGA_s390x) \
      || ((defined(VGA_mips32) || defined(VGA_mips64)) && defined (_MIPSEB))
#  define VG_BIGENDIAN 1
#else
#  error Unknown arch
#endif

#if !defined(offsetof)
#   define offsetof(type,memb) ((SizeT)(HWord)&((type*)0)->memb)
#endif

#define vg_alignof(_type) (sizeof(struct {char c;_type _t;})-sizeof(_type))

#if defined(VGA_x86)
#  define VG_REGPARM(n)            __attribute__((regparm(n)))
#elif defined(VGA_amd64) || defined(VGA_ppc32) \
      || defined(VGA_ppc64be) || defined(VGA_ppc64le) \
      || defined(VGA_arm) || defined(VGA_s390x) \
      || defined(VGA_mips32) || defined(VGA_mips64) \
      || defined(VGA_arm64) || defined(VGA_tilegx)
#  define VG_REGPARM(n)            
#else
#  error Unknown arch
#endif

#define VG_STRINGIFZ(__str)  #__str
#define VG_STRINGIFY(__str)  VG_STRINGIFZ(__str)

#define VG_BUGS_TO "www.valgrind.org"

#if defined(__GNUC__)
#  define LIKELY(x)   __builtin_expect(!!(x), 1)
#  define UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#  define LIKELY(x)   (x)
#  define UNLIKELY(x) (x)
#endif

#if defined(__GNUC__)
#define PRINTF_CHECK(x, y) __attribute__((format(__printf__, x, y)))
#else
#define PRINTF_CHECK(x, y)
#endif

#define CONST_CAST(T,x)    \
   ({                      \
      union {              \
         const T in;      \
         T out;           \
      } var = { .in = x }; var.out;  \
   })

#define STATIC_ASSERT(x)  extern int VG_(VG_(VG_(unused)))[(x) ? 1 : -1] \
                                     __attribute__((unused))

#endif 

