
/* Written 1996-1998 by Werner Almesberger, EPFL LRC/ICA */


#ifndef _ATMSAP_H
#define _ATMSAP_H

#include <stdint.h>
#include <linux/atmsap.h>



#define NLPID_IEEE802_1_SNAP	0x80	


#define ATM_FORUM_OUI		"\x00\xA0\x3E"	
#define EPFL_OUI		"\x00\x60\xD7"	


#define ANS_HLT_VS_ID		ATM_FORUM_OUI "\x00\x00\x00\x01"
					 
#define VOD_HLT_VS_ID		ATM_FORUM_OUI "\x00\x00\x00\x02"
						     
#define AREQUIPA_HLT_VS_ID	EPFL_OUI "\x01\x00\x00\x01"	
#define TTCP_HLT_VS_ID		EPFL_OUI "\x01\x00\x00\x03"	



void atm_tcpip_port_mapping(char *vs_id,uint8_t protocol,uint16_t port);

#endif
