/* dnsmasq is Copyright (c) 2000-2009 Simon Kelley
 
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 dated June, 1991, or
   (at your option) version 3 dated 29 June, 2007.
 
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
     
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#define COPYRIGHT "Copyright (C) 2000-2009 Simon Kelley" 

#ifndef NO_LARGEFILE
#  define _LARGEFILE_SOURCE 1
#  define _FILE_OFFSET_BITS 64
#endif

#ifdef __linux__
#  ifndef __ANDROID__
#      define _GNU_SOURCE
#  endif
#  include <features.h> 
#endif

#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>

#ifdef __APPLE__
#  include <nameser.h>
#  include <arpa/nameser_compat.h>
#else
#  ifdef __ANDROID__
#    include "nameser.h"
#  else
#    include <arpa/nameser.h>
#  endif
#endif

#include <getopt.h>

#include "config.h"

#define gettext_noop(S) (S)
#ifndef LOCALEDIR
#  define _(S) (S)
#else
#  include <libintl.h>
#  include <locale.h>   
#  define _(S) gettext(S)
#endif

#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#if defined(HAVE_SOLARIS_NETWORK)
#include <sys/sockio.h>
#endif
#include <sys/select.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/un.h>
#include <limits.h>
#include <net/if.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <ctype.h>
#include <signal.h>
#include <stddef.h>
#include <time.h>
#include <errno.h>
#include <pwd.h>
#include <grp.h>
#include <stdarg.h>
#if defined(__OpenBSD__) || defined(__NetBSD__) || defined(__sun__) || defined (__sun) || defined (__ANDROID__)
#  include <netinet/if_ether.h>
#else
#  include <net/ethernet.h>
#endif
#include <net/if_arp.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <sys/uio.h>
#include <syslog.h>
#include <dirent.h>
#ifndef HAVE_LINUX_NETWORK
#  include <net/if_dl.h>
#endif

#if defined(HAVE_LINUX_NETWORK)
#include <linux/capability.h>
extern int capset(cap_user_header_t header, cap_user_data_t data);
extern int capget(cap_user_header_t header, cap_user_data_t data);
#define LINUX_CAPABILITY_VERSION_1  0x19980330
#define LINUX_CAPABILITY_VERSION_2  0x20071026
#define LINUX_CAPABILITY_VERSION_3  0x20080522

#include <sys/prctl.h>
#elif defined(HAVE_SOLARIS_NETWORK)
#include <priv.h>
#endif

#define daemon dnsmasq_daemon

struct event_desc {
  int event, data;
};

#define EVENT_RELOAD    1
#define EVENT_DUMP      2
#define EVENT_ALARM     3
#define EVENT_TERM      4
#define EVENT_CHILD     5
#define EVENT_REOPEN    6
#define EVENT_EXITED    7
#define EVENT_KILLED    8
#define EVENT_EXEC_ERR  9
#define EVENT_PIPE_ERR  10
#define EVENT_USER_ERR  11
#define EVENT_CAP_ERR   12
#define EVENT_PIDFILE   13
#define EVENT_HUSER_ERR 14
#define EVENT_GROUP_ERR 15
#define EVENT_DIE       16
#define EVENT_LOG_ERR   17
#define EVENT_FORK_ERR  18

#define EC_GOOD        0
#define EC_BADCONF     1
#define EC_BADNET      2
#define EC_FILE        3
#define EC_NOMEM       4
#define EC_MISC        5
#define EC_INIT_OFFSET 10

#define DNSMASQ_PACKETSZ PACKETSZ+MAXDNAME+RRFIXEDSZ

#define OPT_BOGUSPRIV      (1u<<0)
#define OPT_FILTER         (1u<<1)
#define OPT_LOG            (1u<<2)
#define OPT_SELFMX         (1u<<3)
#define OPT_NO_HOSTS       (1u<<4)
#define OPT_NO_POLL        (1u<<5)
#define OPT_DEBUG          (1u<<6)
#define OPT_ORDER          (1u<<7)
#define OPT_NO_RESOLV      (1u<<8)
#define OPT_EXPAND         (1u<<9)
#define OPT_LOCALMX        (1u<<10)
#define OPT_NO_NEG         (1u<<11)
#define OPT_NODOTS_LOCAL   (1u<<12)
#define OPT_NOWILD         (1u<<13)
#define OPT_ETHERS         (1u<<14)
#define OPT_RESOLV_DOMAIN  (1u<<15)
#define OPT_NO_FORK        (1u<<16)
#define OPT_AUTHORITATIVE  (1u<<17)
#define OPT_LOCALISE       (1u<<18)
#define OPT_DBUS           (1u<<19)
#define OPT_DHCP_FQDN      (1u<<20)
#define OPT_NO_PING        (1u<<21)
#define OPT_LEASE_RO       (1u<<22)
#define OPT_ALL_SERVERS    (1u<<23)
#define OPT_RELOAD         (1u<<24)
#define OPT_TFTP           (1u<<25)
#define OPT_TFTP_SECURE    (1u<<26)
#define OPT_TFTP_NOBLOCK   (1u<<27)
#define OPT_LOG_OPTS       (1u<<28)
#define OPT_TFTP_APREF     (1u<<29)
#define OPT_NO_OVERRIDE    (1u<<30)
#define OPT_NO_REBIND      (1u<<31)

#define MS_TFTP LOG_USER
#define MS_DHCP LOG_DAEMON 

struct all_addr {
  union {
    struct in_addr addr4;
#ifdef HAVE_IPV6
    struct in6_addr addr6;
#endif
  } addr;
};

struct bogus_addr {
  struct in_addr addr;
  struct bogus_addr *next;
};

struct doctor {
  struct in_addr in, end, out, mask;
  struct doctor *next;
};

struct mx_srv_record {
  char *name, *target;
  int issrv, srvport, priority, weight;
  unsigned int offset;
  struct mx_srv_record *next;
};

struct naptr {
  char *name, *replace, *regexp, *services, *flags;
  unsigned int order, pref;
  struct naptr *next;
};

struct txt_record {
  char *name, *txt;
  unsigned short class, len;
  struct txt_record *next;
};

struct ptr_record {
  char *name, *ptr;
  struct ptr_record *next;
};

struct cname {
  char *alias, *target;
  struct cname *next;
};

struct interface_name {
  char *name; 
  char *intr; 
  struct interface_name *next;
};

union bigname {
  char name[MAXDNAME];
  union bigname *next; 
};

struct crec { 
  struct crec *next, *prev, *hash_next;
  time_t ttd; 
  int uid; 
  union {
    struct all_addr addr;
    struct {
      struct crec *cache;
      int uid;
    } cname;
  } addr;
  unsigned short flags;
  union {
    char sname[SMALLDNAME];
    union bigname *bname;
    char *namep;
  } name;
};

#define F_IMMORTAL  1
#define F_CONFIG    2
#define F_REVERSE   4
#define F_FORWARD   8
#define F_DHCP      16 
#define F_NEG       32       
#define F_HOSTS     64
#define F_IPV4      128
#define F_IPV6      256
#define F_BIGNAME   512
#define F_UPSTREAM  1024
#define F_SERVER    2048
#define F_NXDOMAIN  4096
#define F_QUERY     8192
#define F_CNAME     16384
#define F_NOERR     32768

union mysockaddr {
  struct sockaddr sa;
  struct sockaddr_in in;
#if defined(HAVE_IPV6)
  struct sockaddr_in6 in6;
#endif
};

#define SERV_FROM_RESOLV       1  
#define SERV_NO_ADDR           2  
#define SERV_LITERAL_ADDRESS   4   
#define SERV_HAS_DOMAIN        8  
#define SERV_HAS_SOURCE       16  
#define SERV_FOR_NODOTS       32  
#define SERV_WARNED_RECURSIVE 64  
#define SERV_FROM_DBUS       128  
#define SERV_MARK            256  
#define SERV_TYPE    (SERV_HAS_DOMAIN | SERV_FOR_NODOTS)
#define SERV_COUNTED         512  

struct serverfd {
  int fd;
  union mysockaddr source_addr;
  char interface[IF_NAMESIZE+1];
  struct serverfd *next;
  uint32_t mark;
};

struct randfd {
  int fd;
  unsigned short refcount, family;
};
  
struct server {
  union mysockaddr addr, source_addr;
  char interface[IF_NAMESIZE+1];
  struct serverfd *sfd; 
  char *domain;  
  int flags, tcpfd;
  unsigned int queries, failed_queries;
  uint32_t mark;
  struct server *next; 
};

struct irec {
  union mysockaddr addr;
  struct in_addr netmask; 
  int dhcp_ok, mtu;
  struct irec *next;
};

struct listener {
  int fd, tcpfd, tftpfd, family;
  struct irec *iface; 
  struct listener *next;
};

struct iname {
  char *name;
  union mysockaddr addr;
  int isloop, used;
  struct iname *next;
};

struct resolvc {
  struct resolvc *next;
  int is_default, logged;
  time_t mtime;
  char *name;
};

#define AH_DIR      1
#define AH_INACTIVE 2
struct hostsfile {
  struct hostsfile *next;
  int flags;
  char *fname;
  int index; 
};

struct frec {
  union mysockaddr source;
  struct all_addr dest;
  struct server *sentto; 
  struct randfd *rfd4;
#ifdef HAVE_IPV6
  struct randfd *rfd6;
#endif
  unsigned int iface;
  unsigned short orig_id, new_id;
  int fd, forwardall;
  unsigned int crc;
  time_t time;
  struct frec *next;
};

#define ACTION_DEL           1
#define ACTION_OLD_HOSTNAME  2
#define ACTION_OLD           3
#define ACTION_ADD           4

#define DHCP_CHADDR_MAX 16

struct dhcp_lease {
  int clid_len;          
  unsigned char *clid;   
  char *hostname, *fqdn; 
  char *old_hostname;    
  char auth_name;        
  char new;              
  char changed;          
  char aux_changed;      
  time_t expires;        
#ifdef HAVE_BROKEN_RTC
  unsigned int length;
#endif
  int hwaddr_len, hwaddr_type;
  unsigned char hwaddr[DHCP_CHADDR_MAX]; 
  struct in_addr addr, override, giaddr;
  unsigned char *vendorclass, *userclass, *supplied_hostname;
  unsigned int vendorclass_len, userclass_len, supplied_hostname_len;
  int last_interface;
  struct dhcp_lease *next;
};

struct dhcp_netid {
  char *net;
  struct dhcp_netid *next;
};

struct dhcp_netid_list {
  struct dhcp_netid *list;
  struct dhcp_netid_list *next;
};

struct hwaddr_config {
  int hwaddr_len, hwaddr_type;
  unsigned char hwaddr[DHCP_CHADDR_MAX];
  unsigned int wildcard_mask;
  struct hwaddr_config *next;
};

struct dhcp_config {
  unsigned int flags;
  int clid_len;          
  unsigned char *clid;   
  char *hostname, *domain;
  struct dhcp_netid netid;
  struct in_addr addr;
  time_t decline_time;
  unsigned int lease_time;
  struct hwaddr_config *hwaddr;
  struct dhcp_config *next;
};

#define CONFIG_DISABLE           1
#define CONFIG_CLID              2
#define CONFIG_TIME              8
#define CONFIG_NAME             16
#define CONFIG_ADDR             32
#define CONFIG_NETID            64
#define CONFIG_NOCLID          128
#define CONFIG_FROM_ETHERS     256    
#define CONFIG_ADDR_HOSTS      512    
#define CONFIG_DECLINED       1024    
#define CONFIG_BANK           2048    

struct dhcp_opt {
  int opt, len, flags;
  union {
    int encap;
    unsigned int wildcard_mask;
    unsigned char *vendor_class;
  } u;
  unsigned char *val;
  struct dhcp_netid *netid;
  struct dhcp_opt *next;
};

#define DHOPT_ADDR               1
#define DHOPT_STRING             2
#define DHOPT_ENCAPSULATE        4
#define DHOPT_ENCAP_MATCH        8
#define DHOPT_FORCE             16
#define DHOPT_BANK              32
#define DHOPT_ENCAP_DONE        64
#define DHOPT_MATCH            128
#define DHOPT_VENDOR           256
#define DHOPT_HEX              512
#define DHOPT_VENDOR_MATCH    1024

struct dhcp_boot {
  char *file, *sname;
  struct in_addr next_server;
  struct dhcp_netid *netid;
  struct dhcp_boot *next;
};

struct pxe_service {
  unsigned short CSA, type; 
  char *menu, *basename;
  struct in_addr server;
  struct dhcp_netid *netid;
  struct pxe_service *next;
};

#define MATCH_VENDOR     1
#define MATCH_USER       2
#define MATCH_CIRCUIT    3
#define MATCH_REMOTE     4
#define MATCH_SUBSCRIBER 5

struct dhcp_vendor {
  int len, match_type, option;
  char *data;
  struct dhcp_netid netid;
  struct dhcp_vendor *next;
};

struct dhcp_mac {
  unsigned int mask;
  int hwaddr_len, hwaddr_type;
  unsigned char hwaddr[DHCP_CHADDR_MAX];
  struct dhcp_netid netid;
  struct dhcp_mac *next;
};

struct dhcp_bridge {
  char iface[IF_NAMESIZE];
  struct dhcp_bridge *alias, *next;
};

struct cond_domain {
  char *domain;
  struct in_addr start, end;
  struct cond_domain *next;
};

struct dhcp_context {
  unsigned int lease_time, addr_epoch;
  struct in_addr netmask, broadcast;
  struct in_addr local, router;
  struct in_addr start, end; 
  int flags;
  struct dhcp_netid netid, *filter;
  struct dhcp_context *next, *current;
};

#define CONTEXT_STATIC    1
#define CONTEXT_NETMASK   2
#define CONTEXT_BRDCAST   4
#define CONTEXT_PROXY     8


typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;


struct dhcp_packet {
  u8 op, htype, hlen, hops;
  u32 xid;
  u16 secs, flags;
  struct in_addr ciaddr, yiaddr, siaddr, giaddr;
  u8 chaddr[DHCP_CHADDR_MAX], sname[64], file[128];
  u8 options[312];
};

struct ping_result {
  struct in_addr addr;
  time_t time;
  struct ping_result *next;
};

struct tftp_file {
  int refcount, fd;
  off_t size;
  dev_t dev;
  ino_t inode;
  char filename[];
};

struct tftp_transfer {
  int sockfd;
  time_t timeout;
  int backoff;
  unsigned int block, blocksize, expansion;
  off_t offset;
  struct sockaddr_in peer;
  char opt_blocksize, opt_transize, netascii, carrylf;
  struct tftp_file *file;
  struct tftp_transfer *next;
};

extern struct daemon {

  unsigned int options;
  struct resolvc default_resolv, *resolv_files;
  time_t last_resolv;
  struct mx_srv_record *mxnames;
  struct naptr *naptr;
  struct txt_record *txt;
  struct ptr_record *ptr;
  struct cname *cnames;
  struct interface_name *int_names;
  char *mxtarget;
  char *lease_file; 
  char *username, *groupname, *scriptuser;
  int group_set, osport;
  char *domain_suffix;
  struct cond_domain *cond_domain;
  char *runfile; 
  char *lease_change_command;
  struct iname *if_names, *if_addrs, *if_except, *dhcp_except;
  struct bogus_addr *bogus_addr;
  struct server *servers;
  int log_fac; 
  char *log_file; 
  int max_logs;  
  int cachesize, ftabsize;
  int port, query_port, min_port;
  unsigned long local_ttl, neg_ttl;
  struct hostsfile *addn_hosts;
  struct dhcp_context *dhcp;
  struct dhcp_config *dhcp_conf;
  struct dhcp_opt *dhcp_opts, *dhcp_match;
  struct dhcp_vendor *dhcp_vendors;
  struct dhcp_mac *dhcp_macs;
  struct dhcp_boot *boot_config;
  struct pxe_service *pxe_services;
  int enable_pxe;
  struct dhcp_netid_list *dhcp_ignore, *dhcp_ignore_names, *force_broadcast, *bootp_dynamic;
  char *dhcp_hosts_file, *dhcp_opts_file;
  int dhcp_max, tftp_max;
  int dhcp_server_port, dhcp_client_port;
  int start_tftp_port, end_tftp_port; 
  unsigned int min_leasetime;
  struct doctor *doctors;
  unsigned short edns_pktsz;
  char *tftp_prefix; 

  
  char *packet; 
  int packet_buff_sz; 
  char *namebuff; 
  unsigned int local_answer, queries_forwarded;
  struct frec *frec_list;
  struct serverfd *sfds;
  struct irec *interfaces;
  struct listener *listeners;
  struct server *last_server;
  time_t forwardtime;
  int forwardcount;
  struct server *srv_save; 
  size_t packet_len;       
  struct randfd *rfd_save; 
  pid_t tcp_pids[MAX_PROCS];
  struct randfd randomsocks[RANDOM_SOCKS];

  
  int dhcpfd, helperfd; 
#if defined(HAVE_LINUX_NETWORK)
  int netlinkfd;
#elif defined(HAVE_BSD_NETWORK)
  int dhcp_raw_fd, dhcp_icmp_fd;
#endif
  struct iovec dhcp_packet;
  char *dhcp_buff, *dhcp_buff2;
  struct ping_result *ping_results;
  FILE *lease_stream;
  struct dhcp_bridge *bridges;

  
  
  void *dbus;
#ifdef HAVE_DBUS
  struct watch *watches;
#endif

  
  struct tftp_transfer *tftp_trans;

} *daemon;

void cache_init(void);
void log_query(unsigned short flags, char *name, struct all_addr *addr, char *arg); 
char *record_source(int index);
void querystr(char *str, unsigned short type);
struct crec *cache_find_by_addr(struct crec *crecp,
				struct all_addr *addr, time_t now, 
				unsigned short prot);
struct crec *cache_find_by_name(struct crec *crecp, 
				char *name, time_t now, unsigned short  prot);
void cache_end_insert(void);
void cache_start_insert(void);
struct crec *cache_insert(char *name, struct all_addr *addr,
			  time_t now, unsigned long ttl, unsigned short flags);
void cache_reload(void);
void cache_add_dhcp_entry(char *host_name, struct in_addr *host_address, time_t ttd);
void cache_unhash_dhcp(void);
void dump_cache(time_t now);
char *cache_get_name(struct crec *crecp);
char *get_domain(struct in_addr addr);

unsigned short extract_request(HEADER *header, size_t qlen, 
			       char *name, unsigned short *typep);
size_t setup_reply(HEADER *header, size_t  qlen,
		   struct all_addr *addrp, unsigned short flags,
		   unsigned long local_ttl);
int extract_addresses(HEADER *header, size_t qlen, char *namebuff, time_t now);
size_t answer_request(HEADER *header, char *limit, size_t qlen,  
		   struct in_addr local_addr, struct in_addr local_netmask, time_t now);
int check_for_bogus_wildcard(HEADER *header, size_t qlen, char *name, 
			     struct bogus_addr *addr, time_t now);
unsigned char *find_pseudoheader(HEADER *header, size_t plen,
				 size_t *len, unsigned char **p, int *is_sign);
int check_for_local_domain(char *name, time_t now);
unsigned int questions_crc(HEADER *header, size_t plen, char *buff);
size_t resize_packet(HEADER *header, size_t plen, 
		  unsigned char *pheader, size_t hlen);

void rand_init(void);
unsigned short rand16(void);
int legal_hostname(char *c);
char *canonicalise(char *s, int *nomem);
unsigned char *do_rfc1035_name(unsigned char *p, char *sval);
void *safe_malloc(size_t size);
void safe_pipe(int *fd, int read_noblock);
void *whine_malloc(size_t size);
int sa_len(union mysockaddr *addr);
int sockaddr_isequal(union mysockaddr *s1, union mysockaddr *s2);
int hostname_isequal(char *a, char *b);
time_t dnsmasq_time(void);
int is_same_net(struct in_addr a, struct in_addr b, struct in_addr mask);
int retry_send(void);
void prettyprint_time(char *buf, unsigned int t);
int prettyprint_addr(union mysockaddr *addr, char *buf);
int parse_hex(char *in, unsigned char *out, int maxlen, 
	      unsigned int *wildcard_mask, int *mac_type);
int memcmp_masked(unsigned char *a, unsigned char *b, int len, 
		  unsigned int mask);
int expand_buf(struct iovec *iov, size_t size);
char *print_mac(char *buff, unsigned char *mac, int len);
void bump_maxfd(int fd, int *max);
int read_write(int fd, unsigned char *packet, int size, int rw);

void die(char *message, char *arg1, int exit_code);
int log_start(struct passwd *ent_pw, int errfd);
int log_reopen(char *log_file);
void my_syslog(int priority, const char *format, ...);
void set_log_writer(fd_set *set, int *maxfdp);
void check_log_writer(fd_set *set);
void flush_log(void);

void read_opts (int argc, char **argv, char *compile_opts);
char *option_string(unsigned char opt, int *is_ip, int *is_name);
void reread_dhcp(void);

void reply_query(int fd, int family, time_t now);
void receive_query(struct listener *listen, time_t now);
unsigned char *tcp_request(int confd, time_t now,
			   struct in_addr local_addr, struct in_addr netmask);
void server_gone(struct server *server);
struct frec *get_new_frec(time_t now, int *wait);

int indextoname(int fd, int index, char *name);
int local_bind(int fd, union mysockaddr *addr, char *intname, uint32_t mark, int is_tcp);
int random_sock(int family);
void pre_allocate_sfds(void);
int reload_servers(char *fname);
#ifdef __ANDROID__
int set_servers(const char *servers);
void set_interfaces(const char *interfaces);
#endif
void check_servers(void);
int enumerate_interfaces();
struct listener *create_wildcard_listeners(void);
struct listener *create_bound_listeners(void);
int iface_check(int family, struct all_addr *addr, char *name, int *indexp);
int fix_fd(int fd);
struct in_addr get_ifaddr(char *intr);

#ifdef HAVE_DHCP
void dhcp_init(void);
void dhcp_packet(time_t now);
struct dhcp_context *address_available(struct dhcp_context *context, 
				       struct in_addr addr,
				       struct dhcp_netid *netids);
struct dhcp_context *narrow_context(struct dhcp_context *context, 
				    struct in_addr taddr,
				    struct dhcp_netid *netids);
int match_netid(struct dhcp_netid *check, struct dhcp_netid *pool, int negonly);int address_allocate(struct dhcp_context *context,
		     struct in_addr *addrp, unsigned char *hwaddr, int hw_len,
		     struct dhcp_netid *netids, time_t now);
int config_has_mac(struct dhcp_config *config, unsigned char *hwaddr, int len, int type);
struct dhcp_config *find_config(struct dhcp_config *configs,
				struct dhcp_context *context,
				unsigned char *clid, int clid_len,
				unsigned char *hwaddr, int hw_len, 
				int hw_type, char *hostname);
void dhcp_update_configs(struct dhcp_config *configs);
void dhcp_read_ethers(void);
void check_dhcp_hosts(int fatal);
struct dhcp_config *config_find_by_address(struct dhcp_config *configs, struct in_addr addr);
char *strip_hostname(char *hostname);
char *host_from_dns(struct in_addr addr);
char *get_domain(struct in_addr addr);
#endif

#ifdef HAVE_DHCP
void lease_update_file(time_t now);
void lease_update_dns();
void lease_init(time_t now);
struct dhcp_lease *lease_allocate(struct in_addr addr);
void lease_set_hwaddr(struct dhcp_lease *lease, unsigned char *hwaddr,
		      unsigned char *clid, int hw_len, int hw_type, int clid_len);
void lease_set_hostname(struct dhcp_lease *lease, char *name, int auth);
void lease_set_expires(struct dhcp_lease *lease, unsigned int len, time_t now);
void lease_set_interface(struct dhcp_lease *lease, int interface);
struct dhcp_lease *lease_find_by_client(unsigned char *hwaddr, int hw_len, int hw_type,  
					unsigned char *clid, int clid_len);
struct dhcp_lease *lease_find_by_addr(struct in_addr addr);
void lease_prune(struct dhcp_lease *target, time_t now);
void lease_update_from_configs(void);
int do_script_run(time_t now);
void rerun_scripts(void);
#endif

#ifdef HAVE_DHCP
size_t dhcp_reply(struct dhcp_context *context, char *iface_name, int int_index,
		  size_t sz, time_t now, int unicast_dest, int *is_inform);
unsigned char *extended_hwaddr(int hwtype, int hwlen, unsigned char *hwaddr, 
			       int clid_len, unsigned char *clid, int *len_out);
#endif

#ifdef HAVE_DHCP
int make_icmp_sock(void);
int icmp_ping(struct in_addr addr);
#endif
void send_event(int fd, int event, int data);
void clear_cache_and_reload(time_t now);

#ifdef HAVE_LINUX_NETWORK
void netlink_init(void);
void netlink_multicast(void);
#endif

#ifdef HAVE_BSD_NETWORK
void init_bpf(void);
void send_via_bpf(struct dhcp_packet *mess, size_t len,
		  struct in_addr iface_addr, struct ifreq *ifr);
#endif

int iface_enumerate(void *parm, int (*ipv4_callback)(), int (*ipv6_callback)());

#ifdef HAVE_DBUS
char *dbus_init(void);
void check_dbus_listeners(fd_set *rset, fd_set *wset, fd_set *eset);
void set_dbus_listeners(int *maxfdp, fd_set *rset, fd_set *wset, fd_set *eset);
void emit_dbus_signal(int action, struct dhcp_lease *lease, char *hostname);
#endif

#if defined(HAVE_DHCP) && !defined(NO_FORK)
int create_helper(int event_fd, int err_fd, uid_t uid, gid_t gid, long max_fd);
void helper_write(void);
void queue_script(int action, struct dhcp_lease *lease, 
		  char *hostname, time_t now);
int helper_buf_empty(void);
#endif

#ifdef HAVE_TFTP
void tftp_request(struct listener *listen, time_t now);
void check_tftp_listeners(fd_set *rset, time_t now);
#endif
