/*
 *  pcap-linux.c: Packet capture interface to the Linux kernel
 *
 *  Copyright (c) 2000 Torsten Landschoff <torsten@debian.org>
 *  		       Sebastian Krahmer  <krahmer@cs.uni-potsdam.de>
 *
 *  License: BSD
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *  3. The names of the authors may not be used to endorse or promote
 *     products derived from this software without specific prior
 *     written permission.
 *
 *  THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 *  IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 *  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 *  Modifications:     Added PACKET_MMAP support
 *                     Paolo Abeni <paolo.abeni@email.it> 
 *                     Added TPACKET_V3 support
 *                     Gabor Tatarka <gabor.tatarka@ericsson.com>
 *                     
 *                     based on previous works of:
 *                     Simon Patarin <patarin@cs.unibo.it>
 *                     Phil Wood <cpw@lanl.gov>
 *
 * Monitor-mode support for mac80211 includes code taken from the iw
 * command; the copyright notice for that code is
 *
 * Copyright (c) 2007, 2008	Johannes Berg
 * Copyright (c) 2007		Andy Lutomirski
 * Copyright (c) 2007		Mike Kershaw
 * Copyright (c) 2008		Gábor Stefanik
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, 
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static const char rcsid[] _U_ =
    "@(#) $Header: /tcpdump/master/libpcap/pcap-linux.c,v 1.164 2008-12-14 22:00:57 guy Exp $ (LBL)";
#endif



#define _GNU_SOURCE

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/utsname.h>
#include <sys/mman.h>
#include <linux/if.h>
#include <linux/if_packet.h>
#include <linux/sockios.h>
#include <netinet/in.h>
#include <linux/if_ether.h>
#include <net/if_arp.h>
#include <poll.h>
#include <dirent.h>

#include "pcap-int.h"
#include "pcap/sll.h"
#include "pcap/vlan.h"

#ifdef PF_PACKET
# include <linux/if_packet.h>

# ifdef PACKET_HOST
#  define HAVE_PF_PACKET_SOCKETS
#  ifdef PACKET_AUXDATA
#   define HAVE_PACKET_AUXDATA
#  endif 
# endif 


# ifdef TPACKET_HDRLEN
#  define HAVE_PACKET_RING
#  ifdef TPACKET3_HDRLEN
#   define HAVE_TPACKET3
#  endif 
#  ifdef TPACKET2_HDRLEN
#   define HAVE_TPACKET2
#  else  
#   define TPACKET_V1	0    
#  endif 
# endif 
#endif 

#ifdef SO_ATTACH_FILTER
#include <linux/types.h>
#include <linux/filter.h>
#endif

#ifdef HAVE_LINUX_NET_TSTAMP_H
#include <linux/net_tstamp.h>
#endif

#ifdef HAVE_LINUX_WIRELESS_H
#include <linux/wireless.h>
#endif 

#ifdef HAVE_LIBNL
#include <linux/nl80211.h>

#include <netlink/genl/genl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/ctrl.h>
#include <netlink/msg.h>
#include <netlink/attr.h>
#endif 

#ifdef HAVE_LINUX_ETHTOOL_H
#include <linux/ethtool.h>
#endif

#ifndef HAVE_SOCKLEN_T
typedef int		socklen_t;
#endif

#ifndef MSG_TRUNC
#define MSG_TRUNC	0x20
#endif

#ifndef SOL_PACKET
#define SOL_PACKET	263
#endif

#define MAX_LINKHEADER_SIZE	256

#define BIGGER_THAN_ALL_MTUS	(64*1024)

struct pcap_linux {
	u_int	packets_read;	
	long	proc_dropped;	
	struct pcap_stat stat;

	char	*device;	
	int	filter_in_userland; 
	int	blocks_to_filter_in_userland;
	int	must_do_on_close; 
	int	timeout;	
	int	sock_packet;	
	int	cooked;		
	int	ifindex;	
	int	lo_ifindex;	
	bpf_u_int32 oldmode;	
	char	*mondevice;	
	u_char	*mmapbuf;	
	size_t	mmapbuflen;	
	int	vlan_offset;	
	u_int	tp_version;	
	u_int	tp_hdrlen;	
	u_char	*oneshot_buffer; 
#ifdef HAVE_TPACKET3
	unsigned char *current_packet; 
	int packets_left; 
#endif
};

#define MUST_CLEAR_PROMISC	0x00000001	
#define MUST_CLEAR_RFMON	0x00000002	
#define MUST_DELETE_MONIF	0x00000004	

static void map_arphrd_to_dlt(pcap_t *, int, int);
#ifdef HAVE_PF_PACKET_SOCKETS
static short int map_packet_type_to_sll_type(short int);
#endif
static int pcap_activate_linux(pcap_t *);
static int activate_old(pcap_t *);
static int activate_new(pcap_t *);
static int activate_mmap(pcap_t *, int *);
static int pcap_can_set_rfmon_linux(pcap_t *);
static int pcap_read_linux(pcap_t *, int, pcap_handler, u_char *);
static int pcap_read_packet(pcap_t *, pcap_handler, u_char *);
static int pcap_inject_linux(pcap_t *, const void *, size_t);
static int pcap_stats_linux(pcap_t *, struct pcap_stat *);
static int pcap_setfilter_linux(pcap_t *, struct bpf_program *);
static int pcap_setdirection_linux(pcap_t *, pcap_direction_t);
static int pcap_set_datalink_linux(pcap_t *, int);
static void pcap_cleanup_linux(pcap_t *);

union thdr {
	struct tpacket_hdr		*h1;
#ifdef HAVE_TPACKET2
	struct tpacket2_hdr		*h2;
#endif
#ifdef HAVE_TPACKET3
	struct tpacket_block_desc	*h3;
#endif
	void				*raw;
};

#ifdef HAVE_PACKET_RING
#define RING_GET_FRAME(h) (((union thdr **)h->buffer)[h->offset])

static void destroy_ring(pcap_t *handle);
static int create_ring(pcap_t *handle, int *status);
static int prepare_tpacket_socket(pcap_t *handle);
static void pcap_cleanup_linux_mmap(pcap_t *);
static int pcap_read_linux_mmap_v1(pcap_t *, int, pcap_handler , u_char *);
#ifdef HAVE_TPACKET2
static int pcap_read_linux_mmap_v2(pcap_t *, int, pcap_handler , u_char *);
#endif
#ifdef HAVE_TPACKET3
static int pcap_read_linux_mmap_v3(pcap_t *, int, pcap_handler , u_char *);
#endif
static int pcap_setfilter_linux_mmap(pcap_t *, struct bpf_program *);
static int pcap_setnonblock_mmap(pcap_t *p, int nonblock, char *errbuf);
static int pcap_getnonblock_mmap(pcap_t *p, char *errbuf);
static void pcap_oneshot_mmap(u_char *user, const struct pcap_pkthdr *h,
    const u_char *bytes);
#endif

#ifdef HAVE_PF_PACKET_SOCKETS
static int	iface_get_id(int fd, const char *device, char *ebuf);
#endif 
static int	iface_get_mtu(int fd, const char *device, char *ebuf);
static int 	iface_get_arptype(int fd, const char *device, char *ebuf);
#ifdef HAVE_PF_PACKET_SOCKETS
static int 	iface_bind(int fd, int ifindex, char *ebuf);
#ifdef IW_MODE_MONITOR
static int	has_wext(int sock_fd, const char *device, char *ebuf);
#endif 
static int	enter_rfmon_mode(pcap_t *handle, int sock_fd,
    const char *device);
#endif 
static int	iface_get_offload(pcap_t *handle);
static int 	iface_bind_old(int fd, const char *device, char *ebuf);

#ifdef SO_ATTACH_FILTER
static int	fix_program(pcap_t *handle, struct sock_fprog *fcode,
    int is_mapped);
static int	fix_offset(struct bpf_insn *p);
static int	set_kernel_filter(pcap_t *handle, struct sock_fprog *fcode);
static int	reset_kernel_filter(pcap_t *handle);

static struct sock_filter	total_insn
	= BPF_STMT(BPF_RET | BPF_K, 0);
static struct sock_fprog	total_fcode
	= { 1, &total_insn };
#endif 

pcap_t *
pcap_create_interface(const char *device, char *ebuf)
{
	pcap_t *handle;

	handle = pcap_create_common(device, ebuf, sizeof (struct pcap_linux));
	if (handle == NULL)
		return NULL;

	handle->activate_op = pcap_activate_linux;
	handle->can_set_rfmon_op = pcap_can_set_rfmon_linux;
#if defined(HAVE_LINUX_NET_TSTAMP_H) && defined(PACKET_TIMESTAMP)
	handle->tstamp_type_count = 3;
	handle->tstamp_type_list = malloc(3 * sizeof(u_int));
	if (handle->tstamp_type_list == NULL) {
		free(handle);
		return NULL;
	}
	handle->tstamp_type_list[0] = PCAP_TSTAMP_HOST;
	handle->tstamp_type_list[1] = PCAP_TSTAMP_ADAPTER;
	handle->tstamp_type_list[2] = PCAP_TSTAMP_ADAPTER_UNSYNCED;
#endif

#if defined(SIOCGSTAMPNS) && defined(SO_TIMESTAMPNS)
	handle->tstamp_precision_count = 2;
	handle->tstamp_precision_list = malloc(2 * sizeof(u_int));
	if (handle->tstamp_precision_list == NULL) {
		if (handle->tstamp_type_list != NULL)
			free(handle->tstamp_type_list);
		free(handle);
		return NULL;
	}
	handle->tstamp_precision_list[0] = PCAP_TSTAMP_PRECISION_MICRO;
	handle->tstamp_precision_list[1] = PCAP_TSTAMP_PRECISION_NANO;
#endif 

	return handle;
}

#ifdef HAVE_LIBNL

static int
get_mac80211_phydev(pcap_t *handle, const char *device, char *phydev_path,
    size_t phydev_max_pathlen)
{
	char *pathstr;
	ssize_t bytes_read;

	if (asprintf(&pathstr, "/sys/class/net/%s/phy80211", device) == -1) {
		snprintf(handle->errbuf, PCAP_ERRBUF_SIZE,
		    "%s: Can't generate path name string for /sys/class/net device",
		    device);
		return PCAP_ERROR;
	}
	bytes_read = readlink(pathstr, phydev_path, phydev_max_pathlen);
	if (bytes_read == -1) {
		if (errno == ENOENT || errno == EINVAL) {
			free(pathstr);
			return 0;
		}
		snprintf(handle->errbuf, PCAP_ERRBUF_SIZE,
		    "%s: Can't readlink %s: %s", device, pathstr,
		    strerror(errno));
		free(pathstr);
		return PCAP_ERROR;
	}
	free(pathstr);
	phydev_path[bytes_read] = '\0';
	return 1;
}

#ifdef HAVE_LIBNL_SOCKETS
#define get_nl_errmsg	nl_geterror
#else

#define nl_sock nl_handle

static inline struct nl_handle *
nl_socket_alloc(void)
{
	return nl_handle_alloc();
}

static inline void
nl_socket_free(struct nl_handle *h)
{
	nl_handle_destroy(h);
}

#define get_nl_errmsg	strerror

static inline int
__genl_ctrl_alloc_cache(struct nl_handle *h, struct nl_cache **cache)
{
	struct nl_cache *tmp = genl_ctrl_alloc_cache(h);
	if (!tmp)
		return -ENOMEM;
	*cache = tmp;
	return 0;
}
#define genl_ctrl_alloc_cache __genl_ctrl_alloc_cache
#endif 

struct nl80211_state {
	struct nl_sock *nl_sock;
	struct nl_cache *nl_cache;
	struct genl_family *nl80211;
};

static int
nl80211_init(pcap_t *handle, struct nl80211_state *state, const char *device)
{
	int err;

	state->nl_sock = nl_socket_alloc();
	if (!state->nl_sock) {
		snprintf(handle->errbuf, PCAP_ERRBUF_SIZE,
		    "%s: failed to allocate netlink handle", device);
		return PCAP_ERROR;
	}

	if (genl_connect(state->nl_sock)) {
		snprintf(handle->errbuf, PCAP_ERRBUF_SIZE,
		    "%s: failed to connect to generic netlink", device);
		goto out_handle_destroy;
	}

	err = genl_ctrl_alloc_cache(state->nl_sock, &state->nl_cache);
	if (err < 0) {
		snprintf(handle->errbuf, PCAP_ERRBUF_SIZE,
		    "%s: failed to allocate generic netlink cache: %s",
		    device, get_nl_errmsg(-err));
		goto out_handle_destroy;
	}

	state->nl80211 = genl_ctrl_search_by_name(state->nl_cache, "nl80211");
	if (!state->nl80211) {
		snprintf(handle->errbuf, PCAP_ERRBUF_SIZE,
		    "%s: nl80211 not found", device);
		goto out_cache_free;
	}

	return 0;

out_cache_free:
	nl_cache_free(state->nl_cache);
out_handle_destroy:
	nl_socket_free(state->nl_sock);
	return PCAP_ERROR;
}

static void
nl80211_cleanup(struct nl80211_state *state)
{
	genl_family_put(state->nl80211);
	nl_cache_free(state->nl_cache);
	nl_socket_free(state->nl_sock);
}

static int
add_mon_if(pcap_t *handle, int sock_fd, struct nl80211_state *state,
    const char *device, const char *mondevice)
{
	int ifindex;
	struct nl_msg *msg;
	int err;

	ifindex = iface_get_id(sock_fd, device, handle->errbuf);
	if (ifindex == -1)
		return PCAP_ERROR;

	msg = nlmsg_alloc();
	if (!msg) {
		snprintf(handle->errbuf, PCAP_ERRBUF_SIZE,
		    "%s: failed to allocate netlink msg", device);
		return PCAP_ERROR;
	}

	genlmsg_put(msg, 0, 0, genl_family_get_id(state->nl80211), 0,
		    0, NL80211_CMD_NEW_INTERFACE, 0);
	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, ifindex);
	NLA_PUT_STRING(msg, NL80211_ATTR_IFNAME, mondevice);
	NLA_PUT_U32(msg, NL80211_ATTR_IFTYPE, NL80211_IFTYPE_MONITOR);

	err = nl_send_auto_complete(state->nl_sock, msg);
	if (err < 0) {
#if defined HAVE_LIBNL_NLE
		if (err == -NLE_FAILURE) {
#else
		if (err == -ENFILE) {
#endif
			nlmsg_free(msg);
			return 0;
		} else {
			snprintf(handle->errbuf, PCAP_ERRBUF_SIZE,
			    "%s: nl_send_auto_complete failed adding %s interface: %s",
			    device, mondevice, get_nl_errmsg(-err));
			nlmsg_free(msg);
			return PCAP_ERROR;
		}
	}
	err = nl_wait_for_ack(state->nl_sock);
	if (err < 0) {
#if defined HAVE_LIBNL_NLE
		if (err == -NLE_FAILURE) {
#else
		if (err == -ENFILE) {
#endif
			nlmsg_free(msg);
			return 0;
		} else {
			snprintf(handle->errbuf, PCAP_ERRBUF_SIZE,
			    "%s: nl_wait_for_ack failed adding %s interface: %s",
			    device, mondevice, get_nl_errmsg(-err));
			nlmsg_free(msg);
			return PCAP_ERROR;
		}
	}

	nlmsg_free(msg);
	return 1;

nla_put_failure:
	snprintf(handle->errbuf, PCAP_ERRBUF_SIZE,
	    "%s: nl_put failed adding %s interface",
	    device, mondevice);
	nlmsg_free(msg);
	return PCAP_ERROR;
}

static int
del_mon_if(pcap_t *handle, int sock_fd, struct nl80211_state *state,
    const char *device, const char *mondevice)
{
	int ifindex;
	struct nl_msg *msg;
	int err;

	ifindex = iface_get_id(sock_fd, mondevice, handle->errbuf);
	if (ifindex == -1)
		return PCAP_ERROR;

	msg = nlmsg_alloc();
	if (!msg) {
		snprintf(handle->errbuf, PCAP_ERRBUF_SIZE,
		    "%s: failed to allocate netlink msg", device);
		return PCAP_ERROR;
	}

	genlmsg_put(msg, 0, 0, genl_family_get_id(state->nl80211), 0,
		    0, NL80211_CMD_DEL_INTERFACE, 0);
	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, ifindex);

	err = nl_send_auto_complete(state->nl_sock, msg);
	if (err < 0) {
		snprintf(handle->errbuf, PCAP_ERRBUF_SIZE,
		    "%s: nl_send_auto_complete failed deleting %s interface: %s",
		    device, mondevice, get_nl_errmsg(-err));
		nlmsg_free(msg);
		return PCAP_ERROR;
	}
	err = nl_wait_for_ack(state->nl_sock);
	if (err < 0) {
		snprintf(handle->errbuf, PCAP_ERRBUF_SIZE,
		    "%s: nl_wait_for_ack failed adding %s interface: %s",
		    device, mondevice, get_nl_errmsg(-err));
		nlmsg_free(msg);
		return PCAP_ERROR;
	}

	nlmsg_free(msg);
	return 1;

nla_put_failure:
	snprintf(handle->errbuf, PCAP_ERRBUF_SIZE,
	    "%s: nl_put failed deleting %s interface",
	    device, mondevice);
	nlmsg_free(msg);
	return PCAP_ERROR;
}

static int
enter_rfmon_mode_mac80211(pcap_t *handle, int sock_fd, const char *device)
{
	struct pcap_linux *handlep = handle->priv;
	int ret;
	char phydev_path[PATH_MAX+1];
	struct nl80211_state nlstate;
	struct ifreq ifr;
	u_int n;

	ret = get_mac80211_phydev(handle, device, phydev_path, PATH_MAX);
	if (ret < 0)
		return ret;	
	if (ret == 0)
		return 0;	


	ret = nl80211_init(handle, &nlstate, device);
	if (ret != 0)
		return ret;
	for (n = 0; n < UINT_MAX; n++) {
		char mondevice[3+10+1];	

		snprintf(mondevice, sizeof mondevice, "mon%u", n);
		ret = add_mon_if(handle, sock_fd, &nlstate, device, mondevice);
		if (ret == 1) {
			handlep->mondevice = strdup(mondevice);
			goto added;
		}
		if (ret < 0) {
			nl80211_cleanup(&nlstate);
			return ret;
		}
	}

	snprintf(handle->errbuf, PCAP_ERRBUF_SIZE,
	    "%s: No free monN interfaces", device);
	nl80211_cleanup(&nlstate);
	return PCAP_ERROR;

added:

#if 0
	delay.tv_sec = 0;
	delay.tv_nsec = 500000000;
	nanosleep(&delay, NULL);
#endif

	if (!pcap_do_addexit(handle)) {
		return PCAP_ERROR_RFMON_NOTSUP;
	}

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, handlep->mondevice, sizeof(ifr.ifr_name));
	if (ioctl(sock_fd, SIOCGIFFLAGS, &ifr) == -1) {
		snprintf(handle->errbuf, PCAP_ERRBUF_SIZE,
		    "%s: Can't get flags for %s: %s", device,
		    handlep->mondevice, strerror(errno));
		del_mon_if(handle, sock_fd, &nlstate, device,
		    handlep->mondevice);
		nl80211_cleanup(&nlstate);
		return PCAP_ERROR;
	}
	ifr.ifr_flags |= IFF_UP|IFF_RUNNING;
	if (ioctl(sock_fd, SIOCSIFFLAGS, &ifr) == -1) {
		snprintf(handle->errbuf, PCAP_ERRBUF_SIZE,
		    "%s: Can't set flags for %s: %s", device,
		    handlep->mondevice, strerror(errno));
		del_mon_if(handle, sock_fd, &nlstate, device,
		    handlep->mondevice);
		nl80211_cleanup(&nlstate);
		return PCAP_ERROR;
	}

	nl80211_cleanup(&nlstate);

	handlep->must_do_on_close |= MUST_DELETE_MONIF;

	pcap_add_to_pcaps_to_close(handle);

	return 1;
}
#endif 

static int
pcap_can_set_rfmon_linux(pcap_t *handle)
{
#ifdef HAVE_LIBNL
	char phydev_path[PATH_MAX+1];
	int ret;
#endif
#ifdef IW_MODE_MONITOR
	int sock_fd;
	struct iwreq ireq;
#endif

	if (strcmp(handle->opt.source, "any") == 0) {
		return 0;
	}

#ifdef HAVE_LIBNL
	ret = get_mac80211_phydev(handle, handle->opt.source, phydev_path,
	    PATH_MAX);
	if (ret < 0)
		return ret;	
	if (ret == 1)
		return 1;	
#endif

#ifdef IW_MODE_MONITOR
	sock_fd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if (sock_fd == -1) {
		(void)snprintf(handle->errbuf, PCAP_ERRBUF_SIZE,
		    "socket: %s", pcap_strerror(errno));
		return PCAP_ERROR;
	}

	strncpy(ireq.ifr_ifrn.ifrn_name, handle->opt.source,
	    sizeof ireq.ifr_ifrn.ifrn_name);
	ireq.ifr_ifrn.ifrn_name[sizeof ireq.ifr_ifrn.ifrn_name - 1] = 0;
	if (ioctl(sock_fd, SIOCGIWMODE, &ireq) != -1) {
		close(sock_fd);
		return 1;
	}
	if (errno == ENODEV) {
		
		(void)snprintf(handle->errbuf, PCAP_ERRBUF_SIZE,
		    "SIOCGIWMODE failed: %s", pcap_strerror(errno));
		close(sock_fd);
		return PCAP_ERROR_NO_SUCH_DEVICE;
	}
	close(sock_fd);
#endif
	return 0;
}

static long int
linux_if_drops(const char * if_name)
{
	char buffer[512];
	char * bufptr;
	FILE * file;
	int field_to_convert = 3, if_name_sz = strlen(if_name);
	long int dropped_pkts = 0;
	
	file = fopen("/proc/net/dev", "r");
	if (!file)
		return 0;

	while (!dropped_pkts && fgets( buffer, sizeof(buffer), file ))
	{
		if (field_to_convert != 4 && strstr(buffer, "bytes"))
		{
			field_to_convert = 4;
			continue;
		}
	
		
		if ((bufptr = strstr(buffer, if_name)) &&
			(bufptr == buffer || *(bufptr-1) == ' ') &&
			*(bufptr + if_name_sz) == ':')
		{
			bufptr = bufptr + if_name_sz + 1;

			
			while( --field_to_convert && *bufptr != '\0')
			{
				while (*bufptr != '\0' && *(bufptr++) == ' ');
				while (*bufptr != '\0' && *(bufptr++) != ' ');
			}
			
			
			while (*bufptr != '\0' && *bufptr == ' ') bufptr++;
			
			if (*bufptr != '\0')
				dropped_pkts = strtol(bufptr, NULL, 10);

			break;
		}
	}
	
	fclose(file);
	return dropped_pkts;
} 



static void	pcap_cleanup_linux( pcap_t *handle )
{
	struct pcap_linux *handlep = handle->priv;
	struct ifreq	ifr;
#ifdef HAVE_LIBNL
	struct nl80211_state nlstate;
	int ret;
#endif 
#ifdef IW_MODE_MONITOR
	int oldflags;
	struct iwreq ireq;
#endif 

	if (handlep->must_do_on_close != 0) {
		if (handlep->must_do_on_close & MUST_CLEAR_PROMISC) {
			memset(&ifr, 0, sizeof(ifr));
			strncpy(ifr.ifr_name, handlep->device,
			    sizeof(ifr.ifr_name));
			if (ioctl(handle->fd, SIOCGIFFLAGS, &ifr) == -1) {
				fprintf(stderr,
				    "Can't restore interface %s flags (SIOCGIFFLAGS failed: %s).\n"
				    "Please adjust manually.\n"
				    "Hint: This can't happen with Linux >= 2.2.0.\n",
				    handlep->device, strerror(errno));
			} else {
				if (ifr.ifr_flags & IFF_PROMISC) {
					ifr.ifr_flags &= ~IFF_PROMISC;
					if (ioctl(handle->fd, SIOCSIFFLAGS,
					    &ifr) == -1) {
						fprintf(stderr,
						    "Can't restore interface %s flags (SIOCSIFFLAGS failed: %s).\n"
						    "Please adjust manually.\n"
						    "Hint: This can't happen with Linux >= 2.2.0.\n",
						    handlep->device,
						    strerror(errno));
					}
				}
			}
		}

#ifdef HAVE_LIBNL
		if (handlep->must_do_on_close & MUST_DELETE_MONIF) {
			ret = nl80211_init(handle, &nlstate, handlep->device);
			if (ret >= 0) {
				ret = del_mon_if(handle, handle->fd, &nlstate,
				    handlep->device, handlep->mondevice);
				nl80211_cleanup(&nlstate);
			}
			if (ret < 0) {
				fprintf(stderr,
				    "Can't delete monitor interface %s (%s).\n"
				    "Please delete manually.\n",
				    handlep->mondevice, handle->errbuf);
			}
		}
#endif 

#ifdef IW_MODE_MONITOR
		if (handlep->must_do_on_close & MUST_CLEAR_RFMON) {

			oldflags = 0;
			memset(&ifr, 0, sizeof(ifr));
			strncpy(ifr.ifr_name, handlep->device,
			    sizeof(ifr.ifr_name));
			if (ioctl(handle->fd, SIOCGIFFLAGS, &ifr) != -1) {
				if (ifr.ifr_flags & IFF_UP) {
					oldflags = ifr.ifr_flags;
					ifr.ifr_flags &= ~IFF_UP;
					if (ioctl(handle->fd, SIOCSIFFLAGS, &ifr) == -1)
						oldflags = 0;	
				}
			}

			strncpy(ireq.ifr_ifrn.ifrn_name, handlep->device,
			    sizeof ireq.ifr_ifrn.ifrn_name);
			ireq.ifr_ifrn.ifrn_name[sizeof ireq.ifr_ifrn.ifrn_name - 1]
			    = 0;
			ireq.u.mode = handlep->oldmode;
			if (ioctl(handle->fd, SIOCSIWMODE, &ireq) == -1) {
				fprintf(stderr,
				    "Can't restore interface %s wireless mode (SIOCSIWMODE failed: %s).\n"
				    "Please adjust manually.\n",
				    handlep->device, strerror(errno));
			}

			if (oldflags != 0) {
				ifr.ifr_flags = oldflags;
				if (ioctl(handle->fd, SIOCSIFFLAGS, &ifr) == -1) {
					fprintf(stderr,
					    "Can't bring interface %s back up (SIOCSIFFLAGS failed: %s).\n"
					    "Please adjust manually.\n",
					    handlep->device, strerror(errno));
				}
			}
		}
#endif 

		pcap_remove_from_pcaps_to_close(handle);
	}

	if (handlep->mondevice != NULL) {
		free(handlep->mondevice);
		handlep->mondevice = NULL;
	}
	if (handlep->device != NULL) {
		free(handlep->device);
		handlep->device = NULL;
	}
	pcap_cleanup_live_common(handle);
}

static int
pcap_activate_linux(pcap_t *handle)
{
	struct pcap_linux *handlep = handle->priv;
	const char	*device;
	int		status = 0;

	device = handle->opt.source;

	handle->inject_op = pcap_inject_linux;
	handle->setfilter_op = pcap_setfilter_linux;
	handle->setdirection_op = pcap_setdirection_linux;
	handle->set_datalink_op = pcap_set_datalink_linux;
	handle->getnonblock_op = pcap_getnonblock_fd;
	handle->setnonblock_op = pcap_setnonblock_fd;
	handle->cleanup_op = pcap_cleanup_linux;
	handle->read_op = pcap_read_linux;
	handle->stats_op = pcap_stats_linux;

	if (strcmp(device, "any") == 0) {
		if (handle->opt.promisc) {
			handle->opt.promisc = 0;
			
			snprintf(handle->errbuf, PCAP_ERRBUF_SIZE,
			    "Promiscuous mode not supported on the \"any\" device");
			status = PCAP_WARNING_PROMISC_NOTSUP;
		}
	}

	handlep->device	= strdup(device);
	if (handlep->device == NULL) {
		snprintf(handle->errbuf, PCAP_ERRBUF_SIZE, "strdup: %s",
			 pcap_strerror(errno) );
		return PCAP_ERROR;
	}
	
	
	handlep->timeout = handle->opt.timeout;

	if (handle->opt.promisc)
		handlep->proc_dropped = linux_if_drops(handlep->device);

	status = activate_new(handle);
	if (status < 0) {
		goto fail;
	}
	if (status == 1) {
		switch (activate_mmap(handle, &status)) {

		case 1:
			return status;

		case 0:
			break;

		case -1:
			goto fail;
		}
	}
	else if (status == 0) {
		
		if ((status = activate_old(handle)) != 1) {
			goto fail;
		}
	}

	status = 0;
	if (handle->opt.buffer_size != 0) {
		if (setsockopt(handle->fd, SOL_SOCKET, SO_RCVBUF,
		    &handle->opt.buffer_size,
		    sizeof(handle->opt.buffer_size)) == -1) {
			snprintf(handle->errbuf, PCAP_ERRBUF_SIZE,
				 "SO_RCVBUF: %s", pcap_strerror(errno));
			status = PCAP_ERROR;
			goto fail;
		}
	}

	

	handle->buffer	 = malloc(handle->bufsize + handle->offset);
	if (!handle->buffer) {
		snprintf(handle->errbuf, PCAP_ERRBUF_SIZE,
			 "malloc: %s", pcap_strerror(errno));
		status = PCAP_ERROR;
		goto fail;
	}

	handle->selectable_fd = handle->fd;

	return status;

fail:
	pcap_cleanup_linux(handle);
	return status;
}

static int
pcap_read_linux(pcap_t *handle, int max_packets, pcap_handler callback, u_char *user)
{
	return pcap_read_packet(handle, callback, user);
}

static int
pcap_set_datalink_linux(pcap_t *handle, int dlt)
{
	handle->linktype = dlt;
	return 0;
}

static inline int
linux_check_direction(const pcap_t *handle, const struct sockaddr_ll *sll)
{
	struct pcap_linux	*handlep = handle->priv;

	if (sll->sll_pkttype == PACKET_OUTGOING) {
		if (sll->sll_ifindex == handlep->lo_ifindex)
			return 0;

		if (handle->direction == PCAP_D_IN)
			return 0;
	} else {
		if (handle->direction == PCAP_D_OUT)
			return 0;
	}
	return 1;
}

static int
pcap_read_packet(pcap_t *handle, pcap_handler callback, u_char *userdata)
{
	struct pcap_linux	*handlep = handle->priv;
	u_char			*bp;
	int			offset;
#ifdef HAVE_PF_PACKET_SOCKETS
	struct sockaddr_ll	from;
	struct sll_header	*hdrp;
#else
	struct sockaddr		from;
#endif
#if defined(HAVE_PACKET_AUXDATA) && defined(HAVE_LINUX_TPACKET_AUXDATA_TP_VLAN_TCI)
	struct iovec		iov;
	struct msghdr		msg;
	struct cmsghdr		*cmsg;
	union {
		struct cmsghdr	cmsg;
		char		buf[CMSG_SPACE(sizeof(struct tpacket_auxdata))];
	} cmsg_buf;
#else 
	socklen_t		fromlen;
#endif 
	int			packet_len, caplen;
	struct pcap_pkthdr	pcap_header;

#ifdef HAVE_PF_PACKET_SOCKETS
	if (handlep->cooked)
		offset = SLL_HDR_LEN;
	else
		offset = 0;
#else
	offset = 0;
#endif

	bp = handle->buffer + handle->offset;

#if defined(HAVE_PACKET_AUXDATA) && defined(HAVE_LINUX_TPACKET_AUXDATA_TP_VLAN_TCI)
	msg.msg_name		= &from;
	msg.msg_namelen		= sizeof(from);
	msg.msg_iov		= &iov;
	msg.msg_iovlen		= 1;
	msg.msg_control		= &cmsg_buf;
	msg.msg_controllen	= sizeof(cmsg_buf);
	msg.msg_flags		= 0;

	iov.iov_len		= handle->bufsize - offset;
	iov.iov_base		= bp + offset;
#endif 

	do {
		if (handle->break_loop) {
			handle->break_loop = 0;
			return PCAP_ERROR_BREAK;
		}

#if defined(HAVE_PACKET_AUXDATA) && defined(HAVE_LINUX_TPACKET_AUXDATA_TP_VLAN_TCI)
		packet_len = recvmsg(handle->fd, &msg, MSG_TRUNC);
#else 
		fromlen = sizeof(from);
		packet_len = recvfrom(
			handle->fd, bp + offset,
			handle->bufsize - offset, MSG_TRUNC,
			(struct sockaddr *) &from, &fromlen);
#endif 
	} while (packet_len == -1 && errno == EINTR);

	

	if (packet_len == -1) {
		switch (errno) {

		case EAGAIN:
			return 0;	

		case ENETDOWN:
			snprintf(handle->errbuf, PCAP_ERRBUF_SIZE,
				"The interface went down");
			return PCAP_ERROR;

		default:
			snprintf(handle->errbuf, PCAP_ERRBUF_SIZE,
				 "recvfrom: %s", pcap_strerror(errno));
			return PCAP_ERROR;
		}
	}

#ifdef HAVE_PF_PACKET_SOCKETS
	if (!handlep->sock_packet) {
		if (handlep->ifindex != -1 &&
		    from.sll_ifindex != handlep->ifindex)
			return 0;

		if (!linux_check_direction(handle, &from))
			return 0;
	}
#endif

#ifdef HAVE_PF_PACKET_SOCKETS
	if (handlep->cooked) {
		packet_len += SLL_HDR_LEN;

		hdrp = (struct sll_header *)bp;
		hdrp->sll_pkttype = map_packet_type_to_sll_type(from.sll_pkttype);
		hdrp->sll_hatype = htons(from.sll_hatype);
		hdrp->sll_halen = htons(from.sll_halen);
		memcpy(hdrp->sll_addr, from.sll_addr,
		    (from.sll_halen > SLL_ADDRLEN) ?
		      SLL_ADDRLEN :
		      from.sll_halen);
		hdrp->sll_protocol = from.sll_protocol;
	}

#if defined(HAVE_PACKET_AUXDATA) && defined(HAVE_LINUX_TPACKET_AUXDATA_TP_VLAN_TCI)
	if (handlep->vlan_offset != -1) {
		for (cmsg = CMSG_FIRSTHDR(&msg); cmsg; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
			struct tpacket_auxdata *aux;
			unsigned int len;
			struct vlan_tag *tag;

			if (cmsg->cmsg_len < CMSG_LEN(sizeof(struct tpacket_auxdata)) ||
			    cmsg->cmsg_level != SOL_PACKET ||
			    cmsg->cmsg_type != PACKET_AUXDATA)
				continue;

			aux = (struct tpacket_auxdata *)CMSG_DATA(cmsg);
#if defined(TP_STATUS_VLAN_VALID)
			if ((aux->tp_vlan_tci == 0) && !(aux->tp_status & TP_STATUS_VLAN_VALID))
#else
			if (aux->tp_vlan_tci == 0) 
#endif
				continue;

			len = packet_len > iov.iov_len ? iov.iov_len : packet_len;
			if (len < (unsigned int) handlep->vlan_offset)
				break;

			bp -= VLAN_TAG_LEN;
			memmove(bp, bp + VLAN_TAG_LEN, handlep->vlan_offset);

			tag = (struct vlan_tag *)(bp + handlep->vlan_offset);
			tag->vlan_tpid = htons(ETH_P_8021Q);
			tag->vlan_tci = htons(aux->tp_vlan_tci);

			packet_len += VLAN_TAG_LEN;
		}
	}
#endif 
#endif 


	caplen = packet_len;
	if (caplen > handle->snapshot)
		caplen = handle->snapshot;

	
	if (handlep->filter_in_userland && handle->fcode.bf_insns) {
		if (bpf_filter(handle->fcode.bf_insns, bp,
		                packet_len, caplen) == 0)
		{
			
			return 0;
		}
	}

	

	
#if defined(SIOCGSTAMPNS) && defined(SO_TIMESTAMPNS)
	if (handle->opt.tstamp_precision == PCAP_TSTAMP_PRECISION_NANO) {
		if (ioctl(handle->fd, SIOCGSTAMPNS, &pcap_header.ts) == -1) {
			snprintf(handle->errbuf, PCAP_ERRBUF_SIZE,
					"SIOCGSTAMPNS: %s", pcap_strerror(errno));
			return PCAP_ERROR;
		}
        } else
#endif
	{
		if (ioctl(handle->fd, SIOCGSTAMP, &pcap_header.ts) == -1) {
			snprintf(handle->errbuf, PCAP_ERRBUF_SIZE,
					"SIOCGSTAMP: %s", pcap_strerror(errno));
			return PCAP_ERROR;
		}
        }

	pcap_header.caplen	= caplen;
	pcap_header.len		= packet_len;

	handlep->packets_read++;

	
	callback(userdata, &pcap_header, bp);

	return 1;
}

static int
pcap_inject_linux(pcap_t *handle, const void *buf, size_t size)
{
	struct pcap_linux *handlep = handle->priv;
	int ret;

#ifdef HAVE_PF_PACKET_SOCKETS
	if (!handlep->sock_packet) {
		
		if (handlep->ifindex == -1) {
			strlcpy(handle->errbuf,
			    "Sending packets isn't supported on the \"any\" device",
			    PCAP_ERRBUF_SIZE);
			return (-1);
		}

		if (handlep->cooked) {
			strlcpy(handle->errbuf,
			    "Sending packets isn't supported in cooked mode",
			    PCAP_ERRBUF_SIZE);
			return (-1);
		}
	}
#endif

	ret = send(handle->fd, buf, size, 0);
	if (ret == -1) {
		snprintf(handle->errbuf, PCAP_ERRBUF_SIZE, "send: %s",
		    pcap_strerror(errno));
		return (-1);
	}
	return (ret);
}                           

static int
pcap_stats_linux(pcap_t *handle, struct pcap_stat *stats)
{
	struct pcap_linux *handlep = handle->priv;
#ifdef HAVE_TPACKET_STATS
#ifdef HAVE_TPACKET3
	struct tpacket_stats_v3 kstats;
#else 
	struct tpacket_stats kstats;
#endif 
	socklen_t len = sizeof (struct tpacket_stats);
#endif 

	long if_dropped = 0;
	
	if (handle->opt.promisc)
	{
		if_dropped = handlep->proc_dropped;
		handlep->proc_dropped = linux_if_drops(handlep->device);
		handlep->stat.ps_ifdrop += (handlep->proc_dropped - if_dropped);
	}

#ifdef HAVE_TPACKET_STATS
	if (getsockopt(handle->fd, SOL_PACKET, PACKET_STATISTICS,
			&kstats, &len) > -1) {
		handlep->stat.ps_recv += kstats.tp_packets;
		handlep->stat.ps_drop += kstats.tp_drops;
		*stats = handlep->stat;
		return 0;
	}
	else
	{
		if (errno != EOPNOTSUPP) {
			snprintf(handle->errbuf, PCAP_ERRBUF_SIZE,
			    "pcap_stats: %s", pcap_strerror(errno));
			return -1;
		}
	}
#endif
	 
	stats->ps_recv = handlep->packets_read;
	stats->ps_drop = 0;
	stats->ps_ifdrop = handlep->stat.ps_ifdrop;
	return 0;
}

static int
scan_sys_class_net(pcap_if_t **devlistp, char *errbuf)
{
	DIR *sys_class_net_d;
	int fd;
	struct dirent *ent;
	char subsystem_path[PATH_MAX+1];
	struct stat statb;
	char *p;
	char name[512];	
	char *q, *saveq;
	struct ifreq ifrflags;
	int ret = 1;

	sys_class_net_d = opendir("/sys/class/net");
	if (sys_class_net_d == NULL) {
		if (errno == ENOENT)
			return (0);

		(void)snprintf(errbuf, PCAP_ERRBUF_SIZE,
		    "Can't open /sys/class/net: %s", pcap_strerror(errno));
		return (-1);
	}

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0) {
		(void)snprintf(errbuf, PCAP_ERRBUF_SIZE,
		    "socket: %s", pcap_strerror(errno));
		(void)closedir(sys_class_net_d);
		return (-1);
	}

	for (;;) {
		errno = 0;
		ent = readdir(sys_class_net_d);
		if (ent == NULL) {
			break;
		}

		if (strcmp(ent->d_name, ".") == 0 ||
		    strcmp(ent->d_name, "..") == 0)
			continue;

		if (ent->d_type == DT_REG)
			continue;

		snprintf(subsystem_path, sizeof subsystem_path,
		    "/sys/class/net/%s/ifindex", ent->d_name);
		if (lstat(subsystem_path, &statb) != 0) {
			continue;
		}

		p = &ent->d_name[0];
		q = &name[0];
		while (*p != '\0' && isascii(*p) && !isspace(*p)) {
			if (*p == ':') {
				saveq = q;
				while (isascii(*p) && isdigit(*p))
					*q++ = *p++;
				if (*p != ':') {
					q = saveq;
				}
				break;
			} else
				*q++ = *p++;
		}
		*q = '\0';

		strncpy(ifrflags.ifr_name, name, sizeof(ifrflags.ifr_name));
		if (ioctl(fd, SIOCGIFFLAGS, (char *)&ifrflags) < 0) {
			if (errno == ENXIO || errno == ENODEV)
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

		if (pcap_add_if(devlistp, name, ifrflags.ifr_flags, NULL,
		    errbuf) == -1) {
			ret = -1;
			break;
		}
	}
	if (ret != -1) {
		if (errno != 0) {
			(void)snprintf(errbuf, PCAP_ERRBUF_SIZE,
			    "Error reading /sys/class/net: %s",
			    pcap_strerror(errno));
			ret = -1;
		}
	}

	(void)close(fd);
	(void)closedir(sys_class_net_d);
	return (ret);
}

static int
scan_proc_net_dev(pcap_if_t **devlistp, char *errbuf)
{
	FILE *proc_net_f;
	int fd;
	char linebuf[512];
	int linenum;
	char *p;
	char name[512];	
	char *q, *saveq;
	struct ifreq ifrflags;
	int ret = 0;

	proc_net_f = fopen("/proc/net/dev", "r");
	if (proc_net_f == NULL) {
		if (errno == ENOENT)
			return (0);

		(void)snprintf(errbuf, PCAP_ERRBUF_SIZE,
		    "Can't open /proc/net/dev: %s", pcap_strerror(errno));
		return (-1);
	}

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0) {
		(void)snprintf(errbuf, PCAP_ERRBUF_SIZE,
		    "socket: %s", pcap_strerror(errno));
		(void)fclose(proc_net_f);
		return (-1);
	}

	for (linenum = 1;
	    fgets(linebuf, sizeof linebuf, proc_net_f) != NULL; linenum++) {
		if (linenum <= 2)
			continue;

		p = &linebuf[0];

		while (*p != '\0' && isascii(*p) && isspace(*p))
			p++;
		if (*p == '\0' || *p == '\n')
			continue;	

		q = &name[0];
		while (*p != '\0' && isascii(*p) && !isspace(*p)) {
			if (*p == ':') {
				saveq = q;
				while (isascii(*p) && isdigit(*p))
					*q++ = *p++;
				if (*p != ':') {
					q = saveq;
				}
				break;
			} else
				*q++ = *p++;
		}
		*q = '\0';

		strncpy(ifrflags.ifr_name, name, sizeof(ifrflags.ifr_name));
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

		if (pcap_add_if(devlistp, name, ifrflags.ifr_flags, NULL,
		    errbuf) == -1) {
			ret = -1;
			break;
		}
	}
	if (ret != -1) {
		if (ferror(proc_net_f)) {
			(void)snprintf(errbuf, PCAP_ERRBUF_SIZE,
			    "Error reading /proc/net/dev: %s",
			    pcap_strerror(errno));
			ret = -1;
		}
	}

	(void)close(fd);
	(void)fclose(proc_net_f);
	return (ret);
}

static const char any_descr[] = "Pseudo-device that captures on all interfaces";

int
pcap_platform_finddevs(pcap_if_t **alldevsp, char *errbuf)
{
	int ret;

	ret = scan_sys_class_net(alldevsp, errbuf);
	if (ret == -1)
		return (-1);	
	if (ret == 0) {
		if (scan_proc_net_dev(alldevsp, errbuf) == -1)
			return (-1);
	}

	if (pcap_add_if(alldevsp, "any", 0, any_descr, errbuf) < 0)
		return (-1);

	return (0);
}

static int
pcap_setfilter_linux_common(pcap_t *handle, struct bpf_program *filter,
    int is_mmapped)
{
	struct pcap_linux *handlep;
#ifdef SO_ATTACH_FILTER
	struct sock_fprog	fcode;
	int			can_filter_in_kernel;
	int			err = 0;
#endif

	if (!handle)
		return -1;
	if (!filter) {
	        strncpy(handle->errbuf, "setfilter: No filter specified",
			PCAP_ERRBUF_SIZE);
		return -1;
	}

	handlep = handle->priv;

	

	if (install_bpf_program(handle, filter) < 0)
		
		return -1;

	handlep->filter_in_userland = 1;

	

#ifdef SO_ATTACH_FILTER
#ifdef USHRT_MAX
	if (handle->fcode.bf_len > USHRT_MAX) {
		fprintf(stderr, "Warning: Filter too complex for kernel\n");
		fcode.len = 0;
		fcode.filter = NULL;
		can_filter_in_kernel = 0;
	} else
#endif 
	{
		switch (fix_program(handle, &fcode, is_mmapped)) {

		case -1:
		default:
			return -1;

		case 0:
			can_filter_in_kernel = 0;
			break;

		case 1:
			can_filter_in_kernel = 1;
			break;
		}
	}

	if (can_filter_in_kernel) {
		if ((err = set_kernel_filter(handle, &fcode)) == 0)
		{
			handlep->filter_in_userland = 0;
		}
		else if (err == -1)	
		{
			if (errno != ENOPROTOOPT && errno != EOPNOTSUPP) {
				fprintf(stderr,
				    "Warning: Kernel filter failed: %s\n",
					pcap_strerror(errno));
			}
		}
	}

	if (handlep->filter_in_userland)
		reset_kernel_filter(handle);

	if (fcode.filter != NULL)
		free(fcode.filter);

	if (err == -2)
		
		return -1;
#endif 

	return 0;
}

static int
pcap_setfilter_linux(pcap_t *handle, struct bpf_program *filter)
{
	return pcap_setfilter_linux_common(handle, filter, 0);
}


static int
pcap_setdirection_linux(pcap_t *handle, pcap_direction_t d)
{
#ifdef HAVE_PF_PACKET_SOCKETS
	struct pcap_linux *handlep = handle->priv;

	if (!handlep->sock_packet) {
		handle->direction = d;
		return 0;
	}
#endif
	snprintf(handle->errbuf, PCAP_ERRBUF_SIZE,
	    "Setting direction is not supported on SOCK_PACKET sockets");
	return -1;
}

#ifdef HAVE_PF_PACKET_SOCKETS
static short int
map_packet_type_to_sll_type(short int sll_pkttype)
{
	switch (sll_pkttype) {

	case PACKET_HOST:
		return htons(LINUX_SLL_HOST);

	case PACKET_BROADCAST:
		return htons(LINUX_SLL_BROADCAST);

	case PACKET_MULTICAST:
		return  htons(LINUX_SLL_MULTICAST);

	case PACKET_OTHERHOST:
		return htons(LINUX_SLL_OTHERHOST);

	case PACKET_OUTGOING:
		return htons(LINUX_SLL_OUTGOING);

	default:
		return -1;
	}
}
#endif

static void map_arphrd_to_dlt(pcap_t *handle, int arptype, int cooked_ok)
{
	switch (arptype) {

	case ARPHRD_ETHER:
		handle->dlt_list = (u_int *) malloc(sizeof(u_int) * 2);
		if (handle->dlt_list != NULL) {
			handle->dlt_list[0] = DLT_EN10MB;
			handle->dlt_list[1] = DLT_DOCSIS;
			handle->dlt_count = 2;
		}
		

	case ARPHRD_METRICOM:
	case ARPHRD_LOOPBACK:
		handle->linktype = DLT_EN10MB;
		handle->offset = 2;
		break;

	case ARPHRD_EETHER:
		handle->linktype = DLT_EN3MB;
		break;

	case ARPHRD_AX25:
		handle->linktype = DLT_AX25_KISS;
		break;

	case ARPHRD_PRONET:
		handle->linktype = DLT_PRONET;
		break;

	case ARPHRD_CHAOS:
		handle->linktype = DLT_CHAOS;
		break;
#ifndef ARPHRD_CAN
#define ARPHRD_CAN 280
#endif
	case ARPHRD_CAN:
		handle->linktype = DLT_CAN_SOCKETCAN;
		break;

#ifndef ARPHRD_IEEE802_TR
#define ARPHRD_IEEE802_TR 800	
#endif
	case ARPHRD_IEEE802_TR:
	case ARPHRD_IEEE802:
		handle->linktype = DLT_IEEE802;
		handle->offset = 2;
		break;

	case ARPHRD_ARCNET:
		handle->linktype = DLT_ARCNET_LINUX;
		break;

#ifndef ARPHRD_FDDI	
#define ARPHRD_FDDI	774
#endif
	case ARPHRD_FDDI:
		handle->linktype = DLT_FDDI;
		handle->offset = 3;
		break;

#ifndef ARPHRD_ATM  
#define ARPHRD_ATM 19
#endif
	case ARPHRD_ATM:
		if (cooked_ok)
			handle->linktype = DLT_LINUX_SLL;
		else
			handle->linktype = -1;
		break;

#ifndef ARPHRD_IEEE80211  
#define ARPHRD_IEEE80211 801
#endif
	case ARPHRD_IEEE80211:
		handle->linktype = DLT_IEEE802_11;
		break;

#ifndef ARPHRD_IEEE80211_PRISM  
#define ARPHRD_IEEE80211_PRISM 802
#endif
	case ARPHRD_IEEE80211_PRISM:
		handle->linktype = DLT_PRISM_HEADER;
		break;

#ifndef ARPHRD_IEEE80211_RADIOTAP 
#define ARPHRD_IEEE80211_RADIOTAP 803
#endif
	case ARPHRD_IEEE80211_RADIOTAP:
		handle->linktype = DLT_IEEE802_11_RADIO;
		break;

	case ARPHRD_PPP:
		if (cooked_ok)
			handle->linktype = DLT_LINUX_SLL;
		else {
			handle->linktype = DLT_RAW;
		}
		break;

#ifndef ARPHRD_CISCO
#define ARPHRD_CISCO 513 
#endif
	case ARPHRD_CISCO:
		handle->linktype = DLT_C_HDLC;
		break;

	case ARPHRD_TUNNEL:
#ifndef ARPHRD_SIT
#define ARPHRD_SIT 776	
#endif
	case ARPHRD_SIT:
	case ARPHRD_CSLIP:
	case ARPHRD_SLIP6:
	case ARPHRD_CSLIP6:
	case ARPHRD_ADAPT:
	case ARPHRD_SLIP:
#ifndef ARPHRD_RAWHDLC
#define ARPHRD_RAWHDLC 518
#endif
	case ARPHRD_RAWHDLC:
#ifndef ARPHRD_DLCI
#define ARPHRD_DLCI 15
#endif
	case ARPHRD_DLCI:
		handle->linktype = DLT_RAW;
		break;

#ifndef ARPHRD_FRAD
#define ARPHRD_FRAD 770
#endif
	case ARPHRD_FRAD:
		handle->linktype = DLT_FRELAY;
		break;

	case ARPHRD_LOCALTLK:
		handle->linktype = DLT_LTALK;
		break;

	case 18:
		handle->linktype = DLT_IP_OVER_FC;
		break;

#ifndef ARPHRD_FCPP
#define ARPHRD_FCPP	784
#endif
	case ARPHRD_FCPP:
#ifndef ARPHRD_FCAL
#define ARPHRD_FCAL	785
#endif
	case ARPHRD_FCAL:
#ifndef ARPHRD_FCPL
#define ARPHRD_FCPL	786
#endif
	case ARPHRD_FCPL:
#ifndef ARPHRD_FCFABRIC
#define ARPHRD_FCFABRIC	787
#endif
	case ARPHRD_FCFABRIC:
		handle->dlt_list = (u_int *) malloc(sizeof(u_int) * 2);
		if (handle->dlt_list != NULL) {
			handle->dlt_list[0] = DLT_FC_2;
			handle->dlt_list[1] = DLT_FC_2_WITH_FRAME_DELIMS;
			handle->dlt_list[2] = DLT_IP_OVER_FC;
			handle->dlt_count = 3;
		}
		handle->linktype = DLT_FC_2;
		break;

#ifndef ARPHRD_IRDA
#define ARPHRD_IRDA	783
#endif
	case ARPHRD_IRDA:
		
		handle->linktype = DLT_LINUX_IRDA;
		
		break;

#ifndef ARPHRD_LAPD
#define ARPHRD_LAPD	8445
#endif
	case ARPHRD_LAPD:
		
		handle->linktype = DLT_LINUX_LAPD;
		break;

#ifndef ARPHRD_NONE
#define ARPHRD_NONE	0xFFFE
#endif
	case ARPHRD_NONE:
		handle->linktype = DLT_RAW;
		break;

#ifndef ARPHRD_IEEE802154
#define ARPHRD_IEEE802154      804
#endif
       case ARPHRD_IEEE802154:
               handle->linktype =  DLT_IEEE802_15_4_NOFCS;
               break;

	default:
		handle->linktype = -1;
		break;
	}
}


static int
activate_new(pcap_t *handle)
{
#ifdef HAVE_PF_PACKET_SOCKETS
	struct pcap_linux *handlep = handle->priv;
	const char		*device = handle->opt.source;
	int			is_any_device = (strcmp(device, "any") == 0);
	int			sock_fd = -1, arptype;
#ifdef HAVE_PACKET_AUXDATA
	int			val;
#endif
	int			err = 0;
	struct packet_mreq	mr;

	sock_fd = is_any_device ?
		socket(PF_PACKET, SOCK_DGRAM, htons(ETH_P_ALL)) :
		socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));

	if (sock_fd == -1) {
		if (errno == EINVAL || errno == EAFNOSUPPORT) {
			return 0;
		}

		snprintf(handle->errbuf, PCAP_ERRBUF_SIZE, "socket: %s",
			 pcap_strerror(errno) );
		if (errno == EPERM || errno == EACCES) {
			return PCAP_ERROR_PERM_DENIED;
		} else {
			return PCAP_ERROR;
		}
	}

	
	handlep->sock_packet = 0;

	handlep->lo_ifindex = iface_get_id(sock_fd, "lo", handle->errbuf);

	handle->offset	 = 0;

	if (!is_any_device) {
		
		handlep->cooked = 0;

		if (handle->opt.rfmon) {
			err = enter_rfmon_mode(handle, sock_fd, device);
			if (err < 0) {
				
				close(sock_fd);
				return err;
			}
			if (err == 0) {
				close(sock_fd);
				return PCAP_ERROR_RFMON_NOTSUP;
			}

			if (handlep->mondevice != NULL)
				device = handlep->mondevice;
		}
		arptype	= iface_get_arptype(sock_fd, device, handle->errbuf);
		if (arptype < 0) {
			close(sock_fd);
			return arptype;
		}
		map_arphrd_to_dlt(handle, arptype, 1);
		if (handle->linktype == -1 ||
		    handle->linktype == DLT_LINUX_SLL ||
		    handle->linktype == DLT_LINUX_IRDA ||
		    handle->linktype == DLT_LINUX_LAPD ||
		    (handle->linktype == DLT_EN10MB &&
		     (strncmp("isdn", device, 4) == 0 ||
		      strncmp("isdY", device, 4) == 0))) {
			if (close(sock_fd) == -1) {
				snprintf(handle->errbuf, PCAP_ERRBUF_SIZE,
					 "close: %s", pcap_strerror(errno));
				return PCAP_ERROR;
			}
			sock_fd = socket(PF_PACKET, SOCK_DGRAM,
			    htons(ETH_P_ALL));
			if (sock_fd == -1) {
				snprintf(handle->errbuf, PCAP_ERRBUF_SIZE,
				    "socket: %s", pcap_strerror(errno));
				if (errno == EPERM || errno == EACCES) {
					return PCAP_ERROR_PERM_DENIED;
				} else {
					return PCAP_ERROR;
				}
			}
			handlep->cooked = 1;

			if (handle->dlt_list != NULL) {
				free(handle->dlt_list);
				handle->dlt_list = NULL;
				handle->dlt_count = 0;
			}

			if (handle->linktype == -1) {
				snprintf(handle->errbuf, PCAP_ERRBUF_SIZE,
					"arptype %d not "
					"supported by libpcap - "
					"falling back to cooked "
					"socket",
					arptype);
			}

			if (handle->linktype != DLT_LINUX_IRDA &&
			    handle->linktype != DLT_LINUX_LAPD)
				handle->linktype = DLT_LINUX_SLL;
		}

		handlep->ifindex = iface_get_id(sock_fd, device,
		    handle->errbuf);
		if (handlep->ifindex == -1) {
			close(sock_fd);
			return PCAP_ERROR;
		}

		if ((err = iface_bind(sock_fd, handlep->ifindex,
		    handle->errbuf)) != 1) {
		    	close(sock_fd);
			if (err < 0)
				return err;
			else
				return 0;	
		}
	} else {
		if (handle->opt.rfmon) {
			return PCAP_ERROR_RFMON_NOTSUP;
		}

		handlep->cooked = 1;
		handle->linktype = DLT_LINUX_SLL;

		handlep->ifindex = -1;
	}



	if (!is_any_device && handle->opt.promisc) {
		memset(&mr, 0, sizeof(mr));
		mr.mr_ifindex = handlep->ifindex;
		mr.mr_type    = PACKET_MR_PROMISC;
		if (setsockopt(sock_fd, SOL_PACKET, PACKET_ADD_MEMBERSHIP,
		    &mr, sizeof(mr)) == -1) {
			snprintf(handle->errbuf, PCAP_ERRBUF_SIZE,
				"setsockopt: %s", pcap_strerror(errno));
			close(sock_fd);
			return PCAP_ERROR;
		}
	}

#ifdef HAVE_PACKET_AUXDATA
	val = 1;
	if (setsockopt(sock_fd, SOL_PACKET, PACKET_AUXDATA, &val,
		       sizeof(val)) == -1 && errno != ENOPROTOOPT) {
		snprintf(handle->errbuf, PCAP_ERRBUF_SIZE,
			 "setsockopt: %s", pcap_strerror(errno));
		close(sock_fd);
		return PCAP_ERROR;
	}
	handle->offset += VLAN_TAG_LEN;
#endif 

	if (handlep->cooked) {
		if (handle->snapshot < SLL_HDR_LEN + 1)
			handle->snapshot = SLL_HDR_LEN + 1;
	}
	handle->bufsize = handle->snapshot;

	switch (handle->linktype) {

	case DLT_EN10MB:
		handlep->vlan_offset = 2 * ETH_ALEN;
		break;

	case DLT_LINUX_SLL:
		handlep->vlan_offset = 14;
		break;

	default:
		handlep->vlan_offset = -1; 
		break;
	}

	
	handle->fd = sock_fd;

#if defined(SIOCGSTAMPNS) && defined(SO_TIMESTAMPNS)
	if (handle->opt.tstamp_precision == PCAP_TSTAMP_PRECISION_NANO) {
		int nsec_tstamps = 1;

		if (setsockopt(handle->fd, SOL_SOCKET, SO_TIMESTAMPNS, &nsec_tstamps, sizeof(nsec_tstamps)) < 0) {
			snprintf(handle->errbuf, PCAP_ERRBUF_SIZE, "setsockopt: unable to set SO_TIMESTAMPNS");
			return PCAP_ERROR;
		}
	}
#endif 

	return 1;
#else 
	strncpy(ebuf,
		"New packet capturing interface not supported by build "
		"environment", PCAP_ERRBUF_SIZE);
	return 0;
#endif 
}

#ifdef HAVE_PACKET_RING
static int 
activate_mmap(pcap_t *handle, int *status)
{
	struct pcap_linux *handlep = handle->priv;
	int ret;

	handlep->oneshot_buffer = malloc(handle->snapshot);
	if (handlep->oneshot_buffer == NULL) {
		snprintf(handle->errbuf, PCAP_ERRBUF_SIZE,
			 "can't allocate oneshot buffer: %s",
			 pcap_strerror(errno));
		*status = PCAP_ERROR;
		return -1;
	}

	if (handle->opt.buffer_size == 0) {
		
		handle->opt.buffer_size = 2*1024*1024;
	}
	ret = prepare_tpacket_socket(handle);
	if (ret == -1) {
		free(handlep->oneshot_buffer);
		*status = PCAP_ERROR;
		return ret;
	}
	ret = create_ring(handle, status);
	if (ret == 0) {
		free(handlep->oneshot_buffer);
		return 0;
	}
	if (ret == -1) {
		free(handlep->oneshot_buffer);
		return -1;
	}


	switch (handlep->tp_version) {
	case TPACKET_V1:
		handle->read_op = pcap_read_linux_mmap_v1;
		break;
#ifdef HAVE_TPACKET2
	case TPACKET_V2:
		handle->read_op = pcap_read_linux_mmap_v2;
		break;
#endif
#ifdef HAVE_TPACKET3
	case TPACKET_V3:
		handle->read_op = pcap_read_linux_mmap_v3;
		break;
#endif
	}
	handle->cleanup_op = pcap_cleanup_linux_mmap;
	handle->setfilter_op = pcap_setfilter_linux_mmap;
	handle->setnonblock_op = pcap_setnonblock_mmap;
	handle->getnonblock_op = pcap_getnonblock_mmap;
	handle->oneshot_callback = pcap_oneshot_mmap;
	handle->selectable_fd = handle->fd;
	return 1;
}
#else 
static int 
activate_mmap(pcap_t *handle _U_, int *status _U_)
{
	return 0;
}
#endif 

#ifdef HAVE_PACKET_RING

#if defined(HAVE_TPACKET2) || defined(HAVE_TPACKET3)
static int
init_tpacket(pcap_t *handle, int version, const char *version_str)
{
	struct pcap_linux *handlep = handle->priv;
	int val = version;
	socklen_t len = sizeof(val);

	
	if (getsockopt(handle->fd, SOL_PACKET, PACKET_HDRLEN, &val, &len) < 0) {
		if (errno == ENOPROTOOPT || errno == EINVAL)
			return 1;	

		
		snprintf(handle->errbuf, PCAP_ERRBUF_SIZE,
			"can't get %s header len on packet socket: %s",
			version_str,
			pcap_strerror(errno));
		return -1;
	}
	handlep->tp_hdrlen = val;

	val = version;
	if (setsockopt(handle->fd, SOL_PACKET, PACKET_VERSION, &val,
			   sizeof(val)) < 0) {
		snprintf(handle->errbuf, PCAP_ERRBUF_SIZE,
			"can't activate %s on packet socket: %s",
			version_str,
			pcap_strerror(errno));
		return -1;
	}
	handlep->tp_version = version;

	
	val = VLAN_TAG_LEN;
	if (setsockopt(handle->fd, SOL_PACKET, PACKET_RESERVE, &val,
			   sizeof(val)) < 0) {
		snprintf(handle->errbuf, PCAP_ERRBUF_SIZE,
			"can't set up reserve on packet socket: %s",
			pcap_strerror(errno));
		return -1;
	}

	return 0;
}
#endif 

static int
prepare_tpacket_socket(pcap_t *handle)
{
	struct pcap_linux *handlep = handle->priv;
#if defined(HAVE_TPACKET2) || defined(HAVE_TPACKET3)
	int ret;
#endif

	handlep->tp_version = TPACKET_V1;
	handlep->tp_hdrlen = sizeof(struct tpacket_hdr);

#ifdef HAVE_TPACKET3
	if (handle->opt.immediate)
		ret = 1; 
	else
		ret = init_tpacket(handle, TPACKET_V3, "TPACKET_V3");
	if (-1 == ret) {
		
		return -1;
	} else if (1 == ret) {
		
#endif 

#ifdef HAVE_TPACKET2
		ret = init_tpacket(handle, TPACKET_V2, "TPACKET_V2");
		if (-1 == ret) {
			
			return -1;
		}
#endif 

#ifdef HAVE_TPACKET3
	}
#endif 

	return 1;
}

static int
create_ring(pcap_t *handle, int *status)
{
	struct pcap_linux *handlep = handle->priv;
	unsigned i, j, frames_per_block;
#ifdef HAVE_TPACKET3
	struct tpacket_req3 req;
#else
	struct tpacket_req req;
#endif
	socklen_t len;
	unsigned int sk_type, tp_reserve, maclen, tp_hdrlen, netoff, macoff;
	unsigned int frame_size;

	*status = 0;

	switch (handlep->tp_version) {

	case TPACKET_V1:
#ifdef HAVE_TPACKET2
	case TPACKET_V2:
#endif
		frame_size = handle->snapshot;
		if (handle->linktype == DLT_EN10MB) {
			int mtu;
			int offload;

			offload = iface_get_offload(handle);
			if (offload == -1) {
				*status = PCAP_ERROR;
				return -1;
			}
			if (!offload) {
				mtu = iface_get_mtu(handle->fd, handle->opt.source,
				    handle->errbuf);
				if (mtu == -1) {
					*status = PCAP_ERROR;
					return -1;
				}
				if (frame_size > mtu + 18)
					frame_size = mtu + 18;
			}
		}

		len = sizeof(sk_type);
		if (getsockopt(handle->fd, SOL_SOCKET, SO_TYPE, &sk_type,
		    &len) < 0) {
			snprintf(handle->errbuf, PCAP_ERRBUF_SIZE,
			    "getsockopt: %s", pcap_strerror(errno));
			*status = PCAP_ERROR;
			return -1;
		}
#ifdef PACKET_RESERVE
		len = sizeof(tp_reserve);
		if (getsockopt(handle->fd, SOL_PACKET, PACKET_RESERVE,
		    &tp_reserve, &len) < 0) {
			if (errno != ENOPROTOOPT) {
				snprintf(handle->errbuf, PCAP_ERRBUF_SIZE,
				    "getsockopt: %s", pcap_strerror(errno));
				*status = PCAP_ERROR;
				return -1;
			}
			tp_reserve = 0;	
		}
#else
		tp_reserve = 0;	
#endif
		maclen = (sk_type == SOCK_DGRAM) ? 0 : MAX_LINKHEADER_SIZE;
		tp_hdrlen = TPACKET_ALIGN(handlep->tp_hdrlen) + sizeof(struct sockaddr_ll) ;
		netoff = TPACKET_ALIGN(tp_hdrlen + (maclen < 16 ? 16 : maclen)) + tp_reserve;
		macoff = netoff - maclen;
		req.tp_frame_size = TPACKET_ALIGN(macoff + frame_size);
		req.tp_frame_nr = handle->opt.buffer_size/req.tp_frame_size;
		break;

#ifdef HAVE_TPACKET3
	case TPACKET_V3:
		req.tp_frame_size = 131072;
		req.tp_frame_nr = handle->opt.buffer_size/req.tp_frame_size;
		break;
#endif
	}

	req.tp_block_size = getpagesize();
	while (req.tp_block_size < req.tp_frame_size) 
		req.tp_block_size <<= 1;

	frames_per_block = req.tp_block_size/req.tp_frame_size;

#if defined(HAVE_LINUX_NET_TSTAMP_H) && defined(PACKET_TIMESTAMP)
	if (handle->opt.tstamp_type == PCAP_TSTAMP_ADAPTER ||
	    handle->opt.tstamp_type == PCAP_TSTAMP_ADAPTER_UNSYNCED) {
		struct hwtstamp_config hwconfig;
		struct ifreq ifr;
		int timesource;

		memset(&hwconfig, 0, sizeof(hwconfig));
		hwconfig.tx_type = HWTSTAMP_TX_ON;
		hwconfig.rx_filter = HWTSTAMP_FILTER_ALL;

		memset(&ifr, 0, sizeof(ifr));
		strcpy(ifr.ifr_name, handle->opt.source);
		ifr.ifr_data = (void *)&hwconfig;

		if (ioctl(handle->fd, SIOCSHWTSTAMP, &ifr) < 0) {
			switch (errno) {

			case EPERM:
				*status = PCAP_ERROR_PERM_DENIED;
				return -1;

			case EOPNOTSUPP:
				*status = PCAP_WARNING_TSTAMP_TYPE_NOTSUP;
				break;

			default:
				snprintf(handle->errbuf, PCAP_ERRBUF_SIZE,
					"SIOCSHWTSTAMP failed: %s",
					pcap_strerror(errno));
				*status = PCAP_ERROR;
				return -1;
			}
		} else {
			if (handle->opt.tstamp_type == PCAP_TSTAMP_ADAPTER) {
				timesource = SOF_TIMESTAMPING_SYS_HARDWARE;
			} else {
				timesource = SOF_TIMESTAMPING_RAW_HARDWARE;
			}
			if (setsockopt(handle->fd, SOL_PACKET, PACKET_TIMESTAMP,
				(void *)&timesource, sizeof(timesource))) {
				snprintf(handle->errbuf, PCAP_ERRBUF_SIZE, 
					"can't set PACKET_TIMESTAMP: %s", 
					pcap_strerror(errno));
				*status = PCAP_ERROR;
				return -1;
			}
		}
	}
#endif 

	
retry:
	req.tp_block_nr = req.tp_frame_nr / frames_per_block;

	
	req.tp_frame_nr = req.tp_block_nr * frames_per_block;
	
#ifdef HAVE_TPACKET3
	
	req.tp_retire_blk_tov = (handlep->timeout>=0)?handlep->timeout:0;
	
	req.tp_sizeof_priv = 0;
	
	req.tp_feature_req_word = 0;
#endif

	if (setsockopt(handle->fd, SOL_PACKET, PACKET_RX_RING,
					(void *) &req, sizeof(req))) {
		if ((errno == ENOMEM) && (req.tp_block_nr > 1)) {
			if (req.tp_frame_nr < 20)
				req.tp_frame_nr -= 1;
			else
				req.tp_frame_nr -= req.tp_frame_nr/20;
			goto retry;
		}
		if (errno == ENOPROTOOPT) {
			return 0;
		}
		snprintf(handle->errbuf, PCAP_ERRBUF_SIZE,
		    "can't create rx ring on packet socket: %s",
		    pcap_strerror(errno));
		*status = PCAP_ERROR;
		return -1;
	}

	
	handlep->mmapbuflen = req.tp_block_nr * req.tp_block_size;
	handlep->mmapbuf = mmap(0, handlep->mmapbuflen,
	    PROT_READ|PROT_WRITE, MAP_SHARED, handle->fd, 0);
	if (handlep->mmapbuf == MAP_FAILED) {
		snprintf(handle->errbuf, PCAP_ERRBUF_SIZE,
		    "can't mmap rx ring: %s", pcap_strerror(errno));

		
		destroy_ring(handle);
		*status = PCAP_ERROR;
		return -1;
	}

	
	handle->cc = req.tp_frame_nr;
	handle->buffer = malloc(handle->cc * sizeof(union thdr *));
	if (!handle->buffer) {
		snprintf(handle->errbuf, PCAP_ERRBUF_SIZE,
		    "can't allocate ring of frame headers: %s",
		    pcap_strerror(errno));

		destroy_ring(handle);
		*status = PCAP_ERROR;
		return -1;
	}

	
	handle->offset = 0;
	for (i=0; i<req.tp_block_nr; ++i) {
		void *base = &handlep->mmapbuf[i*req.tp_block_size];
		for (j=0; j<frames_per_block; ++j, ++handle->offset) {
			RING_GET_FRAME(handle) = base;
			base += req.tp_frame_size;
		}
	}

	handle->bufsize = req.tp_frame_size;
	handle->offset = 0;
	return 1;
}

static void
destroy_ring(pcap_t *handle)
{
	struct pcap_linux *handlep = handle->priv;

	
	struct tpacket_req req;
	memset(&req, 0, sizeof(req));
	setsockopt(handle->fd, SOL_PACKET, PACKET_RX_RING,
				(void *) &req, sizeof(req));

	
	if (handlep->mmapbuf) {
		
		munmap(handlep->mmapbuf, handlep->mmapbuflen);
		handlep->mmapbuf = NULL;
	}
}

static void
pcap_oneshot_mmap(u_char *user, const struct pcap_pkthdr *h,
    const u_char *bytes)
{
	struct oneshot_userdata *sp = (struct oneshot_userdata *)user;
	pcap_t *handle = sp->pd;
	struct pcap_linux *handlep = handle->priv;

	*sp->hdr = *h;
	memcpy(handlep->oneshot_buffer, bytes, h->caplen);
	*sp->pkt = handlep->oneshot_buffer;
}
    
static void
pcap_cleanup_linux_mmap( pcap_t *handle )
{
	struct pcap_linux *handlep = handle->priv;

	destroy_ring(handle);
	if (handlep->oneshot_buffer != NULL) {
		free(handlep->oneshot_buffer);
		handlep->oneshot_buffer = NULL;
	}
	pcap_cleanup_linux(handle);
}


static int
pcap_getnonblock_mmap(pcap_t *p, char *errbuf)
{
	struct pcap_linux *handlep = p->priv;

	
	return (handlep->timeout<0);
}

static int
pcap_setnonblock_mmap(pcap_t *p, int nonblock, char *errbuf)
{
	struct pcap_linux *handlep = p->priv;

	if (nonblock) {
		if (handlep->timeout >= 0) {
			handlep->timeout = ~handlep->timeout;
		}
	} else {
		if (handlep->timeout < 0) {
			handlep->timeout = ~handlep->timeout;
		}
	}
	return 0;
}

static inline union thdr *
pcap_get_ring_frame(pcap_t *handle, int status)
{
	struct pcap_linux *handlep = handle->priv;
	union thdr h;

	h.raw = RING_GET_FRAME(handle);
	switch (handlep->tp_version) {
	case TPACKET_V1:
		if (status != (h.h1->tp_status ? TP_STATUS_USER :
						TP_STATUS_KERNEL))
			return NULL;
		break;
#ifdef HAVE_TPACKET2
	case TPACKET_V2:
		if (status != (h.h2->tp_status ? TP_STATUS_USER :
						TP_STATUS_KERNEL))
			return NULL;
		break;
#endif
#ifdef HAVE_TPACKET3
	case TPACKET_V3:
		if (status != (h.h3->hdr.bh1.block_status ? TP_STATUS_USER :
						TP_STATUS_KERNEL))
			return NULL;
		break;
#endif
	}
	return h.raw;
}

#ifndef POLLRDHUP
#define POLLRDHUP 0
#endif

static int pcap_wait_for_frames_mmap(pcap_t *handle)
{
	if (!pcap_get_ring_frame(handle, TP_STATUS_USER)) {
		struct pcap_linux *handlep = handle->priv;
		int timeout;
		char c;
		struct pollfd pollinfo;
		int ret;

		pollinfo.fd = handle->fd;
		pollinfo.events = POLLIN;

		if (handlep->timeout == 0) {
#ifdef HAVE_TPACKET3
			if (handlep->tp_version == TPACKET_V3)
				timeout = 1;	
			else
#endif
				timeout = -1;	
		} else if (handlep->timeout > 0)
			timeout = handlep->timeout;	
		else
			timeout = 0;	
		do {
			ret = poll(&pollinfo, 1, timeout);
			if (ret < 0 && errno != EINTR) {
				snprintf(handle->errbuf, PCAP_ERRBUF_SIZE,
					"can't poll on packet socket: %s",
					pcap_strerror(errno));
				return PCAP_ERROR;
			} else if (ret > 0 &&
				(pollinfo.revents & (POLLHUP|POLLRDHUP|POLLERR|POLLNVAL))) {
				if (pollinfo.revents & (POLLHUP | POLLRDHUP)) {
					snprintf(handle->errbuf,
						PCAP_ERRBUF_SIZE,
						"Hangup on packet socket");
					return PCAP_ERROR;
				}
				if (pollinfo.revents & POLLERR) {
					if (recv(handle->fd, &c, sizeof c,
						MSG_PEEK) != -1)
						continue;	
					if (errno == ENETDOWN) {
						snprintf(handle->errbuf,
							PCAP_ERRBUF_SIZE,
							"The interface went down");
					} else {
						snprintf(handle->errbuf,
							PCAP_ERRBUF_SIZE,
							"Error condition on packet socket: %s",
							strerror(errno));
					}
					return PCAP_ERROR;
				}
				if (pollinfo.revents & POLLNVAL) {
					snprintf(handle->errbuf,
						PCAP_ERRBUF_SIZE,
						"Invalid polling request on packet socket");
					return PCAP_ERROR;
				}
			}
			
			if (handle->break_loop) {
				handle->break_loop = 0;
				return PCAP_ERROR_BREAK;
			}
		} while (ret < 0);
	}
	return 0;
}

static int pcap_handle_packet_mmap(
		pcap_t *handle,
		pcap_handler callback,
		u_char *user,
		unsigned char *frame,
		unsigned int tp_len,
		unsigned int tp_mac,
		unsigned int tp_snaplen,
		unsigned int tp_sec,
		unsigned int tp_usec,
		int tp_vlan_tci_valid,
		__u16 tp_vlan_tci)
{
	struct pcap_linux *handlep = handle->priv;
	unsigned char *bp;
	struct sockaddr_ll *sll;
	struct pcap_pkthdr pcaphdr;

	
	if (tp_mac + tp_snaplen > handle->bufsize) {
		snprintf(handle->errbuf, PCAP_ERRBUF_SIZE,
			"corrupted frame on kernel ring mac "
			"offset %d + caplen %d > frame len %d",
			tp_mac, tp_snaplen, handle->bufsize);
		return -1;
	}

	bp = frame + tp_mac;
	if (handlep->filter_in_userland && handle->fcode.bf_insns &&
			(bpf_filter(handle->fcode.bf_insns, bp,
				tp_len, tp_snaplen) == 0))
		return 0;

	sll = (void *)frame + TPACKET_ALIGN(handlep->tp_hdrlen);
	if (!linux_check_direction(handle, sll))
		return 0;

	
	pcaphdr.ts.tv_sec = tp_sec;
	pcaphdr.ts.tv_usec = tp_usec;
	pcaphdr.caplen = tp_snaplen;
	pcaphdr.len = tp_len;

	
	if (handlep->cooked) {
		struct sll_header *hdrp;

		bp -= SLL_HDR_LEN;

		if (bp < (u_char *)frame +
				   TPACKET_ALIGN(handlep->tp_hdrlen) +
				   sizeof(struct sockaddr_ll)) {
			snprintf(handle->errbuf, PCAP_ERRBUF_SIZE,
				"cooked-mode frame doesn't have room for sll header");
			return -1;
		}

		hdrp = (struct sll_header *)bp;
		hdrp->sll_pkttype = map_packet_type_to_sll_type(
						sll->sll_pkttype);
		hdrp->sll_hatype = htons(sll->sll_hatype);
		hdrp->sll_halen = htons(sll->sll_halen);
		memcpy(hdrp->sll_addr, sll->sll_addr, SLL_ADDRLEN);
		hdrp->sll_protocol = sll->sll_protocol;

		
		pcaphdr.caplen += SLL_HDR_LEN;
		pcaphdr.len += SLL_HDR_LEN;
	}

#if defined(HAVE_TPACKET2) || defined(HAVE_TPACKET3)
	if (tp_vlan_tci_valid &&
		handlep->vlan_offset != -1 &&
		tp_snaplen >= (unsigned int) handlep->vlan_offset)
	{
		struct vlan_tag *tag;

		bp -= VLAN_TAG_LEN;
		memmove(bp, bp + VLAN_TAG_LEN, handlep->vlan_offset);

		tag = (struct vlan_tag *)(bp + handlep->vlan_offset);
		tag->vlan_tpid = htons(ETH_P_8021Q);
		tag->vlan_tci = htons(tp_vlan_tci);

		pcaphdr.caplen += VLAN_TAG_LEN;
		pcaphdr.len += VLAN_TAG_LEN;
	}
#endif

	if (pcaphdr.caplen > handle->snapshot)
		pcaphdr.caplen = handle->snapshot;

	
	callback(user, &pcaphdr, bp);

	return 1;
}

static int
pcap_read_linux_mmap_v1(pcap_t *handle, int max_packets, pcap_handler callback,
		u_char *user)
{
	struct pcap_linux *handlep = handle->priv;
	int pkts = 0;
	int ret;

	
	ret = pcap_wait_for_frames_mmap(handle);
	if (ret) {
		return ret;
	}

	while ((pkts < max_packets) || PACKET_COUNT_IS_UNLIMITED(max_packets)) {
		union thdr h;

		h.raw = pcap_get_ring_frame(handle, TP_STATUS_USER);
		if (!h.raw)
			break;

		ret = pcap_handle_packet_mmap(
				handle,
				callback,
				user,
				h.raw,
				h.h1->tp_len,
				h.h1->tp_mac,
				h.h1->tp_snaplen,
				h.h1->tp_sec,
				h.h1->tp_usec,
				0,
				0);
		if (ret == 1) {
			pkts++;
			handlep->packets_read++;
		} else if (ret < 0) {
			return ret;
		}

		h.h1->tp_status = TP_STATUS_KERNEL;
		if (handlep->blocks_to_filter_in_userland > 0) {
			handlep->blocks_to_filter_in_userland--;
			if (handlep->blocks_to_filter_in_userland == 0) {
				handlep->filter_in_userland = 0;
			}
		}

		
		if (++handle->offset >= handle->cc)
			handle->offset = 0;

		
		if (handle->break_loop) {
			handle->break_loop = 0;
			return PCAP_ERROR_BREAK;
		}
	}
	return pkts;
}

#ifdef HAVE_TPACKET2
static int
pcap_read_linux_mmap_v2(pcap_t *handle, int max_packets, pcap_handler callback,
		u_char *user)
{
	struct pcap_linux *handlep = handle->priv;
	int pkts = 0;
	int ret;

	
	ret = pcap_wait_for_frames_mmap(handle);
	if (ret) {
		return ret;
	}

	while ((pkts < max_packets) || PACKET_COUNT_IS_UNLIMITED(max_packets)) {
		union thdr h;

		h.raw = pcap_get_ring_frame(handle, TP_STATUS_USER);
		if (!h.raw)
			break;

		ret = pcap_handle_packet_mmap(
				handle,
				callback,
				user,
				h.raw,
				h.h2->tp_len,
				h.h2->tp_mac,
				h.h2->tp_snaplen,
				h.h2->tp_sec,
				handle->opt.tstamp_precision == PCAP_TSTAMP_PRECISION_NANO ? h.h2->tp_nsec : h.h2->tp_nsec / 1000,
#if defined(TP_STATUS_VLAN_VALID)
				(h.h2->tp_vlan_tci || (h.h2->tp_status & TP_STATUS_VLAN_VALID)),
#else
				h.h2->tp_vlan_tci != 0,
#endif
				h.h2->tp_vlan_tci);
		if (ret == 1) {
			pkts++;
			handlep->packets_read++;
		} else if (ret < 0) {
			return ret;
		}

		h.h2->tp_status = TP_STATUS_KERNEL;
		if (handlep->blocks_to_filter_in_userland > 0) {
			handlep->blocks_to_filter_in_userland--;
			if (handlep->blocks_to_filter_in_userland == 0) {
				handlep->filter_in_userland = 0;
			}
		}

		
		if (++handle->offset >= handle->cc)
			handle->offset = 0;

		
		if (handle->break_loop) {
			handle->break_loop = 0;
			return PCAP_ERROR_BREAK;
		}
	}
	return pkts;
}
#endif 

#ifdef HAVE_TPACKET3
static int
pcap_read_linux_mmap_v3(pcap_t *handle, int max_packets, pcap_handler callback,
		u_char *user)
{
	struct pcap_linux *handlep = handle->priv;
	union thdr h;
	int pkts = 0;
	int ret;

	if (handlep->current_packet == NULL) {
		
		ret = pcap_wait_for_frames_mmap(handle);
		if (ret) {
			return ret;
		}
	}
	h.raw = pcap_get_ring_frame(handle, TP_STATUS_USER);
	if (!h.raw)
		return pkts;

	while ((pkts < max_packets) || PACKET_COUNT_IS_UNLIMITED(max_packets)) {
		if (handlep->current_packet == NULL) {
			h.raw = pcap_get_ring_frame(handle, TP_STATUS_USER);
			if (!h.raw)
				break;

			handlep->current_packet = h.raw + h.h3->hdr.bh1.offset_to_first_pkt;
			handlep->packets_left = h.h3->hdr.bh1.num_pkts;
		}
		int packets_to_read = handlep->packets_left;

		if (!PACKET_COUNT_IS_UNLIMITED(max_packets) && packets_to_read > max_packets) {
			packets_to_read = max_packets;
		}

		while(packets_to_read--) {
			struct tpacket3_hdr* tp3_hdr = (struct tpacket3_hdr*) handlep->current_packet;
			ret = pcap_handle_packet_mmap(
					handle,
					callback,
					user,
					handlep->current_packet,
					tp3_hdr->tp_len,
					tp3_hdr->tp_mac,
					tp3_hdr->tp_snaplen,
					tp3_hdr->tp_sec,
					handle->opt.tstamp_precision == PCAP_TSTAMP_PRECISION_NANO ? tp3_hdr->tp_nsec : tp3_hdr->tp_nsec / 1000,
#if defined(TP_STATUS_VLAN_VALID)
					(tp3_hdr->hv1.tp_vlan_tci || (tp3_hdr->tp_status & TP_STATUS_VLAN_VALID)),
#else
					tp3_hdr->hv1.tp_vlan_tci != 0,
#endif
					tp3_hdr->hv1.tp_vlan_tci);
			if (ret == 1) {
				pkts++;
				handlep->packets_read++;
			} else if (ret < 0) {
				handlep->current_packet = NULL;
				return ret;
			}
			handlep->current_packet += tp3_hdr->tp_next_offset;
			handlep->packets_left--;
		}

		if (handlep->packets_left <= 0) {
			h.h3->hdr.bh1.block_status = TP_STATUS_KERNEL;
			if (handlep->blocks_to_filter_in_userland > 0) {
				handlep->blocks_to_filter_in_userland--;
				if (handlep->blocks_to_filter_in_userland == 0) {
					handlep->filter_in_userland = 0;
				}
			}

			
			if (++handle->offset >= handle->cc)
				handle->offset = 0;

			handlep->current_packet = NULL;
		}

		
		if (handle->break_loop) {
			handle->break_loop = 0;
			return PCAP_ERROR_BREAK;
		}
	}
	return pkts;
}
#endif 

static int 
pcap_setfilter_linux_mmap(pcap_t *handle, struct bpf_program *filter)
{
	struct pcap_linux *handlep = handle->priv;
	int n, offset;
	int ret;

	ret = pcap_setfilter_linux_common(handle, filter, 1);
	if (ret < 0)
		return ret;

	if (handlep->filter_in_userland)
		return ret;

	offset = handle->offset;
	if (--handle->offset < 0)
		handle->offset = handle->cc - 1;
	for (n=0; n < handle->cc; ++n) {
		if (--handle->offset < 0)
			handle->offset = handle->cc - 1;
		if (!pcap_get_ring_frame(handle, TP_STATUS_KERNEL))
			break;
	}

	if (n != 0)
		n--;

	
	handle->offset = offset;

	handlep->blocks_to_filter_in_userland = handle->cc - n;
	handlep->filter_in_userland = 1;
	return ret;
}

#endif 


#ifdef HAVE_PF_PACKET_SOCKETS
static int
iface_get_id(int fd, const char *device, char *ebuf)
{
	struct ifreq	ifr;

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, device, sizeof(ifr.ifr_name));

	if (ioctl(fd, SIOCGIFINDEX, &ifr) == -1) {
		snprintf(ebuf, PCAP_ERRBUF_SIZE,
			 "SIOCGIFINDEX: %s", pcap_strerror(errno));
		return -1;
	}

	return ifr.ifr_ifindex;
}

static int
iface_bind(int fd, int ifindex, char *ebuf)
{
	struct sockaddr_ll	sll;
	int			err;
	socklen_t		errlen = sizeof(err);

	memset(&sll, 0, sizeof(sll));
	sll.sll_family		= AF_PACKET;
	sll.sll_ifindex		= ifindex;
	sll.sll_protocol	= htons(ETH_P_ALL);

	if (bind(fd, (struct sockaddr *) &sll, sizeof(sll)) == -1) {
		if (errno == ENETDOWN) {
			return PCAP_ERROR_IFACE_NOT_UP;
		} else {
			snprintf(ebuf, PCAP_ERRBUF_SIZE,
				 "bind: %s", pcap_strerror(errno));
			return PCAP_ERROR;
		}
	}

	

	if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &errlen) == -1) {
		snprintf(ebuf, PCAP_ERRBUF_SIZE,
			"getsockopt: %s", pcap_strerror(errno));
		return 0;
	}

	if (err == ENETDOWN) {
		return PCAP_ERROR_IFACE_NOT_UP;
	} else if (err > 0) {
		snprintf(ebuf, PCAP_ERRBUF_SIZE,
			"bind: %s", pcap_strerror(err));
		return 0;
	}

	return 1;
}

#ifdef IW_MODE_MONITOR
static int
has_wext(int sock_fd, const char *device, char *ebuf)
{
	struct iwreq ireq;

	strncpy(ireq.ifr_ifrn.ifrn_name, device,
	    sizeof ireq.ifr_ifrn.ifrn_name);
	ireq.ifr_ifrn.ifrn_name[sizeof ireq.ifr_ifrn.ifrn_name - 1] = 0;
	if (ioctl(sock_fd, SIOCGIWNAME, &ireq) >= 0)
		return 1;	
	snprintf(ebuf, PCAP_ERRBUF_SIZE,
	    "%s: SIOCGIWPRIV: %s", device, pcap_strerror(errno));
	if (errno == ENODEV)
		return PCAP_ERROR_NO_SUCH_DEVICE;
	return 0;
}

typedef enum {
	MONITOR_WEXT,
	MONITOR_HOSTAP,
	MONITOR_PRISM,
	MONITOR_PRISM54,
	MONITOR_ACX100,
	MONITOR_RT2500,
	MONITOR_RT2570,
	MONITOR_RT73,
	MONITOR_RTL8XXX
} monitor_type;

static int
enter_rfmon_mode_wext(pcap_t *handle, int sock_fd, const char *device)
{
	struct pcap_linux *handlep = handle->priv;
	int err;
	struct iwreq ireq;
	struct iw_priv_args *priv;
	monitor_type montype;
	int i;
	__u32 cmd;
	struct ifreq ifr;
	int oldflags;
	int args[2];
	int channel;

	err = has_wext(sock_fd, device, handle->errbuf);
	if (err <= 0)
		return err;	
	montype = MONITOR_WEXT;
	cmd = 0;

	memset(&ireq, 0, sizeof ireq);
	strncpy(ireq.ifr_ifrn.ifrn_name, device,
	    sizeof ireq.ifr_ifrn.ifrn_name);
	ireq.ifr_ifrn.ifrn_name[sizeof ireq.ifr_ifrn.ifrn_name - 1] = 0;
	ireq.u.data.pointer = (void *)args;
	ireq.u.data.length = 0;
	ireq.u.data.flags = 0;
	if (ioctl(sock_fd, SIOCGIWPRIV, &ireq) != -1) {
		snprintf(handle->errbuf, PCAP_ERRBUF_SIZE,
		    "%s: SIOCGIWPRIV with a zero-length buffer didn't fail!",
		    device);
		return PCAP_ERROR;
	}
	if (errno != EOPNOTSUPP) {
		if (errno != E2BIG) {
			snprintf(handle->errbuf, PCAP_ERRBUF_SIZE,
			    "%s: SIOCGIWPRIV: %s", device,
			    pcap_strerror(errno));
			return PCAP_ERROR;
		}

		priv = malloc(ireq.u.data.length * sizeof (struct iw_priv_args));
		if (priv == NULL) {
			snprintf(handle->errbuf, PCAP_ERRBUF_SIZE,
			    "malloc: %s", pcap_strerror(errno));
			return PCAP_ERROR;
		}
		ireq.u.data.pointer = (void *)priv;
		if (ioctl(sock_fd, SIOCGIWPRIV, &ireq) == -1) {
			snprintf(handle->errbuf, PCAP_ERRBUF_SIZE,
			    "%s: SIOCGIWPRIV: %s", device,
			    pcap_strerror(errno));
			free(priv);
			return PCAP_ERROR;
		}

		for (i = 0; i < ireq.u.data.length; i++) {
			if (strcmp(priv[i].name, "monitor_type") == 0) {
				if ((priv[i].set_args & IW_PRIV_TYPE_MASK) != IW_PRIV_TYPE_INT)
					break;
				if (!(priv[i].set_args & IW_PRIV_SIZE_FIXED))
					break;
				if ((priv[i].set_args & IW_PRIV_SIZE_MASK) != 1)
					break;
				montype = MONITOR_HOSTAP;
				cmd = priv[i].cmd;
				break;
			}
			if (strcmp(priv[i].name, "set_prismhdr") == 0) {
				if ((priv[i].set_args & IW_PRIV_TYPE_MASK) != IW_PRIV_TYPE_INT)
					break;
				if (!(priv[i].set_args & IW_PRIV_SIZE_FIXED))
					break;
				if ((priv[i].set_args & IW_PRIV_SIZE_MASK) != 1)
					break;
				montype = MONITOR_PRISM54;
				cmd = priv[i].cmd;
				break;
			}
			if (strcmp(priv[i].name, "forceprismheader") == 0) {
				if ((priv[i].set_args & IW_PRIV_TYPE_MASK) != IW_PRIV_TYPE_INT)
					break;
				if (!(priv[i].set_args & IW_PRIV_SIZE_FIXED))
					break;
				if ((priv[i].set_args & IW_PRIV_SIZE_MASK) != 1)
					break;
				montype = MONITOR_RT2570;
				cmd = priv[i].cmd;
				break;
			}
			if (strcmp(priv[i].name, "forceprism") == 0) {
				if ((priv[i].set_args & IW_PRIV_TYPE_MASK) != IW_PRIV_TYPE_CHAR)
					break;
				if (priv[i].set_args & IW_PRIV_SIZE_FIXED)
					break;
				montype = MONITOR_RT73;
				cmd = priv[i].cmd;
				break;
			}
			if (strcmp(priv[i].name, "prismhdr") == 0) {
				if ((priv[i].set_args & IW_PRIV_TYPE_MASK) != IW_PRIV_TYPE_INT)
					break;
				if (!(priv[i].set_args & IW_PRIV_SIZE_FIXED))
					break;
				if ((priv[i].set_args & IW_PRIV_SIZE_MASK) != 1)
					break;
				montype = MONITOR_RTL8XXX;
				cmd = priv[i].cmd;
				break;
			}
			if (strcmp(priv[i].name, "rfmontx") == 0) {
				if ((priv[i].set_args & IW_PRIV_TYPE_MASK) != IW_PRIV_TYPE_INT)
					break;
				if ((priv[i].set_args & IW_PRIV_SIZE_MASK) != 2)
					break;
				montype = MONITOR_RT2500;
				cmd = priv[i].cmd;
				break;
			}
			if (strcmp(priv[i].name, "monitor") == 0) {
				if ((priv[i].set_args & IW_PRIV_TYPE_MASK) != IW_PRIV_TYPE_INT)
					break;
				if (!(priv[i].set_args & IW_PRIV_SIZE_FIXED))
					break;
				switch (priv[i].set_args & IW_PRIV_SIZE_MASK) {

				case 1:
					montype = MONITOR_PRISM;
					cmd = priv[i].cmd;
					break;

				case 2:
					montype = MONITOR_ACX100;
					cmd = priv[i].cmd;
					break;

				default:
					break;
				}
			}
		}
		free(priv);
	}


	strncpy(ireq.ifr_ifrn.ifrn_name, device,
	    sizeof ireq.ifr_ifrn.ifrn_name);
	ireq.ifr_ifrn.ifrn_name[sizeof ireq.ifr_ifrn.ifrn_name - 1] = 0;
	if (ioctl(sock_fd, SIOCGIWMODE, &ireq) == -1) {
		return PCAP_ERROR_RFMON_NOTSUP;
	}

	if (ireq.u.mode == IW_MODE_MONITOR) {
		return 1;
	}

	if (!pcap_do_addexit(handle)) {
		return PCAP_ERROR_RFMON_NOTSUP;
	}

	handlep->oldmode = ireq.u.mode;

	if (montype == MONITOR_PRISM) {
		memset(&ireq, 0, sizeof ireq);
		strncpy(ireq.ifr_ifrn.ifrn_name, device,
		    sizeof ireq.ifr_ifrn.ifrn_name);
		ireq.ifr_ifrn.ifrn_name[sizeof ireq.ifr_ifrn.ifrn_name - 1] = 0;
		ireq.u.data.length = 1;	
		args[0] = 3;	
		memcpy(ireq.u.name, args, sizeof (int));
		if (ioctl(sock_fd, cmd, &ireq) != -1) {
			handlep->must_do_on_close |= MUST_CLEAR_RFMON;

			pcap_add_to_pcaps_to_close(handle);

			return 1;
		}

	}

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, device, sizeof(ifr.ifr_name));
	if (ioctl(sock_fd, SIOCGIFFLAGS, &ifr) == -1) {
		snprintf(handle->errbuf, PCAP_ERRBUF_SIZE,
		    "%s: Can't get flags: %s", device, strerror(errno));
		return PCAP_ERROR;
	}
	oldflags = 0;
	if (ifr.ifr_flags & IFF_UP) {
		oldflags = ifr.ifr_flags;
		ifr.ifr_flags &= ~IFF_UP;
		if (ioctl(sock_fd, SIOCSIFFLAGS, &ifr) == -1) {
			snprintf(handle->errbuf, PCAP_ERRBUF_SIZE,
			    "%s: Can't set flags: %s", device, strerror(errno));
			return PCAP_ERROR;
		}
	}

	strncpy(ireq.ifr_ifrn.ifrn_name, device,
	    sizeof ireq.ifr_ifrn.ifrn_name);
	ireq.ifr_ifrn.ifrn_name[sizeof ireq.ifr_ifrn.ifrn_name - 1] = 0;
	ireq.u.mode = IW_MODE_MONITOR;
	if (ioctl(sock_fd, SIOCSIWMODE, &ireq) == -1) {
		ifr.ifr_flags = oldflags;
		if (ioctl(sock_fd, SIOCSIFFLAGS, &ifr) == -1) {
			snprintf(handle->errbuf, PCAP_ERRBUF_SIZE,
			    "%s: Can't set flags: %s", device, strerror(errno));
			return PCAP_ERROR;
		}
		return PCAP_ERROR_RFMON_NOTSUP;
	}


	switch (montype) {

	case MONITOR_WEXT:
		break;

	case MONITOR_HOSTAP:
		memset(&ireq, 0, sizeof ireq);
		strncpy(ireq.ifr_ifrn.ifrn_name, device,
		    sizeof ireq.ifr_ifrn.ifrn_name);
		ireq.ifr_ifrn.ifrn_name[sizeof ireq.ifr_ifrn.ifrn_name - 1] = 0;
		args[0] = 3;	
		memcpy(ireq.u.name, args, sizeof (int));
		if (ioctl(sock_fd, cmd, &ireq) != -1)
			break;	

		memset(&ireq, 0, sizeof ireq);
		strncpy(ireq.ifr_ifrn.ifrn_name, device,
		    sizeof ireq.ifr_ifrn.ifrn_name);
		ireq.ifr_ifrn.ifrn_name[sizeof ireq.ifr_ifrn.ifrn_name - 1] = 0;
		args[0] = 2;	
		memcpy(ireq.u.name, args, sizeof (int));
		if (ioctl(sock_fd, cmd, &ireq) != -1)
			break;	

		memset(&ireq, 0, sizeof ireq);
		strncpy(ireq.ifr_ifrn.ifrn_name, device,
		    sizeof ireq.ifr_ifrn.ifrn_name);
		ireq.ifr_ifrn.ifrn_name[sizeof ireq.ifr_ifrn.ifrn_name - 1] = 0;
		args[0] = 1;	
		memcpy(ireq.u.name, args, sizeof (int));
		ioctl(sock_fd, cmd, &ireq);
		break;

	case MONITOR_PRISM:
		break;

	case MONITOR_PRISM54:
		memset(&ireq, 0, sizeof ireq);
		strncpy(ireq.ifr_ifrn.ifrn_name, device,
		    sizeof ireq.ifr_ifrn.ifrn_name);
		ireq.ifr_ifrn.ifrn_name[sizeof ireq.ifr_ifrn.ifrn_name - 1] = 0;
		args[0] = 3;	
		memcpy(ireq.u.name, args, sizeof (int));
		ioctl(sock_fd, cmd, &ireq);
		break;

	case MONITOR_ACX100:
		memset(&ireq, 0, sizeof ireq);
		strncpy(ireq.ifr_ifrn.ifrn_name, device,
		    sizeof ireq.ifr_ifrn.ifrn_name);
		ireq.ifr_ifrn.ifrn_name[sizeof ireq.ifr_ifrn.ifrn_name - 1] = 0;
		if (ioctl(sock_fd, SIOCGIWFREQ, &ireq) == -1) {
			snprintf(handle->errbuf, PCAP_ERRBUF_SIZE,
			    "%s: SIOCGIWFREQ: %s", device,
			    pcap_strerror(errno));
			return PCAP_ERROR;
		}
		channel = ireq.u.freq.m;

		memset(&ireq, 0, sizeof ireq);
		strncpy(ireq.ifr_ifrn.ifrn_name, device,
		    sizeof ireq.ifr_ifrn.ifrn_name);
		ireq.ifr_ifrn.ifrn_name[sizeof ireq.ifr_ifrn.ifrn_name - 1] = 0;
		args[0] = 1;		
		args[1] = channel;	
		memcpy(ireq.u.name, args, 2*sizeof (int));
		ioctl(sock_fd, cmd, &ireq);
		break;

	case MONITOR_RT2500:
		memset(&ireq, 0, sizeof ireq);
		strncpy(ireq.ifr_ifrn.ifrn_name, device,
		    sizeof ireq.ifr_ifrn.ifrn_name);
		ireq.ifr_ifrn.ifrn_name[sizeof ireq.ifr_ifrn.ifrn_name - 1] = 0;
		args[0] = 0;	
		memcpy(ireq.u.name, args, sizeof (int));
		ioctl(sock_fd, cmd, &ireq);
		break;

	case MONITOR_RT2570:
		memset(&ireq, 0, sizeof ireq);
		strncpy(ireq.ifr_ifrn.ifrn_name, device,
		    sizeof ireq.ifr_ifrn.ifrn_name);
		ireq.ifr_ifrn.ifrn_name[sizeof ireq.ifr_ifrn.ifrn_name - 1] = 0;
		args[0] = 1;	
		memcpy(ireq.u.name, args, sizeof (int));
		ioctl(sock_fd, cmd, &ireq);
		break;

	case MONITOR_RT73:
		memset(&ireq, 0, sizeof ireq);
		strncpy(ireq.ifr_ifrn.ifrn_name, device,
		    sizeof ireq.ifr_ifrn.ifrn_name);
		ireq.ifr_ifrn.ifrn_name[sizeof ireq.ifr_ifrn.ifrn_name - 1] = 0;
		ireq.u.data.length = 1;	
		ireq.u.data.pointer = "1";
		ireq.u.data.flags = 0;
		ioctl(sock_fd, cmd, &ireq);
		break;

	case MONITOR_RTL8XXX:
		memset(&ireq, 0, sizeof ireq);
		strncpy(ireq.ifr_ifrn.ifrn_name, device,
		    sizeof ireq.ifr_ifrn.ifrn_name);
		ireq.ifr_ifrn.ifrn_name[sizeof ireq.ifr_ifrn.ifrn_name - 1] = 0;
		args[0] = 1;	
		memcpy(ireq.u.name, args, sizeof (int));
		ioctl(sock_fd, cmd, &ireq);
		break;
	}

	if (oldflags != 0) {
		ifr.ifr_flags = oldflags;
		if (ioctl(sock_fd, SIOCSIFFLAGS, &ifr) == -1) {
			snprintf(handle->errbuf, PCAP_ERRBUF_SIZE,
			    "%s: Can't set flags: %s", device, strerror(errno));

			if (ioctl(handle->fd, SIOCSIWMODE, &ireq) == -1) {
				fprintf(stderr,
				    "Can't restore interface wireless mode (SIOCSIWMODE failed: %s).\n"
				    "Please adjust manually.\n",
				    strerror(errno));
			}
			return PCAP_ERROR;
		}
	}

	handlep->must_do_on_close |= MUST_CLEAR_RFMON;

	pcap_add_to_pcaps_to_close(handle);

	return 1;
}
#endif 

static int
enter_rfmon_mode(pcap_t *handle, int sock_fd, const char *device)
{
#if defined(HAVE_LIBNL) || defined(IW_MODE_MONITOR)
	int ret;
#endif

#ifdef HAVE_LIBNL
	ret = enter_rfmon_mode_mac80211(handle, sock_fd, device);
	if (ret < 0)
		return ret;	
	if (ret == 1)
		return 1;	
#endif 

#ifdef IW_MODE_MONITOR
	ret = enter_rfmon_mode_wext(handle, sock_fd, device);
	if (ret < 0)
		return ret;	
	if (ret == 1)
		return 1;	
#endif 

	return 0;
}

#if defined(SIOCETHTOOL) && (defined(ETHTOOL_GTSO) || defined(ETHTOOL_GUFO) || defined(ETHTOOL_GGSO) || defined(ETHTOOL_GFLAGS) || defined(ETHTOOL_GGRO))
static int
iface_ethtool_ioctl(pcap_t *handle, int cmd, const char *cmdname)
{
	struct ifreq	ifr;
	struct ethtool_value eval;

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, handle->opt.source, sizeof(ifr.ifr_name));
	eval.cmd = cmd;
	eval.data = 0;
	ifr.ifr_data = (caddr_t)&eval;
	if (ioctl(handle->fd, SIOCETHTOOL, &ifr) == -1) {
		if (errno == EOPNOTSUPP || errno == EINVAL) {
			return 0;
		}
		snprintf(handle->errbuf, PCAP_ERRBUF_SIZE,
		    "%s: SIOETHTOOL(%s) ioctl failed: %s", handle->opt.source,
		    cmdname, strerror(errno));
		return -1;
	}
	return eval.data;	
}

static int
iface_get_offload(pcap_t *handle)
{
	int ret;

#ifdef ETHTOOL_GTSO
	ret = iface_ethtool_ioctl(handle, ETHTOOL_GTSO, "ETHTOOL_GTSO");
	if (ret == -1)
		return -1;
	if (ret)
		return 1;	
#endif

#ifdef ETHTOOL_GUFO
	ret = iface_ethtool_ioctl(handle, ETHTOOL_GUFO, "ETHTOOL_GUFO");
	if (ret == -1)
		return -1;
	if (ret)
		return 1;	
#endif

#ifdef ETHTOOL_GGSO
	ret = iface_ethtool_ioctl(handle, ETHTOOL_GGSO, "ETHTOOL_GGSO");
	if (ret == -1)
		return -1;
	if (ret)
		return 1;	
#endif

#ifdef ETHTOOL_GFLAGS
	ret = iface_ethtool_ioctl(handle, ETHTOOL_GFLAGS, "ETHTOOL_GFLAGS");
	if (ret == -1)
		return -1;
	if (ret & ETH_FLAG_LRO)
		return 1;	
#endif

#ifdef ETHTOOL_GGRO
	ret = iface_ethtool_ioctl(handle, ETHTOOL_GGRO, "ETHTOOL_GGRO");
	if (ret == -1)
		return -1;
	if (ret)
		return 1;	
#endif

	return 0;
}
#else 
static int
iface_get_offload(pcap_t *handle _U_)
{
	return 0;
}
#endif 

#endif 


static int
activate_old(pcap_t *handle)
{
	struct pcap_linux *handlep = handle->priv;
	int		arptype;
	struct ifreq	ifr;
	const char	*device = handle->opt.source;
	struct utsname	utsname;
	int		mtu;

	

	handle->fd = socket(PF_INET, SOCK_PACKET, htons(ETH_P_ALL));
	if (handle->fd == -1) {
		snprintf(handle->errbuf, PCAP_ERRBUF_SIZE,
			 "socket: %s", pcap_strerror(errno));
		if (errno == EPERM || errno == EACCES) {
			return PCAP_ERROR_PERM_DENIED;
		} else {
			return PCAP_ERROR;
		}
	}

	
	handlep->sock_packet = 1;

	
	handlep->cooked = 0;

	

	if (strcmp(device, "any") == 0) {
		strncpy(handle->errbuf, "pcap_activate: The \"any\" device isn't supported on 2.0[.x]-kernel systems",
			PCAP_ERRBUF_SIZE);
		return PCAP_ERROR;
	}
	if (iface_bind_old(handle->fd, device, handle->errbuf) == -1)
		return PCAP_ERROR;

	arptype = iface_get_arptype(handle->fd, device, handle->errbuf);
	if (arptype < 0)
		return PCAP_ERROR;

	map_arphrd_to_dlt(handle, arptype, 0);
	if (handle->linktype == -1) {
		snprintf(handle->errbuf, PCAP_ERRBUF_SIZE,
			 "unknown arptype %d", arptype);
		return PCAP_ERROR;
	}

	

	if (handle->opt.promisc) {
		memset(&ifr, 0, sizeof(ifr));
		strncpy(ifr.ifr_name, device, sizeof(ifr.ifr_name));
		if (ioctl(handle->fd, SIOCGIFFLAGS, &ifr) == -1) {
			snprintf(handle->errbuf, PCAP_ERRBUF_SIZE,
				 "SIOCGIFFLAGS: %s", pcap_strerror(errno));
			return PCAP_ERROR;
		}
		if ((ifr.ifr_flags & IFF_PROMISC) == 0) {

			if (!pcap_do_addexit(handle)) {
				return PCAP_ERROR;
			}

			ifr.ifr_flags |= IFF_PROMISC;
			if (ioctl(handle->fd, SIOCSIFFLAGS, &ifr) == -1) {
			        snprintf(handle->errbuf, PCAP_ERRBUF_SIZE,
					 "SIOCSIFFLAGS: %s",
					 pcap_strerror(errno));
				return PCAP_ERROR;
			}
			handlep->must_do_on_close |= MUST_CLEAR_PROMISC;

			pcap_add_to_pcaps_to_close(handle);
		}
	}

	if (uname(&utsname) < 0 ||
	    strncmp(utsname.release, "2.0", 3) == 0) {
		mtu = iface_get_mtu(handle->fd, device, handle->errbuf);
		if (mtu == -1)
			return PCAP_ERROR;
		handle->bufsize = MAX_LINKHEADER_SIZE + mtu;
		if (handle->bufsize < handle->snapshot)
			handle->bufsize = handle->snapshot;
	} else {
		handle->bufsize = handle->snapshot;
	}

	handle->offset	 = 0;

	handlep->vlan_offset = -1; 

	return 1;
}

static int
iface_bind_old(int fd, const char *device, char *ebuf)
{
	struct sockaddr	saddr;
	int		err;
	socklen_t	errlen = sizeof(err);

	memset(&saddr, 0, sizeof(saddr));
	strncpy(saddr.sa_data, device, sizeof(saddr.sa_data));
	if (bind(fd, &saddr, sizeof(saddr)) == -1) {
		snprintf(ebuf, PCAP_ERRBUF_SIZE,
			 "bind: %s", pcap_strerror(errno));
		return -1;
	}

	

	if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &errlen) == -1) {
		snprintf(ebuf, PCAP_ERRBUF_SIZE,
			"getsockopt: %s", pcap_strerror(errno));
		return -1;
	}

	if (err > 0) {
		snprintf(ebuf, PCAP_ERRBUF_SIZE,
			"bind: %s", pcap_strerror(err));
		return -1;
	}

	return 0;
}



static int
iface_get_mtu(int fd, const char *device, char *ebuf)
{
	struct ifreq	ifr;

	if (!device)
		return BIGGER_THAN_ALL_MTUS;

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, device, sizeof(ifr.ifr_name));

	if (ioctl(fd, SIOCGIFMTU, &ifr) == -1) {
		snprintf(ebuf, PCAP_ERRBUF_SIZE,
			 "SIOCGIFMTU: %s", pcap_strerror(errno));
		return -1;
	}

	return ifr.ifr_mtu;
}

static int
iface_get_arptype(int fd, const char *device, char *ebuf)
{
	struct ifreq	ifr;

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, device, sizeof(ifr.ifr_name));

	if (ioctl(fd, SIOCGIFHWADDR, &ifr) == -1) {
		snprintf(ebuf, PCAP_ERRBUF_SIZE,
			 "SIOCGIFHWADDR: %s", pcap_strerror(errno));
		if (errno == ENODEV) {
			return PCAP_ERROR_NO_SUCH_DEVICE;
		}
		return PCAP_ERROR;
	}

	return ifr.ifr_hwaddr.sa_family;
}

#ifdef SO_ATTACH_FILTER
static int
fix_program(pcap_t *handle, struct sock_fprog *fcode, int is_mmapped)
{
	struct pcap_linux *handlep = handle->priv;
	size_t prog_size;
	register int i;
	register struct bpf_insn *p;
	struct bpf_insn *f;
	int len;

	prog_size = sizeof(*handle->fcode.bf_insns) * handle->fcode.bf_len;
	len = handle->fcode.bf_len;
	f = (struct bpf_insn *)malloc(prog_size);
	if (f == NULL) {
		snprintf(handle->errbuf, PCAP_ERRBUF_SIZE,
			 "malloc: %s", pcap_strerror(errno));
		return -1;
	}
	memcpy(f, handle->fcode.bf_insns, prog_size);
	fcode->len = len;
	fcode->filter = (struct sock_filter *) f;

	for (i = 0; i < len; ++i) {
		p = &f[i];
		switch (BPF_CLASS(p->code)) {

		case BPF_RET:
			if (!is_mmapped) {
				if (BPF_MODE(p->code) == BPF_K) {
					if (p->k != 0)
						p->k = 65535;
				}
			}
			break;

		case BPF_LD:
		case BPF_LDX:
			switch (BPF_MODE(p->code)) {

			case BPF_ABS:
			case BPF_IND:
			case BPF_MSH:
				if (handlep->cooked) {
					if (fix_offset(p) < 0) {
						return 0;
					}
				}
				break;
			}
			break;
		}
	}
	return 1;	
}

static int
fix_offset(struct bpf_insn *p)
{
	if (p->k >= SLL_HDR_LEN) {
		p->k -= SLL_HDR_LEN;
	} else if (p->k == 0) {
		p->k = SKF_AD_OFF + SKF_AD_PKTTYPE;
	} else if (p->k == 14) {
		p->k = SKF_AD_OFF + SKF_AD_PROTOCOL;
	} else if ((bpf_int32)(p->k) > 0) {
		return -1;
	}
	return 0;
}

static int
set_kernel_filter(pcap_t *handle, struct sock_fprog *fcode)
{
	int total_filter_on = 0;
	int save_mode;
	int ret;
	int save_errno;

	if (setsockopt(handle->fd, SOL_SOCKET, SO_ATTACH_FILTER,
		       &total_fcode, sizeof(total_fcode)) == 0) {
		char drain[1];

		total_filter_on = 1;

		save_mode = fcntl(handle->fd, F_GETFL, 0);
		if (save_mode != -1 &&
		    fcntl(handle->fd, F_SETFL, save_mode | O_NONBLOCK) >= 0) {
			while (recv(handle->fd, &drain, sizeof drain,
			       MSG_TRUNC) >= 0)
				;
			save_errno = errno;
			fcntl(handle->fd, F_SETFL, save_mode);
			if (save_errno != EAGAIN) {
				
				reset_kernel_filter(handle);
				snprintf(handle->errbuf, PCAP_ERRBUF_SIZE,
				 "recv: %s", pcap_strerror(save_errno));
				return -2;
			}
		}
	}

	ret = setsockopt(handle->fd, SOL_SOCKET, SO_ATTACH_FILTER,
			 fcode, sizeof(*fcode));
	if (ret == -1 && total_filter_on) {
		save_errno = errno;

		reset_kernel_filter(handle);

		errno = save_errno;
	}
	return ret;
}

static int
reset_kernel_filter(pcap_t *handle)
{
	int dummy = 0;

	return setsockopt(handle->fd, SOL_SOCKET, SO_DETACH_FILTER,
				   &dummy, sizeof(dummy));
}
#endif
