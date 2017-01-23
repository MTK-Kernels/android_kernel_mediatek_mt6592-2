
/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *	@(#)ieee.h	8.1 (Berkeley) 6/11/93
 */




#define	SNG_EXPBITS	8
#define	SNG_FRACBITS	23

#define	DBL_EXPBITS	11
#define	DBL_FRACBITS	52

#ifndef __VFP_FP__
#define	E80_EXPBITS	15
#define	E80_FRACBITS	64

#define	EXT_EXPBITS	15
#define	EXT_FRACBITS	112
#endif

struct ieee_single {
	u_int	sng_frac:23;
	u_int	sng_exponent:8;
	u_int	sng_sign:1;
};

#ifdef __VFP_FP__
struct ieee_double {
#ifdef __ARMEB__
	u_int	dbl_sign:1;
	u_int	dbl_exp:11;
	u_int	dbl_frach:20;
	u_int	dbl_fracl;
#else 
	u_int	dbl_fracl;
	u_int	dbl_frach:20;
	u_int	dbl_exp:11;
	u_int	dbl_sign:1;
#endif 
};
#else 
struct ieee_double {
	u_int	dbl_frach:20;
	u_int	dbl_exp:11;
	u_int	dbl_sign:1;
	u_int	dbl_fracl;
};

union ieee_double_u {
	double                  dblu_d;
	struct ieee_double      dblu_dbl;
};


struct ieee_e80 {
	u_int	e80_exp:15;
	u_int	e80_zero:16;
	u_int	e80_sign:1;
	u_int	e80_frach:31;
	u_int	e80_j:1;
	u_int	e80_fracl;
};

struct ieee_ext {
	u_int	ext_frach:16;
	u_int	ext_exp:15;
	u_int	ext_sign:1;
	u_int	ext_frachm;
	u_int	ext_fraclm;
	u_int	ext_fracl;
};
#endif 

#define	SNG_EXP_INFNAN	255
#define	DBL_EXP_INFNAN	2047
#ifndef __VFP_FP__
#define	E80_EXP_INFNAN	32767
#define	EXT_EXP_INFNAN	32767
#endif 

#if 0
#define	SNG_QUIETNAN	(1 << 22)
#define	DBL_QUIETNAN	(1 << 19)
#ifndef __VFP_FP__
#define	E80_QUIETNAN	(1 << 15)
#define	EXT_QUIETNAN	(1 << 15)
#endif 
#endif

#define	SNG_EXP_BIAS	127
#define	DBL_EXP_BIAS	1023
#ifndef __VFP_FP__
#define	E80_EXP_BIAS	16383
#define	EXT_EXP_BIAS	16383
#endif 
