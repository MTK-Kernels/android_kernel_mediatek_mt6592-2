/*
 * Copyright (c) 1988, 1989, 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1997
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
    "@(#) $Header: /tcpdump/master/tcpdump/print-sll.c,v 1.19 2005-11-13 12:12:43 guy Exp $ (LBL)";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <tcpdump-stdinc.h>

#include <stdio.h>
#include <string.h>
#include <pcap.h>

#include "interface.h"
#include "addrtoname.h"
#include "ethertype.h"
#include "extract.h"

#include "ether.h"
#include "sll.h"

static const struct tok sll_pkttype_values[] = {
    { LINUX_SLL_HOST, "In" },
    { LINUX_SLL_BROADCAST, "B" },
    { LINUX_SLL_MULTICAST, "M" },
    { LINUX_SLL_OTHERHOST, "P" },
    { LINUX_SLL_OUTGOING, "Out" },
    { 0, NULL}
};

static inline void
sll_print(register const struct sll_header *sllp, u_int length)
{
	u_short ether_type;

        printf("%3s ",tok2str(sll_pkttype_values,"?",EXTRACT_16BITS(&sllp->sll_pkttype)));

	if (EXTRACT_16BITS(&sllp->sll_halen) == 6)
		(void)printf("%s ", etheraddr_string(sllp->sll_addr));

	if (!qflag) {
		ether_type = EXTRACT_16BITS(&sllp->sll_protocol);
	
		if (ether_type <= ETHERMTU) {
			switch (ether_type) {

			case LINUX_SLL_P_802_3:
				(void)printf("802.3");
				break;

			case LINUX_SLL_P_802_2:
				(void)printf("802.2");
				break;

			default:
				(void)printf("ethertype Unknown (0x%04x)",
				    ether_type);
				break;
			}
		} else {
			(void)printf("ethertype %s (0x%04x)",
			    tok2str(ethertype_values, "Unknown", ether_type),
			    ether_type);
		}
		(void)printf(", length %u: ", length);
	}
}

u_int
sll_if_print(const struct pcap_pkthdr *h, const u_char *p)
{
	u_int caplen = h->caplen;
	u_int length = h->len;
	register const struct sll_header *sllp;
	u_short ether_type;
	u_short extracted_ethertype;

	if (caplen < SLL_HDR_LEN) {
		printf("[|sll]");
		return (caplen);
	}

	sllp = (const struct sll_header *)p;

	if (eflag)
		sll_print(sllp, length);

	length -= SLL_HDR_LEN;
	caplen -= SLL_HDR_LEN;
	p += SLL_HDR_LEN;

	ether_type = EXTRACT_16BITS(&sllp->sll_protocol);

recurse:
	if (ether_type <= ETHERMTU) {
		switch (ether_type) {

		case LINUX_SLL_P_802_3:
			ipx_print(p, length);
			break;

		case LINUX_SLL_P_802_2:
			if (llc_print(p, length, caplen, NULL, NULL,
			    &extracted_ethertype) == 0)
				goto unknown;	
			break;

		default:
			extracted_ethertype = 0;
			

		unknown:
			
			if (!eflag)
				sll_print(sllp, length + SLL_HDR_LEN);
			if (extracted_ethertype) {
				printf("(LLC %s) ",
			       etherproto_string(htons(extracted_ethertype)));
			}
			if (!suppress_default_print)
				default_print(p, caplen);
			break;
		}
	} else if (ether_type == ETHERTYPE_8021Q) {
		if (caplen < 4 || length < 4) {
			printf("[|vlan]");
			return (SLL_HDR_LEN);
		}
	        if (eflag) {
	        	u_int16_t tag = EXTRACT_16BITS(p);

			printf("vlan %u, p %u%s, ",
			    tag & 0xfff,
			    tag >> 13,
			    (tag & 0x1000) ? ", CFI" : "");
		}

		ether_type = EXTRACT_16BITS(p + 2);
		if (ether_type <= ETHERMTU)
			ether_type = LINUX_SLL_P_802_2;
		if (!qflag) {
			(void)printf("ethertype %s, ",
			    tok2str(ethertype_values, "Unknown", ether_type));
		}
		p += 4;
		length -= 4;
		caplen -= 4;
		goto recurse;
	} else {
		if (ethertype_print(gndo, ether_type, p, length, caplen) == 0) {
			
			if (!eflag)
				sll_print(sllp, length + SLL_HDR_LEN);
			if (!suppress_default_print)
				default_print(p, caplen);
		}
	}

	return (SLL_HDR_LEN);
}
