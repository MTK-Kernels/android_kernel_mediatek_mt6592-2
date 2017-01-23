
#ifndef lint
static const char rcsid[] _U_ =
    "@(#) $Header: /tcpdump/master/libpcap/pcap-septel.c,v 1.4 2008-04-14 20:40:58 guy Exp $";
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

#include <msg.h>
#include <ss7_inc.h>
#include <sysgct.h>
#include <pack.h>
#include <system.h>

#include "pcap-septel.h"

static int septel_setfilter(pcap_t *p, struct bpf_program *fp);
static int septel_stats(pcap_t *p, struct pcap_stat *ps);
static int septel_setnonblock(pcap_t *p, int nonblock, char *errbuf);

struct pcap_septel {
	struct pcap_stat stat;
}

static int septel_read(pcap_t *p, int cnt, pcap_handler callback, u_char *user) {

  struct pcap_septel *ps = p->priv;
  HDR *h;
  MSG *m;
  int processed = 0 ;
  int t = 0 ;

  unsigned int id = 0xdd;

  
  do  {

    unsigned short packet_len = 0;
    int caplen = 0;
    int counter = 0;
    struct pcap_pkthdr   pcap_header;
    u_char *dp ;

loop:
    if (p->break_loop) {
      p->break_loop = 0;
      return -2;
    }

    do  {
      h = GCT_grab(id);

      m = (MSG*)h;
      counter++ ;

    }
    while  ((m == NULL)&& (counter< 100)) ;

    if (m != NULL) {

      t = h->type ;

      
      if ((t != 0xcf00) && (t != 0x8f01)) {
        relm(h);
        goto loop ;
      }

      
      dp = get_param(m);
      packet_len = m->len;
      caplen =  p->snapshot ;


      if (caplen > packet_len) {

        caplen = packet_len;
      }
      
      if ((p->fcode.bf_insns == NULL) || bpf_filter(p->fcode.bf_insns, dp, packet_len, caplen)) {



        (void)gettimeofday(&pcap_header.ts, NULL);

        
        pcap_header.caplen = caplen;
        pcap_header.len = packet_len;

        
        ps->stat.ps_recv++;

        
        callback(user, &pcap_header, dp);

        processed++ ;

      }
      relm(h);
    }else
      processed++;

  }
  while (processed < cnt) ;

  return processed ;
}


static int
septel_inject(pcap_t *handle, const void *buf _U_, size_t size _U_)
{
  strlcpy(handle->errbuf, "Sending packets isn't supported on Septel cards",
          PCAP_ERRBUF_SIZE);
  return (-1);
}

static pcap_t *septel_activate(pcap_t* handle) {
    
  handle->linktype = DLT_MTP2;
  
  handle->bufsize = 0;

  handle->selectable_fd = -1;

  handle->read_op = septel_read;
  handle->inject_op = septel_inject;
  handle->setfilter_op = septel_setfilter;
  handle->set_datalink_op = NULL; 
  handle->getnonblock_op = pcap_getnonblock_fd;
  handle->setnonblock_op = septel_setnonblock;
  handle->stats_op = septel_stats;

  return 0;
}

pcap_t *septel_create(const char *device, char *ebuf, int *is_ours) {
	const char *cp;
	pcap_t *p;

	
	cp = strrchr(device, '/');
	if (cp == NULL)
		cp = device;
	if (strcmp(cp, "septel") != 0) {
		
		*is_ours = 0;
		return NULL;
	}

	
	*is_ours = 1;

	p = pcap_create_common(device, ebuf, sizeof (struct pcap_septel));
	if (p == NULL)
		return NULL;

	p->activate_op = septel_activate;
	return p;
}

static int septel_stats(pcap_t *p, struct pcap_stat *ps) {
  struct pcap_septel *handlep = p->priv;
  
  
  
  *ps = handlep->stat;
 
  return 0;
}


int
septel_findalldevs(pcap_if_t **devlistp, char *errbuf)
{
unsigned char *p;
  const char description[512]= "Intel/Septel device";
  char name[512]="septel" ;
  int ret = 0;
  pcap_add_if(devlistp,name,0,description,errbuf);

  return (ret); 
}


static int septel_setfilter(pcap_t *p, struct bpf_program *fp) {
  if (!p)
    return -1;
  if (!fp) {
    strncpy(p->errbuf, "setfilter: No filter specified",
	    sizeof(p->errbuf));
    return -1;
  }

  

  if (install_bpf_program(p, fp) < 0) {
    snprintf(p->errbuf, sizeof(p->errbuf),
	     "malloc: %s", pcap_strerror(errno));
    return -1;
  }

  return (0);
}


static int
septel_setnonblock(pcap_t *p, int nonblock, char *errbuf)
{
  fprintf(errbuf, PCAP_ERRBUF_SIZE, "Non-blocking mode not supported on Septel devices");
  return (-1);
}
