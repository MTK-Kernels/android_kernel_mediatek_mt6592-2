/*
 *	Handle firewalling
 *	Linux ethernet bridge
 *
 *	Authors:
 *	Lennert Buytenhek               <buytenh@gnu.org>
 *	Bart De Schuymer		<bart.de.schuymer@pandora.be>
 *
 *	$Id: br_netfilter.c,v 1.3 2002/09/11 17:41:38 bdschuym Exp $
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 *
 *	Lennert dedicates this file to Kerstin Wurdinger.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/ip.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/if_ether.h>
#include <linux/netfilter_bridge.h>
#include <linux/netfilter_ipv4.h>
#include <linux/in_route.h>
#include <net/ip.h>
#include <net/tcp.h>
#include <asm/uaccess.h>
#include <asm/checksum.h>
#include "br_private.h"


#ifndef WE_REALLY_INSIST_ON_NOT_HAVING_NAT_SUPPORT
#define skb_origaddr(skb)		(*((u32 *)((skb)->cb + sizeof((skb)->cb) - 4)))

#define store_orig_dstaddr(skb)		(skb_origaddr(skb) = (skb)->nh.iph->daddr)
#define store_orig_srcaddr(skb)		(skb_origaddr(skb) = (skb)->nh.iph->saddr)
#define dnat_took_place(skb)		(skb_origaddr(skb) != (skb)->nh.iph->daddr)
#define snat_took_place(skb)		(skb_origaddr(skb) != (skb)->nh.iph->saddr)
#else
#define store_orig_dstaddr(skb)
#define store_orig_srcaddr(skb)
#define dnat_took_place(skb)		(0)
#define snat_took_place(skb)		(0)
#endif


#define has_bridge_parent(device)	((device)->br_port != NULL)
#define bridge_parent(device)		(&((device)->br_port->br->dev))


static inline void __maybe_fixup_src_address(struct sk_buff *skb)
{
	if (snat_took_place(skb) &&
	    inet_addr_type(skb->nh.iph->saddr) == RTN_LOCAL) {
		memcpy(skb->mac.ethernet->h_source,
			bridge_parent(skb->dev)->dev_addr,
			ETH_ALEN);
	}
}


static struct net_device __fake_net_device = {
	hard_header_len:	ETH_HLEN
};

static struct rtable __fake_rtable = {
	u: {
		dst: {
			__refcnt:		ATOMIC_INIT(1),
			dev:			&__fake_net_device,
			pmtu:			1500
		}
	},

	rt_flags:	0
};


static void __br_dnat_complain(void)
{
	static unsigned long last_complaint = 0;

	if (jiffies - last_complaint >= 5 * HZ) {
		printk(KERN_WARNING "Performing cross-bridge DNAT requires IP "
			"forwarding to be enabled\n");
		last_complaint = jiffies;
	}
}


static int br_nf_pre_routing_finish_bridge(struct sk_buff *skb)
{
	skb->dev = bridge_parent(skb->dev);
#ifdef CONFIG_NETFILTER_DEBUG
	skb->nf_debug |= (1 << NF_BR_PRE_ROUTING) | (1 << NF_BR_FORWARD);
#endif
	skb->dst->output(skb);
	return 0;
}

#ifdef CONFIG_NETFILTER_DEBUG
#define __br_handle_frame_finish  br_nf_pre_routing_finish_route
static inline int br_nf_pre_routing_finish_route(struct sk_buff *skb)
{
	skb->nf_debug = 0;
	br_handle_frame_finish(skb);
	return 0;
}
#else
#define __br_handle_frame_finish br_handle_frame_finish
#endif


static int br_nf_pre_routing_finish(struct sk_buff *skb)
{
	struct net_device *dev = skb->dev;
	struct iphdr *iph = skb->nh.iph;

#ifdef CONFIG_NETFILTER_DEBUG
	skb->nf_debug ^= (1 << NF_BR_PRE_ROUTING);
#endif
	if (dnat_took_place(skb)) {
		if (ip_route_input(skb, iph->daddr, iph->saddr, iph->tos, dev)) {
			struct rtable *rt;

			if (!ip_route_output(&rt, iph->daddr, 0, iph->tos, 0)) {
				
				
				if (((struct dst_entry *)rt)->dev == dev) {
					skb->dst = (struct dst_entry *)rt;
					goto bridged_dnat;
				}
				__br_dnat_complain();
				dst_release((struct dst_entry *)rt);
			}
			kfree_skb(skb);
			return 0;
		} else {
			if (skb->dst->dev == dev) {
bridged_dnat:
				
				skb->physoutdev = &__fake_net_device;
				skb->dev = skb->physindev;
				NF_HOOK_THRESH(PF_BRIDGE, NF_BR_PRE_ROUTING, skb, skb->dev, NULL,
						br_nf_pre_routing_finish_bridge, 1);
				return 0;
			}
			
			skb->physoutdev = NULL;
			memcpy(skb->mac.ethernet->h_dest, dev->dev_addr, ETH_ALEN);
		}
	} else {
		skb->dst = (struct dst_entry *)&__fake_rtable;
		dst_hold(skb->dst);
	}
	skb->dev = skb->physindev;
	NF_HOOK_THRESH(PF_BRIDGE, NF_BR_PRE_ROUTING, skb, skb->dev, NULL,
			__br_handle_frame_finish, 1);

	return 0;
}

static unsigned int br_nf_pre_routing(unsigned int hook, struct sk_buff **pskb, const struct net_device *in, const struct net_device *out, int (*okfn)(struct sk_buff *))
{
	struct iphdr *iph;
	__u32 len;
	struct sk_buff *skb;

	if ((*pskb)->protocol != __constant_htons(ETH_P_IP))
		return NF_ACCEPT;

	if ((skb = skb_share_check(*pskb, GFP_ATOMIC)) == NULL)
		goto out;

	if (!pskb_may_pull(skb, sizeof(struct iphdr)))
		goto inhdr_error;

	iph = skb->nh.iph;
	if (iph->ihl < 5 || iph->version != 4)
		goto inhdr_error;

	if (!pskb_may_pull(skb, 4*iph->ihl))
		goto inhdr_error;

	iph = skb->nh.iph;
	if (ip_fast_csum((__u8 *)iph, iph->ihl) != 0)
		goto inhdr_error;

	len = ntohs(iph->tot_len);
	if (skb->len < len || len < 4*iph->ihl)
		goto inhdr_error;

	if (skb->len > len) {
		__pskb_trim(skb, len);
		if (skb->ip_summed == CHECKSUM_HW)
			skb->ip_summed = CHECKSUM_NONE;
	}

	skb->physindev = skb->dev;
	skb->dev = bridge_parent(skb->dev);
	if (skb->pkt_type == PACKET_OTHERHOST)
		skb->pkt_type = PACKET_HOST;
	store_orig_dstaddr(skb);

#ifdef CONFIG_NETFILTER_DEBUG
	(*pskb)->nf_debug ^= (1 << NF_IP_PRE_ROUTING);
#endif
	NF_HOOK(PF_INET, NF_IP_PRE_ROUTING, skb, skb->dev, NULL,
		br_nf_pre_routing_finish);

	return NF_STOLEN;

inhdr_error:
out:
	return NF_DROP;
}


static unsigned int br_nf_local_in(unsigned int hook, struct sk_buff **pskb, const struct net_device *in, const struct net_device *out, int (*okfn)(struct sk_buff *))
{
	struct sk_buff *skb = *pskb;

	if (skb->protocol != __constant_htons(ETH_P_IP))
		return NF_ACCEPT;

	if (skb->dst == (struct dst_entry *)&__fake_rtable) {
		dst_release(skb->dst);
		skb->dst = NULL;
	}

	return NF_ACCEPT;
}


static int br_nf_forward_finish(struct sk_buff *skb)
{
#ifdef CONFIG_NETFILTER_DEBUG
	skb->nf_debug ^= (1 << NF_BR_FORWARD);
#endif
	NF_HOOK_THRESH(PF_BRIDGE, NF_BR_FORWARD, skb, skb->physindev,
			skb->dev, br_forward_finish, 1);

	return 0;
}

static unsigned int br_nf_forward(unsigned int hook, struct sk_buff **pskb, const struct net_device *in, const struct net_device *out, int (*okfn)(struct sk_buff *))
{
	struct sk_buff *skb = *pskb;

	
	
	if (skb->protocol != __constant_htons(ETH_P_IP) || skb->physindev == NULL)
		return NF_ACCEPT;

	skb->physoutdev = skb->dev;
#ifdef CONFIG_NETFILTER_DEBUG
	skb->nf_debug ^= (1 << NF_IP_FORWARD);
#endif
	NF_HOOK(PF_INET, NF_IP_FORWARD, skb, bridge_parent(skb->physindev),
			bridge_parent(skb->dev), br_nf_forward_finish);

	return NF_STOLEN;
}


static int br_nf_local_out_finish_forward(struct sk_buff *skb)
{
	struct net_device *dev;

	dev = skb->physindev;
	
	skb->physindev = NULL;
#ifdef CONFIG_NETFILTER_DEBUG
	skb->nf_debug &= ~(1 << NF_BR_FORWARD);
#endif
	NF_HOOK(PF_BRIDGE, NF_BR_FORWARD, skb, dev, skb->dev,
		br_forward_finish);

	return 0;
}

static int br_nf_local_out_finish(struct sk_buff *skb)
{
#ifdef CONFIG_NETFILTER_DEBUG
	skb->nf_debug &= ~(1 << NF_BR_LOCAL_OUT);
#endif
	NF_HOOK_THRESH(PF_BRIDGE, NF_BR_LOCAL_OUT, skb, NULL, skb->dev,
			br_forward_finish, INT_MIN + 1);

	return 0;
}



static unsigned int br_nf_local_out(unsigned int hook, struct sk_buff **pskb, const struct net_device *in, const struct net_device *out, int (*_okfn)(struct sk_buff *))
{
	int hookno, prio;
	int (*okfn)(struct sk_buff *skb);
	struct net_device *realindev;
	struct sk_buff *skb = *pskb;

	if (skb->protocol != __constant_htons(ETH_P_IP))
		return NF_ACCEPT;

	if (skb->dst == NULL)
		return NF_ACCEPT;

	
	
	if (skb->physoutdev == &__fake_net_device) {
		okfn = br_nf_local_out_finish_forward;
	} else if (skb->physoutdev == NULL) {
		
		
		okfn = br_nf_local_out_finish;
	} else {
		printk("ARGH: bridge_or_routed hack doesn't work\n");
		okfn = br_nf_local_out_finish;
	}

	skb->physoutdev = skb->dev;

#ifdef CONFIG_NETFILTER_DEBUG
	skb->nf_debug ^= (1 << NF_BR_LOCAL_OUT);
#endif
	hookno = NF_IP_LOCAL_OUT;
	prio = NF_IP_PRI_BRIDGE_SABOTAGE;
	if ((realindev = skb->physindev) != NULL) {
		hookno = NF_IP_FORWARD;
		
		
		prio = NF_IP_PRI_BRIDGE_SABOTAGE_FORWARD;
		if (has_bridge_parent(realindev))
			realindev = bridge_parent(realindev);
	}

	NF_HOOK_THRESH(PF_INET, hookno, skb, realindev,
			bridge_parent(skb->dev), okfn, prio + 1);

	return NF_STOLEN;
}


static int br_nf_post_routing_finish(struct sk_buff *skb)
{
	__maybe_fixup_src_address(skb);
#ifdef CONFIG_NETFILTER_DEBUG
	skb->nf_debug ^= (1 << NF_BR_POST_ROUTING);
#endif
	NF_HOOK_THRESH(PF_BRIDGE, NF_BR_POST_ROUTING, skb, NULL,
			bridge_parent(skb->dev), br_dev_queue_push_xmit, 1);

	return 0;
}

static unsigned int br_nf_post_routing(unsigned int hook, struct sk_buff **pskb, const struct net_device *in, const struct net_device *out, int (*okfn)(struct sk_buff *))
{
	struct sk_buff *skb = *pskb;

	
	if (skb->mac.raw < skb->head || skb->mac.raw + ETH_HLEN > skb->data) {
		printk(KERN_CRIT "Argh!! Fuck me harder with a chainsaw. ");
		if (skb->dev != NULL) {
			printk("[%s]", skb->dev->name);
			if (has_bridge_parent(skb->dev))
				printk("[%s]", bridge_parent(skb->dev)->name);
		}
		printk("\n");
		return NF_ACCEPT;
	}

	if (skb->protocol != __constant_htons(ETH_P_IP))
		return NF_ACCEPT;

	if (skb->dst == NULL)
		return NF_ACCEPT;

	store_orig_srcaddr(skb);
#ifdef CONFIG_NETFILTER_DEBUG
	skb->nf_debug ^= (1 << NF_IP_POST_ROUTING);
#endif
	NF_HOOK(PF_INET, NF_IP_POST_ROUTING, skb, NULL,
		bridge_parent(skb->dev), br_nf_post_routing_finish);

	return NF_STOLEN;
}


static unsigned int ipv4_sabotage_in(unsigned int hook, struct sk_buff **pskb, const struct net_device *in, const struct net_device *out, int (*okfn)(struct sk_buff *))
{
	if (in->hard_start_xmit == br_dev_xmit &&
	    okfn != br_nf_pre_routing_finish) {
		okfn(*pskb);
		return NF_STOLEN;
	}

	return NF_ACCEPT;
}

static unsigned int ipv4_sabotage_out(unsigned int hook, struct sk_buff **pskb, const struct net_device *in, const struct net_device *out, int (*okfn)(struct sk_buff *))
{
	if (out->hard_start_xmit == br_dev_xmit &&
	    okfn != br_nf_forward_finish &&
	    okfn != br_nf_local_out_finish &&
	    okfn != br_nf_post_routing_finish) {
		struct sk_buff *skb = *pskb;

		if (hook == NF_IP_FORWARD && skb->physindev == NULL)
			skb->physindev = (struct net_device *)in;
		okfn(skb);
		return NF_STOLEN;
	}

	return NF_ACCEPT;
}


static struct nf_hook_ops br_nf_ops[] = {
	{ { NULL, NULL }, br_nf_pre_routing, PF_BRIDGE, NF_BR_PRE_ROUTING, 0 },
	{ { NULL, NULL }, br_nf_local_in, PF_BRIDGE, NF_BR_LOCAL_IN, 0 },
	{ { NULL, NULL }, br_nf_forward, PF_BRIDGE, NF_BR_FORWARD, 0 },
	
	
	{ { NULL, NULL }, br_nf_local_out, PF_BRIDGE, NF_BR_LOCAL_OUT, INT_MIN },
	{ { NULL, NULL }, br_nf_post_routing, PF_BRIDGE, NF_BR_POST_ROUTING, 0 },

	{ { NULL, NULL }, ipv4_sabotage_in, PF_INET, NF_IP_PRE_ROUTING, NF_IP_PRI_FIRST },

	{ { NULL, NULL }, ipv4_sabotage_out, PF_INET, NF_IP_FORWARD, NF_IP_PRI_BRIDGE_SABOTAGE_FORWARD },
	{ { NULL, NULL }, ipv4_sabotage_out, PF_INET, NF_IP_LOCAL_OUT, NF_IP_PRI_BRIDGE_SABOTAGE },
	{ { NULL, NULL }, ipv4_sabotage_out, PF_INET, NF_IP_POST_ROUTING, NF_IP_PRI_FIRST },
};

#define NUMHOOKS (sizeof(br_nf_ops)/sizeof(br_nf_ops[0]))


int br_netfilter_init(void)
{
	int i;

#ifndef WE_REALLY_INSIST_ON_NOT_HAVING_NAT_SUPPORT
	if (sizeof(struct tcp_skb_cb) + 4 >= sizeof(((struct sk_buff *)NULL)->cb)) {
		extern int __too_little_space_in_control_buffer(void);
		__too_little_space_in_control_buffer();
	}
#endif

	for (i=0;i<NUMHOOKS;i++) {
		int ret;

		if ((ret = nf_register_hook(&br_nf_ops[i])) >= 0)
			continue;

		while (i--)
			nf_unregister_hook(&br_nf_ops[i]);

		return ret;
	}

	printk(KERN_NOTICE "Bridge firewalling registered\n");

	return 0;
}

void br_netfilter_fini(void)
{
	int i;

	for (i=NUMHOOKS-1;i>=0;i--)
		nf_unregister_hook(&br_nf_ops[i]);
}
