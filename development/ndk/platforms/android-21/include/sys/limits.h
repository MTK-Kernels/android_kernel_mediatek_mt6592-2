/*
 * Copyright (c) 2002 Marc Espie.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE OPENBSD PROJECT AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OPENBSD
 * PROJECT OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef _SYS_LIMITS_H_
#define _SYS_LIMITS_H_

#include <sys/cdefs.h>
#include <linux/limits.h>


#define	CHAR_BIT	8		

#define	SCHAR_MAX	0x7f		
#define SCHAR_MIN	(-0x7f-1)	

#define	UCHAR_MAX	0xffU		
#ifdef __CHAR_UNSIGNED__
# define CHAR_MIN	0		
# define CHAR_MAX	0xff		
#else
# define CHAR_MAX	0x7f
# define CHAR_MIN	(-0x7f-1)
#endif

#define	USHRT_MAX	0xffffU		
#define	SHRT_MAX	0x7fff		
#define SHRT_MIN        (-0x7fff-1)     

#define	UINT_MAX	0xffffffffU	
#define	INT_MAX		0x7fffffff	
#define	INT_MIN		(-0x7fffffff-1)	

#ifdef __LP64__
# define ULONG_MAX	0xffffffffffffffffUL
					
# define LONG_MAX	0x7fffffffffffffffL
					
# define LONG_MIN	(-0x7fffffffffffffffL-1)
					
#else
# define ULONG_MAX	0xffffffffUL	
# define LONG_MAX	0x7fffffffL	
# define LONG_MIN	(-0x7fffffffL-1)
#endif

#if __BSD_VISIBLE || __ISO_C_VISIBLE >= 1999
# define ULLONG_MAX	0xffffffffffffffffULL
					
# define LLONG_MAX	0x7fffffffffffffffLL
					
# define LLONG_MIN	(-0x7fffffffffffffffLL-1)
					
#endif

#if __BSD_VISIBLE
# define UID_MAX	UINT_MAX	
# define GID_MAX	UINT_MAX	
#endif


#ifdef __LP64__
# define LONG_BIT	64
#else
# define LONG_BIT	32
#endif

# if !defined(DBL_DIG)
#  if defined(__DBL_DIG)
#   define DBL_DIG	__DBL_DIG
#   define DBL_MAX	__DBL_MAX
#   define DBL_MIN	__DBL_MIN

#   define FLT_DIG	__FLT_DIG
#   define FLT_MAX	__FLT_MAX
#   define FLT_MIN	__FLT_MIN
#  else
#   define DBL_DIG	15
#   define DBL_MAX	1.7976931348623157E+308
#   define DBL_MIN	2.2250738585072014E-308

#   define FLT_DIG	6
#   define FLT_MAX	3.40282347E+38F
#   define FLT_MIN	1.17549435E-38F
#  endif
# endif


#define  CHILD_MAX   999
#define  OPEN_MAX    256


#define  _POSIX_VERSION             200112L   
#define  _POSIX2_VERSION            -1        
#define  _POSIX2_C_VERSION          _POSIX_VERSION
#define  _XOPEN_VERSION             500       
#define  _XOPEN_XCU_VERSION         -1        

#if _POSIX_VERSION > 0
#define  _XOPEN_XPG2                1
#define  _XOPEN_XPG3                1
#define  _XOPEN_XPG4                1
#define  _XOPEN_UNIX                1
#endif

#define  _XOPEN_ENH_I18N          -1  
#define  _XOPEN_CRYPT             -1  
#define  _XOPEN_LEGACY            -1  
#define  _XOPEN_REALTIME          -1 
#define  _XOPEN_REALTIME_THREADS  -1  

#define  _POSIX_REALTIME_SIGNALS    -1  
#define  _POSIX_PRIORITY_SCHEDULING  1  
#define  _POSIX_TIMERS               1  
#undef   _POSIX_ASYNCHRONOUS_IO         
#define  _POSIX_SYNCHRONIZED_IO      1  
#define  _POSIX_FSYNC                1  
#define  _POSIX_MAPPED_FILES         1  



#define  _POSIX_THREADS             1    
#define  _POSIX_THREAD_STACKADDR    1    
#define  _POSIX_THREAD_STACKSIZE    1    
#define  _POSIX_THREAD_PRIO_INHERIT 200112L   
#define  _POSIX_THREAD_PRIO_PROTECT 200112L   

#undef   _POSIX_PROCESS_SHARED           
#undef   _POSIX_THREAD_SAFE_FUNCTIONS    
#define  _POSIX_CHOWN_RESTRICTED    1    
#define  _POSIX_MONOTONIC_CLOCK     0    
#define  _POSIX_NO_TRUNC            1    
#define  _POSIX_SAVED_IDS           1    
#define  _POSIX_JOB_CONTROL         1    

#define _POSIX_THREAD_DESTRUCTOR_ITERATIONS 4 
#define PTHREAD_DESTRUCTOR_ITERATIONS _POSIX_THREAD_DESTRUCTOR_ITERATIONS
#define _POSIX_THREAD_KEYS_MAX 128            
#define PTHREAD_KEYS_MAX _POSIX_THREAD_KEYS_MAX
#define _POSIX_THREAD_THREADS_MAX 64          
#define PTHREAD_THREADS_MAX                   


#endif
