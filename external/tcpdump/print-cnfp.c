
/*
 * Copyright (c) 1998 Michael Shalayeff
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Michael Shalayeff.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#ifndef lint
static const char rcsid[] _U_ =
    "@(#) $Header: /tcpdump/master/tcpdump/print-cnfp.c,v 1.17 2005-04-20 20:53:18 guy Exp $";
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

#include "tcp.h"
#include "ipproto.h"

struct nfhdr {
	u_int32_t	ver_cnt;	
	u_int32_t	msys_uptime;
	u_int32_t	utc_sec;
	u_int32_t	utc_nsec;
	u_int32_t	sequence;	
	u_int32_t	reserved;	
};

struct nfrec {
	struct in_addr	src_ina;
	struct in_addr	dst_ina;
	struct in_addr	nhop_ina;
	u_int32_t	ifaces;		
	u_int32_t	packets;
	u_int32_t	octets;
	u_int32_t	start_time;	
	u_int32_t	last_time;	
	u_int32_t	ports;		
	u_int32_t	proto_tos;	
	u_int32_t	asses;		
	u_int32_t	masks;		
	struct in_addr	peer_nexthop;	
};

void
cnfp_print(const u_char *cp, const u_char *bp _U_)
{
	register const struct nfhdr *nh;
	register const struct nfrec *nr;
	struct protoent *pent;
	int nrecs, ver;
#if 0
	time_t t;
#endif

	nh = (const struct nfhdr *)cp;

	if ((const u_char *)(nh + 1) > snapend)
		return;

	nrecs = EXTRACT_32BITS(&nh->ver_cnt) & 0xffff;
	ver = (EXTRACT_32BITS(&nh->ver_cnt) & 0xffff0000) >> 16;
#if 0
	t = EXTRACT_32BITS(&nh->utc_sec);
#endif

	printf("NetFlow v%x, %u.%03u uptime, %u.%09u, ", ver,
	       EXTRACT_32BITS(&nh->msys_uptime)/1000,
	       EXTRACT_32BITS(&nh->msys_uptime)%1000,
	       EXTRACT_32BITS(&nh->utc_sec), EXTRACT_32BITS(&nh->utc_nsec));

	if (ver == 5 || ver == 6) {
		printf("#%u, ", EXTRACT_32BITS(&nh->sequence));
		nr = (const struct nfrec *)&nh[1];
		snaplen -= 24;
	} else {
		nr = (const struct nfrec *)&nh->sequence;
		snaplen -= 16;
	}

	printf("%2u recs", nrecs);

	for (; nrecs-- && (const u_char *)(nr + 1) <= snapend; nr++) {
		char buf[20];
		char asbuf[20];

		printf("\n  started %u.%03u, last %u.%03u",
		       EXTRACT_32BITS(&nr->start_time)/1000,
		       EXTRACT_32BITS(&nr->start_time)%1000,
		       EXTRACT_32BITS(&nr->last_time)/1000,
		       EXTRACT_32BITS(&nr->last_time)%1000);

		asbuf[0] = buf[0] = '\0';
		if (ver == 5 || ver == 6) {
			snprintf(buf, sizeof(buf), "/%u",
				 (EXTRACT_32BITS(&nr->masks) >> 24) & 0xff);
			snprintf(asbuf, sizeof(asbuf), ":%u",
				 (EXTRACT_32BITS(&nr->asses) >> 16) & 0xffff);
		}
		printf("\n    %s%s%s:%u ", intoa(nr->src_ina.s_addr), buf, asbuf,
			EXTRACT_32BITS(&nr->ports) >> 16);

		if (ver == 5 || ver ==6) {
			snprintf(buf, sizeof(buf), "/%d",
				 (EXTRACT_32BITS(&nr->masks) >> 16) & 0xff);
			snprintf(asbuf, sizeof(asbuf), ":%u",
				 EXTRACT_32BITS(&nr->asses) & 0xffff);
		}
		printf("> %s%s%s:%u ", intoa(nr->dst_ina.s_addr), buf, asbuf,
			EXTRACT_32BITS(&nr->ports) & 0xffff);

		printf(">> %s\n    ", intoa(nr->nhop_ina.s_addr));

		pent = getprotobynumber((EXTRACT_32BITS(&nr->proto_tos) >> 8) & 0xff);
		if (!pent || nflag)
			printf("%u ",
			       (EXTRACT_32BITS(&nr->proto_tos) >> 8) & 0xff);
		else
			printf("%s ", pent->p_name);

		
		if (pent && pent->p_proto == IPPROTO_TCP) {
			int flags;
			if (ver == 1)
				flags = (EXTRACT_32BITS(&nr->asses) >> 24) & 0xff;
			else
				flags = (EXTRACT_32BITS(&nr->proto_tos) >> 16) & 0xff;
			if (flags & TH_FIN)	putchar('F');
			if (flags & TH_SYN)	putchar('S');
			if (flags & TH_RST)	putchar('R');
			if (flags & TH_PUSH)	putchar('P');
			if (flags & TH_ACK)	putchar('A');
			if (flags & TH_URG)	putchar('U');
			if (flags)
				putchar(' ');
		}

		buf[0]='\0';
		if (ver == 6) {
			snprintf(buf, sizeof(buf), "(%u<>%u encaps)",
				 (EXTRACT_32BITS(&nr->masks) >> 8) & 0xff,
				 (EXTRACT_32BITS(&nr->masks)) & 0xff);
		}
		printf("tos %u, %u (%u octets) %s",
		       EXTRACT_32BITS(&nr->proto_tos) & 0xff,
		       EXTRACT_32BITS(&nr->packets),
		       EXTRACT_32BITS(&nr->octets), buf);
	}
}
