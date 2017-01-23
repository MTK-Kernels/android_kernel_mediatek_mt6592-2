/*
 * ipv6cp.c - PPP IPV6 Control Protocol.
 *
 * Copyright (c) 1999 Tommi Komulainen.  All rights reserved.
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
 * 3. The name(s) of the authors of this software must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission.
 *
 * 4. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by Tommi Komulainen
 *     <Tommi.Komulainen@iki.fi>".
 *
 * THE AUTHORS OF THIS SOFTWARE DISCLAIM ALL WARRANTIES WITH REGARD TO
 * THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS, IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

/*  Original version, based on RFC2023 :

    Copyright (c) 1995, 1996, 1997 Francis.Dupont@inria.fr, INRIA Rocquencourt,
    Alain.Durand@imag.fr, IMAG,
    Jean-Luc.Richier@imag.fr, IMAG-LSR.

    Copyright (c) 1998, 1999 Francis.Dupont@inria.fr, GIE DYADE,
    Alain.Durand@imag.fr, IMAG,
    Jean-Luc.Richier@imag.fr, IMAG-LSR.

    Ce travail a été fait au sein du GIE DYADE (Groupement d'Intérêt
    Économique ayant pour membres BULL S.A. et l'INRIA).

    Ce logiciel informatique est disponible aux conditions
    usuelles dans la recherche, c'est-à-dire qu'il peut
    être utilisé, copié, modifié, distribué à l'unique
    condition que ce texte soit conservé afin que
    l'origine de ce logiciel soit reconnue.

    Le nom de l'Institut National de Recherche en Informatique
    et en Automatique (INRIA), de l'IMAG, ou d'une personne morale
    ou physique ayant participé à l'élaboration de ce logiciel ne peut
    être utilisé sans son accord préalable explicite.

    Ce logiciel est fourni tel quel sans aucune garantie,
    support ou responsabilité d'aucune sorte.
    Ce logiciel est dérivé de sources d'origine
    "University of California at Berkeley" et
    "Digital Equipment Corporation" couvertes par des copyrights.

    L'Institut d'Informatique et de Mathématiques Appliquées de Grenoble (IMAG)
    est une fédération d'unités mixtes de recherche du CNRS, de l'Institut National
    Polytechnique de Grenoble et de l'Université Joseph Fourier regroupant
    sept laboratoires dont le laboratoire Logiciels, Systèmes, Réseaux (LSR).

    This work has been done in the context of GIE DYADE (joint R & D venture
    between BULL S.A. and INRIA).

    This software is available with usual "research" terms
    with the aim of retain credits of the software. 
    Permission to use, copy, modify and distribute this software for any
    purpose and without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies,
    and the name of INRIA, IMAG, or any contributor not be used in advertising
    or publicity pertaining to this material without the prior explicit
    permission. The software is provided "as is" without any
    warranties, support or liabilities of any kind.
    This software is derived from source code from
    "University of California at Berkeley" and
    "Digital Equipment Corporation" protected by copyrights.

    Grenoble's Institute of Computer Science and Applied Mathematics (IMAG)
    is a federation of seven research units funded by the CNRS, National
    Polytechnic Institute of Grenoble and University Joseph Fourier.
    The research unit in Software, Systems, Networks (LSR) is member of IMAG.
*/

/*
 * Derived from :
 *
 *
 * ipcp.c - PPP IP Control Protocol.
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
 * $Id: ipv6cp.c,v 1.21 2005/08/25 23:59:34 paulus Exp $ 
 */

#define RCSID	"$Id: ipv6cp.c,v 1.21 2005/08/25 23:59:34 paulus Exp $"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "pppd.h"
#include "fsm.h"
#include "ipcp.h"
#include "ipv6cp.h"
#include "magic.h"
#include "pathnames.h"

static const char rcsid[] = RCSID;

ipv6cp_options ipv6cp_wantoptions[NUM_PPP];     
ipv6cp_options ipv6cp_gotoptions[NUM_PPP];	
ipv6cp_options ipv6cp_allowoptions[NUM_PPP];	
ipv6cp_options ipv6cp_hisoptions[NUM_PPP];	
int no_ifaceid_neg = 0;

static int ipv6cp_is_up;

void (*ipv6_up_hook) __P((void)) = NULL;

void (*ipv6_down_hook) __P((void)) = NULL;

struct notifier *ipv6_up_notifier = NULL;
struct notifier *ipv6_down_notifier = NULL;

static void ipv6cp_resetci __P((fsm *));	
static int  ipv6cp_cilen __P((fsm *));	        
static void ipv6cp_addci __P((fsm *, u_char *, int *)); 
static int  ipv6cp_ackci __P((fsm *, u_char *, int));	
static int  ipv6cp_nakci __P((fsm *, u_char *, int, int));
static int  ipv6cp_rejci __P((fsm *, u_char *, int));	
static int  ipv6cp_reqci __P((fsm *, u_char *, int *, int)); 
static void ipv6cp_up __P((fsm *));		
static void ipv6cp_down __P((fsm *));		
static void ipv6cp_finished __P((fsm *));	

fsm ipv6cp_fsm[NUM_PPP];		

static fsm_callbacks ipv6cp_callbacks = { 
    ipv6cp_resetci,		
    ipv6cp_cilen,		
    ipv6cp_addci,		
    ipv6cp_ackci,		
    ipv6cp_nakci,		
    ipv6cp_rejci,		
    ipv6cp_reqci,		
    ipv6cp_up,			
    ipv6cp_down,		
    NULL,			
    ipv6cp_finished,		
    NULL,			
    NULL,			
    NULL,			
    "IPV6CP"			
};

static int setifaceid __P((char **arg));
static void printifaceid __P((option_t *,
			      void (*)(void *, char *, ...), void *));

static option_t ipv6cp_option_list[] = {
    { "ipv6", o_special, (void *)setifaceid,
      "Set interface identifiers for IPV6",
      OPT_A2PRINTER, (void *)printifaceid },

    { "+ipv6", o_bool, &ipv6cp_protent.enabled_flag,
      "Enable IPv6 and IPv6CP", OPT_PRIO | 1 },
    { "noipv6", o_bool, &ipv6cp_protent.enabled_flag,
      "Disable IPv6 and IPv6CP", OPT_PRIOSUB },
    { "-ipv6", o_bool, &ipv6cp_protent.enabled_flag,
      "Disable IPv6 and IPv6CP", OPT_PRIOSUB | OPT_ALIAS },

    { "ipv6cp-accept-local", o_bool, &ipv6cp_allowoptions[0].accept_local,
      "Accept peer's interface identifier for us", 1 },

    { "ipv6cp-use-ipaddr", o_bool, &ipv6cp_allowoptions[0].use_ip,
      "Use (default) IPv4 address as interface identifier", 1 },

    { "ipv6cp-use-persistent", o_bool, &ipv6cp_wantoptions[0].use_persistent,
      "Use uniquely-available persistent value for link local address", 1 },

    { "ipv6cp-restart", o_int, &ipv6cp_fsm[0].timeouttime,
      "Set timeout for IPv6CP", OPT_PRIO },
    { "ipv6cp-max-terminate", o_int, &ipv6cp_fsm[0].maxtermtransmits,
      "Set max #xmits for term-reqs", OPT_PRIO },
    { "ipv6cp-max-configure", o_int, &ipv6cp_fsm[0].maxconfreqtransmits,
      "Set max #xmits for conf-reqs", OPT_PRIO },
    { "ipv6cp-max-failure", o_int, &ipv6cp_fsm[0].maxnakloops,
      "Set max #conf-naks for IPv6CP", OPT_PRIO },

   { NULL }
};


static void ipv6cp_init __P((int));
static void ipv6cp_open __P((int));
static void ipv6cp_close __P((int, char *));
static void ipv6cp_lowerup __P((int));
static void ipv6cp_lowerdown __P((int));
static void ipv6cp_input __P((int, u_char *, int));
static void ipv6cp_protrej __P((int));
static int  ipv6cp_printpkt __P((u_char *, int,
			       void (*) __P((void *, char *, ...)), void *));
static void ipv6_check_options __P((void));
static int  ipv6_demand_conf __P((int));
static int  ipv6_active_pkt __P((u_char *, int));

struct protent ipv6cp_protent = {
    PPP_IPV6CP,
    ipv6cp_init,
    ipv6cp_input,
    ipv6cp_protrej,
    ipv6cp_lowerup,
    ipv6cp_lowerdown,
    ipv6cp_open,
    ipv6cp_close,
    ipv6cp_printpkt,
    NULL,
    0,
    "IPV6CP",
    "IPV6",
    ipv6cp_option_list,
    ipv6_check_options,
    ipv6_demand_conf,
    ipv6_active_pkt
};

static void ipv6cp_clear_addrs __P((int, eui64_t, eui64_t));
static void ipv6cp_script __P((char *));
static void ipv6cp_script_done __P((void *));

#define CILEN_VOID	2
#define CILEN_COMPRESS	4	
#define CILEN_IFACEID   10	

#define CODENAME(x)	((x) == CONFACK ? "ACK" : \
			 (x) == CONFNAK ? "NAK" : "REJ")

static enum script_state {
    s_down,
    s_up,
} ipv6cp_script_state;
static pid_t ipv6cp_script_pid;

static int
setifaceid(argv)
    char **argv;
{
    char *comma, *arg, c;
    ipv6cp_options *wo = &ipv6cp_wantoptions[0];
    struct in6_addr addr;
    static int prio_local, prio_remote;

#define VALIDID(a) ( (((a).s6_addr32[0] == 0) && ((a).s6_addr32[1] == 0)) && \
			(((a).s6_addr32[2] != 0) || ((a).s6_addr32[3] != 0)) )
    
    arg = *argv;
    if ((comma = strchr(arg, ',')) == NULL)
	comma = arg + strlen(arg);
    
    if (comma != arg) {
	c = *comma;
	*comma = '\0';

	if (inet_pton(AF_INET6, arg, &addr) == 0 || !VALIDID(addr)) {
	    option_error("Illegal interface identifier (local): %s", arg);
	    return 0;
	}

	if (option_priority >= prio_local) {
	    eui64_copy(addr.s6_addr32[2], wo->ourid);
	    wo->opt_local = 1;
	    prio_local = option_priority;
	}
	*comma = c;
    }
    
    if (*comma != 0 && *++comma != '\0') {
	if (inet_pton(AF_INET6, comma, &addr) == 0 || !VALIDID(addr)) {
	    option_error("Illegal interface identifier (remote): %s", comma);
	    return 0;
	}
	if (option_priority >= prio_remote) {
	    eui64_copy(addr.s6_addr32[2], wo->hisid);
	    wo->opt_remote = 1;
	    prio_remote = option_priority;
	}
    }

    if (override_value("+ipv6", option_priority, option_source))
	ipv6cp_protent.enabled_flag = 1;
    return 1;
}

char *llv6_ntoa(eui64_t ifaceid);

static void
printifaceid(opt, printer, arg)
    option_t *opt;
    void (*printer) __P((void *, char *, ...));
    void *arg;
{
	ipv6cp_options *wo = &ipv6cp_wantoptions[0];

	if (wo->opt_local)
		printer(arg, "%s", llv6_ntoa(wo->ourid));
	printer(arg, ",");
	if (wo->opt_remote)
		printer(arg, "%s", llv6_ntoa(wo->hisid));
}

char *
llv6_ntoa(ifaceid)
    eui64_t ifaceid;
{
    static char b[64];

    sprintf(b, "fe80::%s", eui64_ntoa(ifaceid));
    return b;
}


static void
ipv6cp_init(unit)
    int unit;
{
    fsm *f = &ipv6cp_fsm[unit];
    ipv6cp_options *wo = &ipv6cp_wantoptions[unit];
    ipv6cp_options *ao = &ipv6cp_allowoptions[unit];

    f->unit = unit;
    f->protocol = PPP_IPV6CP;
    f->callbacks = &ipv6cp_callbacks;
    fsm_init(&ipv6cp_fsm[unit]);

    memset(wo, 0, sizeof(*wo));
    memset(ao, 0, sizeof(*ao));

    wo->accept_local = 1;
    wo->neg_ifaceid = 1;
    ao->neg_ifaceid = 1;

#ifdef IPV6CP_COMP
    wo->neg_vj = 1;
    ao->neg_vj = 1;
    wo->vj_protocol = IPV6CP_COMP;
#endif

}


static void
ipv6cp_open(unit)
    int unit;
{
    fsm_open(&ipv6cp_fsm[unit]);
}


static void
ipv6cp_close(unit, reason)
    int unit;
    char *reason;
{
    fsm_close(&ipv6cp_fsm[unit], reason);
}


static void
ipv6cp_lowerup(unit)
    int unit;
{
    fsm_lowerup(&ipv6cp_fsm[unit]);
}


static void
ipv6cp_lowerdown(unit)
    int unit;
{
    fsm_lowerdown(&ipv6cp_fsm[unit]);
}


static void
ipv6cp_input(unit, p, len)
    int unit;
    u_char *p;
    int len;
{
    fsm_input(&ipv6cp_fsm[unit], p, len);
}


static void
ipv6cp_protrej(unit)
    int unit;
{
    fsm_lowerdown(&ipv6cp_fsm[unit]);
}


static void
ipv6cp_resetci(f)
    fsm *f;
{
    ipv6cp_options *wo = &ipv6cp_wantoptions[f->unit];
    ipv6cp_options *go = &ipv6cp_gotoptions[f->unit];

    wo->req_ifaceid = wo->neg_ifaceid && ipv6cp_allowoptions[f->unit].neg_ifaceid;
    
    if (!wo->opt_local) {
	eui64_magic_nz(wo->ourid);
    }
    
    *go = *wo;
    eui64_zero(go->hisid);	
}


static int
ipv6cp_cilen(f)
    fsm *f;
{
    ipv6cp_options *go = &ipv6cp_gotoptions[f->unit];

#define LENCIVJ(neg)		(neg ? CILEN_COMPRESS : 0)
#define LENCIIFACEID(neg)	(neg ? CILEN_IFACEID : 0)

    return (LENCIIFACEID(go->neg_ifaceid) +
	    LENCIVJ(go->neg_vj));
}


static void
ipv6cp_addci(f, ucp, lenp)
    fsm *f;
    u_char *ucp;
    int *lenp;
{
    ipv6cp_options *go = &ipv6cp_gotoptions[f->unit];
    int len = *lenp;

#define ADDCIVJ(opt, neg, val) \
    if (neg) { \
	int vjlen = CILEN_COMPRESS; \
	if (len >= vjlen) { \
	    PUTCHAR(opt, ucp); \
	    PUTCHAR(vjlen, ucp); \
	    PUTSHORT(val, ucp); \
	    len -= vjlen; \
	} else \
	    neg = 0; \
    }

#define ADDCIIFACEID(opt, neg, val1) \
    if (neg) { \
	int idlen = CILEN_IFACEID; \
	if (len >= idlen) { \
	    PUTCHAR(opt, ucp); \
	    PUTCHAR(idlen, ucp); \
	    eui64_put(val1, ucp); \
	    len -= idlen; \
	} else \
	    neg = 0; \
    }

    ADDCIIFACEID(CI_IFACEID, go->neg_ifaceid, go->ourid);

    ADDCIVJ(CI_COMPRESSTYPE, go->neg_vj, go->vj_protocol);

    *lenp -= len;
}


static int
ipv6cp_ackci(f, p, len)
    fsm *f;
    u_char *p;
    int len;
{
    ipv6cp_options *go = &ipv6cp_gotoptions[f->unit];
    u_short cilen, citype, cishort;
    eui64_t ifaceid;


#define ACKCIVJ(opt, neg, val) \
    if (neg) { \
	int vjlen = CILEN_COMPRESS; \
	if ((len -= vjlen) < 0) \
	    goto bad; \
	GETCHAR(citype, p); \
	GETCHAR(cilen, p); \
	if (cilen != vjlen || \
	    citype != opt)  \
	    goto bad; \
	GETSHORT(cishort, p); \
	if (cishort != val) \
	    goto bad; \
    }

#define ACKCIIFACEID(opt, neg, val1) \
    if (neg) { \
	int idlen = CILEN_IFACEID; \
	if ((len -= idlen) < 0) \
	    goto bad; \
	GETCHAR(citype, p); \
	GETCHAR(cilen, p); \
	if (cilen != idlen || \
	    citype != opt) \
	    goto bad; \
	eui64_get(ifaceid, p); \
	if (! eui64_equals(val1, ifaceid)) \
	    goto bad; \
    }

    ACKCIIFACEID(CI_IFACEID, go->neg_ifaceid, go->ourid);

    ACKCIVJ(CI_COMPRESSTYPE, go->neg_vj, go->vj_protocol);

    if (len != 0)
	goto bad;
    return (1);

bad:
    IPV6CPDEBUG(("ipv6cp_ackci: received bad Ack!"));
    return (0);
}

static int
ipv6cp_nakci(f, p, len, treat_as_reject)
    fsm *f;
    u_char *p;
    int len;
    int treat_as_reject;
{
    ipv6cp_options *go = &ipv6cp_gotoptions[f->unit];
    u_char citype, cilen, *next;
    u_short cishort;
    eui64_t ifaceid;
    ipv6cp_options no;		
    ipv6cp_options try;		

    BZERO(&no, sizeof(no));
    try = *go;

#define NAKCIIFACEID(opt, neg, code) \
    if (go->neg && \
	len >= (cilen = CILEN_IFACEID) && \
	p[1] == cilen && \
	p[0] == opt) { \
	len -= cilen; \
	INCPTR(2, p); \
	eui64_get(ifaceid, p); \
	no.neg = 1; \
	code \
    }

#define NAKCIVJ(opt, neg, code) \
    if (go->neg && \
	((cilen = p[1]) == CILEN_COMPRESS) && \
	len >= cilen && \
	p[0] == opt) { \
	len -= cilen; \
	INCPTR(2, p); \
	GETSHORT(cishort, p); \
	no.neg = 1; \
        code \
    }

    NAKCIIFACEID(CI_IFACEID, neg_ifaceid,
		 if (treat_as_reject) {
		     try.neg_ifaceid = 0;
		 } else if (go->accept_local) {
		     while (eui64_iszero(ifaceid) || 
			    eui64_equals(ifaceid, go->hisid)) 
			 eui64_magic(ifaceid);
		     try.ourid = ifaceid;
		     IPV6CPDEBUG(("local LL address %s", llv6_ntoa(ifaceid)));
		 }
		 );

#ifdef IPV6CP_COMP
    NAKCIVJ(CI_COMPRESSTYPE, neg_vj,
	    {
		if (cishort == IPV6CP_COMP && !treat_as_reject) {
		    try.vj_protocol = cishort;
		} else {
		    try.neg_vj = 0;
		}
	    }
	    );
#else
    NAKCIVJ(CI_COMPRESSTYPE, neg_vj,
	    {
		try.neg_vj = 0;
	    }
	    );
#endif

    while (len >= CILEN_VOID) {
	GETCHAR(citype, p);
	GETCHAR(cilen, p);
	if ( cilen < CILEN_VOID || (len -= cilen) < 0 )
	    goto bad;
	next = p + cilen - 2;

	switch (citype) {
	case CI_COMPRESSTYPE:
	    if (go->neg_vj || no.neg_vj ||
		(cilen != CILEN_COMPRESS))
		goto bad;
	    no.neg_vj = 1;
	    break;
	case CI_IFACEID:
	    if (go->neg_ifaceid || no.neg_ifaceid || cilen != CILEN_IFACEID)
		goto bad;
	    try.neg_ifaceid = 1;
	    eui64_get(ifaceid, p);
	    if (go->accept_local) {
		while (eui64_iszero(ifaceid) || 
		       eui64_equals(ifaceid, go->hisid)) 
		    eui64_magic(ifaceid);
		try.ourid = ifaceid;
	    }
	    no.neg_ifaceid = 1;
	    break;
	}
	p = next;
    }

    
    if (len != 0)
	goto bad;

    if (f->state != OPENED)
	*go = try;

    return 1;

bad:
    IPV6CPDEBUG(("ipv6cp_nakci: received bad Nak!"));
    return 0;
}


static int
ipv6cp_rejci(f, p, len)
    fsm *f;
    u_char *p;
    int len;
{
    ipv6cp_options *go = &ipv6cp_gotoptions[f->unit];
    u_char cilen;
    u_short cishort;
    eui64_t ifaceid;
    ipv6cp_options try;		

    try = *go;
#define REJCIIFACEID(opt, neg, val1) \
    if (go->neg && \
	len >= (cilen = CILEN_IFACEID) && \
	p[1] == cilen && \
	p[0] == opt) { \
	len -= cilen; \
	INCPTR(2, p); \
	eui64_get(ifaceid, p); \
	 \
	if (! eui64_equals(ifaceid, val1)) \
	    goto bad; \
	try.neg = 0; \
    }

#define REJCIVJ(opt, neg, val) \
    if (go->neg && \
	p[1] == CILEN_COMPRESS && \
	len >= p[1] && \
	p[0] == opt) { \
	len -= p[1]; \
	INCPTR(2, p); \
	GETSHORT(cishort, p); \
	  \
	if (cishort != val) \
	    goto bad; \
	try.neg = 0; \
     }

    REJCIIFACEID(CI_IFACEID, neg_ifaceid, go->ourid);

    REJCIVJ(CI_COMPRESSTYPE, neg_vj, go->vj_protocol);

    if (len != 0)
	goto bad;
    if (f->state != OPENED)
	*go = try;
    return 1;

bad:
    IPV6CPDEBUG(("ipv6cp_rejci: received bad Reject!"));
    return 0;
}


static int
ipv6cp_reqci(f, inp, len, reject_if_disagree)
    fsm *f;
    u_char *inp;		
    int *len;			
    int reject_if_disagree;
{
    ipv6cp_options *wo = &ipv6cp_wantoptions[f->unit];
    ipv6cp_options *ho = &ipv6cp_hisoptions[f->unit];
    ipv6cp_options *ao = &ipv6cp_allowoptions[f->unit];
    ipv6cp_options *go = &ipv6cp_gotoptions[f->unit];
    u_char *cip, *next;		
    u_short cilen, citype;	
    u_short cishort;		
    eui64_t ifaceid;		
    int rc = CONFACK;		
    int orc;			
    u_char *p;			
    u_char *ucp = inp;		
    int l = *len;		

    BZERO(ho, sizeof(*ho));
    
    next = inp;
    while (l) {
	orc = CONFACK;			
	cip = p = next;			
	if (l < 2 ||			
	    p[1] < 2 ||			
	    p[1] > l) {			
	    IPV6CPDEBUG(("ipv6cp_reqci: bad CI length!"));
	    orc = CONFREJ;		
	    cilen = l;			
	    l = 0;			
	    goto endswitch;
	}
	GETCHAR(citype, p);		
	GETCHAR(cilen, p);		
	l -= cilen;			
	next += cilen;			

	switch (citype) {		
	case CI_IFACEID:
	    IPV6CPDEBUG(("ipv6cp: received interface identifier "));

	    if (!ao->neg_ifaceid ||
		cilen != CILEN_IFACEID) {	
		orc = CONFREJ;		
		break;
	    }

	    eui64_get(ifaceid, p);
	    IPV6CPDEBUG(("(%s)", llv6_ntoa(ifaceid)));
	    if (eui64_iszero(ifaceid) && eui64_iszero(go->ourid)) {
		orc = CONFREJ;		
		break;
	    }
	    if (!eui64_iszero(wo->hisid) && 
		!eui64_equals(ifaceid, wo->hisid) && 
		eui64_iszero(go->hisid)) {
		    
		orc = CONFNAK;
		ifaceid = wo->hisid;
		go->hisid = ifaceid;
		DECPTR(sizeof(ifaceid), p);
		eui64_put(ifaceid, p);
	    } else
	    if (eui64_iszero(ifaceid) || eui64_equals(ifaceid, go->ourid)) {
		orc = CONFNAK;
		if (eui64_iszero(go->hisid))	
		    ifaceid = wo->hisid;
		while (eui64_iszero(ifaceid) || 
		       eui64_equals(ifaceid, go->ourid)) 
		    eui64_magic(ifaceid);
		go->hisid = ifaceid;
		DECPTR(sizeof(ifaceid), p);
		eui64_put(ifaceid, p);
	    }

	    ho->neg_ifaceid = 1;
	    ho->hisid = ifaceid;
	    break;

	case CI_COMPRESSTYPE:
	    IPV6CPDEBUG(("ipv6cp: received COMPRESSTYPE "));
	    if (!ao->neg_vj ||
		(cilen != CILEN_COMPRESS)) {
		orc = CONFREJ;
		break;
	    }
	    GETSHORT(cishort, p);
	    IPV6CPDEBUG(("(%d)", cishort));

#ifdef IPV6CP_COMP
	    if (!(cishort == IPV6CP_COMP)) {
		orc = CONFREJ;
		break;
	    }

	    ho->neg_vj = 1;
	    ho->vj_protocol = cishort;
	    break;
#else
	    orc = CONFREJ;
	    break;
#endif

	default:
	    orc = CONFREJ;
	    break;
	}

endswitch:
	IPV6CPDEBUG((" (%s)\n", CODENAME(orc)));

	if (orc == CONFACK &&		
	    rc != CONFACK)		
	    continue;			

	if (orc == CONFNAK) {		
	    if (reject_if_disagree)	
		orc = CONFREJ;		
	    else {
		if (rc == CONFREJ)	
		    continue;		
		if (rc == CONFACK) {	
		    rc = CONFNAK;	
		    ucp = inp;		
		}
	    }
	}

	if (orc == CONFREJ &&		
	    rc != CONFREJ) {		
	    rc = CONFREJ;
	    ucp = inp;			
	}

	
	if (ucp != cip)
	    BCOPY(cip, ucp, cilen);	

	
	INCPTR(cilen, ucp);
    }

    if (rc != CONFREJ && !ho->neg_ifaceid &&
	wo->req_ifaceid && !reject_if_disagree) {
	if (rc == CONFACK) {
	    rc = CONFNAK;
	    ucp = inp;				
	    wo->req_ifaceid = 0;		
	}
	PUTCHAR(CI_IFACEID, ucp);
	PUTCHAR(CILEN_IFACEID, ucp);
	eui64_put(wo->hisid, ucp);
    }

    *len = ucp - inp;			
    IPV6CPDEBUG(("ipv6cp: returning Configure-%s", CODENAME(rc)));
    return (rc);			
}


static void
ipv6_check_options()
{
    ipv6cp_options *wo = &ipv6cp_wantoptions[0];

    if (!ipv6cp_protent.enabled_flag)
	return;

    if ((wo->use_persistent) && (!wo->opt_local) && (!wo->opt_remote)) {

	if (ether_to_eui64(&wo->ourid)) {
	    wo->opt_local = 1;
	}
    }

    if (!wo->opt_local) {	
	if (wo->use_ip && eui64_iszero(wo->ourid)) {
	    eui64_setlo32(wo->ourid, ntohl(ipcp_wantoptions[0].ouraddr));
	    if (!eui64_iszero(wo->ourid))
		wo->opt_local = 1;
	}
	
	while (eui64_iszero(wo->ourid))
	    eui64_magic(wo->ourid);
    }

    if (!wo->opt_remote) {
	if (wo->use_ip && eui64_iszero(wo->hisid)) {
	    eui64_setlo32(wo->hisid, ntohl(ipcp_wantoptions[0].hisaddr));
	    if (!eui64_iszero(wo->hisid))
		wo->opt_remote = 1;
	}
    }

    if (demand && (eui64_iszero(wo->ourid) || eui64_iszero(wo->hisid))) {
	option_error("local/remote LL address required for demand-dialling\n");
	exit(EXIT_OPTION_ERROR);
    }
}


static int
ipv6_demand_conf(u)
    int u;
{
    ipv6cp_options *wo = &ipv6cp_wantoptions[u];

    if (!sif6up(u))
	return 0;
    if (!sif6addr(u, wo->ourid, wo->hisid))
	return 0;
#if !defined(__linux__) && !(defined(SVR4) && (defined(SNI) || defined(__USLC__)))
    if (!sifup(u))
	return 0;
#endif
    if (!sifnpmode(u, PPP_IPV6, NPMODE_QUEUE))
	return 0;

    notice("ipv6_demand_conf");
    notice("local  LL address %s", llv6_ntoa(wo->ourid));
    notice("remote LL address %s", llv6_ntoa(wo->hisid));

    return 1;
}


static void
ipv6cp_up(f)
    fsm *f;
{
    ipv6cp_options *ho = &ipv6cp_hisoptions[f->unit];
    ipv6cp_options *go = &ipv6cp_gotoptions[f->unit];
    ipv6cp_options *wo = &ipv6cp_wantoptions[f->unit];

    IPV6CPDEBUG(("ipv6cp: up"));

    if (!ho->neg_ifaceid)
	ho->hisid = wo->hisid;

    if(!no_ifaceid_neg) {
	if (eui64_iszero(ho->hisid)) {
	    error("Could not determine remote LL address");
	    ipv6cp_close(f->unit, "Could not determine remote LL address");
	    return;
	}
	if (eui64_iszero(go->ourid)) {
	    error("Could not determine local LL address");
	    ipv6cp_close(f->unit, "Could not determine local LL address");
	    return;
	}
	if (eui64_equals(go->ourid, ho->hisid)) {
	    error("local and remote LL addresses are equal");
	    ipv6cp_close(f->unit, "local and remote LL addresses are equal");
	    return;
	}
    }
    script_setenv("LLLOCAL", llv6_ntoa(go->ourid), 0);
    script_setenv("LLREMOTE", llv6_ntoa(ho->hisid), 0);

#ifdef IPV6CP_COMP
    
    sif6comp(f->unit, ho->neg_vj);
#endif

    if (demand) {
	if (! eui64_equals(go->ourid, wo->ourid) || 
	    ! eui64_equals(ho->hisid, wo->hisid)) {
	    if (! eui64_equals(go->ourid, wo->ourid))
		warn("Local LL address changed to %s", 
		     llv6_ntoa(go->ourid));
	    if (! eui64_equals(ho->hisid, wo->hisid))
		warn("Remote LL address changed to %s", 
		     llv6_ntoa(ho->hisid));
	    ipv6cp_clear_addrs(f->unit, go->ourid, ho->hisid);

	    
	    if (!sif6addr(f->unit, go->ourid, ho->hisid)) {
		if (debug)
		    warn("sif6addr failed");
		ipv6cp_close(f->unit, "Interface configuration failed");
		return;
	    }

	}
	demand_rexmit(PPP_IPV6);
	sifnpmode(f->unit, PPP_IPV6, NPMODE_PASS);

    } else {
	
	if (!sif6up(f->unit)) {
	    if (debug)
		warn("sif6up failed (IPV6)");
	    ipv6cp_close(f->unit, "Interface configuration failed");
	    return;
	}

	if (!sif6addr(f->unit, go->ourid, ho->hisid)) {
	    if (debug)
		warn("sif6addr failed");
	    ipv6cp_close(f->unit, "Interface configuration failed");
	    return;
	}
	sifnpmode(f->unit, PPP_IPV6, NPMODE_PASS);

	notice("local  LL address %s", llv6_ntoa(go->ourid));
	notice("remote LL address %s", llv6_ntoa(ho->hisid));
    }

    np_up(f->unit, PPP_IPV6);
    ipv6cp_is_up = 1;

    notify(ipv6_up_notifier, 0);
    if (ipv6_up_hook)
       ipv6_up_hook();

    if (ipv6cp_script_state == s_down && ipv6cp_script_pid == 0) {
	ipv6cp_script_state = s_up;
	ipv6cp_script(_PATH_IPV6UP);
    }
}


static void
ipv6cp_down(f)
    fsm *f;
{
    IPV6CPDEBUG(("ipv6cp: down"));
    update_link_stats(f->unit);
    notify(ipv6_down_notifier, 0);
    if (ipv6_down_hook)
       ipv6_down_hook();
    if (ipv6cp_is_up) {
	ipv6cp_is_up = 0;
	np_down(f->unit, PPP_IPV6);
    }
#ifdef IPV6CP_COMP
    sif6comp(f->unit, 0);
#endif

    if (demand) {
	sifnpmode(f->unit, PPP_IPV6, NPMODE_QUEUE);
    } else {
	sifnpmode(f->unit, PPP_IPV6, NPMODE_DROP);
#if !defined(__linux__) && !(defined(SVR4) && (defined(SNI) || defined(__USLC)))
	sif6down(f->unit);
#endif
	ipv6cp_clear_addrs(f->unit, 
			   ipv6cp_gotoptions[f->unit].ourid,
			   ipv6cp_hisoptions[f->unit].hisid);
#if defined(__linux__)
	sif6down(f->unit);
#elif defined(SVR4) && (defined(SNI) || defined(__USLC))
	sifdown(f->unit);
#endif
    }

    
    if (ipv6cp_script_state == s_up && ipv6cp_script_pid == 0) {
	ipv6cp_script_state = s_down;
	ipv6cp_script(_PATH_IPV6DOWN);
    }
}


static void
ipv6cp_clear_addrs(unit, ourid, hisid)
    int unit;
    eui64_t ourid;
    eui64_t hisid;
{
    cif6addr(unit, ourid, hisid);
}


static void
ipv6cp_finished(f)
    fsm *f;
{
    np_finished(f->unit, PPP_IPV6);
}


static void
ipv6cp_script_done(arg)
    void *arg;
{
    ipv6cp_script_pid = 0;
    switch (ipv6cp_script_state) {
    case s_up:
	if (ipv6cp_fsm[0].state != OPENED) {
	    ipv6cp_script_state = s_down;
	    ipv6cp_script(_PATH_IPV6DOWN);
	}
	break;
    case s_down:
	if (ipv6cp_fsm[0].state == OPENED) {
	    ipv6cp_script_state = s_up;
	    ipv6cp_script(_PATH_IPV6UP);
	}
	break;
    }
}


static void
ipv6cp_script(script)
    char *script;
{
    char strspeed[32], strlocal[32], strremote[32];
    char *argv[8];

    sprintf(strspeed, "%d", baud_rate);
    strcpy(strlocal, llv6_ntoa(ipv6cp_gotoptions[0].ourid));
    strcpy(strremote, llv6_ntoa(ipv6cp_hisoptions[0].hisid));

    argv[0] = script;
    argv[1] = ifname;
    argv[2] = devnam;
    argv[3] = strspeed;
    argv[4] = strlocal;
    argv[5] = strremote;
    argv[6] = ipparam;
    argv[7] = NULL;

    ipv6cp_script_pid = run_program(script, argv, 0, ipv6cp_script_done,
				    NULL, 0);
}

static char *ipv6cp_codenames[] = {
    "ConfReq", "ConfAck", "ConfNak", "ConfRej",
    "TermReq", "TermAck", "CodeRej"
};

static int
ipv6cp_printpkt(p, plen, printer, arg)
    u_char *p;
    int plen;
    void (*printer) __P((void *, char *, ...));
    void *arg;
{
    int code, id, len, olen;
    u_char *pstart, *optend;
    u_short cishort;
    eui64_t ifaceid;

    if (plen < HEADERLEN)
	return 0;
    pstart = p;
    GETCHAR(code, p);
    GETCHAR(id, p);
    GETSHORT(len, p);
    if (len < HEADERLEN || len > plen)
	return 0;

    if (code >= 1 && code <= sizeof(ipv6cp_codenames) / sizeof(char *))
	printer(arg, " %s", ipv6cp_codenames[code-1]);
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
	    case CI_COMPRESSTYPE:
		if (olen >= CILEN_COMPRESS) {
		    p += 2;
		    GETSHORT(cishort, p);
		    printer(arg, "compress ");
		    printer(arg, "0x%x", cishort);
		}
		break;
	    case CI_IFACEID:
		if (olen == CILEN_IFACEID) {
		    p += 2;
		    eui64_get(ifaceid, p);
		    printer(arg, "addr %s", llv6_ntoa(ifaceid));
		}
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
    }

    
    for (; len > 0; --len) {
	GETCHAR(code, p);
	printer(arg, " %.2x", code);
    }

    return p - pstart;
}

#define IP6_HDRLEN	40	
#define IP6_NHDR_FRAG	44	
#define TCP_HDRLEN	20
#define TH_FIN		0x01


#define get_ip6nh(x)	(((unsigned char *)(x))[6])
#define get_tcpoff(x)	(((unsigned char *)(x))[12] >> 4)
#define get_tcpflags(x)	(((unsigned char *)(x))[13])

static int
ipv6_active_pkt(pkt, len)
    u_char *pkt;
    int len;
{
    u_char *tcp;

    len -= PPP_HDRLEN;
    pkt += PPP_HDRLEN;
    if (len < IP6_HDRLEN)
	return 0;
    if (get_ip6nh(pkt) == IP6_NHDR_FRAG)
	return 0;
    if (get_ip6nh(pkt) != IPPROTO_TCP)
	return 1;
    if (len < IP6_HDRLEN + TCP_HDRLEN)
	return 0;
    tcp = pkt + IP6_HDRLEN;
    if ((get_tcpflags(tcp) & TH_FIN) != 0 && len == IP6_HDRLEN + get_tcpoff(tcp) * 4)
	return 0;
    return 1;
}
