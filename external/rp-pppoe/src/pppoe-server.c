/***********************************************************************
*
* pppoe-server.c
*
* Implementation of a user-space PPPoE server
*
* Copyright (C) 2000 Roaring Penguin Software Inc.
*
* This program may be distributed according to the terms of the GNU
* General Public License, version 2 or (at your option) any later version.
*
* $Id$
*
* LIC: GPL
*
***********************************************************************/

static char const RCSID[] =
"$Id$";

#include "config.h"

#if defined(HAVE_NETPACKET_PACKET_H) || defined(HAVE_LINUX_IF_PACKET_H)
#define _POSIX_SOURCE 1 
#endif

#define _BSD_SOURCE 1 

#include "pppoe-server.h"
#include "md5.h"

#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif

#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#include <time.h>

#include <signal.h>

#ifdef HAVE_LICENSE
#include "license.h"
#include "licensed-only/servfuncs.h"
static struct License const *ServerLicense;
static struct License const *ClusterLicense;
#else
#define control_session_started(x) (void) 0
#define control_session_terminated(x) (void) 0
#define control_exit() (void) 0
#define realpeerip peerip
#endif

#ifdef HAVE_L2TP
extern PppoeSessionFunctionTable L2TPSessionFunctionTable;
extern void pppoe_to_l2tp_add_interface(EventSelector *es,
					Interface *interface);
#endif

static void InterfaceHandler(EventSelector *es,
			int fd, unsigned int flags, void *data);
static void startPPPD(ClientSession *sess);
static void sendErrorPADS(int sock, unsigned char *source, unsigned char *dest,
			  int errorTag, char *errorMsg);

#define CHECK_ROOM(cursor, start, len) \
do {\
    if (((cursor)-(start))+(len) > MAX_PPPOE_PAYLOAD) { \
	syslog(LOG_ERR, "Would create too-long packet"); \
	return; \
    } \
} while(0)

static void PppoeStopSession(ClientSession *ses, char const *reason);
static int PppoeSessionIsActive(ClientSession *ses);

#define MAX_SERVICE_NAMES 64
static int NumServiceNames = 0;
static char const *ServiceNames[MAX_SERVICE_NAMES];

PppoeSessionFunctionTable DefaultSessionFunctionTable = {
    PppoeStopSession,
    PppoeSessionIsActive,
    NULL
};

ClientSession *Sessions = NULL;
ClientSession *FreeSessions = NULL;
ClientSession *LastFreeSession = NULL;
ClientSession *BusySessions = NULL;

Interface interfaces[MAX_INTERFACES];
int NumInterfaces = 0;

size_t NumSessionSlots;

int MaxSessionsPerMac;

size_t NumActiveSessions = 0;

size_t SessOffset = 0;

EventSelector *event_selector;

static int UseLinuxKernelModePPPoE = 0;

static char *pppoptfile = NULL;

static int Debug = 0;
static int CheckPoolSyntax = 0;

static int Synchronous = 0;

#define SEED_LEN 16
#define MD5_LEN 16
#define COOKIE_LEN (MD5_LEN + sizeof(pid_t)) 

static unsigned char CookieSeed[SEED_LEN];

#define MAXLINE 512

#define DEFAULT_IF "wlan0"

char *ACName = NULL;

char PppoeOptions[SMALLBUF] = "";

unsigned char LocalIP[IPV4ALEN] = {10, 0, 0, 1}; 
unsigned char RemoteIP[IPV4ALEN] = {10, 67, 15, 1}; 

int IncrLocalIP = 0;

int RandomizeSessionNumbers = 0;

int PassUnitOptionToPPPD = 0;

static PPPoETag hostUniq;
static PPPoETag relayId;
static PPPoETag receivedCookie;
static PPPoETag requestedService;

#define HOSTNAMELEN 256

static int
count_sessions_from_mac(unsigned char *eth)
{
    int n=0;
    ClientSession *s = BusySessions;
    while(s) {
	if (!memcmp(eth, s->eth, ETH_ALEN)) n++;
	s = s->next;
    }
    return n;
}

static void
childHandler(pid_t pid, int status, void *s)
{
    ClientSession *session = s;

    
    PPPoEConnection conn;

#ifdef HAVE_L2TP
    if (session->flags & FLAG_ACT_AS_LAC) {
	syslog(LOG_INFO, "Session %u for client "
	       "%02x:%02x:%02x:%02x:%02x:%02x handed off to LNS %s",
	       (unsigned int) ntohs(session->sess),
	       session->eth[0], session->eth[1], session->eth[2],
	       session->eth[3], session->eth[4], session->eth[5],
	       inet_ntoa(session->tunnel_endpoint.sin_addr));
	session->pid = 0;
	session->funcs = &L2TPSessionFunctionTable;
	return;
    }
#endif

    memset(&conn, 0, sizeof(conn));
    conn.useHostUniq = 0;

    syslog(LOG_INFO,
	   "Session %u closed for client "
	   "%02x:%02x:%02x:%02x:%02x:%02x (%d.%d.%d.%d) on %s",
	   (unsigned int) ntohs(session->sess),
	   session->eth[0], session->eth[1], session->eth[2],
	   session->eth[3], session->eth[4], session->eth[5],
	   (int) session->realpeerip[0], (int) session->realpeerip[1],
	   (int) session->realpeerip[2], (int) session->realpeerip[3],
	   session->ethif->name);
    memcpy(conn.myEth, session->ethif->mac, ETH_ALEN);
    conn.discoverySocket = session->ethif->sock;
    conn.session = session->sess;
    memcpy(conn.peerEth, session->eth, ETH_ALEN);
    if (!(session->flags & FLAG_SENT_PADT)) {
	if (session->flags & FLAG_RECVD_PADT) {
	    sendPADT(&conn, "RP-PPPoE: Received PADT from peer");
	} else {
	    sendPADT(&conn, "RP-PPPoE: Child pppd process terminated");
	}
	session->flags |= FLAG_SENT_PADT;
    }

    session->serviceName = "";
    control_session_terminated(session);
    if (pppoe_free_session(session) < 0) {
	return;
    }

}

static void
incrementIPAddress(unsigned char ip[IPV4ALEN])
{
    ip[3]++;
    if (!ip[3]) {
	ip[2]++;
	if (!ip[2]) {
	    ip[1]++;
	    if (!ip[1]) {
		ip[0]++;
	    }
	}
    }
}

void
killAllSessions(void)
{
    ClientSession *sess = BusySessions;
    while(sess) {
	sess->funcs->stop(sess, "Shutting Down");
	sess = sess->next;
    }
#ifdef HAVE_L2TP
    pppoe_close_l2tp_tunnels();
#endif
}

static int
parseAddressPool(char const *fname, int install)
{
    FILE *fp = fopen(fname, "r");
    int numAddrs = 0;
    unsigned int a, b, c, d;
    unsigned int e, f, g, h;
    char line[MAXLINE];

    if (!fp) {
	sysErr("Cannot open address pool file");
	exit(1);
    }

    while (!feof(fp)) {
	if (!fgets(line, MAXLINE, fp)) {
	    break;
	}
	if ((sscanf(line, "%u.%u.%u.%u:%u.%u.%u.%u",
		    &a, &b, &c, &d, &e, &f, &g, &h) == 8) &&
	    a < 256 && b < 256 && c < 256 && d < 256 &&
	    e < 256 && f < 256 && g < 256 && h < 256) {

	    
	    if (install) {
		Sessions[numAddrs].myip[0] = (unsigned char) a;
		Sessions[numAddrs].myip[1] = (unsigned char) b;
		Sessions[numAddrs].myip[2] = (unsigned char) c;
		Sessions[numAddrs].myip[3] = (unsigned char) d;
		Sessions[numAddrs].peerip[0] = (unsigned char) e;
		Sessions[numAddrs].peerip[1] = (unsigned char) f;
		Sessions[numAddrs].peerip[2] = (unsigned char) g;
		Sessions[numAddrs].peerip[3] = (unsigned char) h;
#ifdef HAVE_LICENSE
		memcpy(Sessions[numAddrs].realpeerip,
		       Sessions[numAddrs].peerip, IPV4ALEN);
#endif
	    }
	    numAddrs++;
	} else if ((sscanf(line, "%u.%u.%u.%u-%u", &a, &b, &c, &d, &e) == 5) &&
		   a < 256 && b < 256 && c < 256 && d < 256 && e < 256) {
	    
	    if (e < d) {
		f = d;
		d = e;
		e = f;
	    }
	    if (install) {
		while (d <= e) {
		    Sessions[numAddrs].peerip[0] = (unsigned char) a;
		    Sessions[numAddrs].peerip[1] = (unsigned char) b;
		    Sessions[numAddrs].peerip[2] = (unsigned char) c;
		    Sessions[numAddrs].peerip[3] = (unsigned char) d;
#ifdef HAVE_LICENSE
		    memcpy(Sessions[numAddrs].realpeerip,
			   Sessions[numAddrs].peerip, IPV4ALEN);
#endif
		d++;
		numAddrs++;
		}
	    } else {
		numAddrs += (e-d) + 1;
	    }
	} else if ((sscanf(line, "%u.%u.%u.%u", &a, &b, &c, &d) == 4) &&
		   a < 256 && b < 256 && c < 256 && d < 256) {
	    
	    if (install) {
		Sessions[numAddrs].peerip[0] = (unsigned char) a;
		Sessions[numAddrs].peerip[1] = (unsigned char) b;
		Sessions[numAddrs].peerip[2] = (unsigned char) c;
		Sessions[numAddrs].peerip[3] = (unsigned char) d;
#ifdef HAVE_LICENSE
		memcpy(Sessions[numAddrs].realpeerip,
		       Sessions[numAddrs].peerip, IPV4ALEN);
#endif
	    }
	    numAddrs++;
	}
    }
    fclose(fp);
    if (!numAddrs) {
	rp_fatal("No valid ip addresses found in pool file");
    }
    return numAddrs;
}

void
parsePADITags(UINT16_t type, UINT16_t len, unsigned char *data,
	      void *extra)
{
    switch(type) {
    case TAG_SERVICE_NAME:
	
	requestedService.type = htons(type);
	requestedService.length = htons(len);
	memcpy(requestedService.payload, data, len);
	break;
    case TAG_RELAY_SESSION_ID:
	relayId.type = htons(type);
	relayId.length = htons(len);
	memcpy(relayId.payload, data, len);
	break;
    case TAG_HOST_UNIQ:
	hostUniq.type = htons(type);
	hostUniq.length = htons(len);
	memcpy(hostUniq.payload, data, len);
	break;
    }
}

void
parsePADRTags(UINT16_t type, UINT16_t len, unsigned char *data,
	      void *extra)
{
    switch(type) {
    case TAG_RELAY_SESSION_ID:
	relayId.type = htons(type);
	relayId.length = htons(len);
	memcpy(relayId.payload, data, len);
	break;
    case TAG_HOST_UNIQ:
	hostUniq.type = htons(type);
	hostUniq.length = htons(len);
	memcpy(hostUniq.payload, data, len);
	break;
    case TAG_AC_COOKIE:
	receivedCookie.type = htons(type);
	receivedCookie.length = htons(len);
	memcpy(receivedCookie.payload, data, len);
	break;
    case TAG_SERVICE_NAME:
	requestedService.type = htons(type);
	requestedService.length = htons(len);
	memcpy(requestedService.payload, data, len);
	break;
    }
}

void
fatalSys(char const *str)
{
    char buf[SMALLBUF];
    snprintf(buf, SMALLBUF, "%s: %s", str, strerror(errno));
    printErr(buf);
    control_exit();
    exit(EXIT_FAILURE);
}

void
sysErr(char const *str)
{
    char buf[1024];
    sprintf(buf, "%.256s: %.256s", str, strerror(errno));
    printErr(buf);
}

void
rp_fatal(char const *str)
{
    printErr(str);
    control_exit();
    exit(EXIT_FAILURE);
}

void
genCookie(unsigned char const *peerEthAddr,
	  unsigned char const *myEthAddr,
	  unsigned char const *seed,
	  unsigned char *cookie)
{
    struct MD5Context ctx;
    pid_t pid = getpid();

    MD5Init(&ctx);
    MD5Update(&ctx, peerEthAddr, ETH_ALEN);
    MD5Update(&ctx, myEthAddr, ETH_ALEN);
    MD5Update(&ctx, seed, SEED_LEN);
    MD5Final(cookie, &ctx);
    memcpy(cookie+MD5_LEN, &pid, sizeof(pid));
}

void
processPADI(Interface *ethif, PPPoEPacket *packet, int len)
{
    PPPoEPacket pado;
    PPPoETag acname;
    PPPoETag servname;
    PPPoETag cookie;
    size_t acname_len;
    unsigned char *cursor = pado.payload;
    UINT16_t plen;

    int sock = ethif->sock;
    int i;
    int ok = 0;
    unsigned char *myAddr = ethif->mac;

    
    if (NOT_UNICAST(packet->ethHdr.h_source)) {
	syslog(LOG_ERR, "PADI packet from non-unicast source address");
	return;
    }

    if (MaxSessionsPerMac) {
	if (count_sessions_from_mac(packet->ethHdr.h_source) >= MaxSessionsPerMac) {
	    syslog(LOG_INFO, "PADI: Client %02x:%02x:%02x:%02x:%02x:%02x attempted to create more than %d session(s)",
		   packet->ethHdr.h_source[0],
		   packet->ethHdr.h_source[1],
		   packet->ethHdr.h_source[2],
		   packet->ethHdr.h_source[3],
		   packet->ethHdr.h_source[4],
		   packet->ethHdr.h_source[5],
		   MaxSessionsPerMac);
	    return;
	}
    }

    acname.type = htons(TAG_AC_NAME);
    acname_len = strlen(ACName);
    acname.length = htons(acname_len);
    memcpy(acname.payload, ACName, acname_len);

    relayId.type = 0;
    hostUniq.type = 0;
    requestedService.type = 0;
    parsePacket(packet, parsePADITags, NULL);

    if (requestedService.type) {
	int slen = ntohs(requestedService.length);
	if (slen) {
	    for (i=0; i<NumServiceNames; i++) {
		if (slen == strlen(ServiceNames[i]) &&
		    !memcmp(ServiceNames[i], &requestedService.payload, slen)) {
		    ok = 1;
		    break;
		}
	    }
	} else {
	    ok = 1;		
	}
    } else {
	ok = 1;			
    }

    if (!ok) {
	
	return;
    }

    
    cookie.type = htons(TAG_AC_COOKIE);
    cookie.length = htons(COOKIE_LEN);
    genCookie(packet->ethHdr.h_source, myAddr, CookieSeed, cookie.payload);

    
    memcpy(pado.ethHdr.h_dest, packet->ethHdr.h_source, ETH_ALEN);
    memcpy(pado.ethHdr.h_source, myAddr, ETH_ALEN);
    pado.ethHdr.h_proto = htons(Eth_PPPOE_Discovery);
    pado.ver = 1;
    pado.type = 1;
    pado.code = CODE_PADO;
    pado.session = 0;
    plen = TAG_HDR_SIZE + acname_len;

    CHECK_ROOM(cursor, pado.payload, acname_len+TAG_HDR_SIZE);
    memcpy(cursor, &acname, acname_len + TAG_HDR_SIZE);
    cursor += acname_len + TAG_HDR_SIZE;

    servname.type = htons(TAG_SERVICE_NAME);
    if (!NumServiceNames) {
	servname.length = 0;
	CHECK_ROOM(cursor, pado.payload, TAG_HDR_SIZE);
	memcpy(cursor, &servname, TAG_HDR_SIZE);
	cursor += TAG_HDR_SIZE;
	plen += TAG_HDR_SIZE;
    } else {
	for (i=0; i<NumServiceNames; i++) {
	    int slen = strlen(ServiceNames[i]);
	    servname.length = htons(slen);
	    CHECK_ROOM(cursor, pado.payload, TAG_HDR_SIZE+slen);
	    memcpy(cursor, &servname, TAG_HDR_SIZE);
	    memcpy(cursor+TAG_HDR_SIZE, ServiceNames[i], slen);
	    cursor += TAG_HDR_SIZE+slen;
	    plen += TAG_HDR_SIZE+slen;
	}
    }

    CHECK_ROOM(cursor, pado.payload, TAG_HDR_SIZE + COOKIE_LEN);
    memcpy(cursor, &cookie, TAG_HDR_SIZE + COOKIE_LEN);
    cursor += TAG_HDR_SIZE + COOKIE_LEN;
    plen += TAG_HDR_SIZE + COOKIE_LEN;

    if (relayId.type) {
	CHECK_ROOM(cursor, pado.payload, ntohs(relayId.length) + TAG_HDR_SIZE);
	memcpy(cursor, &relayId, ntohs(relayId.length) + TAG_HDR_SIZE);
	cursor += ntohs(relayId.length) + TAG_HDR_SIZE;
	plen += ntohs(relayId.length) + TAG_HDR_SIZE;
    }
    if (hostUniq.type) {
	CHECK_ROOM(cursor, pado.payload, ntohs(hostUniq.length)+TAG_HDR_SIZE);
	memcpy(cursor, &hostUniq, ntohs(hostUniq.length) + TAG_HDR_SIZE);
	cursor += ntohs(hostUniq.length) + TAG_HDR_SIZE;
	plen += ntohs(hostUniq.length) + TAG_HDR_SIZE;
    }
    pado.length = htons(plen);
    sendPacket(NULL, sock, &pado, (int) (plen + HDR_SIZE));
}

void
processPADT(Interface *ethif, PPPoEPacket *packet, int len)
{
    size_t i;

    unsigned char *myAddr = ethif->mac;

    
    if (memcmp(packet->ethHdr.h_dest, myAddr, ETH_ALEN)) return;

    
    i = ntohs(packet->session) - 1 - SessOffset;
    if (i >= NumSessionSlots) return;
    if (Sessions[i].sess != packet->session) {
	syslog(LOG_ERR, "Session index %u doesn't match session number %u",
	       (unsigned int) i, (unsigned int) ntohs(packet->session));
	return;
    }


    
    if (memcmp(packet->ethHdr.h_source, Sessions[i].eth, ETH_ALEN)) {
	syslog(LOG_WARNING, "PADT for session %u received from "
	       "%02X:%02X:%02X:%02X:%02X:%02X; should be from "
	       "%02X:%02X:%02X:%02X:%02X:%02X",
	       (unsigned int) ntohs(packet->session),
	       packet->ethHdr.h_source[0],
	       packet->ethHdr.h_source[1],
	       packet->ethHdr.h_source[2],
	       packet->ethHdr.h_source[3],
	       packet->ethHdr.h_source[4],
	       packet->ethHdr.h_source[5],
	       Sessions[i].eth[0],
	       Sessions[i].eth[1],
	       Sessions[i].eth[2],
	       Sessions[i].eth[3],
	       Sessions[i].eth[4],
	       Sessions[i].eth[5]);
	return;
    }
    Sessions[i].flags |= FLAG_RECVD_PADT;
    parsePacket(packet, parseLogErrs, NULL);
    Sessions[i].funcs->stop(&Sessions[i], "Received PADT");
}

void
processPADR(Interface *ethif, PPPoEPacket *packet, int len)
{
    unsigned char cookieBuffer[COOKIE_LEN];
    ClientSession *cliSession;
    pid_t child;
    PPPoEPacket pads;
    unsigned char *cursor = pads.payload;
    UINT16_t plen;
    int i;
    int sock = ethif->sock;
    unsigned char *myAddr = ethif->mac;
    int slen = 0;
    char const *serviceName = NULL;

#ifdef HAVE_LICENSE
    int freemem;
#endif

    
    relayId.type = 0;
    hostUniq.type = 0;
    receivedCookie.type = 0;
    requestedService.type = 0;

    
    if (memcmp(packet->ethHdr.h_dest, myAddr, ETH_ALEN)) return;

    
    if (NOT_UNICAST(packet->ethHdr.h_source)) {
	syslog(LOG_ERR, "PADR packet from non-unicast source address");
	return;
    }

    if (MaxSessionsPerMac) {
	if (count_sessions_from_mac(packet->ethHdr.h_source) >= MaxSessionsPerMac) {
	    syslog(LOG_INFO, "PADR: Client %02x:%02x:%02x:%02x:%02x:%02x attempted to create more than %d session(s)",
		   packet->ethHdr.h_source[0],
		   packet->ethHdr.h_source[1],
		   packet->ethHdr.h_source[2],
		   packet->ethHdr.h_source[3],
		   packet->ethHdr.h_source[4],
		   packet->ethHdr.h_source[5],
		   MaxSessionsPerMac);
	    return;
	}
    }
    parsePacket(packet, parsePADRTags, NULL);

    
    if (!receivedCookie.type) {
	
	return;
    }

    
    if (receivedCookie.length != htons(COOKIE_LEN)) {
	
	return;
    }

    genCookie(packet->ethHdr.h_source, myAddr, CookieSeed, cookieBuffer);
    if (memcmp(receivedCookie.payload, cookieBuffer, COOKIE_LEN)) {
	
	return;
    }

    
    if (!requestedService.type) {
	syslog(LOG_ERR, "Received PADR packet with no SERVICE_NAME tag");
	sendErrorPADS(sock, myAddr, packet->ethHdr.h_source,
		      TAG_SERVICE_NAME_ERROR, "RP-PPPoE: Server: No service name tag");
	return;
    }

    slen = ntohs(requestedService.length);
    if (slen) {
	
	for(i=0; i<NumServiceNames; i++) {
	    if (slen == strlen(ServiceNames[i]) &&
		!memcmp(ServiceNames[i], &requestedService.payload, slen)) {
		serviceName = ServiceNames[i];
		break;
	    }
	}

	if (!serviceName) {
	    syslog(LOG_ERR, "Received PADR packet asking for unsupported service %.*s", (int) ntohs(requestedService.length), requestedService.payload);
	    sendErrorPADS(sock, myAddr, packet->ethHdr.h_source,
			  TAG_SERVICE_NAME_ERROR, "RP-PPPoE: Server: Invalid service name tag");
	    return;
	}
    } else {
	serviceName = "";
    }


#ifdef HAVE_LICENSE
    /* Are we licensed for this many sessions? */
    if (License_NumLicenses("PPPOE-SESSIONS") <= NumActiveSessions) {
	syslog(LOG_ERR, "Insufficient session licenses (%02x:%02x:%02x:%02x:%02x:%02x)",
	       (unsigned int) packet->ethHdr.h_source[0],
	       (unsigned int) packet->ethHdr.h_source[1],
	       (unsigned int) packet->ethHdr.h_source[2],
	       (unsigned int) packet->ethHdr.h_source[3],
	       (unsigned int) packet->ethHdr.h_source[4],
	       (unsigned int) packet->ethHdr.h_source[5]);
	sendErrorPADS(sock, myAddr, packet->ethHdr.h_source,
		      TAG_AC_SYSTEM_ERROR, "RP-PPPoE: Server: No session licenses available");
	return;
    }
#endif
    
#ifdef HAVE_LICENSE
    freemem = getFreeMem();
    if (freemem < MIN_FREE_MEMORY) {
	syslog(LOG_WARNING,
	       "Insufficient free memory to create session: Want %d, have %d",
	       MIN_FREE_MEMORY, freemem);
	sendErrorPADS(sock, myAddr, packet->ethHdr.h_source,
		      TAG_AC_SYSTEM_ERROR, "RP-PPPoE: Insufficient free RAM");
	return;
    }
#endif
    
    cliSession = pppoe_alloc_session();
    if (!cliSession) {
	syslog(LOG_ERR, "No client slots available (%02x:%02x:%02x:%02x:%02x:%02x)",
	       (unsigned int) packet->ethHdr.h_source[0],
	       (unsigned int) packet->ethHdr.h_source[1],
	       (unsigned int) packet->ethHdr.h_source[2],
	       (unsigned int) packet->ethHdr.h_source[3],
	       (unsigned int) packet->ethHdr.h_source[4],
	       (unsigned int) packet->ethHdr.h_source[5]);
	sendErrorPADS(sock, myAddr, packet->ethHdr.h_source,
		      TAG_AC_SYSTEM_ERROR, "RP-PPPoE: Server: No client slots available");
	return;
    }

    
    memcpy(cliSession->eth, packet->ethHdr.h_source, ETH_ALEN);
    cliSession->ethif = ethif;
    cliSession->flags = 0;
    cliSession->funcs = &DefaultSessionFunctionTable;
    cliSession->startTime = time(NULL);
    cliSession->serviceName = serviceName;

    
    child = fork();
    if (child < 0) {
	sendErrorPADS(sock, myAddr, packet->ethHdr.h_source,
		      TAG_AC_SYSTEM_ERROR, "RP-PPPoE: Server: Unable to start session process");
	pppoe_free_session(cliSession);
	return;
    }
    if (child != 0) {
	
	cliSession->pid = child;
	Event_HandleChildExit(event_selector, child,
			      childHandler, cliSession);
	control_session_started(cliSession);
	return;
    }

    

    
    closelog();
    for (i=0; i<CLOSEFD; i++) {
	if (i != sock) {
	    close(i);
	}
    }

    openlog("pppoe-server", LOG_PID, LOG_DAEMON);
    setsid();

    
    memcpy(pads.ethHdr.h_dest, packet->ethHdr.h_source, ETH_ALEN);
    memcpy(pads.ethHdr.h_source, myAddr, ETH_ALEN);
    pads.ethHdr.h_proto = htons(Eth_PPPOE_Discovery);
    pads.ver = 1;
    pads.type = 1;
    pads.code = CODE_PADS;

    pads.session = cliSession->sess;
    plen = 0;

    if (!slen && NumServiceNames) {
	slen = strlen(ServiceNames[0]);
	memcpy(&requestedService.payload, ServiceNames[0], slen);
	requestedService.length = htons(slen);
    }
    memcpy(cursor, &requestedService, TAG_HDR_SIZE+slen);
    cursor += TAG_HDR_SIZE+slen;
    plen += TAG_HDR_SIZE+slen;

    if (relayId.type) {
	memcpy(cursor, &relayId, ntohs(relayId.length) + TAG_HDR_SIZE);
	cursor += ntohs(relayId.length) + TAG_HDR_SIZE;
	plen += ntohs(relayId.length) + TAG_HDR_SIZE;
    }
    if (hostUniq.type) {
	memcpy(cursor, &hostUniq, ntohs(hostUniq.length) + TAG_HDR_SIZE);
	cursor += ntohs(hostUniq.length) + TAG_HDR_SIZE;
	plen += ntohs(hostUniq.length) + TAG_HDR_SIZE;
    }
    pads.length = htons(plen);
    sendPacket(NULL, sock, &pads, (int) (plen + HDR_SIZE));

    
    close(sock);

    startPPPD(cliSession);
}

static void
termHandler(int sig)
{
    syslog(LOG_INFO,
	   "Terminating on signal %d -- killing all PPPoE sessions",
	   sig);
    killAllSessions();
    control_exit();
    exit(0);
}

void
usage(char const *argv0)
{
    fprintf(stderr, "Usage: %s [options]\n", argv0);
    fprintf(stderr, "Options:\n");
#ifdef USE_BPF
    fprintf(stderr, "   -I if_name     -- Specify interface (REQUIRED)\n");
#else
    fprintf(stderr, "   -I if_name     -- Specify interface (default %s.)\n",
	    DEFAULT_IF);
#endif
    fprintf(stderr, "   -T timeout     -- Specify inactivity timeout in seconds.\n");
    fprintf(stderr, "   -C name        -- Set access concentrator name.\n");
    fprintf(stderr, "   -m MSS         -- Clamp incoming and outgoing MSS options.\n");
    fprintf(stderr, "   -L ip          -- Set local IP address.\n");
    fprintf(stderr, "   -l             -- Increment local IP address for each session.\n");
    fprintf(stderr, "   -R ip          -- Set start address of remote IP pool.\n");
    fprintf(stderr, "   -S name        -- Advertise specified service-name.\n");
    fprintf(stderr, "   -O fname       -- Use PPPD options from specified file\n");
    fprintf(stderr, "                     (default %s).\n", PPPOE_SERVER_OPTIONS);
    fprintf(stderr, "   -p fname       -- Optain IP address pool from specified file.\n");
    fprintf(stderr, "   -N num         -- Allow 'num' concurrent sessions.\n");
    fprintf(stderr, "   -o offset      -- Assign session numbers starting at offset+1.\n");
    fprintf(stderr, "   -f disc:sess   -- Set Ethernet frame types (hex).\n");
    fprintf(stderr, "   -s             -- Use synchronous PPP mode.\n");
#ifdef HAVE_LINUX_KERNEL_PPPOE
    fprintf(stderr, "   -k             -- Use kernel-mode PPPoE.\n");
#endif
    fprintf(stderr, "   -u             -- Pass 'unit' option to pppd.\n");
    fprintf(stderr, "   -r             -- Randomize session numbers.\n");
    fprintf(stderr, "   -d             -- Debug session creation.\n");
    fprintf(stderr, "   -x n           -- Limit to 'n' sessions/MAC address.\n");
    fprintf(stderr, "   -P             -- Check pool file for correctness and exit.\n");
#ifdef HAVE_LICENSE
    fprintf(stderr, "   -c secret:if:port -- Enable clustering on interface 'if'.\n");
    fprintf(stderr, "   -1             -- Allow only one session per user.\n");
#endif

    fprintf(stderr, "   -h             -- Print usage information.\n\n");
    fprintf(stderr, "PPPoE-Server Version %s, Copyright (C) 2001-2006 Roaring Penguin Software Inc.\n", VERSION);

#ifndef HAVE_LICENSE
    fprintf(stderr, "PPPoE-Server comes with ABSOLUTELY NO WARRANTY.\n");
    fprintf(stderr, "This is free software, and you are welcome to redistribute it\n");
    fprintf(stderr, "under the terms of the GNU General Public License, version 2\n");
    fprintf(stderr, "or (at your option) any later version.\n");
#endif
    fprintf(stderr, "http://www.roaringpenguin.com\n");
}

int
main(int argc, char **argv)
{

    FILE *fp;
    int i, j;
    int opt;
    int d[IPV4ALEN];
    int beDaemon = 1;
    int found;
    unsigned int discoveryType, sessionType;
    char *addressPoolFname = NULL;
#ifdef HAVE_LICENSE
    int use_clustering = 0;
#endif

#ifndef HAVE_LINUX_KERNEL_PPPOE
    char *options = "x:hI:C:L:R:T:m:FN:f:O:o:sp:lrudPc:S:1";
#else
    char *options = "x:hI:C:L:R:T:m:FN:f:O:o:skp:lrudPc:S:1";
#endif

    if (getuid() != geteuid() ||
	getgid() != getegid()) {
	fprintf(stderr, "SECURITY WARNING: pppoe-server will NOT run suid or sgid.  Fix your installation.\n");
	exit(1);
    }

    memset(interfaces, 0, sizeof(interfaces));

    
    openlog("pppoe-server", LOG_PID, LOG_DAEMON);

    
    NumSessionSlots = DEFAULT_MAX_SESSIONS;
    MaxSessionsPerMac = 0; 
    NumActiveSessions = 0;

    
    while((opt = getopt(argc, argv, options)) != -1) {
	switch(opt) {
	case 'x':
	    if (sscanf(optarg, "%d", &MaxSessionsPerMac) != 1) {
		usage(argv[0]);
		exit(EXIT_FAILURE);
	    }
	    if (MaxSessionsPerMac < 0) {
		MaxSessionsPerMac = 0;
	    }
	    break;

#ifdef HAVE_LINUX_KERNEL_PPPOE
	case 'k':
	    UseLinuxKernelModePPPoE = 1;
	    break;
#endif
	case 'S':
	    if (NumServiceNames == MAX_SERVICE_NAMES) {
		fprintf(stderr, "Too many '-S' options (%d max)",
			MAX_SERVICE_NAMES);
		exit(1);
	    }
	    ServiceNames[NumServiceNames] = strdup(optarg);
	    if (!ServiceNames[NumServiceNames]) {
		fprintf(stderr, "Out of memory");
		exit(1);
	    }
	    NumServiceNames++;
	    break;
	case 'c':
#ifndef HAVE_LICENSE
	    fprintf(stderr, "Clustering capability not available.\n");
	    exit(1);
#else
	    cluster_handle_option(optarg);
	    use_clustering = 1;
	    break;
#endif

	case 'd':
	    Debug = 1;
	    break;
	case 'P':
	    CheckPoolSyntax = 1;
	    break;
	case 'u':
	    PassUnitOptionToPPPD = 1;
	    break;

	case 'r':
	    RandomizeSessionNumbers = 1;
	    break;

	case 'l':
	    IncrLocalIP = 1;
	    break;

	case 'p':
	    SET_STRING(addressPoolFname, optarg);
	    break;

	case 's':
	    Synchronous = 1;
	    
	    snprintf(PppoeOptions + strlen(PppoeOptions),
		     SMALLBUF-strlen(PppoeOptions),
		     " -s");
	    break;

	case 'f':
	    if (sscanf(optarg, "%x:%x", &discoveryType, &sessionType) != 2) {
		fprintf(stderr, "Illegal argument to -f: Should be disc:sess in hex\n");
		exit(EXIT_FAILURE);
	    }
	    Eth_PPPOE_Discovery = (UINT16_t) discoveryType;
	    Eth_PPPOE_Session   = (UINT16_t) sessionType;
	    
	    snprintf(PppoeOptions + strlen(PppoeOptions),
		     SMALLBUF-strlen(PppoeOptions),
		     " -%c %s", opt, optarg);
	    break;

	case 'F':
	    beDaemon = 0;
	    break;

	case 'N':
	    if (sscanf(optarg, "%d", &opt) != 1) {
		usage(argv[0]);
		exit(EXIT_FAILURE);
	    }
	    if (opt <= 0) {
		fprintf(stderr, "-N: Value must be positive\n");
		exit(EXIT_FAILURE);
	    }
	    NumSessionSlots = opt;
	    break;

	case 'O':
	    SET_STRING(pppoptfile, optarg);
	    break;

	case 'o':
	    if (sscanf(optarg, "%d", &opt) != 1) {
		usage(argv[0]);
		exit(EXIT_FAILURE);
	    }
	    if (opt < 0) {
		fprintf(stderr, "-o: Value must be non-negative\n");
		exit(EXIT_FAILURE);
	    }
	    SessOffset = (size_t) opt;
	    break;

	case 'I':
	    if (NumInterfaces >= MAX_INTERFACES) {
		fprintf(stderr, "Too many -I options (max %d)\n",
			MAX_INTERFACES);
		exit(EXIT_FAILURE);
	    }
	    found = 0;
	    for (i=0; i<NumInterfaces; i++) {
		if (!strncmp(interfaces[i].name, optarg, IFNAMSIZ)) {
		    found = 1;
		    break;
		}
	    }
	    if (!found) {
		strncpy(interfaces[NumInterfaces].name, optarg, IFNAMSIZ);
		NumInterfaces++;
	    }
	    break;

	case 'C':
	    SET_STRING(ACName, optarg);
	    break;

	case 'L':
	case 'R':
	    
	    if (sscanf(optarg, "%d.%d.%d.%d", &d[0], &d[1], &d[2], &d[3]) != 4) {
		usage(argv[0]);
		exit(EXIT_FAILURE);
	    }
	    for (i=0; i<IPV4ALEN; i++) {
		if (d[i] < 0 || d[i] > 255) {
		    usage(argv[0]);
		    exit(EXIT_FAILURE);
		}
		if (opt == 'L') {
		    LocalIP[i] = (unsigned char) d[i];
		} else {
		    RemoteIP[i] = (unsigned char) d[i];
		}
	    }
	    break;

	case 'T':
	case 'm':
	    
	    snprintf(PppoeOptions + strlen(PppoeOptions),
		     SMALLBUF-strlen(PppoeOptions),
		     " -%c %s", opt, optarg);
	    break;

	case 'h':
	    usage(argv[0]);
	    exit(EXIT_SUCCESS);
	case '1':
#ifdef HAVE_LICENSE
	    MaxSessionsPerUser = 1;
#else
	    fprintf(stderr, "-1 option not valid.\n");
	    exit(1);
#endif
	    break;
	}
    }

    if (!pppoptfile) {
	pppoptfile = PPPOE_SERVER_OPTIONS;
    }

#ifdef HAVE_LICENSE
    License_SetVersion(SERVPOET_VERSION);
    License_ReadBundleFile("/etc/rp/bundle.txt");
    License_ReadFile("/etc/rp/license.txt");
    ServerLicense = License_GetFeature("PPPOE-SERVER");
    if (!ServerLicense) {
	fprintf(stderr, "License: GetFeature failed: %s\n",
		License_ErrorMessage());
	exit(1);
    }
#endif

#ifdef USE_LINUX_PACKET
#ifndef HAVE_STRUCT_SOCKADDR_LL
    fprintf(stderr, "The PPPoE server does not work on Linux 2.0 kernels.\n");
    exit(EXIT_FAILURE);
#endif
#endif

    if (!NumInterfaces) {
	strcpy(interfaces[0].name, DEFAULT_IF);
	NumInterfaces = 1;
    }

    if (!ACName) {
	ACName = malloc(HOSTNAMELEN);
	if (gethostname(ACName, HOSTNAMELEN) < 0) {
	    fatalSys("gethostname");
	}
    }

    
    if (addressPoolFname) {
	NumSessionSlots = parseAddressPool(addressPoolFname, 0);
	if (CheckPoolSyntax) {
	    printf("%lu\n", (unsigned long) NumSessionSlots);
	    exit(0);
	}
    }

    
    if (NumSessionSlots + SessOffset > 65534) {
	fprintf(stderr, "-N and -o options must add up to at most 65534\n");
	exit(EXIT_FAILURE);
    }

    
    Sessions = calloc(NumSessionSlots, sizeof(ClientSession));
    if (!Sessions) {
	rp_fatal("Cannot allocate memory for session slots");
    }

    
    for (i=0; i<NumSessionSlots; i++) {
	memcpy(Sessions[i].myip, LocalIP, sizeof(LocalIP));
	if (IncrLocalIP) {
	    incrementIPAddress(LocalIP);
	}
    }

    
    if (addressPoolFname) {
	(void) parseAddressPool(addressPoolFname, 1);
    }

    
    for (i=0; i<NumSessionSlots; i++) {
	Sessions[i].pid = 0;
	Sessions[i].funcs = &DefaultSessionFunctionTable;
	Sessions[i].sess = htons(i+1+SessOffset);

	if (!addressPoolFname) {
	    memcpy(Sessions[i].peerip, RemoteIP, sizeof(RemoteIP));
#ifdef HAVE_LICENSE
	    memcpy(Sessions[i].realpeerip, RemoteIP, sizeof(RemoteIP));
#endif
	    incrementIPAddress(RemoteIP);
	}
    }

    fp = fopen("/dev/urandom", "r");
    if (fp) {
	unsigned int x;
	fread(&x, 1, sizeof(x), fp);
	srand(x);
	fread(&CookieSeed, 1, SEED_LEN, fp);
	fclose(fp);
    } else {
	srand((unsigned int) getpid() * (unsigned int) time(NULL));
	CookieSeed[0] = getpid() & 0xFF;
	CookieSeed[1] = (getpid() >> 8) & 0xFF;
	for (i=2; i<SEED_LEN; i++) {
	    CookieSeed[i] = (rand() >> (i % 9)) & 0xFF;
	}
    }

    if (RandomizeSessionNumbers) {
	int *permutation;
	int tmp;
	permutation = malloc(sizeof(int) * NumSessionSlots);
	if (!permutation) {
	    fprintf(stderr, "Could not allocate memory to randomize session numbers\n");
	    exit(EXIT_FAILURE);
	}
	for (i=0; i<NumSessionSlots; i++) {
	    permutation[i] = i;
	}
	for (i=0; i<NumSessionSlots-1; i++) {
	    j = i + rand() % (NumSessionSlots - i);
	    if (j != i) {
		tmp = permutation[j];
		permutation[j] = permutation[i];
		permutation[i] = tmp;
	    }
	}
	
	FreeSessions = &Sessions[permutation[0]];
	LastFreeSession = &Sessions[permutation[NumSessionSlots-1]];
	for (i=0; i<NumSessionSlots-1; i++) {
	    Sessions[permutation[i]].next = &Sessions[permutation[i+1]];
	}
	Sessions[permutation[NumSessionSlots-1]].next = NULL;
	free(permutation);
    } else {
	
	FreeSessions = &Sessions[0];
	LastFreeSession = &Sessions[NumSessionSlots - 1];
	for (i=0; i<NumSessionSlots-1; i++) {
	    Sessions[i].next = &Sessions[i+1];
	}
	Sessions[NumSessionSlots-1].next = NULL;
    }

    if (Debug) {
	
	ClientSession *ses = FreeSessions;
	while(ses) {
	    printf("Session %u local %d.%d.%d.%d remote %d.%d.%d.%d\n",
		   (unsigned int) (ntohs(ses->sess)),
		   ses->myip[0], ses->myip[1],
		   ses->myip[2], ses->myip[3],
		   ses->peerip[0], ses->peerip[1],
		   ses->peerip[2], ses->peerip[3]);
	    ses = ses->next;
	}
	exit(0);
    }

    
    for (i=0; i<NumInterfaces; i++) {
	interfaces[i].sock = openInterface(interfaces[i].name, Eth_PPPOE_Discovery, interfaces[i].mac);
    }

    
    signal(SIGPIPE, SIG_IGN);

    
    event_selector = Event_CreateSelector();
    if (!event_selector) {
	rp_fatal("Could not create EventSelector -- probably out of memory");
    }

    
    if (Event_HandleSignal(event_selector, SIGTERM, termHandler) < 0 ||
	Event_HandleSignal(event_selector, SIGINT, termHandler) < 0) {
	fatalSys("Event_HandleSignal");
    }

    
#ifdef HAVE_LICENSE
    if (control_init(argc, argv, event_selector)) {
	rp_fatal("control_init failed");
    }
#endif

    
    for (i = 0; i<NumInterfaces; i++) {
	interfaces[i].eh = Event_AddHandler(event_selector,
					    interfaces[i].sock,
					    EVENT_FLAG_READABLE,
					    InterfaceHandler,
					    &interfaces[i]);
#ifdef HAVE_L2TP
	interfaces[i].session_sock = -1;
#endif
	if (!interfaces[i].eh) {
	    rp_fatal("Event_AddHandler failed");
	}
    }

#ifdef HAVE_LICENSE
    if (use_clustering) {
	ClusterLicense = License_GetFeature("PPPOE-CLUSTER");
	if (!ClusterLicense) {
	    fprintf(stderr, "License: GetFeature failed: %s\n",
		    License_ErrorMessage());
	    exit(1);
	}
	if (!License_Expired(ClusterLicense)) {
	    if (cluster_init(event_selector) < 0) {
		rp_fatal("cluster_init failed");
	    }
	}
    }
#endif

#ifdef HAVE_L2TP
    for (i=0; i<NumInterfaces; i++) {
	pppoe_to_l2tp_add_interface(event_selector,
				    &interfaces[i]);
    }
#endif

    
    if (beDaemon) {
	i = fork();
	if (i < 0) {
	    fatalSys("fork");
	} else if (i != 0) {
	    
	    exit(EXIT_SUCCESS);
	}
	setsid();
	signal(SIGHUP, SIG_IGN);
	i = fork();
	if (i < 0) {
	    fatalSys("fork");
	} else if (i != 0) {
	    exit(EXIT_SUCCESS);
	}

	chdir("/");

	
	for (i=0; i<3; i++) {
	    close(i);
	}
	i = open("/dev/null", O_RDWR);
	if (i >= 0) {
	    dup2(i, 0);
	    dup2(i, 1);
	    dup2(i, 2);
	    if (i > 2) close(i);
	}
    }

    for(;;) {
	i = Event_HandleEvent(event_selector);
	if (i < 0) {
	    fatalSys("Event_HandleEvent");
	}

#ifdef HAVE_LICENSE
	if (License_Expired(ServerLicense)) {
	    syslog(LOG_INFO, "Server license has expired -- killing all PPPoE sessions");
	    killAllSessions();
	    control_exit();
	    exit(0);
	}
#endif
    }
    return 0;
}

void
serverProcessPacket(Interface *i)
{
    int len;
    PPPoEPacket packet;
    int sock = i->sock;

    if (receivePacket(sock, &packet, &len) < 0) {
	return;
    }

    
    if (ntohs(packet.length) + HDR_SIZE > len) {
	syslog(LOG_ERR, "Bogus PPPoE length field (%u)",
	       (unsigned int) ntohs(packet.length));
	return;
    }

    
    if (packet.ver != 1 || packet.type != 1) {
	
	return;
    }
    switch(packet.code) {
    case CODE_PADI:
	processPADI(i, &packet, len);
	break;
    case CODE_PADR:
	processPADR(i, &packet, len);
	break;
    case CODE_PADT:
	
	processPADT(i, &packet, len);
	break;
    case CODE_SESS:
	
	break;
    case CODE_PADO:
    case CODE_PADS:
	
	break;
    default:
	
	break;
    }
}

void
sendErrorPADS(int sock,
	      unsigned char *source,
	      unsigned char *dest,
	      int errorTag,
	      char *errorMsg)
{
    PPPoEPacket pads;
    unsigned char *cursor = pads.payload;
    UINT16_t plen;
    PPPoETag err;
    int elen = strlen(errorMsg);

    memcpy(pads.ethHdr.h_dest, dest, ETH_ALEN);
    memcpy(pads.ethHdr.h_source, source, ETH_ALEN);
    pads.ethHdr.h_proto = htons(Eth_PPPOE_Discovery);
    pads.ver = 1;
    pads.type = 1;
    pads.code = CODE_PADS;

    pads.session = htons(0);
    plen = 0;

    err.type = htons(errorTag);
    err.length = htons(elen);

    memcpy(err.payload, errorMsg, elen);
    memcpy(cursor, &err, TAG_HDR_SIZE+elen);
    cursor += TAG_HDR_SIZE + elen;
    plen += TAG_HDR_SIZE + elen;

    if (relayId.type) {
	memcpy(cursor, &relayId, ntohs(relayId.length) + TAG_HDR_SIZE);
	cursor += ntohs(relayId.length) + TAG_HDR_SIZE;
	plen += ntohs(relayId.length) + TAG_HDR_SIZE;
    }
    if (hostUniq.type) {
	memcpy(cursor, &hostUniq, ntohs(hostUniq.length) + TAG_HDR_SIZE);
	cursor += ntohs(hostUniq.length) + TAG_HDR_SIZE;
	plen += ntohs(hostUniq.length) + TAG_HDR_SIZE;
    }
    pads.length = htons(plen);
    sendPacket(NULL, sock, &pads, (int) (plen + HDR_SIZE));
}


void
startPPPDUserMode(ClientSession *session)
{
    
    char *argv[32];

    char buffer[SMALLBUF];

    int c = 0;

    argv[c++] = "pppd";
    argv[c++] = "pty";

    
    snprintf(buffer, SMALLBUF, "%s -n -I %s -e %u:%02x:%02x:%02x:%02x:%02x:%02x%s -S '%s'",
	     PPPOE_PATH, session->ethif->name,
	     (unsigned int) ntohs(session->sess),
	     session->eth[0], session->eth[1], session->eth[2],
	     session->eth[3], session->eth[4], session->eth[5],
	     PppoeOptions, session->serviceName);
    argv[c++] = strdup(buffer);
    if (!argv[c-1]) {
	
	exit(EXIT_FAILURE);
    }

    argv[c++] = "file";
    argv[c++] = pppoptfile;

    snprintf(buffer, SMALLBUF, "%d.%d.%d.%d:%d.%d.%d.%d",
	    (int) session->myip[0], (int) session->myip[1],
	    (int) session->myip[2], (int) session->myip[3],
	    (int) session->peerip[0], (int) session->peerip[1],
	    (int) session->peerip[2], (int) session->peerip[3]);
    syslog(LOG_INFO,
	   "Session %u created for client %02x:%02x:%02x:%02x:%02x:%02x (%d.%d.%d.%d) on %s using Service-Name '%s'",
	   (unsigned int) ntohs(session->sess),
	   session->eth[0], session->eth[1], session->eth[2],
	   session->eth[3], session->eth[4], session->eth[5],
	   (int) session->peerip[0], (int) session->peerip[1],
	   (int) session->peerip[2], (int) session->peerip[3],
	   session->ethif->name,
	   session->serviceName);
    argv[c++] = strdup(buffer);
    if (!argv[c-1]) {
	
	exit(EXIT_FAILURE);
    }
    argv[c++] = "nodetach";
    argv[c++] = "noaccomp";
    argv[c++] = "nobsdcomp";
    argv[c++] = "nodeflate";
    argv[c++] = "nopcomp";
    argv[c++] = "novj";
    argv[c++] = "novjccomp";
    argv[c++] = "default-asyncmap";
    if (Synchronous) {
	argv[c++] = "sync";
    }
    if (PassUnitOptionToPPPD) {
	argv[c++] = "unit";
	sprintf(buffer, "%u", (unsigned int) (ntohs(session->sess) - 1 - SessOffset));
	argv[c++] = buffer;
    }
    argv[c++] = NULL;

    execv(PPPD_PATH, argv);
    exit(EXIT_FAILURE);
}

void
startPPPDLinuxKernelMode(ClientSession *session)
{
    
    char *argv[32];

    int c = 0;

    char buffer[SMALLBUF];

    argv[c++] = "pppd";
    argv[c++] = "plugin";
    argv[c++] = PLUGIN_PATH;

    
    snprintf(buffer, SMALLBUF, "nic-%s", session->ethif->name);
    argv[c++] = strdup(buffer);
    if (!argv[c-1]) {
	exit(EXIT_FAILURE);
    }

    snprintf(buffer, SMALLBUF, "%u:%02x:%02x:%02x:%02x:%02x:%02x",
	     (unsigned int) ntohs(session->sess),
	     session->eth[0], session->eth[1], session->eth[2],
	     session->eth[3], session->eth[4], session->eth[5]);
    argv[c++] = "rp_pppoe_sess";
    argv[c++] = strdup(buffer);
    if (!argv[c-1]) {
	
	exit(EXIT_FAILURE);
    }
    argv[c++] = "rp_pppoe_service";
    argv[c++] = (char *) session->serviceName;
    argv[c++] = "file";
    argv[c++] = pppoptfile;

    snprintf(buffer, SMALLBUF, "%d.%d.%d.%d:%d.%d.%d.%d",
	    (int) session->myip[0], (int) session->myip[1],
	    (int) session->myip[2], (int) session->myip[3],
	    (int) session->peerip[0], (int) session->peerip[1],
	    (int) session->peerip[2], (int) session->peerip[3]);
    syslog(LOG_INFO,
	   "Session %u created for client %02x:%02x:%02x:%02x:%02x:%02x (%d.%d.%d.%d) on %s using Service-Name '%s'",
	   (unsigned int) ntohs(session->sess),
	   session->eth[0], session->eth[1], session->eth[2],
	   session->eth[3], session->eth[4], session->eth[5],
	   (int) session->peerip[0], (int) session->peerip[1],
	   (int) session->peerip[2], (int) session->peerip[3],
	   session->ethif->name,
	   session->serviceName);
    argv[c++] = strdup(buffer);
    if (!argv[c-1]) {
	
	exit(EXIT_FAILURE);
    }
    argv[c++] = "nodetach";
    argv[c++] = "noaccomp";
    argv[c++] = "nobsdcomp";
    argv[c++] = "nodeflate";
    argv[c++] = "nopcomp";
    argv[c++] = "novj";
    argv[c++] = "novjccomp";
    argv[c++] = "default-asyncmap";
    if (PassUnitOptionToPPPD) {
	argv[c++] = "unit";
	sprintf(buffer, "%u", (unsigned int) (ntohs(session->sess) - 1 - SessOffset));
	argv[c++] = buffer;
    }
    argv[c++] = NULL;
    execv(PPPD_PATH, argv);
    exit(EXIT_FAILURE);
}

void
startPPPD(ClientSession *session)
{
    if (UseLinuxKernelModePPPoE) startPPPDLinuxKernelMode(session);
    else startPPPDUserMode(session);
}

void
InterfaceHandler(EventSelector *es,
		 int fd,
		 unsigned int flags,
		 void *data)
{
    serverProcessPacket((Interface *) data);
}

static void
PppoeStopSession(ClientSession *ses,
		 char const *reason)
{
    
    PPPoEConnection conn;

    memset(&conn, 0, sizeof(conn));
    conn.useHostUniq = 0;

    memcpy(conn.myEth, ses->ethif->mac, ETH_ALEN);
    conn.discoverySocket = ses->ethif->sock;
    conn.session = ses->sess;
    memcpy(conn.peerEth, ses->eth, ETH_ALEN);
    sendPADT(&conn, reason);
    ses->flags |= FLAG_SENT_PADT;

    if (ses->pid) {
	kill(ses->pid, SIGTERM);
    }
    ses->funcs = &DefaultSessionFunctionTable;
}

static int
PppoeSessionIsActive(ClientSession *ses)
{
    return (ses->pid != 0);
}

#ifdef HAVE_LICENSE
int
getFreeMem(void)
{
    char buf[512];
    int memfree=0, buffers=0, cached=0;
    FILE *fp = fopen("/proc/meminfo", "r");
    if (!fp) return -1;

    while (fgets(buf, sizeof(buf), fp)) {
	if (!strncmp(buf, "MemFree:", 8)) {
	    if (sscanf(buf, "MemFree: %d", &memfree) != 1) {
		fclose(fp);
		return -1;
	    }
	} else if (!strncmp(buf, "Buffers:", 8)) {
	    if (sscanf(buf, "Buffers: %d", &buffers) != 1) {
		fclose(fp);
		return -1;
	    }
	} else if (!strncmp(buf, "Cached:", 7)) {
	    if (sscanf(buf, "Cached: %d", &cached) != 1) {
		fclose(fp);
		return -1;
	    }
	}
    }
    fclose(fp);
    
    return memfree;
}
#endif

ClientSession *
pppoe_alloc_session(void)
{
    ClientSession *ses = FreeSessions;
    if (!ses) return NULL;

    
    if (ses == LastFreeSession) {
	LastFreeSession = NULL;
    }
    FreeSessions = ses->next;

    
    ses->next = BusySessions;
    BusySessions = ses;

    
    ses->funcs = &DefaultSessionFunctionTable;
    ses->pid = 0;
    ses->ethif = NULL;
    memset(ses->eth, 0, ETH_ALEN);
    ses->flags = 0;
    ses->startTime = time(NULL);
    ses->serviceName = "";
#ifdef HAVE_LICENSE
    memset(ses->user, 0, MAX_USERNAME_LEN+1);
    memset(ses->realm, 0, MAX_USERNAME_LEN+1);
    memset(ses->realpeerip, 0, IPV4ALEN);
#endif
#ifdef HAVE_L2TP
    ses->l2tp_ses = NULL;
#endif
    NumActiveSessions++;
    return ses;
}

int
pppoe_free_session(ClientSession *ses)
{
    ClientSession *cur, *prev;

    cur = BusySessions;
    prev = NULL;
    while (cur) {
	if (ses == cur) break;
	prev = cur;
	cur = cur->next;
    }

    if (!cur) {
	syslog(LOG_ERR, "pppoe_free_session: Could not find session %p on busy list", (void *) ses);
	return -1;
    }

    
    if (prev) {
	prev->next = ses->next;
    } else {
	BusySessions = ses->next;
    }

    
    ses->next = NULL;
    if (LastFreeSession) {
	LastFreeSession->next = ses;
	LastFreeSession = ses;
    } else {
	FreeSessions = ses;
	LastFreeSession = ses;
    }

    
    ses->funcs = &DefaultSessionFunctionTable;
    ses->pid = 0;
    ses->flags = 0;
#ifdef HAVE_L2TP
    ses->l2tp_ses = NULL;
#endif
    NumActiveSessions--;
    return 0;
}

void
sendHURLorMOTM(PPPoEConnection *conn, char const *url, UINT16_t tag)
{
    PPPoEPacket packet;
    PPPoETag hurl;
    size_t elen;
    unsigned char *cursor = packet.payload;
    UINT16_t plen = 0;

    if (!conn->session) return;
    if (conn->discoverySocket < 0) return;

    if (tag == TAG_HURL) {
	if (strncmp(url, "http://", 7)) {
	    syslog(LOG_WARNING, "sendHURL(%s): URL must begin with http://", url);
	    return;
	}
    } else {
	tag = TAG_MOTM;
    }

    memcpy(packet.ethHdr.h_dest, conn->peerEth, ETH_ALEN);
    memcpy(packet.ethHdr.h_source, conn->myEth, ETH_ALEN);

    packet.ethHdr.h_proto = htons(Eth_PPPOE_Discovery);
    packet.ver = 1;
    packet.type = 1;
    packet.code = CODE_PADM;
    packet.session = conn->session;

    elen = strlen(url);
    if (elen > 256) {
	syslog(LOG_WARNING, "MOTM or HURL too long: %d", (int) elen);
	return;
    }

    hurl.type = htons(tag);
    hurl.length = htons(elen);
    strcpy((char *) hurl.payload, url);
    memcpy(cursor, &hurl, elen + TAG_HDR_SIZE);
    cursor += elen + TAG_HDR_SIZE;
    plen += elen + TAG_HDR_SIZE;

    packet.length = htons(plen);

    sendPacket(conn, conn->discoverySocket, &packet, (int) (plen + HDR_SIZE));
#ifdef DEBUGGING_ENABLED
    if (conn->debugFile) {
	dumpPacket(conn->debugFile, &packet, "SENT");
	fprintf(conn->debugFile, "\n");
	fflush(conn->debugFile);
    }
#endif
}
