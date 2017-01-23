/*
 * Copyright (c) 1993, 1994, 1995, 1996, 1997
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
 *
 * @(#) $Header: /tcpdump/master/libpcap/pcap1.h,v 1.5 2008-05-30 01:43:21 guy Exp $ (LBL)
 */

#ifndef lib_pcap_h
#define lib_pcap_h

#ifdef WIN32
#include <pcap-stdinc.h>
#else 
#include <sys/types.h>
#include <sys/time.h>
#endif 

#ifndef PCAP_DONT_INCLUDE_PCAP_BPF_H
#include <pcap/bpf.h>
#endif

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PCAP_VERSION_MAJOR 3
#define PCAP_VERSION_MINOR 0

#define PCAP_ERRBUF_SIZE 256

#if BPF_RELEASE - 0 < 199406
typedef	int bpf_int32;
typedef	u_int bpf_u_int32;
#endif

typedef struct pcap pcap_t;
typedef struct pcap_dumper pcap_dumper_t;
typedef struct pcap_if pcap_if_t;
typedef struct pcap_addr pcap_addr_t;


enum pcap1_info_types {
        PCAP_DATACAPTURE,
	PCAP_TIMESTAMP,
	PCAP_WALLTIME,
	PCAP_TIMESKEW,
	PCAP_PROBEPLACE,              
	PCAP_COMMENT,                 
};

struct pcap1_info_container {
	bpf_u_int32 info_len;         
	bpf_u_int32 info_type;        
	unsigned char info_data[0];
};

struct pcap1_info_timestamp {
	struct pcap1_info_container pic;
	bpf_u_int32    nanoseconds;   
	bpf_u_int32    seconds;       
	bpf_u_int16    macroseconds;  
	bpf_u_int16    sigfigs;	      
};	
	
struct pcap1_info_packet {
	struct pcap1_info_container pic;
	bpf_u_int32 caplen;	
	bpf_u_int32 len;	
	bpf_u_int32 linktype;	
	bpf_u_int32 ifIndex;	
	unsigned char packet_data[0];
};	

enum pcap1_probe {
	INBOUND  =1,
	OUTBOUND =2,
	FORWARD  =3,
	PREENCAP =4,
	POSTDECAP=5,
};

struct pcap1_info_probe {
	struct pcap1_info_container pic;
	bpf_u_int32                 probeloc;   
        unsigned char               probe_desc[0];
};
	
struct pcap1_info_comment {
	struct pcap1_info_container pic;
        unsigned char               comment[0];
};
	
struct pcap1_packet_header {
	bpf_u_int32 magic;
	u_short     version_major;
	u_short     version_minor;
        bpf_u_int32 block_len;
	struct pcap1_info_container pics[0];
};


struct pcap_stat {
	u_int ps_recv;		
	u_int ps_drop;		
	u_int ps_ifdrop;	
#ifdef WIN32
	u_int bs_capt;		
#endif 
};

struct pcap_if {
	struct pcap_if *next;
	char *name;		
	char *description;	
	struct pcap_addr *addresses;
	bpf_u_int32 flags;	
};

#define PCAP_IF_LOOPBACK	0x00000001	

struct pcap_addr {
	struct pcap_addr *next;
	struct sockaddr *addr;		
	struct sockaddr *netmask;	
	struct sockaddr *broadaddr;	
	struct sockaddr *dstaddr;	
};

typedef void (*pcap_handler)(u_char *, const struct pcap_pkthdr *,
			     const u_char *);

char	*pcap_lookupdev(char *);
int	pcap_lookupnet(const char *, bpf_u_int32 *, bpf_u_int32 *, char *);
pcap_t	*pcap_open_live(const char *, int, int, int, char *);
pcap_t	*pcap_open_dead(int, int);
pcap_t	*pcap_open_offline(const char *, char *);
void	pcap_close(pcap_t *);
int	pcap_loop(pcap_t *, int, pcap_handler, u_char *);
int	pcap_dispatch(pcap_t *, int, pcap_handler, u_char *);
const u_char*
	pcap_next(pcap_t *, struct pcap_pkthdr *);
int 	pcap_next_ex(pcap_t *, struct pcap_pkthdr **, const u_char **);
void	pcap_breakloop(pcap_t *);
int	pcap_stats(pcap_t *, struct pcap_stat *);
int	pcap_setfilter(pcap_t *, struct bpf_program *);
int	pcap_getnonblock(pcap_t *, char *);
int	pcap_setnonblock(pcap_t *, int, char *);
void	pcap_perror(pcap_t *, char *);
char	*pcap_strerror(int);
char	*pcap_geterr(pcap_t *);
int	pcap_compile(pcap_t *, struct bpf_program *, char *, int,
	    bpf_u_int32);
int	pcap_compile_nopcap(int, int, struct bpf_program *,
	    char *, int, bpf_u_int32);
void	pcap_freecode(struct bpf_program *);
int	pcap_datalink(pcap_t *);
int	pcap_list_datalinks(pcap_t *, int **);
int	pcap_set_datalink(pcap_t *, int);
int	pcap_datalink_name_to_val(const char *);
const char *pcap_datalink_val_to_name(int);
const char *pcap_datalink_val_to_description(int);
int	pcap_snapshot(pcap_t *);
int	pcap_is_swapped(pcap_t *);
int	pcap_major_version(pcap_t *);
int	pcap_minor_version(pcap_t *);

FILE	*pcap_file(pcap_t *);
int	pcap_fileno(pcap_t *);

pcap_dumper_t *pcap_dump_open(pcap_t *, const char *);
int	pcap_dump_flush(pcap_dumper_t *);
void	pcap_dump_close(pcap_dumper_t *);
void	pcap_dump(u_char *, const struct pcap_pkthdr *, const u_char *);
FILE	*pcap_dump_file(pcap_dumper_t *);

int	pcap_findalldevs(pcap_if_t **, char *);
void	pcap_freealldevs(pcap_if_t *);

const char *pcap_lib_version(void);

u_int	bpf_filter(struct bpf_insn *, u_char *, u_int, u_int);
int	bpf_validate(struct bpf_insn *f, int len);
char	*bpf_image(struct bpf_insn *, int);
void	bpf_dump(struct bpf_program *, int);

#ifdef WIN32

int pcap_setbuff(pcap_t *p, int dim);
int pcap_setmode(pcap_t *p, int mode);
int pcap_sendpacket(pcap_t *p, u_char *buf, int size);
int pcap_setmintocopy(pcap_t *p, int size);

#ifdef WPCAP
#include <Win32-Extensions.h>
#endif

#define MODE_CAPT 0
#define MODE_STAT 1

#else

int	pcap_get_selectable_fd(pcap_t *);

#endif 

#ifdef __cplusplus
}
#endif

#endif
