
/*
 * ppp_defs.h - PPP definitions.
 *
 * Copyright (c) 1984 Paul Mackerras. All rights reserved.
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
 */

#ifndef _PPP_DEFS_H_
#define _PPP_DEFS_H_

#if defined(PPP_ADDRESS)
#define USING_UAPI
#endif

#define PPP_HDRLEN	4	
#define PPP_FCSLEN	2	

#define	PPP_MTU		1500	
#define PPP_MAXMTU	65535 - (PPP_HDRLEN + PPP_FCSLEN)
#define PPP_MINMTU	64
#define PPP_MRU		1500	
#define PPP_MAXMRU	65000	
#define PPP_MINMRU	128

#if !defined(USING_UAPI)
#define PPP_ADDRESS(p)	(((u_char *)(p))[0])
#define PPP_CONTROL(p)	(((u_char *)(p))[1])
#define PPP_PROTOCOL(p)	((((u_char *)(p))[2] << 8) + ((u_char *)(p))[3])
#endif

#define	PPP_ALLSTATIONS	0xff	
#define	PPP_UI		0x03	
#define	PPP_FLAG	0x7e	
#define	PPP_ESCAPE	0x7d	
#define	PPP_TRANS	0x20	

#define PPP_IP		0x21	
#define PPP_AT		0x29	
#define PPP_IPX		0x2b	
#define	PPP_VJC_COMP	0x2d	
#define	PPP_VJC_UNCOMP	0x2f	
#define PPP_IPV6	0x57	
#define PPP_COMP	0xfd	
#define PPP_IPCP	0x8021	
#define PPP_ATCP	0x8029	
#define PPP_IPXCP	0x802b	
#define PPP_IPV6CP	0x8057	
#define PPP_CCP		0x80fd	
#define PPP_ECP		0x8053	
#define PPP_LCP		0xc021	
#define PPP_PAP		0xc023	
#define PPP_LQR		0xc025	
#define PPP_CHAP	0xc223	
#define PPP_CBCP	0xc029	
#define PPP_EAP		0xc227	

#define PPP_INITFCS	0xffff	
#define PPP_GOODFCS	0xf0b8	
#define PPP_FCS(fcs, c)	(((fcs) >> 8) ^ fcstab[((fcs) ^ (c)) & 0xff])


#if !defined(__BIT_TYPES_DEFINED__) && !defined(_BITYPES) \
 && !defined(__FreeBSD__) && (NS_TARGET < 40)
#ifdef	UINT32_T
typedef UINT32_T	u_int32_t;
#else
typedef unsigned int	u_int32_t;
typedef unsigned short  u_int16_t;
#endif
#endif

typedef u_int32_t	ext_accm[8];

#if defined(USING_UAPI)
#define ifr__name b.ifr_ifrn.ifrn_name
#define stats_ptr b.ifr_ifru.ifru_data
struct ifpppstatsreq {
   struct ifreq b;
   struct ppp_stats stats;
};
#else
enum NPmode {
    NPMODE_PASS,		
    NPMODE_DROP,		
    NPMODE_ERROR,		
    NPMODE_QUEUE		
};

struct pppstat	{
    unsigned int ppp_ibytes;	
    unsigned int ppp_ipackets;	
    unsigned int ppp_ierrors;	
    unsigned int ppp_obytes;	
    unsigned int ppp_opackets;	
    unsigned int ppp_oerrors;	
};

struct vjstat {
    unsigned int vjs_packets;	
    unsigned int vjs_compressed; 
    unsigned int vjs_searches;	
    unsigned int vjs_misses;	
    unsigned int vjs_uncompressedin; 
    unsigned int vjs_compressedin; 
    unsigned int vjs_errorin;	
    unsigned int vjs_tossed;	
};

struct ppp_stats {
    struct pppstat p;		
    struct vjstat vj;		
};

struct compstat {
    unsigned int unc_bytes;	
    unsigned int unc_packets;	
    unsigned int comp_bytes;	
    unsigned int comp_packets;	
    unsigned int inc_bytes;	
    unsigned int inc_packets;	
    unsigned int ratio;		
};

struct ppp_comp_stats {
    struct compstat c;		
    struct compstat d;		
};

struct ppp_idle {
    time_t xmit_idle;		
    time_t recv_idle;		
};

#endif

#ifndef __P
#ifdef __STDC__
#define __P(x)	x
#else
#define __P(x)	()
#endif
#endif

#endif 
