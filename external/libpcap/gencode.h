/*
 * Copyright (c) 1990, 1991, 1992, 1993, 1994, 1995, 1996
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
 * @(#) $Header: /tcpdump/master/libpcap/gencode.h,v 1.71 2007-11-18 02:03:52 guy Exp $ (LBL)
 */

/*
 * ATM support:
 *
 * Copyright (c) 1997 Yen Yen Lim and North Dakota State University
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
 *      This product includes software developed by Yen Yen Lim and
 *      North Dakota State University
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef HAVE___ATTRIBUTE__
#define __attribute__(x)
#endif 


#define Q_HOST		1
#define Q_NET		2
#define Q_PORT		3
#define Q_GATEWAY	4
#define Q_PROTO		5
#define Q_PROTOCHAIN	6
#define Q_PORTRANGE	7


#define Q_LINK		1
#define Q_IP		2
#define Q_ARP		3
#define Q_RARP		4
#define Q_SCTP		5
#define Q_TCP		6
#define Q_UDP		7
#define Q_ICMP		8
#define Q_IGMP		9
#define Q_IGRP		10


#define	Q_ATALK		11
#define	Q_DECNET	12
#define	Q_LAT		13
#define Q_SCA		14
#define	Q_MOPRC		15
#define	Q_MOPDL		16


#define Q_IPV6		17
#define Q_ICMPV6	18
#define Q_AH		19
#define Q_ESP		20

#define Q_PIM		21
#define Q_VRRP		22

#define Q_AARP		23

#define Q_ISO		24
#define Q_ESIS		25
#define Q_ISIS		26
#define Q_CLNP		27

#define Q_STP		28

#define Q_IPX		29

#define Q_NETBEUI	30

#define Q_ISIS_L1       31
#define Q_ISIS_L2       32
#define Q_ISIS_IIH      33
#define Q_ISIS_LAN_IIH  34
#define Q_ISIS_PTP_IIH  35
#define Q_ISIS_SNP      36
#define Q_ISIS_CSNP     37
#define Q_ISIS_PSNP     38
#define Q_ISIS_LSP      39

#define Q_RADIO		40

#define Q_CARP		41


#define Q_SRC		1
#define Q_DST		2
#define Q_OR		3
#define Q_AND		4
#define Q_ADDR1		5
#define Q_ADDR2		6
#define Q_ADDR3		7
#define Q_ADDR4		8
#define Q_RA		9
#define Q_TA		10

#define Q_DEFAULT	0
#define Q_UNDEF		255

#define A_METAC		22	
#define A_BCC		23	
#define A_OAMF4SC	24	
#define A_OAMF4EC	25	
#define A_SC		26	
#define A_ILMIC		27	
#define A_OAM		28	
#define A_OAMF4		29	
#define A_LANE		30	
#define A_LLC		31	

#define A_SETUP		41	
#define A_CALLPROCEED	42	
#define A_CONNECT	43	
#define A_CONNECTACK	44	
#define A_RELEASE	45	
#define A_RELEASE_DONE	46	
 
#define A_VPI		51
#define A_VCI		52
#define A_PROTOTYPE	53
#define A_MSGTYPE	54
#define A_CALLREFTYPE	55

#define A_CONNECTMSG	70	
#define A_METACONNECT	71	

#define M_FISU		22	
#define M_LSSU		23	
#define M_MSU		24	

#define MH_FISU		25	
#define MH_LSSU		26	
#define MH_MSU		27	

#define M_SIO		1
#define M_OPC		2
#define M_DPC		3
#define M_SLS		4

#define MH_SIO		5
#define MH_OPC		6
#define MH_DPC		7
#define MH_SLS		8


struct slist;

struct stmt {
	int code;
	struct slist *jt;	
	struct slist *jf;	
	bpf_int32 k;
};

struct slist {
	struct stmt s;
	struct slist *next;
};

typedef bpf_u_int32 atomset;
#define ATOMMASK(n) (1 << (n))
#define ATOMELEM(d, n) (d & ATOMMASK(n))

typedef bpf_u_int32 *uset;

#define N_ATOMS (BPF_MEMWORDS+2)

struct edge {
	int id;
	int code;
	uset edom;
	struct block *succ;
	struct block *pred;
	struct edge *next;	
};

struct block {
	int id;
	struct slist *stmts;	
	struct stmt s;		
	int mark;
	u_int longjt;		
	u_int longjf;		
	int level;
	int offset;
	int sense;
	struct edge et;
	struct edge ef;
	struct block *head;
	struct block *link;	
	uset dom;
	uset closure;
	struct edge *in_edges;
	atomset def, kill;
	atomset in_use;
	atomset out_use;
	int oval;
	int val[N_ATOMS];
};

struct arth {
	struct block *b;	
	struct slist *s;	
	int regno;		
};

struct qual {
	unsigned char addr;
	unsigned char proto;
	unsigned char dir;
	unsigned char pad;
};

struct arth *gen_loadi(int);
struct arth *gen_load(int, struct arth *, int);
struct arth *gen_loadlen(void);
struct arth *gen_neg(struct arth *);
struct arth *gen_arth(int, struct arth *, struct arth *);

void gen_and(struct block *, struct block *);
void gen_or(struct block *, struct block *);
void gen_not(struct block *);

struct block *gen_scode(const char *, struct qual);
struct block *gen_ecode(const u_char *, struct qual);
struct block *gen_acode(const u_char *, struct qual);
struct block *gen_mcode(const char *, const char *, int, struct qual);
#ifdef INET6
struct block *gen_mcode6(const char *, const char *, int, struct qual);
#endif
struct block *gen_ncode(const char *, bpf_u_int32, struct qual);
struct block *gen_proto_abbrev(int);
struct block *gen_relation(int, struct arth *, struct arth *, int);
struct block *gen_less(int);
struct block *gen_greater(int);
struct block *gen_byteop(int, int, int);
struct block *gen_broadcast(int);
struct block *gen_multicast(int);
struct block *gen_inbound(int);

struct block *gen_vlan(int);
struct block *gen_mpls(int);

struct block *gen_pppoed(void);
struct block *gen_pppoes(int);

struct block *gen_atmfield_code(int atmfield, bpf_int32 jvalue, bpf_u_int32 jtype, int reverse);
struct block *gen_atmtype_abbrev(int type);
struct block *gen_atmmulti_abbrev(int type);

struct block *gen_mtp2type_abbrev(int type);
struct block *gen_mtp3field_code(int mtp3field, bpf_u_int32 jvalue, bpf_u_int32 jtype, int reverse);

struct block *gen_pf_ifname(const char *);
struct block *gen_pf_rnr(int);
struct block *gen_pf_srnr(int);
struct block *gen_pf_ruleset(char *);
struct block *gen_pf_reason(int);
struct block *gen_pf_action(int);
struct block *gen_pf_dir(int);

struct block *gen_p80211_type(int, int);
struct block *gen_p80211_fcdir(int);

void bpf_optimize(struct block **);
void bpf_error(const char *, ...)
    __attribute__((noreturn))
#ifdef __ATTRIBUTE___FORMAT_OK
    __attribute__((format (printf, 1, 2)))
#endif 
    ;

void finish_parse(struct block *);
char *sdup(const char *);

struct bpf_insn *icode_to_fcode(struct block *, u_int *);
int pcap_parse(void);
void lex_init(const char *);
void lex_cleanup(void);
void sappend(struct slist *, struct slist *);

#define JT(b)  ((b)->et.succ)
#define JF(b)  ((b)->ef.succ)

extern int no_optimize;
