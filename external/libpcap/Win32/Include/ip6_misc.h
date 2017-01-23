/*
 * Copyright (c) 1993, 1994, 1997
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * @(#) $Header: /tcpdump/master/libpcap/Win32/Include/ip6_misc.h,v 1.5 2006-01-22 18:02:18 gianluca Exp $ (LBL)
 */


#include <winsock2.h>

#include <ws2tcpip.h>

#ifndef __MINGW32__
#define	IN_MULTICAST(a)		IN_CLASSD(a)
#endif

#define	IN_EXPERIMENTAL(a)	((((u_int32_t) (a)) & 0xf0000000) == 0xf0000000)

#define	IN_LOOPBACKNET		127

#if defined(__MINGW32__) && defined(DEFINE_ADDITIONAL_IPV6_STUFF)
struct in6_addr
  {
    union
      {
	u_int8_t		u6_addr8[16];
	u_int16_t	u6_addr16[8];
	u_int32_t	u6_addr32[4];
      } in6_u;
#define s6_addr			in6_u.u6_addr8
#define s6_addr16		in6_u.u6_addr16
#define s6_addr32		in6_u.u6_addr32
#define s6_addr64		in6_u.u6_addr64
  };

#define IN6ADDR_ANY_INIT { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 }
#define IN6ADDR_LOOPBACK_INIT { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1 }
#endif 


#if (defined _MSC_VER) || (defined(__MINGW32__) && defined(DEFINE_ADDITIONAL_IPV6_STUFF))
typedef unsigned short	sa_family_t;
#endif


#if defined(__MINGW32__) && defined(DEFINE_ADDITIONAL_IPV6_STUFF)

#define	__SOCKADDR_COMMON(sa_prefix) \
  sa_family_t sa_prefix##family

struct sockaddr_in6
  {
    __SOCKADDR_COMMON (sin6_);
    u_int16_t sin6_port;		
    u_int32_t sin6_flowinfo;	
    struct in6_addr sin6_addr;	
  };

#define IN6_IS_ADDR_V4MAPPED(a) \
	((((u_int32_t *) (a))[0] == 0) && (((u_int32_t *) (a))[1] == 0) && \
	 (((u_int32_t *) (a))[2] == htonl (0xffff)))

#define IN6_IS_ADDR_MULTICAST(a) (((u_int8_t *) (a))[0] == 0xff)

#define IN6_IS_ADDR_LINKLOCAL(a) \
	((((u_int32_t *) (a))[0] & htonl (0xffc00000)) == htonl (0xfe800000))

#define IN6_IS_ADDR_LOOPBACK(a) \
	(((u_int32_t *) (a))[0] == 0 && ((u_int32_t *) (a))[1] == 0 && \
	 ((u_int32_t *) (a))[2] == 0 && ((u_int32_t *) (a))[3] == htonl (1))
#endif 

#define ip6_vfc   ip6_ctlun.ip6_un2_vfc
#define ip6_flow  ip6_ctlun.ip6_un1.ip6_un1_flow
#define ip6_plen  ip6_ctlun.ip6_un1.ip6_un1_plen
#define ip6_nxt   ip6_ctlun.ip6_un1.ip6_un1_nxt
#define ip6_hlim  ip6_ctlun.ip6_un1.ip6_un1_hlim
#define ip6_hops  ip6_ctlun.ip6_un1.ip6_un1_hlim

#define nd_rd_type               nd_rd_hdr.icmp6_type
#define nd_rd_code               nd_rd_hdr.icmp6_code
#define nd_rd_cksum              nd_rd_hdr.icmp6_cksum
#define nd_rd_reserved           nd_rd_hdr.icmp6_data32[0]

#define IPPROTO_HOPOPTS		0	
#define IPPROTO_IPV6		41  
#define IPPROTO_ROUTING		43	
#define IPPROTO_FRAGMENT	44	
#define IPPROTO_ESP		50	
#define IPPROTO_AH		51	
#define IPPROTO_ICMPV6		58	
#define IPPROTO_NONE		59	
#define IPPROTO_DSTOPTS		60	
#define IPPROTO_PIM			103 

#define	 IPV6_RTHDR_TYPE_0 0

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


#if defined(__MINGW32__) && defined(DEFINE_ADDITIONAL_IPV6_STUFF)
#ifndef EAI_ADDRFAMILY
struct addrinfo {
	int	ai_flags;	
	int	ai_family;	
	int	ai_socktype;	
	int	ai_protocol;	
	size_t	ai_addrlen;	
	char	*ai_canonname;	
	struct sockaddr *ai_addr;	
	struct addrinfo *ai_next;	
};
#endif
#endif 
