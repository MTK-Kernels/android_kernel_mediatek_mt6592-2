/*
 * 	NET3	Protocol independent device support routines.
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 *	Derived from the non IP parts of dev.c 1.0.19
 * 		Authors:	Ross Biro, <bir7@leland.Stanford.Edu>
 *				Fred N. van Kempen, <waltje@uWalt.NL.Mugnet.ORG>
 *				Mark Evans, <evansmp@uhura.aston.ac.uk>
 *
 *	Additional Authors:
 *		Florian la Roche <rzsfl@rz.uni-sb.de>
 *		Alan Cox <gw4pts@gw4pts.ampr.org>
 *		David Hinds <dhinds@allegro.stanford.edu>
 *		Alexey Kuznetsov <kuznet@ms2.inr.ac.ru>
 *		Adam Sulmicki <adam@cfar.umd.edu>
 *              Pekka Riikonen <priikone@poesidon.pspt.fi>
 *
 *	Changes:
 *              D.J. Barrow     :       Fixed bug where dev->refcnt gets set
 *              			to 2 if register_netdev gets called
 *              			before net_dev_init & also removed a
 *              			few lines of code in the process.
 *		Alan Cox	:	device private ioctl copies fields back.
 *		Alan Cox	:	Transmit queue code does relevant
 *					stunts to keep the queue safe.
 *		Alan Cox	:	Fixed double lock.
 *		Alan Cox	:	Fixed promisc NULL pointer trap
 *		????????	:	Support the full private ioctl range
 *		Alan Cox	:	Moved ioctl permission check into
 *					drivers
 *		Tim Kordas	:	SIOCADDMULTI/SIOCDELMULTI
 *		Alan Cox	:	100 backlog just doesn't cut it when
 *					you start doing multicast video 8)
 *		Alan Cox	:	Rewrote net_bh and list manager.
 *		Alan Cox	: 	Fix ETH_P_ALL echoback lengths.
 *		Alan Cox	:	Took out transmit every packet pass
 *					Saved a few bytes in the ioctl handler
 *		Alan Cox	:	Network driver sets packet type before
 *					calling netif_rx. Saves a function
 *					call a packet.
 *		Alan Cox	:	Hashed net_bh()
 *		Richard Kooijman:	Timestamp fixes.
 *		Alan Cox	:	Wrong field in SIOCGIFDSTADDR
 *		Alan Cox	:	Device lock protection.
 *		Alan Cox	: 	Fixed nasty side effect of device close
 *					changes.
 *		Rudi Cilibrasi	:	Pass the right thing to
 *					set_mac_address()
 *		Dave Miller	:	32bit quantity for the device lock to
 *					make it work out on a Sparc.
 *		Bjorn Ekwall	:	Added KERNELD hack.
 *		Alan Cox	:	Cleaned up the backlog initialise.
 *		Craig Metz	:	SIOCGIFCONF fix if space for under
 *					1 device.
 *	    Thomas Bogendoerfer :	Return ENODEV for dev_open, if there
 *					is no device open function.
 *		Andi Kleen	:	Fix error reporting for SIOCGIFCONF
 *	    Michael Chastain	:	Fix signed/unsigned for SIOCGIFCONF
 *		Cyrus Durgin	:	Cleaned for KMOD
 *		Adam Sulmicki   :	Bug Fix : Network Device Unload
 *					A network device unload needs to purge
 *					the backlog queue.
 *	Paul Rusty Russell	:	SIOCSIFNAME
 *              Pekka Riikonen  :	Netdev boot-time settings code
 *              Andrew Morton   :       Make unregister_netdevice wait
 *              			indefinitely on dev->refcnt
 * 		J Hadi Salim	:	- Backlog queue sampling
 *				        - netif_rx() feedback
 */

#include <asm/uaccess.h>
#include <asm/system.h>
#include <asm/bitops.h>
#include <linux/config.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/socket.h>
#include <linux/sockios.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/if_ether.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/notifier.h>
#include <linux/skbuff.h>
#include <linux/brlock.h>
#include <net/sock.h>
#include <linux/rtnetlink.h>
#include <linux/proc_fs.h>
#include <linux/stat.h>
#include <linux/if_bridge.h>
#include <linux/divert.h>
#include <net/dst.h>
#include <net/pkt_sched.h>
#include <net/profile.h>
#include <net/checksum.h>
#include <linux/highmem.h>
#include <linux/init.h>
#include <linux/kmod.h>
#include <linux/module.h>
#if defined(CONFIG_NET_RADIO) || defined(CONFIG_NET_PCMCIA_RADIO)
#include <linux/wireless.h>		
#include <net/iw_handler.h>
#endif	
#ifdef CONFIG_PLIP
extern int plip_init(void);
#endif


#undef RAND_LIE

#undef OFFLINE_SAMPLE

NET_PROFILE_DEFINE(dev_queue_xmit)
NET_PROFILE_DEFINE(softnet_process)

const char *if_port_text[] = {
  "unknown",
  "BNC",
  "10baseT",
  "AUI",
  "100baseT",
  "100baseTX",
  "100baseFX"
};


static struct packet_type *ptype_base[16];	
static struct packet_type *ptype_all;		

#ifdef OFFLINE_SAMPLE
static void sample_queue(unsigned long dummy);
static struct timer_list samp_timer = { function: sample_queue };
#endif

#ifdef CONFIG_HOTPLUG
static int net_run_sbin_hotplug(struct net_device *dev, char *action);
#else
#define net_run_sbin_hotplug(dev, action) ({ 0; })
#endif


static struct notifier_block *netdev_chain;

struct softnet_data softnet_data[NR_CPUS] __cacheline_aligned;

#ifdef CONFIG_NET_FASTROUTE
int netdev_fastroute;
int netdev_fastroute_obstacles;
#endif




int netdev_nit;



void dev_add_pack(struct packet_type *pt)
{
	int hash;

	br_write_lock_bh(BR_NETPROTO_LOCK);

#ifdef CONFIG_NET_FASTROUTE
	
	if (pt->data && (long)(pt->data) != 1) {
		netdev_fastroute_obstacles++;
		dev_clear_fastroute(pt->dev);
	}
#endif
	if (pt->type == htons(ETH_P_ALL)) {
		netdev_nit++;
		pt->next  = ptype_all;
		ptype_all = pt;
	} else {
		hash = ntohs(pt->type) & 15;
		pt->next = ptype_base[hash];
		ptype_base[hash] = pt;
	}
	br_write_unlock_bh(BR_NETPROTO_LOCK);
}


void dev_remove_pack(struct packet_type *pt)
{
	struct packet_type **pt1;

	br_write_lock_bh(BR_NETPROTO_LOCK);

	if (pt->type == htons(ETH_P_ALL)) {
		netdev_nit--;
		pt1 = &ptype_all;
	} else
		pt1 = &ptype_base[ntohs(pt->type) & 15];

	for (; *pt1; pt1 = &((*pt1)->next)) {
		if (pt == *pt1) {
			*pt1 = pt->next;
#ifdef CONFIG_NET_FASTROUTE
			if (pt->data)
				netdev_fastroute_obstacles--;
#endif
			goto out;
		}
	}
	printk(KERN_WARNING "dev_remove_pack: %p not found.\n", pt);
out:
	br_write_unlock_bh(BR_NETPROTO_LOCK);
}


static struct netdev_boot_setup dev_boot_setup[NETDEV_BOOT_SETUP_MAX];

int netdev_boot_setup_add(char *name, struct ifmap *map)
{
	struct netdev_boot_setup *s;
	int i;

	s = dev_boot_setup;
	for (i = 0; i < NETDEV_BOOT_SETUP_MAX; i++) {
		if (s[i].name[0] == '\0' || s[i].name[0] == ' ') {
			memset(s[i].name, 0, sizeof(s[i].name));
			strcpy(s[i].name, name);
			memcpy(&s[i].map, map, sizeof(s[i].map));
			break;
		}
	}

	return i >= NETDEV_BOOT_SETUP_MAX ? 0 : 1;
}

int netdev_boot_setup_check(struct net_device *dev)
{
	struct netdev_boot_setup *s = dev_boot_setup;
	int i;

	for (i = 0; i < NETDEV_BOOT_SETUP_MAX; i++) {
		if (s[i].name[0] != '\0' && s[i].name[0] != ' ' &&
		    !strncmp(dev->name, s[i].name, strlen(s[i].name))) {
			dev->irq 	= s[i].map.irq;
			dev->base_addr 	= s[i].map.base_addr;
			dev->mem_start 	= s[i].map.mem_start;
			dev->mem_end 	= s[i].map.mem_end;
			return 1;
		}
	}
	return 0;
}

int __init netdev_boot_setup(char *str)
{
	int ints[5];
	struct ifmap map;

	str = get_options(str, ARRAY_SIZE(ints), ints);
	if (!str || !*str)
		return 0;

	
	memset(&map, 0, sizeof(map));
	if (ints[0] > 0)
		map.irq = ints[1];
	if (ints[0] > 1)
		map.base_addr = ints[2];
	if (ints[0] > 2)
		map.mem_start = ints[3];
	if (ints[0] > 3)
		map.mem_end = ints[4];

	
	return netdev_boot_setup_add(str, &map);
}

__setup("netdev=", netdev_boot_setup);



struct net_device *__dev_get_by_name(const char *name)
{
	struct net_device *dev;

	for (dev = dev_base; dev; dev = dev->next)
		if (!strncmp(dev->name, name, IFNAMSIZ))
			break;
	return dev;
}


struct net_device *dev_get_by_name(const char *name)
{
	struct net_device *dev;

	read_lock(&dev_base_lock);
	dev = __dev_get_by_name(name);
	if (dev)
		dev_hold(dev);
	read_unlock(&dev_base_lock);
	return dev;
}


int dev_get(const char *name)
{
	struct net_device *dev;

	read_lock(&dev_base_lock);
	dev = __dev_get_by_name(name);
	read_unlock(&dev_base_lock);
	return dev != NULL;
}


struct net_device *__dev_get_by_index(int ifindex)
{
	struct net_device *dev;

	for (dev = dev_base; dev; dev = dev->next)
		if (dev->ifindex == ifindex)
			break;
	return dev;
}



struct net_device *dev_get_by_index(int ifindex)
{
	struct net_device *dev;

	read_lock(&dev_base_lock);
	dev = __dev_get_by_index(ifindex);
	if (dev)
		dev_hold(dev);
	read_unlock(&dev_base_lock);
	return dev;
}


struct net_device *dev_getbyhwaddr(unsigned short type, char *ha)
{
	struct net_device *dev;

	ASSERT_RTNL();

	for (dev = dev_base; dev; dev = dev->next)
		if (dev->type == type &&
		    !memcmp(dev->dev_addr, ha, dev->addr_len))
			break;
	return dev;
}


int dev_alloc_name(struct net_device *dev, const char *name)
{
	int i;
	char buf[32];
	char *p;

	p = strchr(name, '%');
	if (p && (p[1] != 'd' || strchr(p + 2, '%')))
		return -EINVAL;

	for (i = 0; i < 100; i++) {
		snprintf(buf, sizeof(buf), name, i);
		if (!__dev_get_by_name(buf)) {
			strcpy(dev->name, buf);
			return i;
		}
	}
	return -ENFILE;	
}


struct net_device *dev_alloc(const char *name, int *err)
{
	struct net_device *dev = kmalloc(sizeof(*dev), GFP_KERNEL);

	if (!dev)
		*err = -ENOBUFS;
	else {
		memset(dev, 0, sizeof(*dev));
		*err = dev_alloc_name(dev, name);
		if (*err < 0) {
			kfree(dev);
			dev = NULL;
		}
	}
	return dev;
}

void netdev_state_change(struct net_device *dev)
{
	if (dev->flags & IFF_UP) {
		notifier_call_chain(&netdev_chain, NETDEV_CHANGE, dev);
		rtmsg_ifinfo(RTM_NEWLINK, dev, 0);
	}
}


#ifdef CONFIG_KMOD


void dev_load(const char *name)
{
	if (!dev_get(name) && capable(CAP_SYS_MODULE))
		request_module(name);
}

#else

extern inline void dev_load(const char *unused){;}

#endif

static int default_rebuild_header(struct sk_buff *skb)
{
	printk(KERN_DEBUG "%s: default_rebuild_header called -- BUG!\n",
	       skb->dev ? skb->dev->name : "NULL!!!");
	kfree_skb(skb);
	return 1;
}

int dev_open(struct net_device *dev)
{
	int ret = 0;


	if (dev->flags & IFF_UP)
		return 0;

	if (!netif_device_present(dev))
		return -ENODEV;

	if (try_inc_mod_count(dev->owner)) {
		set_bit(__LINK_STATE_START, &dev->state);
		if (dev->open) {
			ret = dev->open(dev);
			if (ret) {
				clear_bit(__LINK_STATE_START, &dev->state);
				if (dev->owner)
					__MOD_DEC_USE_COUNT(dev->owner);
			}
		}
	} else {
		ret = -ENODEV;
	}


	if (!ret) {
		dev->flags |= IFF_UP;

		dev_mc_upload(dev);

		dev_activate(dev);

		notifier_call_chain(&netdev_chain, NETDEV_UP, dev);
	}
	return ret;
}

#ifdef CONFIG_NET_FASTROUTE

static void dev_do_clear_fastroute(struct net_device *dev)
{
	if (dev->accept_fastpath) {
		int i;

		for (i = 0; i <= NETDEV_FASTROUTE_HMASK; i++) {
			struct dst_entry *dst;

			write_lock_irq(&dev->fastpath_lock);
			dst = dev->fastpath[i];
			dev->fastpath[i] = NULL;
			write_unlock_irq(&dev->fastpath_lock);

			dst_release(dst);
		}
	}
}

void dev_clear_fastroute(struct net_device *dev)
{
	if (dev) {
		dev_do_clear_fastroute(dev);
	} else {
		read_lock(&dev_base_lock);
		for (dev = dev_base; dev; dev = dev->next)
			dev_do_clear_fastroute(dev);
		read_unlock(&dev_base_lock);
	}
}
#endif

int dev_close(struct net_device *dev)
{
	if (!(dev->flags & IFF_UP))
		return 0;

	notifier_call_chain(&netdev_chain, NETDEV_GOING_DOWN, dev);

	dev_deactivate(dev);

	clear_bit(__LINK_STATE_START, &dev->state);


	smp_mb__after_clear_bit(); 
	while (test_bit(__LINK_STATE_RX_SCHED, &dev->state)) {
		
		current->state = TASK_INTERRUPTIBLE;
		schedule_timeout(1);
	}

	if (dev->stop)
		dev->stop(dev);


	dev->flags &= ~IFF_UP;
#ifdef CONFIG_NET_FASTROUTE
	dev_clear_fastroute(dev);
#endif

	notifier_call_chain(&netdev_chain, NETDEV_DOWN, dev);

	if (dev->owner)
		__MOD_DEC_USE_COUNT(dev->owner);

	return 0;
}




int register_netdevice_notifier(struct notifier_block *nb)
{
	return notifier_chain_register(&netdev_chain, nb);
}


int unregister_netdevice_notifier(struct notifier_block *nb)
{
	return notifier_chain_unregister(&netdev_chain, nb);
}


void dev_queue_xmit_nit(struct sk_buff *skb, struct net_device *dev)
{
	struct packet_type *ptype;
	do_gettimeofday(&skb->stamp);

	br_read_lock(BR_NETPROTO_LOCK);
	for (ptype = ptype_all; ptype; ptype = ptype->next) {
		if ((ptype->dev == dev || !ptype->dev) &&
		    (struct sock *)ptype->data != skb->sk) {
			struct sk_buff *skb2= skb_clone(skb, GFP_ATOMIC);
			if (!skb2)
				break;

			skb2->mac.raw = skb2->data;

			if (skb2->nh.raw < skb2->data ||
			    skb2->nh.raw > skb2->tail) {
				if (net_ratelimit())
					printk(KERN_DEBUG "protocol %04x is "
							  "buggy, dev %s\n",
					       skb2->protocol, dev->name);
				skb2->nh.raw = skb2->data;
			}

			skb2->h.raw = skb2->nh.raw;
			skb2->pkt_type = PACKET_OUTGOING;
			ptype->func(skb2, skb->dev, ptype);
		}
	}
	br_read_unlock(BR_NETPROTO_LOCK);
}

struct sk_buff *skb_checksum_help(struct sk_buff *skb)
{
	unsigned int csum;
	int offset = skb->h.raw - skb->data;

	if (offset > (int)skb->len)
		BUG();
	csum = skb_checksum(skb, offset, skb->len-offset, 0);

	offset = skb->tail - skb->h.raw;
	if (offset <= 0)
		BUG();
	if (skb->csum + 2 > offset)
		BUG();

	*(u16*)(skb->h.raw + skb->csum) = csum_fold(csum);
	skb->ip_summed = CHECKSUM_NONE;
	return skb;
}

#ifdef CONFIG_HIGHMEM

static inline int illegal_highdma(struct net_device *dev, struct sk_buff *skb)
{
	int i;

	if (dev->features & NETIF_F_HIGHDMA)
		return 0;

	for (i = 0; i < skb_shinfo(skb)->nr_frags; i++)
		if (skb_shinfo(skb)->frags[i].page >= highmem_start_page)
			return 1;

	return 0;
}
#else
#define illegal_highdma(dev, skb)	(0)
#endif


int dev_queue_xmit(struct sk_buff *skb)
{
	struct net_device *dev = skb->dev;
	struct Qdisc *q;
	int rc = -ENOMEM;

	if (skb_shinfo(skb)->frag_list &&
	    !(dev->features & NETIF_F_FRAGLIST) &&
	    skb_linearize(skb, GFP_ATOMIC))
		goto out_kfree_skb;

	if (skb_shinfo(skb)->nr_frags &&
	    (!(dev->features & NETIF_F_SG) || illegal_highdma(dev, skb)) &&
	    skb_linearize(skb, GFP_ATOMIC))
		goto out_kfree_skb;

	if (skb->ip_summed == CHECKSUM_HW &&
	    (!(dev->features & (NETIF_F_HW_CSUM | NETIF_F_NO_CSUM)) &&
	     (!(dev->features & NETIF_F_IP_CSUM) ||
	      skb->protocol != htons(ETH_P_IP)))) {
		if ((skb = skb_checksum_help(skb)) == NULL)
			goto out;
	}

	
	spin_lock_bh(&dev->queue_lock);
	q = dev->qdisc;
	if (q->enqueue) {
		rc = q->enqueue(skb, q);

		qdisc_run(dev);

		spin_unlock_bh(&dev->queue_lock);
		rc = rc == NET_XMIT_BYPASS ? NET_XMIT_SUCCESS : rc;
		goto out;
	}

	if (dev->flags & IFF_UP) {
		int cpu = smp_processor_id();

		if (dev->xmit_lock_owner != cpu) {
			preempt_disable();
			spin_unlock(&dev->queue_lock);
			spin_lock(&dev->xmit_lock);
			dev->xmit_lock_owner = cpu;
			preempt_enable();

			if (!netif_queue_stopped(dev)) {
				if (netdev_nit)
					dev_queue_xmit_nit(skb, dev);

				rc = 0;
				if (!dev->hard_start_xmit(skb, dev)) {
					dev->xmit_lock_owner = -1;
					spin_unlock_bh(&dev->xmit_lock);
					goto out;
				}
			}
			dev->xmit_lock_owner = -1;
			spin_unlock_bh(&dev->xmit_lock);
			if (net_ratelimit())
				printk(KERN_DEBUG "Virtual device %s asks to "
				       "queue packet!\n", dev->name);
			goto out_enetdown;
		} else {
			if (net_ratelimit())
				printk(KERN_DEBUG "Dead loop on virtual device "
				       "%s, fix it urgently!\n", dev->name);
		}
	}
	spin_unlock_bh(&dev->queue_lock);
out_enetdown:
	rc = -ENETDOWN;
out_kfree_skb:
	kfree_skb(skb);
out:
	return rc;
}



int netdev_max_backlog = 300;
int weight_p = 64;            
int no_cong_thresh = 10;
int no_cong = 20;
int lo_cong = 100;
int mod_cong = 290;

struct netif_rx_stats netdev_rx_stat[NR_CPUS];


#ifdef CONFIG_NET_HW_FLOWCONTROL
atomic_t netdev_dropping = ATOMIC_INIT(0);
static unsigned long netdev_fc_mask = 1;
unsigned long netdev_fc_xoff;
spinlock_t netdev_fc_lock = SPIN_LOCK_UNLOCKED;

static struct
{
	void (*stimul)(struct net_device *);
	struct net_device *dev;
} netdev_fc_slots[BITS_PER_LONG];

int netdev_register_fc(struct net_device *dev,
		       void (*stimul)(struct net_device *dev))
{
	int bit = 0;
	unsigned long flags;

	spin_lock_irqsave(&netdev_fc_lock, flags);
	if (netdev_fc_mask != ~0UL) {
		bit = ffz(netdev_fc_mask);
		netdev_fc_slots[bit].stimul = stimul;
		netdev_fc_slots[bit].dev = dev;
		set_bit(bit, &netdev_fc_mask);
		clear_bit(bit, &netdev_fc_xoff);
	}
	spin_unlock_irqrestore(&netdev_fc_lock, flags);
	return bit;
}

void netdev_unregister_fc(int bit)
{
	unsigned long flags;

	spin_lock_irqsave(&netdev_fc_lock, flags);
	if (bit > 0) {
		netdev_fc_slots[bit].stimul = NULL;
		netdev_fc_slots[bit].dev = NULL;
		clear_bit(bit, &netdev_fc_mask);
		clear_bit(bit, &netdev_fc_xoff);
	}
	spin_unlock_irqrestore(&netdev_fc_lock, flags);
}

static void netdev_wakeup(void)
{
	unsigned long xoff;

	spin_lock(&netdev_fc_lock);
	xoff = netdev_fc_xoff;
	netdev_fc_xoff = 0;
	while (xoff) {
		int i = ffz(~xoff);
		xoff &= ~(1 << i);
		netdev_fc_slots[i].stimul(netdev_fc_slots[i].dev);
	}
	spin_unlock(&netdev_fc_lock);
}
#endif

static void get_sample_stats(int cpu)
{
#ifdef RAND_LIE
	unsigned long rd;
	int rq;
#endif
	int blog = softnet_data[cpu].input_pkt_queue.qlen;
	int avg_blog = softnet_data[cpu].avg_blog;

	avg_blog = (avg_blog >> 1) + (blog >> 1);

	if (avg_blog > mod_cong) {
		
		softnet_data[cpu].cng_level = NET_RX_CN_HIGH;
#ifdef RAND_LIE
		rd = net_random();
		rq = rd % netdev_max_backlog;
		if (rq < avg_blog) 
			softnet_data[cpu].cng_level = NET_RX_DROP;
#endif
	} else if (avg_blog > lo_cong) {
		softnet_data[cpu].cng_level = NET_RX_CN_MOD;
#ifdef RAND_LIE
		rd = net_random();
		rq = rd % netdev_max_backlog;
			if (rq < avg_blog) 
				softnet_data[cpu].cng_level = NET_RX_CN_HIGH;
#endif
	} else if (avg_blog > no_cong)
		softnet_data[cpu].cng_level = NET_RX_CN_LOW;
	else  
		softnet_data[cpu].cng_level = NET_RX_SUCCESS;

	softnet_data[cpu].avg_blog = avg_blog;
}

#ifdef OFFLINE_SAMPLE
static void sample_queue(unsigned long dummy)
{
	int next_tick = 1;
	int cpu = smp_processor_id();

	get_sample_stats(cpu);
	next_tick += jiffies;
	mod_timer(&samp_timer, next_tick);
}
#endif



int netif_rx(struct sk_buff *skb)
{
	int this_cpu;
	struct softnet_data *queue;
	unsigned long flags;

	if (!skb->stamp.tv_sec)
		do_gettimeofday(&skb->stamp);

	local_irq_save(flags);
	this_cpu = smp_processor_id();
	queue = &softnet_data[this_cpu];

	netdev_rx_stat[this_cpu].total++;
	if (queue->input_pkt_queue.qlen <= netdev_max_backlog) {
		if (queue->input_pkt_queue.qlen) {
			if (queue->throttle)
				goto drop;

enqueue:
			dev_hold(skb->dev);
			__skb_queue_tail(&queue->input_pkt_queue, skb);
#ifndef OFFLINE_SAMPLE
			get_sample_stats(this_cpu);
#endif
			local_irq_restore(flags);
			return queue->cng_level;
		}

		if (queue->throttle) {
			queue->throttle = 0;
#ifdef CONFIG_NET_HW_FLOWCONTROL
			if (atomic_dec_and_test(&netdev_dropping))
				netdev_wakeup();
#endif
		}

		netif_rx_schedule(&queue->backlog_dev);
		goto enqueue;
	}

	if (!queue->throttle) {
		queue->throttle = 1;
		netdev_rx_stat[this_cpu].throttled++;
#ifdef CONFIG_NET_HW_FLOWCONTROL
		atomic_inc(&netdev_dropping);
#endif
	}

drop:
	netdev_rx_stat[this_cpu].dropped++;
	local_irq_restore(flags);

	kfree_skb(skb);
	return NET_RX_DROP;
}

static int deliver_to_old_ones(struct packet_type *pt,
			       struct sk_buff *skb, int last)
{
	static spinlock_t net_bh_lock = SPIN_LOCK_UNLOCKED;
	int ret = NET_RX_DROP;

	if (!last) {
		skb = skb_clone(skb, GFP_ATOMIC);
		if (!skb)
			goto out;
	}
	if (skb_is_nonlinear(skb) && skb_linearize(skb, GFP_ATOMIC))
		goto out_kfree;


	
	spin_lock(&net_bh_lock);

	
	tasklet_disable(bh_task_vec+TIMER_BH);

	ret = pt->func(skb, skb->dev, pt);

	tasklet_hi_enable(bh_task_vec+TIMER_BH);
	spin_unlock(&net_bh_lock);
out:
	return ret;
out_kfree:
	kfree_skb(skb);
	goto out;
}

static __inline__ void skb_bond(struct sk_buff *skb)
{
	struct net_device *dev = skb->dev;

	if (dev->master)
		skb->dev = dev->master;
}

static void net_tx_action(struct softirq_action *h)
{
	int cpu = smp_processor_id();

	if (softnet_data[cpu].completion_queue) {
		struct sk_buff *clist;

		local_irq_disable();
		clist = softnet_data[cpu].completion_queue;
		softnet_data[cpu].completion_queue = NULL;
		local_irq_enable();

		while (clist) {
			struct sk_buff *skb = clist;
			clist = clist->next;

			BUG_TRAP(!atomic_read(&skb->users));
			__kfree_skb(skb);
		}
	}

	if (softnet_data[cpu].output_queue) {
		struct net_device *head;

		local_irq_disable();
		head = softnet_data[cpu].output_queue;
		softnet_data[cpu].output_queue = NULL;
		local_irq_enable();

		while (head) {
			struct net_device *dev = head;
			head = head->next_sched;

			smp_mb__before_clear_bit();
			clear_bit(__LINK_STATE_SCHED, &dev->state);

			if (spin_trylock(&dev->queue_lock)) {
				qdisc_run(dev);
				spin_unlock(&dev->queue_lock);
			} else {
				netif_schedule(dev);
			}
		}
	}
}

void net_call_rx_atomic(void (*fn)(void))
{
	br_write_lock_bh(BR_NETPROTO_LOCK);
	fn();
	br_write_unlock_bh(BR_NETPROTO_LOCK);
}

#if defined(CONFIG_BRIDGE) || defined(CONFIG_BRIDGE_MODULE)
int (*br_handle_frame_hook)(struct sk_buff *skb) = NULL;
#endif

static __inline__ int handle_bridge(struct sk_buff *skb,
				     struct packet_type *pt_prev)
{
	int ret = NET_RX_DROP;

	if (pt_prev) {
		if (!pt_prev->data)
			ret = deliver_to_old_ones(pt_prev, skb, 0);
		else {
			atomic_inc(&skb->users);
			ret = pt_prev->func(skb, skb->dev, pt_prev);
		}
	}

	return ret;
}


#ifdef CONFIG_NET_DIVERT
static inline int handle_diverter(struct sk_buff *skb)
{
	
	if (skb->dev->divert && skb->dev->divert->divert)
		divert_frame(skb);
	return 0;
}
#endif   

int netif_receive_skb(struct sk_buff *skb)
{
	struct packet_type *ptype, *pt_prev;
	int ret = NET_RX_DROP;
	unsigned short type = skb->protocol;

	if (!skb->stamp.tv_sec)
		do_gettimeofday(&skb->stamp);

	skb_bond(skb);

	netdev_rx_stat[smp_processor_id()].total++;

#ifdef CONFIG_NET_FASTROUTE
	if (skb->pkt_type == PACKET_FASTROUTE) {
		netdev_rx_stat[smp_processor_id()].fastroute_deferred_out++;
		return dev_queue_xmit(skb);
	}
#endif

	skb->h.raw = skb->nh.raw = skb->data;

	pt_prev = NULL;
	for (ptype = ptype_all; ptype; ptype = ptype->next) {
		if (!ptype->dev || ptype->dev == skb->dev) {
			if (pt_prev) {
				if (!pt_prev->data) {
					ret = deliver_to_old_ones(pt_prev,
								  skb, 0);
				} else {
					atomic_inc(&skb->users);
					ret = pt_prev->func(skb, skb->dev,
							    pt_prev);
				}
			}
			pt_prev = ptype;
		}
	}

#ifdef CONFIG_NET_DIVERT
	if (skb->dev->divert && skb->dev->divert->divert)
		ret = handle_diverter(skb);
#endif 

#if defined(CONFIG_BRIDGE) || defined(CONFIG_BRIDGE_MODULE)
	if (skb->dev->br_port && br_handle_frame_hook) {
		int ret;

		ret = handle_bridge(skb, pt_prev);
		if (br_handle_frame_hook(skb) == 0)
			return ret;
		pt_prev = NULL;
	}
#endif

	for (ptype = ptype_base[ntohs(type) & 15]; ptype; ptype = ptype->next) {
		if (ptype->type == type &&
		    (!ptype->dev || ptype->dev == skb->dev)) {
			if (pt_prev) {
				if (!pt_prev->data) {
					ret = deliver_to_old_ones(pt_prev,
								  skb, 0);
				} else {
					atomic_inc(&skb->users);
					ret = pt_prev->func(skb, skb->dev,
							    pt_prev);
				}
			}
			pt_prev = ptype;
		}
	}

	if (pt_prev) {
		if (!pt_prev->data) {
			ret = deliver_to_old_ones(pt_prev, skb, 1);
		} else {
			ret = pt_prev->func(skb, skb->dev, pt_prev);
		}
	} else {
		kfree_skb(skb);
		ret = NET_RX_DROP;
	}

	return ret;
}

static int process_backlog(struct net_device *backlog_dev, int *budget)
{
	int work = 0;
	int quota = min(backlog_dev->quota, *budget);
	int this_cpu = smp_processor_id();
	struct softnet_data *queue = &softnet_data[this_cpu];
	unsigned long start_time = jiffies;

	for (;;) {
		struct sk_buff *skb;
		struct net_device *dev;

		local_irq_disable();
		skb = __skb_dequeue(&queue->input_pkt_queue);
		if (!skb)
			goto job_done;
		local_irq_enable();

		dev = skb->dev;

		netif_receive_skb(skb);

		dev_put(dev);

		work++;

		if (work >= quota || jiffies - start_time > 1)
			break;

#ifdef CONFIG_NET_HW_FLOWCONTROL
		if (queue->throttle &&
		    queue->input_pkt_queue.qlen < no_cong_thresh ) {
			if (atomic_dec_and_test(&netdev_dropping)) {
				queue->throttle = 0;
				netdev_wakeup();
				break;
			}
		}
#endif
	}

	backlog_dev->quota -= work;
	*budget -= work;
	return -1;

job_done:
	backlog_dev->quota -= work;
	*budget -= work;

	list_del(&backlog_dev->poll_list);
	clear_bit(__LINK_STATE_RX_SCHED, &backlog_dev->state);

	if (queue->throttle) {
		queue->throttle = 0;
#ifdef CONFIG_NET_HW_FLOWCONTROL
		if (atomic_dec_and_test(&netdev_dropping))
			netdev_wakeup();
#endif
	}
	local_irq_enable();
	return 0;
}

static void net_rx_action(struct softirq_action *h)
{
	int this_cpu = smp_processor_id();
	struct softnet_data *queue = &softnet_data[this_cpu];
	unsigned long start_time = jiffies;
	int budget = netdev_max_backlog;

	br_read_lock(BR_NETPROTO_LOCK);
	local_irq_disable();

	while (!list_empty(&queue->poll_list)) {
		struct net_device *dev;

		if (budget <= 0 || jiffies - start_time > 1)
			goto softnet_break;

		local_irq_enable();

		dev = list_entry(queue->poll_list.next,
				 struct net_device, poll_list);

		if (dev->quota <= 0 || dev->poll(dev, &budget)) {
			local_irq_disable();
			list_del(&dev->poll_list);
			list_add_tail(&dev->poll_list, &queue->poll_list);
			if (dev->quota < 0)
				dev->quota += dev->weight;
			else
				dev->quota = dev->weight;
		} else {
			dev_put(dev);
			local_irq_disable();
		}
	}
out:
	local_irq_enable();
	br_read_unlock(BR_NETPROTO_LOCK);
	return;

softnet_break:
	netdev_rx_stat[this_cpu].time_squeeze++;
	__cpu_raise_softirq(this_cpu, NET_RX_SOFTIRQ);
	goto out;
}

static gifconf_func_t * gifconf_list [NPROTO];

int register_gifconf(unsigned int family, gifconf_func_t * gifconf)
{
	if (family >= NPROTO)
		return -EINVAL;
	gifconf_list[family] = gifconf;
	return 0;
}




static int dev_ifname(struct ifreq *arg)
{
	struct net_device *dev;
	struct ifreq ifr;


	if (copy_from_user(&ifr, arg, sizeof(struct ifreq)))
		return -EFAULT;

	read_lock(&dev_base_lock);
	dev = __dev_get_by_index(ifr.ifr_ifindex);
	if (!dev) {
		read_unlock(&dev_base_lock);
		return -ENODEV;
	}

	strcpy(ifr.ifr_name, dev->name);
	read_unlock(&dev_base_lock);

	if (copy_to_user(arg, &ifr, sizeof(struct ifreq)))
		return -EFAULT;
	return 0;
}


static int dev_ifconf(char *arg)
{
	struct ifconf ifc;
	struct net_device *dev;
	char *pos;
	int len;
	int total;
	int i;


	if (copy_from_user(&ifc, arg, sizeof(struct ifconf)))
		return -EFAULT;

	pos = ifc.ifc_buf;
	len = ifc.ifc_len;


	total = 0;
	for (dev = dev_base; dev; dev = dev->next) {
		for (i = 0; i < NPROTO; i++) {
			if (gifconf_list[i]) {
				int done;
				if (!pos)
					done = gifconf_list[i](dev, NULL, 0);
				else
					done = gifconf_list[i](dev, pos + total,
							       len - total);
				if (done < 0)
					return -EFAULT;
				total += done;
			}
		}
  	}

	ifc.ifc_len = total;

	return copy_to_user(arg, &ifc, sizeof(struct ifconf)) ? -EFAULT : 0;
}


#ifdef CONFIG_PROC_FS

static int sprintf_stats(char *buffer, struct net_device *dev)
{
	struct net_device_stats *stats = dev->get_stats ? dev->get_stats(dev) :
							  NULL;
	int size;

	if (stats)
		size = sprintf(buffer, "%6s:%8lu %7lu %4lu %4lu %4lu %5lu "
				       "%10lu %9lu %8lu %7lu %4lu %4lu %4lu "
				       "%5lu %7lu %10lu\n",
 		   dev->name,
		   stats->rx_bytes,
		   stats->rx_packets, stats->rx_errors,
		   stats->rx_dropped + stats->rx_missed_errors,
		   stats->rx_fifo_errors,
		   stats->rx_length_errors + stats->rx_over_errors +
		   	stats->rx_crc_errors + stats->rx_frame_errors,
		   stats->rx_compressed, stats->multicast,
		   stats->tx_bytes,
		   stats->tx_packets, stats->tx_errors, stats->tx_dropped,
		   stats->tx_fifo_errors, stats->collisions,
		   stats->tx_carrier_errors + stats->tx_aborted_errors +
		   	stats->tx_window_errors + stats->tx_heartbeat_errors,
		   stats->tx_compressed);
	else
		size = sprintf(buffer, "%6s: No statistics available.\n",
			       dev->name);

	return size;
}

static int dev_get_info(char *buffer, char **start, off_t offset, int length)
{
	int len = 0;
	off_t begin = 0;
	off_t pos = 0;
	int size;
	struct net_device *dev;

	size = sprintf(buffer,
		"Inter-|   Receive                                                |  Transmit\n"
		" face |bytes    packets errs drop fifo frame compressed multicast|bytes    packets errs drop fifo colls carrier compressed\n");

	pos += size;
	len += size;

	read_lock(&dev_base_lock);
	for (dev = dev_base; dev; dev = dev->next) {
		size = sprintf_stats(buffer+len, dev);
		len += size;
		pos = begin + len;

		if (pos < offset) {
			len = 0;
			begin = pos;
		}
		if (pos > offset + length)
			break;
	}
	read_unlock(&dev_base_lock);

	*start = buffer + (offset - begin);	
	len -= offset - begin;			
	if (len > length)
		len = length;			
	if (len < 0)
		len = 0;
	return len;
}

static int dev_proc_stats(char *buffer, char **start, off_t offset,
			  int length, int *eof, void *data)
{
	int i;
	int len = 0;

	for (i = 0; i < NR_CPUS; i++) {
		if (!cpu_online(i))
			continue;

		len += sprintf(buffer + len, "%08x %08x %08x %08x %08x %08x "
					     "%08x %08x %08x\n",
			       netdev_rx_stat[i].total,
			       netdev_rx_stat[i].dropped,
			       netdev_rx_stat[i].time_squeeze,
			       netdev_rx_stat[i].throttled,
			       netdev_rx_stat[i].fastroute_hit,
			       netdev_rx_stat[i].fastroute_success,
			       netdev_rx_stat[i].fastroute_defer,
			       netdev_rx_stat[i].fastroute_deferred_out,
#if 0
			       netdev_rx_stat[i].fastroute_latency_reduction
#else
			       netdev_rx_stat[i].cpu_collision
#endif
			       );
	}

	len -= offset;

	if (len > length)
		len = length;
	if (len < 0)
		len = 0;

	*start = buffer + offset;
	*eof = 1;

	return len;
}

#endif	


int netdev_set_master(struct net_device *slave, struct net_device *master)
{
	struct net_device *old = slave->master;

	ASSERT_RTNL();

	if (master) {
		if (old)
			return -EBUSY;
		dev_hold(master);
	}

	br_write_lock_bh(BR_NETPROTO_LOCK);
	slave->master = master;
	br_write_unlock_bh(BR_NETPROTO_LOCK);

	if (old)
		dev_put(old);

	if (master)
		slave->flags |= IFF_SLAVE;
	else
		slave->flags &= ~IFF_SLAVE;

	rtmsg_ifinfo(RTM_NEWLINK, slave, IFF_SLAVE);
	return 0;
}

void dev_set_promiscuity(struct net_device *dev, int inc)
{
	unsigned short old_flags = dev->flags;

	dev->flags |= IFF_PROMISC;
	if ((dev->promiscuity += inc) == 0)
		dev->flags &= ~IFF_PROMISC;
	if (dev->flags ^ old_flags) {
#ifdef CONFIG_NET_FASTROUTE
		if (dev->flags & IFF_PROMISC) {
			netdev_fastroute_obstacles++;
			dev_clear_fastroute(dev);
		} else
			netdev_fastroute_obstacles--;
#endif
		dev_mc_upload(dev);
		printk(KERN_INFO "device %s %s promiscuous mode\n",
		       dev->name, (dev->flags & IFF_PROMISC) ? "entered" :
		       					       "left");
	}
}


void dev_set_allmulti(struct net_device *dev, int inc)
{
	unsigned short old_flags = dev->flags;

	dev->flags |= IFF_ALLMULTI;
	if ((dev->allmulti += inc) == 0)
		dev->flags &= ~IFF_ALLMULTI;
	if (dev->flags ^ old_flags)
		dev_mc_upload(dev);
}

int dev_change_flags(struct net_device *dev, unsigned flags)
{
	int ret;
	int old_flags = dev->flags;


	dev->flags = (flags & (IFF_DEBUG | IFF_NOTRAILERS | IFF_NOARP |
			       IFF_DYNAMIC | IFF_MULTICAST | IFF_PORTSEL |
			       IFF_AUTOMEDIA)) |
		     (dev->flags & (IFF_UP | IFF_VOLATILE | IFF_PROMISC |
				    IFF_ALLMULTI));


	dev_mc_upload(dev);


	ret = 0;
	if ((old_flags ^ flags) & IFF_UP) {	
		ret = ((old_flags & IFF_UP) ? dev_close : dev_open)(dev);

		if (!ret)
			dev_mc_upload(dev);
	}

	if (dev->flags & IFF_UP &&
	    ((old_flags ^ dev->flags) &~ (IFF_UP | IFF_PROMISC | IFF_ALLMULTI |
					  IFF_VOLATILE)))
		notifier_call_chain(&netdev_chain, NETDEV_CHANGE, dev);

	if ((flags ^ dev->gflags) & IFF_PROMISC) {
		int inc = (flags & IFF_PROMISC) ? +1 : -1;
		dev->gflags ^= IFF_PROMISC;
		dev_set_promiscuity(dev, inc);
	}

	if ((flags ^ dev->gflags) & IFF_ALLMULTI) {
		int inc = (flags & IFF_ALLMULTI) ? +1 : -1;
		dev->gflags ^= IFF_ALLMULTI;
		dev_set_allmulti(dev, inc);
	}

	if (old_flags ^ dev->flags)
		rtmsg_ifinfo(RTM_NEWLINK, dev, old_flags ^ dev->flags);

	return ret;
}

static int dev_ifsioc(struct ifreq *ifr, unsigned int cmd)
{
	int err;
	struct net_device *dev = __dev_get_by_name(ifr->ifr_name);

	if (!dev)
		return -ENODEV;

	switch (cmd) {
		case SIOCGIFFLAGS:	
			ifr->ifr_flags = (dev->flags & ~(IFF_PROMISC |
							 IFF_ALLMULTI |
							 IFF_RUNNING)) | 
					 (dev->gflags & (IFF_PROMISC |
							 IFF_ALLMULTI));
			if (netif_running(dev) && netif_carrier_ok(dev))
				ifr->ifr_flags |= IFF_RUNNING;
			return 0;

		case SIOCSIFFLAGS:	
			return dev_change_flags(dev, ifr->ifr_flags);

		case SIOCGIFMETRIC:	
			ifr->ifr_metric = 0;
			return 0;

		case SIOCSIFMETRIC:	
			return -EOPNOTSUPP;

		case SIOCGIFMTU:	
			ifr->ifr_mtu = dev->mtu;
			return 0;

		case SIOCSIFMTU:	
			if (ifr->ifr_mtu == dev->mtu)
				return 0;

			if (ifr->ifr_mtu < 0)
				return -EINVAL;

			if (!netif_device_present(dev))
				return -ENODEV;

			err = 0;
			if (dev->change_mtu)
				err = dev->change_mtu(dev, ifr->ifr_mtu);
			else
				dev->mtu = ifr->ifr_mtu;
			if (!err && dev->flags & IFF_UP)
				notifier_call_chain(&netdev_chain,
						    NETDEV_CHANGEMTU, dev);
			return err;

		case SIOCGIFHWADDR:
			memcpy(ifr->ifr_hwaddr.sa_data, dev->dev_addr,
			       MAX_ADDR_LEN);
			ifr->ifr_hwaddr.sa_family = dev->type;
			return 0;

		case SIOCSIFHWADDR:
			if (!dev->set_mac_address)
				return -EOPNOTSUPP;
			if (ifr->ifr_hwaddr.sa_family != dev->type)
				return -EINVAL;
			if (!netif_device_present(dev))
				return -ENODEV;
			err = dev->set_mac_address(dev, &ifr->ifr_hwaddr);
			if (!err)
				notifier_call_chain(&netdev_chain,
						    NETDEV_CHANGEADDR, dev);
			return err;

		case SIOCSIFHWBROADCAST:
			if (ifr->ifr_hwaddr.sa_family != dev->type)
				return -EINVAL;
			memcpy(dev->broadcast, ifr->ifr_hwaddr.sa_data,
			       MAX_ADDR_LEN);
			notifier_call_chain(&netdev_chain,
					    NETDEV_CHANGEADDR, dev);
			return 0;

		case SIOCGIFMAP:
			ifr->ifr_map.mem_start = dev->mem_start;
			ifr->ifr_map.mem_end   = dev->mem_end;
			ifr->ifr_map.base_addr = dev->base_addr;
			ifr->ifr_map.irq       = dev->irq;
			ifr->ifr_map.dma       = dev->dma;
			ifr->ifr_map.port      = dev->if_port;
			return 0;

		case SIOCSIFMAP:
			if (dev->set_config) {
				if (!netif_device_present(dev))
					return -ENODEV;
				return dev->set_config(dev, &ifr->ifr_map);
			}
			return -EOPNOTSUPP;

		case SIOCADDMULTI:
			if (!dev->set_multicast_list ||
			    ifr->ifr_hwaddr.sa_family != AF_UNSPEC)
				return -EINVAL;
			if (!netif_device_present(dev))
				return -ENODEV;
			dev_mc_add(dev, ifr->ifr_hwaddr.sa_data,
				   dev->addr_len, 1);
			return 0;

		case SIOCDELMULTI:
			if (!dev->set_multicast_list ||
			    ifr->ifr_hwaddr.sa_family != AF_UNSPEC)
				return -EINVAL;
			if (!netif_device_present(dev))
				return -ENODEV;
			dev_mc_delete(dev, ifr->ifr_hwaddr.sa_data,
				      dev->addr_len, 1);
			return 0;

		case SIOCGIFINDEX:
			ifr->ifr_ifindex = dev->ifindex;
			return 0;

		case SIOCGIFTXQLEN:
			ifr->ifr_qlen = dev->tx_queue_len;
			return 0;

		case SIOCSIFTXQLEN:
			if (ifr->ifr_qlen < 0)
				return -EINVAL;
			dev->tx_queue_len = ifr->ifr_qlen;
			return 0;

		case SIOCSIFNAME:
			if (dev->flags & IFF_UP)
				return -EBUSY;
			if (__dev_get_by_name(ifr->ifr_newname))
				return -EEXIST;
			memcpy(dev->name, ifr->ifr_newname, IFNAMSIZ);
			dev->name[IFNAMSIZ - 1] = 0;
			notifier_call_chain(&netdev_chain,
					    NETDEV_CHANGENAME, dev);
			return 0;


		default:
			if ((cmd >= SIOCDEVPRIVATE &&
			    cmd <= SIOCDEVPRIVATE + 15) ||
			    cmd == SIOCBONDENSLAVE ||
			    cmd == SIOCBONDRELEASE ||
			    cmd == SIOCBONDSETHWADDR ||
			    cmd == SIOCBONDSLAVEINFOQUERY ||
			    cmd == SIOCBONDINFOQUERY ||
			    cmd == SIOCBONDCHANGEACTIVE ||
			    cmd == SIOCETHTOOL ||
			    cmd == SIOCGMIIPHY ||
			    cmd == SIOCGMIIREG ||
			    cmd == SIOCSMIIREG ||
			    cmd == SIOCWANDEV) {
				err = -EOPNOTSUPP;
				if (dev->do_ioctl) {
					if (netif_device_present(dev))
						err = dev->do_ioctl(dev, ifr,
								    cmd);
					else
						err = -ENODEV;
				}
			} else
				err = -EINVAL;

	}
	return err;
}



int dev_ioctl(unsigned int cmd, void *arg)
{
	struct ifreq ifr;
	int ret;
	char *colon;


	if (cmd == SIOCGIFCONF) {
		rtnl_shlock();
		ret = dev_ifconf((char *) arg);
		rtnl_shunlock();
		return ret;
	}
	if (cmd == SIOCGIFNAME)
		return dev_ifname((struct ifreq *)arg);

	if (copy_from_user(&ifr, arg, sizeof(struct ifreq)))
		return -EFAULT;

	ifr.ifr_name[IFNAMSIZ-1] = 0;

	colon = strchr(ifr.ifr_name, ':');
	if (colon)
		*colon = 0;


	switch (cmd) {
		case SIOCGIFFLAGS:
		case SIOCGIFMETRIC:
		case SIOCGIFMTU:
		case SIOCGIFHWADDR:
		case SIOCGIFSLAVE:
		case SIOCGIFMAP:
		case SIOCGIFINDEX:
		case SIOCGIFTXQLEN:
			dev_load(ifr.ifr_name);
			read_lock(&dev_base_lock);
			ret = dev_ifsioc(&ifr, cmd);
			read_unlock(&dev_base_lock);
			if (!ret) {
				if (colon)
					*colon = ':';
				if (copy_to_user(arg, &ifr,
						 sizeof(struct ifreq)))
					ret = -EFAULT;
			}
			return ret;

		case SIOCETHTOOL:
		case SIOCGMIIPHY:
		case SIOCGMIIREG:
			if (!capable(CAP_NET_ADMIN))
				return -EPERM;
			dev_load(ifr.ifr_name);
			dev_probe_lock();
			rtnl_lock();
			ret = dev_ifsioc(&ifr, cmd);
			rtnl_unlock();
			dev_probe_unlock();
			if (!ret) {
				if (colon)
					*colon = ':';
				if (copy_to_user(arg, &ifr,
						 sizeof(struct ifreq)))
					ret = -EFAULT;
			}
			return ret;

		case SIOCSIFFLAGS:
		case SIOCSIFMETRIC:
		case SIOCSIFMTU:
		case SIOCSIFMAP:
		case SIOCSIFHWADDR:
		case SIOCSIFSLAVE:
		case SIOCADDMULTI:
		case SIOCDELMULTI:
		case SIOCSIFHWBROADCAST:
		case SIOCSIFTXQLEN:
		case SIOCSIFNAME:
		case SIOCSMIIREG:
		case SIOCBONDENSLAVE:
		case SIOCBONDRELEASE:
		case SIOCBONDSETHWADDR:
		case SIOCBONDSLAVEINFOQUERY:
		case SIOCBONDINFOQUERY:
		case SIOCBONDCHANGEACTIVE:
			if (!capable(CAP_NET_ADMIN))
				return -EPERM;
			dev_load(ifr.ifr_name);
			dev_probe_lock();
			rtnl_lock();
			ret = dev_ifsioc(&ifr, cmd);
			rtnl_unlock();
			dev_probe_unlock();
			return ret;

		case SIOCGIFMEM:
		case SIOCSIFMEM:
		case SIOCSIFLINK:
			return -EINVAL;

		default:
			if (cmd == SIOCWANDEV ||
			    (cmd >= SIOCDEVPRIVATE &&
			     cmd <= SIOCDEVPRIVATE + 15)) {
				dev_load(ifr.ifr_name);
				dev_probe_lock();
				rtnl_lock();
				ret = dev_ifsioc(&ifr, cmd);
				rtnl_unlock();
				dev_probe_unlock();
				if (!ret && copy_to_user(arg, &ifr,
							 sizeof(struct ifreq)))
					ret = -EFAULT;
				return ret;
			}
#ifdef WIRELESS_EXT
			
			if (cmd >= SIOCIWFIRST && cmd <= SIOCIWLAST) {
				if (IW_IS_SET(cmd) || cmd == SIOCGIWENCODE) {
					if (!capable(CAP_NET_ADMIN))
						return -EPERM;
				}
				dev_load(ifr.ifr_name);
				rtnl_lock();
				
				ret = wireless_process_ioctl(&ifr, cmd);
				rtnl_unlock();
				if (!ret && IW_IS_GET(cmd) &&
				    copy_to_user(arg, &ifr,
					    	 sizeof(struct ifreq)))
					ret = -EFAULT;
				return ret;
			}
#endif	
			return -EINVAL;
	}
}


int dev_new_index(void)
{
	static int ifindex;
	for (;;) {
		if (++ifindex <= 0)
			ifindex = 1;
		if (!__dev_get_by_index(ifindex))
			return ifindex;
	}
}

static int dev_boot_phase = 1;


int register_netdevice(struct net_device *dev)
{
	struct net_device *d, **dp;
	int ret;

	BUG_ON(dev_boot_phase);

	spin_lock_init(&dev->queue_lock);
	spin_lock_init(&dev->xmit_lock);
	dev->xmit_lock_owner = -1;
#ifdef CONFIG_NET_FASTROUTE
	dev->fastpath_lock = RW_LOCK_UNLOCKED;
#endif

#ifdef CONFIG_NET_DIVERT
	ret = alloc_divert_blk(dev);
	if (ret)
		goto out;
#endif 

	dev->iflink = -1;

	
	ret = -EIO;
	if (dev->init && dev->init(dev))
		goto out_err;

	dev->ifindex = dev_new_index();
	if (dev->iflink == -1)
		dev->iflink = dev->ifindex;

	
	ret = -EEXIST;
	for (dp = &dev_base; (d = *dp) != NULL; dp = &d->next) {
		if (d == dev || !strcmp(d->name, dev->name))
			goto out_err;
	}

	if (!dev->rebuild_header)
		dev->rebuild_header = default_rebuild_header;


	set_bit(__LINK_STATE_PRESENT, &dev->state);

	dev->next = NULL;
	dev_init_scheduler(dev);
	write_lock_bh(&dev_base_lock);
	*dp = dev;
	dev_hold(dev);
	dev->deadbeaf = 0;
	write_unlock_bh(&dev_base_lock);

	
	notifier_call_chain(&netdev_chain, NETDEV_REGISTER, dev);

	net_run_sbin_hotplug(dev, "register");
	ret = 0;

out:
	return ret;
out_err:
#ifdef CONFIG_NET_DIVERT
	free_divert_blk(dev);
#endif
	goto out;
}

int netdev_finish_unregister(struct net_device *dev)
{
	BUG_TRAP(!dev->ip_ptr);
	BUG_TRAP(!dev->ip6_ptr);
	BUG_TRAP(!dev->dn_ptr);

	if (!dev->deadbeaf) {
		printk(KERN_ERR "Freeing alive device %p, %s\n",
		       dev, dev->name);
		return 0;
	}
#ifdef NET_REFCNT_DEBUG
	printk(KERN_DEBUG "netdev_finish_unregister: %s%s.\n", dev->name,
	       (dev->features & NETIF_F_DYNALLOC)?"":", old style");
#endif
	if (dev->destructor)
		dev->destructor(dev);
	if (dev->features & NETIF_F_DYNALLOC)
		kfree(dev);
	return 0;
}


int unregister_netdevice(struct net_device *dev)
{
	unsigned long now, warning_time;
	struct net_device *d, **dp;

	BUG_ON(dev_boot_phase);

	
	if (dev->flags & IFF_UP)
		dev_close(dev);

	BUG_TRAP(!dev->deadbeaf);
	dev->deadbeaf = 1;

	
	for (dp = &dev_base; (d = *dp) != NULL; dp = &d->next) {
		if (d == dev) {
			write_lock_bh(&dev_base_lock);
			*dp = d->next;
			write_unlock_bh(&dev_base_lock);
			break;
		}
	}
	if (!d) {
		printk(KERN_DEBUG "unregister_netdevice: device %s/%p never "
				  "was registered\n", dev->name, dev);
		return -ENODEV;
	}

	
	br_write_lock_bh(BR_NETPROTO_LOCK);
	br_write_unlock_bh(BR_NETPROTO_LOCK);


#ifdef CONFIG_NET_FASTROUTE
	dev_clear_fastroute(dev);
#endif

	
	dev_shutdown(dev);
	
	net_run_sbin_hotplug(dev, "unregister");
	
	notifier_call_chain(&netdev_chain, NETDEV_UNREGISTER, dev);
	
	dev_mc_discard(dev);

	if (dev->uninit)
		dev->uninit(dev);

	
	BUG_TRAP(!dev->master);

#ifdef CONFIG_NET_DIVERT
	free_divert_blk(dev);
#endif

	if (dev->features & NETIF_F_DYNALLOC) {
#ifdef NET_REFCNT_DEBUG
		if (atomic_read(&dev->refcnt) != 1)
			printk(KERN_DEBUG "unregister_netdevice: holding %s "
					  "refcnt=%d\n",
			       dev->name, atomic_read(&dev->refcnt) - 1);
#endif
		goto out;
	}

	
	if (atomic_read(&dev->refcnt) == 1)
		goto out;

#ifdef NET_REFCNT_DEBUG
	printk(KERN_DEBUG "unregister_netdevice: waiting %s refcnt=%d\n",
	       dev->name, atomic_read(&dev->refcnt));
#endif


	now = warning_time = jiffies;
	while (atomic_read(&dev->refcnt) != 1) {
		if ((jiffies - now) > 1 * HZ) {
			
			notifier_call_chain(&netdev_chain,
					    NETDEV_UNREGISTER, dev);
		}
		current->state = TASK_INTERRUPTIBLE;
		schedule_timeout(HZ / 4);
		current->state = TASK_RUNNING;
		if ((jiffies - warning_time) > 10 * HZ) {
			printk(KERN_EMERG "unregister_netdevice: waiting for "
			       "%s to become free. Usage count = %d\n",
			       dev->name, atomic_read(&dev->refcnt));
			warning_time = jiffies;
		}
	}
out:
	dev_put(dev);
	return 0;
}



extern void net_device_init(void);
extern void ip_auto_config(void);
#ifdef CONFIG_NET_DIVERT
extern void dv_init(void);
#endif 


static int __init net_dev_init(void)
{
	struct net_device *dev, **dp;
	int i;

	BUG_ON(!dev_boot_phase);

#ifdef CONFIG_NET_DIVERT
	dv_init();
#endif 


	for (i = 0; i < NR_CPUS; i++) {
		struct softnet_data *queue;

		queue = &softnet_data[i];
		skb_queue_head_init(&queue->input_pkt_queue);
		queue->throttle = 0;
		queue->cng_level = 0;
		queue->avg_blog = 10; 
		queue->completion_queue = NULL;
		INIT_LIST_HEAD(&queue->poll_list);
		set_bit(__LINK_STATE_START, &queue->backlog_dev.state);
		queue->backlog_dev.weight = weight_p;
		queue->backlog_dev.poll = process_backlog;
		atomic_set(&queue->backlog_dev.refcnt, 1);
	}

#ifdef CONFIG_NET_PROFILE
	net_profile_init();
	NET_PROFILE_REGISTER(dev_queue_xmit);
	NET_PROFILE_REGISTER(softnet_process);
#endif

#ifdef OFFLINE_SAMPLE
	samp_timer.expires = jiffies + (10 * HZ);
	add_timer(&samp_timer);
#endif


	dp = &dev_base;
	while ((dev = *dp) != NULL) {
		spin_lock_init(&dev->queue_lock);
		spin_lock_init(&dev->xmit_lock);
#ifdef CONFIG_NET_FASTROUTE
		dev->fastpath_lock = RW_LOCK_UNLOCKED;
#endif
		dev->xmit_lock_owner = -1;
		dev->iflink = -1;
		dev_hold(dev);

		if (strchr(dev->name, '%'))
			dev_alloc_name(dev, dev->name);

		netdev_boot_setup_check(dev);

		if (dev->init && dev->init(dev)) {
			dev->deadbeaf = 1;
			dp = &dev->next;
		} else {
			dp = &dev->next;
			dev->ifindex = dev_new_index();
			if (dev->iflink == -1)
				dev->iflink = dev->ifindex;
			if (!dev->rebuild_header)
				dev->rebuild_header = default_rebuild_header;
			dev_init_scheduler(dev);
			set_bit(__LINK_STATE_PRESENT, &dev->state);
		}
	}

	dp = &dev_base;
	while ((dev = *dp) != NULL) {
		if (dev->deadbeaf) {
			write_lock_bh(&dev_base_lock);
			*dp = dev->next;
			write_unlock_bh(&dev_base_lock);
			dev_put(dev);
		} else {
			dp = &dev->next;
		}
	}

#ifdef CONFIG_PROC_FS
	proc_net_create("dev", 0, dev_get_info);
	create_proc_read_entry("net/softnet_stat", 0, 0, dev_proc_stats, NULL);
#ifdef WIRELESS_EXT
	
	proc_net_create("wireless", 0, dev_get_wireless_info);
#endif	
#endif	

	dev_boot_phase = 0;

	open_softirq(NET_TX_SOFTIRQ, net_tx_action, NULL);
	open_softirq(NET_RX_SOFTIRQ, net_rx_action, NULL);

	dst_init();
	dev_mcast_init();

#ifdef CONFIG_NET_SCHED
	pktsched_init();
#endif

	net_device_init();

	return 0;
}

subsys_initcall(net_dev_init);

#ifdef CONFIG_HOTPLUG


static int net_run_sbin_hotplug(struct net_device *dev, char *action)
{
	char *argv[3], *envp[5], ifname[12 + IFNAMSIZ], action_str[32];
	int i;

	sprintf(ifname, "INTERFACE=%s", dev->name);
	sprintf(action_str, "ACTION=%s", action);

        i = 0;
        argv[i++] = hotplug_path;
        argv[i++] = "net";
        argv[i] = 0;

	i = 0;
	
	envp [i++] = "HOME=/";
	envp [i++] = "PATH=/sbin:/bin:/usr/sbin:/usr/bin";
	envp [i++] = ifname;
	envp [i++] = action_str;
	envp [i] = 0;

	return call_usermodehelper(argv [0], argv, envp);
}
#endif
