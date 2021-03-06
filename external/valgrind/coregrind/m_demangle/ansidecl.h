/* ANSI and traditional C compatability macros
   Copyright 1991, 1992, 1993, 1994, 1995, 1996, 1998, 1999, 2000, 2001,
   2002, 2003, 2004, 2005, 2006, 2007, 2009, 2010, 2013
   Free Software Foundation, Inc.
   This file is part of the GNU C Library.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA.  */


#ifndef	_ANSIDECL_H
#define _ANSIDECL_H	1

#ifdef __cplusplus
extern "C" {
#endif



#ifndef GCC_VERSION
#define GCC_VERSION (__GNUC__ * 1000 + __GNUC_MINOR__)
#endif 

#if defined (__STDC__) || defined(__cplusplus) || defined (_AIX) || (defined (__mips) && defined (_SYSTYPE_SVR4)) || defined(_WIN32)

#define PTR		void *

#undef const
#undef volatile
#undef signed

#undef inline
#if __STDC_VERSION__ >= 199901L || defined(__cplusplus) || (defined(__SUNPRO_C) && defined(__C99FEATURES__))
#else
# if GCC_VERSION >= 2007
#  define inline __inline__   
# else
#  define inline  
# endif
#endif

#else	

#define PTR		char *

#undef const
#undef volatile
#undef signed
#undef inline
#define const
#define volatile
#define signed
#define inline

#endif	


#if (GCC_VERSION < 2007)
# define __attribute__(x)
#endif

#ifndef ATTRIBUTE_MALLOC
# if (GCC_VERSION >= 2096)
#  define ATTRIBUTE_MALLOC __attribute__ ((__malloc__))
# else
#  define ATTRIBUTE_MALLOC
# endif 
#endif 

#ifndef ATTRIBUTE_UNUSED_LABEL
# ifndef __cplusplus
#  if GCC_VERSION >= 2093
#   define ATTRIBUTE_UNUSED_LABEL ATTRIBUTE_UNUSED
#  else
#   define ATTRIBUTE_UNUSED_LABEL
#  endif
# else
#  if GCC_VERSION >= 4005
#   define ATTRIBUTE_UNUSED_LABEL ATTRIBUTE_UNUSED ;
#  else
#   define ATTRIBUTE_UNUSED_LABEL
#  endif
# endif
#endif

#ifndef ATTRIBUTE_UNUSED
#if GCC_VERSION >= 3004
#  define ATTRIBUTE_UNUSED __attribute__ ((__unused__))
#else
#define ATTRIBUTE_UNUSED
#endif
#endif 

#if ! defined(__cplusplus) || (GCC_VERSION >= 3004)
# define ARG_UNUSED(NAME) NAME ATTRIBUTE_UNUSED
#else 
# define ARG_UNUSED(NAME) NAME
#endif 

#ifndef ATTRIBUTE_NORETURN
#define ATTRIBUTE_NORETURN __attribute__ ((__noreturn__))
#endif 

#ifndef ATTRIBUTE_NONNULL
# if (GCC_VERSION >= 3003)
#  define ATTRIBUTE_NONNULL(m) __attribute__ ((__nonnull__ (m)))
# else
#  define ATTRIBUTE_NONNULL(m)
# endif 
#endif 

#ifndef ATTRIBUTE_RETURNS_NONNULL
# if (GCC_VERSION >= 4009)
#  define ATTRIBUTE_RETURNS_NONNULL __attribute__ ((__returns_nonnull__))
# else
#  define ATTRIBUTE_RETURNS_NONNULL
# endif 
#endif 

#ifndef ATTRIBUTE_PURE
# if (GCC_VERSION >= 3000)
#  define ATTRIBUTE_PURE __attribute__ ((__pure__))
# else
#  define ATTRIBUTE_PURE
# endif 
#endif 

#ifndef ATTRIBUTE_PRINTF
#define ATTRIBUTE_PRINTF(m, n) __attribute__ ((__format__ (__printf__, m, n))) ATTRIBUTE_NONNULL(m)
#define ATTRIBUTE_PRINTF_1 ATTRIBUTE_PRINTF(1, 2)
#define ATTRIBUTE_PRINTF_2 ATTRIBUTE_PRINTF(2, 3)
#define ATTRIBUTE_PRINTF_3 ATTRIBUTE_PRINTF(3, 4)
#define ATTRIBUTE_PRINTF_4 ATTRIBUTE_PRINTF(4, 5)
#define ATTRIBUTE_PRINTF_5 ATTRIBUTE_PRINTF(5, 6)
#endif 

#ifndef ATTRIBUTE_FPTR_PRINTF
# if (GCC_VERSION >= 3001)
#  define ATTRIBUTE_FPTR_PRINTF(m, n) ATTRIBUTE_PRINTF(m, n)
# else
#  define ATTRIBUTE_FPTR_PRINTF(m, n)
# endif 
# define ATTRIBUTE_FPTR_PRINTF_1 ATTRIBUTE_FPTR_PRINTF(1, 2)
# define ATTRIBUTE_FPTR_PRINTF_2 ATTRIBUTE_FPTR_PRINTF(2, 3)
# define ATTRIBUTE_FPTR_PRINTF_3 ATTRIBUTE_FPTR_PRINTF(3, 4)
# define ATTRIBUTE_FPTR_PRINTF_4 ATTRIBUTE_FPTR_PRINTF(4, 5)
# define ATTRIBUTE_FPTR_PRINTF_5 ATTRIBUTE_FPTR_PRINTF(5, 6)
#endif 

#ifndef ATTRIBUTE_NULL_PRINTF
# if (GCC_VERSION >= 3003)
#  define ATTRIBUTE_NULL_PRINTF(m, n) __attribute__ ((__format__ (__printf__, m, n)))
# else
#  define ATTRIBUTE_NULL_PRINTF(m, n)
# endif 
# define ATTRIBUTE_NULL_PRINTF_1 ATTRIBUTE_NULL_PRINTF(1, 2)
# define ATTRIBUTE_NULL_PRINTF_2 ATTRIBUTE_NULL_PRINTF(2, 3)
# define ATTRIBUTE_NULL_PRINTF_3 ATTRIBUTE_NULL_PRINTF(3, 4)
# define ATTRIBUTE_NULL_PRINTF_4 ATTRIBUTE_NULL_PRINTF(4, 5)
# define ATTRIBUTE_NULL_PRINTF_5 ATTRIBUTE_NULL_PRINTF(5, 6)
#endif 

#ifndef ATTRIBUTE_SENTINEL
# if (GCC_VERSION >= 3005)
#  define ATTRIBUTE_SENTINEL __attribute__ ((__sentinel__))
# else
#  define ATTRIBUTE_SENTINEL
# endif 
#endif 


#ifndef ATTRIBUTE_ALIGNED_ALIGNOF
# if (GCC_VERSION >= 3000)
#  define ATTRIBUTE_ALIGNED_ALIGNOF(m) __attribute__ ((__aligned__ (__alignof__ (m))))
# else
#  define ATTRIBUTE_ALIGNED_ALIGNOF(m)
# endif 
#endif 

#ifndef ATTRIBUTE_PACKED
# define ATTRIBUTE_PACKED __attribute__ ((packed))
#endif

#ifndef ATTRIBUTE_COLD
# if (GCC_VERSION >= 4003)
#  define ATTRIBUTE_COLD __attribute__ ((__cold__))
# else
#  define ATTRIBUTE_COLD
# endif 
#endif 
#ifndef ATTRIBUTE_HOT
# if (GCC_VERSION >= 4003)
#  define ATTRIBUTE_HOT __attribute__ ((__hot__))
# else
#  define ATTRIBUTE_HOT
# endif 
#endif 

#if GCC_VERSION < 2008
#define __extension__
#endif

#ifdef __cplusplus
#define EXPORTED_CONST extern const
#else
#define EXPORTED_CONST const
#endif


#ifdef __cplusplus
#define ENUM_BITFIELD(TYPE) enum TYPE
#elif (GCC_VERSION > 2000)
#define ENUM_BITFIELD(TYPE) __extension__ enum TYPE
#else
#define ENUM_BITFIELD(TYPE) unsigned int
#endif

#ifdef __cplusplus
}
#endif

#endif	
