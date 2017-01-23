
/*
 * Copyright (c) 2001-2002 Opsycon AB  (www.opsycon.se / www.opsycon.com)
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
#ifndef _MIPS64_ASM_H
#define _MIPS64_ASM_H

#include <machine/regdef.h>

#ifdef NEED_OLD_RM7KFIX
#define ITLBNOPFIX      nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;
#else
#define ITLBNOPFIX      nop;nop;nop;nop
#endif

#define	_MIPS_ISA_MIPS1	1	
#define	_MIPS_ISA_MIPS2	2	
#define	_MIPS_ISA_MIPS3	3	
#define	_MIPS_ISA_MIPS4	4	
#ifdef __linux__
#define	_MIPS_ISA_MIPS5 5
#define	_MIPS_ISA_MIPS32 6
#define	_MIPS_ISA_MIPS64 7
#else
#define	_MIPS_ISA_MIPS32 32	
#endif

#if !defined(ABICALLS) && !defined(_NO_ABICALLS)
#define	ABICALLS	.abicalls
#endif

#if defined(ABICALLS) && !defined(_KERNEL)
	ABICALLS
#endif

#define _C_LABEL(x) x		

#if !defined(__MIPSEL__) && !defined(__MIPSEB__)
#error "__MIPSEL__ or __MIPSEB__ must be defined"
#endif
#if defined(__MIPSEL__)
#define LWLO    lwl
#define LWHI    lwr
#define	SWLO	swl
#define	SWHI	swr
#define LDLO    ldl
#define LDHI    ldr
#define	SDLO	sdl
#define	SDHI	sdr
#endif
#if defined(__MIPSEB__)
#define LWLO    lwr
#define LWHI    lwl
#define	SWLO	swr
#define	SWHI	swl
#define LDLO    ldr
#define LDHI    ldl
#define	SDLO	sdr
#define	SDHI	sdl
#endif

#if defined(ABICALLS) && !defined(_KERNEL) && !defined(_STANDALONE)

#ifndef _MIPS_SIM
#define _MIPS_SIM 1
#define _ABIO32	1
#endif
#ifndef _MIPS_ISA
#define _MIPS_ISA 2
#define _MIPS_ISA_MIPS2 2
#endif

#if (_MIPS_SIM == _ABIO32) || (_MIPS_SIM == _ABI32)
#define NARGSAVE	4

#define	SETUP_GP		\
	.set	noreorder;	\
	.cpload	t9;		\
	.set	reorder;

#define	SAVE_GP(x)		\
	.cprestore x

#define	SETUP_GP64(gpoff, name)
#define	RESTORE_GP64
#endif

#if (_MIPS_SIM == _ABI64) || (_MIPS_SIM == _ABIN32)
#define NARGSAVE	0

#define	SETUP_GP
#define	SAVE_GP(x)
#define	SETUP_GP64(gpoff, name)	\
	.cpsetup t9, gpoff, name
#define	RESTORE_GP64		\
	.cpreturn
#endif

#define	MKFSIZ(narg,locals) (((narg+locals)*REGSZ+31)&(~31))

#else 

#define	NARGSAVE	4
#define	SETUP_GP
#define	SAVE_GP(x)

#define	ALIGNSZ		16	
#define	FRAMESZ(sz)	(((sz) + (ALIGNSZ-1)) & ~(ALIGNSZ-1))

#endif

#if (_MIPS_ISA == _MIPS_ISA_MIPS1 || _MIPS_ISA == _MIPS_ISA_MIPS2 || _MIPS_ISA == _MIPS_ISA_MIPS32)
#define REGSZ		4	
#define LOGREGSZ	2	
#define	REG_S	sw
#define	REG_L	lw
#define	CF_SZ		24	
#define	CF_ARGSZ	16	
#define	CF_RA_OFFS	20	
#endif

#if (_MIPS_ISA == _MIPS_ISA_MIPS3 || _MIPS_ISA == _MIPS_ISA_MIPS4)
#define REGSZ		8	
#define LOGREGSZ	3	
#define	REG_S	sd
#define	REG_L	ld
#define	CF_SZ		48	
#define	CF_ARGSZ	32	
#define	CF_RA_OFFS	40	
#endif

#define REGSZ_FP	 8	

#ifndef __LP64__
#define	PTR_L		lw
#define	PTR_S		sw
#define	PTR_SUB		sub
#define	PTR_ADD		add
#define	PTR_SUBU	subu
#define	PTR_ADDU	addu
#define LI		li
#define	LA		la
#define	PTR_SLL		sll
#define	PTR_SRL		srl
#define	PTR_VAL		.word
#else
#define	PTR_L		ld
#define	PTR_S		sd
#define	PTR_ADD		dadd
#define	PTR_SUB		dsub
#define	PTR_SUBU	dsubu
#define	PTR_ADDU	daddu
#define LI		dli
#define LA		dla
#define	PTR_SLL		dsll
#define	PTR_SRL		dsrl
#define	PTR_VAL		.dword
#endif

#if defined(XGPROF) || defined(XPROF)
#define	MCOUNT			\
	PTR_SUBU sp, sp, 32;	\
	SAVE_GP(16);		\
	sw	ra, 28(sp);	\
	sw	gp, 24(sp);	\
	.set	noat;		\
	.set	noreorder;	\
	move	AT, ra;		\
	jal	_mcount;	\
	PTR_SUBU sp, sp, 8;	\
	lw	ra, 28(sp);	\
	PTR_ADDU sp, sp, 32;	\
	.set reorder;		\
	.set	at;
#else
#define	MCOUNT
#endif

#define LEAF(x, fsize)		\
	.align	3;		\
	.globl x;		\
	.ent x, 0;		\
x: ;				\
	.frame sp, fsize, ra;	\
	SETUP_GP		\
	MCOUNT

#define	ALEAF(x)		\
	.globl	x;		\
x:

#define NLEAF(x, fsize)		\
	.align	3;		\
	.globl x;		\
	.ent x, 0;		\
x: ;				\
	.frame sp, fsize, ra;	\
	SETUP_GP

#define NON_LEAF(x, fsize, retpc) \
	.align	3;		\
	.globl x;		\
	.ent x, 0;		\
x: ;				\
	.frame sp, fsize, retpc; \
	SETUP_GP		\
	MCOUNT

#define NNON_LEAF(x, fsize, retpc) \
	.align	3;		\
	.globl x;		\
	.ent x, 0;		\
x: ;				\
	.frame sp, fsize, retpc	\
	SETUP_GP

#define END(x) \
	.end x

#define PANIC(msg) \
	LA	a0, 9f; \
	jal	panic;	\
	nop	;	\
	MSG(msg)

#define	PRINTF(msg) \
	la	a0, 9f; \
	jal	printf; \
	nop	;	\
	MSG(msg)

#define	MSG(msg) \
	.rdata; \
9:	.asciiz	msg; \
	.text

#define ASMSTR(str) \
	.asciiz str; \
	.align	3

#endif 
