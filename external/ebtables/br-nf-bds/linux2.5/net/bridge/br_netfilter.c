/*
 *	Handle firewalling
 *	Linux ethernet bridge
 *
 *	Authors:
 *	Lennert Buytenhek               <buytenh@gnu.org>
 *	Bart De Schuymer		<bart.de.schuymer@pandora.be>
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
#include <asm/uaccess.h>
#include <asm/checksum.h>
#include "br_private.h"


#define skb_origaddr(skb)	 (((struct bridge_skb_cb *) \
				 (skb->cb))->daddr.ipv4)
#define store_orig_dstaddr(skb)	 (skb_origaddr(skb) = (skb)->nh.iph->daddr)
#define dnat_took_place(skb)	 (skb_origaddr(skb) != (skb)->nh.iph->daddr)
#define clear_cb(skb)		 (memset(&skb_origaddr(skb), 0, \
				 sizeof(struct bridge_skb_cb)))

#define has_bridge_parent(device)	((device)->br_port != NULL)
#define bridge_parent(device)		(&((device)->br_port->br->dev))

static struct net_device __fake_net_device = {
	hard_header_len:	ETH_HLEN
};

static struct rtable __fake_rtable = {
	u: {
		dst: {
			__refcnt:		ATOMIC_INIT(1),
			dev:			&__fake_net_device,
			path:			&__fake_rtable.u.dst,
			metrics:		{[RTAX_MTU] 1500},
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
#ifdef CONFIG_NETFILTER_DEBUG
	skb->nf_debug |= (1 << NF_BR_PRE_ROUTING) | (1 << NF_BR_FORWARD);
#endif

	if (skb->pkt_type == PACKET_OTHERHOST) {
		skb->pkt_type = PACKET_HOST;
		skb->nf_bridge->mask |= BRNF_PKT_TYPE;
	}

	skb->dev = bridge_parent(skb->dev);
	skb->dst->output(skb);
	return 0;
}

static int br_nf_pre_routing_finish(struct sk_buff *skb)
{
	struct net_device *dev = skb->dev;
	struct iphdr *iph = skb->nh.iph;
	struct nf_bridge_info *nf_bridge = skb->nf_bridge;

#ifdef CONFIG_NETFILTER_DEBUG
	skb->nf_debug ^= (1 << NF_BR_PRE_ROUTING);
#endif

	if (nf_bridge->mask & BRNF_PKT_TYPE) {
		skb->pkt_type = PACKET_OTHERHOST;
		nf_bridge->mask ^= BRNF_PKT_TYPE;
	}

	if (dnat_took_place(skb)) {
		if (ip_route_input(skb, iph->daddr, iph->saddr, iph->tos,
		    dev)) {
			struct rtable *rt;
			struct flowi fl = { .nl_u = 
			{ .ip4_u = { .daddr = iph->daddr, .saddr = 0 ,
				     .tos = iph->tos} }, .proto = 0};

			if (!ip_route_output_key(&rt, &fl)) {
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
				nf_bridge->mask |= BRNF_BRIDGED_DNAT;
				skb->dev = nf_bridge->physindev;
				clear_cb(skb);
				NF_HOOK_THRESH(PF_BRIDGE, NF_BR_PRE_ROUTING,
					       skb, skb->dev, NULL,
					       br_nf_pre_routing_finish_bridge,
					       1);
				return 0;
			}
			memcpy(skb->mac.ethernet->h_dest, dev->dev_addr,
			       ETH_ALEN);
		}
	} else {
		skb->dst = (struct dst_entry *)&__fake_rtable;
		dst_hold(skb->dst);
	}

	clear_cb(skb);
	skb->dev = nf_bridge->physindev;
	NF_HOOK_THRESH(PF_BRIDGE, NF_BR_PRE_ROUTING, skb, skb->dev, NULL,
		       br_handle_frame_finish, 1);

	return 0;
}

static unsigned int br_nf_pre_routing(unsigned int hook, struct sk_buff **pskb,
   const struct net_device *in, const struct net_device *out,
   int (*okfn)(struct sk_buff *))
{
	struct iphdr *iph;
	__u32 len;
	struct sk_buff *skb;
	struct nf_bridge_info *nf_bridge;

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

#ifdef CONFIG_NETFILTER_DEBUG
	skb->nf_debug ^= (1 << NF_IP_PRE_ROUTING);
#endif
 	if ((nf_bridge = nf_bridge_alloc(skb)) == NULL)
		return NF_DROP;

	if (skb->pkt_type == PACKET_OTHERHOST) {
		skb->pkt_type = PACKET_HOST;
		nf_bridge->mask |= BRNF_PKT_TYPE;
	}

	nf_bridge->physindev = skb->dev;
	skb->dev = bridge_parent(skb->dev);
	store_orig_dstaddr(skb);

	NF_HOOK(PF_INET, NF_IP_PRE_ROUTING, skb, skb->dev, NULL,
		br_nf_pre_routing_finish);

	return NF_STOLEN;

inhdr_error:
out:
	return NF_DROP;
}


static unsigned int br_nf_local_in(unsigned int hook, struct sk_buff **pskb,
   const struct net_device *in, const struct net_device *out,
   int (*okfn)(struct sk_buff *))
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
	struct nf_bridge_info *nf_bridge = skb->nf_bridge;

#ifdef CONFIG_NETFILTER_DEBUG
	skb->nf_debug ^= (1 << NF_BR_FORWARD);
#endif

	if (nf_bridge->mask & BRNF_PKT_TYPE) {
		skb->pkt_type = PACKET_OTHERHOST;
		nf_bridge->mask ^= BRNF_PKT_TYPE;
	}

	NF_HOOK_THRESH(PF_BRIDGE, NF_BR_FORWARD, skb, nf_bridge->physindev,
			skb->dev, br_forward_finish, 1);

	return 0;
}

static unsigned int br_nf_forward(unsigned int hook, struct sk_buff **pskb,
   const struct net_device *in, const struct net_device *out,
   int (*okfn)(struct sk_buff *))
{
	struct sk_buff *skb = *pskb;
	struct nf_bridge_info *nf_bridge;

	if (skb->protocol != __constant_htons(ETH_P_IP))
		return NF_ACCEPT;

#ifdef CONFIG_NETFILTER_DEBUG
	skb->nf_debug ^= (1 << NF_BR_FORWARD);
#endif

	nf_bridge = skb->nf_bridge;
	if (skb->pkt_type == PACKET_OTHERHOST) {
		skb->pkt_type = PACKET_HOST;
		nf_bridge->mask |= BRNF_PKT_TYPE;
	}

	nf_bridge->physoutdev = skb->dev;

	NF_HOOK(PF_INET, NF_IP_FORWARD, skb, bridge_parent(nf_bridge->physindev),
			bridge_parent(skb->dev), br_nf_forward_finish);

	return NF_STOLEN;
}


static int br_nf_local_out_finish(struct sk_buff *skb)
{
#ifdef CONFIG_NETFILTER_DEBUG
	skb->nf_debug &= ~(1 << NF_BR_LOCAL_OUT);
#endif

	NF_HOOK_THRESH(PF_BRIDGE, NF_BR_LOCAL_OUT, skb, NULL, skb->dev,
			br_forward_finish, NF_BR_PRI_FIRST + 1);

	return 0;
}



static unsigned int br_nf_local_out(unsigned int hook, struct sk_buff **pskb,
   const struct net_device *in, const struct net_device *out,
   int (*_okfn)(struct sk_buff *))
{
	int (*okfn)(struct sk_buff *skb);
	struct net_device *realindev;
	struct sk_buff *skb = *pskb;
	struct nf_bridge_info *nf_bridge;

	if (skb->protocol != __constant_htons(ETH_P_IP))
		return NF_ACCEPT;

	if (skb->dst == NULL)
		return NF_ACCEPT;

	nf_bridge = skb->nf_bridge;
	nf_bridge->physoutdev = skb->dev;

	realindev = nf_bridge->physindev;

	if (nf_bridge->mask & BRNF_BRIDGED_DNAT) {
		okfn = br_forward_finish;

		if (nf_bridge->mask & BRNF_PKT_TYPE) {
			skb->pkt_type = PACKET_OTHERHOST;
			nf_bridge->mask ^= BRNF_PKT_TYPE;
		}

		NF_HOOK(PF_BRIDGE, NF_BR_FORWARD, skb, realindev,
			skb->dev, okfn);
	} else {
		okfn = br_nf_local_out_finish;
		if (realindev != NULL) {
			if (((nf_bridge->mask & BRNF_DONT_TAKE_PARENT) == 0) &&
			    has_bridge_parent(realindev))
				realindev = bridge_parent(realindev);

			NF_HOOK_THRESH(PF_INET, NF_IP_FORWARD, skb, realindev,
				       bridge_parent(skb->dev), okfn,
				       NF_IP_PRI_BRIDGE_SABOTAGE_FORWARD + 1);
		} else {
#ifdef CONFIG_NETFILTER_DEBUG
			skb->nf_debug ^= (1 << NF_IP_LOCAL_OUT);
#endif

			NF_HOOK_THRESH(PF_INET, NF_IP_LOCAL_OUT, skb, realindev,
				       bridge_parent(skb->dev), okfn,
				       NF_IP_PRI_BRIDGE_SABOTAGE_LOCAL_OUT + 1);
		}
	}

	return NF_STOLEN;
}


static unsigned int br_nf_post_routing(unsigned int hook, struct sk_buff **pskb,
   const struct net_device *in, const struct net_device *out,
   int (*okfn)(struct sk_buff *))
{
	struct sk_buff *skb = *pskb;
	struct nf_bridge_info *nf_bridge = (*pskb)->nf_bridge;

	
	if (skb->mac.raw < skb->head || skb->mac.raw + ETH_HLEN > skb->data) {
		printk(KERN_CRIT "br_netfilter: Argh!! br_nf_post_routing: "
				 "bad mac.raw pointer.");
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

#ifdef CONFIG_NETFILTER_DEBUG
	skb->nf_debug ^= (1 << NF_IP_POST_ROUTING);
#endif

	if (skb->pkt_type == PACKET_OTHERHOST) {
		skb->pkt_type = PACKET_HOST;
		nf_bridge->mask |= BRNF_PKT_TYPE;
	}

	memcpy(nf_bridge->hh, skb->data - 16, 16);

	NF_HOOK(PF_INET, NF_IP_POST_ROUTING, skb, NULL,
		bridge_parent(skb->dev), br_dev_queue_push_xmit);

	return NF_STOLEN;
}



static unsigned int ipv4_sabotage_in(unsigned int hook, struct sk_buff **pskb,
   const struct net_device *in, const struct net_device *out,
   int (*okfn)(struct sk_buff *))
{
	if (in->hard_start_xmit == br_dev_xmit &&
	    okfn != br_nf_pre_routing_finish) {
		okfn(*pskb);
		return NF_STOLEN;
	}

	return NF_ACCEPT;
}

static unsigned int ipv4_sabotage_out(unsigned int hook, struct sk_buff **pskb,
   const struct net_device *in, const struct net_device *out,
   int (*okfn)(struct sk_buff *))
{
	if (out->hard_start_xmit == br_dev_xmit &&
	    okfn != br_nf_forward_finish &&
	    okfn != br_nf_local_out_finish &&
	    okfn != br_dev_queue_push_xmit) {
		struct sk_buff *skb = *pskb;
		struct nf_bridge_info *nf_bridge;

		if (!skb->nf_bridge && !nf_bridge_alloc(skb))
			return NF_DROP;

		nf_bridge = skb->nf_bridge;

		if (hook == NF_IP_FORWARD && nf_bridge->physindev == NULL) {
			nf_bridge->mask &= BRNF_DONT_TAKE_PARENT;
			nf_bridge->physindev = (struct net_device *)in;
		}
		okfn(skb);
		return NF_STOLEN;
	}

	return NF_ACCEPT;
}

static struct nf_hook_ops br_nf_ops[] = {
	{ { NULL, NULL }, br_nf_pre_routing, PF_BRIDGE, NF_BR_PRE_ROUTING, NF_BR_PRI_BRNF },
	{ { NULL, NULL }, br_nf_local_in, PF_BRIDGE, NF_BR_LOCAL_IN, NF_BR_PRI_BRNF },
	{ { NULL, NULL }, br_nf_forward, PF_BRIDGE, NF_BR_FORWARD, NF_BR_PRI_BRNF },
	{ { NULL, NULL }, br_nf_local_out, PF_BRIDGE, NF_BR_LOCAL_OUT, NF_BR_PRI_FIRST },
	{ { NULL, NULL }, br_nf_post_routing, PF_BRIDGE, NF_BR_POST_ROUTING, NF_BR_PRI_LAST },
	{ { NULL, NULL }, ipv4_sabotage_in, PF_INET, NF_IP_PRE_ROUTING, NF_IP_PRI_FIRST },
	{ { NULL, NULL }, ipv4_sabotage_out, PF_INET, NF_IP_FORWARD, NF_IP_PRI_BRIDGE_SABOTAGE_FORWARD },
	{ { NULL, NULL }, ipv4_sabotage_out, PF_INET, NF_IP_LOCAL_OUT, NF_IP_PRI_BRIDGE_SABOTAGE_LOCAL_OUT },
	{ { NULL, NULL }, ipv4_sabotage_out, PF_INET, NF_IP_POST_ROUTING, NF_IP_PRI_FIRST }
};

#define NUMHOOKS (sizeof(br_nf_ops)/sizeof(br_nf_ops[0]))

int br_netfilter_init(void)
{
	int i;

	for (i = 0; i < NUMHOOKS; i++) {
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

	for (i = NUMHOOKS - 1; i >= 0; i--)
		nf_unregister_hook(&br_nf_ops[i]);
}
