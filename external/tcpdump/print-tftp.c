/*
 * Copyright (c) 1990, 1991, 1993, 1994, 1995, 1996, 1997
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
 * Format and print trivial file transfer protocol packets.
 */

#ifndef lint
static const char rcsid[] _U_ =
    "@(#) $Header: /tcpdump/master/tcpdump/print-tftp.c,v 1.39 2008-04-11 16:47:38 gianluca Exp $ (LBL)";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <tcpdump-stdinc.h>

#ifdef SEGSIZE
#undef SEGSIZE					
#endif

#include <stdio.h>
#include <string.h>

#include "interface.h"
#include "addrtoname.h"
#include "extract.h"
#include "tftp.h"

static const struct tok op2str[] = {
	{ RRQ,		"RRQ" },	
	{ WRQ,		"WRQ" },	
	{ DATA,		"DATA" },	
	{ ACK,		"ACK" },	
	{ TFTP_ERROR,	"ERROR" },	
	{ OACK,		"OACK" },	
	{ 0,		NULL }
};

static const struct tok err2str[] = {
	{ EUNDEF,	"EUNDEF" },	
	{ ENOTFOUND,	"ENOTFOUND" },	
	{ EACCESS,	"EACCESS" },	
	{ ENOSPACE,	"ENOSPACE" },	
	{ EBADOP,	"EBADOP" },	
	{ EBADID,	"EBADID" },	
	{ EEXISTS,	"EEXISTS" },	
	{ ENOUSER,	"ENOUSER" },	
	{ 0,		NULL }
};

void
tftp_print(register const u_char *bp, u_int length)
{
	register const struct tftphdr *tp;
	register const char *cp;
	register const u_char *p;
	register int opcode, i;
	static char tstr[] = " [|tftp]";

	tp = (const struct tftphdr *)bp;

	
	printf(" %d", length);

	
	TCHECK(tp->th_opcode);
	opcode = EXTRACT_16BITS(&tp->th_opcode);
	cp = tok2str(op2str, "tftp-#%d", opcode);
	printf(" %s", cp);
	
	if (*cp == 't')
		return;

	switch (opcode) {

	case RRQ:
	case WRQ:
	case OACK:
		p = (u_char *)tp->th_stuff;
		putchar(' ');
		
		if (opcode != OACK)
			putchar('"');
		i = fn_print(p, snapend);
		if (opcode != OACK)
			putchar('"');

		
		while ((p = (const u_char *)strchr((const char *)p, '\0')) != NULL) {
			if (length <= (u_int)(p - (const u_char *)&tp->th_block))
				break;
			p++;
			if (*p != '\0') {
				putchar(' ');
				fn_print(p, snapend);
			}
		}
		
		if (i)
			goto trunc;
		break;

	case ACK:
	case DATA:
		TCHECK(tp->th_block);
		printf(" block %d", EXTRACT_16BITS(&tp->th_block));
		break;

	case TFTP_ERROR:
		
		TCHECK(tp->th_code);
		printf(" %s \"", tok2str(err2str, "tftp-err-#%d \"",
				       EXTRACT_16BITS(&tp->th_code)));
		
		i = fn_print((const u_char *)tp->th_data, snapend);
		putchar('"');
		if (i)
			goto trunc;
		break;

	default:
		
		printf("(unknown #%d)", opcode);
		break;
	}
	return;
trunc:
	fputs(tstr, stdout);
	return;
}
