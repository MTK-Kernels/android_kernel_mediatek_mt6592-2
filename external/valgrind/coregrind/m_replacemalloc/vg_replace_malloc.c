

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
#include "pub_core_vki.h"           
#include "pub_core_clreq.h"         
                                    
#include "pub_core_debuginfo.h"     
#include "pub_core_mallocfree.h"    
#include "pub_core_redir.h"         
#include "pub_core_replacemalloc.h"





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

static inline int my_getpagesize ( void )
{
#  if defined(VGPV_arm_linux_android) \
      || defined(VGPV_x86_linux_android) \
      || defined(VGPV_mips32_linux_android) \
      || defined(VGPV_arm64_linux_android)
   return 4096; 
#  else
   extern int getpagesize (void);
   return getpagesize();
#  endif
}


static UWord umulHW ( UWord u, UWord v )
{
   UWord u0, v0, w0, rHi;
   UWord u1, v1, w1,w2,t;
   UWord halfMask  = sizeof(UWord)==4 ? (UWord)0xFFFF
                                      : (UWord)0xFFFFFFFFULL;
   UWord halfShift = sizeof(UWord)==4 ? 16 : 32;
   u0  = u & halfMask;
   u1  = u >> halfShift;
   v0  = v & halfMask;
   v1  = v >> halfShift;
   w0  = u0 * v0;
   t   = u1 * v0 + (w0 >> halfShift);
   w1  = t & halfMask;
   w2  = t >> halfShift;
   w1  = u0 * v1 + w1;
   rHi = u1 * v1 + w2 + (w1 >> halfShift);
   return rHi;
}



static struct vg_mallocfunc_info info;
static int init_done;
#define DO_INIT if (UNLIKELY(!init_done)) init()

__attribute__((constructor))
static void init(void);

#define MALLOC_TRACE(format, args...)  \
   if (info.clo_trace_malloc) {        \
      VALGRIND_INTERNAL_PRINTF(format, ## args ); }


#define TRIGGER_MEMCHECK_ERROR_IF_UNDEFINED(x) \
   if ((ULong)x == 0) __asm__ __volatile__( "" ::: "memory" )


#define ALLOC_or_NULL(soname, fnname, vg_replacement) \
   \
   void* VG_REPLACE_FUNCTION_EZU(10010,soname,fnname) (SizeT n); \
   void* VG_REPLACE_FUNCTION_EZU(10010,soname,fnname) (SizeT n)  \
   { \
      void* v; \
      \
      DO_INIT; \
      TRIGGER_MEMCHECK_ERROR_IF_UNDEFINED(n); \
      MALLOC_TRACE(#fnname "(%llu)", (ULong)n ); \
      \
      v = (void*)VALGRIND_NON_SIMD_CALL1( info.tl_##vg_replacement, n ); \
      MALLOC_TRACE(" = %p\n", v ); \
      return v; \
   }

#define ZONEALLOC_or_NULL(soname, fnname, vg_replacement) \
   \
   void* VG_REPLACE_FUNCTION_EZU(10020,soname,fnname) (void *zone, SizeT n); \
   void* VG_REPLACE_FUNCTION_EZU(10020,soname,fnname) (void *zone, SizeT n)  \
   { \
      void* v; \
      \
      DO_INIT; \
      TRIGGER_MEMCHECK_ERROR_IF_UNDEFINED((UWord) zone);	\
      TRIGGER_MEMCHECK_ERROR_IF_UNDEFINED(n);                   \
      MALLOC_TRACE(#fnname "(%p, %llu)", zone, (ULong)n ); \
      \
      v = (void*)VALGRIND_NON_SIMD_CALL1( info.tl_##vg_replacement, n ); \
      MALLOC_TRACE(" = %p\n", v ); \
      return v; \
   }


#define ALLOC_or_BOMB(soname, fnname, vg_replacement)  \
   \
   void* VG_REPLACE_FUNCTION_EZU(10030,soname,fnname) (SizeT n); \
   void* VG_REPLACE_FUNCTION_EZU(10030,soname,fnname) (SizeT n)  \
   { \
      void* v; \
      \
      DO_INIT; \
      TRIGGER_MEMCHECK_ERROR_IF_UNDEFINED(n);           \
      MALLOC_TRACE(#fnname "(%llu)", (ULong)n );        \
      \
      v = (void*)VALGRIND_NON_SIMD_CALL1( info.tl_##vg_replacement, n ); \
      MALLOC_TRACE(" = %p\n", v ); \
      if (NULL == v) { \
         VALGRIND_PRINTF( \
            "new/new[] failed and should throw an exception, but Valgrind\n"); \
         VALGRIND_PRINTF_BACKTRACE( \
            "   cannot throw exceptions and so is aborting instead.  Sorry.\n"); \
            my_exit(1); \
      } \
      return v; \
   }

#define SO_SYN_MALLOC VG_SO_SYN(somalloc)

#if defined(VGO_linux)
 ALLOC_or_NULL(VG_Z_LIBSTDCXX_SONAME, malloc,      malloc);
 ALLOC_or_NULL(VG_Z_LIBC_SONAME,      malloc,      malloc);
 ALLOC_or_NULL(SO_SYN_MALLOC,         malloc,      malloc);

#elif defined(VGO_darwin)
 ALLOC_or_NULL(VG_Z_LIBC_SONAME,      malloc,      malloc);
 ALLOC_or_NULL(SO_SYN_MALLOC,         malloc,      malloc);
 ZONEALLOC_or_NULL(VG_Z_LIBC_SONAME,  malloc_zone_malloc, malloc);
 ZONEALLOC_or_NULL(SO_SYN_MALLOC,     malloc_zone_malloc, malloc);

#endif



#if defined(VGO_linux)
 
 ALLOC_or_BOMB(VG_Z_LIBSTDCXX_SONAME,  builtin_new,    __builtin_new);
 ALLOC_or_BOMB(VG_Z_LIBC_SONAME,       builtin_new,    __builtin_new);
 ALLOC_or_BOMB(VG_Z_LIBSTDCXX_SONAME,  __builtin_new,  __builtin_new);
 ALLOC_or_BOMB(VG_Z_LIBC_SONAME,       __builtin_new,  __builtin_new);
 
 #if VG_WORDSIZE == 4
  ALLOC_or_BOMB(VG_Z_LIBSTDCXX_SONAME, _Znwj,          __builtin_new);
  ALLOC_or_BOMB(VG_Z_LIBC_SONAME,      _Znwj,          __builtin_new);
  ALLOC_or_BOMB(SO_SYN_MALLOC,         _Znwj,          __builtin_new);
 #endif
 
 #if VG_WORDSIZE == 8
  ALLOC_or_BOMB(VG_Z_LIBSTDCXX_SONAME, _Znwm,          __builtin_new);
  ALLOC_or_BOMB(VG_Z_LIBC_SONAME,      _Znwm,          __builtin_new);
  ALLOC_or_BOMB(SO_SYN_MALLOC,         _Znwm,          __builtin_new);
 #endif

#elif defined(VGO_darwin)
 
 #if VG_WORDSIZE == 4
  
  
 #endif
 
 #if 1 
  
  
 #endif

#endif



#if defined(VGO_linux)
 
 #if VG_WORDSIZE == 4
  ALLOC_or_NULL(VG_Z_LIBSTDCXX_SONAME, _ZnwjRKSt9nothrow_t,  __builtin_new);
  ALLOC_or_NULL(VG_Z_LIBC_SONAME,      _ZnwjRKSt9nothrow_t,  __builtin_new);
  ALLOC_or_NULL(SO_SYN_MALLOC,         _ZnwjRKSt9nothrow_t,  __builtin_new);
 #endif
 
 #if VG_WORDSIZE == 8
  ALLOC_or_NULL(VG_Z_LIBSTDCXX_SONAME, _ZnwmRKSt9nothrow_t,  __builtin_new);
  ALLOC_or_NULL(VG_Z_LIBC_SONAME,      _ZnwmRKSt9nothrow_t,  __builtin_new);
  ALLOC_or_NULL(SO_SYN_MALLOC,         _ZnwmRKSt9nothrow_t,  __builtin_new);
 #endif

#elif defined(VGO_darwin)
 
 #if VG_WORDSIZE == 4
  
  
 #endif
 
 #if 1 
  
  
 #endif

#endif



#if defined(VGO_linux)
 
 ALLOC_or_BOMB(VG_Z_LIBSTDCXX_SONAME,  __builtin_vec_new, __builtin_vec_new );
 ALLOC_or_BOMB(VG_Z_LIBC_SONAME,       __builtin_vec_new, __builtin_vec_new );
 
 #if VG_WORDSIZE == 4
  ALLOC_or_BOMB(VG_Z_LIBSTDCXX_SONAME, _Znaj,             __builtin_vec_new );
  ALLOC_or_BOMB(VG_Z_LIBC_SONAME,      _Znaj,             __builtin_vec_new );
  ALLOC_or_BOMB(SO_SYN_MALLOC,         _Znaj,             __builtin_vec_new );
 #endif
 
 #if VG_WORDSIZE == 8
  ALLOC_or_BOMB(VG_Z_LIBSTDCXX_SONAME, _Znam,             __builtin_vec_new );
  ALLOC_or_BOMB(VG_Z_LIBC_SONAME,      _Znam,             __builtin_vec_new );
  ALLOC_or_BOMB(SO_SYN_MALLOC,         _Znam,             __builtin_vec_new );
 #endif

#elif defined(VGO_darwin)
 
 #if VG_WORDSIZE == 4
  
  
 #endif
 
 #if 1 
  
  
 #endif

#endif



#if defined(VGO_linux)
 
 #if VG_WORDSIZE == 4
  ALLOC_or_NULL(VG_Z_LIBSTDCXX_SONAME, _ZnajRKSt9nothrow_t, __builtin_vec_new );
  ALLOC_or_NULL(VG_Z_LIBC_SONAME,      _ZnajRKSt9nothrow_t, __builtin_vec_new );
  ALLOC_or_NULL(SO_SYN_MALLOC,         _ZnajRKSt9nothrow_t, __builtin_vec_new );
 #endif
 
 #if VG_WORDSIZE == 8
  ALLOC_or_NULL(VG_Z_LIBSTDCXX_SONAME, _ZnamRKSt9nothrow_t, __builtin_vec_new );
  ALLOC_or_NULL(VG_Z_LIBC_SONAME,      _ZnamRKSt9nothrow_t, __builtin_vec_new );
  ALLOC_or_NULL(SO_SYN_MALLOC,         _ZnamRKSt9nothrow_t, __builtin_vec_new );
 #endif

#elif defined(VGO_darwin)
 
 #if VG_WORDSIZE == 4
  
  
 #endif
 
 #if 1 
  
  
 #endif

#endif



#define ZONEFREE(soname, fnname, vg_replacement) \
   \
   void VG_REPLACE_FUNCTION_EZU(10040,soname,fnname) (void *zone, void *p); \
   void VG_REPLACE_FUNCTION_EZU(10040,soname,fnname) (void *zone, void *p)  \
   { \
      DO_INIT; \
      TRIGGER_MEMCHECK_ERROR_IF_UNDEFINED((UWord) zone);	\
      MALLOC_TRACE(#fnname "(%p, %p)\n", zone, p ); \
      if (p == NULL)  \
         return; \
      (void)VALGRIND_NON_SIMD_CALL1( info.tl_##vg_replacement, p ); \
   }

#define FREE(soname, fnname, vg_replacement) \
   \
   void VG_REPLACE_FUNCTION_EZU(10050,soname,fnname) (void *p); \
   void VG_REPLACE_FUNCTION_EZU(10050,soname,fnname) (void *p)  \
   { \
      DO_INIT; \
      MALLOC_TRACE(#fnname "(%p)\n", p ); \
      if (p == NULL)  \
         return; \
      (void)VALGRIND_NON_SIMD_CALL1( info.tl_##vg_replacement, p ); \
   }


#if defined(VGO_linux)
 FREE(VG_Z_LIBSTDCXX_SONAME,  free,                 free );
 FREE(VG_Z_LIBC_SONAME,       free,                 free );
 FREE(SO_SYN_MALLOC,          free,                 free );

#elif defined(VGO_darwin)
 FREE(VG_Z_LIBC_SONAME,       free,                 free );
 FREE(SO_SYN_MALLOC,          free,                 free );
 ZONEFREE(VG_Z_LIBC_SONAME,   malloc_zone_free,     free );
 ZONEFREE(SO_SYN_MALLOC,      malloc_zone_free,     free );

#endif



#if defined(VGO_linux)
 FREE(VG_Z_LIBSTDCXX_SONAME,  cfree,                free );
 FREE(VG_Z_LIBC_SONAME,       cfree,                free );
 FREE(SO_SYN_MALLOC,          cfree,                free );

#elif defined(VGO_darwin)
 
 

#endif



#if defined(VGO_linux)
 
 FREE(VG_Z_LIBSTDCXX_SONAME,   __builtin_delete,     __builtin_delete );
 FREE(VG_Z_LIBC_SONAME,        __builtin_delete,     __builtin_delete );
 
 FREE(VG_Z_LIBSTDCXX_SONAME,  _ZdlPv,               __builtin_delete );
 FREE(VG_Z_LIBC_SONAME,       _ZdlPv,               __builtin_delete );
 FREE(SO_SYN_MALLOC,          _ZdlPv,               __builtin_delete );

#elif defined(VGO_darwin)
 
 
 

#endif



#if defined(VGO_linux)
 
 FREE(VG_Z_LIBSTDCXX_SONAME, _ZdlPvRKSt9nothrow_t,  __builtin_delete );
 FREE(VG_Z_LIBC_SONAME,      _ZdlPvRKSt9nothrow_t,  __builtin_delete );
 FREE(SO_SYN_MALLOC,         _ZdlPvRKSt9nothrow_t,  __builtin_delete );

#elif defined(VGO_darwin)
 
 
 

#endif



#if defined(VGO_linux)
 
 FREE(VG_Z_LIBSTDCXX_SONAME,   __builtin_vec_delete, __builtin_vec_delete );
 FREE(VG_Z_LIBC_SONAME,        __builtin_vec_delete, __builtin_vec_delete );
 
 FREE(VG_Z_LIBSTDCXX_SONAME,  _ZdaPv,               __builtin_vec_delete );
 FREE(VG_Z_LIBC_SONAME,       _ZdaPv,               __builtin_vec_delete );
 FREE(SO_SYN_MALLOC,          _ZdaPv,               __builtin_vec_delete );

#elif defined(VGO_darwin)
 
 
 
 
 
 

#endif



#if defined(VGO_linux)
 
 FREE(VG_Z_LIBSTDCXX_SONAME,  _ZdaPvRKSt9nothrow_t, __builtin_vec_delete );
 FREE(VG_Z_LIBC_SONAME,       _ZdaPvRKSt9nothrow_t, __builtin_vec_delete );
 FREE(SO_SYN_MALLOC,          _ZdaPvRKSt9nothrow_t, __builtin_vec_delete );

#elif defined(VGO_darwin)
 
 
 

#endif



#define ZONECALLOC(soname, fnname) \
   \
   void* VG_REPLACE_FUNCTION_EZU(10060,soname,fnname) \
            ( void *zone, SizeT nmemb, SizeT size ); \
   void* VG_REPLACE_FUNCTION_EZU(10060,soname,fnname) \
            ( void *zone, SizeT nmemb, SizeT size )  \
   { \
      void* v; \
      \
      DO_INIT; \
      TRIGGER_MEMCHECK_ERROR_IF_UNDEFINED((UWord) zone); \
      TRIGGER_MEMCHECK_ERROR_IF_UNDEFINED(nmemb); \
      TRIGGER_MEMCHECK_ERROR_IF_UNDEFINED(size); \
      MALLOC_TRACE("zone_calloc(%p, %llu,%llu)", zone, (ULong)nmemb, (ULong)size ); \
      \
      v = (void*)VALGRIND_NON_SIMD_CALL2( info.tl_calloc, nmemb, size ); \
      MALLOC_TRACE(" = %p\n", v ); \
      return v; \
   }

#define CALLOC(soname, fnname) \
   \
   void* VG_REPLACE_FUNCTION_EZU(10070,soname,fnname) \
            ( SizeT nmemb, SizeT size ); \
   void* VG_REPLACE_FUNCTION_EZU(10070,soname,fnname) \
            ( SizeT nmemb, SizeT size )  \
   { \
      void* v; \
      \
      DO_INIT; \
      MALLOC_TRACE("calloc(%llu,%llu)", (ULong)nmemb, (ULong)size ); \
      \
 \
 \
      if (umulHW(size, nmemb) != 0) return NULL; \
      v = (void*)VALGRIND_NON_SIMD_CALL2( info.tl_calloc, nmemb, size ); \
      MALLOC_TRACE(" = %p\n", v ); \
      return v; \
   }

#if defined(VGO_linux)
 CALLOC(VG_Z_LIBC_SONAME, calloc);
 CALLOC(SO_SYN_MALLOC,    calloc);

#elif defined(VGO_darwin)
 CALLOC(VG_Z_LIBC_SONAME, calloc);
 CALLOC(SO_SYN_MALLOC,    calloc);
 ZONECALLOC(VG_Z_LIBC_SONAME, malloc_zone_calloc);
 ZONECALLOC(SO_SYN_MALLOC,    malloc_zone_calloc);

#endif



#define ZONEREALLOC(soname, fnname) \
   \
   void* VG_REPLACE_FUNCTION_EZU(10080,soname,fnname) \
            ( void *zone, void* ptrV, SizeT new_size ); \
   void* VG_REPLACE_FUNCTION_EZU(10080,soname,fnname) \
            ( void *zone, void* ptrV, SizeT new_size ) \
   { \
      void* v; \
      \
      DO_INIT; \
      MALLOC_TRACE("zone_realloc(%p,%p,%llu)", zone, ptrV, (ULong)new_size ); \
      \
      if (ptrV == NULL) \
 \
         return VG_REPLACE_FUNCTION_EZU(10010,VG_Z_LIBC_SONAME,malloc) \
                   (new_size); \
      if (new_size <= 0) { \
         VG_REPLACE_FUNCTION_EZU(10050,VG_Z_LIBC_SONAME,free)(ptrV); \
         MALLOC_TRACE(" = 0\n"); \
         return NULL; \
      } \
      v = (void*)VALGRIND_NON_SIMD_CALL2( info.tl_realloc, ptrV, new_size ); \
      MALLOC_TRACE(" = %p\n", v ); \
      return v; \
   }

#define REALLOC(soname, fnname) \
   \
   void* VG_REPLACE_FUNCTION_EZU(10090,soname,fnname) \
            ( void* ptrV, SizeT new_size );\
   void* VG_REPLACE_FUNCTION_EZU(10090,soname,fnname) \
            ( void* ptrV, SizeT new_size ) \
   { \
      void* v; \
      \
      DO_INIT; \
      MALLOC_TRACE("realloc(%p,%llu)", ptrV, (ULong)new_size ); \
      \
      if (ptrV == NULL) \
 \
         return VG_REPLACE_FUNCTION_EZU(10010,VG_Z_LIBC_SONAME,malloc) \
                   (new_size); \
      if (new_size <= 0) { \
         VG_REPLACE_FUNCTION_EZU(10050,VG_Z_LIBC_SONAME,free)(ptrV); \
         MALLOC_TRACE(" = 0\n"); \
         return NULL; \
      } \
      v = (void*)VALGRIND_NON_SIMD_CALL2( info.tl_realloc, ptrV, new_size ); \
      MALLOC_TRACE(" = %p\n", v ); \
      return v; \
   }

#if defined(VGO_linux)
 REALLOC(VG_Z_LIBC_SONAME, realloc);
 REALLOC(SO_SYN_MALLOC,    realloc);

#elif defined(VGO_darwin)
 REALLOC(VG_Z_LIBC_SONAME, realloc);
 REALLOC(SO_SYN_MALLOC,    realloc);
 ZONEREALLOC(VG_Z_LIBC_SONAME, malloc_zone_realloc);
 ZONEREALLOC(SO_SYN_MALLOC,    malloc_zone_realloc);

#endif



#define ZONEMEMALIGN(soname, fnname) \
   \
   void* VG_REPLACE_FUNCTION_EZU(10100,soname,fnname) \
            ( void *zone, SizeT alignment, SizeT n ); \
   void* VG_REPLACE_FUNCTION_EZU(10100,soname,fnname) \
            ( void *zone, SizeT alignment, SizeT n ) \
   { \
      void* v; \
      \
      DO_INIT; \
      TRIGGER_MEMCHECK_ERROR_IF_UNDEFINED((UWord) zone);	\
      TRIGGER_MEMCHECK_ERROR_IF_UNDEFINED(n); \
      MALLOC_TRACE("zone_memalign(%p, al %llu, size %llu)", \
                   zone, (ULong)alignment, (ULong)n );  \
      \
       \
      if (alignment < VG_MIN_MALLOC_SZB) \
         alignment = VG_MIN_MALLOC_SZB; \
      \
       \
      while (0 != (alignment & (alignment - 1))) alignment++; \
      \
      v = (void*)VALGRIND_NON_SIMD_CALL2( info.tl_memalign, alignment, n ); \
      MALLOC_TRACE(" = %p\n", v ); \
      return v; \
   }

#define MEMALIGN(soname, fnname) \
   \
   void* VG_REPLACE_FUNCTION_EZU(10110,soname,fnname) \
            ( SizeT alignment, SizeT n ); \
   void* VG_REPLACE_FUNCTION_EZU(10110,soname,fnname) \
            ( SizeT alignment, SizeT n )  \
   { \
      void* v; \
      \
      DO_INIT; \
      TRIGGER_MEMCHECK_ERROR_IF_UNDEFINED(n); \
      MALLOC_TRACE("memalign(al %llu, size %llu)", \
                   (ULong)alignment, (ULong)n ); \
      \
       \
      if (alignment < VG_MIN_MALLOC_SZB) \
         alignment = VG_MIN_MALLOC_SZB; \
      \
       \
      while (0 != (alignment & (alignment - 1))) alignment++; \
      \
      v = (void*)VALGRIND_NON_SIMD_CALL2( info.tl_memalign, alignment, n ); \
      MALLOC_TRACE(" = %p\n", v ); \
      return v; \
   }

#if defined(VGO_linux)
 MEMALIGN(VG_Z_LIBC_SONAME, memalign);
 MEMALIGN(SO_SYN_MALLOC,    memalign);

#elif defined(VGO_darwin)
 MEMALIGN(VG_Z_LIBC_SONAME, memalign);
 MEMALIGN(SO_SYN_MALLOC,    memalign);
 ZONEMEMALIGN(VG_Z_LIBC_SONAME, malloc_zone_memalign);
 ZONEMEMALIGN(SO_SYN_MALLOC,    malloc_zone_memalign);

#endif



#define VALLOC(soname, fnname) \
   \
   void* VG_REPLACE_FUNCTION_EZU(10120,soname,fnname) ( SizeT size ); \
   void* VG_REPLACE_FUNCTION_EZU(10120,soname,fnname) ( SizeT size ) \
   { \
      static int pszB = 0; \
      if (pszB == 0) \
         pszB = my_getpagesize(); \
      return VG_REPLACE_FUNCTION_EZU(10110,VG_Z_LIBC_SONAME,memalign) \
                ((SizeT)pszB, size); \
   }

#define ZONEVALLOC(soname, fnname) \
   \
   void* VG_REPLACE_FUNCTION_EZU(10130,soname,fnname) \
            ( void *zone, SizeT size ); \
   void* VG_REPLACE_FUNCTION_EZU(10130,soname,fnname) \
            ( void *zone, SizeT size )  \
   { \
      static int pszB = 0; \
      if (pszB == 0) \
         pszB = my_getpagesize(); \
      TRIGGER_MEMCHECK_ERROR_IF_UNDEFINED((UWord) zone);	      \
      return VG_REPLACE_FUNCTION_EZU(10110,VG_Z_LIBC_SONAME,memalign) \
                ((SizeT)pszB, size); \
   }

#if defined(VGO_linux)
 VALLOC(VG_Z_LIBC_SONAME, valloc);
 VALLOC(SO_SYN_MALLOC, valloc);

#elif defined(VGO_darwin)
 VALLOC(VG_Z_LIBC_SONAME, valloc);
 VALLOC(SO_SYN_MALLOC, valloc);
 ZONEVALLOC(VG_Z_LIBC_SONAME, malloc_zone_valloc);
 ZONEVALLOC(SO_SYN_MALLOC,    malloc_zone_valloc);

#endif




#define MALLOPT(soname, fnname) \
   \
   int VG_REPLACE_FUNCTION_EZU(10140,soname,fnname) ( int cmd, int value ); \
   int VG_REPLACE_FUNCTION_EZU(10140,soname,fnname) ( int cmd, int value ) \
   { \
 \
      TRIGGER_MEMCHECK_ERROR_IF_UNDEFINED(cmd); \
      TRIGGER_MEMCHECK_ERROR_IF_UNDEFINED(value); \
      return 1; \
   }

#if defined(VGO_linux)
 MALLOPT(VG_Z_LIBC_SONAME, mallopt);
 MALLOPT(SO_SYN_MALLOC,    mallopt);

#elif defined(VGO_darwin)
 

#endif


#define MALLOC_TRIM(soname, fnname) \
   \
   int VG_REPLACE_FUNCTION_EZU(10150,soname,fnname) ( SizeT pad ); \
   int VG_REPLACE_FUNCTION_EZU(10150,soname,fnname) ( SizeT pad ) \
   { \
 \
      TRIGGER_MEMCHECK_ERROR_IF_UNDEFINED(pad); \
      return 0; \
   }

#if defined(VGO_linux)
 MALLOC_TRIM(VG_Z_LIBC_SONAME, malloc_trim);
 MALLOC_TRIM(SO_SYN_MALLOC,    malloc_trim);

#elif defined(VGO_darwin)
 

#endif



#define POSIX_MEMALIGN(soname, fnname) \
   \
   int VG_REPLACE_FUNCTION_EZU(10160,soname,fnname) \
          ( void **memptr, SizeT alignment, SizeT size ); \
   int VG_REPLACE_FUNCTION_EZU(10160,soname,fnname) \
          ( void **memptr, SizeT alignment, SizeT size ) \
   { \
      void *mem; \
      \
 \
      if (alignment % sizeof (void *) != 0 \
          || (alignment & (alignment - 1)) != 0) \
         return VKI_EINVAL; \
      \
      mem = VG_REPLACE_FUNCTION_EZU(10110,VG_Z_LIBC_SONAME,memalign) \
               (alignment, size); \
      \
      if (mem != NULL) { \
        *memptr = mem; \
        return 0; \
      } \
      \
      return VKI_ENOMEM; \
   }

#if defined(VGO_linux)
 POSIX_MEMALIGN(VG_Z_LIBC_SONAME, posix_memalign);
 POSIX_MEMALIGN(SO_SYN_MALLOC,    posix_memalign);

#elif defined(VGO_darwin)
 

#endif



#define MALLOC_USABLE_SIZE(soname, fnname) \
   \
   SizeT VG_REPLACE_FUNCTION_EZU(10170,soname,fnname) ( void* p ); \
   SizeT VG_REPLACE_FUNCTION_EZU(10170,soname,fnname) ( void* p ) \
   {  \
      SizeT pszB; \
      \
      DO_INIT; \
      MALLOC_TRACE("malloc_usable_size(%p)", p ); \
      if (NULL == p) \
         return 0; \
      \
      pszB = (SizeT)VALGRIND_NON_SIMD_CALL1( info.tl_malloc_usable_size, p ); \
      MALLOC_TRACE(" = %llu\n", (ULong)pszB ); \
      \
      return pszB; \
   }

#if defined(VGO_linux)
 MALLOC_USABLE_SIZE(VG_Z_LIBC_SONAME, malloc_usable_size);
 MALLOC_USABLE_SIZE(SO_SYN_MALLOC,    malloc_usable_size);
 MALLOC_USABLE_SIZE(VG_Z_LIBC_SONAME, malloc_size);
 MALLOC_USABLE_SIZE(SO_SYN_MALLOC,    malloc_size);
# if defined(VGPV_arm_linux_android) || defined(VGPV_x86_linux_android) \
     || defined(VGPV_mips32_linux_android)
  MALLOC_USABLE_SIZE(VG_Z_LIBC_SONAME, dlmalloc_usable_size);
  MALLOC_USABLE_SIZE(SO_SYN_MALLOC,    dlmalloc_usable_size);
# endif

#elif defined(VGO_darwin)
 
 MALLOC_USABLE_SIZE(VG_Z_LIBC_SONAME, malloc_size);
 MALLOC_USABLE_SIZE(SO_SYN_MALLOC,    malloc_size);

#endif




static void panic(const char *str)
{
   VALGRIND_PRINTF_BACKTRACE("Program aborting because of call to %s\n", str);
   my_exit(99);
   *(volatile int *)0 = 'x';
}

#define PANIC(soname, fnname) \
   \
   void VG_REPLACE_FUNCTION_EZU(10180,soname,fnname) ( void ); \
   void VG_REPLACE_FUNCTION_EZU(10180,soname,fnname) ( void )  \
   { \
      panic(#fnname); \
   }

#if defined(VGO_linux)
 PANIC(VG_Z_LIBC_SONAME, pvalloc);
 PANIC(VG_Z_LIBC_SONAME, malloc_get_state);
 PANIC(VG_Z_LIBC_SONAME, malloc_set_state);

#elif defined(VGO_darwin)
 PANIC(VG_Z_LIBC_SONAME, pvalloc);
 PANIC(VG_Z_LIBC_SONAME, malloc_get_state);
 PANIC(VG_Z_LIBC_SONAME, malloc_set_state);

#endif


#define MALLOC_STATS(soname, fnname) \
   \
   void VG_REPLACE_FUNCTION_EZU(10190,soname,fnname) ( void ); \
   void VG_REPLACE_FUNCTION_EZU(10190,soname,fnname) ( void )  \
   { \
       \
   } 

#if defined(VGO_linux)
 MALLOC_STATS(VG_Z_LIBC_SONAME, malloc_stats);
 MALLOC_STATS(SO_SYN_MALLOC,    malloc_stats);

#elif defined(VGO_darwin)
 

#endif



#define MALLINFO(soname, fnname) \
   \
   struct vg_mallinfo VG_REPLACE_FUNCTION_EZU(10200,soname,fnname) ( void ); \
   struct vg_mallinfo VG_REPLACE_FUNCTION_EZU(10200,soname,fnname) ( void ) \
   { \
      static struct vg_mallinfo mi; \
      DO_INIT; \
      MALLOC_TRACE("mallinfo()\n"); \
      (void)VALGRIND_NON_SIMD_CALL1( info.mallinfo, &mi ); \
      return mi; \
   }

#if defined(VGO_linux)
 MALLINFO(VG_Z_LIBC_SONAME, mallinfo);
 MALLINFO(SO_SYN_MALLOC,    mallinfo);

#elif defined(VGO_darwin)
 

#endif



#if defined(VGO_darwin)

static size_t my_malloc_size ( void* zone, void* ptr )
{
   DO_INIT;
   TRIGGER_MEMCHECK_ERROR_IF_UNDEFINED((UWord) zone);
   TRIGGER_MEMCHECK_ERROR_IF_UNDEFINED((UWord) ptr);
   size_t res = (size_t)VALGRIND_NON_SIMD_CALL1(
                           info.tl_malloc_usable_size, ptr);
   return res;
}

static vki_malloc_zone_t vg_default_zone = {
    NULL, 
    NULL, 
    (void*)my_malloc_size, 
    (void*)VG_REPLACE_FUNCTION_EZU(10020,VG_Z_LIBC_SONAME,malloc_zone_malloc), 
    (void*)VG_REPLACE_FUNCTION_EZU(10060,VG_Z_LIBC_SONAME,malloc_zone_calloc), 
    (void*)VG_REPLACE_FUNCTION_EZU(10130,VG_Z_LIBC_SONAME,malloc_zone_valloc), 
    (void*)VG_REPLACE_FUNCTION_EZU(10040,VG_Z_LIBC_SONAME,malloc_zone_free), 
    (void*)VG_REPLACE_FUNCTION_EZU(10080,VG_Z_LIBC_SONAME,malloc_zone_realloc), 
    NULL, 
    "ValgrindMallocZone", 
    NULL, 
    NULL, 
    NULL, 
    2,  
    NULL,    
    NULL, 
    NULL, 
};


#define DEFAULT_ZONE(soname, fnname) \
   \
   void *VG_REPLACE_FUNCTION_EZU(10210,soname,fnname) ( void ); \
   void *VG_REPLACE_FUNCTION_EZU(10210,soname,fnname) ( void )  \
   { \
      return &vg_default_zone; \
   }

DEFAULT_ZONE(VG_Z_LIBC_SONAME, malloc_default_zone);
DEFAULT_ZONE(SO_SYN_MALLOC,    malloc_default_zone);
DEFAULT_ZONE(VG_Z_LIBC_SONAME, malloc_default_purgeable_zone);
DEFAULT_ZONE(SO_SYN_MALLOC,    malloc_default_purgeable_zone);


#define CREATE_ZONE(soname, fnname) \
   \
   void *VG_REPLACE_FUNCTION_EZU(10220,soname,fnname)(size_t sz, unsigned fl); \
   void *VG_REPLACE_FUNCTION_EZU(10220,soname,fnname)(size_t sz, unsigned fl)  \
   { \
      return &vg_default_zone; \
   }
CREATE_ZONE(VG_Z_LIBC_SONAME, malloc_create_zone);


#define ZONE_FROM_PTR(soname, fnname) \
   \
   void *VG_REPLACE_FUNCTION_EZU(10230,soname,fnname) ( void* ptr ); \
   void *VG_REPLACE_FUNCTION_EZU(10230,soname,fnname) ( void* ptr )  \
   { \
      return &vg_default_zone; \
   }

ZONE_FROM_PTR(VG_Z_LIBC_SONAME, malloc_zone_from_ptr);
ZONE_FROM_PTR(SO_SYN_MALLOC,    malloc_zone_from_ptr);


#define ZONE_CHECK(soname, fnname) \
   \
   int VG_REPLACE_FUNCTION_EZU(10240,soname,fnname)(void* zone); \
   int VG_REPLACE_FUNCTION_EZU(10240,soname,fnname)(void* zone)  \
   { \
      TRIGGER_MEMCHECK_ERROR_IF_UNDEFINED(zone); \
      panic(#fnname); \
      return 1; \
   }

ZONE_CHECK(VG_Z_LIBC_SONAME, malloc_zone_check);    
ZONE_CHECK(SO_SYN_MALLOC,    malloc_zone_check);


#define ZONE_REGISTER(soname, fnname) \
   \
   void VG_REPLACE_FUNCTION_EZU(10250,soname,fnname)(void* zone); \
   void VG_REPLACE_FUNCTION_EZU(10250,soname,fnname)(void* zone)  \
   { \
      TRIGGER_MEMCHECK_ERROR_IF_UNDEFINED(zone); \
   }

ZONE_REGISTER(VG_Z_LIBC_SONAME, malloc_zone_register);
ZONE_REGISTER(SO_SYN_MALLOC,    malloc_zone_register);


#define ZONE_UNREGISTER(soname, fnname) \
   \
   void VG_REPLACE_FUNCTION_EZU(10260,soname,fnname)(void* zone); \
   void VG_REPLACE_FUNCTION_EZU(10260,soname,fnname)(void* zone)  \
   { \
      TRIGGER_MEMCHECK_ERROR_IF_UNDEFINED(zone); \
   }

ZONE_UNREGISTER(VG_Z_LIBC_SONAME, malloc_zone_unregister);
ZONE_UNREGISTER(SO_SYN_MALLOC,    malloc_zone_unregister);


#define ZONE_SET_NAME(soname, fnname) \
   \
   void VG_REPLACE_FUNCTION_EZU(10270,soname,fnname)(void* zone, char* nm); \
   void VG_REPLACE_FUNCTION_EZU(10270,soname,fnname)(void* zone, char* nm)  \
   { \
      TRIGGER_MEMCHECK_ERROR_IF_UNDEFINED(zone); \
   }

ZONE_SET_NAME(VG_Z_LIBC_SONAME, malloc_set_zone_name);
ZONE_SET_NAME(SO_SYN_MALLOC,    malloc_set_zone_name);


#define ZONE_GET_NAME(soname, fnname) \
   \
   char* VG_REPLACE_FUNCTION_EZU(10280,soname,fnname)(void* zone); \
   char* VG_REPLACE_FUNCTION_EZU(10280,soname,fnname)(void* zone)  \
   { \
      TRIGGER_MEMCHECK_ERROR_IF_UNDEFINED(zone); \
      return vg_default_zone.zone_name; \
   }

ZONE_SET_NAME(VG_Z_LIBC_SONAME, malloc_get_zone_name);
ZONE_SET_NAME(SO_SYN_MALLOC,    malloc_get_zone_name);

#endif 




__attribute__((constructor))
static void init(void)
{
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   if (init_done)
      return;

   init_done = 1;

   VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__GET_MALLOCFUNCS, &info,
                                   0, 0, 0, 0);
}

