/*
 * Copyright (c) 1988, 1989, 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1997, 2000
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
 * Support for splitting captures into multiple files with a maximum
 * file size:
 *
 * Copyright (c) 2001
 *	Seth Webster <swebster@sst.ll.mit.edu>
 */

#ifndef lint
static const char copyright[] _U_ =
    "@(#) Copyright (c) 1988, 1989, 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1997, 2000\n\
The Regents of the University of California.  All rights reserved.\n";
static const char rcsid[] _U_ =
    "@(#) $Header: /tcpdump/master/tcpdump/tcpdump.c,v 1.283 2008-09-25 21:45:50 guy Exp $ (LBL)";
#endif

/*
 * tcpdump - monitor tcp/ip traffic on an ethernet.
 *
 * First written in 1987 by Van Jacobson, Lawrence Berkeley Laboratory.
 * Mercilessly hacked and occasionally improved since then via the
 * combined efforts of Van, Steve McCanne and Craig Leres of LBL.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <tcpdump-stdinc.h>

#ifdef WIN32
#include "getopt.h"
#include "w32_fzs.h"
extern int strcasecmp (const char *__s1, const char *__s2);
extern int SIZE_BUF;
#define off_t long
#define uint UINT
#endif 

#ifdef HAVE_SMI_H
#include <smi.h>
#endif

#include <pcap.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#ifndef WIN32
#include <sys/wait.h>
#include <sys/resource.h>
#include <pwd.h>
#include <grp.h>
#include <errno.h>
#endif 

#ifdef HAVE_CAP_NG_H
#include <cap-ng.h>
#endif 

#include "netdissect.h"
#include "interface.h"
#include "addrtoname.h"
#include "machdep.h"
#include "setsignal.h"
#include "gmt2local.h"
#include "pcap-missing.h"

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

#ifdef SIGINFO
#define SIGNAL_REQ_INFO SIGINFO
#elif SIGUSR1
#define SIGNAL_REQ_INFO SIGUSR1
#endif

netdissect_options Gndo;
netdissect_options *gndo = &Gndo;

static int dflag;			
static int Lflag;			
#ifdef HAVE_PCAP_SET_TSTAMP_TYPE
static int Jflag;			
#endif
#ifdef HAVE_PCAP_SETDIRECTION
int Pflag = -1;	
#endif
static char *zflag = NULL;		

static int infodelay;
static int infoprint;

char *program_name;

int32_t thiszone;		

static RETSIGTYPE cleanup(int);
static RETSIGTYPE child_cleanup(int);
static void usage(void) __attribute__((noreturn));
static void show_dlts_and_exit(const char *device, pcap_t *pd) __attribute__((noreturn));

static void print_packet(u_char *, const struct pcap_pkthdr *, const u_char *);
static void ndo_default_print(netdissect_options *, const u_char *, u_int);
static void dump_packet_and_trunc(u_char *, const struct pcap_pkthdr *, const u_char *);
static void dump_packet(u_char *, const struct pcap_pkthdr *, const u_char *);
static void droproot(const char *, const char *);
static void ndo_error(netdissect_options *ndo, const char *fmt, ...)
     __attribute__ ((noreturn, format (printf, 2, 3)));
static void ndo_warning(netdissect_options *ndo, const char *fmt, ...);

#ifdef SIGNAL_REQ_INFO
RETSIGTYPE requestinfo(int);
#endif

#if defined(USE_WIN32_MM_TIMER)
  #include <MMsystem.h>
  static UINT timer_id;
  static void CALLBACK verbose_stats_dump(UINT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR);
#elif defined(HAVE_ALARM)
  static void verbose_stats_dump(int sig);
#endif

static void info(int);
static u_int packets_captured;

struct printer {
        if_printer f;
	int type;
};


struct ndo_printer {
        if_ndo_printer f;
	int type;
};


static struct printer printers[] = {
	{ arcnet_if_print,	DLT_ARCNET },
#ifdef DLT_ARCNET_LINUX
	{ arcnet_linux_if_print, DLT_ARCNET_LINUX },
#endif
	{ token_if_print,	DLT_IEEE802 },
#ifdef DLT_LANE8023
	{ lane_if_print,        DLT_LANE8023 },
#endif
#ifdef DLT_CIP
	{ cip_if_print,         DLT_CIP },
#endif
#ifdef DLT_ATM_CLIP
	{ cip_if_print,		DLT_ATM_CLIP },
#endif
	{ sl_if_print,		DLT_SLIP },
#ifdef DLT_SLIP_BSDOS
	{ sl_bsdos_if_print,	DLT_SLIP_BSDOS },
#endif
	{ ppp_if_print,		DLT_PPP },
#ifdef DLT_PPP_WITHDIRECTION
	{ ppp_if_print,		DLT_PPP_WITHDIRECTION },
#endif
#ifdef DLT_PPP_BSDOS
	{ ppp_bsdos_if_print,	DLT_PPP_BSDOS },
#endif
	{ fddi_if_print,	DLT_FDDI },
	{ null_if_print,	DLT_NULL },
#ifdef DLT_LOOP
	{ null_if_print,	DLT_LOOP },
#endif
	{ raw_if_print,		DLT_RAW },
	{ atm_if_print,		DLT_ATM_RFC1483 },
#ifdef DLT_C_HDLC
	{ chdlc_if_print,	DLT_C_HDLC },
#endif
#ifdef DLT_HDLC
	{ chdlc_if_print,	DLT_HDLC },
#endif
#ifdef DLT_PPP_SERIAL
	{ ppp_hdlc_if_print,	DLT_PPP_SERIAL },
#endif
#ifdef DLT_PPP_ETHER
	{ pppoe_if_print,	DLT_PPP_ETHER },
#endif
#ifdef DLT_LINUX_SLL
	{ sll_if_print,		DLT_LINUX_SLL },
#endif
#ifdef DLT_IEEE802_11
	{ ieee802_11_if_print,	DLT_IEEE802_11},
#endif
#ifdef DLT_LTALK
	{ ltalk_if_print,	DLT_LTALK },
#endif
#if defined(DLT_PFLOG) && defined(HAVE_NET_PFVAR_H)
	{ pflog_if_print,	DLT_PFLOG },
#endif
#ifdef DLT_FR
	{ fr_if_print,		DLT_FR },
#endif
#ifdef DLT_FRELAY
	{ fr_if_print,		DLT_FRELAY },
#endif
#ifdef DLT_SUNATM
	{ sunatm_if_print,	DLT_SUNATM },
#endif
#ifdef DLT_IP_OVER_FC
	{ ipfc_if_print,	DLT_IP_OVER_FC },
#endif
#ifdef DLT_PRISM_HEADER
	{ prism_if_print,	DLT_PRISM_HEADER },
#endif
#ifdef DLT_IEEE802_11_RADIO
	{ ieee802_11_radio_if_print,	DLT_IEEE802_11_RADIO },
#endif
#ifdef DLT_ENC
	{ enc_if_print,		DLT_ENC },
#endif
#ifdef DLT_SYMANTEC_FIREWALL
	{ symantec_if_print,	DLT_SYMANTEC_FIREWALL },
#endif
#ifdef DLT_APPLE_IP_OVER_IEEE1394
	{ ap1394_if_print,	DLT_APPLE_IP_OVER_IEEE1394 },
#endif
#ifdef DLT_IEEE802_11_RADIO_AVS
	{ ieee802_11_radio_avs_if_print,	DLT_IEEE802_11_RADIO_AVS },
#endif
#ifdef DLT_JUNIPER_ATM1
	{ juniper_atm1_print,	DLT_JUNIPER_ATM1 },
#endif
#ifdef DLT_JUNIPER_ATM2
	{ juniper_atm2_print,	DLT_JUNIPER_ATM2 },
#endif
#ifdef DLT_JUNIPER_MFR
	{ juniper_mfr_print,	DLT_JUNIPER_MFR },
#endif
#ifdef DLT_JUNIPER_MLFR
	{ juniper_mlfr_print,	DLT_JUNIPER_MLFR },
#endif
#ifdef DLT_JUNIPER_MLPPP
	{ juniper_mlppp_print,	DLT_JUNIPER_MLPPP },
#endif
#ifdef DLT_JUNIPER_PPPOE
	{ juniper_pppoe_print,	DLT_JUNIPER_PPPOE },
#endif
#ifdef DLT_JUNIPER_PPPOE_ATM
	{ juniper_pppoe_atm_print, DLT_JUNIPER_PPPOE_ATM },
#endif
#ifdef DLT_JUNIPER_GGSN
	{ juniper_ggsn_print,	DLT_JUNIPER_GGSN },
#endif
#ifdef DLT_JUNIPER_ES
	{ juniper_es_print,	DLT_JUNIPER_ES },
#endif
#ifdef DLT_JUNIPER_MONITOR
	{ juniper_monitor_print, DLT_JUNIPER_MONITOR },
#endif
#ifdef DLT_JUNIPER_SERVICES
	{ juniper_services_print, DLT_JUNIPER_SERVICES },
#endif
#ifdef DLT_JUNIPER_ETHER
	{ juniper_ether_print,	DLT_JUNIPER_ETHER },
#endif
#ifdef DLT_JUNIPER_PPP
	{ juniper_ppp_print,	DLT_JUNIPER_PPP },
#endif
#ifdef DLT_JUNIPER_FRELAY
	{ juniper_frelay_print,	DLT_JUNIPER_FRELAY },
#endif
#ifdef DLT_JUNIPER_CHDLC
	{ juniper_chdlc_print,	DLT_JUNIPER_CHDLC },
#endif
#ifdef DLT_MFR
	{ mfr_if_print,		DLT_MFR },
#endif
#if defined(DLT_BLUETOOTH_HCI_H4_WITH_PHDR) && defined(HAVE_PCAP_BLUETOOTH_H)
	{ bt_if_print,		DLT_BLUETOOTH_HCI_H4_WITH_PHDR},
#endif
#ifdef HAVE_PCAP_USB_H
#ifdef DLT_USB_LINUX
	{ usb_linux_48_byte_print, DLT_USB_LINUX},
#endif 
#ifdef DLT_USB_LINUX_MMAPPED
	{ usb_linux_64_byte_print, DLT_USB_LINUX_MMAPPED},
#endif 
#endif 
#ifdef DLT_IPV4
	{ raw_if_print,		DLT_IPV4 },
#endif
#ifdef DLT_IPV6
	{ raw_if_print,		DLT_IPV6 },
#endif
	{ NULL,			0 },
};

static struct ndo_printer ndo_printers[] = {
	{ ether_if_print,	DLT_EN10MB },
#ifdef DLT_IPNET
	{ ipnet_if_print,	DLT_IPNET },
#endif
#ifdef DLT_IEEE802_15_4
	{ ieee802_15_4_if_print, DLT_IEEE802_15_4 },
#endif
#ifdef DLT_IEEE802_15_4_NOFCS
	{ ieee802_15_4_if_print, DLT_IEEE802_15_4_NOFCS },
#endif
#ifdef DLT_PPI
	{ ppi_if_print,		DLT_PPI },
#endif
#ifdef DLT_NETANALYZER
	{ netanalyzer_if_print, DLT_NETANALYZER },
#endif
#ifdef DLT_NETANALYZER_TRANSPARENT
	{ netanalyzer_transparent_if_print, DLT_NETANALYZER_TRANSPARENT },
#endif
#if defined(DLT_NFLOG) && defined(HAVE_PCAP_NFLOG_H)
	{ nflog_if_print,	DLT_NFLOG},
#endif
	{ NULL,			0 },
};

if_printer
lookup_printer(int type)
{
	struct printer *p;

	for (p = printers; p->f; ++p)
		if (type == p->type)
			return p->f;

	return NULL;
	
}

if_ndo_printer
lookup_ndo_printer(int type)
{
	struct ndo_printer *p;

	for (p = ndo_printers; p->f; ++p)
		if (type == p->type)
			return p->f;

	return NULL;
	
}

static pcap_t *pd;

static int supports_monitor_mode;

extern int optind;
extern int opterr;
extern char *optarg;

struct print_info {
        netdissect_options *ndo;
        union {
                if_printer     printer;
                if_ndo_printer ndo_printer;
        } p;
        int ndo_type;
};

struct dump_info {
	char	*WFileName;
	char	*CurrentFileName;
	pcap_t	*pd;
	pcap_dumper_t *p;
};

#ifdef HAVE_PCAP_SET_TSTAMP_TYPE
static void
show_tstamp_types_and_exit(const char *device, pcap_t *pd)
{
	int n_tstamp_types;
	int *tstamp_types = 0;
	const char *tstamp_type_name;
	int i;

	n_tstamp_types = pcap_list_tstamp_types(pd, &tstamp_types);
	if (n_tstamp_types < 0)
		error("%s", pcap_geterr(pd));

	if (n_tstamp_types == 0) {
		fprintf(stderr, "Time stamp type cannot be set for %s\n",
		    device);
		exit(0);
	}
	fprintf(stderr, "Time stamp types for %s (use option -j to set):\n",
	    device);
	for (i = 0; i < n_tstamp_types; i++) {
		tstamp_type_name = pcap_tstamp_type_val_to_name(tstamp_types[i]);
		if (tstamp_type_name != NULL) {
			(void) fprintf(stderr, "  %s (%s)\n", tstamp_type_name,
			    pcap_tstamp_type_val_to_description(tstamp_types[i]));
		} else {
			(void) fprintf(stderr, "  %d\n", tstamp_types[i]);
		}
	}
	pcap_free_tstamp_types(tstamp_types);
	exit(0);
}
#endif

static void
show_dlts_and_exit(const char *device, pcap_t *pd)
{
	int n_dlts;
	int *dlts = 0;
	const char *dlt_name;

	n_dlts = pcap_list_datalinks(pd, &dlts);
	if (n_dlts < 0)
		error("%s", pcap_geterr(pd));
	else if (n_dlts == 0 || !dlts)
		error("No data link types.");

	if (supports_monitor_mode)
		(void) fprintf(stderr, "Data link types for %s %s (use option -y to set):\n",
		    device,
		    Iflag ? "when in monitor mode" : "when not in monitor mode");
	else
		(void) fprintf(stderr, "Data link types for %s (use option -y to set):\n",
		    device);

	while (--n_dlts >= 0) {
		dlt_name = pcap_datalink_val_to_name(dlts[n_dlts]);
		if (dlt_name != NULL) {
			(void) fprintf(stderr, "  %s (%s)", dlt_name,
			    pcap_datalink_val_to_description(dlts[n_dlts]));

			if (lookup_printer(dlts[n_dlts]) == NULL
                            && lookup_ndo_printer(dlts[n_dlts]) == NULL)
				(void) fprintf(stderr, " (printing not supported)");
			fprintf(stderr, "\n");
		} else {
			(void) fprintf(stderr, "  DLT %d (printing not supported)\n",
			    dlts[n_dlts]);
		}
	}
#ifdef HAVE_PCAP_FREE_DATALINKS
	pcap_free_datalinks(dlts);
#endif
	exit(0);
}

#if defined(HAVE_PCAP_CREATE) || defined(WIN32)
#define B_FLAG		"B:"
#define B_FLAG_USAGE	" [ -B size ]"
#else 
#define B_FLAG
#define B_FLAG_USAGE
#endif 

#ifdef HAVE_PCAP_CREATE
#define I_FLAG		"I"
#else 
#define I_FLAG
#endif 

#ifdef HAVE_PCAP_SET_TSTAMP_TYPE
#define j_FLAG		"j:"
#define j_FLAG_USAGE	" [ -j tstamptype ]"
#define J_FLAG		"J"
#else 
#define j_FLAG
#define j_FLAG_USAGE
#define J_FLAG
#endif 

#ifdef HAVE_PCAP_FINDALLDEVS
#ifndef HAVE_PCAP_IF_T
#undef HAVE_PCAP_FINDALLDEVS
#endif
#endif

#ifdef HAVE_PCAP_FINDALLDEVS
#define D_FLAG	"D"
#else
#define D_FLAG
#endif

#ifdef HAVE_PCAP_DUMP_FLUSH
#define U_FLAG	"U"
#else
#define U_FLAG
#endif

#ifdef HAVE_PCAP_SETDIRECTION
#define P_FLAG "P:"
#define Q_FLAG "Q:"
#else
#define P_FLAG
#define Q_FLAG
#endif

#ifndef WIN32
static void
droproot(const char *username, const char *chroot_dir)
{
	struct passwd *pw = NULL;

	if (chroot_dir && !username) {
		fprintf(stderr, "tcpdump: Chroot without dropping root is insecure\n");
		exit(1);
	}
	
	pw = getpwnam(username);
	if (pw) {
		if (chroot_dir) {
			if (chroot(chroot_dir) != 0 || chdir ("/") != 0) {
				fprintf(stderr, "tcpdump: Couldn't chroot/chdir to '%.64s': %s\n",
				    chroot_dir, pcap_strerror(errno));
				exit(1);
			}
		}
#ifdef HAVE_CAP_NG_H
		int ret = capng_change_id(pw->pw_uid, pw->pw_gid, CAPNG_NO_FLAG);
		if (ret < 0) {
			printf("error : ret %d\n", ret);
		}
		
		capng_update(CAPNG_DROP, CAPNG_EFFECTIVE, CAP_SETUID);
		capng_update(CAPNG_DROP, CAPNG_EFFECTIVE, CAP_SETUID);
		capng_update(CAPNG_DROP, CAPNG_PERMITTED, CAP_SETUID);
		capng_update(CAPNG_DROP, CAPNG_PERMITTED, CAP_SETUID);
		capng_apply(CAPNG_SELECT_BOTH);

#else
		if (initgroups(pw->pw_name, pw->pw_gid) != 0 ||
		    setgid(pw->pw_gid) != 0 || setuid(pw->pw_uid) != 0) {
			fprintf(stderr, "tcpdump: Couldn't change to '%.32s' uid=%lu gid=%lu: %s\n",
			    username, 
			    (unsigned long)pw->pw_uid,
			    (unsigned long)pw->pw_gid,
			    pcap_strerror(errno));
			exit(1);
		}
#endif 
	}
	else {
		fprintf(stderr, "tcpdump: Couldn't find user '%.32s'\n",
		    username);
		exit(1);
	}
}
#endif 

static int
getWflagChars(int x)
{
	int c = 0;

	x -= 1;
	while (x > 0) {
		c += 1;
		x /= 10;
	}

	return c;
}


static void
MakeFilename(char *buffer, char *orig_name, int cnt, int max_chars)
{
        char *filename = malloc(PATH_MAX + 1);
        if (filename == NULL)
            error("Makefilename: malloc");

        
        if (Gflag != 0) {
          struct tm *local_tm;

          
          if ((local_tm = localtime(&Gflag_time)) == NULL) {
                  error("MakeTimedFilename: localtime");
          }

          strftime(filename, PATH_MAX, orig_name, local_tm);
        } else {
          strncpy(filename, orig_name, PATH_MAX);
        }

	if (cnt == 0 && max_chars == 0)
		strncpy(buffer, filename, PATH_MAX + 1);
	else
		if (snprintf(buffer, PATH_MAX + 1, "%s%0*d", filename, max_chars, cnt) > PATH_MAX)
                  
                  error("too many output files or filename is too long (> %d)", PATH_MAX);
        free(filename);
}

static int tcpdump_printf(netdissect_options *ndo _U_,
			  const char *fmt, ...)
{
  
  va_list args;
  int ret;

  va_start(args, fmt);
  ret=vfprintf(stdout, fmt, args);
  va_end(args);

  return ret;
}

static struct print_info
get_print_info(int type)
{
	struct print_info printinfo;

	printinfo.ndo_type = 1;
	printinfo.ndo = gndo;
	printinfo.p.ndo_printer = lookup_ndo_printer(type);
	if (printinfo.p.ndo_printer == NULL) {
		printinfo.p.printer = lookup_printer(type);
		printinfo.ndo_type = 0;
		if (printinfo.p.printer == NULL) {
			gndo->ndo_dltname = pcap_datalink_val_to_name(type);
			if (gndo->ndo_dltname != NULL)
				error("packet printing is not supported for link type %s: use -w",
				      gndo->ndo_dltname);
			else
				error("packet printing is not supported for link type %d: use -w", type);
		}
	}
	return (printinfo);
}

static char *
get_next_file(FILE *VFile, char *ptr)
{
	char *ret;

	ret = fgets(ptr, PATH_MAX, VFile);
	if (!ret)
		return NULL;

	if (ptr[strlen(ptr) - 1] == '\n')
		ptr[strlen(ptr) - 1] = '\0';

	return ret;
}

int
main(int argc, char **argv)
{
	register int cnt, op, i;
	bpf_u_int32 localnet, netmask;
	register char *cp, *infile, *cmdbuf, *device, *RFileName, *VFileName, *WFileName;
	pcap_handler callback;
	int type;
	int dlt;
	int new_dlt;
	const char *dlt_name;
	struct bpf_program fcode;
#ifndef WIN32
	RETSIGTYPE (*oldhandler)(int);
#endif
	struct print_info printinfo;
	struct dump_info dumpinfo;
	u_char *pcap_userdata;
	char ebuf[PCAP_ERRBUF_SIZE];
	char VFileLine[PATH_MAX + 1];
	char *username = NULL;
	char *chroot_dir = NULL;
	char *ret = NULL;
	char *end;
#ifdef HAVE_PCAP_FINDALLDEVS
	pcap_if_t *devpointer;
	int devnum;
#endif
	int status;
	FILE *VFile;
#ifdef WIN32
	if(wsockinit() != 0) return 1;
#endif 

	jflag=-1;	
        gndo->ndo_Oflag=1;
	gndo->ndo_Rflag=1;
	gndo->ndo_dlt=-1;
	gndo->ndo_default_print=ndo_default_print;
	gndo->ndo_printf=tcpdump_printf;
	gndo->ndo_error=ndo_error;
	gndo->ndo_warning=ndo_warning;
	gndo->ndo_snaplen = DEFAULT_SNAPLEN;
  
	cnt = -1;
	device = NULL;
	infile = NULL;
	RFileName = NULL;
	VFileName = NULL;
	VFile = NULL;
	WFileName = NULL;
	dlt = -1;
	if ((cp = strrchr(argv[0], '/')) != NULL)
		program_name = cp + 1;
	else
		program_name = argv[0];

	if (abort_on_misalignment(ebuf, sizeof(ebuf)) < 0)
		error("%s", ebuf);

#ifdef LIBSMI
	smiInit("tcpdump");
#endif

	while (
	    (op = getopt(argc, argv, "aAb" B_FLAG "c:C:d" D_FLAG "eE:fF:G:hHi:" I_FLAG j_FLAG J_FLAG "KlLm:M:nNOp" P_FLAG "q" Q_FLAG "r:Rs:StT:u" U_FLAG "vV:w:W:xXy:Yz:Z:")) != -1)
		switch (op) {

		case 'a':
			
			break;

		case 'A':
			++Aflag;
			break;

		case 'b':
			++bflag;
			break;

#if defined(HAVE_PCAP_CREATE) || defined(WIN32)
		case 'B':
			Bflag = atoi(optarg)*1024;
			if (Bflag <= 0)
				error("invalid packet buffer size %s", optarg);
			break;
#endif 

		case 'c':
			cnt = atoi(optarg);
			if (cnt <= 0)
				error("invalid packet count %s", optarg);
			break;

		case 'C':
			Cflag = atoi(optarg) * 1000000;
			if (Cflag < 0)
				error("invalid file size %s", optarg);
			break;

		case 'd':
			++dflag;
			break;

#ifdef HAVE_PCAP_FINDALLDEVS
		case 'D':
			if (pcap_findalldevs(&devpointer, ebuf) < 0)
				error("%s", ebuf);
			else {
				for (i = 0; devpointer != 0; i++) {
					printf("%d.%s", i+1, devpointer->name);
					if (devpointer->description != NULL)
						printf(" (%s)", devpointer->description);
					printf("\n");
					devpointer = devpointer->next;
				}
			}
			return 0;
#endif 

		case 'L':
			Lflag++;
			break;

		case 'e':
			++eflag;
			break;

		case 'E':
#ifndef HAVE_LIBCRYPTO
			warning("crypto code not compiled in");
#endif
			gndo->ndo_espsecret = optarg;
			break;

		case 'f':
			++fflag;
			break;

		case 'F':
			infile = optarg;
			break;

		case 'G':
			Gflag = atoi(optarg);
			if (Gflag < 0)
				error("invalid number of seconds %s", optarg);

                        
                        Gflag_count = 0;

			
			if ((Gflag_time = time(NULL)) == (time_t)-1) {
				error("main: can't get current time: %s",
				    pcap_strerror(errno));
			}
			break;

		case 'h':
			usage();
			break;

		case 'H':
			++Hflag;
			break;

		case 'i':
			if (optarg[0] == '0' && optarg[1] == 0)
				error("Invalid adapter index");
			
#ifdef HAVE_PCAP_FINDALLDEVS
			devnum = strtol(optarg, &end, 10);
			if (optarg != end && *end == '\0') {
				if (devnum < 0)
					error("Invalid adapter index");

				if (pcap_findalldevs(&devpointer, ebuf) < 0)
					error("%s", ebuf);
				else {
					for (i = 0;
					    i < devnum-1 && devpointer != NULL;
					    i++, devpointer = devpointer->next)
						;
					if (devpointer == NULL)
						error("Invalid adapter index");
				}
				device = devpointer->name;
				break;
			}
#endif 
			device = optarg;
			break;

#ifdef HAVE_PCAP_CREATE
		case 'I':
			++Iflag;
			break;
#endif 

#ifdef HAVE_PCAP_SET_TSTAMP_TYPE
		case 'j':
			jflag = pcap_tstamp_type_name_to_val(optarg);
			if (jflag < 0)
				error("invalid time stamp type %s", optarg);
			break;

		case 'J':
			Jflag++;
			break;
#endif

		case 'l':
#ifdef WIN32
			setvbuf(stdout, NULL, _IONBF, 0);
#else 
#ifdef HAVE_SETLINEBUF
			setlinebuf(stdout);
#else
			setvbuf(stdout, NULL, _IOLBF, 0);
#endif
#endif 
			break;

		case 'K':
			++Kflag;
			break;

		case 'm':
#ifdef LIBSMI
			if (smiLoadModule(optarg) == 0) {
				error("could not load MIB module %s", optarg);
			}
			sflag = 1;
#else
			(void)fprintf(stderr, "%s: ignoring option `-m %s' ",
				      program_name, optarg);
			(void)fprintf(stderr, "(no libsmi support)\n");
#endif
			break;

		case 'M':
			
#ifndef HAVE_LIBCRYPTO
			warning("crypto code not compiled in");
#endif
			sigsecret = optarg;
			break;

		case 'n':
			++nflag;
			break;

		case 'N':
			++Nflag;
			break;

		case 'O':
			Oflag = 0;
			break;

		case 'p':
			++pflag;
			break;
#ifdef HAVE_PCAP_SETDIRECTION
		case 'P':
			warning("don't use -P, use -Q; -P will be used for pcap-ng output in the future");
			

		case 'Q':
			if (strcasecmp(optarg, "in") == 0)
				Pflag = PCAP_D_IN;
			else if (strcasecmp(optarg, "out") == 0)
				Pflag = PCAP_D_OUT;
			else if (strcasecmp(optarg, "inout") == 0)
				Pflag = PCAP_D_INOUT;
			else
				error("unknown capture direction `%s'", optarg);
			break;
#endif 
		case 'q':
			++qflag;
			++suppress_default_print;
			break;

		case 'r':
			RFileName = optarg;
			break;

		case 'R':
			Rflag = 0;
			break;

		case 's':
			snaplen = strtol(optarg, &end, 0);
			if (optarg == end || *end != '\0'
			    || snaplen < 0 || snaplen > MAXIMUM_SNAPLEN)
				error("invalid snaplen %s", optarg);
			else if (snaplen == 0)
				snaplen = MAXIMUM_SNAPLEN;
			break;

		case 'S':
			++Sflag;
			break;

		case 't':
			++tflag;
			break;

		case 'T':
			if (strcasecmp(optarg, "vat") == 0)
				packettype = PT_VAT;
			else if (strcasecmp(optarg, "wb") == 0)
				packettype = PT_WB;
			else if (strcasecmp(optarg, "rpc") == 0)
				packettype = PT_RPC;
			else if (strcasecmp(optarg, "rtp") == 0)
				packettype = PT_RTP;
			else if (strcasecmp(optarg, "rtcp") == 0)
				packettype = PT_RTCP;
			else if (strcasecmp(optarg, "snmp") == 0)
				packettype = PT_SNMP;
			else if (strcasecmp(optarg, "cnfp") == 0)
				packettype = PT_CNFP;
			else if (strcasecmp(optarg, "tftp") == 0)
				packettype = PT_TFTP;
			else if (strcasecmp(optarg, "aodv") == 0)
				packettype = PT_AODV;
			else if (strcasecmp(optarg, "carp") == 0)
				packettype = PT_CARP;
			else if (strcasecmp(optarg, "radius") == 0)
				packettype = PT_RADIUS;
			else if (strcasecmp(optarg, "zmtp1") == 0)
				packettype = PT_ZMTP1;
			else if (strcasecmp(optarg, "vxlan") == 0)
				packettype = PT_VXLAN;
			else if (strcasecmp(optarg, "pgm") == 0)
				packettype = PT_PGM;
			else if (strcasecmp(optarg, "pgm_zmtp1") == 0)
				packettype = PT_PGM_ZMTP1;
			else if (strcasecmp(optarg, "lmp") == 0)
				packettype = PT_LMP;
			else
				error("unknown packet type `%s'", optarg);
			break;

		case 'u':
			++uflag;
			break;

#ifdef HAVE_PCAP_DUMP_FLUSH
		case 'U':
			++Uflag;
			break;
#endif

		case 'v':
			++vflag;
			break;

		case 'V':
			VFileName = optarg;
			break;

		case 'w':
			WFileName = optarg;
			break;

		case 'W':
			Wflag = atoi(optarg);
			if (Wflag < 0) 
				error("invalid number of output files %s", optarg);
			WflagChars = getWflagChars(Wflag);
			break;

		case 'x':
			++xflag;
			++suppress_default_print;
			break;

		case 'X':
			++Xflag;
			++suppress_default_print;
			break;

		case 'y':
			gndo->ndo_dltname = optarg;
			gndo->ndo_dlt =
			  pcap_datalink_name_to_val(gndo->ndo_dltname);
			if (gndo->ndo_dlt < 0)
				error("invalid data link type %s", gndo->ndo_dltname);
			break;

#if defined(HAVE_PCAP_DEBUG) || defined(HAVE_YYDEBUG)
		case 'Y':
			{
			
#ifdef HAVE_PCAP_DEBUG
			extern int pcap_debug;
			pcap_debug = 1;
#else
			extern int yydebug;
			yydebug = 1;
#endif
			}
			break;
#endif
		case 'z':
			if (optarg) {
				zflag = strdup(optarg);
			} else {
				usage();
				
			}
			break;

		case 'Z':
			if (optarg) {
				username = strdup(optarg);
			}
			else {
				usage();
				
			}
			break;

		default:
			usage();
			
		}

	switch (tflag) {

	case 0: 
	case 4: 
		thiszone = gmt2local(0);
		break;

	case 1: 
	case 2: 
	case 3: 
        case 5: 
		break;

	default: 
		error("only -t, -tt, -ttt, -tttt and -ttttt are supported");
		break;
	}

	if (fflag != 0 && (VFileName != NULL || RFileName != NULL))
		error("-f can not be used with -V or -r");

	if (VFileName != NULL && RFileName != NULL)
		error("-V and -r are mutually exclusive.");

#ifdef WITH_CHROOT
	
	if (getuid() == 0 || geteuid() == 0) {
		
		if (!chroot_dir)
			chroot_dir = WITH_CHROOT;
	}
#endif

#ifdef WITH_USER
	
	if (getuid() == 0 || geteuid() == 0) {
		 
		if (!username)
			username = WITH_USER;
	}
#endif

	if (RFileName != NULL || VFileName != NULL) {
#ifndef WIN32
		if (setgid(getgid()) != 0 || setuid(getuid()) != 0 )
			fprintf(stderr, "Warning: setgid/setuid failed !\n");
#endif 
		if (VFileName != NULL) {
			if (VFileName[0] == '-' && VFileName[1] == '\0')
				VFile = stdin;
			else
				VFile = fopen(VFileName, "r");

			if (VFile == NULL)
				error("Unable to open file: %s\n", strerror(errno));

			ret = get_next_file(VFile, VFileLine);
			if (!ret)
				error("Nothing in %s\n", VFileName);
			RFileName = VFileLine;
		}

		pd = pcap_open_offline(RFileName, ebuf);
		if (pd == NULL)
			error("%s", ebuf);
		dlt = pcap_datalink(pd);
		dlt_name = pcap_datalink_val_to_name(dlt);
		if (dlt_name == NULL) {
			fprintf(stderr, "reading from file %s, link-type %u\n",
			    RFileName, dlt);
		} else {
			fprintf(stderr,
			    "reading from file %s, link-type %s (%s)\n",
			    RFileName, dlt_name,
			    pcap_datalink_val_to_description(dlt));
		}
		localnet = 0;
		netmask = 0;
	} else {
		if (device == NULL) {
			device = pcap_lookupdev(ebuf);
			if (device == NULL)
				error("%s", ebuf);
		}
#ifdef WIN32
		if(strlen(device) == 1)	
		{						
			fprintf(stderr, "%s: listening on %ws\n", program_name, device);
		}
		else
		{
			fprintf(stderr, "%s: listening on %s\n", program_name, device);
		}

		fflush(stderr);	
#endif 
#ifdef HAVE_PCAP_CREATE
		pd = pcap_create(device, ebuf);
		if (pd == NULL)
			error("%s", ebuf);
#ifdef HAVE_PCAP_SET_TSTAMP_TYPE
		if (Jflag)
			show_tstamp_types_and_exit(device, pd);
#endif
		if (pcap_can_set_rfmon(pd) == 1)
			supports_monitor_mode = 1;
		else
			supports_monitor_mode = 0;
		status = pcap_set_snaplen(pd, snaplen);
		if (status != 0)
			error("%s: Can't set snapshot length: %s",
			    device, pcap_statustostr(status));
		status = pcap_set_promisc(pd, !pflag);
		if (status != 0)
			error("%s: Can't set promiscuous mode: %s",
			    device, pcap_statustostr(status));
		if (Iflag) {
			status = pcap_set_rfmon(pd, 1);
			if (status != 0)
				error("%s: Can't set monitor mode: %s",
				    device, pcap_statustostr(status));
		}
		status = pcap_set_timeout(pd, 1000);
		if (status != 0)
			error("%s: pcap_set_timeout failed: %s",
			    device, pcap_statustostr(status));
		if (Bflag != 0) {
			status = pcap_set_buffer_size(pd, Bflag);
			if (status != 0)
				error("%s: Can't set buffer size: %s",
				    device, pcap_statustostr(status));
		}
#ifdef HAVE_PCAP_SET_TSTAMP_TYPE
                if (jflag != -1) {
			status = pcap_set_tstamp_type(pd, jflag);
			if (status < 0)
				error("%s: Can't set time stamp type: %s",
			    	    device, pcap_statustostr(status));
		}
#endif
		status = pcap_activate(pd);
		if (status < 0) {
			cp = pcap_geterr(pd);
			if (status == PCAP_ERROR)
				error("%s", cp);
			else if ((status == PCAP_ERROR_NO_SUCH_DEVICE ||
			          status == PCAP_ERROR_PERM_DENIED) &&
			         *cp != '\0')
				error("%s: %s\n(%s)", device,
				    pcap_statustostr(status), cp);
			else
				error("%s: %s", device,
				    pcap_statustostr(status));
		} else if (status > 0) {
			cp = pcap_geterr(pd);
			if (status == PCAP_WARNING)
				warning("%s", cp);
			else if (status == PCAP_WARNING_PROMISC_NOTSUP &&
			         *cp != '\0')
				warning("%s: %s\n(%s)", device,
				    pcap_statustostr(status), cp);
			else
				warning("%s: %s", device,
				    pcap_statustostr(status));
		}
#ifdef HAVE_PCAP_SETDIRECTION
		if (Pflag != -1) {
			status = pcap_setdirection(pd, Pflag);
			if (status != 0)
				error("%s: pcap_setdirection() failed: %s",
				      device,  pcap_geterr(pd));
		}
#endif
#else
		*ebuf = '\0';
		pd = pcap_open_live(device, snaplen, !pflag, 1000, ebuf);
		if (pd == NULL)
			error("%s", ebuf);
		else if (*ebuf)
			warning("%s", ebuf);
#endif 
#ifndef WIN32
		if (setgid(getgid()) != 0 || setuid(getuid()) != 0)
			fprintf(stderr, "Warning: setgid/setuid failed !\n");
#endif 
#if !defined(HAVE_PCAP_CREATE) && defined(WIN32)
		if(Bflag != 0)
			if(pcap_setbuff(pd, Bflag)==-1){
				error("%s", pcap_geterr(pd));
			}
#endif 
		if (Lflag)
			show_dlts_and_exit(device, pd);
		if (gndo->ndo_dlt >= 0) {
#ifdef HAVE_PCAP_SET_DATALINK
			if (pcap_set_datalink(pd, gndo->ndo_dlt) < 0)
				error("%s", pcap_geterr(pd));
#else
			if (gndo->ndo_dlt != pcap_datalink(pd)) {
				error("%s is not one of the DLTs supported by this device\n",
				      gndo->ndo_dltname);
			}
#endif
			(void)fprintf(stderr, "%s: data link type %s\n",
				      program_name, gndo->ndo_dltname);
			(void)fflush(stderr);
		}
		i = pcap_snapshot(pd);
		if (snaplen < i) {
			warning("snaplen raised from %d to %d", snaplen, i);
			snaplen = i;
		}
		if (pcap_lookupnet(device, &localnet, &netmask, ebuf) < 0) {
			localnet = 0;
			netmask = 0;
			warning("%s", ebuf);
		}
	}
	if (infile)
		cmdbuf = read_infile(infile);
	else
		cmdbuf = copy_argv(&argv[optind]);

	if (pcap_compile(pd, &fcode, cmdbuf, Oflag, netmask) < 0)
		error("%s", pcap_geterr(pd));
	if (dflag) {
		bpf_dump(&fcode, dflag);
		pcap_close(pd);
		free(cmdbuf);
		exit(0);
	}
	init_addrtoname(localnet, netmask);
        init_checksum();

#ifndef WIN32	
	(void)setsignal(SIGPIPE, cleanup);
	(void)setsignal(SIGTERM, cleanup);
	(void)setsignal(SIGINT, cleanup);
#endif 
#if defined(HAVE_FORK) || defined(HAVE_VFORK)
	(void)setsignal(SIGCHLD, child_cleanup);
#endif
	
#ifndef WIN32	
	if ((oldhandler = setsignal(SIGHUP, cleanup)) != SIG_DFL)
		(void)setsignal(SIGHUP, oldhandler);
#endif 

#ifndef WIN32

#ifdef HAVE_CAP_NG_H
	
	if ((getuid() == 0 || geteuid() == 0) && WFileName) {
		if (username) {
			
			capng_clear(CAPNG_EFFECTIVE);
			
			capng_update(CAPNG_ADD, CAPNG_PERMITTED, CAP_SETUID);
			capng_update(CAPNG_ADD, CAPNG_PERMITTED, CAP_SETGID);
			capng_update(CAPNG_ADD, CAPNG_PERMITTED, CAP_DAC_OVERRIDE);

			capng_update(CAPNG_ADD, CAPNG_EFFECTIVE, CAP_SETUID);
			capng_update(CAPNG_ADD, CAPNG_EFFECTIVE, CAP_SETGID);
			capng_update(CAPNG_ADD, CAPNG_EFFECTIVE, CAP_DAC_OVERRIDE);

			capng_apply(CAPNG_SELECT_BOTH);
		}
	}
#endif 

	if (getuid() == 0 || geteuid() == 0) {
		if (username || chroot_dir)
			droproot(username, chroot_dir);

	}
#endif 

	if (pcap_setfilter(pd, &fcode) < 0)
		error("%s", pcap_geterr(pd));
	if (WFileName) {
		pcap_dumper_t *p;
		
		dumpinfo.CurrentFileName = (char *)malloc(PATH_MAX + 1);

		if (dumpinfo.CurrentFileName == NULL)
			error("malloc of dumpinfo.CurrentFileName");

		
		if (Cflag != 0)
		  MakeFilename(dumpinfo.CurrentFileName, WFileName, 0, WflagChars);
		else
		  MakeFilename(dumpinfo.CurrentFileName, WFileName, 0, 0);

		p = pcap_dump_open(pd, dumpinfo.CurrentFileName);
#ifdef HAVE_CAP_NG_H
        
        capng_clear(CAPNG_EFFECTIVE);
#endif
		if (p == NULL)
			error("%s", pcap_geterr(pd));
		if (Cflag != 0 || Gflag != 0) {
			callback = dump_packet_and_trunc;
			dumpinfo.WFileName = WFileName;
			dumpinfo.pd = pd;
			dumpinfo.p = p;
			pcap_userdata = (u_char *)&dumpinfo;
		} else {
			callback = dump_packet;
			pcap_userdata = (u_char *)p;
		}
#ifdef HAVE_PCAP_DUMP_FLUSH
		if (Uflag)
			pcap_dump_flush(p);
#endif
	} else {
		type = pcap_datalink(pd);
		printinfo = get_print_info(type);
		callback = print_packet;
		pcap_userdata = (u_char *)&printinfo;
	}

#ifdef SIGNAL_REQ_INFO
	if (RFileName == NULL)
		(void)setsignal(SIGNAL_REQ_INFO, requestinfo);
#endif

	if (vflag > 0 && WFileName) {
#ifdef USE_WIN32_MM_TIMER
		
		timer_id = timeSetEvent(1000, 100, verbose_stats_dump, 0, TIME_PERIODIC);
		setvbuf(stderr, NULL, _IONBF, 0);
#elif defined(HAVE_ALARM)
		(void)setsignal(SIGALRM, verbose_stats_dump);
		alarm(1);
#endif
	}

#ifndef WIN32
	if (RFileName == NULL) {
		if (!vflag && !WFileName) {
			(void)fprintf(stderr,
			    "%s: verbose output suppressed, use -v or -vv for full protocol decode\n",
			    program_name);
		} else
			(void)fprintf(stderr, "%s: ", program_name);
		dlt = pcap_datalink(pd);
		dlt_name = pcap_datalink_val_to_name(dlt);
		if (dlt_name == NULL) {
			(void)fprintf(stderr, "listening on %s, link-type %u, capture size %u bytes\n",
			    device, dlt, snaplen);
		} else {
			(void)fprintf(stderr, "listening on %s, link-type %s (%s), capture size %u bytes\n",
			    device, dlt_name,
			    pcap_datalink_val_to_description(dlt), snaplen);
		}
		(void)fflush(stderr);
	}
#endif 
	do {
		status = pcap_loop(pd, cnt, callback, pcap_userdata);
		if (WFileName == NULL) {
			if (status == -2) {
				putchar('\n');
			}
			(void)fflush(stdout);
		}
                if (status == -2) {
			VFileName = NULL;
			ret = NULL;
		}
		if (status == -1) {
			(void)fprintf(stderr, "%s: pcap_loop: %s\n",
			    program_name, pcap_geterr(pd));
		}
		if (RFileName == NULL) {
			info(1);
		}
		pcap_close(pd);
		if (VFileName != NULL) {
			ret = get_next_file(VFile, VFileLine);
			if (ret) {
				RFileName = VFileLine;
				pd = pcap_open_offline(RFileName, ebuf);
				if (pd == NULL)
					error("%s", ebuf);
				new_dlt = pcap_datalink(pd);
				if (WFileName && new_dlt != dlt)
					error("%s: new dlt does not match original", RFileName);
				printinfo = get_print_info(new_dlt);
				dlt_name = pcap_datalink_val_to_name(new_dlt);
				if (dlt_name == NULL) {
					fprintf(stderr, "reading from file %s, link-type %u\n",
					RFileName, new_dlt);
				} else {
					fprintf(stderr,
					"reading from file %s, link-type %s (%s)\n",
					RFileName, dlt_name,
					pcap_datalink_val_to_description(new_dlt));
				}
				if (pcap_compile(pd, &fcode, cmdbuf, Oflag, netmask) < 0)
					error("%s", pcap_geterr(pd));
				if (pcap_setfilter(pd, &fcode) < 0)
					error("%s", pcap_geterr(pd));
			}
		}
	}
	while (ret != NULL);

	free(cmdbuf);
	exit(status == -1 ? 1 : 0);
}

static RETSIGTYPE
cleanup(int signo _U_)
{
#ifdef USE_WIN32_MM_TIMER
	if (timer_id)
		timeKillEvent(timer_id);
	timer_id = 0;
#elif defined(HAVE_ALARM)
	alarm(0);
#endif

#ifdef HAVE_PCAP_BREAKLOOP
	pcap_breakloop(pd);
#else
	if (pd != NULL && pcap_file(pd) == NULL) {
		putchar('\n');
		(void)fflush(stdout);
		info(1);
	}
	exit(0);
#endif
}

#if defined(HAVE_FORK) || defined(HAVE_VFORK)
static RETSIGTYPE
child_cleanup(int signo _U_)
{
  wait(NULL);
}
#endif 

static void
info(register int verbose)
{
	struct pcap_stat stat;

	stat.ps_ifdrop = 0;
	if (pcap_stats(pd, &stat) < 0) {
		(void)fprintf(stderr, "pcap_stats: %s\n", pcap_geterr(pd));
		infoprint = 0;
		return;
	}

	if (!verbose)
		fprintf(stderr, "%s: ", program_name);

	(void)fprintf(stderr, "%u packet%s captured", packets_captured,
	    PLURAL_SUFFIX(packets_captured));
	if (!verbose)
		fputs(", ", stderr);
	else
		putc('\n', stderr);
	(void)fprintf(stderr, "%u packet%s received by filter", stat.ps_recv,
	    PLURAL_SUFFIX(stat.ps_recv));
	if (!verbose)
		fputs(", ", stderr);
	else
		putc('\n', stderr);
	(void)fprintf(stderr, "%u packet%s dropped by kernel", stat.ps_drop,
	    PLURAL_SUFFIX(stat.ps_drop));
	if (stat.ps_ifdrop != 0) {
		if (!verbose)
			fputs(", ", stderr);
		else
			putc('\n', stderr);
		(void)fprintf(stderr, "%u packet%s dropped by interface\n",
		    stat.ps_ifdrop, PLURAL_SUFFIX(stat.ps_ifdrop));
	} else
		putc('\n', stderr);
	infoprint = 0;
}

#if defined(HAVE_FORK) || defined(HAVE_VFORK)
static void
compress_savefile(const char *filename)
{
# ifdef HAVE_FORK
	if (fork())
# else
	if (vfork())
# endif
		return;
#ifdef NZERO
	setpriority(PRIO_PROCESS, 0, NZERO - 1);
#else
	setpriority(PRIO_PROCESS, 0, 19);
#endif
	if (execlp(zflag, zflag, filename, (char *)NULL) == -1)
		fprintf(stderr,
			"compress_savefile:execlp(%s, %s): %s\n",
			zflag,
			filename,
			strerror(errno));
# ifdef HAVE_FORK
	exit(1);
# else
	_exit(1);
# endif
}
#else  
static void
compress_savefile(const char *filename)
{
	fprintf(stderr,
		"compress_savefile failed. Functionality not implemented under your system\n");
}
#endif 

static void
dump_packet_and_trunc(u_char *user, const struct pcap_pkthdr *h, const u_char *sp)
{
	struct dump_info *dump_info;

	++packets_captured;

	++infodelay;

	dump_info = (struct dump_info *)user;

	if (Gflag != 0) {
		
		time_t t;

		
		if ((t = time(NULL)) == (time_t)-1) {
			error("dump_and_trunc_packet: can't get current_time: %s",
			    pcap_strerror(errno));
		}


		
		if (t - Gflag_time >= Gflag) {
			
			Gflag_time = t;
			
			Gflag_count++;
			pcap_dump_close(dump_info->p);

			if (zflag != NULL)
				compress_savefile(dump_info->CurrentFileName);

			if (Cflag == 0 && Wflag > 0 && Gflag_count >= Wflag) {
				(void)fprintf(stderr, "Maximum file limit reached: %d\n",
				    Wflag);
				exit(0);
				
			}
			if (dump_info->CurrentFileName != NULL)
				free(dump_info->CurrentFileName);
			
			dump_info->CurrentFileName = (char *)malloc(PATH_MAX + 1);
			if (dump_info->CurrentFileName == NULL)
				error("dump_packet_and_trunc: malloc");
			if (Cflag != 0)
				MakeFilename(dump_info->CurrentFileName, dump_info->WFileName, 0,
				    WflagChars);
			else
				MakeFilename(dump_info->CurrentFileName, dump_info->WFileName, 0, 0);

#ifdef HAVE_CAP_NG_H
			capng_update(CAPNG_ADD, CAPNG_EFFECTIVE, CAP_DAC_OVERRIDE);
			capng_apply(CAPNG_EFFECTIVE);
#endif 
			dump_info->p = pcap_dump_open(dump_info->pd, dump_info->CurrentFileName);
#ifdef HAVE_CAP_NG_H
			capng_update(CAPNG_DROP, CAPNG_EFFECTIVE, CAP_DAC_OVERRIDE);
			capng_apply(CAPNG_EFFECTIVE);
#endif 
			if (dump_info->p == NULL)
				error("%s", pcap_geterr(pd));
		}
	}

	/*
	 * XXX - this won't prevent capture files from getting
	 * larger than Cflag - the last packet written to the
	 * file could put it over Cflag.
	 */
	if (Cflag != 0 && pcap_dump_ftell(dump_info->p) > Cflag) {
		pcap_dump_close(dump_info->p);

		if (zflag != NULL)
			compress_savefile(dump_info->CurrentFileName);

		Cflag_count++;
		if (Wflag > 0) {
			if (Cflag_count >= Wflag)
				Cflag_count = 0;
		}
		if (dump_info->CurrentFileName != NULL)
			free(dump_info->CurrentFileName);
		dump_info->CurrentFileName = (char *)malloc(PATH_MAX + 1);
		if (dump_info->CurrentFileName == NULL)
			error("dump_packet_and_trunc: malloc");
		MakeFilename(dump_info->CurrentFileName, dump_info->WFileName, Cflag_count, WflagChars);
		dump_info->p = pcap_dump_open(dump_info->pd, dump_info->CurrentFileName);
		if (dump_info->p == NULL)
			error("%s", pcap_geterr(pd));
	}

	pcap_dump((u_char *)dump_info->p, h, sp);
#ifdef HAVE_PCAP_DUMP_FLUSH
	if (Uflag)
		pcap_dump_flush(dump_info->p);
#endif

	--infodelay;
	if (infoprint)
		info(0);
}

static void
dump_packet(u_char *user, const struct pcap_pkthdr *h, const u_char *sp)
{
	++packets_captured;

	++infodelay;

	pcap_dump(user, h, sp);
#ifdef HAVE_PCAP_DUMP_FLUSH
	if (Uflag)
		pcap_dump_flush((pcap_dumper_t *)user);
#endif

	--infodelay;
	if (infoprint)
		info(0);
}

static void
print_packet(u_char *user, const struct pcap_pkthdr *h, const u_char *sp)
{
	struct print_info *print_info;
	u_int hdrlen;

	++packets_captured;

	++infodelay;
	ts_print(&h->ts);

	print_info = (struct print_info *)user;

	snapend = sp + h->caplen;

        if(print_info->ndo_type) {
                hdrlen = (*print_info->p.ndo_printer)(print_info->ndo, h, sp);
        } else {
                hdrlen = (*print_info->p.printer)(h, sp);
        }
                
	if (Xflag) {
		if (Xflag > 1) {
			hex_and_ascii_print("\n\t", sp, h->caplen);
		} else {
			if (h->caplen > hdrlen)
				hex_and_ascii_print("\n\t", sp + hdrlen,
				    h->caplen - hdrlen);
		}
	} else if (xflag) {
		if (xflag > 1) {
			hex_print("\n\t", sp, h->caplen);
		} else {
			if (h->caplen > hdrlen)
				hex_print("\n\t", sp + hdrlen,
				    h->caplen - hdrlen);
		}
	} else if (Aflag) {
		if (Aflag > 1) {
			ascii_print(sp, h->caplen);
		} else {
			if (h->caplen > hdrlen)
				ascii_print(sp + hdrlen, h->caplen - hdrlen);
		}
	}

	putchar('\n');

	--infodelay;
	if (infoprint)
		info(0);
}

#ifdef WIN32
	char WDversion[]="current-cvs.tcpdump.org";
#if !defined(HAVE_GENERATED_VERSION)
	char version[]="current-cvs.tcpdump.org";
#endif
	char pcap_version[]="current-cvs.tcpdump.org";
	char Wpcap_version[]="3.1";
#endif

static void
ndo_default_print(netdissect_options *ndo _U_, const u_char *bp, u_int length)
{
	hex_and_ascii_print("\n\t", bp, length); 
}

void
default_print(const u_char *bp, u_int length)
{
	ndo_default_print(gndo, bp, length);
}

#ifdef SIGNAL_REQ_INFO
RETSIGTYPE requestinfo(int signo _U_)
{
	if (infodelay)
		++infoprint;
	else
		info(0);
}
#endif

#ifdef USE_WIN32_MM_TIMER
void CALLBACK verbose_stats_dump (UINT timer_id _U_, UINT msg _U_, DWORD_PTR arg _U_,
				  DWORD_PTR dw1 _U_, DWORD_PTR dw2 _U_)
{
	struct pcap_stat stat;

	if (infodelay == 0 && pcap_stats(pd, &stat) >= 0)
		fprintf(stderr, "Got %u\r", packets_captured);
}
#elif defined(HAVE_ALARM)
static void verbose_stats_dump(int sig _U_)
{
	struct pcap_stat stat;

	if (infodelay == 0 && pcap_stats(pd, &stat) >= 0)
		fprintf(stderr, "Got %u\r", packets_captured);
	alarm(1);
}
#endif

static void
usage(void)
{
	extern char version[];
#ifndef HAVE_PCAP_LIB_VERSION
#if defined(WIN32) || defined(HAVE_PCAP_VERSION)
	extern char pcap_version[];
#else 
	static char pcap_version[] = "unknown";
#endif 
#endif 

#ifdef HAVE_PCAP_LIB_VERSION
#ifdef WIN32
	(void)fprintf(stderr, "%s version %s, based on tcpdump version %s\n", program_name, WDversion, version);
#else 
	(void)fprintf(stderr, "%s version %s\n", program_name, version);
#endif 
	(void)fprintf(stderr, "%s\n",pcap_lib_version());
#else 
#ifdef WIN32
	(void)fprintf(stderr, "%s version %s, based on tcpdump version %s\n", program_name, WDversion, version);
	(void)fprintf(stderr, "WinPcap version %s, based on libpcap version %s\n",Wpcap_version, pcap_version);
#else 
	(void)fprintf(stderr, "%s version %s\n", program_name, version);
	(void)fprintf(stderr, "libpcap version %s\n", pcap_version);
#endif 
#endif 
	(void)fprintf(stderr,
"Usage: %s [-aAbd" D_FLAG "efhH" I_FLAG J_FLAG "KlLnNOpqRStu" U_FLAG "vxX]" B_FLAG_USAGE " [ -c count ]\n", program_name);
	(void)fprintf(stderr,
"\t\t[ -C file_size ] [ -E algo:secret ] [ -F file ] [ -G seconds ]\n");
	(void)fprintf(stderr,
"\t\t[ -i interface ]" j_FLAG_USAGE " [ -M secret ]\n");
#ifdef HAVE_PCAP_SETDIRECTION
	(void)fprintf(stderr,
"\t\t[ -Q in|out|inout ]\n");
#endif
	(void)fprintf(stderr,
"\t\t[ -r file ] [ -s snaplen ] [ -T type ] [ -V file ] [ -w file ]\n");
	(void)fprintf(stderr,
"\t\t[ -W filecount ] [ -y datalinktype ] [ -z command ]\n");
	(void)fprintf(stderr,
"\t\t[ -Z user ] [ expression ]\n");
	exit(1);
}



static void
ndo_error(netdissect_options *ndo _U_, const char *fmt, ...)
{
	va_list ap;

	(void)fprintf(stderr, "%s: ", program_name);
	va_start(ap, fmt);
	(void)vfprintf(stderr, fmt, ap);
	va_end(ap);
	if (*fmt) {
		fmt += strlen(fmt);
		if (fmt[-1] != '\n')
			(void)fputc('\n', stderr);
	}
	exit(1);
	
}

static void
ndo_warning(netdissect_options *ndo _U_, const char *fmt, ...)
{
	va_list ap;

	(void)fprintf(stderr, "%s: WARNING: ", program_name);
	va_start(ap, fmt);
	(void)vfprintf(stderr, fmt, ap);
	va_end(ap);
	if (*fmt) {
		fmt += strlen(fmt);
		if (fmt[-1] != '\n')
			(void)fputc('\n', stderr);
	}
}
