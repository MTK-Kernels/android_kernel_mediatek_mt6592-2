/*
 * ppp-comp.h - Definitions for doing PPP packet compression.
 *
 * Copyright (c) 1984 Paul Mackerras. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The name(s) of the authors of this software must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission.
 *
 * 4. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by Paul Mackerras
 *     <paulus@samba.org>".
 *
 * THE AUTHORS OF THIS SOFTWARE DISCLAIM ALL WARRANTIES WITH REGARD TO
 * THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS, IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: ppp-comp.h,v 1.13 2002/12/06 09:49:15 paulus Exp $
 */

#ifndef _NET_PPP_COMP_H
#define _NET_PPP_COMP_H

#ifndef DO_BSD_COMPRESS
#define DO_BSD_COMPRESS	1	
#endif
#ifndef DO_DEFLATE
#define DO_DEFLATE	1	
#endif
#define DO_PREDICTOR_1	0
#define DO_PREDICTOR_2	0

#ifdef PACKETPTR
struct compressor {
	int	compress_proto;	

	
	void	*(*comp_alloc) __P((u_char *options, int opt_len));
	
	void	(*comp_free) __P((void *state));
	
	int	(*comp_init) __P((void *state, u_char *options, int opt_len,
				  int unit, int hdrlen, int debug));
	
	void	(*comp_reset) __P((void *state));
	
	int	(*compress) __P((void *state, PACKETPTR *mret,
				 PACKETPTR mp, int orig_len, int max_len));
	
	void	(*comp_stat) __P((void *state, struct compstat *stats));

	
	void	*(*decomp_alloc) __P((u_char *options, int opt_len));
	
	void	(*decomp_free) __P((void *state));
	
	int	(*decomp_init) __P((void *state, u_char *options, int opt_len,
				    int unit, int hdrlen, int mru, int debug));
	
	void	(*decomp_reset) __P((void *state));
	
	int	(*decompress) __P((void *state, PACKETPTR mp,
				   PACKETPTR *dmpp));
	
	void	(*incomp) __P((void *state, PACKETPTR mp));
	
	void	(*decomp_stat) __P((void *state, struct compstat *stats));
};
#endif 

#define DECOMP_OK		0	
#define DECOMP_ERROR		1	
#define DECOMP_FATALERROR	2	

#define CCP_CONFREQ	1
#define CCP_CONFACK	2
#define CCP_TERMREQ	5
#define CCP_TERMACK	6
#define CCP_RESETREQ	14
#define CCP_RESETACK	15

#define CCP_MAX_OPTION_LENGTH	32

#define CCP_CODE(dp)		((dp)[0])
#define CCP_ID(dp)		((dp)[1])
#define CCP_LENGTH(dp)		(((dp)[2] << 8) + (dp)[3])
#define CCP_HDRLEN		4

#define CCP_OPT_CODE(dp)	((dp)[0])
#define CCP_OPT_LENGTH(dp)	((dp)[1])
#define CCP_OPT_MINLEN		2

#define CI_BSD_COMPRESS		21	
#define CILEN_BSD_COMPRESS	3	

#define BSD_NBITS(x)		((x) & 0x1F)	
#define BSD_VERSION(x)		((x) >> 5)	
#define BSD_CURRENT_VERSION	1		
#define BSD_MAKE_OPT(v, n)	(((v) << 5) | (n))

#define BSD_MIN_BITS		9	
#define BSD_MAX_BITS		15	

#define CI_DEFLATE		26	
#define CI_DEFLATE_DRAFT	24	
#define CILEN_DEFLATE		4	

#define DEFLATE_MIN_SIZE	8
#define DEFLATE_MAX_SIZE	15
#define DEFLATE_METHOD_VAL	8
#define DEFLATE_SIZE(x)		(((x) >> 4) + DEFLATE_MIN_SIZE)
#define DEFLATE_METHOD(x)	((x) & 0x0F)
#define DEFLATE_MAKE_OPT(w)	((((w) - DEFLATE_MIN_SIZE) << 4) \
				 + DEFLATE_METHOD_VAL)
#define DEFLATE_CHK_SEQUENCE	0

#define CI_MPPE			18	
#define CILEN_MPPE		6	

#define CI_PREDICTOR_1		1	
#define CILEN_PREDICTOR_1	2	
#define CI_PREDICTOR_2		2	
#define CILEN_PREDICTOR_2	2	

#endif 
