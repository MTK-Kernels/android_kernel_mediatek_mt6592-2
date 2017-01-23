/*
 * Definitions for tcp compression routines.
 *
 * $Id: slcompress.h,v 1.4 1994/09/21 06:50:08 paulus Exp $
 *
 * Copyright (c) 1989 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 *	Van Jacobson (van@helios.ee.lbl.gov), Dec 31, 1989:
 *	- Initial distribution.
 */

#ifndef _SLCOMPRESS_H_
#define _SLCOMPRESS_H_

#define MAX_STATES 16		
#define MAX_HDR MLEN		



#define TYPE_IP 0x40
#define TYPE_UNCOMPRESSED_TCP 0x70
#define TYPE_COMPRESSED_TCP 0x80
#define TYPE_ERROR 0x00

#define NEW_C	0x40	
#define NEW_I	0x20
#define NEW_S	0x08
#define NEW_A	0x04
#define NEW_W	0x02
#define NEW_U	0x01

#define SPECIAL_I (NEW_S|NEW_W|NEW_U)		
#define SPECIAL_D (NEW_S|NEW_A|NEW_W|NEW_U)	
#define SPECIALS_MASK (NEW_S|NEW_A|NEW_W|NEW_U)

#define TCP_PUSH_BIT 0x10


struct cstate {
	struct cstate *cs_next;	
	u_short cs_hlen;	
	u_char cs_id;		
	u_char cs_filler;
	union {
		char csu_hdr[MAX_HDR];
		struct ip csu_ip;	
	} slcs_u;
};
#define cs_ip slcs_u.csu_ip
#define cs_hdr slcs_u.csu_hdr

struct slcompress {
	struct cstate *last_cs;	
	u_char last_recv;	
	u_char last_xmit;	
	u_short flags;
#ifndef SL_NO_STATS
	int sls_packets;	
	int sls_compressed;	
	int sls_searches;	
	int sls_misses;		
	int sls_uncompressedin;	
	int sls_compressedin;	
	int sls_errorin;	
	int sls_tossed;		
#endif
	struct cstate tstate[MAX_STATES];	
	struct cstate rstate[MAX_STATES];	
};
#define SLF_TOSS 1		

void	sl_compress_init __P((struct slcompress *));
void	sl_compress_setup __P((struct slcompress *, int));
u_int	sl_compress_tcp __P((struct mbuf *,
	    struct ip *, struct slcompress *, int));
int	sl_uncompress_tcp __P((u_char **, int, u_int, struct slcompress *));
int	sl_uncompress_tcp_core __P((u_char *, int, int, u_int,
	    struct slcompress *, u_char **, u_int *));

#endif 
