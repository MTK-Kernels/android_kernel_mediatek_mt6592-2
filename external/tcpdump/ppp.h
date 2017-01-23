/*
 * Point to Point Protocol (PPP) RFC1331
 *
 * Copyright 1989 by Carnegie Mellon.
 *
 * Permission to use, copy, modify, and distribute this program for any
 * purpose and without fee is hereby granted, provided that this copyright
 * and permission notice appear on all copies and supporting documentation,
 * the name of Carnegie Mellon not be used in advertising or publicity
 * pertaining to distribution of the program without specific prior
 * permission, and notice be given in supporting documentation that copying
 * and distribution is by permission of Carnegie Mellon and Stanford
 * University.  Carnegie Mellon makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 */
#define PPP_HDRLEN	4	

#define PPP_ADDRESS	0xff	
#define PPP_CONTROL	0x03	

#define PPP_WITHDIRECTION_IN  0x00 
#define PPP_WITHDIRECTION_OUT 0x01 

#define PPP_IP		0x0021	
#define PPP_OSI		0x0023	
#define PPP_NS		0x0025	
#define PPP_DECNET	0x0027	
#define PPP_APPLE	0x0029	
#define PPP_IPX		0x002b	
#define PPP_VJC		0x002d	
#define PPP_VJNC	0x002f	
#define PPP_BRPDU	0x0031	
#define PPP_STII	0x0033	
#define PPP_VINES	0x0035	
#define PPP_ML          0x003d  
#define PPP_IPV6	0x0057	
#define	PPP_COMP	0x00fd	

#define PPP_HELLO	0x0201	
#define PPP_LUXCOM	0x0231	
#define PPP_SNS		0x0233	
#define PPP_MPLS_UCAST  0x0281  
#define PPP_MPLS_MCAST  0x0283  

#define PPP_IPCP	0x8021	
#define PPP_OSICP	0x8023	
#define PPP_NSCP	0x8025	
#define PPP_DECNETCP	0x8027	
#define PPP_APPLECP	0x8029	
#define PPP_IPXCP	0x802b	
#define PPP_STIICP	0x8033	
#define PPP_VINESCP	0x8035	
#define PPP_IPV6CP	0x8057	
#define PPP_CCP		0x80fd	
#define PPP_MPLSCP      0x8281  

#define PPP_LCP		0xc021	
#define PPP_PAP		0xc023	
#define PPP_LQM		0xc025	
#define PPP_SPAP        0xc027
#define PPP_CHAP	0xc223	
#define PPP_BACP	0xc02b	
#define PPP_BAP		0xc02d	
#define PPP_MPCP		0xc03d	
#define PPP_SPAP_OLD    0xc123
#define PPP_EAP         0xc227
