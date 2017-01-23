/*
 *	Routines having to do with the 'struct sk_buff' memory handlers.
 *
 *	Authors:	Alan Cox <iiitac@pyr.swan.ac.uk>
 *			Florian La Roche <rzsfl@rz.uni-sb.de>
 *
 *	Version:	$Id: skbuff.c,v 1.4 2002/09/17 21:49:57 bdschuym Exp $
 *
 *	Fixes:	
 *		Alan Cox	:	Fixed the worst of the load balancer bugs.
 *		Dave Platt	:	Interrupt stacking fix.
 *	Richard Kooijman	:	Timestamp fixes.
 *		Alan Cox	:	Changed buffer format.
 *		Alan Cox	:	destructor hook for AF_UNIX etc.
 *		Linus Torvalds	:	Better skb_clone.
 *		Alan Cox	:	Added skb_copy.
 *		Alan Cox	:	Added all the changed routines Linus
 *					only put in the headers
 *		Ray VanTassle	:	Fixed --skb->lock in free
 *		Alan Cox	:	skb_copy copy arp field
 *		Andi Kleen	:	slabified it.
 *
 *	NOTE:
 *		The __skb_ routines should be called with interrupts 
 *	disabled, or you better be *real* sure that the operation is atomic 
 *	with respect to whatever list is being frobbed (e.g. via lock_sock()
 *	or via disabling bottom half handlers, etc).
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 */


#include <linux/config.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/in.h>
#include <linux/inet.h>
#include <linux/slab.h>
#include <linux/netdevice.h>
#include <linux/string.h>
#include <linux/skbuff.h>
#include <linux/cache.h>
#include <linux/rtnetlink.h>
#include <linux/init.h>
#include <linux/highmem.h>

#include <net/protocol.h>
#include <net/dst.h>
#include <net/sock.h>
#include <net/checksum.h>

#include <asm/uaccess.h>
#include <asm/system.h>

int sysctl_hot_list_len = 128;

static kmem_cache_t *skbuff_head_cache;

static union {
	struct sk_buff_head	list;
	char			pad[SMP_CACHE_BYTES];
} skb_head_pool[NR_CPUS];


 
void skb_over_panic(struct sk_buff *skb, int sz, void *here)
{
	printk("skput:over: %p:%d put:%d dev:%s", 
		here, skb->len, sz, skb->dev ? skb->dev->name : "<NULL>");
	BUG();
}

 

void skb_under_panic(struct sk_buff *skb, int sz, void *here)
{
        printk("skput:under: %p:%d put:%d dev:%s",
                here, skb->len, sz, skb->dev ? skb->dev->name : "<NULL>");
	BUG();
}

static __inline__ struct sk_buff *skb_head_from_pool(void)
{
	struct sk_buff_head *list = &skb_head_pool[smp_processor_id()].list;

	if (skb_queue_len(list)) {
		struct sk_buff *skb;
		unsigned long flags;

		local_irq_save(flags);
		skb = __skb_dequeue(list);
		local_irq_restore(flags);
		return skb;
	}
	return NULL;
}

static __inline__ void skb_head_to_pool(struct sk_buff *skb)
{
	struct sk_buff_head *list = &skb_head_pool[smp_processor_id()].list;

	if (skb_queue_len(list) < sysctl_hot_list_len) {
		unsigned long flags;

		local_irq_save(flags);
		__skb_queue_head(list, skb);
		local_irq_restore(flags);

		return;
	}
	kmem_cache_free(skbuff_head_cache, skb);
}



 
struct sk_buff *alloc_skb(unsigned int size,int gfp_mask)
{
	struct sk_buff *skb;
	u8 *data;

	if (in_interrupt() && (gfp_mask & __GFP_WAIT)) {
		static int count = 0;
		if (++count < 5) {
			printk(KERN_ERR "alloc_skb called nonatomically "
			       "from interrupt %p\n", NET_CALLER(size));
 			BUG();
		}
		gfp_mask &= ~__GFP_WAIT;
	}

	
	skb = skb_head_from_pool();
	if (skb == NULL) {
		skb = kmem_cache_alloc(skbuff_head_cache, gfp_mask & ~__GFP_DMA);
		if (skb == NULL)
			goto nohead;
	}

	
	size = SKB_DATA_ALIGN(size);
	data = kmalloc(size + sizeof(struct skb_shared_info), gfp_mask);
	if (data == NULL)
		goto nodata;

	 
	skb->truesize = size + sizeof(struct sk_buff);

	
	skb->head = data;
	skb->data = data;
	skb->tail = data;
	skb->end = data + size;

	
	skb->len = 0;
	skb->cloned = 0;
	skb->data_len = 0;

	atomic_set(&skb->users, 1); 
	atomic_set(&(skb_shinfo(skb)->dataref), 1);
	skb_shinfo(skb)->nr_frags = 0;
	skb_shinfo(skb)->frag_list = NULL;
	return skb;

nodata:
	skb_head_to_pool(skb);
nohead:
	return NULL;
}


 
static inline void skb_headerinit(void *p, kmem_cache_t *cache, 
				  unsigned long flags)
{
	struct sk_buff *skb = p;

	skb->next = NULL;
	skb->prev = NULL;
	skb->list = NULL;
	skb->sk = NULL;
	skb->stamp.tv_sec=0;	
	skb->dev = NULL;
	skb->physindev = NULL;
	skb->physoutdev = NULL;
	skb->dst = NULL;
	memset(skb->cb, 0, sizeof(skb->cb));
	skb->pkt_type = PACKET_HOST;	
	skb->ip_summed = 0;
	skb->priority = 0;
	skb->security = 0;	
	skb->destructor = NULL;

#ifdef CONFIG_NETFILTER
	skb->nfmark = skb->nfcache = 0;
	skb->nfct = NULL;
#ifdef CONFIG_NETFILTER_DEBUG
	skb->nf_debug = 0;
#endif
#endif
#ifdef CONFIG_NET_SCHED
	skb->tc_index = 0;
#endif
}

static void skb_drop_fraglist(struct sk_buff *skb)
{
	struct sk_buff *list = skb_shinfo(skb)->frag_list;

	skb_shinfo(skb)->frag_list = NULL;

	do {
		struct sk_buff *this = list;
		list = list->next;
		kfree_skb(this);
	} while (list);
}

static void skb_clone_fraglist(struct sk_buff *skb)
{
	struct sk_buff *list;

	for (list = skb_shinfo(skb)->frag_list; list; list=list->next)
		skb_get(list);
}

static void skb_release_data(struct sk_buff *skb)
{
	if (!skb->cloned ||
	    atomic_dec_and_test(&(skb_shinfo(skb)->dataref))) {
		if (skb_shinfo(skb)->nr_frags) {
			int i;
			for (i = 0; i < skb_shinfo(skb)->nr_frags; i++)
				put_page(skb_shinfo(skb)->frags[i].page);
		}

		if (skb_shinfo(skb)->frag_list)
			skb_drop_fraglist(skb);

		kfree(skb->head);
	}
}

void kfree_skbmem(struct sk_buff *skb)
{
	skb_release_data(skb);
	skb_head_to_pool(skb);
}


void __kfree_skb(struct sk_buff *skb)
{
	if (skb->list) {
	 	printk(KERN_WARNING "Warning: kfree_skb passed an skb still "
		       "on a list (from %p).\n", NET_CALLER(skb));
		BUG();
	}

	dst_release(skb->dst);
	if(skb->destructor) {
		if (in_irq()) {
			printk(KERN_WARNING "Warning: kfree_skb on hard IRQ %p\n",
				NET_CALLER(skb));
		}
		skb->destructor(skb);
	}
#ifdef CONFIG_NETFILTER
	nf_conntrack_put(skb->nfct);
#endif
	skb_headerinit(skb, NULL, 0);  
	kfree_skbmem(skb);
}


struct sk_buff *skb_clone(struct sk_buff *skb, int gfp_mask)
{
	struct sk_buff *n;

	n = skb_head_from_pool();
	if (!n) {
		n = kmem_cache_alloc(skbuff_head_cache, gfp_mask);
		if (!n)
			return NULL;
	}

#define C(x) n->x = skb->x

	n->next = n->prev = NULL;
	n->list = NULL;
	n->sk = NULL;
	C(stamp);
	C(dev);
	C(physindev);
	C(physoutdev);
	C(h);
	C(nh);
	C(mac);
	C(dst);
	dst_clone(n->dst);
	memcpy(n->cb, skb->cb, sizeof(skb->cb));
	C(len);
	C(data_len);
	C(csum);
	n->cloned = 1;
	C(pkt_type);
	C(ip_summed);
	C(priority);
	atomic_set(&n->users, 1);
	C(protocol);
	C(security);
	C(truesize);
	C(head);
	C(data);
	C(tail);
	C(end);
	n->destructor = NULL;
#ifdef CONFIG_NETFILTER
	C(nfmark);
	C(nfcache);
	C(nfct);
#ifdef CONFIG_NETFILTER_DEBUG
	C(nf_debug);
#endif
#endif 
#if defined(CONFIG_HIPPI)
	C(private);
#endif
#ifdef CONFIG_NET_SCHED
	C(tc_index);
#endif

	atomic_inc(&(skb_shinfo(skb)->dataref));
	skb->cloned = 1;
#ifdef CONFIG_NETFILTER
	nf_conntrack_get(skb->nfct);
#endif
	return n;
}

static void copy_skb_header(struct sk_buff *new, const struct sk_buff *old)
{
	unsigned long offset = new->data - old->data;

	new->list=NULL;
	new->sk=NULL;
	new->dev=old->dev;
	new->physindev=old->physindev;
	new->physoutdev=old->physoutdev;
	new->priority=old->priority;
	new->protocol=old->protocol;
	new->dst=dst_clone(old->dst);
	new->h.raw=old->h.raw+offset;
	new->nh.raw=old->nh.raw+offset;
	new->mac.raw=old->mac.raw+offset;
	memcpy(new->cb, old->cb, sizeof(old->cb));
	atomic_set(&new->users, 1);
	new->pkt_type=old->pkt_type;
	new->stamp=old->stamp;
	new->destructor = NULL;
	new->security=old->security;
#ifdef CONFIG_NETFILTER
	new->nfmark=old->nfmark;
	new->nfcache=old->nfcache;
	new->nfct=old->nfct;
	nf_conntrack_get(new->nfct);
#ifdef CONFIG_NETFILTER_DEBUG
	new->nf_debug=old->nf_debug;
#endif
#endif
#ifdef CONFIG_NET_SCHED
	new->tc_index = old->tc_index;
#endif
}

 
struct sk_buff *skb_copy(const struct sk_buff *skb, int gfp_mask)
{
	struct sk_buff *n;
	int headerlen = skb->data-skb->head;

	n=alloc_skb(skb->end - skb->head + skb->data_len, gfp_mask);
	if(n==NULL)
		return NULL;

	
	skb_reserve(n,headerlen);
	
	skb_put(n,skb->len);
	n->csum = skb->csum;
	n->ip_summed = skb->ip_summed;

	if (skb_copy_bits(skb, -headerlen, n->head, headerlen+skb->len))
		BUG();

	copy_skb_header(n, skb);

	return n;
}

int skb_linearize(struct sk_buff *skb, int gfp_mask)
{
	unsigned int size;
	u8 *data;
	long offset;
	int headerlen = skb->data - skb->head;
	int expand = (skb->tail+skb->data_len) - skb->end;

	if (skb_shared(skb))
		BUG();

	if (expand <= 0)
		expand = 0;

	size = (skb->end - skb->head + expand);
	size = SKB_DATA_ALIGN(size);
	data = kmalloc(size + sizeof(struct skb_shared_info), gfp_mask);
	if (data == NULL)
		return -ENOMEM;

	
	if (skb_copy_bits(skb, -headerlen, data, headerlen+skb->len))
		BUG();

	
	offset = data - skb->head;

	
	skb_release_data(skb);

	skb->head = data;
	skb->end  = data + size;

	
	skb->h.raw += offset;
	skb->nh.raw += offset;
	skb->mac.raw += offset;
	skb->tail += offset;
	skb->data += offset;

	
	atomic_set(&(skb_shinfo(skb)->dataref), 1);
	skb_shinfo(skb)->nr_frags = 0;
	skb_shinfo(skb)->frag_list = NULL;

	
	skb->cloned = 0;

	skb->tail += skb->data_len;
	skb->data_len = 0;
	return 0;
}



struct sk_buff *pskb_copy(struct sk_buff *skb, int gfp_mask)
{
	struct sk_buff *n;

	n=alloc_skb(skb->end - skb->head, gfp_mask);
	if(n==NULL)
		return NULL;

	
	skb_reserve(n,skb->data-skb->head);
	
	skb_put(n,skb_headlen(skb));
	
	memcpy(n->data, skb->data, n->len);
	n->csum = skb->csum;
	n->ip_summed = skb->ip_summed;

	n->data_len = skb->data_len;
	n->len = skb->len;

	if (skb_shinfo(skb)->nr_frags) {
		int i;

		for (i = 0; i < skb_shinfo(skb)->nr_frags; i++) {
			skb_shinfo(n)->frags[i] = skb_shinfo(skb)->frags[i];
			get_page(skb_shinfo(n)->frags[i].page);
		}
		skb_shinfo(n)->nr_frags = i;
	}

	if (skb_shinfo(skb)->frag_list) {
		skb_shinfo(n)->frag_list = skb_shinfo(skb)->frag_list;
		skb_clone_fraglist(n);
	}

	copy_skb_header(n, skb);

	return n;
}


int pskb_expand_head(struct sk_buff *skb, int nhead, int ntail, int gfp_mask)
{
	int i;
	u8 *data;
	int size = nhead + (skb->end - skb->head) + ntail;
	long off;

	if (skb_shared(skb))
		BUG();

	size = SKB_DATA_ALIGN(size);

	data = kmalloc(size + sizeof(struct skb_shared_info), gfp_mask);
	if (data == NULL)
		goto nodata;

	memcpy(data+nhead, skb->head, skb->tail-skb->head);
	memcpy(data+size, skb->end, sizeof(struct skb_shared_info));

	for (i=0; i<skb_shinfo(skb)->nr_frags; i++)
		get_page(skb_shinfo(skb)->frags[i].page);

	if (skb_shinfo(skb)->frag_list)
		skb_clone_fraglist(skb);

	skb_release_data(skb);

	off = (data+nhead) - skb->head;

	skb->head = data;
	skb->end  = data+size;

	skb->data += off;
	skb->tail += off;
	skb->mac.raw += off;
	skb->h.raw += off;
	skb->nh.raw += off;
	skb->cloned = 0;
	atomic_set(&skb_shinfo(skb)->dataref, 1);
	return 0;

nodata:
	return -ENOMEM;
}


struct sk_buff *
skb_realloc_headroom(struct sk_buff *skb, unsigned int headroom)
{
	struct sk_buff *skb2;
	int delta = headroom - skb_headroom(skb);

	if (delta <= 0)
		return pskb_copy(skb, GFP_ATOMIC);

	skb2 = skb_clone(skb, GFP_ATOMIC);
	if (skb2 == NULL ||
	    !pskb_expand_head(skb2, SKB_DATA_ALIGN(delta), 0, GFP_ATOMIC))
		return skb2;

	kfree_skb(skb2);
	return NULL;
}


 

struct sk_buff *skb_copy_expand(const struct sk_buff *skb,
				int newheadroom,
				int newtailroom,
				int gfp_mask)
{
	struct sk_buff *n;

 	 
	n=alloc_skb(newheadroom + skb->len + newtailroom,
		    gfp_mask);
	if(n==NULL)
		return NULL;

	skb_reserve(n,newheadroom);

	
	skb_put(n,skb->len);

	
	if (skb_copy_bits(skb, 0, n->data, skb->len))
		BUG();

	copy_skb_header(n, skb);
	return n;
}


int ___pskb_trim(struct sk_buff *skb, unsigned int len, int realloc)
{
	int offset = skb_headlen(skb);
	int nfrags = skb_shinfo(skb)->nr_frags;
	int i;

	for (i=0; i<nfrags; i++) {
		int end = offset + skb_shinfo(skb)->frags[i].size;
		if (end > len) {
			if (skb_cloned(skb)) {
				if (!realloc)
					BUG();
				if (pskb_expand_head(skb, 0, 0, GFP_ATOMIC))
					return -ENOMEM;
			}
			if (len <= offset) {
				put_page(skb_shinfo(skb)->frags[i].page);
				skb_shinfo(skb)->nr_frags--;
			} else {
				skb_shinfo(skb)->frags[i].size = len-offset;
			}
		}
		offset = end;
	}

	if (offset < len) {
		skb->data_len -= skb->len - len;
		skb->len = len;
	} else {
		if (len <= skb_headlen(skb)) {
			skb->len = len;
			skb->data_len = 0;
			skb->tail = skb->data + len;
			if (skb_shinfo(skb)->frag_list && !skb_cloned(skb))
				skb_drop_fraglist(skb);
		} else {
			skb->data_len -= skb->len - len;
			skb->len = len;
		}
	}

	return 0;
}


unsigned char * __pskb_pull_tail(struct sk_buff *skb, int delta)
{
	int i, k, eat;

	eat = (skb->tail+delta) - skb->end;

	if (eat > 0 || skb_cloned(skb)) {
		if (pskb_expand_head(skb, 0, eat>0 ? eat+128 : 0, GFP_ATOMIC))
			return NULL;
	}

	if (skb_copy_bits(skb, skb_headlen(skb), skb->tail, delta))
		BUG();

	if (skb_shinfo(skb)->frag_list == NULL)
		goto pull_pages;

	
	eat = delta;
	for (i=0; i<skb_shinfo(skb)->nr_frags; i++) {
		if (skb_shinfo(skb)->frags[i].size >= eat)
			goto pull_pages;
		eat -= skb_shinfo(skb)->frags[i].size;
	}

	if (eat) {
		struct sk_buff *list = skb_shinfo(skb)->frag_list;
		struct sk_buff *clone = NULL;
		struct sk_buff *insp = NULL;

		do {
			if (list == NULL)
				BUG();

			if (list->len <= eat) {
				
				eat -= list->len;
				list = list->next;
				insp = list;
			} else {
				

				if (skb_shared(list)) {
					
					clone = skb_clone(list, GFP_ATOMIC);
					if (clone == NULL)
						return NULL;
					insp = list->next;
					list = clone;
				} else {
					insp = list;
				}
				if (pskb_pull(list, eat) == NULL) {
					if (clone)
						kfree_skb(clone);
					return NULL;
				}
				break;
			}
		} while (eat);

		
		while ((list = skb_shinfo(skb)->frag_list) != insp) {
			skb_shinfo(skb)->frag_list = list->next;
			kfree_skb(list);
		}
		
		if (clone) {
			clone->next = list;
			skb_shinfo(skb)->frag_list = clone;
		}
	}
	

pull_pages:
	eat = delta;
	k = 0;
	for (i=0; i<skb_shinfo(skb)->nr_frags; i++) {
		if (skb_shinfo(skb)->frags[i].size <= eat) {
			put_page(skb_shinfo(skb)->frags[i].page);
			eat -= skb_shinfo(skb)->frags[i].size;
		} else {
			skb_shinfo(skb)->frags[k] = skb_shinfo(skb)->frags[i];
			if (eat) {
				skb_shinfo(skb)->frags[k].page_offset += eat;
				skb_shinfo(skb)->frags[k].size -= eat;
				eat = 0;
			}
			k++;
		}
	}
	skb_shinfo(skb)->nr_frags = k;

	skb->tail += delta;
	skb->data_len -= delta;

	return skb->tail;
}


int skb_copy_bits(const struct sk_buff *skb, int offset, void *to, int len)
{
	int i, copy;
	int start = skb->len - skb->data_len;

	if (offset > (int)skb->len-len)
		goto fault;

	
	if ((copy = start-offset) > 0) {
		if (copy > len)
			copy = len;
		memcpy(to, skb->data + offset, copy);
		if ((len -= copy) == 0)
			return 0;
		offset += copy;
		to += copy;
	}

	for (i = 0; i < skb_shinfo(skb)->nr_frags; i++) {
		int end;

		BUG_TRAP(start <= offset+len);

		end = start + skb_shinfo(skb)->frags[i].size;
		if ((copy = end-offset) > 0) {
			u8 *vaddr;

			if (copy > len)
				copy = len;

			vaddr = kmap_skb_frag(&skb_shinfo(skb)->frags[i]);
			memcpy(to, vaddr+skb_shinfo(skb)->frags[i].page_offset+
			       offset-start, copy);
			kunmap_skb_frag(vaddr);

			if ((len -= copy) == 0)
				return 0;
			offset += copy;
			to += copy;
		}
		start = end;
	}

	if (skb_shinfo(skb)->frag_list) {
		struct sk_buff *list;

		for (list = skb_shinfo(skb)->frag_list; list; list=list->next) {
			int end;

			BUG_TRAP(start <= offset+len);

			end = start + list->len;
			if ((copy = end-offset) > 0) {
				if (copy > len)
					copy = len;
				if (skb_copy_bits(list, offset-start, to, copy))
					goto fault;
				if ((len -= copy) == 0)
					return 0;
				offset += copy;
				to += copy;
			}
			start = end;
		}
	}
	if (len == 0)
		return 0;

fault:
	return -EFAULT;
}


unsigned int skb_checksum(const struct sk_buff *skb, int offset, int len, unsigned int csum)
{
	int i, copy;
	int start = skb->len - skb->data_len;
	int pos = 0;

	
	if ((copy = start-offset) > 0) {
		if (copy > len)
			copy = len;
		csum = csum_partial(skb->data+offset, copy, csum);
		if ((len -= copy) == 0)
			return csum;
		offset += copy;
		pos = copy;
	}

	for (i=0; i<skb_shinfo(skb)->nr_frags; i++) {
		int end;

		BUG_TRAP(start <= offset+len);

		end = start + skb_shinfo(skb)->frags[i].size;
		if ((copy = end-offset) > 0) {
			unsigned int csum2;
			u8 *vaddr;
			skb_frag_t *frag = &skb_shinfo(skb)->frags[i];

			if (copy > len)
				copy = len;
			vaddr = kmap_skb_frag(frag);
			csum2 = csum_partial(vaddr + frag->page_offset +
					     offset-start, copy, 0);
			kunmap_skb_frag(vaddr);
			csum = csum_block_add(csum, csum2, pos);
			if (!(len -= copy))
				return csum;
			offset += copy;
			pos += copy;
		}
		start = end;
	}

	if (skb_shinfo(skb)->frag_list) {
		struct sk_buff *list;

		for (list = skb_shinfo(skb)->frag_list; list; list=list->next) {
			int end;

			BUG_TRAP(start <= offset+len);

			end = start + list->len;
			if ((copy = end-offset) > 0) {
				unsigned int csum2;
				if (copy > len)
					copy = len;
				csum2 = skb_checksum(list, offset-start, copy, 0);
				csum = csum_block_add(csum, csum2, pos);
				if ((len -= copy) == 0)
					return csum;
				offset += copy;
				pos += copy;
			}
			start = end;
		}
	}
	if (len == 0)
		return csum;

	BUG();
	return csum;
}


unsigned int skb_copy_and_csum_bits(const struct sk_buff *skb, int offset, u8 *to, int len, unsigned int csum)
{
	int i, copy;
	int start = skb->len - skb->data_len;
	int pos = 0;

	
	if ((copy = start-offset) > 0) {
		if (copy > len)
			copy = len;
		csum = csum_partial_copy_nocheck(skb->data+offset, to, copy, csum);
		if ((len -= copy) == 0)
			return csum;
		offset += copy;
		to += copy;
		pos = copy;
	}

	for (i=0; i<skb_shinfo(skb)->nr_frags; i++) {
		int end;

		BUG_TRAP(start <= offset+len);

		end = start + skb_shinfo(skb)->frags[i].size;
		if ((copy = end-offset) > 0) {
			unsigned int csum2;
			u8 *vaddr;
			skb_frag_t *frag = &skb_shinfo(skb)->frags[i];

			if (copy > len)
				copy = len;
			vaddr = kmap_skb_frag(frag);
			csum2 = csum_partial_copy_nocheck(vaddr + frag->page_offset +
						      offset-start, to, copy, 0);
			kunmap_skb_frag(vaddr);
			csum = csum_block_add(csum, csum2, pos);
			if (!(len -= copy))
				return csum;
			offset += copy;
			to += copy;
			pos += copy;
		}
		start = end;
	}

	if (skb_shinfo(skb)->frag_list) {
		struct sk_buff *list;

		for (list = skb_shinfo(skb)->frag_list; list; list=list->next) {
			unsigned int csum2;
			int end;

			BUG_TRAP(start <= offset+len);

			end = start + list->len;
			if ((copy = end-offset) > 0) {
				if (copy > len)
					copy = len;
				csum2 = skb_copy_and_csum_bits(list, offset-start, to, copy, 0);
				csum = csum_block_add(csum, csum2, pos);
				if ((len -= copy) == 0)
					return csum;
				offset += copy;
				to += copy;
				pos += copy;
			}
			start = end;
		}
	}
	if (len == 0)
		return csum;

	BUG();
	return csum;
}

void skb_copy_and_csum_dev(const struct sk_buff *skb, u8 *to)
{
	unsigned int csum;
	long csstart;

	if (skb->ip_summed == CHECKSUM_HW)
		csstart = skb->h.raw - skb->data;
	else
		csstart = skb->len - skb->data_len;

	if (csstart > skb->len - skb->data_len)
		BUG();

	memcpy(to, skb->data, csstart);

	csum = 0;
	if (csstart != skb->len)
		csum = skb_copy_and_csum_bits(skb, csstart, to+csstart,
				skb->len-csstart, 0);

	if (skb->ip_summed == CHECKSUM_HW) {
		long csstuff = csstart + skb->csum;

		*((unsigned short *)(to + csstuff)) = csum_fold(csum);
	}
}

#if 0
void skb_add_mtu(int mtu)
{
	
	mtu = SKB_DATA_ALIGN(mtu) + sizeof(struct skb_shared_info);

	kmem_add_cache_size(mtu);
}
#endif

void __init skb_init(void)
{
	int i;

	skbuff_head_cache = kmem_cache_create("skbuff_head_cache",
					      sizeof(struct sk_buff),
					      0,
					      SLAB_HWCACHE_ALIGN,
					      skb_headerinit, NULL);
	if (!skbuff_head_cache)
		panic("cannot create skbuff cache");

	for (i=0; i<NR_CPUS; i++)
		skb_queue_head_init(&skb_head_pool[i].list);
}
