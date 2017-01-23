
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <float.h>
#include <fcntl.h>
#include <io.h>

#if defined(USE_32BIT_DRIVERS)
  #include "msdos/pm_drvr/pmdrvr.h"
  #include "msdos/pm_drvr/pci.h"
  #include "msdos/pm_drvr/bios32.h"
  #include "msdos/pm_drvr/module.h"
  #include "msdos/pm_drvr/3c501.h"
  #include "msdos/pm_drvr/3c503.h"
  #include "msdos/pm_drvr/3c509.h"
  #include "msdos/pm_drvr/3c59x.h"
  #include "msdos/pm_drvr/3c515.h"
  #include "msdos/pm_drvr/3c90x.h"
  #include "msdos/pm_drvr/3c575_cb.h"
  #include "msdos/pm_drvr/ne.h"
  #include "msdos/pm_drvr/wd.h"
  #include "msdos/pm_drvr/accton.h"
  #include "msdos/pm_drvr/cs89x0.h"
  #include "msdos/pm_drvr/rtl8139.h"
  #include "msdos/pm_drvr/ne2k-pci.h"
#endif

#include "pcap.h"
#include "pcap-dos.h"
#include "pcap-int.h"
#include "msdos/pktdrvr.h"

#ifdef USE_NDIS2
#include "msdos/ndis2.h"
#endif

#include <arpa/inet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_ether.h>
#include <net/if_packe.h>
#include <tcp.h>

#if defined(USE_32BIT_DRIVERS)
  #define FLUSHK()       do { _printk_safe = 1; _printk_flush(); } while (0)
  #define NDIS_NEXT_DEV  &rtl8139_dev

  static char *rx_pool = NULL;
  static void init_32bit (void);

  static int  pktq_init     (struct rx_ringbuf *q, int size, int num, char *pool);
  static int  pktq_check    (struct rx_ringbuf *q);
  static int  pktq_inc_out  (struct rx_ringbuf *q);
  static int  pktq_in_index (struct rx_ringbuf *q) LOCKED_FUNC;
  static void pktq_clear    (struct rx_ringbuf *q) LOCKED_FUNC;

  static struct rx_elem *pktq_in_elem  (struct rx_ringbuf *q) LOCKED_FUNC;
  static struct rx_elem *pktq_out_elem (struct rx_ringbuf *q);

#else
  #define FLUSHK()      ((void)0)
  #define NDIS_NEXT_DEV  NULL
#endif

extern WORD  _pktdevclass;
extern BOOL  _eth_is_init;
extern int   _w32_dynamic_host;
extern int   _watt_do_exit;
extern int   _watt_is_init;
extern int   _w32__bootp_on, _w32__dhcp_on, _w32__rarp_on, _w32__do_mask_req;
extern void (*_w32_usr_post_init) (void);
extern void (*_w32_print_hook)();

extern void dbug_write (const char *);  
extern int  pkt_get_mtu (void);

static int ref_count = 0;

static u_long mac_count    = 0;
static u_long filter_count = 0;

static volatile BOOL exc_occured = 0;

static struct device *handle_to_device [20];

static int  pcap_activate_dos (pcap_t *p);
static int  pcap_read_dos (pcap_t *p, int cnt, pcap_handler callback,
                           u_char *data);
static void pcap_cleanup_dos (pcap_t *p);
static int  pcap_stats_dos (pcap_t *p, struct pcap_stat *ps);
static int  pcap_sendpacket_dos (pcap_t *p, const void *buf, size_t len);
static int  pcap_setfilter_dos (pcap_t *p, struct bpf_program *fp);

static int  ndis_probe (struct device *dev);
static int  pkt_probe  (struct device *dev);

static void close_driver (void);
static int  init_watt32 (struct pcap *pcap, const char *dev_name, char *err_buf);
static int  first_init (const char *name, char *ebuf, int promisc);

static void watt32_recv_hook (u_char *dummy, const struct pcap_pkthdr *pcap,
                              const u_char *buf);

static struct device ndis_dev = {
              "ndis",
              "NDIS2 LanManager",
              0,
              0,0,0,0,0,0,
              NDIS_NEXT_DEV,  
              ndis_probe
            };

static struct device pkt_dev = {
              "pkt",
              "Packet-Driver",
              0,
              0,0,0,0,0,0,
              &ndis_dev,
              pkt_probe
            };

static struct device *get_device (int fd)
{
  if (fd <= 0 || fd >= sizeof(handle_to_device)/sizeof(handle_to_device[0]))
     return (NULL);
  return handle_to_device [fd-1];
}

struct pcap_dos {
	void (*wait_proc)(void); 
	struct pcap_stat stat;
};

pcap_t *pcap_create_interface (const char *device, char *ebuf)
{
	pcap_t *p;

	p = pcap_create_common(device, ebuf, sizeof (struct pcap_dos));
	if (p == NULL)
		return (NULL);

	p->activate_op = pcap_activate_dos;
	return (p);
}

static int pcap_activate_dos (pcap_t *pcap)
{ 
  struct pcap_dos *pcapd = pcap->priv;

  if (pcap->opt.rfmon) {
    return (PCAP_ERROR_RFMON_NOTSUP);
  }

  if (pcap->snapshot < ETH_MIN+8)
      pcap->snapshot = ETH_MIN+8;

  if (pcap->snapshot > ETH_MAX)   
      pcap->snapshot = ETH_MAX;

  pcap->linktype          = DLT_EN10MB;  
  pcap->cleanup_op        = pcap_cleanup_dos;
  pcap->read_op           = pcap_read_dos;
  pcap->stats_op          = pcap_stats_dos;
  pcap->inject_op         = pcap_sendpacket_dos;
  pcap->setfilter_op      = pcap_setfilter_dos;
  pcap->setdirection_op   = NULL; 
  pcap->fd                = ++ref_count;

  if (pcap->fd == 1)  
  {
    if (!init_watt32(pcap, pcap->opt.source, pcap->errbuf) ||
        !first_init(pcap->opt.source, pcap->errbuf, pcap->opt.promisc))
    {
      return (PCAP_ERROR);
    } 
    atexit (close_driver);
  }
  else if (stricmp(active_dev->name,pcap->opt.source))
  {
    snprintf (pcap->errbuf, PCAP_ERRBUF_SIZE,
              "Cannot use different devices simultaneously "
              "(`%s' vs. `%s')", active_dev->name, pcap->opt.source);
    return (PCAP_ERROR);
  }
  handle_to_device [pcap->fd-1] = active_dev;
  return (0);
}

static int
pcap_read_one (pcap_t *p, pcap_handler callback, u_char *data)
{
  struct pcap_dos *pd = p->priv;
  struct pcap_pkthdr pcap;
  struct timeval     now, expiry = { 0,0 };
  BYTE  *rx_buf;
  int    rx_len = 0;

  if (p->opt.timeout > 0)
  {
    gettimeofday2 (&now, NULL);
    expiry.tv_usec = now.tv_usec + 1000UL * p->opt.timeout;
    expiry.tv_sec  = now.tv_sec;
    while (expiry.tv_usec >= 1000000L)
    {
      expiry.tv_usec -= 1000000L;
      expiry.tv_sec++;
    }
  }

  while (!exc_occured)
  {
    volatile struct device *dev; 

    dev = get_device (p->fd);
    if (!dev)
       break;

    PCAP_ASSERT (dev->copy_rx_buf || dev->peek_rx_buf);
    FLUSHK();

    if (dev->peek_rx_buf)
    {
      PCAP_ASSERT (dev->release_rx_buf);
      rx_len = (*dev->peek_rx_buf) (&rx_buf);
    }
    else
    {
      BYTE buf [ETH_MAX+100]; 
      rx_len = (*dev->copy_rx_buf) (buf, p->snapshot);
      rx_buf = buf;
    }

    if (rx_len > 0)  
    {
      mac_count++;

      FLUSHK();

      pcap.caplen = min (rx_len, p->snapshot);
      pcap.len    = rx_len;

      if (callback &&
          (!p->fcode.bf_insns || bpf_filter(p->fcode.bf_insns, rx_buf, pcap.len, pcap.caplen)))
      {
        filter_count++;

        gettimeofday2 (&pcap.ts, NULL);
        (*callback) (data, &pcap, rx_buf);
      }

      if (dev->release_rx_buf)
        (*dev->release_rx_buf) (rx_buf);

      if (pcap_pkt_debug > 0)
      {
        if (callback == watt32_recv_hook)
             dbug_write ("pcap_recv_hook\n");
        else dbug_write ("pcap_read_op\n");
      }
      FLUSHK();
      return (1);
    }

    if (p->opt.timeout <= 0 || (volatile int)p->fd <= 0)
       break;

    gettimeofday2 (&now, NULL);

    if (timercmp(&now, &expiry, >))
       break;

#ifndef DJGPP
    kbhit();    
#endif

    if (p->wait_proc)
      (*p->wait_proc)();     
  }

  if (rx_len < 0)            
  {
    pd->stat.ps_drop++;
#ifdef USE_32BIT_DRIVERS
    if (pcap_pkt_debug > 1)
       printk ("pkt-err %s\n", pktInfo.error);
#endif
    return (-1);
  }
  return (0);
}

static int
pcap_read_dos (pcap_t *p, int cnt, pcap_handler callback, u_char *data)
{
  struct pcap_dos *pd = p->priv;
  int rc, num = 0;

  while (num <= cnt || PACKET_COUNT_IS_UNLIMITED(cnt))
  {
    if (p->fd <= 0)
       return (-1);
    rc = pcap_read_one (p, callback, data);
    if (rc > 0)
       num++;
    if (rc < 0)
       break;
    _w32_os_yield();  
  }
  return (num);
}

static int pcap_stats_dos (pcap_t *p, struct pcap_stat *ps)
{
  struct net_device_stats *stats;
  struct pcap_dos         *pd;
  struct device           *dev = p ? get_device(p->fd) : NULL;

  if (!dev)
  {
    strcpy (p->errbuf, "illegal pcap handle");
    return (-1);
  }

  if (!dev->get_stats || (stats = (*dev->get_stats)(dev)) == NULL)
  {
    strcpy (p->errbuf, "device statistics not available");
    return (-1);
  }

  FLUSHK();

  pd = p->priv;
  pd->stat.ps_recv   = stats->rx_packets;
  pd->stat.ps_drop  += stats->rx_missed_errors;
  pd->stat.ps_ifdrop = stats->rx_dropped +  
                         stats->rx_errors;    
  if (ps)
     *ps = pd->stat;

  return (0);
}

int pcap_stats_ex (pcap_t *p, struct pcap_stat_ex *se)
{
  struct device *dev = p ? get_device (p->fd) : NULL;

  if (!dev || !dev->get_stats)
  {
    strlcpy (p->errbuf, "detailed device statistics not available",
             PCAP_ERRBUF_SIZE);
    return (-1);
  }

  if (!strnicmp(dev->name,"pkt",3))
  {
    strlcpy (p->errbuf, "pktdrvr doesn't have detailed statistics",
             PCAP_ERRBUF_SIZE);
    return (-1);
  }             
  memcpy (se, (*dev->get_stats)(dev), sizeof(*se));
  return (0);
}

static int pcap_setfilter_dos (pcap_t *p, struct bpf_program *fp)
{
  if (!p)
     return (-1);
  p->fcode = *fp;
  return (0);
}

u_long pcap_mac_packets (void)
{
  return (mac_count);
}

u_long pcap_filter_packets (void)
{
  return (filter_count);
}

static void pcap_cleanup_dos (pcap_t *p)
{
  struct pcap_dos *pd;

  if (p && !exc_occured)
  {
    pd = p->priv;
    if (pcap_stats(p,NULL) < 0)
       pd->stat.ps_drop = 0;
    if (!get_device(p->fd))
       return;

    handle_to_device [p->fd-1] = NULL;
    p->fd = 0;
    if (ref_count > 0)
        ref_count--;
    if (ref_count > 0)
       return;
  }
  close_driver();
}

char *pcap_lookupdev (char *ebuf)
{
  struct device *dev;

#ifdef USE_32BIT_DRIVERS
  init_32bit();
#endif

  for (dev = (struct device*)dev_base; dev; dev = dev->next)
  {
    PCAP_ASSERT (dev->probe);

    if ((*dev->probe)(dev))
    {
      FLUSHK();
      probed_dev = (struct device*) dev; 
      return (char*) dev->name;
    }
  }

  if (ebuf)
     strcpy (ebuf, "No driver found");
  return (NULL);
}

int pcap_lookupnet (const char *device, bpf_u_int32 *localnet,
                    bpf_u_int32 *netmask, char *errbuf)
{
  if (!_watt_is_init)
  {
    strcpy (errbuf, "pcap_open_offline() or pcap_activate() must be "
                    "called first");
    return (-1);
  }

  *netmask  = _w32_sin_mask;
  *localnet = my_ip_addr & *netmask;
  if (*localnet == 0)
  {
    if (IN_CLASSA(*netmask))
       *localnet = IN_CLASSA_NET;
    else if (IN_CLASSB(*netmask))
       *localnet = IN_CLASSB_NET;
    else if (IN_CLASSC(*netmask))
       *localnet = IN_CLASSC_NET;
    else
    {
      sprintf (errbuf, "inet class for 0x%lx unknown", *netmask);
      return (-1);
    }
  }
  ARGSUSED (device);
  return (0);
}      

int pcap_findalldevs (pcap_if_t **alldevsp, char *errbuf)
{
  struct device     *dev;
  struct sockaddr_ll sa_ll_1, sa_ll_2;
  struct sockaddr   *addr, *netmask, *broadaddr, *dstaddr;
  pcap_if_t *devlist = NULL;
  int       ret = 0;
  size_t    addr_size = sizeof(struct sockaddr_ll);

  for (dev = (struct device*)dev_base; dev; dev = dev->next)
  {
    PCAP_ASSERT (dev->probe);

    if (!(*dev->probe)(dev))
       continue;

    PCAP_ASSERT (dev->close);  
    FLUSHK();
    (*dev->close) (dev);

    memset (&sa_ll_1, 0, sizeof(sa_ll_1));
    memset (&sa_ll_2, 0, sizeof(sa_ll_2));
    sa_ll_1.sll_family = AF_PACKET;
    sa_ll_2.sll_family = AF_PACKET;

    addr      = (struct sockaddr*) &sa_ll_1;
    netmask   = (struct sockaddr*) &sa_ll_1;
    dstaddr   = (struct sockaddr*) &sa_ll_1;
    broadaddr = (struct sockaddr*) &sa_ll_2;
    memset (&sa_ll_2.sll_addr, 0xFF, sizeof(sa_ll_2.sll_addr));

    if (pcap_add_if(&devlist, dev->name, dev->flags,
                    dev->long_name, errbuf) < 0)
    {
      ret = -1;
      break;
    }
    if (add_addr_to_iflist(&devlist,dev->name, dev->flags, addr, addr_size,
                           netmask, addr_size, broadaddr, addr_size,
                           dstaddr, addr_size, errbuf) < 0)
    {
      ret = -1;
      break;
    }
  }

  if (devlist && ret < 0)
  {
    pcap_freealldevs (devlist);
    devlist = NULL;
  }
  else
  if (!devlist)
     strcpy (errbuf, "No drivers found");

  *alldevsp = devlist;
  return (ret);
}

void pcap_assert (const char *what, const char *file, unsigned line)
{
  FLUSHK();
  fprintf (stderr, "%s (%u): Assertion \"%s\" failed\n",
           file, line, what);
  close_driver();
  _exit (-1);
}

void pcap_set_wait (pcap_t *p, void (*yield)(void), int wait)
{
  struct pcap_dos *pd;
  if (p)
  {
    pd                   = p->priv;
    pd->wait_proc        = yield;
    p->opt.timeout        = wait;
  }
}

static struct device *
open_driver (const char *dev_name, char *ebuf, int promisc)
{
  struct device *dev;

  for (dev = (struct device*)dev_base; dev; dev = dev->next)
  {
    PCAP_ASSERT (dev->name);

    if (strcmp (dev_name,dev->name))
       continue;

    if (!probed_dev)   
    {
      PCAP_ASSERT (dev->probe);

      if (!(*dev->probe)(dev))    
      {
        sprintf (ebuf, "failed to detect device `%s'", dev_name);
        return (NULL);
      }
      probed_dev = dev;  
    }
    else if (dev != probed_dev)
    {
      goto not_probed;
    }

    FLUSHK();

    if (promisc)
         dev->flags |=  (IFF_ALLMULTI | IFF_PROMISC);
    else dev->flags &= ~(IFF_ALLMULTI | IFF_PROMISC);

    PCAP_ASSERT (dev->open);

    if (!(*dev->open)(dev))
    {
      sprintf (ebuf, "failed to activate device `%s'", dev_name);
      if (pktInfo.error && !strncmp(dev->name,"pkt",3))
      {
        strcat (ebuf, ": ");
        strcat (ebuf, pktInfo.error);
      }
      return (NULL);
    }

    if (promisc && dev->set_multicast_list)
       (*dev->set_multicast_list) (dev);

    active_dev = dev;   
    break;
  }

  if (!dev)
  {
    sprintf (ebuf, "device `%s' not supported", dev_name);
    return (NULL);
  }

not_probed:
  if (!probed_dev)
  {
    sprintf (ebuf, "device `%s' not probed", dev_name);
    return (NULL);
  }
  return (dev);
}

static void close_driver (void)
{
  
  struct device *dev = active_dev;

  if (dev && dev->close)
  {
    (*dev->close) (dev);
    FLUSHK();
  }

  active_dev = NULL;

#ifdef USE_32BIT_DRIVERS
  if (rx_pool)
  {
    k_free (rx_pool);
    rx_pool = NULL;
  }
  if (dev)
     pcibios_exit();
#endif
}


#ifdef __DJGPP__
static void setup_signals (void (*handler)(int))
{
  signal (SIGSEGV,handler);
  signal (SIGILL, handler);
  signal (SIGFPE, handler);
}

static void exc_handler (int sig)
{
#ifdef USE_32BIT_DRIVERS
  if (active_dev->irq > 0)    
  {
    disable_irq (active_dev->irq);
    irq_eoi_cmd (active_dev->irq);
    _printk_safe = 1;
  }
#endif

  switch (sig)
  {
    case SIGSEGV:
         fputs ("Catching SIGSEGV.\n", stderr);
         break;
    case SIGILL:
         fputs ("Catching SIGILL.\n", stderr);
         break;
    case SIGFPE:
         _fpreset();
         fputs ("Catching SIGFPE.\n", stderr);
         break;
    default:
         fprintf (stderr, "Catching signal %d.\n", sig);
  }
  exc_occured = 1;
  pcap_cleanup_dos (NULL);
}
#endif  


static int first_init (const char *name, char *ebuf, int promisc)
{
  struct device *dev;

#ifdef USE_32BIT_DRIVERS
  rx_pool = k_calloc (RECEIVE_BUF_SIZE, RECEIVE_QUEUE_SIZE);
  if (!rx_pool)
  {
    strcpy (ebuf, "Not enough memory (Rx pool)");
    return (0);
  }
#endif

#ifdef __DJGPP__
  setup_signals (exc_handler);
#endif

#ifdef USE_32BIT_DRIVERS
  init_32bit();
#endif

  dev = open_driver (name, ebuf, promisc);
  if (!dev)
  {
#ifdef USE_32BIT_DRIVERS
    k_free (rx_pool);
    rx_pool = NULL;
#endif

#ifdef __DJGPP__
    setup_signals (SIG_DFL);
#endif
    return (0);
  }

#ifdef USE_32BIT_DRIVERS
  if (dev->copy_rx_buf == NULL)
  {
    dev->get_rx_buf     = get_rxbuf;
    dev->peek_rx_buf    = peek_rxbuf;
    dev->release_rx_buf = release_rxbuf;
    pktq_init (&dev->queue, RECEIVE_BUF_SIZE, RECEIVE_QUEUE_SIZE, rx_pool);
  }
#endif
  return (1);
}

#ifdef USE_32BIT_DRIVERS
static void init_32bit (void)
{
  static int init_pci = 0;

  if (!_printk_file)
     _printk_init (64*1024, NULL); 

  if (!init_pci)
     (void)pci_init();             
  init_pci = 1;
}
#endif


static char rxbuf [ETH_MAX+100]; 
static WORD etype;
static pcap_t pcap_save;

static void watt32_recv_hook (u_char *dummy, const struct pcap_pkthdr *pcap,
                              const u_char *buf)
{
  
  struct ether_header *ep = (struct ether_header*) buf;

  memcpy (rxbuf, buf, pcap->caplen);
  etype = ep->ether_type;
  ARGSUSED (dummy);
}

#if (WATTCP_VER >= 0x0224)
static void *pcap_recv_hook (WORD *type)
{
  int len = pcap_read_dos (&pcap_save, 1, watt32_recv_hook, NULL);

  if (len < 0)
     return (NULL);

  *type = etype;
  return (void*) &rxbuf;
}

static int pcap_xmit_hook (const void *buf, unsigned len)
{
  int rc = 0;

  if (pcap_pkt_debug > 0)
     dbug_write ("pcap_xmit_hook: ");

  if (active_dev && active_dev->xmit)
     if ((*active_dev->xmit) (active_dev, buf, len) > 0)
        rc = len;

  if (pcap_pkt_debug > 0)
     dbug_write (rc ? "ok\n" : "fail\n");
  return (rc);
}
#endif

static int pcap_sendpacket_dos (pcap_t *p, const void *buf, size_t len)
{
  struct device *dev = p ? get_device(p->fd) : NULL;

  if (!dev || !dev->xmit)
     return (-1);
  return (*dev->xmit) (dev, buf, len);
}

static void (*prev_post_hook) (void);

static void pcap_init_hook (void)
{
  _w32__bootp_on = _w32__dhcp_on = _w32__rarp_on = 0;
  _w32__do_mask_req = 0;
  _w32_dynamic_host = 0;
  if (prev_post_hook)
    (*prev_post_hook)();
}

static void null_print (void) {}

static int init_watt32 (struct pcap *pcap, const char *dev_name, char *err_buf)
{
  char *env;
  int   rc, MTU, has_ip_addr;
  int   using_pktdrv = 1;

  if (_watt_is_init)
     sock_exit();

  env = getenv ("PCAP_DEBUG");
  if (env && atoi(env) > 0 &&
      pcap_pkt_debug < 0)   
  {
    dbug_init();
    pcap_pkt_debug = atoi (env);
  }

  _watt_do_exit      = 0;    
  prev_post_hook     = _w32_usr_post_init;
  _w32_usr_post_init = pcap_init_hook;
  _w32_print_hook    = null_print;

  if (dev_name && strncmp(dev_name,"pkt",3))
     using_pktdrv = FALSE;

  rc = sock_init();
  has_ip_addr = (rc != 8);  

  _watt_is_init = 1;  

  if (!using_pktdrv || !has_ip_addr)  
  {
    static const char myip[] = "192.168.0.1";
    static const char mask[] = "255.255.255.0";

    printf ("Just guessing, using IP %s and netmask %s\n", myip, mask);
    my_ip_addr    = aton (myip);
    _w32_sin_mask = aton (mask);
  }
  else if (rc && using_pktdrv)
  {
    sprintf (err_buf, "sock_init() failed, code %d", rc);
    return (0);
  }

#if (WATTCP_VER >= 0x0224)
  _eth_recv_hook = pcap_recv_hook;
  _eth_xmit_hook = pcap_xmit_hook;
#endif

  if (using_pktdrv)
  {
    _eth_release();
  
  }

  memcpy (&pcap_save, pcap, sizeof(pcap_save));
  MTU = pkt_get_mtu();
  pcap_save.fcode.bf_insns = NULL;
  pcap_save.linktype       = _eth_get_hwtype (NULL, NULL);
  pcap_save.snapshot       = MTU > 0 ? MTU : ETH_MAX; 

#if 1
  last_nameserver = 0;
#endif
  return (1);
}

int EISA_bus = 0;  


static const struct config_table debug_tab[] = {
            { "PKT.DEBUG",       ARG_ATOI,   &pcap_pkt_debug    },
            { "PKT.VECTOR",      ARG_ATOX_W, NULL               },
            { "NDIS.DEBUG",      ARG_ATOI,   NULL               },
#ifdef USE_32BIT_DRIVERS
            { "3C503.DEBUG",     ARG_ATOI,   &ei_debug          },
            { "3C503.IO_BASE",   ARG_ATOX_W, &el2_dev.base_addr },
            { "3C503.MEMORY",    ARG_ATOX_W, &el2_dev.mem_start },
            { "3C503.IRQ",       ARG_ATOI,   &el2_dev.irq       },
            { "3C505.DEBUG",     ARG_ATOI,   NULL               },
            { "3C505.BASE",      ARG_ATOX_W, NULL               },
            { "3C507.DEBUG",     ARG_ATOI,   NULL               },
            { "3C509.DEBUG",     ARG_ATOI,   &el3_debug         },
            { "3C509.ILOOP",     ARG_ATOI,   &el3_max_loop      },
            { "3C529.DEBUG",     ARG_ATOI,   NULL               },
            { "3C575.DEBUG",     ARG_ATOI,   &debug_3c575       },
            { "3C59X.DEBUG",     ARG_ATOI,   &vortex_debug      },
            { "3C59X.IFACE0",    ARG_ATOI,   &vortex_options[0] },
            { "3C59X.IFACE1",    ARG_ATOI,   &vortex_options[1] },
            { "3C59X.IFACE2",    ARG_ATOI,   &vortex_options[2] },
            { "3C59X.IFACE3",    ARG_ATOI,   &vortex_options[3] },
            { "3C90X.DEBUG",     ARG_ATOX_W, &tc90xbc_debug     },
            { "ACCT.DEBUG",      ARG_ATOI,   &ethpk_debug       },
            { "CS89.DEBUG",      ARG_ATOI,   &cs89_debug        },
            { "RTL8139.DEBUG",   ARG_ATOI,   &rtl8139_debug     },
        
            { "SMC.DEBUG",       ARG_ATOI,   &ei_debug          },
        
            { "PCI.DEBUG",       ARG_ATOI,   &pci_debug         },
            { "BIOS32.DEBUG",    ARG_ATOI,   &bios32_debug      },
            { "IRQ.DEBUG",       ARG_ATOI,   &irq_debug         },
            { "TIMER.IRQ",       ARG_ATOI,   &timer_irq         },
#endif
            { NULL }
          };

int pcap_config_hook (const char *name, const char *value)
{
  return parse_config_table (debug_tab, NULL, name, value);
}

struct device       *active_dev = NULL;      
struct device       *probed_dev = NULL;      
const struct device *dev_base   = &pkt_dev;  

int pcap_pkt_debug = -1;

static void pkt_close (struct device *dev)
{
  BOOL okay = PktExitDriver();

  if (pcap_pkt_debug > 1)
     fprintf (stderr, "pkt_close(): %d\n", okay);

  if (dev->priv)
     free (dev->priv);
  dev->priv = NULL;
}

static int pkt_open (struct device *dev)
{
  PKT_RX_MODE mode;

  if (dev->flags & IFF_PROMISC)
       mode = PDRX_ALL_PACKETS;
  else mode = PDRX_BROADCAST;

  if (!PktInitDriver(mode))
     return (0);
 
  PktResetStatistics (pktInfo.handle);
  PktQueueBusy (FALSE);
  return (1);
}

static int pkt_xmit (struct device *dev, const void *buf, int len)
{
  struct net_device_stats *stats = (struct net_device_stats*) dev->priv;

  if (pcap_pkt_debug > 0)
     dbug_write ("pcap_xmit\n");

  if (!PktTransmit(buf,len))
  {
    stats->tx_errors++;
    return (0);
  }
  return (len);
}

static void *pkt_stats (struct device *dev)
{
  struct net_device_stats *stats = (struct net_device_stats*) dev->priv;

  if (!stats || !PktSessStatistics(pktInfo.handle))
     return (NULL);

  stats->rx_packets       = pktStat.inPackets;
  stats->rx_errors        = pktStat.lost;
  stats->rx_missed_errors = PktRxDropped();
  return (stats);
}

static int pkt_probe (struct device *dev)
{
  if (!PktSearchDriver())
     return (0);

  dev->open           = pkt_open;
  dev->xmit           = pkt_xmit;
  dev->close          = pkt_close;
  dev->get_stats      = pkt_stats;
  dev->copy_rx_buf    = PktReceive;  
  dev->get_rx_buf     = NULL;
  dev->peek_rx_buf    = NULL;
  dev->release_rx_buf = NULL;
  dev->priv           = calloc (sizeof(struct net_device_stats), 1);
  if (!dev->priv)
     return (0);
  return (1);
}

static void ndis_close (struct device *dev)
{
#ifdef USE_NDIS2
  NdisShutdown();
#endif
  ARGSUSED (dev);
}

static int ndis_open (struct device *dev)
{
  int promis = (dev->flags & IFF_PROMISC);

#ifdef USE_NDIS2
  if (!NdisInit(promis))
     return (0);
  return (1);
#else
  ARGSUSED (promis);
  return (0);
#endif
}

static void *ndis_stats (struct device *dev)
{
  static struct net_device_stats stats;

  
  ARGSUSED (dev);
  return (&stats);
}

static int ndis_probe (struct device *dev)
{
#ifdef USE_NDIS2
  if (!NdisOpen())
     return (0);
#endif

  dev->open           = ndis_open;
  dev->xmit           = NULL;
  dev->close          = ndis_close;
  dev->get_stats      = ndis_stats;
  dev->copy_rx_buf    = NULL;       
  dev->get_rx_buf     = NULL;       
  dev->peek_rx_buf    = NULL;
  dev->release_rx_buf = NULL;
  return (0);
}

#if defined(USE_32BIT_DRIVERS)

struct device el2_dev LOCKED_VAR = {
              "3c503",
              "EtherLink II",
              0,
              0,0,0,0,0,0,
              NULL,
              el2_probe
            };

struct device el3_dev LOCKED_VAR = {
              "3c509",
              "EtherLink III",
              0,
              0,0,0,0,0,0,
              &el2_dev,
              el3_probe
            };

struct device tc515_dev LOCKED_VAR = {
              "3c515",
              "EtherLink PCI",
              0,
              0,0,0,0,0,0,
              &el3_dev,
              tc515_probe
            };

struct device tc59_dev LOCKED_VAR = {
              "3c59x",
              "EtherLink PCI",
              0,
              0,0,0,0,0,0,
              &tc515_dev,
              tc59x_probe
            };

struct device tc90xbc_dev LOCKED_VAR = {
              "3c90x",
              "EtherLink 90X",
              0,
              0,0,0,0,0,0,
              &tc59_dev,
              tc90xbc_probe
            };

struct device wd_dev LOCKED_VAR = {
              "wd",
              "Westen Digital",
              0,
              0,0,0,0,0,0,
              &tc90xbc_dev,
              wd_probe
            };

struct device ne_dev LOCKED_VAR = {
              "ne",
              "NEx000",
              0,
              0,0,0,0,0,0,
              &wd_dev,
              ne_probe
            };

struct device acct_dev LOCKED_VAR = {
              "acct",
              "Accton EtherPocket",
              0,
              0,0,0,0,0,0,
              &ne_dev,
              ethpk_probe
            };

struct device cs89_dev LOCKED_VAR = {
              "cs89",
              "Crystal Semiconductor",
              0,
              0,0,0,0,0,0,
              &acct_dev,
              cs89x0_probe
            };

struct device rtl8139_dev LOCKED_VAR = {
              "rtl8139",
              "RealTek PCI",
              0,
              0,0,0,0,0,0,
              &cs89_dev,
              rtl8139_probe     
            };            

int peek_rxbuf (BYTE **buf)
{
  struct rx_elem *tail, *head;

  PCAP_ASSERT (pktq_check (&active_dev->queue));

  DISABLE();
  tail = pktq_out_elem (&active_dev->queue);
  head = pktq_in_elem (&active_dev->queue);
  ENABLE();

  if (head != tail)
  {
    PCAP_ASSERT (tail->size < active_dev->queue.elem_size-4-2);

    *buf = &tail->data[0];
    return (tail->size);
  }
  *buf = NULL;
  return (0);
}

int release_rxbuf (BYTE *buf)
{
#ifndef NDEBUG
  struct rx_elem *tail = pktq_out_elem (&active_dev->queue);

  PCAP_ASSERT (&tail->data[0] == buf);
#else
  ARGSUSED (buf);
#endif
  pktq_inc_out (&active_dev->queue);
  return (1);
}

BYTE *get_rxbuf (int len)
{
  int idx;

  if (len < ETH_MIN || len > ETH_MAX)
     return (NULL);

  idx = pktq_in_index (&active_dev->queue);

#ifdef DEBUG
  {
    static int fan_idx LOCKED_VAR = 0;
    writew ("-\\|/"[fan_idx++] | (15 << 8),      
            0xB8000 + 2*79);  
    fan_idx &= 3;
  }
#endif

  if (idx != active_dev->queue.out_index)
  {
    struct rx_elem *head = pktq_in_elem (&active_dev->queue);

    head->size = len;
    active_dev->queue.in_index = idx;
    return (&head->data[0]);
  }

  pktq_clear (&active_dev->queue);
  return (NULL);
}

#define PKTQ_MARKER  0xDEADBEEF

static int pktq_check (struct rx_ringbuf *q)
{
#ifndef NDEBUG
  int   i;
  char *buf;
#endif

  if (!q || !q->num_elem || !q->buf_start)
     return (0);

#ifndef NDEBUG
  buf = q->buf_start;

  for (i = 0; i < q->num_elem; i++)
  {
    buf += q->elem_size;
    if (*(DWORD*)(buf - sizeof(DWORD)) != PKTQ_MARKER)
       return (0);
  }
#endif
  return (1);
}

static int pktq_init (struct rx_ringbuf *q, int size, int num, char *pool)
{
  int i;

  q->elem_size = size;
  q->num_elem  = num;
  q->buf_start = pool;
  q->in_index  = 0;
  q->out_index = 0;

  PCAP_ASSERT (size >= sizeof(struct rx_elem) + sizeof(DWORD));
  PCAP_ASSERT (num);
  PCAP_ASSERT (pool);

  for (i = 0; i < num; i++)
  {
#if 0
    struct rx_elem *elem = (struct rx_elem*) pool;

    PCAP_ASSERT (((unsigned)(&elem->data[0]) & 3) == 0);
#endif
    pool += size;
    *(DWORD*) (pool - sizeof(DWORD)) = PKTQ_MARKER;
  }
  return (1);
}

static int pktq_inc_out (struct rx_ringbuf *q)
{
  q->out_index++;
  if (q->out_index >= q->num_elem)
      q->out_index = 0;
  return (q->out_index);
}

static int pktq_in_index (struct rx_ringbuf *q)
{
  volatile int index = q->in_index + 1;

  if (index >= q->num_elem)
      index = 0;
  return (index);
}

static struct rx_elem *pktq_in_elem (struct rx_ringbuf *q)
{
  return (struct rx_elem*) (q->buf_start + (q->elem_size * q->in_index));
}

static struct rx_elem *pktq_out_elem (struct rx_ringbuf *q)
{
  return (struct rx_elem*) (q->buf_start + (q->elem_size * q->out_index));
}

static void pktq_clear (struct rx_ringbuf *q)
{
  q->in_index = q->out_index;
}

#undef __IOPORT_H
#undef __DMA_H

#define extern
#define __inline__

#include "msdos/pm_drvr/ioport.h"
#include "msdos/pm_drvr/dma.h"

#endif 

