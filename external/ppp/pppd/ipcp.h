/*
 * ipcp.h - IP Control Protocol definitions.
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
 * $Id: ipcp.h,v 1.14 2002/12/04 23:03:32 paulus Exp $
 */

#define CI_ADDRS	1	
#define CI_COMPRESSTYPE	2	
#define	CI_ADDR		3

#define CI_MS_DNS1	129	
#define CI_MS_WINS1	130	
#define CI_MS_DNS2	131	
#define CI_MS_WINS2	132	

#define MAX_STATES 16		

#define IPCP_VJMODE_OLD 1	
#define IPCP_VJMODE_RFC1172 2	
#define IPCP_VJMODE_RFC1332 3	
                                

#define IPCP_VJ_COMP 0x002d	
#define IPCP_VJ_COMP_OLD 0x0037	
				 

typedef struct ipcp_options {
    bool neg_addr;		
    bool old_addrs;		
    bool req_addr;		
    bool default_route;		
    bool proxy_arp;		
    bool neg_vj;		
    bool old_vj;		
    bool accept_local;		
    bool accept_remote;		
    bool req_dns1;		
    bool req_dns2;		
    int  vj_protocol;		
    int  maxslotindex;		
    bool cflag;
    u_int32_t ouraddr, hisaddr;	
    u_int32_t dnsaddr[2];	
    u_int32_t winsaddr[2];	
} ipcp_options;

extern fsm ipcp_fsm[];
extern ipcp_options ipcp_wantoptions[];
extern ipcp_options ipcp_gotoptions[];
extern ipcp_options ipcp_allowoptions[];
extern ipcp_options ipcp_hisoptions[];

char *ip_ntoa __P((u_int32_t));

extern struct protent ipcp_protent;
