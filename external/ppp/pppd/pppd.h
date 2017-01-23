/*
 * pppd.h - PPP daemon global declarations.
 *
 * Copyright (c) 1984-2000 Carnegie Mellon University. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The name "Carnegie Mellon University" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For permission or any legal
 *    details, please contact
 *      Office of Technology Transfer
 *      Carnegie Mellon University
 *      5000 Forbes Avenue
 *      Pittsburgh, PA  15213-3890
 *      (412) 268-4387, fax: (412) 268-7395
 *      tech-transfer@andrew.cmu.edu
 *
 * 4. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by Computing Services
 *     at Carnegie Mellon University (http://www.cmu.edu/computing/)."
 *
 * CARNEGIE MELLON UNIVERSITY DISCLAIMS ALL WARRANTIES WITH REGARD TO
 * THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS, IN NO EVENT SHALL CARNEGIE MELLON UNIVERSITY BE LIABLE
 * FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: pppd.h,v 1.96 2008/06/23 11:47:18 paulus Exp $
 */


#ifndef __PPPD_H__
#define __PPPD_H__

#include <stdio.h>		
#include <limits.h>		
#include <sys/param.h>		
#include <sys/types.h>		
#include <sys/time.h>		
#include <net/ppp_defs.h>
#include "patchlevel.h"

#if defined(__STDC__)
#include <stdarg.h>
#define __V(x)	x
#else
#include <varargs.h>
#define __V(x)	(va_alist) va_dcl
#define const
#define volatile
#endif

#ifdef INET6
#include "eui64.h"
#endif


#define NUM_PPP		1	
#define MAXWORDLEN	1024	
#define MAXARGS		1	
#define MAXNAMELEN	256	
#define MAXSECRETLEN	256	


typedef unsigned char	bool;

enum opt_type {
	o_special_noarg = 0,
	o_special = 1,
	o_bool,
	o_int,
	o_uint32,
	o_string,
	o_wild
};

typedef struct {
	char	*name;		
	enum opt_type type;
	void	*addr;
	char	*description;
	unsigned int flags;
	void	*addr2;
	int	upper_limit;
	int	lower_limit;
	const char *source;
	short int priority;
	short int winner;
} option_t;

#define OPT_VALUE	0xff	
#define OPT_HEX		0x100	
#define OPT_NOARG	0x200	
#define OPT_OR		0x400	
#define OPT_INC		0x400	
#define OPT_A2OR	0x800	
#define OPT_PRIV	0x1000	
#define OPT_STATIC	0x2000	
#define OPT_NOINCR	0x2000	
#define OPT_LLIMIT	0x4000	
#define OPT_ULIMIT	0x8000	
#define OPT_LIMITS	(OPT_LLIMIT|OPT_ULIMIT)
#define OPT_ZEROOK	0x10000	
#define OPT_HIDE	0x10000	
#define OPT_A2LIST	0x20000 
#define OPT_A2CLRB	0x20000 
#define OPT_ZEROINF	0x40000	
#define OPT_PRIO	0x80000	
#define OPT_PRIOSUB	0x100000 
#define OPT_ALIAS	0x200000 
#define OPT_A2COPY	0x400000 
#define OPT_ENABLE	0x800000 
#define OPT_A2CLR	0x1000000 
#define OPT_PRIVFIX	0x2000000 
#define OPT_INITONLY	0x4000000 
#define OPT_DEVEQUIV	0x8000000 
#define OPT_DEVNAM	(OPT_INITONLY | OPT_DEVEQUIV)
#define OPT_A2PRINTER	0x10000000 
#define OPT_A2STRVAL	0x20000000 
#define OPT_NOPRINT	0x40000000 

#define OPT_VAL(x)	((x) & OPT_VALUE)

#define OPRIO_DEFAULT	0	
#define OPRIO_CFGFILE	1	
#define OPRIO_CMDLINE	2	
#define OPRIO_SECFILE	3	
#define OPRIO_ROOT	100	

#ifndef GIDSET_TYPE
#define GIDSET_TYPE	gid_t
#endif

struct permitted_ip {
    int		permit;		
    u_int32_t	base;		
    u_int32_t	mask;		
};

struct pppd_stats {
    unsigned int	bytes_in;
    unsigned int	bytes_out;
    unsigned int	pkts_in;
    unsigned int	pkts_out;
};

struct wordlist {
    struct wordlist	*next;
    char		*word;
};

#define MAX_ENDP_LEN	20	
struct epdisc {
    unsigned char	class;
    unsigned char	length;
    unsigned char	value[MAX_ENDP_LEN];
};

#define EPD_NULL	0	
#define EPD_LOCAL	1
#define EPD_IP		2
#define EPD_MAC		3
#define EPD_MAGIC	4
#define EPD_PHONENUM	5

typedef void (*notify_func) __P((void *, int));
typedef void (*printer_func) __P((void *, char *, ...));

struct notifier {
    struct notifier *next;
    notify_func	    func;
    void	    *arg;
};


extern int	hungup;		
extern int	ifunit;		
extern char	ifname[];	
extern char	hostname[];	
extern u_char	outpacket_buf[]; 
extern int	devfd;		
extern int	fd_ppp;		
extern int	phase;		
extern int	baud_rate;	
extern char	*progname;	
extern int	redirect_stderr;
extern char	peer_authname[];
extern int	auth_done[NUM_PPP]; 
extern int	privileged;	
extern int	need_holdoff;	
extern char	**script_env;	
extern int	detached;	
extern GIDSET_TYPE groups[NGROUPS_MAX];	
extern int	ngroups;	
extern struct pppd_stats link_stats; 
extern int	link_stats_valid; 
extern unsigned	link_connect_time; 
extern int	using_pty;	
extern int	log_to_fd;	
extern bool	log_default;	
extern char	*no_ppp_msg;	
extern volatile int status;	
extern bool	devnam_fixed;	
extern int	unsuccess;	
extern int	do_callback;	
extern int	doing_callback;	
extern int	error_count;	
extern char	ppp_devnam[MAXPATHLEN];
extern char     remote_number[MAXNAMELEN]; 
extern int      ppp_session_number; 
extern int	fd_devnull;	

extern int	listen_time;	
extern bool	doing_multilink;
extern bool	multilink_master;
extern bool	bundle_eof;
extern bool	bundle_terminating;

extern struct notifier *pidchange;   
extern struct notifier *phasechange; 
extern struct notifier *exitnotify;  
extern struct notifier *sigreceived; 
extern struct notifier *ip_up_notifier;     
extern struct notifier *ip_down_notifier;   
extern struct notifier *ipv6_up_notifier;   
extern struct notifier *ipv6_down_notifier; 
extern struct notifier *auth_up_notifier; 
extern struct notifier *link_down_notifier; 
extern struct notifier *fork_notifier;	

#define CALLBACK_DIALIN		1	
#define CALLBACK_DIALOUT	2	


extern int	debug;		
extern int	kdebugflag;	
extern int	default_device;	
extern char	devnam[MAXPATHLEN];	
extern int	crtscts;	
extern int	stop_bits;	
extern bool	modem;		
extern int	inspeed;	
extern u_int32_t netmask;	
extern bool	lockflag;	
extern bool	nodetach;	
extern bool	updetach;	
extern bool	master_detach;	
extern char	*initializer;	
extern char	*connect_script; 
extern char	*disconnect_script; 
extern char	*welcomer;	
extern char	*ptycommand;	
extern int	maxconnect;	
extern char	user[MAXNAMELEN];
extern char	passwd[MAXSECRETLEN];	
extern bool	auth_required;	
extern bool	persist;	
extern bool	uselogin;	
extern bool	session_mgmt;	
extern char	our_name[MAXNAMELEN];
extern char	remote_name[MAXNAMELEN]; 
extern bool	explicit_remote;
extern bool	demand;		
extern char	*ipparam;	
extern bool	cryptpap;	
extern int	idle_time_limit;
extern int	holdoff;	
extern bool	holdoff_specified; 
extern bool	notty;		
extern char	*pty_socket;	
extern char	*record_file;	
extern bool	sync_serial;	
extern int	maxfail;	
extern char	linkname[MAXPATHLEN]; 
extern bool	tune_kernel;	
extern int	connect_delay;	
extern int	max_data_rate;	
extern int	req_unit;	
extern bool	multilink;	
extern bool	noendpoint;	
extern char	*bundle_name;	
extern bool	dump_options;	
extern bool	dryrun;		
extern int	child_wait;	

#ifdef MAXOCTETS
extern unsigned int maxoctets;	     
extern int       maxoctets_dir;      
extern int       maxoctets_timeout;  
#define PPP_OCTETS_DIRECTION_SUM        0
#define PPP_OCTETS_DIRECTION_IN         1
#define PPP_OCTETS_DIRECTION_OUT        2
#define PPP_OCTETS_DIRECTION_MAXOVERAL  3
#define PPP_OCTETS_DIRECTION_MAXSESSION 4	
#endif

#ifdef PPP_FILTER
extern struct	bpf_program pass_filter;   
extern struct	bpf_program active_filter; 
#endif

#ifdef MSLANMAN
extern bool	ms_lanman;	
				
#endif

#define PAP_WITHPEER	0x1
#define PAP_PEER	0x2
#define CHAP_WITHPEER	0x4
#define CHAP_PEER	0x8
#define EAP_WITHPEER	0x10
#define EAP_PEER	0x20

#define CHAP_MD5_WITHPEER	0x40
#define CHAP_MD5_PEER		0x80
#define CHAP_MS_SHIFT		8	
#define CHAP_MS_WITHPEER	0x100
#define CHAP_MS_PEER		0x200
#define CHAP_MS2_WITHPEER	0x400
#define CHAP_MS2_PEER		0x800

extern char *current_option;	
extern int  privileged_option;	
extern char *option_source;	
extern int  option_priority;	

#define PHASE_DEAD		0
#define PHASE_INITIALIZE	1
#define PHASE_SERIALCONN	2
#define PHASE_DORMANT		3
#define PHASE_ESTABLISH		4
#define PHASE_AUTHENTICATE	5
#define PHASE_CALLBACK		6
#define PHASE_NETWORK		7
#define PHASE_RUNNING		8
#define PHASE_TERMINATE		9
#define PHASE_DISCONNECT	10
#define PHASE_HOLDOFF		11
#define PHASE_MASTER		12

struct protent {
    u_short protocol;		
    
    void (*init) __P((int unit));
    
    void (*input) __P((int unit, u_char *pkt, int len));
    
    void (*protrej) __P((int unit));
    
    void (*lowerup) __P((int unit));
    
    void (*lowerdown) __P((int unit));
    
    void (*open) __P((int unit));
    
    void (*close) __P((int unit, char *reason));
    
    int  (*printpkt) __P((u_char *pkt, int len, printer_func printer,
			  void *arg));
    
    void (*datainput) __P((int unit, u_char *pkt, int len));
    bool enabled_flag;		
    char *name;			
    char *data_name;		
    option_t *options;		
    
    void (*check_options) __P((void));
    
    int  (*demand_conf) __P((int unit));
    
    int  (*active_pkt) __P((u_char *pkt, int len));
};

extern struct protent *protocols[];

struct channel {
	
	option_t *options;
	
	void (*process_extra_options) __P((void));
	
	void (*check_options) __P((void));
	
	int  (*connect) __P((void));
	
	void (*disconnect) __P((void));
	
	int  (*establish_ppp) __P((int));
	
	void (*disestablish_ppp) __P((int));
	
	void (*send_config) __P((int, u_int32_t, int, int));
	
	void (*recv_config) __P((int, u_int32_t, int, int));
	
	void (*cleanup) __P((void));
	
	void (*close) __P((void));
};

extern struct channel *the_channel;

struct userenv {
	struct userenv *ue_next;
	char *ue_value;		
	bool ue_isset;		
	bool ue_priv;		
	const char *ue_source;	
	char ue_name[1];	
};

extern struct userenv *userenv_list;


void set_ifunit __P((int));	
void detach __P((void));	
void die __P((int));		
void quit __P((void));		
void novm __P((char *));	
void timeout __P((void (*func)(void *), void *arg, int s, int us));
				
void untimeout __P((void (*func)(void *), void *arg));
				
void record_child __P((int, char *, void (*) (void *), void *, int));
pid_t safe_fork __P((int, int, int));	
int  device_script __P((char *cmd, int in, int out, int dont_wait));
				
pid_t run_program __P((char *prog, char **args, int must_exist,
		       void (*done)(void *), void *arg, int wait));
				
void reopen_log __P((void));	
void print_link_stats __P((void)); 
void reset_link_stats __P((int)); 
void update_link_stats __P((int)); 
void script_setenv __P((char *, char *, int));	
void script_unsetenv __P((char *));		
void new_phase __P((int));	
void add_notifier __P((struct notifier **, notify_func, void *));
void remove_notifier __P((struct notifier **, notify_func, void *));
void notify __P((struct notifier *, int));
int  ppp_send_config __P((int, int, u_int32_t, int, int));
int  ppp_recv_config __P((int, int, u_int32_t, int, int));
const char *protocol_name __P((int));
void remove_pidfiles __P((void));
void lock_db __P((void));
void unlock_db __P((void));

void tty_init __P((void));

void log_packet __P((u_char *, int, char *, int));
				
void print_string __P((char *, int,  printer_func, void *));
				
int slprintf __P((char *, int, char *, ...));		
int vslprintf __P((char *, int, char *, va_list));	
#if !defined(__ANDROID__)
size_t strlcpy __P((char *, const char *, size_t));	
size_t strlcat __P((char *, const char *, size_t));	
#endif
void dbglog __P((char *, ...));	
void info __P((char *, ...));	
void notice __P((char *, ...));	
void warn __P((char *, ...));	
void error __P((char *, ...));	
void fatal __P((char *, ...));	
void init_pr_log __P((const char *, int)); 
void pr_log __P((void *, char *, ...));	
void end_pr_log __P((void));	
void dump_packet __P((const char *, u_char *, int));
				
ssize_t complete_read __P((int, void *, size_t));
				

void link_required __P((int));	  
void start_link __P((int));	  
void link_terminated __P((int));  
void link_down __P((int));	  
void upper_layers_down __P((int));
void link_established __P((int)); 
void start_networks __P((int));   
void continue_networks __P((int)); 
void np_up __P((int, int));	  
void np_down __P((int, int));	  
void np_finished __P((int, int)); 
void auth_peer_fail __P((int, int));
				
void auth_peer_success __P((int, int, int, char *, int));
				
void auth_withpeer_fail __P((int, int));
				
void auth_withpeer_success __P((int, int, int));
				
void auth_check_options __P((void));
				
void auth_reset __P((int));	
int  check_passwd __P((int, char *, int, char *, int, char **));
				
int  get_secret __P((int, char *, char *, char *, int *, int));
				
int  get_srp_secret __P((int unit, char *client, char *server, char *secret,
    int am_server));
int  auth_ip_addr __P((int, u_int32_t));
				
int  auth_number __P((void));	
int  bad_ip_adrs __P((u_int32_t));
				

void demand_conf __P((void));	
void demand_block __P((void));	
void demand_unblock __P((void)); 
void demand_discard __P((void)); 
void demand_rexmit __P((int));	
int  loop_chars __P((unsigned char *, int)); 
int  loop_frame __P((unsigned char *, int)); 

#ifdef HAVE_MULTILINK
void mp_check_options __P((void)); 
int  mp_join_bundle __P((void));  
void mp_exit_bundle __P((void));  
void mp_bundle_terminated __P((void));
char *epdisc_to_str __P((struct epdisc *)); 
int  str_to_epdisc __P((struct epdisc *, char *)); 
#else
#define mp_bundle_terminated()	
#define mp_exit_bundle()	
#define doing_multilink		0
#define multilink_master	0
#endif

void sys_init __P((void));	
void sys_cleanup __P((void));	
int  sys_check_options __P((void)); 
void sys_close __P((void));	
int  ppp_available __P((void));	
int  get_pty __P((int *, int *, char *, int));	
int  open_ppp_loopback __P((void)); 
int  tty_establish_ppp __P((int));  
void tty_disestablish_ppp __P((int)); 
void generic_disestablish_ppp __P((int dev_fd)); 
int  generic_establish_ppp __P((int dev_fd)); 
void make_new_bundle __P((int, int, int, int)); 
int  bundle_attach __P((int));	
void cfg_bundle __P((int, int, int, int)); 
void destroy_bundle __P((void)); 
void clean_check __P((void));	
void set_up_tty __P((int, int)); 
void restore_tty __P((int));	
void setdtr __P((int, int));	
void output __P((int, u_char *, int)); 
void wait_input __P((struct timeval *));
				
void add_fd __P((int));		
void remove_fd __P((int));	
int  read_packet __P((u_char *)); 
int  get_loop_output __P((void)); 
void tty_send_config __P((int, u_int32_t, int, int));
				
void tty_set_xaccm __P((ext_accm));
				
void tty_recv_config __P((int, u_int32_t, int, int));
				
int  ccp_test __P((int, u_char *, int, int));
				
void ccp_flags_set __P((int, int, int));
				
int  ccp_fatal_error __P((int)); 
int  get_idle_time __P((int, struct ppp_idle *));
				
int  get_ppp_stats __P((int, struct pppd_stats *));
				
void netif_set_mtu __P((int, int)); 
int  netif_get_mtu __P((int));      
int  sifvjcomp __P((int, int, int, int));
				
int  sifup __P((int));		
int  sifnpmode __P((int u, int proto, enum NPmode mode));
				
int  sifdown __P((int));	
int  sifaddr __P((int, u_int32_t, u_int32_t, u_int32_t));
				
int  cifaddr __P((int, u_int32_t, u_int32_t));
				
#ifdef INET6
int  ether_to_eui64(eui64_t *p_eui64);	
int  sif6up __P((int));		
int  sif6down __P((int));	
int  sif6addr __P((int, eui64_t, eui64_t));
				
int  cif6addr __P((int, eui64_t, eui64_t));
				
#endif
int  sifdefaultroute __P((int, u_int32_t, u_int32_t));
				
int  cifdefaultroute __P((int, u_int32_t, u_int32_t));
				
int  sifproxyarp __P((int, u_int32_t));
				
int  cifproxyarp __P((int, u_int32_t));
				
u_int32_t GetMask __P((u_int32_t)); 
int  lock __P((char *));	
int  relock __P((int));		
void unlock __P((void));	
void logwtmp __P((const char *, const char *, const char *));
				
int  get_host_seed __P((void));	
int  have_route_to __P((u_int32_t)); 
#ifdef PPP_FILTER
int  set_filters __P((struct bpf_program *pass, struct bpf_program *active));
				
#endif
#ifdef IPX_CHANGE
int  sipxfaddr __P((int, unsigned long, unsigned char *));
int  cipxfaddr __P((int));
#endif
int  get_if_hwaddr __P((u_char *addr, char *name));
char *get_first_ethernet __P((void));

int setipaddr __P((char *, char **, int)); 
int  parse_args __P((int argc, char **argv));
				
int  options_from_file __P((char *filename, int must_exist, int check_prot,
			    int privileged));
				
int  options_from_user __P((void)); 
int  options_for_tty __P((void)); 
int  options_from_list __P((struct wordlist *, int privileged));
				
int  getword __P((FILE *f, char *word, int *newlinep, char *filename));
				
void option_error __P((char *fmt, ...));
				
int int_option __P((char *, int *));
				
void add_options __P((option_t *)); 
void check_options __P((void));	
int  override_value __P((const char *, int, const char *));
				
void print_options __P((printer_func, void *));
				

int parse_dotted_ip __P((char *, u_int32_t *));

extern int (*new_phase_hook) __P((int));
extern int (*idle_time_hook) __P((struct ppp_idle *));
extern int (*holdoff_hook) __P((void));
extern int (*pap_check_hook) __P((void));
extern int (*pap_auth_hook) __P((char *user, char *passwd, char **msgp,
				 struct wordlist **paddrs,
				 struct wordlist **popts));
extern void (*pap_logout_hook) __P((void));
extern int (*pap_passwd_hook) __P((char *user, char *passwd));
extern int (*allowed_address_hook) __P((u_int32_t addr));
extern void (*ip_up_hook) __P((void));
extern void (*ip_down_hook) __P((void));
extern void (*ip_choose_hook) __P((u_int32_t *));
extern void (*ipv6_up_hook) __P((void));
extern void (*ipv6_down_hook) __P((void));

extern int (*chap_check_hook) __P((void));
extern int (*chap_passwd_hook) __P((char *user, char *passwd));
extern void (*multilink_join_hook) __P((void));

extern void (*snoop_recv_hook) __P((unsigned char *p, int len));
extern void (*snoop_send_hook) __P((unsigned char *p, int len));

#define GETCHAR(c, cp) { \
	(c) = *(cp)++; \
}
#define PUTCHAR(c, cp) { \
	*(cp)++ = (u_char) (c); \
}


#define GETSHORT(s, cp) { \
	(s) = *(cp)++ << 8; \
	(s) |= *(cp)++; \
}
#define PUTSHORT(s, cp) { \
	*(cp)++ = (u_char) ((s) >> 8); \
	*(cp)++ = (u_char) (s); \
}

#define GETLONG(l, cp) { \
	(l) = *(cp)++ << 8; \
	(l) |= *(cp)++; (l) <<= 8; \
	(l) |= *(cp)++; (l) <<= 8; \
	(l) |= *(cp)++; \
}
#define PUTLONG(l, cp) { \
	*(cp)++ = (u_char) ((l) >> 24); \
	*(cp)++ = (u_char) ((l) >> 16); \
	*(cp)++ = (u_char) ((l) >> 8); \
	*(cp)++ = (u_char) (l); \
}

#define INCPTR(n, cp)	((cp) += (n))
#define DECPTR(n, cp)	((cp) -= (n))


#define TIMEOUT(r, f, t)	timeout((r), (f), (t), 0)
#define UNTIMEOUT(r, f)		untimeout((r), (f))

#define BCOPY(s, d, l)		memcpy(d, s, l)
#define BZERO(s, n)		memset(s, 0, n)
#define	BCMP(s1, s2, l)		memcmp(s1, s2, l)

#define PRINTMSG(m, l)		{ info("Remote message: %0.*v", l, m); }

#define MAKEHEADER(p, t) { \
    PUTCHAR(PPP_ALLSTATIONS, p); \
    PUTCHAR(PPP_UI, p); \
    PUTSHORT(t, p); }

#define EXIT_OK			0
#define EXIT_FATAL_ERROR	1
#define EXIT_OPTION_ERROR	2
#define EXIT_NOT_ROOT		3
#define EXIT_NO_KERNEL_SUPPORT	4
#define EXIT_USER_REQUEST	5
#define EXIT_LOCK_FAILED	6
#define EXIT_OPEN_FAILED	7
#define EXIT_CONNECT_FAILED	8
#define EXIT_PTYCMD_FAILED	9
#define EXIT_NEGOTIATION_FAILED	10
#define EXIT_PEER_AUTH_FAILED	11
#define EXIT_IDLE_TIMEOUT	12
#define EXIT_CONNECT_TIME	13
#define EXIT_CALLBACK		14
#define EXIT_PEER_DEAD		15
#define EXIT_HANGUP		16
#define EXIT_LOOPBACK		17
#define EXIT_INIT_FAILED	18
#define EXIT_AUTH_TOPEER_FAILED	19
#ifdef MAXOCTETS
#define EXIT_TRAFFIC_LIMIT	20
#endif
#define EXIT_CNID_AUTH_FAILED	21

#ifdef DEBUGALL
#define DEBUGMAIN	1
#define DEBUGFSM	1
#define DEBUGLCP	1
#define DEBUGIPCP	1
#define DEBUGIPV6CP	1
#define DEBUGUPAP	1
#define DEBUGCHAP	1
#endif

#ifndef LOG_PPP			
#if defined(DEBUGMAIN) || defined(DEBUGFSM) || defined(DEBUGSYS) \
  || defined(DEBUGLCP) || defined(DEBUGIPCP) || defined(DEBUGUPAP) \
  || defined(DEBUGCHAP) || defined(DEBUG) || defined(DEBUGIPV6CP)
#define LOG_PPP LOG_LOCAL2
#else
#define LOG_PPP LOG_DAEMON
#endif
#endif 

#ifdef DEBUGMAIN
#define MAINDEBUG(x)	if (debug) dbglog x
#else
#define MAINDEBUG(x)
#endif

#ifdef DEBUGSYS
#define SYSDEBUG(x)	if (debug) dbglog x
#else
#define SYSDEBUG(x)
#endif

#ifdef DEBUGFSM
#define FSMDEBUG(x)	if (debug) dbglog x
#else
#define FSMDEBUG(x)
#endif

#ifdef DEBUGLCP
#define LCPDEBUG(x)	if (debug) dbglog x
#else
#define LCPDEBUG(x)
#endif

#ifdef DEBUGIPCP
#define IPCPDEBUG(x)	if (debug) dbglog x
#else
#define IPCPDEBUG(x)
#endif

#ifdef DEBUGIPV6CP
#define IPV6CPDEBUG(x)  if (debug) dbglog x
#else
#define IPV6CPDEBUG(x)
#endif

#ifdef DEBUGUPAP
#define UPAPDEBUG(x)	if (debug) dbglog x
#else
#define UPAPDEBUG(x)
#endif

#ifdef DEBUGCHAP
#define CHAPDEBUG(x)	if (debug) dbglog x
#else
#define CHAPDEBUG(x)
#endif

#ifdef DEBUGIPXCP
#define IPXCPDEBUG(x)	if (debug) dbglog x
#else
#define IPXCPDEBUG(x)
#endif

#ifndef SIGTYPE
#if defined(sun) || defined(SYSV) || defined(POSIX_SOURCE)
#define SIGTYPE void
#else
#define SIGTYPE int
#endif 
#endif 

#ifndef MIN
#define MIN(a, b)	((a) < (b)? (a): (b))
#endif
#ifndef MAX
#define MAX(a, b)	((a) > (b)? (a): (b))
#endif

#ifndef offsetof
#define offsetof(type, member) ((size_t) &((type *)0)->member)
#endif

#endif 
