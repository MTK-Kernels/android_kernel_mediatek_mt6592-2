/*
 * Copyright (c) 1992, 1993, 1994, 1995, 1996
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
 * @(#) $Header: /tcpdump/master/tcpdump/fddi.h,v 1.11 2002-12-11 07:13:51 guy Exp $ (LBL)
 */



struct fddi_header {
	u_char  fddi_fc;		
	u_char  fddi_dhost[6];
	u_char  fddi_shost[6];
};

#define FDDI_HDRLEN 13


#define	FDDIFC_C		0x80		
#define	FDDIFC_L		0x40		
#define	FDDIFC_F		0x30		
#define	FDDIFC_Z		0x0f		

#define	FDDIFC_VOID		0x40		
#define	FDDIFC_NRT		0x80		
#define	FDDIFC_RT		0xc0		
#define	FDDIFC_SMT_INFO		0x41		
#define	FDDIFC_SMT_NSA		0x4F		
#define	FDDIFC_MAC_BEACON	0xc2		
#define	FDDIFC_MAC_CLAIM	0xc3		
#define	FDDIFC_LLC_ASYNC	0x50		
#define	FDDIFC_LLC_SYNC		0xd0		
#define	FDDIFC_IMP_ASYNC	0x60		
#define	FDDIFC_IMP_SYNC		0xe0		
#define FDDIFC_SMT		0x40		
#define FDDIFC_MAC		0xc0		

#define	FDDIFC_CLFF		0xF0		
#define	FDDIFC_ZZZZ		0x0F		
