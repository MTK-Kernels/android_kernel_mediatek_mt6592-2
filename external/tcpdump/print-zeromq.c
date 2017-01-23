/*
 * This file implements decoding of ZeroMQ network protocol(s).
 *
 *
 * Copyright (c) 2013 The TCPDUMP project
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <tcpdump-stdinc.h>

#include <stdio.h>

#include "interface.h"
#include "extract.h"

#define VBYTES 128


static const u_char *
zmtp1_print_frame(const u_char *cp, const u_char *ep) {
	u_int64_t body_len_declared, body_len_captured, header_len;
	u_int8_t flags;

	printf("\n\t");
	TCHECK2(*cp, 1); 

	if (cp[0] != 0xFF) {
		header_len = 1; 
		body_len_declared = cp[0];
		if (body_len_declared == 0)
			return cp + header_len; 
		printf(" frame flags+body  (8-bit) length %u", cp[0]);
		TCHECK2(*cp, header_len + 1); 
		flags = cp[1];
	} else {
		header_len = 1 + 8; 
		printf(" frame flags+body (64-bit) length");
		TCHECK2(*cp, header_len); 
		body_len_declared = EXTRACT_64BITS(cp + 1);
		if (body_len_declared == 0)
			return cp + header_len; 
		printf(" %" PRIu64, body_len_declared);
		TCHECK2(*cp, header_len + 1); 
		flags = cp[9];
	}

	body_len_captured = ep - cp - header_len;
	if (body_len_declared > body_len_captured)
		printf(" (%" PRIu64 " captured)", body_len_captured);
	printf(", flags 0x%02x", flags);

	if (vflag) {
		u_int64_t body_len_printed = MIN(body_len_captured, body_len_declared);

		printf(" (%s|%s|%s|%s|%s|%s|%s|%s)",
			flags & 0x80 ? "MBZ" : "-",
			flags & 0x40 ? "MBZ" : "-",
			flags & 0x20 ? "MBZ" : "-",
			flags & 0x10 ? "MBZ" : "-",
			flags & 0x08 ? "MBZ" : "-",
			flags & 0x04 ? "MBZ" : "-",
			flags & 0x02 ? "MBZ" : "-",
			flags & 0x01 ? "MORE" : "-");

		if (vflag == 1)
			body_len_printed = MIN(VBYTES + 1, body_len_printed);
		if (body_len_printed > 1) {
			printf(", first %" PRIu64 " byte(s) of body:", body_len_printed - 1);
			hex_and_ascii_print("\n\t ", cp + header_len + 1, body_len_printed - 1);
			printf("\n");
		}
	}

	TCHECK2(*cp, header_len + body_len_declared); 
	return cp + header_len + body_len_declared;

trunc:
	printf(" [|zmtp1]");
	return ep;
}

void
zmtp1_print(const u_char *cp, u_int len) {
	const u_char *ep = MIN(snapend, cp + len);

	printf(": ZMTP/1.0");
	while (cp < ep)
		cp = zmtp1_print_frame(cp, ep);
}


static const u_char *
zmtp1_print_intermediate_part(const u_char *cp, const u_int len) {
	u_int frame_offset;
	u_int64_t remaining_len;

	TCHECK2(*cp, 2);
	frame_offset = EXTRACT_16BITS(cp);
	printf("\n\t frame offset 0x%04x", frame_offset);
	cp += 2;
	remaining_len = snapend - cp; 

	if (frame_offset == 0xFFFF)
		frame_offset = len - 2; 
	else if (2 + frame_offset > len) {
		printf(" (exceeds datagram declared length)");
		goto trunc;
	}

	
	if (frame_offset) {
		printf("\n\t frame intermediate part, %u bytes", frame_offset);
		if (frame_offset > remaining_len)
			printf(" (%"PRIu64" captured)", remaining_len);
		if (vflag) {
			u_int64_t len_printed = MIN(frame_offset, remaining_len);

			if (vflag == 1)
				len_printed = MIN(VBYTES, len_printed);
			if (len_printed > 1) {
				printf(", first %"PRIu64" byte(s):", len_printed);
				hex_and_ascii_print("\n\t ", cp, len_printed);
				printf("\n");
			}
		}
	}
	return cp + frame_offset;

trunc:
	printf(" [|zmtp1]");
	return cp + len;
}

void
zmtp1_print_datagram(const u_char *cp, const u_int len) {
	const u_char *ep = MIN(snapend, cp + len);

	cp = zmtp1_print_intermediate_part(cp, len);
	while (cp < ep)
		cp = zmtp1_print_frame(cp, ep);
}
