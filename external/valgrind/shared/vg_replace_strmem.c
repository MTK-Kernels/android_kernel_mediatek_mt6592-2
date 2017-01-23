

/*
   This file is part of Valgrind.

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

#include "pub_tool_basics.h"
#include "pub_tool_poolalloc.h"
#include "pub_tool_hashtable.h"
#include "pub_tool_redir.h"
#include "pub_tool_tooliface.h"
#include "pub_tool_clreq.h"




static inline
Bool is_overlap ( void* dst, const void* src, SizeT dstlen, SizeT srclen )
{
   Addr loS, hiS, loD, hiD;

   if (dstlen == 0 || srclen == 0)
      return False;

   loS = (Addr)src;
   loD = (Addr)dst;
   hiS = loS + srclen - 1;
   hiD = loD + dstlen - 1;

   
   if (loS < loD) {
      return !(hiS < loD);
   }
   else if (loD < loS) {
      return !(hiD < loS);
   }
   else {
      return True;
   }
}


__attribute__ ((__noreturn__))
static inline void my_exit ( int x )
{
#  if defined(VGPV_arm_linux_android) || defined(VGPV_mips32_linux_android) \
      || defined(VGPV_arm64_linux_android)
   __asm__ __volatile__(".word 0xFFFFFFFF");
   while (1) {}
#  elif defined(VGPV_x86_linux_android)
   __asm__ __volatile__("ud2");
   while (1) {}
#  else
   extern __attribute__ ((__noreturn__)) void _exit(int status);
   _exit(x);
#  endif
}


#ifndef RECORD_OVERLAP_ERROR
#define RECORD_OVERLAP_ERROR(s, src, dst, len) do { } while (0)
#endif
#ifndef VALGRIND_CHECK_VALUE_IS_DEFINED
#define VALGRIND_CHECK_VALUE_IS_DEFINED(__lvalue) 1
#endif



#define STRRCHR(soname, fnname) \
   char* VG_REPLACE_FUNCTION_EZU(20010,soname,fnname)( const char* s, int c ); \
   char* VG_REPLACE_FUNCTION_EZU(20010,soname,fnname)( const char* s, int c ) \
   { \
      HChar ch = (HChar)c;   \
      const HChar* p = s;       \
      const HChar* last = NULL; \
      while (True) { \
         if (*p == ch) last = p; \
         if (*p == 0) return CONST_CAST(HChar *,last);    \
         p++; \
      } \
   }

#if defined(VGO_linux)
 STRRCHR(VG_Z_LIBC_SONAME,   strrchr)
 STRRCHR(VG_Z_LIBC_SONAME,   rindex)
 STRRCHR(VG_Z_LIBC_SONAME,   __GI_strrchr)
 STRRCHR(VG_Z_LIBC_SONAME,   __strrchr_sse2)
 STRRCHR(VG_Z_LIBC_SONAME,   __strrchr_sse2_no_bsf)
 STRRCHR(VG_Z_LIBC_SONAME,   __strrchr_sse42)
 STRRCHR(VG_Z_LD_LINUX_SO_2, rindex)
#if defined(VGPV_arm_linux_android) || defined(VGPV_x86_linux_android) \
    || defined(VGPV_mips32_linux_android)
  STRRCHR(NONE, __dl_strrchr); 
#endif

#elif defined(VGO_darwin)
 
 
 
 
 STRRCHR(VG_Z_LIBC_SONAME, strrchr)
# if DARWIN_VERS == DARWIN_10_9 || DARWIN_VERS == DARWIN_10_10
  STRRCHR(libsystemZucZddylib, strrchr)
# endif

#endif



#define STRCHR(soname, fnname) \
   char* VG_REPLACE_FUNCTION_EZU(20020,soname,fnname) ( const char* s, int c ); \
   char* VG_REPLACE_FUNCTION_EZU(20020,soname,fnname) ( const char* s, int c ) \
   { \
      HChar  ch = (HChar)c ; \
      const HChar* p  = s;   \
      while (True) { \
         if (*p == ch) return CONST_CAST(HChar *,p);  \
         if (*p == 0) return NULL; \
         p++; \
      } \
   }

#if defined(VGO_linux)
 STRCHR(VG_Z_LIBC_SONAME,          strchr)
 STRCHR(VG_Z_LIBC_SONAME,          __GI_strchr)
 STRCHR(VG_Z_LIBC_SONAME,          __strchr_sse2)
 STRCHR(VG_Z_LIBC_SONAME,          __strchr_sse2_no_bsf)
 STRCHR(VG_Z_LIBC_SONAME,          index)
# if !defined(VGP_x86_linux)
  STRCHR(VG_Z_LD_LINUX_SO_2,        strchr)
  STRCHR(VG_Z_LD_LINUX_SO_2,        index)
  STRCHR(VG_Z_LD_LINUX_X86_64_SO_2, strchr)
  STRCHR(VG_Z_LD_LINUX_X86_64_SO_2, index)
# endif

#elif defined(VGO_darwin)
 STRCHR(VG_Z_LIBC_SONAME, strchr)
# if DARWIN_VERS == DARWIN_10_9
  STRCHR(libsystemZuplatformZddylib, _platform_strchr)
# endif
# if DARWIN_VERS == DARWIN_10_10
  
  STRCHR(libsystemZuplatformZddylib, _platform_strchr$VARIANT$Generic)
  STRCHR(libsystemZuplatformZddylib, _platform_strchr$VARIANT$Haswell)
# endif
#endif



#define STRCAT(soname, fnname) \
   char* VG_REPLACE_FUNCTION_EZU(20030,soname,fnname) \
            ( char* dst, const char* src ); \
   char* VG_REPLACE_FUNCTION_EZU(20030,soname,fnname) \
            ( char* dst, const char* src ) \
   { \
      const HChar* src_orig = src; \
            HChar* dst_orig = dst; \
      while (*dst) dst++; \
      while (*src) *dst++ = *src++; \
      *dst = 0; \
      \
       \
       \
      if (is_overlap(dst_orig,  \
                     src_orig,  \
                     (Addr)dst-(Addr)dst_orig+1,  \
                     (Addr)src-(Addr)src_orig+1)) \
         RECORD_OVERLAP_ERROR("strcat", dst_orig, src_orig, 0); \
      \
      return dst_orig; \
   }

#if defined(VGO_linux)
 STRCAT(VG_Z_LIBC_SONAME, strcat)
 STRCAT(VG_Z_LIBC_SONAME, __GI_strcat)

#elif defined(VGO_darwin)
 

#endif



#define STRNCAT(soname, fnname) \
   char* VG_REPLACE_FUNCTION_EZU(20040,soname,fnname) \
            ( char* dst, const char* src, SizeT n ); \
   char* VG_REPLACE_FUNCTION_EZU(20040,soname,fnname) \
            ( char* dst, const char* src, SizeT n ) \
   { \
      const HChar* src_orig = src; \
            HChar* dst_orig = dst; \
      SizeT m = 0; \
      \
      while (*dst) dst++; \
      while (m < n && *src) { m++; *dst++ = *src++; }  \
      *dst = 0;                                        \
      \
       \
       \
      if (is_overlap(dst_orig,  \
                     src_orig,  \
                     (Addr)dst-(Addr)dst_orig+1, \
                     (Addr)src-(Addr)src_orig+1)) \
         RECORD_OVERLAP_ERROR("strncat", dst_orig, src_orig, n); \
      \
      return dst_orig; \
   }

#if defined(VGO_linux)
 STRNCAT(VG_Z_LIBC_SONAME, strncat)

#elif defined(VGO_darwin)
 
 

#endif



#define STRLCAT(soname, fnname) \
   SizeT VG_REPLACE_FUNCTION_EZU(20050,soname,fnname) \
        ( char* dst, const char* src, SizeT n ); \
   SizeT VG_REPLACE_FUNCTION_EZU(20050,soname,fnname) \
        ( char* dst, const char* src, SizeT n ) \
   { \
      const HChar* src_orig = src; \
      HChar* dst_orig = dst; \
      SizeT m = 0; \
      \
      while (m < n && *dst) { m++; dst++; } \
      if (m < n) { \
          \
         while (m < n-1 && *src) { m++; *dst++ = *src++; } \
         *dst = 0; \
      } else { \
          \
      } \
       \
      while (*src) { m++; src++; } \
       \
       \
      if (is_overlap(dst_orig,  \
                     src_orig,  \
                     (Addr)dst-(Addr)dst_orig+1,  \
                     (Addr)src-(Addr)src_orig+1)) \
         RECORD_OVERLAP_ERROR("strlcat", dst_orig, src_orig, n); \
      \
      return m; \
   }

#if defined(VGO_linux)

#elif defined(VGO_darwin)
 
 
 STRLCAT(VG_Z_LIBC_SONAME, strlcat)

#endif



#define STRNLEN(soname, fnname) \
   SizeT VG_REPLACE_FUNCTION_EZU(20060,soname,fnname) \
            ( const char* str, SizeT n ); \
   SizeT VG_REPLACE_FUNCTION_EZU(20060,soname,fnname) \
            ( const char* str, SizeT n ) \
   { \
      SizeT i = 0; \
      while (i < n && str[i] != 0) i++; \
      return i; \
   }

#if defined(VGO_linux)
 STRNLEN(VG_Z_LIBC_SONAME, strnlen)
 STRNLEN(VG_Z_LIBC_SONAME, __GI_strnlen)

#elif defined(VGO_darwin)
# if DARWIN_VERS == DARWIN_10_9
  STRNLEN(libsystemZucZddylib, strnlen)
# endif

#endif




#define STRLEN(soname, fnname) \
   SizeT VG_REPLACE_FUNCTION_EZU(20070,soname,fnname) \
      ( const char* str ); \
   SizeT VG_REPLACE_FUNCTION_EZU(20070,soname,fnname) \
      ( const char* str )  \
   { \
      SizeT i = 0; \
      while (str[i] != 0) i++; \
      return i; \
   }

#if defined(VGO_linux)
 STRLEN(VG_Z_LIBC_SONAME,          strlen)
 STRLEN(VG_Z_LIBC_SONAME,          __GI_strlen)
 STRLEN(VG_Z_LIBC_SONAME,          __strlen_sse2)
 STRLEN(VG_Z_LIBC_SONAME,          __strlen_sse2_no_bsf)
 STRLEN(VG_Z_LIBC_SONAME,          __strlen_sse42)
 STRLEN(VG_Z_LD_LINUX_SO_2,        strlen)
 STRLEN(VG_Z_LD_LINUX_X86_64_SO_2, strlen)
# if defined(VGPV_arm_linux_android) \
     || defined(VGPV_x86_linux_android) \
     || defined(VGPV_mips32_linux_android)
  STRLEN(NONE, __dl_strlen); 
# endif

#elif defined(VGO_darwin)
 STRLEN(VG_Z_LIBC_SONAME, strlen)
# if DARWIN_VERS == DARWIN_10_9 || DARWIN_VERS == DARWIN_10_10
  STRLEN(libsystemZucZddylib, strlen)
# endif
#endif



#define STRCPY(soname, fnname) \
   char* VG_REPLACE_FUNCTION_EZU(20080,soname,fnname) \
      ( char* dst, const char* src ); \
   char* VG_REPLACE_FUNCTION_EZU(20080,soname,fnname) \
      ( char* dst, const char* src ) \
   { \
      const HChar* src_orig = src; \
            HChar* dst_orig = dst; \
      \
      while (*src) *dst++ = *src++; \
      *dst = 0; \
      \
       \
       \
      if (is_overlap(dst_orig,  \
                     src_orig,  \
                     (Addr)dst-(Addr)dst_orig+1, \
                     (Addr)src-(Addr)src_orig+1)) \
         RECORD_OVERLAP_ERROR("strcpy", dst_orig, src_orig, 0); \
      \
      return dst_orig; \
   }

#if defined(VGO_linux)
 STRCPY(VG_Z_LIBC_SONAME, strcpy)
 STRCPY(VG_Z_LIBC_SONAME, __GI_strcpy)

#elif defined(VGO_darwin)
 STRCPY(VG_Z_LIBC_SONAME, strcpy)
# if DARWIN_VERS == DARWIN_10_9
  STRCPY(libsystemZucZddylib, strcpy)
# endif

#endif



#define STRNCPY(soname, fnname) \
   char* VG_REPLACE_FUNCTION_EZU(20090,soname,fnname) \
            ( char* dst, const char* src, SizeT n ); \
   char* VG_REPLACE_FUNCTION_EZU(20090,soname,fnname) \
            ( char* dst, const char* src, SizeT n ) \
   { \
      const HChar* src_orig = src; \
            HChar* dst_orig = dst; \
      SizeT m = 0; \
      \
      while (m   < n && *src) { m++; *dst++ = *src++; } \
       \
       \
      if (is_overlap(dst_orig, src_orig, n, (m < n) ? m+1 : n)) \
         RECORD_OVERLAP_ERROR("strncpy", dst, src, n); \
      while (m++ < n) *dst++ = 0;          \
      \
      return dst_orig; \
   }

#if defined(VGO_linux)
 STRNCPY(VG_Z_LIBC_SONAME, strncpy)
 STRNCPY(VG_Z_LIBC_SONAME, __GI_strncpy)
 STRNCPY(VG_Z_LIBC_SONAME, __strncpy_sse2)
 STRNCPY(VG_Z_LIBC_SONAME, __strncpy_sse2_unaligned)

#elif defined(VGO_darwin)
 STRNCPY(VG_Z_LIBC_SONAME, strncpy)
# if DARWIN_VERS == DARWIN_10_9 || DARWIN_VERS == DARWIN_10_10
  STRNCPY(libsystemZucZddylib, strncpy)
# endif

#endif



#define STRLCPY(soname, fnname) \
   SizeT VG_REPLACE_FUNCTION_EZU(20100,soname,fnname) \
       ( char* dst, const char* src, SizeT n ); \
   SizeT VG_REPLACE_FUNCTION_EZU(20100,soname,fnname) \
       ( char* dst, const char* src, SizeT n ) \
   { \
      const HChar* src_orig = src; \
      HChar* dst_orig = dst; \
      SizeT m = 0; \
      \
      while (m < n-1 && *src) { m++; *dst++ = *src++; } \
       \
       \
       \
      if (is_overlap(dst_orig, src_orig, n, (m < n) ? m+1 : n)) \
          RECORD_OVERLAP_ERROR("strlcpy", dst, src, n); \
       \
      if (n > 0) *dst = 0; \
       \
      while (*src) src++; \
      return src - src_orig; \
   }

#if defined(VGO_linux)

#if defined(VGPV_arm_linux_android) || defined(VGPV_x86_linux_android) \
    || defined(VGPV_mips32_linux_android)
 STRLCPY(VG_Z_LIBC_SONAME, strlcpy);
#endif

#elif defined(VGO_darwin)
 
 
 STRLCPY(VG_Z_LIBC_SONAME, strlcpy)

#endif



#define STRNCMP(soname, fnname) \
   int VG_REPLACE_FUNCTION_EZU(20110,soname,fnname) \
          ( const char* s1, const char* s2, SizeT nmax ); \
   int VG_REPLACE_FUNCTION_EZU(20110,soname,fnname) \
          ( const char* s1, const char* s2, SizeT nmax ) \
   { \
      SizeT n = 0; \
      while (True) { \
         if (n >= nmax) return 0; \
         if (*s1 == 0 && *s2 == 0) return 0; \
         if (*s1 == 0) return -1; \
         if (*s2 == 0) return 1; \
         \
         if (*(const UChar*)s1 < *(const UChar*)s2) return -1; \
         if (*(const UChar*)s1 > *(const UChar*)s2) return 1; \
         \
         s1++; s2++; n++; \
      } \
   }

#if defined(VGO_linux)
 STRNCMP(VG_Z_LIBC_SONAME, strncmp)
 STRNCMP(VG_Z_LIBC_SONAME, __GI_strncmp)
 STRNCMP(VG_Z_LIBC_SONAME, __strncmp_sse2)
 STRNCMP(VG_Z_LIBC_SONAME, __strncmp_sse42)

#elif defined(VGO_darwin)
 STRNCMP(VG_Z_LIBC_SONAME,        strncmp)
# if DARWIN_VERS == DARWIN_10_9 || DARWIN_VERS == DARWIN_10_10
  STRNCMP(libsystemZuplatformZddylib, _platform_strncmp)
# endif

#endif



#define STRCASECMP(soname, fnname) \
   int VG_REPLACE_FUNCTION_EZU(20120,soname,fnname) \
          ( const char* s1, const char* s2 ); \
   int VG_REPLACE_FUNCTION_EZU(20120,soname,fnname) \
          ( const char* s1, const char* s2 ) \
   { \
      extern int tolower(int); \
      register UChar c1; \
      register UChar c2; \
      while (True) { \
         c1 = tolower(*(const UChar *)s1); \
         c2 = tolower(*(const UChar *)s2); \
         if (c1 != c2) break; \
         if (c1 == 0) break; \
         s1++; s2++; \
      } \
      if ((UChar)c1 < (UChar)c2) return -1; \
      if ((UChar)c1 > (UChar)c2) return 1; \
      return 0; \
   }

#if defined(VGO_linux)
# if !defined(VGPV_arm_linux_android) \
     && !defined(VGPV_x86_linux_android) \
     && !defined(VGPV_mips32_linux_android) \
     && !defined(VGPV_arm64_linux_android)
  STRCASECMP(VG_Z_LIBC_SONAME, strcasecmp)
  STRCASECMP(VG_Z_LIBC_SONAME, __GI_strcasecmp)
# endif

#elif defined(VGO_darwin)
 

#endif



#define STRNCASECMP(soname, fnname) \
   int VG_REPLACE_FUNCTION_EZU(20130,soname,fnname) \
          ( const char* s1, const char* s2, SizeT nmax ); \
   int VG_REPLACE_FUNCTION_EZU(20130,soname,fnname) \
          ( const char* s1, const char* s2, SizeT nmax ) \
   { \
      extern int tolower(int); \
      SizeT n = 0; \
      while (True) { \
         if (n >= nmax) return 0; \
         if (*s1 == 0 && *s2 == 0) return 0; \
         if (*s1 == 0) return -1; \
         if (*s2 == 0) return 1; \
         \
         if (tolower(*(const UChar *)s1) \
             < tolower(*(const UChar*)s2)) return -1; \
         if (tolower(*(const UChar *)s1) \
             > tolower(*(const UChar *)s2)) return 1; \
         \
         s1++; s2++; n++; \
      } \
   }

#if defined(VGO_linux)
# if !defined(VGPV_arm_linux_android) \
     && !defined(VGPV_x86_linux_android) \
     && !defined(VGPV_mips32_linux_android) \
     && !defined(VGPV_arm64_linux_android)
  STRNCASECMP(VG_Z_LIBC_SONAME, strncasecmp)
  STRNCASECMP(VG_Z_LIBC_SONAME, __GI_strncasecmp)
# endif

#elif defined(VGO_darwin)
 
 

#endif



#define STRCASECMP_L(soname, fnname) \
   int VG_REPLACE_FUNCTION_EZU(20140,soname,fnname) \
          ( const char* s1, const char* s2, void* locale ); \
   int VG_REPLACE_FUNCTION_EZU(20140,soname,fnname) \
          ( const char* s1, const char* s2, void* locale ) \
   { \
      extern int tolower_l(int, void*) __attribute__((weak)); \
      register UChar c1; \
      register UChar c2; \
      while (True) { \
         c1 = tolower_l(*(const UChar *)s1, locale); \
         c2 = tolower_l(*(const UChar *)s2, locale); \
         if (c1 != c2) break; \
         if (c1 == 0) break; \
         s1++; s2++; \
      } \
      if ((UChar)c1 < (UChar)c2) return -1; \
      if ((UChar)c1 > (UChar)c2) return 1; \
      return 0; \
   }

#if defined(VGO_linux)
 STRCASECMP_L(VG_Z_LIBC_SONAME, strcasecmp_l)
 STRCASECMP_L(VG_Z_LIBC_SONAME, __GI_strcasecmp_l)
 STRCASECMP_L(VG_Z_LIBC_SONAME, __GI___strcasecmp_l)

#elif defined(VGO_darwin)
 

#endif



#define STRNCASECMP_L(soname, fnname) \
   int VG_REPLACE_FUNCTION_EZU(20150,soname,fnname) \
          ( const char* s1, const char* s2, SizeT nmax, void* locale ); \
   int VG_REPLACE_FUNCTION_EZU(20150,soname,fnname) \
          ( const char* s1, const char* s2, SizeT nmax, void* locale ) \
   { \
      extern int tolower_l(int, void*) __attribute__((weak));    \
      SizeT n = 0; \
      while (True) { \
         if (n >= nmax) return 0; \
         if (*s1 == 0 && *s2 == 0) return 0; \
         if (*s1 == 0) return -1; \
         if (*s2 == 0) return 1; \
         \
         if (tolower_l(*(const UChar *)s1, locale) \
             < tolower_l(*(const UChar *)s2, locale)) return -1; \
         if (tolower_l(*(const UChar *)s1, locale) \
             > tolower_l(*(const UChar *)s2, locale)) return 1; \
         \
         s1++; s2++; n++; \
      } \
   }

#if defined(VGO_linux)
 STRNCASECMP_L(VG_Z_LIBC_SONAME, strncasecmp_l)
 STRNCASECMP_L(VG_Z_LIBC_SONAME, __GI_strncasecmp_l)
 STRNCASECMP_L(VG_Z_LIBC_SONAME, __GI___strncasecmp_l)

#elif defined(VGO_darwin)
 
 

#endif



#define STRCMP(soname, fnname) \
   int VG_REPLACE_FUNCTION_EZU(20160,soname,fnname) \
          ( const char* s1, const char* s2 ); \
   int VG_REPLACE_FUNCTION_EZU(20160,soname,fnname) \
          ( const char* s1, const char* s2 ) \
   { \
      register UChar c1; \
      register UChar c2; \
      while (True) { \
         c1 = *(const UChar *)s1; \
         c2 = *(const UChar *)s2; \
         if (c1 != c2) break; \
         if (c1 == 0) break; \
         s1++; s2++; \
      } \
      if ((UChar)c1 < (UChar)c2) return -1; \
      if ((UChar)c1 > (UChar)c2) return 1; \
      return 0; \
   }

#if defined(VGO_linux)
 STRCMP(VG_Z_LIBC_SONAME,          strcmp)
 STRCMP(VG_Z_LIBC_SONAME,          __GI_strcmp)
 STRCMP(VG_Z_LIBC_SONAME,          __strcmp_sse2)
 STRCMP(VG_Z_LIBC_SONAME,          __strcmp_sse42)
 STRCMP(VG_Z_LD_LINUX_X86_64_SO_2, strcmp)
 STRCMP(VG_Z_LD64_SO_1,            strcmp)
# if defined(VGPV_arm_linux_android) || defined(VGPV_x86_linux_android) \
     || defined(VGPV_mips32_linux_android)
  STRCMP(NONE, __dl_strcmp); 
# endif

#elif defined(VGO_darwin)
 STRCMP(VG_Z_LIBC_SONAME, strcmp)
# if DARWIN_VERS == DARWIN_10_9 || DARWIN_VERS == DARWIN_10_10
  STRCMP(libsystemZuplatformZddylib, _platform_strcmp)
# endif

#endif



#define MEMCHR(soname, fnname) \
   void* VG_REPLACE_FUNCTION_EZU(20170,soname,fnname) \
            (const void *s, int c, SizeT n); \
   void* VG_REPLACE_FUNCTION_EZU(20170,soname,fnname) \
            (const void *s, int c, SizeT n) \
   { \
      SizeT i; \
      UChar c0 = (UChar)c; \
      const UChar* p = s; \
      for (i = 0; i < n; i++) \
         if (p[i] == c0) return CONST_CAST(void *,&p[i]); \
      return NULL; \
   }

#if defined(VGO_linux)
 MEMCHR(VG_Z_LIBC_SONAME, memchr)
 MEMCHR(VG_Z_LIBC_SONAME, __GI_memchr)

#elif defined(VGO_darwin)
# if DARWIN_VERS == DARWIN_10_9
  MEMCHR(VG_Z_DYLD,                   memchr)
  MEMCHR(libsystemZuplatformZddylib, _platform_memchr)
# endif
# if DARWIN_VERS == DARWIN_10_10
  MEMCHR(VG_Z_DYLD,                   memchr)
  
  MEMCHR(libsystemZuplatformZddylib, _platform_memchr$VARIANT$Generic)
# endif

#endif



#define MEMRCHR(soname, fnname) \
   void* VG_REPLACE_FUNCTION_EZU(20360,soname,fnname) \
            (const void *s, int c, SizeT n); \
   void* VG_REPLACE_FUNCTION_EZU(20360,soname,fnname) \
            (const void *s, int c, SizeT n) \
   { \
      SizeT i; \
      UChar c0 = (UChar)c; \
      const UChar* p = s; \
      for (i = 0; i < n; i++) \
         if (p[n-1-i] == c0) return CONST_CAST(void *,&p[n-1-i]); \
      return NULL; \
   }

#if defined(VGO_linux)
 MEMRCHR(VG_Z_LIBC_SONAME, memrchr)

#elif defined(VGO_darwin)
 
 

#endif



#define MEMMOVE_OR_MEMCPY(becTag, soname, fnname, do_ol_check)  \
   void* VG_REPLACE_FUNCTION_EZZ(becTag,soname,fnname) \
            ( void *dst, const void *src, SizeT len ); \
   void* VG_REPLACE_FUNCTION_EZZ(becTag,soname,fnname) \
            ( void *dst, const void *src, SizeT len ) \
   { \
      if (do_ol_check && is_overlap(dst, src, len, len)) \
         RECORD_OVERLAP_ERROR("memcpy", dst, src, len); \
      \
      const Addr WS = sizeof(UWord);  \
      const Addr WM = WS - 1;         \
      \
      if (len > 0) { \
         if (dst < src || !is_overlap(dst, src, len, len)) { \
         \
             \
            SizeT n = len; \
            Addr  d = (Addr)dst; \
            Addr  s = (Addr)src; \
            \
            if (((s^d) & WM) == 0) { \
                \
                \
               while ((s & WM) != 0 && n >= 1) \
                  { *(UChar*)d = *(UChar*)s; s += 1; d += 1; n -= 1; } \
                \
               while (n >= WS) \
                  { *(UWord*)d = *(UWord*)s; s += WS; d += WS; n -= WS; } \
               if (n == 0) \
                  return dst; \
            } \
            if (((s|d) & 1) == 0) { \
                \
               while (n >= 2) \
                  { *(UShort*)d = *(UShort*)s; s += 2; d += 2; n -= 2; } \
            } \
             \
            while (n >= 1) \
               { *(UChar*)d = *(UChar*)s; s += 1; d += 1; n -= 1; } \
         \
         } else if (dst > src) { \
         \
            SizeT n = len; \
            Addr  d = ((Addr)dst) + n; \
            Addr  s = ((Addr)src) + n; \
            \
             \
            if (((s^d) & WM) == 0) { \
                \
                \
               while ((s & WM) != 0 && n >= 1) \
                  { s -= 1; d -= 1; *(UChar*)d = *(UChar*)s; n -= 1; } \
                \
               while (n >= WS) \
                  { s -= WS; d -= WS; *(UWord*)d = *(UWord*)s; n -= WS; } \
               if (n == 0) \
                  return dst; \
            } \
            if (((s|d) & 1) == 0) { \
                \
               while (n >= 2) \
                  { s -= 2; d -= 2; *(UShort*)d = *(UShort*)s; n -= 2; } \
            } \
             \
            while (n >= 1) \
               { s -= 1; d -= 1; *(UChar*)d = *(UChar*)s; n -= 1; } \
            \
         } \
      } \
      \
      return dst; \
   }

#define MEMMOVE(soname, fnname)  \
   MEMMOVE_OR_MEMCPY(20181, soname, fnname, 0)

#define MEMCPY(soname, fnname) \
   MEMMOVE_OR_MEMCPY(20180, soname, fnname, 1)

#if defined(VGO_linux)
 MEMMOVE(VG_Z_LIBC_SONAME, memcpyZAGLIBCZu2Zd2Zd5) 
 MEMCPY(VG_Z_LIBC_SONAME,  memcpyZAZAGLIBCZu2Zd14) 
 MEMCPY(VG_Z_LIBC_SONAME,  memcpy) 
 MEMCPY(VG_Z_LIBC_SONAME,    __GI_memcpy)
 MEMCPY(VG_Z_LIBC_SONAME,    __memcpy_sse2)
 MEMCPY(VG_Z_LD_SO_1,      memcpy) 
 MEMCPY(VG_Z_LD64_SO_1,    memcpy) 
 MEMCPY(NONE, ZuintelZufastZumemcpy)

#elif defined(VGO_darwin)
# if DARWIN_VERS <= DARWIN_10_6
  MEMCPY(VG_Z_LIBC_SONAME,  memcpy)
# endif
 MEMCPY(VG_Z_LIBC_SONAME,  memcpyZDVARIANTZDsse3x) 
 MEMCPY(VG_Z_LIBC_SONAME,  memcpyZDVARIANTZDsse42) 

#endif



#define MEMCMP(soname, fnname) \
   int VG_REPLACE_FUNCTION_EZU(20190,soname,fnname)       \
          ( const void *s1V, const void *s2V, SizeT n ); \
   int VG_REPLACE_FUNCTION_EZU(20190,soname,fnname)       \
          ( const void *s1V, const void *s2V, SizeT n )  \
   { \
      const SizeT WS = sizeof(UWord);  \
      const SizeT WM = WS - 1;         \
      Addr s1A = (Addr)s1V; \
      Addr s2A = (Addr)s2V; \
      \
      if (((s1A | s2A) & WM) == 0) { \
          \
          \
         while (n >= WS) { \
            UWord w1 = *(UWord*)s1A; \
            UWord w2 = *(UWord*)s2A; \
            if (w1 != w2) break; \
            s1A += WS; \
            s2A += WS; \
            n -= WS; \
         } \
      } \
      \
      const UChar* s1 = (const UChar*) s1A; \
      const UChar* s2 = (const UChar*) s2A; \
      \
      while (n != 0) { \
         UChar a0 = s1[0]; \
         UChar b0 = s2[0]; \
         s1 += 1; \
         s2 += 1; \
         int res = ((int)a0) - ((int)b0); \
         if (res != 0) \
            return res; \
         n -= 1; \
      } \
      return 0; \
   }

#if defined(VGO_linux)
 MEMCMP(VG_Z_LIBC_SONAME, memcmp)
 MEMCMP(VG_Z_LIBC_SONAME, __GI_memcmp)
 MEMCMP(VG_Z_LIBC_SONAME, __memcmp_sse2)
 MEMCMP(VG_Z_LIBC_SONAME, __memcmp_sse4_1)
 MEMCMP(VG_Z_LIBC_SONAME, bcmp)
 MEMCMP(VG_Z_LD_SO_1,     bcmp)

#elif defined(VGO_darwin)
# if DARWIN_VERS == DARWIN_10_9 || DARWIN_VERS == DARWIN_10_10
  MEMCMP(libsystemZuplatformZddylib, _platform_memcmp)
# endif

#endif



#define STPCPY(soname, fnname) \
   char* VG_REPLACE_FUNCTION_EZU(20200,soname,fnname) \
            ( char* dst, const char* src ); \
   char* VG_REPLACE_FUNCTION_EZU(20200,soname,fnname) \
            ( char* dst, const char* src ) \
   { \
      const HChar* src_orig = src; \
            HChar* dst_orig = dst; \
      \
      while (*src) *dst++ = *src++; \
      *dst = 0; \
      \
       \
       \
      if (is_overlap(dst_orig,  \
                     src_orig,  \
                     (Addr)dst-(Addr)dst_orig+1,  \
                     (Addr)src-(Addr)src_orig+1)) \
         RECORD_OVERLAP_ERROR("stpcpy", dst_orig, src_orig, 0); \
      \
      return dst; \
   }

#if defined(VGO_linux)
 STPCPY(VG_Z_LIBC_SONAME,          stpcpy)
 STPCPY(VG_Z_LIBC_SONAME,          __GI_stpcpy)
 STPCPY(VG_Z_LIBC_SONAME,          __stpcpy_sse2)
 STPCPY(VG_Z_LIBC_SONAME,          __stpcpy_sse2_unaligned)
 STPCPY(VG_Z_LD_LINUX_SO_2,        stpcpy)
 STPCPY(VG_Z_LD_LINUX_X86_64_SO_2, stpcpy)

#elif defined(VGO_darwin)
 
 

#endif



#define STPNCPY(soname, fnname) \
   char* VG_REPLACE_FUNCTION_EZU(20420,soname,fnname) \
            ( char* dst, const char* src, SizeT n ); \
   char* VG_REPLACE_FUNCTION_EZU(20420,soname,fnname) \
            ( char* dst, const char* src, SizeT n ) \
   { \
      const HChar* src_orig = src; \
            HChar* dst_str  = dst; \
      SizeT m = 0; \
      \
      while (m   < n && *src) { m++; *dst++ = *src++; } \
       \
       \
      if (is_overlap(dst_str, src_orig, n, (m < n) ? m+1 : n)) \
         RECORD_OVERLAP_ERROR("stpncpy", dst, src, n); \
      dst_str = dst; \
      while (m++ < n) *dst++ = 0;          \
      \
      return dst_str; \
   }

#if defined(VGO_linux)
 STPNCPY(VG_Z_LIBC_SONAME, stpncpy)
#endif




#define MEMSET(soname, fnname) \
   void* VG_REPLACE_FUNCTION_EZU(20210,soname,fnname) \
            (void *s, Int c, SizeT n); \
   void* VG_REPLACE_FUNCTION_EZU(20210,soname,fnname) \
            (void *s, Int c, SizeT n) \
   { \
      if (sizeof(void*) == 8) { \
         Addr  a  = (Addr)s;   \
         ULong c8 = (c & 0xFF); \
         c8 = (c8 << 8) | c8; \
         c8 = (c8 << 16) | c8; \
         c8 = (c8 << 32) | c8; \
         while ((a & 7) != 0 && n >= 1) \
            { *(UChar*)a = (UChar)c; a += 1; n -= 1; } \
         while (n >= 8) \
            { *(ULong*)a = c8; a += 8; n -= 8; } \
         while (n >= 1) \
            { *(UChar*)a = (UChar)c; a += 1; n -= 1; } \
         return s; \
      } else { \
         Addr a  = (Addr)s;   \
         UInt c4 = (c & 0xFF); \
         c4 = (c4 << 8) | c4; \
         c4 = (c4 << 16) | c4; \
         while ((a & 3) != 0 && n >= 1) \
            { *(UChar*)a = (UChar)c; a += 1; n -= 1; } \
         while (n >= 4) \
            { *(UInt*)a = c4; a += 4; n -= 4; } \
         while (n >= 1) \
            { *(UChar*)a = (UChar)c; a += 1; n -= 1; } \
         return s; \
      } \
   }

#if defined(VGO_linux)
 MEMSET(VG_Z_LIBC_SONAME, memset)

#elif defined(VGO_darwin)
 
 
 MEMSET(VG_Z_LIBC_SONAME, memset)

#endif




#if defined(VGO_linux)
 MEMMOVE(VG_Z_LIBC_SONAME, memmove)
 MEMMOVE(VG_Z_LIBC_SONAME, __GI_memmove)

#elif defined(VGO_darwin)
# if DARWIN_VERS <= DARWIN_10_6
  MEMMOVE(VG_Z_LIBC_SONAME, memmove)
# endif
 MEMMOVE(VG_Z_LIBC_SONAME,  memmoveZDVARIANTZDsse3x) 
 MEMMOVE(VG_Z_LIBC_SONAME,  memmoveZDVARIANTZDsse42) 
# if DARWIN_VERS == DARWIN_10_9 || DARWIN_VERS == DARWIN_10_10
  
  MEMMOVE(libsystemZuplatformZddylib, ZuplatformZumemmoveZDVARIANTZDIvybridge)
# endif
#endif



#define BCOPY(soname, fnname) \
   void VG_REPLACE_FUNCTION_EZU(20230,soname,fnname) \
            (const void *srcV, void *dstV, SizeT n); \
   void VG_REPLACE_FUNCTION_EZU(20230,soname,fnname) \
            (const void *srcV, void *dstV, SizeT n) \
   { \
      SizeT i; \
      HChar* dst = dstV; \
      const HChar* src = srcV; \
      if (dst < src) { \
         for (i = 0; i < n; i++) \
            dst[i] = src[i]; \
      } \
      else  \
      if (dst > src) { \
         for (i = 0; i < n; i++) \
            dst[n-i-1] = src[n-i-1]; \
      } \
   }

#if defined(VGO_linux)
 BCOPY(VG_Z_LIBC_SONAME, bcopy)

#elif defined(VGO_darwin)
 
 

#endif



#define GLIBC25___MEMMOVE_CHK(soname, fnname) \
   void* VG_REPLACE_FUNCTION_EZU(20240,soname,fnname) \
            (void *dstV, const void *srcV, SizeT n, SizeT destlen); \
   void* VG_REPLACE_FUNCTION_EZU(20240,soname,fnname) \
            (void *dstV, const void *srcV, SizeT n, SizeT destlen) \
   { \
      SizeT i; \
      HChar* dst = dstV;        \
      const HChar* src = srcV; \
      if (destlen < n) \
         goto badness; \
      if (dst < src) { \
         for (i = 0; i < n; i++) \
            dst[i] = src[i]; \
      } \
      else  \
      if (dst > src) { \
         for (i = 0; i < n; i++) \
            dst[n-i-1] = src[n-i-1]; \
      } \
      return dst; \
     badness: \
      VALGRIND_PRINTF_BACKTRACE( \
         "*** memmove_chk: buffer overflow detected ***: " \
         "program terminated\n"); \
     my_exit(127); \
      \
     return NULL; \
   }

#if defined(VGO_linux)
 GLIBC25___MEMMOVE_CHK(VG_Z_LIBC_SONAME, __memmove_chk)

#elif defined(VGO_darwin)

#endif



#define GLIBC232_STRCHRNUL(soname, fnname) \
   char* VG_REPLACE_FUNCTION_EZU(20250,soname,fnname) \
            (const char* s, int c_in); \
   char* VG_REPLACE_FUNCTION_EZU(20250,soname,fnname) \
            (const char* s, int c_in) \
   { \
      HChar c = (HChar) c_in; \
      const HChar* char_ptr = s; \
      while (1) { \
         if (*char_ptr == 0) return CONST_CAST(HChar *,char_ptr);  \
         if (*char_ptr == c) return CONST_CAST(HChar *,char_ptr);  \
         char_ptr++; \
      } \
   }

#if defined(VGO_linux)
 GLIBC232_STRCHRNUL(VG_Z_LIBC_SONAME, strchrnul)

#elif defined(VGO_darwin)

#endif



#define GLIBC232_RAWMEMCHR(soname, fnname) \
   void* VG_REPLACE_FUNCTION_EZU(20260,soname,fnname) \
            (const void* s, int c_in); \
   void* VG_REPLACE_FUNCTION_EZU(20260,soname,fnname) \
            (const void* s, int c_in) \
   { \
      UChar c = (UChar) c_in; \
      const UChar* char_ptr = s; \
      while (1) { \
         if (*char_ptr == c) return CONST_CAST(void *,char_ptr); \
         char_ptr++; \
      } \
   }

#if defined (VGO_linux)
 GLIBC232_RAWMEMCHR(VG_Z_LIBC_SONAME, rawmemchr)
 GLIBC232_RAWMEMCHR(VG_Z_LIBC_SONAME, __GI___rawmemchr)

#elif defined(VGO_darwin)

#endif



#define GLIBC25___STRCPY_CHK(soname,fnname) \
   char* VG_REPLACE_FUNCTION_EZU(20270,soname,fnname) \
            (char* dst, const char* src, SizeT len); \
   char* VG_REPLACE_FUNCTION_EZU(20270,soname,fnname) \
            (char* dst, const char* src, SizeT len) \
   { \
      HChar* ret = dst; \
      if (! len) \
         goto badness; \
      while ((*dst++ = *src++) != '\0') \
         if (--len == 0) \
            goto badness; \
      return ret; \
     badness: \
      VALGRIND_PRINTF_BACKTRACE( \
         "*** strcpy_chk: buffer overflow detected ***: " \
         "program terminated\n"); \
     my_exit(127); \
      \
     return NULL; \
   }

#if defined(VGO_linux)
 GLIBC25___STRCPY_CHK(VG_Z_LIBC_SONAME, __strcpy_chk)

#elif defined(VGO_darwin)

#endif



#define GLIBC25___STPCPY_CHK(soname,fnname) \
   char* VG_REPLACE_FUNCTION_EZU(20280,soname,fnname) \
            (char* dst, const char* src, SizeT len); \
   char* VG_REPLACE_FUNCTION_EZU(20280,soname,fnname) \
            (char* dst, const char* src, SizeT len) \
   { \
      if (! len) \
         goto badness; \
      while ((*dst++ = *src++) != '\0') \
         if (--len == 0) \
            goto badness; \
      return dst - 1; \
     badness: \
      VALGRIND_PRINTF_BACKTRACE( \
         "*** stpcpy_chk: buffer overflow detected ***: " \
         "program terminated\n"); \
     my_exit(127); \
      \
     return NULL; \
   }

#if defined(VGO_linux)
 GLIBC25___STPCPY_CHK(VG_Z_LIBC_SONAME, __stpcpy_chk)

#elif defined(VGO_darwin)

#endif



#define GLIBC25_MEMPCPY(soname, fnname) \
   void* VG_REPLACE_FUNCTION_EZU(20290,soname,fnname) \
            ( void *dst, const void *src, SizeT len ); \
   void* VG_REPLACE_FUNCTION_EZU(20290,soname,fnname) \
            ( void *dst, const void *src, SizeT len ) \
   { \
      SizeT len_saved = len; \
      \
      if (len == 0) \
         return dst; \
      \
      if (is_overlap(dst, src, len, len)) \
         RECORD_OVERLAP_ERROR("mempcpy", dst, src, len); \
      \
      if ( dst > src ) { \
         register HChar *d = (char *)dst + len - 1; \
         register const HChar *s = (const char *)src + len - 1; \
         while ( len-- ) { \
            *d-- = *s--; \
         } \
      } else if ( dst < src ) { \
         register HChar *d = dst; \
         register const HChar *s = src; \
         while ( len-- ) { \
            *d++ = *s++; \
         } \
      } \
      return (void*)( ((char*)dst) + len_saved ); \
   }

#if defined(VGO_linux)
 GLIBC25_MEMPCPY(VG_Z_LIBC_SONAME, mempcpy)
 GLIBC25_MEMPCPY(VG_Z_LIBC_SONAME, __GI_mempcpy)
 GLIBC25_MEMPCPY(VG_Z_LD_SO_1,     mempcpy) 
 GLIBC25_MEMPCPY(VG_Z_LD_LINUX_SO_3, mempcpy) 
 GLIBC25_MEMPCPY(VG_Z_LD_LINUX_X86_64_SO_2, mempcpy) 

#elif defined(VGO_darwin)
 

#endif



#define GLIBC26___MEMCPY_CHK(soname, fnname) \
   void* VG_REPLACE_FUNCTION_EZU(20300,soname,fnname) \
            (void* dst, const void* src, SizeT len, SizeT dstlen ); \
   void* VG_REPLACE_FUNCTION_EZU(20300,soname,fnname) \
            (void* dst, const void* src, SizeT len, SizeT dstlen ) \
   { \
      register HChar *d; \
      register const HChar *s; \
      \
      if (dstlen < len) goto badness; \
      \
      if (len == 0) \
         return dst; \
      \
      if (is_overlap(dst, src, len, len)) \
         RECORD_OVERLAP_ERROR("memcpy_chk", dst, src, len); \
      \
      if ( dst > src ) { \
         d = (HChar *)dst + len - 1; \
         s = (const HChar *)src + len - 1; \
         while ( len-- ) { \
            *d-- = *s--; \
         } \
      } else if ( dst < src ) { \
         d = (HChar *)dst; \
         s = (const HChar *)src; \
         while ( len-- ) { \
            *d++ = *s++; \
         } \
      } \
      return dst; \
     badness: \
      VALGRIND_PRINTF_BACKTRACE( \
         "*** memcpy_chk: buffer overflow detected ***: " \
         "program terminated\n"); \
     my_exit(127); \
      \
     return NULL; \
   }

#if defined(VGO_linux)
 GLIBC26___MEMCPY_CHK(VG_Z_LIBC_SONAME, __memcpy_chk)

#elif defined(VGO_darwin)

#endif



#define STRSTR(soname, fnname) \
   char* VG_REPLACE_FUNCTION_EZU(20310,soname,fnname) \
         (const char* haystack, const char* needle); \
   char* VG_REPLACE_FUNCTION_EZU(20310,soname,fnname) \
         (const char* haystack, const char* needle) \
   { \
      const HChar* h = haystack; \
      const HChar* n = needle; \
      \
       \
      UWord nlen = 0; \
      while (n[nlen]) nlen++; \
      \
       \
      if (nlen == 0) return CONST_CAST(HChar *,h);         \
      \
       \
      HChar n0 = n[0]; \
      \
      while (1) { \
         const HChar hh = *h; \
         if (hh == 0) return NULL; \
         if (hh != n0) { h++; continue; } \
         \
         UWord i; \
         for (i = 0; i < nlen; i++) { \
            if (n[i] != h[i]) \
               break; \
         } \
          \
         if (i == nlen) \
           return CONST_CAST(HChar *,h);          \
         \
         h++; \
      } \
   }

#if defined(VGO_linux)
 STRSTR(VG_Z_LIBC_SONAME,          strstr)
 STRSTR(VG_Z_LIBC_SONAME,          __strstr_sse2)
 STRSTR(VG_Z_LIBC_SONAME,          __strstr_sse42)

#elif defined(VGO_darwin)

#endif



#define STRPBRK(soname, fnname) \
   char* VG_REPLACE_FUNCTION_EZU(20320,soname,fnname) \
         (const char* sV, const char* acceptV); \
   char* VG_REPLACE_FUNCTION_EZU(20320,soname,fnname) \
         (const char* sV, const char* acceptV) \
   { \
      const HChar* s = sV; \
      const HChar* accept = acceptV; \
      \
       \
      UWord nacc = 0; \
      while (accept[nacc]) nacc++; \
      \
       \
      if (nacc == 0) return NULL; \
      \
       \
      while (1) { \
         UWord i; \
         HChar sc = *s; \
         if (sc == 0) \
            break; \
         for (i = 0; i < nacc; i++) { \
            if (sc == accept[i]) \
              return CONST_CAST(HChar *,s);       \
         } \
         s++; \
      } \
      \
      return NULL; \
   }

#if defined(VGO_linux)
 STRPBRK(VG_Z_LIBC_SONAME,          strpbrk)

#elif defined(VGO_darwin)

#endif



#define STRCSPN(soname, fnname) \
   SizeT VG_REPLACE_FUNCTION_EZU(20330,soname,fnname) \
         (const char* sV, const char* rejectV); \
   SizeT VG_REPLACE_FUNCTION_EZU(20330,soname,fnname) \
         (const char* sV, const char* rejectV) \
   { \
      const HChar* s = sV; \
      const HChar* reject = rejectV; \
      \
       \
      UWord nrej = 0; \
      while (reject[nrej]) nrej++; \
      \
      UWord len = 0; \
      while (1) { \
         UWord i; \
         HChar sc = *s; \
         if (sc == 0) \
            break; \
         for (i = 0; i < nrej; i++) { \
            if (sc == reject[i]) \
               break; \
         } \
          \
         if (i < nrej) \
            break; \
         s++; \
         len++; \
      } \
      \
      return len; \
   }

#if defined(VGO_linux)
 STRCSPN(VG_Z_LIBC_SONAME,          strcspn)

#elif defined(VGO_darwin)

#endif



#define STRSPN(soname, fnname) \
   SizeT VG_REPLACE_FUNCTION_EZU(20340,soname,fnname) \
         (const char* sV, const char* acceptV); \
   SizeT VG_REPLACE_FUNCTION_EZU(20340,soname,fnname) \
         (const char* sV, const char* acceptV) \
   { \
      const UChar* s = (const UChar *)sV;        \
      const UChar* accept = (const UChar *)acceptV;     \
      \
       \
      UWord nacc = 0; \
      while (accept[nacc]) nacc++; \
      if (nacc == 0) return 0; \
      \
      UWord len = 0; \
      while (1) { \
         UWord i; \
         HChar sc = *s; \
         if (sc == 0) \
            break; \
         for (i = 0; i < nacc; i++) { \
            if (sc == accept[i]) \
               break; \
         } \
          \
         if (i == nacc) \
            break; \
         s++; \
         len++; \
      } \
      \
      return len; \
   }

#if defined(VGO_linux)
 STRSPN(VG_Z_LIBC_SONAME,          strspn)

#elif defined(VGO_darwin)

#endif



#define STRCASESTR(soname, fnname) \
   char* VG_REPLACE_FUNCTION_EZU(20350,soname,fnname) \
         (const char* haystack, const char* needle); \
   char* VG_REPLACE_FUNCTION_EZU(20350,soname,fnname) \
         (const char* haystack, const char* needle) \
   { \
      extern int tolower(int); \
      const HChar* h = haystack; \
      const HChar* n = needle;   \
      \
       \
      UWord nlen = 0; \
      while (n[nlen]) nlen++; \
      \
       \
      if (nlen == 0) return CONST_CAST(HChar *,h);       \
      \
       \
      UChar n0 = tolower(n[0]);                 \
      \
      while (1) { \
         UChar hh = tolower(*h);    \
         if (hh == 0) return NULL; \
         if (hh != n0) { h++; continue; } \
         \
         UWord i; \
         for (i = 0; i < nlen; i++) { \
            if (tolower(n[i]) != tolower(h[i]))  \
               break; \
         } \
          \
         if (i == nlen) \
           return CONST_CAST(HChar *,h);    \
         \
         h++; \
      } \
   }

#if defined(VGO_linux)
# if !defined(VGPV_arm_linux_android) \
     && !defined(VGPV_x86_linux_android) \
     && !defined(VGPV_mips32_linux_android) \
     && !defined(VGPV_arm64_linux_android)
  STRCASESTR(VG_Z_LIBC_SONAME,      strcasestr)
# endif

#elif defined(VGO_darwin)

#endif




#define WCSLEN(soname, fnname) \
   SizeT VG_REPLACE_FUNCTION_EZU(20370,soname,fnname) \
      ( const UInt* str ); \
   SizeT VG_REPLACE_FUNCTION_EZU(20370,soname,fnname) \
      ( const UInt* str )  \
   { \
      SizeT i = 0; \
      while (str[i] != 0) i++; \
      return i; \
   }

#if defined(VGO_linux)
 WCSLEN(VG_Z_LIBC_SONAME,          wcslen)

#elif defined(VGO_darwin)

#endif



#define WCSCMP(soname, fnname) \
   int VG_REPLACE_FUNCTION_EZU(20380,soname,fnname) \
          ( const Int* s1, const Int* s2 ); \
   int VG_REPLACE_FUNCTION_EZU(20380,soname,fnname) \
          ( const Int* s1, const Int* s2 ) \
   { \
      register Int c1; \
      register Int c2; \
      while (True) { \
         c1 = *s1; \
         c2 = *s2; \
         if (c1 != c2) break; \
         if (c1 == 0) break; \
         s1++; s2++; \
      } \
      if (c1 < c2) return -1; \
      if (c1 > c2) return 1; \
      return 0; \
   }

#if defined(VGO_linux)
 WCSCMP(VG_Z_LIBC_SONAME,          wcscmp)
#endif



#define WCSCPY(soname, fnname) \
   Int* VG_REPLACE_FUNCTION_EZU(20390,soname,fnname) \
      ( Int* dst, const Int* src ); \
   Int* VG_REPLACE_FUNCTION_EZU(20390,soname,fnname) \
      ( Int* dst, const Int* src ) \
   { \
      const Int* src_orig = src; \
            Int* dst_orig = dst; \
      \
      while (*src) *dst++ = *src++; \
      *dst = 0; \
      \
       \
       \
      if (is_overlap(dst_orig,  \
                     src_orig,  \
                     (Addr)dst-(Addr)dst_orig+1, \
                     (Addr)src-(Addr)src_orig+1)) \
         RECORD_OVERLAP_ERROR("wcscpy", dst_orig, src_orig, 0); \
      \
      return dst_orig; \
   }

#if defined(VGO_linux)
 WCSCPY(VG_Z_LIBC_SONAME, wcscpy)
#endif




#define WCSCHR(soname, fnname) \
   Int* VG_REPLACE_FUNCTION_EZU(20400,soname,fnname) ( const Int* s, Int c ); \
   Int* VG_REPLACE_FUNCTION_EZU(20400,soname,fnname) ( const Int* s, Int c ) \
   { \
      const Int* p = s; \
      while (True) { \
         if (*p == c) return CONST_CAST(Int *,p);  \
         if (*p == 0) return NULL; \
         p++; \
      } \
   }

#if defined(VGO_linux)
 WCSCHR(VG_Z_LIBC_SONAME,          wcschr)
#endif


#define WCSRCHR(soname, fnname) \
   Int* VG_REPLACE_FUNCTION_EZU(20410,soname,fnname)( const Int* s, Int c ); \
   Int* VG_REPLACE_FUNCTION_EZU(20410,soname,fnname)( const Int* s, Int c ) \
   { \
      const Int* p = s; \
      const Int* last = NULL; \
      while (True) { \
         if (*p == c) last = p; \
         if (*p == 0) return CONST_CAST(Int *,last);  \
         p++; \
      } \
   }

#if defined(VGO_linux)
 WCSRCHR(VG_Z_LIBC_SONAME, wcsrchr)
#endif


#if defined(VGO_linux)



int VG_WRAP_FUNCTION_ZU(VG_Z_LIBC_SONAME, putenv) (char* string);
int VG_WRAP_FUNCTION_ZU(VG_Z_LIBC_SONAME, putenv) (char* string)
{
    OrigFn fn;
    Word result;
    const HChar* p = string;
    VALGRIND_GET_ORIG_FN(fn);
    if (p)
        while (*p++)
            __asm__ __volatile__("" ::: "memory");
    CALL_FN_W_W(result, fn, string);
    return result;
}



int VG_WRAP_FUNCTION_ZU(VG_Z_LIBC_SONAME, unsetenv) (const char* name);
int VG_WRAP_FUNCTION_ZU(VG_Z_LIBC_SONAME, unsetenv) (const char* name)
{
    OrigFn fn;
    Word result;
    const HChar* p = name;
    VALGRIND_GET_ORIG_FN(fn);
    if (p)
        while (*p++)
            __asm__ __volatile__("" ::: "memory");
    CALL_FN_W_W(result, fn, name);
    return result;
}



int VG_WRAP_FUNCTION_ZU(VG_Z_LIBC_SONAME, setenv)
    (const char* name, const char* value, int overwrite);
int VG_WRAP_FUNCTION_ZU(VG_Z_LIBC_SONAME, setenv)
    (const char* name, const char* value, int overwrite)
{
    OrigFn fn;
    Word result;
    const HChar* p;
    VALGRIND_GET_ORIG_FN(fn);
    if (name)
        for (p = name; *p; p++)
            __asm__ __volatile__("" ::: "memory");
    if (value)
        for (p = value; *p; p++)
            __asm__ __volatile__("" ::: "memory");
    (void) VALGRIND_CHECK_VALUE_IS_DEFINED (overwrite);
    CALL_FN_W_WWW(result, fn, name, value, overwrite);
    return result;
}

#endif 

