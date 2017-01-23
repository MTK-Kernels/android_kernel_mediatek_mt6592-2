/*
 * Copyright (c) 1982, 1986, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * @(#) $Id: arcnet.h,v 1.3 2003-01-23 09:05:37 guy Exp $ (LBL)
 *
 * from: NetBSD: if_arc.h,v 1.13 1999/11/19 20:41:19 thorpej Exp
 */

struct	arc_header {
	u_int8_t  arc_shost;
	u_int8_t  arc_dhost;
	u_int8_t  arc_type;
	u_int8_t  arc_flag;
	u_int16_t arc_seqid;

	u_int8_t  arc_type2;	
	u_int8_t  arc_flag2;	
	u_int16_t arc_seqid2;	
};

#define	ARC_HDRLEN		3
#define	ARC_HDRNEWLEN		6
#define	ARC_HDRNEWLEN_EXC	10

#define	ARCTYPE_IP_OLD		240	
#define	ARCTYPE_ARP_OLD		241	

#define	ARCTYPE_IP		212	
#define	ARCTYPE_ARP		213	
#define	ARCTYPE_REVARP		214	

#define	ARCTYPE_ATALK		221	
#define	ARCTYPE_BANIAN		247	
#define	ARCTYPE_IPX		250	

#define ARCTYPE_INET6		0xc4	
#define ARCTYPE_DIAGNOSE	0x80	

struct	arc_linux_header {
	u_int8_t  arc_shost;
	u_int8_t  arc_dhost;
	u_int16_t arc_offset;
	u_int8_t  arc_type;
	u_int8_t  arc_flag;
	u_int16_t arc_seqid;
};

#define	ARC_LINUX_HDRLEN	5
#define	ARC_LINUX_HDRNEWLEN	8
