/*
 * lcp.h - Link Control Protocol definitions.
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
 * $Id: lcp.h,v 1.20 2004/11/14 22:53:42 carlsonj Exp $
 */

#define CI_VENDOR	0	
#define CI_MRU		1	
#define CI_ASYNCMAP	2	
#define CI_AUTHTYPE	3	
#define CI_QUALITY	4	
#define CI_MAGICNUMBER	5	
#define CI_PCOMPRESSION	7	
#define CI_ACCOMPRESSION 8	
#define CI_FCSALTERN	9	
#define CI_SDP		10	
#define CI_NUMBERED	11	
#define CI_CALLBACK	13	
#define CI_MRRU		17	
#define CI_SSNHF	18	
#define CI_EPDISC	19	
#define CI_MPPLUS	22	
#define CI_LDISC	23	
#define CI_LCPAUTH	24	
#define CI_COBS		25	
#define CI_PREFELIS	26	
#define CI_MPHDRFMT	27	
#define CI_I18N		28	
#define CI_SDL		29	

#define PROTREJ		8	
#define ECHOREQ		9	
#define ECHOREP		10	
#define DISCREQ		11	
#define IDENTIF		12	
#define TIMEREM		13	

#define CBCP_OPT	6	

typedef struct lcp_options {
    bool passive;		
    bool silent;		
    bool restart;		
    bool neg_mru;		
    bool neg_asyncmap;		
    bool neg_upap;		
    bool neg_chap;		
    bool neg_eap;		
    bool neg_magicnumber;	
    bool neg_pcompression;	
    bool neg_accompression;	
    bool neg_lqr;		
    bool neg_cbcp;		
    bool neg_mrru;		
    bool neg_ssnhf;		
    bool neg_endpoint;		
    int  mru;			
    int	 mrru;			
    u_char chap_mdtype;		
    u_int32_t asyncmap;		
    u_int32_t magicnumber;
    int  numloops;		
    u_int32_t lqr_period;	
    struct epdisc endpoint;	
} lcp_options;

extern fsm lcp_fsm[];
extern lcp_options lcp_wantoptions[];
extern lcp_options lcp_gotoptions[];
extern lcp_options lcp_allowoptions[];
extern lcp_options lcp_hisoptions[];

#define DEFMRU	1500		
#define MINMRU	128		
#define MAXMRU	16384		

void lcp_open __P((int));
void lcp_close __P((int, char *));
void lcp_lowerup __P((int));
void lcp_lowerdown __P((int));
void lcp_sprotrej __P((int, u_char *, int));	

extern struct protent lcp_protent;

#define DEFLOOPBACKFAIL	10
