/*
 * Copyright (C) 2001 WIDE Project.  All rights reserved.
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

#ifndef lint
static const char rcsid[] _U_ =
    "@(#) $Header: /tcpdump/master/tcpdump/print-mpls.c,v 1.14 2005-07-05 09:38:19 hannes Exp $ (LBL)";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <tcpdump-stdinc.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "addrtoname.h"
#include "interface.h"
#include "extract.h"			
#include "mpls.h"

static const char *mpls_labelname[] = {
	"IPv4 explicit NULL", "router alert", "IPv6 explicit NULL",
	"implicit NULL", "rsvd",
	"rsvd", "rsvd", "rsvd", "rsvd", "rsvd",
	"rsvd", "rsvd", "rsvd", "rsvd", "rsvd",
	"rsvd",
};

enum mpls_packet_type {
	PT_UNKNOWN,
	PT_IPV4,
	PT_IPV6,
	PT_OSI
};

void
mpls_print(const u_char *bp, u_int length)
{
	const u_char *p;
	u_int32_t label_entry;
	u_int16_t label_stack_depth = 0;
	enum mpls_packet_type pt = PT_UNKNOWN;

	p = bp;
	printf("MPLS");
	do {
		TCHECK2(*p, sizeof(label_entry));
		label_entry = EXTRACT_32BITS(p);
		printf("%s(label %u",
		       (label_stack_depth && vflag) ? "\n\t" : " ",
       		       MPLS_LABEL(label_entry));
		label_stack_depth++;
		if (vflag &&
		    MPLS_LABEL(label_entry) < sizeof(mpls_labelname) / sizeof(mpls_labelname[0]))
			printf(" (%s)", mpls_labelname[MPLS_LABEL(label_entry)]);
		printf(", exp %u", MPLS_EXP(label_entry));
		if (MPLS_STACK(label_entry))
			printf(", [S]");
		printf(", ttl %u)", MPLS_TTL(label_entry));

		p += sizeof(label_entry);
	} while (!MPLS_STACK(label_entry));

	switch (MPLS_LABEL(label_entry)) {

	case 0:	
	case 3:	
		pt = PT_IPV4;
		break;

	case 2:	
		pt = PT_IPV6;
		break;

	default:
		switch(*p) {

		case 0x45:
		case 0x46:
		case 0x47:
		case 0x48:
		case 0x49:
		case 0x4a:
		case 0x4b:
		case 0x4c:
		case 0x4d:
		case 0x4e:
		case 0x4f:
			pt = PT_IPV4;
			break;
				
		case 0x60:
		case 0x61:
		case 0x62:
		case 0x63:
		case 0x64:
		case 0x65:
		case 0x66:
		case 0x67:
		case 0x68:
		case 0x69:
		case 0x6a:
		case 0x6b:
		case 0x6c:
		case 0x6d:
		case 0x6e:
		case 0x6f:
			pt = PT_IPV6;
			break;

		case 0x81:
		case 0x82:
		case 0x83:
			pt = PT_OSI;
			break;

		default:
			
			break;
		}
	}

	if (pt == PT_UNKNOWN) {
		if (!suppress_default_print)
			default_print(p, length - (p - bp));
		return;
	}
	if (vflag)
		printf("\n\t");
	else
		printf(" ");
	switch (pt) {

	case PT_IPV4:
		ip_print(gndo, p, length - (p - bp));
		break;

	case PT_IPV6:
#ifdef INET6
		ip6_print(gndo, p, length - (p - bp));
#else
		printf("IPv6, length: %u", length);
#endif
		break;

	case PT_OSI:
		isoclns_print(p, length - (p - bp), length - (p - bp));
		break;

	default:
		break;
	}
	return;

trunc:
	printf("[|MPLS]");
}


