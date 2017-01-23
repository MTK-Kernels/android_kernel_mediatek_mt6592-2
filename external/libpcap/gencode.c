/*
 * Copyright (c) 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1997, 1998
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
 */
#ifndef lint
static const char rcsid[] _U_ =
    "@(#) $Header: /tcpdump/master/libpcap/gencode.c,v 1.309 2008-12-23 20:13:29 guy Exp $ (LBL)";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef WIN32
#include <pcap-stdinc.h>
#else 
#if HAVE_INTTYPES_H
#include <inttypes.h>
#elif HAVE_STDINT_H
#include <stdint.h>
#endif
#ifdef HAVE_SYS_BITYPES_H
#include <sys/bitypes.h>
#endif
#include <sys/types.h>
#include <sys/socket.h>
#endif 

#ifdef __MINGW32__
#include "ip6_misc.h"
#endif

#ifndef WIN32

#ifdef __NetBSD__
#include <sys/param.h>
#endif

#include <netinet/in.h>
#include <arpa/inet.h>

#endif 

#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <setjmp.h>
#include <stdarg.h>

#ifdef MSDOS
#include "pcap-dos.h"
#endif

#include "pcap-int.h"

#include "ethertype.h"
#include "nlpid.h"
#include "llc.h"
#include "gencode.h"
#include "ieee80211.h"
#include "atmuni31.h"
#include "sunatmpos.h"
#include "ppp.h"
#include "pcap/sll.h"
#include "pcap/ipnet.h"
#include "arcnet.h"
#if defined(linux) && defined(PF_PACKET) && defined(SO_ATTACH_FILTER)
#include <linux/types.h>
#include <linux/if_packet.h>
#include <linux/filter.h>
#endif
#ifdef HAVE_NET_PFVAR_H
#include <sys/socket.h>
#include <net/if.h>
#include <net/pfvar.h>
#include <net/if_pflog.h>
#endif
#ifndef offsetof
#define offsetof(s, e) ((size_t)&((s *)0)->e)
#endif
#ifdef INET6
#ifndef WIN32
#include <netdb.h>	
#endif 
#endif 
#include <pcap/namedb.h>

#define ETHERMTU	1500

#ifndef IPPROTO_HOPOPTS
#define IPPROTO_HOPOPTS 0
#endif
#ifndef IPPROTO_ROUTING
#define IPPROTO_ROUTING 43
#endif
#ifndef IPPROTO_FRAGMENT
#define IPPROTO_FRAGMENT 44
#endif
#ifndef IPPROTO_DSTOPTS
#define IPPROTO_DSTOPTS 60
#endif
#ifndef IPPROTO_SCTP
#define IPPROTO_SCTP 132
#endif

#ifdef HAVE_OS_PROTO_H
#include "os-proto.h"
#endif

#define JMP(c) ((c)|BPF_JMP|BPF_K)

static jmp_buf top_ctx;
static pcap_t *bpf_pcap;

#ifdef WIN32
static u_int	orig_linktype = (u_int)-1, orig_nl = (u_int)-1, label_stack_depth = (u_int)-1;
#else
static u_int	orig_linktype = -1U, orig_nl = -1U, label_stack_depth = -1U;
#endif

static int	pcap_fddipad;

void
bpf_error(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	if (bpf_pcap != NULL)
		(void)vsnprintf(pcap_geterr(bpf_pcap), PCAP_ERRBUF_SIZE,
		    fmt, ap);
	va_end(ap);
	longjmp(top_ctx, 1);
	
}

static void init_linktype(pcap_t *);

static void init_regs(void);
static int alloc_reg(void);
static void free_reg(int);

static struct block *root;

enum e_offrel {
	OR_PACKET,	
	OR_LINK,	
	OR_MACPL,	
	OR_NET,		
	OR_NET_NOSNAP,	
	OR_TRAN_IPV4,	
	OR_TRAN_IPV6	
};

#ifdef INET6
static struct addrinfo *ai;
#endif

#define NCHUNKS 16
#define CHUNK0SIZE 1024
struct chunk {
	u_int n_left;
	void *m;
};

static struct chunk chunks[NCHUNKS];
static int cur_chunk;

static void *newchunk(u_int);
static void freechunks(void);
static inline struct block *new_block(int);
static inline struct slist *new_stmt(int);
static struct block *gen_retblk(int);
static inline void syntax(void);

static void backpatch(struct block *, struct block *);
static void merge(struct block *, struct block *);
static struct block *gen_cmp(enum e_offrel, u_int, u_int, bpf_int32);
static struct block *gen_cmp_gt(enum e_offrel, u_int, u_int, bpf_int32);
static struct block *gen_cmp_ge(enum e_offrel, u_int, u_int, bpf_int32);
static struct block *gen_cmp_lt(enum e_offrel, u_int, u_int, bpf_int32);
static struct block *gen_cmp_le(enum e_offrel, u_int, u_int, bpf_int32);
static struct block *gen_mcmp(enum e_offrel, u_int, u_int, bpf_int32,
    bpf_u_int32);
static struct block *gen_bcmp(enum e_offrel, u_int, u_int, const u_char *);
static struct block *gen_ncmp(enum e_offrel, bpf_u_int32, bpf_u_int32,
    bpf_u_int32, bpf_u_int32, int, bpf_int32);
static struct slist *gen_load_llrel(u_int, u_int);
static struct slist *gen_load_macplrel(u_int, u_int);
static struct slist *gen_load_a(enum e_offrel, u_int, u_int);
static struct slist *gen_loadx_iphdrlen(void);
static struct block *gen_uncond(int);
static inline struct block *gen_true(void);
static inline struct block *gen_false(void);
static struct block *gen_ether_linktype(int);
static struct block *gen_ipnet_linktype(int);
static struct block *gen_linux_sll_linktype(int);
static struct slist *gen_load_prism_llprefixlen(void);
static struct slist *gen_load_avs_llprefixlen(void);
static struct slist *gen_load_radiotap_llprefixlen(void);
static struct slist *gen_load_ppi_llprefixlen(void);
static void insert_compute_vloffsets(struct block *);
static struct slist *gen_llprefixlen(void);
static struct slist *gen_off_macpl(void);
static int ethertype_to_ppptype(int);
static struct block *gen_linktype(int);
static struct block *gen_snap(bpf_u_int32, bpf_u_int32);
static struct block *gen_llc_linktype(int);
static struct block *gen_hostop(bpf_u_int32, bpf_u_int32, int, int, u_int, u_int);
#ifdef INET6
static struct block *gen_hostop6(struct in6_addr *, struct in6_addr *, int, int, u_int, u_int);
#endif
static struct block *gen_ahostop(const u_char *, int);
static struct block *gen_ehostop(const u_char *, int);
static struct block *gen_fhostop(const u_char *, int);
static struct block *gen_thostop(const u_char *, int);
static struct block *gen_wlanhostop(const u_char *, int);
static struct block *gen_ipfchostop(const u_char *, int);
static struct block *gen_dnhostop(bpf_u_int32, int);
static struct block *gen_mpls_linktype(int);
static struct block *gen_host(bpf_u_int32, bpf_u_int32, int, int, int);
#ifdef INET6
static struct block *gen_host6(struct in6_addr *, struct in6_addr *, int, int, int);
#endif
#ifndef INET6
static struct block *gen_gateway(const u_char *, bpf_u_int32 **, int, int);
#endif
static struct block *gen_ipfrag(void);
static struct block *gen_portatom(int, bpf_int32);
static struct block *gen_portrangeatom(int, bpf_int32, bpf_int32);
static struct block *gen_portatom6(int, bpf_int32);
static struct block *gen_portrangeatom6(int, bpf_int32, bpf_int32);
struct block *gen_portop(int, int, int);
static struct block *gen_port(int, int, int);
struct block *gen_portrangeop(int, int, int, int);
static struct block *gen_portrange(int, int, int, int);
struct block *gen_portop6(int, int, int);
static struct block *gen_port6(int, int, int);
struct block *gen_portrangeop6(int, int, int, int);
static struct block *gen_portrange6(int, int, int, int);
static int lookup_proto(const char *, int);
static struct block *gen_protochain(int, int, int);
static struct block *gen_proto(int, int, int);
static struct slist *xfer_to_x(struct arth *);
static struct slist *xfer_to_a(struct arth *);
static struct block *gen_mac_multicast(int);
static struct block *gen_len(int, int);
static struct block *gen_check_802_11_data_frame(void);

static struct block *gen_ppi_dlt_check(void);
static struct block *gen_msg_abbrev(int type);

static void *
newchunk(n)
	u_int n;
{
	struct chunk *cp;
	int k;
	size_t size;

#ifndef __NetBSD__
	
	n = (n + sizeof(long) - 1) & ~(sizeof(long) - 1);
#else
	
	n = ALIGN(n);
#endif

	cp = &chunks[cur_chunk];
	if (n > cp->n_left) {
		++cp, k = ++cur_chunk;
		if (k >= NCHUNKS)
			bpf_error("out of memory");
		size = CHUNK0SIZE << k;
		cp->m = (void *)malloc(size);
		if (cp->m == NULL)
			bpf_error("out of memory");
		memset((char *)cp->m, 0, size);
		cp->n_left = size;
		if (n > size)
			bpf_error("out of memory");
	}
	cp->n_left -= n;
	return (void *)((char *)cp->m + cp->n_left);
}

static void
freechunks()
{
	int i;

	cur_chunk = 0;
	for (i = 0; i < NCHUNKS; ++i)
		if (chunks[i].m != NULL) {
			free(chunks[i].m);
			chunks[i].m = NULL;
		}
}

char *
sdup(s)
	register const char *s;
{
	int n = strlen(s) + 1;
	char *cp = newchunk(n);

	strlcpy(cp, s, n);
	return (cp);
}

static inline struct block *
new_block(code)
	int code;
{
	struct block *p;

	p = (struct block *)newchunk(sizeof(*p));
	p->s.code = code;
	p->head = p;

	return p;
}

static inline struct slist *
new_stmt(code)
	int code;
{
	struct slist *p;

	p = (struct slist *)newchunk(sizeof(*p));
	p->s.code = code;

	return p;
}

static struct block *
gen_retblk(v)
	int v;
{
	struct block *b = new_block(BPF_RET|BPF_K);

	b->s.k = v;
	return b;
}

static inline void
syntax()
{
	bpf_error("syntax error in filter expression");
}

static bpf_u_int32 netmask;
static int snaplen;
int no_optimize;
#ifdef WIN32
static int
pcap_compile_unsafe(pcap_t *p, struct bpf_program *program,
	     const char *buf, int optimize, bpf_u_int32 mask);

int
pcap_compile(pcap_t *p, struct bpf_program *program,
	     const char *buf, int optimize, bpf_u_int32 mask)
{
	int result;

	EnterCriticalSection(&g_PcapCompileCriticalSection);

	result = pcap_compile_unsafe(p, program, buf, optimize, mask);

	LeaveCriticalSection(&g_PcapCompileCriticalSection);
	
	return result;
}

static int
pcap_compile_unsafe(pcap_t *p, struct bpf_program *program,
	     const char *buf, int optimize, bpf_u_int32 mask)
#else 
int
pcap_compile(pcap_t *p, struct bpf_program *program,
	     const char *buf, int optimize, bpf_u_int32 mask)
#endif 
{
	extern int n_errors;
	const char * volatile xbuf = buf;
	u_int len;

	if (!p->activated) {
		snprintf(p->errbuf, PCAP_ERRBUF_SIZE,
		    "not-yet-activated pcap_t passed to pcap_compile");
		return (-1);
	}
	no_optimize = 0;
	n_errors = 0;
	root = NULL;
	bpf_pcap = p;
	init_regs();
	if (setjmp(top_ctx)) {
#ifdef INET6
		if (ai != NULL) {
			freeaddrinfo(ai);
			ai = NULL;
		}
#endif
		lex_cleanup();
		freechunks();
		return (-1);
	}

	netmask = mask;

	snaplen = pcap_snapshot(p);
	if (snaplen == 0) {
		snprintf(p->errbuf, PCAP_ERRBUF_SIZE,
			 "snaplen of 0 rejects all packets");
		return -1;
	}

	lex_init(xbuf ? xbuf : "");
	init_linktype(p);
	(void)pcap_parse();

	if (n_errors)
		syntax();

	if (root == NULL)
		root = gen_retblk(snaplen);

	if (optimize && !no_optimize) {
		bpf_optimize(&root);
		if (root == NULL ||
		    (root->s.code == (BPF_RET|BPF_K) && root->s.k == 0))
			bpf_error("expression rejects all packets");
	}
	program->bf_insns = icode_to_fcode(root, &len);
	program->bf_len = len;

	lex_cleanup();
	freechunks();
	return (0);
}

int
pcap_compile_nopcap(int snaplen_arg, int linktype_arg,
		    struct bpf_program *program,
	     const char *buf, int optimize, bpf_u_int32 mask)
{
	pcap_t *p;
	int ret;

	p = pcap_open_dead(linktype_arg, snaplen_arg);
	if (p == NULL)
		return (-1);
	ret = pcap_compile(p, program, buf, optimize, mask);
	pcap_close(p);
	return (ret);
}

void
pcap_freecode(struct bpf_program *program)
{
	program->bf_len = 0;
	if (program->bf_insns != NULL) {
		free((char *)program->bf_insns);
		program->bf_insns = NULL;
	}
}

static void
backpatch(list, target)
	struct block *list, *target;
{
	struct block *next;

	while (list) {
		if (!list->sense) {
			next = JT(list);
			JT(list) = target;
		} else {
			next = JF(list);
			JF(list) = target;
		}
		list = next;
	}
}

static void
merge(b0, b1)
	struct block *b0, *b1;
{
	register struct block **p = &b0;

	
	while (*p)
		p = !((*p)->sense) ? &JT(*p) : &JF(*p);

	
	*p = b1;
}

void
finish_parse(p)
	struct block *p;
{
	struct block *ppi_dlt_check;

	insert_compute_vloffsets(p->head);
	
	ppi_dlt_check = gen_ppi_dlt_check();
	if (ppi_dlt_check != NULL)
		gen_and(ppi_dlt_check, p);

	backpatch(p, gen_retblk(snaplen));
	p->sense = !p->sense;
	backpatch(p, gen_retblk(0));
	root = p->head;
}

void
gen_and(b0, b1)
	struct block *b0, *b1;
{
	backpatch(b0, b1->head);
	b0->sense = !b0->sense;
	b1->sense = !b1->sense;
	merge(b1, b0);
	b1->sense = !b1->sense;
	b1->head = b0->head;
}

void
gen_or(b0, b1)
	struct block *b0, *b1;
{
	b0->sense = !b0->sense;
	backpatch(b0, b1->head);
	b0->sense = !b0->sense;
	merge(b1, b0);
	b1->head = b0->head;
}

void
gen_not(b)
	struct block *b;
{
	b->sense = !b->sense;
}

static struct block *
gen_cmp(offrel, offset, size, v)
	enum e_offrel offrel;
	u_int offset, size;
	bpf_int32 v;
{
	return gen_ncmp(offrel, offset, size, 0xffffffff, BPF_JEQ, 0, v);
}

static struct block *
gen_cmp_gt(offrel, offset, size, v)
	enum e_offrel offrel;
	u_int offset, size;
	bpf_int32 v;
{
	return gen_ncmp(offrel, offset, size, 0xffffffff, BPF_JGT, 0, v);
}

static struct block *
gen_cmp_ge(offrel, offset, size, v)
	enum e_offrel offrel;
	u_int offset, size;
	bpf_int32 v;
{
	return gen_ncmp(offrel, offset, size, 0xffffffff, BPF_JGE, 0, v);
}

static struct block *
gen_cmp_lt(offrel, offset, size, v)
	enum e_offrel offrel;
	u_int offset, size;
	bpf_int32 v;
{
	return gen_ncmp(offrel, offset, size, 0xffffffff, BPF_JGE, 1, v);
}

static struct block *
gen_cmp_le(offrel, offset, size, v)
	enum e_offrel offrel;
	u_int offset, size;
	bpf_int32 v;
{
	return gen_ncmp(offrel, offset, size, 0xffffffff, BPF_JGT, 1, v);
}

static struct block *
gen_mcmp(offrel, offset, size, v, mask)
	enum e_offrel offrel;
	u_int offset, size;
	bpf_int32 v;
	bpf_u_int32 mask;
{
	return gen_ncmp(offrel, offset, size, mask, BPF_JEQ, 0, v);
}

static struct block *
gen_bcmp(offrel, offset, size, v)
	enum e_offrel offrel;
	register u_int offset, size;
	register const u_char *v;
{
	register struct block *b, *tmp;

	b = NULL;
	while (size >= 4) {
		register const u_char *p = &v[size - 4];
		bpf_int32 w = ((bpf_int32)p[0] << 24) |
		    ((bpf_int32)p[1] << 16) | ((bpf_int32)p[2] << 8) | p[3];

		tmp = gen_cmp(offrel, offset + size - 4, BPF_W, w);
		if (b != NULL)
			gen_and(b, tmp);
		b = tmp;
		size -= 4;
	}
	while (size >= 2) {
		register const u_char *p = &v[size - 2];
		bpf_int32 w = ((bpf_int32)p[0] << 8) | p[1];

		tmp = gen_cmp(offrel, offset + size - 2, BPF_H, w);
		if (b != NULL)
			gen_and(b, tmp);
		b = tmp;
		size -= 2;
	}
	if (size > 0) {
		tmp = gen_cmp(offrel, offset, BPF_B, (bpf_int32)v[0]);
		if (b != NULL)
			gen_and(b, tmp);
		b = tmp;
	}
	return b;
}

static struct block *
gen_ncmp(offrel, offset, size, mask, jtype, reverse, v)
	enum e_offrel offrel;
	bpf_int32 v;
	bpf_u_int32 offset, size, mask, jtype;
	int reverse;
{
	struct slist *s, *s2;
	struct block *b;

	s = gen_load_a(offrel, offset, size);

	if (mask != 0xffffffff) {
		s2 = new_stmt(BPF_ALU|BPF_AND|BPF_K);
		s2->s.k = mask;
		sappend(s, s2);
	}

	b = new_block(JMP(jtype));
	b->stmts = s;
	b->s.k = v;
	if (reverse && (jtype == BPF_JGT || jtype == BPF_JGE))
		gen_not(b);
	return b;
}


static u_int off_ll;

static int reg_off_ll;

static u_int off_mac;

static u_int off_macpl;

static int off_macpl_is_variable;

static int reg_off_macpl;

static u_int off_linktype;

static int is_pppoes = 0;

static int is_atm = 0;

static int is_lane = 0;

static u_int off_vpi;
static u_int off_vci;
static u_int off_proto;

static u_int off_li;
static u_int off_li_hsl;

static u_int off_sio;
static u_int off_opc;
static u_int off_dpc;
static u_int off_sls;

static u_int off_payload;

static u_int off_nl;
static u_int off_nl_nosnap;

static int linktype;

static void
init_linktype(p)
	pcap_t *p;
{
	linktype = pcap_datalink(p);
	pcap_fddipad = p->fddipad;

	off_mac = 0;
	is_atm = 0;
	is_lane = 0;
	off_vpi = -1;
	off_vci = -1;
	off_proto = -1;
	off_payload = -1;

	is_pppoes = 0;

	off_li = -1;
	off_li_hsl = -1;
	off_sio = -1;
	off_opc = -1;
	off_dpc = -1;
	off_sls = -1;

	off_ll = 0;
	off_macpl = 0;
	off_macpl_is_variable = 0;

	orig_linktype = -1;
	orig_nl = -1;
        label_stack_depth = 0;

	reg_off_ll = -1;
	reg_off_macpl = -1;

	switch (linktype) {

	case DLT_ARCNET:
		off_linktype = 2;
		off_macpl = 6;
		off_nl = 0;		
		off_nl_nosnap = 0;	
		return;

	case DLT_ARCNET_LINUX:
		off_linktype = 4;
		off_macpl = 8;
		off_nl = 0;		
		off_nl_nosnap = 0;	
		return;

	case DLT_EN10MB:
		off_linktype = 12;
		off_macpl = 14;		
		off_nl = 0;		
		off_nl_nosnap = 3;	
		return;

	case DLT_SLIP:
		off_linktype = -1;
		off_macpl = 16;
		off_nl = 0;
		off_nl_nosnap = 0;	
		return;

	case DLT_SLIP_BSDOS:
		
		off_linktype = -1;
		
		off_macpl = 24;
		off_nl = 0;
		off_nl_nosnap = 0;	
		return;

	case DLT_NULL:
	case DLT_LOOP:
		off_linktype = 0;
		off_macpl = 4;
		off_nl = 0;
		off_nl_nosnap = 0;	
		return;

	case DLT_ENC:
		off_linktype = 0;
		off_macpl = 12;
		off_nl = 0;
		off_nl_nosnap = 0;	
		return;

	case DLT_PPP:
	case DLT_PPP_PPPD:
	case DLT_C_HDLC:		
	case DLT_PPP_SERIAL:		
		off_linktype = 2;
		off_macpl = 4;
		off_nl = 0;
		off_nl_nosnap = 0;	
		return;

	case DLT_PPP_ETHER:
		off_linktype = 6;
		off_macpl = 8;
		off_nl = 0;
		off_nl_nosnap = 0;	
		return;

	case DLT_PPP_BSDOS:
		off_linktype = 5;
		off_macpl = 24;
		off_nl = 0;
		off_nl_nosnap = 0;	
		return;

	case DLT_FDDI:
		off_linktype = 13;
		off_linktype += pcap_fddipad;
		off_macpl = 13;		
		off_macpl += pcap_fddipad;
		off_nl = 8;		
		off_nl_nosnap = 3;	
		return;

	case DLT_IEEE802:
		off_linktype = 14;
		off_macpl = 14;		
		off_nl = 8;		
		off_nl_nosnap = 3;	
		return;

	case DLT_IEEE802_11:
	case DLT_PRISM_HEADER:
	case DLT_IEEE802_11_RADIO_AVS:
	case DLT_IEEE802_11_RADIO:
		off_linktype = 24;
		off_macpl = 0;		
		off_macpl_is_variable = 1;
		off_nl = 8;		
		off_nl_nosnap = 3;	
		return;

	case DLT_PPI:
		off_linktype = 24;
		off_macpl = 0;		
		off_macpl_is_variable = 1;
		off_nl = 8;		
		off_nl_nosnap = 3;	
		return;

	case DLT_ATM_RFC1483:
	case DLT_ATM_CLIP:	
		off_linktype = 0;
		off_macpl = 0;		
		off_nl = 8;		
		off_nl_nosnap = 3;	
		return;

	case DLT_SUNATM:
		is_atm = 1;
		off_vpi = SUNATM_VPI_POS;
		off_vci = SUNATM_VCI_POS;
		off_proto = PROTO_POS;
		off_mac = -1;	
		off_payload = SUNATM_PKT_BEGIN_POS;
		off_linktype = off_payload;
		off_macpl = off_payload;	
		off_nl = 8;		
		off_nl_nosnap = 3;	
		return;

	case DLT_RAW:
	case DLT_IPV4:
	case DLT_IPV6:
		off_linktype = -1;
		off_macpl = 0;
		off_nl = 0;
		off_nl_nosnap = 0;	
		return;

	case DLT_LINUX_SLL:	
		off_linktype = 14;
		off_macpl = 16;
		off_nl = 0;
		off_nl_nosnap = 0;	
		return;

	case DLT_LTALK:
		off_linktype = -1;
		off_macpl = 0;
		off_nl = 0;
		off_nl_nosnap = 0;	
		return;

	case DLT_IP_OVER_FC:
		off_linktype = 16;
		off_macpl = 16;
		off_nl = 8;		
		off_nl_nosnap = 3;	
		return;

	case DLT_FRELAY:
		off_linktype = -1;
		off_macpl = 0;
		off_nl = 0;
		off_nl_nosnap = 0;	
		return;

	case DLT_MFR:
		off_linktype = -1;
		off_macpl = 0;
		off_nl = 4;
		off_nl_nosnap = 0;	
		return;

	case DLT_APPLE_IP_OVER_IEEE1394:
		off_linktype = 16;
		off_macpl = 18;
		off_nl = 0;
		off_nl_nosnap = 0;	
		return;

	case DLT_SYMANTEC_FIREWALL:
		off_linktype = 6;
		off_macpl = 44;
		off_nl = 0;		
		off_nl_nosnap = 0;	
		return;

#ifdef HAVE_NET_PFVAR_H
	case DLT_PFLOG:
		off_linktype = 0;
		off_macpl = PFLOG_HDRLEN;
		off_nl = 0;
		off_nl_nosnap = 0;	
		return;
#endif

        case DLT_JUNIPER_MFR:
        case DLT_JUNIPER_MLFR:
        case DLT_JUNIPER_MLPPP:
        case DLT_JUNIPER_PPP:
        case DLT_JUNIPER_CHDLC:
        case DLT_JUNIPER_FRELAY:
                off_linktype = 4;
		off_macpl = 4;
		off_nl = 0;
		off_nl_nosnap = -1;	
                return;

	case DLT_JUNIPER_ATM1:
		off_linktype = 4;	
		off_macpl = 4;	
		off_nl = 0;
		off_nl_nosnap = 10;
		return;

	case DLT_JUNIPER_ATM2:
		off_linktype = 8;	
		off_macpl = 8;	
		off_nl = 0;
		off_nl_nosnap = 10;
		return;

	case DLT_JUNIPER_PPPOE:
        case DLT_JUNIPER_ETHER:
        	off_macpl = 14;
		off_linktype = 16;
		off_nl = 18;		
		off_nl_nosnap = 21;	
		return;

	case DLT_JUNIPER_PPPOE_ATM:
		off_linktype = 4;
		off_macpl = 6;
		off_nl = 0;
		off_nl_nosnap = -1;	
		return;

	case DLT_JUNIPER_GGSN:
		off_linktype = 6;
		off_macpl = 12;
		off_nl = 0;
		off_nl_nosnap = -1;	
		return;

	case DLT_JUNIPER_ES:
		off_linktype = 6;
		off_macpl = -1;		
		off_nl = -1;		
		off_nl_nosnap = -1;	
		return;

	case DLT_JUNIPER_MONITOR:
		off_linktype = 12;
		off_macpl = 12;
		off_nl = 0;		
		off_nl_nosnap = -1;	
		return;

	case DLT_BACNET_MS_TP:
		off_linktype = -1;
		off_macpl = -1;
		off_nl = -1;
		off_nl_nosnap = -1;
		return;

	case DLT_JUNIPER_SERVICES:
		off_linktype = 12;
		off_macpl = -1;		
		off_nl = -1;		
		off_nl_nosnap = -1;	
		return;

	case DLT_JUNIPER_VP:
		off_linktype = 18;
		off_macpl = -1;
		off_nl = -1;
		off_nl_nosnap = -1;
		return;

	case DLT_JUNIPER_ST:
		off_linktype = 18;
		off_macpl = -1;
		off_nl = -1;
		off_nl_nosnap = -1;
		return;

	case DLT_JUNIPER_ISM:
		off_linktype = 8;
		off_macpl = -1;
		off_nl = -1;
		off_nl_nosnap = -1;
		return;

	case DLT_JUNIPER_VS:
	case DLT_JUNIPER_SRX_E2E:
	case DLT_JUNIPER_FIBRECHANNEL:
	case DLT_JUNIPER_ATM_CEMIC:
		off_linktype = 8;
		off_macpl = -1;
		off_nl = -1;
		off_nl_nosnap = -1;
		return;

	case DLT_MTP2:
		off_li = 2;
		off_li_hsl = 4;
		off_sio = 3;
		off_opc = 4;
		off_dpc = 4;
		off_sls = 7;
		off_linktype = -1;
		off_macpl = -1;
		off_nl = -1;
		off_nl_nosnap = -1;
		return;

	case DLT_MTP2_WITH_PHDR:
		off_li = 6;
		off_li_hsl = 8;
		off_sio = 7;
		off_opc = 8;
		off_dpc = 8;
		off_sls = 11;
		off_linktype = -1;
		off_macpl = -1;
		off_nl = -1;
		off_nl_nosnap = -1;
		return;

	case DLT_ERF:
		off_li = 22;
		off_li_hsl = 24;
		off_sio = 23;
		off_opc = 24;
		off_dpc = 24;
		off_sls = 27;
		off_linktype = -1;
		off_macpl = -1;
		off_nl = -1;
		off_nl_nosnap = -1;
		return;

	case DLT_PFSYNC:
		off_linktype = -1;
		off_macpl = 4;
		off_nl = 0;
		off_nl_nosnap = 0;
		return;

	case DLT_AX25_KISS:
		off_linktype = -1;	
		off_macpl = -1;
		off_nl = -1;		
		off_nl_nosnap = -1;	
		off_mac = 1;		
		return;

	case DLT_IPNET:
		off_linktype = 1;
		off_macpl = 24;		
		off_nl = 0;
		off_nl_nosnap = -1;
		return;

	case DLT_NETANALYZER:
		off_mac = 4;		
		off_linktype = 16;	
		off_macpl = 18;		
		off_nl = 0;		
		off_nl_nosnap = 3;	
		return;

	case DLT_NETANALYZER_TRANSPARENT:
		off_mac = 12;		
		off_linktype = 24;	
		off_macpl = 26;		
		off_nl = 0;		
		off_nl_nosnap = 3;	
		return;

	default:
		if (linktype >= DLT_MATCHING_MIN &&
		    linktype <= DLT_MATCHING_MAX) {
			off_linktype = -1;
			off_macpl = -1;
			off_nl = -1;
			off_nl_nosnap = -1;
			return;
		}

	}
	bpf_error("unknown data link type %d", linktype);
	
}

static struct slist *
gen_load_llrel(offset, size)
	u_int offset, size;
{
	struct slist *s, *s2;

	s = gen_llprefixlen();

	if (s != NULL) {
		s2 = new_stmt(BPF_LD|BPF_IND|size);
		s2->s.k = offset;
		sappend(s, s2);
	} else {
		s = new_stmt(BPF_LD|BPF_ABS|size);
		s->s.k = offset + off_ll;
	}
	return s;
}

static struct slist *
gen_load_macplrel(offset, size)
	u_int offset, size;
{
	struct slist *s, *s2;

	s = gen_off_macpl();

	if (s != NULL) {
		s2 = new_stmt(BPF_LD|BPF_IND|size);
		s2->s.k = offset;
		sappend(s, s2);
	} else {
		s = new_stmt(BPF_LD|BPF_ABS|size);
		s->s.k = off_macpl + offset;
	}
	return s;
}

static struct slist *
gen_load_a(offrel, offset, size)
	enum e_offrel offrel;
	u_int offset, size;
{
	struct slist *s, *s2;

	switch (offrel) {

	case OR_PACKET:
                s = new_stmt(BPF_LD|BPF_ABS|size);
                s->s.k = offset;
		break;

	case OR_LINK:
		s = gen_load_llrel(offset, size);
		break;

	case OR_MACPL:
		s = gen_load_macplrel(offset, size);
		break;

	case OR_NET:
		s = gen_load_macplrel(off_nl + offset, size);
		break;

	case OR_NET_NOSNAP:
		s = gen_load_macplrel(off_nl_nosnap + offset, size);
		break;

	case OR_TRAN_IPV4:
		s = gen_loadx_iphdrlen();

		s2 = new_stmt(BPF_LD|BPF_IND|size);
		s2->s.k = off_macpl + off_nl + offset;
		sappend(s, s2);
		break;

	case OR_TRAN_IPV6:
		s = gen_load_macplrel(off_nl + 40 + offset, size);
		break;

	default:
		abort();
		return NULL;
	}
	return s;
}

static struct slist *
gen_loadx_iphdrlen()
{
	struct slist *s, *s2;

	s = gen_off_macpl();
	if (s != NULL) {
		s2 = new_stmt(BPF_LD|BPF_IND|BPF_B);
		s2->s.k = off_nl;
		sappend(s, s2);
		s2 = new_stmt(BPF_ALU|BPF_AND|BPF_K);
		s2->s.k = 0xf;
		sappend(s, s2);
		s2 = new_stmt(BPF_ALU|BPF_LSH|BPF_K);
		s2->s.k = 2;
		sappend(s, s2);

		sappend(s, new_stmt(BPF_ALU|BPF_ADD|BPF_X));
		sappend(s, new_stmt(BPF_MISC|BPF_TAX));
	} else {
		s = new_stmt(BPF_LDX|BPF_MSH|BPF_B);
		s->s.k = off_macpl + off_nl;
	}
	return s;
}

static struct block *
gen_uncond(rsense)
	int rsense;
{
	struct block *b;
	struct slist *s;

	s = new_stmt(BPF_LD|BPF_IMM);
	s->s.k = !rsense;
	b = new_block(JMP(BPF_JEQ));
	b->stmts = s;

	return b;
}

static inline struct block *
gen_true()
{
	return gen_uncond(1);
}

static inline struct block *
gen_false()
{
	return gen_uncond(0);
}

#define	SWAPLONG(y) \
((((y)&0xff)<<24) | (((y)&0xff00)<<8) | (((y)&0xff0000)>>8) | (((y)>>24)&0xff))

static struct block *
gen_ether_linktype(proto)
	register int proto;
{
	struct block *b0, *b1;

	switch (proto) {

	case LLCSAP_ISONS:
	case LLCSAP_IP:
	case LLCSAP_NETBEUI:
		b0 = gen_cmp_gt(OR_LINK, off_linktype, BPF_H, ETHERMTU);
		gen_not(b0);
		b1 = gen_cmp(OR_MACPL, 0, BPF_H, (bpf_int32)
			     ((proto << 8) | proto));
		gen_and(b0, b1);
		return b1;

	case LLCSAP_IPX:

		b0 = gen_cmp(OR_MACPL, 0, BPF_B, (bpf_int32)LLCSAP_IPX);
		b1 = gen_cmp(OR_MACPL, 0, BPF_H, (bpf_int32)0xFFFF);
		gen_or(b0, b1);

		b0 = gen_snap(0x000000, ETHERTYPE_IPX);
		gen_or(b0, b1);

		b0 = gen_cmp_gt(OR_LINK, off_linktype, BPF_H, ETHERMTU);
		gen_not(b0);

		gen_and(b0, b1);

		b0 = gen_cmp(OR_LINK, off_linktype, BPF_H,
		    (bpf_int32)ETHERTYPE_IPX);
		gen_or(b0, b1);
		return b1;

	case ETHERTYPE_ATALK:
	case ETHERTYPE_AARP:

		b0 = gen_cmp_gt(OR_LINK, off_linktype, BPF_H, ETHERMTU);
		gen_not(b0);

		if (proto == ETHERTYPE_ATALK)
			b1 = gen_snap(0x080007, ETHERTYPE_ATALK);
		else	
			b1 = gen_snap(0x000000, ETHERTYPE_AARP);
		gen_and(b0, b1);

		b0 = gen_cmp(OR_LINK, off_linktype, BPF_H, (bpf_int32)proto);

		gen_or(b0, b1);
		return b1;

	default:
		if (proto <= ETHERMTU) {
			b0 = gen_cmp_gt(OR_LINK, off_linktype, BPF_H, ETHERMTU);
			gen_not(b0);
			b1 = gen_cmp(OR_LINK, off_linktype + 2, BPF_B,
			    (bpf_int32)proto);
			gen_and(b0, b1);
			return b1;
		} else {
			return gen_cmp(OR_LINK, off_linktype, BPF_H,
			    (bpf_int32)proto);
		}
	}
}

static struct block *
gen_ipnet_linktype(proto)
	register int proto;
{
	switch (proto) {

	case ETHERTYPE_IP:
		return gen_cmp(OR_LINK, off_linktype, BPF_B,
		    (bpf_int32)IPH_AF_INET);
		

	case ETHERTYPE_IPV6:
		return gen_cmp(OR_LINK, off_linktype, BPF_B,
		    (bpf_int32)IPH_AF_INET6);
		

	default:
		break;
	}

	return gen_false();
}

static struct block *
gen_linux_sll_linktype(proto)
	register int proto;
{
	struct block *b0, *b1;

	switch (proto) {

	case LLCSAP_ISONS:
	case LLCSAP_IP:
	case LLCSAP_NETBEUI:
		b0 = gen_cmp(OR_LINK, off_linktype, BPF_H, LINUX_SLL_P_802_2);
		b1 = gen_cmp(OR_MACPL, 0, BPF_H, (bpf_int32)
			     ((proto << 8) | proto));
		gen_and(b0, b1);
		return b1;

	case LLCSAP_IPX:
		b0 = gen_cmp(OR_MACPL, 0, BPF_B, (bpf_int32)LLCSAP_IPX);
		b1 = gen_snap(0x000000, ETHERTYPE_IPX);
		gen_or(b0, b1);
		b0 = gen_cmp(OR_LINK, off_linktype, BPF_H, LINUX_SLL_P_802_2);
		gen_and(b0, b1);

		b0 = gen_cmp(OR_LINK, off_linktype, BPF_H, LINUX_SLL_P_802_3);
		gen_or(b0, b1);

		b0 = gen_cmp(OR_LINK, off_linktype, BPF_H,
		    (bpf_int32)ETHERTYPE_IPX);
		gen_or(b0, b1);
		return b1;

	case ETHERTYPE_ATALK:
	case ETHERTYPE_AARP:

		b0 = gen_cmp(OR_LINK, off_linktype, BPF_H, LINUX_SLL_P_802_2);

		if (proto == ETHERTYPE_ATALK)
			b1 = gen_snap(0x080007, ETHERTYPE_ATALK);
		else	
			b1 = gen_snap(0x000000, ETHERTYPE_AARP);
		gen_and(b0, b1);

		b0 = gen_cmp(OR_LINK, off_linktype, BPF_H, (bpf_int32)proto);

		gen_or(b0, b1);
		return b1;

	default:
		if (proto <= ETHERMTU) {
			b0 = gen_cmp(OR_LINK, off_linktype, BPF_H,
			    LINUX_SLL_P_802_2);
			b1 = gen_cmp(OR_LINK, off_macpl, BPF_B,
			     (bpf_int32)proto);
			gen_and(b0, b1);
			return b1;
		} else {
			return gen_cmp(OR_LINK, off_linktype, BPF_H,
			    (bpf_int32)proto);
		}
	}
}

static struct slist *
gen_load_prism_llprefixlen()
{
	struct slist *s1, *s2;
	struct slist *sjeq_avs_cookie;
	struct slist *sjcommon;

	no_optimize = 1;

	if (reg_off_ll != -1) {
		s1 = new_stmt(BPF_LD|BPF_W|BPF_ABS);
		s1->s.k = 0;

		s2 = new_stmt(BPF_ALU|BPF_AND|BPF_K);
		s2->s.k = 0xFFFFF000;
		sappend(s1, s2);

		sjeq_avs_cookie = new_stmt(JMP(BPF_JEQ));
		sjeq_avs_cookie->s.k = 0x80211000;
		sappend(s1, sjeq_avs_cookie);

		s2 = new_stmt(BPF_LD|BPF_W|BPF_ABS);
		s2->s.k = 4;
		sappend(s1, s2);
		sjeq_avs_cookie->s.jt = s2;

		sjcommon = new_stmt(JMP(BPF_JA));
		sjcommon->s.k = 1;
		sappend(s1, sjcommon);

		s2 = new_stmt(BPF_LD|BPF_W|BPF_IMM);
		s2->s.k = 144;
		sappend(s1, s2);
		sjeq_avs_cookie->s.jf = s2;

		s2 = new_stmt(BPF_ST);
		s2->s.k = reg_off_ll;
		sappend(s1, s2);
		sjcommon->s.jf = s2;

		s2 = new_stmt(BPF_MISC|BPF_TAX);
		sappend(s1, s2);

		return (s1);
	} else
		return (NULL);
}

static struct slist *
gen_load_avs_llprefixlen()
{
	struct slist *s1, *s2;

	if (reg_off_ll != -1) {
		s1 = new_stmt(BPF_LD|BPF_W|BPF_ABS);
		s1->s.k = 4;

		s2 = new_stmt(BPF_ST);
		s2->s.k = reg_off_ll;
		sappend(s1, s2);

		s2 = new_stmt(BPF_MISC|BPF_TAX);
		sappend(s1, s2);

		return (s1);
	} else
		return (NULL);
}

static struct slist *
gen_load_radiotap_llprefixlen()
{
	struct slist *s1, *s2;

	if (reg_off_ll != -1) {

		s1 = new_stmt(BPF_LD|BPF_B|BPF_ABS);
		s1->s.k = 3;
		s2 = new_stmt(BPF_ALU|BPF_LSH|BPF_K);
		sappend(s1, s2);
		s2->s.k = 8;
		s2 = new_stmt(BPF_MISC|BPF_TAX);
		sappend(s1, s2);

		s2 = new_stmt(BPF_LD|BPF_B|BPF_ABS);
		sappend(s1, s2);
		s2->s.k = 2;
		s2 = new_stmt(BPF_ALU|BPF_OR|BPF_X);
		sappend(s1, s2);

		s2 = new_stmt(BPF_ST);
		s2->s.k = reg_off_ll;
		sappend(s1, s2);

		s2 = new_stmt(BPF_MISC|BPF_TAX);
		sappend(s1, s2);

		return (s1);
	} else
		return (NULL);
}

static struct slist *
gen_load_ppi_llprefixlen()
{
	struct slist *s1, *s2;
	
	if (reg_off_ll != -1) {

		s1 = new_stmt(BPF_LD|BPF_B|BPF_ABS);
		s1->s.k = 3;
		s2 = new_stmt(BPF_ALU|BPF_LSH|BPF_K);
		sappend(s1, s2);
		s2->s.k = 8;
		s2 = new_stmt(BPF_MISC|BPF_TAX);
		sappend(s1, s2);

		s2 = new_stmt(BPF_LD|BPF_B|BPF_ABS);
		sappend(s1, s2);
		s2->s.k = 2;
		s2 = new_stmt(BPF_ALU|BPF_OR|BPF_X);
		sappend(s1, s2);

		s2 = new_stmt(BPF_ST);
		s2->s.k = reg_off_ll;
		sappend(s1, s2);

		s2 = new_stmt(BPF_MISC|BPF_TAX);
		sappend(s1, s2);

		return (s1);
	} else
		return (NULL);
}

static struct slist *
gen_load_802_11_header_len(struct slist *s, struct slist *snext)
{
	struct slist *s2;
	struct slist *sjset_data_frame_1;
	struct slist *sjset_data_frame_2;
	struct slist *sjset_qos;
	struct slist *sjset_radiotap_flags;
	struct slist *sjset_radiotap_tsft;
	struct slist *sjset_tsft_datapad, *sjset_notsft_datapad;
	struct slist *s_roundup;

	if (reg_off_macpl == -1) {
		return (s);
	}

	no_optimize = 1;
	
	if (s == NULL) {
		s = new_stmt(BPF_LDX|BPF_IMM);
		s->s.k = off_ll;
	}

	s2 = new_stmt(BPF_MISC|BPF_TXA);
	sappend(s, s2);
	s2 = new_stmt(BPF_ALU|BPF_ADD|BPF_K);
	s2->s.k = 24;
	sappend(s, s2);
	s2 = new_stmt(BPF_ST);
	s2->s.k = reg_off_macpl;
	sappend(s, s2);

	s2 = new_stmt(BPF_LD|BPF_IND|BPF_B);
	s2->s.k = 0;
	sappend(s, s2);

	sjset_data_frame_1 = new_stmt(JMP(BPF_JSET));
	sjset_data_frame_1->s.k = 0x08;
	sappend(s, sjset_data_frame_1);
		
	sjset_data_frame_1->s.jt = sjset_data_frame_2 = new_stmt(JMP(BPF_JSET));
	sjset_data_frame_2->s.k = 0x04;
	sappend(s, sjset_data_frame_2);
	sjset_data_frame_1->s.jf = snext;

	sjset_data_frame_2->s.jt = snext;
	sjset_data_frame_2->s.jf = sjset_qos = new_stmt(JMP(BPF_JSET));
	sjset_qos->s.k = 0x80;	
	sappend(s, sjset_qos);
		
	sjset_qos->s.jt = s2 = new_stmt(BPF_LD|BPF_MEM);
	s2->s.k = reg_off_macpl;
	sappend(s, s2);
	s2 = new_stmt(BPF_ALU|BPF_ADD|BPF_IMM);
	s2->s.k = 2;
	sappend(s, s2);
	s2 = new_stmt(BPF_ST);
	s2->s.k = reg_off_macpl;
	sappend(s, s2);

	if (linktype == DLT_IEEE802_11_RADIO) {
		sjset_qos->s.jf = s2 = new_stmt(BPF_LD|BPF_ABS|BPF_W);
		s2->s.k = 4;
		sappend(s, s2);

		sjset_radiotap_flags = new_stmt(JMP(BPF_JSET));
		sjset_radiotap_flags->s.k = SWAPLONG(0x00000002);
		sappend(s, sjset_radiotap_flags);

		sjset_radiotap_flags->s.jf = snext;

		sjset_radiotap_tsft = sjset_radiotap_flags->s.jt =
		    new_stmt(JMP(BPF_JSET));
		sjset_radiotap_tsft->s.k = SWAPLONG(0x00000001);
		sappend(s, sjset_radiotap_tsft);

		sjset_radiotap_tsft->s.jt = s2 = new_stmt(BPF_LD|BPF_ABS|BPF_B);
		s2->s.k = 16;
		sappend(s, s2);

		sjset_tsft_datapad = new_stmt(JMP(BPF_JSET));
		sjset_tsft_datapad->s.k = 0x20;
		sappend(s, sjset_tsft_datapad);

		sjset_radiotap_tsft->s.jf = s2 = new_stmt(BPF_LD|BPF_ABS|BPF_B);
		s2->s.k = 8;
		sappend(s, s2);

		sjset_notsft_datapad = new_stmt(JMP(BPF_JSET));
		sjset_notsft_datapad->s.k = 0x20;
		sappend(s, sjset_notsft_datapad);

		s_roundup = new_stmt(BPF_LD|BPF_MEM);
		s_roundup->s.k = reg_off_macpl;
		sappend(s, s_roundup);
		s2 = new_stmt(BPF_ALU|BPF_ADD|BPF_IMM);
		s2->s.k = 3;
		sappend(s, s2);
		s2 = new_stmt(BPF_ALU|BPF_AND|BPF_IMM);
		s2->s.k = ~3;
		sappend(s, s2);
		s2 = new_stmt(BPF_ST);
		s2->s.k = reg_off_macpl;
		sappend(s, s2);

		sjset_tsft_datapad->s.jt = s_roundup;
		sjset_tsft_datapad->s.jf = snext;
		sjset_notsft_datapad->s.jt = s_roundup;
		sjset_notsft_datapad->s.jf = snext;
	} else
		sjset_qos->s.jf = snext;

	return s;
}

static void
insert_compute_vloffsets(b)
	struct block *b;
{
	struct slist *s;

	switch (linktype) {

	case DLT_PRISM_HEADER:
		s = gen_load_prism_llprefixlen();
		break;

	case DLT_IEEE802_11_RADIO_AVS:
		s = gen_load_avs_llprefixlen();
		break;

	case DLT_IEEE802_11_RADIO:
		s = gen_load_radiotap_llprefixlen();
		break;

	case DLT_PPI:
		s = gen_load_ppi_llprefixlen();
		break;

	default:
		s = NULL;
		break;
	}

	switch (linktype) {

	case DLT_IEEE802_11:
	case DLT_PRISM_HEADER:
	case DLT_IEEE802_11_RADIO_AVS:
	case DLT_IEEE802_11_RADIO:
	case DLT_PPI:
		s = gen_load_802_11_header_len(s, b->stmts);
		break;
	}

	if (s != NULL) {
		sappend(s, b->stmts);
		b->stmts = s;
	}
}

static struct block *
gen_ppi_dlt_check(void)
{
	struct slist *s_load_dlt;
	struct block *b;

	if (linktype == DLT_PPI)
	{
		s_load_dlt = new_stmt(BPF_LD|BPF_W|BPF_ABS);
		s_load_dlt->s.k = 4;

		b = new_block(JMP(BPF_JEQ));

		b->stmts = s_load_dlt;
		b->s.k = SWAPLONG(DLT_IEEE802_11);
	}
	else
	{
		b = NULL;
	}

	return b;
}

static struct slist *
gen_prism_llprefixlen(void)
{
	struct slist *s;

	if (reg_off_ll == -1) {
		reg_off_ll = alloc_reg();
	}

	s = new_stmt(BPF_LDX|BPF_MEM);
	s->s.k = reg_off_ll;
	return s;
}

static struct slist *
gen_avs_llprefixlen(void)
{
	struct slist *s;

	if (reg_off_ll == -1) {
		reg_off_ll = alloc_reg();
	}

	s = new_stmt(BPF_LDX|BPF_MEM);
	s->s.k = reg_off_ll;
	return s;
}

static struct slist *
gen_radiotap_llprefixlen(void)
{
	struct slist *s;

	if (reg_off_ll == -1) {
		reg_off_ll = alloc_reg();
	}

	s = new_stmt(BPF_LDX|BPF_MEM);
	s->s.k = reg_off_ll;
	return s;
}

static struct slist *
gen_ppi_llprefixlen(void)
{
	struct slist *s;

	if (reg_off_ll == -1) {
		reg_off_ll = alloc_reg();
	}

	s = new_stmt(BPF_LDX|BPF_MEM);
	s->s.k = reg_off_ll;
	return s;
}

static struct slist *
gen_llprefixlen(void)
{
	switch (linktype) {

	case DLT_PRISM_HEADER:
		return gen_prism_llprefixlen();

	case DLT_IEEE802_11_RADIO_AVS:
		return gen_avs_llprefixlen();

	case DLT_IEEE802_11_RADIO:
		return gen_radiotap_llprefixlen();

	case DLT_PPI:
		return gen_ppi_llprefixlen();

	default:
		return NULL;
	}
}

static struct slist *
gen_off_macpl(void)
{
	struct slist *s;

	if (off_macpl_is_variable) {
		if (reg_off_macpl == -1) {
			reg_off_macpl = alloc_reg();
		}

		s = new_stmt(BPF_LDX|BPF_MEM);
		s->s.k = reg_off_macpl;
		return s;
	} else {
		return NULL;
	}
}

static int
ethertype_to_ppptype(proto)
	int proto;
{
	switch (proto) {

	case ETHERTYPE_IP:
		proto = PPP_IP;
		break;

	case ETHERTYPE_IPV6:
		proto = PPP_IPV6;
		break;

	case ETHERTYPE_DN:
		proto = PPP_DECNET;
		break;

	case ETHERTYPE_ATALK:
		proto = PPP_APPLE;
		break;

	case ETHERTYPE_NS:
		proto = PPP_NS;
		break;

	case LLCSAP_ISONS:
		proto = PPP_OSI;
		break;

	case LLCSAP_8021D:
		proto = PPP_BRPDU;
		break;

	case LLCSAP_IPX:
		proto = PPP_IPX;
		break;
	}
	return (proto);
}

static struct block *
gen_linktype(proto)
	register int proto;
{
	struct block *b0, *b1, *b2;

	
	if (label_stack_depth > 0) {
		switch (proto) {
		case ETHERTYPE_IP:
		case PPP_IP:
			
			return gen_mpls_linktype(Q_IP); 

		case ETHERTYPE_IPV6:
		case PPP_IPV6:
			
			return gen_mpls_linktype(Q_IPV6); 

		default:
			bpf_error("unsupported protocol over mpls");
			
		}
	}

	if (is_pppoes) {

		proto = ethertype_to_ppptype(proto);
		return gen_cmp(OR_MACPL, off_linktype, BPF_H, (bpf_int32)proto);
	}

	switch (linktype) {

	case DLT_EN10MB:
	case DLT_NETANALYZER:
	case DLT_NETANALYZER_TRANSPARENT:
		return gen_ether_linktype(proto);
		
		break;

	case DLT_C_HDLC:
		switch (proto) {

		case LLCSAP_ISONS:
			proto = (proto << 8 | LLCSAP_ISONS);
			

		default:
			return gen_cmp(OR_LINK, off_linktype, BPF_H,
			    (bpf_int32)proto);
			
			break;
		}
		break;

	case DLT_IEEE802_11:
	case DLT_PRISM_HEADER:
	case DLT_IEEE802_11_RADIO_AVS:
	case DLT_IEEE802_11_RADIO:
	case DLT_PPI:
		b0 = gen_check_802_11_data_frame();

		b1 = gen_llc_linktype(proto);
		gen_and(b0, b1);
		return b1;
		
		break;

	case DLT_FDDI:
		return gen_llc_linktype(proto);
		
		break;

	case DLT_IEEE802:
		return gen_llc_linktype(proto);
		
		break;

	case DLT_ATM_RFC1483:
	case DLT_ATM_CLIP:
	case DLT_IP_OVER_FC:
		return gen_llc_linktype(proto);
		
		break;

	case DLT_SUNATM:
		if (is_lane) {
			b0 = gen_cmp(OR_LINK, SUNATM_PKT_BEGIN_POS, BPF_H,
			    0xFF00);
			gen_not(b0);

			b1 = gen_ether_linktype(proto);
			gen_and(b0, b1);
			return b1;
		} else {
			b0 = gen_atmfield_code(A_PROTOTYPE, PT_LLC, BPF_JEQ, 0);
			b1 = gen_llc_linktype(proto);
			gen_and(b0, b1);
			return b1;
		}
		
		break;

	case DLT_LINUX_SLL:
		return gen_linux_sll_linktype(proto);
		
		break;

	case DLT_SLIP:
	case DLT_SLIP_BSDOS:
	case DLT_RAW:
		switch (proto) {

		case ETHERTYPE_IP:
			
			return gen_mcmp(OR_LINK, 0, BPF_B, 0x40, 0xF0);

		case ETHERTYPE_IPV6:
			
			return gen_mcmp(OR_LINK, 0, BPF_B, 0x60, 0xF0);

		default:
			return gen_false();		
		}
		
		break;

	case DLT_IPV4:
		if (proto == ETHERTYPE_IP)
			return gen_true();		

		
		return gen_false();
		
		break;

	case DLT_IPV6:
		if (proto == ETHERTYPE_IPV6)
			return gen_true();		

		
		return gen_false();
		
		break;

	case DLT_PPP:
	case DLT_PPP_PPPD:
	case DLT_PPP_SERIAL:
	case DLT_PPP_ETHER:
		proto = ethertype_to_ppptype(proto);
		return gen_cmp(OR_LINK, off_linktype, BPF_H, (bpf_int32)proto);
		
		break;

	case DLT_PPP_BSDOS:
		switch (proto) {

		case ETHERTYPE_IP:
			b0 = gen_cmp(OR_LINK, off_linktype, BPF_H, PPP_IP);
			b1 = gen_cmp(OR_LINK, off_linktype, BPF_H, PPP_VJC);
			gen_or(b0, b1);
			b0 = gen_cmp(OR_LINK, off_linktype, BPF_H, PPP_VJNC);
			gen_or(b1, b0);
			return b0;

		default:
			proto = ethertype_to_ppptype(proto);
			return gen_cmp(OR_LINK, off_linktype, BPF_H,
				(bpf_int32)proto);
		}
		
		break;

	case DLT_NULL:
	case DLT_LOOP:
	case DLT_ENC:
		switch (proto) {

		case ETHERTYPE_IP:
			proto = AF_INET;
			break;

#ifdef INET6
		case ETHERTYPE_IPV6:
			proto = AF_INET6;
			break;
#endif

		default:
			return gen_false();
		}

		if (linktype == DLT_NULL || linktype == DLT_ENC) {
			if (bpf_pcap->rfile != NULL && bpf_pcap->swapped)
				proto = SWAPLONG(proto);
			proto = htonl(proto);
		}
		return (gen_cmp(OR_LINK, 0, BPF_W, (bpf_int32)proto));

#ifdef HAVE_NET_PFVAR_H
	case DLT_PFLOG:
		if (proto == ETHERTYPE_IP)
			return (gen_cmp(OR_LINK, offsetof(struct pfloghdr, af),
			    BPF_B, (bpf_int32)AF_INET));
		else if (proto == ETHERTYPE_IPV6)
			return (gen_cmp(OR_LINK, offsetof(struct pfloghdr, af),
			    BPF_B, (bpf_int32)AF_INET6));
		else
			return gen_false();
		
		break;
#endif 

	case DLT_ARCNET:
	case DLT_ARCNET_LINUX:
		switch (proto) {

		default:
			return gen_false();

		case ETHERTYPE_IPV6:
			return (gen_cmp(OR_LINK, off_linktype, BPF_B,
				(bpf_int32)ARCTYPE_INET6));

		case ETHERTYPE_IP:
			b0 = gen_cmp(OR_LINK, off_linktype, BPF_B,
				     (bpf_int32)ARCTYPE_IP);
			b1 = gen_cmp(OR_LINK, off_linktype, BPF_B,
				     (bpf_int32)ARCTYPE_IP_OLD);
			gen_or(b0, b1);
			return (b1);

		case ETHERTYPE_ARP:
			b0 = gen_cmp(OR_LINK, off_linktype, BPF_B,
				     (bpf_int32)ARCTYPE_ARP);
			b1 = gen_cmp(OR_LINK, off_linktype, BPF_B,
				     (bpf_int32)ARCTYPE_ARP_OLD);
			gen_or(b0, b1);
			return (b1);

		case ETHERTYPE_REVARP:
			return (gen_cmp(OR_LINK, off_linktype, BPF_B,
					(bpf_int32)ARCTYPE_REVARP));

		case ETHERTYPE_ATALK:
			return (gen_cmp(OR_LINK, off_linktype, BPF_B,
					(bpf_int32)ARCTYPE_ATALK));
		}
		
		break;

	case DLT_LTALK:
		switch (proto) {
		case ETHERTYPE_ATALK:
			return gen_true();
		default:
			return gen_false();
		}
		
		break;

	case DLT_FRELAY:
		switch (proto) {

		case ETHERTYPE_IP:
			return gen_cmp(OR_LINK, 2, BPF_H, (0x03<<8) | 0xcc);

		case ETHERTYPE_IPV6:
			return gen_cmp(OR_LINK, 2, BPF_H, (0x03<<8) | 0x8e);

		case LLCSAP_ISONS:
			b0 = gen_cmp(OR_LINK, 2, BPF_H, (0x03<<8) | ISO8473_CLNP);
			b1 = gen_cmp(OR_LINK, 2, BPF_H, (0x03<<8) | ISO9542_ESIS);
			b2 = gen_cmp(OR_LINK, 2, BPF_H, (0x03<<8) | ISO10589_ISIS);
			gen_or(b1, b2);
			gen_or(b0, b2);
			return b2;

		default:
			return gen_false();
		}
		
		break;

	case DLT_MFR:
		bpf_error("Multi-link Frame Relay link-layer type filtering not implemented");

        case DLT_JUNIPER_MFR:
        case DLT_JUNIPER_MLFR:
        case DLT_JUNIPER_MLPPP:
	case DLT_JUNIPER_ATM1:
	case DLT_JUNIPER_ATM2:
	case DLT_JUNIPER_PPPOE:
	case DLT_JUNIPER_PPPOE_ATM:
        case DLT_JUNIPER_GGSN:
        case DLT_JUNIPER_ES:
        case DLT_JUNIPER_MONITOR:
        case DLT_JUNIPER_SERVICES:
        case DLT_JUNIPER_ETHER:
        case DLT_JUNIPER_PPP:
        case DLT_JUNIPER_FRELAY:
        case DLT_JUNIPER_CHDLC:
        case DLT_JUNIPER_VP:
        case DLT_JUNIPER_ST:
        case DLT_JUNIPER_ISM:
        case DLT_JUNIPER_VS:
        case DLT_JUNIPER_SRX_E2E:
        case DLT_JUNIPER_FIBRECHANNEL:
	case DLT_JUNIPER_ATM_CEMIC:

		return gen_mcmp(OR_LINK, 0, BPF_W, 0x4d474300, 0xffffff00); 

	case DLT_BACNET_MS_TP:
		return gen_mcmp(OR_LINK, 0, BPF_W, 0x55FF0000, 0xffff0000);

	case DLT_IPNET:
		return gen_ipnet_linktype(proto);

	case DLT_LINUX_IRDA:
		bpf_error("IrDA link-layer type filtering not implemented");

	case DLT_DOCSIS:
		bpf_error("DOCSIS link-layer type filtering not implemented");

	case DLT_MTP2:
	case DLT_MTP2_WITH_PHDR:
		bpf_error("MTP2 link-layer type filtering not implemented");

	case DLT_ERF:
		bpf_error("ERF link-layer type filtering not implemented");

	case DLT_PFSYNC:
		bpf_error("PFSYNC link-layer type filtering not implemented");

	case DLT_LINUX_LAPD:
		bpf_error("LAPD link-layer type filtering not implemented");

	case DLT_USB:
	case DLT_USB_LINUX:
	case DLT_USB_LINUX_MMAPPED:
		bpf_error("USB link-layer type filtering not implemented");

	case DLT_BLUETOOTH_HCI_H4:
	case DLT_BLUETOOTH_HCI_H4_WITH_PHDR:
		bpf_error("Bluetooth link-layer type filtering not implemented");

	case DLT_CAN20B:
	case DLT_CAN_SOCKETCAN:
		bpf_error("CAN link-layer type filtering not implemented");

	case DLT_IEEE802_15_4:
	case DLT_IEEE802_15_4_LINUX:
	case DLT_IEEE802_15_4_NONASK_PHY:
	case DLT_IEEE802_15_4_NOFCS:
		bpf_error("IEEE 802.15.4 link-layer type filtering not implemented");

	case DLT_IEEE802_16_MAC_CPS_RADIO:
		bpf_error("IEEE 802.16 link-layer type filtering not implemented");

	case DLT_SITA:
		bpf_error("SITA link-layer type filtering not implemented");

	case DLT_RAIF1:
		bpf_error("RAIF1 link-layer type filtering not implemented");

	case DLT_IPMB:
		bpf_error("IPMB link-layer type filtering not implemented");

	case DLT_AX25_KISS:
		bpf_error("AX.25 link-layer type filtering not implemented");
	}

	if (off_linktype == (u_int)-1)
		abort();

	return gen_cmp(OR_LINK, off_linktype, BPF_H, (bpf_int32)proto);
}

static struct block *
gen_snap(orgcode, ptype)
	bpf_u_int32 orgcode;
	bpf_u_int32 ptype;
{
	u_char snapblock[8];

	snapblock[0] = LLCSAP_SNAP;	
	snapblock[1] = LLCSAP_SNAP;	
	snapblock[2] = 0x03;		
	snapblock[3] = (orgcode >> 16);	
	snapblock[4] = (orgcode >> 8);	
	snapblock[5] = (orgcode >> 0);	
	snapblock[6] = (ptype >> 8);	
	snapblock[7] = (ptype >> 0);	
	return gen_bcmp(OR_MACPL, 0, 8, snapblock);
}

static struct block *
gen_llc_linktype(proto)
	int proto;
{
	switch (proto) {

	case LLCSAP_IP:
	case LLCSAP_ISONS:
	case LLCSAP_NETBEUI:
		return gen_cmp(OR_MACPL, 0, BPF_H, (bpf_u_int32)
			     ((proto << 8) | proto));

	case LLCSAP_IPX:
		return gen_cmp(OR_MACPL, 0, BPF_B,
		    (bpf_int32)LLCSAP_IPX);

	case ETHERTYPE_ATALK:
		return gen_snap(0x080007, ETHERTYPE_ATALK);

	default:
		if (proto <= ETHERMTU) {
			return gen_cmp(OR_MACPL, 0, BPF_B, (bpf_int32)proto);
		} else {
			return gen_cmp(OR_MACPL, 6, BPF_H, (bpf_int32)proto);
		}
	}
}

static struct block *
gen_hostop(addr, mask, dir, proto, src_off, dst_off)
	bpf_u_int32 addr;
	bpf_u_int32 mask;
	int dir, proto;
	u_int src_off, dst_off;
{
	struct block *b0, *b1;
	u_int offset;

	switch (dir) {

	case Q_SRC:
		offset = src_off;
		break;

	case Q_DST:
		offset = dst_off;
		break;

	case Q_AND:
		b0 = gen_hostop(addr, mask, Q_SRC, proto, src_off, dst_off);
		b1 = gen_hostop(addr, mask, Q_DST, proto, src_off, dst_off);
		gen_and(b0, b1);
		return b1;

	case Q_OR:
	case Q_DEFAULT:
		b0 = gen_hostop(addr, mask, Q_SRC, proto, src_off, dst_off);
		b1 = gen_hostop(addr, mask, Q_DST, proto, src_off, dst_off);
		gen_or(b0, b1);
		return b1;

	default:
		abort();
	}
	b0 = gen_linktype(proto);
	b1 = gen_mcmp(OR_NET, offset, BPF_W, (bpf_int32)addr, mask);
	gen_and(b0, b1);
	return b1;
}

#ifdef INET6
static struct block *
gen_hostop6(addr, mask, dir, proto, src_off, dst_off)
	struct in6_addr *addr;
	struct in6_addr *mask;
	int dir, proto;
	u_int src_off, dst_off;
{
	struct block *b0, *b1;
	u_int offset;
	u_int32_t *a, *m;

	switch (dir) {

	case Q_SRC:
		offset = src_off;
		break;

	case Q_DST:
		offset = dst_off;
		break;

	case Q_AND:
		b0 = gen_hostop6(addr, mask, Q_SRC, proto, src_off, dst_off);
		b1 = gen_hostop6(addr, mask, Q_DST, proto, src_off, dst_off);
		gen_and(b0, b1);
		return b1;

	case Q_OR:
	case Q_DEFAULT:
		b0 = gen_hostop6(addr, mask, Q_SRC, proto, src_off, dst_off);
		b1 = gen_hostop6(addr, mask, Q_DST, proto, src_off, dst_off);
		gen_or(b0, b1);
		return b1;

	default:
		abort();
	}
	
	a = (u_int32_t *)addr;
	m = (u_int32_t *)mask;
	b1 = gen_mcmp(OR_NET, offset + 12, BPF_W, ntohl(a[3]), ntohl(m[3]));
	b0 = gen_mcmp(OR_NET, offset + 8, BPF_W, ntohl(a[2]), ntohl(m[2]));
	gen_and(b0, b1);
	b0 = gen_mcmp(OR_NET, offset + 4, BPF_W, ntohl(a[1]), ntohl(m[1]));
	gen_and(b0, b1);
	b0 = gen_mcmp(OR_NET, offset + 0, BPF_W, ntohl(a[0]), ntohl(m[0]));
	gen_and(b0, b1);
	b0 = gen_linktype(proto);
	gen_and(b0, b1);
	return b1;
}
#endif

static struct block *
gen_ehostop(eaddr, dir)
	register const u_char *eaddr;
	register int dir;
{
	register struct block *b0, *b1;

	switch (dir) {
	case Q_SRC:
		return gen_bcmp(OR_LINK, off_mac + 6, 6, eaddr);

	case Q_DST:
		return gen_bcmp(OR_LINK, off_mac + 0, 6, eaddr);

	case Q_AND:
		b0 = gen_ehostop(eaddr, Q_SRC);
		b1 = gen_ehostop(eaddr, Q_DST);
		gen_and(b0, b1);
		return b1;

	case Q_DEFAULT:
	case Q_OR:
		b0 = gen_ehostop(eaddr, Q_SRC);
		b1 = gen_ehostop(eaddr, Q_DST);
		gen_or(b0, b1);
		return b1;

	case Q_ADDR1:
		bpf_error("'addr1' is only supported on 802.11 with 802.11 headers");
		break;

	case Q_ADDR2:
		bpf_error("'addr2' is only supported on 802.11 with 802.11 headers");
		break;

	case Q_ADDR3:
		bpf_error("'addr3' is only supported on 802.11 with 802.11 headers");
		break;

	case Q_ADDR4:
		bpf_error("'addr4' is only supported on 802.11 with 802.11 headers");
		break;

	case Q_RA:
		bpf_error("'ra' is only supported on 802.11 with 802.11 headers");
		break;

	case Q_TA:
		bpf_error("'ta' is only supported on 802.11 with 802.11 headers");
		break;
	}
	abort();
	
}

static struct block *
gen_fhostop(eaddr, dir)
	register const u_char *eaddr;
	register int dir;
{
	struct block *b0, *b1;

	switch (dir) {
	case Q_SRC:
		return gen_bcmp(OR_LINK, 6 + 1 + pcap_fddipad, 6, eaddr);

	case Q_DST:
		return gen_bcmp(OR_LINK, 0 + 1 + pcap_fddipad, 6, eaddr);

	case Q_AND:
		b0 = gen_fhostop(eaddr, Q_SRC);
		b1 = gen_fhostop(eaddr, Q_DST);
		gen_and(b0, b1);
		return b1;

	case Q_DEFAULT:
	case Q_OR:
		b0 = gen_fhostop(eaddr, Q_SRC);
		b1 = gen_fhostop(eaddr, Q_DST);
		gen_or(b0, b1);
		return b1;

	case Q_ADDR1:
		bpf_error("'addr1' is only supported on 802.11");
		break;

	case Q_ADDR2:
		bpf_error("'addr2' is only supported on 802.11");
		break;

	case Q_ADDR3:
		bpf_error("'addr3' is only supported on 802.11");
		break;

	case Q_ADDR4:
		bpf_error("'addr4' is only supported on 802.11");
		break;

	case Q_RA:
		bpf_error("'ra' is only supported on 802.11");
		break;

	case Q_TA:
		bpf_error("'ta' is only supported on 802.11");
		break;
	}
	abort();
	
}

static struct block *
gen_thostop(eaddr, dir)
	register const u_char *eaddr;
	register int dir;
{
	register struct block *b0, *b1;

	switch (dir) {
	case Q_SRC:
		return gen_bcmp(OR_LINK, 8, 6, eaddr);

	case Q_DST:
		return gen_bcmp(OR_LINK, 2, 6, eaddr);

	case Q_AND:
		b0 = gen_thostop(eaddr, Q_SRC);
		b1 = gen_thostop(eaddr, Q_DST);
		gen_and(b0, b1);
		return b1;

	case Q_DEFAULT:
	case Q_OR:
		b0 = gen_thostop(eaddr, Q_SRC);
		b1 = gen_thostop(eaddr, Q_DST);
		gen_or(b0, b1);
		return b1;

	case Q_ADDR1:
		bpf_error("'addr1' is only supported on 802.11");
		break;

	case Q_ADDR2:
		bpf_error("'addr2' is only supported on 802.11");
		break;

	case Q_ADDR3:
		bpf_error("'addr3' is only supported on 802.11");
		break;

	case Q_ADDR4:
		bpf_error("'addr4' is only supported on 802.11");
		break;

	case Q_RA:
		bpf_error("'ra' is only supported on 802.11");
		break;

	case Q_TA:
		bpf_error("'ta' is only supported on 802.11");
		break;
	}
	abort();
	
}

static struct block *
gen_wlanhostop(eaddr, dir)
	register const u_char *eaddr;
	register int dir;
{
	register struct block *b0, *b1, *b2;
	register struct slist *s;

#ifdef ENABLE_WLAN_FILTERING_PATCH
	no_optimize = 1;
#endif 

	switch (dir) {
	case Q_SRC:

		s = gen_load_a(OR_LINK, 1, BPF_B);
		b1 = new_block(JMP(BPF_JSET));
		b1->s.k = 0x01;	
		b1->stmts = s;

		b0 = gen_bcmp(OR_LINK, 24, 6, eaddr);
		gen_and(b1, b0);

		s = gen_load_a(OR_LINK, 1, BPF_B);
		b2 = new_block(JMP(BPF_JSET));
		b2->s.k = 0x01;	
		b2->stmts = s;
		gen_not(b2);

		b1 = gen_bcmp(OR_LINK, 16, 6, eaddr);
		gen_and(b2, b1);

		gen_or(b1, b0);

		s = gen_load_a(OR_LINK, 1, BPF_B);
		b1 = new_block(JMP(BPF_JSET));
		b1->s.k = 0x02;	
		b1->stmts = s;
		gen_and(b1, b0);

		s = gen_load_a(OR_LINK, 1, BPF_B);
		b2 = new_block(JMP(BPF_JSET));
		b2->s.k = 0x02;	
		b2->stmts = s;
		gen_not(b2);

		b1 = gen_bcmp(OR_LINK, 10, 6, eaddr);
		gen_and(b2, b1);

		gen_or(b1, b0);

		s = gen_load_a(OR_LINK, 0, BPF_B);
		b1 = new_block(JMP(BPF_JSET));
		b1->s.k = 0x08;
		b1->stmts = s;

		gen_and(b1, b0);

		s = gen_load_a(OR_LINK, 0, BPF_B);
		b2 = new_block(JMP(BPF_JSET));
		b2->s.k = 0x08;
		b2->stmts = s;
		gen_not(b2);

		b1 = gen_bcmp(OR_LINK, 10, 6, eaddr);
		gen_and(b2, b1);

		gen_or(b1, b0);

		s = gen_load_a(OR_LINK, 0, BPF_B);
		b1 = new_block(JMP(BPF_JSET));
		b1->s.k = 0x04;
		b1->stmts = s;
		gen_not(b1);

		gen_and(b1, b0);
		return b0;

	case Q_DST:

		s = gen_load_a(OR_LINK, 1, BPF_B);
		b1 = new_block(JMP(BPF_JSET));
		b1->s.k = 0x01;	
		b1->stmts = s;

		b0 = gen_bcmp(OR_LINK, 16, 6, eaddr);
		gen_and(b1, b0);

		s = gen_load_a(OR_LINK, 1, BPF_B);
		b2 = new_block(JMP(BPF_JSET));
		b2->s.k = 0x01;	
		b2->stmts = s;
		gen_not(b2);

		b1 = gen_bcmp(OR_LINK, 4, 6, eaddr);
		gen_and(b2, b1);

		gen_or(b1, b0);

		s = gen_load_a(OR_LINK, 0, BPF_B);
		b1 = new_block(JMP(BPF_JSET));
		b1->s.k = 0x08;
		b1->stmts = s;

		gen_and(b1, b0);

		s = gen_load_a(OR_LINK, 0, BPF_B);
		b2 = new_block(JMP(BPF_JSET));
		b2->s.k = 0x08;
		b2->stmts = s;
		gen_not(b2);

		b1 = gen_bcmp(OR_LINK, 4, 6, eaddr);
		gen_and(b2, b1);

		gen_or(b1, b0);

		s = gen_load_a(OR_LINK, 0, BPF_B);
		b1 = new_block(JMP(BPF_JSET));
		b1->s.k = 0x04;
		b1->stmts = s;
		gen_not(b1);

		gen_and(b1, b0);
		return b0;

	case Q_RA:

		s = gen_load_a(OR_LINK, 0, BPF_B);
		b1 = new_block(JMP(BPF_JSET));
		b1->s.k = 0x08;
		b1->stmts = s;

		b0 = gen_bcmp(OR_LINK, 4, 6, eaddr);

		gen_and(b1, b0);
		return (b0);

	case Q_TA:

		b0 = gen_mcmp(OR_LINK, 0, BPF_B, IEEE80211_FC0_TYPE_CTL,
			IEEE80211_FC0_TYPE_MASK);
		gen_not(b0);
		b1 = gen_mcmp(OR_LINK, 0, BPF_B, IEEE80211_FC0_SUBTYPE_CTS,
			IEEE80211_FC0_SUBTYPE_MASK);
		gen_not(b1);
		b2 = gen_mcmp(OR_LINK, 0, BPF_B, IEEE80211_FC0_SUBTYPE_ACK,
			IEEE80211_FC0_SUBTYPE_MASK);
		gen_not(b2);
		gen_and(b1, b2);
		gen_or(b0, b2);

		s = gen_load_a(OR_LINK, 0, BPF_B);
		b1 = new_block(JMP(BPF_JSET));
		b1->s.k = 0x08;
		b1->stmts = s;

		gen_and(b1, b2);

		b1 = gen_bcmp(OR_LINK, 10, 6, eaddr);
		gen_and(b2, b1);
		return b1;

	case Q_ADDR1:
		return (gen_bcmp(OR_LINK, 4, 6, eaddr));

	case Q_ADDR2:
		b0 = gen_mcmp(OR_LINK, 0, BPF_B, IEEE80211_FC0_TYPE_CTL,
			IEEE80211_FC0_TYPE_MASK);
		gen_not(b0);
		b1 = gen_mcmp(OR_LINK, 0, BPF_B, IEEE80211_FC0_SUBTYPE_CTS,
			IEEE80211_FC0_SUBTYPE_MASK);
		gen_not(b1);
		b2 = gen_mcmp(OR_LINK, 0, BPF_B, IEEE80211_FC0_SUBTYPE_ACK,
			IEEE80211_FC0_SUBTYPE_MASK);
		gen_not(b2);
		gen_and(b1, b2);
		gen_or(b0, b2);
		b1 = gen_bcmp(OR_LINK, 10, 6, eaddr);
		gen_and(b2, b1);
		return b1;

	case Q_ADDR3:
		b0 = gen_mcmp(OR_LINK, 0, BPF_B, IEEE80211_FC0_TYPE_CTL,
			IEEE80211_FC0_TYPE_MASK);
		gen_not(b0);
		b1 = gen_bcmp(OR_LINK, 16, 6, eaddr);
		gen_and(b0, b1);
		return b1;

	case Q_ADDR4:
		b0 = gen_mcmp(OR_LINK, 1, BPF_B,
			IEEE80211_FC1_DIR_DSTODS, IEEE80211_FC1_DIR_MASK);
		b1 = gen_bcmp(OR_LINK, 24, 6, eaddr);
		gen_and(b0, b1);
		return b1;

	case Q_AND:
		b0 = gen_wlanhostop(eaddr, Q_SRC);
		b1 = gen_wlanhostop(eaddr, Q_DST);
		gen_and(b0, b1);
		return b1;

	case Q_DEFAULT:
	case Q_OR:
		b0 = gen_wlanhostop(eaddr, Q_SRC);
		b1 = gen_wlanhostop(eaddr, Q_DST);
		gen_or(b0, b1);
		return b1;
	}
	abort();
	
}

static struct block *
gen_ipfchostop(eaddr, dir)
	register const u_char *eaddr;
	register int dir;
{
	register struct block *b0, *b1;

	switch (dir) {
	case Q_SRC:
		return gen_bcmp(OR_LINK, 10, 6, eaddr);

	case Q_DST:
		return gen_bcmp(OR_LINK, 2, 6, eaddr);

	case Q_AND:
		b0 = gen_ipfchostop(eaddr, Q_SRC);
		b1 = gen_ipfchostop(eaddr, Q_DST);
		gen_and(b0, b1);
		return b1;

	case Q_DEFAULT:
	case Q_OR:
		b0 = gen_ipfchostop(eaddr, Q_SRC);
		b1 = gen_ipfchostop(eaddr, Q_DST);
		gen_or(b0, b1);
		return b1;

	case Q_ADDR1:
		bpf_error("'addr1' is only supported on 802.11");
		break;

	case Q_ADDR2:
		bpf_error("'addr2' is only supported on 802.11");
		break;

	case Q_ADDR3:
		bpf_error("'addr3' is only supported on 802.11");
		break;

	case Q_ADDR4:
		bpf_error("'addr4' is only supported on 802.11");
		break;

	case Q_RA:
		bpf_error("'ra' is only supported on 802.11");
		break;

	case Q_TA:
		bpf_error("'ta' is only supported on 802.11");
		break;
	}
	abort();
	
}

static struct block *
gen_dnhostop(addr, dir)
	bpf_u_int32 addr;
	int dir;
{
	struct block *b0, *b1, *b2, *tmp;
	u_int offset_lh;	
	u_int offset_sh;	

	switch (dir) {

	case Q_DST:
		offset_sh = 1;	
		offset_lh = 7;	
		break;

	case Q_SRC:
		offset_sh = 3;	
		offset_lh = 15;	
		break;

	case Q_AND:
		
		b0 = gen_dnhostop(addr, Q_SRC);
		b1 = gen_dnhostop(addr, Q_DST);
		gen_and(b0, b1);
		return b1;

	case Q_OR:
	case Q_DEFAULT:
		
		b0 = gen_dnhostop(addr, Q_SRC);
		b1 = gen_dnhostop(addr, Q_DST);
		gen_or(b0, b1);
		return b1;

	case Q_ISO:
		bpf_error("ISO host filtering not implemented");

	default:
		abort();
	}
	b0 = gen_linktype(ETHERTYPE_DN);
	
	tmp = gen_mcmp(OR_NET, 2, BPF_H,
	    (bpf_int32)ntohs(0x0681), (bpf_int32)ntohs(0x07FF));
	b1 = gen_cmp(OR_NET, 2 + 1 + offset_lh,
	    BPF_H, (bpf_int32)ntohs((u_short)addr));
	gen_and(tmp, b1);
	
	tmp = gen_mcmp(OR_NET, 2, BPF_B, (bpf_int32)0x06, (bpf_int32)0x7);
	b2 = gen_cmp(OR_NET, 2 + offset_lh, BPF_H, (bpf_int32)ntohs((u_short)addr));
	gen_and(tmp, b2);
	gen_or(b2, b1);
	
	tmp = gen_mcmp(OR_NET, 2, BPF_H,
	    (bpf_int32)ntohs(0x0281), (bpf_int32)ntohs(0x07FF));
	b2 = gen_cmp(OR_NET, 2 + 1 + offset_sh, BPF_H, (bpf_int32)ntohs((u_short)addr));
	gen_and(tmp, b2);
	gen_or(b2, b1);
	
	tmp = gen_mcmp(OR_NET, 2, BPF_B, (bpf_int32)0x02, (bpf_int32)0x7);
	b2 = gen_cmp(OR_NET, 2 + offset_sh, BPF_H, (bpf_int32)ntohs((u_short)addr));
	gen_and(tmp, b2);
	gen_or(b2, b1);

	
	gen_and(b0, b1);
	return b1;
}

static struct block *
gen_mpls_linktype(proto)
	int proto;
{
	struct block *b0, *b1;

        switch (proto) {

        case Q_IP:
                
                b0 = gen_mcmp(OR_NET, -2, BPF_B, 0x01, 0x01);
                
                b1 = gen_mcmp(OR_NET, 0, BPF_B, 0x40, 0xf0);
                gen_and(b0, b1);
                return b1;
 
       case Q_IPV6:
                
                b0 = gen_mcmp(OR_NET, -2, BPF_B, 0x01, 0x01);
                
                b1 = gen_mcmp(OR_NET, 0, BPF_B, 0x60, 0xf0);
                gen_and(b0, b1);
                return b1;
 
       default:
                abort();
        }
}

static struct block *
gen_host(addr, mask, proto, dir, type)
	bpf_u_int32 addr;
	bpf_u_int32 mask;
	int proto;
	int dir;
	int type;
{
	struct block *b0, *b1;
	const char *typestr;

	if (type == Q_NET)
		typestr = "net";
	else
		typestr = "host";

	switch (proto) {

	case Q_DEFAULT:
		b0 = gen_host(addr, mask, Q_IP, dir, type);
		if (label_stack_depth == 0) {
			b1 = gen_host(addr, mask, Q_ARP, dir, type);
			gen_or(b0, b1);
			b0 = gen_host(addr, mask, Q_RARP, dir, type);
			gen_or(b1, b0);
		}
		return b0;

	case Q_IP:
		return gen_hostop(addr, mask, dir, ETHERTYPE_IP, 12, 16);

	case Q_RARP:
		return gen_hostop(addr, mask, dir, ETHERTYPE_REVARP, 14, 24);

	case Q_ARP:
		return gen_hostop(addr, mask, dir, ETHERTYPE_ARP, 14, 24);

	case Q_TCP:
		bpf_error("'tcp' modifier applied to %s", typestr);

	case Q_SCTP:
		bpf_error("'sctp' modifier applied to %s", typestr);

	case Q_UDP:
		bpf_error("'udp' modifier applied to %s", typestr);

	case Q_ICMP:
		bpf_error("'icmp' modifier applied to %s", typestr);

	case Q_IGMP:
		bpf_error("'igmp' modifier applied to %s", typestr);

	case Q_IGRP:
		bpf_error("'igrp' modifier applied to %s", typestr);

	case Q_PIM:
		bpf_error("'pim' modifier applied to %s", typestr);

	case Q_VRRP:
		bpf_error("'vrrp' modifier applied to %s", typestr);

	case Q_CARP:
		bpf_error("'carp' modifier applied to %s", typestr);

	case Q_ATALK:
		bpf_error("ATALK host filtering not implemented");

	case Q_AARP:
		bpf_error("AARP host filtering not implemented");

	case Q_DECNET:
		return gen_dnhostop(addr, dir);

	case Q_SCA:
		bpf_error("SCA host filtering not implemented");

	case Q_LAT:
		bpf_error("LAT host filtering not implemented");

	case Q_MOPDL:
		bpf_error("MOPDL host filtering not implemented");

	case Q_MOPRC:
		bpf_error("MOPRC host filtering not implemented");

	case Q_IPV6:
		bpf_error("'ip6' modifier applied to ip host");

	case Q_ICMPV6:
		bpf_error("'icmp6' modifier applied to %s", typestr);

	case Q_AH:
		bpf_error("'ah' modifier applied to %s", typestr);

	case Q_ESP:
		bpf_error("'esp' modifier applied to %s", typestr);

	case Q_ISO:
		bpf_error("ISO host filtering not implemented");

	case Q_ESIS:
		bpf_error("'esis' modifier applied to %s", typestr);

	case Q_ISIS:
		bpf_error("'isis' modifier applied to %s", typestr);

	case Q_CLNP:
		bpf_error("'clnp' modifier applied to %s", typestr);

	case Q_STP:
		bpf_error("'stp' modifier applied to %s", typestr);

	case Q_IPX:
		bpf_error("IPX host filtering not implemented");

	case Q_NETBEUI:
		bpf_error("'netbeui' modifier applied to %s", typestr);

	case Q_RADIO:
		bpf_error("'radio' modifier applied to %s", typestr);

	default:
		abort();
	}
	
}

#ifdef INET6
static struct block *
gen_host6(addr, mask, proto, dir, type)
	struct in6_addr *addr;
	struct in6_addr *mask;
	int proto;
	int dir;
	int type;
{
	const char *typestr;

	if (type == Q_NET)
		typestr = "net";
	else
		typestr = "host";

	switch (proto) {

	case Q_DEFAULT:
		return gen_host6(addr, mask, Q_IPV6, dir, type);

	case Q_LINK:
		bpf_error("link-layer modifier applied to ip6 %s", typestr);

	case Q_IP:
		bpf_error("'ip' modifier applied to ip6 %s", typestr);

	case Q_RARP:
		bpf_error("'rarp' modifier applied to ip6 %s", typestr);

	case Q_ARP:
		bpf_error("'arp' modifier applied to ip6 %s", typestr);

	case Q_SCTP:
		bpf_error("'sctp' modifier applied to %s", typestr);

	case Q_TCP:
		bpf_error("'tcp' modifier applied to %s", typestr);

	case Q_UDP:
		bpf_error("'udp' modifier applied to %s", typestr);

	case Q_ICMP:
		bpf_error("'icmp' modifier applied to %s", typestr);

	case Q_IGMP:
		bpf_error("'igmp' modifier applied to %s", typestr);

	case Q_IGRP:
		bpf_error("'igrp' modifier applied to %s", typestr);

	case Q_PIM:
		bpf_error("'pim' modifier applied to %s", typestr);

	case Q_VRRP:
		bpf_error("'vrrp' modifier applied to %s", typestr);

	case Q_CARP:
		bpf_error("'carp' modifier applied to %s", typestr);

	case Q_ATALK:
		bpf_error("ATALK host filtering not implemented");

	case Q_AARP:
		bpf_error("AARP host filtering not implemented");

	case Q_DECNET:
		bpf_error("'decnet' modifier applied to ip6 %s", typestr);

	case Q_SCA:
		bpf_error("SCA host filtering not implemented");

	case Q_LAT:
		bpf_error("LAT host filtering not implemented");

	case Q_MOPDL:
		bpf_error("MOPDL host filtering not implemented");

	case Q_MOPRC:
		bpf_error("MOPRC host filtering not implemented");

	case Q_IPV6:
		return gen_hostop6(addr, mask, dir, ETHERTYPE_IPV6, 8, 24);

	case Q_ICMPV6:
		bpf_error("'icmp6' modifier applied to %s", typestr);

	case Q_AH:
		bpf_error("'ah' modifier applied to %s", typestr);

	case Q_ESP:
		bpf_error("'esp' modifier applied to %s", typestr);

	case Q_ISO:
		bpf_error("ISO host filtering not implemented");

	case Q_ESIS:
		bpf_error("'esis' modifier applied to %s", typestr);

	case Q_ISIS:
		bpf_error("'isis' modifier applied to %s", typestr);

	case Q_CLNP:
		bpf_error("'clnp' modifier applied to %s", typestr);

	case Q_STP:
		bpf_error("'stp' modifier applied to %s", typestr);

	case Q_IPX:
		bpf_error("IPX host filtering not implemented");

	case Q_NETBEUI:
		bpf_error("'netbeui' modifier applied to %s", typestr);

	case Q_RADIO:
		bpf_error("'radio' modifier applied to %s", typestr);

	default:
		abort();
	}
	
}
#endif

#ifndef INET6
static struct block *
gen_gateway(eaddr, alist, proto, dir)
	const u_char *eaddr;
	bpf_u_int32 **alist;
	int proto;
	int dir;
{
	struct block *b0, *b1, *tmp;

	if (dir != 0)
		bpf_error("direction applied to 'gateway'");

	switch (proto) {
	case Q_DEFAULT:
	case Q_IP:
	case Q_ARP:
	case Q_RARP:
		switch (linktype) {
		case DLT_EN10MB:
		case DLT_NETANALYZER:
		case DLT_NETANALYZER_TRANSPARENT:
			b0 = gen_ehostop(eaddr, Q_OR);
			break;
		case DLT_FDDI:
			b0 = gen_fhostop(eaddr, Q_OR);
			break;
		case DLT_IEEE802:
			b0 = gen_thostop(eaddr, Q_OR);
			break;
		case DLT_IEEE802_11:
		case DLT_PRISM_HEADER:
		case DLT_IEEE802_11_RADIO_AVS:
		case DLT_IEEE802_11_RADIO:
		case DLT_PPI:
			b0 = gen_wlanhostop(eaddr, Q_OR);
			break;
		case DLT_SUNATM:
			if (!is_lane)
				bpf_error(
				    "'gateway' supported only on ethernet/FDDI/token ring/802.11/ATM LANE/Fibre Channel");
			b1 = gen_cmp(OR_LINK, SUNATM_PKT_BEGIN_POS,
			    BPF_H, 0xFF00);
			gen_not(b1);

			b0 = gen_ehostop(eaddr, Q_OR);
			gen_and(b1, b0);
			break;
		case DLT_IP_OVER_FC:
			b0 = gen_ipfchostop(eaddr, Q_OR);
			break;
		default:
			bpf_error(
			    "'gateway' supported only on ethernet/FDDI/token ring/802.11/ATM LANE/Fibre Channel");
		}
		b1 = gen_host(**alist++, 0xffffffff, proto, Q_OR, Q_HOST);
		while (*alist) {
			tmp = gen_host(**alist++, 0xffffffff, proto, Q_OR,
			    Q_HOST);
			gen_or(b1, tmp);
			b1 = tmp;
		}
		gen_not(b1);
		gen_and(b0, b1);
		return b1;
	}
	bpf_error("illegal modifier of 'gateway'");
	
}
#endif

struct block *
gen_proto_abbrev(proto)
	int proto;
{
	struct block *b0;
	struct block *b1;

	switch (proto) {

	case Q_SCTP:
		b1 = gen_proto(IPPROTO_SCTP, Q_IP, Q_DEFAULT);
		b0 = gen_proto(IPPROTO_SCTP, Q_IPV6, Q_DEFAULT);
		gen_or(b0, b1);
		break;

	case Q_TCP:
		b1 = gen_proto(IPPROTO_TCP, Q_IP, Q_DEFAULT);
		b0 = gen_proto(IPPROTO_TCP, Q_IPV6, Q_DEFAULT);
		gen_or(b0, b1);
		break;

	case Q_UDP:
		b1 = gen_proto(IPPROTO_UDP, Q_IP, Q_DEFAULT);
		b0 = gen_proto(IPPROTO_UDP, Q_IPV6, Q_DEFAULT);
		gen_or(b0, b1);
		break;

	case Q_ICMP:
		b1 = gen_proto(IPPROTO_ICMP, Q_IP, Q_DEFAULT);
		break;

#ifndef	IPPROTO_IGMP
#define	IPPROTO_IGMP	2
#endif

	case Q_IGMP:
		b1 = gen_proto(IPPROTO_IGMP, Q_IP, Q_DEFAULT);
		break;

#ifndef	IPPROTO_IGRP
#define	IPPROTO_IGRP	9
#endif
	case Q_IGRP:
		b1 = gen_proto(IPPROTO_IGRP, Q_IP, Q_DEFAULT);
		break;

#ifndef IPPROTO_PIM
#define IPPROTO_PIM	103
#endif

	case Q_PIM:
		b1 = gen_proto(IPPROTO_PIM, Q_IP, Q_DEFAULT);
		b0 = gen_proto(IPPROTO_PIM, Q_IPV6, Q_DEFAULT);
		gen_or(b0, b1);
		break;

#ifndef IPPROTO_VRRP
#define IPPROTO_VRRP	112
#endif

	case Q_VRRP:
		b1 = gen_proto(IPPROTO_VRRP, Q_IP, Q_DEFAULT);
		break;

#ifndef IPPROTO_CARP
#define IPPROTO_CARP	112
#endif

	case Q_CARP:
		b1 = gen_proto(IPPROTO_CARP, Q_IP, Q_DEFAULT);
		break;

	case Q_IP:
		b1 =  gen_linktype(ETHERTYPE_IP);
		break;

	case Q_ARP:
		b1 =  gen_linktype(ETHERTYPE_ARP);
		break;

	case Q_RARP:
		b1 =  gen_linktype(ETHERTYPE_REVARP);
		break;

	case Q_LINK:
		bpf_error("link layer applied in wrong context");

	case Q_ATALK:
		b1 =  gen_linktype(ETHERTYPE_ATALK);
		break;

	case Q_AARP:
		b1 =  gen_linktype(ETHERTYPE_AARP);
		break;

	case Q_DECNET:
		b1 =  gen_linktype(ETHERTYPE_DN);
		break;

	case Q_SCA:
		b1 =  gen_linktype(ETHERTYPE_SCA);
		break;

	case Q_LAT:
		b1 =  gen_linktype(ETHERTYPE_LAT);
		break;

	case Q_MOPDL:
		b1 =  gen_linktype(ETHERTYPE_MOPDL);
		break;

	case Q_MOPRC:
		b1 =  gen_linktype(ETHERTYPE_MOPRC);
		break;

	case Q_IPV6:
		b1 = gen_linktype(ETHERTYPE_IPV6);
		break;

#ifndef IPPROTO_ICMPV6
#define IPPROTO_ICMPV6	58
#endif
	case Q_ICMPV6:
		b1 = gen_proto(IPPROTO_ICMPV6, Q_IPV6, Q_DEFAULT);
		break;

#ifndef IPPROTO_AH
#define IPPROTO_AH	51
#endif
	case Q_AH:
		b1 = gen_proto(IPPROTO_AH, Q_IP, Q_DEFAULT);
		b0 = gen_proto(IPPROTO_AH, Q_IPV6, Q_DEFAULT);
		gen_or(b0, b1);
		break;

#ifndef IPPROTO_ESP
#define IPPROTO_ESP	50
#endif
	case Q_ESP:
		b1 = gen_proto(IPPROTO_ESP, Q_IP, Q_DEFAULT);
		b0 = gen_proto(IPPROTO_ESP, Q_IPV6, Q_DEFAULT);
		gen_or(b0, b1);
		break;

	case Q_ISO:
		b1 = gen_linktype(LLCSAP_ISONS);
		break;

	case Q_ESIS:
		b1 = gen_proto(ISO9542_ESIS, Q_ISO, Q_DEFAULT);
		break;

	case Q_ISIS:
		b1 = gen_proto(ISO10589_ISIS, Q_ISO, Q_DEFAULT);
		break;

	case Q_ISIS_L1: 
		b0 = gen_proto(ISIS_L1_LAN_IIH, Q_ISIS, Q_DEFAULT);
		b1 = gen_proto(ISIS_PTP_IIH, Q_ISIS, Q_DEFAULT); 
		gen_or(b0, b1);
		b0 = gen_proto(ISIS_L1_LSP, Q_ISIS, Q_DEFAULT);
		gen_or(b0, b1);
		b0 = gen_proto(ISIS_L1_CSNP, Q_ISIS, Q_DEFAULT);
		gen_or(b0, b1);
		b0 = gen_proto(ISIS_L1_PSNP, Q_ISIS, Q_DEFAULT);
		gen_or(b0, b1);
		break;

	case Q_ISIS_L2: 
		b0 = gen_proto(ISIS_L2_LAN_IIH, Q_ISIS, Q_DEFAULT);
		b1 = gen_proto(ISIS_PTP_IIH, Q_ISIS, Q_DEFAULT); 
		gen_or(b0, b1);
		b0 = gen_proto(ISIS_L2_LSP, Q_ISIS, Q_DEFAULT);
		gen_or(b0, b1);
		b0 = gen_proto(ISIS_L2_CSNP, Q_ISIS, Q_DEFAULT);
		gen_or(b0, b1);
		b0 = gen_proto(ISIS_L2_PSNP, Q_ISIS, Q_DEFAULT);
		gen_or(b0, b1);
		break;

	case Q_ISIS_IIH: 
		b0 = gen_proto(ISIS_L1_LAN_IIH, Q_ISIS, Q_DEFAULT);
		b1 = gen_proto(ISIS_L2_LAN_IIH, Q_ISIS, Q_DEFAULT);
		gen_or(b0, b1);
		b0 = gen_proto(ISIS_PTP_IIH, Q_ISIS, Q_DEFAULT);
		gen_or(b0, b1);
		break;

	case Q_ISIS_LSP:
		b0 = gen_proto(ISIS_L1_LSP, Q_ISIS, Q_DEFAULT);
		b1 = gen_proto(ISIS_L2_LSP, Q_ISIS, Q_DEFAULT);
		gen_or(b0, b1);
		break;

	case Q_ISIS_SNP:
		b0 = gen_proto(ISIS_L1_CSNP, Q_ISIS, Q_DEFAULT);
		b1 = gen_proto(ISIS_L2_CSNP, Q_ISIS, Q_DEFAULT);
		gen_or(b0, b1);
		b0 = gen_proto(ISIS_L1_PSNP, Q_ISIS, Q_DEFAULT);
		gen_or(b0, b1);
		b0 = gen_proto(ISIS_L2_PSNP, Q_ISIS, Q_DEFAULT);
		gen_or(b0, b1);
		break;

	case Q_ISIS_CSNP:
		b0 = gen_proto(ISIS_L1_CSNP, Q_ISIS, Q_DEFAULT);
		b1 = gen_proto(ISIS_L2_CSNP, Q_ISIS, Q_DEFAULT);
		gen_or(b0, b1);
		break;

	case Q_ISIS_PSNP:
		b0 = gen_proto(ISIS_L1_PSNP, Q_ISIS, Q_DEFAULT);
		b1 = gen_proto(ISIS_L2_PSNP, Q_ISIS, Q_DEFAULT);
		gen_or(b0, b1);
		break;

	case Q_CLNP:
		b1 = gen_proto(ISO8473_CLNP, Q_ISO, Q_DEFAULT);
		break;

	case Q_STP:
		b1 = gen_linktype(LLCSAP_8021D);
		break;

	case Q_IPX:
		b1 = gen_linktype(LLCSAP_IPX);
		break;

	case Q_NETBEUI:
		b1 = gen_linktype(LLCSAP_NETBEUI);
		break;

	case Q_RADIO:
		bpf_error("'radio' is not a valid protocol type");

	default:
		abort();
	}
	return b1;
}

static struct block *
gen_ipfrag()
{
	struct slist *s;
	struct block *b;

	
	s = gen_load_a(OR_NET, 6, BPF_H);
	b = new_block(JMP(BPF_JSET));
	b->s.k = 0x1fff;
	b->stmts = s;
	gen_not(b);

	return b;
}

static struct block *
gen_portatom(off, v)
	int off;
	bpf_int32 v;
{
	return gen_cmp(OR_TRAN_IPV4, off, BPF_H, v);
}

static struct block *
gen_portatom6(off, v)
	int off;
	bpf_int32 v;
{
	return gen_cmp(OR_TRAN_IPV6, off, BPF_H, v);
}

struct block *
gen_portop(port, proto, dir)
	int port, proto, dir;
{
	struct block *b0, *b1, *tmp;

	
	tmp = gen_cmp(OR_NET, 9, BPF_B, (bpf_int32)proto);
	b0 = gen_ipfrag();
	gen_and(tmp, b0);

	switch (dir) {
	case Q_SRC:
		b1 = gen_portatom(0, (bpf_int32)port);
		break;

	case Q_DST:
		b1 = gen_portatom(2, (bpf_int32)port);
		break;

	case Q_OR:
	case Q_DEFAULT:
		tmp = gen_portatom(0, (bpf_int32)port);
		b1 = gen_portatom(2, (bpf_int32)port);
		gen_or(tmp, b1);
		break;

	case Q_AND:
		tmp = gen_portatom(0, (bpf_int32)port);
		b1 = gen_portatom(2, (bpf_int32)port);
		gen_and(tmp, b1);
		break;

	default:
		abort();
	}
	gen_and(b0, b1);

	return b1;
}

static struct block *
gen_port(port, ip_proto, dir)
	int port;
	int ip_proto;
	int dir;
{
	struct block *b0, *b1, *tmp;

	b0 =  gen_linktype(ETHERTYPE_IP);

	switch (ip_proto) {
	case IPPROTO_UDP:
	case IPPROTO_TCP:
	case IPPROTO_SCTP:
		b1 = gen_portop(port, ip_proto, dir);
		break;

	case PROTO_UNDEF:
		tmp = gen_portop(port, IPPROTO_TCP, dir);
		b1 = gen_portop(port, IPPROTO_UDP, dir);
		gen_or(tmp, b1);
		tmp = gen_portop(port, IPPROTO_SCTP, dir);
		gen_or(tmp, b1);
		break;

	default:
		abort();
	}
	gen_and(b0, b1);
	return b1;
}

struct block *
gen_portop6(port, proto, dir)
	int port, proto, dir;
{
	struct block *b0, *b1, *tmp;

	
	
	b0 = gen_cmp(OR_NET, 6, BPF_B, (bpf_int32)proto);

	switch (dir) {
	case Q_SRC:
		b1 = gen_portatom6(0, (bpf_int32)port);
		break;

	case Q_DST:
		b1 = gen_portatom6(2, (bpf_int32)port);
		break;

	case Q_OR:
	case Q_DEFAULT:
		tmp = gen_portatom6(0, (bpf_int32)port);
		b1 = gen_portatom6(2, (bpf_int32)port);
		gen_or(tmp, b1);
		break;

	case Q_AND:
		tmp = gen_portatom6(0, (bpf_int32)port);
		b1 = gen_portatom6(2, (bpf_int32)port);
		gen_and(tmp, b1);
		break;

	default:
		abort();
	}
	gen_and(b0, b1);

	return b1;
}

static struct block *
gen_port6(port, ip_proto, dir)
	int port;
	int ip_proto;
	int dir;
{
	struct block *b0, *b1, *tmp;

	
	b0 =  gen_linktype(ETHERTYPE_IPV6);

	switch (ip_proto) {
	case IPPROTO_UDP:
	case IPPROTO_TCP:
	case IPPROTO_SCTP:
		b1 = gen_portop6(port, ip_proto, dir);
		break;

	case PROTO_UNDEF:
		tmp = gen_portop6(port, IPPROTO_TCP, dir);
		b1 = gen_portop6(port, IPPROTO_UDP, dir);
		gen_or(tmp, b1);
		tmp = gen_portop6(port, IPPROTO_SCTP, dir);
		gen_or(tmp, b1);
		break;

	default:
		abort();
	}
	gen_and(b0, b1);
	return b1;
}

static struct block *
gen_portrangeatom(off, v1, v2)
	int off;
	bpf_int32 v1, v2;
{
	struct block *b1, *b2;

	if (v1 > v2) {
		bpf_int32 vtemp;

		vtemp = v1;
		v1 = v2;
		v2 = vtemp;
	}

	b1 = gen_cmp_ge(OR_TRAN_IPV4, off, BPF_H, v1);
	b2 = gen_cmp_le(OR_TRAN_IPV4, off, BPF_H, v2);

	gen_and(b1, b2); 

	return b2;
}

struct block *
gen_portrangeop(port1, port2, proto, dir)
	int port1, port2;
	int proto;
	int dir;
{
	struct block *b0, *b1, *tmp;

	
	tmp = gen_cmp(OR_NET, 9, BPF_B, (bpf_int32)proto);
	b0 = gen_ipfrag();
	gen_and(tmp, b0);

	switch (dir) {
	case Q_SRC:
		b1 = gen_portrangeatom(0, (bpf_int32)port1, (bpf_int32)port2);
		break;

	case Q_DST:
		b1 = gen_portrangeatom(2, (bpf_int32)port1, (bpf_int32)port2);
		break;

	case Q_OR:
	case Q_DEFAULT:
		tmp = gen_portrangeatom(0, (bpf_int32)port1, (bpf_int32)port2);
		b1 = gen_portrangeatom(2, (bpf_int32)port1, (bpf_int32)port2);
		gen_or(tmp, b1);
		break;

	case Q_AND:
		tmp = gen_portrangeatom(0, (bpf_int32)port1, (bpf_int32)port2);
		b1 = gen_portrangeatom(2, (bpf_int32)port1, (bpf_int32)port2);
		gen_and(tmp, b1);
		break;

	default:
		abort();
	}
	gen_and(b0, b1);

	return b1;
}

static struct block *
gen_portrange(port1, port2, ip_proto, dir)
	int port1, port2;
	int ip_proto;
	int dir;
{
	struct block *b0, *b1, *tmp;

	
	b0 =  gen_linktype(ETHERTYPE_IP);

	switch (ip_proto) {
	case IPPROTO_UDP:
	case IPPROTO_TCP:
	case IPPROTO_SCTP:
		b1 = gen_portrangeop(port1, port2, ip_proto, dir);
		break;

	case PROTO_UNDEF:
		tmp = gen_portrangeop(port1, port2, IPPROTO_TCP, dir);
		b1 = gen_portrangeop(port1, port2, IPPROTO_UDP, dir);
		gen_or(tmp, b1);
		tmp = gen_portrangeop(port1, port2, IPPROTO_SCTP, dir);
		gen_or(tmp, b1);
		break;

	default:
		abort();
	}
	gen_and(b0, b1);
	return b1;
}

static struct block *
gen_portrangeatom6(off, v1, v2)
	int off;
	bpf_int32 v1, v2;
{
	struct block *b1, *b2;

	if (v1 > v2) {
		bpf_int32 vtemp;

		vtemp = v1;
		v1 = v2;
		v2 = vtemp;
	}

	b1 = gen_cmp_ge(OR_TRAN_IPV6, off, BPF_H, v1);
	b2 = gen_cmp_le(OR_TRAN_IPV6, off, BPF_H, v2);

	gen_and(b1, b2); 

	return b2;
}

struct block *
gen_portrangeop6(port1, port2, proto, dir)
	int port1, port2;
	int proto;
	int dir;
{
	struct block *b0, *b1, *tmp;

	
	
	b0 = gen_cmp(OR_NET, 6, BPF_B, (bpf_int32)proto);

	switch (dir) {
	case Q_SRC:
		b1 = gen_portrangeatom6(0, (bpf_int32)port1, (bpf_int32)port2);
		break;

	case Q_DST:
		b1 = gen_portrangeatom6(2, (bpf_int32)port1, (bpf_int32)port2);
		break;

	case Q_OR:
	case Q_DEFAULT:
		tmp = gen_portrangeatom6(0, (bpf_int32)port1, (bpf_int32)port2);
		b1 = gen_portrangeatom6(2, (bpf_int32)port1, (bpf_int32)port2);
		gen_or(tmp, b1);
		break;

	case Q_AND:
		tmp = gen_portrangeatom6(0, (bpf_int32)port1, (bpf_int32)port2);
		b1 = gen_portrangeatom6(2, (bpf_int32)port1, (bpf_int32)port2);
		gen_and(tmp, b1);
		break;

	default:
		abort();
	}
	gen_and(b0, b1);

	return b1;
}

static struct block *
gen_portrange6(port1, port2, ip_proto, dir)
	int port1, port2;
	int ip_proto;
	int dir;
{
	struct block *b0, *b1, *tmp;

	
	b0 =  gen_linktype(ETHERTYPE_IPV6);

	switch (ip_proto) {
	case IPPROTO_UDP:
	case IPPROTO_TCP:
	case IPPROTO_SCTP:
		b1 = gen_portrangeop6(port1, port2, ip_proto, dir);
		break;

	case PROTO_UNDEF:
		tmp = gen_portrangeop6(port1, port2, IPPROTO_TCP, dir);
		b1 = gen_portrangeop6(port1, port2, IPPROTO_UDP, dir);
		gen_or(tmp, b1);
		tmp = gen_portrangeop6(port1, port2, IPPROTO_SCTP, dir);
		gen_or(tmp, b1);
		break;

	default:
		abort();
	}
	gen_and(b0, b1);
	return b1;
}

static int
lookup_proto(name, proto)
	register const char *name;
	register int proto;
{
	register int v;

	switch (proto) {

	case Q_DEFAULT:
	case Q_IP:
	case Q_IPV6:
		v = pcap_nametoproto(name);
		if (v == PROTO_UNDEF)
			bpf_error("unknown ip proto '%s'", name);
		break;

	case Q_LINK:
		
		v = pcap_nametoeproto(name);
		if (v == PROTO_UNDEF) {
			v = pcap_nametollc(name);
			if (v == PROTO_UNDEF)
				bpf_error("unknown ether proto '%s'", name);
		}
		break;

	case Q_ISO:
		if (strcmp(name, "esis") == 0)
			v = ISO9542_ESIS;
		else if (strcmp(name, "isis") == 0)
			v = ISO10589_ISIS;
		else if (strcmp(name, "clnp") == 0)
			v = ISO8473_CLNP;
		else
			bpf_error("unknown osi proto '%s'", name);
		break;

	default:
		v = PROTO_UNDEF;
		break;
	}
	return v;
}

#if 0
struct stmt *
gen_joinsp(s, n)
	struct stmt **s;
	int n;
{
	return NULL;
}
#endif

static struct block *
gen_protochain(v, proto, dir)
	int v;
	int proto;
	int dir;
{
#ifdef NO_PROTOCHAIN
	return gen_proto(v, proto, dir);
#else
	struct block *b0, *b;
	struct slist *s[100];
	int fix2, fix3, fix4, fix5;
	int ahcheck, again, end;
	int i, max;
	int reg2 = alloc_reg();

	memset(s, 0, sizeof(s));
	fix2 = fix3 = fix4 = fix5 = 0;

	switch (proto) {
	case Q_IP:
	case Q_IPV6:
		break;
	case Q_DEFAULT:
		b0 = gen_protochain(v, Q_IP, dir);
		b = gen_protochain(v, Q_IPV6, dir);
		gen_or(b0, b);
		return b;
	default:
		bpf_error("bad protocol applied for 'protochain'");
		
	}

	switch (linktype) {

	case DLT_IEEE802_11:
	case DLT_PRISM_HEADER:
	case DLT_IEEE802_11_RADIO_AVS:
	case DLT_IEEE802_11_RADIO:
	case DLT_PPI:
		bpf_error("'protochain' not supported with 802.11");
	}

	no_optimize = 1; 

	i = 0;
	s[i] = new_stmt(0);	
	i++;

	switch (proto) {
	case Q_IP:
		b0 = gen_linktype(ETHERTYPE_IP);

		
		s[i] = new_stmt(BPF_LD|BPF_ABS|BPF_B);
		s[i]->s.k = off_macpl + off_nl + 9;
		i++;
		
		s[i] = new_stmt(BPF_LDX|BPF_MSH|BPF_B);
		s[i]->s.k = off_macpl + off_nl;
		i++;
		break;

	case Q_IPV6:
		b0 = gen_linktype(ETHERTYPE_IPV6);

		
		s[i] = new_stmt(BPF_LD|BPF_ABS|BPF_B);
		s[i]->s.k = off_macpl + off_nl + 6;
		i++;
		
		s[i] = new_stmt(BPF_LDX|BPF_IMM);
		s[i]->s.k = 40;
		i++;
		break;

	default:
		bpf_error("unsupported proto to gen_protochain");
		
	}

	
	again = i;
	s[i] = new_stmt(BPF_JMP|BPF_JEQ|BPF_K);
	s[i]->s.k = v;
	s[i]->s.jt = NULL;		
	s[i]->s.jf = NULL;		
	fix5 = i;
	i++;

#ifndef IPPROTO_NONE
#define IPPROTO_NONE	59
#endif
	
	s[i] = new_stmt(BPF_JMP|BPF_JEQ|BPF_K);
	s[i]->s.jt = NULL;	
	s[i]->s.jf = NULL;	
	s[i]->s.k = IPPROTO_NONE;
	s[fix5]->s.jf = s[i];
	fix2 = i;
	i++;

	if (proto == Q_IPV6) {
		int v6start, v6end, v6advance, j;

		v6start = i;
		
		s[i] = new_stmt(BPF_JMP|BPF_JEQ|BPF_K);
		s[i]->s.jt = NULL;	
		s[i]->s.jf = NULL;	
		s[i]->s.k = IPPROTO_HOPOPTS;
		s[fix2]->s.jf = s[i];
		i++;
		
		s[i - 1]->s.jf = s[i] = new_stmt(BPF_JMP|BPF_JEQ|BPF_K);
		s[i]->s.jt = NULL;	
		s[i]->s.jf = NULL;	
		s[i]->s.k = IPPROTO_DSTOPTS;
		i++;
		
		s[i - 1]->s.jf = s[i] = new_stmt(BPF_JMP|BPF_JEQ|BPF_K);
		s[i]->s.jt = NULL;	
		s[i]->s.jf = NULL;	
		s[i]->s.k = IPPROTO_ROUTING;
		i++;
		
		s[i - 1]->s.jf = s[i] = new_stmt(BPF_JMP|BPF_JEQ|BPF_K);
		s[i]->s.jt = NULL;	
		s[i]->s.jf = NULL;	
		s[i]->s.k = IPPROTO_FRAGMENT;
		fix3 = i;
		v6end = i;
		i++;

		
		v6advance = i;

		
		s[i] = new_stmt(BPF_LD|BPF_IND|BPF_B);
		s[i]->s.k = off_macpl + off_nl;
		i++;
		
		s[i] = new_stmt(BPF_ST);
		s[i]->s.k = reg2;
		i++;
		
		s[i] = new_stmt(BPF_LD|BPF_IND|BPF_B);
		s[i]->s.k = off_macpl + off_nl + 1;
		i++;
		
		s[i] = new_stmt(BPF_ALU|BPF_ADD|BPF_K);
		s[i]->s.k = 1;
		i++;
		
		s[i] = new_stmt(BPF_ALU|BPF_MUL|BPF_K);
		s[i]->s.k = 8;
		i++;
		
		s[i] = new_stmt(BPF_ALU|BPF_ADD|BPF_X);
		s[i]->s.k = 0;
		i++;
		
		s[i] = new_stmt(BPF_MISC|BPF_TAX);
		i++;
		
		s[i] = new_stmt(BPF_LD|BPF_MEM);
		s[i]->s.k = reg2;
		i++;

		
		s[i] = new_stmt(BPF_JMP|BPF_JA);
		s[i]->s.k = again - i - 1;
		s[i - 1]->s.jf = s[i];
		i++;

		
		for (j = v6start; j <= v6end; j++)
			s[j]->s.jt = s[v6advance];
	} else {
		
		s[i] = new_stmt(BPF_ALU|BPF_ADD|BPF_K);
		s[i]->s.k = 0;
		s[fix2]->s.jf = s[i];
		i++;
	}

	
	ahcheck = i;
	
	s[i] = new_stmt(BPF_JMP|BPF_JEQ|BPF_K);
	s[i]->s.jt = NULL;	
	s[i]->s.jf = NULL;	
	s[i]->s.k = IPPROTO_AH;
	if (fix3)
		s[fix3]->s.jf = s[ahcheck];
	fix4 = i;
	i++;

	
	s[i - 1]->s.jt = s[i] = new_stmt(BPF_MISC|BPF_TXA);
	i++;
	
	s[i] = new_stmt(BPF_LD|BPF_IND|BPF_B);
	s[i]->s.k = off_macpl + off_nl;
	i++;
	
	s[i] = new_stmt(BPF_ST);
	s[i]->s.k = reg2;
	i++;
	
	s[i - 1]->s.jt = s[i] = new_stmt(BPF_MISC|BPF_TXA);
	i++;
	
	s[i] = new_stmt(BPF_ALU|BPF_ADD|BPF_K);
	s[i]->s.k = 1;
	i++;
	
	s[i] = new_stmt(BPF_MISC|BPF_TAX);
	i++;
	
	s[i] = new_stmt(BPF_LD|BPF_IND|BPF_B);
	s[i]->s.k = off_macpl + off_nl;
	i++;
	
	s[i] = new_stmt(BPF_ALU|BPF_ADD|BPF_K);
	s[i]->s.k = 2;
	i++;
	
	s[i] = new_stmt(BPF_ALU|BPF_MUL|BPF_K);
	s[i]->s.k = 4;
	i++;
	
	s[i] = new_stmt(BPF_MISC|BPF_TAX);
	i++;
	
	s[i] = new_stmt(BPF_LD|BPF_MEM);
	s[i]->s.k = reg2;
	i++;

	
	s[i] = new_stmt(BPF_JMP|BPF_JA);
	s[i]->s.k = again - i - 1;
	i++;

	
	end = i;
	s[i] = new_stmt(BPF_ALU|BPF_ADD|BPF_K);
	s[i]->s.k = 0;
	s[fix2]->s.jt = s[end];
	s[fix4]->s.jf = s[end];
	s[fix5]->s.jt = s[end];
	i++;

	max = i;
	for (i = 0; i < max - 1; i++)
		s[i]->next = s[i + 1];
	s[max - 1]->next = NULL;

	b = new_block(JMP(BPF_JEQ));
	b->stmts = s[1];	
	b->s.k = v;

	free_reg(reg2);

	gen_and(b0, b);
	return b;
#endif
}

static struct block *
gen_check_802_11_data_frame()
{
	struct slist *s;
	struct block *b0, *b1;

	s = gen_load_a(OR_LINK, 0, BPF_B);
	b0 = new_block(JMP(BPF_JSET));
	b0->s.k = 0x08;
	b0->stmts = s;
	
	s = gen_load_a(OR_LINK, 0, BPF_B);
	b1 = new_block(JMP(BPF_JSET));
	b1->s.k = 0x04;
	b1->stmts = s;
	gen_not(b1);

	gen_and(b1, b0);

	return b0;
}

static struct block *
gen_proto(v, proto, dir)
	int v;
	int proto;
	int dir;
{
	struct block *b0, *b1;
#ifndef CHASE_CHAIN
	struct block *b2;
#endif

	if (dir != Q_DEFAULT)
		bpf_error("direction applied to 'proto'");

	switch (proto) {
	case Q_DEFAULT:
		b0 = gen_proto(v, Q_IP, dir);
		b1 = gen_proto(v, Q_IPV6, dir);
		gen_or(b0, b1);
		return b1;

	case Q_IP:
		b0 = gen_linktype(ETHERTYPE_IP);
#ifndef CHASE_CHAIN
		b1 = gen_cmp(OR_NET, 9, BPF_B, (bpf_int32)v);
#else
		b1 = gen_protochain(v, Q_IP);
#endif
		gen_and(b0, b1);
		return b1;

	case Q_ISO:
		switch (linktype) {

		case DLT_FRELAY:
			return gen_cmp(OR_LINK, 2, BPF_H, (0x03<<8) | v);
			
			break;

		case DLT_C_HDLC:
			b0 = gen_linktype(LLCSAP_ISONS<<8 | LLCSAP_ISONS);
			
			b1 = gen_cmp(OR_NET_NOSNAP, 1, BPF_B, (long)v);
			gen_and(b0, b1);
			return b1;

		default:
			b0 = gen_linktype(LLCSAP_ISONS);
			b1 = gen_cmp(OR_NET_NOSNAP, 0, BPF_B, (long)v);
			gen_and(b0, b1);
			return b1;
		}

	case Q_ISIS:
		b0 = gen_proto(ISO10589_ISIS, Q_ISO, Q_DEFAULT);
		b1 = gen_cmp(OR_NET_NOSNAP, 4, BPF_B, (long)v);
		gen_and(b0, b1);
		return b1;

	case Q_ARP:
		bpf_error("arp does not encapsulate another protocol");
		

	case Q_RARP:
		bpf_error("rarp does not encapsulate another protocol");
		

	case Q_ATALK:
		bpf_error("atalk encapsulation is not specifiable");
		

	case Q_DECNET:
		bpf_error("decnet encapsulation is not specifiable");
		

	case Q_SCA:
		bpf_error("sca does not encapsulate another protocol");
		

	case Q_LAT:
		bpf_error("lat does not encapsulate another protocol");
		

	case Q_MOPRC:
		bpf_error("moprc does not encapsulate another protocol");
		

	case Q_MOPDL:
		bpf_error("mopdl does not encapsulate another protocol");
		

	case Q_LINK:
		return gen_linktype(v);

	case Q_UDP:
		bpf_error("'udp proto' is bogus");
		

	case Q_TCP:
		bpf_error("'tcp proto' is bogus");
		

	case Q_SCTP:
		bpf_error("'sctp proto' is bogus");
		

	case Q_ICMP:
		bpf_error("'icmp proto' is bogus");
		

	case Q_IGMP:
		bpf_error("'igmp proto' is bogus");
		

	case Q_IGRP:
		bpf_error("'igrp proto' is bogus");
		

	case Q_PIM:
		bpf_error("'pim proto' is bogus");
		

	case Q_VRRP:
		bpf_error("'vrrp proto' is bogus");
		

	case Q_CARP:
		bpf_error("'carp proto' is bogus");
		

	case Q_IPV6:
		b0 = gen_linktype(ETHERTYPE_IPV6);
#ifndef CHASE_CHAIN
		b2 = gen_cmp(OR_NET, 6, BPF_B, IPPROTO_FRAGMENT);
		b1 = gen_cmp(OR_NET, 40, BPF_B, (bpf_int32)v);
		gen_and(b2, b1);
		b2 = gen_cmp(OR_NET, 6, BPF_B, (bpf_int32)v);
		gen_or(b2, b1);
#else
		b1 = gen_protochain(v, Q_IPV6);
#endif
		gen_and(b0, b1);
		return b1;

	case Q_ICMPV6:
		bpf_error("'icmp6 proto' is bogus");

	case Q_AH:
		bpf_error("'ah proto' is bogus");

	case Q_ESP:
		bpf_error("'ah proto' is bogus");

	case Q_STP:
		bpf_error("'stp proto' is bogus");

	case Q_IPX:
		bpf_error("'ipx proto' is bogus");

	case Q_NETBEUI:
		bpf_error("'netbeui proto' is bogus");

	case Q_RADIO:
		bpf_error("'radio proto' is bogus");

	default:
		abort();
		
	}
	
}

struct block *
gen_scode(name, q)
	register const char *name;
	struct qual q;
{
	int proto = q.proto;
	int dir = q.dir;
	int tproto;
	u_char *eaddr;
	bpf_u_int32 mask, addr;
#ifndef INET6
	bpf_u_int32 **alist;
#else
	int tproto6;
	struct sockaddr_in *sin4;
	struct sockaddr_in6 *sin6;
	struct addrinfo *res, *res0;
	struct in6_addr mask128;
#endif 
	struct block *b, *tmp;
	int port, real_proto;
	int port1, port2;

	switch (q.addr) {

	case Q_NET:
		addr = pcap_nametonetaddr(name);
		if (addr == 0)
			bpf_error("unknown network '%s'", name);
		
		mask = 0xffffffff;
		while (addr && (addr & 0xff000000) == 0) {
			addr <<= 8;
			mask <<= 8;
		}
		return gen_host(addr, mask, proto, dir, q.addr);

	case Q_DEFAULT:
	case Q_HOST:
		if (proto == Q_LINK) {
			switch (linktype) {

			case DLT_EN10MB:
			case DLT_NETANALYZER:
			case DLT_NETANALYZER_TRANSPARENT:
				eaddr = pcap_ether_hostton(name);
				if (eaddr == NULL)
					bpf_error(
					    "unknown ether host '%s'", name);
				b = gen_ehostop(eaddr, dir);
				free(eaddr);
				return b;

			case DLT_FDDI:
				eaddr = pcap_ether_hostton(name);
				if (eaddr == NULL)
					bpf_error(
					    "unknown FDDI host '%s'", name);
				b = gen_fhostop(eaddr, dir);
				free(eaddr);
				return b;

			case DLT_IEEE802:
				eaddr = pcap_ether_hostton(name);
				if (eaddr == NULL)
					bpf_error(
					    "unknown token ring host '%s'", name);
				b = gen_thostop(eaddr, dir);
				free(eaddr);
				return b;

			case DLT_IEEE802_11:
			case DLT_PRISM_HEADER:
			case DLT_IEEE802_11_RADIO_AVS:
			case DLT_IEEE802_11_RADIO:
			case DLT_PPI:
				eaddr = pcap_ether_hostton(name);
				if (eaddr == NULL)
					bpf_error(
					    "unknown 802.11 host '%s'", name);
				b = gen_wlanhostop(eaddr, dir);
				free(eaddr);
				return b;

			case DLT_IP_OVER_FC:
				eaddr = pcap_ether_hostton(name);
				if (eaddr == NULL)
					bpf_error(
					    "unknown Fibre Channel host '%s'", name);
				b = gen_ipfchostop(eaddr, dir);
				free(eaddr);
				return b;

			case DLT_SUNATM:
				if (!is_lane)
					break;

				tmp = gen_cmp(OR_LINK, SUNATM_PKT_BEGIN_POS,
				    BPF_H, 0xFF00);
				gen_not(tmp);

				eaddr = pcap_ether_hostton(name);
				if (eaddr == NULL)
					bpf_error(
					    "unknown ether host '%s'", name);
				b = gen_ehostop(eaddr, dir);
				gen_and(tmp, b);
				free(eaddr);
				return b;
			}

			bpf_error("only ethernet/FDDI/token ring/802.11/ATM LANE/Fibre Channel supports link-level host name");
		} else if (proto == Q_DECNET) {
			unsigned short dn_addr = __pcap_nametodnaddr(name);
			return (gen_host(dn_addr, 0, proto, dir, q.addr));
		} else {
#ifndef INET6
			alist = pcap_nametoaddr(name);
			if (alist == NULL || *alist == NULL)
				bpf_error("unknown host '%s'", name);
			tproto = proto;
			if (off_linktype == (u_int)-1 && tproto == Q_DEFAULT)
				tproto = Q_IP;
			b = gen_host(**alist++, 0xffffffff, tproto, dir, q.addr);
			while (*alist) {
				tmp = gen_host(**alist++, 0xffffffff,
					       tproto, dir, q.addr);
				gen_or(b, tmp);
				b = tmp;
			}
			return b;
#else
			memset(&mask128, 0xff, sizeof(mask128));
			res0 = res = pcap_nametoaddrinfo(name);
			if (res == NULL)
				bpf_error("unknown host '%s'", name);
			ai = res;
			b = tmp = NULL;
			tproto = tproto6 = proto;
			if (off_linktype == -1 && tproto == Q_DEFAULT) {
				tproto = Q_IP;
				tproto6 = Q_IPV6;
			}
			for (res = res0; res; res = res->ai_next) {
				switch (res->ai_family) {
				case AF_INET:
					if (tproto == Q_IPV6)
						continue;

					sin4 = (struct sockaddr_in *)
						res->ai_addr;
					tmp = gen_host(ntohl(sin4->sin_addr.s_addr),
						0xffffffff, tproto, dir, q.addr);
					break;
				case AF_INET6:
					if (tproto6 == Q_IP)
						continue;

					sin6 = (struct sockaddr_in6 *)
						res->ai_addr;
					tmp = gen_host6(&sin6->sin6_addr,
						&mask128, tproto6, dir, q.addr);
					break;
				default:
					continue;
				}
				if (b)
					gen_or(b, tmp);
				b = tmp;
			}
			ai = NULL;
			freeaddrinfo(res0);
			if (b == NULL) {
				bpf_error("unknown host '%s'%s", name,
				    (proto == Q_DEFAULT)
					? ""
					: " for specified address family");
			}
			return b;
#endif 
		}

	case Q_PORT:
		if (proto != Q_DEFAULT &&
		    proto != Q_UDP && proto != Q_TCP && proto != Q_SCTP)
			bpf_error("illegal qualifier of 'port'");
		if (pcap_nametoport(name, &port, &real_proto) == 0)
			bpf_error("unknown port '%s'", name);
		if (proto == Q_UDP) {
			if (real_proto == IPPROTO_TCP)
				bpf_error("port '%s' is tcp", name);
			else if (real_proto == IPPROTO_SCTP)
				bpf_error("port '%s' is sctp", name);
			else
				
				real_proto = IPPROTO_UDP;
		}
		if (proto == Q_TCP) {
			if (real_proto == IPPROTO_UDP)
				bpf_error("port '%s' is udp", name);

			else if (real_proto == IPPROTO_SCTP)
				bpf_error("port '%s' is sctp", name);
			else
				
				real_proto = IPPROTO_TCP;
		}
		if (proto == Q_SCTP) {
			if (real_proto == IPPROTO_UDP)
				bpf_error("port '%s' is udp", name);

			else if (real_proto == IPPROTO_TCP)
				bpf_error("port '%s' is tcp", name);
			else
				
				real_proto = IPPROTO_SCTP;
		}
		if (port < 0)
			bpf_error("illegal port number %d < 0", port);
		if (port > 65535)
			bpf_error("illegal port number %d > 65535", port);
		b = gen_port(port, real_proto, dir);
		gen_or(gen_port6(port, real_proto, dir), b);
		return b;

	case Q_PORTRANGE:
		if (proto != Q_DEFAULT &&
		    proto != Q_UDP && proto != Q_TCP && proto != Q_SCTP)
			bpf_error("illegal qualifier of 'portrange'");
		if (pcap_nametoportrange(name, &port1, &port2, &real_proto) == 0) 
			bpf_error("unknown port in range '%s'", name);
		if (proto == Q_UDP) {
			if (real_proto == IPPROTO_TCP)
				bpf_error("port in range '%s' is tcp", name);
			else if (real_proto == IPPROTO_SCTP)
				bpf_error("port in range '%s' is sctp", name);
			else
				
				real_proto = IPPROTO_UDP;
		}
		if (proto == Q_TCP) {
			if (real_proto == IPPROTO_UDP)
				bpf_error("port in range '%s' is udp", name);
			else if (real_proto == IPPROTO_SCTP)
				bpf_error("port in range '%s' is sctp", name);
			else
				
				real_proto = IPPROTO_TCP;
		}
		if (proto == Q_SCTP) {
			if (real_proto == IPPROTO_UDP)
				bpf_error("port in range '%s' is udp", name);
			else if (real_proto == IPPROTO_TCP)
				bpf_error("port in range '%s' is tcp", name);
			else
				
				real_proto = IPPROTO_SCTP;	
		}
		if (port1 < 0)
			bpf_error("illegal port number %d < 0", port1);
		if (port1 > 65535)
			bpf_error("illegal port number %d > 65535", port1);
		if (port2 < 0)
			bpf_error("illegal port number %d < 0", port2);
		if (port2 > 65535)
			bpf_error("illegal port number %d > 65535", port2);

		b = gen_portrange(port1, port2, real_proto, dir);
		gen_or(gen_portrange6(port1, port2, real_proto, dir), b);
		return b;

	case Q_GATEWAY:
#ifndef INET6
		eaddr = pcap_ether_hostton(name);
		if (eaddr == NULL)
			bpf_error("unknown ether host: %s", name);

		alist = pcap_nametoaddr(name);
		if (alist == NULL || *alist == NULL)
			bpf_error("unknown host '%s'", name);
		b = gen_gateway(eaddr, alist, proto, dir);
		free(eaddr);
		return b;
#else
		bpf_error("'gateway' not supported in this configuration");
#endif 

	case Q_PROTO:
		real_proto = lookup_proto(name, proto);
		if (real_proto >= 0)
			return gen_proto(real_proto, proto, dir);
		else
			bpf_error("unknown protocol: %s", name);

	case Q_PROTOCHAIN:
		real_proto = lookup_proto(name, proto);
		if (real_proto >= 0)
			return gen_protochain(real_proto, proto, dir);
		else
			bpf_error("unknown protocol: %s", name);

	case Q_UNDEF:
		syntax();
		
	}
	abort();
	
}

struct block *
gen_mcode(s1, s2, masklen, q)
	register const char *s1, *s2;
	register int masklen;
	struct qual q;
{
	register int nlen, mlen;
	bpf_u_int32 n, m;

	nlen = __pcap_atoin(s1, &n);
	
	n <<= 32 - nlen;

	if (s2 != NULL) {
		mlen = __pcap_atoin(s2, &m);
		
		m <<= 32 - mlen;
		if ((n & ~m) != 0)
			bpf_error("non-network bits set in \"%s mask %s\"",
			    s1, s2);
	} else {
		
		if (masklen > 32)
			bpf_error("mask length must be <= 32");
		if (masklen == 0) {
			m = 0;
		} else
			m = 0xffffffff << (32 - masklen);
		if ((n & ~m) != 0)
			bpf_error("non-network bits set in \"%s/%d\"",
			    s1, masklen);
	}

	switch (q.addr) {

	case Q_NET:
		return gen_host(n, m, q.proto, q.dir, q.addr);

	default:
		bpf_error("Mask syntax for networks only");
		
	}
	
	return NULL;
}

struct block *
gen_ncode(s, v, q)
	register const char *s;
	bpf_u_int32 v;
	struct qual q;
{
	bpf_u_int32 mask;
	int proto = q.proto;
	int dir = q.dir;
	register int vlen;

	if (s == NULL)
		vlen = 32;
	else if (q.proto == Q_DECNET)
		vlen = __pcap_atodn(s, &v);
	else
		vlen = __pcap_atoin(s, &v);

	switch (q.addr) {

	case Q_DEFAULT:
	case Q_HOST:
	case Q_NET:
		if (proto == Q_DECNET)
			return gen_host(v, 0, proto, dir, q.addr);
		else if (proto == Q_LINK) {
			bpf_error("illegal link layer address");
		} else {
			mask = 0xffffffff;
			if (s == NULL && q.addr == Q_NET) {
				
				while (v && (v & 0xff000000) == 0) {
					v <<= 8;
					mask <<= 8;
				}
			} else {
				
				v <<= 32 - vlen;
				mask <<= 32 - vlen;
			}
			return gen_host(v, mask, proto, dir, q.addr);
		}

	case Q_PORT:
		if (proto == Q_UDP)
			proto = IPPROTO_UDP;
		else if (proto == Q_TCP)
			proto = IPPROTO_TCP;
		else if (proto == Q_SCTP)
			proto = IPPROTO_SCTP;
		else if (proto == Q_DEFAULT)
			proto = PROTO_UNDEF;
		else
			bpf_error("illegal qualifier of 'port'");

		if (v > 65535)
			bpf_error("illegal port number %u > 65535", v);

	    {
		struct block *b;
		b = gen_port((int)v, proto, dir);
		gen_or(gen_port6((int)v, proto, dir), b);
		return b;
	    }

	case Q_PORTRANGE:
		if (proto == Q_UDP)
			proto = IPPROTO_UDP;
		else if (proto == Q_TCP)
			proto = IPPROTO_TCP;
		else if (proto == Q_SCTP)
			proto = IPPROTO_SCTP;
		else if (proto == Q_DEFAULT)
			proto = PROTO_UNDEF;
		else
			bpf_error("illegal qualifier of 'portrange'");

		if (v > 65535)
			bpf_error("illegal port number %u > 65535", v);

	    {
		struct block *b;
		b = gen_portrange((int)v, (int)v, proto, dir);
		gen_or(gen_portrange6((int)v, (int)v, proto, dir), b);
		return b;
	    }

	case Q_GATEWAY:
		bpf_error("'gateway' requires a name");
		

	case Q_PROTO:
		return gen_proto((int)v, proto, dir);

	case Q_PROTOCHAIN:
		return gen_protochain((int)v, proto, dir);

	case Q_UNDEF:
		syntax();
		

	default:
		abort();
		
	}
	
}

#ifdef INET6
struct block *
gen_mcode6(s1, s2, masklen, q)
	register const char *s1, *s2;
	register int masklen;
	struct qual q;
{
	struct addrinfo *res;
	struct in6_addr *addr;
	struct in6_addr mask;
	struct block *b;
	u_int32_t *a, *m;

	if (s2)
		bpf_error("no mask %s supported", s2);

	res = pcap_nametoaddrinfo(s1);
	if (!res)
		bpf_error("invalid ip6 address %s", s1);
	ai = res;
	if (res->ai_next)
		bpf_error("%s resolved to multiple address", s1);
	addr = &((struct sockaddr_in6 *)res->ai_addr)->sin6_addr;

	if (sizeof(mask) * 8 < masklen)
		bpf_error("mask length must be <= %u", (unsigned int)(sizeof(mask) * 8));
	memset(&mask, 0, sizeof(mask));
	memset(&mask, 0xff, masklen / 8);
	if (masklen % 8) {
		mask.s6_addr[masklen / 8] =
			(0xff << (8 - masklen % 8)) & 0xff;
	}

	a = (u_int32_t *)addr;
	m = (u_int32_t *)&mask;
	if ((a[0] & ~m[0]) || (a[1] & ~m[1])
	 || (a[2] & ~m[2]) || (a[3] & ~m[3])) {
		bpf_error("non-network bits set in \"%s/%d\"", s1, masklen);
	}

	switch (q.addr) {

	case Q_DEFAULT:
	case Q_HOST:
		if (masklen != 128)
			bpf_error("Mask syntax for networks only");
		

	case Q_NET:
		b = gen_host6(addr, &mask, q.proto, q.dir, q.addr);
		ai = NULL;
		freeaddrinfo(res);
		return b;

	default:
		bpf_error("invalid qualifier against IPv6 address");
		
	}
	return NULL;
}
#endif 

struct block *
gen_ecode(eaddr, q)
	register const u_char *eaddr;
	struct qual q;
{
	struct block *b, *tmp;

	if ((q.addr == Q_HOST || q.addr == Q_DEFAULT) && q.proto == Q_LINK) {
		switch (linktype) {
		case DLT_EN10MB:
		case DLT_NETANALYZER:
		case DLT_NETANALYZER_TRANSPARENT:
			return gen_ehostop(eaddr, (int)q.dir);
		case DLT_FDDI:
			return gen_fhostop(eaddr, (int)q.dir);
		case DLT_IEEE802:
			return gen_thostop(eaddr, (int)q.dir);
		case DLT_IEEE802_11:
		case DLT_PRISM_HEADER:
		case DLT_IEEE802_11_RADIO_AVS:
		case DLT_IEEE802_11_RADIO:
		case DLT_PPI:
			return gen_wlanhostop(eaddr, (int)q.dir);
		case DLT_SUNATM:
			if (is_lane) {
				tmp = gen_cmp(OR_LINK, SUNATM_PKT_BEGIN_POS, BPF_H,
					0xFF00);
				gen_not(tmp);

				b = gen_ehostop(eaddr, (int)q.dir);
				gen_and(tmp, b);
				return b;
			}
			break;
		case DLT_IP_OVER_FC:
			return gen_ipfchostop(eaddr, (int)q.dir);
		default:
			bpf_error("ethernet addresses supported only on ethernet/FDDI/token ring/802.11/ATM LANE/Fibre Channel");
			break;
		}
	}
	bpf_error("ethernet address used in non-ether expression");
	
	return NULL;
}

void
sappend(s0, s1)
	struct slist *s0, *s1;
{
	while (s0->next)
		s0 = s0->next;
	s0->next = s1;
}

static struct slist *
xfer_to_x(a)
	struct arth *a;
{
	struct slist *s;

	s = new_stmt(BPF_LDX|BPF_MEM);
	s->s.k = a->regno;
	return s;
}

static struct slist *
xfer_to_a(a)
	struct arth *a;
{
	struct slist *s;

	s = new_stmt(BPF_LD|BPF_MEM);
	s->s.k = a->regno;
	return s;
}

struct arth *
gen_load(proto, inst, size)
	int proto;
	struct arth *inst;
	int size;
{
	struct slist *s, *tmp;
	struct block *b;
	int regno = alloc_reg();

	free_reg(inst->regno);
	switch (size) {

	default:
		bpf_error("data size must be 1, 2, or 4");

	case 1:
		size = BPF_B;
		break;

	case 2:
		size = BPF_H;
		break;

	case 4:
		size = BPF_W;
		break;
	}
	switch (proto) {
	default:
		bpf_error("unsupported index operation");

	case Q_RADIO:
		if (linktype != DLT_IEEE802_11_RADIO_AVS &&
		    linktype != DLT_IEEE802_11_RADIO &&
		    linktype != DLT_PRISM_HEADER)
			bpf_error("radio information not present in capture");

		s = xfer_to_x(inst);

		tmp = new_stmt(BPF_LD|BPF_IND|size);
		sappend(s, tmp);
		sappend(inst->s, s);
		break;

	case Q_LINK:
		s = gen_llprefixlen();

		if (s != NULL) {
			sappend(s, xfer_to_a(inst));
			sappend(s, new_stmt(BPF_ALU|BPF_ADD|BPF_X));
			sappend(s, new_stmt(BPF_MISC|BPF_TAX));
		} else
			s = xfer_to_x(inst);

		tmp = new_stmt(BPF_LD|BPF_IND|size);
		tmp->s.k = off_ll;
		sappend(s, tmp);
		sappend(inst->s, s);
		break;

	case Q_IP:
	case Q_ARP:
	case Q_RARP:
	case Q_ATALK:
	case Q_DECNET:
	case Q_SCA:
	case Q_LAT:
	case Q_MOPRC:
	case Q_MOPDL:
	case Q_IPV6:
		s = gen_off_macpl();

		if (s != NULL) {
			sappend(s, xfer_to_a(inst));
			sappend(s, new_stmt(BPF_ALU|BPF_ADD|BPF_X));
			sappend(s, new_stmt(BPF_MISC|BPF_TAX));
		} else
			s = xfer_to_x(inst);

		tmp = new_stmt(BPF_LD|BPF_IND|size);
		tmp->s.k = off_macpl + off_nl;
		sappend(s, tmp);
		sappend(inst->s, s);

		b = gen_proto_abbrev(proto);
		if (inst->b)
			gen_and(inst->b, b);
		inst->b = b;
		break;

	case Q_SCTP:
	case Q_TCP:
	case Q_UDP:
	case Q_ICMP:
	case Q_IGMP:
	case Q_IGRP:
	case Q_PIM:
	case Q_VRRP:
	case Q_CARP:
		s = gen_loadx_iphdrlen();

		sappend(s, xfer_to_a(inst));
		sappend(s, new_stmt(BPF_ALU|BPF_ADD|BPF_X));
		sappend(s, new_stmt(BPF_MISC|BPF_TAX));
		sappend(s, tmp = new_stmt(BPF_LD|BPF_IND|size));
		tmp->s.k = off_macpl + off_nl;
		sappend(inst->s, s);

		gen_and(gen_proto_abbrev(proto), b = gen_ipfrag());
		if (inst->b)
			gen_and(inst->b, b);
		gen_and(gen_proto_abbrev(Q_IP), b);
		inst->b = b;
		break;
	case Q_ICMPV6:
		bpf_error("IPv6 upper-layer protocol is not supported by proto[x]");
		
	}
	inst->regno = regno;
	s = new_stmt(BPF_ST);
	s->s.k = regno;
	sappend(inst->s, s);

	return inst;
}

struct block *
gen_relation(code, a0, a1, reversed)
	int code;
	struct arth *a0, *a1;
	int reversed;
{
	struct slist *s0, *s1, *s2;
	struct block *b, *tmp;

	s0 = xfer_to_x(a1);
	s1 = xfer_to_a(a0);
	if (code == BPF_JEQ) {
		s2 = new_stmt(BPF_ALU|BPF_SUB|BPF_X);
		b = new_block(JMP(code));
		sappend(s1, s2);
	}
	else
		b = new_block(BPF_JMP|code|BPF_X);
	if (reversed)
		gen_not(b);

	sappend(s0, s1);
	sappend(a1->s, s0);
	sappend(a0->s, a1->s);

	b->stmts = a0->s;

	free_reg(a0->regno);
	free_reg(a1->regno);

	
	if (a0->b) {
		if (a1->b) {
			gen_and(a0->b, tmp = a1->b);
		}
		else
			tmp = a0->b;
	} else
		tmp = a1->b;

	if (tmp)
		gen_and(tmp, b);

	return b;
}

struct arth *
gen_loadlen()
{
	int regno = alloc_reg();
	struct arth *a = (struct arth *)newchunk(sizeof(*a));
	struct slist *s;

	s = new_stmt(BPF_LD|BPF_LEN);
	s->next = new_stmt(BPF_ST);
	s->next->s.k = regno;
	a->s = s;
	a->regno = regno;

	return a;
}

struct arth *
gen_loadi(val)
	int val;
{
	struct arth *a;
	struct slist *s;
	int reg;

	a = (struct arth *)newchunk(sizeof(*a));

	reg = alloc_reg();

	s = new_stmt(BPF_LD|BPF_IMM);
	s->s.k = val;
	s->next = new_stmt(BPF_ST);
	s->next->s.k = reg;
	a->s = s;
	a->regno = reg;

	return a;
}

struct arth *
gen_neg(a)
	struct arth *a;
{
	struct slist *s;

	s = xfer_to_a(a);
	sappend(a->s, s);
	s = new_stmt(BPF_ALU|BPF_NEG);
	s->s.k = 0;
	sappend(a->s, s);
	s = new_stmt(BPF_ST);
	s->s.k = a->regno;
	sappend(a->s, s);

	return a;
}

struct arth *
gen_arth(code, a0, a1)
	int code;
	struct arth *a0, *a1;
{
	struct slist *s0, *s1, *s2;

	s0 = xfer_to_x(a1);
	s1 = xfer_to_a(a0);
	s2 = new_stmt(BPF_ALU|BPF_X|code);

	sappend(s1, s2);
	sappend(s0, s1);
	sappend(a1->s, s0);
	sappend(a0->s, a1->s);

	free_reg(a0->regno);
	free_reg(a1->regno);

	s0 = new_stmt(BPF_ST);
	a0->regno = s0->s.k = alloc_reg();
	sappend(a0->s, s0);

	return a0;
}

static int regused[BPF_MEMWORDS];
static int curreg;

static void
init_regs()
{
	curreg = 0;
	memset(regused, 0, sizeof regused);
}

static int
alloc_reg()
{
	int n = BPF_MEMWORDS;

	while (--n >= 0) {
		if (regused[curreg])
			curreg = (curreg + 1) % BPF_MEMWORDS;
		else {
			regused[curreg] = 1;
			return curreg;
		}
	}
	bpf_error("too many registers needed to evaluate expression");
	
	return 0;
}

static void
free_reg(n)
	int n;
{
	regused[n] = 0;
}

static struct block *
gen_len(jmp, n)
	int jmp, n;
{
	struct slist *s;
	struct block *b;

	s = new_stmt(BPF_LD|BPF_LEN);
	b = new_block(JMP(jmp));
	b->stmts = s;
	b->s.k = n;

	return b;
}

struct block *
gen_greater(n)
	int n;
{
	return gen_len(BPF_JGE, n);
}

struct block *
gen_less(n)
	int n;
{
	struct block *b;

	b = gen_len(BPF_JGT, n);
	gen_not(b);

	return b;
}

struct block *
gen_byteop(op, idx, val)
	int op, idx, val;
{
	struct block *b;
	struct slist *s;

	switch (op) {
	default:
		abort();

	case '=':
		return gen_cmp(OR_LINK, (u_int)idx, BPF_B, (bpf_int32)val);

	case '<':
		b = gen_cmp_lt(OR_LINK, (u_int)idx, BPF_B, (bpf_int32)val);
		return b;

	case '>':
		b = gen_cmp_gt(OR_LINK, (u_int)idx, BPF_B, (bpf_int32)val);
		return b;

	case '|':
		s = new_stmt(BPF_ALU|BPF_OR|BPF_K);
		break;

	case '&':
		s = new_stmt(BPF_ALU|BPF_AND|BPF_K);
		break;
	}
	s->s.k = val;
	b = new_block(JMP(BPF_JEQ));
	b->stmts = s;
	gen_not(b);

	return b;
}

static u_char abroadcast[] = { 0x0 };

struct block *
gen_broadcast(proto)
	int proto;
{
	bpf_u_int32 hostmask;
	struct block *b0, *b1, *b2;
	static u_char ebroadcast[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

	switch (proto) {

	case Q_DEFAULT:
	case Q_LINK:
		switch (linktype) {
		case DLT_ARCNET:
		case DLT_ARCNET_LINUX:
			return gen_ahostop(abroadcast, Q_DST);
		case DLT_EN10MB:
		case DLT_NETANALYZER:
		case DLT_NETANALYZER_TRANSPARENT:
			return gen_ehostop(ebroadcast, Q_DST);
		case DLT_FDDI:
			return gen_fhostop(ebroadcast, Q_DST);
		case DLT_IEEE802:
			return gen_thostop(ebroadcast, Q_DST);
		case DLT_IEEE802_11:
		case DLT_PRISM_HEADER:
		case DLT_IEEE802_11_RADIO_AVS:
		case DLT_IEEE802_11_RADIO:
		case DLT_PPI:
			return gen_wlanhostop(ebroadcast, Q_DST);
		case DLT_IP_OVER_FC:
			return gen_ipfchostop(ebroadcast, Q_DST);
		case DLT_SUNATM:
			if (is_lane) {
				b1 = gen_cmp(OR_LINK, SUNATM_PKT_BEGIN_POS,
				    BPF_H, 0xFF00);
				gen_not(b1);

				b0 = gen_ehostop(ebroadcast, Q_DST);
				gen_and(b1, b0);
				return b0;
			}
			break;
		default:
			bpf_error("not a broadcast link");
		}
		break;

	case Q_IP:
		if (netmask == PCAP_NETMASK_UNKNOWN)
			bpf_error("netmask not known, so 'ip broadcast' not supported");
		b0 = gen_linktype(ETHERTYPE_IP);
		hostmask = ~netmask;
		b1 = gen_mcmp(OR_NET, 16, BPF_W, (bpf_int32)0, hostmask);
		b2 = gen_mcmp(OR_NET, 16, BPF_W,
			      (bpf_int32)(~0 & hostmask), hostmask);
		gen_or(b1, b2);
		gen_and(b0, b2);
		return b2;
	}
	bpf_error("only link-layer/IP broadcast filters supported");
	
	return NULL;
}

static struct block *
gen_mac_multicast(offset)
	int offset;
{
	register struct block *b0;
	register struct slist *s;

	
	s = gen_load_a(OR_LINK, offset, BPF_B);
	b0 = new_block(JMP(BPF_JSET));
	b0->s.k = 1;
	b0->stmts = s;
	return b0;
}

struct block *
gen_multicast(proto)
	int proto;
{
	register struct block *b0, *b1, *b2;
	register struct slist *s;

	switch (proto) {

	case Q_DEFAULT:
	case Q_LINK:
		switch (linktype) {
		case DLT_ARCNET:
		case DLT_ARCNET_LINUX:
			
			return gen_ahostop(abroadcast, Q_DST);
		case DLT_EN10MB:
		case DLT_NETANALYZER:
		case DLT_NETANALYZER_TRANSPARENT:
			
			return gen_mac_multicast(0);
		case DLT_FDDI:
			
			return gen_mac_multicast(1);
		case DLT_IEEE802:
			
			return gen_mac_multicast(2);
		case DLT_IEEE802_11:
		case DLT_PRISM_HEADER:
		case DLT_IEEE802_11_RADIO_AVS:
		case DLT_IEEE802_11_RADIO:
		case DLT_PPI:

			s = gen_load_a(OR_LINK, 1, BPF_B);
			b1 = new_block(JMP(BPF_JSET));
			b1->s.k = 0x01;	
			b1->stmts = s;

			b0 = gen_mac_multicast(16);
			gen_and(b1, b0);

			s = gen_load_a(OR_LINK, 1, BPF_B);
			b2 = new_block(JMP(BPF_JSET));
			b2->s.k = 0x01;	
			b2->stmts = s;
			gen_not(b2);

			b1 = gen_mac_multicast(4);
			gen_and(b2, b1);

			gen_or(b1, b0);

			s = gen_load_a(OR_LINK, 0, BPF_B);
			b1 = new_block(JMP(BPF_JSET));
			b1->s.k = 0x08;
			b1->stmts = s;

			gen_and(b1, b0);

			s = gen_load_a(OR_LINK, 0, BPF_B);
			b2 = new_block(JMP(BPF_JSET));
			b2->s.k = 0x08;
			b2->stmts = s;
			gen_not(b2);

			b1 = gen_mac_multicast(4);
			gen_and(b2, b1);

			gen_or(b1, b0);

			s = gen_load_a(OR_LINK, 0, BPF_B);
			b1 = new_block(JMP(BPF_JSET));
			b1->s.k = 0x04;
			b1->stmts = s;
			gen_not(b1);

			gen_and(b1, b0);
			return b0;
		case DLT_IP_OVER_FC:
			b0 = gen_mac_multicast(2);
			return b0;
		case DLT_SUNATM:
			if (is_lane) {
				b1 = gen_cmp(OR_LINK, SUNATM_PKT_BEGIN_POS,
				    BPF_H, 0xFF00);
				gen_not(b1);

				
				b0 = gen_mac_multicast(off_mac);
				gen_and(b1, b0);
				return b0;
			}
			break;
		default:
			break;
		}
		
		break;

	case Q_IP:
		b0 = gen_linktype(ETHERTYPE_IP);
		b1 = gen_cmp_ge(OR_NET, 16, BPF_B, (bpf_int32)224);
		gen_and(b0, b1);
		return b1;

	case Q_IPV6:
		b0 = gen_linktype(ETHERTYPE_IPV6);
		b1 = gen_cmp(OR_NET, 24, BPF_B, (bpf_int32)255);
		gen_and(b0, b1);
		return b1;
	}
	bpf_error("link-layer multicast filters supported only on ethernet/FDDI/token ring/ARCNET/802.11/ATM LANE/Fibre Channel");
	
	return NULL;
}

struct block *
gen_inbound(dir)
	int dir;
{
	register struct block *b0;

	switch (linktype) {
	case DLT_SLIP:
		b0 = gen_relation(BPF_JEQ,
			  gen_load(Q_LINK, gen_loadi(0), 1),
			  gen_loadi(0),
			  dir);
		break;

	case DLT_IPNET:
		if (dir) {
			
			b0 = gen_cmp(OR_LINK, 2, BPF_H, IPNET_OUTBOUND);
		} else {
			
			b0 = gen_cmp(OR_LINK, 2, BPF_H, IPNET_INBOUND);
		}
		break;

	case DLT_LINUX_SLL:
		
		b0 = gen_cmp(OR_LINK, 0, BPF_H, LINUX_SLL_OUTGOING);
		if (!dir) {
			
			gen_not(b0);
		}
		break;

#ifdef HAVE_NET_PFVAR_H
	case DLT_PFLOG:
		b0 = gen_cmp(OR_LINK, offsetof(struct pfloghdr, dir), BPF_B,
		    (bpf_int32)((dir == 0) ? PF_IN : PF_OUT));
		break;
#endif

	case DLT_PPP_PPPD:
		if (dir) {
			
			b0 = gen_cmp(OR_LINK, 0, BPF_B, PPP_PPPD_OUT);
		} else {
			
			b0 = gen_cmp(OR_LINK, 0, BPF_B, PPP_PPPD_IN);
		}
		break;

        case DLT_JUNIPER_MFR:
        case DLT_JUNIPER_MLFR:
        case DLT_JUNIPER_MLPPP:
	case DLT_JUNIPER_ATM1:
	case DLT_JUNIPER_ATM2:
	case DLT_JUNIPER_PPPOE:
	case DLT_JUNIPER_PPPOE_ATM:
        case DLT_JUNIPER_GGSN:
        case DLT_JUNIPER_ES:
        case DLT_JUNIPER_MONITOR:
        case DLT_JUNIPER_SERVICES:
        case DLT_JUNIPER_ETHER:
        case DLT_JUNIPER_PPP:
        case DLT_JUNIPER_FRELAY:
        case DLT_JUNIPER_CHDLC:
        case DLT_JUNIPER_VP:
        case DLT_JUNIPER_ST:
        case DLT_JUNIPER_ISM:
        case DLT_JUNIPER_VS:
        case DLT_JUNIPER_SRX_E2E:
        case DLT_JUNIPER_FIBRECHANNEL:
	case DLT_JUNIPER_ATM_CEMIC:

		if (dir) {
			
			b0 = gen_mcmp(OR_LINK, 3, BPF_B, 0, 0x01);
		} else {
			
			b0 = gen_mcmp(OR_LINK, 3, BPF_B, 1, 0x01);
		}
		break;

	default:
#if defined(linux) && defined(PF_PACKET) && defined(SO_ATTACH_FILTER)
		if (bpf_pcap->rfile != NULL) {
			
			bpf_error("inbound/outbound not supported on linktype %d when reading savefiles",
			    linktype);
			b0 = NULL;
			
		}
		
		b0 = gen_cmp(OR_LINK, SKF_AD_OFF + SKF_AD_PKTTYPE, BPF_H,
		             PACKET_OUTGOING);
		if (!dir) {
			
			gen_not(b0);
		}
#else 
		bpf_error("inbound/outbound not supported on linktype %d",
		    linktype);
		b0 = NULL;
		
#endif 
	}
	return (b0);
}

#ifdef HAVE_NET_PFVAR_H
struct block *
gen_pf_ifname(const char *ifname)
{
	struct block *b0;
	u_int len, off;

	if (linktype != DLT_PFLOG) {
		bpf_error("ifname supported only on PF linktype");
		
	}
	len = sizeof(((struct pfloghdr *)0)->ifname);
	off = offsetof(struct pfloghdr, ifname);
	if (strlen(ifname) >= len) {
		bpf_error("ifname interface names can only be %d characters",
		    len-1);
		
	}
	b0 = gen_bcmp(OR_LINK, off, strlen(ifname), (const u_char *)ifname);
	return (b0);
}

struct block *
gen_pf_ruleset(char *ruleset)
{
	struct block *b0;

	if (linktype != DLT_PFLOG) {
		bpf_error("ruleset supported only on PF linktype");
		
	}

	if (strlen(ruleset) >= sizeof(((struct pfloghdr *)0)->ruleset)) {
		bpf_error("ruleset names can only be %ld characters",
		    (long)(sizeof(((struct pfloghdr *)0)->ruleset) - 1));
		
	}

	b0 = gen_bcmp(OR_LINK, offsetof(struct pfloghdr, ruleset),
	    strlen(ruleset), (const u_char *)ruleset);
	return (b0);
}

struct block *
gen_pf_rnr(int rnr)
{
	struct block *b0;

	if (linktype != DLT_PFLOG) {
		bpf_error("rnr supported only on PF linktype");
		
	}

	b0 = gen_cmp(OR_LINK, offsetof(struct pfloghdr, rulenr), BPF_W,
		 (bpf_int32)rnr);
	return (b0);
}

struct block *
gen_pf_srnr(int srnr)
{
	struct block *b0;

	if (linktype != DLT_PFLOG) {
		bpf_error("srnr supported only on PF linktype");
		
	}

	b0 = gen_cmp(OR_LINK, offsetof(struct pfloghdr, subrulenr), BPF_W,
	    (bpf_int32)srnr);
	return (b0);
}

struct block *
gen_pf_reason(int reason)
{
	struct block *b0;

	if (linktype != DLT_PFLOG) {
		bpf_error("reason supported only on PF linktype");
		
	}

	b0 = gen_cmp(OR_LINK, offsetof(struct pfloghdr, reason), BPF_B,
	    (bpf_int32)reason);
	return (b0);
}

struct block *
gen_pf_action(int action)
{
	struct block *b0;

	if (linktype != DLT_PFLOG) {
		bpf_error("action supported only on PF linktype");
		
	}

	b0 = gen_cmp(OR_LINK, offsetof(struct pfloghdr, action), BPF_B,
	    (bpf_int32)action);
	return (b0);
}
#else 
struct block *
gen_pf_ifname(const char *ifname)
{
	bpf_error("libpcap was compiled without pf support");
	
	return (NULL);
}

struct block *
gen_pf_ruleset(char *ruleset)
{
	bpf_error("libpcap was compiled on a machine without pf support");
	
	return (NULL);
}

struct block *
gen_pf_rnr(int rnr)
{
	bpf_error("libpcap was compiled on a machine without pf support");
	
	return (NULL);
}

struct block *
gen_pf_srnr(int srnr)
{
	bpf_error("libpcap was compiled on a machine without pf support");
	
	return (NULL);
}

struct block *
gen_pf_reason(int reason)
{
	bpf_error("libpcap was compiled on a machine without pf support");
	
	return (NULL);
}

struct block *
gen_pf_action(int action)
{
	bpf_error("libpcap was compiled on a machine without pf support");
	
	return (NULL);
}
#endif 

struct block *
gen_p80211_type(int type, int mask)
{
	struct block *b0;

	switch (linktype) {

	case DLT_IEEE802_11:
	case DLT_PRISM_HEADER:
	case DLT_IEEE802_11_RADIO_AVS:
	case DLT_IEEE802_11_RADIO:
		b0 = gen_mcmp(OR_LINK, 0, BPF_B, (bpf_int32)type,
		    (bpf_int32)mask);
		break;

	default:
		bpf_error("802.11 link-layer types supported only on 802.11");
		
	}

	return (b0);
}

struct block *
gen_p80211_fcdir(int fcdir)
{
	struct block *b0;

	switch (linktype) {

	case DLT_IEEE802_11:
	case DLT_PRISM_HEADER:
	case DLT_IEEE802_11_RADIO_AVS:
	case DLT_IEEE802_11_RADIO:
		break;

	default:
		bpf_error("frame direction supported only with 802.11 headers");
		
	}

	b0 = gen_mcmp(OR_LINK, 1, BPF_B, (bpf_int32)fcdir,
		(bpf_u_int32)IEEE80211_FC1_DIR_MASK);

	return (b0);
}

struct block *
gen_acode(eaddr, q)
	register const u_char *eaddr;
	struct qual q;
{
	switch (linktype) {

	case DLT_ARCNET:
	case DLT_ARCNET_LINUX:
		if ((q.addr == Q_HOST || q.addr == Q_DEFAULT) &&
		    q.proto == Q_LINK)
			return (gen_ahostop(eaddr, (int)q.dir));
		else {
			bpf_error("ARCnet address used in non-arc expression");
			
		}
		break;

	default:
		bpf_error("aid supported only on ARCnet");
		
	}
	bpf_error("ARCnet address used in non-arc expression");
	
	return NULL;
}

static struct block *
gen_ahostop(eaddr, dir)
	register const u_char *eaddr;
	register int dir;
{
	register struct block *b0, *b1;

	switch (dir) {
	
	case Q_SRC:
		return gen_bcmp(OR_LINK, 0, 1, eaddr);

	case Q_DST:
		return gen_bcmp(OR_LINK, 1, 1, eaddr);

	case Q_AND:
		b0 = gen_ahostop(eaddr, Q_SRC);
		b1 = gen_ahostop(eaddr, Q_DST);
		gen_and(b0, b1);
		return b1;

	case Q_DEFAULT:
	case Q_OR:
		b0 = gen_ahostop(eaddr, Q_SRC);
		b1 = gen_ahostop(eaddr, Q_DST);
		gen_or(b0, b1);
		return b1;

	case Q_ADDR1:
		bpf_error("'addr1' is only supported on 802.11");
		break;

	case Q_ADDR2:
		bpf_error("'addr2' is only supported on 802.11");
		break;

	case Q_ADDR3:
		bpf_error("'addr3' is only supported on 802.11");
		break;

	case Q_ADDR4:
		bpf_error("'addr4' is only supported on 802.11");
		break;

	case Q_RA:
		bpf_error("'ra' is only supported on 802.11");
		break;

	case Q_TA:
		bpf_error("'ta' is only supported on 802.11");
		break;
	}
	abort();
	
}

struct block *
gen_vlan(vlan_num)
	int vlan_num;
{
	struct	block	*b0, *b1;

	
	if (label_stack_depth > 0)
		bpf_error("no VLAN match after MPLS");

	orig_nl = off_nl;

	switch (linktype) {

	case DLT_EN10MB:
	case DLT_NETANALYZER:
	case DLT_NETANALYZER_TRANSPARENT:
		
		b0 = gen_cmp(OR_LINK, off_linktype, BPF_H,
		    (bpf_int32)ETHERTYPE_8021Q);
		b1 = gen_cmp(OR_LINK, off_linktype, BPF_H,
		    (bpf_int32)ETHERTYPE_8021QINQ);
		gen_or(b0,b1);
		b0 = b1;

		
		if (vlan_num >= 0) {
			b1 = gen_mcmp(OR_MACPL, 0, BPF_H,
			    (bpf_int32)vlan_num, 0x0fff);
			gen_and(b0, b1);
			b0 = b1;
		}

		off_macpl += 4;
		off_linktype += 4;
#if 0
		off_nl_nosnap += 4;
		off_nl += 4;
#endif
		break;

	default:
		bpf_error("no VLAN support for data link type %d",
		      linktype);
		
	}

	return (b0);
}

struct block *
gen_mpls(label_num)
	int label_num;
{
	struct	block	*b0,*b1;

        orig_nl = off_nl;

        if (label_stack_depth > 0) {
            
            b0 = gen_mcmp(OR_MACPL, orig_nl-2, BPF_B, 0, 0x01);
        } else {
            switch (linktype) {
                
            case DLT_C_HDLC: 
            case DLT_EN10MB:
            case DLT_NETANALYZER:
            case DLT_NETANALYZER_TRANSPARENT:
                    b0 = gen_linktype(ETHERTYPE_MPLS);
                    break;
                
            case DLT_PPP:
                    b0 = gen_linktype(PPP_MPLS_UCAST);
                    break;
                
                
            default:
                    bpf_error("no MPLS support for data link type %d",
                          linktype);
                    b0 = NULL;
                    
                    break;
            }
        }

	
	if (label_num >= 0) {
		label_num = label_num << 12; 
		b1 = gen_mcmp(OR_MACPL, orig_nl, BPF_W, (bpf_int32)label_num,
		    0xfffff000); 
		gen_and(b0, b1);
		b0 = b1;
	}

        off_nl_nosnap += 4;
        off_nl += 4;
        label_stack_depth++;
	return (b0);
}

struct block *
gen_pppoed()
{
	
	return gen_linktype((bpf_int32)ETHERTYPE_PPPOED);
}

struct block *
gen_pppoes(sess_num)
	int sess_num;
{
	struct block *b0, *b1;

	b0 = gen_linktype((bpf_int32)ETHERTYPE_PPPOES);

	orig_linktype = off_linktype;	
	orig_nl = off_nl;
	is_pppoes = 1;

	
	if (sess_num >= 0) {
		b1 = gen_mcmp(OR_MACPL, orig_nl, BPF_W,
		    (bpf_int32)sess_num, 0x0000ffff);
		gen_and(b0, b1);
		b0 = b1;
	}

	off_linktype = off_nl + 6;

	off_nl = 6+2;
	off_nl_nosnap = 6+2;

	return b0;
}

struct block *
gen_atmfield_code(atmfield, jvalue, jtype, reverse)
	int atmfield;
	bpf_int32 jvalue;
	bpf_u_int32 jtype;
	int reverse;
{
	struct block *b0;

	switch (atmfield) {

	case A_VPI:
		if (!is_atm)
			bpf_error("'vpi' supported only on raw ATM");
		if (off_vpi == (u_int)-1)
			abort();
		b0 = gen_ncmp(OR_LINK, off_vpi, BPF_B, 0xffffffff, jtype,
		    reverse, jvalue);
		break;

	case A_VCI:
		if (!is_atm)
			bpf_error("'vci' supported only on raw ATM");
		if (off_vci == (u_int)-1)
			abort();
		b0 = gen_ncmp(OR_LINK, off_vci, BPF_H, 0xffffffff, jtype,
		    reverse, jvalue);
		break;

	case A_PROTOTYPE:
		if (off_proto == (u_int)-1)
			abort();	
		b0 = gen_ncmp(OR_LINK, off_proto, BPF_B, 0x0f, jtype,
		    reverse, jvalue);
		break;

	case A_MSGTYPE:
		if (off_payload == (u_int)-1)
			abort();
		b0 = gen_ncmp(OR_LINK, off_payload + MSG_TYPE_POS, BPF_B,
		    0xffffffff, jtype, reverse, jvalue);
		break;

	case A_CALLREFTYPE:
		if (!is_atm)
			bpf_error("'callref' supported only on raw ATM");
		if (off_proto == (u_int)-1)
			abort();
		b0 = gen_ncmp(OR_LINK, off_proto, BPF_B, 0xffffffff,
		    jtype, reverse, jvalue);
		break;

	default:
		abort();
	}
	return b0;
}

struct block *
gen_atmtype_abbrev(type)
	int type;
{
	struct block *b0, *b1;

	switch (type) {

	case A_METAC:
		
		if (!is_atm)
			bpf_error("'metac' supported only on raw ATM");
		b0 = gen_atmfield_code(A_VPI, 0, BPF_JEQ, 0);
		b1 = gen_atmfield_code(A_VCI, 1, BPF_JEQ, 0);
		gen_and(b0, b1);
		break;

	case A_BCC:
		
		if (!is_atm)
			bpf_error("'bcc' supported only on raw ATM");
		b0 = gen_atmfield_code(A_VPI, 0, BPF_JEQ, 0);
		b1 = gen_atmfield_code(A_VCI, 2, BPF_JEQ, 0);
		gen_and(b0, b1);
		break;

	case A_OAMF4SC:
		
		if (!is_atm)
			bpf_error("'oam4sc' supported only on raw ATM");
		b0 = gen_atmfield_code(A_VPI, 0, BPF_JEQ, 0);
		b1 = gen_atmfield_code(A_VCI, 3, BPF_JEQ, 0);
		gen_and(b0, b1);
		break;

	case A_OAMF4EC:
		
		if (!is_atm)
			bpf_error("'oam4ec' supported only on raw ATM");
		b0 = gen_atmfield_code(A_VPI, 0, BPF_JEQ, 0);
		b1 = gen_atmfield_code(A_VCI, 4, BPF_JEQ, 0);
		gen_and(b0, b1);
		break;

	case A_SC:
		
		if (!is_atm)
			bpf_error("'sc' supported only on raw ATM");
		b0 = gen_atmfield_code(A_VPI, 0, BPF_JEQ, 0);
		b1 = gen_atmfield_code(A_VCI, 5, BPF_JEQ, 0);
		gen_and(b0, b1);
		break;

	case A_ILMIC:
		
		if (!is_atm)
			bpf_error("'ilmic' supported only on raw ATM");
		b0 = gen_atmfield_code(A_VPI, 0, BPF_JEQ, 0);
		b1 = gen_atmfield_code(A_VCI, 16, BPF_JEQ, 0);
		gen_and(b0, b1);
		break;

	case A_LANE:
		
		if (!is_atm)
			bpf_error("'lane' supported only on raw ATM");
		b1 = gen_atmfield_code(A_PROTOTYPE, PT_LANE, BPF_JEQ, 0);

		is_lane = 1;
		off_mac = off_payload + 2;	
		off_linktype = off_mac + 12;
		off_macpl = off_mac + 14;	
		off_nl = 0;			
		off_nl_nosnap = 3;		
		break;

	case A_LLC:
		
		if (!is_atm)
			bpf_error("'llc' supported only on raw ATM");
		b1 = gen_atmfield_code(A_PROTOTYPE, PT_LLC, BPF_JEQ, 0);
		is_lane = 0;
		break;

	default:
		abort();
	}
	return b1;
}

struct block *
gen_mtp2type_abbrev(type)
	int type;
{
	struct block *b0, *b1;

	switch (type) {

	case M_FISU:
		if ( (linktype != DLT_MTP2) &&
		     (linktype != DLT_ERF) &&
		     (linktype != DLT_MTP2_WITH_PHDR) )
			bpf_error("'fisu' supported only on MTP2");
		
		b0 = gen_ncmp(OR_PACKET, off_li, BPF_B, 0x3f, BPF_JEQ, 0, 0);
		break;

	case M_LSSU:
		if ( (linktype != DLT_MTP2) &&
		     (linktype != DLT_ERF) &&
		     (linktype != DLT_MTP2_WITH_PHDR) )
			bpf_error("'lssu' supported only on MTP2");
		b0 = gen_ncmp(OR_PACKET, off_li, BPF_B, 0x3f, BPF_JGT, 1, 2);
		b1 = gen_ncmp(OR_PACKET, off_li, BPF_B, 0x3f, BPF_JGT, 0, 0);
		gen_and(b1, b0);
		break;

	case M_MSU:
		if ( (linktype != DLT_MTP2) &&
		     (linktype != DLT_ERF) &&
		     (linktype != DLT_MTP2_WITH_PHDR) )
			bpf_error("'msu' supported only on MTP2");
		b0 = gen_ncmp(OR_PACKET, off_li, BPF_B, 0x3f, BPF_JGT, 0, 2);
		break;

	case MH_FISU:
		if ( (linktype != DLT_MTP2) &&
		     (linktype != DLT_ERF) &&
		     (linktype != DLT_MTP2_WITH_PHDR) )
			bpf_error("'hfisu' supported only on MTP2_HSL");
		
		b0 = gen_ncmp(OR_PACKET, off_li_hsl, BPF_H, 0xff80, BPF_JEQ, 0, 0);
		break;

	case MH_LSSU:
		if ( (linktype != DLT_MTP2) &&
		     (linktype != DLT_ERF) &&
		     (linktype != DLT_MTP2_WITH_PHDR) )
			bpf_error("'hlssu' supported only on MTP2_HSL");
		b0 = gen_ncmp(OR_PACKET, off_li_hsl, BPF_H, 0xff80, BPF_JGT, 1, 0x0100);
		b1 = gen_ncmp(OR_PACKET, off_li_hsl, BPF_H, 0xff80, BPF_JGT, 0, 0);
		gen_and(b1, b0);
		break;

	case MH_MSU:
		if ( (linktype != DLT_MTP2) &&
		     (linktype != DLT_ERF) &&
		     (linktype != DLT_MTP2_WITH_PHDR) )
			bpf_error("'hmsu' supported only on MTP2_HSL");
		b0 = gen_ncmp(OR_PACKET, off_li_hsl, BPF_H, 0xff80, BPF_JGT, 0, 0x0100);
		break;

	default:
		abort();
	}
	return b0;
}

struct block *
gen_mtp3field_code(mtp3field, jvalue, jtype, reverse)
	int mtp3field;
	bpf_u_int32 jvalue;
	bpf_u_int32 jtype;
	int reverse;
{
	struct block *b0;
	bpf_u_int32 val1 , val2 , val3;
	u_int newoff_sio=off_sio;
	u_int newoff_opc=off_opc;
	u_int newoff_dpc=off_dpc;
	u_int newoff_sls=off_sls;

	switch (mtp3field) {

	case MH_SIO:
		newoff_sio += 3; 
		

	case M_SIO:
		if (off_sio == (u_int)-1)
			bpf_error("'sio' supported only on SS7");
		
		if(jvalue > 255)
		        bpf_error("sio value %u too big; max value = 255",
		            jvalue);
		b0 = gen_ncmp(OR_PACKET, newoff_sio, BPF_B, 0xffffffff,
		    (u_int)jtype, reverse, (u_int)jvalue);
		break;

	case MH_OPC:
		newoff_opc+=3;
        case M_OPC:
	        if (off_opc == (u_int)-1)
			bpf_error("'opc' supported only on SS7");
		
		if (jvalue > 16383)
		        bpf_error("opc value %u too big; max value = 16383",
		            jvalue);
		val1 = jvalue & 0x00003c00;
		val1 = val1 >>10;
		val2 = jvalue & 0x000003fc;
		val2 = val2 <<6;
		val3 = jvalue & 0x00000003;
		val3 = val3 <<22;
		jvalue = val1 + val2 + val3;
		b0 = gen_ncmp(OR_PACKET, newoff_opc, BPF_W, 0x00c0ff0f,
		    (u_int)jtype, reverse, (u_int)jvalue);
		break;

	case MH_DPC:
		newoff_dpc += 3;
		

	case M_DPC:
	        if (off_dpc == (u_int)-1)
			bpf_error("'dpc' supported only on SS7");
		
		if (jvalue > 16383)
		        bpf_error("dpc value %u too big; max value = 16383",
		            jvalue);
		val1 = jvalue & 0x000000ff;
		val1 = val1 << 24;
		val2 = jvalue & 0x00003f00;
		val2 = val2 << 8;
		jvalue = val1 + val2;
		b0 = gen_ncmp(OR_PACKET, newoff_dpc, BPF_W, 0xff3f0000,
		    (u_int)jtype, reverse, (u_int)jvalue);
		break;

	case MH_SLS:
	  newoff_sls+=3;
	case M_SLS:
	        if (off_sls == (u_int)-1)
			bpf_error("'sls' supported only on SS7");
		
		if (jvalue > 15)
		         bpf_error("sls value %u too big; max value = 15",
		             jvalue);
		jvalue = jvalue << 4;
		b0 = gen_ncmp(OR_PACKET, newoff_sls, BPF_B, 0xf0,
		    (u_int)jtype,reverse, (u_int)jvalue);
		break;

	default:
		abort();
	}
	return b0;
}

static struct block *
gen_msg_abbrev(type)
	int type;
{
	struct block *b1;

	switch (type) {

	case A_SETUP:
		b1 = gen_atmfield_code(A_MSGTYPE, SETUP, BPF_JEQ, 0);
		break;

	case A_CALLPROCEED:
		b1 = gen_atmfield_code(A_MSGTYPE, CALL_PROCEED, BPF_JEQ, 0);
		break;

	case A_CONNECT:
		b1 = gen_atmfield_code(A_MSGTYPE, CONNECT, BPF_JEQ, 0);
		break;

	case A_CONNECTACK:
		b1 = gen_atmfield_code(A_MSGTYPE, CONNECT_ACK, BPF_JEQ, 0);
		break;

	case A_RELEASE:
		b1 = gen_atmfield_code(A_MSGTYPE, RELEASE, BPF_JEQ, 0);
		break;

	case A_RELEASE_DONE:
		b1 = gen_atmfield_code(A_MSGTYPE, RELEASE_DONE, BPF_JEQ, 0);
		break;

	default:
		abort();
	}
	return b1;
}

struct block *
gen_atmmulti_abbrev(type)
	int type;
{
	struct block *b0, *b1;

	switch (type) {

	case A_OAM:
		if (!is_atm)
			bpf_error("'oam' supported only on raw ATM");
		b1 = gen_atmmulti_abbrev(A_OAMF4);
		break;

	case A_OAMF4:
		if (!is_atm)
			bpf_error("'oamf4' supported only on raw ATM");
		
		b0 = gen_atmfield_code(A_VCI, 3, BPF_JEQ, 0);
		b1 = gen_atmfield_code(A_VCI, 4, BPF_JEQ, 0);
		gen_or(b0, b1);
		b0 = gen_atmfield_code(A_VPI, 0, BPF_JEQ, 0);
		gen_and(b0, b1);
		break;

	case A_CONNECTMSG:
		if (!is_atm)
			bpf_error("'connectmsg' supported only on raw ATM");
		b0 = gen_msg_abbrev(A_SETUP);
		b1 = gen_msg_abbrev(A_CALLPROCEED);
		gen_or(b0, b1);
		b0 = gen_msg_abbrev(A_CONNECT);
		gen_or(b0, b1);
		b0 = gen_msg_abbrev(A_CONNECTACK);
		gen_or(b0, b1);
		b0 = gen_msg_abbrev(A_RELEASE);
		gen_or(b0, b1);
		b0 = gen_msg_abbrev(A_RELEASE_DONE);
		gen_or(b0, b1);
		b0 = gen_atmtype_abbrev(A_SC);
		gen_and(b0, b1);
		break;

	case A_METACONNECT:
		if (!is_atm)
			bpf_error("'metaconnect' supported only on raw ATM");
		b0 = gen_msg_abbrev(A_SETUP);
		b1 = gen_msg_abbrev(A_CALLPROCEED);
		gen_or(b0, b1);
		b0 = gen_msg_abbrev(A_CONNECT);
		gen_or(b0, b1);
		b0 = gen_msg_abbrev(A_RELEASE);
		gen_or(b0, b1);
		b0 = gen_msg_abbrev(A_RELEASE_DONE);
		gen_or(b0, b1);
		b0 = gen_atmtype_abbrev(A_METAC);
		gen_and(b0, b1);
		break;

	default:
		abort();
	}
	return b1;
}
