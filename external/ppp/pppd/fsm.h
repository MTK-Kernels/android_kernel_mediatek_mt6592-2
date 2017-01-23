/*
 * fsm.h - {Link, IP} Control Protocol Finite State Machine definitions.
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
 * $Id: fsm.h,v 1.10 2004/11/13 02:28:15 paulus Exp $
 */

#define HEADERLEN	4


#define CONFREQ		1	
#define CONFACK		2	
#define CONFNAK		3	
#define CONFREJ		4	
#define TERMREQ		5	
#define TERMACK		6	
#define CODEREJ		7	


typedef struct fsm {
    int unit;			
    int protocol;		
    int state;			
    int flags;			
    u_char id;			
    u_char reqid;		
    u_char seen_ack;		
    int timeouttime;		
    int maxconfreqtransmits;	
    int retransmits;		
    int maxtermtransmits;	
    int nakloops;		
    int rnakloops;		
    int maxnakloops;		
    struct fsm_callbacks *callbacks;	
    char *term_reason;		
    int term_reason_len;	
} fsm;


typedef struct fsm_callbacks {
    void (*resetci)		
		__P((fsm *));
    int  (*cilen)		
		__P((fsm *));
    void (*addci) 		
		__P((fsm *, u_char *, int *));
    int  (*ackci)		
		__P((fsm *, u_char *, int));
    int  (*nakci)		
		__P((fsm *, u_char *, int, int));
    int  (*rejci)		
		__P((fsm *, u_char *, int));
    int  (*reqci)		
		__P((fsm *, u_char *, int *, int));
    void (*up)			
		__P((fsm *));
    void (*down)		
		__P((fsm *));
    void (*starting)		
		__P((fsm *));
    void (*finished)		
		__P((fsm *));
    void (*protreject)		
		__P((int));
    void (*retransmit)		
		__P((fsm *));
    int  (*extcode)		
		__P((fsm *, int, int, u_char *, int));
    char *proto_name;		
} fsm_callbacks;


#define INITIAL		0	
#define STARTING	1	
#define CLOSED		2	
#define STOPPED		3	
#define CLOSING		4	
#define STOPPING	5	
#define REQSENT		6	
#define ACKRCVD		7	
#define ACKSENT		8	
#define OPENED		9	


#define OPT_PASSIVE	1	
#define OPT_RESTART	2	
#define OPT_SILENT	4	


#define DEFTIMEOUT	3	
#define DEFMAXTERMREQS	2	
#define DEFMAXCONFREQS	10	
#define DEFMAXNAKLOOPS	5	


void fsm_init __P((fsm *));
void fsm_lowerup __P((fsm *));
void fsm_lowerdown __P((fsm *));
void fsm_open __P((fsm *));
void fsm_close __P((fsm *, char *));
void fsm_input __P((fsm *, u_char *, int));
void fsm_protreject __P((fsm *));
void fsm_sdata __P((fsm *, int, int, u_char *, int));


extern int peer_mru[];		
