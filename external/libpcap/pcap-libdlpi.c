/*
 * Copyright (c) 1993, 1994, 1995, 1996, 1997
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
 * This code contributed by Sagun Shakya (sagun.shakya@sun.com)
 */

#ifndef lint
static const char rcsid[] _U_ =
	"@(#) $Header: /tcpdump/master/libpcap/pcap-libdlpi.c,v 1.6 2008-04-14 20:40:58 guy Exp $ (LBL)";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/time.h>
#include <sys/bufmod.h>
#include <sys/stream.h>
#include <libdlpi.h>
#include <errno.h>
#include <memory.h>
#include <stropts.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pcap-int.h"
#include "dlpisubs.h"

static int dlpromiscon(pcap_t *, bpf_u_int32);
static int pcap_read_libdlpi(pcap_t *, int, pcap_handler, u_char *);
static int pcap_inject_libdlpi(pcap_t *, const void *, size_t);
static void pcap_libdlpi_err(const char *, const char *, int, char *);
static void pcap_cleanup_libdlpi(pcap_t *);

static boolean_t list_interfaces(const char *, void *);

typedef struct linknamelist {
	char	linkname[DLPI_LINKNAME_MAX];
	struct linknamelist *lnl_next;
} linknamelist_t;

typedef struct linkwalk {
	linknamelist_t	*lw_list;
	int		lw_err;
} linkwalk_t;

static boolean_t
list_interfaces(const char *linkname, void *arg)
{
	linkwalk_t	*lwp = arg;
	linknamelist_t	*entry;

	if ((entry = calloc(1, sizeof(linknamelist_t))) == NULL) {
		lwp->lw_err = ENOMEM;
		return (B_TRUE);
	}
	(void) strlcpy(entry->linkname, linkname, DLPI_LINKNAME_MAX);

	if (lwp->lw_list == NULL) {
		lwp->lw_list = entry;
	} else {
		entry->lnl_next = lwp->lw_list;
		lwp->lw_list = entry;
	}

	return (B_FALSE);
}

static int
pcap_activate_libdlpi(pcap_t *p)
{
	struct pcap_dlpi *pd = p->priv;
	int retv;
	dlpi_handle_t dh;
	dlpi_info_t dlinfo;
	int err = PCAP_ERROR;

	retv = dlpi_open(p->opt.source, &dh, DLPI_RAW|DLPI_PASSIVE);
	if (retv != DLPI_SUCCESS) {
		if (retv == DLPI_ELINKNAMEINVAL || retv == DLPI_ENOLINK)
			err = PCAP_ERROR_NO_SUCH_DEVICE;
		else if (retv == DL_SYSERR &&
		    (errno == EPERM || errno == EACCES))
			err = PCAP_ERROR_PERM_DENIED;
		pcap_libdlpi_err(p->opt.source, "dlpi_open", retv,
		    p->errbuf);
		return (err);
	}
	pd->dlpi_hd = dh;

	if (p->opt.rfmon) {
		err = PCAP_ERROR_RFMON_NOTSUP;
		goto bad;
	}

	
	if ((retv = dlpi_bind(pd->dlpi_hd, DLPI_ANY_SAP, 0)) != DLPI_SUCCESS) {
		pcap_libdlpi_err(p->opt.source, "dlpi_bind", retv, p->errbuf);
		goto bad;
	}

	
	if (p->opt.promisc) {
		err = dlpromiscon(p, DL_PROMISC_PHYS);
		if (err < 0) {
			if (err == PCAP_ERROR_PERM_DENIED)
				err = PCAP_ERROR_PROMISC_PERM_DENIED;
			goto bad;
		}
	} else {
		
		err = dlpromiscon(p, DL_PROMISC_MULTI);
		if (err < 0)
			goto bad;
	}

	
	err = dlpromiscon(p, DL_PROMISC_SAP);
	if (err < 0) {
		if (p->opt.promisc)
			err = PCAP_WARNING;
		else
			goto bad;
	}

	
	if ((retv = dlpi_info(pd->dlpi_hd, &dlinfo, 0)) != DLPI_SUCCESS) {
		pcap_libdlpi_err(p->opt.source, "dlpi_info", retv, p->errbuf);
		goto bad;
	}

	if (pcap_process_mactype(p, dlinfo.di_mactype) != 0)
		goto bad;

	p->fd = dlpi_fd(pd->dlpi_hd);

	
	if (pcap_conf_bufmod(p, p->snapshot) != 0)
		goto bad;

	if (ioctl(p->fd, I_FLUSH, FLUSHR) != 0) {
		snprintf(p->errbuf, PCAP_ERRBUF_SIZE, "FLUSHR: %s",
		    pcap_strerror(errno));
		goto bad;
	}

	
	if (pcap_alloc_databuf(p) != 0)
		goto bad;

	p->selectable_fd = p->fd;

	p->read_op = pcap_read_libdlpi;
	p->inject_op = pcap_inject_libdlpi;
	p->setfilter_op = install_bpf_program;	
	p->setdirection_op = NULL;	
	p->set_datalink_op = NULL;	
	p->getnonblock_op = pcap_getnonblock_fd;
	p->setnonblock_op = pcap_setnonblock_fd;
	p->stats_op = pcap_stats_dlpi;
	p->cleanup_op = pcap_cleanup_libdlpi;

	return (0);
bad:
	pcap_cleanup_libdlpi(p);
	return (err);
}

#define STRINGIFY(n)	#n

static int
dlpromiscon(pcap_t *p, bpf_u_int32 level)
{
	struct pcap_dlpi *pd = p->priv;
	int retv;
	int err;

	retv = dlpi_promiscon(pd->dlpi_hd, level);
	if (retv != DLPI_SUCCESS) {
		if (retv == DL_SYSERR &&
		    (errno == EPERM || errno == EACCES))
			err = PCAP_ERROR_PERM_DENIED;
		else
			err = PCAP_ERROR;
		pcap_libdlpi_err(p->opt.source, "dlpi_promiscon" STRINGIFY(level),
		    retv, p->errbuf);
		return (err);
	}
	return (0);
}

int
pcap_platform_finddevs(pcap_if_t **alldevsp, char *errbuf)
{
	int retv = 0;

	linknamelist_t	*entry, *next;
	linkwalk_t	lw = {NULL, 0};
	int 		save_errno;

	

	dlpi_walk(list_interfaces, &lw, 0);

	if (lw.lw_err != 0) {
		snprintf(errbuf, PCAP_ERRBUF_SIZE,
		    "dlpi_walk: %s", pcap_strerror(lw.lw_err));
		retv = -1;
		goto done;
	}

	
	for (entry = lw.lw_list; entry != NULL; entry = entry->lnl_next) {
		if (pcap_add_if(alldevsp, entry->linkname, 0, NULL, errbuf) < 0)
			retv = -1;
	}
done:
	save_errno = errno;
	for (entry = lw.lw_list; entry != NULL; entry = next) {
		next = entry->lnl_next;
		free(entry);
	}
	errno = save_errno;

	return (retv);
}

static int
pcap_read_libdlpi(pcap_t *p, int count, pcap_handler callback, u_char *user)
{
	struct pcap_dlpi *pd = p->priv;
	int len;
	u_char *bufp;
	size_t msglen;
	int retv;

	len = p->cc;
	if (len != 0) {
		bufp = p->bp;
		goto process_pkts;
	}
	do {
		
		if (p->break_loop) {
			p->break_loop = 0;
			return (-2);
		}

		msglen = p->bufsize;
		bufp = p->buffer + p->offset;

		retv = dlpi_recv(pd->dlpi_hd, NULL, NULL, bufp,
		    &msglen, -1, NULL);
		if (retv != DLPI_SUCCESS) {
			if (retv == DL_SYSERR && errno == EINTR) {
				len = 0;
				continue;
			}
			pcap_libdlpi_err(dlpi_linkname(pd->dlpi_hd),
			    "dlpi_recv", retv, p->errbuf);
			return (-1);
		}
		len = msglen;
	} while (len == 0);

process_pkts:
	return (pcap_process_pkts(p, callback, user, count, bufp, len));
}

static int
pcap_inject_libdlpi(pcap_t *p, const void *buf, size_t size)
{
	struct pcap_dlpi *pd = p->priv;
	int retv;

	retv = dlpi_send(pd->dlpi_hd, NULL, 0, buf, size, NULL);
	if (retv != DLPI_SUCCESS) {
		pcap_libdlpi_err(dlpi_linkname(pd->dlpi_hd), "dlpi_send", retv,
		    p->errbuf);
		return (-1);
	}
	return (size);
}

static void
pcap_cleanup_libdlpi(pcap_t *p)
{
	struct pcap_dlpi *pd = p->priv;

	if (pd->dlpi_hd != NULL) {
		dlpi_close(pd->dlpi_hd);
		pd->dlpi_hd = NULL;
		p->fd = -1;
	}
	pcap_cleanup_live_common(p);
}

static void
pcap_libdlpi_err(const char *linkname, const char *func, int err, char *errbuf)
{
	snprintf(errbuf, PCAP_ERRBUF_SIZE, "libpcap: %s failed on %s: %s",
	    func, linkname, dlpi_strerror(err));
}

pcap_t *
pcap_create_interface(const char *device, char *ebuf)
{
	pcap_t *p;

	p = pcap_create_common(device, ebuf, sizeof (struct pcap_dlpi));
	if (p == NULL)
		return (NULL);

	p->activate_op = pcap_activate_libdlpi;
	return (p);
}
