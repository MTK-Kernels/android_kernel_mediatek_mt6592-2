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
    "@(#) $Header: /tcpdump/master/libpcap/fad-gifc.c,v 1.12 2008-08-06 07:34:09 guy Exp $ (LBL)";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#ifdef HAVE_SYS_SOCKIO_H
#include <sys/sockio.h>
#endif
#include <sys/time.h>				

struct mbuf;		
struct rtentry;		
#include <net/if.h>
#include <netinet/in.h>

#include <ctype.h>
#include <errno.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pcap-int.h"

#ifdef HAVE_OS_PROTO_H
#include "os-proto.h"
#endif

#ifndef SA_LEN
#ifdef HAVE_SOCKADDR_SA_LEN
#define SA_LEN(addr)	((addr)->sa_len)
#else 
#define SA_LEN(addr)	(sizeof (struct sockaddr))
#endif 
#endif 

#define MAX_SA_LEN	255

int
pcap_findalldevs_interfaces(pcap_if_t **alldevsp, char *errbuf)
{
	pcap_if_t *devlist = NULL;
	register int fd;
	register struct ifreq *ifrp, *ifend, *ifnext;
	int n;
	struct ifconf ifc;
	char *buf = NULL;
	unsigned buf_size;
#if defined (HAVE_SOLARIS) || defined (HAVE_HPUX10_20_OR_LATER)
	char *p, *q;
#endif
	struct ifreq ifrflags, ifrnetmask, ifrbroadaddr, ifrdstaddr;
	struct sockaddr *netmask, *broadaddr, *dstaddr;
	size_t netmask_size, broadaddr_size, dstaddr_size;
	int ret = 0;

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0) {
		(void)snprintf(errbuf, PCAP_ERRBUF_SIZE,
		    "socket: %s", pcap_strerror(errno));
		return (-1);
	}

	buf_size = 8192;
	for (;;) {
		buf = malloc(buf_size);
		if (buf == NULL) {
			(void)snprintf(errbuf, PCAP_ERRBUF_SIZE,
			    "malloc: %s", pcap_strerror(errno));
			(void)close(fd);
			return (-1);
		}

		ifc.ifc_len = buf_size;
		ifc.ifc_buf = buf;
		memset(buf, 0, buf_size);
		if (ioctl(fd, SIOCGIFCONF, (char *)&ifc) < 0
		    && errno != EINVAL) {
			(void)snprintf(errbuf, PCAP_ERRBUF_SIZE,
			    "SIOCGIFCONF: %s", pcap_strerror(errno));
			(void)close(fd);
			free(buf);
			return (-1);
		}
		if (ifc.ifc_len < buf_size &&
		    (buf_size - ifc.ifc_len) > sizeof(ifrp->ifr_name) + MAX_SA_LEN)
			break;
		free(buf);
		buf_size *= 2;
	}

	ifrp = (struct ifreq *)buf;
	ifend = (struct ifreq *)(buf + ifc.ifc_len);

	for (; ifrp < ifend; ifrp = ifnext) {
		n = SA_LEN(&ifrp->ifr_addr) + sizeof(ifrp->ifr_name);
		if (n < sizeof(*ifrp))
			ifnext = ifrp + 1;
		else
			ifnext = (struct ifreq *)((char *)ifrp + n);

		if (!(*ifrp->ifr_name))
			break;

		if (strncmp(ifrp->ifr_name, "dummy", 5) == 0)
			continue;

		strncpy(ifrflags.ifr_name, ifrp->ifr_name,
		    sizeof(ifrflags.ifr_name));
		if (ioctl(fd, SIOCGIFFLAGS, (char *)&ifrflags) < 0) {
			if (errno == ENXIO)
				continue;
			(void)snprintf(errbuf, PCAP_ERRBUF_SIZE,
			    "SIOCGIFFLAGS: %.*s: %s",
			    (int)sizeof(ifrflags.ifr_name),
			    ifrflags.ifr_name,
			    pcap_strerror(errno));
			ret = -1;
			break;
		}
		if (!(ifrflags.ifr_flags & IFF_UP))
			continue;

		strncpy(ifrnetmask.ifr_name, ifrp->ifr_name,
		    sizeof(ifrnetmask.ifr_name));
		memcpy(&ifrnetmask.ifr_addr, &ifrp->ifr_addr,
		    sizeof(ifrnetmask.ifr_addr));
		if (ioctl(fd, SIOCGIFNETMASK, (char *)&ifrnetmask) < 0) {
			if (errno == EADDRNOTAVAIL) {
				netmask = NULL;
				netmask_size = 0;
			} else {
				(void)snprintf(errbuf, PCAP_ERRBUF_SIZE,
				    "SIOCGIFNETMASK: %.*s: %s",
				    (int)sizeof(ifrnetmask.ifr_name),
				    ifrnetmask.ifr_name,
				    pcap_strerror(errno));
				ret = -1;
				break;
			}
		} else {
			netmask = &ifrnetmask.ifr_addr;
			netmask_size = SA_LEN(netmask);
		}

		if (ifrflags.ifr_flags & IFF_BROADCAST) {
			strncpy(ifrbroadaddr.ifr_name, ifrp->ifr_name,
			    sizeof(ifrbroadaddr.ifr_name));
			memcpy(&ifrbroadaddr.ifr_addr, &ifrp->ifr_addr,
			    sizeof(ifrbroadaddr.ifr_addr));
			if (ioctl(fd, SIOCGIFBRDADDR,
			    (char *)&ifrbroadaddr) < 0) {
				if (errno == EADDRNOTAVAIL) {
					broadaddr = NULL;
					broadaddr_size = 0;
				} else {
					(void)snprintf(errbuf, PCAP_ERRBUF_SIZE,
					    "SIOCGIFBRDADDR: %.*s: %s",
					    (int)sizeof(ifrbroadaddr.ifr_name),
					    ifrbroadaddr.ifr_name,
					    pcap_strerror(errno));
					ret = -1;
					break;
				}
			} else {
				broadaddr = &ifrbroadaddr.ifr_broadaddr;
				broadaddr_size = SA_LEN(broadaddr);
			}
		} else {
			broadaddr = NULL;
			broadaddr_size = 0;
		}

		if (ifrflags.ifr_flags & IFF_POINTOPOINT) {
			strncpy(ifrdstaddr.ifr_name, ifrp->ifr_name,
			    sizeof(ifrdstaddr.ifr_name));
			memcpy(&ifrdstaddr.ifr_addr, &ifrp->ifr_addr,
			    sizeof(ifrdstaddr.ifr_addr));
			if (ioctl(fd, SIOCGIFDSTADDR,
			    (char *)&ifrdstaddr) < 0) {
				if (errno == EADDRNOTAVAIL) {
					dstaddr = NULL;
					dstaddr_size = 0;
				} else {
					(void)snprintf(errbuf, PCAP_ERRBUF_SIZE,
					    "SIOCGIFDSTADDR: %.*s: %s",
					    (int)sizeof(ifrdstaddr.ifr_name),
					    ifrdstaddr.ifr_name,
					    pcap_strerror(errno));
					ret = -1;
					break;
				}
			} else {
				dstaddr = &ifrdstaddr.ifr_dstaddr;
				dstaddr_size = SA_LEN(dstaddr);
			}
		} else {
			dstaddr = NULL;
			dstaddr_size = 0;
		}

#if defined (HAVE_SOLARIS) || defined (HAVE_HPUX10_20_OR_LATER)
		p = strchr(ifrp->ifr_name, ':');
		if (p != NULL) {
			q = p + 1;
			while (isdigit((unsigned char)*q))
				q++;
			if (*q == '\0') {
				*p = '\0';
			}
		}
#endif

		if (add_addr_to_iflist(&devlist, ifrp->ifr_name,
		    ifrflags.ifr_flags, &ifrp->ifr_addr,
		    SA_LEN(&ifrp->ifr_addr), netmask, netmask_size,
		    broadaddr, broadaddr_size, dstaddr, dstaddr_size,
		    errbuf) < 0) {
			ret = -1;
			break;
		}
	}
	free(buf);
	(void)close(fd);

	if (ret == -1) {
		if (devlist != NULL) {
			pcap_freealldevs(devlist);
			devlist = NULL;
		}
	}

	*alldevsp = devlist;
	return (ret);
}
