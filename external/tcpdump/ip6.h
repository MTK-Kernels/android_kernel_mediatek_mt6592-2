
/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Copyright (c) 1982, 1986, 1993
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
 *	@(#)ip.h	8.1 (Berkeley) 6/10/93
 */

#ifndef _NETINET_IP6_H_
#define _NETINET_IP6_H_


struct ip6_hdr {
	union {
		struct ip6_hdrctl {
			u_int32_t ip6_un1_flow;	
			u_int16_t ip6_un1_plen;	
			u_int8_t  ip6_un1_nxt;	
			u_int8_t  ip6_un1_hlim;	
		} ip6_un1;
		u_int8_t ip6_un2_vfc;	
	} ip6_ctlun;
	struct in6_addr ip6_src;	
	struct in6_addr ip6_dst;	
} UNALIGNED;

#define ip6_vfc		ip6_ctlun.ip6_un2_vfc
#define ip6_flow	ip6_ctlun.ip6_un1.ip6_un1_flow
#define ip6_plen	ip6_ctlun.ip6_un1.ip6_un1_plen
#define ip6_nxt		ip6_ctlun.ip6_un1.ip6_un1_nxt
#define ip6_hlim	ip6_ctlun.ip6_un1.ip6_un1_hlim
#define ip6_hops	ip6_ctlun.ip6_un1.ip6_un1_hlim

#define IPV6_FLOWINFO_MASK	((u_int32_t)htonl(0x0fffffff))	
#define IPV6_FLOWLABEL_MASK	((u_int32_t)htonl(0x000fffff))	
#if 1
#define IP6TOS_CE		0x01	
#define IP6TOS_ECT		0x02	
#endif


struct	ip6_ext {
	u_int8_t ip6e_nxt;
	u_int8_t ip6e_len;
} UNALIGNED;

struct ip6_hbh {
	u_int8_t ip6h_nxt;	
	u_int8_t ip6h_len;	
	
} UNALIGNED;

struct ip6_dest {
	u_int8_t ip6d_nxt;	
	u_int8_t ip6d_len;	
	
} UNALIGNED;

#define IP6OPT_PAD1		0x00	
#define IP6OPT_PADN		0x01	
#define IP6OPT_JUMBO		0xC2	
#define IP6OPT_JUMBO_LEN	6
#define IP6OPT_ROUTER_ALERT	0x05	

#define IP6OPT_RTALERT_LEN	4
#define IP6OPT_RTALERT_MLD	0	
#define IP6OPT_RTALERT_RSVP	1	
#define IP6OPT_RTALERT_ACTNET	2 	
#define IP6OPT_MINLEN		2

#define IP6OPT_BINDING_UPDATE	0xc6	
#define IP6OPT_BINDING_ACK	0x07	
#define IP6OPT_BINDING_REQ	0x08	
#define IP6OPT_HOME_ADDRESS	0xc9	
#define IP6OPT_EID		0x8a	

#define IP6OPT_TYPE(o)		((o) & 0xC0)
#define IP6OPT_TYPE_SKIP	0x00
#define IP6OPT_TYPE_DISCARD	0x40
#define IP6OPT_TYPE_FORCEICMP	0x80
#define IP6OPT_TYPE_ICMP	0xC0

#define IP6OPT_MUTABLE		0x20

struct ip6_rthdr {
	u_int8_t  ip6r_nxt;	
	u_int8_t  ip6r_len;	
	u_int8_t  ip6r_type;	
	u_int8_t  ip6r_segleft;	
	
} UNALIGNED;

struct ip6_rthdr0 {
	u_int8_t  ip6r0_nxt;		
	u_int8_t  ip6r0_len;		
	u_int8_t  ip6r0_type;		
	u_int8_t  ip6r0_segleft;	
	u_int8_t  ip6r0_reserved;	
	u_int8_t  ip6r0_slmap[3];	
	struct in6_addr ip6r0_addr[1];	
} UNALIGNED;

struct ip6_frag {
	u_int8_t  ip6f_nxt;		
	u_int8_t  ip6f_reserved;	
	u_int16_t ip6f_offlg;		
	u_int32_t ip6f_ident;		
} UNALIGNED;

#define IP6F_OFF_MASK		0xfff8	
#define IP6F_RESERVED_MASK	0x0006	
#define IP6F_MORE_FRAG		0x0001	

extern int nextproto6_cksum(const struct ip6_hdr *, const u_int8_t *, u_int, u_int);

#endif 
