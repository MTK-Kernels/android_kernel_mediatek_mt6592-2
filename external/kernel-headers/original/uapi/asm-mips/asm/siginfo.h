/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1998, 1999, 2001, 2003 Ralf Baechle
 * Copyright (C) 2000, 2001 Silicon Graphics, Inc.
 */
#ifndef _UAPI_ASM_SIGINFO_H
#define _UAPI_ASM_SIGINFO_H


#define __ARCH_SIGEV_PREAMBLE_SIZE (sizeof(long) + 2*sizeof(int))
#undef __ARCH_SI_TRAPNO 

#define HAVE_ARCH_SIGINFO_T

#define HAVE_ARCH_COPY_SIGINFO
struct siginfo;

#if _MIPS_SZLONG == 32
#define __ARCH_SI_PREAMBLE_SIZE (3 * sizeof(int))
#elif _MIPS_SZLONG == 64
#define __ARCH_SI_PREAMBLE_SIZE (4 * sizeof(int))
#else
#error _MIPS_SZLONG neither 32 nor 64
#endif

#define __ARCH_SIGSYS

#include <asm-generic/siginfo.h>

typedef struct siginfo {
	int si_signo;
	int si_code;
	int si_errno;
	int __pad0[SI_MAX_SIZE / sizeof(int) - SI_PAD_SIZE - 3];

	union {
		int _pad[SI_PAD_SIZE];

		
		struct {
			pid_t _pid;		
			__ARCH_SI_UID_T _uid;	
		} _kill;

		
		struct {
			timer_t _tid;		
			int _overrun;		
			char _pad[sizeof( __ARCH_SI_UID_T) - sizeof(int)];
			sigval_t _sigval;	
			int _sys_private;	
		} _timer;

		
		struct {
			pid_t _pid;		
			__ARCH_SI_UID_T _uid;	
			sigval_t _sigval;
		} _rt;

		
		struct {
			pid_t _pid;		
			__ARCH_SI_UID_T _uid;	
			int _status;		
			clock_t _utime;
			clock_t _stime;
		} _sigchld;

		
		struct {
			pid_t _pid;		
			clock_t _utime;
			int _status;		
			clock_t _stime;
		} _irix_sigchld;

		
		struct {
			void __user *_addr; 
#ifdef __ARCH_SI_TRAPNO
			int _trapno;	
#endif
			short _addr_lsb;
		} _sigfault;

		
		struct {
			__ARCH_SI_BAND_T _band; 
			int _fd;
		} _sigpoll;

		
		struct {
			void __user *_call_addr; 
			int _syscall;	
			unsigned int _arch;	
		} _sigsys;
	} _sifields;
} siginfo_t;

#undef SI_ASYNCIO
#undef SI_TIMER
#undef SI_MESGQ
#define SI_ASYNCIO	-2	
#define SI_TIMER __SI_CODE(__SI_TIMER, -3) 
#define SI_MESGQ __SI_CODE(__SI_MESGQ, -4) 


#endif 
