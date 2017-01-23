/*
 * Copyright (c) 1988, 1989, 1990, 1991, 1993, 1994, 1995, 1996
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
 */

#ifndef lint
static const char rcsid[] _U_ =
    "@(#) $Header: /tcpdump/master/tcpdump/print-icmp.c,v 1.87 2007-09-13 17:42:31 guy Exp $ (LBL)";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <tcpdump-stdinc.h>

#include <stdio.h>
#include <string.h>

#include "interface.h"
#include "addrtoname.h"
#include "extract.h"			

#include "ip.h"
#include "udp.h"
#include "ipproto.h"
#include "mpls.h"


struct icmp {
	u_int8_t  icmp_type;		
	u_int8_t  icmp_code;		
	u_int16_t icmp_cksum;		
	union {
		u_int8_t ih_pptr;			
		struct in_addr ih_gwaddr;	
		struct ih_idseq {
			u_int16_t icd_id;
			u_int16_t icd_seq;
		} ih_idseq;
		u_int32_t ih_void;
	} icmp_hun;
#define	icmp_pptr	icmp_hun.ih_pptr
#define	icmp_gwaddr	icmp_hun.ih_gwaddr
#define	icmp_id		icmp_hun.ih_idseq.icd_id
#define	icmp_seq	icmp_hun.ih_idseq.icd_seq
#define	icmp_void	icmp_hun.ih_void
	union {
		struct id_ts {
			u_int32_t its_otime;
			u_int32_t its_rtime;
			u_int32_t its_ttime;
		} id_ts;
		struct id_ip  {
			struct ip idi_ip;
			
		} id_ip;
		u_int32_t id_mask;
		u_int8_t id_data[1];
	} icmp_dun;
#define	icmp_otime	icmp_dun.id_ts.its_otime
#define	icmp_rtime	icmp_dun.id_ts.its_rtime
#define	icmp_ttime	icmp_dun.id_ts.its_ttime
#define	icmp_ip		icmp_dun.id_ip.idi_ip
#define	icmp_mask	icmp_dun.id_mask
#define	icmp_data	icmp_dun.id_data
};

#define ICMP_MPLS_EXT_EXTRACT_VERSION(x) (((x)&0xf0)>>4) 
#define ICMP_MPLS_EXT_VERSION 2

#define	ICMP_MINLEN	8				
#define ICMP_EXTD_MINLEN (156 - sizeof (struct ip))     
#define	ICMP_TSLEN	(8 + 3 * sizeof (u_int32_t))	
#define	ICMP_MASKLEN	12				
#define	ICMP_ADVLENMIN	(8 + sizeof (struct ip) + 8)	
#define	ICMP_ADVLEN(p)	(8 + (IP_HL(&(p)->icmp_ip) << 2) + 8)
	

#define	ICMP_ECHOREPLY		0		
#define	ICMP_UNREACH		3		
#define		ICMP_UNREACH_NET	0		
#define		ICMP_UNREACH_HOST	1		
#define		ICMP_UNREACH_PROTOCOL	2		
#define		ICMP_UNREACH_PORT	3		
#define		ICMP_UNREACH_NEEDFRAG	4		
#define		ICMP_UNREACH_SRCFAIL	5		
#define		ICMP_UNREACH_NET_UNKNOWN 6		
#define		ICMP_UNREACH_HOST_UNKNOWN 7		
#define		ICMP_UNREACH_ISOLATED	8		
#define		ICMP_UNREACH_NET_PROHIB	9		
#define		ICMP_UNREACH_HOST_PROHIB 10		
#define		ICMP_UNREACH_TOSNET	11		
#define		ICMP_UNREACH_TOSHOST	12		
#define	ICMP_SOURCEQUENCH	4		
#define	ICMP_REDIRECT		5		
#define		ICMP_REDIRECT_NET	0		
#define		ICMP_REDIRECT_HOST	1		
#define		ICMP_REDIRECT_TOSNET	2		
#define		ICMP_REDIRECT_TOSHOST	3		
#define	ICMP_ECHO		8		
#define	ICMP_ROUTERADVERT	9		
#define	ICMP_ROUTERSOLICIT	10		
#define	ICMP_TIMXCEED		11		
#define		ICMP_TIMXCEED_INTRANS	0		
#define		ICMP_TIMXCEED_REASS	1		
#define	ICMP_PARAMPROB		12		
#define		ICMP_PARAMPROB_OPTABSENT 1		
#define	ICMP_TSTAMP		13		
#define	ICMP_TSTAMPREPLY	14		
#define	ICMP_IREQ		15		
#define	ICMP_IREQREPLY		16		
#define	ICMP_MASKREQ		17		
#define	ICMP_MASKREPLY		18		

#define	ICMP_MAXTYPE		18

#define	ICMP_INFOTYPE(type) \
	((type) == ICMP_ECHOREPLY || (type) == ICMP_ECHO || \
	(type) == ICMP_ROUTERADVERT || (type) == ICMP_ROUTERSOLICIT || \
	(type) == ICMP_TSTAMP || (type) == ICMP_TSTAMPREPLY || \
	(type) == ICMP_IREQ || (type) == ICMP_IREQREPLY || \
	(type) == ICMP_MASKREQ || (type) == ICMP_MASKREPLY)
#define	ICMP_MPLS_EXT_TYPE(type) \
	((type) == ICMP_UNREACH || \
         (type) == ICMP_TIMXCEED || \
         (type) == ICMP_PARAMPROB)
#ifndef ICMP_UNREACH_NET_UNKNOWN
#define ICMP_UNREACH_NET_UNKNOWN	6	
#endif
#ifndef ICMP_UNREACH_HOST_UNKNOWN
#define ICMP_UNREACH_HOST_UNKNOWN	7	
#endif
#ifndef ICMP_UNREACH_ISOLATED
#define ICMP_UNREACH_ISOLATED		8	
#endif
#ifndef ICMP_UNREACH_NET_PROHIB
#define ICMP_UNREACH_NET_PROHIB		9	
#endif
#ifndef ICMP_UNREACH_HOST_PROHIB
#define ICMP_UNREACH_HOST_PROHIB	10	
#endif
#ifndef ICMP_UNREACH_TOSNET
#define ICMP_UNREACH_TOSNET		11	
#endif
#ifndef ICMP_UNREACH_TOSHOST
#define ICMP_UNREACH_TOSHOST		12	
#endif

#ifndef ICMP_UNREACH_FILTER_PROHIB
#define ICMP_UNREACH_FILTER_PROHIB	13	
#endif
#ifndef ICMP_UNREACH_HOST_PRECEDENCE
#define ICMP_UNREACH_HOST_PRECEDENCE	14	
#endif
#ifndef ICMP_UNREACH_PRECEDENCE_CUTOFF
#define ICMP_UNREACH_PRECEDENCE_CUTOFF	15	
#endif

static const struct tok icmp2str[] = {
	{ ICMP_ECHOREPLY,		"echo reply" },
	{ ICMP_SOURCEQUENCH,		"source quench" },
	{ ICMP_ECHO,			"echo request" },
	{ ICMP_ROUTERSOLICIT,		"router solicitation" },
	{ ICMP_TSTAMP,			"time stamp request" },
	{ ICMP_TSTAMPREPLY,		"time stamp reply" },
	{ ICMP_IREQ,			"information request" },
	{ ICMP_IREQREPLY,		"information reply" },
	{ ICMP_MASKREQ,			"address mask request" },
	{ 0,				NULL }
};

static const struct tok unreach2str[] = {
	{ ICMP_UNREACH_NET,		"net %s unreachable" },
	{ ICMP_UNREACH_HOST,		"host %s unreachable" },
	{ ICMP_UNREACH_SRCFAIL,
	    "%s unreachable - source route failed" },
	{ ICMP_UNREACH_NET_UNKNOWN,	"net %s unreachable - unknown" },
	{ ICMP_UNREACH_HOST_UNKNOWN,	"host %s unreachable - unknown" },
	{ ICMP_UNREACH_ISOLATED,
	    "%s unreachable - source host isolated" },
	{ ICMP_UNREACH_NET_PROHIB,
	    "net %s unreachable - admin prohibited" },
	{ ICMP_UNREACH_HOST_PROHIB,
	    "host %s unreachable - admin prohibited" },
	{ ICMP_UNREACH_TOSNET,
	    "net %s unreachable - tos prohibited" },
	{ ICMP_UNREACH_TOSHOST,
	    "host %s unreachable - tos prohibited" },
	{ ICMP_UNREACH_FILTER_PROHIB,
	    "host %s unreachable - admin prohibited filter" },
	{ ICMP_UNREACH_HOST_PRECEDENCE,
	    "host %s unreachable - host precedence violation" },
	{ ICMP_UNREACH_PRECEDENCE_CUTOFF,
	    "host %s unreachable - precedence cutoff" },
	{ 0,				NULL }
};

static const struct tok type2str[] = {
	{ ICMP_REDIRECT_NET,		"redirect %s to net %s" },
	{ ICMP_REDIRECT_HOST,		"redirect %s to host %s" },
	{ ICMP_REDIRECT_TOSNET,		"redirect-tos %s to net %s" },
	{ ICMP_REDIRECT_TOSHOST,	"redirect-tos %s to host %s" },
	{ 0,				NULL }
};

struct mtu_discovery {
	u_int16_t unused;
	u_int16_t nexthopmtu;
};

struct ih_rdiscovery {
	u_int8_t ird_addrnum;
	u_int8_t ird_addrsiz;
	u_int16_t ird_lifetime;
};

struct id_rdiscovery {
	u_int32_t ird_addr;
	u_int32_t ird_pref;
};


struct icmp_ext_t {
    u_int8_t icmp_type;
    u_int8_t icmp_code;
    u_int8_t icmp_checksum[2];
    u_int8_t icmp_reserved;
    u_int8_t icmp_length;
    u_int8_t icmp_reserved2[2];
    u_int8_t icmp_ext_legacy_header[128]; 
    u_int8_t icmp_ext_version_res[2];
    u_int8_t icmp_ext_checksum[2];
    u_int8_t icmp_ext_data[1];
};

struct icmp_mpls_ext_object_header_t {
    u_int8_t length[2];
    u_int8_t class_num;
    u_int8_t ctype;
};

static const struct tok icmp_mpls_ext_obj_values[] = {
    { 1, "MPLS Stack Entry" },
    { 2, "Extended Payload" },
    { 0, NULL}
};

const char *icmp_tstamp_print(u_int);

const char *
icmp_tstamp_print(u_int tstamp) {
    u_int msec,sec,min,hrs;

    static char buf[64];

    msec = tstamp % 1000;
    sec = tstamp / 1000;
    min = sec / 60; sec -= min * 60;
    hrs = min / 60; min -= hrs * 60;
    snprintf(buf, sizeof(buf), "%02u:%02u:%02u.%03u",hrs,min,sec,msec);
    return buf;
}
 
void
icmp_print(const u_char *bp, u_int plen, const u_char *bp2, int fragmented)
{
	char *cp;
	const struct icmp *dp;
        const struct icmp_ext_t *ext_dp;
	const struct ip *ip;
	const char *str, *fmt;
	const struct ip *oip;
	const struct udphdr *ouh;
        const u_int8_t *obj_tptr;
        u_int32_t raw_label;
        const u_char *snapend_save;
	const struct icmp_mpls_ext_object_header_t *icmp_mpls_ext_object_header;
	u_int hlen, dport, mtu, obj_tlen, obj_class_num, obj_ctype;
	char buf[MAXHOSTNAMELEN + 100];
	struct cksum_vec vec[1];

	dp = (struct icmp *)bp;
        ext_dp = (struct icmp_ext_t *)bp;
	ip = (struct ip *)bp2;
	str = buf;

	TCHECK(dp->icmp_code);
	switch (dp->icmp_type) {

	case ICMP_ECHO:
	case ICMP_ECHOREPLY:
		TCHECK(dp->icmp_seq);
		(void)snprintf(buf, sizeof(buf), "echo %s, id %u, seq %u",
                               dp->icmp_type == ICMP_ECHO ?
                               "request" : "reply",
                               EXTRACT_16BITS(&dp->icmp_id),
                               EXTRACT_16BITS(&dp->icmp_seq));
		break;

	case ICMP_UNREACH:
		TCHECK(dp->icmp_ip.ip_dst);
		switch (dp->icmp_code) {

		case ICMP_UNREACH_PROTOCOL:
			TCHECK(dp->icmp_ip.ip_p);
			(void)snprintf(buf, sizeof(buf),
			    "%s protocol %d unreachable",
			    ipaddr_string(&dp->icmp_ip.ip_dst),
			    dp->icmp_ip.ip_p);
			break;

		case ICMP_UNREACH_PORT:
			TCHECK(dp->icmp_ip.ip_p);
			oip = &dp->icmp_ip;
			hlen = IP_HL(oip) * 4;
			ouh = (struct udphdr *)(((u_char *)oip) + hlen);
			TCHECK(ouh->uh_dport);
			dport = EXTRACT_16BITS(&ouh->uh_dport);
			switch (oip->ip_p) {

			case IPPROTO_TCP:
				(void)snprintf(buf, sizeof(buf),
					"%s tcp port %s unreachable",
					ipaddr_string(&oip->ip_dst),
					tcpport_string(dport));
				break;

			case IPPROTO_UDP:
				(void)snprintf(buf, sizeof(buf),
					"%s udp port %s unreachable",
					ipaddr_string(&oip->ip_dst),
					udpport_string(dport));
				break;

			default:
				(void)snprintf(buf, sizeof(buf),
					"%s protocol %d port %d unreachable",
					ipaddr_string(&oip->ip_dst),
					oip->ip_p, dport);
				break;
			}
			break;

		case ICMP_UNREACH_NEEDFRAG:
		    {
			register const struct mtu_discovery *mp;
			mp = (struct mtu_discovery *)(u_char *)&dp->icmp_void;
			mtu = EXTRACT_16BITS(&mp->nexthopmtu);
			if (mtu) {
				(void)snprintf(buf, sizeof(buf),
				    "%s unreachable - need to frag (mtu %d)",
				    ipaddr_string(&dp->icmp_ip.ip_dst), mtu);
			} else {
				(void)snprintf(buf, sizeof(buf),
				    "%s unreachable - need to frag",
				    ipaddr_string(&dp->icmp_ip.ip_dst));
			}
		    }
			break;

		default:
			fmt = tok2str(unreach2str, "#%d %%s unreachable",
			    dp->icmp_code);
			(void)snprintf(buf, sizeof(buf), fmt,
			    ipaddr_string(&dp->icmp_ip.ip_dst));
			break;
		}
		break;

	case ICMP_REDIRECT:
		TCHECK(dp->icmp_ip.ip_dst);
		fmt = tok2str(type2str, "redirect-#%d %%s to net %%s",
		    dp->icmp_code);
		(void)snprintf(buf, sizeof(buf), fmt,
		    ipaddr_string(&dp->icmp_ip.ip_dst),
		    ipaddr_string(&dp->icmp_gwaddr));
		break;

	case ICMP_ROUTERADVERT:
	    {
		register const struct ih_rdiscovery *ihp;
		register const struct id_rdiscovery *idp;
		u_int lifetime, num, size;

		(void)snprintf(buf, sizeof(buf), "router advertisement");
		cp = buf + strlen(buf);

		ihp = (struct ih_rdiscovery *)&dp->icmp_void;
		TCHECK(*ihp);
		(void)strncpy(cp, " lifetime ", sizeof(buf) - (cp - buf));
		cp = buf + strlen(buf);
		lifetime = EXTRACT_16BITS(&ihp->ird_lifetime);
		if (lifetime < 60) {
			(void)snprintf(cp, sizeof(buf) - (cp - buf), "%u",
			    lifetime);
		} else if (lifetime < 60 * 60) {
			(void)snprintf(cp, sizeof(buf) - (cp - buf), "%u:%02u",
			    lifetime / 60, lifetime % 60);
		} else {
			(void)snprintf(cp, sizeof(buf) - (cp - buf),
			    "%u:%02u:%02u",
			    lifetime / 3600,
			    (lifetime % 3600) / 60,
			    lifetime % 60);
		}
		cp = buf + strlen(buf);

		num = ihp->ird_addrnum;
		(void)snprintf(cp, sizeof(buf) - (cp - buf), " %d:", num);
		cp = buf + strlen(buf);

		size = ihp->ird_addrsiz;
		if (size != 2) {
			(void)snprintf(cp, sizeof(buf) - (cp - buf),
			    " [size %d]", size);
			break;
		}
		idp = (struct id_rdiscovery *)&dp->icmp_data;
		while (num-- > 0) {
			TCHECK(*idp);
			(void)snprintf(cp, sizeof(buf) - (cp - buf), " {%s %u}",
			    ipaddr_string(&idp->ird_addr),
			    EXTRACT_32BITS(&idp->ird_pref));
			cp = buf + strlen(buf);
			++idp;
		}
	    }
		break;

	case ICMP_TIMXCEED:
		TCHECK(dp->icmp_ip.ip_dst);
		switch (dp->icmp_code) {

		case ICMP_TIMXCEED_INTRANS:
			str = "time exceeded in-transit";
			break;

		case ICMP_TIMXCEED_REASS:
			str = "ip reassembly time exceeded";
			break;

		default:
			(void)snprintf(buf, sizeof(buf), "time exceeded-#%d",
			    dp->icmp_code);
			break;
		}
		break;

	case ICMP_PARAMPROB:
		if (dp->icmp_code)
			(void)snprintf(buf, sizeof(buf),
			    "parameter problem - code %d", dp->icmp_code);
		else {
			TCHECK(dp->icmp_pptr);
			(void)snprintf(buf, sizeof(buf),
			    "parameter problem - octet %d", dp->icmp_pptr);
		}
		break;

	case ICMP_MASKREPLY:
		TCHECK(dp->icmp_mask);
		(void)snprintf(buf, sizeof(buf), "address mask is 0x%08x",
		    EXTRACT_32BITS(&dp->icmp_mask));
		break;

	case ICMP_TSTAMP:
		TCHECK(dp->icmp_seq);
		(void)snprintf(buf, sizeof(buf),
		    "time stamp query id %u seq %u",
		    EXTRACT_16BITS(&dp->icmp_id),
		    EXTRACT_16BITS(&dp->icmp_seq));
		break;

	case ICMP_TSTAMPREPLY:
		TCHECK(dp->icmp_ttime);
		(void)snprintf(buf, sizeof(buf),
		    "time stamp reply id %u seq %u: org %s",
                               EXTRACT_16BITS(&dp->icmp_id),
                               EXTRACT_16BITS(&dp->icmp_seq),
                               icmp_tstamp_print(EXTRACT_32BITS(&dp->icmp_otime)));

                (void)snprintf(buf+strlen(buf),sizeof(buf)-strlen(buf),", recv %s",
                         icmp_tstamp_print(EXTRACT_32BITS(&dp->icmp_rtime)));
                (void)snprintf(buf+strlen(buf),sizeof(buf)-strlen(buf),", xmit %s",
                         icmp_tstamp_print(EXTRACT_32BITS(&dp->icmp_ttime)));
                break;

	default:
		str = tok2str(icmp2str, "type-#%d", dp->icmp_type);
		break;
	}
	(void)printf("ICMP %s, length %u", str, plen);
	if (vflag && !fragmented) { 
		u_int16_t sum, icmp_sum;
		struct cksum_vec vec[1];
		if (TTEST2(*bp, plen)) {
			vec[0].ptr = (const u_int8_t *)(void *)dp;
			vec[0].len = plen;
			sum = in_cksum(vec, 1);
			if (sum != 0) {
				icmp_sum = EXTRACT_16BITS(&dp->icmp_cksum);
				(void)printf(" (wrong icmp cksum %x (->%x)!)",
					     icmp_sum,
					     in_cksum_shouldbe(icmp_sum, sum));
			}
		}
	}

	if (vflag >= 1 && !ICMP_INFOTYPE(dp->icmp_type)) {
		bp += 8;
		(void)printf("\n\t");
		ip = (struct ip *)bp;
		snaplen = snapend - bp;
                snapend_save = snapend;
		ip_print(gndo, bp, EXTRACT_16BITS(&ip->ip_len));
                snapend = snapend_save;
	}

        if (vflag >= 1 && plen > ICMP_EXTD_MINLEN && ICMP_MPLS_EXT_TYPE(dp->icmp_type)) {

            TCHECK(*ext_dp);

            if (!ext_dp->icmp_length) {
                vec[0].ptr = (const u_int8_t *)(void *)&ext_dp->icmp_ext_version_res;
                vec[0].len = plen - ICMP_EXTD_MINLEN;
                if (in_cksum(vec, 1)) {
                    return;
                }
            }

            printf("\n\tMPLS extension v%u",
                   ICMP_MPLS_EXT_EXTRACT_VERSION(*(ext_dp->icmp_ext_version_res)));
            
            if (ICMP_MPLS_EXT_EXTRACT_VERSION(*(ext_dp->icmp_ext_version_res)) !=
                ICMP_MPLS_EXT_VERSION) {
                printf(" packet not supported");
                return;
            }

            hlen = plen - ICMP_EXTD_MINLEN;
            vec[0].ptr = (const u_int8_t *)(void *)&ext_dp->icmp_ext_version_res;
            vec[0].len = hlen;
            printf(", checksum 0x%04x (%scorrect), length %u",
                   EXTRACT_16BITS(ext_dp->icmp_ext_checksum),
                   in_cksum(vec, 1) ? "in" : "",
                   hlen);

            hlen -= 4; 
            obj_tptr = (u_int8_t *)ext_dp->icmp_ext_data;

            while (hlen > sizeof(struct icmp_mpls_ext_object_header_t)) {

                icmp_mpls_ext_object_header = (struct icmp_mpls_ext_object_header_t *)obj_tptr;
                TCHECK(*icmp_mpls_ext_object_header);
                obj_tlen = EXTRACT_16BITS(icmp_mpls_ext_object_header->length);
                obj_class_num = icmp_mpls_ext_object_header->class_num;
                obj_ctype = icmp_mpls_ext_object_header->ctype;
                obj_tptr += sizeof(struct icmp_mpls_ext_object_header_t);

                printf("\n\t  %s Object (%u), Class-Type: %u, length %u",
                       tok2str(icmp_mpls_ext_obj_values,"unknown",obj_class_num),
                       obj_class_num,
                       obj_ctype,
                       obj_tlen);

                hlen-=sizeof(struct icmp_mpls_ext_object_header_t); 

                                
                if ((obj_class_num == 0) ||
                    (obj_tlen < sizeof(struct icmp_mpls_ext_object_header_t))) {
                    return;
                }
                obj_tlen-=sizeof(struct icmp_mpls_ext_object_header_t);

                switch (obj_class_num) {
                case 1:
                    switch(obj_ctype) {
                    case 1:
                        TCHECK2(*obj_tptr, 4);
                        raw_label = EXTRACT_32BITS(obj_tptr);
                        printf("\n\t    label %u, exp %u", MPLS_LABEL(raw_label), MPLS_EXP(raw_label));
                        if (MPLS_STACK(raw_label))
                            printf(", [S]");
                        printf(", ttl %u", MPLS_TTL(raw_label));
                        break;
                    default:
                        print_unknown_data(obj_tptr, "\n\t    ", obj_tlen);
                    }
                    break;

                case 2:
                default:
                    print_unknown_data(obj_tptr, "\n\t    ", obj_tlen);
                    break;
                }
                if (hlen < obj_tlen)
                    break;
                hlen -= obj_tlen;
                obj_tptr += obj_tlen;
            }
        }

	return;
trunc:
	fputs("[|icmp]", stdout);
}
