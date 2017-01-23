#ifndef __LINUX_BRIDGE_EBT_NAT_H
#define __LINUX_BRIDGE_EBT_NAT_H

struct ebt_inat_tuple
{
	int enabled;
	unsigned char mac[ETH_ALEN];
	
	int target;
};

struct ebt_inat_info
{
	uint32_t ip_subnet;
	struct ebt_inat_tuple a[256];
	
	int target;
};

#define EBT_ISNAT_TARGET "isnat"
#define EBT_IDNAT_TARGET "idnat"

#endif
