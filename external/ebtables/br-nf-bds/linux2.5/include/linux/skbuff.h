/*
 *	Definitions for the 'struct sk_buff' memory handlers.
 *
 *	Authors:
 *		Alan Cox, <gw4pts@gw4pts.ampr.org>
 *		Florian La Roche, <rzsfl@rz.uni-sb.de>
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 */

#ifndef _LINUX_SKBUFF_H
#define _LINUX_SKBUFF_H

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/time.h>
#include <linux/cache.h>

#include <asm/atomic.h>
#include <asm/types.h>
#include <linux/spinlock.h>
#include <linux/mm.h>
#include <linux/highmem.h>
#include <linux/poll.h>
#include <linux/net.h>

#define HAVE_ALLOC_SKB		
#define HAVE_ALIGNABLE_SKB	
#define SLAB_SKB 		

#define CHECKSUM_NONE 0
#define CHECKSUM_HW 1
#define CHECKSUM_UNNECESSARY 2

#define SKB_DATA_ALIGN(X)	(((X) + (SMP_CACHE_BYTES - 1)) & \
				 ~(SMP_CACHE_BYTES - 1))
#define SKB_MAX_ORDER(X, ORDER)	(((PAGE_SIZE << (ORDER)) - (X) - \
				  sizeof(struct skb_shared_info)) & \
				  ~(SMP_CACHE_BYTES - 1))
#define SKB_MAX_HEAD(X)		(SKB_MAX_ORDER((X), 0))
#define SKB_MAX_ALLOC		(SKB_MAX_ORDER(0, 2))


#ifdef __i386__
#define NET_CALLER(arg) (*(((void **)&arg) - 1))
#else
#define NET_CALLER(arg) __builtin_return_address(0)
#endif

#ifdef CONFIG_NETFILTER
struct nf_conntrack {
	atomic_t use;
	void (*destroy)(struct nf_conntrack *);
};

struct nf_ct_info {
	struct nf_conntrack *master;
};

struct nf_bridge_info {
	atomic_t use;
	struct net_device *physindev;
	struct net_device *physoutdev;
	unsigned int mask;
	unsigned long hh[16 / sizeof(unsigned long)];
};
#endif

struct sk_buff_head {
	
	struct sk_buff	*next;
	struct sk_buff	*prev;

	__u32		qlen;
	spinlock_t	lock;
};

struct sk_buff;

#define MAX_SKB_FRAGS (65536/PAGE_SIZE + 2)

typedef struct skb_frag_struct skb_frag_t;

struct skb_frag_struct {
	struct page *page;
	__u16 page_offset;
	__u16 size;
};

struct skb_shared_info {
	atomic_t	dataref;
	unsigned int	nr_frags;
	unsigned short	tso_size;
	unsigned short	tso_segs;
	struct sk_buff	*frag_list;
	skb_frag_t	frags[MAX_SKB_FRAGS];
};


struct sk_buff {
	
	struct sk_buff		*next;
	struct sk_buff		*prev;

	struct sk_buff_head	*list;
	struct sock		*sk;
	struct timeval		stamp;
	struct net_device	*dev;

	union {
		struct tcphdr	*th;
		struct udphdr	*uh;
		struct icmphdr	*icmph;
		struct igmphdr	*igmph;
		struct iphdr	*ipiph;
		unsigned char	*raw;
	} h;

	union {
		struct iphdr	*iph;
		struct ipv6hdr	*ipv6h;
		struct arphdr	*arph;
		unsigned char	*raw;
	} nh;

	union {
	  	struct ethhdr	*ethernet;
	  	unsigned char 	*raw;
	} mac;

	struct  dst_entry	*dst;

	char			cb[48];

	unsigned int		len,
				data_len,
				csum;
	unsigned char		__unused,
				cloned,
				pkt_type,
				ip_summed;
	__u32			priority;
	atomic_t		users;
	unsigned short		protocol,
				security;
	unsigned int		truesize;

	unsigned char		*head,
				*data,
				*tail,
				*end;

	void			(*destructor)(struct sk_buff *skb);
#ifdef CONFIG_NETFILTER
        unsigned long		nfmark;
	__u32			nfcache;
	struct nf_ct_info	*nfct;
#ifdef CONFIG_NETFILTER_DEBUG
        unsigned int		nf_debug;
#endif
	struct nf_bridge_info	*nf_bridge;
#endif 
#if defined(CONFIG_HIPPI)
	union {
		__u32		ifield;
	} private;
#endif
#ifdef CONFIG_NET_SCHED
       __u32			tc_index;               
#endif
};

#define SK_WMEM_MAX	65535
#define SK_RMEM_MAX	65535

#ifdef __KERNEL__
#include <linux/slab.h>

#include <asm/system.h>

extern void	       __kfree_skb(struct sk_buff *skb);
extern struct sk_buff *alloc_skb(unsigned int size, int priority);
extern void	       kfree_skbmem(struct sk_buff *skb);
extern struct sk_buff *skb_clone(struct sk_buff *skb, int priority);
extern struct sk_buff *skb_copy(const struct sk_buff *skb, int priority);
extern struct sk_buff *pskb_copy(struct sk_buff *skb, int gfp_mask);
extern int	       pskb_expand_head(struct sk_buff *skb,
					int nhead, int ntail, int gfp_mask);
extern struct sk_buff *skb_realloc_headroom(struct sk_buff *skb,
					    unsigned int headroom);
extern struct sk_buff *skb_copy_expand(const struct sk_buff *skb,
				       int newheadroom, int newtailroom,
				       int priority);
#define dev_kfree_skb(a)	kfree_skb(a)
extern void	      skb_over_panic(struct sk_buff *skb, int len,
				     void *here);
extern void	      skb_under_panic(struct sk_buff *skb, int len,
				      void *here);

#define skb_shinfo(SKB)		((struct skb_shared_info *)((SKB)->end))

static inline int skb_queue_empty(struct sk_buff_head *list)
{
	return list->next == (struct sk_buff *)list;
}

static inline struct sk_buff *skb_get(struct sk_buff *skb)
{
	atomic_inc(&skb->users);
	return skb;
}


static inline void kfree_skb(struct sk_buff *skb)
{
	if (atomic_read(&skb->users) == 1 || atomic_dec_and_test(&skb->users))
		__kfree_skb(skb);
}

static inline void kfree_skb_fast(struct sk_buff *skb)
{
	if (atomic_read(&skb->users) == 1 || atomic_dec_and_test(&skb->users))
		kfree_skbmem(skb);
}

/**
 *	skb_cloned - is the buffer a clone
 *	@skb: buffer to check
 *
 *	Returns true if the buffer was generated with skb_clone() and is
 *	one of multiple shared copies of the buffer. Cloned buffers are
 *	shared data so must not be written to under normal circumstances.
 */
static inline int skb_cloned(struct sk_buff *skb)
{
	return skb->cloned && atomic_read(&skb_shinfo(skb)->dataref) != 1;
}

static inline int skb_shared(struct sk_buff *skb)
{
	return atomic_read(&skb->users) != 1;
}

static inline struct sk_buff *skb_share_check(struct sk_buff *skb, int pri)
{
	if (skb_shared(skb)) {
		struct sk_buff *nskb = skb_clone(skb, pri);
		kfree_skb(skb);
		skb = nskb;
	}
	return skb;
}


static inline struct sk_buff *skb_unshare(struct sk_buff *skb, int pri)
{
	if (skb_cloned(skb)) {
		struct sk_buff *nskb = skb_copy(skb, pri);
		kfree_skb(skb);	
		skb = nskb;
	}
	return skb;
}

static inline struct sk_buff *skb_peek(struct sk_buff_head *list_)
{
	struct sk_buff *list = ((struct sk_buff *)list_)->next;
	if (list == (struct sk_buff *)list_)
		list = NULL;
	return list;
}

static inline struct sk_buff *skb_peek_tail(struct sk_buff_head *list_)
{
	struct sk_buff *list = ((struct sk_buff *)list_)->prev;
	if (list == (struct sk_buff *)list_)
		list = NULL;
	return list;
}

static inline __u32 skb_queue_len(struct sk_buff_head *list_)
{
	return list_->qlen;
}

static inline void skb_queue_head_init(struct sk_buff_head *list)
{
	spin_lock_init(&list->lock);
	list->prev = list->next = (struct sk_buff *)list;
	list->qlen = 0;
}


static inline void __skb_queue_head(struct sk_buff_head *list,
				    struct sk_buff *newsk)
{
	struct sk_buff *prev, *next;

	newsk->list = list;
	list->qlen++;
	prev = (struct sk_buff *)list;
	next = prev->next;
	newsk->next = next;
	newsk->prev = prev;
	next->prev  = prev->next = newsk;
}


static inline void skb_queue_head(struct sk_buff_head *list,
				  struct sk_buff *newsk)
{
	unsigned long flags;

	spin_lock_irqsave(&list->lock, flags);
	__skb_queue_head(list, newsk);
	spin_unlock_irqrestore(&list->lock, flags);
}

static inline void __skb_queue_tail(struct sk_buff_head *list,
				   struct sk_buff *newsk)
{
	struct sk_buff *prev, *next;

	newsk->list = list;
	list->qlen++;
	next = (struct sk_buff *)list;
	prev = next->prev;
	newsk->next = next;
	newsk->prev = prev;
	next->prev  = prev->next = newsk;
}

static inline void skb_queue_tail(struct sk_buff_head *list,
				  struct sk_buff *newsk)
{
	unsigned long flags;

	spin_lock_irqsave(&list->lock, flags);
	__skb_queue_tail(list, newsk);
	spin_unlock_irqrestore(&list->lock, flags);
}

static inline struct sk_buff *__skb_dequeue(struct sk_buff_head *list)
{
	struct sk_buff *next, *prev, *result;

	prev = (struct sk_buff *) list;
	next = prev->next;
	result = NULL;
	if (next != prev) {
		result	     = next;
		next	     = next->next;
		list->qlen--;
		next->prev   = prev;
		prev->next   = next;
		result->next = result->prev = NULL;
		result->list = NULL;
	}
	return result;
}


static inline struct sk_buff *skb_dequeue(struct sk_buff_head *list)
{
	unsigned long flags;
	struct sk_buff *result;

	spin_lock_irqsave(&list->lock, flags);
	result = __skb_dequeue(list);
	spin_unlock_irqrestore(&list->lock, flags);
	return result;
}


static inline void __skb_insert(struct sk_buff *newsk,
				struct sk_buff *prev, struct sk_buff *next,
				struct sk_buff_head *list)
{
	newsk->next = next;
	newsk->prev = prev;
	next->prev  = prev->next = newsk;
	newsk->list = list;
	list->qlen++;
}


static inline void skb_insert(struct sk_buff *old, struct sk_buff *newsk)
{
	unsigned long flags;

	spin_lock_irqsave(&old->list->lock, flags);
	__skb_insert(newsk, old->prev, old, old->list);
	spin_unlock_irqrestore(&old->list->lock, flags);
}


static inline void __skb_append(struct sk_buff *old, struct sk_buff *newsk)
{
	__skb_insert(newsk, old, old->next, old->list);
}



static inline void skb_append(struct sk_buff *old, struct sk_buff *newsk)
{
	unsigned long flags;

	spin_lock_irqsave(&old->list->lock, flags);
	__skb_append(old, newsk);
	spin_unlock_irqrestore(&old->list->lock, flags);
}

static inline void __skb_unlink(struct sk_buff *skb, struct sk_buff_head *list)
{
	struct sk_buff *next, *prev;

	list->qlen--;
	next	   = skb->next;
	prev	   = skb->prev;
	skb->next  = skb->prev = NULL;
	skb->list  = NULL;
	next->prev = prev;
	prev->next = next;
}

static inline void skb_unlink(struct sk_buff *skb)
{
	struct sk_buff_head *list = skb->list;

	if (list) {
		unsigned long flags;

		spin_lock_irqsave(&list->lock, flags);
		if (skb->list == list)
			__skb_unlink(skb, skb->list);
		spin_unlock_irqrestore(&list->lock, flags);
	}
}


static inline struct sk_buff *__skb_dequeue_tail(struct sk_buff_head *list)
{
	struct sk_buff *skb = skb_peek_tail(list);
	if (skb)
		__skb_unlink(skb, list);
	return skb;
}

static inline struct sk_buff *skb_dequeue_tail(struct sk_buff_head *list)
{
	unsigned long flags;
	struct sk_buff *result;

	spin_lock_irqsave(&list->lock, flags);
	result = __skb_dequeue_tail(list);
	spin_unlock_irqrestore(&list->lock, flags);
	return result;
}

static inline int skb_is_nonlinear(const struct sk_buff *skb)
{
	return skb->data_len;
}

static inline int skb_headlen(const struct sk_buff *skb)
{
	return skb->len - skb->data_len;
}

static inline int skb_pagelen(const struct sk_buff *skb)
{
	int i, len = 0;

	for (i = (int)skb_shinfo(skb)->nr_frags - 1; i >= 0; i--)
		len += skb_shinfo(skb)->frags[i].size;
	return len + skb_headlen(skb);
}

#define SKB_PAGE_ASSERT(skb) do { if (skb_shinfo(skb)->nr_frags) \
					BUG(); } while (0)
#define SKB_FRAG_ASSERT(skb) do { if (skb_shinfo(skb)->frag_list) \
					BUG(); } while (0)
#define SKB_LINEAR_ASSERT(skb) do { if (skb_is_nonlinear(skb)) \
					BUG(); } while (0)

static inline unsigned char *__skb_put(struct sk_buff *skb, unsigned int len)
{
	unsigned char *tmp = skb->tail;
	SKB_LINEAR_ASSERT(skb);
	skb->tail += len;
	skb->len  += len;
	return tmp;
}

static inline unsigned char *skb_put(struct sk_buff *skb, unsigned int len)
{
	unsigned char *tmp = skb->tail;
	SKB_LINEAR_ASSERT(skb);
	skb->tail += len;
	skb->len  += len;
	if (skb->tail>skb->end)
		skb_over_panic(skb, len, current_text_addr());
	return tmp;
}

static inline unsigned char *__skb_push(struct sk_buff *skb, unsigned int len)
{
	skb->data -= len;
	skb->len  += len;
	return skb->data;
}

static inline unsigned char *skb_push(struct sk_buff *skb, unsigned int len)
{
	skb->data -= len;
	skb->len  += len;
	if (skb->data<skb->head)
		skb_under_panic(skb, len, current_text_addr());
	return skb->data;
}

static inline char *__skb_pull(struct sk_buff *skb, unsigned int len)
{
	skb->len -= len;
	if (skb->len < skb->data_len)
		BUG();
	return skb->data += len;
}

static inline unsigned char *skb_pull(struct sk_buff *skb, unsigned int len)
{
	return (len > skb->len) ? NULL : __skb_pull(skb, len);
}

extern unsigned char *__pskb_pull_tail(struct sk_buff *skb, int delta);

static inline char *__pskb_pull(struct sk_buff *skb, unsigned int len)
{
	if (len > skb_headlen(skb) &&
	    !__pskb_pull_tail(skb, len-skb_headlen(skb)))
		return NULL;
	skb->len -= len;
	return skb->data += len;
}

static inline unsigned char *pskb_pull(struct sk_buff *skb, unsigned int len)
{
	return (len > skb->len) ? NULL : __pskb_pull(skb, len);
}

static inline int pskb_may_pull(struct sk_buff *skb, unsigned int len)
{
	if (len <= skb_headlen(skb))
		return 1;
	if (len > skb->len)
		return 0;
	return __pskb_pull_tail(skb, len-skb_headlen(skb)) != NULL;
}

static inline int skb_headroom(const struct sk_buff *skb)
{
	return skb->data - skb->head;
}

static inline int skb_tailroom(const struct sk_buff *skb)
{
	return skb_is_nonlinear(skb) ? 0 : skb->end - skb->tail;
}

static inline void skb_reserve(struct sk_buff *skb, unsigned int len)
{
	skb->data += len;
	skb->tail += len;
}

extern int ___pskb_trim(struct sk_buff *skb, unsigned int len, int realloc);

static inline void __skb_trim(struct sk_buff *skb, unsigned int len)
{
	if (!skb->data_len) {
		skb->len  = len;
		skb->tail = skb->data + len;
	} else
		___pskb_trim(skb, len, 0);
}

static inline void skb_trim(struct sk_buff *skb, unsigned int len)
{
	if (skb->len > len)
		__skb_trim(skb, len);
}


static inline int __pskb_trim(struct sk_buff *skb, unsigned int len)
{
	if (!skb->data_len) {
		skb->len  = len;
		skb->tail = skb->data+len;
		return 0;
	}
	return ___pskb_trim(skb, len, 1);
}

static inline int pskb_trim(struct sk_buff *skb, unsigned int len)
{
	return (len < skb->len) ? __pskb_trim(skb, len) : 0;
}

static inline void skb_orphan(struct sk_buff *skb)
{
	if (skb->destructor)
		skb->destructor(skb);
	skb->destructor = NULL;
	skb->sk		= NULL;
}

static inline void skb_queue_purge(struct sk_buff_head *list)
{
	struct sk_buff *skb;
	while ((skb = skb_dequeue(list)) != NULL)
		kfree_skb(skb);
}

static inline void __skb_queue_purge(struct sk_buff_head *list)
{
	struct sk_buff *skb;
	while ((skb = __skb_dequeue(list)) != NULL)
		kfree_skb(skb);
}

static inline struct sk_buff *__dev_alloc_skb(unsigned int length,
					      int gfp_mask)
{
	struct sk_buff *skb = alloc_skb(length + 16, gfp_mask);
	if (skb)
		skb_reserve(skb, 16);
	return skb;
}

static inline struct sk_buff *dev_alloc_skb(unsigned int length)
{
	return __dev_alloc_skb(length, GFP_ATOMIC);
}

static inline int skb_cow(struct sk_buff *skb, unsigned int headroom)
{
	int delta = (headroom > 16 ? headroom : 16) - skb_headroom(skb);

	if (delta < 0)
		delta = 0;

	if (delta || skb_cloned(skb))
		return pskb_expand_head(skb, (delta + 15) & ~15, 0, GFP_ATOMIC);
	return 0;
}

int skb_linearize(struct sk_buff *skb, int gfp);

static inline void *kmap_skb_frag(const skb_frag_t *frag)
{
#ifdef CONFIG_HIGHMEM
	if (in_irq())
		BUG();

	local_bh_disable();
#endif
	return kmap_atomic(frag->page, KM_SKB_DATA_SOFTIRQ);
}

static inline void kunmap_skb_frag(void *vaddr)
{
	kunmap_atomic(vaddr, KM_SKB_DATA_SOFTIRQ);
#ifdef CONFIG_HIGHMEM
	local_bh_enable();
#endif
}

#define skb_queue_walk(queue, skb) \
		for (skb = (queue)->next;			\
		     (skb != (struct sk_buff *)(queue));	\
		     skb = skb->next)


extern struct sk_buff *skb_recv_datagram(struct sock *sk, unsigned flags,
					 int noblock, int *err);
extern unsigned int    datagram_poll(struct file *file, struct socket *sock,
				     struct poll_table_struct *wait);
extern int	       skb_copy_datagram(const struct sk_buff *from,
					 int offset, char *to, int size);
extern int	       skb_copy_datagram_iovec(const struct sk_buff *from,
					       int offset, struct iovec *to,
					       int size);
extern int	       skb_copy_and_csum_datagram(const struct sk_buff *skb,
						  int offset, u8 *to, int len,
						  unsigned int *csump);
extern int	       skb_copy_and_csum_datagram_iovec(const
							struct sk_buff *skb,
							int hlen,
							struct iovec *iov);
extern void	       skb_free_datagram(struct sock *sk, struct sk_buff *skb);
extern unsigned int    skb_checksum(const struct sk_buff *skb, int offset,
				    int len, unsigned int csum);
extern int	       skb_copy_bits(const struct sk_buff *skb, int offset,
				     void *to, int len);
extern unsigned int    skb_copy_and_csum_bits(const struct sk_buff *skb,
					      int offset, u8 *to, int len,
					      unsigned int csum);
extern void	       skb_copy_and_csum_dev(const struct sk_buff *skb, u8 *to);

extern void skb_init(void);
extern void skb_add_mtu(int mtu);

#ifdef CONFIG_NETFILTER
static inline void nf_conntrack_put(struct nf_ct_info *nfct)
{
	if (nfct && atomic_dec_and_test(&nfct->master->use))
		nfct->master->destroy(nfct->master);
}
static inline void nf_conntrack_get(struct nf_ct_info *nfct)
{
	if (nfct)
		atomic_inc(&nfct->master->use);
}

static inline void nf_bridge_put(struct nf_bridge_info *nf_bridge)
{
	if (nf_bridge && atomic_dec_and_test(&nf_bridge->use))
		kfree(nf_bridge);
}
static inline void nf_bridge_get(struct nf_bridge_info *nf_bridge)
{
	if (nf_bridge)
		atomic_inc(&nf_bridge->use);
}
#endif

#endif	
#endif	
