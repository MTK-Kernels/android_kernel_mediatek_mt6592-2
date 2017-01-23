/*
 * Copyright (c) 1988-1997
 *	The Regents of the University of California.  All rights reserved.
 *
 * Copyright (c) 1998-2012  Michael Richardson <mcr@tcpdump.org>
 *      The TCPDUMP project
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
 * @(#) $Header: /tcpdump/master/tcpdump/netdissect.h,v 1.27 2008-08-16 11:36:20 hannes Exp $ (LBL)
 */

#ifndef netdissect_h
#define netdissect_h

#ifdef HAVE_OS_PROTO_H
#include "os-proto.h"
#endif
#include <sys/types.h>

#ifndef HAVE___ATTRIBUTE__
#define __attribute__(x)
#endif


#include <stdarg.h>

#if !defined(HAVE_SNPRINTF)
int snprintf (char *str, size_t sz, const char *format, ...)
#ifdef __ATTRIBUTE___FORMAT_OK
     __attribute__((format (printf, 3, 4)))
#endif 
     ;
#endif 

#if !defined(HAVE_VSNPRINTF)
int vsnprintf (char *str, size_t sz, const char *format, va_list ap)
#ifdef __ATTRIBUTE___FORMAT_OK
     __attribute__((format (printf, 3, 0)))
#endif 
     ;
#endif 

#ifndef HAVE_STRLCAT
extern size_t strlcat (char *, const char *, size_t);
#endif
#ifndef HAVE_STRLCPY
extern size_t strlcpy (char *, const char *, size_t);
#endif

#ifndef HAVE_STRDUP
extern char *strdup (const char *str);
#endif

#ifndef HAVE_STRSEP
extern char *strsep(char **, const char *);
#endif

struct tok {
	int v;			
	const char *s;		
};

#define TOKBUFSIZE 128
extern const char *tok2strbuf(const struct tok *, const char *, int,
			      char *buf, size_t bufsize);

extern const char *tok2str(const struct tok *, const char *, int);
extern char *bittok2str(const struct tok *, const char *, int);
extern char *bittok2str_nosep(const struct tok *, const char *, int);


typedef struct netdissect_options netdissect_options;

struct netdissect_options {
  int ndo_aflag;		
  int ndo_bflag;		
  int ndo_eflag;		
  int ndo_fflag;		
  int ndo_Kflag;		
  int ndo_nflag;		
  int ndo_Nflag;		
  int ndo_qflag;		
  int ndo_Rflag;		
  int ndo_sflag;		
  int ndo_Sflag;		
  int ndo_tflag;		
  int ndo_Uflag;		
  int ndo_uflag;		
  int ndo_vflag;		
  int ndo_xflag;		
  int ndo_Xflag;		
  int ndo_Aflag;		
  int ndo_Bflag;		
  int ndo_Iflag;		
  int ndo_Oflag;                
  int ndo_dlt;                  
  int ndo_jflag;                
  int ndo_pflag;                

  int ndo_Cflag;                
  int ndo_Cflag_count;      
  int ndo_Gflag;            
  int ndo_Gflag_count;      
  time_t ndo_Gflag_time;    
  int ndo_Wflag;          
  int ndo_WflagChars;
  int ndo_Hflag;		
  int ndo_suppress_default_print; 
  const char *ndo_dltname;

  char *ndo_espsecret;
  struct sa_list *ndo_sa_list_head;  
  struct sa_list *ndo_sa_default;

  char *ndo_sigsecret;     	

  struct esp_algorithm *ndo_espsecret_xform;   
  char                 *ndo_espsecret_key;

  int   ndo_packettype;	

  char *ndo_program_name;	

  int32_t ndo_thiszone;	

  int   ndo_snaplen;

  
  const u_char *ndo_packetp;
  const u_char *ndo_snapend;

  
  int ndo_infodelay;

  
  void (*ndo_default_print)(netdissect_options *,
  		      register const u_char *bp, register u_int length);
  void (*ndo_info)(netdissect_options *, int verbose);

  int  (*ndo_printf)(netdissect_options *,
		     const char *fmt, ...)
#ifdef __ATTRIBUTE___FORMAT_OK_FOR_FUNCTION_POINTERS
		     __attribute__ ((format (printf, 2, 3)))
#endif
		     ;
  void (*ndo_error)(netdissect_options *,
		    const char *fmt, ...)
#ifdef __ATTRIBUTE___NORETURN_OK_FOR_FUNCTION_POINTERS
		     __attribute__ ((noreturn))
#endif 
#ifdef __ATTRIBUTE___FORMAT_OK_FOR_FUNCTION_POINTERS
		     __attribute__ ((format (printf, 2, 3)))
#endif 
		     ;
  void (*ndo_warning)(netdissect_options *,
		      const char *fmt, ...)
#ifdef __ATTRIBUTE___FORMAT_OK_FOR_FUNCTION_POINTERS
		     __attribute__ ((format (printf, 2, 3)))
#endif
		     ;
};

#define PT_VAT		1	
#define PT_WB		2	
#define PT_RPC		3	
#define PT_RTP		4	
#define PT_RTCP		5	
#define PT_SNMP		6	
#define PT_CNFP		7	
#define PT_TFTP		8	
#define PT_AODV		9	
#define PT_CARP		10	
#define PT_RADIUS	11	
#define PT_ZMTP1	12	
#define PT_VXLAN	13	
#define PT_PGM		14	
#define PT_PGM_ZMTP1	15	
#define PT_LMP		16	

#ifndef min
#define min(a,b) ((a)>(b)?(b):(a))
#endif
#ifndef max
#define max(a,b) ((b)>(a)?(b):(a))
#endif

#define MAXIMUM_SNAPLEN	65535

#define DEFAULT_SNAPLEN	MAXIMUM_SNAPLEN

#define ESRC(ep) ((ep)->ether_shost)
#define EDST(ep) ((ep)->ether_dhost)

#ifndef NTOHL
#define NTOHL(x)	(x) = ntohl(x)
#define NTOHS(x)	(x) = ntohs(x)
#define HTONL(x)	(x) = htonl(x)
#define HTONS(x)	(x) = htons(x)
#endif

#define ND_TTEST2(var, l) (ndo->ndo_snapend - (l) <= ndo->ndo_snapend && \
			(const u_char *)&(var) <= ndo->ndo_snapend - (l))

#define ND_TTEST(var) ND_TTEST2(var, sizeof(var))

#define ND_TCHECK2(var, l) if (!ND_TTEST2(var, l)) goto trunc

#define ND_TCHECK(var) ND_TCHECK2(var, sizeof(var))

#define ND_PRINT(STUFF) (*ndo->ndo_printf)STUFF
#define ND_DEFAULTPRINT(ap, length) (*ndo->ndo_default_print)(ndo, ap, length)

#if 0
extern void ts_print(netdissect_options *ipdo,
		     const struct timeval *);
extern void relts_print(int);
#endif

extern int fn_print(const u_char *, const u_char *);
extern int fn_printn(const u_char *, u_int, const u_char *);
extern const char *tok2str(const struct tok *, const char *, int);

extern void wrapup(int);

#if 0
extern char *read_infile(netdissect_options *, char *);
extern char *copy_argv(netdissect_options *, char **);
#endif

#define ND_ISASCII(c)	(!((c) & 0x80))	
#define ND_ISPRINT(c)	((c) >= 0x20 && (c) <= 0x7E)
#define ND_ISGRAPH(c)	((c) > 0x20 && (c) <= 0x7E)
#define ND_TOASCII(c)	((c) & 0x7F)

extern void safeputchar(int);
extern void safeputs(const char *, int);

#ifdef LBL_ALIGN
extern void unaligned_memcpy(void *, const void *, size_t);
extern int unaligned_memcmp(const void *, const void *, size_t);
#define UNALIGNED_MEMCPY(p, q, l)	unaligned_memcpy((p), (q), (l))
#define UNALIGNED_MEMCMP(p, q, l)	unaligned_memcmp((p), (q), (l))
#else
#define UNALIGNED_MEMCPY(p, q, l)	memcpy((p), (q), (l))
#define UNALIGNED_MEMCMP(p, q, l)	memcmp((p), (q), (l))
#endif

#define PLURAL_SUFFIX(n) \
	(((n) != 1) ? "s" : "")

#if 0
extern const char *isonsap_string(netdissect_options *, const u_char *);
extern const char *protoid_string(netdissect_options *, const u_char *);
extern const char *dnname_string(netdissect_options *, u_short);
extern const char *dnnum_string(netdissect_options *, u_short);
#endif


#include <pcap.h>

typedef u_int (*if_ndo_printer)(struct netdissect_options *ndo,
				const struct pcap_pkthdr *, const u_char *);
typedef u_int (*if_printer)(const struct pcap_pkthdr *, const u_char *);

extern if_ndo_printer lookup_ndo_printer(int);
extern if_printer lookup_printer(int);

extern void eap_print(netdissect_options *,const u_char *, u_int);
extern int esp_print(netdissect_options *,
		     register const u_char *bp, int len, register const u_char *bp2,
		     int *nhdr, int *padlen);
extern void arp_print(netdissect_options *,const u_char *, u_int, u_int);
extern void tipc_print(netdissect_options *, const u_char *, u_int, u_int);
extern void msnlb_print(netdissect_options *, const u_char *);
extern void icmp6_print(netdissect_options *ndo, const u_char *,
                        u_int, const u_char *, int);
extern void isakmp_print(netdissect_options *,const u_char *,
			 u_int, const u_char *);
extern void isakmp_rfc3948_print(netdissect_options *,const u_char *,
				 u_int, const u_char *);
extern void ip_print(netdissect_options *,const u_char *, u_int);
extern void ip_print_inner(netdissect_options *ndo,
			   const u_char *bp, u_int length, u_int nh,
			   const u_char *bp2);
extern void rrcp_print(netdissect_options *,const u_char *, u_int);

extern void ether_print(netdissect_options *,
                        const u_char *, u_int, u_int,
                        void (*)(netdissect_options *, const u_char *),
                        const u_char *);

extern u_int ether_if_print(netdissect_options *,
                            const struct pcap_pkthdr *,const u_char *);
extern u_int netanalyzer_if_print(netdissect_options *,
                                  const struct pcap_pkthdr *,const u_char *);
extern u_int netanalyzer_transparent_if_print(netdissect_options *,
                                              const struct pcap_pkthdr *,
                                              const u_char *);

extern int ethertype_print(netdissect_options *,u_short, const u_char *,
			     u_int, u_int);

#if 0
extern void ascii_print(netdissect_options *,u_int);
extern void hex_and_ascii_print_with_offset(netdissect_options *,const char *,
				    u_int, u_int);
extern void hex_and_ascii_print(netdissect_options *,const char *, u_int);
extern void hex_print_with_offset(netdissect_options *,const char *,
				  u_int, u_int);
extern void hex_print(netdissect_options *,const char *, u_int);
extern void telnet_print(netdissect_options *,const u_char *, u_int);
extern int llc_print(netdissect_options *,
		     const u_char *, u_int, u_int, const u_char *,
		     const u_char *, u_short *);
extern void aarp_print(netdissect_options *,const u_char *, u_int);
extern void atalk_print(netdissect_options *,const u_char *, u_int);
extern void atm_if_print(u_char *,const struct pcap_pkthdr *, const u_char *);
extern void bootp_print(netdissect_options *,const u_char *,
			u_int, u_short, u_short);
extern void bgp_print(netdissect_options *,const u_char *, int);
extern void bxxp_print(netdissect_options *,const u_char *, u_int);
extern void chdlc_if_print(u_char *user, const struct pcap_pkthdr *h,
			   register const u_char *p);
extern void chdlc_print(netdissect_options *ndo,
			register const u_char *p, u_int length, u_int caplen);
extern void cisco_autorp_print(netdissect_options *,
			       const u_char *, u_int);
extern void cnfp_print(netdissect_options *,const u_char *cp,
		       u_int len, const u_char *bp);
extern void decnet_print(netdissect_options *,const u_char *,
			 u_int, u_int);
extern void default_print(netdissect_options *,const u_char *, u_int);
extern void dvmrp_print(netdissect_options *,const u_char *, u_int);
extern void egp_print(netdissect_options *,const u_char *, u_int,
		      const u_char *);

extern void arcnet_if_print(u_char*,const struct pcap_pkthdr *,const u_char *);
extern void token_if_print(u_char *,const struct pcap_pkthdr *,const u_char *);
extern void fddi_if_print(u_char *,const struct pcap_pkthdr *, const u_char *);

extern void gre_print(netdissect_options *,const u_char *, u_int);
extern void icmp_print(netdissect_options *,const u_char *, u_int,
		       const u_char *);
extern void hsrp_print(netdissect_options *ndo,
		       register const u_char *bp, register u_int len);
extern void ieee802_11_if_print(u_char *,const struct pcap_pkthdr *, const u_char *);
extern void igmp_print(netdissect_options *,
		       register const u_char *, u_int);
extern void igrp_print(netdissect_options *,const u_char *, u_int,
		       const u_char *);
extern int nextproto4_cksum(const struct ip *, const u_int8_t *, u_int, u_int);
extern void ipN_print(netdissect_options *,const u_char *, u_int);
extern void ipx_print(netdissect_options *,const u_char *, u_int);
extern void isoclns_print(netdissect_options *,const u_char *,
			  u_int, u_int, const u_char *,	const u_char *);
extern void krb_print(netdissect_options *,const u_char *, u_int);
extern void llap_print(netdissect_options *,const u_char *, u_int);
extern const char *linkaddr_string(netdissect_options *ndo,
				   const u_char *ep, const unsigned int len);
extern void ltalk_if_print(netdissect_options *ndo,
			   u_char *user, const struct pcap_pkthdr *h,
			   const u_char *p);
extern void mpls_print(netdissect_options *ndo,
		       const u_char *bp, u_int length);
extern void msdp_print(netdissect_options *ndo,
		       const unsigned char *sp, u_int length);
extern void nfsreply_print(netdissect_options *,const u_char *,
			   u_int, const u_char *);
extern void nfsreq_print(netdissect_options *,const u_char *,
			 u_int, const u_char *);
extern void ns_print(netdissect_options *,const u_char *, u_int);
extern void ntp_print(netdissect_options *,const u_char *, u_int);
extern void null_if_print(u_char *,const struct pcap_pkthdr *, const u_char *);
extern void ospf_print(netdissect_options *,const u_char *,
		       u_int, const u_char *);
extern void pimv1_print(netdissect_options *,const u_char *, u_int);
extern void mobile_print(netdissect_options *,const u_char *, u_int);
extern void pim_print(netdissect_options *,const u_char *, u_int, u_int);
extern void pppoe_if_print(u_char *,const struct pcap_pkthdr *, const u_char *);
extern void pppoe_print(netdissect_options *,const u_char *, u_int);
extern void ppp_print(netdissect_options *,
		      register const u_char *, u_int);

extern void ppp_if_print(u_char *,const struct pcap_pkthdr *, const u_char *);
extern void ppp_hdlc_if_print(u_char *,
			      const struct pcap_pkthdr *, const u_char *);
extern void ppp_bsdos_if_print(u_char *,
			       const struct pcap_pkthdr *, const u_char *);

extern int vjc_print(netdissect_options *,register const char *,
		     register u_int, u_short);

extern void raw_if_print(u_char *,
			 const struct pcap_pkthdr *, const u_char *);

extern void rip_print(netdissect_options *,const u_char *, u_int);
extern void rpki_rtr_print(netdissect_options *,const u_char *, u_int);

extern void sctp_print(netdissect_options *ndo,
		       const u_char *bp, const u_char *bp2,
		       u_int sctpPacketLength);

extern void sl_if_print(u_char *,const struct pcap_pkthdr *, const u_char *);

extern void lane_if_print(u_char *,const struct pcap_pkthdr *,const u_char *);
extern void cip_if_print(u_char *,const struct pcap_pkthdr *,const u_char *);
extern void sl_bsdos_if_print(u_char *,
			      const struct pcap_pkthdr *, const u_char *);
extern void sll_if_print(u_char *,
			 const struct pcap_pkthdr *, const u_char *);

extern void snmp_print(netdissect_options *,const u_char *, u_int);
extern void sunrpcrequest_print(netdissect_options *,const u_char *,
				u_int, const u_char *);
extern void tcp_print(netdissect_options *,const u_char *, u_int,
		      const u_char *, int);
extern void tftp_print(netdissect_options *,const u_char *, u_int);
extern void timed_print(netdissect_options *,const u_char *, u_int);
extern void udp_print(netdissect_options *,const u_char *, u_int,
		      const u_char *, int);
extern void wb_print(netdissect_options *,const void *, u_int);
extern int ah_print(netdissect_options *,register const u_char *,
		    register const u_char *);
extern void esp_print_decodesecret(netdissect_options *ndo);
extern int ipcomp_print(netdissect_options *,register const u_char *,
			register const u_char *, int *);
extern void rx_print(netdissect_options *,register const u_char *,
		     int, int, int, u_char *);
extern void netbeui_print(netdissect_options *,u_short,
			  const u_char *, int);
extern void ipx_netbios_print(netdissect_options *,const u_char *, u_int);
extern void nbt_tcp_print(netdissect_options *,const u_char *, int);
extern void nbt_udp137_print(netdissect_options *,
			     const u_char *data, int);
extern void nbt_udp138_print(netdissect_options *,
			     const u_char *data, int);
extern char *smb_errstr(netdissect_options *,int, int);
extern const char *nt_errstr(netdissect_options *, u_int32_t);
extern void print_data(netdissect_options *,const unsigned char *, int);
extern void l2tp_print(netdissect_options *,const u_char *, u_int);
extern void lcp_print(netdissect_options *,const u_char *, u_int);
extern void vrrp_print(netdissect_options *,const u_char *bp,
		       u_int len, int ttl);
extern void carp_print(netdissect_options *,const u_char *bp,
		       u_int len, int ttl);
extern void cdp_print(netdissect_options *,const u_char *,
		      u_int, u_int, const u_char *, const u_char *);
extern void stp_print(netdissect_options *,const u_char *p, u_int length);
extern void radius_print(netdissect_options *,const u_char *, u_int);
extern void lwres_print(netdissect_options *,const u_char *, u_int);
extern void pptp_print(netdissect_options *,const u_char *, u_int);
#endif

extern u_int ipnet_if_print(netdissect_options *,const struct pcap_pkthdr *, const u_char *);
extern u_int ppi_if_print(netdissect_options *,const struct pcap_pkthdr *, const u_char *);
extern u_int nflog_if_print(netdissect_options *,const struct pcap_pkthdr *, const u_char *);
extern u_int ieee802_15_4_if_print(netdissect_options *,const struct pcap_pkthdr *, const u_char *);

#ifdef INET6
extern void ip6_print(netdissect_options *,const u_char *, u_int);
#if 0
extern void ip6_opt_print(netdissect_options *,const u_char *, int);
extern int nextproto6_cksum(const struct ip6_hdr *, const u_int8_t *, u_int, u_int);
extern int hbhopt_print(netdissect_options *,const u_char *);
extern int dstopt_print(netdissect_options *,const u_char *);
extern int frag6_print(netdissect_options *,const u_char *,
		       const u_char *);
extern void icmp6_print(netdissect_options *,const u_char *,
			const u_char *);
extern void ripng_print(netdissect_options *,const u_char *, int);
extern int rt6_print(netdissect_options *,const u_char *, const u_char *);
extern void ospf6_print(netdissect_options *,const u_char *, u_int);
extern void dhcp6_print(netdissect_options *,const u_char *,
			u_int, u_int16_t, u_int16_t);

extern void zephyr_print(netdissect_options * ndo,
			 const u_char *cp, int length);
#endif 

#endif 

#if 0
struct cksum_vec {
	const u_int8_t	*ptr;
	int		len;
};
extern u_int16_t in_cksum(const struct cksum_vec *, int);
extern u_int16_t in_cksum_shouldbe(u_int16_t, u_int16_t);
#endif

extern void esp_print_decodesecret(netdissect_options *ndo);
extern int esp_print_decrypt_buffer_by_ikev2(netdissect_options *ndo,
					     int initiator,
					     u_char spii[8], u_char spir[8],
					     u_char *buf, u_char *end);


extern void geonet_print(netdissect_options *ndo,const u_char *eth_hdr,const u_char *geo_pck, u_int len);
extern void calm_fast_print(netdissect_options *ndo,const u_char *eth_hdr,const u_char *calm_pck, u_int len);

#endif  
