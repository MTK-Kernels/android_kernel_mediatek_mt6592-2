/*
 * Marko Kiiskila carnil@cs.tut.fi
 *
 * Tampere University of Technology - Telecommunications Laboratory
 *
 * Permission to use, copy, modify and distribute this
 * software and its documentation is hereby granted,
 * provided that both the copyright notice and this
 * permission notice appear in all copies of the software,
 * derivative works or modified versions, and any portions
 * thereof, that both notices appear in supporting
 * documentation, and that the use of this software is
 * acknowledged in any publications resulting from using
 * the software.
 *
 * TUT ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION AND DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS
 * SOFTWARE.
 *
 */

#ifndef lint
static const char rcsid[] _U_ =
    "@(#) $Header: /tcpdump/master/tcpdump/print-cip.c,v 1.26 2005-07-07 01:22:17 guy Exp $ (LBL)";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include <tcpdump-stdinc.h>

#include <stdio.h>
#include <pcap.h>

#include "interface.h"
#include "addrtoname.h"
#include "ethertype.h"
#include "ether.h"

#define RFC1483LLC_LEN	8

static unsigned char rfcllc[] = {
	0xaa,	
	0xaa,	
	0x03,	
	0x00,	
	0x00,
	0x00 };

static inline void
cip_print(int length)
{
	printf("%d: ", length);
}

u_int
cip_if_print(const struct pcap_pkthdr *h, const u_char *p)
{
	u_int caplen = h->caplen;
	u_int length = h->len;
	u_short extracted_ethertype;

	if (memcmp(rfcllc, p, sizeof(rfcllc))==0 && caplen < RFC1483LLC_LEN) {
		printf("[|cip]");
		return (0);
	}

	if (eflag)
		cip_print(length);

	if (memcmp(rfcllc, p, sizeof(rfcllc)) == 0) {
		if (llc_print(p, length, caplen, NULL, NULL,
		    &extracted_ethertype) == 0) {
			
			if (!eflag)
				cip_print(length);
			if (extracted_ethertype) {
				printf("(LLC %s) ",
			       etherproto_string(htons(extracted_ethertype)));
			}
			if (!suppress_default_print)
				default_print(p, caplen);
		}
	} else {
		ip_print(gndo, p, length);
	}

	return (0);
}


