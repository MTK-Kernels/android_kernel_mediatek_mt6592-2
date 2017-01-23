/*
 * Copyright (c) 1993, 1994, 1995, 1996, 1998
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
 */
#ifndef lint
static const char rcsid[] _U_ =
    "@(#) $Header: /tcpdump/master/libpcap/pcap-bpf.c,v 1.116 2008-09-16 18:42:29 guy Exp $ (LBL)";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/param.h>			
#ifdef HAVE_ZEROCOPY_BPF
#include <sys/mman.h>
#endif
#include <sys/socket.h>
#include <time.h>
#include <sys/ioctl.h>
#ifdef HAVE_SYS_IOCCOM_H
#include <sys/ioccom.h>
#endif
#include <sys/utsname.h>

#ifdef HAVE_ZEROCOPY_BPF
#include <machine/atomic.h>
#endif

#include <net/if.h>

#ifdef _AIX

#define PCAP_DONT_INCLUDE_PCAP_BPF_H

#include <sys/types.h>

#undef _AIX
#include <net/bpf.h>
#define _AIX

#include <net/if_types.h>		
#include <sys/sysconfig.h>
#include <sys/device.h>
#include <sys/cfgodm.h>
#include <cf.h>

#ifdef __64BIT__
#define domakedev makedev64
#define getmajor major64
#define bpf_hdr bpf_hdr32
#else 
#define domakedev makedev
#define getmajor major
#endif 

#define BPF_NAME "bpf"
#define BPF_MINORS 4
#define DRIVER_PATH "/usr/lib/drivers"
#define BPF_NODE "/dev/bpf"
static int bpfloadedflag = 0;
static int odmlockid = 0;

static int bpf_load(char *errbuf);

#else 

#include <net/bpf.h>

#endif 

#include <ctype.h>
#include <fcntl.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef HAVE_NET_IF_MEDIA_H
# include <net/if_media.h>
#endif

#include "pcap-int.h"

#ifdef HAVE_OS_PROTO_H
#include "os-proto.h"
#endif

#if defined(__NetBSD__) && __NetBSD_Version__ > 106000000
#define       PCAP_FDDIPAD 3
#endif

struct pcap_bpf {
#ifdef PCAP_FDDIPAD
	int fddipad;
#endif

#ifdef HAVE_ZEROCOPY_BPF
	u_char *zbuf1, *zbuf2, *zbuffer;
	u_int zbufsize;
	u_int zerocopy;
	u_int interrupted;
	struct timespec firstsel;
	struct bpf_zbuf_header *bzh;
	int nonblock;		
#endif 

	char *device;		
	int filtering_in_kernel; 
	int must_do_on_close;	
};

#define MUST_CLEAR_RFMON	0x00000001	

#ifdef BIOCGDLTLIST
# if (defined(HAVE_NET_IF_MEDIA_H) && defined(IFM_IEEE80211)) && !defined(__APPLE__)
#define HAVE_BSD_IEEE80211
# endif

# if defined(__APPLE__) || defined(HAVE_BSD_IEEE80211)
static int find_802_11(struct bpf_dltlist *);

#  ifdef HAVE_BSD_IEEE80211
static int monitor_mode(pcap_t *, int);
#  endif

#  if defined(__APPLE__)
static void remove_en(pcap_t *);
static void remove_802_11(pcap_t *);
#  endif

# endif 

#endif 

#if defined(sun) && defined(LIFNAMSIZ) && defined(lifr_zoneid)
#include <zone.h>
#endif

#ifndef DLT_DOCSIS
#define DLT_DOCSIS	143
#endif

#ifndef DLT_PRISM_HEADER
#define DLT_PRISM_HEADER	119
#endif
#ifndef DLT_AIRONET_HEADER
#define DLT_AIRONET_HEADER	120
#endif
#ifndef DLT_IEEE802_11_RADIO
#define DLT_IEEE802_11_RADIO	127
#endif
#ifndef DLT_IEEE802_11_RADIO_AVS
#define DLT_IEEE802_11_RADIO_AVS 163
#endif

static int pcap_can_set_rfmon_bpf(pcap_t *p);
static int pcap_activate_bpf(pcap_t *p);
static int pcap_setfilter_bpf(pcap_t *p, struct bpf_program *fp);
static int pcap_setdirection_bpf(pcap_t *, pcap_direction_t);
static int pcap_set_datalink_bpf(pcap_t *p, int dlt);

static int
pcap_getnonblock_bpf(pcap_t *p, char *errbuf)
{ 
#ifdef HAVE_ZEROCOPY_BPF
	struct pcap_bpf *pb = p->priv;

	if (pb->zerocopy)
		return (pb->nonblock);
#endif
	return (pcap_getnonblock_fd(p, errbuf));
}

static int
pcap_setnonblock_bpf(pcap_t *p, int nonblock, char *errbuf)
{   
#ifdef HAVE_ZEROCOPY_BPF
	struct pcap_bpf *pb = p->priv;

	if (pb->zerocopy) {
		pb->nonblock = nonblock;
		return (0);
	}
#endif
	return (pcap_setnonblock_fd(p, nonblock, errbuf));
}

#ifdef HAVE_ZEROCOPY_BPF
static int
pcap_next_zbuf_shm(pcap_t *p, int *cc)
{
	struct pcap_bpf *pb = p->priv;
	struct bpf_zbuf_header *bzh;

	if (pb->zbuffer == pb->zbuf2 || pb->zbuffer == NULL) {
		bzh = (struct bpf_zbuf_header *)pb->zbuf1;
		if (bzh->bzh_user_gen !=
		    atomic_load_acq_int(&bzh->bzh_kernel_gen)) {
			pb->bzh = bzh;
			pb->zbuffer = (u_char *)pb->zbuf1;
			p->buffer = pb->zbuffer + sizeof(*bzh);
			*cc = bzh->bzh_kernel_len;
			return (1);
		}
	} else if (pb->zbuffer == pb->zbuf1) {
		bzh = (struct bpf_zbuf_header *)pb->zbuf2;
		if (bzh->bzh_user_gen !=
		    atomic_load_acq_int(&bzh->bzh_kernel_gen)) {
			pb->bzh = bzh;
			pb->zbuffer = (u_char *)pb->zbuf2;
  			p->buffer = pb->zbuffer + sizeof(*bzh);
			*cc = bzh->bzh_kernel_len;
			return (1);
		}
	}
	*cc = 0;
	return (0);
}

static int
pcap_next_zbuf(pcap_t *p, int *cc)
{
	struct pcap_bpf *pb = p->priv;
	struct bpf_zbuf bz;
	struct timeval tv;
	struct timespec cur;
	fd_set r_set;
	int data, r;
	int expire, tmout;

#define TSTOMILLI(ts) (((ts)->tv_sec * 1000) + ((ts)->tv_nsec / 1000000))
	data = pcap_next_zbuf_shm(p, cc);
	if (data)
		return (data);
	tmout = p->opt.timeout;
	if (tmout)
		(void) clock_gettime(CLOCK_MONOTONIC, &cur);
	if (pb->interrupted && p->opt.timeout) {
		expire = TSTOMILLI(&pb->firstsel) + p->opt.timeout;
		tmout = expire - TSTOMILLI(&cur);
#undef TSTOMILLI
		if (tmout <= 0) {
			pb->interrupted = 0;
			data = pcap_next_zbuf_shm(p, cc);
			if (data)
				return (data);
			if (ioctl(p->fd, BIOCROTZBUF, &bz) < 0) {
				(void) snprintf(p->errbuf, PCAP_ERRBUF_SIZE,
				    "BIOCROTZBUF: %s", strerror(errno));
				return (PCAP_ERROR);
			}
			return (pcap_next_zbuf_shm(p, cc));
		}
	}
	if (!pb->nonblock) {
		FD_ZERO(&r_set);
		FD_SET(p->fd, &r_set);
		if (tmout != 0) {
			tv.tv_sec = tmout / 1000;
			tv.tv_usec = (tmout * 1000) % 1000000;
		}
		r = select(p->fd + 1, &r_set, NULL, NULL,
		    p->opt.timeout != 0 ? &tv : NULL);
		if (r < 0 && errno == EINTR) {
			if (!pb->interrupted && p->opt.timeout) {
				pb->interrupted = 1;
				pb->firstsel = cur;
			}
			return (0);
		} else if (r < 0) {
			(void) snprintf(p->errbuf, PCAP_ERRBUF_SIZE,
			    "select: %s", strerror(errno));
			return (PCAP_ERROR);
		}
	}
	pb->interrupted = 0;
	data = pcap_next_zbuf_shm(p, cc);
	if (data)
		return (data);
	if (ioctl(p->fd, BIOCROTZBUF, &bz) < 0) {
		(void) snprintf(p->errbuf, PCAP_ERRBUF_SIZE,
		    "BIOCROTZBUF: %s", strerror(errno));
		return (PCAP_ERROR);
	}
	return (pcap_next_zbuf_shm(p, cc));
}

static int
pcap_ack_zbuf(pcap_t *p)
{
	struct pcap_bpf *pb = p->priv;

	atomic_store_rel_int(&pb->bzh->bzh_user_gen,
	    pb->bzh->bzh_kernel_gen);
	pb->bzh = NULL;
	p->buffer = NULL;
	return (0);
}
#endif 

pcap_t *
pcap_create_interface(const char *device, char *ebuf)
{
	pcap_t *p;

	p = pcap_create_common(device, ebuf, sizeof (struct pcap_bpf));
	if (p == NULL)
		return (NULL);

	p->activate_op = pcap_activate_bpf;
	p->can_set_rfmon_op = pcap_can_set_rfmon_bpf;
	return (p);
}

static int
bpf_open(pcap_t *p)
{
	int fd;
#ifdef HAVE_CLONING_BPF
	static const char device[] = "/dev/bpf";
#else
	int n = 0;
	char device[sizeof "/dev/bpf0000000000"];
#endif

#ifdef _AIX
	if (bpf_load(p->errbuf) == PCAP_ERROR)
		return (PCAP_ERROR);
#endif

#ifdef HAVE_CLONING_BPF
	if ((fd = open(device, O_RDWR)) == -1 &&
	    (errno != EACCES || (fd = open(device, O_RDONLY)) == -1)) {
		if (errno == EACCES)
			fd = PCAP_ERROR_PERM_DENIED;
		else
			fd = PCAP_ERROR;
		snprintf(p->errbuf, PCAP_ERRBUF_SIZE,
		  "(cannot open device) %s: %s", device, pcap_strerror(errno));
	}
#else
	do {
		(void)snprintf(device, sizeof(device), "/dev/bpf%d", n++);
		fd = open(device, O_RDWR);
		if (fd == -1 && errno == EACCES)
			fd = open(device, O_RDONLY);
	} while (fd < 0 && errno == EBUSY);

	if (fd < 0) {
		switch (errno) {

		case ENOENT:
			fd = PCAP_ERROR;
			if (n == 1) {
				snprintf(p->errbuf, PCAP_ERRBUF_SIZE,
				    "(there are no BPF devices)");
			} else {
				snprintf(p->errbuf, PCAP_ERRBUF_SIZE,
				    "(all BPF devices are busy)");
			}
			break;

		case EACCES:
			fd = PCAP_ERROR_PERM_DENIED;
			snprintf(p->errbuf, PCAP_ERRBUF_SIZE,
			    "(cannot open BPF device) %s: %s", device,
			    pcap_strerror(errno));
			break;

		default:
			fd = PCAP_ERROR;
			snprintf(p->errbuf, PCAP_ERRBUF_SIZE,
			    "(cannot open BPF device) %s: %s", device,
			    pcap_strerror(errno));
			break;
		}
	}
#endif

	return (fd);
}

#ifdef BIOCGDLTLIST
static int
get_dlt_list(int fd, int v, struct bpf_dltlist *bdlp, char *ebuf)
{
	memset(bdlp, 0, sizeof(*bdlp));
	if (ioctl(fd, BIOCGDLTLIST, (caddr_t)bdlp) == 0) {
		u_int i;
		int is_ethernet;

		bdlp->bfl_list = (u_int *) malloc(sizeof(u_int) * (bdlp->bfl_len + 1));
		if (bdlp->bfl_list == NULL) {
			(void)snprintf(ebuf, PCAP_ERRBUF_SIZE, "malloc: %s",
			    pcap_strerror(errno));
			return (PCAP_ERROR);
		}

		if (ioctl(fd, BIOCGDLTLIST, (caddr_t)bdlp) < 0) {
			(void)snprintf(ebuf, PCAP_ERRBUF_SIZE,
			    "BIOCGDLTLIST: %s", pcap_strerror(errno));
			free(bdlp->bfl_list);
			return (PCAP_ERROR);
		}

		if (v == DLT_EN10MB) {
			is_ethernet = 1;
			for (i = 0; i < bdlp->bfl_len; i++) {
				if (bdlp->bfl_list[i] != DLT_EN10MB
#ifdef DLT_IPNET
				    && bdlp->bfl_list[i] != DLT_IPNET
#endif
				    ) {
					is_ethernet = 0;
					break;
				}
			}
			if (is_ethernet) {
				bdlp->bfl_list[bdlp->bfl_len] = DLT_DOCSIS;
				bdlp->bfl_len++;
			}
		}
	} else {
		if (errno != EINVAL) {
			(void)snprintf(ebuf, PCAP_ERRBUF_SIZE,
			    "BIOCGDLTLIST: %s", pcap_strerror(errno));
			return (PCAP_ERROR);
		}
	}
	return (0);
}
#endif

static int
pcap_can_set_rfmon_bpf(pcap_t *p)
{
#if defined(__APPLE__)
	struct utsname osinfo;
	struct ifreq ifr;
	int fd;
#ifdef BIOCGDLTLIST
	struct bpf_dltlist bdl;
#endif

	if (uname(&osinfo) == -1) {
		return (0);
	}
	if (osinfo.release[0] < '8' && osinfo.release[1] == '.') {
		return (0);
	}
	if (osinfo.release[0] == '8' && osinfo.release[1] == '.') {
		if (strncmp(p->opt.source, "en", 2) != 0) {
			return (0);
		}
		fd = socket(AF_INET, SOCK_DGRAM, 0);
		if (fd == -1) {
			(void)snprintf(p->errbuf, PCAP_ERRBUF_SIZE,
			    "socket: %s", pcap_strerror(errno));
			return (PCAP_ERROR);
		}
		strlcpy(ifr.ifr_name, "wlt", sizeof(ifr.ifr_name));
		strlcat(ifr.ifr_name, p->opt.source + 2, sizeof(ifr.ifr_name));
		if (ioctl(fd, SIOCGIFFLAGS, (char *)&ifr) < 0) {
			close(fd);
			return (0);
		}
		close(fd);
		return (1);
	}

#ifdef BIOCGDLTLIST
	fd = bpf_open(p);
	if (fd < 0)
		return (fd);	

	(void)strncpy(ifr.ifr_name, p->opt.source, sizeof(ifr.ifr_name));
	if (ioctl(fd, BIOCSETIF, (caddr_t)&ifr) < 0) {
		switch (errno) {

		case ENXIO:
			close(fd);
			return (PCAP_ERROR_NO_SUCH_DEVICE);

		case ENETDOWN:
			close(fd);
			return (PCAP_ERROR_IFACE_NOT_UP);

		default:
			snprintf(p->errbuf, PCAP_ERRBUF_SIZE,
			    "BIOCSETIF: %s: %s",
			    p->opt.source, pcap_strerror(errno));
			close(fd);
			return (PCAP_ERROR);
		}
	}

	if (get_dlt_list(fd, DLT_NULL, &bdl, p->errbuf) == PCAP_ERROR) {
		close(fd);
		return (PCAP_ERROR);
	}
	if (find_802_11(&bdl) != -1) {
		free(bdl.bfl_list);
		close(fd);
		return (1);
	}
	free(bdl.bfl_list);
#endif 
	return (0);
#elif defined(HAVE_BSD_IEEE80211)
	int ret;

	ret = monitor_mode(p, 0);
	if (ret == PCAP_ERROR_RFMON_NOTSUP)
		return (0);	
	if (ret == 0)
		return (1);	
	return (ret);
#else
	return (0);
#endif
}

static int
pcap_stats_bpf(pcap_t *p, struct pcap_stat *ps)
{
	struct bpf_stat s;

	if (ioctl(p->fd, BIOCGSTATS, (caddr_t)&s) < 0) {
		snprintf(p->errbuf, PCAP_ERRBUF_SIZE, "BIOCGSTATS: %s",
		    pcap_strerror(errno));
		return (PCAP_ERROR);
	}

	ps->ps_recv = s.bs_recv;
	ps->ps_drop = s.bs_drop;
	ps->ps_ifdrop = 0;
	return (0);
}

static int
pcap_read_bpf(pcap_t *p, int cnt, pcap_handler callback, u_char *user)
{
	struct pcap_bpf *pb = p->priv;
	int cc;
	int n = 0;
	register u_char *bp, *ep;
	u_char *datap;
#ifdef PCAP_FDDIPAD
	register int pad;
#endif
#ifdef HAVE_ZEROCOPY_BPF
	int i;
#endif

 again:
	if (p->break_loop) {
		p->break_loop = 0;
		return (PCAP_ERROR_BREAK);
	}
	cc = p->cc;
	if (p->cc == 0) {
#ifdef HAVE_ZEROCOPY_BPF
		if (pb->zerocopy) {
			if (p->buffer != NULL)
				pcap_ack_zbuf(p);
			i = pcap_next_zbuf(p, &cc);
			if (i == 0)
				goto again;
			if (i < 0)
				return (PCAP_ERROR);
		} else
#endif
		{
			cc = read(p->fd, (char *)p->buffer, p->bufsize);
		}
		if (cc < 0) {
			
			switch (errno) {

			case EINTR:
				goto again;

#ifdef _AIX
			case EFAULT:
				goto again;
#endif

			case EWOULDBLOCK:
				return (0);

			case ENXIO:
				snprintf(p->errbuf, PCAP_ERRBUF_SIZE,
				    "The interface went down");
				return (PCAP_ERROR);

#if defined(sun) && !defined(BSD) && !defined(__svr4__) && !defined(__SVR4)
			case EINVAL:
				if (lseek(p->fd, 0L, SEEK_CUR) +
				    p->bufsize < 0) {
					(void)lseek(p->fd, 0L, SEEK_SET);
					goto again;
				}
				
#endif
			}
			snprintf(p->errbuf, PCAP_ERRBUF_SIZE, "read: %s",
			    pcap_strerror(errno));
			return (PCAP_ERROR);
		}
		bp = p->buffer;
	} else
		bp = p->bp;

#define bhp ((struct bpf_hdr *)bp)
	ep = bp + cc;
#ifdef PCAP_FDDIPAD
	pad = p->fddipad;
#endif
	while (bp < ep) {
		register int caplen, hdrlen;

		if (p->break_loop) {
			p->bp = bp;
			p->cc = ep - bp;
			if (p->cc < 0)
				p->cc = 0;
			if (n == 0) {
				p->break_loop = 0;
				return (PCAP_ERROR_BREAK);
			} else
				return (n);
		}

		caplen = bhp->bh_caplen;
		hdrlen = bhp->bh_hdrlen;
		datap = bp + hdrlen;
		if (pb->filtering_in_kernel ||
		    bpf_filter(p->fcode.bf_insns, datap, bhp->bh_datalen, caplen)) {
			struct pcap_pkthdr pkthdr;

			pkthdr.ts.tv_sec = bhp->bh_tstamp.tv_sec;
#ifdef _AIX
			pkthdr.ts.tv_usec = bhp->bh_tstamp.tv_usec/1000;
#else
			pkthdr.ts.tv_usec = bhp->bh_tstamp.tv_usec;
#endif
#ifdef PCAP_FDDIPAD
			if (caplen > pad)
				pkthdr.caplen = caplen - pad;
			else
				pkthdr.caplen = 0;
			if (bhp->bh_datalen > pad)
				pkthdr.len = bhp->bh_datalen - pad;
			else
				pkthdr.len = 0;
			datap += pad;
#else
			pkthdr.caplen = caplen;
			pkthdr.len = bhp->bh_datalen;
#endif
			(*callback)(user, &pkthdr, datap);
			bp += BPF_WORDALIGN(caplen + hdrlen);
			if (++n >= cnt && !PACKET_COUNT_IS_UNLIMITED(cnt)) {
				p->bp = bp;
				p->cc = ep - bp;
				if (p->cc < 0)
					p->cc = 0;
				return (n);
			}
		} else {
			bp += BPF_WORDALIGN(caplen + hdrlen);
		}
	}
#undef bhp
	p->cc = 0;
	return (n);
}

static int
pcap_inject_bpf(pcap_t *p, const void *buf, size_t size)
{
	int ret;

	ret = write(p->fd, buf, size);
#ifdef __APPLE__
	if (ret == -1 && errno == EAFNOSUPPORT) {
		u_int spoof_eth_src = 0;

		if (ioctl(p->fd, BIOCSHDRCMPLT, &spoof_eth_src) == -1) {
			(void)snprintf(p->errbuf, PCAP_ERRBUF_SIZE,
			    "send: can't turn off BIOCSHDRCMPLT: %s",
			    pcap_strerror(errno));
			return (PCAP_ERROR);
		}

		ret = write(p->fd, buf, size);
	}
#endif 
	if (ret == -1) {
		snprintf(p->errbuf, PCAP_ERRBUF_SIZE, "send: %s",
		    pcap_strerror(errno));
		return (PCAP_ERROR);
	}
	return (ret);
}

#ifdef _AIX
static int
bpf_odminit(char *errbuf)
{
	char *errstr;

	if (odm_initialize() == -1) {
		if (odm_err_msg(odmerrno, &errstr) == -1)
			errstr = "Unknown error";
		snprintf(errbuf, PCAP_ERRBUF_SIZE,
		    "bpf_load: odm_initialize failed: %s",
		    errstr);
		return (PCAP_ERROR);
	}

	if ((odmlockid = odm_lock("/etc/objrepos/config_lock", ODM_WAIT)) == -1) {
		if (odm_err_msg(odmerrno, &errstr) == -1)
			errstr = "Unknown error";
		snprintf(errbuf, PCAP_ERRBUF_SIZE,
		    "bpf_load: odm_lock of /etc/objrepos/config_lock failed: %s",
		    errstr);
		(void)odm_terminate();
		return (PCAP_ERROR);
	}

	return (0);
}

static int
bpf_odmcleanup(char *errbuf)
{
	char *errstr;

	if (odm_unlock(odmlockid) == -1) {
		if (errbuf != NULL) {
			if (odm_err_msg(odmerrno, &errstr) == -1)
				errstr = "Unknown error";
			snprintf(errbuf, PCAP_ERRBUF_SIZE,
			    "bpf_load: odm_unlock failed: %s",
			    errstr);
		}
		return (PCAP_ERROR);
	}

	if (odm_terminate() == -1) {
		if (errbuf != NULL) {
			if (odm_err_msg(odmerrno, &errstr) == -1)
				errstr = "Unknown error";
			snprintf(errbuf, PCAP_ERRBUF_SIZE,
			    "bpf_load: odm_terminate failed: %s",
			    errstr);
		}
		return (PCAP_ERROR);
	}

	return (0);
}

static int
bpf_load(char *errbuf)
{
	long major;
	int *minors;
	int numminors, i, rc;
	char buf[1024];
	struct stat sbuf;
	struct bpf_config cfg_bpf;
	struct cfg_load cfg_ld;
	struct cfg_kmod cfg_km;

	if (bpfloadedflag)
		return (0);

	if (bpf_odminit(errbuf) == PCAP_ERROR)
		return (PCAP_ERROR);

	major = genmajor(BPF_NAME);
	if (major == -1) {
		snprintf(errbuf, PCAP_ERRBUF_SIZE,
		    "bpf_load: genmajor failed: %s", pcap_strerror(errno));
		(void)bpf_odmcleanup(NULL);
		return (PCAP_ERROR);
	}

	minors = getminor(major, &numminors, BPF_NAME);
	if (!minors) {
		minors = genminor("bpf", major, 0, BPF_MINORS, 1, 1);
		if (!minors) {
			snprintf(errbuf, PCAP_ERRBUF_SIZE,
			    "bpf_load: genminor failed: %s",
			    pcap_strerror(errno));
			(void)bpf_odmcleanup(NULL);
			return (PCAP_ERROR);
		}
	}

	if (bpf_odmcleanup(errbuf) == PCAP_ERROR)
		return (PCAP_ERROR);

	rc = stat(BPF_NODE "0", &sbuf);
	if (rc == -1 && errno != ENOENT) {
		snprintf(errbuf, PCAP_ERRBUF_SIZE,
		    "bpf_load: can't stat %s: %s",
		    BPF_NODE "0", pcap_strerror(errno));
		return (PCAP_ERROR);
	}

	if (rc == -1 || getmajor(sbuf.st_rdev) != major) {
		for (i = 0; i < BPF_MINORS; i++) {
			sprintf(buf, "%s%d", BPF_NODE, i);
			unlink(buf);
			if (mknod(buf, S_IRUSR | S_IFCHR, domakedev(major, i)) == -1) {
				snprintf(errbuf, PCAP_ERRBUF_SIZE,
				    "bpf_load: can't mknod %s: %s",
				    buf, pcap_strerror(errno));
				return (PCAP_ERROR);
			}
		}
	}

	
	memset(&cfg_ld, 0x0, sizeof(cfg_ld));
	cfg_ld.path = buf;
	sprintf(cfg_ld.path, "%s/%s", DRIVER_PATH, BPF_NAME);
	if ((sysconfig(SYS_QUERYLOAD, (void *)&cfg_ld, sizeof(cfg_ld)) == -1) ||
	    (cfg_ld.kmid == 0)) {
		
		if (sysconfig(SYS_SINGLELOAD, (void *)&cfg_ld, sizeof(cfg_ld)) == -1) {
			snprintf(errbuf, PCAP_ERRBUF_SIZE,
			    "bpf_load: could not load driver: %s",
			    strerror(errno));
			return (PCAP_ERROR);
		}
	}

	
	cfg_km.cmd = CFG_INIT;
	cfg_km.kmid = cfg_ld.kmid;
	cfg_km.mdilen = sizeof(cfg_bpf);
	cfg_km.mdiptr = (void *)&cfg_bpf;
	for (i = 0; i < BPF_MINORS; i++) {
		cfg_bpf.devno = domakedev(major, i);
		if (sysconfig(SYS_CFGKMOD, (void *)&cfg_km, sizeof(cfg_km)) == -1) {
			snprintf(errbuf, PCAP_ERRBUF_SIZE,
			    "bpf_load: could not configure driver: %s",
			    strerror(errno));
			return (PCAP_ERROR);
		}
	}

	bpfloadedflag = 1;

	return (0);
}
#endif

static void
pcap_cleanup_bpf(pcap_t *p)
{
	struct pcap_bpf *pb = p->priv;
#ifdef HAVE_BSD_IEEE80211
	int sock;
	struct ifmediareq req;
	struct ifreq ifr;
#endif

	if (pb->must_do_on_close != 0) {
#ifdef HAVE_BSD_IEEE80211
		if (pb->must_do_on_close & MUST_CLEAR_RFMON) {
			sock = socket(AF_INET, SOCK_DGRAM, 0);
			if (sock == -1) {
				fprintf(stderr,
				    "Can't restore interface flags (socket() failed: %s).\n"
				    "Please adjust manually.\n",
				    strerror(errno));
			} else {
				memset(&req, 0, sizeof(req));
				strncpy(req.ifm_name, pb->device,
				    sizeof(req.ifm_name));
				if (ioctl(sock, SIOCGIFMEDIA, &req) < 0) {
					fprintf(stderr,
					    "Can't restore interface flags (SIOCGIFMEDIA failed: %s).\n"
					    "Please adjust manually.\n",
					    strerror(errno));
				} else {
					if (req.ifm_current & IFM_IEEE80211_MONITOR) {
						memset(&ifr, 0, sizeof(ifr));
						(void)strncpy(ifr.ifr_name,
						    pb->device,
						    sizeof(ifr.ifr_name));
						ifr.ifr_media =
						    req.ifm_current & ~IFM_IEEE80211_MONITOR;
						if (ioctl(sock, SIOCSIFMEDIA,
						    &ifr) == -1) {
							fprintf(stderr,
							    "Can't restore interface flags (SIOCSIFMEDIA failed: %s).\n"
							    "Please adjust manually.\n",
							    strerror(errno));
						}
					}
				}
				close(sock);
			}
		}
#endif 

		pcap_remove_from_pcaps_to_close(p);
		pb->must_do_on_close = 0;
	}

#ifdef HAVE_ZEROCOPY_BPF
	if (pb->zerocopy) {
		if (pb->zbuf1 != MAP_FAILED && pb->zbuf1 != NULL)
			(void) munmap(pb->zbuf1, pb->zbufsize);
		if (pb->zbuf2 != MAP_FAILED && pb->zbuf2 != NULL)
			(void) munmap(pb->zbuf2, pb->zbufsize);
		p->buffer = NULL;
	}
#endif
	if (pb->device != NULL) {
		free(pb->device);
		pb->device = NULL;
	}
	pcap_cleanup_live_common(p);
}

static int
check_setif_failure(pcap_t *p, int error)
{
#ifdef __APPLE__
	int fd;
	struct ifreq ifr;
	int err;
#endif

	if (error == ENXIO) {
#ifdef __APPLE__
		if (p->opt.rfmon && strncmp(p->opt.source, "wlt", 3) == 0) {
			fd = socket(AF_INET, SOCK_DGRAM, 0);
			if (fd != -1) {
				strlcpy(ifr.ifr_name, "en",
				    sizeof(ifr.ifr_name));
				strlcat(ifr.ifr_name, p->opt.source + 3,
				    sizeof(ifr.ifr_name));
				if (ioctl(fd, SIOCGIFFLAGS, (char *)&ifr) < 0) {
					err = PCAP_ERROR_NO_SUCH_DEVICE;
					snprintf(p->errbuf, PCAP_ERRBUF_SIZE,
					    "SIOCGIFFLAGS on %s failed: %s",
					    ifr.ifr_name, pcap_strerror(errno));
				} else {
					err = PCAP_ERROR_RFMON_NOTSUP;
				}
				close(fd);
			} else {
				err = PCAP_ERROR_NO_SUCH_DEVICE;
				snprintf(p->errbuf, PCAP_ERRBUF_SIZE,
				    "socket() failed: %s",
				    pcap_strerror(errno));
			}
			return (err);
		}
#endif
		snprintf(p->errbuf, PCAP_ERRBUF_SIZE, "BIOCSETIF failed: %s",
		    pcap_strerror(errno));
		return (PCAP_ERROR_NO_SUCH_DEVICE);
	} else if (errno == ENETDOWN) {
		return (PCAP_ERROR_IFACE_NOT_UP);
	} else {
		snprintf(p->errbuf, PCAP_ERRBUF_SIZE, "BIOCSETIF: %s: %s",
		    p->opt.source, pcap_strerror(errno));
		return (PCAP_ERROR);
	}
}

#ifdef _AIX
#define DEFAULT_BUFSIZE	32768
#else
#define DEFAULT_BUFSIZE	524288
#endif

static int
pcap_activate_bpf(pcap_t *p)
{
	struct pcap_bpf *pb = p->priv;
	int status = 0;
	int fd;
#ifdef LIFNAMSIZ
	char *zonesep;
	struct lifreq ifr;
	char *ifrname = ifr.lifr_name;
	const size_t ifnamsiz = sizeof(ifr.lifr_name);
#else
	struct ifreq ifr;
	char *ifrname = ifr.ifr_name;
	const size_t ifnamsiz = sizeof(ifr.ifr_name);
#endif
	struct bpf_version bv;
#ifdef __APPLE__
	int sockfd;
	char *wltdev = NULL;
#endif
#ifdef BIOCGDLTLIST
	struct bpf_dltlist bdl;
#if defined(__APPLE__) || defined(HAVE_BSD_IEEE80211)
	int new_dlt;
#endif
#endif 
#if defined(BIOCGHDRCMPLT) && defined(BIOCSHDRCMPLT)
	u_int spoof_eth_src = 1;
#endif
	u_int v;
	struct bpf_insn total_insn;
	struct bpf_program total_prog;
	struct utsname osinfo;
	int have_osinfo = 0;
#ifdef HAVE_ZEROCOPY_BPF
	struct bpf_zbuf bz;
	u_int bufmode, zbufmax;
#endif

	fd = bpf_open(p);
	if (fd < 0) {
		status = fd;
		goto bad;
	}

	p->fd = fd;

	if (ioctl(fd, BIOCVERSION, (caddr_t)&bv) < 0) {
		snprintf(p->errbuf, PCAP_ERRBUF_SIZE, "BIOCVERSION: %s",
		    pcap_strerror(errno));
		status = PCAP_ERROR;
		goto bad;
	}
	if (bv.bv_major != BPF_MAJOR_VERSION ||
	    bv.bv_minor < BPF_MINOR_VERSION) {
		snprintf(p->errbuf, PCAP_ERRBUF_SIZE,
		    "kernel bpf filter out of date");
		status = PCAP_ERROR;
		goto bad;
	}

#if defined(LIFNAMSIZ) && defined(ZONENAME_MAX) && defined(lifr_zoneid)
	if ((zonesep = strchr(p->opt.source, '/')) != NULL) {
		char zonename[ZONENAME_MAX];
		int  znamelen;
		char *lnamep;

		znamelen = zonesep - p->opt.source;
		(void) strlcpy(zonename, p->opt.source, znamelen + 1);
		lnamep = strdup(zonesep + 1);
		ifr.lifr_zoneid = getzoneidbyname(zonename);
		free(p->opt.source);
		p->opt.source = lnamep;
	}
#endif

	pb->device = strdup(p->opt.source);
	if (pb->device == NULL) {
		snprintf(p->errbuf, PCAP_ERRBUF_SIZE, "strdup: %s",
		     pcap_strerror(errno));
		status = PCAP_ERROR;
		goto bad;
	}

	if (uname(&osinfo) == 0)
		have_osinfo = 1;

#ifdef __APPLE__
	if (p->opt.rfmon) {
		if (have_osinfo) {
			if (osinfo.release[0] < '8' &&
			    osinfo.release[1] == '.') {
				status = PCAP_ERROR_RFMON_NOTSUP;
				goto bad;
			}
			if (osinfo.release[0] == '8' &&
			    osinfo.release[1] == '.') {
				if (strncmp(p->opt.source, "en", 2) != 0) {
					sockfd = socket(AF_INET, SOCK_DGRAM, 0);
					if (sockfd != -1) {
						strlcpy(ifrname,
						    p->opt.source, ifnamsiz);
						if (ioctl(sockfd, SIOCGIFFLAGS,
						    (char *)&ifr) < 0) {
							status = PCAP_ERROR_NO_SUCH_DEVICE;
							snprintf(p->errbuf,
							    PCAP_ERRBUF_SIZE,
							    "SIOCGIFFLAGS failed: %s",
							    pcap_strerror(errno));
						} else
							status = PCAP_ERROR_RFMON_NOTSUP;
						close(sockfd);
					} else {
						status = PCAP_ERROR_NO_SUCH_DEVICE;
						snprintf(p->errbuf,
						    PCAP_ERRBUF_SIZE,
						    "socket() failed: %s",
						    pcap_strerror(errno));
					}
					goto bad;
				}
				wltdev = malloc(strlen(p->opt.source) + 2);
				if (wltdev == NULL) {
					(void)snprintf(p->errbuf,
					    PCAP_ERRBUF_SIZE, "malloc: %s",
					    pcap_strerror(errno));
					status = PCAP_ERROR;
					goto bad;
				}
				strcpy(wltdev, "wlt");
				strcat(wltdev, p->opt.source + 2);
				free(p->opt.source);
				p->opt.source = wltdev;
			}
		}
	}
#endif 
#ifdef HAVE_ZEROCOPY_BPF
	bufmode = BPF_BUFMODE_ZBUF;
	if (ioctl(fd, BIOCSETBUFMODE, (caddr_t)&bufmode) == 0) {
		pb->zerocopy = 1;

		if (ioctl(fd, BIOCGETZMAX, (caddr_t)&zbufmax) < 0) {
			snprintf(p->errbuf, PCAP_ERRBUF_SIZE, "BIOCGETZMAX: %s",
			    pcap_strerror(errno));
			goto bad;
		}

		if (p->opt.buffer_size != 0) {
			v = p->opt.buffer_size;
		} else {
			if ((ioctl(fd, BIOCGBLEN, (caddr_t)&v) < 0) ||
			    v < DEFAULT_BUFSIZE)
				v = DEFAULT_BUFSIZE;
		}
#ifndef roundup
#define roundup(x, y)   ((((x)+((y)-1))/(y))*(y))  
#endif
		pb->zbufsize = roundup(v, getpagesize());
		if (pb->zbufsize > zbufmax)
			pb->zbufsize = zbufmax;
		pb->zbuf1 = mmap(NULL, pb->zbufsize, PROT_READ | PROT_WRITE,
		    MAP_ANON, -1, 0);
		pb->zbuf2 = mmap(NULL, pb->zbufsize, PROT_READ | PROT_WRITE,
		    MAP_ANON, -1, 0);
		if (pb->zbuf1 == MAP_FAILED || pb->zbuf2 == MAP_FAILED) {
			snprintf(p->errbuf, PCAP_ERRBUF_SIZE, "mmap: %s",
			    pcap_strerror(errno));
			goto bad;
		}
		memset(&bz, 0, sizeof(bz)); 
		bz.bz_bufa = pb->zbuf1;
		bz.bz_bufb = pb->zbuf2;
		bz.bz_buflen = pb->zbufsize;
		if (ioctl(fd, BIOCSETZBUF, (caddr_t)&bz) < 0) {
			snprintf(p->errbuf, PCAP_ERRBUF_SIZE, "BIOCSETZBUF: %s",
			    pcap_strerror(errno));
			goto bad;
		}
		(void)strncpy(ifrname, p->opt.source, ifnamsiz);
		if (ioctl(fd, BIOCSETIF, (caddr_t)&ifr) < 0) {
			snprintf(p->errbuf, PCAP_ERRBUF_SIZE, "BIOCSETIF: %s: %s",
			    p->opt.source, pcap_strerror(errno));
			goto bad;
		}
		v = pb->zbufsize - sizeof(struct bpf_zbuf_header);
	} else
#endif
	{
		if (p->opt.buffer_size != 0) {
			if (ioctl(fd, BIOCSBLEN,
			    (caddr_t)&p->opt.buffer_size) < 0) {
				snprintf(p->errbuf, PCAP_ERRBUF_SIZE,
				    "BIOCSBLEN: %s: %s", p->opt.source,
				    pcap_strerror(errno));
				status = PCAP_ERROR;
				goto bad;
			}

			(void)strncpy(ifrname, p->opt.source, ifnamsiz);
#ifdef BIOCSETLIF
			if (ioctl(fd, BIOCSETLIF, (caddr_t)&ifr) < 0)
#else
			if (ioctl(fd, BIOCSETIF, (caddr_t)&ifr) < 0)
#endif
			{
				status = check_setif_failure(p, errno);
				goto bad;
			}
		} else {
			if ((ioctl(fd, BIOCGBLEN, (caddr_t)&v) < 0) ||
			    v < DEFAULT_BUFSIZE)
				v = DEFAULT_BUFSIZE;
			for ( ; v != 0; v >>= 1) {
				(void) ioctl(fd, BIOCSBLEN, (caddr_t)&v);

				(void)strncpy(ifrname, p->opt.source, ifnamsiz);
#ifdef BIOCSETLIF
				if (ioctl(fd, BIOCSETLIF, (caddr_t)&ifr) >= 0)
#else
				if (ioctl(fd, BIOCSETIF, (caddr_t)&ifr) >= 0)
#endif
					break;	

				if (errno != ENOBUFS) {
					status = check_setif_failure(p, errno);
					goto bad;
				}
			}

			if (v == 0) {
				snprintf(p->errbuf, PCAP_ERRBUF_SIZE,
				    "BIOCSBLEN: %s: No buffer size worked",
				    p->opt.source);
				status = PCAP_ERROR;
				goto bad;
			}
		}
	}

	
	if (ioctl(fd, BIOCGDLT, (caddr_t)&v) < 0) {
		snprintf(p->errbuf, PCAP_ERRBUF_SIZE, "BIOCGDLT: %s",
		    pcap_strerror(errno));
		status = PCAP_ERROR;
		goto bad;
	}

#ifdef _AIX
	switch (v) {

	case IFT_ETHER:
	case IFT_ISO88023:
		v = DLT_EN10MB;
		break;

	case IFT_FDDI:
		v = DLT_FDDI;
		break;

	case IFT_ISO88025:
		v = DLT_IEEE802;
		break;

	case IFT_LOOP:
		v = DLT_NULL;
		break;

	default:
		snprintf(p->errbuf, PCAP_ERRBUF_SIZE, "unknown interface type %u",
		    v);
		status = PCAP_ERROR;
		goto bad;
	}
#endif
#if _BSDI_VERSION - 0 >= 199510
	
	switch (v) {

	case DLT_SLIP:
		v = DLT_SLIP_BSDOS;
		break;

	case DLT_PPP:
		v = DLT_PPP_BSDOS;
		break;

	case 11:	
		v = DLT_FRELAY;
		break;

	case 12:	
		v = DLT_CHDLC;
		break;
	}
#endif

#ifdef BIOCGDLTLIST
	if (get_dlt_list(fd, v, &bdl, p->errbuf) == -1) {
		status = PCAP_ERROR;
		goto bad;
	}
	p->dlt_count = bdl.bfl_len;
	p->dlt_list = bdl.bfl_list;

#ifdef __APPLE__
	if (have_osinfo) {
		if (isdigit((unsigned)osinfo.release[0]) &&
		     (osinfo.release[0] == '9' ||
		     isdigit((unsigned)osinfo.release[1]))) {
			new_dlt = find_802_11(&bdl);
			if (new_dlt != -1) {
				if (p->opt.rfmon) {
					remove_en(p);

					if (new_dlt != v) {
						if (ioctl(p->fd, BIOCSDLT,
						    &new_dlt) != -1) {
							v = new_dlt;
						}
					}
				} else {
					if (!p->oldstyle)
						remove_802_11(p);
				}
			} else {
				if (p->opt.rfmon) {
					status = PCAP_ERROR_RFMON_NOTSUP;
					goto bad;
				}
			}
		}
	}
#elif defined(HAVE_BSD_IEEE80211)
	if (p->opt.rfmon) {
		status = monitor_mode(p, 1);
		if (status != 0) {
			goto bad;
		}

		new_dlt = find_802_11(&bdl);
		if (new_dlt != -1) {
			if (new_dlt != v) {
				if (ioctl(p->fd, BIOCSDLT, &new_dlt) != -1) {
					v = new_dlt;
				}
			}
		}
	}
#endif 
#endif 

	if (v == DLT_EN10MB && p->dlt_count == 0) {
		p->dlt_list = (u_int *) malloc(sizeof(u_int) * 2);
		if (p->dlt_list != NULL) {
			p->dlt_list[0] = DLT_EN10MB;
			p->dlt_list[1] = DLT_DOCSIS;
			p->dlt_count = 2;
		}
	}
#ifdef PCAP_FDDIPAD
	if (v == DLT_FDDI)
		p->fddipad = PCAP_FDDIPAD;
	else
#endif
		p->fddipad = 0;
	p->linktype = v;

#if defined(BIOCGHDRCMPLT) && defined(BIOCSHDRCMPLT)
	/*
	 * Do a BIOCSHDRCMPLT, if defined, to turn that flag on, so
	 * the link-layer source address isn't forcibly overwritten.
	 * (Should we ignore errors?  Should we do this only if
	 * we're open for writing?)
	 *
	 * XXX - I seem to remember some packet-sending bug in some
	 * BSDs - check CVS log for "bpf.c"?
	 */
	if (ioctl(fd, BIOCSHDRCMPLT, &spoof_eth_src) == -1) {
		(void)snprintf(p->errbuf, PCAP_ERRBUF_SIZE,
		    "BIOCSHDRCMPLT: %s", pcap_strerror(errno));
		status = PCAP_ERROR;
		goto bad;
	}
#endif
	
#ifdef HAVE_ZEROCOPY_BPF
	if (p->opt.timeout && !pb->zerocopy) {
#else
	if (p->opt.timeout) {
#endif
		struct timeval to;
#ifdef HAVE_STRUCT_BPF_TIMEVAL
		struct BPF_TIMEVAL bpf_to;

		if (IOCPARM_LEN(BIOCSRTIMEOUT) != sizeof(struct timeval)) {
			bpf_to.tv_sec = p->opt.timeout / 1000;
			bpf_to.tv_usec = (p->opt.timeout * 1000) % 1000000;
			if (ioctl(p->fd, BIOCSRTIMEOUT, (caddr_t)&bpf_to) < 0) {
				snprintf(p->errbuf, PCAP_ERRBUF_SIZE,
				    "BIOCSRTIMEOUT: %s", pcap_strerror(errno));
				status = PCAP_ERROR;
				goto bad;
			}
		} else {
#endif
			to.tv_sec = p->opt.timeout / 1000;
			to.tv_usec = (p->opt.timeout * 1000) % 1000000;
			if (ioctl(p->fd, BIOCSRTIMEOUT, (caddr_t)&to) < 0) {
				snprintf(p->errbuf, PCAP_ERRBUF_SIZE,
				    "BIOCSRTIMEOUT: %s", pcap_strerror(errno));
				status = PCAP_ERROR;
				goto bad;
			}
#ifdef HAVE_STRUCT_BPF_TIMEVAL
		}
#endif
	}

#ifdef	BIOCIMMEDIATE
#ifndef _AIX
	if (p->opt.immediate) {
#endif 
		v = 1;
		if (ioctl(p->fd, BIOCIMMEDIATE, &v) < 0) {
			snprintf(p->errbuf, PCAP_ERRBUF_SIZE,
			    "BIOCIMMEDIATE: %s", pcap_strerror(errno));
			status = PCAP_ERROR;
			goto bad;
		}
#ifndef _AIX
	}
#endif 
#else 
	if (p->opt.immediate) {
		snprintf(p->errbuf, PCAP_ERRBUF_SIZE, "Immediate mode not supported");
		status = PCAP_ERROR;
		goto bad;
	}
#endif 

	if (p->opt.promisc) {
		
		if (ioctl(p->fd, BIOCPROMISC, NULL) < 0) {
			snprintf(p->errbuf, PCAP_ERRBUF_SIZE, "BIOCPROMISC: %s",
			    pcap_strerror(errno));
			status = PCAP_WARNING_PROMISC_NOTSUP;
		}
	}

	if (ioctl(fd, BIOCGBLEN, (caddr_t)&v) < 0) {
		snprintf(p->errbuf, PCAP_ERRBUF_SIZE, "BIOCGBLEN: %s",
		    pcap_strerror(errno));
		status = PCAP_ERROR;
		goto bad;
	}
	p->bufsize = v;
#ifdef HAVE_ZEROCOPY_BPF
	if (!pb->zerocopy) {
#endif
	p->buffer = (u_char *)malloc(p->bufsize);
	if (p->buffer == NULL) {
		snprintf(p->errbuf, PCAP_ERRBUF_SIZE, "malloc: %s",
		    pcap_strerror(errno));
		status = PCAP_ERROR;
		goto bad;
	}
#ifdef _AIX
	memset(p->buffer, 0x0, p->bufsize);
#endif
#ifdef HAVE_ZEROCOPY_BPF
	}
#endif

	total_insn.code = (u_short)(BPF_RET | BPF_K);
	total_insn.jt = 0;
	total_insn.jf = 0;
	total_insn.k = p->snapshot;

	total_prog.bf_len = 1;
	total_prog.bf_insns = &total_insn;
	if (ioctl(p->fd, BIOCSETF, (caddr_t)&total_prog) < 0) {
		snprintf(p->errbuf, PCAP_ERRBUF_SIZE, "BIOCSETF: %s",
		    pcap_strerror(errno));
		status = PCAP_ERROR;
		goto bad;
	}

	p->selectable_fd = p->fd;	
	if (have_osinfo) {
		if (strcmp(osinfo.sysname, "FreeBSD") == 0) {
			if (strncmp(osinfo.release, "4.3-", 4) == 0 ||
			     strncmp(osinfo.release, "4.4-", 4) == 0)
				p->selectable_fd = -1;
		}
	}

	p->read_op = pcap_read_bpf;
	p->inject_op = pcap_inject_bpf;
	p->setfilter_op = pcap_setfilter_bpf;
	p->setdirection_op = pcap_setdirection_bpf;
	p->set_datalink_op = pcap_set_datalink_bpf;
	p->getnonblock_op = pcap_getnonblock_bpf;
	p->setnonblock_op = pcap_setnonblock_bpf;
	p->stats_op = pcap_stats_bpf;
	p->cleanup_op = pcap_cleanup_bpf;

	return (status);
 bad:
	pcap_cleanup_bpf(p);
	return (status);
}

int
pcap_platform_finddevs(pcap_if_t **alldevsp, char *errbuf)
{
	return (0);
}

#ifdef HAVE_BSD_IEEE80211
static int
monitor_mode(pcap_t *p, int set)
{
	struct pcap_bpf *pb = p->priv;
	int sock;
	struct ifmediareq req;
	int *media_list;
	int i;
	int can_do;
	struct ifreq ifr;

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock == -1) {
		snprintf(p->errbuf, PCAP_ERRBUF_SIZE, "can't open socket: %s",
		    pcap_strerror(errno));
		return (PCAP_ERROR);
	}

	memset(&req, 0, sizeof req);
	strncpy(req.ifm_name, p->opt.source, sizeof req.ifm_name);

	if (ioctl(sock, SIOCGIFMEDIA, &req) < 0) {
		switch (errno) {

		case ENXIO:
			close(sock);
			return (PCAP_ERROR_NO_SUCH_DEVICE);

		case EINVAL:
			close(sock);
			return (PCAP_ERROR_RFMON_NOTSUP);

		default:
			snprintf(p->errbuf, PCAP_ERRBUF_SIZE,
			    "SIOCGIFMEDIA 1: %s", pcap_strerror(errno));
			close(sock);
			return (PCAP_ERROR);
		}
	}
	if (req.ifm_count == 0) {
		close(sock);
		return (PCAP_ERROR_RFMON_NOTSUP);
	}

	media_list = malloc(req.ifm_count * sizeof(int));
	if (media_list == NULL) {
		snprintf(p->errbuf, PCAP_ERRBUF_SIZE, "malloc: %s",
		    pcap_strerror(errno));
		close(sock);
		return (PCAP_ERROR);
	}
	req.ifm_ulist = media_list;
	if (ioctl(sock, SIOCGIFMEDIA, &req) < 0) {
		snprintf(p->errbuf, PCAP_ERRBUF_SIZE, "SIOCGIFMEDIA: %s",
		    pcap_strerror(errno));
		free(media_list);
		close(sock);
		return (PCAP_ERROR);
	}

	can_do = 0;
	for (i = 0; i < req.ifm_count; i++) {
		if (IFM_TYPE(media_list[i]) == IFM_IEEE80211
		    && IFM_SUBTYPE(media_list[i]) == IFM_AUTO) {
			
			if (media_list[i] & IFM_IEEE80211_MONITOR) {
				can_do = 1;
				break;
			}
		}
	}
	free(media_list);
	if (!can_do) {
		close(sock);
		return (PCAP_ERROR_RFMON_NOTSUP);
	}

	if (set) {
		if ((req.ifm_current & IFM_IEEE80211_MONITOR) == 0) {

			if (!pcap_do_addexit(p)) {
				snprintf(p->errbuf, PCAP_ERRBUF_SIZE,
				     "atexit failed");
				close(sock);
				return (PCAP_ERROR);
			}
			memset(&ifr, 0, sizeof(ifr));
			(void)strncpy(ifr.ifr_name, p->opt.source,
			    sizeof(ifr.ifr_name));
			ifr.ifr_media = req.ifm_current | IFM_IEEE80211_MONITOR;
			if (ioctl(sock, SIOCSIFMEDIA, &ifr) == -1) {
				snprintf(p->errbuf, PCAP_ERRBUF_SIZE,
				     "SIOCSIFMEDIA: %s", pcap_strerror(errno));
				close(sock);
				return (PCAP_ERROR);
			}

			pb->must_do_on_close |= MUST_CLEAR_RFMON;

			pcap_add_to_pcaps_to_close(p);
		}
	}
	return (0);
}
#endif 

#if defined(BIOCGDLTLIST) && (defined(__APPLE__) || defined(HAVE_BSD_IEEE80211))
static int
find_802_11(struct bpf_dltlist *bdlp)
{
	int new_dlt;
	int i;

	new_dlt = -1;
	for (i = 0; i < bdlp->bfl_len; i++) {
		switch (bdlp->bfl_list[i]) {

		case DLT_IEEE802_11:
			if (new_dlt == -1)
				new_dlt = bdlp->bfl_list[i];
			break;

		case DLT_PRISM_HEADER:
		case DLT_AIRONET_HEADER:
		case DLT_IEEE802_11_RADIO_AVS:
			if (new_dlt != DLT_IEEE802_11_RADIO)
				new_dlt = bdlp->bfl_list[i];
			break;

		case DLT_IEEE802_11_RADIO:
			new_dlt = bdlp->bfl_list[i];
			break;

		default:
			break;
		}
	}

	return (new_dlt);
}
#endif 

#if defined(__APPLE__) && defined(BIOCGDLTLIST)
static void
remove_en(pcap_t *p)
{
	int i, j;

	j = 0;
	for (i = 0; i < p->dlt_count; i++) {
		switch (p->dlt_list[i]) {

		case DLT_EN10MB:
			continue;

		default:
			break;
		}

		p->dlt_list[j] = p->dlt_list[i];
		j++;
	}

	p->dlt_count = j;
}

static void
remove_802_11(pcap_t *p)
{
	int i, j;

	j = 0;
	for (i = 0; i < p->dlt_count; i++) {
		switch (p->dlt_list[i]) {

		case DLT_IEEE802_11:
		case DLT_PRISM_HEADER:
		case DLT_AIRONET_HEADER:
		case DLT_IEEE802_11_RADIO:
		case DLT_IEEE802_11_RADIO_AVS:
			continue;

		default:
			break;
		}

		p->dlt_list[j] = p->dlt_list[i];
		j++;
	}

	p->dlt_count = j;
}
#endif 

static int
pcap_setfilter_bpf(pcap_t *p, struct bpf_program *fp)
{
	struct pcap_bpf *pb = p->priv;

	pcap_freecode(&p->fcode);

	if (ioctl(p->fd, BIOCSETF, (caddr_t)fp) == 0) {
		pb->filtering_in_kernel = 1;	

		p->cc = 0;
		return (0);
	}

	if (errno != EINVAL) {
		snprintf(p->errbuf, PCAP_ERRBUF_SIZE, "BIOCSETF: %s",
		    pcap_strerror(errno));
		return (-1);
	}

	if (install_bpf_program(p, fp) < 0)
		return (-1);
	pb->filtering_in_kernel = 0;	
	return (0);
}

static int
pcap_setdirection_bpf(pcap_t *p, pcap_direction_t d)
{
#if defined(BIOCSDIRECTION)
	u_int direction;

	direction = (d == PCAP_D_IN) ? BPF_D_IN :
	    ((d == PCAP_D_OUT) ? BPF_D_OUT : BPF_D_INOUT);
	if (ioctl(p->fd, BIOCSDIRECTION, &direction) == -1) {
		(void) snprintf(p->errbuf, sizeof(p->errbuf),
		    "Cannot set direction to %s: %s",
		        (d == PCAP_D_IN) ? "PCAP_D_IN" :
			((d == PCAP_D_OUT) ? "PCAP_D_OUT" : "PCAP_D_INOUT"),
			strerror(errno));
		return (-1);
	}
	return (0);
#elif defined(BIOCSSEESENT)
	u_int seesent;

	if (d == PCAP_D_OUT) {
		snprintf(p->errbuf, sizeof(p->errbuf),
		    "Setting direction to PCAP_D_OUT is not supported on BPF");
		return -1;
	}

	seesent = (d == PCAP_D_INOUT);
	if (ioctl(p->fd, BIOCSSEESENT, &seesent) == -1) {
		(void) snprintf(p->errbuf, sizeof(p->errbuf),
		    "Cannot set direction to %s: %s",
		        (d == PCAP_D_INOUT) ? "PCAP_D_INOUT" : "PCAP_D_IN",
			strerror(errno));
		return (-1);
	}
	return (0);
#else
	(void) snprintf(p->errbuf, sizeof(p->errbuf),
	    "This system doesn't support BIOCSSEESENT, so the direction can't be set");
	return (-1);
#endif
}

static int
pcap_set_datalink_bpf(pcap_t *p, int dlt)
{
#ifdef BIOCSDLT
	if (ioctl(p->fd, BIOCSDLT, &dlt) == -1) {
		(void) snprintf(p->errbuf, sizeof(p->errbuf),
		    "Cannot set DLT %d: %s", dlt, strerror(errno));
		return (-1);
	}
#endif
	return (0);
}
