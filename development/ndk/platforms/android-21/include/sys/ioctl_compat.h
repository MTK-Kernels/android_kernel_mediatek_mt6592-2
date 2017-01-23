
/*
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *	@(#)ioctl_compat.h	8.4 (Berkeley) 1/21/94
 */

#ifndef _SYS_IOCTL_COMPAT_H_
#define	_SYS_IOCTL_COMPAT_H_


#if !defined(__mips__)
struct tchars {
	char	t_intrc;	
	char	t_quitc;	
	char	t_startc;	
	char	t_stopc;	
	char	t_eofc;		
	char	t_brkc;		
};

struct ltchars {
	char	t_suspc;	
	char	t_dsuspc;	
	char	t_rprntc;	
	char	t_flushc;	
	char	t_werasc;	
	char	t_lnextc;	
};

#ifndef _SGTTYB_
#define	_SGTTYB_
struct sgttyb {
	char	sg_ispeed;		
	char	sg_ospeed;		
	char	sg_erase;		
	char	sg_kill;		
	short	sg_flags;		
};
#endif
#endif

#ifdef USE_OLD_TTY
# undef  TIOCGETD
# define TIOCGETD	_IOR('t', 0, int)	
# undef  TIOCSETD
# define TIOCSETD	_IOW('t', 1, int)	
#else
# define OTIOCGETD	_IOR('t', 0, int)	
# define OTIOCSETD	_IOW('t', 1, int)	
#endif
#define	TIOCHPCL	_IO('t', 2)		
#if !defined(__mips__)
#define	TIOCGETP	_IOR('t', 8,struct sgttyb)
#define	TIOCSETP	_IOW('t', 9,struct sgttyb)
#define	TIOCSETN	_IOW('t',10,struct sgttyb)
#endif
#define	TIOCSETC	_IOW('t',17,struct tchars)
#define	TIOCGETC	_IOR('t',18,struct tchars)
#if 0
#define		TANDEM		0x00000001	
#define		CBREAK		0x00000002	
#define		LCASE		0x00000004	
#define		ECHO		0x00000008	
#define		CRMOD		0x00000010	
#define		RAW		0x00000020	
#define		ODDP		0x00000040	
#define		EVENP		0x00000080	
#define		ANYP		0x000000c0	
#define		NLDELAY		0x00000300	
#define			NL0	0x00000000
#define			NL1	0x00000100	
#define			NL2	0x00000200	
#define			NL3	0x00000300
#define		TBDELAY		0x00000c00	
#define			TAB0	0x00000000
#define			TAB1	0x00000400	
#define			TAB2	0x00000800
#define		XTABS		0x00000c00	
#define		CRDELAY		0x00003000	
#define			CR0	0x00000000
#define			CR1	0x00001000	
#define			CR2	0x00002000	
#define			CR3	0x00003000	
#define		VTDELAY		0x00004000	
#define			FF0	0x00000000
#define			FF1	0x00004000	
#define		BSDELAY		0x00008000	
#define			BS0	0x00000000
#define			BS1	0x00008000
#define		ALLDELAY	(NLDELAY|TBDELAY|CRDELAY|VTDELAY|BSDELAY)
#define		CRTBS		0x00010000	
#define		PRTERA		0x00020000	
#define		CRTERA		0x00040000	
#define		TILDE		0x00080000	
#define		MDMBUF		0x00100000	
#define		LITOUT		0x00200000	
#define		TOSTOP		0x00400000	
#define		FLUSHO		0x00800000	
#define		NOHANG		0x01000000	
#define		L001000		0x02000000
#define		CRTKIL		0x04000000	
#define		PASS8		0x08000000
#define		CTLECH		0x10000000	
#define		PENDIN		0x20000000	
#define		DECCTQ		0x40000000	
#define		NOFLSH		0x80000000	
#endif
#define	TIOCLBIS	_IOW('t', 127, int)	
#define	TIOCLBIC	_IOW('t', 126, int)	
#define	TIOCLSET	_IOW('t', 125, int)	
#define	TIOCLGET	_IOR('t', 124, int)	
#define		LCRTBS		(CRTBS>>16)
#define		LPRTERA		(PRTERA>>16)
#define		LCRTERA		(CRTERA>>16)
#define		LTILDE		(TILDE>>16)
#define		LMDMBUF		(MDMBUF>>16)
#define		LLITOUT		(LITOUT>>16)
#define		LTOSTOP		(TOSTOP>>16)
#define		LFLUSHO		(FLUSHO>>16)
#define		LNOHANG		(NOHANG>>16)
#define		LCRTKIL		(CRTKIL>>16)
#define		LPASS8		(PASS8>>16)
#define		LCTLECH		(CTLECH>>16)
#define		LPENDIN		(PENDIN>>16)
#define		LDECCTQ		(DECCTQ>>16)
#define		LNOFLSH		(NOFLSH>>16)
#if !defined(__mips__)
#define	TIOCSLTC	_IOW('t',117,struct ltchars)
#define	TIOCGLTC	_IOR('t',116,struct ltchars)
#endif
#define OTIOCCONS	_IO('t', 98)	
#define	OTTYDISC	0
#define	NETLDISC	1
#define	NTTYDISC	2

#endif 
