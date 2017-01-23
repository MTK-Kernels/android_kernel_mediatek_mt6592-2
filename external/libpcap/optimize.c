/*
 * Copyright (c) 1988, 1989, 1990, 1991, 1993, 1994, 1995, 1996
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
 *  Optimization module for tcpdump intermediate representation.
 */
#ifndef lint
static const char rcsid[] _U_ =
    "@(#) $Header: /tcpdump/master/libpcap/optimize.c,v 1.91 2008-01-02 04:16:46 guy Exp $ (LBL)";
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
#endif 

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>

#include <errno.h>

#include "pcap-int.h"

#include "gencode.h"

#ifdef HAVE_OS_PROTO_H
#include "os-proto.h"
#endif

#ifdef BDEBUG
extern int dflag;
#endif

#if defined(MSDOS) && !defined(__DJGPP__)
extern int _w32_ffs (int mask);
#define ffs _w32_ffs
#endif

#if defined(WIN32) && defined (_MSC_VER)
int ffs(int mask);
#endif

#define NOP -1

#define A_ATOM BPF_MEMWORDS
#define X_ATOM (BPF_MEMWORDS+1)

#define AX_ATOM N_ATOMS

static int done;

static int cur_mark;
#define isMarked(p) ((p)->mark == cur_mark)
#define unMarkAll() cur_mark += 1
#define Mark(p) ((p)->mark = cur_mark)

static void opt_init(struct block *);
static void opt_cleanup(void);

static void intern_blocks(struct block *);

static void find_inedges(struct block *);
#ifdef BDEBUG
static void opt_dump(struct block *);
#endif

static int n_blocks;
struct block **blocks;
static int n_edges;
struct edge **edges;

static int nodewords;
static int edgewords;
struct block **levels;
bpf_u_int32 *space;
#define BITS_PER_WORD (8*sizeof(bpf_u_int32))
#define SET_MEMBER(p, a) \
((p)[(unsigned)(a) / BITS_PER_WORD] & (1 << ((unsigned)(a) % BITS_PER_WORD)))

#define SET_INSERT(p, a) \
(p)[(unsigned)(a) / BITS_PER_WORD] |= (1 << ((unsigned)(a) % BITS_PER_WORD))

#define SET_DELETE(p, a) \
(p)[(unsigned)(a) / BITS_PER_WORD] &= ~(1 << ((unsigned)(a) % BITS_PER_WORD))

#define SET_INTERSECT(a, b, n)\
{\
	register bpf_u_int32 *_x = a, *_y = b;\
	register int _n = n;\
	while (--_n >= 0) *_x++ &= *_y++;\
}

#define SET_SUBTRACT(a, b, n)\
{\
	register bpf_u_int32 *_x = a, *_y = b;\
	register int _n = n;\
	while (--_n >= 0) *_x++ &=~ *_y++;\
}

#define SET_UNION(a, b, n)\
{\
	register bpf_u_int32 *_x = a, *_y = b;\
	register int _n = n;\
	while (--_n >= 0) *_x++ |= *_y++;\
}

static uset all_dom_sets;
static uset all_closure_sets;
static uset all_edge_sets;

#ifndef MAX
#define MAX(a,b) ((a)>(b)?(a):(b))
#endif

static void
find_levels_r(struct block *b)
{
	int level;

	if (isMarked(b))
		return;

	Mark(b);
	b->link = 0;

	if (JT(b)) {
		find_levels_r(JT(b));
		find_levels_r(JF(b));
		level = MAX(JT(b)->level, JF(b)->level) + 1;
	} else
		level = 0;
	b->level = level;
	b->link = levels[level];
	levels[level] = b;
}

static void
find_levels(struct block *root)
{
	memset((char *)levels, 0, n_blocks * sizeof(*levels));
	unMarkAll();
	find_levels_r(root);
}

static void
find_dom(struct block *root)
{
	int i;
	struct block *b;
	bpf_u_int32 *x;

	x = all_dom_sets;
	i = n_blocks * nodewords;
	while (--i >= 0)
		*x++ = ~0;
	
	for (i = nodewords; --i >= 0;)
		root->dom[i] = 0;

	
	for (i = root->level; i >= 0; --i) {
		for (b = levels[i]; b; b = b->link) {
			SET_INSERT(b->dom, b->id);
			if (JT(b) == 0)
				continue;
			SET_INTERSECT(JT(b)->dom, b->dom, nodewords);
			SET_INTERSECT(JF(b)->dom, b->dom, nodewords);
		}
	}
}

static void
propedom(struct edge *ep)
{
	SET_INSERT(ep->edom, ep->id);
	if (ep->succ) {
		SET_INTERSECT(ep->succ->et.edom, ep->edom, edgewords);
		SET_INTERSECT(ep->succ->ef.edom, ep->edom, edgewords);
	}
}

static void
find_edom(struct block *root)
{
	int i;
	uset x;
	struct block *b;

	x = all_edge_sets;
	for (i = n_edges * edgewords; --i >= 0; )
		x[i] = ~0;

	
	memset(root->et.edom, 0, edgewords * sizeof(*(uset)0));
	memset(root->ef.edom, 0, edgewords * sizeof(*(uset)0));
	for (i = root->level; i >= 0; --i) {
		for (b = levels[i]; b != 0; b = b->link) {
			propedom(&b->et);
			propedom(&b->ef);
		}
	}
}

static void
find_closure(struct block *root)
{
	int i;
	struct block *b;

	memset((char *)all_closure_sets, 0,
	      n_blocks * nodewords * sizeof(*all_closure_sets));

	
	for (i = root->level; i >= 0; --i) {
		for (b = levels[i]; b; b = b->link) {
			SET_INSERT(b->closure, b->id);
			if (JT(b) == 0)
				continue;
			SET_UNION(JT(b)->closure, b->closure, nodewords);
			SET_UNION(JF(b)->closure, b->closure, nodewords);
		}
	}
}

static int
atomuse(struct stmt *s)
{
	register int c = s->code;

	if (c == NOP)
		return -1;

	switch (BPF_CLASS(c)) {

	case BPF_RET:
		return (BPF_RVAL(c) == BPF_A) ? A_ATOM :
			(BPF_RVAL(c) == BPF_X) ? X_ATOM : -1;

	case BPF_LD:
	case BPF_LDX:
		return (BPF_MODE(c) == BPF_IND) ? X_ATOM :
			(BPF_MODE(c) == BPF_MEM) ? s->k : -1;

	case BPF_ST:
		return A_ATOM;

	case BPF_STX:
		return X_ATOM;

	case BPF_JMP:
	case BPF_ALU:
		if (BPF_SRC(c) == BPF_X)
			return AX_ATOM;
		return A_ATOM;

	case BPF_MISC:
		return BPF_MISCOP(c) == BPF_TXA ? X_ATOM : A_ATOM;
	}
	abort();
	
}

static int
atomdef(struct stmt *s)
{
	if (s->code == NOP)
		return -1;

	switch (BPF_CLASS(s->code)) {

	case BPF_LD:
	case BPF_ALU:
		return A_ATOM;

	case BPF_LDX:
		return X_ATOM;

	case BPF_ST:
	case BPF_STX:
		return s->k;

	case BPF_MISC:
		return BPF_MISCOP(s->code) == BPF_TAX ? X_ATOM : A_ATOM;
	}
	return -1;
}

static void
compute_local_ud(struct block *b)
{
	struct slist *s;
	atomset def = 0, use = 0, kill = 0;
	int atom;

	for (s = b->stmts; s; s = s->next) {
		if (s->s.code == NOP)
			continue;
		atom = atomuse(&s->s);
		if (atom >= 0) {
			if (atom == AX_ATOM) {
				if (!ATOMELEM(def, X_ATOM))
					use |= ATOMMASK(X_ATOM);
				if (!ATOMELEM(def, A_ATOM))
					use |= ATOMMASK(A_ATOM);
			}
			else if (atom < N_ATOMS) {
				if (!ATOMELEM(def, atom))
					use |= ATOMMASK(atom);
			}
			else
				abort();
		}
		atom = atomdef(&s->s);
		if (atom >= 0) {
			if (!ATOMELEM(use, atom))
				kill |= ATOMMASK(atom);
			def |= ATOMMASK(atom);
		}
	}
	if (BPF_CLASS(b->s.code) == BPF_JMP) {
		atom = atomuse(&b->s);
		if (atom >= 0) {
			if (atom == AX_ATOM) {
				if (!ATOMELEM(def, X_ATOM))
					use |= ATOMMASK(X_ATOM);
				if (!ATOMELEM(def, A_ATOM))
					use |= ATOMMASK(A_ATOM);
			}
			else if (atom < N_ATOMS) {
				if (!ATOMELEM(def, atom))
					use |= ATOMMASK(atom);
			}
			else
				abort();
		}
	}

	b->def = def;
	b->kill = kill;
	b->in_use = use;
}

static void
find_ud(struct block *root)
{
	int i, maxlevel;
	struct block *p;

	maxlevel = root->level;
	for (i = maxlevel; i >= 0; --i)
		for (p = levels[i]; p; p = p->link) {
			compute_local_ud(p);
			p->out_use = 0;
		}

	for (i = 1; i <= maxlevel; ++i) {
		for (p = levels[i]; p; p = p->link) {
			p->out_use |= JT(p)->in_use | JF(p)->in_use;
			p->in_use |= p->out_use &~ p->kill;
		}
	}
}

struct valnode {
	int code;
	int v0, v1;
	int val;
	struct valnode *next;
};

#define MODULUS 213
static struct valnode *hashtbl[MODULUS];
static int curval;
static int maxval;

#define K(i) F(BPF_LD|BPF_IMM|BPF_W, i, 0L)

struct vmapinfo {
	int is_const;
	bpf_int32 const_val;
};

struct vmapinfo *vmap;
struct valnode *vnode_base;
struct valnode *next_vnode;

static void
init_val(void)
{
	curval = 0;
	next_vnode = vnode_base;
	memset((char *)vmap, 0, maxval * sizeof(*vmap));
	memset((char *)hashtbl, 0, sizeof hashtbl);
}

static int
F(int code, int v0, int v1)
{
	u_int hash;
	int val;
	struct valnode *p;

	hash = (u_int)code ^ (v0 << 4) ^ (v1 << 8);
	hash %= MODULUS;

	for (p = hashtbl[hash]; p; p = p->next)
		if (p->code == code && p->v0 == v0 && p->v1 == v1)
			return p->val;

	val = ++curval;
	if (BPF_MODE(code) == BPF_IMM &&
	    (BPF_CLASS(code) == BPF_LD || BPF_CLASS(code) == BPF_LDX)) {
		vmap[val].const_val = v0;
		vmap[val].is_const = 1;
	}
	p = next_vnode++;
	p->val = val;
	p->code = code;
	p->v0 = v0;
	p->v1 = v1;
	p->next = hashtbl[hash];
	hashtbl[hash] = p;

	return val;
}

static inline void
vstore(struct stmt *s, int *valp, int newval, int alter)
{
	if (alter && *valp == newval)
		s->code = NOP;
	else
		*valp = newval;
}

static void
fold_op(struct stmt *s, int v0, int v1)
{
	bpf_u_int32 a, b;

	a = vmap[v0].const_val;
	b = vmap[v1].const_val;

	switch (BPF_OP(s->code)) {
	case BPF_ADD:
		a += b;
		break;

	case BPF_SUB:
		a -= b;
		break;

	case BPF_MUL:
		a *= b;
		break;

	case BPF_DIV:
		if (b == 0)
			bpf_error("division by zero");
		a /= b;
		break;

	case BPF_AND:
		a &= b;
		break;

	case BPF_OR:
		a |= b;
		break;

	case BPF_LSH:
		a <<= b;
		break;

	case BPF_RSH:
		a >>= b;
		break;

	default:
		abort();
	}
	s->k = a;
	s->code = BPF_LD|BPF_IMM;
	done = 0;
}

static inline struct slist *
this_op(struct slist *s)
{
	while (s != 0 && s->s.code == NOP)
		s = s->next;
	return s;
}

static void
opt_not(struct block *b)
{
	struct block *tmp = JT(b);

	JT(b) = JF(b);
	JF(b) = tmp;
}

static void
opt_peep(struct block *b)
{
	struct slist *s;
	struct slist *next, *last;
	int val;

	s = b->stmts;
	if (s == 0)
		return;

	last = s;
	for (; ; s = next) {
		s = this_op(s);
		if (s == 0)
			break;	

		next = this_op(s->next);
		if (next == 0)
			break;	
		last = next;

		if (s->s.code == BPF_ST &&
		    next->s.code == (BPF_LDX|BPF_MEM) &&
		    s->s.k == next->s.k) {
			done = 0;
			next->s.code = BPF_MISC|BPF_TAX;
		}
		if (s->s.code == (BPF_LD|BPF_IMM) &&
		    next->s.code == (BPF_MISC|BPF_TAX)) {
			s->s.code = BPF_LDX|BPF_IMM;
			next->s.code = BPF_MISC|BPF_TXA;
			done = 0;
		}
		if (s->s.code == (BPF_LD|BPF_IMM)) {
			struct slist *add, *tax, *ild;

			if (ATOMELEM(b->out_use, X_ATOM))
				continue;

			if (next->s.code != (BPF_LDX|BPF_MSH|BPF_B))
				add = next;
			else
				add = this_op(next->next);
			if (add == 0 || add->s.code != (BPF_ALU|BPF_ADD|BPF_X))
				continue;

			tax = this_op(add->next);
			if (tax == 0 || tax->s.code != (BPF_MISC|BPF_TAX))
				continue;

			ild = this_op(tax->next);
			if (ild == 0 || BPF_CLASS(ild->s.code) != BPF_LD ||
			    BPF_MODE(ild->s.code) != BPF_IND)
				continue;
			ild->s.k += s->s.k;
			s->s.code = NOP;
			add->s.code = NOP;
			tax->s.code = NOP;
			done = 0;
		}
	}
	if (b->s.code == (BPF_JMP|BPF_JEQ|BPF_K) &&
	    !ATOMELEM(b->out_use, A_ATOM)) {
		if (last->s.code == (BPF_ALU|BPF_SUB|BPF_X)) {
			val = b->val[X_ATOM];
			if (vmap[val].is_const) {
				b->s.k += vmap[val].const_val;
				last->s.code = NOP;
				done = 0;
			} else if (b->s.k == 0) {
				last->s.code = NOP;
				b->s.code = BPF_JMP|BPF_JEQ|BPF_X;
				done = 0;
			}
		}
		else if (last->s.code == (BPF_ALU|BPF_SUB|BPF_K)) {
			last->s.code = NOP;
			b->s.k += last->s.k;
			done = 0;
		}
		else if (last->s.code == (BPF_ALU|BPF_AND|BPF_K) &&
		    b->s.k == 0) {
			b->s.k = last->s.k;
			b->s.code = BPF_JMP|BPF_K|BPF_JSET;
			last->s.code = NOP;
			done = 0;
			opt_not(b);
		}
	}
	if (b->s.code == (BPF_JMP|BPF_K|BPF_JSET)) {
		if (b->s.k == 0)
			JT(b) = JF(b);
		if (b->s.k == 0xffffffff)
			JF(b) = JT(b);
	}
	val = b->val[X_ATOM];
	if (vmap[val].is_const && BPF_SRC(b->s.code) == BPF_X) {
		bpf_int32 v = vmap[val].const_val;
		b->s.code &= ~BPF_X;
		b->s.k = v;
	}
	val = b->val[A_ATOM];
	if (vmap[val].is_const && BPF_SRC(b->s.code) == BPF_K) {
		bpf_int32 v = vmap[val].const_val;
		switch (BPF_OP(b->s.code)) {

		case BPF_JEQ:
			v = v == b->s.k;
			break;

		case BPF_JGT:
			v = (unsigned)v > b->s.k;
			break;

		case BPF_JGE:
			v = (unsigned)v >= b->s.k;
			break;

		case BPF_JSET:
			v &= b->s.k;
			break;

		default:
			abort();
		}
		if (JF(b) != JT(b))
			done = 0;
		if (v)
			JF(b) = JT(b);
		else
			JT(b) = JF(b);
	}
}

static void
opt_stmt(struct stmt *s, int val[], int alter)
{
	int op;
	int v;

	switch (s->code) {

	case BPF_LD|BPF_ABS|BPF_W:
	case BPF_LD|BPF_ABS|BPF_H:
	case BPF_LD|BPF_ABS|BPF_B:
		v = F(s->code, s->k, 0L);
		vstore(s, &val[A_ATOM], v, alter);
		break;

	case BPF_LD|BPF_IND|BPF_W:
	case BPF_LD|BPF_IND|BPF_H:
	case BPF_LD|BPF_IND|BPF_B:
		v = val[X_ATOM];
		if (alter && vmap[v].is_const) {
			s->code = BPF_LD|BPF_ABS|BPF_SIZE(s->code);
			s->k += vmap[v].const_val;
			v = F(s->code, s->k, 0L);
			done = 0;
		}
		else
			v = F(s->code, s->k, v);
		vstore(s, &val[A_ATOM], v, alter);
		break;

	case BPF_LD|BPF_LEN:
		v = F(s->code, 0L, 0L);
		vstore(s, &val[A_ATOM], v, alter);
		break;

	case BPF_LD|BPF_IMM:
		v = K(s->k);
		vstore(s, &val[A_ATOM], v, alter);
		break;

	case BPF_LDX|BPF_IMM:
		v = K(s->k);
		vstore(s, &val[X_ATOM], v, alter);
		break;

	case BPF_LDX|BPF_MSH|BPF_B:
		v = F(s->code, s->k, 0L);
		vstore(s, &val[X_ATOM], v, alter);
		break;

	case BPF_ALU|BPF_NEG:
		if (alter && vmap[val[A_ATOM]].is_const) {
			s->code = BPF_LD|BPF_IMM;
			s->k = -vmap[val[A_ATOM]].const_val;
			val[A_ATOM] = K(s->k);
		}
		else
			val[A_ATOM] = F(s->code, val[A_ATOM], 0L);
		break;

	case BPF_ALU|BPF_ADD|BPF_K:
	case BPF_ALU|BPF_SUB|BPF_K:
	case BPF_ALU|BPF_MUL|BPF_K:
	case BPF_ALU|BPF_DIV|BPF_K:
	case BPF_ALU|BPF_AND|BPF_K:
	case BPF_ALU|BPF_OR|BPF_K:
	case BPF_ALU|BPF_LSH|BPF_K:
	case BPF_ALU|BPF_RSH|BPF_K:
		op = BPF_OP(s->code);
		if (alter) {
			if (s->k == 0) {
				if (op == BPF_ADD ||
				    op == BPF_LSH || op == BPF_RSH ||
				    op == BPF_OR) {
					s->code = NOP;
					break;
				}
				if (op == BPF_MUL || op == BPF_AND) {
					s->code = BPF_LD|BPF_IMM;
					val[A_ATOM] = K(s->k);
					break;
				}
			}
			if (vmap[val[A_ATOM]].is_const) {
				fold_op(s, val[A_ATOM], K(s->k));
				val[A_ATOM] = K(s->k);
				break;
			}
		}
		val[A_ATOM] = F(s->code, val[A_ATOM], K(s->k));
		break;

	case BPF_ALU|BPF_ADD|BPF_X:
	case BPF_ALU|BPF_SUB|BPF_X:
	case BPF_ALU|BPF_MUL|BPF_X:
	case BPF_ALU|BPF_DIV|BPF_X:
	case BPF_ALU|BPF_AND|BPF_X:
	case BPF_ALU|BPF_OR|BPF_X:
	case BPF_ALU|BPF_LSH|BPF_X:
	case BPF_ALU|BPF_RSH|BPF_X:
		op = BPF_OP(s->code);
		if (alter && vmap[val[X_ATOM]].is_const) {
			if (vmap[val[A_ATOM]].is_const) {
				fold_op(s, val[A_ATOM], val[X_ATOM]);
				val[A_ATOM] = K(s->k);
			}
			else {
				s->code = BPF_ALU|BPF_K|op;
				s->k = vmap[val[X_ATOM]].const_val;
				done = 0;
				val[A_ATOM] =
					F(s->code, val[A_ATOM], K(s->k));
			}
			break;
		}
		if (alter && vmap[val[A_ATOM]].is_const
		    && vmap[val[A_ATOM]].const_val == 0) {
			if (op == BPF_ADD || op == BPF_OR) {
				s->code = BPF_MISC|BPF_TXA;
				vstore(s, &val[A_ATOM], val[X_ATOM], alter);
				break;
			}
			else if (op == BPF_MUL || op == BPF_DIV ||
				 op == BPF_AND || op == BPF_LSH || op == BPF_RSH) {
				s->code = BPF_LD|BPF_IMM;
				s->k = 0;
				vstore(s, &val[A_ATOM], K(s->k), alter);
				break;
			}
			else if (op == BPF_NEG) {
				s->code = NOP;
				break;
			}
		}
		val[A_ATOM] = F(s->code, val[A_ATOM], val[X_ATOM]);
		break;

	case BPF_MISC|BPF_TXA:
		vstore(s, &val[A_ATOM], val[X_ATOM], alter);
		break;

	case BPF_LD|BPF_MEM:
		v = val[s->k];
		if (alter && vmap[v].is_const) {
			s->code = BPF_LD|BPF_IMM;
			s->k = vmap[v].const_val;
			done = 0;
		}
		vstore(s, &val[A_ATOM], v, alter);
		break;

	case BPF_MISC|BPF_TAX:
		vstore(s, &val[X_ATOM], val[A_ATOM], alter);
		break;

	case BPF_LDX|BPF_MEM:
		v = val[s->k];
		if (alter && vmap[v].is_const) {
			s->code = BPF_LDX|BPF_IMM;
			s->k = vmap[v].const_val;
			done = 0;
		}
		vstore(s, &val[X_ATOM], v, alter);
		break;

	case BPF_ST:
		vstore(s, &val[s->k], val[A_ATOM], alter);
		break;

	case BPF_STX:
		vstore(s, &val[s->k], val[X_ATOM], alter);
		break;
	}
}

static void
deadstmt(register struct stmt *s, register struct stmt *last[])
{
	register int atom;

	atom = atomuse(s);
	if (atom >= 0) {
		if (atom == AX_ATOM) {
			last[X_ATOM] = 0;
			last[A_ATOM] = 0;
		}
		else
			last[atom] = 0;
	}
	atom = atomdef(s);
	if (atom >= 0) {
		if (last[atom]) {
			done = 0;
			last[atom]->code = NOP;
		}
		last[atom] = s;
	}
}

static void
opt_deadstores(register struct block *b)
{
	register struct slist *s;
	register int atom;
	struct stmt *last[N_ATOMS];

	memset((char *)last, 0, sizeof last);

	for (s = b->stmts; s != 0; s = s->next)
		deadstmt(&s->s, last);
	deadstmt(&b->s, last);

	for (atom = 0; atom < N_ATOMS; ++atom)
		if (last[atom] && !ATOMELEM(b->out_use, atom)) {
			last[atom]->code = NOP;
			done = 0;
		}
}

static void
opt_blk(struct block *b, int do_stmts)
{
	struct slist *s;
	struct edge *p;
	int i;
	bpf_int32 aval, xval;

#if 0
	for (s = b->stmts; s && s->next; s = s->next)
		if (BPF_CLASS(s->s.code) == BPF_JMP) {
			do_stmts = 0;
			break;
		}
#endif

	p = b->in_edges;
	if (p == 0) {
		memset((char *)b->val, 0, sizeof(b->val));
	} else {
		memcpy((char *)b->val, (char *)p->pred->val, sizeof(b->val));
		while ((p = p->next) != NULL) {
			for (i = 0; i < N_ATOMS; ++i)
				if (b->val[i] != p->pred->val[i])
					b->val[i] = 0;
		}
	}
	aval = b->val[A_ATOM];
	xval = b->val[X_ATOM];
	for (s = b->stmts; s; s = s->next)
		opt_stmt(&s->s, b->val, do_stmts);

	if (do_stmts &&
	    ((b->out_use == 0 && aval != 0 && b->val[A_ATOM] == aval &&
	      xval != 0 && b->val[X_ATOM] == xval) ||
	     BPF_CLASS(b->s.code) == BPF_RET)) {
		if (b->stmts != 0) {
			b->stmts = 0;
			done = 0;
		}
	} else {
		opt_peep(b);
		opt_deadstores(b);
	}
	if (BPF_SRC(b->s.code) == BPF_K)
		b->oval = K(b->s.k);
	else
		b->oval = b->val[X_ATOM];
	b->et.code = b->s.code;
	b->ef.code = -b->s.code;
}

static int
use_conflict(struct block *b, struct block *succ)
{
	int atom;
	atomset use = succ->out_use;

	if (use == 0)
		return 0;

	for (atom = 0; atom < N_ATOMS; ++atom)
		if (ATOMELEM(use, atom))
			if (b->val[atom] != succ->val[atom])
				return 1;
	return 0;
}

static struct block *
fold_edge(struct block *child, struct edge *ep)
{
	int sense;
	int aval0, aval1, oval0, oval1;
	int code = ep->code;

	if (code < 0) {
		code = -code;
		sense = 0;
	} else
		sense = 1;

	if (child->s.code != code)
		return 0;

	aval0 = child->val[A_ATOM];
	oval0 = child->oval;
	aval1 = ep->pred->val[A_ATOM];
	oval1 = ep->pred->oval;

	if (aval0 != aval1)
		return 0;

	if (oval0 == oval1)
		return sense ? JT(child) : JF(child);

	if (sense && code == (BPF_JMP|BPF_JEQ|BPF_K))
		return JF(child);

	return 0;
}

static void
opt_j(struct edge *ep)
{
	register int i, k;
	register struct block *target;

	if (JT(ep->succ) == 0)
		return;

	if (JT(ep->succ) == JF(ep->succ)) {
		if (!use_conflict(ep->pred, ep->succ->et.succ)) {
			done = 0;
			ep->succ = JT(ep->succ);
		}
	}
 top:
	for (i = 0; i < edgewords; ++i) {
		register bpf_u_int32 x = ep->edom[i];

		while (x != 0) {
			k = ffs(x) - 1;
			x &=~ (1 << k);
			k += i * BITS_PER_WORD;

			target = fold_edge(ep->succ, edges[k]);
			if (target != 0 && !use_conflict(ep->pred, target)) {
				done = 0;
				ep->succ = target;
				if (JT(target) != 0)
					goto top;
				return;
			}
		}
	}
}


static void
or_pullup(struct block *b)
{
	int val, at_top;
	struct block *pull;
	struct block **diffp, **samep;
	struct edge *ep;

	ep = b->in_edges;
	if (ep == 0)
		return;

	val = ep->pred->val[A_ATOM];
	for (ep = ep->next; ep != 0; ep = ep->next)
		if (val != ep->pred->val[A_ATOM])
			return;

	if (JT(b->in_edges->pred) == b)
		diffp = &JT(b->in_edges->pred);
	else
		diffp = &JF(b->in_edges->pred);

	at_top = 1;
	while (1) {
		if (*diffp == 0)
			return;

		if (JT(*diffp) != JT(b))
			return;

		if (!SET_MEMBER((*diffp)->dom, b->id))
			return;

		if ((*diffp)->val[A_ATOM] != val)
			break;

		diffp = &JF(*diffp);
		at_top = 0;
	}
	samep = &JF(*diffp);
	while (1) {
		if (*samep == 0)
			return;

		if (JT(*samep) != JT(b))
			return;

		if (!SET_MEMBER((*samep)->dom, b->id))
			return;

		if ((*samep)->val[A_ATOM] == val)
			break;

		samep = &JF(*samep);
	}
#ifdef notdef
	
	for (i = 0; i < N_ATOMS; ++i)
		if ((*samep)->val[i] != pred->val[i])
			return;
#endif
	
	pull = *samep;
	*samep = JF(pull);
	JF(pull) = *diffp;

	if (at_top) {
		for (ep = b->in_edges; ep != 0; ep = ep->next) {
			if (JT(ep->pred) == b)
				JT(ep->pred) = pull;
			else
				JF(ep->pred) = pull;
		}
	}
	else
		*diffp = pull;

	done = 0;
}

static void
and_pullup(struct block *b)
{
	int val, at_top;
	struct block *pull;
	struct block **diffp, **samep;
	struct edge *ep;

	ep = b->in_edges;
	if (ep == 0)
		return;

	val = ep->pred->val[A_ATOM];
	for (ep = ep->next; ep != 0; ep = ep->next)
		if (val != ep->pred->val[A_ATOM])
			return;

	if (JT(b->in_edges->pred) == b)
		diffp = &JT(b->in_edges->pred);
	else
		diffp = &JF(b->in_edges->pred);

	at_top = 1;
	while (1) {
		if (*diffp == 0)
			return;

		if (JF(*diffp) != JF(b))
			return;

		if (!SET_MEMBER((*diffp)->dom, b->id))
			return;

		if ((*diffp)->val[A_ATOM] != val)
			break;

		diffp = &JT(*diffp);
		at_top = 0;
	}
	samep = &JT(*diffp);
	while (1) {
		if (*samep == 0)
			return;

		if (JF(*samep) != JF(b))
			return;

		if (!SET_MEMBER((*samep)->dom, b->id))
			return;

		if ((*samep)->val[A_ATOM] == val)
			break;

		samep = &JT(*samep);
	}
#ifdef notdef
	
	for (i = 0; i < N_ATOMS; ++i)
		if ((*samep)->val[i] != pred->val[i])
			return;
#endif
	
	pull = *samep;
	*samep = JT(pull);
	JT(pull) = *diffp;

	if (at_top) {
		for (ep = b->in_edges; ep != 0; ep = ep->next) {
			if (JT(ep->pred) == b)
				JT(ep->pred) = pull;
			else
				JF(ep->pred) = pull;
		}
	}
	else
		*diffp = pull;

	done = 0;
}

static void
opt_blks(struct block *root, int do_stmts)
{
	int i, maxlevel;
	struct block *p;

	init_val();
	maxlevel = root->level;

	find_inedges(root);
	for (i = maxlevel; i >= 0; --i)
		for (p = levels[i]; p; p = p->link)
			opt_blk(p, do_stmts);

	if (do_stmts)
		return;

	for (i = 1; i <= maxlevel; ++i) {
		for (p = levels[i]; p; p = p->link) {
			opt_j(&p->et);
			opt_j(&p->ef);
		}
	}

	find_inedges(root);
	for (i = 1; i <= maxlevel; ++i) {
		for (p = levels[i]; p; p = p->link) {
			or_pullup(p);
			and_pullup(p);
		}
	}
}

static inline void
link_inedge(struct edge *parent, struct block *child)
{
	parent->next = child->in_edges;
	child->in_edges = parent;
}

static void
find_inedges(struct block *root)
{
	int i;
	struct block *b;

	for (i = 0; i < n_blocks; ++i)
		blocks[i]->in_edges = 0;

	for (i = root->level; i > 0; --i) {
		for (b = levels[i]; b != 0; b = b->link) {
			link_inedge(&b->et, JT(b));
			link_inedge(&b->ef, JF(b));
		}
	}
}

static void
opt_root(struct block **b)
{
	struct slist *tmp, *s;

	s = (*b)->stmts;
	(*b)->stmts = 0;
	while (BPF_CLASS((*b)->s.code) == BPF_JMP && JT(*b) == JF(*b))
		*b = JT(*b);

	tmp = (*b)->stmts;
	if (tmp != 0)
		sappend(s, tmp);
	(*b)->stmts = s;

	if (BPF_CLASS((*b)->s.code) == BPF_RET)
		(*b)->stmts = 0;
}

static void
opt_loop(struct block *root, int do_stmts)
{

#ifdef BDEBUG
	if (dflag > 1) {
		printf("opt_loop(root, %d) begin\n", do_stmts);
		opt_dump(root);
	}
#endif
	do {
		done = 1;
		find_levels(root);
		find_dom(root);
		find_closure(root);
		find_ud(root);
		find_edom(root);
		opt_blks(root, do_stmts);
#ifdef BDEBUG
		if (dflag > 1) {
			printf("opt_loop(root, %d) bottom, done=%d\n", do_stmts, done);
			opt_dump(root);
		}
#endif
	} while (!done);
}

void
bpf_optimize(struct block **rootp)
{
	struct block *root;

	root = *rootp;

	opt_init(root);
	opt_loop(root, 0);
	opt_loop(root, 1);
	intern_blocks(root);
#ifdef BDEBUG
	if (dflag > 1) {
		printf("after intern_blocks()\n");
		opt_dump(root);
	}
#endif
	opt_root(rootp);
#ifdef BDEBUG
	if (dflag > 1) {
		printf("after opt_root()\n");
		opt_dump(root);
	}
#endif
	opt_cleanup();
}

static void
make_marks(struct block *p)
{
	if (!isMarked(p)) {
		Mark(p);
		if (BPF_CLASS(p->s.code) != BPF_RET) {
			make_marks(JT(p));
			make_marks(JF(p));
		}
	}
}

static void
mark_code(struct block *p)
{
	cur_mark += 1;
	make_marks(p);
}

static int
eq_slist(struct slist *x, struct slist *y)
{
	while (1) {
		while (x && x->s.code == NOP)
			x = x->next;
		while (y && y->s.code == NOP)
			y = y->next;
		if (x == 0)
			return y == 0;
		if (y == 0)
			return x == 0;
		if (x->s.code != y->s.code || x->s.k != y->s.k)
			return 0;
		x = x->next;
		y = y->next;
	}
}

static inline int
eq_blk(struct block *b0, struct block *b1)
{
	if (b0->s.code == b1->s.code &&
	    b0->s.k == b1->s.k &&
	    b0->et.succ == b1->et.succ &&
	    b0->ef.succ == b1->ef.succ)
		return eq_slist(b0->stmts, b1->stmts);
	return 0;
}

static void
intern_blocks(struct block *root)
{
	struct block *p;
	int i, j;
	int done1; 
 top:
	done1 = 1;
	for (i = 0; i < n_blocks; ++i)
		blocks[i]->link = 0;

	mark_code(root);

	for (i = n_blocks - 1; --i >= 0; ) {
		if (!isMarked(blocks[i]))
			continue;
		for (j = i + 1; j < n_blocks; ++j) {
			if (!isMarked(blocks[j]))
				continue;
			if (eq_blk(blocks[i], blocks[j])) {
				blocks[i]->link = blocks[j]->link ?
					blocks[j]->link : blocks[j];
				break;
			}
		}
	}
	for (i = 0; i < n_blocks; ++i) {
		p = blocks[i];
		if (JT(p) == 0)
			continue;
		if (JT(p)->link) {
			done1 = 0;
			JT(p) = JT(p)->link;
		}
		if (JF(p)->link) {
			done1 = 0;
			JF(p) = JF(p)->link;
		}
	}
	if (!done1)
		goto top;
}

static void
opt_cleanup(void)
{
	free((void *)vnode_base);
	free((void *)vmap);
	free((void *)edges);
	free((void *)space);
	free((void *)levels);
	free((void *)blocks);
}

static u_int
slength(struct slist *s)
{
	u_int n = 0;

	for (; s; s = s->next)
		if (s->s.code != NOP)
			++n;
	return n;
}

static int
count_blocks(struct block *p)
{
	if (p == 0 || isMarked(p))
		return 0;
	Mark(p);
	return count_blocks(JT(p)) + count_blocks(JF(p)) + 1;
}

static void
number_blks_r(struct block *p)
{
	int n;

	if (p == 0 || isMarked(p))
		return;

	Mark(p);
	n = n_blocks++;
	p->id = n;
	blocks[n] = p;

	number_blks_r(JT(p));
	number_blks_r(JF(p));
}

static u_int
count_stmts(struct block *p)
{
	u_int n;

	if (p == 0 || isMarked(p))
		return 0;
	Mark(p);
	n = count_stmts(JT(p)) + count_stmts(JF(p));
	return slength(p->stmts) + n + 1 + p->longjt + p->longjf;
}

static void
opt_init(struct block *root)
{
	bpf_u_int32 *p;
	int i, n, max_stmts;

	unMarkAll();
	n = count_blocks(root);
	blocks = (struct block **)calloc(n, sizeof(*blocks));
	if (blocks == NULL)
		bpf_error("malloc");
	unMarkAll();
	n_blocks = 0;
	number_blks_r(root);

	n_edges = 2 * n_blocks;
	edges = (struct edge **)calloc(n_edges, sizeof(*edges));
	if (edges == NULL)
		bpf_error("malloc");

	levels = (struct block **)calloc(n_blocks, sizeof(*levels));
	if (levels == NULL)
		bpf_error("malloc");

	edgewords = n_edges / (8 * sizeof(bpf_u_int32)) + 1;
	nodewords = n_blocks / (8 * sizeof(bpf_u_int32)) + 1;

	
	space = (bpf_u_int32 *)malloc(2 * n_blocks * nodewords * sizeof(*space)
				 + n_edges * edgewords * sizeof(*space));
	if (space == NULL)
		bpf_error("malloc");
	p = space;
	all_dom_sets = p;
	for (i = 0; i < n; ++i) {
		blocks[i]->dom = p;
		p += nodewords;
	}
	all_closure_sets = p;
	for (i = 0; i < n; ++i) {
		blocks[i]->closure = p;
		p += nodewords;
	}
	all_edge_sets = p;
	for (i = 0; i < n; ++i) {
		register struct block *b = blocks[i];

		b->et.edom = p;
		p += edgewords;
		b->ef.edom = p;
		p += edgewords;
		b->et.id = i;
		edges[i] = &b->et;
		b->ef.id = n_blocks + i;
		edges[n_blocks + i] = &b->ef;
		b->et.pred = b;
		b->ef.pred = b;
	}
	max_stmts = 0;
	for (i = 0; i < n; ++i)
		max_stmts += slength(blocks[i]->stmts) + 1;
	maxval = 3 * max_stmts;
	vmap = (struct vmapinfo *)calloc(maxval, sizeof(*vmap));
	vnode_base = (struct valnode *)calloc(maxval, sizeof(*vnode_base));
	if (vmap == NULL || vnode_base == NULL)
		bpf_error("malloc");
}

static struct bpf_insn *fstart;
static struct bpf_insn *ftail;

#ifdef BDEBUG
int bids[1000];
#endif

static int
convert_code_r(struct block *p)
{
	struct bpf_insn *dst;
	struct slist *src;
	int slen;
	u_int off;
	int extrajmps;		
	struct slist **offset = NULL;

	if (p == 0 || isMarked(p))
		return (1);
	Mark(p);

	if (convert_code_r(JF(p)) == 0)
		return (0);
	if (convert_code_r(JT(p)) == 0)
		return (0);

	slen = slength(p->stmts);
	dst = ftail -= (slen + 1 + p->longjt + p->longjf);
		

	p->offset = dst - fstart;

	
	if (slen) {
		offset = (struct slist **)calloc(slen, sizeof(struct slist *));
		if (!offset) {
			bpf_error("not enough core");
			
		}
	}
	src = p->stmts;
	for (off = 0; off < slen && src; off++) {
#if 0
		printf("off=%d src=%x\n", off, src);
#endif
		offset[off] = src;
		src = src->next;
	}

	off = 0;
	for (src = p->stmts; src; src = src->next) {
		if (src->s.code == NOP)
			continue;
		dst->code = (u_short)src->s.code;
		dst->k = src->s.k;

		
		if (BPF_CLASS(src->s.code) != BPF_JMP || src->s.code == (BPF_JMP|BPF_JA)) {
#if 0
			if (src->s.jt || src->s.jf) {
				bpf_error("illegal jmp destination");
				
			}
#endif
			goto filled;
		}
		if (off == slen - 2)	
			goto filled;

	    {
		int i;
		int jt, jf;
		const char *ljerr = "%s for block-local relative jump: off=%d";

#if 0
		printf("code=%x off=%d %x %x\n", src->s.code,
			off, src->s.jt, src->s.jf);
#endif

		if (!src->s.jt || !src->s.jf) {
			bpf_error(ljerr, "no jmp destination", off);
			
		}

		jt = jf = 0;
		for (i = 0; i < slen; i++) {
			if (offset[i] == src->s.jt) {
				if (jt) {
					bpf_error(ljerr, "multiple matches", off);
					
				}

				dst->jt = i - off - 1;
				jt++;
			}
			if (offset[i] == src->s.jf) {
				if (jf) {
					bpf_error(ljerr, "multiple matches", off);
					
				}
				dst->jf = i - off - 1;
				jf++;
			}
		}
		if (!jt || !jf) {
			bpf_error(ljerr, "no destination found", off);
			
		}
	    }
filled:
		++dst;
		++off;
	}
	if (offset)
		free(offset);

#ifdef BDEBUG
	bids[dst - fstart] = p->id + 1;
#endif
	dst->code = (u_short)p->s.code;
	dst->k = p->s.k;
	if (JT(p)) {
		extrajmps = 0;
		off = JT(p)->offset - (p->offset + slen) - 1;
		if (off >= 256) {
		    
		    if (p->longjt == 0) {
		    	
			p->longjt++;
			return(0);
		    }
		    
		    dst->jt = extrajmps;
		    extrajmps++;
		    dst[extrajmps].code = BPF_JMP|BPF_JA;
		    dst[extrajmps].k = off - extrajmps;
		}
		else
		    dst->jt = off;
		off = JF(p)->offset - (p->offset + slen) - 1;
		if (off >= 256) {
		    
		    if (p->longjf == 0) {
		    	
			p->longjf++;
			return(0);
		    }
		    
		    
		    dst->jf = extrajmps;
		    extrajmps++;
		    dst[extrajmps].code = BPF_JMP|BPF_JA;
		    dst[extrajmps].k = off - extrajmps;
		}
		else
		    dst->jf = off;
	}
	return (1);
}


struct bpf_insn *
icode_to_fcode(struct block *root, u_int *lenp)
{
	u_int n;
	struct bpf_insn *fp;

	while (1) {
	    unMarkAll();
	    n = *lenp = count_stmts(root);

	    fp = (struct bpf_insn *)malloc(sizeof(*fp) * n);
	    if (fp == NULL)
		    bpf_error("malloc");
	    memset((char *)fp, 0, sizeof(*fp) * n);
	    fstart = fp;
	    ftail = fp + n;

	    unMarkAll();
	    if (convert_code_r(root))
		break;
	    free(fp);
	}

	return fp;
}

int
install_bpf_program(pcap_t *p, struct bpf_program *fp)
{
	size_t prog_size;

	if (!bpf_validate(fp->bf_insns, fp->bf_len)) {
		snprintf(p->errbuf, sizeof(p->errbuf),
			"BPF program is not valid");
		return (-1);
	}

	pcap_freecode(&p->fcode);

	prog_size = sizeof(*fp->bf_insns) * fp->bf_len;
	p->fcode.bf_len = fp->bf_len;
	p->fcode.bf_insns = (struct bpf_insn *)malloc(prog_size);
	if (p->fcode.bf_insns == NULL) {
		snprintf(p->errbuf, sizeof(p->errbuf),
			 "malloc: %s", pcap_strerror(errno));
		return (-1);
	}
	memcpy(p->fcode.bf_insns, fp->bf_insns, prog_size);
	return (0);
}

#ifdef BDEBUG
static void
opt_dump(struct block *root)
{
	struct bpf_program f;

	memset(bids, 0, sizeof bids);
	f.bf_insns = icode_to_fcode(root, &f.bf_len);
	bpf_dump(&f, 1);
	putchar('\n');
	free((char *)f.bf_insns);
}
#endif
