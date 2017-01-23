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
 * sf-pcap.c - libpcap-file-format-specific code from savefile.c
 *	Extraction/creation by Jeffrey Mogul, DECWRL
 *	Modified by Steve McCanne, LBL.
 *
 * Used to save the received packet headers, after filtering, to
 * a file, and then read them later.
 * The first record in the file contains saved values for the machine
 * dependent values so we can print the dump file on any architecture.
 */

#ifndef lint
static const char rcsid[] _U_ =
    "@(#) $Header$ (LBL)";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef WIN32
#include <pcap-stdinc.h>
#else 
#if HAVE_INTTYPES_H
#include <inttypes.h>
#elif HAVE_STDINT_H
#include <stdint.h>
#endif
#ifdef HAVE_SYS_BITYPES_H
#include <sys/bitypes.h>
#endif
#include <sys/types.h>
#endif 

#include <errno.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pcap-int.h"

#include "pcap-common.h"

#ifdef HAVE_OS_PROTO_H
#include "os-proto.h"
#endif

#include "sf-pcap.h"

#if defined(WIN32)
  #define SET_BINMODE(f)  _setmode(_fileno(f), _O_BINARY)
#elif defined(MSDOS)
  #if defined(__HIGHC__)
  #define SET_BINMODE(f)  setmode(f, O_BINARY)
  #else
  #define SET_BINMODE(f)  setmode(fileno(f), O_BINARY)
  #endif
#endif

#define TCPDUMP_MAGIC		0xa1b2c3d4

#define KUZNETZOV_TCPDUMP_MAGIC	0xa1b2cd34

#define FMESQUITA_TCPDUMP_MAGIC	0xa1b234cd

#define NAVTEL_TCPDUMP_MAGIC	0xa12b3c4d

#define NSEC_TCPDUMP_MAGIC	0xa1b23c4d

#define LT_LINKTYPE(x)		((x) & 0x03FFFFFF)
#define LT_LINKTYPE_EXT(x)	((x) & 0xFC000000)

static int pcap_next_packet(pcap_t *p, struct pcap_pkthdr *hdr, u_char **datap);

typedef enum {
	NOT_SWAPPED,
	SWAPPED,
	MAYBE_SWAPPED
} swapped_type_t;

typedef enum {
	PASS_THROUGH,
	SCALE_UP,
	SCALE_DOWN
} tstamp_scale_type_t;

struct pcap_sf {
	size_t hdrsize;
	swapped_type_t lengths_swapped;
	tstamp_scale_type_t scale_type;
};

pcap_t *
pcap_check_header(bpf_u_int32 magic, FILE *fp, u_int precision, char *errbuf,
    int *err)
{
	struct pcap_file_header hdr;
	size_t amt_read;
	pcap_t *p;
	int swapped = 0;
	struct pcap_sf *ps;

	*err = 0;

	if (magic != TCPDUMP_MAGIC && magic != KUZNETZOV_TCPDUMP_MAGIC &&
	    magic != NSEC_TCPDUMP_MAGIC) {
		magic = SWAPLONG(magic);
		if (magic != TCPDUMP_MAGIC && magic != KUZNETZOV_TCPDUMP_MAGIC &&
		    magic != NSEC_TCPDUMP_MAGIC)
			return (NULL);	
		swapped = 1;
	}

	hdr.magic = magic;
	amt_read = fread(((char *)&hdr) + sizeof hdr.magic, 1,
	    sizeof(hdr) - sizeof(hdr.magic), fp);
	if (amt_read != sizeof(hdr) - sizeof(hdr.magic)) {
		if (ferror(fp)) {
			snprintf(errbuf, PCAP_ERRBUF_SIZE,
			    "error reading dump file: %s",
			    pcap_strerror(errno));
		} else {
			snprintf(errbuf, PCAP_ERRBUF_SIZE,
			    "truncated dump file; tried to read %lu file header bytes, only got %lu",
			    (unsigned long)sizeof(hdr),
			    (unsigned long)amt_read);
		}
		*err = 1;
		return (NULL);
	}

	if (swapped) {
		hdr.version_major = SWAPSHORT(hdr.version_major);
		hdr.version_minor = SWAPSHORT(hdr.version_minor);
		hdr.thiszone = SWAPLONG(hdr.thiszone);
		hdr.sigfigs = SWAPLONG(hdr.sigfigs);
		hdr.snaplen = SWAPLONG(hdr.snaplen);
		hdr.linktype = SWAPLONG(hdr.linktype);
	}

	if (hdr.version_major < PCAP_VERSION_MAJOR) {
		snprintf(errbuf, PCAP_ERRBUF_SIZE,
		    "archaic pcap savefile format");
		*err = 1;
		return (NULL);
	}

	p = pcap_open_offline_common(errbuf, sizeof (struct pcap_sf));
	if (p == NULL) {
		
		*err = 1;
		return (NULL);
	}
	p->swapped = swapped;
	p->version_major = hdr.version_major;
	p->version_minor = hdr.version_minor;
	p->tzoff = hdr.thiszone;
	p->snapshot = hdr.snaplen;
	p->linktype = linktype_to_dlt(LT_LINKTYPE(hdr.linktype));
	p->linktype_ext = LT_LINKTYPE_EXT(hdr.linktype);

	p->next_packet_op = pcap_next_packet;

	ps = p->priv;

	p->opt.tstamp_precision = precision;

	switch (precision) {

	case PCAP_TSTAMP_PRECISION_MICRO:
		if (magic == NSEC_TCPDUMP_MAGIC) {
			ps->scale_type = SCALE_DOWN;
		} else {
			ps->scale_type = PASS_THROUGH;
		}
		break;

	case PCAP_TSTAMP_PRECISION_NANO:
		if (magic == NSEC_TCPDUMP_MAGIC) {
			ps->scale_type = PASS_THROUGH;
		} else {
			ps->scale_type = SCALE_UP;
		}
		break;

	default:
		snprintf(errbuf, PCAP_ERRBUF_SIZE,
		    "unknown time stamp resolution %u", precision);
		free(p);
		*err = 1;
		return (NULL);
	}

	/*
	 * We interchanged the caplen and len fields at version 2.3,
	 * in order to match the bpf header layout.  But unfortunately
	 * some files were written with version 2.3 in their headers
	 * but without the interchanged fields.
	 *
	 * In addition, DG/UX tcpdump writes out files with a version
	 * number of 543.0, and with the caplen and len fields in the
	 * pre-2.3 order.
	 */
	switch (hdr.version_major) {

	case 2:
		if (hdr.version_minor < 3)
			ps->lengths_swapped = SWAPPED;
		else if (hdr.version_minor == 3)
			ps->lengths_swapped = MAYBE_SWAPPED;
		else
			ps->lengths_swapped = NOT_SWAPPED;
		break;

	case 543:
		ps->lengths_swapped = SWAPPED;
		break;

	default:
		ps->lengths_swapped = NOT_SWAPPED;
		break;
	}

	if (magic == KUZNETZOV_TCPDUMP_MAGIC) {
		ps->hdrsize = sizeof(struct pcap_sf_patched_pkthdr);

		if (p->linktype == DLT_EN10MB) {
			p->snapshot += 14;
		}
	} else
		ps->hdrsize = sizeof(struct pcap_sf_pkthdr);

	p->bufsize = p->snapshot;
	if (p->bufsize <= 0) {
		p->bufsize = 65536;
	}
	p->buffer = malloc(p->bufsize);
	if (p->buffer == NULL) {
		snprintf(errbuf, PCAP_ERRBUF_SIZE, "out of memory");
		free(p);
		*err = 1;
		return (NULL);
	}

	p->cleanup_op = sf_cleanup;

	return (p);
}

static int
pcap_next_packet(pcap_t *p, struct pcap_pkthdr *hdr, u_char **data)
{
	struct pcap_sf *ps = p->priv;
	struct pcap_sf_patched_pkthdr sf_hdr;
	FILE *fp = p->rfile;
	size_t amt_read;
	bpf_u_int32 t;

	amt_read = fread(&sf_hdr, 1, ps->hdrsize, fp);
	if (amt_read != ps->hdrsize) {
		if (ferror(fp)) {
			snprintf(p->errbuf, PCAP_ERRBUF_SIZE,
			    "error reading dump file: %s",
			    pcap_strerror(errno));
			return (-1);
		} else {
			if (amt_read != 0) {
				snprintf(p->errbuf, PCAP_ERRBUF_SIZE,
				    "truncated dump file; tried to read %lu header bytes, only got %lu",
				    (unsigned long)ps->hdrsize,
				    (unsigned long)amt_read);
				return (-1);
			}
			
			return (1);
		}
	}

	if (p->swapped) {
		/* these were written in opposite byte order */
		hdr->caplen = SWAPLONG(sf_hdr.caplen);
		hdr->len = SWAPLONG(sf_hdr.len);
		hdr->ts.tv_sec = SWAPLONG(sf_hdr.ts.tv_sec);
		hdr->ts.tv_usec = SWAPLONG(sf_hdr.ts.tv_usec);
	} else {
		hdr->caplen = sf_hdr.caplen;
		hdr->len = sf_hdr.len;
		hdr->ts.tv_sec = sf_hdr.ts.tv_sec;
		hdr->ts.tv_usec = sf_hdr.ts.tv_usec;
	}

	switch (ps->scale_type) {

	case PASS_THROUGH:
		break;

	case SCALE_UP:
		hdr->ts.tv_usec = hdr->ts.tv_usec * 1000;
		break;

	case SCALE_DOWN:
		hdr->ts.tv_usec = hdr->ts.tv_usec / 1000;
		break;
	}

	
	switch (ps->lengths_swapped) {

	case NOT_SWAPPED:
		break;

	case MAYBE_SWAPPED:
		if (hdr->caplen <= hdr->len) {
			break;
		}
		

	case SWAPPED:
		t = hdr->caplen;
		hdr->caplen = hdr->len;
		hdr->len = t;
		break;
	}

	if (hdr->caplen > p->bufsize) {
		static u_char *tp = NULL;
		static size_t tsize = 0;

		if (hdr->caplen > 65535) {
			snprintf(p->errbuf, PCAP_ERRBUF_SIZE,
			    "bogus savefile header");
			return (-1);
		}

		if (tsize < hdr->caplen) {
			tsize = ((hdr->caplen + 1023) / 1024) * 1024;
			if (tp != NULL)
				free((u_char *)tp);
			tp = (u_char *)malloc(tsize);
			if (tp == NULL) {
				tsize = 0;
				snprintf(p->errbuf, PCAP_ERRBUF_SIZE,
				    "BUFMOD hack malloc");
				return (-1);
			}
		}
		amt_read = fread((char *)tp, 1, hdr->caplen, fp);
		if (amt_read != hdr->caplen) {
			if (ferror(fp)) {
				snprintf(p->errbuf, PCAP_ERRBUF_SIZE,
				    "error reading dump file: %s",
				    pcap_strerror(errno));
			} else {
				snprintf(p->errbuf, PCAP_ERRBUF_SIZE,
				    "truncated dump file; tried to read %u captured bytes, only got %lu",
				    hdr->caplen, (unsigned long)amt_read);
			}
			return (-1);
		}
		hdr->caplen = p->bufsize;
		memcpy(p->buffer, (char *)tp, p->bufsize);
	} else {
		
		amt_read = fread(p->buffer, 1, hdr->caplen, fp);
		if (amt_read != hdr->caplen) {
			if (ferror(fp)) {
				snprintf(p->errbuf, PCAP_ERRBUF_SIZE,
				    "error reading dump file: %s",
				    pcap_strerror(errno));
			} else {
				snprintf(p->errbuf, PCAP_ERRBUF_SIZE,
				    "truncated dump file; tried to read %u captured bytes, only got %lu",
				    hdr->caplen, (unsigned long)amt_read);
			}
			return (-1);
		}
	}
	*data = p->buffer;

	if (p->swapped)
		swap_pseudo_headers(p->linktype, hdr, *data);

	return (0);
}

static int
sf_write_header(pcap_t *p, FILE *fp, int linktype, int thiszone, int snaplen)
{
	struct pcap_file_header hdr;

	hdr.magic = p->opt.tstamp_precision == PCAP_TSTAMP_PRECISION_NANO ? NSEC_TCPDUMP_MAGIC : TCPDUMP_MAGIC;
	hdr.version_major = PCAP_VERSION_MAJOR;
	hdr.version_minor = PCAP_VERSION_MINOR;

	hdr.thiszone = thiszone;
	hdr.snaplen = snaplen;
	hdr.sigfigs = 0;
	hdr.linktype = linktype;

	if (fwrite((char *)&hdr, sizeof(hdr), 1, fp) != 1)
		return (-1);

	return (0);
}

void
pcap_dump(u_char *user, const struct pcap_pkthdr *h, const u_char *sp)
{
	register FILE *f;
	struct pcap_sf_pkthdr sf_hdr;

	f = (FILE *)user;
	sf_hdr.ts.tv_sec  = h->ts.tv_sec;
	sf_hdr.ts.tv_usec = h->ts.tv_usec;
	sf_hdr.caplen     = h->caplen;
	sf_hdr.len        = h->len;
	
	(void)fwrite(&sf_hdr, sizeof(sf_hdr), 1, f);
	(void)fwrite(sp, h->caplen, 1, f);
}

static pcap_dumper_t *
pcap_setup_dump(pcap_t *p, int linktype, FILE *f, const char *fname)
{

#if defined(WIN32) || defined(MSDOS)
	if (f == stdout)
		SET_BINMODE(f);
	else
		setbuf(f, NULL);
#endif
	if (sf_write_header(p, f, linktype, p->tzoff, p->snapshot) == -1) {
		snprintf(p->errbuf, PCAP_ERRBUF_SIZE, "Can't write to %s: %s",
		    fname, pcap_strerror(errno));
		if (f != stdout)
			(void)fclose(f);
		return (NULL);
	}
	return ((pcap_dumper_t *)f);
}

pcap_dumper_t *
pcap_dump_open(pcap_t *p, const char *fname)
{
	FILE *f;
	int linktype;

	if (!p->activated) {
		snprintf(p->errbuf, PCAP_ERRBUF_SIZE,
		    "%s: not-yet-activated pcap_t passed to pcap_dump_open",
		    fname);
		return (NULL);
	}
	linktype = dlt_to_linktype(p->linktype);
	if (linktype == -1) {
		snprintf(p->errbuf, PCAP_ERRBUF_SIZE,
		    "%s: link-layer type %d isn't supported in savefiles",
		    fname, p->linktype);
		return (NULL);
	}
	linktype |= p->linktype_ext;

	if (fname[0] == '-' && fname[1] == '\0') {
		f = stdout;
		fname = "standard output";
	} else {
#if !defined(WIN32) && !defined(MSDOS)
		f = fopen(fname, "w");
#else
		f = fopen(fname, "wb");
#endif
		if (f == NULL) {
			snprintf(p->errbuf, PCAP_ERRBUF_SIZE, "%s: %s",
			    fname, pcap_strerror(errno));
			return (NULL);
		}
	}
	return (pcap_setup_dump(p, linktype, f, fname));
}

pcap_dumper_t *
pcap_dump_fopen(pcap_t *p, FILE *f)
{	
	int linktype;

	linktype = dlt_to_linktype(p->linktype);
	if (linktype == -1) {
		snprintf(p->errbuf, PCAP_ERRBUF_SIZE,
		    "stream: link-layer type %d isn't supported in savefiles",
		    p->linktype);
		return (NULL);
	}
	linktype |= p->linktype_ext;

	return (pcap_setup_dump(p, linktype, f, "stream"));
}

FILE *
pcap_dump_file(pcap_dumper_t *p)
{
	return ((FILE *)p);
}

long
pcap_dump_ftell(pcap_dumper_t *p)
{
	return (ftell((FILE *)p));
}

int
pcap_dump_flush(pcap_dumper_t *p)
{

	if (fflush((FILE *)p) == EOF)
		return (-1);
	else
		return (0);
}

void
pcap_dump_close(pcap_dumper_t *p)
{

#ifdef notyet
	if (ferror((FILE *)p))
		return-an-error;
	
#endif
	(void)fclose((FILE *)p);
}
