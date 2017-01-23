/*
 * Copyright (c) 1982, 1986, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)udp.h	8.1 (Berkeley) 6/10/93
 */

struct udphdr {
	u_int16_t	uh_sport;		
	u_int16_t	uh_dport;		
	u_int16_t	uh_ulen;		
	u_int16_t	uh_sum;			
};

#define TFTP_PORT 69		
#define KERBEROS_PORT 88	
#define SUNRPC_PORT 111		
#define SNMP_PORT 161		
#define NTP_PORT 123		
#define SNMPTRAP_PORT 162	
#define ISAKMP_PORT 500		
#define SYSLOG_PORT 514         
#define TIMED_PORT 525		
#define RIP_PORT 520		
#define LDP_PORT 646
#define AODV_PORT 654		
#define OLSR_PORT 698           
#define KERBEROS_SEC_PORT 750	
#define L2TP_PORT 1701		
#define SIP_PORT 5060
#define ISAKMP_PORT_NATT  4500  
#define ISAKMP_PORT_USER1 7500	
#define ISAKMP_PORT_USER2 8500	
#define RX_PORT_LOW 7000	
#define RX_PORT_HIGH 7009	
#define NETBIOS_NS_PORT   137
#define NETBIOS_DGRAM_PORT   138
#define CISCO_AUTORP_PORT 496	
#define RADIUS_PORT 1645
#define RADIUS_NEW_PORT 1812
#define RADIUS_ACCOUNTING_PORT 1646
#define RADIUS_NEW_ACCOUNTING_PORT 1813
#define HSRP_PORT 1985		
#define LMP_PORT                701 
#define LWRES_PORT		921
#define VQP_PORT		1589
#define ZEPHYR_SRV_PORT		2103
#define ZEPHYR_CLT_PORT		2104
#define VAT_PORT		3456
#define MPLS_LSP_PING_PORT      3503 
#define BFD_CONTROL_PORT        3784 
#define BFD_ECHO_PORT           3785 
#define WB_PORT			4567
#define SFLOW_PORT              6343 
#define LWAPP_DATA_PORT         12222 
#define LWAPP_CONTROL_PORT      12223 
#define OTV_PORT                8472  
#define VXLAN_PORT              4789  

#ifdef INET6
#define RIPNG_PORT 521		
#define DHCP6_SERV_PORT 546	
#define DHCP6_CLI_PORT 547	
#define BABEL_PORT 6696
#define BABEL_PORT_OLD 6697
#endif
