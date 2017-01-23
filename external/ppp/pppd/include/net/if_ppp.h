
/*
 * if_ppp.h - Point-to-Point Protocol definitions.
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

#ifndef _IF_PPP_H_
#define _IF_PPP_H_

#define SC_COMP_PROT	0x00000001	
#define SC_COMP_AC	0x00000002	
#define	SC_COMP_TCP	0x00000004	
#define SC_NO_TCP_CCID	0x00000008	
#define SC_REJ_COMP_AC	0x00000010	
#define SC_REJ_COMP_TCP	0x00000020	
#define SC_CCP_OPEN	0x00000040	
#define SC_CCP_UP	0x00000080	
#define SC_DEBUG	0x00010000	
#define SC_LOG_INPKT	0x00020000	
#define SC_LOG_OUTPKT	0x00040000	
#define SC_LOG_RAWIN	0x00080000	
#define SC_LOG_FLUSH	0x00100000	
#define SC_RCV_B7_0	0x01000000	
#define SC_RCV_B7_1	0x02000000	
#define SC_RCV_EVNP	0x04000000	
#define SC_RCV_ODDP	0x08000000	
#define SC_SYNC		0x00200000	
#define	SC_MASK		0x0fff00ff	

#define SC_TIMEOUT	0x00000400	
#define SC_VJ_RESET	0x00000800	
#define SC_COMP_RUN	0x00001000	
#define SC_DECOMP_RUN	0x00002000	
#define SC_DC_ERROR	0x00004000	
#define SC_DC_FERROR	0x00008000	
#define SC_TBUSY	0x10000000	
#define SC_PKTLOST	0x20000000	
#define	SC_FLUSH	0x40000000	
#define	SC_ESCAPED	0x80000000	


struct npioctl {
    int		protocol;	
    enum NPmode	mode;
};

struct ppp_option_data {
	u_char	*ptr;
	u_int	length;
	int	transmit;
};

struct ifpppstatsreq {
    char ifr_name[IFNAMSIZ];
    struct ppp_stats stats;
};

struct ifpppcstatsreq {
    char ifr_name[IFNAMSIZ];
    struct ppp_comp_stats stats;
};


#define	PPPIOCGFLAGS	_IOR('t', 90, int)	
#define	PPPIOCSFLAGS	_IOW('t', 89, int)	
#define	PPPIOCGASYNCMAP	_IOR('t', 88, int)	
#define	PPPIOCSASYNCMAP	_IOW('t', 87, int)	
#define	PPPIOCGUNIT	_IOR('t', 86, int)	
#define	PPPIOCGRASYNCMAP _IOR('t', 85, int)	
#define	PPPIOCSRASYNCMAP _IOW('t', 84, int)	
#define	PPPIOCGMRU	_IOR('t', 83, int)	
#define	PPPIOCSMRU	_IOW('t', 82, int)	
#define	PPPIOCSMAXCID	_IOW('t', 81, int)	
#define PPPIOCGXASYNCMAP _IOR('t', 80, ext_accm) 
#define PPPIOCSXASYNCMAP _IOW('t', 79, ext_accm) 
#define PPPIOCXFERUNIT	_IO('t', 78)		
#define PPPIOCSCOMPRESS	_IOW('t', 77, struct ppp_option_data)
#define PPPIOCGNPMODE	_IOWR('t', 76, struct npioctl) 
#define PPPIOCSNPMODE	_IOW('t', 75, struct npioctl)  
#define PPPIOCGIDLE	_IOR('t', 74, struct ppp_idle) 
#ifdef PPP_FILTER
#define PPPIOCSPASS	_IOW('t', 71, struct bpf_program) 
#define PPPIOCSACTIVE	_IOW('t', 70, struct bpf_program) 
#endif 

#define PPPIOCGMTU	_IOR('t', 73, int)	
#define PPPIOCSMTU	_IOW('t', 72, int)	

#define SIOCGPPPSTATS	_IOWR('i', 123, struct ifpppstatsreq)
#define SIOCGPPPCSTATS	_IOWR('i', 122, struct ifpppcstatsreq)

#if !defined(ifr_mtu)
#define ifr_mtu	ifr_ifru.ifru_metric
#endif

#if (defined(_KERNEL) || defined(KERNEL)) && !defined(NeXT)
void pppattach __P((void));
void pppintr __P((void));
#endif
#endif 
