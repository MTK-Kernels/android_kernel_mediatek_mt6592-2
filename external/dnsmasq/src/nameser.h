
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
 * Portions Copyright (c) 1995 by International Business Machines, Inc.
 *
 * International Business Machines, Inc. (hereinafter called IBM) grants
 * permission under its copyrights to use, copy, modify, and distribute this
 * Software with or without fee, provided that the above copyright notice and
 * all paragraphs of this notice appear in all copies, and that the name of IBM
 * not be used in connection with the marketing of any product incorporating
 * the Software or modifications thereof, without specific, written prior
 * permission.
 *
 * To the extent it has a right to do so, IBM grants an immunity from suit
 * under its patents, if any, for the use, sale or manufacture of products to
 * the extent that such products are used for performing Domain Name System
 * dynamic updates in TCP/IP networks by means of the Software.  No immunity is
 * granted for any product per se or for any other function of any product.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", AND IBM DISCLAIMS ALL WARRANTIES,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE.  IN NO EVENT SHALL IBM BE LIABLE FOR ANY SPECIAL,
 * DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE, EVEN
 * IF IBM IS APPRISED OF THE POSSIBILITY OF SUCH DAMAGES.
 * --Copyright--
 */


#ifndef _NAMESER_H_
#define _NAMESER_H_

#include <sys/cdefs.h>
#include <sys/param.h>


#define __BIND		19960801	

#define PACKETSZ	512		
#define MAXDNAME	1025		
#define MAXCDNAME	255		
#define MAXLABEL	63		
#define HFIXEDSZ	12		
#define QFIXEDSZ	4		
#define RRFIXEDSZ	10		
#define INT32SZ		4		
#define INT16SZ		2		
#define INADDRSZ	4		
#define IN6ADDRSZ	16		

#define NAMESERVER_PORT	53

#define QUERY		0x0		
#define IQUERY		0x1		
#define STATUS		0x2		
		
#define NS_NOTIFY_OP	0x4		
#define NOERROR		0		
#define FORMERR		1		
#define SERVFAIL	2		
#define NXDOMAIN	3		
#define NOTIMP		4		
#define REFUSED		5		

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
#define T_RP		17		
#define T_AFSDB		18		
#define T_X25		19		
#define T_ISDN		20		
#define T_RT		21		
#define T_NSAP		22		
#define T_NSAP_PTR	23		
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
#define T_RRSIG		46		
#define T_NSEC		47		
#define T_DNSKEY	48		
	
#define T_UINFO		100		
#define T_UID		101		
#define T_GID		102		
#define T_UNSPEC	103		
	
#define	T_TKEY		249		
#define	T_TSIG		250		
#define	T_IXFR		251		
#define T_AXFR		252		
#define T_MAILB		253		
#define T_MAILA		254		
#define T_ANY		255		


#define C_IN		1		
#define C_CHAOS		3		
#define C_HS		4		
	
#define C_ANY		255		

#define	KEYFLAG_TYPEMASK	0xC000	
#define	KEYFLAG_TYPE_AUTH_CONF	0x0000	
#define	KEYFLAG_TYPE_CONF_ONLY	0x8000	
#define	KEYFLAG_TYPE_AUTH_ONLY	0x4000	
#define	KEYFLAG_TYPE_NO_KEY	0xC000	
#define	KEYFLAG_NO_AUTH		0x8000	
#define	KEYFLAG_NO_CONF		0x4000	

#define	KEYFLAG_EXPERIMENTAL	0x2000	
#define	KEYFLAG_RESERVED3	0x1000  
#define	KEYFLAG_RESERVED4	0x0800  
#define	KEYFLAG_USERACCOUNT	0x0400	
#define	KEYFLAG_ENTITY		0x0200	
#define	KEYFLAG_ZONEKEY		0x0100	
#define	KEYFLAG_IPSEC		0x0080  
#define	KEYFLAG_EMAIL		0x0040  
#define	KEYFLAG_RESERVED10	0x0020  
#define	KEYFLAG_RESERVED11	0x0010  
#define	KEYFLAG_SIGNATORYMASK	0x000F	

#define  KEYFLAG_RESERVED_BITMASK ( KEYFLAG_RESERVED3 | \
				    KEYFLAG_RESERVED4 | \
				    KEYFLAG_RESERVED10| KEYFLAG_RESERVED11) 

#define	ALGORITHM_MD5RSA	1	
#define	ALGORITHM_EXPIRE_ONLY	253	
#define	ALGORITHM_PRIVATE_OID	254	

					
#define	MIN_MD5RSA_KEY_PART_BITS	 512
#define	MAX_MD5RSA_KEY_PART_BITS	2552
					
#define	MAX_MD5RSA_KEY_BYTES		((MAX_MD5RSA_KEY_PART_BITS+7/8)*2+3)
					
#define	MAX_KEY_BASE64			(((MAX_MD5RSA_KEY_BYTES+2)/3)*4)

#define DNS_MESSAGEEXTFLAG_DO	0x8000U

#define CONV_SUCCESS	0
#define CONV_OVERFLOW	(-1)
#define CONV_BADFMT	(-2)
#define CONV_BADCKSUM	(-3)
#define CONV_BADBUFLEN	(-4)

#if !defined(_BYTE_ORDER) || \
    (_BYTE_ORDER != _BIG_ENDIAN && _BYTE_ORDER != _LITTLE_ENDIAN && \
    _BYTE_ORDER != _PDP_ENDIAN)
#error "Undefined or invalid _BYTE_ORDER";
#endif


typedef struct {
	unsigned	id :16;		
#if _BYTE_ORDER == _BIG_ENDIAN
			
	unsigned	qr: 1;		
	unsigned	opcode: 4;	
	unsigned	aa: 1;		
	unsigned	tc: 1;		
	unsigned	rd: 1;		
			
	unsigned	ra: 1;		
	unsigned	unused :1;	
	unsigned	ad: 1;		
	unsigned	cd: 1;		
	unsigned	rcode :4;	
#endif
#if _BYTE_ORDER == _LITTLE_ENDIAN || _BYTE_ORDER == _PDP_ENDIAN
			
	unsigned	rd :1;		
	unsigned	tc :1;		
	unsigned	aa :1;		
	unsigned	opcode :4;	
	unsigned	qr :1;		
			
	unsigned	rcode :4;	
	unsigned	cd: 1;		
	unsigned	ad: 1;		
	unsigned	unused :1;	
	unsigned	ra :1;		
#endif
			
	unsigned	qdcount :16;	
	unsigned	ancount :16;	
	unsigned	nscount :16;	
	unsigned	arcount :16;	
} HEADER;

#define INDIR_MASK	0xc0

extern	u_int16_t	_getshort(const unsigned char *);
extern	u_int32_t	_getlong(const unsigned char *);

#define GETSHORT(s, cp) { \
	unsigned char *t_cp = (unsigned char *)(cp); \
	(s) = ((u_int16_t)t_cp[0] << 8) \
	    | ((u_int16_t)t_cp[1]) \
	    ; \
	(cp) += INT16SZ; \
}

#define GETLONG(l, cp) { \
	unsigned char *t_cp = (unsigned char *)(cp); \
	(l) = ((u_int32_t)t_cp[0] << 24) \
	    | ((u_int32_t)t_cp[1] << 16) \
	    | ((u_int32_t)t_cp[2] << 8) \
	    | ((u_int32_t)t_cp[3]) \
	    ; \
	(cp) += INT32SZ; \
}

#define PUTSHORT(s, cp) { \
	u_int16_t t_s = (u_int16_t)(s); \
	unsigned char *t_cp = (unsigned char *)(cp); \
	*t_cp++ = t_s >> 8; \
	*t_cp   = t_s; \
	(cp) += INT16SZ; \
}

#define PUTLONG(l, cp) { \
	u_int32_t t_l = (u_int32_t)(l); \
	unsigned char *t_cp = (unsigned char *)(cp); \
	*t_cp++ = t_l >> 24; \
	*t_cp++ = t_l >> 16; \
	*t_cp++ = t_l >> 8; \
	*t_cp   = t_l; \
	(cp) += INT32SZ; \
}

#endif 
