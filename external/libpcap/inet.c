/*
 * Copyright (c) 1994, 1995, 1996, 1997, 1998
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the Computer Systems
 *	Engineering Group at Lawrence Berkeley Laboratory.
 * 4. Neither the name of the University nor of the Laboratory may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static const char rcsid[] _U_ =
    "@(#) $Header: /tcpdump/master/libpcap/inet.c,v 1.79 2008-04-20 18:19:02 guy Exp $ (LBL)";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef WIN32
#include <pcap-stdinc.h>
#else 

#include <sys/param.h>
#ifndef MSDOS
#include <sys/file.h>
#endif
#include <sys/ioctl.h>
#include <sys/socket.h>
#ifdef HAVE_SYS_SOCKIO_H
#include <sys/sockio.h>
#endif

struct mbuf;		
struct rtentry;		
#include <net/if.h>
#include <netinet/in.h>
#endif 

#include <ctype.h>
#include <errno.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if !defined(WIN32) && !defined(__BORLANDC__)
#include <unistd.h>
#endif 
#ifdef HAVE_LIMITS_H
#include <limits.h>
#else
#define INT_MAX		2147483647
#endif

#include "pcap-int.h"

#ifdef HAVE_OS_PROTO_H
#include "os-proto.h"
#endif

#ifdef IFF_LOOPBACK
#define ISLOOPBACK(name, flags) ((flags) & IFF_LOOPBACK)
#else
#define ISLOOPBACK(name, flags) ((name)[0] == 'l' && (name)[1] == 'o' && \
    (isdigit((unsigned char)((name)[2])) || (name)[2] == '\0'))
#endif

struct sockaddr *
dup_sockaddr(struct sockaddr *sa, size_t sa_length)
{
	struct sockaddr *newsa;

	if ((newsa = malloc(sa_length)) == NULL)
		return (NULL);
	return (memcpy(newsa, sa, sa_length));
}

static int
get_instance(const char *name)
{
	const char *cp, *endcp;
	int n;

	if (strcmp(name, "any") == 0) {
		return INT_MAX;
	}

	endcp = name + strlen(name);
	for (cp = name; cp < endcp && !isdigit((unsigned char)*cp); ++cp)
		continue;

	if (isdigit((unsigned char)*cp))
		n = atoi(cp);
	else
		n = 0;
	return (n);
}

int
add_or_find_if(pcap_if_t **curdev_ret, pcap_if_t **alldevs, const char *name,
    u_int flags, const char *description, char *errbuf)
{
	pcap_t *p;
	pcap_if_t *curdev, *prevdev, *nextdev;
	int this_instance;
	char open_errbuf[PCAP_ERRBUF_SIZE];

	for (curdev = *alldevs; curdev != NULL; curdev = curdev->next) {
		if (strcmp(name, curdev->name) == 0)
			break;	
	}

	if (curdev == NULL) {
#ifdef __APPLE__
		if (strncmp(name, "wlt", 3) == 0) {
			char *en_name;
			size_t en_name_len;

			en_name_len = strlen(name) - 1;
			en_name = malloc(en_name_len + 1);
			if (en_name == NULL) {
				(void)snprintf(errbuf, PCAP_ERRBUF_SIZE,
				    "malloc: %s", pcap_strerror(errno));
				return (-1);
			}
			strcpy(en_name, "en");
			strcat(en_name, name + 3);
			p = pcap_open_live(en_name, 68, 0, 0, open_errbuf);
			free(en_name);
		} else
#endif 
		p = pcap_open_live(name, 68, 0, 0, open_errbuf);
		if (p == NULL) {
			*curdev_ret = NULL;
			return (0);
		}
		pcap_close(p);

		curdev = malloc(sizeof(pcap_if_t));
		if (curdev == NULL) {
			(void)snprintf(errbuf, PCAP_ERRBUF_SIZE,
			    "malloc: %s", pcap_strerror(errno));
			return (-1);
		}

		curdev->next = NULL;
		curdev->name = strdup(name);
		if (curdev->name == NULL) {
			(void)snprintf(errbuf, PCAP_ERRBUF_SIZE,
			    "malloc: %s", pcap_strerror(errno));
			free(curdev);
			return (-1);
		}
		if (description != NULL) {
			curdev->description = strdup(description);
			if (curdev->description == NULL) {
				(void)snprintf(errbuf, PCAP_ERRBUF_SIZE,
				    "malloc: %s", pcap_strerror(errno));
				free(curdev->name);
				free(curdev);
				return (-1);
			}
		} else {
			curdev->description = NULL;
		}
		curdev->addresses = NULL;	
		curdev->flags = 0;
		if (ISLOOPBACK(name, flags))
			curdev->flags |= PCAP_IF_LOOPBACK;

		this_instance = get_instance(name);

		prevdev = NULL;
		for (;;) {
			if (prevdev == NULL) {
				nextdev = *alldevs;
			} else
				nextdev = prevdev->next;

			if (nextdev == NULL) {
				break;
			}

			if (!(curdev->flags & PCAP_IF_LOOPBACK) &&
			    (nextdev->flags & PCAP_IF_LOOPBACK)) {
				break;
			}

			if (this_instance < get_instance(nextdev->name) &&
			    (!(curdev->flags & PCAP_IF_LOOPBACK) ||
			       (nextdev->flags & PCAP_IF_LOOPBACK))) {
				break;
			}

			prevdev = nextdev;
		}

		curdev->next = nextdev;

		if (prevdev == NULL) {
			*alldevs = curdev;
		} else
			prevdev->next = curdev;
	}

	*curdev_ret = curdev;
	return (0);
}

int
add_addr_to_iflist(pcap_if_t **alldevs, const char *name, u_int flags,
    struct sockaddr *addr, size_t addr_size,
    struct sockaddr *netmask, size_t netmask_size,
    struct sockaddr *broadaddr, size_t broadaddr_size,
    struct sockaddr *dstaddr, size_t dstaddr_size,
    char *errbuf)
{
	pcap_if_t *curdev;
	char *description = NULL;
	pcap_addr_t *curaddr, *prevaddr, *nextaddr;
#ifdef SIOCGIFDESCR
	int s;
	struct ifreq ifrdesc;
#ifndef IFDESCRSIZE
	size_t descrlen = 64;
#else
	size_t descrlen = IFDESCRSIZE;
#endif 
#endif 

#ifdef SIOCGIFDESCR
	memset(&ifrdesc, 0, sizeof ifrdesc);
	strlcpy(ifrdesc.ifr_name, name, sizeof ifrdesc.ifr_name);
	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s >= 0) {
#ifdef __FreeBSD__
		for (;;) {
			free(description);
			if ((description = malloc(descrlen)) != NULL) {
				ifrdesc.ifr_buffer.buffer = description;
				ifrdesc.ifr_buffer.length = descrlen;
				if (ioctl(s, SIOCGIFDESCR, &ifrdesc) == 0) {
					if (ifrdesc.ifr_buffer.buffer ==
					    description)
						break;
					else
						descrlen = ifrdesc.ifr_buffer.length;
				} else {
					free(description);
					description = NULL;
					break;
				}
			} else
				break;
		}
#else 
		if ((description = malloc(descrlen)) != NULL) {
			ifrdesc.ifr_data = (caddr_t)description;
			if (ioctl(s, SIOCGIFDESCR, &ifrdesc) != 0) {
				free(description);
				description = NULL;
			}
		}
#endif 
		close(s);
		if (description != NULL && strlen(description) == 0) {
			free(description);
			description = NULL;
		}
	}
#endif 

	if (add_or_find_if(&curdev, alldevs, name, flags, description,
	    errbuf) == -1) {
		free(description);
		return (-1);
	}
	free(description);
	if (curdev == NULL) {
		return (0);
	}

	curaddr = malloc(sizeof(pcap_addr_t));
	if (curaddr == NULL) {
		(void)snprintf(errbuf, PCAP_ERRBUF_SIZE,
		    "malloc: %s", pcap_strerror(errno));
		return (-1);
	}

	curaddr->next = NULL;
	if (addr != NULL) {
		curaddr->addr = dup_sockaddr(addr, addr_size);
		if (curaddr->addr == NULL) {
			(void)snprintf(errbuf, PCAP_ERRBUF_SIZE,
			    "malloc: %s", pcap_strerror(errno));
			free(curaddr);
			return (-1);
		}
	} else
		curaddr->addr = NULL;

	if (netmask != NULL) {
		curaddr->netmask = dup_sockaddr(netmask, netmask_size);
		if (curaddr->netmask == NULL) {
			(void)snprintf(errbuf, PCAP_ERRBUF_SIZE,
			    "malloc: %s", pcap_strerror(errno));
			if (curaddr->addr != NULL)
				free(curaddr->addr);
			free(curaddr);
			return (-1);
		}
	} else
		curaddr->netmask = NULL;

	if (broadaddr != NULL) {
		curaddr->broadaddr = dup_sockaddr(broadaddr, broadaddr_size);
		if (curaddr->broadaddr == NULL) {
			(void)snprintf(errbuf, PCAP_ERRBUF_SIZE,
			    "malloc: %s", pcap_strerror(errno));
			if (curaddr->netmask != NULL)
				free(curaddr->netmask);
			if (curaddr->addr != NULL)
				free(curaddr->addr);
			free(curaddr);
			return (-1);
		}
	} else
		curaddr->broadaddr = NULL;

	if (dstaddr != NULL) {
		curaddr->dstaddr = dup_sockaddr(dstaddr, dstaddr_size);
		if (curaddr->dstaddr == NULL) {
			(void)snprintf(errbuf, PCAP_ERRBUF_SIZE,
			    "malloc: %s", pcap_strerror(errno));
			if (curaddr->broadaddr != NULL)
				free(curaddr->broadaddr);
			if (curaddr->netmask != NULL)
				free(curaddr->netmask);
			if (curaddr->addr != NULL)
				free(curaddr->addr);
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

int
pcap_add_if(pcap_if_t **devlist, const char *name, u_int flags,
    const char *description, char *errbuf)
{
	pcap_if_t *curdev;

	return (add_or_find_if(&curdev, devlist, name, flags, description,
	    errbuf));
}


void
pcap_freealldevs(pcap_if_t *alldevs)
{
	pcap_if_t *curdev, *nextdev;
	pcap_addr_t *curaddr, *nextaddr;

	for (curdev = alldevs; curdev != NULL; curdev = nextdev) {
		nextdev = curdev->next;

		for (curaddr = curdev->addresses; curaddr != NULL; curaddr = nextaddr) {
			nextaddr = curaddr->next;
			if (curaddr->addr)
				free(curaddr->addr);
			if (curaddr->netmask)
				free(curaddr->netmask);
			if (curaddr->broadaddr)
				free(curaddr->broadaddr);
			if (curaddr->dstaddr)
				free(curaddr->dstaddr);
			free(curaddr);
		}

		free(curdev->name);

		if (curdev->description != NULL)
			free(curdev->description);

		free(curdev);
	}
}

#if !defined(WIN32) && !defined(MSDOS)

char *
pcap_lookupdev(errbuf)
	register char *errbuf;
{
	pcap_if_t *alldevs;
#ifndef IF_NAMESIZE
#define IF_NAMESIZE IFNAMSIZ
#endif
	static char device[IF_NAMESIZE + 1];
	char *ret;

	if (pcap_findalldevs(&alldevs, errbuf) == -1)
		return (NULL);

	if (alldevs == NULL || (alldevs->flags & PCAP_IF_LOOPBACK)) {
		(void)strlcpy(errbuf, "no suitable device found",
		    PCAP_ERRBUF_SIZE);
		ret = NULL;
	} else {
		(void)strlcpy(device, alldevs->name, sizeof(device));
		ret = device;
	}

	pcap_freealldevs(alldevs);
	return (ret);
}

int
pcap_lookupnet(device, netp, maskp, errbuf)
	register const char *device;
	register bpf_u_int32 *netp, *maskp;
	register char *errbuf;
{
	register int fd;
	register struct sockaddr_in *sin4;
	struct ifreq ifr;

	if (!device || strcmp(device, "any") == 0
#ifdef HAVE_DAG_API
	    || strstr(device, "dag") != NULL
#endif
#ifdef HAVE_SEPTEL_API
	    || strstr(device, "septel") != NULL
#endif
#ifdef PCAP_SUPPORT_BT
	    || strstr(device, "bluetooth") != NULL
#endif
#ifdef PCAP_SUPPORT_USB
	    || strstr(device, "usbmon") != NULL
#endif
#ifdef HAVE_SNF_API
	    || strstr(device, "snf") != NULL
#endif
	    ) {
		*netp = *maskp = 0;
		return 0;
	}

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0) {
		(void)snprintf(errbuf, PCAP_ERRBUF_SIZE, "socket: %s",
		    pcap_strerror(errno));
		return (-1);
	}
	memset(&ifr, 0, sizeof(ifr));
#ifdef linux
	
	ifr.ifr_addr.sa_family = AF_INET;
#endif
	(void)strncpy(ifr.ifr_name, device, sizeof(ifr.ifr_name));
	if (ioctl(fd, SIOCGIFADDR, (char *)&ifr) < 0) {
		if (errno == EADDRNOTAVAIL) {
			(void)snprintf(errbuf, PCAP_ERRBUF_SIZE,
			    "%s: no IPv4 address assigned", device);
		} else {
			(void)snprintf(errbuf, PCAP_ERRBUF_SIZE,
			    "SIOCGIFADDR: %s: %s",
			    device, pcap_strerror(errno));
		}
		(void)close(fd);
		return (-1);
	}
	sin4 = (struct sockaddr_in *)&ifr.ifr_addr;
	*netp = sin4->sin_addr.s_addr;
	memset(&ifr, 0, sizeof(ifr));
#ifdef linux
	
	ifr.ifr_addr.sa_family = AF_INET;
#endif
	(void)strncpy(ifr.ifr_name, device, sizeof(ifr.ifr_name));
	if (ioctl(fd, SIOCGIFNETMASK, (char *)&ifr) < 0) {
		(void)snprintf(errbuf, PCAP_ERRBUF_SIZE,
		    "SIOCGIFNETMASK: %s: %s", device, pcap_strerror(errno));
		(void)close(fd);
		return (-1);
	}
	(void)close(fd);
	*maskp = sin4->sin_addr.s_addr;
	if (*maskp == 0) {
		if (IN_CLASSA(*netp))
			*maskp = IN_CLASSA_NET;
		else if (IN_CLASSB(*netp))
			*maskp = IN_CLASSB_NET;
		else if (IN_CLASSC(*netp))
			*maskp = IN_CLASSC_NET;
		else {
			(void)snprintf(errbuf, PCAP_ERRBUF_SIZE,
			    "inet class for 0x%x unknown", *netp);
			return (-1);
		}
	}
	*netp &= *maskp;
	return (0);
}

#elif defined(WIN32)

char *
pcap_lookupdev(errbuf)
	register char *errbuf;
{
	DWORD dwVersion;
	DWORD dwWindowsMajorVersion;
	dwVersion = GetVersion();	
	dwWindowsMajorVersion = (DWORD)(LOBYTE(LOWORD(dwVersion)));
	
	if (dwVersion >= 0x80000000 && dwWindowsMajorVersion >= 4) {
		ULONG NameLength = 8192;
		static char AdaptersName[8192];
		
		if (PacketGetAdapterNames(AdaptersName,&NameLength) )
			return (AdaptersName);
		else
			return NULL;
	} else {
		ULONG NameLength = 8192;
		static WCHAR AdaptersName[8192];
		char *tAstr;
		WCHAR *tUstr;
		WCHAR *TAdaptersName = (WCHAR*)malloc(8192 * sizeof(WCHAR));
		int NAdapts = 0;

		if(TAdaptersName == NULL)
		{
			(void)snprintf(errbuf, PCAP_ERRBUF_SIZE, "memory allocation failure");
			return NULL;
		}

		if ( !PacketGetAdapterNames((PTSTR)TAdaptersName,&NameLength) )
		{
			(void)snprintf(errbuf, PCAP_ERRBUF_SIZE,
				"PacketGetAdapterNames: %s",
				pcap_win32strerror());
			free(TAdaptersName);
			return NULL;
		}


		tAstr = (char*)TAdaptersName;
		tUstr = (WCHAR*)AdaptersName;

		while(sscanf(tAstr, "%S", tUstr) > 0)
		{
			tAstr += strlen(tAstr) + 1;
			tUstr += wcslen(tUstr) + 1;
			NAdapts ++;
		}

		tAstr++;
		*tUstr = 0;
		tUstr++;

		while(NAdapts--)
		{
			char* tmp = (char*)tUstr;
			strcpy(tmp, tAstr);
			tmp += strlen(tAstr) + 1;
			tUstr = (WCHAR*)tmp;
			tAstr += strlen(tAstr) + 1;
		}

		free(TAdaptersName);
		return (char *)(AdaptersName);
	}	
}


int
pcap_lookupnet(device, netp, maskp, errbuf)
	register const char *device;
	register bpf_u_int32 *netp, *maskp;
	register char *errbuf;
{
	npf_if_addr if_addrs[MAX_NETWORK_ADDRESSES];
	LONG if_addr_size = 1;
	struct sockaddr_in *t_addr;
	unsigned int i;

	if (!PacketGetNetInfoEx((void *)device, if_addrs, &if_addr_size)) {
		*netp = *maskp = 0;
		return (0);
	}

	for(i=0; i<MAX_NETWORK_ADDRESSES; i++)
	{
		if(if_addrs[i].IPAddress.ss_family == AF_INET)
		{
			t_addr = (struct sockaddr_in *) &(if_addrs[i].IPAddress);
			*netp = t_addr->sin_addr.S_un.S_addr;
			t_addr = (struct sockaddr_in *) &(if_addrs[i].SubnetMask);
			*maskp = t_addr->sin_addr.S_un.S_addr;

			*netp &= *maskp;
			return (0);
		}
				
	}

	*netp = *maskp = 0;
	return (0);
}

#endif 
