#ifndef __LINUX_BRIDGE_NETFILTER_H
#define __LINUX_BRIDGE_NETFILTER_H


#include <linux/config.h>
#include <linux/netfilter.h>
#include <asm/atomic.h>

#define NF_BR_PRE_ROUTING	0
#define NF_BR_LOCAL_IN		1
#define NF_BR_FORWARD		2
#define NF_BR_LOCAL_OUT		3
#define NF_BR_POST_ROUTING	4
#define NF_BR_BROUTING		5
#define NF_BR_NUMHOOKS		6

#define BRNF_PKT_TYPE			0x01
#define BRNF_BRIDGED_DNAT		0x02
#define BRNF_DONT_TAKE_PARENT		0x04

enum nf_br_hook_priorities {
	NF_BR_PRI_FIRST = INT_MIN,
	NF_BR_PRI_NAT_DST_BRIDGED = -300,
	NF_BR_PRI_FILTER_BRIDGED = -200,
	NF_BR_PRI_BRNF = 0,
	NF_BR_PRI_NAT_DST_OTHER = 100,
	NF_BR_PRI_FILTER_OTHER = 200,
	NF_BR_PRI_NAT_SRC = 300,
	NF_BR_PRI_LAST = INT_MAX,
};

static inline
struct nf_bridge_info *nf_bridge_alloc(struct sk_buff *skb)
{
	struct nf_bridge_info **nf_bridge = &(skb->nf_bridge);

	if ((*nf_bridge = kmalloc(sizeof(**nf_bridge), GFP_ATOMIC)) != NULL) {
		atomic_set(&(*nf_bridge)->use, 1);
		(*nf_bridge)->mask = 0;
		(*nf_bridge)->physindev = (*nf_bridge)->physoutdev = NULL;
	}

	return *nf_bridge;
}

struct bridge_skb_cb {
	union {
		__u32 ipv4;
	} daddr;
};

#endif
