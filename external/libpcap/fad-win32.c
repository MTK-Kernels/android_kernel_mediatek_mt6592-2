/*
 * Copyright (c) 2002 - 2005 NetGroup, Politecnico di Torino (Italy)
 * Copyright (c) 2005 - 2006 CACE Technologies, Davis (California)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Politecnico di Torino, CACE Technologies 
 * nor the names of its contributors may be used to endorse or promote 
 * products derived from this software without specific prior written 
 * permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef lint
static const char rcsid[] _U_ =
    "@(#) $Header: /tcpdump/master/libpcap/fad-win32.c,v 1.15 2007-09-25 20:34:36 guy Exp $ (LBL)";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <pcap.h>
#include <pcap-int.h>
#include <Packet32.h>

#include <errno.h>
	
static int
add_addr_to_list(pcap_if_t *curdev, struct sockaddr *addr,
    struct sockaddr *netmask, struct sockaddr *broadaddr,
    struct sockaddr *dstaddr, char *errbuf)
{
	pcap_addr_t *curaddr, *prevaddr, *nextaddr;

	curaddr = (pcap_addr_t*)malloc(sizeof(pcap_addr_t));
	if (curaddr == NULL) {
		(void)snprintf(errbuf, PCAP_ERRBUF_SIZE,
		    "malloc: %s", pcap_strerror(errno));
		return (-1);
	}

	curaddr->next = NULL;
	if (addr != NULL) {
		curaddr->addr = (struct sockaddr*)dup_sockaddr(addr, sizeof(struct sockaddr_storage));
		if (curaddr->addr == NULL) {
			(void)snprintf(errbuf, PCAP_ERRBUF_SIZE,
			    "malloc: %s", pcap_strerror(errno));
			free(curaddr);
			return (-1);
		}
	} else
		curaddr->addr = NULL;

	if (netmask != NULL) {
		curaddr->netmask = (struct sockaddr*)dup_sockaddr(netmask, sizeof(struct sockaddr_storage));
		if (curaddr->netmask == NULL) {
			(void)snprintf(errbuf, PCAP_ERRBUF_SIZE,
			    "malloc: %s", pcap_strerror(errno));
			free(curaddr);
			return (-1);
		}
	} else
		curaddr->netmask = NULL;
		
	if (broadaddr != NULL) {
		curaddr->broadaddr = (struct sockaddr*)dup_sockaddr(broadaddr, sizeof(struct sockaddr_storage));
		if (curaddr->broadaddr == NULL) {
			(void)snprintf(errbuf, PCAP_ERRBUF_SIZE,
			    "malloc: %s", pcap_strerror(errno));
			free(curaddr);
			return (-1);
		}
	} else
		curaddr->broadaddr = NULL;
		
	if (dstaddr != NULL) {
		curaddr->dstaddr = (struct sockaddr*)dup_sockaddr(dstaddr, sizeof(struct sockaddr_storage));
		if (curaddr->dstaddr == NULL) {
			(void)snprintf(errbuf, PCAP_ERRBUF_SIZE,
			    "malloc: %s", pcap_strerror(errno));
			free(curaddr);
			return (-1);
		}
	} else
		curaddr->dstaddr = NULL;
		
	for (prevaddr = curdev->addresses; prevaddr != NULL; prevaddr = nextaddr) {
		nextaddr = prevaddr->next;
		if (nextaddr == NULL) {
			break;
		}
	}

	if (prevaddr == NULL) {
		curdev->addresses = curaddr;
	} else {
		prevaddr->next = curaddr;
	}

	return (0);
}


static int
pcap_add_if_win32(pcap_if_t **devlist, char *name, const char *desc,
    char *errbuf)
{
	pcap_if_t *curdev;
	npf_if_addr if_addrs[MAX_NETWORK_ADDRESSES];
	LONG if_addr_size;
	int res = 0;

	if_addr_size = MAX_NETWORK_ADDRESSES;

	if (add_or_find_if(&curdev, devlist, name, 0, desc, errbuf) == -1) {
		return (-1);
	}

	if (!PacketGetNetInfoEx((void *)name, if_addrs, &if_addr_size)) {
		return (0);
	}

	while (if_addr_size-- > 0) {
		if(curdev == NULL)
			break;
		res = add_addr_to_list(curdev,
		    (struct sockaddr *)&if_addrs[if_addr_size].IPAddress,
		    (struct sockaddr *)&if_addrs[if_addr_size].SubnetMask,
		    (struct sockaddr *)&if_addrs[if_addr_size].Broadcast,
		    NULL,
			errbuf);
		if (res == -1) {
			break;
		}
	}

	return (res);
}


int
pcap_findalldevs_interfaces(pcap_if_t **alldevsp, char *errbuf)
{
	pcap_if_t *devlist = NULL;
	int ret = 0;
	const char *desc;
	char *AdaptersName;
	ULONG NameLength;
	char *name;
	
	NameLength = 0;
	if (!PacketGetAdapterNames(NULL, &NameLength))
	{
		DWORD last_error = GetLastError();

		if (last_error != ERROR_INSUFFICIENT_BUFFER)
		{
			snprintf(errbuf, PCAP_ERRBUF_SIZE,
				"PacketGetAdapterNames: %s",
				pcap_win32strerror());
			return (-1);
		}
	}

	if (NameLength > 0)
		AdaptersName = (char*) malloc(NameLength);
	else
	{
		*alldevsp = NULL;
		return 0;
	}
	if (AdaptersName == NULL)
	{
		snprintf(errbuf, PCAP_ERRBUF_SIZE, "Cannot allocate enough memory to list the adapters.");
		return (-1);
	}			

	if (!PacketGetAdapterNames(AdaptersName, &NameLength)) {
		snprintf(errbuf, PCAP_ERRBUF_SIZE,
			"PacketGetAdapterNames: %s",
			pcap_win32strerror());
		free(AdaptersName);
		return (-1);
	}
	
	desc = &AdaptersName[0];
	while (*desc != '\0' || *(desc + 1) != '\0')
		desc++;
	
	desc += 2;
	
	name = &AdaptersName[0];
	while (*name != '\0') {
		if (pcap_add_if_win32(&devlist, name, desc, errbuf) == -1) {
			ret = -1;
			break;
		}
		name += strlen(name) + 1;
		desc += strlen(desc) + 1;
	}

	if (ret != -1) {
		if (pcap_platform_finddevs(&devlist, errbuf) < 0)
			ret = -1;
	}
	
	if (ret == -1) {
		if (devlist != NULL) {
			pcap_freealldevs(devlist);
			devlist = NULL;
		}
	}
	
	*alldevsp = devlist;
	free(AdaptersName);
	return (ret);
}
