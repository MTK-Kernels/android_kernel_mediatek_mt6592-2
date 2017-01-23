/*
 * auth.c - PPP authentication and phase control.
 *
 * Copyright (c) 1993-2002 Paul Mackerras. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. The name(s) of the authors of this software must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission.
 *
 * 3. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by Paul Mackerras
 *     <paulus@samba.org>".
 *
 * THE AUTHORS OF THIS SOFTWARE DISCLAIM ALL WARRANTIES WITH REGARD TO
 * THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS, IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Derived from main.c, which is:
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
 */

#define RCSID	"$Id: auth.c,v 1.117 2008/07/01 12:27:56 paulus Exp $"

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <pwd.h>
#include <grp.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <utmp.h>
#include <fcntl.h>
#if defined(_PATH_LASTLOG) && defined(__linux__)
#include <lastlog.h>
#endif

#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#ifdef HAS_SHADOW
#include <shadow.h>
#ifndef PW_PPP
#define PW_PPP PW_LOGIN
#endif
#endif
#include <time.h>

#include "pppd.h"
#include "fsm.h"
#include "lcp.h"
#include "ccp.h"
#include "ecp.h"
#include "ipcp.h"
#include "upap.h"
#include "chap-new.h"
#include "eap.h"
#ifdef CBCP_SUPPORT
#include "cbcp.h"
#endif
#include "pathnames.h"
#include "session.h"

static const char rcsid[] = RCSID;

#define NONWILD_SERVER	1
#define NONWILD_CLIENT	2

#define ISWILD(word)	(word[0] == '*' && word[1] == 0)

char peer_authname[MAXNAMELEN];

static int auth_pending[NUM_PPP];

int auth_done[NUM_PPP];

static struct permitted_ip *addresses[NUM_PPP];

static struct wordlist *noauth_addrs;

char remote_number[MAXNAMELEN];

static struct wordlist *permitted_numbers;

static struct wordlist *extra_options;

static int num_np_open;

static int num_np_up;

static int passwd_from_file;

static bool default_auth;

int (*idle_time_hook) __P((struct ppp_idle *)) = NULL;

int (*pap_check_hook) __P((void)) = NULL;

int (*pap_auth_hook) __P((char *user, char *passwd, char **msgp,
			  struct wordlist **paddrs,
			  struct wordlist **popts)) = NULL;

void (*pap_logout_hook) __P((void)) = NULL;

int (*pap_passwd_hook) __P((char *user, char *passwd)) = NULL;

int (*chap_check_hook) __P((void)) = NULL;

int (*chap_passwd_hook) __P((char *user, char *passwd)) = NULL;

int (*null_auth_hook) __P((struct wordlist **paddrs,
			   struct wordlist **popts)) = NULL;

int (*allowed_address_hook) __P((u_int32_t addr)) = NULL;

#ifdef HAVE_MULTILINK
void (*multilink_join_hook) __P((void)) = NULL;
#endif

struct notifier *auth_up_notifier = NULL;

struct notifier *link_down_notifier = NULL;

enum script_state {
    s_down,
    s_up
};

static enum script_state auth_state = s_down;
static enum script_state auth_script_state = s_down;
static pid_t auth_script_pid = 0;

bool uselogin = 0;		
bool session_mgmt = 0;		
bool cryptpap = 0;		
bool refuse_pap = 0;		
bool refuse_chap = 0;		
bool refuse_eap = 0;		
#ifdef CHAPMS
bool refuse_mschap = 0;		
bool refuse_mschap_v2 = 0;	
#else
bool refuse_mschap = 1;		
bool refuse_mschap_v2 = 1;	
#endif
bool usehostname = 0;		
bool auth_required = 0;		
bool allow_any_ip = 0;		
bool explicit_remote = 0;	
bool explicit_user = 0;		
bool explicit_passwd = 0;	
char remote_name[MAXNAMELEN];	

static char *uafname;		

extern char *crypt __P((const char *, const char *));


static void network_phase __P((int));
static void check_idle __P((void *));
static void connect_time_expired __P((void *));
static int  null_login __P((int));
static int  get_pap_passwd __P((char *));
static int  have_pap_secret __P((int *));
static int  have_chap_secret __P((char *, char *, int, int *));
static int  have_srp_secret __P((char *client, char *server, int need_ip,
    int *lacks_ipp));
static int  ip_addr_check __P((u_int32_t, struct permitted_ip *));
static int  scan_authfile __P((FILE *, char *, char *, char *,
			       struct wordlist **, struct wordlist **,
			       char *, int));
static void free_wordlist __P((struct wordlist *));
static void auth_script __P((char *));
static void auth_script_done __P((void *));
static void set_allowed_addrs __P((int, struct wordlist *, struct wordlist *));
static int  some_ip_ok __P((struct wordlist *));
static int  setupapfile __P((char **));
static int  privgroup __P((char **));
static int  set_noauth_addr __P((char **));
static int  set_permitted_number __P((char **));
static void check_access __P((FILE *, char *));
static int  wordlist_count __P((struct wordlist *));

#ifdef MAXOCTETS
static void check_maxoctets __P((void *));
#endif

option_t auth_options[] = {
    { "auth", o_bool, &auth_required,
      "Require authentication from peer", OPT_PRIO | 1 },
    { "noauth", o_bool, &auth_required,
      "Don't require peer to authenticate", OPT_PRIOSUB | OPT_PRIV,
      &allow_any_ip },
    { "require-pap", o_bool, &lcp_wantoptions[0].neg_upap,
      "Require PAP authentication from peer",
      OPT_PRIOSUB | 1, &auth_required },
    { "+pap", o_bool, &lcp_wantoptions[0].neg_upap,
      "Require PAP authentication from peer",
      OPT_ALIAS | OPT_PRIOSUB | 1, &auth_required },
    { "require-chap", o_bool, &auth_required,
      "Require CHAP authentication from peer",
      OPT_PRIOSUB | OPT_A2OR | MDTYPE_MD5,
      &lcp_wantoptions[0].chap_mdtype },
    { "+chap", o_bool, &auth_required,
      "Require CHAP authentication from peer",
      OPT_ALIAS | OPT_PRIOSUB | OPT_A2OR | MDTYPE_MD5,
      &lcp_wantoptions[0].chap_mdtype },
#ifdef CHAPMS
    { "require-mschap", o_bool, &auth_required,
      "Require MS-CHAP authentication from peer",
      OPT_PRIOSUB | OPT_A2OR | MDTYPE_MICROSOFT,
      &lcp_wantoptions[0].chap_mdtype },
    { "+mschap", o_bool, &auth_required,
      "Require MS-CHAP authentication from peer",
      OPT_ALIAS | OPT_PRIOSUB | OPT_A2OR | MDTYPE_MICROSOFT,
      &lcp_wantoptions[0].chap_mdtype },
    { "require-mschap-v2", o_bool, &auth_required,
      "Require MS-CHAPv2 authentication from peer",
      OPT_PRIOSUB | OPT_A2OR | MDTYPE_MICROSOFT_V2,
      &lcp_wantoptions[0].chap_mdtype },
    { "+mschap-v2", o_bool, &auth_required,
      "Require MS-CHAPv2 authentication from peer",
      OPT_ALIAS | OPT_PRIOSUB | OPT_A2OR | MDTYPE_MICROSOFT_V2,
      &lcp_wantoptions[0].chap_mdtype },
#endif

    { "refuse-pap", o_bool, &refuse_pap,
      "Don't agree to auth to peer with PAP", 1 },
    { "-pap", o_bool, &refuse_pap,
      "Don't allow PAP authentication with peer", OPT_ALIAS | 1 },
    { "refuse-chap", o_bool, &refuse_chap,
      "Don't agree to auth to peer with CHAP",
      OPT_A2CLRB | MDTYPE_MD5,
      &lcp_allowoptions[0].chap_mdtype },
    { "-chap", o_bool, &refuse_chap,
      "Don't allow CHAP authentication with peer",
      OPT_ALIAS | OPT_A2CLRB | MDTYPE_MD5,
      &lcp_allowoptions[0].chap_mdtype },
#ifdef CHAPMS
    { "refuse-mschap", o_bool, &refuse_mschap,
      "Don't agree to auth to peer with MS-CHAP",
      OPT_A2CLRB | MDTYPE_MICROSOFT,
      &lcp_allowoptions[0].chap_mdtype },
    { "-mschap", o_bool, &refuse_mschap,
      "Don't allow MS-CHAP authentication with peer",
      OPT_ALIAS | OPT_A2CLRB | MDTYPE_MICROSOFT,
      &lcp_allowoptions[0].chap_mdtype },
    { "refuse-mschap-v2", o_bool, &refuse_mschap_v2,
      "Don't agree to auth to peer with MS-CHAPv2",
      OPT_A2CLRB | MDTYPE_MICROSOFT_V2,
      &lcp_allowoptions[0].chap_mdtype },
    { "-mschap-v2", o_bool, &refuse_mschap_v2,
      "Don't allow MS-CHAPv2 authentication with peer",
      OPT_ALIAS | OPT_A2CLRB | MDTYPE_MICROSOFT_V2,
      &lcp_allowoptions[0].chap_mdtype },
#endif

    { "require-eap", o_bool, &lcp_wantoptions[0].neg_eap,
      "Require EAP authentication from peer", OPT_PRIOSUB | 1,
      &auth_required },
    { "refuse-eap", o_bool, &refuse_eap,
      "Don't agree to authenticate to peer with EAP", 1 },

    { "name", o_string, our_name,
      "Set local name for authentication",
      OPT_PRIO | OPT_PRIV | OPT_STATIC, NULL, MAXNAMELEN },

    { "+ua", o_special, (void *)setupapfile,
      "Get PAP user and password from file",
      OPT_PRIO | OPT_A2STRVAL, &uafname },

    { "user", o_string, user,
      "Set name for auth with peer", OPT_PRIO | OPT_STATIC,
      &explicit_user, MAXNAMELEN },

    { "password", o_string, passwd,
      "Password for authenticating us to the peer",
      OPT_PRIO | OPT_STATIC | OPT_HIDE,
      &explicit_passwd, MAXSECRETLEN },

    { "usehostname", o_bool, &usehostname,
      "Must use hostname for authentication", 1 },

    { "remotename", o_string, remote_name,
      "Set remote name for authentication", OPT_PRIO | OPT_STATIC,
      &explicit_remote, MAXNAMELEN },

    { "login", o_bool, &uselogin,
      "Use system password database for PAP", OPT_A2COPY | 1 ,
      &session_mgmt },
    { "enable-session", o_bool, &session_mgmt,
      "Enable session accounting for remote peers", OPT_PRIV | 1 },

    { "papcrypt", o_bool, &cryptpap,
      "PAP passwords are encrypted", 1 },

    { "privgroup", o_special, (void *)privgroup,
      "Allow group members to use privileged options", OPT_PRIV | OPT_A2LIST },

    { "allow-ip", o_special, (void *)set_noauth_addr,
      "Set IP address(es) which can be used without authentication",
      OPT_PRIV | OPT_A2LIST },

    { "remotenumber", o_string, remote_number,
      "Set remote telephone number for authentication", OPT_PRIO | OPT_STATIC,
      NULL, MAXNAMELEN },

    { "allow-number", o_special, (void *)set_permitted_number,
      "Set telephone number(s) which are allowed to connect",
      OPT_PRIV | OPT_A2LIST },

    { NULL }
};

static int
setupapfile(argv)
    char **argv;
{
    FILE *ufile;
    int l;
    uid_t euid;
    char u[MAXNAMELEN], p[MAXSECRETLEN];
    char *fname;

    lcp_allowoptions[0].neg_upap = 1;

    
    fname = strdup(*argv);
    if (fname == NULL)
	novm("+ua file name");
    euid = geteuid();
    if (seteuid(getuid()) == -1) {
	option_error("unable to reset uid before opening %s: %m", fname);
	return 0;
    }
    ufile = fopen(fname, "r");
    if (seteuid(euid) == -1)
	fatal("unable to regain privileges: %m");
    if (ufile == NULL) {
	option_error("unable to open user login data file %s", fname);
	return 0;
    }
    check_access(ufile, fname);
    uafname = fname;

    
    if (fgets(u, MAXNAMELEN - 1, ufile) == NULL
	|| fgets(p, MAXSECRETLEN - 1, ufile) == NULL) {
	fclose(ufile);
	option_error("unable to read user login data file %s", fname);
	return 0;
    }
    fclose(ufile);

    
    l = strlen(u);
    if (l > 0 && u[l-1] == '\n')
	u[l-1] = 0;
    l = strlen(p);
    if (l > 0 && p[l-1] == '\n')
	p[l-1] = 0;

    if (override_value("user", option_priority, fname)) {
	strlcpy(user, u, sizeof(user));
	explicit_user = 1;
    }
    if (override_value("passwd", option_priority, fname)) {
	strlcpy(passwd, p, sizeof(passwd));
	explicit_passwd = 1;
    }

    return (1);
}


static int
privgroup(argv)
    char **argv;
{
    struct group *g;
    int i;

    g = getgrnam(*argv);
    if (g == 0) {
	option_error("group %s is unknown", *argv);
	return 0;
    }
    for (i = 0; i < ngroups; ++i) {
	if (groups[i] == g->gr_gid) {
	    privileged = 1;
	    break;
	}
    }
    return 1;
}


static int
set_noauth_addr(argv)
    char **argv;
{
    char *addr = *argv;
    int l = strlen(addr) + 1;
    struct wordlist *wp;

    wp = (struct wordlist *) malloc(sizeof(struct wordlist) + l);
    if (wp == NULL)
	novm("allow-ip argument");
    wp->word = (char *) (wp + 1);
    wp->next = noauth_addrs;
    BCOPY(addr, wp->word, l);
    noauth_addrs = wp;
    return 1;
}


static int
set_permitted_number(argv)
    char **argv;
{
    char *number = *argv;
    int l = strlen(number) + 1;
    struct wordlist *wp;

    wp = (struct wordlist *) malloc(sizeof(struct wordlist) + l);
    if (wp == NULL)
	novm("allow-number argument");
    wp->word = (char *) (wp + 1);
    wp->next = permitted_numbers;
    BCOPY(number, wp->word, l);
    permitted_numbers = wp;
    return 1;
}


void
link_required(unit)
    int unit;
{
}

void start_link(unit)
    int unit;
{
    status = EXIT_CONNECT_FAILED;
    new_phase(PHASE_SERIALCONN);

    hungup = 0;
    devfd = the_channel->connect();
    if (devfd < 0)
	goto fail;

    
    fd_ppp = the_channel->establish_ppp(devfd);
    if (fd_ppp < 0) {
	status = EXIT_FATAL_ERROR;
	goto disconnect;
    }

    if (!demand && ifunit >= 0)
	set_ifunit(1);

    if (ifunit >= 0)
	notice("Connect: %s <--> %s", ifname, ppp_devnam);
    else
	notice("Starting negotiation on %s", ppp_devnam);
    add_fd(fd_ppp);

    status = EXIT_NEGOTIATION_FAILED;
    new_phase(PHASE_ESTABLISH);

    lcp_lowerup(0);
    return;

 disconnect:
    new_phase(PHASE_DISCONNECT);
    if (the_channel->disconnect)
	the_channel->disconnect();

 fail:
    new_phase(PHASE_DEAD);
    if (the_channel->cleanup)
	(*the_channel->cleanup)();
}

void
link_terminated(unit)
    int unit;
{
    if (phase == PHASE_DEAD || phase == PHASE_MASTER)
	return;
    new_phase(PHASE_DISCONNECT);

    if (pap_logout_hook) {
	pap_logout_hook();
    }
    session_end(devnam);

    if (!doing_multilink) {
	notice("Connection terminated.");
	print_link_stats();
    } else
	notice("Link terminated.");

    if (!doing_multilink && !demand)
	remove_pidfiles();

    if (fd_ppp >= 0) {
	remove_fd(fd_ppp);
	clean_check();
	the_channel->disestablish_ppp(devfd);
	if (doing_multilink)
	    mp_exit_bundle();
	fd_ppp = -1;
    }
    if (!hungup)
	lcp_lowerdown(0);
    if (!doing_multilink && !demand)
	script_unsetenv("IFNAME");

    if (devfd >= 0 && the_channel->disconnect) {
	the_channel->disconnect();
	devfd = -1;
    }
    if (the_channel->cleanup)
	(*the_channel->cleanup)();

    if (doing_multilink && multilink_master) {
	if (!bundle_terminating) {
	    new_phase(PHASE_MASTER);
	    if (master_detach && !detached)
		detach();
	} else
	    mp_bundle_terminated();
    } else
	new_phase(PHASE_DEAD);
}

void
link_down(unit)
    int unit;
{
    if (auth_state != s_down) {
	notify(link_down_notifier, 0);
	auth_state = s_down;
	if (auth_script_state == s_up && auth_script_pid == 0) {
	    update_link_stats(unit);
	    auth_script_state = s_down;
	    auth_script(_PATH_AUTHDOWN);
	}
    }
    if (!doing_multilink) {
	upper_layers_down(unit);
	if (phase != PHASE_DEAD && phase != PHASE_MASTER)
	    new_phase(PHASE_ESTABLISH);
    }
}

void upper_layers_down(int unit)
{
    int i;
    struct protent *protp;

    for (i = 0; (protp = protocols[i]) != NULL; ++i) {
	if (!protp->enabled_flag)
	    continue;
        if (protp->protocol != PPP_LCP && protp->lowerdown != NULL)
	    (*protp->lowerdown)(unit);
        if (protp->protocol < 0xC000 && protp->close != NULL)
	    (*protp->close)(unit, "LCP down");
    }
    num_np_open = 0;
    num_np_up = 0;
}

void
link_established(unit)
    int unit;
{
    int auth;
    lcp_options *wo = &lcp_wantoptions[unit];
    lcp_options *go = &lcp_gotoptions[unit];
    lcp_options *ho = &lcp_hisoptions[unit];
    int i;
    struct protent *protp;

    if (!doing_multilink) {
	for (i = 0; (protp = protocols[i]) != NULL; ++i)
	    if (protp->protocol != PPP_LCP && protp->enabled_flag
		&& protp->lowerup != NULL)
		(*protp->lowerup)(unit);
    }

    if (!auth_required && noauth_addrs != NULL)
	set_allowed_addrs(unit, NULL, NULL);

    if (auth_required && !(go->neg_upap || go->neg_chap || go->neg_eap)) {
	if (noauth_addrs != NULL) {
	    set_allowed_addrs(unit, NULL, NULL);
	} else if (!wo->neg_upap || uselogin || !null_login(unit)) {
	    warn("peer refused to authenticate: terminating link");
	    status = EXIT_PEER_AUTH_FAILED;
	    lcp_close(unit, "peer refused to authenticate");
	    return;
	}
    }

    new_phase(PHASE_AUTHENTICATE);
    auth = 0;
    if (go->neg_eap) {
	eap_authpeer(unit, our_name);
	auth |= EAP_PEER;
    } else if (go->neg_chap) {
	chap_auth_peer(unit, our_name, CHAP_DIGEST(go->chap_mdtype));
	auth |= CHAP_PEER;
    } else if (go->neg_upap) {
	upap_authpeer(unit);
	auth |= PAP_PEER;
    }
    if (ho->neg_eap) {
	eap_authwithpeer(unit, user);
	auth |= EAP_WITHPEER;
    } else if (ho->neg_chap) {
	chap_auth_with_peer(unit, user, CHAP_DIGEST(ho->chap_mdtype));
	auth |= CHAP_WITHPEER;
    } else if (ho->neg_upap) {
	if (passwd[0] == 0 && !explicit_passwd) {
	    passwd_from_file = 1;
	    if (!get_pap_passwd(passwd))
		error("No secret found for PAP login");
	}
	upap_authwithpeer(unit, user, passwd);
	auth |= PAP_WITHPEER;
    }
    auth_pending[unit] = auth;
    auth_done[unit] = 0;

    if (!auth)
	network_phase(unit);
}

static void
network_phase(unit)
    int unit;
{
    lcp_options *go = &lcp_gotoptions[unit];

    
    if (*remote_number)
	notice("peer from calling number %q authorized", remote_number);

    if (go->neg_chap || go->neg_upap || go->neg_eap) {
	notify(auth_up_notifier, 0);
	auth_state = s_up;
	if (auth_script_state == s_down && auth_script_pid == 0) {
	    auth_script_state = s_up;
	    auth_script(_PATH_AUTHUP);
	}
    }

#ifdef CBCP_SUPPORT
    if (go->neg_cbcp) {
	new_phase(PHASE_CALLBACK);
	(*cbcp_protent.open)(unit);
	return;
    }
#endif

    if (extra_options) {
	options_from_list(extra_options, 1);
	free_wordlist(extra_options);
	extra_options = 0;
    }
    start_networks(unit);
}

void
start_networks(unit)
    int unit;
{
    int i;
    struct protent *protp;
    int ecp_required, mppe_required;

    new_phase(PHASE_NETWORK);

#ifdef HAVE_MULTILINK
    if (multilink) {
	if (mp_join_bundle()) {
	    if (multilink_join_hook)
		(*multilink_join_hook)();
	    if (updetach && !nodetach)
		detach();
	    return;
	}
    }
#endif 

#ifdef PPP_FILTER
    if (!demand)
	set_filters(&pass_filter, &active_filter);
#endif
    
    for (i = 0; (protp = protocols[i]) != NULL; ++i)
	if ((protp->protocol == PPP_ECP || protp->protocol == PPP_CCP)
	    && protp->enabled_flag && protp->open != NULL)
	    (*protp->open)(0);

    ecp_required = ecp_gotoptions[unit].required;
    mppe_required = ccp_gotoptions[unit].mppe;
    if (!ecp_required && !mppe_required)
	continue_networks(unit);
}

void
continue_networks(unit)
    int unit;
{
    int i;
    struct protent *protp;

    for (i = 0; (protp = protocols[i]) != NULL; ++i)
	if (protp->protocol < 0xC000
	    && protp->protocol != PPP_CCP && protp->protocol != PPP_ECP
	    && protp->enabled_flag && protp->open != NULL) {
	    (*protp->open)(0);
	    ++num_np_open;
	}

    if (num_np_open == 0)
	
	lcp_close(0, "No network protocols running");
}

void
auth_peer_fail(unit, protocol)
    int unit, protocol;
{
    status = EXIT_PEER_AUTH_FAILED;
    lcp_close(unit, "Authentication failed");
}

void
auth_peer_success(unit, protocol, prot_flavor, name, namelen)
    int unit, protocol, prot_flavor;
    char *name;
    int namelen;
{
    int bit;

    switch (protocol) {
    case PPP_CHAP:
	bit = CHAP_PEER;
	switch (prot_flavor) {
	case CHAP_MD5:
	    bit |= CHAP_MD5_PEER;
	    break;
#ifdef CHAPMS
	case CHAP_MICROSOFT:
	    bit |= CHAP_MS_PEER;
	    break;
	case CHAP_MICROSOFT_V2:
	    bit |= CHAP_MS2_PEER;
	    break;
#endif
	}
	break;
    case PPP_PAP:
	bit = PAP_PEER;
	break;
    case PPP_EAP:
	bit = EAP_PEER;
	break;
    default:
	warn("auth_peer_success: unknown protocol %x", protocol);
	return;
    }

    if (namelen > sizeof(peer_authname) - 1)
	namelen = sizeof(peer_authname) - 1;
    BCOPY(name, peer_authname, namelen);
    peer_authname[namelen] = 0;
    script_setenv("PEERNAME", peer_authname, 0);

    
    auth_done[unit] |= bit;

    if ((auth_pending[unit] &= ~bit) == 0)
        network_phase(unit);
}

void
auth_withpeer_fail(unit, protocol)
    int unit, protocol;
{
    if (passwd_from_file)
	BZERO(passwd, MAXSECRETLEN);
    status = EXIT_AUTH_TOPEER_FAILED;
    lcp_close(unit, "Failed to authenticate ourselves to peer");
}

void
auth_withpeer_success(unit, protocol, prot_flavor)
    int unit, protocol, prot_flavor;
{
    int bit;
    const char *prot = "";

    switch (protocol) {
    case PPP_CHAP:
	bit = CHAP_WITHPEER;
	prot = "CHAP";
	switch (prot_flavor) {
	case CHAP_MD5:
	    bit |= CHAP_MD5_WITHPEER;
	    break;
#ifdef CHAPMS
	case CHAP_MICROSOFT:
	    bit |= CHAP_MS_WITHPEER;
	    break;
	case CHAP_MICROSOFT_V2:
	    bit |= CHAP_MS2_WITHPEER;
	    break;
#endif
	}
	break;
    case PPP_PAP:
	if (passwd_from_file)
	    BZERO(passwd, MAXSECRETLEN);
	bit = PAP_WITHPEER;
	prot = "PAP";
	break;
    case PPP_EAP:
	bit = EAP_WITHPEER;
	prot = "EAP";
	break;
    default:
	warn("auth_withpeer_success: unknown protocol %x", protocol);
	bit = 0;
    }

    notice("%s authentication succeeded", prot);

    
    auth_done[unit] |= bit;

    if ((auth_pending[unit] &= ~bit) == 0)
	network_phase(unit);
}


void
np_up(unit, proto)
    int unit, proto;
{
    int tlim;

    if (num_np_up == 0) {
	status = EXIT_OK;
	unsuccess = 0;
	new_phase(PHASE_RUNNING);

	if (idle_time_hook != 0)
	    tlim = (*idle_time_hook)(NULL);
	else
	    tlim = idle_time_limit;
	if (tlim > 0)
	    TIMEOUT(check_idle, NULL, tlim);

	if (maxconnect > 0)
	    TIMEOUT(connect_time_expired, 0, maxconnect);

#ifdef MAXOCTETS
	if (maxoctets > 0)
	    TIMEOUT(check_maxoctets, NULL, maxoctets_timeout);
#endif

	if (updetach && !nodetach)
	    detach();
    }
    ++num_np_up;
}

void
np_down(unit, proto)
    int unit, proto;
{
    if (--num_np_up == 0) {
	UNTIMEOUT(check_idle, NULL);
	UNTIMEOUT(connect_time_expired, NULL);
#ifdef MAXOCTETS
	UNTIMEOUT(check_maxoctets, NULL);
#endif	
	new_phase(PHASE_NETWORK);
    }
}

void
np_finished(unit, proto)
    int unit, proto;
{
    if (--num_np_open <= 0) {
	
	lcp_close(0, "No network protocols running");
    }
}

#ifdef MAXOCTETS
static void
check_maxoctets(arg)
    void *arg;
{
    unsigned int used;

    update_link_stats(ifunit);
    link_stats_valid=0;
    
    switch(maxoctets_dir) {
	case PPP_OCTETS_DIRECTION_IN:
	    used = link_stats.bytes_in;
	    break;
	case PPP_OCTETS_DIRECTION_OUT:
	    used = link_stats.bytes_out;
	    break;
	case PPP_OCTETS_DIRECTION_MAXOVERAL:
	case PPP_OCTETS_DIRECTION_MAXSESSION:
	    used = (link_stats.bytes_in > link_stats.bytes_out) ? link_stats.bytes_in : link_stats.bytes_out;
	    break;
	default:
	    used = link_stats.bytes_in+link_stats.bytes_out;
	    break;
    }
    if (used > maxoctets) {
	notice("Traffic limit reached. Limit: %u Used: %u", maxoctets, used);
	status = EXIT_TRAFFIC_LIMIT;
	lcp_close(0, "Traffic limit");
	need_holdoff = 0;
    } else {
        TIMEOUT(check_maxoctets, NULL, maxoctets_timeout);
    }
}
#endif

static void
check_idle(arg)
    void *arg;
{
    struct ppp_idle idle;
    time_t itime;
    int tlim;

    if (!get_idle_time(0, &idle))
	return;
    if (idle_time_hook != 0) {
	tlim = idle_time_hook(&idle);
    } else {
	itime = MIN(idle.xmit_idle, idle.recv_idle);
	tlim = idle_time_limit - itime;
    }
    if (tlim <= 0) {
	
	notice("Terminating connection due to lack of activity.");
	status = EXIT_IDLE_TIMEOUT;
	lcp_close(0, "Link inactive");
	need_holdoff = 0;
    } else {
	TIMEOUT(check_idle, NULL, tlim);
    }
}

static void
connect_time_expired(arg)
    void *arg;
{
    info("Connect time expired");
    status = EXIT_CONNECT_TIME;
    lcp_close(0, "Connect time expired");	
}

void
auth_check_options()
{
    lcp_options *wo = &lcp_wantoptions[0];
    int can_auth;
    int lacks_ip;

    
    if (our_name[0] == 0 || usehostname)
	strlcpy(our_name, hostname, sizeof(our_name));
    if (user[0] == 0 && !explicit_user)
	strlcpy(user, our_name, sizeof(user));

    if (!auth_required && !allow_any_ip && have_route_to(0) && !privileged) {
	auth_required = 1;
	default_auth = 1;
    }

    
    if (wo->chap_mdtype)
	wo->neg_chap = 1;

    
    if (auth_required) {
	allow_any_ip = 0;
	if (!wo->neg_chap && !wo->neg_upap && !wo->neg_eap) {
	    wo->neg_chap = chap_mdtype_all != MDTYPE_NONE;
	    wo->chap_mdtype = chap_mdtype_all;
	    wo->neg_upap = 1;
	    wo->neg_eap = 1;
	}
    } else {
	wo->neg_chap = 0;
	wo->chap_mdtype = MDTYPE_NONE;
	wo->neg_upap = 0;
	wo->neg_eap = 0;
    }

    lacks_ip = 0;
    can_auth = wo->neg_upap && (uselogin || have_pap_secret(&lacks_ip));
    if (!can_auth && (wo->neg_chap || wo->neg_eap)) {
	can_auth = have_chap_secret((explicit_remote? remote_name: NULL),
				    our_name, 1, &lacks_ip);
    }
    if (!can_auth && wo->neg_eap) {
	can_auth = have_srp_secret((explicit_remote? remote_name: NULL),
				    our_name, 1, &lacks_ip);
    }

    if (auth_required && !can_auth && noauth_addrs == NULL) {
	if (default_auth) {
	    option_error(
"By default the remote system is required to authenticate itself");
	    option_error(
"(because this system has a default route to the internet)");
	} else if (explicit_remote)
	    option_error(
"The remote system (%s) is required to authenticate itself",
			 remote_name);
	else
	    option_error(
"The remote system is required to authenticate itself");
	option_error(
"but I couldn't find any suitable secret (password) for it to use to do so.");
	if (lacks_ip)
	    option_error(
"(None of the available passwords would let it use an IP address.)");

	exit(1);
    }

    if (!auth_number()) {
	warn("calling number %q is not authorized", remote_number);
	exit(EXIT_CNID_AUTH_FAILED);
    }
}

void
auth_reset(unit)
    int unit;
{
    lcp_options *go = &lcp_gotoptions[unit];
    lcp_options *ao = &lcp_allowoptions[unit];
    int hadchap;

    hadchap = -1;
    ao->neg_upap = !refuse_pap && (passwd[0] != 0 || get_pap_passwd(NULL));
    ao->neg_chap = (!refuse_chap || !refuse_mschap || !refuse_mschap_v2)
	&& (passwd[0] != 0 ||
	    (hadchap = have_chap_secret(user, (explicit_remote? remote_name:
					       NULL), 0, NULL)));
    ao->neg_eap = !refuse_eap && (
	passwd[0] != 0 ||
	(hadchap == 1 || (hadchap == -1 && have_chap_secret(user,
	    (explicit_remote? remote_name: NULL), 0, NULL))) ||
	have_srp_secret(user, (explicit_remote? remote_name: NULL), 0, NULL));

    hadchap = -1;
    if (go->neg_upap && !uselogin && !have_pap_secret(NULL))
	go->neg_upap = 0;
    if (go->neg_chap) {
	if (!(hadchap = have_chap_secret((explicit_remote? remote_name: NULL),
			      our_name, 1, NULL)))
	    go->neg_chap = 0;
    }
    if (go->neg_eap &&
	(hadchap == 0 || (hadchap == -1 &&
	    !have_chap_secret((explicit_remote? remote_name: NULL), our_name,
		1, NULL))) &&
	!have_srp_secret((explicit_remote? remote_name: NULL), our_name, 1,
	    NULL))
	go->neg_eap = 0;
}


int
check_passwd(unit, auser, userlen, apasswd, passwdlen, msg)
    int unit;
    char *auser;
    int userlen;
    char *apasswd;
    int passwdlen;
    char **msg;
{
#if defined(__ANDROID__)
    return UPAP_AUTHNAK;
#else
    int ret;
    char *filename;
    FILE *f;
    struct wordlist *addrs = NULL, *opts = NULL;
    char passwd[256], user[256];
    char secret[MAXWORDLEN];
    static int attempts = 0;

    slprintf(passwd, sizeof(passwd), "%.*v", passwdlen, apasswd);
    slprintf(user, sizeof(user), "%.*v", userlen, auser);
    *msg = "";

    if (pap_auth_hook) {
	ret = (*pap_auth_hook)(user, passwd, msg, &addrs, &opts);
	if (ret >= 0) {
	    if (ret)
		set_allowed_addrs(unit, addrs, opts);
	    else if (opts != 0)
		free_wordlist(opts);
	    if (addrs != 0)
		free_wordlist(addrs);
	    BZERO(passwd, sizeof(passwd));
	    return ret? UPAP_AUTHACK: UPAP_AUTHNAK;
	}
    }

    filename = _PATH_UPAPFILE;
    addrs = opts = NULL;
    ret = UPAP_AUTHNAK;
    f = fopen(filename, "r");
    if (f == NULL) {
	error("Can't open PAP password file %s: %m", filename);

    } else {
	check_access(f, filename);
	if (scan_authfile(f, user, our_name, secret, &addrs, &opts, filename, 0) < 0) {
	    warn("no PAP secret found for %s", user);
	} else {
	    int login_secret = strcmp(secret, "@login") == 0;
	    ret = UPAP_AUTHACK;
	    if (uselogin || login_secret) {
		
		if (session_full(user, passwd, devnam, msg) == 0) {
		    ret = UPAP_AUTHNAK;
		}
	    } else if (session_mgmt) {
		if (session_check(user, NULL, devnam, NULL) == 0) {
		    warn("Peer %q failed PAP Session verification", user);
		    ret = UPAP_AUTHNAK;
		}
	    }
	    if (secret[0] != 0 && !login_secret) {
		
		if (cryptpap || strcmp(passwd, secret) != 0) {
		    char *cbuf = crypt(passwd, secret);
		    if (!cbuf || strcmp(cbuf, secret) != 0)
			ret = UPAP_AUTHNAK;
		}
	    }
	}
	fclose(f);
    }

    if (ret == UPAP_AUTHNAK) {
        if (**msg == 0)
	    *msg = "Login incorrect";
	if (attempts++ >= 10) {
	    warn("%d LOGIN FAILURES ON %s, %s", attempts, devnam, user);
	    lcp_close(unit, "login failed");
	}
	if (attempts > 3)
	    sleep((u_int) (attempts - 3) * 5);
	if (opts != NULL)
	    free_wordlist(opts);

    } else {
	attempts = 0;			
	if (**msg == 0)
	    *msg = "Login ok";
	set_allowed_addrs(unit, addrs, opts);
    }

    if (addrs != NULL)
	free_wordlist(addrs);
    BZERO(passwd, sizeof(passwd));
    BZERO(secret, sizeof(secret));

    return ret;
#endif
}

static int
null_login(unit)
    int unit;
{
    char *filename;
    FILE *f;
    int i, ret;
    struct wordlist *addrs, *opts;
    char secret[MAXWORDLEN];

    ret = -1;
    if (null_auth_hook)
	ret = (*null_auth_hook)(&addrs, &opts);

    if (ret <= 0) {
	filename = _PATH_UPAPFILE;
	addrs = NULL;
	f = fopen(filename, "r");
	if (f == NULL)
	    return 0;
	check_access(f, filename);

	i = scan_authfile(f, "", our_name, secret, &addrs, &opts, filename, 0);
	ret = i >= 0 && secret[0] == 0;
	BZERO(secret, sizeof(secret));
	fclose(f);
    }

    if (ret)
	set_allowed_addrs(unit, addrs, opts);
    else if (opts != 0)
	free_wordlist(opts);
    if (addrs != 0)
	free_wordlist(addrs);

    return ret;
}


static int
get_pap_passwd(passwd)
    char *passwd;
{
    char *filename;
    FILE *f;
    int ret;
    char secret[MAXWORDLEN];

    if (pap_passwd_hook) {
	ret = (*pap_passwd_hook)(user, passwd);
	if (ret >= 0)
	    return ret;
    }

    filename = _PATH_UPAPFILE;
    f = fopen(filename, "r");
    if (f == NULL)
	return 0;
    check_access(f, filename);
    ret = scan_authfile(f, user,
			(remote_name[0]? remote_name: NULL),
			secret, NULL, NULL, filename, 0);
    fclose(f);
    if (ret < 0)
	return 0;
    if (passwd != NULL)
	strlcpy(passwd, secret, MAXSECRETLEN);
    BZERO(secret, sizeof(secret));
    return 1;
}


static int
have_pap_secret(lacks_ipp)
    int *lacks_ipp;
{
    FILE *f;
    int ret;
    char *filename;
    struct wordlist *addrs;

    
    if (pap_check_hook) {
	ret = (*pap_check_hook)();
	if (ret >= 0)
	    return ret;
    }

    filename = _PATH_UPAPFILE;
    f = fopen(filename, "r");
    if (f == NULL)
	return 0;

    ret = scan_authfile(f, (explicit_remote? remote_name: NULL), our_name,
			NULL, &addrs, NULL, filename, 0);
    fclose(f);
    if (ret >= 0 && !some_ip_ok(addrs)) {
	if (lacks_ipp != 0)
	    *lacks_ipp = 1;
	ret = -1;
    }
    if (addrs != 0)
	free_wordlist(addrs);

    return ret >= 0;
}


static int
have_chap_secret(client, server, need_ip, lacks_ipp)
    char *client;
    char *server;
    int need_ip;
    int *lacks_ipp;
{
    FILE *f;
    int ret;
    char *filename;
    struct wordlist *addrs;

    if (chap_check_hook) {
	ret = (*chap_check_hook)();
	if (ret >= 0) {
	    return ret;
	}
    }

    filename = _PATH_CHAPFILE;
    f = fopen(filename, "r");
    if (f == NULL)
	return 0;

    if (client != NULL && client[0] == 0)
	client = NULL;
    else if (server != NULL && server[0] == 0)
	server = NULL;

    ret = scan_authfile(f, client, server, NULL, &addrs, NULL, filename, 0);
    fclose(f);
    if (ret >= 0 && need_ip && !some_ip_ok(addrs)) {
	if (lacks_ipp != 0)
	    *lacks_ipp = 1;
	ret = -1;
    }
    if (addrs != 0)
	free_wordlist(addrs);

    return ret >= 0;
}


static int
have_srp_secret(client, server, need_ip, lacks_ipp)
    char *client;
    char *server;
    int need_ip;
    int *lacks_ipp;
{
    FILE *f;
    int ret;
    char *filename;
    struct wordlist *addrs;

    filename = _PATH_SRPFILE;
    f = fopen(filename, "r");
    if (f == NULL)
	return 0;

    if (client != NULL && client[0] == 0)
	client = NULL;
    else if (server != NULL && server[0] == 0)
	server = NULL;

    ret = scan_authfile(f, client, server, NULL, &addrs, NULL, filename, 0);
    fclose(f);
    if (ret >= 0 && need_ip && !some_ip_ok(addrs)) {
	if (lacks_ipp != 0)
	    *lacks_ipp = 1;
	ret = -1;
    }
    if (addrs != 0)
	free_wordlist(addrs);

    return ret >= 0;
}


int
get_secret(unit, client, server, secret, secret_len, am_server)
    int unit;
    char *client;
    char *server;
    char *secret;
    int *secret_len;
    int am_server;
{
    FILE *f;
    int ret, len;
    char *filename;
    struct wordlist *addrs, *opts;
    char secbuf[MAXWORDLEN];

    if (!am_server && passwd[0] != 0) {
	strlcpy(secbuf, passwd, sizeof(secbuf));
    } else if (!am_server && chap_passwd_hook) {
	if ( (*chap_passwd_hook)(client, secbuf) < 0) {
	    error("Unable to obtain CHAP password for %s on %s from plugin",
		  client, server);
	    return 0;
	}
    } else {
	filename = _PATH_CHAPFILE;
	addrs = NULL;
	secbuf[0] = 0;

	f = fopen(filename, "r");
	if (f == NULL) {
	    error("Can't open chap secret file %s: %m", filename);
	    return 0;
	}
	check_access(f, filename);

	ret = scan_authfile(f, client, server, secbuf, &addrs, &opts, filename, 0);
	fclose(f);
	if (ret < 0)
	    return 0;

	if (am_server)
	    set_allowed_addrs(unit, addrs, opts);
	else if (opts != 0)
	    free_wordlist(opts);
	if (addrs != 0)
	    free_wordlist(addrs);
    }

    len = strlen(secbuf);
    if (len > MAXSECRETLEN) {
	error("Secret for %s on %s is too long", client, server);
	len = MAXSECRETLEN;
    }
    BCOPY(secbuf, secret, len);
    BZERO(secbuf, sizeof(secbuf));
    *secret_len = len;

    return 1;
}


int
get_srp_secret(unit, client, server, secret, am_server)
    int unit;
    char *client;
    char *server;
    char *secret;
    int am_server;
{
    FILE *fp;
    int ret;
    char *filename;
    struct wordlist *addrs, *opts;

    if (!am_server && passwd[0] != '\0') {
	strlcpy(secret, passwd, MAXWORDLEN);
    } else {
	filename = _PATH_SRPFILE;
	addrs = NULL;

	fp = fopen(filename, "r");
	if (fp == NULL) {
	    error("Can't open srp secret file %s: %m", filename);
	    return 0;
	}
	check_access(fp, filename);

	secret[0] = '\0';
	ret = scan_authfile(fp, client, server, secret, &addrs, &opts,
	    filename, am_server);
	fclose(fp);
	if (ret < 0)
	    return 0;

	if (am_server)
	    set_allowed_addrs(unit, addrs, opts);
	else if (opts != NULL)
	    free_wordlist(opts);
	if (addrs != NULL)
	    free_wordlist(addrs);
    }

    return 1;
}

static void
set_allowed_addrs(unit, addrs, opts)
    int unit;
    struct wordlist *addrs;
    struct wordlist *opts;
{
    int n;
    struct wordlist *ap, **plink;
    struct permitted_ip *ip;
    char *ptr_word, *ptr_mask;
    struct hostent *hp;
    struct netent *np;
    u_int32_t a, mask, ah, offset;
    struct ipcp_options *wo = &ipcp_wantoptions[unit];
    u_int32_t suggested_ip = 0;

    if (addresses[unit] != NULL)
	free(addresses[unit]);
    addresses[unit] = NULL;
    if (extra_options != NULL)
	free_wordlist(extra_options);
    extra_options = opts;

    n = wordlist_count(addrs) + wordlist_count(noauth_addrs);
    if (n == 0)
	return;
    ip = (struct permitted_ip *) malloc((n + 1) * sizeof(struct permitted_ip));
    if (ip == 0)
	return;

    
    for (plink = &addrs; *plink != NULL; plink = &(*plink)->next)
	;
    *plink = noauth_addrs;

    n = 0;
    for (ap = addrs; ap != NULL; ap = ap->next) {
	
	ptr_word = ap->word;
	if (strcmp(ptr_word, "-") == 0)
	    break;
	if (strcmp(ptr_word, "*") == 0) {
	    ip[n].permit = 1;
	    ip[n].base = ip[n].mask = 0;
	    ++n;
	    break;
	}

	ip[n].permit = 1;
	if (*ptr_word == '!') {
	    ip[n].permit = 0;
	    ++ptr_word;
	}

	mask = ~ (u_int32_t) 0;
	offset = 0;
	ptr_mask = strchr (ptr_word, '/');
	if (ptr_mask != NULL) {
	    int bit_count;
	    char *endp;

	    bit_count = (int) strtol (ptr_mask+1, &endp, 10);
	    if (bit_count <= 0 || bit_count > 32) {
		warn("invalid address length %v in auth. address list",
		     ptr_mask+1);
		continue;
	    }
	    bit_count = 32 - bit_count;	
	    if (*endp == '+') {
		offset = ifunit + 1;
		++endp;
	    }
	    if (*endp != 0) {
		warn("invalid address length syntax: %v", ptr_mask+1);
		continue;
	    }
	    *ptr_mask = '\0';
	    mask <<= bit_count;
	}

	hp = gethostbyname(ptr_word);
	if (hp != NULL && hp->h_addrtype == AF_INET) {
	    a = *(u_int32_t *)hp->h_addr;
	} else {
	    np = getnetbyname (ptr_word);
	    if (np != NULL && np->n_addrtype == AF_INET) {
		a = htonl ((u_int32_t)np->n_net);
		if (ptr_mask == NULL) {
		    
		    ah = ntohl(a);
		    if (IN_CLASSA(ah))
			mask = IN_CLASSA_NET;
		    else if (IN_CLASSB(ah))
			mask = IN_CLASSB_NET;
		    else if (IN_CLASSC(ah))
			mask = IN_CLASSC_NET;
		}
	    } else {
		a = inet_addr (ptr_word);
	    }
	}

	if (ptr_mask != NULL)
	    *ptr_mask = '/';

	if (a == (u_int32_t)-1L) {
	    warn("unknown host %s in auth. address list", ap->word);
	    continue;
	}
	if (offset != 0) {
	    if (offset >= ~mask) {
		warn("interface unit %d too large for subnet %v",
		     ifunit, ptr_word);
		continue;
	    }
	    a = htonl((ntohl(a) & mask) + offset);
	    mask = ~(u_int32_t)0;
	}
	ip[n].mask = htonl(mask);
	ip[n].base = a & ip[n].mask;
	++n;
	if (~mask == 0 && suggested_ip == 0)
	    suggested_ip = a;
    }
    *plink = NULL;

    ip[n].permit = 0;		
    ip[n].base = 0;		
    ip[n].mask = 0;

    addresses[unit] = ip;

    if (suggested_ip != 0
	&& (wo->hisaddr == 0 || !auth_ip_addr(unit, wo->hisaddr))) {
	wo->hisaddr = suggested_ip;
	if (n > 1)
	    wo->accept_remote = 1;
    }
}

int
auth_ip_addr(unit, addr)
    int unit;
    u_int32_t addr;
{
    int ok;

    
    if (bad_ip_adrs(addr))
	return 0;

    if (allowed_address_hook) {
	ok = allowed_address_hook(addr);
	if (ok >= 0) return ok;
    }

    if (addresses[unit] != NULL) {
	ok = ip_addr_check(addr, addresses[unit]);
	if (ok >= 0)
	    return ok;
    }

    if (auth_required)
	return 0;		
    return allow_any_ip || privileged || !have_route_to(addr);
}

static int
ip_addr_check(addr, addrs)
    u_int32_t addr;
    struct permitted_ip *addrs;
{
    for (; ; ++addrs)
	if ((addr & addrs->mask) == addrs->base)
	    return addrs->permit;
}

int
bad_ip_adrs(addr)
    u_int32_t addr;
{
    addr = ntohl(addr);
    return (addr >> IN_CLASSA_NSHIFT) == IN_LOOPBACKNET
	|| IN_MULTICAST(addr) || IN_BADCLASS(addr);
}

static int
some_ip_ok(addrs)
    struct wordlist *addrs;
{
    for (; addrs != 0; addrs = addrs->next) {
	if (addrs->word[0] == '-')
	    break;
	if (addrs->word[0] != '!')
	    return 1;		
    }
    return 0;
}

int
auth_number()
{
    struct wordlist *wp = permitted_numbers;
    int l;

    
    if (!wp)
	return 1;

    
    while (wp) {
	
	l = strlen(wp->word);
	if ((wp->word)[l - 1] == '*')
	    l--;
	if (!strncasecmp(wp->word, remote_number, l))
	    return 1;
	wp = wp->next;
    }

    return 0;
}

static void
check_access(f, filename)
    FILE *f;
    char *filename;
{
    struct stat sbuf;

    if (fstat(fileno(f), &sbuf) < 0) {
	warn("cannot stat secret file %s: %m", filename);
    } else if ((sbuf.st_mode & (S_IRWXG | S_IRWXO)) != 0) {
	warn("Warning - secret file %s has world and/or group access",
	     filename);
    }
}


static int
scan_authfile(f, client, server, secret, addrs, opts, filename, flags)
    FILE *f;
    char *client;
    char *server;
    char *secret;
    struct wordlist **addrs;
    struct wordlist **opts;
    char *filename;
    int flags;
{
    int newline, xxx;
    int got_flag, best_flag;
    FILE *sf;
    struct wordlist *ap, *addr_list, *alist, **app;
    char word[MAXWORDLEN];
    char atfile[MAXWORDLEN];
    char lsecret[MAXWORDLEN];
    char *cp;

    if (addrs != NULL)
	*addrs = NULL;
    if (opts != NULL)
	*opts = NULL;
    addr_list = NULL;
    if (!getword(f, word, &newline, filename))
	return -1;		
    newline = 1;
    best_flag = -1;
    for (;;) {
	while (!newline && getword(f, word, &newline, filename))
	    ;
	if (!newline)
	    break;		

	got_flag = 0;
	if (client != NULL && strcmp(word, client) != 0 && !ISWILD(word)) {
	    newline = 0;
	    continue;
	}
	if (!ISWILD(word))
	    got_flag = NONWILD_CLIENT;

	if (!getword(f, word, &newline, filename))
	    break;
	if (newline)
	    continue;
	if (!ISWILD(word)) {
	    if (server != NULL && strcmp(word, server) != 0)
		continue;
	    got_flag |= NONWILD_SERVER;
	}

	if (got_flag <= best_flag)
	    continue;

	if (!getword(f, word, &newline, filename))
	    break;
	if (newline)
	    continue;

	if (flags && ((cp = strchr(word, ':')) == NULL ||
	    strchr(cp + 1, ':') == NULL))
	    continue;

	if (secret != NULL) {
	    if (word[0] == '@' && word[1] == '/') {
		strlcpy(atfile, word+1, sizeof(atfile));
		if ((sf = fopen(atfile, "r")) == NULL) {
		    warn("can't open indirect secret file %s", atfile);
		    continue;
		}
		check_access(sf, atfile);
		if (!getword(sf, word, &xxx, atfile)) {
		    warn("no secret in indirect secret file %s", atfile);
		    fclose(sf);
		    continue;
		}
		fclose(sf);
	    }
	    strlcpy(lsecret, word, sizeof(lsecret));
	}

	app = &alist;
	for (;;) {
	    if (!getword(f, word, &newline, filename) || newline)
		break;
	    ap = (struct wordlist *)
		    malloc(sizeof(struct wordlist) + strlen(word) + 1);
	    if (ap == NULL)
		novm("authorized addresses");
	    ap->word = (char *) (ap + 1);
	    strcpy(ap->word, word);
	    *app = ap;
	    app = &ap->next;
	}
	*app = NULL;

	best_flag = got_flag;
	if (addr_list)
	    free_wordlist(addr_list);
	addr_list = alist;
	if (secret != NULL)
	    strlcpy(secret, lsecret, MAXWORDLEN);

	if (!newline)
	    break;
    }

    
    for (app = &addr_list; (ap = *app) != NULL; app = &ap->next)
	if (strcmp(ap->word, "--") == 0)
	    break;
    
    if (ap != NULL) {
	ap = ap->next;		
	free(*app);			
	*app = NULL;		
    }
    if (opts != NULL)
	*opts = ap;
    else if (ap != NULL)
	free_wordlist(ap);
    if (addrs != NULL)
	*addrs = addr_list;
    else if (addr_list != NULL)
	free_wordlist(addr_list);

    return best_flag;
}

static int
wordlist_count(wp)
    struct wordlist *wp;
{
    int n;

    for (n = 0; wp != NULL; wp = wp->next)
	++n;
    return n;
}

static void
free_wordlist(wp)
    struct wordlist *wp;
{
    struct wordlist *next;

    while (wp != NULL) {
	next = wp->next;
	free(wp);
	wp = next;
    }
}

static void
auth_script_done(arg)
    void *arg;
{
    auth_script_pid = 0;
    switch (auth_script_state) {
    case s_up:
	if (auth_state == s_down) {
	    auth_script_state = s_down;
	    auth_script(_PATH_AUTHDOWN);
	}
	break;
    case s_down:
	if (auth_state == s_up) {
	    auth_script_state = s_up;
	    auth_script(_PATH_AUTHUP);
	}
	break;
    }
}

static void
auth_script(script)
    char *script;
{
    char strspeed[32];
    struct passwd *pw;
    char struid[32];
    char *user_name;
    char *argv[8];

    if ((pw = getpwuid(getuid())) != NULL && pw->pw_name != NULL)
	user_name = pw->pw_name;
    else {
	slprintf(struid, sizeof(struid), "%d", getuid());
	user_name = struid;
    }
    slprintf(strspeed, sizeof(strspeed), "%d", baud_rate);

    argv[0] = script;
    argv[1] = ifname;
    argv[2] = peer_authname;
    argv[3] = user_name;
    argv[4] = devnam;
    argv[5] = strspeed;
    argv[6] = NULL;

    auth_script_pid = run_program(script, argv, 0, auth_script_done, NULL, 0);
}
