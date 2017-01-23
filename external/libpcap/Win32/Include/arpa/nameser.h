/*
 * ++Copyright++ 1983, 1989, 1993
 * -
 * Copyright (c) 1983, 1989, 1993
 *    The Regents of the University of California.  All rights reserved.
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
 * 	This product includes software developed by the University of
 * 	California, Berkeley and its contributors.
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
 * -
 * Portions Copyright (c) 1993 by Digital Equipment Corporation.
 * 
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies, and that
 * the name of Digital Equipment Corporation not be used in advertising or
 * publicity pertaining to distribution of the document or software without
 * specific, written prior permission.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND DIGITAL EQUIPMENT CORP. DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS.   IN NO EVENT SHALL DIGITAL EQUIPMENT
 * CORPORATION BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 * -
 * --Copyright--
 */


#ifndef _NAMESER_H_
#define	_NAMESER_H_

#ifndef WIN32
#include <sys/param.h>
#if (!defined(BSD)) || (BSD < 199306)
# include <sys/bitypes.h>
#else
# include <sys/types.h>
#endif
#include <sys/cdefs.h>
#else 
#include <pcap-stdinc.h>
#define __LITTLE_ENDIAN 1
#define __BYTE_ORDER __LITTLE_ENDIAN
#endif


#define	__BIND		19940417	

#define PACKETSZ	512		
#define MAXDNAME	256		
#define MAXCDNAME	255		
#define MAXLABEL	63		
#define	HFIXEDSZ	12		
#define QFIXEDSZ	4		
#define RRFIXEDSZ	10		
#define	INT32SZ		4		
#define	INT16SZ		2		
#define	INADDRSZ	4		

#define NAMESERVER_PORT	53

#define QUERY		0x0		
#define IQUERY		0x1		
#define STATUS		0x2		
#define	NS_NOTIFY_OP	0x4		
#ifdef ALLOW_UPDATES
	
# define UPDATEA	0x9		
# define UPDATED	0xa		
# define UPDATEDA	0xb		
# define UPDATEM	0xc		
# define UPDATEMA	0xd		
# define ZONEINIT	0xe		
# define ZONEREF	0xf		
#endif

#ifdef HAVE_ADDRINFO
#define NOERROR		0		
#endif 
#define FORMERR		1		
#define SERVFAIL	2		
#define NXDOMAIN	3		
#define NOTIMP		4		
#define REFUSED		5		
#ifdef ALLOW_UPDATES
	
# define NOCHANGE	0xf		
#endif

#define T_A		1		
#define T_NS		2		
#define T_MD		3		
#define T_MF		4		
#define T_CNAME		5		
#define T_SOA		6		
#define T_MB		7		
#define T_MG		8		
#define T_MR		9		
#define T_NULL		10		
#define T_WKS		11		
#define T_PTR		12		
#define T_HINFO		13		
#define T_MINFO		14		
#define T_MX		15		
#define T_TXT		16		
#define	T_RP		17		
#define T_AFSDB		18		
#define T_X25		19		
#define T_ISDN		20		
#define T_RT		21		
#define T_NSAP		22		
#define T_NSAP_PTR	23		
#define	T_SIG		24		
#define	T_KEY		25		
#define	T_PX		26		
#define	T_GPOS		27		
#define	T_AAAA		28		
#define	T_LOC		29		
	
#define T_UINFO		100		
#define T_UID		101		
#define T_GID		102		
#define T_UNSPEC	103		
	
#define T_AXFR		252		
#define T_MAILB		253		
#define T_MAILA		254		
#define T_ANY		255		


#define C_IN		1		
#define C_CHAOS		3		
#define C_HS		4		
	
#define C_ANY		255		

#define CONV_SUCCESS	0
#define CONV_OVERFLOW	(-1)
#define CONV_BADFMT	(-2)
#define CONV_BADCKSUM	(-3)
#define CONV_BADBUFLEN	(-4)

#ifndef __BYTE_ORDER
#if (BSD >= 199103)
# include <machine/endian.h>
#else
#ifdef linux
# include <endian.h>
#else
#define	__LITTLE_ENDIAN	1234	
#define	__BIG_ENDIAN	4321	
#define	__PDP_ENDIAN	3412	

#if defined(vax) || defined(ns32000) || defined(sun386) || defined(i386) || \
    defined(MIPSEL) || defined(_MIPSEL) || defined(BIT_ZERO_ON_RIGHT) || \
    defined(__alpha__) || defined(__alpha)
#define __BYTE_ORDER	__LITTLE_ENDIAN
#endif

#if defined(sel) || defined(pyr) || defined(mc68000) || defined(sparc) || \
    defined(is68k) || defined(tahoe) || defined(ibm032) || defined(ibm370) || \
    defined(MIPSEB) || defined(_MIPSEB) || defined(_IBMR2) || defined(DGUX) ||\
    defined(apollo) || defined(__convex__) || defined(_CRAY) || \
    defined(__hppa) || defined(__hp9000) || \
    defined(__hp9000s300) || defined(__hp9000s700) || \
    defined (BIT_ZERO_ON_LEFT) || defined(m68k)
#define __BYTE_ORDER	__BIG_ENDIAN
#endif
#endif 
#endif 
#endif 

#if !defined(__BYTE_ORDER) || \
    (__BYTE_ORDER != __BIG_ENDIAN && __BYTE_ORDER != __LITTLE_ENDIAN && \
    __BYTE_ORDER != __PDP_ENDIAN)
  error "Undefined or invalid __BYTE_ORDER";
#endif


typedef struct {
	unsigned	id :16;		
#if __BYTE_ORDER == __BIG_ENDIAN
			
	unsigned	qr: 1;		
	unsigned	opcode: 4;	
	unsigned	aa: 1;		
	unsigned	tc: 1;		
	unsigned	rd: 1;		
			
	unsigned	ra: 1;		
	unsigned	pr: 1;		
	unsigned	unused :2;	
	unsigned	rcode :4;	
#endif
#if __BYTE_ORDER == __LITTLE_ENDIAN || __BYTE_ORDER == __PDP_ENDIAN
			
	unsigned	rd :1;		
	unsigned	tc :1;		
	unsigned	aa :1;		
	unsigned	opcode :4;	
	unsigned	qr :1;		
			
	unsigned	rcode :4;	
	unsigned	unused :2;	
	unsigned	pr :1;		
	unsigned	ra :1;		
#endif
			
	unsigned	qdcount :16;	
	unsigned	ancount :16;	
	unsigned	nscount :16;	
	unsigned	arcount :16;	
} HEADER;

#define INDIR_MASK	0xc0

struct rrec {
	int16_t		r_zone;			
	int16_t		r_class;		
	int16_t		r_type;			
	u_int32_t	r_ttl;			
	int		r_size;			
	char		*r_data;		
};


#define GETSHORT(s, cp) { \
	register u_char *t_cp = (u_char *)(cp); \
	(s) = ((u_int16_t)t_cp[0] << 8) \
	    | ((u_int16_t)t_cp[1]) \
	    ; \
	(cp) += INT16SZ; \
}

#define GETLONG(l, cp) { \
	register u_char *t_cp = (u_char *)(cp); \
	(l) = ((u_int32_t)t_cp[0] << 24) \
	    | ((u_int32_t)t_cp[1] << 16) \
	    | ((u_int32_t)t_cp[2] << 8) \
	    | ((u_int32_t)t_cp[3]) \
	    ; \
	(cp) += INT32SZ; \
}

#define PUTSHORT(s, cp) { \
	register u_int16_t t_s = (u_int16_t)(s); \
	register u_char *t_cp = (u_char *)(cp); \
	*t_cp++ = t_s >> 8; \
	*t_cp   = t_s; \
	(cp) += INT16SZ; \
}

#define PUTLONG(l, cp) { \
	register u_int32_t t_l = (u_int32_t)(l); \
	register u_char *t_cp = (u_char *)(cp); \
	*t_cp++ = t_l >> 24; \
	*t_cp++ = t_l >> 16; \
	*t_cp++ = t_l >> 8; \
	*t_cp   = t_l; \
	(cp) += INT32SZ; \
}

#endif 
