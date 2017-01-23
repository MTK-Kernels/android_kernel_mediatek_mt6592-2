
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

static inline int
output_maybe_reroute(struct sk_buff *skb)
{
	return skb->dst->output(skb);
}

int ip_build_and_send_pkt(struct sk_buff *skb, struct sock *sk,
			  u32 saddr, u32 daddr, struct ip_options *opt)
{
	struct rtable *rt = (struct rtable *)skb->dst;
	struct iphdr *iph;

	
	if (opt)
		iph=(struct iphdr *)skb_push(skb,sizeof(struct iphdr) + opt->optlen);
	else
		iph=(struct iphdr *)skb_push(skb,sizeof(struct iphdr));

	iph->version  = 4;
	iph->ihl      = 5;
	iph->tos      = sk->protinfo.af_inet.tos;
	if (ip_dont_fragment(sk, &rt->u.dst))
		iph->frag_off = htons(IP_DF);
	else
		iph->frag_off = 0;
	iph->ttl      = sk->protinfo.af_inet.ttl;
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

	
	return NF_HOOK(PF_INET, NF_IP_LOCAL_OUT, skb, NULL, rt->u.dst.dev,
		       output_maybe_reroute);
}

static inline int ip_finish_output2(struct sk_buff *skb)
{
	struct dst_entry *dst = skb->dst;
	struct hh_cache *hh = dst->hh;

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
	skb->protocol = __constant_htons(ETH_P_IP);

	return NF_HOOK(PF_INET, NF_IP_POST_ROUTING, skb, NULL, dev,
		       ip_finish_output2);
}

int ip_mc_output(struct sk_buff *skb)
{
	struct sock *sk = skb->sk;
	struct rtable *rt = (struct rtable*)skb->dst;
	struct net_device *dev = rt->u.dst.dev;

	IP_INC_STATS(IpOutRequests);
#ifdef CONFIG_IP_ROUTE_NAT
	if (rt->rt_flags & RTCF_NAT)
		ip_do_nat(skb);
#endif

	skb->dev = dev;
	skb->protocol = __constant_htons(ETH_P_IP);


	if (rt->rt_flags&RTCF_MULTICAST) {
		if ((!sk || sk->protinfo.af_inet.mc_loop)
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

	return ip_finish_output(skb);
}

int ip_output(struct sk_buff *skb)
{
#ifdef CONFIG_IP_ROUTE_NAT
	struct rtable *rt = (struct rtable*)skb->dst;
#endif

	IP_INC_STATS(IpOutRequests);

#ifdef CONFIG_IP_ROUTE_NAT
	if (rt->rt_flags&RTCF_NAT)
		ip_do_nat(skb);
#endif

	return ip_finish_output(skb);
}

static inline int ip_queue_xmit2(struct sk_buff *skb)
{
	struct sock *sk = skb->sk;
	struct rtable *rt = (struct rtable *)skb->dst;
	struct net_device *dev;
	struct iphdr *iph = skb->nh.iph;

	dev = rt->u.dst.dev;

	if (skb_headroom(skb) < dev->hard_header_len && dev->hard_header) {
		struct sk_buff *skb2;

		skb2 = skb_realloc_headroom(skb, (dev->hard_header_len + 15) & ~15);
		kfree_skb(skb);
		if (skb2 == NULL)
			return -ENOMEM;
		if (sk)
			skb_set_owner_w(skb2, sk);
		skb = skb2;
		iph = skb->nh.iph;
	}

	if (skb->len > rt->u.dst.pmtu)
		goto fragment;

	ip_select_ident(iph, &rt->u.dst, sk);

	
	ip_send_check(iph);

	skb->priority = sk->priority;
	return skb->dst->output(skb);

fragment:
	if (ip_dont_fragment(sk, &rt->u.dst)) {
		NETDEBUG(printk(KERN_DEBUG "sending pkt_too_big (len[%u] pmtu[%u]) to self\n",
				skb->len, rt->u.dst.pmtu));

		icmp_send(skb, ICMP_DEST_UNREACH, ICMP_FRAG_NEEDED,
			  htonl(rt->u.dst.pmtu));
		kfree_skb(skb);
		return -EMSGSIZE;
	}
	ip_select_ident(iph, &rt->u.dst, sk);
	if (skb->ip_summed == CHECKSUM_HW &&
	    (skb = skb_checksum_help(skb)) == NULL)
		return -ENOMEM;
	return ip_fragment(skb, skb->dst->output);
}

int ip_queue_xmit(struct sk_buff *skb)
{
	struct sock *sk = skb->sk;
	struct ip_options *opt = sk->protinfo.af_inet.opt;
	struct rtable *rt;
	struct iphdr *iph;

	rt = (struct rtable *) skb->dst;
	if (rt != NULL)
		goto packet_routed;

	
	rt = (struct rtable *)__sk_dst_check(sk, 0);
	if (rt == NULL) {
		u32 daddr;

		
		daddr = sk->daddr;
		if(opt && opt->srr)
			daddr = opt->faddr;

		if (ip_route_output(&rt, daddr, sk->saddr,
				    RT_CONN_FLAGS(sk),
				    sk->bound_dev_if))
			goto no_route;
		__sk_dst_set(sk, &rt->u.dst);
		sk->route_caps = rt->u.dst.dev->features;
	}
	skb->dst = dst_clone(&rt->u.dst);

packet_routed:
	if (opt && opt->is_strictroute && rt->rt_dst != rt->rt_gateway)
		goto no_route;

	
	iph = (struct iphdr *) skb_push(skb, sizeof(struct iphdr) + (opt ? opt->optlen : 0));
	*((__u16 *)iph)	= htons((4 << 12) | (5 << 8) | (sk->protinfo.af_inet.tos & 0xff));
	iph->tot_len = htons(skb->len);
	if (ip_dont_fragment(sk, &rt->u.dst))
		iph->frag_off = __constant_htons(IP_DF);
	else
		iph->frag_off = 0;
	iph->ttl      = sk->protinfo.af_inet.ttl;
	iph->protocol = sk->protocol;
	iph->saddr    = rt->rt_src;
	iph->daddr    = rt->rt_dst;
	skb->nh.iph   = iph;
	

	if(opt && opt->optlen) {
		iph->ihl += opt->optlen >> 2;
		ip_options_build(skb, opt, sk->daddr, rt, 0);
	}

	return NF_HOOK(PF_INET, NF_IP_LOCAL_OUT, skb, NULL, rt->u.dst.dev,
		       ip_queue_xmit2);

no_route:
	IP_INC_STATS(IpOutNoRoutes);
	kfree_skb(skb);
	return -EHOSTUNREACH;
}


static int ip_build_xmit_slow(struct sock *sk,
		  int getfrag (const void *,
			       char *,
			       unsigned int,	
			       unsigned int),
		  const void *frag,
		  unsigned length,
		  struct ipcm_cookie *ipc,
		  struct rtable *rt,
		  int flags)
{
	unsigned int fraglen, maxfraglen, fragheaderlen;
	int err;
	int offset, mf;
	int mtu;
	u16 id;

	int hh_len = (rt->u.dst.dev->hard_header_len + 15)&~15;
	int nfrags=0;
	struct ip_options *opt = ipc->opt;
	int df = 0;

	mtu = rt->u.dst.pmtu;
	if (ip_dont_fragment(sk, &rt->u.dst))
		df = htons(IP_DF);

	length -= sizeof(struct iphdr);

	if (opt) {
		fragheaderlen = sizeof(struct iphdr) + opt->optlen;
		maxfraglen = ((mtu-sizeof(struct iphdr)-opt->optlen) & ~7) + fragheaderlen;
	} else {
		fragheaderlen = sizeof(struct iphdr);


		maxfraglen = ((mtu-sizeof(struct iphdr)) & ~7) + fragheaderlen;
	}

	if (length + fragheaderlen > 0xFFFF) {
		ip_local_error(sk, EMSGSIZE, rt->rt_dst, sk->dport, mtu);
		return -EMSGSIZE;
	}


	offset = length - (length % (maxfraglen - fragheaderlen));


	fraglen = length - offset + fragheaderlen;

	if (length-offset==0) {
		fraglen = maxfraglen;
		offset -= maxfraglen-fragheaderlen;
	}


	mf = 0;


	if (offset > 0 && sk->protinfo.af_inet.pmtudisc==IP_PMTUDISC_DO) { 
		ip_local_error(sk, EMSGSIZE, rt->rt_dst, sk->dport, mtu);
 		return -EMSGSIZE;
	}
	if (flags&MSG_PROBE)
		goto out;


	id = sk->protinfo.af_inet.id++;

	do {
		char *data;
		struct sk_buff * skb;

		if (!(flags & MSG_DONTWAIT) || nfrags == 0) {
			skb = sock_alloc_send_skb(sk, fraglen + hh_len + 15,
						  (flags & MSG_DONTWAIT), &err);
		} else {
			skb = sock_wmalloc(sk, fraglen + hh_len + 15, 1,
					   sk->allocation);
			if (!skb)
				err = -ENOBUFS;
		}
		if (skb == NULL)
			goto error;


		skb->priority = sk->priority;
		skb->dst = dst_clone(&rt->u.dst);
		skb_reserve(skb, hh_len);


		data = skb_put(skb, fraglen);
		skb->nh.iph = (struct iphdr *)data;


		{
			struct iphdr *iph = (struct iphdr *)data;

			iph->version = 4;
			iph->ihl = 5;
			if (opt) {
				iph->ihl += opt->optlen>>2;
				ip_options_build(skb, opt,
						 ipc->addr, rt, offset);
			}
			iph->tos = sk->protinfo.af_inet.tos;
			iph->tot_len = htons(fraglen - fragheaderlen + iph->ihl*4);
			iph->frag_off = htons(offset>>3)|mf|df;
			iph->id = id;
			if (!mf) {
				if (offset || !df) {
					__ip_select_ident(iph, &rt->u.dst);
					id = iph->id;
				}

				mf = htons(IP_MF);
			}
			if (rt->rt_type == RTN_MULTICAST)
				iph->ttl = sk->protinfo.af_inet.mc_ttl;
			else
				iph->ttl = sk->protinfo.af_inet.ttl;
			iph->protocol = sk->protocol;
			iph->check = 0;
			iph->saddr = rt->rt_src;
			iph->daddr = rt->rt_dst;
			iph->check = ip_fast_csum((unsigned char *)iph, iph->ihl);
			data += iph->ihl*4;
		}


		if (getfrag(frag, data, offset, fraglen-fragheaderlen)) {
			err = -EFAULT;
			kfree_skb(skb);
			goto error;
		}

		offset -= (maxfraglen-fragheaderlen);
		fraglen = maxfraglen;

		nfrags++;

		err = NF_HOOK(PF_INET, NF_IP_LOCAL_OUT, skb, NULL, 
			      skb->dst->dev, output_maybe_reroute);
		if (err) {
			if (err > 0)
				err = sk->protinfo.af_inet.recverr ? net_xmit_errno(err) : 0;
			if (err)
				goto error;
		}
	} while (offset >= 0);

	if (nfrags>1)
		ip_statistics[smp_processor_id()*2 + !in_softirq()].IpFragCreates += nfrags;
out:
	return 0;

error:
	IP_INC_STATS(IpOutDiscards);
	if (nfrags>1)
		ip_statistics[smp_processor_id()*2 + !in_softirq()].IpFragCreates += nfrags;
	return err; 
}

int ip_build_xmit(struct sock *sk, 
		  int getfrag (const void *,
			       char *,
			       unsigned int,	
			       unsigned int),
		  const void *frag,
		  unsigned length,
		  struct ipcm_cookie *ipc,
		  struct rtable *rt,
		  int flags)
{
	int err;
	struct sk_buff *skb;
	int df;
	struct iphdr *iph;


	if (!sk->protinfo.af_inet.hdrincl) {
		length += sizeof(struct iphdr);

		if (length > rt->u.dst.pmtu || ipc->opt != NULL)  
			return ip_build_xmit_slow(sk,getfrag,frag,length,ipc,rt,flags); 
	} else {
		if (length > rt->u.dst.dev->mtu) {
			ip_local_error(sk, EMSGSIZE, rt->rt_dst, sk->dport, rt->u.dst.dev->mtu);
			return -EMSGSIZE;
		}
	}
	if (flags&MSG_PROBE)
		goto out;

	df = 0;
	if (ip_dont_fragment(sk, &rt->u.dst))
		df = htons(IP_DF);

 
	{
	int hh_len = (rt->u.dst.dev->hard_header_len + 15)&~15;

	skb = sock_alloc_send_skb(sk, length+hh_len+15,
				  flags&MSG_DONTWAIT, &err);
	if(skb==NULL)
		goto error; 
	skb_reserve(skb, hh_len);
	}

	skb->priority = sk->priority;
	skb->dst = dst_clone(&rt->u.dst);

	skb->nh.iph = iph = (struct iphdr *)skb_put(skb, length);

	if(!sk->protinfo.af_inet.hdrincl) {
		iph->version=4;
		iph->ihl=5;
		iph->tos=sk->protinfo.af_inet.tos;
		iph->tot_len = htons(length);
		iph->frag_off = df;
		iph->ttl=sk->protinfo.af_inet.mc_ttl;
		ip_select_ident(iph, &rt->u.dst, sk);
		if (rt->rt_type != RTN_MULTICAST)
			iph->ttl=sk->protinfo.af_inet.ttl;
		iph->protocol=sk->protocol;
		iph->saddr=rt->rt_src;
		iph->daddr=rt->rt_dst;
		iph->check=0;
		iph->check = ip_fast_csum((unsigned char *)iph, iph->ihl);
		err = getfrag(frag, ((char *)iph)+iph->ihl*4,0, length-iph->ihl*4);
	}
	else
		err = getfrag(frag, (void *)iph, 0, length);

	if (err)
		goto error_fault;

	err = NF_HOOK(PF_INET, NF_IP_LOCAL_OUT, skb, NULL, rt->u.dst.dev,
		      output_maybe_reroute);
	if (err > 0)
		err = sk->protinfo.af_inet.recverr ? net_xmit_errno(err) : 0;
	if (err)
		goto error;
out:
	return 0;

error_fault:
	err = -EFAULT;
	kfree_skb(skb);
error:
	IP_INC_STATS(IpOutDiscards);
	return err; 
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


	hlen = iph->ihl * 4;
	left = skb->len - hlen;		
	mtu = rt->u.dst.pmtu - hlen;	
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

		if ((skb2 = alloc_skb(len+hlen+dev->hard_header_len+15,GFP_ATOMIC)) == NULL) {
			NETDEBUG(printk(KERN_INFO "IP: frag: no memory for new fragment!\n"));
			err = -ENOMEM;
			goto fail;
		}


		skb2->pkt_type = skb->pkt_type;
		skb2->priority = skb->priority;
		skb_reserve(skb2, (dev->hard_header_len+15)&~15);
		skb_put(skb2, len + hlen);
		skb2->nh.raw = skb2->data;
		skb2->h.raw = skb2->data + hlen;
		skb2->protocol = skb->protocol;
		skb2->security = skb->security;


		if (skb->sk)
			skb_set_owner_w(skb2, skb->sk);
		skb2->dst = dst_clone(skb->dst);
		skb2->dev = skb->dev;
		skb2->physindev = skb->physindev;
		skb2->physoutdev = skb->physoutdev;


		memcpy(skb2->nh.raw, skb->data, hlen);

		if (skb_copy_bits(skb, ptr, skb2->h.raw, len))
			BUG();
		left -= len;

		iph = skb2->nh.iph;
		iph->frag_off = htons((offset >> 3));

		if (offset == 0)
			ip_options_fragment(skb);

		
		IPCB(skb2)->flags = IPCB(skb)->flags;

		if (left > 0 || not_last_frag)
			iph->frag_off |= htons(IP_MF);
		ptr += len;
		offset += len;

#ifdef CONFIG_NET_SCHED
		skb2->tc_index = skb->tc_index;
#endif
#ifdef CONFIG_NETFILTER
		skb2->nfmark = skb->nfmark;
		
		skb2->nfct = skb->nfct;
		nf_conntrack_get(skb2->nfct);
#ifdef CONFIG_NETFILTER_DEBUG
		skb2->nf_debug = skb->nf_debug;
#endif
#endif


		IP_INC_STATS(IpFragCreates);

		iph->tot_len = htons(len + hlen);

		ip_send_check(iph);

		
		memcpy(skb2->data - 16, skb->data - 16, 16);

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

static int ip_reply_glue_bits(const void *dptr, char *to, unsigned int offset, 
			      unsigned int fraglen)
{
        struct ip_reply_arg *dp = (struct ip_reply_arg*)dptr;
	u16 *pktp = (u16 *)to;
	struct iovec *iov; 
	int len; 
	int hdrflag = 1; 

	iov = &dp->iov[0]; 
	if (offset >= iov->iov_len) { 
		offset -= iov->iov_len;
		iov++; 
		hdrflag = 0; 
	}
	len = iov->iov_len - offset;
	if (fraglen > len) {  
		dp->csum = csum_partial_copy_nocheck(iov->iov_base+offset, to, len,
					     dp->csum);
		offset = 0;
		fraglen -= len; 
		to += len; 
		iov++;
	}

	dp->csum = csum_partial_copy_nocheck(iov->iov_base+offset, to, fraglen, 
					     dp->csum); 

	if (hdrflag && dp->csumoffset)
		*(pktp + dp->csumoffset) = csum_fold(dp->csum); 
	return 0;	       
}

void ip_send_reply(struct sock *sk, struct sk_buff *skb, struct ip_reply_arg *arg,
		   unsigned int len)
{
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

	if (ip_route_output(&rt, daddr, rt->rt_spec_dst, RT_TOS(skb->nh.iph->tos), 0))
		return;

	bh_lock_sock(sk);
	sk->protinfo.af_inet.tos = skb->nh.iph->tos;
	sk->priority = skb->priority;
	sk->protocol = skb->nh.iph->protocol;
	ip_build_xmit(sk, ip_reply_glue_bits, arg, len, &ipc, rt, MSG_DONTWAIT);
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
