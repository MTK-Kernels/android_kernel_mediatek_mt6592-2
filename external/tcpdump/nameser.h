/*
 * Copyright (c) 1983, 1989, 1993
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
 *      @(#)nameser.h	8.2 (Berkeley) 2/16/94
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

#include <sys/types.h>

#define PACKETSZ	512		
#define MAXDNAME	256		
#define MAXCDNAME	255		
#define MAXLABEL	63		
	
#define QFIXEDSZ	4
	
#define RRFIXEDSZ	10

#define NAMESERVER_PORT	53

#define MULTICASTDNS_PORT	5353

#define QUERY		0x0		
#define IQUERY		0x1		
#define STATUS		0x2		
#if 0
#define xxx		0x3		
#endif
	
#define UPDATEA		0x9		
#define UPDATED		0xa		
#define UPDATEDA	0xb		
#define UPDATEM		0xc		
#define UPDATEMA	0xd		

#define ZONEINIT	0xe		
#define ZONEREF		0xf		

#ifdef T_NULL
#undef T_NULL
#endif
#ifdef T_OPT
#undef T_OPT
#endif
#ifdef T_UNSPEC
#undef T_UNSPEC
#endif
#ifdef NOERROR
#undef NOERROR
#endif

#define NOERROR		0		
#define FORMERR		1		
#define SERVFAIL	2		
#define NXDOMAIN	3		
#define NOTIMP		4		
#define REFUSED		5		
	
#define NOCHANGE	0xf		

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
#define	T_AFSDB		18		
#define T_X25		19		
#define T_ISDN		20		
#define T_RT		21		
#define	T_NSAP		22		
#define	T_NSAP_PTR	23		
#define T_SIG		24		
#define T_KEY		25		
#define T_PX		26		
#define T_GPOS		27		
#define T_AAAA		28		
#define T_LOC		29		
#define T_NXT		30		
#define T_EID		31		
#define T_NIMLOC	32		
#define T_SRV		33		
#define T_ATMA		34		
#define T_NAPTR		35		
#define T_KX		36		
#define T_CERT		37		
#define T_A6		38		
#define T_DNAME		39		
#define T_SINK		40		
#define T_OPT		41		
#define T_APL		42		
#define T_DS		43		
#define T_SSHFP		44		
#define T_IPSECKEY	45		
#define T_RRSIG		46		
#define T_NSEC		47		
#define T_DNSKEY	48		
	
#define T_SPF		99		
#define T_UINFO		100		
#define T_UID		101		
#define T_GID		102		
#define T_UNSPEC	103		
#define T_UNSPECA	104		
	
#define T_TKEY		249		
#define T_TSIG		250		
#define T_IXFR		251		
#define T_AXFR		252		
#define T_MAILB		253		
#define T_MAILA		254		
#define T_ANY		255		


#define C_IN		1		
#define C_CHAOS		3		
#define C_HS		4		
	
#define C_ANY		255		
#define C_QU		0x8000		
#define C_CACHE_FLUSH	0x8000		

#define CONV_SUCCESS 0
#define CONV_OVERFLOW -1
#define CONV_BADFMT -2
#define CONV_BADCKSUM -3
#define CONV_BADBUFLEN -4

typedef struct {
	u_int16_t id;		
	u_int8_t  flags1;	
	u_int8_t  flags2;	
	u_int16_t qdcount;	
	u_int16_t ancount;	
	u_int16_t nscount;	
	u_int16_t arcount;	
} HEADER;

#define DNS_QR(np)	((np)->flags1 & 0x80)		
#define DNS_OPCODE(np)	((((np)->flags1) >> 3) & 0xF)	
#define DNS_AA(np)	((np)->flags1 & 0x04)		
#define DNS_TC(np)	((np)->flags1 & 0x02)		
#define DNS_RD(np)	((np)->flags1 & 0x01)		

#define DNS_RA(np)	((np)->flags2 & 0x80)	
#define DNS_AD(np)	((np)->flags2 & 0x20)	
#define DNS_CD(np)	((np)->flags2 & 0x10)	
#define DNS_RCODE(np)	((np)->flags2 & 0xF)	

#define INDIR_MASK	0xc0	
#define EDNS0_MASK	0x40	
#  define EDNS0_ELT_BITLABEL 0x01

struct rrec {
	int16_t	r_zone;			
	int16_t	r_class;		
	int16_t	r_type;			
	u_int32_t	r_ttl;			
	int	r_size;			
	char	*r_data;		
};

#define GETSHORT(s, cp) { \
	register u_char *t_cp = (u_char *)(cp); \
	(s) = ((u_int16_t)t_cp[0] << 8) | (u_int16_t)t_cp[1]; \
	(cp) += 2; \
}

#define GETLONG(l, cp) { \
	register u_char *t_cp = (u_char *)(cp); \
	(l) = (((u_int32_t)t_cp[0]) << 24) \
	    | (((u_int32_t)t_cp[1]) << 16) \
	    | (((u_int32_t)t_cp[2]) << 8) \
	    | (((u_int32_t)t_cp[3])); \
	(cp) += 4; \
}

#define PUTSHORT(s, cp) { \
	register u_int16_t t_s = (u_int16_t)(s); \
	register u_char *t_cp = (u_char *)(cp); \
	*t_cp++ = t_s >> 8; \
	*t_cp   = t_s; \
	(cp) += 2; \
}

#define PUTLONG(l, cp) { \
	register u_int32_t t_l = (u_int32_t)(l); \
	register u_char *t_cp = (u_char *)(cp); \
	*t_cp++ = t_l >> 24; \
	*t_cp++ = t_l >> 16; \
	*t_cp++ = t_l >> 8; \
	*t_cp   = t_l; \
	(cp) += 4; \
}

#endif 
