/*
 * pppio.h - ioctl and other misc. definitions for STREAMS modules.
 *
 * Copyright (c) 1994 Paul Mackerras. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The name(s) of the authors of this software must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission.
 *
 * 4. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by Paul Mackerras
 *     <paulus@samba.org>".
 *
 * THE AUTHORS OF THIS SOFTWARE DISCLAIM ALL WARRANTIES WITH REGARD TO
 * THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS, IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: pppio.h,v 1.9 2002/12/06 09:49:15 paulus Exp $
 */

#define _PPPIO(n)	(('p' << 8) + (n))

#define PPPIO_NEWPPA	_PPPIO(130)	
#define PPPIO_GETSTAT	_PPPIO(131)	
#define PPPIO_GETCSTAT	_PPPIO(132)	
#define PPPIO_MTU	_PPPIO(133)	
#define PPPIO_MRU	_PPPIO(134)	
#define PPPIO_CFLAGS	_PPPIO(135)	
#define PPPIO_XCOMP	_PPPIO(136)	
#define PPPIO_RCOMP	_PPPIO(137)	
#define PPPIO_XACCM	_PPPIO(138)	
#define PPPIO_RACCM	_PPPIO(139)	
#define PPPIO_VJINIT	_PPPIO(140)	
#define PPPIO_ATTACH	_PPPIO(141)	
#define PPPIO_LASTMOD	_PPPIO(142)	
#define PPPIO_GCLEAN	_PPPIO(143)	
#define PPPIO_DEBUG	_PPPIO(144)	
#define PPPIO_BIND	_PPPIO(145)	
#define PPPIO_NPMODE	_PPPIO(146)	
#define PPPIO_GIDLE	_PPPIO(147)	
#define PPPIO_PASSFILT	_PPPIO(148)	
#define PPPIO_ACTIVEFILT _PPPIO(149)	

#define COMP_AC		0x1		
#define DECOMP_AC	0x2		
#define COMP_PROT	0x4		
#define DECOMP_PROT	0x8		

#define COMP_VJC	0x10		
#define COMP_VJCCID	0x20		
#define DECOMP_VJC	0x40		
#define DECOMP_VJCCID	0x80		

#define CCP_ISOPEN	0x100		
#define CCP_ISUP	0x200		
#define CCP_ERROR	0x400		
#define CCP_FATALERROR	0x800		
#define CCP_COMP_RUN	0x1000		
#define CCP_DECOMP_RUN	0x2000		

#define RCV_B7_0	1		
#define RCV_B7_1	2		
#define RCV_EVNP	4		
#define RCV_ODDP	8		

#define PPPCTL_OERROR	0xe0		
#define PPPCTL_IERROR	0xe1		
#define PPPCTL_MTU	0xe2		
#define PPPCTL_MRU	0xe3		
#define PPPCTL_UNIT	0xe4		

#define PPPDBG_DUMP	0x10000		
#define PPPDBG_LOG	0x100		
#define PPPDBG_DRIVER	0		
#define PPPDBG_IF	1		
#define PPPDBG_COMP	2		
#define PPPDBG_AHDLC	3		
