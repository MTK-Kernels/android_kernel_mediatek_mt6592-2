/*
 * eap.h - Extensible Authentication Protocol for PPP (RFC 2284)
 *
 * Copyright (c) 2001 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * Non-exclusive rights to redistribute, modify, translate, and use
 * this software in source and binary forms, in whole or in part, is
 * hereby granted, provided that the above copyright notice is
 * duplicated in any source form, and that neither the name of the
 * copyright holder nor the author is used to endorse or promote
 * products derived from this software.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Original version by James Carlson
 *
 * $Id: eap.h,v 1.2 2003/06/11 23:56:26 paulus Exp $
 */

#ifndef PPP_EAP_H
#define	PPP_EAP_H

#ifdef	__cplusplus
extern "C" {
#endif

#define	EAP_HEADERLEN	4


#define	EAP_REQUEST	1
#define	EAP_RESPONSE	2
#define	EAP_SUCCESS	3
#define	EAP_FAILURE	4

#define	EAPT_IDENTITY		1
#define	EAPT_NOTIFICATION	2
#define	EAPT_NAK		3	
#define	EAPT_MD5CHAP		4
#define	EAPT_OTP		5	
#define	EAPT_TOKEN		6	
#define	EAPT_RSA		9	
#define	EAPT_DSS		10	
#define	EAPT_KEA		11	
#define	EAPT_KEA_VALIDATE	12	
#define	EAPT_TLS		13	
#define	EAPT_DEFENDER		14	
#define	EAPT_W2K		15	
#define	EAPT_ARCOT		16	
#define	EAPT_CISCOWIRELESS	17	
#define	EAPT_NOKIACARD		18	
#define	EAPT_SRP		19	

#define	EAPSRP_CHALLENGE	1	
#define	EAPSRP_CKEY		1	
#define	EAPSRP_SKEY		2	
#define	EAPSRP_CVALIDATOR	2	
#define	EAPSRP_SVALIDATOR	3	
#define	EAPSRP_ACK		3	
#define	EAPSRP_LWRECHALLENGE	4	

#define	SRPVAL_EBIT	0x00000001	

#define	SRP_PSEUDO_ID	"pseudo_"
#define	SRP_PSEUDO_LEN	7

#define MD5_SIGNATURE_SIZE	16
#define MIN_CHALLENGE_LENGTH	16
#define MAX_CHALLENGE_LENGTH	24

enum eap_state_code {
	eapInitial = 0,	
	eapPending,	
	eapClosed,	
	eapListen,	
	eapIdentify,	
	eapSRP1,	
	eapSRP2,	
	eapSRP3,	
	eapMD5Chall,	
	eapOpen,	
	eapSRP4,	
	eapBadAuth	
};

#define	EAP_STATES	\
	"Initial", "Pending", "Closed", "Listen", "Identify", \
	"SRP1", "SRP2", "SRP3", "MD5Chall", "Open", "SRP4", "BadAuth"

#define	eap_client_active(esp)	((esp)->es_client.ea_state == eapListen)
#define	eap_server_active(esp)	\
	((esp)->es_server.ea_state >= eapIdentify && \
	 (esp)->es_server.ea_state <= eapMD5Chall)

struct eap_auth {
	char *ea_name;		
	char *ea_peer;		
	void *ea_session;	
	u_char *ea_skey;	
	int ea_timeout;		
	int ea_maxrequests;	
	u_short ea_namelen;	
	u_short ea_peerlen;	
	enum eap_state_code ea_state;
	u_char ea_id;		
	u_char ea_requests;	
	u_char ea_responses;	
	u_char ea_type;		
	u_int32_t ea_keyflags;	
};

typedef struct eap_state {
	int es_unit;			
	struct eap_auth es_client;	
	struct eap_auth es_server;	
	int es_savedtime;		
	int es_rechallenge;		
	int es_lwrechallenge;		
	bool es_usepseudo;		
	int es_usedpseudo;		
	int es_challen;			
	u_char es_challenge[MAX_CHALLENGE_LENGTH];
} eap_state;

#define	EAP_DEFTIMEOUT		3	
#define	EAP_DEFTRANSMITS	10	
#define	EAP_DEFREQTIME		20	
#define	EAP_DEFALLOWREQ		20	

extern eap_state eap_states[];

void eap_authwithpeer __P((int unit, char *localname));
void eap_authpeer __P((int unit, char *localname));

extern struct protent eap_protent;

#ifdef	__cplusplus
}
#endif

#endif 

