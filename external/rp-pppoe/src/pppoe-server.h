/**********************************************************************
*
* pppoe-server.h
*
* Definitions for PPPoE server
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
#include "event.h"

#ifdef HAVE_L2TP
#include "l2tp/l2tp.h"
#endif

#define MAX_USERNAME_LEN 31
typedef struct {
    char name[IFNAMSIZ+1];	
    int sock;			
    unsigned char mac[ETH_ALEN]; 
    EventHandler *eh;		

    
#ifdef HAVE_L2TP
    int session_sock;		
    EventHandler *lac_eh;	
#endif
} Interface;

#define FLAG_RECVD_PADT      1
#define FLAG_USER_SET        2
#define FLAG_IP_SET          4
#define FLAG_SENT_PADT       8

#define FLAG_ACT_AS_LAC      256
#define FLAG_ACT_AS_LNS      512

struct ClientSessionStruct;

typedef struct PppoeSessionFunctionTable_t {
    
    void (*stop)(struct ClientSessionStruct *ses, char const *reason);

    
    int (*isActive)(struct ClientSessionStruct *ses);

    
    char const * (*describe)(struct ClientSessionStruct *ses);
} PppoeSessionFunctionTable;

extern PppoeSessionFunctionTable DefaultSessionFunctionTable;

typedef struct ClientSessionStruct {
    struct ClientSessionStruct *next; 
    PppoeSessionFunctionTable *funcs; 
    pid_t pid;			
    Interface *ethif;		
    unsigned char myip[IPV4ALEN]; 
    unsigned char peerip[IPV4ALEN]; 
    UINT16_t sess;		
    unsigned char eth[ETH_ALEN]; 
    unsigned int flags;		
    time_t startTime;		
    char const *serviceName;	
#ifdef HAVE_LICENSE
    char user[MAX_USERNAME_LEN+1]; 
    char realm[MAX_USERNAME_LEN+1]; 
    unsigned char realpeerip[IPV4ALEN];	
    int maxSessionsPerUser;	
#endif
#ifdef HAVE_L2TP
    l2tp_session *l2tp_ses;	
    struct sockaddr_in tunnel_endpoint;	
#endif
} ClientSession;

#define CLOSEFD 64

#define MAX_INTERFACES 64

#define DEFAULT_MAX_SESSIONS 64

extern ClientSession *Sessions;

extern Interface interfaces[MAX_INTERFACES];
extern int NumInterfaces;

extern size_t NumSessionSlots;

extern size_t NumActiveSessions;

extern size_t SessOffset;

extern char *ACName;

extern unsigned char LocalIP[IPV4ALEN];
extern unsigned char RemoteIP[IPV4ALEN];

#define MIN_FREE_MEMORY 10000

extern int IncrLocalIP;

extern ClientSession *FreeSessions;

extern ClientSession *LastFreeSession;

extern ClientSession *BusySessions;

extern EventSelector *event_selector;
extern int GotAlarm;

extern void setAlarm(unsigned int secs);
extern void killAllSessions(void);
extern void serverProcessPacket(Interface *i);
extern void processPADT(Interface *ethif, PPPoEPacket *packet, int len);
extern void processPADR(Interface *ethif, PPPoEPacket *packet, int len);
extern void processPADI(Interface *ethif, PPPoEPacket *packet, int len);
extern void usage(char const *msg);
extern ClientSession *pppoe_alloc_session(void);
extern int pppoe_free_session(ClientSession *ses);
extern void sendHURLorMOTM(PPPoEConnection *conn, char const *url, UINT16_t tag);

#ifdef HAVE_LICENSE
extern int getFreeMem(void);
#endif
