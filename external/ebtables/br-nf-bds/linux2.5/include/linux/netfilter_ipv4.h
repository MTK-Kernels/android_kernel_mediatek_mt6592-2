#ifndef __LINUX_IP_NETFILTER_H
#define __LINUX_IP_NETFILTER_H

/* IPv4-specific defines for netfilter. 
 * (C)1998 Rusty Russell -- This code is GPL.
 */

#include <linux/config.h>
#include <linux/netfilter.h>

#define NFC_IP_SRC		0x0001
#define NFC_IP_DST		0x0002
#define NFC_IP_IF_IN		0x0004
#define NFC_IP_IF_OUT		0x0008
#define NFC_IP_TOS		0x0010
#define NFC_IP_PROTO		0x0020
#define NFC_IP_OPTIONS		0x0040
#define NFC_IP_FRAG		0x0080

#define NFC_IP_TCPFLAGS		0x0100
#define NFC_IP_SRC_PT		0x0200
#define NFC_IP_DST_PT		0x0400
#define NFC_IP_PROTO_UNKNOWN	0x2000

#define NF_IP_PRE_ROUTING	0
#define NF_IP_LOCAL_IN		1
#define NF_IP_FORWARD		2
#define NF_IP_LOCAL_OUT		3
#define NF_IP_POST_ROUTING	4
#define NF_IP_NUMHOOKS		5

enum nf_ip_hook_priorities {
	NF_IP_PRI_FIRST = INT_MIN,
	NF_IP_PRI_CONNTRACK = -200,
	NF_IP_PRI_BRIDGE_SABOTAGE_FORWARD = -175,
	NF_IP_PRI_MANGLE = -150,
	NF_IP_PRI_NAT_DST = -100,
	NF_IP_PRI_BRIDGE_SABOTAGE_LOCAL_OUT = -50,
	NF_IP_PRI_FILTER = 0,
	NF_IP_PRI_NAT_SRC = 100,
	NF_IP_PRI_LAST = INT_MAX,
};

#define SO_ORIGINAL_DST 80

#ifdef __KERNEL__
#ifdef CONFIG_NETFILTER_DEBUG
void nf_debug_ip_local_deliver(struct sk_buff *skb);
void nf_debug_ip_loopback_xmit(struct sk_buff *newskb);
void nf_debug_ip_finish_output2(struct sk_buff *skb);
#endif 

extern int ip_route_me_harder(struct sk_buff **pskb);
#endif 

#endif 
