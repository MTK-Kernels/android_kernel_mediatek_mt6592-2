/*
 * lcp.c - PPP Link Control Protocol.
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

#define RCSID	"$Id: lcp.c,v 1.76 2006/05/22 00:04:07 paulus Exp $"


#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "pppd.h"
#include "fsm.h"
#include "lcp.h"
#include "chap-new.h"
#include "magic.h"

static const char rcsid[] = RCSID;

#define DELAYED_UP	0x100

static void lcp_delayed_up __P((void *));

int	lcp_echo_interval = 0; 	
int	lcp_echo_fails = 0;	
bool	lax_recv = 0;		
bool	noendpoint = 0;		

static int noopt __P((char **));

#ifdef HAVE_MULTILINK
static int setendpoint __P((char **));
static void printendpoint __P((option_t *, void (*)(void *, char *, ...),
			       void *));
#endif 

static option_t lcp_option_list[] = {
    
    { "-all", o_special_noarg, (void *)noopt,
      "Don't request/allow any LCP options" },

    { "noaccomp", o_bool, &lcp_wantoptions[0].neg_accompression,
      "Disable address/control compression",
      OPT_A2CLR, &lcp_allowoptions[0].neg_accompression },
    { "-ac", o_bool, &lcp_wantoptions[0].neg_accompression,
      "Disable address/control compression",
      OPT_ALIAS | OPT_A2CLR, &lcp_allowoptions[0].neg_accompression },

    { "asyncmap", o_uint32, &lcp_wantoptions[0].asyncmap,
      "Set asyncmap (for received packets)",
      OPT_OR, &lcp_wantoptions[0].neg_asyncmap },
    { "-as", o_uint32, &lcp_wantoptions[0].asyncmap,
      "Set asyncmap (for received packets)",
      OPT_ALIAS | OPT_OR, &lcp_wantoptions[0].neg_asyncmap },
    { "default-asyncmap", o_uint32, &lcp_wantoptions[0].asyncmap,
      "Disable asyncmap negotiation",
      OPT_OR | OPT_NOARG | OPT_VAL(~0U) | OPT_A2CLR,
      &lcp_allowoptions[0].neg_asyncmap },
    { "-am", o_uint32, &lcp_wantoptions[0].asyncmap,
      "Disable asyncmap negotiation",
      OPT_ALIAS | OPT_OR | OPT_NOARG | OPT_VAL(~0U) | OPT_A2CLR,
      &lcp_allowoptions[0].neg_asyncmap },

    { "nomagic", o_bool, &lcp_wantoptions[0].neg_magicnumber,
      "Disable magic number negotiation (looped-back line detection)",
      OPT_A2CLR, &lcp_allowoptions[0].neg_magicnumber },
    { "-mn", o_bool, &lcp_wantoptions[0].neg_magicnumber,
      "Disable magic number negotiation (looped-back line detection)",
      OPT_ALIAS | OPT_A2CLR, &lcp_allowoptions[0].neg_magicnumber },

    { "mru", o_int, &lcp_wantoptions[0].mru,
      "Set MRU (maximum received packet size) for negotiation",
      OPT_PRIO, &lcp_wantoptions[0].neg_mru },
    { "default-mru", o_bool, &lcp_wantoptions[0].neg_mru,
      "Disable MRU negotiation (use default 1500)",
      OPT_PRIOSUB | OPT_A2CLR, &lcp_allowoptions[0].neg_mru },
    { "-mru", o_bool, &lcp_wantoptions[0].neg_mru,
      "Disable MRU negotiation (use default 1500)",
      OPT_ALIAS | OPT_PRIOSUB | OPT_A2CLR, &lcp_allowoptions[0].neg_mru },

    { "mtu", o_int, &lcp_allowoptions[0].mru,
      "Set our MTU", OPT_LIMITS, NULL, MAXMRU, MINMRU },

    { "nopcomp", o_bool, &lcp_wantoptions[0].neg_pcompression,
      "Disable protocol field compression",
      OPT_A2CLR, &lcp_allowoptions[0].neg_pcompression },
    { "-pc", o_bool, &lcp_wantoptions[0].neg_pcompression,
      "Disable protocol field compression",
      OPT_ALIAS | OPT_A2CLR, &lcp_allowoptions[0].neg_pcompression },

    { "passive", o_bool, &lcp_wantoptions[0].passive,
      "Set passive mode", 1 },
    { "-p", o_bool, &lcp_wantoptions[0].passive,
      "Set passive mode", OPT_ALIAS | 1 },

    { "silent", o_bool, &lcp_wantoptions[0].silent,
      "Set silent mode", 1 },

    { "lcp-echo-failure", o_int, &lcp_echo_fails,
      "Set number of consecutive echo failures to indicate link failure",
      OPT_PRIO },
    { "lcp-echo-interval", o_int, &lcp_echo_interval,
      "Set time in seconds between LCP echo requests", OPT_PRIO },
    { "lcp-restart", o_int, &lcp_fsm[0].timeouttime,
      "Set time in seconds between LCP retransmissions", OPT_PRIO },
    { "lcp-max-terminate", o_int, &lcp_fsm[0].maxtermtransmits,
      "Set maximum number of LCP terminate-request transmissions", OPT_PRIO },
    { "lcp-max-configure", o_int, &lcp_fsm[0].maxconfreqtransmits,
      "Set maximum number of LCP configure-request transmissions", OPT_PRIO },
    { "lcp-max-failure", o_int, &lcp_fsm[0].maxnakloops,
      "Set limit on number of LCP configure-naks", OPT_PRIO },

    { "receive-all", o_bool, &lax_recv,
      "Accept all received control characters", 1 },

#ifdef HAVE_MULTILINK
    { "mrru", o_int, &lcp_wantoptions[0].mrru,
      "Maximum received packet size for multilink bundle",
      OPT_PRIO, &lcp_wantoptions[0].neg_mrru },

    { "mpshortseq", o_bool, &lcp_wantoptions[0].neg_ssnhf,
      "Use short sequence numbers in multilink headers",
      OPT_PRIO | 1, &lcp_allowoptions[0].neg_ssnhf },
    { "nompshortseq", o_bool, &lcp_wantoptions[0].neg_ssnhf,
      "Don't use short sequence numbers in multilink headers",
      OPT_PRIOSUB | OPT_A2CLR, &lcp_allowoptions[0].neg_ssnhf },

    { "endpoint", o_special, (void *) setendpoint,
      "Endpoint discriminator for multilink",
      OPT_PRIO | OPT_A2PRINTER, (void *) printendpoint },
#endif 

    { "noendpoint", o_bool, &noendpoint,
      "Don't send or accept multilink endpoint discriminator", 1 },

    {NULL}
};

fsm lcp_fsm[NUM_PPP];			
lcp_options lcp_wantoptions[NUM_PPP];	
lcp_options lcp_gotoptions[NUM_PPP];	
lcp_options lcp_allowoptions[NUM_PPP];	
lcp_options lcp_hisoptions[NUM_PPP];	

static int lcp_echos_pending = 0;	
static int lcp_echo_number   = 0;	
static int lcp_echo_timer_running = 0;  

static u_char nak_buffer[PPP_MRU];	

static void lcp_resetci __P((fsm *));	
static int  lcp_cilen __P((fsm *));		
static void lcp_addci __P((fsm *, u_char *, int *)); 
static int  lcp_ackci __P((fsm *, u_char *, int)); 
static int  lcp_nakci __P((fsm *, u_char *, int, int)); 
static int  lcp_rejci __P((fsm *, u_char *, int)); 
static int  lcp_reqci __P((fsm *, u_char *, int *, int)); 
static void lcp_up __P((fsm *));		
static void lcp_down __P((fsm *));		
static void lcp_starting __P((fsm *));	
static void lcp_finished __P((fsm *));	
static int  lcp_extcode __P((fsm *, int, int, u_char *, int));
static void lcp_rprotrej __P((fsm *, u_char *, int));


static void lcp_echo_lowerup __P((int));
static void lcp_echo_lowerdown __P((int));
static void LcpEchoTimeout __P((void *));
static void lcp_received_echo_reply __P((fsm *, int, u_char *, int));
static void LcpSendEchoRequest __P((fsm *));
static void LcpLinkFailure __P((fsm *));
static void LcpEchoCheck __P((fsm *));

static fsm_callbacks lcp_callbacks = {	
    lcp_resetci,		
    lcp_cilen,			
    lcp_addci,			
    lcp_ackci,			
    lcp_nakci,			
    lcp_rejci,			
    lcp_reqci,			
    lcp_up,			
    lcp_down,			
    lcp_starting,		
    lcp_finished,		
    NULL,			
    NULL,			
    lcp_extcode,		
    "LCP"			
};


static void lcp_init __P((int));
static void lcp_input __P((int, u_char *, int));
static void lcp_protrej __P((int));
static int  lcp_printpkt __P((u_char *, int,
			      void (*) __P((void *, char *, ...)), void *));

struct protent lcp_protent = {
    PPP_LCP,
    lcp_init,
    lcp_input,
    lcp_protrej,
    lcp_lowerup,
    lcp_lowerdown,
    lcp_open,
    lcp_close,
    lcp_printpkt,
    NULL,
    1,
    "LCP",
    NULL,
    lcp_option_list,
    NULL,
    NULL,
    NULL
};

int lcp_loopbackfail = DEFLOOPBACKFAIL;

#define CILEN_VOID	2
#define CILEN_CHAR	3
#define CILEN_SHORT	4	
#define CILEN_CHAP	5	
#define CILEN_LONG	6	
#define CILEN_LQR	8	
#define CILEN_CBCP	3

#define CODENAME(x)	((x) == CONFACK ? "ACK" : \
			 (x) == CONFNAK ? "NAK" : "REJ")

static int
noopt(argv)
    char **argv;
{
    BZERO((char *) &lcp_wantoptions[0], sizeof (struct lcp_options));
    BZERO((char *) &lcp_allowoptions[0], sizeof (struct lcp_options));

    return (1);
}

#ifdef HAVE_MULTILINK
static int
setendpoint(argv)
    char **argv;
{
    if (str_to_epdisc(&lcp_wantoptions[0].endpoint, *argv)) {
	lcp_wantoptions[0].neg_endpoint = 1;
	return 1;
    }
    option_error("Can't parse '%s' as an endpoint discriminator", *argv);
    return 0;
}

static void
printendpoint(opt, printer, arg)
    option_t *opt;
    void (*printer) __P((void *, char *, ...));
    void *arg;
{
	printer(arg, "%s", epdisc_to_str(&lcp_wantoptions[0].endpoint));
}
#endif 

static void
lcp_init(unit)
    int unit;
{
    fsm *f = &lcp_fsm[unit];
    lcp_options *wo = &lcp_wantoptions[unit];
    lcp_options *ao = &lcp_allowoptions[unit];

    f->unit = unit;
    f->protocol = PPP_LCP;
    f->callbacks = &lcp_callbacks;

    fsm_init(f);

    BZERO(wo, sizeof(*wo));
    wo->neg_mru = 1;
    wo->mru = DEFMRU;
    wo->neg_asyncmap = 1;
    wo->neg_magicnumber = 1;
    wo->neg_pcompression = 1;
    wo->neg_accompression = 1;

    BZERO(ao, sizeof(*ao));
    ao->neg_mru = 1;
    ao->mru = MAXMRU;
    ao->neg_asyncmap = 1;
    ao->neg_chap = 1;
    ao->chap_mdtype = chap_mdtype_all;
    ao->neg_upap = 1;
    ao->neg_eap = 1;
    ao->neg_magicnumber = 1;
    ao->neg_pcompression = 1;
    ao->neg_accompression = 1;
    ao->neg_endpoint = 1;
}


void
lcp_open(unit)
    int unit;
{
    fsm *f = &lcp_fsm[unit];
    lcp_options *wo = &lcp_wantoptions[unit];

    f->flags &= ~(OPT_PASSIVE | OPT_SILENT);
    if (wo->passive)
	f->flags |= OPT_PASSIVE;
    if (wo->silent)
	f->flags |= OPT_SILENT;
    fsm_open(f);
}


void
lcp_close(unit, reason)
    int unit;
    char *reason;
{
    fsm *f = &lcp_fsm[unit];
    int oldstate;

    if (phase != PHASE_DEAD && phase != PHASE_MASTER)
	new_phase(PHASE_TERMINATE);

    if (f->flags & DELAYED_UP) {
	untimeout(lcp_delayed_up, f);
	f->state = STOPPED;
    }
    oldstate = f->state;

    fsm_close(f, reason);
    if (oldstate == STOPPED && f->flags & (OPT_PASSIVE|OPT_SILENT|DELAYED_UP)) {
	f->flags &= ~DELAYED_UP;
	lcp_finished(f);
    }
}


void
lcp_lowerup(unit)
    int unit;
{
    lcp_options *wo = &lcp_wantoptions[unit];
    fsm *f = &lcp_fsm[unit];

    if (ppp_send_config(unit, PPP_MRU, 0xffffffff, 0, 0) < 0
	|| ppp_recv_config(unit, PPP_MRU, (lax_recv? 0: 0xffffffff),
			   wo->neg_pcompression, wo->neg_accompression) < 0)
	    return;
    peer_mru[unit] = PPP_MRU;

    if (listen_time != 0) {
	f->flags |= DELAYED_UP;
	timeout(lcp_delayed_up, f, 0, listen_time * 1000);
    } else
	fsm_lowerup(f);
}


void
lcp_lowerdown(unit)
    int unit;
{
    fsm *f = &lcp_fsm[unit];

    if (f->flags & DELAYED_UP) {
	f->flags &= ~DELAYED_UP;
	untimeout(lcp_delayed_up, f);
    } else
	fsm_lowerdown(&lcp_fsm[unit]);
}


static void
lcp_delayed_up(arg)
    void *arg;
{
    fsm *f = arg;

    if (f->flags & DELAYED_UP) {
	f->flags &= ~DELAYED_UP;
	fsm_lowerup(f);
    }
}


static void
lcp_input(unit, p, len)
    int unit;
    u_char *p;
    int len;
{
    fsm *f = &lcp_fsm[unit];

    if (f->flags & DELAYED_UP) {
	f->flags &= ~DELAYED_UP;
	untimeout(lcp_delayed_up, f);
	fsm_lowerup(f);
    }
    fsm_input(f, p, len);
}

static int
lcp_extcode(f, code, id, inp, len)
    fsm *f;
    int code, id;
    u_char *inp;
    int len;
{
    u_char *magp;

    switch( code ){
    case PROTREJ:
	lcp_rprotrej(f, inp, len);
	break;
    
    case ECHOREQ:
	if (f->state != OPENED)
	    break;
	magp = inp;
	PUTLONG(lcp_gotoptions[f->unit].magicnumber, magp);
	fsm_sdata(f, ECHOREP, id, inp, len);
	break;
    
    case ECHOREP:
	lcp_received_echo_reply(f, id, inp, len);
	break;

    case DISCREQ:
    case IDENTIF:
    case TIMEREM:
	break;

    default:
	return 0;
    }
    return 1;
}

    
static void
lcp_rprotrej(f, inp, len)
    fsm *f;
    u_char *inp;
    int len;
{
    int i;
    struct protent *protp;
    u_short prot;
    const char *pname;

    if (len < 2) {
	LCPDEBUG(("lcp_rprotrej: Rcvd short Protocol-Reject packet!"));
	return;
    }

    GETSHORT(prot, inp);

    if( f->state != OPENED ){
	LCPDEBUG(("Protocol-Reject discarded: LCP in state %d", f->state));
	return;
    }

    pname = protocol_name(prot);

    for (i = 0; (protp = protocols[i]) != NULL; ++i)
	if (protp->protocol == prot && protp->enabled_flag) {
	    if (pname == NULL)
		dbglog("Protocol-Reject for 0x%x received", prot);
	    else
		dbglog("Protocol-Reject for '%s' (0x%x) received", pname,
		       prot);
	    (*protp->protrej)(f->unit);
	    return;
	}

    if (pname == NULL)
	warn("Protocol-Reject for unsupported protocol 0x%x", prot);
    else
	warn("Protocol-Reject for unsupported protocol '%s' (0x%x)", pname,
	     prot);
}


static void
lcp_protrej(unit)
    int unit;
{
    error("Received Protocol-Reject for LCP!");
    fsm_protreject(&lcp_fsm[unit]);
}


void
lcp_sprotrej(unit, p, len)
    int unit;
    u_char *p;
    int len;
{
    p += 2;
    len -= 2;

    fsm_sdata(&lcp_fsm[unit], PROTREJ, ++lcp_fsm[unit].id,
	      p, len);
}


static void
lcp_resetci(f)
    fsm *f;
{
    lcp_options *wo = &lcp_wantoptions[f->unit];
    lcp_options *go = &lcp_gotoptions[f->unit];
    lcp_options *ao = &lcp_allowoptions[f->unit];

    wo->magicnumber = magic();
    wo->numloops = 0;
    *go = *wo;
    if (!multilink) {
	go->neg_mrru = 0;
	go->neg_ssnhf = 0;
	go->neg_endpoint = 0;
    }
    if (noendpoint)
	ao->neg_endpoint = 0;
    peer_mru[f->unit] = PPP_MRU;
    auth_reset(f->unit);
}


static int
lcp_cilen(f)
    fsm *f;
{
    lcp_options *go = &lcp_gotoptions[f->unit];

#define LENCIVOID(neg)	((neg) ? CILEN_VOID : 0)
#define LENCICHAP(neg)	((neg) ? CILEN_CHAP : 0)
#define LENCISHORT(neg)	((neg) ? CILEN_SHORT : 0)
#define LENCILONG(neg)	((neg) ? CILEN_LONG : 0)
#define LENCILQR(neg)	((neg) ? CILEN_LQR: 0)
#define LENCICBCP(neg)	((neg) ? CILEN_CBCP: 0)
    return (LENCISHORT(go->neg_mru && go->mru != DEFMRU) +
	    LENCILONG(go->neg_asyncmap && go->asyncmap != 0xFFFFFFFF) +
	    LENCISHORT(go->neg_eap) +
	    LENCICHAP(!go->neg_eap && go->neg_chap) +
	    LENCISHORT(!go->neg_eap && !go->neg_chap && go->neg_upap) +
	    LENCILQR(go->neg_lqr) +
	    LENCICBCP(go->neg_cbcp) +
	    LENCILONG(go->neg_magicnumber) +
	    LENCIVOID(go->neg_pcompression) +
	    LENCIVOID(go->neg_accompression) +
	    LENCISHORT(go->neg_mrru) +
	    LENCIVOID(go->neg_ssnhf) +
	    (go->neg_endpoint? CILEN_CHAR + go->endpoint.length: 0));
}


static void
lcp_addci(f, ucp, lenp)
    fsm *f;
    u_char *ucp;
    int *lenp;
{
    lcp_options *go = &lcp_gotoptions[f->unit];
    u_char *start_ucp = ucp;

#define ADDCIVOID(opt, neg) \
    if (neg) { \
	PUTCHAR(opt, ucp); \
	PUTCHAR(CILEN_VOID, ucp); \
    }
#define ADDCISHORT(opt, neg, val) \
    if (neg) { \
	PUTCHAR(opt, ucp); \
	PUTCHAR(CILEN_SHORT, ucp); \
	PUTSHORT(val, ucp); \
    }
#define ADDCICHAP(opt, neg, val) \
    if (neg) { \
	PUTCHAR((opt), ucp); \
	PUTCHAR(CILEN_CHAP, ucp); \
	PUTSHORT(PPP_CHAP, ucp); \
	PUTCHAR((CHAP_DIGEST(val)), ucp); \
    }
#define ADDCILONG(opt, neg, val) \
    if (neg) { \
	PUTCHAR(opt, ucp); \
	PUTCHAR(CILEN_LONG, ucp); \
	PUTLONG(val, ucp); \
    }
#define ADDCILQR(opt, neg, val) \
    if (neg) { \
	PUTCHAR(opt, ucp); \
	PUTCHAR(CILEN_LQR, ucp); \
	PUTSHORT(PPP_LQR, ucp); \
	PUTLONG(val, ucp); \
    }
#define ADDCICHAR(opt, neg, val) \
    if (neg) { \
	PUTCHAR(opt, ucp); \
	PUTCHAR(CILEN_CHAR, ucp); \
	PUTCHAR(val, ucp); \
    }
#define ADDCIENDP(opt, neg, class, val, len) \
    if (neg) { \
	int i; \
	PUTCHAR(opt, ucp); \
	PUTCHAR(CILEN_CHAR + len, ucp); \
	PUTCHAR(class, ucp); \
	for (i = 0; i < len; ++i) \
	    PUTCHAR(val[i], ucp); \
    }

    ADDCISHORT(CI_MRU, go->neg_mru && go->mru != DEFMRU, go->mru);
    ADDCILONG(CI_ASYNCMAP, go->neg_asyncmap && go->asyncmap != 0xFFFFFFFF,
	      go->asyncmap);
    ADDCISHORT(CI_AUTHTYPE, go->neg_eap, PPP_EAP);
    ADDCICHAP(CI_AUTHTYPE, !go->neg_eap && go->neg_chap, go->chap_mdtype);
    ADDCISHORT(CI_AUTHTYPE, !go->neg_eap && !go->neg_chap && go->neg_upap,
	       PPP_PAP);
    ADDCILQR(CI_QUALITY, go->neg_lqr, go->lqr_period);
    ADDCICHAR(CI_CALLBACK, go->neg_cbcp, CBCP_OPT);
    ADDCILONG(CI_MAGICNUMBER, go->neg_magicnumber, go->magicnumber);
    ADDCIVOID(CI_PCOMPRESSION, go->neg_pcompression);
    ADDCIVOID(CI_ACCOMPRESSION, go->neg_accompression);
    ADDCISHORT(CI_MRRU, go->neg_mrru, go->mrru);
    ADDCIVOID(CI_SSNHF, go->neg_ssnhf);
    ADDCIENDP(CI_EPDISC, go->neg_endpoint, go->endpoint.class,
	      go->endpoint.value, go->endpoint.length);

    if (ucp - start_ucp != *lenp) {
	
	error("Bug in lcp_addci: wrong length");
    }
}


static int
lcp_ackci(f, p, len)
    fsm *f;
    u_char *p;
    int len;
{
    lcp_options *go = &lcp_gotoptions[f->unit];
    u_char cilen, citype, cichar;
    u_short cishort;
    u_int32_t cilong;

#define ACKCIVOID(opt, neg) \
    if (neg) { \
	if ((len -= CILEN_VOID) < 0) \
	    goto bad; \
	GETCHAR(citype, p); \
	GETCHAR(cilen, p); \
	if (cilen != CILEN_VOID || \
	    citype != opt) \
	    goto bad; \
    }
#define ACKCISHORT(opt, neg, val) \
    if (neg) { \
	if ((len -= CILEN_SHORT) < 0) \
	    goto bad; \
	GETCHAR(citype, p); \
	GETCHAR(cilen, p); \
	if (cilen != CILEN_SHORT || \
	    citype != opt) \
	    goto bad; \
	GETSHORT(cishort, p); \
	if (cishort != val) \
	    goto bad; \
    }
#define ACKCICHAR(opt, neg, val) \
    if (neg) { \
	if ((len -= CILEN_CHAR) < 0) \
	    goto bad; \
	GETCHAR(citype, p); \
	GETCHAR(cilen, p); \
	if (cilen != CILEN_CHAR || \
	    citype != opt) \
	    goto bad; \
	GETCHAR(cichar, p); \
	if (cichar != val) \
	    goto bad; \
    }
#define ACKCICHAP(opt, neg, val) \
    if (neg) { \
	if ((len -= CILEN_CHAP) < 0) \
	    goto bad; \
	GETCHAR(citype, p); \
	GETCHAR(cilen, p); \
	if (cilen != CILEN_CHAP || \
	    citype != (opt)) \
	    goto bad; \
	GETSHORT(cishort, p); \
	if (cishort != PPP_CHAP) \
	    goto bad; \
	GETCHAR(cichar, p); \
	if (cichar != (CHAP_DIGEST(val))) \
	  goto bad; \
    }
#define ACKCILONG(opt, neg, val) \
    if (neg) { \
	if ((len -= CILEN_LONG) < 0) \
	    goto bad; \
	GETCHAR(citype, p); \
	GETCHAR(cilen, p); \
	if (cilen != CILEN_LONG || \
	    citype != opt) \
	    goto bad; \
	GETLONG(cilong, p); \
	if (cilong != val) \
	    goto bad; \
    }
#define ACKCILQR(opt, neg, val) \
    if (neg) { \
	if ((len -= CILEN_LQR) < 0) \
	    goto bad; \
	GETCHAR(citype, p); \
	GETCHAR(cilen, p); \
	if (cilen != CILEN_LQR || \
	    citype != opt) \
	    goto bad; \
	GETSHORT(cishort, p); \
	if (cishort != PPP_LQR) \
	    goto bad; \
	GETLONG(cilong, p); \
	if (cilong != val) \
	  goto bad; \
    }
#define ACKCIENDP(opt, neg, class, val, vlen) \
    if (neg) { \
	int i; \
	if ((len -= CILEN_CHAR + vlen) < 0) \
	    goto bad; \
	GETCHAR(citype, p); \
	GETCHAR(cilen, p); \
	if (cilen != CILEN_CHAR + vlen || \
	    citype != opt) \
	    goto bad; \
	GETCHAR(cichar, p); \
	if (cichar != class) \
	    goto bad; \
	for (i = 0; i < vlen; ++i) { \
	    GETCHAR(cichar, p); \
	    if (cichar != val[i]) \
		goto bad; \
	} \
    }

    ACKCISHORT(CI_MRU, go->neg_mru && go->mru != DEFMRU, go->mru);
    ACKCILONG(CI_ASYNCMAP, go->neg_asyncmap && go->asyncmap != 0xFFFFFFFF,
	      go->asyncmap);
    ACKCISHORT(CI_AUTHTYPE, go->neg_eap, PPP_EAP);
    ACKCICHAP(CI_AUTHTYPE, !go->neg_eap && go->neg_chap, go->chap_mdtype);
    ACKCISHORT(CI_AUTHTYPE, !go->neg_eap && !go->neg_chap && go->neg_upap,
	       PPP_PAP);
    ACKCILQR(CI_QUALITY, go->neg_lqr, go->lqr_period);
    ACKCICHAR(CI_CALLBACK, go->neg_cbcp, CBCP_OPT);
    ACKCILONG(CI_MAGICNUMBER, go->neg_magicnumber, go->magicnumber);
    ACKCIVOID(CI_PCOMPRESSION, go->neg_pcompression);
    ACKCIVOID(CI_ACCOMPRESSION, go->neg_accompression);
    ACKCISHORT(CI_MRRU, go->neg_mrru, go->mrru);
    ACKCIVOID(CI_SSNHF, go->neg_ssnhf);
    ACKCIENDP(CI_EPDISC, go->neg_endpoint, go->endpoint.class,
	      go->endpoint.value, go->endpoint.length);

    if (len != 0)
	goto bad;
    return (1);
bad:
    LCPDEBUG(("lcp_acki: received bad Ack!"));
    return (0);
}


static int
lcp_nakci(f, p, len, treat_as_reject)
    fsm *f;
    u_char *p;
    int len;
    int treat_as_reject;
{
    lcp_options *go = &lcp_gotoptions[f->unit];
    lcp_options *wo = &lcp_wantoptions[f->unit];
    u_char citype, cichar, *next;
    u_short cishort;
    u_int32_t cilong;
    lcp_options no;		
    lcp_options try;		
    int looped_back = 0;
    int cilen;

    BZERO(&no, sizeof(no));
    try = *go;

#define NAKCIVOID(opt, neg) \
    if (go->neg && \
	len >= CILEN_VOID && \
	p[1] == CILEN_VOID && \
	p[0] == opt) { \
	len -= CILEN_VOID; \
	INCPTR(CILEN_VOID, p); \
	no.neg = 1; \
	try.neg = 0; \
    }
#define NAKCICHAP(opt, neg, code) \
    if (go->neg && \
	len >= CILEN_CHAP && \
	p[1] == CILEN_CHAP && \
	p[0] == opt) { \
	len -= CILEN_CHAP; \
	INCPTR(2, p); \
	GETSHORT(cishort, p); \
	GETCHAR(cichar, p); \
	no.neg = 1; \
	code \
    }
#define NAKCICHAR(opt, neg, code) \
    if (go->neg && \
	len >= CILEN_CHAR && \
	p[1] == CILEN_CHAR && \
	p[0] == opt) { \
	len -= CILEN_CHAR; \
	INCPTR(2, p); \
	GETCHAR(cichar, p); \
	no.neg = 1; \
	code \
    }
#define NAKCISHORT(opt, neg, code) \
    if (go->neg && \
	len >= CILEN_SHORT && \
	p[1] == CILEN_SHORT && \
	p[0] == opt) { \
	len -= CILEN_SHORT; \
	INCPTR(2, p); \
	GETSHORT(cishort, p); \
	no.neg = 1; \
	code \
    }
#define NAKCILONG(opt, neg, code) \
    if (go->neg && \
	len >= CILEN_LONG && \
	p[1] == CILEN_LONG && \
	p[0] == opt) { \
	len -= CILEN_LONG; \
	INCPTR(2, p); \
	GETLONG(cilong, p); \
	no.neg = 1; \
	code \
    }
#define NAKCILQR(opt, neg, code) \
    if (go->neg && \
	len >= CILEN_LQR && \
	p[1] == CILEN_LQR && \
	p[0] == opt) { \
	len -= CILEN_LQR; \
	INCPTR(2, p); \
	GETSHORT(cishort, p); \
	GETLONG(cilong, p); \
	no.neg = 1; \
	code \
    }
#define NAKCIENDP(opt, neg) \
    if (go->neg && \
	len >= CILEN_CHAR && \
	p[0] == opt && \
	p[1] >= CILEN_CHAR && \
	p[1] <= len) { \
	len -= p[1]; \
	INCPTR(p[1], p); \
	no.neg = 1; \
	try.neg = 0; \
    }

    if (go->neg_mru && go->mru != DEFMRU) {
	NAKCISHORT(CI_MRU, neg_mru,
		   if (cishort <= wo->mru || cishort <= DEFMRU)
		       try.mru = cishort;
		   );
    }

    if (go->neg_asyncmap && go->asyncmap != 0xFFFFFFFF) {
	NAKCILONG(CI_ASYNCMAP, neg_asyncmap,
		  try.asyncmap = go->asyncmap | cilong;
		  );
    }

    if ((go->neg_chap || go->neg_upap || go->neg_eap)
	&& len >= CILEN_SHORT
	&& p[0] == CI_AUTHTYPE && p[1] >= CILEN_SHORT && p[1] <= len) {
	cilen = p[1];
	len -= cilen;
	no.neg_chap = go->neg_chap;
	no.neg_upap = go->neg_upap;
	no.neg_eap = go->neg_eap;
	INCPTR(2, p);
	GETSHORT(cishort, p);
	if (cishort == PPP_PAP && cilen == CILEN_SHORT) {
	    
	    if (go->neg_eap)
		try.neg_eap = 0;

	    
	    else if (go->neg_chap)
		try.neg_chap = 0;
	    else
		goto bad;

	} else if (cishort == PPP_CHAP && cilen == CILEN_CHAP) {
	    GETCHAR(cichar, p);
	    
	    if (go->neg_eap) {
		try.neg_eap = 0;
		
		if (CHAP_CANDIGEST(go->chap_mdtype, cichar))
		    try.chap_mdtype = CHAP_MDTYPE_D(cichar);
	    } else if (go->neg_chap) {
		if (cichar != CHAP_DIGEST(go->chap_mdtype)) {
		    if (CHAP_CANDIGEST(go->chap_mdtype, cichar)) {
			
			try.chap_mdtype = CHAP_MDTYPE_D(cichar);
		    } else {
			
			try.chap_mdtype &= ~(CHAP_MDTYPE(try.chap_mdtype));
			if (try.chap_mdtype == MDTYPE_NONE) 
			    try.neg_chap = 0;
		    }
		} else {
		    goto bad;
		}
	    } else {
		try.neg_upap = 0;
	    }

	} else {

	    if (cishort == PPP_EAP && cilen == CILEN_SHORT && go->neg_eap)
		dbglog("Unexpected Conf-Nak for EAP");

	    if (go->neg_eap)
		try.neg_eap = 0;
	    else if (go->neg_chap)
		try.neg_chap = 0;
	    else
		try.neg_upap = 0;
	    p += cilen - CILEN_SHORT;
	}
    }

    NAKCILQR(CI_QUALITY, neg_lqr,
	     if (cishort != PPP_LQR)
		 try.neg_lqr = 0;
	     else
		 try.lqr_period = cilong;
	     );

    NAKCICHAR(CI_CALLBACK, neg_cbcp,
              try.neg_cbcp = 0;
              );

    NAKCILONG(CI_MAGICNUMBER, neg_magicnumber,
	      try.magicnumber = magic();
	      looped_back = 1;
	      );

    NAKCIVOID(CI_PCOMPRESSION, neg_pcompression);
    NAKCIVOID(CI_ACCOMPRESSION, neg_accompression);

    if (go->neg_mrru) {
	NAKCISHORT(CI_MRRU, neg_mrru,
		   if (treat_as_reject)
		       try.neg_mrru = 0;
		   else if (cishort <= wo->mrru)
		       try.mrru = cishort;
		   );
    }

    NAKCIVOID(CI_SSNHF, neg_ssnhf);

    NAKCIENDP(CI_EPDISC, neg_endpoint);

    while (len >= CILEN_VOID) {
	GETCHAR(citype, p);
	GETCHAR(cilen, p);
	if (cilen < CILEN_VOID || (len -= cilen) < 0)
	    goto bad;
	next = p + cilen - 2;

	switch (citype) {
	case CI_MRU:
	    if ((go->neg_mru && go->mru != DEFMRU)
		|| no.neg_mru || cilen != CILEN_SHORT)
		goto bad;
	    GETSHORT(cishort, p);
	    if (cishort < DEFMRU) {
		try.neg_mru = 1;
		try.mru = cishort;
	    }
	    break;
	case CI_ASYNCMAP:
	    if ((go->neg_asyncmap && go->asyncmap != 0xFFFFFFFF)
		|| no.neg_asyncmap || cilen != CILEN_LONG)
		goto bad;
	    break;
	case CI_AUTHTYPE:
	    if (go->neg_chap || no.neg_chap || go->neg_upap || no.neg_upap ||
		go->neg_eap || no.neg_eap)
		goto bad;
	    break;
	case CI_MAGICNUMBER:
	    if (go->neg_magicnumber || no.neg_magicnumber ||
		cilen != CILEN_LONG)
		goto bad;
	    break;
	case CI_PCOMPRESSION:
	    if (go->neg_pcompression || no.neg_pcompression
		|| cilen != CILEN_VOID)
		goto bad;
	    break;
	case CI_ACCOMPRESSION:
	    if (go->neg_accompression || no.neg_accompression
		|| cilen != CILEN_VOID)
		goto bad;
	    break;
	case CI_QUALITY:
	    if (go->neg_lqr || no.neg_lqr || cilen != CILEN_LQR)
		goto bad;
	    break;
	case CI_MRRU:
	    if (go->neg_mrru || no.neg_mrru || cilen != CILEN_SHORT)
		goto bad;
	    break;
	case CI_SSNHF:
	    if (go->neg_ssnhf || no.neg_ssnhf || cilen != CILEN_VOID)
		goto bad;
	    try.neg_ssnhf = 1;
	    break;
	case CI_EPDISC:
	    if (go->neg_endpoint || no.neg_endpoint || cilen < CILEN_CHAR)
		goto bad;
	    break;
	}
	p = next;
    }

    if (f->state != OPENED) {
	if (looped_back) {
	    if (++try.numloops >= lcp_loopbackfail) {
		notice("Serial line is looped back.");
		status = EXIT_LOOPBACK;
		lcp_close(f->unit, "Loopback detected");
	    }
	} else
	    try.numloops = 0;
	*go = try;
    }

    return 1;

bad:
    LCPDEBUG(("lcp_nakci: received bad Nak!"));
    return 0;
}


static int
lcp_rejci(f, p, len)
    fsm *f;
    u_char *p;
    int len;
{
    lcp_options *go = &lcp_gotoptions[f->unit];
    u_char cichar;
    u_short cishort;
    u_int32_t cilong;
    lcp_options try;		

    try = *go;

#define REJCIVOID(opt, neg) \
    if (go->neg && \
	len >= CILEN_VOID && \
	p[1] == CILEN_VOID && \
	p[0] == opt) { \
	len -= CILEN_VOID; \
	INCPTR(CILEN_VOID, p); \
	try.neg = 0; \
    }
#define REJCISHORT(opt, neg, val) \
    if (go->neg && \
	len >= CILEN_SHORT && \
	p[1] == CILEN_SHORT && \
	p[0] == opt) { \
	len -= CILEN_SHORT; \
	INCPTR(2, p); \
	GETSHORT(cishort, p); \
	 \
	if (cishort != val) \
	    goto bad; \
	try.neg = 0; \
    }
#define REJCICHAP(opt, neg, val) \
    if (go->neg && \
	len >= CILEN_CHAP && \
	p[1] == CILEN_CHAP && \
	p[0] == opt) { \
	len -= CILEN_CHAP; \
	INCPTR(2, p); \
	GETSHORT(cishort, p); \
	GETCHAR(cichar, p); \
	 \
	if ((cishort != PPP_CHAP) || (cichar != (CHAP_DIGEST(val)))) \
	    goto bad; \
	try.neg = 0; \
	try.neg_eap = try.neg_upap = 0; \
    }
#define REJCILONG(opt, neg, val) \
    if (go->neg && \
	len >= CILEN_LONG && \
	p[1] == CILEN_LONG && \
	p[0] == opt) { \
	len -= CILEN_LONG; \
	INCPTR(2, p); \
	GETLONG(cilong, p); \
	 \
	if (cilong != val) \
	    goto bad; \
	try.neg = 0; \
    }
#define REJCILQR(opt, neg, val) \
    if (go->neg && \
	len >= CILEN_LQR && \
	p[1] == CILEN_LQR && \
	p[0] == opt) { \
	len -= CILEN_LQR; \
	INCPTR(2, p); \
	GETSHORT(cishort, p); \
	GETLONG(cilong, p); \
	 \
	if (cishort != PPP_LQR || cilong != val) \
	    goto bad; \
	try.neg = 0; \
    }
#define REJCICBCP(opt, neg, val) \
    if (go->neg && \
	len >= CILEN_CBCP && \
	p[1] == CILEN_CBCP && \
	p[0] == opt) { \
	len -= CILEN_CBCP; \
	INCPTR(2, p); \
	GETCHAR(cichar, p); \
	 \
	if (cichar != val) \
	    goto bad; \
	try.neg = 0; \
    }
#define REJCIENDP(opt, neg, class, val, vlen) \
    if (go->neg && \
	len >= CILEN_CHAR + vlen && \
	p[0] == opt && \
	p[1] == CILEN_CHAR + vlen) { \
	int i; \
	len -= CILEN_CHAR + vlen; \
	INCPTR(2, p); \
	GETCHAR(cichar, p); \
	if (cichar != class) \
	    goto bad; \
	for (i = 0; i < vlen; ++i) { \
	    GETCHAR(cichar, p); \
	    if (cichar != val[i]) \
		goto bad; \
	} \
	try.neg = 0; \
    }

    REJCISHORT(CI_MRU, neg_mru, go->mru);
    REJCILONG(CI_ASYNCMAP, neg_asyncmap, go->asyncmap);
    REJCISHORT(CI_AUTHTYPE, neg_eap, PPP_EAP);
    if (!go->neg_eap) {
	REJCICHAP(CI_AUTHTYPE, neg_chap, go->chap_mdtype);
	if (!go->neg_chap) {
	    REJCISHORT(CI_AUTHTYPE, neg_upap, PPP_PAP);
	}
    }
    REJCILQR(CI_QUALITY, neg_lqr, go->lqr_period);
    REJCICBCP(CI_CALLBACK, neg_cbcp, CBCP_OPT);
    REJCILONG(CI_MAGICNUMBER, neg_magicnumber, go->magicnumber);
    REJCIVOID(CI_PCOMPRESSION, neg_pcompression);
    REJCIVOID(CI_ACCOMPRESSION, neg_accompression);
    REJCISHORT(CI_MRRU, neg_mrru, go->mrru);
    REJCIVOID(CI_SSNHF, neg_ssnhf);
    REJCIENDP(CI_EPDISC, neg_endpoint, go->endpoint.class,
	      go->endpoint.value, go->endpoint.length);

    if (len != 0)
	goto bad;
    if (f->state != OPENED)
	*go = try;
    return 1;

bad:
    LCPDEBUG(("lcp_rejci: received bad Reject!"));
    return 0;
}


static int
lcp_reqci(f, inp, lenp, reject_if_disagree)
    fsm *f;
    u_char *inp;		
    int *lenp;			
    int reject_if_disagree;
{
    lcp_options *go = &lcp_gotoptions[f->unit];
    lcp_options *ho = &lcp_hisoptions[f->unit];
    lcp_options *ao = &lcp_allowoptions[f->unit];
    u_char *cip, *next;		
    int cilen, citype, cichar;	
    u_short cishort;		
    u_int32_t cilong;		
    int rc = CONFACK;		
    int orc;			
    u_char *p;			
    u_char *rejp;		
    u_char *nakp;		
    int l = *lenp;		

    BZERO(ho, sizeof(*ho));

    next = inp;
    nakp = nak_buffer;
    rejp = inp;
    while (l) {
	orc = CONFACK;			
	cip = p = next;			
	if (l < 2 ||			
	    p[1] < 2 ||			
	    p[1] > l) {			
	    LCPDEBUG(("lcp_reqci: bad CI length!"));
	    orc = CONFREJ;		
	    cilen = l;			
	    l = 0;			
	    citype = 0;
	    goto endswitch;
	}
	GETCHAR(citype, p);		
	GETCHAR(cilen, p);		
	l -= cilen;			
	next += cilen;			

	switch (citype) {		
	case CI_MRU:
	    if (!ao->neg_mru ||		
		cilen != CILEN_SHORT) {	
		orc = CONFREJ;		
		break;
	    }
	    GETSHORT(cishort, p);	

	    if (cishort < MINMRU) {
		orc = CONFNAK;		
		PUTCHAR(CI_MRU, nakp);
		PUTCHAR(CILEN_SHORT, nakp);
		PUTSHORT(MINMRU, nakp);	
		break;
	    }
	    ho->neg_mru = 1;		
	    ho->mru = cishort;		
	    break;

	case CI_ASYNCMAP:
	    if (!ao->neg_asyncmap ||
		cilen != CILEN_LONG) {
		orc = CONFREJ;
		break;
	    }
	    GETLONG(cilong, p);

	    if ((ao->asyncmap & ~cilong) != 0) {
		orc = CONFNAK;
		PUTCHAR(CI_ASYNCMAP, nakp);
		PUTCHAR(CILEN_LONG, nakp);
		PUTLONG(ao->asyncmap | cilong, nakp);
		break;
	    }
	    ho->neg_asyncmap = 1;
	    ho->asyncmap = cilong;
	    break;

	case CI_AUTHTYPE:
	    if (cilen < CILEN_SHORT ||
		!(ao->neg_upap || ao->neg_chap || ao->neg_eap)) {
		dbglog("No auth is possible");
		orc = CONFREJ;
		break;
	    }
	    GETSHORT(cishort, p);


	    if (cishort == PPP_PAP) {
		
		if (ho->neg_chap || ho->neg_eap ||
		    cilen != CILEN_SHORT) {
		    LCPDEBUG(("lcp_reqci: rcvd AUTHTYPE PAP, rejecting..."));
		    orc = CONFREJ;
		    break;
		}
		if (!ao->neg_upap) {	
		    orc = CONFNAK;	
		    PUTCHAR(CI_AUTHTYPE, nakp);
		    if (ao->neg_eap) {
			PUTCHAR(CILEN_SHORT, nakp);
			PUTSHORT(PPP_EAP, nakp);
		    } else {
			PUTCHAR(CILEN_CHAP, nakp);
			PUTSHORT(PPP_CHAP, nakp);
			PUTCHAR(CHAP_DIGEST(ao->chap_mdtype), nakp);
		    }
		    break;
		}
		ho->neg_upap = 1;
		break;
	    }
	    if (cishort == PPP_CHAP) {
		
		if (ho->neg_upap || ho->neg_eap ||
		    cilen != CILEN_CHAP) {
		    LCPDEBUG(("lcp_reqci: rcvd AUTHTYPE CHAP, rejecting..."));
		    orc = CONFREJ;
		    break;
		}
		if (!ao->neg_chap) {	
		    orc = CONFNAK;	
		    PUTCHAR(CI_AUTHTYPE, nakp);
		    PUTCHAR(CILEN_SHORT, nakp);
		    if (ao->neg_eap) {
			PUTSHORT(PPP_EAP, nakp);
		    } else {
			PUTSHORT(PPP_PAP, nakp);
		    }
		    break;
		}
		GETCHAR(cichar, p);	
		if (!(CHAP_CANDIGEST(ao->chap_mdtype, cichar))) {
		    orc = CONFNAK;
		    PUTCHAR(CI_AUTHTYPE, nakp);
		    PUTCHAR(CILEN_CHAP, nakp);
		    PUTSHORT(PPP_CHAP, nakp);
		    PUTCHAR(CHAP_DIGEST(ao->chap_mdtype), nakp);
		    break;
		}
		ho->chap_mdtype = CHAP_MDTYPE_D(cichar); 
		ho->neg_chap = 1;
		break;
	    }
	    if (cishort == PPP_EAP) {
		
		if (ho->neg_chap || ho->neg_upap || cilen != CILEN_SHORT) {
		    LCPDEBUG(("lcp_reqci: rcvd AUTHTYPE EAP, rejecting..."));
		    orc = CONFREJ;
		    break;
		}
		if (!ao->neg_eap) {	
		    orc = CONFNAK;	
		    PUTCHAR(CI_AUTHTYPE, nakp);
		    if (ao->neg_chap) {
			PUTCHAR(CILEN_CHAP, nakp);
			PUTSHORT(PPP_CHAP, nakp);
			PUTCHAR(CHAP_DIGEST(ao->chap_mdtype), nakp);
		    } else {
			PUTCHAR(CILEN_SHORT, nakp);
			PUTSHORT(PPP_PAP, nakp);
		    }
		    break;
		}
		ho->neg_eap = 1;
		break;
	    }

	    orc = CONFNAK;
	    PUTCHAR(CI_AUTHTYPE, nakp);
	    if (ao->neg_eap) {
		PUTCHAR(CILEN_SHORT, nakp);
		PUTSHORT(PPP_EAP, nakp);
	    } else if (ao->neg_chap) {
		PUTCHAR(CILEN_CHAP, nakp);
		PUTSHORT(PPP_CHAP, nakp);
		PUTCHAR(CHAP_DIGEST(ao->chap_mdtype), nakp);
	    } else {
		PUTCHAR(CILEN_SHORT, nakp);
		PUTSHORT(PPP_PAP, nakp);
	    }
	    break;

	case CI_QUALITY:
	    if (!ao->neg_lqr ||
		cilen != CILEN_LQR) {
		orc = CONFREJ;
		break;
	    }

	    GETSHORT(cishort, p);
	    GETLONG(cilong, p);

	    if (cishort != PPP_LQR) {
		orc = CONFNAK;
		PUTCHAR(CI_QUALITY, nakp);
		PUTCHAR(CILEN_LQR, nakp);
		PUTSHORT(PPP_LQR, nakp);
		PUTLONG(ao->lqr_period, nakp);
		break;
	    }
	    break;

	case CI_MAGICNUMBER:
	    if (!(ao->neg_magicnumber || go->neg_magicnumber) ||
		cilen != CILEN_LONG) {
		orc = CONFREJ;
		break;
	    }
	    GETLONG(cilong, p);

	    if (go->neg_magicnumber &&
		cilong == go->magicnumber) {
		cilong = magic();	
		orc = CONFNAK;
		PUTCHAR(CI_MAGICNUMBER, nakp);
		PUTCHAR(CILEN_LONG, nakp);
		PUTLONG(cilong, nakp);
		break;
	    }
	    ho->neg_magicnumber = 1;
	    ho->magicnumber = cilong;
	    break;


	case CI_PCOMPRESSION:
	    if (!ao->neg_pcompression ||
		cilen != CILEN_VOID) {
		orc = CONFREJ;
		break;
	    }
	    ho->neg_pcompression = 1;
	    break;

	case CI_ACCOMPRESSION:
	    if (!ao->neg_accompression ||
		cilen != CILEN_VOID) {
		orc = CONFREJ;
		break;
	    }
	    ho->neg_accompression = 1;
	    break;

	case CI_MRRU:
	    if (!ao->neg_mrru || !multilink ||
		cilen != CILEN_SHORT) {
		orc = CONFREJ;
		break;
	    }

	    GETSHORT(cishort, p);
	    
	    ho->neg_mrru = 1;
	    ho->mrru = cishort;
	    break;

	case CI_SSNHF:
	    if (!ao->neg_ssnhf || !multilink ||
		cilen != CILEN_VOID) {
		orc = CONFREJ;
		break;
	    }
	    ho->neg_ssnhf = 1;
	    break;

	case CI_EPDISC:
	    if (!ao->neg_endpoint ||
		cilen < CILEN_CHAR ||
		cilen > CILEN_CHAR + MAX_ENDP_LEN) {
		orc = CONFREJ;
		break;
	    }
	    GETCHAR(cichar, p);
	    cilen -= CILEN_CHAR;
	    ho->neg_endpoint = 1;
	    ho->endpoint.class = cichar;
	    ho->endpoint.length = cilen;
	    BCOPY(p, ho->endpoint.value, cilen);
	    INCPTR(cilen, p);
	    break;

	default:
	    LCPDEBUG(("lcp_reqci: rcvd unknown option %d", citype));
	    orc = CONFREJ;
	    break;
	}

endswitch:
	if (orc == CONFACK &&		
	    rc != CONFACK)		
	    continue;			

	if (orc == CONFNAK) {		
	    if (reject_if_disagree	
		&& citype != CI_MAGICNUMBER) {
		orc = CONFREJ;		
	    } else {
		if (rc == CONFREJ)	
		    continue;		
		rc = CONFNAK;
	    }
	}
	if (orc == CONFREJ) {		
	    rc = CONFREJ;
	    if (cip != rejp)		
		BCOPY(cip, rejp, cilen); 
	    INCPTR(cilen, rejp);	
	}
    }


    switch (rc) {
    case CONFACK:
	*lenp = next - inp;
	break;
    case CONFNAK:
	*lenp = nakp - nak_buffer;
	BCOPY(nak_buffer, inp, *lenp);
	break;
    case CONFREJ:
	*lenp = rejp - inp;
	break;
    }

    LCPDEBUG(("lcp_reqci: returning CONF%s.", CODENAME(rc)));
    return (rc);			
}


static void
lcp_up(f)
    fsm *f;
{
    lcp_options *wo = &lcp_wantoptions[f->unit];
    lcp_options *ho = &lcp_hisoptions[f->unit];
    lcp_options *go = &lcp_gotoptions[f->unit];
    lcp_options *ao = &lcp_allowoptions[f->unit];
    int mtu, mru;

    if (!go->neg_magicnumber)
	go->magicnumber = 0;
    if (!ho->neg_magicnumber)
	ho->magicnumber = 0;

    mtu = ho->neg_mru? ho->mru: PPP_MRU;
    mru = go->neg_mru? MAX(wo->mru, go->mru): PPP_MRU;
#ifdef HAVE_MULTILINK
    if (!(multilink && go->neg_mrru && ho->neg_mrru))
#endif 
	netif_set_mtu(f->unit, MIN(MIN(mtu, mru), ao->mru));
    ppp_send_config(f->unit, mtu,
		    (ho->neg_asyncmap? ho->asyncmap: 0xffffffff),
		    ho->neg_pcompression, ho->neg_accompression);
    ppp_recv_config(f->unit, mru,
		    (lax_recv? 0: go->neg_asyncmap? go->asyncmap: 0xffffffff),
		    go->neg_pcompression, go->neg_accompression);

    if (ho->neg_mru)
	peer_mru[f->unit] = ho->mru;

    lcp_echo_lowerup(f->unit);  

    link_established(f->unit);
}


static void
lcp_down(f)
    fsm *f;
{
    lcp_options *go = &lcp_gotoptions[f->unit];

    lcp_echo_lowerdown(f->unit);

    link_down(f->unit);

    ppp_send_config(f->unit, PPP_MRU, 0xffffffff, 0, 0);
    ppp_recv_config(f->unit, PPP_MRU,
		    (go->neg_asyncmap? go->asyncmap: 0xffffffff),
		    go->neg_pcompression, go->neg_accompression);
    peer_mru[f->unit] = PPP_MRU;
}


static void
lcp_starting(f)
    fsm *f;
{
    link_required(f->unit);
}


static void
lcp_finished(f)
    fsm *f;
{
    link_terminated(f->unit);
}


static char *lcp_codenames[] = {
    "ConfReq", "ConfAck", "ConfNak", "ConfRej",
    "TermReq", "TermAck", "CodeRej", "ProtRej",
    "EchoReq", "EchoRep", "DiscReq", "Ident",
    "TimeRem"
};

static int
lcp_printpkt(p, plen, printer, arg)
    u_char *p;
    int plen;
    void (*printer) __P((void *, char *, ...));
    void *arg;
{
    int code, id, len, olen, i;
    u_char *pstart, *optend;
    u_short cishort;
    u_int32_t cilong;

    if (plen < HEADERLEN)
	return 0;
    pstart = p;
    GETCHAR(code, p);
    GETCHAR(id, p);
    GETSHORT(len, p);
    if (len < HEADERLEN || len > plen)
	return 0;

    if (code >= 1 && code <= sizeof(lcp_codenames) / sizeof(char *))
	printer(arg, " %s", lcp_codenames[code-1]);
    else
	printer(arg, " code=0x%x", code);
    printer(arg, " id=0x%x", id);
    len -= HEADERLEN;
    switch (code) {
    case CONFREQ:
    case CONFACK:
    case CONFNAK:
    case CONFREJ:
	
	while (len >= 2) {
	    GETCHAR(code, p);
	    GETCHAR(olen, p);
	    p -= 2;
	    if (olen < 2 || olen > len) {
		break;
	    }
	    printer(arg, " <");
	    len -= olen;
	    optend = p + olen;
	    switch (code) {
	    case CI_MRU:
		if (olen == CILEN_SHORT) {
		    p += 2;
		    GETSHORT(cishort, p);
		    printer(arg, "mru %d", cishort);
		}
		break;
	    case CI_ASYNCMAP:
		if (olen == CILEN_LONG) {
		    p += 2;
		    GETLONG(cilong, p);
		    printer(arg, "asyncmap 0x%x", cilong);
		}
		break;
	    case CI_AUTHTYPE:
		if (olen >= CILEN_SHORT) {
		    p += 2;
		    printer(arg, "auth ");
		    GETSHORT(cishort, p);
		    switch (cishort) {
		    case PPP_PAP:
			printer(arg, "pap");
			break;
		    case PPP_CHAP:
			printer(arg, "chap");
			if (p < optend) {
			    switch (*p) {
			    case CHAP_MD5:
				printer(arg, " MD5");
				++p;
				break;
			    case CHAP_MICROSOFT:
				printer(arg, " MS");
				++p;
				break;

			    case CHAP_MICROSOFT_V2:
				printer(arg, " MS-v2");
				++p;
				break;
			    }
			}
			break;
		    case PPP_EAP:
			printer(arg, "eap");
			break;
		    default:
			printer(arg, "0x%x", cishort);
		    }
		}
		break;
	    case CI_QUALITY:
		if (olen >= CILEN_SHORT) {
		    p += 2;
		    printer(arg, "quality ");
		    GETSHORT(cishort, p);
		    switch (cishort) {
		    case PPP_LQR:
			printer(arg, "lqr");
			break;
		    default:
			printer(arg, "0x%x", cishort);
		    }
		}
		break;
	    case CI_CALLBACK:
		if (olen >= CILEN_CHAR) {
		    p += 2;
		    printer(arg, "callback ");
		    GETCHAR(cishort, p);
		    switch (cishort) {
		    case CBCP_OPT:
			printer(arg, "CBCP");
			break;
		    default:
			printer(arg, "0x%x", cishort);
		    }
		}
		break;
	    case CI_MAGICNUMBER:
		if (olen == CILEN_LONG) {
		    p += 2;
		    GETLONG(cilong, p);
		    printer(arg, "magic 0x%x", cilong);
		}
		break;
	    case CI_PCOMPRESSION:
		if (olen == CILEN_VOID) {
		    p += 2;
		    printer(arg, "pcomp");
		}
		break;
	    case CI_ACCOMPRESSION:
		if (olen == CILEN_VOID) {
		    p += 2;
		    printer(arg, "accomp");
		}
		break;
	    case CI_MRRU:
		if (olen == CILEN_SHORT) {
		    p += 2;
		    GETSHORT(cishort, p);
		    printer(arg, "mrru %d", cishort);
		}
		break;
	    case CI_SSNHF:
		if (olen == CILEN_VOID) {
		    p += 2;
		    printer(arg, "ssnhf");
		}
		break;
	    case CI_EPDISC:
#ifdef HAVE_MULTILINK
		if (olen >= CILEN_CHAR) {
		    struct epdisc epd;
		    p += 2;
		    GETCHAR(epd.class, p);
		    epd.length = olen - CILEN_CHAR;
		    if (epd.length > MAX_ENDP_LEN)
			epd.length = MAX_ENDP_LEN;
		    if (epd.length > 0) {
			BCOPY(p, epd.value, epd.length);
			p += epd.length;
		    }
		    printer(arg, "endpoint [%s]", epdisc_to_str(&epd));
		}
#else
		printer(arg, "endpoint");
#endif
		break;
	    }
	    while (p < optend) {
		GETCHAR(code, p);
		printer(arg, " %.2x", code);
	    }
	    printer(arg, ">");
	}
	break;

    case TERMACK:
    case TERMREQ:
	if (len > 0 && *p >= ' ' && *p < 0x7f) {
	    printer(arg, " ");
	    print_string((char *)p, len, printer, arg);
	    p += len;
	    len = 0;
	}
	break;

    case ECHOREQ:
    case ECHOREP:
    case DISCREQ:
	if (len >= 4) {
	    GETLONG(cilong, p);
	    printer(arg, " magic=0x%x", cilong);
	    len -= 4;
	}
	break;

    case IDENTIF:
    case TIMEREM:
	if (len >= 4) {
	    GETLONG(cilong, p);
	    printer(arg, " magic=0x%x", cilong);
	    len -= 4;
	}
	if (code == TIMEREM) {
	    if (len < 4)
		break;
	    GETLONG(cilong, p);
	    printer(arg, " seconds=%u", cilong);
	    len -= 4;
	}
	if (len > 0) {
	    printer(arg, " ");
	    print_string((char *)p, len, printer, arg);
	    p += len;
	    len = 0;
	}
	break;
    }

    
    for (i = 0; i < len && i < 32; ++i) {
	GETCHAR(code, p);
	printer(arg, " %.2x", code);
    }
    if (i < len) {
	printer(arg, " ...");
	p += len - i;
    }

    return p - pstart;
}


static
void LcpLinkFailure (f)
    fsm *f;
{
    if (f->state == OPENED) {
	info("No response to %d echo-requests", lcp_echos_pending);
        notice("Serial link appears to be disconnected.");
	status = EXIT_PEER_DEAD;
	lcp_close(f->unit, "Peer not responding");
    }
}


static void
LcpEchoCheck (f)
    fsm *f;
{
    LcpSendEchoRequest (f);
    if (f->state != OPENED)
	return;

    if (lcp_echo_timer_running)
	warn("assertion lcp_echo_timer_running==0 failed");
    TIMEOUT (LcpEchoTimeout, f, lcp_echo_interval);
    lcp_echo_timer_running = 1;
}


static void
LcpEchoTimeout (arg)
    void *arg;
{
    if (lcp_echo_timer_running != 0) {
        lcp_echo_timer_running = 0;
        LcpEchoCheck ((fsm *) arg);
    }
}


static void
lcp_received_echo_reply (f, id, inp, len)
    fsm *f;
    int id;
    u_char *inp;
    int len;
{
    u_int32_t magic;

    
    if (len < 4) {
	dbglog("lcp: received short Echo-Reply, length %d", len);
	return;
    }
    GETLONG(magic, inp);
    if (lcp_gotoptions[f->unit].neg_magicnumber
	&& magic == lcp_gotoptions[f->unit].magicnumber) {
	warn("appear to have received our own echo-reply!");
	return;
    }

    
    lcp_echos_pending = 0;
}


static void
LcpSendEchoRequest (f)
    fsm *f;
{
    u_int32_t lcp_magic;
    u_char pkt[4], *pktp;

    if (lcp_echo_fails != 0) {
        if (lcp_echos_pending >= lcp_echo_fails) {
            LcpLinkFailure(f);
	    lcp_echos_pending = 0;
	}
    }

    if (f->state == OPENED) {
        lcp_magic = lcp_gotoptions[f->unit].magicnumber;
	pktp = pkt;
	PUTLONG(lcp_magic, pktp);
        fsm_sdata(f, ECHOREQ, lcp_echo_number++ & 0xFF, pkt, pktp - pkt);
	++lcp_echos_pending;
    }
}


static void
lcp_echo_lowerup (unit)
    int unit;
{
    fsm *f = &lcp_fsm[unit];

    
    lcp_echos_pending      = 0;
    lcp_echo_number        = 0;
    lcp_echo_timer_running = 0;
  
    
    if (lcp_echo_interval != 0)
        LcpEchoCheck (f);
}


static void
lcp_echo_lowerdown (unit)
    int unit;
{
    fsm *f = &lcp_fsm[unit];

    if (lcp_echo_timer_running != 0) {
        UNTIMEOUT (LcpEchoTimeout, f);
        lcp_echo_timer_running = 0;
    }
}
