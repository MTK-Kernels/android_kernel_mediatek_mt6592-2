
/*
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Berkeley Software Design, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)cdefs.h	8.8 (Berkeley) 1/9/95
 */

#ifndef	_SYS_CDEFS_H_
#define	_SYS_CDEFS_H_

#ifdef __GNUC__
#define	__GNUC_PREREQ__(x, y)						\
	((__GNUC__ == (x) && __GNUC_MINOR__ >= (y)) ||			\
	 (__GNUC__ > (x)))
#else
#define	__GNUC_PREREQ__(x, y)	0
#endif

#include <sys/cdefs_elf.h>

#if defined(__cplusplus)
#define	__BEGIN_DECLS		extern "C" {
#define	__END_DECLS		}
#define	__static_cast(x,y)	static_cast<x>(y)
#else
#define	__BEGIN_DECLS
#define	__END_DECLS
#define	__static_cast(x,y)	(x)y
#endif


#define	___STRING(x)	__STRING(x)
#define	___CONCAT(x,y)	__CONCAT(x,y)

#if defined(__STDC__) || defined(__cplusplus)
#define	__P(protos)	protos		
#define	__CONCAT(x,y)	x ## y
#define	__STRING(x)	#x

#define	__const		const		
#define	__signed	signed
#define	__volatile	volatile
#if defined(__cplusplus)
#define	__inline	inline		
#else
#if !defined(__GNUC__) && !defined(__lint__)
#define	__inline			
#endif 
#endif 

#else	
#define	__P(protos)	()		
#define	__CONCAT(x,y)	xy
#define	__STRING(x)	"x"

#ifndef __GNUC__
#define	__const				
#define	__inline
#define	__signed
#define	__volatile
#endif	

#ifndef	NO_ANSI_KEYWORDS
#define	const		__const		
#define	inline		__inline
#define	signed		__signed
#define	volatile	__volatile
#endif 
#endif	

#ifdef __AUDIT__
#define	__aconst	__const
#else
#define	__aconst
#endif

#define __UNCONST(a)	((void *)(unsigned long)(const void *)(a))

#if !__GNUC_PREREQ__(2, 0)
#define	__extension__		
#endif

#if !__GNUC_PREREQ__(2, 5)
#define	__attribute__(x)	
#if defined(__GNUC__) && !defined(__STRICT_ANSI__)
#define	__dead		__volatile
#define	__pure		__const
#endif
#endif

#ifndef __dead
#define	__dead
#define	__pure
#endif

#if __GNUC_PREREQ__(2, 7)
#define	__unused	__attribute__((__unused__))
#else
#define	__unused	
#endif

#if __GNUC_PREREQ__(3, 1)
#define	__used		__attribute__((__used__))
#else
#define	__used		
#endif

#if __GNUC_PREREQ__(2, 7)
#define	__packed	__attribute__((__packed__))
#define	__aligned(x)	__attribute__((__aligned__(x)))
#define	__section(x)	__attribute__((__section__(x)))
#elif defined(__lint__)
#define	__packed	
#define	__aligned(x)	
#define	__section(x)	
#else
#define	__packed	error: no __packed for this compiler
#define	__aligned(x)	error: no __aligned for this compiler
#define	__section(x)	error: no __section for this compiler
#endif

#if !__GNUC_PREREQ__(2, 8)
#define	__extension__
#endif

#if __GNUC_PREREQ__(2, 8)
#define __statement(x)	__extension__(x)
#elif defined(lint)
#define __statement(x)	(0)
#else
#define __statement(x)	(x)
#endif

#if defined(__STDC__VERSION__) && __STDC_VERSION__ >= 199901L
#define	__restrict	restrict
#else
#if !__GNUC_PREREQ__(2, 92)
#define	__restrict	
#endif
#endif

#if !defined(__STDC_VERSION__) || !(__STDC_VERSION__ >= 199901L)
#if __GNUC_PREREQ__(2, 6)
#define	__func__	__PRETTY_FUNCTION__
#elif __GNUC_PREREQ__(2, 4)
#define	__func__	__FUNCTION__
#else
#define	__func__	""
#endif
#endif 

#if defined(_KERNEL)
#if defined(NO_KERNEL_RCSIDS)
#undef __KERNEL_RCSID
#define	__KERNEL_RCSID(_n, _s)		
#endif 
#endif 

#if !defined(_STANDALONE) && !defined(_KERNEL)
#ifdef __GNUC__
#define	__RENAME(x)	___RENAME(x)
#else
#ifdef __lint__
#define	__RENAME(x)	__symbolrename(x)
#else
#error "No function renaming possible"
#endif 
#endif 
#else 
#define	__RENAME(x)	no renaming in kernel or standalone environment
#endif

#if __GNUC_PREREQ__(2, 95)
#define	__insn_barrier()	__asm __volatile("":::"memory")
#else
#define	__insn_barrier()	
#endif

#if __GNUC_PREREQ__(2, 96)
#define	__predict_true(exp)	__builtin_expect((exp) != 0, 1)
#define	__predict_false(exp)	__builtin_expect((exp) != 0, 0)
#else
#define	__predict_true(exp)	(exp)
#define	__predict_false(exp)	(exp)
#endif

#if __GNUC_PREREQ__(2, 96)
#define __noreturn    __attribute__((__noreturn__))
#define __mallocfunc  __attribute__((malloc))
#else
#define __noreturn
#define __mallocfunc
#endif

#define	__link_set_foreach(pvar, set)					\
	for (pvar = __link_set_start(set); pvar < __link_set_end(set); pvar++)

#define	__link_set_entry(set, idx)	(__link_set_begin(set)[idx])

#define	__FBSDID(s)	struct __hack


#if defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE == 1
#undef _POSIX_C_SOURCE		
#define	_POSIX_C_SOURCE		199009
#endif

#if defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE == 2
#undef _POSIX_C_SOURCE
#define	_POSIX_C_SOURCE		199209
#endif

#ifdef _XOPEN_SOURCE
#if _XOPEN_SOURCE - 0 >= 700
#define	__XSI_VISIBLE		700
#undef _POSIX_C_SOURCE
#define	_POSIX_C_SOURCE		200809
#elif _XOPEN_SOURCE - 0 >= 600
#define	__XSI_VISIBLE		600
#undef _POSIX_C_SOURCE
#define	_POSIX_C_SOURCE		200112
#elif _XOPEN_SOURCE - 0 >= 500
#define	__XSI_VISIBLE		500
#undef _POSIX_C_SOURCE
#define	_POSIX_C_SOURCE		199506
#endif
#endif

#if defined(_POSIX_SOURCE) && !defined(_POSIX_C_SOURCE)
#define	_POSIX_C_SOURCE		198808
#endif
#ifdef _POSIX_C_SOURCE
#if _POSIX_C_SOURCE >= 200809
#define	__POSIX_VISIBLE		200809
#define	__ISO_C_VISIBLE		1999
#elif _POSIX_C_SOURCE >= 200112
#define	__POSIX_VISIBLE		200112
#define	__ISO_C_VISIBLE		1999
#elif _POSIX_C_SOURCE >= 199506
#define	__POSIX_VISIBLE		199506
#define	__ISO_C_VISIBLE		1990
#elif _POSIX_C_SOURCE >= 199309
#define	__POSIX_VISIBLE		199309
#define	__ISO_C_VISIBLE		1990
#elif _POSIX_C_SOURCE >= 199209
#define	__POSIX_VISIBLE		199209
#define	__ISO_C_VISIBLE		1990
#elif _POSIX_C_SOURCE >= 199009
#define	__POSIX_VISIBLE		199009
#define	__ISO_C_VISIBLE		1990
#else
#define	__POSIX_VISIBLE		198808
#define	__ISO_C_VISIBLE		0
#endif 
#else
#if defined(_ANSI_SOURCE)	
#define	__POSIX_VISIBLE		0
#define	__XSI_VISIBLE		0
#define	__BSD_VISIBLE		0
#define	__ISO_C_VISIBLE		1990
#elif defined(_C99_SOURCE)	
#define	__POSIX_VISIBLE		0
#define	__XSI_VISIBLE		0
#define	__BSD_VISIBLE		0
#define	__ISO_C_VISIBLE		1999
#else				
#define	__POSIX_VISIBLE		200809
#define	__XSI_VISIBLE		700
#define	__BSD_VISIBLE		1
#define	__ISO_C_VISIBLE		1999
#endif
#endif

#ifndef __XPG_VISIBLE
# define __XPG_VISIBLE          700
#endif
#ifndef __POSIX_VISIBLE
# define __POSIX_VISIBLE        200809
#endif
#ifndef __ISO_C_VISIBLE
# define __ISO_C_VISIBLE        1999
#endif
#ifndef __BSD_VISIBLE
# define __BSD_VISIBLE          1
#endif

#define  __BIONIC__   1
#include <android/api-level.h>

#if defined(__ANDROID__) && !defined(__LP64__) && defined( __arm__)
#define __NDK_FPABI__ __attribute__((pcs("aapcs")))
#else
#define __NDK_FPABI__
#endif

#if (!defined(_NDK_MATH_NO_SOFTFP) || _NDK_MATH_NO_SOFTFP != 1) && !defined(__clang__)
#define __NDK_FPABI_MATH__ __NDK_FPABI__
#else
#define __NDK_FPABI_MATH__  
#endif

#endif 
