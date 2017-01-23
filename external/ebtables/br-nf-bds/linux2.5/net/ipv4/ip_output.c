
#include <asm/uaccess.h>
#include <asm/system.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/config.h>

#include <linux/socket.h>
#include <linux/sockios.h>
#include <linux/in.h>
#include <linux/inet.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/proc_fs.h>
#include <linux/stat.h>
#include <linux/init.h>

#include <net/snmp.h>
#include <net/ip.h>
#include <net/protocol.h>
#include <net/route.h>
#include <net/tcp.h>
#include <net/udp.h>
#include <linux/skbuff.h>
#include <net/sock.h>
#include <net/arp.h>
#include <net/icmp.h>
#include <net/raw.h>
#include <net/checksum.h>
#include <net/inetpeer.h>
#include <linux/igmp.h>
#include <linux/netfilter_ipv4.h>
#include <linux/mroute.h>
#include <linux/netlink.h>


int sysctl_ip_dynaddr = 0;
int sysctl_ip_default_ttl = IPDEFTTL;

__inline__ void ip_send_check(struct iphdr *iph)
{
	iph->check = 0;
	iph->check = ip_fast_csum((unsigned char *)iph, iph->ihl);
}

static int ip_dev_loopback_xmit(struct sk_buff *newskb)
{
	newskb->mac.raw = newskb->data;
	__skb_pull(newskb, newskb->nh.raw - newskb->data);
	newskb->pkt_type = PACKET_LOOPBACK;
	newskb->ip_summed = CHECKSUM_UNNECESSARY;
	BUG_TRAP(newskb->dst);

#ifdef CONFIG_NETFILTER_DEBUG
	nf_debug_ip_loopback_xmit(newskb);
#endif
	netif_rx(newskb);
	return 0;
}

int ip_build_and_send_pkt(struct sk_buff *skb, struct sock *sk,
			  u32 saddr, u32 daddr, struct ip_options *opt)
{
	struct inet_opt *inet = inet_sk(sk);
	struct rtable *rt = (struct rtable *)skb->dst;
	struct iphdr *iph;

	
	if (opt)
		iph=(struct iphdr *)skb_push(skb,sizeof(struct iphdr) + opt->optlen);
	else
		iph=(struct iphdr *)skb_push(skb,sizeof(struct iphdr));

	iph->version  = 4;
	iph->ihl      = 5;
	iph->tos      = inet->tos;
	if (ip_dont_fragment(sk, &rt->u.dst))
		iph->frag_off = htons(IP_DF);
	else
		iph->frag_off = 0;
	iph->ttl      = inet->ttl;
	iph->daddr    = rt->rt_dst;
	iph->saddr    = rt->rt_src;
	iph->protocol = sk->protocol;
	iph->tot_len  = htons(skb->len);
	ip_select_ident(iph, &rt->u.dst, sk);
	skb->nh.iph   = iph;

	if (opt && opt->optlen) {
		iph->ihl += opt->optlen>>2;
		ip_options_build(skb, opt, daddr, rt, 0);
	}
	ip_send_check(iph);

	skb->priority = sk->priority;

	
	return NF_HOOK(PF_INET, NF_IP_LOCAL_OUT, skb, NULL, rt->u.dst.dev,
		       dst_output);
}

static inline int ip_finish_output2(struct sk_buff *skb)
{
	struct dst_entry *dst = skb->dst;
	struct hh_cache *hh = dst->hh;
	struct net_device *dev = dst->dev;

	
	if (unlikely(skb_headroom(skb) < dev->hard_header_len
		     && dev->hard_header)) {
		struct sk_buff *skb2;

		skb2 = skb_realloc_headroom(skb, (dev->hard_header_len&~15) + 16);
		if (skb2 == NULL) {
			kfree_skb(skb);
			return -ENOMEM;
		}
		if (skb->sk)
			skb_set_owner_w(skb2, skb->sk);
		kfree_skb(skb);
		skb = skb2;
	}

#ifdef CONFIG_NETFILTER_DEBUG
	nf_debug_ip_finish_output2(skb);
#endif 

	if (hh) {
		read_lock_bh(&hh->hh_lock);
  		memcpy(skb->data - 16, hh->hh_data, 16);
		read_unlock_bh(&hh->hh_lock);
	        skb_push(skb, hh->hh_len);
		return hh->hh_output(skb);
	} else if (dst->neighbour)
		return dst->neighbour->output(skb);

	if (net_ratelimit())
		printk(KERN_DEBUG "ip_finish_output2: No header cache and no neighbour!\n");
	kfree_skb(skb);
	return -EINVAL;
}

__inline__ int ip_finish_output(struct sk_buff *skb)
{
	struct net_device *dev = skb->dst->dev;

	skb->dev = dev;
	skb->protocol = htons(ETH_P_IP);

	return NF_HOOK(PF_INET, NF_IP_POST_ROUTING, skb, NULL, dev,
		       ip_finish_output2);
}

int ip_mc_output(struct sk_buff *skb)
{
	struct sock *sk = skb->sk;
	struct rtable *rt = (struct rtable*)skb->dst;
	struct net_device *dev = rt->u.dst.dev;

	IP_INC_STATS(IpOutRequests);

	skb->dev = dev;
	skb->protocol = htons(ETH_P_IP);


	if (rt->rt_flags&RTCF_MULTICAST) {
		if ((!sk || inet_sk(sk)->mc_loop)
#ifdef CONFIG_IP_MROUTE
		    && ((rt->rt_flags&RTCF_LOCAL) || !(IPCB(skb)->flags&IPSKB_FORWARDED))
#endif
		) {
			struct sk_buff *newskb = skb_clone(skb, GFP_ATOMIC);
			if (newskb)
				NF_HOOK(PF_INET, NF_IP_POST_ROUTING, newskb, NULL,
					newskb->dev, 
					ip_dev_loopback_xmit);
		}

		

		if (skb->nh.iph->ttl == 0) {
			kfree_skb(skb);
			return 0;
		}
	}

	if (rt->rt_flags&RTCF_BROADCAST) {
		struct sk_buff *newskb = skb_clone(skb, GFP_ATOMIC);
		if (newskb)
			NF_HOOK(PF_INET, NF_IP_POST_ROUTING, newskb, NULL,
				newskb->dev, ip_dev_loopback_xmit);
	}

	if (skb->len > dev->mtu || skb_shinfo(skb)->frag_list)
		return ip_fragment(skb, ip_finish_output);
	else
		return ip_finish_output(skb);
}

int ip_output(struct sk_buff *skb)
{
	IP_INC_STATS(IpOutRequests);

	if ((skb->len > skb->dst->dev->mtu || skb_shinfo(skb)->frag_list) &&
	    !skb_shinfo(skb)->tso_size)
		return ip_fragment(skb, ip_finish_output);
	else
		return ip_finish_output(skb);
}

int ip_queue_xmit(struct sk_buff *skb)
{
	struct sock *sk = skb->sk;
	struct inet_opt *inet = inet_sk(sk);
	struct ip_options *opt = inet->opt;
	struct rtable *rt;
	struct iphdr *iph;

	rt = (struct rtable *) skb->dst;
	if (rt != NULL)
		goto packet_routed;

	
	rt = (struct rtable *)__sk_dst_check(sk, 0);
	if (rt == NULL) {
		u32 daddr;

		
		daddr = inet->daddr;
		if(opt && opt->srr)
			daddr = opt->faddr;

		{
			struct flowi fl = { .nl_u = { .ip4_u =
						      { .daddr = daddr,
							.saddr = inet->saddr,
							.tos = RT_CONN_FLAGS(sk) } },
					    .oif = sk->bound_dev_if };

			if (ip_route_output_key(&rt, &fl))
				goto no_route;
		}
		__sk_dst_set(sk, &rt->u.dst);
		tcp_v4_setup_caps(sk, &rt->u.dst);
	}
	skb->dst = dst_clone(&rt->u.dst);

packet_routed:
	if (opt && opt->is_strictroute && rt->rt_dst != rt->rt_gateway)
		goto no_route;

	
	iph = (struct iphdr *) skb_push(skb, sizeof(struct iphdr) + (opt ? opt->optlen : 0));
	*((__u16 *)iph)	= htons((4 << 12) | (5 << 8) | (inet->tos & 0xff));
	iph->tot_len = htons(skb->len);
	if (ip_dont_fragment(sk, &rt->u.dst))
		iph->frag_off = htons(IP_DF);
	else
		iph->frag_off = 0;
	iph->ttl      = inet->ttl;
	iph->protocol = sk->protocol;
	iph->saddr    = rt->rt_src;
	iph->daddr    = rt->rt_dst;
	skb->nh.iph   = iph;
	

	if(opt && opt->optlen) {
		iph->ihl += opt->optlen >> 2;
		ip_options_build(skb, opt, inet->daddr, rt, 0);
	}

	if (skb->len > rt->u.dst.pmtu && (sk->route_caps&NETIF_F_TSO)) {
		unsigned int hlen;

		
		hlen = ((skb->h.raw - skb->data) + (skb->h.th->doff << 2));
		skb_shinfo(skb)->tso_size = rt->u.dst.pmtu - hlen;
		skb_shinfo(skb)->tso_segs =
			(skb->len - hlen + skb_shinfo(skb)->tso_size - 1)/
				skb_shinfo(skb)->tso_size - 1;
	}

	ip_select_ident_more(iph, &rt->u.dst, sk, skb_shinfo(skb)->tso_segs);

	
	ip_send_check(iph);

	skb->priority = sk->priority;

	return NF_HOOK(PF_INET, NF_IP_LOCAL_OUT, skb, NULL, rt->u.dst.dev,
		       dst_output);

no_route:
	IP_INC_STATS(IpOutNoRoutes);
	kfree_skb(skb);
	return -EHOSTUNREACH;
}


static void ip_copy_metadata(struct sk_buff *to, struct sk_buff *from)
{
	to->pkt_type = from->pkt_type;
	to->priority = from->priority;
	to->protocol = from->protocol;
	to->security = from->security;
	to->dst = dst_clone(from->dst);
	to->dev = from->dev;

	
	IPCB(to)->flags = IPCB(from)->flags;

#ifdef CONFIG_NET_SCHED
	to->tc_index = from->tc_index;
#endif
#ifdef CONFIG_NETFILTER
	to->nfmark = from->nfmark;
	
	to->nfct = from->nfct;
	nf_conntrack_get(to->nfct);
	to->nf_bridge = from->nf_bridge;
	nf_bridge_get(to->nf_bridge);
#ifdef CONFIG_NETFILTER_DEBUG
	to->nf_debug = from->nf_debug;
#endif
#endif
}


int ip_fragment(struct sk_buff *skb, int (*output)(struct sk_buff*))
{
	struct iphdr *iph;
	int raw = 0;
	int ptr;
	struct net_device *dev;
	struct sk_buff *skb2;
	unsigned int mtu, hlen, left, len; 
	int offset;
	int not_last_frag;
	struct rtable *rt = (struct rtable*)skb->dst;
	int err = 0;

	dev = rt->u.dst.dev;


	iph = skb->nh.iph;

	if (unlikely(iph->frag_off & htons(IP_DF))) {
		icmp_send(skb, ICMP_DEST_UNREACH, ICMP_FRAG_NEEDED,
			  htonl(rt->u.dst.pmtu));
		kfree_skb(skb);
		return -EMSGSIZE;
	}


	hlen = iph->ihl * 4;
	mtu = rt->u.dst.pmtu - hlen;	

	if (skb_shinfo(skb)->frag_list) {
		struct sk_buff *frag;
		int first_len = skb_pagelen(skb);

		if (first_len - hlen > mtu ||
		    ((first_len - hlen) & 7) ||
		    (iph->frag_off & htons(IP_MF|IP_OFFSET)) ||
		    skb_cloned(skb))
			goto slow_path;

		for (frag = skb_shinfo(skb)->frag_list; frag; frag = frag->next) {
			
			if (frag->len > mtu ||
			    ((frag->len & 7) && frag->next) ||
			    skb_headroom(frag) < hlen)
			    goto slow_path;

			
			if (frag->sk == NULL)
				goto slow_path;

			
			if (skb_shared(frag))
				goto slow_path;
		}

		

		err = 0;
		offset = 0;
		frag = skb_shinfo(skb)->frag_list;
		skb_shinfo(skb)->frag_list = 0;
		skb->data_len = first_len - skb_headlen(skb);
		skb->len = first_len;
		iph->tot_len = htons(first_len);
		iph->frag_off |= htons(IP_MF);
		ip_send_check(iph);

		for (;;) {
			if (frag) {
				frag->h.raw = frag->data;
				frag->nh.raw = __skb_push(frag, hlen);
				memcpy(frag->nh.raw, iph, hlen);
				iph = frag->nh.iph;
				iph->tot_len = htons(frag->len);
				ip_copy_metadata(frag, skb);
				if (offset == 0)
					ip_options_fragment(frag);
				offset += skb->len - hlen;
				iph->frag_off = htons(offset>>3);
				if (frag->next != NULL)
					iph->frag_off |= htons(IP_MF);
				
				ip_send_check(iph);
			}

			err = output(skb);

			if (err || !frag)
				break;

			skb = frag;
			frag = skb->next;
			skb->next = NULL;
		}

		if (err == 0) {
			IP_INC_STATS(IpFragOKs);
			return 0;
		}

		while (frag) {
			skb = frag->next;
			kfree_skb(frag);
			frag = skb;
		}
		IP_INC_STATS(IpFragFails);
		return err;
	}

slow_path:
	left = skb->len - hlen;		
	ptr = raw + hlen;		


	offset = (ntohs(iph->frag_off) & IP_OFFSET) << 3;
	not_last_frag = iph->frag_off & htons(IP_MF);


	while(left > 0)	{
		len = left;
		
		if (len > mtu)
			len = mtu;
		if (len < left)	{
			len &= ~7;
		}

		if ((skb2 = alloc_skb(len+hlen+rt->u.dst.dev->hard_header_len+16,GFP_ATOMIC)) == NULL) {
			NETDEBUG(printk(KERN_INFO "IP: frag: no memory for new fragment!\n"));
			err = -ENOMEM;
			goto fail;
		}


		ip_copy_metadata(skb2, skb);
		skb_reserve(skb2, (rt->u.dst.dev->hard_header_len&~15)+16);
		skb_put(skb2, len + hlen);
		skb2->nh.raw = skb2->data;
		skb2->h.raw = skb2->data + hlen;


		if (skb->sk)
			skb_set_owner_w(skb2, skb->sk);


		memcpy(skb2->nh.raw, skb->data, hlen);

		if (skb_copy_bits(skb, ptr, skb2->h.raw, len))
			BUG();
		left -= len;

		iph = skb2->nh.iph;
		iph->frag_off = htons((offset >> 3));

		if (offset == 0)
			ip_options_fragment(skb);

		if (left > 0 || not_last_frag)
			iph->frag_off |= htons(IP_MF);
		ptr += len;
		offset += len;


		IP_INC_STATS(IpFragCreates);

		iph->tot_len = htons(len + hlen);

		ip_send_check(iph);

		err = output(skb2);
		if (err)
			goto fail;
	}
	kfree_skb(skb);
	IP_INC_STATS(IpFragOKs);
	return err;

fail:
	kfree_skb(skb); 
	IP_INC_STATS(IpFragFails);
	return err;
}

int
ip_generic_getfrag(void *from, char *to, int offset, int len, int odd, struct sk_buff *skb)
{
	struct iovec *iov = from;

	if (skb->ip_summed == CHECKSUM_HW) {
		if (memcpy_fromiovecend(to, iov, offset, len) < 0)
			return -EFAULT;
	} else {
		unsigned int csum = 0;
		if (csum_partial_copy_fromiovecend(to, iov, offset, len, &csum) < 0)
			return -EFAULT;
		skb->csum = csum_block_add(skb->csum, csum, odd);
	}
	return 0;
}

static inline int
skb_can_coalesce(struct sk_buff *skb, int i, struct page *page, int off)
{
	if (i) {
		skb_frag_t *frag = &skb_shinfo(skb)->frags[i-1];
		return page == frag->page &&
			off == frag->page_offset+frag->size;
	}
	return 0;
}

static inline void
skb_fill_page_desc(struct sk_buff *skb, int i, struct page *page, int off, int size)
{
	skb_frag_t *frag = &skb_shinfo(skb)->frags[i];
	frag->page = page;
	frag->page_offset = off;
	frag->size = size;
	skb_shinfo(skb)->nr_frags = i+1;
}

static inline unsigned int
csum_page(struct page *page, int offset, int copy)
{
	char *kaddr;
	unsigned int csum;
	kaddr = kmap(page);
	csum = csum_partial(kaddr + offset, copy, 0);
	kunmap(page);
	return csum;
}

int ip_append_data(struct sock *sk,
		   int getfrag(void *from, char *to, int offset, int len,
			       int odd, struct sk_buff *skb),
		   void *from, int length, int transhdrlen,
		   struct ipcm_cookie *ipc, struct rtable *rt,
		   unsigned int flags)
{
	struct inet_opt *inet = inet_sk(sk);
	struct sk_buff *skb;

	struct ip_options *opt = NULL;
	int hh_len;
	int exthdrlen;
	int mtu;
	int copy;
	int err;
	int offset = 0;
	unsigned int maxfraglen, fragheaderlen;
	int csummode = CHECKSUM_NONE;

	if (flags&MSG_PROBE)
		return 0;

	if (skb_queue_empty(&sk->write_queue)) {
		opt = ipc->opt;
		if (opt) {
			if (inet->cork.opt == NULL)
				inet->cork.opt = kmalloc(sizeof(struct ip_options)+40, sk->allocation);
			memcpy(inet->cork.opt, opt, sizeof(struct ip_options)+opt->optlen);
			inet->cork.flags |= IPCORK_OPT;
			inet->cork.addr = ipc->addr;
		}
		dst_hold(&rt->u.dst);
		inet->cork.fragsize = mtu = rt->u.dst.pmtu;
		inet->cork.rt = rt;
		inet->cork.length = 0;
		inet->sndmsg_page = NULL;
		inet->sndmsg_off = 0;
		if ((exthdrlen = rt->u.dst.header_len) != 0) {
			length += exthdrlen;
			transhdrlen += exthdrlen;
		}
	} else {
		rt = inet->cork.rt;
		if (inet->cork.flags & IPCORK_OPT)
			opt = inet->cork.opt;

		transhdrlen = 0;
		exthdrlen = 0;
		mtu = inet->cork.fragsize;
	}
	hh_len = (rt->u.dst.dev->hard_header_len&~15) + 16;

	fragheaderlen = sizeof(struct iphdr) + (opt ? opt->optlen : 0);
	maxfraglen = ((mtu-fragheaderlen) & ~7) + fragheaderlen;

	if (inet->cork.length + length > 0xFFFF - fragheaderlen) {
		ip_local_error(sk, EMSGSIZE, rt->rt_dst, inet->dport, mtu-exthdrlen);
		return -EMSGSIZE;
	}

	if (transhdrlen &&
	    length + fragheaderlen <= maxfraglen &&
	    rt->u.dst.dev->features&(NETIF_F_IP_CSUM|NETIF_F_NO_CSUM|NETIF_F_HW_CSUM) &&
	    !exthdrlen)
		csummode = CHECKSUM_HW;

	inet->cork.length += length;

	if ((skb = skb_peek_tail(&sk->write_queue)) == NULL)
		goto alloc_new_skb;

	while (length > 0) {
		if ((copy = maxfraglen - skb->len) <= 0) {
			char *data;
			unsigned int datalen;
			unsigned int fraglen;
			unsigned int alloclen;
			BUG_TRAP(copy == 0);

alloc_new_skb:
			datalen = maxfraglen - fragheaderlen;
			if (datalen > length)
				datalen = length;

			fraglen = datalen + fragheaderlen;
			if ((flags & MSG_MORE) && 
			    !(rt->u.dst.dev->features&NETIF_F_SG))
				alloclen = maxfraglen;
			else
				alloclen = datalen + fragheaderlen;
			if (!(flags & MSG_DONTWAIT) || transhdrlen) {
				skb = sock_alloc_send_skb(sk, 
						alloclen + hh_len + 15,
						(flags & MSG_DONTWAIT), &err);
			} else {
				skb = sock_wmalloc(sk, 
						alloclen + hh_len + 15, 1,
						sk->allocation);
				if (unlikely(skb == NULL))
					err = -ENOBUFS;
			}
			if (skb == NULL)
				goto error;

			skb->ip_summed = csummode;
			skb->csum = 0;
			skb_reserve(skb, hh_len);

			data = skb_put(skb, fraglen);
			skb->nh.raw = __skb_pull(skb, exthdrlen);
			data += fragheaderlen;
			skb->h.raw = data + exthdrlen;

			copy = datalen - transhdrlen;
			if (copy > 0 && getfrag(from, data + transhdrlen, offset, copy, 0, skb) < 0) {
				err = -EFAULT;
				kfree_skb(skb);
				goto error;
			}

			offset += copy;
			length -= datalen;
			transhdrlen = 0;
			exthdrlen = 0;
			csummode = CHECKSUM_NONE;

			__skb_queue_tail(&sk->write_queue, skb);
			continue;
		}

		if (copy > length)
			copy = length;

		if (!(rt->u.dst.dev->features&NETIF_F_SG)) {
			unsigned int off;

			off = skb->len;
			if (getfrag(from, skb_put(skb, copy), 
					offset, copy, off, skb) < 0) {
				__skb_trim(skb, off);
				err = -EFAULT;
				goto error;
			}
		} else {
			int i = skb_shinfo(skb)->nr_frags;
			skb_frag_t *frag = &skb_shinfo(skb)->frags[i-1];
			struct page *page = inet->sndmsg_page;
			int off = inet->sndmsg_off;
			unsigned int left;

			if (page && (left = PAGE_SIZE - off) > 0) {
				if (copy >= left)
					copy = left;
				if (page != frag->page) {
					if (i == MAX_SKB_FRAGS) {
						err = -EMSGSIZE;
						goto error;
					}
					get_page(page);
	 				skb_fill_page_desc(skb, i, page, inet->sndmsg_off, 0);
					frag = &skb_shinfo(skb)->frags[i];
				}
			} else if (i < MAX_SKB_FRAGS) {
				if (copy > PAGE_SIZE)
					copy = PAGE_SIZE;
				page = alloc_pages(sk->allocation, 0);
				if (page == NULL)  {
					err = -ENOMEM;
					goto error;
				}
				inet->sndmsg_page = page;
				inet->sndmsg_off = 0;

				skb_fill_page_desc(skb, i, page, 0, 0);
				frag = &skb_shinfo(skb)->frags[i];
				skb->truesize += PAGE_SIZE;
				atomic_add(PAGE_SIZE, &sk->wmem_alloc);
			} else {
				err = -EMSGSIZE;
				goto error;
			}
			if (getfrag(from, page_address(frag->page)+frag->page_offset+frag->size, offset, copy, skb->len, skb) < 0) {
				err = -EFAULT;
				goto error;
			}
			inet->sndmsg_off += copy;
			frag->size += copy;
			skb->len += copy;
			skb->data_len += copy;
		}
		offset += copy;
		length -= copy;
	}

	return 0;

error:
	inet->cork.length -= length;
	IP_INC_STATS(IpOutDiscards);
	return err; 
}

ssize_t	ip_append_page(struct sock *sk, struct page *page,
		       int offset, size_t size, int flags)
{
	struct inet_opt *inet = inet_sk(sk);
	struct sk_buff *skb;
	struct rtable *rt;
	struct ip_options *opt = NULL;
	int hh_len;
	int mtu;
	int len;
	int err;
	unsigned int maxfraglen, fragheaderlen;

	if (inet->hdrincl)
		return -EPERM;

	if (flags&MSG_PROBE)
		return 0;

	if (skb_queue_empty(&sk->write_queue))
		return -EINVAL;

	rt = inet->cork.rt;
	if (inet->cork.flags & IPCORK_OPT)
		opt = inet->cork.opt;

	if (!(rt->u.dst.dev->features&NETIF_F_SG))
		return -EOPNOTSUPP;

	hh_len = (rt->u.dst.dev->hard_header_len&~15)+16;
	mtu = inet->cork.fragsize;

	fragheaderlen = sizeof(struct iphdr) + (opt ? opt->optlen : 0);
	maxfraglen = ((mtu-fragheaderlen) & ~7) + fragheaderlen;

	if (inet->cork.length + size > 0xFFFF - fragheaderlen) {
		ip_local_error(sk, EMSGSIZE, rt->rt_dst, inet->dport, mtu);
		return -EMSGSIZE;
	}

	if ((skb = skb_peek_tail(&sk->write_queue)) == NULL)
		return -EINVAL;

	inet->cork.length += size;

	while (size > 0) {
		int i;
		if ((len = maxfraglen - skb->len) <= 0) {
			char *data;
			struct iphdr *iph;
			BUG_TRAP(len == 0);

			skb = sock_wmalloc(sk, fragheaderlen + hh_len + 15, 1,
					   sk->allocation);
			if (unlikely(!skb)) {
				err = -ENOBUFS;
				goto error;
			}

			skb->ip_summed = CHECKSUM_NONE;
			skb->csum = 0;
			skb_reserve(skb, hh_len);

			data = skb_put(skb, fragheaderlen);
			skb->nh.iph = iph = (struct iphdr *)data;
			data += fragheaderlen;
			skb->h.raw = data;

			__skb_queue_tail(&sk->write_queue, skb);
			continue;
		}

		i = skb_shinfo(skb)->nr_frags;
		if (len > size)
			len = size;
		if (skb_can_coalesce(skb, i, page, offset)) {
			skb_shinfo(skb)->frags[i-1].size += len;
		} else if (i < MAX_SKB_FRAGS) {
			get_page(page);
			skb_fill_page_desc(skb, i, page, offset, len);
		} else {
			err = -EMSGSIZE;
			goto error;
		}

		if (skb->ip_summed == CHECKSUM_NONE) {
			unsigned int csum;
			csum = csum_page(page, offset, len);
			skb->csum = csum_block_add(skb->csum, csum, skb->len);
		}

		skb->len += len;
		skb->data_len += len;
		offset += len;
		size -= len;
	}
	return 0;

error:
	inet->cork.length -= size;
	IP_INC_STATS(IpOutDiscards);
	return err;
}

int ip_push_pending_frames(struct sock *sk)
{
	struct sk_buff *skb, *tmp_skb;
	struct sk_buff **tail_skb;
	struct inet_opt *inet = inet_sk(sk);
	struct ip_options *opt = NULL;
	struct rtable *rt = inet->cork.rt;
	struct iphdr *iph;
	int df = 0;
	__u8 ttl;
	int err = 0;

	if ((skb = __skb_dequeue(&sk->write_queue)) == NULL)
		goto out;
	tail_skb = &(skb_shinfo(skb)->frag_list);

	while ((tmp_skb = __skb_dequeue(&sk->write_queue)) != NULL) {
		__skb_pull(tmp_skb, skb->h.raw - skb->nh.raw);
		*tail_skb = tmp_skb;
		tail_skb = &(tmp_skb->next);
		skb->len += tmp_skb->len;
		skb->data_len += tmp_skb->len;
#if 0 
		skb->truesize += tmp_skb->truesize;
		__sock_put(tmp_skb->sk);
		tmp_skb->destructor = NULL;
		tmp_skb->sk = NULL;
#endif
	}

	if (inet->pmtudisc == IP_PMTUDISC_DO ||
	    (!skb_shinfo(skb)->frag_list && ip_dont_fragment(sk, &rt->u.dst)))
		df = htons(IP_DF);

	if (inet->cork.flags & IPCORK_OPT)
		opt = inet->cork.opt;

	if (rt->rt_type == RTN_MULTICAST)
		ttl = inet->mc_ttl;
	else
		ttl = inet->ttl;

	iph = (struct iphdr *)skb->data;
	iph->version = 4;
	iph->ihl = 5;
	if (opt) {
		iph->ihl += opt->optlen>>2;
		ip_options_build(skb, opt, inet->cork.addr, rt, 0);
	}
	iph->tos = inet->tos;
	iph->tot_len = htons(skb->len);
	iph->frag_off = df;
	if (!df) {
		__ip_select_ident(iph, &rt->u.dst, 0);
	} else {
		iph->id = htons(inet->id++);
	}
	iph->ttl = ttl;
	iph->protocol = sk->protocol;
	iph->saddr = rt->rt_src;
	iph->daddr = rt->rt_dst;
	ip_send_check(iph);

	skb->priority = sk->priority;
	skb->dst = dst_clone(&rt->u.dst);

	
	err = NF_HOOK(PF_INET, NF_IP_LOCAL_OUT, skb, NULL, 
		      skb->dst->dev, dst_output);
	if (err) {
		if (err > 0)
			err = inet->recverr ? net_xmit_errno(err) : 0;
		if (err)
			goto error;
	}

out:
	inet->cork.flags &= ~IPCORK_OPT;
	if (inet->cork.rt) {
		ip_rt_put(inet->cork.rt);
		inet->cork.rt = NULL;
	}
	return err;

error:
	IP_INC_STATS(IpOutDiscards);
	goto out;
}

void ip_flush_pending_frames(struct sock *sk)
{
	struct inet_opt *inet = inet_sk(sk);
	struct sk_buff *skb;

	while ((skb = __skb_dequeue_tail(&sk->write_queue)) != NULL)
		kfree_skb(skb);

	inet->cork.flags &= ~IPCORK_OPT;
	if (inet->cork.opt) {
		kfree(inet->cork.opt);
		inet->cork.opt = NULL;
	}
	if (inet->cork.rt) {
		ip_rt_put(inet->cork.rt);
		inet->cork.rt = NULL;
	}
}


static int ip_reply_glue_bits(void *dptr, char *to, int offset, 
			      int len, int odd, struct sk_buff *skb)
{
	unsigned int csum;

	csum = csum_partial_copy_nocheck(dptr+offset, to, len, 0);
	skb->csum = csum_block_add(skb->csum, csum, odd);
	return 0;  
}

void ip_send_reply(struct sock *sk, struct sk_buff *skb, struct ip_reply_arg *arg,
		   unsigned int len)
{
	struct inet_opt *inet = inet_sk(sk);
	struct {
		struct ip_options	opt;
		char			data[40];
	} replyopts;
	struct ipcm_cookie ipc;
	u32 daddr;
	struct rtable *rt = (struct rtable*)skb->dst;

	if (ip_options_echo(&replyopts.opt, skb))
		return;

	daddr = ipc.addr = rt->rt_src;
	ipc.opt = NULL;

	if (replyopts.opt.optlen) {
		ipc.opt = &replyopts.opt;

		if (ipc.opt->srr)
			daddr = replyopts.opt.faddr;
	}

	{
		struct flowi fl = { .nl_u = { .ip4_u =
					      { .daddr = daddr,
						.saddr = rt->rt_spec_dst,
						.tos = RT_TOS(skb->nh.iph->tos) } } };
		if (ip_route_output_key(&rt, &fl))
			return;
	}

	bh_lock_sock(sk);
	inet->tos = skb->nh.iph->tos;
	sk->priority = skb->priority;
	sk->protocol = skb->nh.iph->protocol;
	ip_append_data(sk, ip_reply_glue_bits, arg->iov->iov_base, len, 0,
		       &ipc, rt, MSG_DONTWAIT);
	if ((skb = skb_peek(&sk->write_queue)) != NULL) {
		if (arg->csumoffset >= 0)
			*((u16 *)skb->h.raw + arg->csumoffset) = csum_fold(csum_add(skb->csum, arg->csum));
		skb->ip_summed = CHECKSUM_NONE;
		ip_push_pending_frames(sk);
	}

	bh_unlock_sock(sk);

	ip_rt_put(rt);
}


static struct packet_type ip_packet_type =
{
	__constant_htons(ETH_P_IP),
	NULL,	
	ip_rcv,
	(void*)1,
	NULL,
};


void __init ip_init(void)
{
	dev_add_pack(&ip_packet_type);

	ip_rt_init();
	inet_initpeers();

#ifdef CONFIG_IP_MULTICAST
	proc_net_create("igmp", 0, ip_mc_procinfo);
#endif
}
