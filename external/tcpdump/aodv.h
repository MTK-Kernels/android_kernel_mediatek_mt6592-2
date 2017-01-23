/*
 * Copyright (c) 2003 Bruce M. Simpson <bms@spc.org>
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
 *        This product includes software developed by Bruce M. Simpson.
 * 4. Neither the name of Bruce M. Simpson nor the names of co-
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bruce M. Simpson AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL Bruce M. Simpson OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef _AODV_H_
#define _AODV_H_

struct aodv_rreq {
	u_int8_t	rreq_type;	
	u_int8_t	rreq_flags;	
	u_int8_t	rreq_zero0;	
	u_int8_t	rreq_hops;	
	u_int32_t	rreq_id;	
	u_int32_t	rreq_da;	
	u_int32_t	rreq_ds;	
	u_int32_t	rreq_oa;	
	u_int32_t	rreq_os;	
};
#ifdef INET6
struct aodv_rreq6 {
	u_int8_t	rreq_type;	
	u_int8_t	rreq_flags;	
	u_int8_t	rreq_zero0;	
	u_int8_t	rreq_hops;	
	u_int32_t	rreq_id;	
	struct in6_addr	rreq_da;	
	u_int32_t	rreq_ds;	
	struct in6_addr	rreq_oa;	
	u_int32_t	rreq_os;	
};
struct aodv_rreq6_draft_01 {
	u_int8_t	rreq_type;	
	u_int8_t	rreq_flags;	
	u_int8_t	rreq_zero0;	
	u_int8_t	rreq_hops;	
	u_int32_t	rreq_id;	
	u_int32_t	rreq_ds;	
	u_int32_t	rreq_os;	
	struct in6_addr	rreq_da;	
	struct in6_addr	rreq_oa;	
};
#endif

#define	RREQ_JOIN	0x80		
#define	RREQ_REPAIR	0x40		
#define	RREQ_GRAT	0x20		
#define	RREQ_DEST	0x10		
#define	RREQ_UNKNOWN	0x08		
#define	RREQ_FLAGS_MASK	0xF8		

struct aodv_rrep {
	u_int8_t	rrep_type;	
	u_int8_t	rrep_flags;	
	u_int8_t	rrep_ps;	
	u_int8_t	rrep_hops;	
	u_int32_t	rrep_da;	
	u_int32_t	rrep_ds;	
	u_int32_t	rrep_oa;	
	u_int32_t	rrep_life;	
};
#ifdef INET6
struct aodv_rrep6 {
	u_int8_t	rrep_type;	
	u_int8_t	rrep_flags;	
	u_int8_t	rrep_ps;	
	u_int8_t	rrep_hops;	
	struct in6_addr	rrep_da;	
	u_int32_t	rrep_ds;	
	struct in6_addr	rrep_oa;	
	u_int32_t	rrep_life;	
};
struct aodv_rrep6_draft_01 {
	u_int8_t	rrep_type;	
	u_int8_t	rrep_flags;	
	u_int8_t	rrep_ps;	
	u_int8_t	rrep_hops;	
	u_int32_t	rrep_ds;	
	struct in6_addr	rrep_da;	
	struct in6_addr	rrep_oa;	
	u_int32_t	rrep_life;	
};
#endif

#define	RREP_REPAIR		0x80	
#define	RREP_ACK		0x40	
#define	RREP_FLAGS_MASK		0xC0	
#define	RREP_PREFIX_MASK	0x1F	

struct rerr_unreach {
	u_int32_t	u_da;	
	u_int32_t	u_ds;	
};
#ifdef INET6
struct rerr_unreach6 {
	struct in6_addr	u_da;	
	u_int32_t	u_ds;	
};
struct rerr_unreach6_draft_01 {
	struct in6_addr	u_da;	
	u_int32_t	u_ds;	
};
#endif

struct aodv_rerr {
	u_int8_t	rerr_type;	
	u_int8_t	rerr_flags;	
	u_int8_t	rerr_zero0;	
	u_int8_t	rerr_dc;	
	union {
		struct	rerr_unreach dest[1];
#ifdef INET6
		struct	rerr_unreach6 dest6[1];
		struct	rerr_unreach6_draft_01 dest6_draft_01[1];
#endif
	} r;
};

#define RERR_NODELETE		0x80	
#define RERR_FLAGS_MASK		0x80	

struct aodv_rrep_ack {
	u_int8_t	ra_type;
	u_int8_t	ra_zero0;
};

union aodv {
	struct aodv_rreq rreq;
	struct aodv_rrep rrep;
	struct aodv_rerr rerr;
	struct aodv_rrep_ack rrep_ack;
#ifdef INET6
	struct aodv_rreq6 rreq6;
	struct aodv_rreq6_draft_01 rreq6_draft_01;
	struct aodv_rrep6 rrep6;
	struct aodv_rrep6_draft_01 rrep6_draft_01;
#endif
};

#define	AODV_RREQ		1	
#define	AODV_RREP		2	
#define	AODV_RERR		3	
#define	AODV_RREP_ACK		4	

#define AODV_V6_DRAFT_01_RREQ		16	
#define AODV_V6_DRAFT_01_RREP		17	
#define AODV_V6_DRAFT_01_RERR		18	
#define AODV_V6_DRAFT_01_RREP_ACK	19	

struct aodv_ext {
	u_int8_t	type;		
	u_int8_t	length;		
};

struct aodv_hello {
	struct	aodv_ext	eh;		
	u_int8_t		interval[4];	
};

#define	AODV_EXT_HELLO	1

#endif 
