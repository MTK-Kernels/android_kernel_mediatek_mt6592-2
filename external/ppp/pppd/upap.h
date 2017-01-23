/*
 * upap.h - User/Password Authentication Protocol definitions.
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
 * $Id: upap.h,v 1.8 2002/12/04 23:03:33 paulus Exp $
 */

#define UPAP_HEADERLEN	4


#define UPAP_AUTHREQ	1	
#define UPAP_AUTHACK	2	
#define UPAP_AUTHNAK	3	


typedef struct upap_state {
    int us_unit;		
    char *us_user;		
    int us_userlen;		
    char *us_passwd;		
    int us_passwdlen;		
    int us_clientstate;		
    int us_serverstate;		
    u_char us_id;		
    int us_timeouttime;		
    int us_transmits;		
    int us_maxtransmits;	
    int us_reqtimeout;		
} upap_state;


#define UPAPCS_INITIAL	0	
#define UPAPCS_CLOSED	1	
#define UPAPCS_PENDING	2	
#define UPAPCS_AUTHREQ	3	
#define UPAPCS_OPEN	4	
#define UPAPCS_BADAUTH	5	

#define UPAPSS_INITIAL	0	
#define UPAPSS_CLOSED	1	
#define UPAPSS_PENDING	2	
#define UPAPSS_LISTEN	3	
#define UPAPSS_OPEN	4	
#define UPAPSS_BADAUTH	5	


#define UPAP_DEFTIMEOUT	3	
#define UPAP_DEFREQTIME	30	

extern upap_state upap[];

void upap_authwithpeer __P((int, char *, char *));
void upap_authpeer __P((int));

extern struct protent pap_protent;
