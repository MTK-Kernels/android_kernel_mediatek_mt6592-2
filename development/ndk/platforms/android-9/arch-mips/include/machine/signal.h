
/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell.
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
 *	@(#)signal.h	8.1 (Berkeley) 6/10/93
 */

#ifndef _MIPS_SIGNAL_H_
#define _MIPS_SIGNAL_H_

#include <sys/cdefs.h>

#if !defined(__LANGUAGE_ASSEMBLY)
#include <sys/types.h>

typedef int sig_atomic_t;

#if __BSD_VISIBLE || __XPG_VISIBLE >= 420


#if defined(__ANDROID__)


struct sigcontext {
	unsigned int sc_regmask;
	unsigned int sc_status;
	unsigned long long sc_pc;
	unsigned long long sc_regs[32];
	unsigned long long sc_fpregs[32];
	unsigned int sc_acx;
	unsigned int sc_fpc_csr;
	unsigned int sc_fpc_eir;
	unsigned int sc_used_math;
	unsigned int sc_dsp;
	unsigned long long sc_mdhi;
	unsigned long long sc_mdlo;
	unsigned long sc_hi1;
	unsigned long sc_lo1;
	unsigned long sc_hi2;
	unsigned long sc_lo2;
	unsigned long sc_hi3;
	unsigned long sc_lo3;
};

#else

struct	sigcontext {
	long	sc_onstack;	
	long	 sc_mask;	
	__register_t sc_pc;	
	__register_t sc_regs[32]; 
	__register_t mullo;	
	__register_t mulhi;	
	f_register_t sc_fpregs[33]; 
	long	sc_fpused;	
	long	sc_fpc_eir;	
	long	xxx[8];		
};
#endif
#endif 

#else 

#ifdef __ANDROID__

#define	SC_REGMASK	(0*REGSZ)
#define	SC_STATUS	(1*REGSZ)
#define	SC_PC		(2*REGSZ)
#define	SC_REGS		(SC_PC+8)
#define	SC_FPREGS	(SC_REGS+32*8)
#define	SC_ACX		(SC_FPREGS+32*REGSZ_FP)
#define	SC_FPC_CSR	(SC_ACX+1*REGSZ)
#define	SC_FPC_EIR	(SC_ACX+2*REGSZ)
#define	SC_USED_MATH	(SC_ACX+3*REGSZ)
#define	SC_DSP		(SC_ACX+4*REGSZ)
#define	SC_MDHI		(SC_ACX+5*REGSZ)
#define	SC_MDLO		(SC_MDHI+8)
#define	SC_HI1		(SC_MDLO+8)
#define	SC_LO1		(SC_HI1+1*REGSZ)
#define	SC_HI2		(SC_HI1+2*REGSZ)
#define	SC_LO2		(SC_HI1+3*REGSZ)
#define	SC_HI3		(SC_HI1+4*REGSZ)
#define	SC_LO3		(SC_HI1+5*REGSZ)
#define	SC_MASK		SC_REGMASK
#define	SC_FPUSED	SC_USED_MATH

#else

#define SC_ONSTACK	(0 * REGSZ)
#define	SC_MASK		(1 * REGSZ)
#define	SC_PC		(2 * REGSZ)
#define	SC_REGS		(3 * REGSZ)
#define	SC_MULLO	(35 * REGSZ)
#define	SC_MULHI	(36 * REGSZ)
#define	SC_FPREGS	(37 * REGSZ)
#define	SC_FPUSED	(70 * REGSZ)
#define	SC_FPC_EIR	(71 * REGSZ)

#endif 

#endif 

#endif	
