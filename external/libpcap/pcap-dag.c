
#ifndef lint
static const char rcsid[] _U_ =
	"@(#) $Header: /tcpdump/master/libpcap/pcap-dag.c,v 1.39 2008-04-14 20:40:58 guy Exp $ (LBL)";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/param.h>			

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "pcap-int.h"

#include <ctype.h>
#include <netinet/in.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

struct mbuf;		
struct rtentry;		
#include <net/if.h>

#include "dagnew.h"
#include "dagapi.h"

#include "pcap-dag.h"

#ifndef DAG_MAX_BOARDS
#define DAG_MAX_BOARDS 32
#endif

#define ATM_CELL_SIZE		52
#define ATM_HDR_SIZE		4

#define MTP2_SENT_OFFSET		0	
#define MTP2_ANNEX_A_USED_OFFSET	1	
#define MTP2_LINK_NUMBER_OFFSET		2	
#define MTP2_HDR_LEN			4	

#define MTP2_ANNEX_A_NOT_USED      0
#define MTP2_ANNEX_A_USED          1
#define MTP2_ANNEX_A_USED_UNKNOWN  2

struct sunatm_hdr {
	unsigned char	flags;		
	unsigned char	vpi;		
	unsigned short	vci;		
};

struct pcap_dag {
	struct pcap_stat stat;
#ifdef HAVE_DAG_STREAMS_API
	u_char	*dag_mem_bottom;	
	u_char	*dag_mem_top;	
#else 
	void	*dag_mem_base;	
	u_int	dag_mem_bottom;	
	u_int	dag_mem_top;	
#endif 
	int	dag_fcs_bits;	
	int	dag_offset_flags; 
	int	dag_stream;	
	int	dag_timeout;	
};

typedef struct pcap_dag_node {
	struct pcap_dag_node *next;
	pcap_t *p;
	pid_t pid;
} pcap_dag_node_t;

static pcap_dag_node_t *pcap_dags = NULL;
static int atexit_handler_installed = 0;
static const unsigned short endian_test_word = 0x0100;

#define IS_BIGENDIAN() (*((unsigned char *)&endian_test_word))

#define MAX_DAG_PACKET 65536

static unsigned char TempPkt[MAX_DAG_PACKET];

static int dag_setfilter(pcap_t *p, struct bpf_program *fp);
static int dag_stats(pcap_t *p, struct pcap_stat *ps);
static int dag_set_datalink(pcap_t *p, int dlt);
static int dag_get_datalink(pcap_t *p);
static int dag_setnonblock(pcap_t *p, int nonblock, char *errbuf);

static void
delete_pcap_dag(pcap_t *p)
{
	pcap_dag_node_t *curr = NULL, *prev = NULL;

	for (prev = NULL, curr = pcap_dags; curr != NULL && curr->p != p; prev = curr, curr = curr->next) {
		
	}

	if (curr != NULL && curr->p == p) {
		if (prev != NULL) {
			prev->next = curr->next;
		} else {
			pcap_dags = curr->next;
		}
	}
}


static void
dag_platform_cleanup(pcap_t *p)
{
	struct pcap_dag *pd;

	if (p != NULL) {
		pd = p->priv;
#ifdef HAVE_DAG_STREAMS_API
		if(dag_stop_stream(p->fd, pd->dag_stream) < 0)
			fprintf(stderr,"dag_stop_stream: %s\n", strerror(errno));
		
		if(dag_detach_stream(p->fd, pd->dag_stream) < 0)
			fprintf(stderr,"dag_detach_stream: %s\n", strerror(errno));
#else
		if(dag_stop(p->fd) < 0)
			fprintf(stderr,"dag_stop: %s\n", strerror(errno));
#endif 
		if(p->fd != -1) {
			if(dag_close(p->fd) < 0)
				fprintf(stderr,"dag_close: %s\n", strerror(errno));
			p->fd = -1;
		}
		delete_pcap_dag(p);
		pcap_cleanup_live_common(p);
	}
	
}

static void
atexit_handler(void)
{
	while (pcap_dags != NULL) {
		if (pcap_dags->pid == getpid()) {
			dag_platform_cleanup(pcap_dags->p);
		} else {
			delete_pcap_dag(pcap_dags->p);
		}
	}
}

static int
new_pcap_dag(pcap_t *p)
{
	pcap_dag_node_t *node = NULL;

	if ((node = malloc(sizeof(pcap_dag_node_t))) == NULL) {
		return -1;
	}

	if (!atexit_handler_installed) {
		atexit(atexit_handler);
		atexit_handler_installed = 1;
	}

	node->next = pcap_dags;
	node->p = p;
	node->pid = getpid();

	pcap_dags = node;

	return 0;
}

static unsigned int
dag_erf_ext_header_count(uint8_t * erf, size_t len)
{
	uint32_t hdr_num = 0;
	uint8_t  hdr_type;

	
	if ( erf == NULL )
		return 0;
	if ( len < 16 )
		return 0;

	
	if ( (erf[8] & 0x80) == 0x00 )
		return 0;

	
	do {
	
		
		if ( len < (24 + (hdr_num * 8)) )
			return hdr_num;

		
		hdr_type = erf[(16 + (hdr_num * 8))];
		hdr_num++;

	} while ( hdr_type & 0x80 );

	return hdr_num;
}

static int
dag_read(pcap_t *p, int cnt, pcap_handler callback, u_char *user)
{
	struct pcap_dag *pd = p->priv;
	unsigned int processed = 0;
	int flags = pd->dag_offset_flags;
	unsigned int nonblocking = flags & DAGF_NONBLOCK;
	unsigned int num_ext_hdr = 0;

	
	while (pd->dag_mem_top - pd->dag_mem_bottom < dag_record_size) {
 
		if (p->break_loop) {
			p->break_loop = 0;
			return -2;
		}

#ifdef HAVE_DAG_STREAMS_API
		if ( NULL == (pd->dag_mem_top = dag_advance_stream(p->fd, pd->dag_stream, &(pd->dag_mem_bottom))) ) {
		     return -1;
		}
#else
		
		pd->dag_mem_top = dag_offset(p->fd, &(pd->dag_mem_bottom), flags);
#endif 

		if (nonblocking && (pd->dag_mem_top - pd->dag_mem_bottom < dag_record_size))
		{
			
			return 0;
		}
		
		if(!nonblocking &&
		   pd->dag_timeout &&
		   (pd->dag_mem_top - pd->dag_mem_bottom < dag_record_size))
		{
			
			return 0;
		}

	}
	
	
	while (pd->dag_mem_top - pd->dag_mem_bottom >= dag_record_size) {
		
		unsigned short packet_len = 0;
		int caplen = 0;
		struct pcap_pkthdr	pcap_header;
		
#ifdef HAVE_DAG_STREAMS_API
		dag_record_t *header = (dag_record_t *)(pd->dag_mem_bottom);
#else
		dag_record_t *header = (dag_record_t *)(pd->dag_mem_base + pd->dag_mem_bottom);
#endif 

		u_char *dp = ((u_char *)header); 
		unsigned short rlen;
 
		if (p->break_loop) {
			p->break_loop = 0;
			return -2;
		}
 
		rlen = ntohs(header->rlen);
		if (rlen < dag_record_size)
		{
			strncpy(p->errbuf, "dag_read: record too small", PCAP_ERRBUF_SIZE);
			return -1;
		}
		pd->dag_mem_bottom += rlen;

		
		switch((header->type & 0x7f)) {
			
		case TYPE_COLOR_HDLC_POS:
		case TYPE_COLOR_ETH:
		case TYPE_DSM_COLOR_HDLC_POS:
		case TYPE_DSM_COLOR_ETH:
		case TYPE_COLOR_MC_HDLC_POS:
		case TYPE_COLOR_HASH_ETH:
		case TYPE_COLOR_HASH_POS:
			break;

		default:
			if (header->lctr) {
				if (pd->stat.ps_drop > (UINT_MAX - ntohs(header->lctr))) {
					pd->stat.ps_drop = UINT_MAX;
				} else {
					pd->stat.ps_drop += ntohs(header->lctr);
				}
			}
		}
		
		if ((header->type & 0x7f) == TYPE_PAD) {
			continue;
		}

		num_ext_hdr = dag_erf_ext_header_count(dp, rlen);

		
		if (p->linktype == DLT_ERF) {
			packet_len = ntohs(header->wlen) + dag_record_size;
			caplen = rlen;
			switch ((header->type & 0x7f)) {
			case TYPE_MC_AAL5:
			case TYPE_MC_ATM:
			case TYPE_MC_HDLC:
			case TYPE_MC_RAW_CHANNEL:
			case TYPE_MC_RAW:
			case TYPE_MC_AAL2:
			case TYPE_COLOR_MC_HDLC_POS:
				packet_len += 4; 
				break;

			case TYPE_COLOR_HASH_ETH:
			case TYPE_DSM_COLOR_ETH:
			case TYPE_COLOR_ETH:
			case TYPE_ETH:
				packet_len += 2; 
				break;
			} 

			
			packet_len += (8 * num_ext_hdr);

			if (caplen > packet_len) {
				caplen = packet_len;
			}
		} else {
			

			
			dp += dag_record_size;
			
			dp += 8 * num_ext_hdr;
			
			switch((header->type & 0x7f)) {
			case TYPE_ATM:
			case TYPE_AAL5:
				if (header->type == TYPE_AAL5) {
					packet_len = ntohs(header->wlen);
					caplen = rlen - dag_record_size;
				}
			case TYPE_MC_ATM:
				if (header->type == TYPE_MC_ATM) {
					caplen = packet_len = ATM_CELL_SIZE;
					dp+=4;
				}
			case TYPE_MC_AAL5:
				if (header->type == TYPE_MC_AAL5) {
					packet_len = ntohs(header->wlen);
					caplen = rlen - dag_record_size - 4;
					dp+=4;
				}
				if (header->type == TYPE_ATM) {
					caplen = packet_len = ATM_CELL_SIZE;
				}
				if (p->linktype == DLT_SUNATM) {
					struct sunatm_hdr *sunatm = (struct sunatm_hdr *)dp;
					unsigned long rawatm;
					
					rawatm = ntohl(*((unsigned long *)dp));
					sunatm->vci = htons((rawatm >>  4) & 0xffff);
					sunatm->vpi = (rawatm >> 20) & 0x00ff;
					sunatm->flags = ((header->flags.iface & 1) ? 0x80 : 0x00) | 
						((sunatm->vpi == 0 && sunatm->vci == htons(5)) ? 6 :
						 ((sunatm->vpi == 0 && sunatm->vci == htons(16)) ? 5 : 
						  ((dp[ATM_HDR_SIZE] == 0xaa &&
						    dp[ATM_HDR_SIZE+1] == 0xaa &&
						    dp[ATM_HDR_SIZE+2] == 0x03) ? 2 : 1)));

				} else {
					packet_len -= ATM_HDR_SIZE;
					caplen -= ATM_HDR_SIZE;
					dp += ATM_HDR_SIZE;
				}
				break;

			case TYPE_COLOR_HASH_ETH:
			case TYPE_DSM_COLOR_ETH:
			case TYPE_COLOR_ETH:
			case TYPE_ETH:
				packet_len = ntohs(header->wlen);
				packet_len -= (pd->dag_fcs_bits >> 3);
				caplen = rlen - dag_record_size - 2;
				if (caplen > packet_len) {
					caplen = packet_len;
				}
				dp += 2;
				break;

			case TYPE_COLOR_HASH_POS:
			case TYPE_DSM_COLOR_HDLC_POS:
			case TYPE_COLOR_HDLC_POS:
			case TYPE_HDLC_POS:
				packet_len = ntohs(header->wlen);
				packet_len -= (pd->dag_fcs_bits >> 3);
				caplen = rlen - dag_record_size;
				if (caplen > packet_len) {
					caplen = packet_len;
				}
				break;

			case TYPE_COLOR_MC_HDLC_POS:
			case TYPE_MC_HDLC:
				packet_len = ntohs(header->wlen);
				packet_len -= (pd->dag_fcs_bits >> 3);
				caplen = rlen - dag_record_size - 4;
				if (caplen > packet_len) {
					caplen = packet_len;
				}
				
				dp += 4;
#ifdef DLT_MTP2_WITH_PHDR
				if (p->linktype == DLT_MTP2_WITH_PHDR) {
					
					caplen += MTP2_HDR_LEN;
					packet_len += MTP2_HDR_LEN;
					
					TempPkt[MTP2_SENT_OFFSET] = 0;
					TempPkt[MTP2_ANNEX_A_USED_OFFSET] = MTP2_ANNEX_A_USED_UNKNOWN;
					*(TempPkt+MTP2_LINK_NUMBER_OFFSET) = ((header->rec.mc_hdlc.mc_header>>16)&0x01);
					*(TempPkt+MTP2_LINK_NUMBER_OFFSET+1) = ((header->rec.mc_hdlc.mc_header>>24)&0xff);
					memcpy(TempPkt+MTP2_HDR_LEN, dp, caplen);
					dp = TempPkt;
				}
#endif
				break;

			case TYPE_IPV4:
			case TYPE_IPV6:
				packet_len = ntohs(header->wlen);
				caplen = rlen - dag_record_size;
				if (caplen > packet_len) {
					caplen = packet_len;
				}
				break;

			
			case TYPE_MC_RAW:
			case TYPE_MC_RAW_CHANNEL:
			case TYPE_IP_COUNTER:
			case TYPE_TCP_FLOW_COUNTER:
			case TYPE_INFINIBAND:
			case TYPE_RAW_LINK:
			case TYPE_INFINIBAND_LINK:
			default:
				continue;
			} 

			
			caplen -= (8 * num_ext_hdr);

		} 
		
		if (caplen > p->snapshot)
			caplen = p->snapshot;

		
		if ((p->fcode.bf_insns == NULL) || bpf_filter(p->fcode.bf_insns, dp, packet_len, caplen)) {
			
			
			register unsigned long long ts;
				
			if (IS_BIGENDIAN()) {
				ts = SWAPLL(header->ts);
			} else {
				ts = header->ts;
			}

			pcap_header.ts.tv_sec = ts >> 32;
			ts = (ts & 0xffffffffULL) * 1000000;
			ts += 0x80000000; 
			pcap_header.ts.tv_usec = ts >> 32;		
			if (pcap_header.ts.tv_usec >= 1000000) {
				pcap_header.ts.tv_usec -= 1000000;
				pcap_header.ts.tv_sec++;
			}

			
			pcap_header.caplen = caplen;
			pcap_header.len = packet_len;
	
			
			pd->stat.ps_recv++;
	
			
			callback(user, &pcap_header, dp);
	
			
			processed++;
			if (processed == cnt && !PACKET_COUNT_IS_UNLIMITED(cnt))
			{
				
				return cnt;
			}
		}
	}

	return processed;
}

static int
dag_inject(pcap_t *p, const void *buf _U_, size_t size _U_)
{
	strlcpy(p->errbuf, "Sending packets isn't supported on DAG cards",
	    PCAP_ERRBUF_SIZE);
	return (-1);
}

static int dag_activate(pcap_t* handle)
{
	struct pcap_dag *handlep = handle->priv;
#if 0
	char conf[30]; 
#endif
	char *s;
	int n;
	daginf_t* daginf;
	char * newDev = NULL;
	char * device = handle->opt.source;
#ifdef HAVE_DAG_STREAMS_API
	uint32_t mindata;
	struct timeval maxwait;
	struct timeval poll;
#endif

	if (device == NULL) {
		snprintf(handle->errbuf, PCAP_ERRBUF_SIZE, "device is NULL: %s", pcap_strerror(errno));
		return -1;
	}

	

#ifdef HAVE_DAG_STREAMS_API
	newDev = (char *)malloc(strlen(device) + 16);
	if (newDev == NULL) {
		snprintf(handle->errbuf, PCAP_ERRBUF_SIZE, "Can't allocate string for device name: %s\n", pcap_strerror(errno));
		goto fail;
	}
	
	
	if (dag_parse_name(device, newDev, strlen(device) + 16, &handlep->dag_stream) < 0) {
		snprintf(handle->errbuf, PCAP_ERRBUF_SIZE, "dag_parse_name: %s\n", pcap_strerror(errno));
		goto fail;
	}
	device = newDev;

	if (handlep->dag_stream%2) {
		snprintf(handle->errbuf, PCAP_ERRBUF_SIZE, "dag_parse_name: tx (even numbered) streams not supported for capture\n");
		goto fail;
	}
#else
	if (strncmp(device, "/dev/", 5) != 0) {
		newDev = (char *)malloc(strlen(device) + 5);
		if (newDev == NULL) {
			snprintf(handle->errbuf, PCAP_ERRBUF_SIZE, "Can't allocate string for device name: %s\n", pcap_strerror(errno));
			goto fail;
		}
		strcpy(newDev, "/dev/");
		strcat(newDev, device);
		device = newDev;
	}
#endif 

	
	if((handle->fd = dag_open((char *)device)) < 0) {
		snprintf(handle->errbuf, PCAP_ERRBUF_SIZE, "dag_open %s: %s", device, pcap_strerror(errno));
		goto fail;
	}

#ifdef HAVE_DAG_STREAMS_API
	
	if (dag_attach_stream(handle->fd, handlep->dag_stream, 0, 0) < 0) {
		snprintf(handle->errbuf, PCAP_ERRBUF_SIZE, "dag_attach_stream: %s\n", pcap_strerror(errno));
		goto failclose;
	}

	if (dag_get_stream_poll(handle->fd, handlep->dag_stream,
				&mindata, &maxwait, &poll) < 0) {
		snprintf(handle->errbuf, PCAP_ERRBUF_SIZE, "dag_get_stream_poll: %s\n", pcap_strerror(errno));
		goto faildetach;
	}
	
	if (handle->opt.immediate) {
		mindata = 0;
	} else {
		mindata = 65536;
	}

	maxwait.tv_sec = handle->opt.timeout/1000;
	maxwait.tv_usec = (handle->opt.timeout%1000) * 1000;

	if (dag_set_stream_poll(handle->fd, handlep->dag_stream,
				mindata, &maxwait, &poll) < 0) {
		snprintf(handle->errbuf, PCAP_ERRBUF_SIZE, "dag_set_stream_poll: %s\n", pcap_strerror(errno));
		goto faildetach;
	}
		
#else
	if((handlep->dag_mem_base = dag_mmap(handle->fd)) == MAP_FAILED) {
		snprintf(handle->errbuf, PCAP_ERRBUF_SIZE,"dag_mmap %s: %s\n", device, pcap_strerror(errno));
		goto failclose;
	}

#endif 

#if 0
	
	if (handle->snapshot == 0 || handle->snapshot > MAX_DAG_SNAPLEN) {
		handle->snapshot = MAX_DAG_SNAPLEN;
	} else if (snaplen < MIN_DAG_SNAPLEN) {
		handle->snapshot = MIN_DAG_SNAPLEN;
	}
	
	snprintf(conf, 30, "varlen slen=%d", (snaplen + 3) & ~3); 

	if(dag_configure(handle->fd, conf) < 0) {
		snprintf(handle->errbuf, PCAP_ERRBUF_SIZE,"dag_configure %s: %s\n", device, pcap_strerror(errno));
		goto faildetach;
	}
#endif	
	
#ifdef HAVE_DAG_STREAMS_API
	if(dag_start_stream(handle->fd, handlep->dag_stream) < 0) {
		snprintf(handle->errbuf, PCAP_ERRBUF_SIZE, "dag_start_stream %s: %s\n", device, pcap_strerror(errno));
		goto faildetach;
	}
#else
	if(dag_start(handle->fd) < 0) {
		snprintf(handle->errbuf, PCAP_ERRBUF_SIZE, "dag_start %s: %s\n", device, pcap_strerror(errno));
		goto failclose;
	}
#endif 

	handlep->dag_mem_bottom = 0;
	handlep->dag_mem_top = 0;

	daginf = dag_info(handle->fd);
	if ((0x4200 == daginf->device_code) || (0x4230 == daginf->device_code))	{
		
		handlep->dag_fcs_bits = 0;

		
		handle->linktype_ext = LT_FCS_DATALINK_EXT(0);
	} else {
		handlep->dag_fcs_bits = 32;

		
		if ((s = getenv("ERF_FCS_BITS")) != NULL) {
			if ((n = atoi(s)) == 0 || n == 16 || n == 32) {
				handlep->dag_fcs_bits = n;
			} else {
				snprintf(handle->errbuf, PCAP_ERRBUF_SIZE,
					"pcap_activate %s: bad ERF_FCS_BITS value (%d) in environment\n", device, n);
				goto failstop;
			}
		}

		if ((s = getenv("ERF_DONT_STRIP_FCS")) != NULL) {
			handle->linktype_ext = LT_FCS_DATALINK_EXT(handlep->dag_fcs_bits/16);

			
			handlep->dag_fcs_bits = 0;
		}
	}

	handlep->dag_timeout	= handle->opt.timeout;

	handle->linktype = -1;
	if (dag_get_datalink(handle) < 0)
		goto failstop;
	
	handle->bufsize = 0;

	if (new_pcap_dag(handle) < 0) {
		snprintf(handle->errbuf, PCAP_ERRBUF_SIZE, "new_pcap_dag %s: %s\n", device, pcap_strerror(errno));
		goto failstop;
	}

	handle->selectable_fd = -1;

	if (newDev != NULL) {
		free((char *)newDev);
	}

	handle->read_op = dag_read;
	handle->inject_op = dag_inject;
	handle->setfilter_op = dag_setfilter;
	handle->setdirection_op = NULL; 
	handle->set_datalink_op = dag_set_datalink;
	handle->getnonblock_op = pcap_getnonblock_fd;
	handle->setnonblock_op = dag_setnonblock;
	handle->stats_op = dag_stats;
	handle->cleanup_op = dag_platform_cleanup;
	handlep->stat.ps_drop = 0;
	handlep->stat.ps_recv = 0;
	handlep->stat.ps_ifdrop = 0;
	return 0;

#ifdef HAVE_DAG_STREAMS_API 
failstop:
	if (dag_stop_stream(handle->fd, handlep->dag_stream) < 0) {
		fprintf(stderr,"dag_stop_stream: %s\n", strerror(errno));
	}
	
faildetach:
	if (dag_detach_stream(handle->fd, handlep->dag_stream) < 0)
		fprintf(stderr,"dag_detach_stream: %s\n", strerror(errno));
#else
failstop:
	if (dag_stop(handle->fd) < 0)
		fprintf(stderr,"dag_stop: %s\n", strerror(errno));
#endif 
	
failclose:
	if (dag_close(handle->fd) < 0)
		fprintf(stderr,"dag_close: %s\n", strerror(errno));
	delete_pcap_dag(handle);

fail:
	pcap_cleanup_live_common(handle);
	if (newDev != NULL) {
		free((char *)newDev);
	}

	return PCAP_ERROR;
}

pcap_t *dag_create(const char *device, char *ebuf, int *is_ours)
{
	const char *cp;
	char *cpend;
	long devnum;
	pcap_t *p;
#ifdef HAVE_DAG_STREAMS_API
	long stream = 0;
#endif

	
	cp = strrchr(device, '/');
	if (cp == NULL)
		cp = device;
	
	if (strncmp(cp, "dag", 3) != 0) {
		
		*is_ours = 0;
		return NULL;
	}
	
	cp += 3;
	devnum = strtol(cp, &cpend, 10);
#ifdef HAVE_DAG_STREAMS_API
	if (*cpend == ':') {
		
		stream = strtol(++cpend, &cpend, 10);
	}
#endif
	if (cpend == cp || *cpend != '\0') {
		
		*is_ours = 0;
		return NULL;
	}
	if (devnum < 0 || devnum >= DAG_MAX_BOARDS) {
		
		*is_ours = 0;
		return NULL;
	}
#ifdef HAVE_DAG_STREAMS_API
	if (stream <0 || stream >= DAG_STREAM_MAX) {
		
		*is_ours = 0;
		return NULL;
	}
#endif

	
	*is_ours = 1;

	p = pcap_create_common(device, ebuf, sizeof (struct pcap_dag));
	if (p == NULL)
		return NULL;

	p->activate_op = dag_activate;
	return p;
}

static int
dag_stats(pcap_t *p, struct pcap_stat *ps) {
	struct pcap_dag *pd = p->priv;

	
	
	
	*ps = pd->stat;
 
	return 0;
}

int
dag_findalldevs(pcap_if_t **devlistp, char *errbuf)
{
	char name[12];	
	int ret = 0;
	int c;
	char dagname[DAGNAME_BUFSIZE];
	int dagstream;
	int dagfd;

	
	for (c = 0; c < DAG_MAX_BOARDS; c++) {
		snprintf(name, 12, "dag%d", c);
		if (-1 == dag_parse_name(name, dagname, DAGNAME_BUFSIZE, &dagstream))
		{
			return -1;
		}
		if ( (dagfd = dag_open(dagname)) >= 0 ) {
			if (pcap_add_if(devlistp, name, 0, NULL, errbuf) == -1) {
				ret = -1;
			}
#ifdef HAVE_DAG_STREAMS_API
			{
				int stream, rxstreams;
				rxstreams = dag_rx_get_stream_count(dagfd);
				for(stream=0;stream<DAG_STREAM_MAX;stream+=2) {
					if (0 == dag_attach_stream(dagfd, stream, 0, 0)) {
						dag_detach_stream(dagfd, stream);

						snprintf(name,  10, "dag%d:%d", c, stream);
						if (pcap_add_if(devlistp, name, 0, NULL, errbuf) == -1) {
							ret = -1;
						}
						
						rxstreams--;
						if(rxstreams <= 0) {
							break;
						}
					}
				}				
			}
#endif  
			dag_close(dagfd);
		}

	}
	return (ret);
}

static int
dag_setfilter(pcap_t *p, struct bpf_program *fp)
{
	if (!p)
		return -1;
	if (!fp) {
		strncpy(p->errbuf, "setfilter: No filter specified",
			sizeof(p->errbuf));
		return -1;
	}

	

	if (install_bpf_program(p, fp) < 0)
		return -1;

	return (0);
}

static int
dag_set_datalink(pcap_t *p, int dlt)
{
	p->linktype = dlt;

	return (0);
}

static int
dag_setnonblock(pcap_t *p, int nonblock, char *errbuf)
{
	struct pcap_dag *pd = p->priv;

	if (pcap_setnonblock_fd(p, nonblock, errbuf) < 0)
		return (-1);
#ifdef HAVE_DAG_STREAMS_API
	{
		uint32_t mindata;
		struct timeval maxwait;
		struct timeval poll;
		
		if (dag_get_stream_poll(p->fd, pd->dag_stream,
					&mindata, &maxwait, &poll) < 0) {
			snprintf(errbuf, PCAP_ERRBUF_SIZE, "dag_get_stream_poll: %s\n", pcap_strerror(errno));
			return -1;
		}
		
		if(nonblock)
			mindata = 0;
		else
			mindata = 65536;
		
		if (dag_set_stream_poll(p->fd, pd->dag_stream,
					mindata, &maxwait, &poll) < 0) {
			snprintf(errbuf, PCAP_ERRBUF_SIZE, "dag_set_stream_poll: %s\n", pcap_strerror(errno));
			return -1;
		}
	}
#endif 
	if (nonblock) {
		pd->dag_offset_flags |= DAGF_NONBLOCK;
	} else {
		pd->dag_offset_flags &= ~DAGF_NONBLOCK;
	}
	return (0);
}
		
static int
dag_get_datalink(pcap_t *p)
{
	struct pcap_dag *pd = p->priv;
	int index=0, dlt_index=0;
	uint8_t types[255];

	memset(types, 0, 255);

	if (p->dlt_list == NULL && (p->dlt_list = malloc(255*sizeof(*(p->dlt_list)))) == NULL) {
		(void)snprintf(p->errbuf, sizeof(p->errbuf), "malloc: %s", pcap_strerror(errno));
		return (-1);
	}

	p->linktype = 0;

#ifdef HAVE_DAG_GET_STREAM_ERF_TYPES
	
	if (dag_get_stream_erf_types(p->fd, pd->dag_stream, types, 255) < 0) {
		snprintf(p->errbuf, sizeof(p->errbuf), "dag_get_stream_erf_types: %s", pcap_strerror(errno));
		return (-1);		
	}
	
	while (types[index]) {

#elif defined HAVE_DAG_GET_ERF_TYPES
	
	if (dag_get_erf_types(p->fd, types, 255) < 0) {
		snprintf(p->errbuf, sizeof(p->errbuf), "dag_get_erf_types: %s", pcap_strerror(errno));
		return (-1);		
	}
	
	while (types[index]) {
#else
	
	types[index] = dag_linktype(p->fd);

	{
#endif
		switch((types[index] & 0x7f)) {

		case TYPE_HDLC_POS:
		case TYPE_COLOR_HDLC_POS:
		case TYPE_DSM_COLOR_HDLC_POS:
		case TYPE_COLOR_HASH_POS:

			if (p->dlt_list != NULL) {
				p->dlt_list[dlt_index++] = DLT_CHDLC;
				p->dlt_list[dlt_index++] = DLT_PPP_SERIAL;
				p->dlt_list[dlt_index++] = DLT_FRELAY;
			}
			if(!p->linktype)
				p->linktype = DLT_CHDLC;
			break;

		case TYPE_ETH:
		case TYPE_COLOR_ETH:
		case TYPE_DSM_COLOR_ETH:
		case TYPE_COLOR_HASH_ETH:
			if (p->dlt_list != NULL) {
				p->dlt_list[dlt_index++] = DLT_EN10MB;
				p->dlt_list[dlt_index++] = DLT_DOCSIS;
			}
			if(!p->linktype)
				p->linktype = DLT_EN10MB;
			break;

		case TYPE_ATM: 
		case TYPE_AAL5:
		case TYPE_MC_ATM:
		case TYPE_MC_AAL5:
			if (p->dlt_list != NULL) {
				p->dlt_list[dlt_index++] = DLT_ATM_RFC1483;
				p->dlt_list[dlt_index++] = DLT_SUNATM;
			}
			if(!p->linktype)
				p->linktype = DLT_ATM_RFC1483;
			break;

		case TYPE_COLOR_MC_HDLC_POS:
		case TYPE_MC_HDLC:
			if (p->dlt_list != NULL) {
				p->dlt_list[dlt_index++] = DLT_CHDLC;
				p->dlt_list[dlt_index++] = DLT_PPP_SERIAL;
				p->dlt_list[dlt_index++] = DLT_FRELAY;
				p->dlt_list[dlt_index++] = DLT_MTP2;
				p->dlt_list[dlt_index++] = DLT_MTP2_WITH_PHDR;
				p->dlt_list[dlt_index++] = DLT_LAPD;
			}
			if(!p->linktype)
				p->linktype = DLT_CHDLC;
			break;

		case TYPE_IPV4:
		case TYPE_IPV6:
			if(!p->linktype)
				p->linktype = DLT_RAW;
			break;

		case TYPE_LEGACY:
		case TYPE_MC_RAW:
		case TYPE_MC_RAW_CHANNEL:
		case TYPE_IP_COUNTER:
		case TYPE_TCP_FLOW_COUNTER:
		case TYPE_INFINIBAND:
		case TYPE_RAW_LINK:
		case TYPE_INFINIBAND_LINK:
		default:
			
			
			break;

		} 
		index++;
	}

	p->dlt_list[dlt_index++] = DLT_ERF;

	p->dlt_count = dlt_index;

	if(!p->linktype)
		p->linktype = DLT_ERF;

	return p->linktype;
}
