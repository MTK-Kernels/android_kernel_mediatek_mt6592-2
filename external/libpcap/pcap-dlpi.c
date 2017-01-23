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
 * This code contributed by Atanu Ghosh (atanu@cs.ucl.ac.uk),
 * University College London, and subsequently modified by
 * Guy Harris (guy@alum.mit.edu), Mark Pizzolato
 * <List-tcpdump-workers@subscriptions.pizzolato.net>,
 * Mark C. Brown (mbrown@hp.com), and Sagun Shakya <Sagun.Shakya@Sun.COM>.
 */


#ifndef lint
static const char rcsid[] _U_ =
    "@(#) $Header: /tcpdump/master/libpcap/pcap-dlpi.c,v 1.128 2008-12-02 16:20:23 guy Exp $ (LBL)";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/time.h>
#ifdef HAVE_SYS_BUFMOD_H
#include <sys/bufmod.h>
#endif
#include <sys/dlpi.h>
#ifdef HAVE_SYS_DLPI_EXT_H
#include <sys/dlpi_ext.h>
#endif
#ifdef HAVE_HPUX9
#include <sys/socket.h>
#endif
#ifdef DL_HP_PPA_REQ
#include <sys/stat.h>
#endif
#include <sys/stream.h>
#if defined(HAVE_SOLARIS) && defined(HAVE_SYS_BUFMOD_H)
#include <sys/systeminfo.h>
#endif

#ifdef HAVE_HPUX9
#include <net/if.h>
#endif

#include <ctype.h>
#ifdef HAVE_HPUX9
#include <nlist.h>
#endif
#include <errno.h>
#include <fcntl.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stropts.h>
#include <unistd.h>

#ifdef HAVE_LIMITS_H
#include <limits.h>
#else
#define INT_MAX		2147483647
#endif

#include "pcap-int.h"
#include "dlpisubs.h"

#ifdef HAVE_OS_PROTO_H
#include "os-proto.h"
#endif

#ifndef PCAP_DEV_PREFIX
#ifdef _AIX
#define PCAP_DEV_PREFIX "/dev/dlpi"
#else
#define PCAP_DEV_PREFIX "/dev"
#endif
#endif

#define	MAXDLBUF	8192

static char *split_dname(char *, int *, char *);
static int dl_doattach(int, int, char *);
#ifdef DL_HP_RAWDLS
static int dl_dohpuxbind(int, char *);
#endif
static int dlpromiscon(pcap_t *, bpf_u_int32);
static int dlbindreq(int, bpf_u_int32, char *);
static int dlbindack(int, char *, char *, int *);
static int dlokack(int, const char *, char *, char *);
static int dlinforeq(int, char *);
static int dlinfoack(int, char *, char *);

#ifdef HAVE_DLPI_PASSIVE
static void dlpassive(int, char *);
#endif

#ifdef DL_HP_RAWDLS
static int dlrawdatareq(int, const u_char *, int);
#endif
static int recv_ack(int, int, const char *, char *, char *, int *);
static char *dlstrerror(bpf_u_int32);
static char *dlprim(bpf_u_int32);
#if defined(HAVE_SOLARIS) && defined(HAVE_SYS_BUFMOD_H)
static char *get_release(bpf_u_int32 *, bpf_u_int32 *, bpf_u_int32 *);
#endif
static int send_request(int, char *, int, char *, char *);
#ifdef HAVE_HPUX9
static int dlpi_kread(int, off_t, void *, u_int, char *);
#endif
#ifdef HAVE_DEV_DLPI
static int get_dlpi_ppa(int, const char *, int, char *);
#endif

static bpf_u_int32 ctlbuf[MAXDLBUF];
static struct strbuf ctl = {
	MAXDLBUF,
	0,
	(char *)ctlbuf
};

#define MAKE_DL_PRIMITIVES(ptr)	((union DL_primitives *)(void *)(ptr))

static int
pcap_read_dlpi(pcap_t *p, int cnt, pcap_handler callback, u_char *user)
{
	int cc;
	u_char *bp;
	int flags;
	struct strbuf data;

	flags = 0;
	cc = p->cc;
	if (cc == 0) {
		data.buf = (char *)p->buffer + p->offset;
		data.maxlen = p->bufsize;
		data.len = 0;
		do {
			if (p->break_loop) {
				p->break_loop = 0;
				return (-2);
			}
			if (getmsg(p->fd, &ctl, &data, &flags) < 0) {
				
				switch (errno) {

				case EINTR:
					cc = 0;
					continue;

				case EAGAIN:
					return (0);
				}
				strlcpy(p->errbuf, pcap_strerror(errno),
				    sizeof(p->errbuf));
				return (-1);
			}
			cc = data.len;
		} while (cc == 0);
		bp = p->buffer + p->offset;
	} else
		bp = p->bp;

	return (pcap_process_pkts(p, callback, user, cnt, bp, cc));
}

static int
pcap_inject_dlpi(pcap_t *p, const void *buf, size_t size)
{
#ifdef DL_HP_RAWDLS
	struct pcap_dlpi *pd = p->priv;
#endif
	int ret;

#if defined(DLIOCRAW)
	ret = write(p->fd, buf, size);
	if (ret == -1) {
		snprintf(p->errbuf, PCAP_ERRBUF_SIZE, "send: %s",
		    pcap_strerror(errno));
		return (-1);
	}
#elif defined(DL_HP_RAWDLS)
	if (pd->send_fd < 0) {
		snprintf(p->errbuf, PCAP_ERRBUF_SIZE,
		    "send: Output FD couldn't be opened");
		return (-1);
	}
	ret = dlrawdatareq(pd->send_fd, buf, size);
	if (ret == -1) {
		snprintf(p->errbuf, PCAP_ERRBUF_SIZE, "send: %s",
		    pcap_strerror(errno));
		return (-1);
	}
	/*
	 * putmsg() returns either 0 or -1; it doesn't indicate how
	 * many bytes were written (presumably they were all written
	 * or none of them were written).  OpenBSD's pcap_inject()
	 * returns the number of bytes written, so, for API compatibility,
	 * we return the number of bytes we were told to write.
	 */
	ret = size;
#else 
	strlcpy(p->errbuf, "send: Not supported on this version of this OS",
	    PCAP_ERRBUF_SIZE);
	ret = -1;
#endif 
	return (ret);
}   

#ifndef DL_IPATM
#define DL_IPATM	0x12	
#endif

#ifdef HAVE_SOLARIS
#ifndef A_GET_UNITS
#define A_GET_UNITS	(('A'<<8)|118)
#endif 
#ifndef A_PROMISCON_REQ
#define A_PROMISCON_REQ	(('A'<<8)|121)
#endif 
#endif 

static void
pcap_cleanup_dlpi(pcap_t *p)
{
#ifdef DL_HP_RAWDLS
	struct pcap_dlpi *pd = p->priv;

	if (pd->send_fd >= 0) {
		close(pd->send_fd);
		pd->send_fd = -1;
	}
#endif
	pcap_cleanup_live_common(p);
}

static int
pcap_activate_dlpi(pcap_t *p)
{
#ifdef DL_HP_RAWDLS
	struct pcap_dlpi *pd = p->priv;
#endif
	register char *cp;
	int ppa;
#ifdef HAVE_SOLARIS
	int isatm = 0;
#endif
	register dl_info_ack_t *infop;
#ifdef HAVE_SYS_BUFMOD_H
	bpf_u_int32 ss;
#ifdef HAVE_SOLARIS
	register char *release;
	bpf_u_int32 osmajor, osminor, osmicro;
#endif
#endif
	bpf_u_int32 buf[MAXDLBUF];
	char dname[100];
#ifndef HAVE_DEV_DLPI
	char dname2[100];
#endif
	int status = PCAP_ERROR;

#ifdef HAVE_DEV_DLPI
	cp = strrchr(p->opt.source, '/');
	if (cp == NULL)
		strlcpy(dname, p->opt.source, sizeof(dname));
	else
		strlcpy(dname, cp + 1, sizeof(dname));

	cp = split_dname(dname, &ppa, p->errbuf);
	if (cp == NULL) {
		status = PCAP_ERROR_NO_SUCH_DEVICE;
		goto bad;
	}
	*cp = '\0';

	cp = "/dev/dlpi";
	if ((p->fd = open(cp, O_RDWR)) < 0) {
		if (errno == EPERM || errno == EACCES)
			status = PCAP_ERROR_PERM_DENIED;
		snprintf(p->errbuf, PCAP_ERRBUF_SIZE,
		    "%s: %s", cp, pcap_strerror(errno));
		goto bad;
	}

#ifdef DL_HP_RAWDLS
	pd->send_fd = open(cp, O_RDWR);
#endif

	ppa = get_dlpi_ppa(p->fd, dname, ppa, p->errbuf);
	if (ppa < 0) {
		status = ppa;
		goto bad;
	}
#else
	if (*p->opt.source == '/')
		strlcpy(dname, p->opt.source, sizeof(dname));
	else
		snprintf(dname, sizeof(dname), "%s/%s", PCAP_DEV_PREFIX,
		    p->opt.source);

	cp = split_dname(dname, &ppa, p->errbuf);
	if (cp == NULL) {
		status = PCAP_ERROR_NO_SUCH_DEVICE;
		goto bad;
	}

	strlcpy(dname2, dname, sizeof(dname));
	*cp = '\0';

	
	if ((p->fd = open(dname, O_RDWR)) < 0) {
		if (errno != ENOENT) {
			if (errno == EPERM || errno == EACCES)
				status = PCAP_ERROR_PERM_DENIED;
			snprintf(p->errbuf, PCAP_ERRBUF_SIZE, "%s: %s", dname,
			    pcap_strerror(errno));
			goto bad;
		}

		
		if ((p->fd = open(dname2, O_RDWR)) < 0) {
			if (errno == ENOENT) {
				status = PCAP_ERROR_NO_SUCH_DEVICE;

				snprintf(p->errbuf, PCAP_ERRBUF_SIZE,
				    "%s: No DLPI device found", p->opt.source);
			} else {
				if (errno == EPERM || errno == EACCES)
					status = PCAP_ERROR_PERM_DENIED;
				snprintf(p->errbuf, PCAP_ERRBUF_SIZE, "%s: %s",
				    dname2, pcap_strerror(errno));
			}
			goto bad;
		}
		
		ppa = 0;
	}
#endif

	if (dlinforeq(p->fd, p->errbuf) < 0 ||
	    dlinfoack(p->fd, (char *)buf, p->errbuf) < 0)
		goto bad;
	infop = &(MAKE_DL_PRIMITIVES(buf))->info_ack;
#ifdef HAVE_SOLARIS
	if (infop->dl_mac_type == DL_IPATM)
		isatm = 1;
#endif
	if (infop->dl_provider_style == DL_STYLE2) {
		status = dl_doattach(p->fd, ppa, p->errbuf);
		if (status < 0)
			goto bad;
#ifdef DL_HP_RAWDLS
		if (pd->send_fd >= 0) {
			if (dl_doattach(pd->send_fd, ppa, p->errbuf) < 0)
				goto bad;
		}
#endif
	}

	if (p->opt.rfmon) {
		status = PCAP_ERROR_RFMON_NOTSUP;
		goto bad;
	}

#ifdef HAVE_DLPI_PASSIVE
	dlpassive(p->fd, p->errbuf);
#endif
#if !defined(HAVE_HPUX9) && !defined(HAVE_HPUX10_20_OR_LATER) && !defined(sinix)
#ifdef _AIX
	if ((dlbindreq(p->fd, 1537, p->errbuf) < 0 &&
	     dlbindreq(p->fd, 2, p->errbuf) < 0) ||
	     dlbindack(p->fd, (char *)buf, p->errbuf, NULL) < 0)
		goto bad;
#elif defined(DL_HP_RAWDLS)
	if (dl_dohpuxbind(p->fd, p->errbuf) < 0)
		goto bad;
	if (pd->send_fd >= 0) {
		if (dl_dohpuxbind(pd->send_fd, p->errbuf) < 0)
			goto bad;
	}
#else 
	if (dlbindreq(p->fd, 0, p->errbuf) < 0 ||
	    dlbindack(p->fd, (char *)buf, p->errbuf, NULL) < 0)
		goto bad;
#endif 
#endif 

#ifdef HAVE_SOLARIS
	if (isatm) {
		if (strioctl(p->fd, A_PROMISCON_REQ, 0, NULL) < 0) {
			snprintf(p->errbuf, PCAP_ERRBUF_SIZE,
			    "A_PROMISCON_REQ: %s", pcap_strerror(errno));
			goto bad;
		}
	} else
#endif
	if (p->opt.promisc) {
		status = dlpromiscon(p, DL_PROMISC_PHYS);
		if (status < 0) {
			if (status == PCAP_ERROR_PERM_DENIED)
				status = PCAP_ERROR_PROMISC_PERM_DENIED;
			goto bad;
		}

#if !defined(__hpux) && !defined(sinix)
		status = dlpromiscon(p, DL_PROMISC_MULTI);
		if (status < 0)
			status = PCAP_WARNING;
#endif
	}
#ifndef sinix
#if defined(__hpux)
	
	if (!p->opt.promisc) {
#elif defined(HAVE_SOLARIS)
	
	if (!isatm) {
#else
	
	{
#endif
		status = dlpromiscon(p, DL_PROMISC_SAP);
		if (status < 0) {
			if (p->opt.promisc)
				status = PCAP_WARNING;
			else
				goto bad;
		}
	}
#endif 

#if defined(HAVE_HPUX9) || defined(HAVE_HPUX10_20_OR_LATER)
	if (dl_dohpuxbind(p->fd, p->errbuf) < 0)
		goto bad;
	if (pd->send_fd >= 0) {
		if (dl_dohpuxbind(pd->send_fd, p->errbuf) < 0)
			goto bad;
	}
#endif

	if (dlinforeq(p->fd, p->errbuf) < 0 ||
	    dlinfoack(p->fd, (char *)buf, p->errbuf) < 0)
		goto bad;

	infop = &(MAKE_DL_PRIMITIVES(buf))->info_ack;
	if (pcap_process_mactype(p, infop->dl_mac_type) != 0)
		goto bad;

#ifdef	DLIOCRAW
	if (strioctl(p->fd, DLIOCRAW, 0, NULL) < 0) {
		snprintf(p->errbuf, PCAP_ERRBUF_SIZE, "DLIOCRAW: %s",
		    pcap_strerror(errno));
		goto bad;
	}
#endif

#ifdef HAVE_SYS_BUFMOD_H
	ss = p->snapshot;

#ifdef HAVE_SOLARIS
	release = get_release(&osmajor, &osminor, &osmicro);
	if (osmajor == 5 && (osminor <= 2 || (osminor == 3 && osmicro < 2)) &&
	    getenv("BUFMOD_FIXED") == NULL) {
		snprintf(p->errbuf, PCAP_ERRBUF_SIZE,
		"WARNING: bufmod is broken in SunOS %s; ignoring snaplen.",
		    release);
		ss = 0;
		status = PCAP_WARNING;
	}
#endif

	
	if (pcap_conf_bufmod(p, ss) != 0)
		goto bad;
#endif

	if (ioctl(p->fd, I_FLUSH, FLUSHR) != 0) {
		snprintf(p->errbuf, PCAP_ERRBUF_SIZE, "FLUSHR: %s",
		    pcap_strerror(errno));
		goto bad;
	}

	
	if (pcap_alloc_databuf(p) != 0)
		goto bad;

	
	if (status < 0)
		status = 0;

	p->selectable_fd = p->fd;

	p->read_op = pcap_read_dlpi;
	p->inject_op = pcap_inject_dlpi;
	p->setfilter_op = install_bpf_program;	
	p->setdirection_op = NULL;	
	p->set_datalink_op = NULL;	
	p->getnonblock_op = pcap_getnonblock_fd;
	p->setnonblock_op = pcap_setnonblock_fd;
	p->stats_op = pcap_stats_dlpi;
	p->cleanup_op = pcap_cleanup_dlpi;

	return (status);
bad:
	pcap_cleanup_dlpi(p);
	return (status);
}

static char *
split_dname(char *device, int *unitp, char *ebuf)
{
	char *cp;
	char *eos;
	long unit;

	cp = device + strlen(device) - 1;
	if (*cp < '0' || *cp > '9') {
		snprintf(ebuf, PCAP_ERRBUF_SIZE, "%s missing unit number",
		    device);
		return (NULL);
	}

	
	while (cp-1 >= device && *(cp-1) >= '0' && *(cp-1) <= '9')
		cp--;

	errno = 0;
	unit = strtol(cp, &eos, 10);
	if (*eos != '\0') {
		snprintf(ebuf, PCAP_ERRBUF_SIZE, "%s bad unit number", device);
		return (NULL);
	}
	if (errno == ERANGE || unit > INT_MAX) {
		snprintf(ebuf, PCAP_ERRBUF_SIZE, "%s unit number too large",
		    device);
		return (NULL);
	}
	if (unit < 0) {
		snprintf(ebuf, PCAP_ERRBUF_SIZE, "%s unit number is negative",
		    device);
		return (NULL);
	}
	*unitp = (int)unit;
	return (cp);
}

static int
dl_doattach(int fd, int ppa, char *ebuf)
{
	dl_attach_req_t	req;
	bpf_u_int32 buf[MAXDLBUF];
	int err;

	req.dl_primitive = DL_ATTACH_REQ;
	req.dl_ppa = ppa;
	if (send_request(fd, (char *)&req, sizeof(req), "attach", ebuf) < 0)
		return (PCAP_ERROR);

	err = dlokack(fd, "attach", (char *)buf, ebuf);
	if (err < 0)
		return (err);
	return (0);
}

#ifdef DL_HP_RAWDLS
static int
dl_dohpuxbind(int fd, char *ebuf)
{
	int hpsap;
	int uerror;
	bpf_u_int32 buf[MAXDLBUF];

	hpsap = 22;
	for (;;) {
		if (dlbindreq(fd, hpsap, ebuf) < 0)
			return (-1);
		if (dlbindack(fd, (char *)buf, ebuf, &uerror) >= 0)
			break;
		if (uerror != EBUSY) {
			return (-1);
		}

		*ebuf = '\0';
		hpsap++;
		if (hpsap > 100) {
			strlcpy(ebuf,
			    "All SAPs from 22 through 100 are in use",
			    PCAP_ERRBUF_SIZE);
			return (-1);
		}
	}
	return (0);
}
#endif

#define STRINGIFY(n)	#n

static int
dlpromiscon(pcap_t *p, bpf_u_int32 level)
{
	dl_promiscon_req_t req;
	bpf_u_int32 buf[MAXDLBUF];
	int err;

	req.dl_primitive = DL_PROMISCON_REQ;
	req.dl_level = level;
	if (send_request(p->fd, (char *)&req, sizeof(req), "promiscon",
	    p->errbuf) < 0)
		return (PCAP_ERROR);
	err = dlokack(p->fd, "promiscon" STRINGIFY(level), (char *)buf,
	    p->errbuf);
	if (err < 0)
		return (err);
	return (0);
}

int
pcap_platform_finddevs(pcap_if_t **alldevsp, char *errbuf)
{
#ifdef HAVE_SOLARIS
	int fd;
	union {
		u_int nunits;
		char pad[516];	
	} buf;
	char baname[2+1+1];
	u_int i;

	if ((fd = open("/dev/ba", O_RDWR)) < 0) {
		return (0);
	}

	if (strioctl(fd, A_GET_UNITS, sizeof(buf), (char *)&buf) < 0) {
		snprintf(errbuf, PCAP_ERRBUF_SIZE, "A_GET_UNITS: %s",
		    pcap_strerror(errno));
		return (-1);
	}
	for (i = 0; i < buf.nunits; i++) {
		snprintf(baname, sizeof baname, "ba%u", i);
		if (pcap_add_if(alldevsp, baname, 0, NULL, errbuf) < 0)
			return (-1);
	}
#endif

	return (0);
}

static int
send_request(int fd, char *ptr, int len, char *what, char *ebuf)
{
	struct	strbuf	ctl;
	int	flags;

	ctl.maxlen = 0;
	ctl.len = len;
	ctl.buf = ptr;

	flags = 0;
	if (putmsg(fd, &ctl, (struct strbuf *) NULL, flags) < 0) {
		snprintf(ebuf, PCAP_ERRBUF_SIZE,
		    "send_request: putmsg \"%s\": %s",
		    what, pcap_strerror(errno));
		return (-1);
	}
	return (0);
}

static int
recv_ack(int fd, int size, const char *what, char *bufp, char *ebuf, int *uerror)
{
	union	DL_primitives	*dlp;
	struct	strbuf	ctl;
	int	flags;

	if (uerror != NULL)
		*uerror = 0;

	ctl.maxlen = MAXDLBUF;
	ctl.len = 0;
	ctl.buf = bufp;

	flags = 0;
	if (getmsg(fd, &ctl, (struct strbuf*)NULL, &flags) < 0) {
		snprintf(ebuf, PCAP_ERRBUF_SIZE, "recv_ack: %s getmsg: %s",
		    what, pcap_strerror(errno));
		return (PCAP_ERROR);
	}

	dlp = MAKE_DL_PRIMITIVES(ctl.buf);
	switch (dlp->dl_primitive) {

	case DL_INFO_ACK:
	case DL_BIND_ACK:
	case DL_OK_ACK:
#ifdef DL_HP_PPA_ACK
	case DL_HP_PPA_ACK:
#endif
		
		break;

	case DL_ERROR_ACK:
		switch (dlp->error_ack.dl_errno) {

		case DL_SYSERR:
			if (uerror != NULL)
				*uerror = dlp->error_ack.dl_unix_errno;
			snprintf(ebuf, PCAP_ERRBUF_SIZE,
			    "recv_ack: %s: UNIX error - %s",
			    what, pcap_strerror(dlp->error_ack.dl_unix_errno));
			if (dlp->error_ack.dl_unix_errno == EPERM ||
			    dlp->error_ack.dl_unix_errno == EACCES)
				return (PCAP_ERROR_PERM_DENIED);
			break;

		default:
			snprintf(ebuf, PCAP_ERRBUF_SIZE, "recv_ack: %s: %s",
			    what, dlstrerror(dlp->error_ack.dl_errno));
			if (dlp->error_ack.dl_errno == DL_BADPPA)
				return (PCAP_ERROR_NO_SUCH_DEVICE);
			else if (dlp->error_ack.dl_errno == DL_ACCESS)
				return (PCAP_ERROR_PERM_DENIED);
			break;
		}
		return (PCAP_ERROR);

	default:
		snprintf(ebuf, PCAP_ERRBUF_SIZE,
		    "recv_ack: %s: Unexpected primitive ack %s",
		    what, dlprim(dlp->dl_primitive));
		return (PCAP_ERROR);
	}

	if (ctl.len < size) {
		snprintf(ebuf, PCAP_ERRBUF_SIZE,
		    "recv_ack: %s: Ack too small (%d < %d)",
		    what, ctl.len, size);
		return (PCAP_ERROR);
	}
	return (ctl.len);
}

static char *
dlstrerror(bpf_u_int32 dl_errno)
{
	static char errstring[6+2+8+1];

	switch (dl_errno) {

	case DL_ACCESS:
		return ("Improper permissions for request");

	case DL_BADADDR:
		return ("DLSAP addr in improper format or invalid");

	case DL_BADCORR:
		return ("Seq number not from outstand DL_CONN_IND");

	case DL_BADDATA:
		return ("User data exceeded provider limit");

	case DL_BADPPA:
#ifdef HAVE_DEV_DLPI
		return ("Specified PPA was invalid");
#else
		return ("Specified PPA (device unit) was invalid");
#endif

	case DL_BADPRIM:
		return ("Primitive received not known by provider");

	case DL_BADQOSPARAM:
		return ("QOS parameters contained invalid values");

	case DL_BADQOSTYPE:
		return ("QOS structure type is unknown/unsupported");

	case DL_BADSAP:
		return ("Bad LSAP selector");

	case DL_BADTOKEN:
		return ("Token used not an active stream");

	case DL_BOUND:
		return ("Attempted second bind with dl_max_conind");

	case DL_INITFAILED:
		return ("Physical link initialization failed");

	case DL_NOADDR:
		return ("Provider couldn't allocate alternate address");

	case DL_NOTINIT:
		return ("Physical link not initialized");

	case DL_OUTSTATE:
		return ("Primitive issued in improper state");

	case DL_SYSERR:
		return ("UNIX system error occurred");

	case DL_UNSUPPORTED:
		return ("Requested service not supplied by provider");

	case DL_UNDELIVERABLE:
		return ("Previous data unit could not be delivered");

	case DL_NOTSUPPORTED:
		return ("Primitive is known but not supported");

	case DL_TOOMANY:
		return ("Limit exceeded");

	case DL_NOTENAB:
		return ("Promiscuous mode not enabled");

	case DL_BUSY:
		return ("Other streams for PPA in post-attached");

	case DL_NOAUTO:
		return ("Automatic handling XID&TEST not supported");

	case DL_NOXIDAUTO:
		return ("Automatic handling of XID not supported");

	case DL_NOTESTAUTO:
		return ("Automatic handling of TEST not supported");

	case DL_XIDAUTO:
		return ("Automatic handling of XID response");

	case DL_TESTAUTO:
		return ("Automatic handling of TEST response");

	case DL_PENDING:
		return ("Pending outstanding connect indications");

	default:
		sprintf(errstring, "Error %02x", dl_errno);
		return (errstring);
	}
}

static char *
dlprim(bpf_u_int32 prim)
{
	static char primbuf[80];

	switch (prim) {

	case DL_INFO_REQ:
		return ("DL_INFO_REQ");

	case DL_INFO_ACK:
		return ("DL_INFO_ACK");

	case DL_ATTACH_REQ:
		return ("DL_ATTACH_REQ");

	case DL_DETACH_REQ:
		return ("DL_DETACH_REQ");

	case DL_BIND_REQ:
		return ("DL_BIND_REQ");

	case DL_BIND_ACK:
		return ("DL_BIND_ACK");

	case DL_UNBIND_REQ:
		return ("DL_UNBIND_REQ");

	case DL_OK_ACK:
		return ("DL_OK_ACK");

	case DL_ERROR_ACK:
		return ("DL_ERROR_ACK");

	case DL_SUBS_BIND_REQ:
		return ("DL_SUBS_BIND_REQ");

	case DL_SUBS_BIND_ACK:
		return ("DL_SUBS_BIND_ACK");

	case DL_UNITDATA_REQ:
		return ("DL_UNITDATA_REQ");

	case DL_UNITDATA_IND:
		return ("DL_UNITDATA_IND");

	case DL_UDERROR_IND:
		return ("DL_UDERROR_IND");

	case DL_UDQOS_REQ:
		return ("DL_UDQOS_REQ");

	case DL_CONNECT_REQ:
		return ("DL_CONNECT_REQ");

	case DL_CONNECT_IND:
		return ("DL_CONNECT_IND");

	case DL_CONNECT_RES:
		return ("DL_CONNECT_RES");

	case DL_CONNECT_CON:
		return ("DL_CONNECT_CON");

	case DL_TOKEN_REQ:
		return ("DL_TOKEN_REQ");

	case DL_TOKEN_ACK:
		return ("DL_TOKEN_ACK");

	case DL_DISCONNECT_REQ:
		return ("DL_DISCONNECT_REQ");

	case DL_DISCONNECT_IND:
		return ("DL_DISCONNECT_IND");

	case DL_RESET_REQ:
		return ("DL_RESET_REQ");

	case DL_RESET_IND:
		return ("DL_RESET_IND");

	case DL_RESET_RES:
		return ("DL_RESET_RES");

	case DL_RESET_CON:
		return ("DL_RESET_CON");

	default:
		(void) sprintf(primbuf, "unknown primitive 0x%x", prim);
		return (primbuf);
	}
}

static int
dlbindreq(int fd, bpf_u_int32 sap, char *ebuf)
{

	dl_bind_req_t	req;

	memset((char *)&req, 0, sizeof(req));
	req.dl_primitive = DL_BIND_REQ;
	
#if defined(DL_HP_RAWDLS)
	req.dl_max_conind = 1;			
	req.dl_service_mode = DL_HP_RAWDLS;
#elif defined(DL_CLDLS)
	req.dl_service_mode = DL_CLDLS;
#endif
	req.dl_sap = sap;

	return (send_request(fd, (char *)&req, sizeof(req), "bind", ebuf));
}

static int
dlbindack(int fd, char *bufp, char *ebuf, int *uerror)
{

	return (recv_ack(fd, DL_BIND_ACK_SIZE, "bind", bufp, ebuf, uerror));
}

static int
dlokack(int fd, const char *what, char *bufp, char *ebuf)
{

	return (recv_ack(fd, DL_OK_ACK_SIZE, what, bufp, ebuf, NULL));
}


static int
dlinforeq(int fd, char *ebuf)
{
	dl_info_req_t req;

	req.dl_primitive = DL_INFO_REQ;

	return (send_request(fd, (char *)&req, sizeof(req), "info", ebuf));
}

static int
dlinfoack(int fd, char *bufp, char *ebuf)
{

	return (recv_ack(fd, DL_INFO_ACK_SIZE, "info", bufp, ebuf, NULL));
}

#ifdef HAVE_DLPI_PASSIVE
static void
dlpassive(int fd, char *ebuf)
{
	dl_passive_req_t req;
	bpf_u_int32 buf[MAXDLBUF];

	req.dl_primitive = DL_PASSIVE_REQ;

	if (send_request(fd, (char *)&req, sizeof(req), "dlpassive", ebuf) == 0)
	    (void) dlokack(fd, "dlpassive", (char *)buf, ebuf);
}
#endif

#ifdef DL_HP_RAWDLS
static int
dlrawdatareq(int fd, const u_char *datap, int datalen)
{
	struct strbuf ctl, data;
	long buf[MAXDLBUF];	
	union DL_primitives *dlp;
	int dlen;

	dlp = MAKE_DL_PRIMITIVES(buf);

	dlp->dl_primitive = DL_HP_RAWDATA_REQ;
	dlen = DL_HP_RAWDATA_REQ_SIZE;

	ctl.maxlen = 0;
	ctl.len = dlen;
	ctl.buf = (void *)buf;

	data.maxlen = 0;
	data.len = datalen;
	data.buf = (void *)datap;

	return (putmsg(fd, &ctl, &data, 0));
}
#endif 

#if defined(HAVE_SOLARIS) && defined(HAVE_SYS_BUFMOD_H)
static char *
get_release(bpf_u_int32 *majorp, bpf_u_int32 *minorp, bpf_u_int32 *microp)
{
	char *cp;
	static char buf[32];

	*majorp = 0;
	*minorp = 0;
	*microp = 0;
	if (sysinfo(SI_RELEASE, buf, sizeof(buf)) < 0)
		return ("?");
	cp = buf;
	if (!isdigit((unsigned char)*cp))
		return (buf);
	*majorp = strtol(cp, &cp, 10);
	if (*cp++ != '.')
		return (buf);
	*minorp =  strtol(cp, &cp, 10);
	if (*cp++ != '.')
		return (buf);
	*microp =  strtol(cp, &cp, 10);
	return (buf);
}
#endif

#ifdef DL_HP_PPA_REQ


static int
get_dlpi_ppa(register int fd, register const char *device, register int unit,
    register char *ebuf)
{
	register dl_hp_ppa_ack_t *ap;
	register dl_hp_ppa_info_t *ipstart, *ip;
	register int i;
	char dname[100];
	register u_long majdev;
	struct stat statbuf;
	dl_hp_ppa_req_t	req;
	char buf[MAXDLBUF];
	char *ppa_data_buf;
	dl_hp_ppa_ack_t	*dlp;
	struct strbuf ctl;
	int flags;
	int ppa;

	memset((char *)&req, 0, sizeof(req));
	req.dl_primitive = DL_HP_PPA_REQ;

	memset((char *)buf, 0, sizeof(buf));
	if (send_request(fd, (char *)&req, sizeof(req), "hpppa", ebuf) < 0)
		return (PCAP_ERROR);

	ctl.maxlen = DL_HP_PPA_ACK_SIZE;
	ctl.len = 0;
	ctl.buf = (char *)buf;

	flags = 0;
	
	if (getmsg(fd, &ctl, (struct strbuf *)NULL, &flags) < 0) {
		snprintf(ebuf, PCAP_ERRBUF_SIZE,
		    "get_dlpi_ppa: hpppa getmsg: %s", pcap_strerror(errno));
		return (PCAP_ERROR);
	}

	dlp = (dl_hp_ppa_ack_t *)ctl.buf;
	if (dlp->dl_primitive != DL_HP_PPA_ACK) {
		snprintf(ebuf, PCAP_ERRBUF_SIZE,
		    "get_dlpi_ppa: hpppa unexpected primitive ack 0x%x",
		    (bpf_u_int32)dlp->dl_primitive);
		return (PCAP_ERROR);
	}

	if (ctl.len < DL_HP_PPA_ACK_SIZE) {
		snprintf(ebuf, PCAP_ERRBUF_SIZE,
		    "get_dlpi_ppa: hpppa ack too small (%d < %lu)",
		     ctl.len, (unsigned long)DL_HP_PPA_ACK_SIZE);
		return (PCAP_ERROR);
	}

	
	if ((ppa_data_buf = (char *)malloc(dlp->dl_length)) == NULL) {
		snprintf(ebuf, PCAP_ERRBUF_SIZE,
		    "get_dlpi_ppa: hpppa malloc: %s", pcap_strerror(errno));
		return (PCAP_ERROR);
	}
	ctl.maxlen = dlp->dl_length;
	ctl.len = 0;
	ctl.buf = (char *)ppa_data_buf;
	
	if (getmsg(fd, &ctl, (struct strbuf *)NULL, &flags) < 0) {
		snprintf(ebuf, PCAP_ERRBUF_SIZE,
		    "get_dlpi_ppa: hpppa getmsg: %s", pcap_strerror(errno));
		free(ppa_data_buf);
		return (PCAP_ERROR);
	}
	if (ctl.len < dlp->dl_length) {
		snprintf(ebuf, PCAP_ERRBUF_SIZE,
		    "get_dlpi_ppa: hpppa ack too small (%d < %lu)",
		    ctl.len, (unsigned long)dlp->dl_length);
		free(ppa_data_buf);
		return (PCAP_ERROR);
	}

	ap = (dl_hp_ppa_ack_t *)buf;
	ipstart = (dl_hp_ppa_info_t *)ppa_data_buf;
	ip = ipstart;

#ifdef HAVE_HP_PPA_INFO_T_DL_MODULE_ID_1
	for (i = 0; i < ap->dl_count; i++) {
		if ((strcmp((const char *)ip->dl_module_id_1, device) == 0 ||
		     strcmp((const char *)ip->dl_module_id_2, device) == 0) &&
		    ip->dl_instance_num == unit)
			break;

		ip = (dl_hp_ppa_info_t *)((u_char *)ipstart + ip->dl_next_offset);
	}
#else
	i = ap->dl_count;
#endif

	if (i == ap->dl_count) {
		snprintf(dname, sizeof(dname), "/dev/%s%d", device, unit);
		if (stat(dname, &statbuf) < 0) {
			snprintf(ebuf, PCAP_ERRBUF_SIZE, "stat: %s: %s",
			    dname, pcap_strerror(errno));
			return (PCAP_ERROR);
		}
		majdev = major(statbuf.st_rdev);

		ip = ipstart;

		for (i = 0; i < ap->dl_count; i++) {
			if (ip->dl_mjr_num == majdev &&
			    ip->dl_instance_num == unit)
				break;

			ip = (dl_hp_ppa_info_t *)((u_char *)ipstart + ip->dl_next_offset);
		}
	}
	if (i == ap->dl_count) {
		snprintf(ebuf, PCAP_ERRBUF_SIZE,
		    "can't find /dev/dlpi PPA for %s%d", device, unit);
		return (PCAP_ERROR_NO_SUCH_DEVICE);
	}
	if (ip->dl_hdw_state == HDW_DEAD) {
		snprintf(ebuf, PCAP_ERRBUF_SIZE,
		    "%s%d: hardware state: DOWN\n", device, unit);
		free(ppa_data_buf);
		return (PCAP_ERROR);
	}
	ppa = ip->dl_ppa;
	free(ppa_data_buf);
	return (ppa);
}
#endif

#ifdef HAVE_HPUX9
static struct nlist nl[] = {
#define NL_IFNET 0
	{ "ifnet" },
	{ "" }
};

static char path_vmunix[] = "/hp-ux";

static int
get_dlpi_ppa(register int fd, register const char *ifname, register int unit,
    register char *ebuf)
{
	register const char *cp;
	register int kd;
	void *addr;
	struct ifnet ifnet;
	char if_name[sizeof(ifnet.if_name) + 1];

	cp = strrchr(ifname, '/');
	if (cp != NULL)
		ifname = cp + 1;
	if (nlist(path_vmunix, &nl) < 0) {
		snprintf(ebuf, PCAP_ERRBUF_SIZE, "nlist %s failed",
		    path_vmunix);
		return (-1);
	}
	if (nl[NL_IFNET].n_value == 0) {
		snprintf(ebuf, PCAP_ERRBUF_SIZE,
		    "could't find %s kernel symbol",
		    nl[NL_IFNET].n_name);
		return (-1);
	}
	kd = open("/dev/kmem", O_RDONLY);
	if (kd < 0) {
		snprintf(ebuf, PCAP_ERRBUF_SIZE, "kmem open: %s",
		    pcap_strerror(errno));
		return (-1);
	}
	if (dlpi_kread(kd, nl[NL_IFNET].n_value,
	    &addr, sizeof(addr), ebuf) < 0) {
		close(kd);
		return (-1);
	}
	for (; addr != NULL; addr = ifnet.if_next) {
		if (dlpi_kread(kd, (off_t)addr,
		    &ifnet, sizeof(ifnet), ebuf) < 0 ||
		    dlpi_kread(kd, (off_t)ifnet.if_name,
		    if_name, sizeof(ifnet.if_name), ebuf) < 0) {
			(void)close(kd);
			return (-1);
		}
		if_name[sizeof(ifnet.if_name)] = '\0';
		if (strcmp(if_name, ifname) == 0 && ifnet.if_unit == unit)
			return (ifnet.if_index);
	}

	snprintf(ebuf, PCAP_ERRBUF_SIZE, "Can't find %s", ifname);
	return (-1);
}

static int
dlpi_kread(register int fd, register off_t addr,
    register void *buf, register u_int len, register char *ebuf)
{
	register int cc;

	if (lseek(fd, addr, SEEK_SET) < 0) {
		snprintf(ebuf, PCAP_ERRBUF_SIZE, "lseek: %s",
		    pcap_strerror(errno));
		return (-1);
	}
	cc = read(fd, buf, len);
	if (cc < 0) {
		snprintf(ebuf, PCAP_ERRBUF_SIZE, "read: %s",
		    pcap_strerror(errno));
		return (-1);
	} else if (cc != len) {
		snprintf(ebuf, PCAP_ERRBUF_SIZE, "short read (%d != %d)", cc,
		    len);
		return (-1);
	}
	return (cc);
}
#endif

pcap_t *
pcap_create_interface(const char *device, char *ebuf)
{
	pcap_t *p;
#ifdef DL_HP_RAWDLS
	struct pcap_dlpi *pd;
#endif

	p = pcap_create_common(device, ebuf, sizeof (struct pcap_dlpi));
	if (p == NULL)
		return (NULL);

#ifdef DL_HP_RAWDLS
	pd = p->priv;
	pd->send_fd = -1;	
#endif

	p->activate_op = pcap_activate_dlpi;
	return (p);
}
