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
    "@(#) $Header: /tcpdump/master/libpcap/fad-glifc.c,v 1.7 2008-01-30 09:35:48 guy Exp $ (LBL)";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/param.h>
#include <sys/file.h>
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

int
pcap_findalldevs_interfaces(pcap_if_t **alldevsp, char *errbuf)
{
	pcap_if_t *devlist = NULL;
	register int fd4, fd6, fd;
	register struct lifreq *ifrp, *ifend;
	struct lifnum ifn;
	struct lifconf ifc;
	char *buf = NULL;
	unsigned buf_size;
#ifdef HAVE_SOLARIS
	char *p, *q;
#endif
	struct lifreq ifrflags, ifrnetmask, ifrbroadaddr, ifrdstaddr;
	struct sockaddr *netmask, *broadaddr, *dstaddr;
	int ret = 0;

	fd4 = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd4 < 0) {
		(void)snprintf(errbuf, PCAP_ERRBUF_SIZE,
		    "socket: %s", pcap_strerror(errno));
		return (-1);
	}

	fd6 = socket(AF_INET6, SOCK_DGRAM, 0);
	if (fd6 < 0) {
		(void)snprintf(errbuf, PCAP_ERRBUF_SIZE,
		    "socket: %s", pcap_strerror(errno));
		(void)close(fd4);
		return (-1);
	}

	ifn.lifn_family = AF_UNSPEC;
	ifn.lifn_flags = 0;
	ifn.lifn_count = 0;
	if (ioctl(fd4, SIOCGLIFNUM, (char *)&ifn) < 0) {
		(void)snprintf(errbuf, PCAP_ERRBUF_SIZE,
		    "SIOCGLIFNUM: %s", pcap_strerror(errno));
		(void)close(fd6);
		(void)close(fd4);
		return (-1);
	}

	buf_size = ifn.lifn_count * sizeof (struct lifreq);
	buf = malloc(buf_size);
	if (buf == NULL) {
		(void)snprintf(errbuf, PCAP_ERRBUF_SIZE,
		    "malloc: %s", pcap_strerror(errno));
		(void)close(fd6);
		(void)close(fd4);
		return (-1);
	}

	ifc.lifc_len = buf_size;
	ifc.lifc_buf = buf;
	ifc.lifc_family = AF_UNSPEC;
	ifc.lifc_flags = 0;
	memset(buf, 0, buf_size);
	if (ioctl(fd4, SIOCGLIFCONF, (char *)&ifc) < 0) {
		(void)snprintf(errbuf, PCAP_ERRBUF_SIZE,
		    "SIOCGLIFCONF: %s", pcap_strerror(errno));
		(void)close(fd6);
		(void)close(fd4);
		free(buf);
		return (-1);
	}

	ifrp = (struct lifreq *)buf;
	ifend = (struct lifreq *)(buf + ifc.lifc_len);

	for (; ifrp < ifend; ifrp++) {
		if (((struct sockaddr *)&ifrp->lifr_addr)->sa_family == AF_INET6)
			fd = fd6;
		else
			fd = fd4;

		if (strncmp(ifrp->lifr_name, "dummy", 5) == 0)
			continue;

#ifdef HAVE_SOLARIS
		p = strchr(ifrp->lifr_name, ':');
		if (p != NULL) {
			while (isdigit((unsigned char)*p))
				p++;
			if (*p == '\0') {
				continue;
			}
		}
#endif

		strncpy(ifrflags.lifr_name, ifrp->lifr_name,
		    sizeof(ifrflags.lifr_name));
		if (ioctl(fd, SIOCGLIFFLAGS, (char *)&ifrflags) < 0) {
			if (errno == ENXIO)
				continue;
			(void)snprintf(errbuf, PCAP_ERRBUF_SIZE,
			    "SIOCGLIFFLAGS: %.*s: %s",
			    (int)sizeof(ifrflags.lifr_name),
			    ifrflags.lifr_name,
			    pcap_strerror(errno));
			ret = -1;
			break;
		}
		if (!(ifrflags.lifr_flags & IFF_UP))
			continue;

		strncpy(ifrnetmask.lifr_name, ifrp->lifr_name,
		    sizeof(ifrnetmask.lifr_name));
		memcpy(&ifrnetmask.lifr_addr, &ifrp->lifr_addr,
		    sizeof(ifrnetmask.lifr_addr));
		if (ioctl(fd, SIOCGLIFNETMASK, (char *)&ifrnetmask) < 0) {
			if (errno == EADDRNOTAVAIL) {
				netmask = NULL;
			} else {
				(void)snprintf(errbuf, PCAP_ERRBUF_SIZE,
				    "SIOCGLIFNETMASK: %.*s: %s",
				    (int)sizeof(ifrnetmask.lifr_name),
				    ifrnetmask.lifr_name,
				    pcap_strerror(errno));
				ret = -1;
				break;
			}
		} else
			netmask = (struct sockaddr *)&ifrnetmask.lifr_addr;

		if (ifrflags.lifr_flags & IFF_BROADCAST) {
			strncpy(ifrbroadaddr.lifr_name, ifrp->lifr_name,
			    sizeof(ifrbroadaddr.lifr_name));
			memcpy(&ifrbroadaddr.lifr_addr, &ifrp->lifr_addr,
			    sizeof(ifrbroadaddr.lifr_addr));
			if (ioctl(fd, SIOCGLIFBRDADDR,
			    (char *)&ifrbroadaddr) < 0) {
				if (errno == EADDRNOTAVAIL) {
					broadaddr = NULL;
				} else {
					(void)snprintf(errbuf, PCAP_ERRBUF_SIZE,
					    "SIOCGLIFBRDADDR: %.*s: %s",
					    (int)sizeof(ifrbroadaddr.lifr_name),
					    ifrbroadaddr.lifr_name,
					    pcap_strerror(errno));
					ret = -1;
					break;
				}
			} else
				broadaddr = (struct sockaddr *)&ifrbroadaddr.lifr_broadaddr;
		} else {
			broadaddr = NULL;
		}

		if (ifrflags.lifr_flags & IFF_POINTOPOINT) {
			strncpy(ifrdstaddr.lifr_name, ifrp->lifr_name,
			    sizeof(ifrdstaddr.lifr_name));
			memcpy(&ifrdstaddr.lifr_addr, &ifrp->lifr_addr,
			    sizeof(ifrdstaddr.lifr_addr));
			if (ioctl(fd, SIOCGLIFDSTADDR,
			    (char *)&ifrdstaddr) < 0) {
				if (errno == EADDRNOTAVAIL) {
					dstaddr = NULL;
				} else {
					(void)snprintf(errbuf, PCAP_ERRBUF_SIZE,
					    "SIOCGLIFDSTADDR: %.*s: %s",
					    (int)sizeof(ifrdstaddr.lifr_name),
					    ifrdstaddr.lifr_name,
					    pcap_strerror(errno));
					ret = -1;
					break;
				}
			} else
				dstaddr = (struct sockaddr *)&ifrdstaddr.lifr_dstaddr;
		} else
			dstaddr = NULL;

#ifdef HAVE_SOLARIS
		p = strchr(ifrp->lifr_name, ':');
		if (p != NULL) {
			q = p + 1;
			while (isdigit((unsigned char)*q))
				q++;
			if (*q == '\0') {
				*p = '\0';
			}
		}
#endif

		if (add_addr_to_iflist(&devlist, ifrp->lifr_name,
		    ifrflags.lifr_flags, (struct sockaddr *)&ifrp->lifr_addr,
		    sizeof (struct sockaddr_storage),
		    netmask, sizeof (struct sockaddr_storage),
		    broadaddr, sizeof (struct sockaddr_storage),
		    dstaddr, sizeof (struct sockaddr_storage), errbuf) < 0) {
			ret = -1;
			break;
		}
	}
	free(buf);
	(void)close(fd6);
	(void)close(fd4);

	if (ret == -1) {
		if (devlist != NULL) {
			pcap_freealldevs(devlist);
			devlist = NULL;
		}
	}

	*alldevsp = devlist;
	return (ret);
}
