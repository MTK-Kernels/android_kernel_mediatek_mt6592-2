/**********************************************************************
*
* relay.h
*
* Definitions for PPPoE relay
*
* Copyright (C) 2001-2006 Roaring Penguin Software Inc.
*
* This program may be distributed according to the terms of the GNU
* General Public License, version 2 or (at your option) any later version.
*
* LIC: GPL
*
* $Id$
*
***********************************************************************/

#include "pppoe.h"

typedef struct InterfaceStruct {
    char name[IFNAMSIZ+1];	
    int discoverySock;		
    int sessionSock;		
    int clientOK;		
    int acOK;			
    unsigned char mac[ETH_ALEN]; 
} PPPoEInterface;

struct SessionHashStruct;
typedef struct SessionStruct {
    struct SessionStruct *next;	
    struct SessionStruct *prev;	
    struct SessionHashStruct *acHash; 
    struct SessionHashStruct *clientHash; 
    unsigned int epoch;		
    UINT16_t sesNum;		
} PPPoESession;

typedef struct SessionHashStruct {
    struct SessionHashStruct *next; 
    struct SessionHashStruct *prev; 
    struct SessionHashStruct *peer; 
    PPPoEInterface const *interface;	
    unsigned char peerMac[ETH_ALEN]; 
    UINT16_t sesNum;		
    PPPoESession *ses;		
} SessionHash;


void relayGotSessionPacket(PPPoEInterface const *i);
void relayGotDiscoveryPacket(PPPoEInterface const *i);
PPPoEInterface *findInterface(int sock);
unsigned int hash(unsigned char const *mac, UINT16_t sesNum);
SessionHash *findSession(unsigned char const *mac, UINT16_t sesNum);
void deleteHash(SessionHash *hash);
PPPoESession *createSession(PPPoEInterface const *ac,
			    PPPoEInterface const *cli,
			    unsigned char const *acMac,
			    unsigned char const *cliMac,
			    UINT16_t acSes);
void freeSession(PPPoESession *ses, char const *msg);
void addInterface(char const *ifname, int clientOK, int acOK);
void usage(char const *progname);
void initRelay(int nsess);
void relayLoop(void);
void addHash(SessionHash *sh);
void unhash(SessionHash *sh);

void relayHandlePADT(PPPoEInterface const *iface, PPPoEPacket *packet, int size);
void relayHandlePADI(PPPoEInterface const *iface, PPPoEPacket *packet, int size);
void relayHandlePADO(PPPoEInterface const *iface, PPPoEPacket *packet, int size);
void relayHandlePADR(PPPoEInterface const *iface, PPPoEPacket *packet, int size);
void relayHandlePADS(PPPoEInterface const *iface, PPPoEPacket *packet, int size);

int addTag(PPPoEPacket *packet, PPPoETag const *tag);
int insertBytes(PPPoEPacket *packet, unsigned char *loc,
		void const *bytes, int length);
int removeBytes(PPPoEPacket *packet, unsigned char *loc,
		int length);
void relaySendError(unsigned char code,
		    UINT16_t session,
		    PPPoEInterface const *iface,
		    unsigned char const *mac,
		    PPPoETag const *hostUniq,
		    char const *errMsg);

void alarmHandler(int sig);
void cleanSessions(void);

#define MAX_INTERFACES 8
#define DEFAULT_SESSIONS 5000

#define HASHTAB_SIZE 18917
