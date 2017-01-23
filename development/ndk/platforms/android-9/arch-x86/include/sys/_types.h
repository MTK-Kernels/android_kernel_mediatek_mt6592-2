
/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)types.h	8.3 (Berkeley) 1/5/94
 */

#ifndef _SYS__TYPES_H_
#define	_SYS__TYPES_H_

#undef  __KERNEL_STRICT_NAMES
#define __KERNEL_STRICT_NAMES  1

#include <machine/_types.h>

typedef	unsigned long	__cpuid_t;	
typedef	__int32_t	__dev_t;	
typedef	__uint32_t	__fixpt_t;	
typedef	__uint32_t	__gid_t;	
typedef	__uint32_t	__id_t;		
typedef __uint32_t	__in_addr_t;	
typedef __uint16_t	__in_port_t;	
typedef	__uint32_t	__ino_t;	
typedef	long		__key_t;	
typedef	__uint32_t	__mode_t;	
typedef	__uint32_t	__nlink_t;	
typedef	__int32_t	__pid_t;	
typedef __uint64_t	__rlim_t;	
typedef __uint16_t	__sa_family_t;	
typedef	__int32_t	__segsz_t;	
typedef __uint32_t	__socklen_t;	
typedef	__int32_t	__swblk_t;	
typedef	__uint32_t	__uid_t;	
typedef	__uint32_t	__useconds_t;	
typedef	__int32_t	__suseconds_t;	

typedef union {
	char __mbstate8[128];
	__int64_t __mbstateL;			
} __mbstate_t;

#define  __KERNEL_STRICT_NAMES  1

#endif 
