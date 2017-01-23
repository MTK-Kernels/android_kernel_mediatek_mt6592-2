/*
 * Copyright (c) 1990, 1991, 1992, 1993, 1994, 1995, 1996
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * packet filter subroutines for tcpdump
 *	Extraction/creation by Jeffrey Mogul, DECWRL
 */

#ifndef lint
static const char rcsid[] _U_ =
    "@(#) $Header: /tcpdump/master/libpcap/pcap-pf.c,v 1.97 2008-04-14 20:40:58 guy Exp $ (LBL)";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/time.h>
#include <sys/timeb.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <net/pfilt.h>

struct mbuf;
struct rtentry;
#include <net/if.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#include <netinet/ip_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <netinet/tcp.h>
#include <netinet/tcpip.h>

#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PCAP_DONT_INCLUDE_PCAP_BPF_H
#include <net/bpf.h>

#include "pcap-int.h"

#ifdef HAVE_OS_PROTO_H
#include "os-proto.h"
#endif

#define       PCAP_FDDIPAD 3

struct pcap_pf {
	int	filtering_in_kernel; 
	u_long	TotPkts;	
	u_long	TotAccepted;	
	u_long	TotDrops;	
	long	TotMissed;	
	long	OrigMissed;	
};

static int pcap_setfilter_pf(pcap_t *, struct bpf_program *);

#define BUFSPACE (200 * 256)

static int
pcap_read_pf(pcap_t *pc, int cnt, pcap_handler callback, u_char *user)
{
	struct pcap_pf *pf = pc->priv;
	register u_char *p, *bp;
	register int cc, n, buflen, inc;
	register struct enstamp *sp;
#ifdef LBL_ALIGN
	struct enstamp stamp;
#endif
	register int pad;

 again:
	cc = pc->cc;
	if (cc == 0) {
		cc = read(pc->fd, (char *)pc->buffer + pc->offset, pc->bufsize);
		if (cc < 0) {
			if (errno == EWOULDBLOCK)
				return (0);
			if (errno == EINVAL &&
			    lseek(pc->fd, 0L, SEEK_CUR) + pc->bufsize < 0) {
				(void)lseek(pc->fd, 0L, SEEK_SET);
				goto again;
			}
			snprintf(pc->errbuf, sizeof(pc->errbuf), "pf read: %s",
				pcap_strerror(errno));
			return (-1);
		}
		bp = pc->buffer + pc->offset;
	} else
		bp = pc->bp;
	n = 0;
	pad = pc->fddipad;
	while (cc > 0) {
		if (pc->break_loop) {
			if (n == 0) {
				pc->break_loop = 0;
				return (-2);
			} else {
				pc->cc = cc;
				pc->bp = bp;
				return (n);
			}
		}
		if (cc < sizeof(*sp)) {
			snprintf(pc->errbuf, sizeof(pc->errbuf),
			    "pf short read (%d)", cc);
			return (-1);
		}
#ifdef LBL_ALIGN
		if ((long)bp & 3) {
			sp = &stamp;
			memcpy((char *)sp, (char *)bp, sizeof(*sp));
		} else
#endif
			sp = (struct enstamp *)bp;
		if (sp->ens_stamplen != sizeof(*sp)) {
			snprintf(pc->errbuf, sizeof(pc->errbuf),
			    "pf short stamplen (%d)",
			    sp->ens_stamplen);
			return (-1);
		}

		p = bp + sp->ens_stamplen;
		buflen = sp->ens_count;
		if (buflen > pc->snapshot)
			buflen = pc->snapshot;

		
		inc = ENALIGN(buflen + sp->ens_stamplen);
		cc -= inc;
		bp += inc;
		pf->TotPkts++;
		pf->TotDrops += sp->ens_dropped;
		pf->TotMissed = sp->ens_ifoverflows;
		if (pf->OrigMissed < 0)
			pf->OrigMissed = pf->TotMissed;

		if (pf->filtering_in_kernel ||
		    bpf_filter(pc->fcode.bf_insns, p, sp->ens_count, buflen)) {
			struct pcap_pkthdr h;
			pf->TotAccepted++;
			h.ts = sp->ens_tstamp;
			h.len = sp->ens_count - pad;
			p += pad;
			buflen -= pad;
			h.caplen = buflen;
			(*callback)(user, &h, p);
			if (++n >= cnt && !PACKET_COUNT_IS_UNLIMITED(cnt)) {
				pc->cc = cc;
				pc->bp = bp;
				return (n);
			}
		}
	}
	pc->cc = 0;
	return (n);
}

static int
pcap_inject_pf(pcap_t *p, const void *buf, size_t size)
{
	int ret;

	ret = write(p->fd, buf, size);
	if (ret == -1) {
		snprintf(p->errbuf, PCAP_ERRBUF_SIZE, "send: %s",
		    pcap_strerror(errno));
		return (-1);
	}
	return (ret);
}                           

static int
pcap_stats_pf(pcap_t *p, struct pcap_stat *ps)
{
	struct pcap_pf *pf = p->priv;

	ps->ps_recv = pf->TotAccepted;
	ps->ps_drop = pf->TotDrops;
	ps->ps_ifdrop = pf->TotMissed - pf->OrigMissed;
	return (0);
}

#ifndef DLT_DOCSIS
#define DLT_DOCSIS	143
#endif

static int
pcap_activate_pf(pcap_t *p)
{
	struct pcap_pf *pf = p->priv;
	short enmode;
	int backlog = -1;	
	struct enfilter Filter;
	struct endevp devparams;

	p->fd = pfopen(p->opt.source, O_RDWR);
	if (p->fd == -1 && errno == EACCES)
		p->fd = pfopen(p->opt.source, O_RDONLY);
	if (p->fd < 0) {
		snprintf(p->errbuf, PCAP_ERRBUF_SIZE, "pf open: %s: %s\n\
your system may not be properly configured; see the packetfilter(4) man page\n",
			p->opt.source, pcap_strerror(errno));
		goto bad;
	}
	pf->OrigMissed = -1;
	enmode = ENTSTAMP|ENNONEXCL;
	if (!p->opt.immediate)
		enmode |= ENBATCH;
	if (p->opt.promisc)
		enmode |= ENPROMISC;
	if (ioctl(p->fd, EIOCMBIS, (caddr_t)&enmode) < 0) {
		snprintf(p->errbuf, PCAP_ERRBUF_SIZE, "EIOCMBIS: %s",
		    pcap_strerror(errno));
		goto bad;
	}
#ifdef	ENCOPYALL
	
	enmode = ENCOPYALL;
	(void)ioctl(p->fd, EIOCMBIS, (caddr_t)&enmode);
#endif
	
	if (ioctl(p->fd, EIOCSETW, (caddr_t)&backlog) < 0) {
		snprintf(p->errbuf, PCAP_ERRBUF_SIZE, "EIOCSETW: %s",
		    pcap_strerror(errno));
		goto bad;
	}
	
	if (ioctl(p->fd, EIOCDEVP, (caddr_t)&devparams) < 0) {
		snprintf(p->errbuf, PCAP_ERRBUF_SIZE, "EIOCDEVP: %s",
		    pcap_strerror(errno));
		goto bad;
	}
	
#ifndef	ENDT_FDDI
#define	ENDT_FDDI	4
#endif
	switch (devparams.end_dev_type) {

	case ENDT_10MB:
		p->linktype = DLT_EN10MB;
		p->offset = 2;
		p->dlt_list = (u_int *) malloc(sizeof(u_int) * 2);
		if (p->dlt_list != NULL) {
			p->dlt_list[0] = DLT_EN10MB;
			p->dlt_list[1] = DLT_DOCSIS;
			p->dlt_count = 2;
		}
		break;

	case ENDT_FDDI:
		p->linktype = DLT_FDDI;
		break;

#ifdef ENDT_SLIP
	case ENDT_SLIP:
		p->linktype = DLT_SLIP;
		break;
#endif

#ifdef ENDT_PPP
	case ENDT_PPP:
		p->linktype = DLT_PPP;
		break;
#endif

#ifdef ENDT_LOOPBACK
	case ENDT_LOOPBACK:
		p->linktype = DLT_EN10MB;
		p->offset = 2;
		break;
#endif

#ifdef ENDT_TRN
	case ENDT_TRN:
		p->linktype = DLT_IEEE802;
		break;
#endif

	default:
		snprintf(p->errbuf, PCAP_ERRBUF_SIZE,
		    "unknown data-link type %u", devparams.end_dev_type);
		goto bad;
	}
	
	if (p->linktype == DLT_FDDI) {
		p->fddipad = PCAP_FDDIPAD;

		
		p->snapshot += PCAP_FDDIPAD;
	} else
		p->fddipad = 0;
	if (ioctl(p->fd, EIOCTRUNCATE, (caddr_t)&p->snapshot) < 0) {
		snprintf(p->errbuf, PCAP_ERRBUF_SIZE, "EIOCTRUNCATE: %s",
		    pcap_strerror(errno));
		goto bad;
	}
	
	memset(&Filter, 0, sizeof(Filter));
	Filter.enf_Priority = 37;	
	Filter.enf_FilterLen = 0;	
	if (ioctl(p->fd, EIOCSETF, (caddr_t)&Filter) < 0) {
		snprintf(p->errbuf, PCAP_ERRBUF_SIZE, "EIOCSETF: %s",
		    pcap_strerror(errno));
		goto bad;
	}

	if (p->opt.timeout != 0) {
		struct timeval timeout;
		timeout.tv_sec = p->opt.timeout / 1000;
		timeout.tv_usec = (p->opt.timeout * 1000) % 1000000;
		if (ioctl(p->fd, EIOCSRTIMEOUT, (caddr_t)&timeout) < 0) {
			snprintf(p->errbuf, PCAP_ERRBUF_SIZE, "EIOCSRTIMEOUT: %s",
				pcap_strerror(errno));
			goto bad;
		}
	}

	p->bufsize = BUFSPACE;
	p->buffer = (u_char*)malloc(p->bufsize + p->offset);
	if (p->buffer == NULL) {
		strlcpy(p->errbuf, pcap_strerror(errno), PCAP_ERRBUF_SIZE);
		goto bad;
	}

	p->selectable_fd = p->fd;

	p->read_op = pcap_read_pf;
	p->inject_op = pcap_inject_pf;
	p->setfilter_op = pcap_setfilter_pf;
	p->setdirection_op = NULL;	
	p->set_datalink_op = NULL;	
	p->getnonblock_op = pcap_getnonblock_fd;
	p->setnonblock_op = pcap_setnonblock_fd;
	p->stats_op = pcap_stats_pf;

	return (0);
 bad:
	pcap_cleanup_live_common(p);
	return (PCAP_ERROR);
}

pcap_t *
pcap_create_interface(const char *device, char *ebuf)
{
	pcap_t *p;

	p = pcap_create_common(device, ebuf, sizeof (struct pcap_pf));
	if (p == NULL)
		return (NULL);

	p->activate_op = pcap_activate_pf;
	return (p);
}

int
pcap_platform_finddevs(pcap_if_t **alldevsp, char *errbuf)
{
	return (0);
}

static int
pcap_setfilter_pf(pcap_t *p, struct bpf_program *fp)
{
	struct pcap_pf *pf = p->priv;
	struct bpf_version bv;

	if (ioctl(p->fd, BIOCVERSION, (caddr_t)&bv) >= 0) {
		if (bv.bv_major == BPF_MAJOR_VERSION &&
		    bv.bv_minor >= BPF_MINOR_VERSION) {
			if (ioctl(p->fd, BIOCSETF, (caddr_t)fp) < 0) {
				snprintf(p->errbuf, sizeof(p->errbuf),
				    "BIOCSETF: %s", pcap_strerror(errno));
				return (-1);
			}

			fprintf(stderr, "tcpdump: Using kernel BPF filter\n");
			pf->filtering_in_kernel = 1;

			p->cc = 0;
			return (0);
		}

		fprintf(stderr,
	    "tcpdump: Requires BPF language %d.%d or higher; kernel is %d.%d\n",
		    BPF_MAJOR_VERSION, BPF_MINOR_VERSION,
		    bv.bv_major, bv.bv_minor);
	}

	if (install_bpf_program(p, fp) < 0)
		return (-1);

	fprintf(stderr, "tcpdump: Filtering in user process\n");
	pf->filtering_in_kernel = 0;
	return (0);
}
