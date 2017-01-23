/*
 * Description: EBTables 802.1Q match extension kernelspace module.
 * Authors: Nick Fedchik <nick@fedchik.org.ua>
 *          Bart De Schuymer <bdschuym@pandora.be>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/if_ether.h>
#include <linux/if_vlan.h>
#include <linux/module.h>
#include <linux/netfilter_bridge/ebtables.h>
#include <linux/netfilter_bridge/ebt_vlan.h>

static unsigned char debug;
#define MODULE_VERSION "0.6"

MODULE_PARM(debug, "0-1b");
MODULE_PARM_DESC(debug, "debug=1 is turn on debug messages");
MODULE_AUTHOR("Nick Fedchik <nick@fedchik.org.ua>");
MODULE_DESCRIPTION("802.1Q match module (ebtables extension), v"
		   MODULE_VERSION);
MODULE_LICENSE("GPL");


#define DEBUG_MSG(...) if (debug) printk (KERN_DEBUG "ebt_vlan: " __VA_ARGS__)
#define INV_FLAG(_inv_flag_) (info->invflags & _inv_flag_) ? "!" : ""
#define GET_BITMASK(_BIT_MASK_) info->bitmask & _BIT_MASK_
#define SET_BITMASK(_BIT_MASK_) info->bitmask |= _BIT_MASK_
#define EXIT_ON_MISMATCH(_MATCH_,_MASK_) {if (!((info->_MATCH_ == _MATCH_)^!!(info->invflags & _MASK_))) return EBT_NOMATCH;}

static int
ebt_filter_vlan(const struct sk_buff *skb,
		const struct net_device *in,
		const struct net_device *out,
		const void *data, unsigned int datalen)
{
	struct ebt_vlan_info *info = (struct ebt_vlan_info *) data;
	struct vlan_ethhdr frame;

	unsigned short TCI;	
	unsigned short id;	
	unsigned char prio;	
	
	unsigned short encap;

	if (skb_copy_bits(skb, 0, &frame, sizeof(frame)))
		return EBT_NOMATCH;

	TCI = ntohs(frame.h_vlan_TCI);
	id = TCI & VLAN_VID_MASK;
	prio = (TCI >> 13) & 0x7;
	encap = frame.h_vlan_encapsulated_proto;

	
	if (GET_BITMASK(EBT_VLAN_ID))
		EXIT_ON_MISMATCH(id, EBT_VLAN_ID);

	
	if (GET_BITMASK(EBT_VLAN_PRIO))
		EXIT_ON_MISMATCH(prio, EBT_VLAN_PRIO);

	
	if (GET_BITMASK(EBT_VLAN_ENCAP))
		EXIT_ON_MISMATCH(encap, EBT_VLAN_ENCAP);

	return EBT_MATCH;
}

static int
ebt_check_vlan(const char *tablename,
	       unsigned int hooknr,
	       const struct ebt_entry *e, void *data, unsigned int datalen)
{
	struct ebt_vlan_info *info = (struct ebt_vlan_info *) data;

	
	if (datalen != sizeof(struct ebt_vlan_info)) {
		DEBUG_MSG
		    ("passed size %d is not eq to ebt_vlan_info (%Zd)\n",
		     datalen, sizeof(struct ebt_vlan_info));
		return -EINVAL;
	}

	
	if (e->ethproto != __constant_htons(ETH_P_8021Q)) {
		DEBUG_MSG
		    ("passed entry proto %2.4X is not 802.1Q (8100)\n",
		     (unsigned short) ntohs(e->ethproto));
		return -EINVAL;
	}

	if (info->bitmask & ~EBT_VLAN_MASK) {
		DEBUG_MSG("bitmask %2X is out of mask (%2X)\n",
			  info->bitmask, EBT_VLAN_MASK);
		return -EINVAL;
	}

	
	if (info->invflags & ~EBT_VLAN_MASK) {
		DEBUG_MSG("inversion flags %2X is out of mask (%2X)\n",
			  info->invflags, EBT_VLAN_MASK);
		return -EINVAL;
	}

	if (GET_BITMASK(EBT_VLAN_ID)) {
		if (!!info->id) { 
			if (info->id > VLAN_GROUP_ARRAY_LEN) {
				DEBUG_MSG
				    ("id %d is out of range (1-4096)\n",
				     info->id);
				return -EINVAL;
			}
			info->bitmask &= ~EBT_VLAN_PRIO;
		}
		
	}

	if (GET_BITMASK(EBT_VLAN_PRIO)) {
		if ((unsigned char) info->prio > 7) {
			DEBUG_MSG("prio %d is out of range (0-7)\n",
			     info->prio);
			return -EINVAL;
		}
	}
	if (GET_BITMASK(EBT_VLAN_ENCAP)) {
		if ((unsigned short) ntohs(info->encap) < ETH_ZLEN) {
			DEBUG_MSG
			    ("encap frame length %d is less than minimal\n",
			     ntohs(info->encap));
			return -EINVAL;
		}
	}

	return 0;
}

static struct ebt_match filter_vlan = {
	.name		= EBT_VLAN_MATCH,
	.match		= ebt_filter_vlan,
	.check		= ebt_check_vlan,
	.me		= THIS_MODULE,
};

static int __init init(void)
{
	DEBUG_MSG("ebtables 802.1Q extension module v"
		  MODULE_VERSION "\n");
	DEBUG_MSG("module debug=%d\n", !!debug);
	return ebt_register_match(&filter_vlan);
}

static void __exit fini(void)
{
	ebt_unregister_match(&filter_vlan);
}

module_init(init);
module_exit(fini);
