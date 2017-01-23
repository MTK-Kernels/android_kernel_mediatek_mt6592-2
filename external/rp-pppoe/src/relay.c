/***********************************************************************
*
* relay.c
*
* Implementation of PPPoE relay
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
static char const RCSID[] =
"$Id$";

#define _GNU_SOURCE 1 

#include "relay.h"

#include <signal.h>

#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#ifdef HAVE_SYS_UIO_H
#include <sys/uio.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif


PPPoEInterface Interfaces[MAX_INTERFACES];
int NumInterfaces;

int NumSessions;
int MaxSessions;
PPPoESession *AllSessions;
PPPoESession *FreeSessions;
PPPoESession *ActiveSessions;

SessionHash *AllHashes;
SessionHash *FreeHashes;
SessionHash *Buckets[HASHTAB_SIZE];

volatile unsigned int Epoch = 0;
volatile unsigned int CleanCounter = 0;

#define MIN_CLEAN_PERIOD 30  
#define TIMEOUT_DIVISOR 20   
unsigned int CleanPeriod = MIN_CLEAN_PERIOD;

unsigned int IdleTimeout = MIN_CLEAN_PERIOD * TIMEOUT_DIVISOR;

int CleanPipe[2];

#define MY_RELAY_TAG_LEN (sizeof(int) + ETH_ALEN)

#define CLOSEFD 64

static int
keepDescriptor(int fd)
{
    int i;
    if (fd == CleanPipe[0] || fd == CleanPipe[1]) return 1;
    for (i=0; i<NumInterfaces; i++) {
	if (fd == Interfaces[i].discoverySock ||
	    fd == Interfaces[i].sessionSock) return 1;
    }
    return 0;
}

int
addTag(PPPoEPacket *packet, PPPoETag const *tag)
{
    return insertBytes(packet, packet->payload, tag,
		       ntohs(tag->length) + TAG_HDR_SIZE);
}

int
insertBytes(PPPoEPacket *packet,
	    unsigned char *loc,
	    void const *bytes,
	    int len)
{
    int toMove;
    int plen = ntohs(packet->length);
    
    if (loc < packet->payload ||
	loc > packet->payload + plen ||
	len + plen > MAX_PPPOE_PAYLOAD) {
	return -1;
    }

    toMove = (packet->payload + plen) - loc;
    memmove(loc+len, loc, toMove);
    memcpy(loc, bytes, len);
    packet->length = htons(plen + len);
    return len;
}

int
removeBytes(PPPoEPacket *packet,
	    unsigned char *loc,
	    int len)
{
    int toMove;
    int plen = ntohs(packet->length);
    
    if (len < 0 || len > plen ||
	loc < packet->payload ||
	loc + len > packet->payload + plen) {
	return -1;
    }

    toMove = ((packet->payload + plen) - loc) - len;
    memmove(loc, loc+len, toMove);
    packet->length = htons(plen - len);
    return len;
}

void
usage(char const *argv0)
{
    fprintf(stderr, "Usage: %s [options]\n", argv0);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "   -S if_name     -- Specify interface for PPPoE Server\n");
    fprintf(stderr, "   -C if_name     -- Specify interface for PPPoE Client\n");
    fprintf(stderr, "   -B if_name     -- Specify interface for both clients and server\n");
    fprintf(stderr, "   -n nsess       -- Maxmimum number of sessions to relay\n");
    fprintf(stderr, "   -i timeout     -- Idle timeout in seconds (0 = no timeout)\n");
    fprintf(stderr, "   -F             -- Do not fork into background\n");
    fprintf(stderr, "   -h             -- Print this help message\n");

    fprintf(stderr, "\nPPPoE Version %s, Copyright (C) 2001-2006 Roaring Penguin Software Inc.\n", VERSION);
    fprintf(stderr, "PPPoE comes with ABSOLUTELY NO WARRANTY.\n");
    fprintf(stderr, "This is free software, and you are welcome to redistribute it under the terms\n");
    fprintf(stderr, "of the GNU General Public License, version 2 or any later version.\n");
    fprintf(stderr, "http://www.roaringpenguin.com\n");
    exit(EXIT_SUCCESS);
}

int
main(int argc, char *argv[])
{
    int opt;
    int nsess = DEFAULT_SESSIONS;
    struct sigaction sa;
    int beDaemon = 1;

    if (getuid() != geteuid() ||
	getgid() != getegid()) {
	fprintf(stderr, "SECURITY WARNING: pppoe-relay will NOT run suid or sgid.  Fix your installation.\n");
	exit(1);
    }


    openlog("pppoe-relay", LOG_PID, LOG_DAEMON);

    while((opt = getopt(argc, argv, "hC:S:B:n:i:F")) != -1) {
	switch(opt) {
	case 'h':
	    usage(argv[0]);
	    break;
	case 'F':
	    beDaemon = 0;
	    break;
	case 'C':
	    addInterface(optarg, 1, 0);
	    break;
	case 'S':
	    addInterface(optarg, 0, 1);
	    break;
	case 'B':
	    addInterface(optarg, 1, 1);
	    break;
	case 'i':
	    if (sscanf(optarg, "%u", &IdleTimeout) != 1) {
		fprintf(stderr, "Illegal argument to -i: should be -i timeout\n");
		exit(EXIT_FAILURE);
	    }
	    CleanPeriod = IdleTimeout / TIMEOUT_DIVISOR;
	    if (CleanPeriod < MIN_CLEAN_PERIOD) CleanPeriod = MIN_CLEAN_PERIOD;
	    break;
	case 'n':
	    if (sscanf(optarg, "%d", &nsess) != 1) {
		fprintf(stderr, "Illegal argument to -n: should be -n #sessions\n");
		exit(EXIT_FAILURE);
	    }
	    if (nsess < 1 || nsess > 65534) {
		fprintf(stderr, "Illegal argument to -n: must range from 1 to 65534\n");
		exit(EXIT_FAILURE);
	    }
	    break;
	default:
	    usage(argv[0]);
	}
    }

#ifdef USE_LINUX_PACKET
#ifndef HAVE_STRUCT_SOCKADDR_LL
    fprintf(stderr, "The PPPoE relay does not work on Linux 2.0 kernels.\n");
    exit(EXIT_FAILURE);
#endif
#endif

    
    if (NumInterfaces < 2) {
	fprintf(stderr, "%s: Must define at least two interfaces\n",
		argv[0]);
	exit(EXIT_FAILURE);
    }

    
    if (pipe(CleanPipe) < 0) {
	fatalSys("pipe");
    }

    
    sa.sa_handler = alarmHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGALRM, &sa, NULL) < 0) {
	fatalSys("sigaction");
    }

    
    initRelay(nsess);

    
    if (beDaemon) {
	int i;
	i = fork();
	if (i < 0) {
	    fatalSys("fork");
	} else if (i != 0) {
	    
	    exit(0);
	}
	setsid();
	signal(SIGHUP, SIG_IGN);
	i = fork();
	if (i < 0) {
	    fatalSys("fork");
	} else if (i != 0) {
	    exit(0);
	}

	chdir("/");
	closelog();
	for (i=0; i<CLOSEFD; i++) {
	    if (!keepDescriptor(i)) {
		close(i);
	    }
	}
	
	openlog("pppoe-relay", LOG_PID, LOG_DAEMON);
    }

    
    if (IdleTimeout) alarm(1);

    
    relayLoop();

    
    return EXIT_FAILURE;
}

void
addInterface(char const *ifname,
	     int clientOK,
	     int acOK)
{
    PPPoEInterface *i;
    int j;
    for (j=0; j<NumInterfaces; j++) {
	if (!strncmp(Interfaces[j].name, ifname, IFNAMSIZ)) {
	    fprintf(stderr, "Interface %s specified more than once.\n", ifname);
	    exit(EXIT_FAILURE);
	}
    }

    if (NumInterfaces >= MAX_INTERFACES) {
	fprintf(stderr, "Too many interfaces (%d max)\n",
		MAX_INTERFACES);
	exit(EXIT_FAILURE);
    }
    i = &Interfaces[NumInterfaces++];
    strncpy(i->name, ifname, IFNAMSIZ);
    i->name[IFNAMSIZ] = 0;

    i->discoverySock = openInterface(ifname, Eth_PPPOE_Discovery, i->mac);
    i->sessionSock   = openInterface(ifname, Eth_PPPOE_Session,   NULL);
    i->clientOK = clientOK;
    i->acOK = acOK;
}

void
initRelay(int nsess)
{
    int i;
    NumSessions = 0;
    MaxSessions = nsess;

    AllSessions = calloc(MaxSessions, sizeof(PPPoESession));
    if (!AllSessions) {
	rp_fatal("Unable to allocate memory for PPPoE session table");
    }
    AllHashes = calloc(MaxSessions*2, sizeof(SessionHash));
    if (!AllHashes) {
	rp_fatal("Unable to allocate memory for PPPoE hash table");
    }

    
    AllSessions[0].prev = NULL;
    if (MaxSessions > 1) {
	AllSessions[0].next = &AllSessions[1];
    } else {
	AllSessions[0].next = NULL;
    }
    for (i=1; i<MaxSessions-1; i++) {
	AllSessions[i].prev = &AllSessions[i-1];
	AllSessions[i].next = &AllSessions[i+1];
    }
    if (MaxSessions > 1) {
	AllSessions[MaxSessions-1].prev = &AllSessions[MaxSessions-2];
	AllSessions[MaxSessions-1].next = NULL;
    }

    FreeSessions = AllSessions;
    ActiveSessions = NULL;

    
    for (i=0; i<MaxSessions; i++) {
	AllSessions[i].sesNum = htons((UINT16_t) i+1);
    }

    
    AllHashes[0].prev = NULL;
    AllHashes[0].next = &AllHashes[1];
    for (i=1; i<2*MaxSessions-1; i++) {
	AllHashes[i].prev = &AllHashes[i-1];
	AllHashes[i].next = &AllHashes[i+1];
    }
    AllHashes[2*MaxSessions-1].prev = &AllHashes[2*MaxSessions-2];
    AllHashes[2*MaxSessions-1].next = NULL;

    FreeHashes = AllHashes;
}

PPPoESession *
createSession(PPPoEInterface const *ac,
	      PPPoEInterface const *cli,
	      unsigned char const *acMac,
	      unsigned char const *cliMac,
	      UINT16_t acSes)
{
    PPPoESession *sess;
    SessionHash *acHash, *cliHash;

    if (NumSessions >= MaxSessions) {
	printErr("Maximum number of sessions reached -- cannot create new session");
	return NULL;
    }

    
    sess = FreeSessions;
    FreeSessions = sess->next;
    NumSessions++;

    
    sess->next = ActiveSessions;
    if (sess->next) {
	sess->next->prev = sess;
    }
    ActiveSessions = sess;
    sess->prev = NULL;

    sess->epoch = Epoch;

    
    acHash = FreeHashes;
    cliHash = acHash->next;
    FreeHashes = cliHash->next;

    acHash->peer = cliHash;
    cliHash->peer = acHash;

    sess->acHash = acHash;
    sess->clientHash = cliHash;

    acHash->interface = ac;
    cliHash->interface = cli;

    memcpy(acHash->peerMac, acMac, ETH_ALEN);
    acHash->sesNum = acSes;
    acHash->ses = sess;

    memcpy(cliHash->peerMac, cliMac, ETH_ALEN);
    cliHash->sesNum = sess->sesNum;
    cliHash->ses = sess;

    addHash(acHash);
    addHash(cliHash);

    
    syslog(LOG_INFO,
	   "Opened session: server=%02x:%02x:%02x:%02x:%02x:%02x(%s:%d), client=%02x:%02x:%02x:%02x:%02x:%02x(%s:%d)",
	   acHash->peerMac[0], acHash->peerMac[1],
	   acHash->peerMac[2], acHash->peerMac[3],
	   acHash->peerMac[4], acHash->peerMac[5],
	   acHash->interface->name,
	   ntohs(acHash->sesNum),
	   cliHash->peerMac[0], cliHash->peerMac[1],
	   cliHash->peerMac[2], cliHash->peerMac[3],
	   cliHash->peerMac[4], cliHash->peerMac[5],
	   cliHash->interface->name,
	   ntohs(cliHash->sesNum));

    return sess;
}

void
freeSession(PPPoESession *ses, char const *msg)
{
    syslog(LOG_INFO,
	   "Closed session: server=%02x:%02x:%02x:%02x:%02x:%02x(%s:%d), client=%02x:%02x:%02x:%02x:%02x:%02x(%s:%d): %s",
	   ses->acHash->peerMac[0], ses->acHash->peerMac[1],
	   ses->acHash->peerMac[2], ses->acHash->peerMac[3],
	   ses->acHash->peerMac[4], ses->acHash->peerMac[5],
	   ses->acHash->interface->name,
	   ntohs(ses->acHash->sesNum),
	   ses->clientHash->peerMac[0], ses->clientHash->peerMac[1],
	   ses->clientHash->peerMac[2], ses->clientHash->peerMac[3],
	   ses->clientHash->peerMac[4], ses->clientHash->peerMac[5],
	   ses->clientHash->interface->name,
	   ntohs(ses->clientHash->sesNum), msg);

    
    if (ses->prev) {
	ses->prev->next = ses->next;
    } else {
	ActiveSessions = ses->next;
    }
    if (ses->next) {
	ses->next->prev = ses->prev;
    }

    ses->next = FreeSessions;
    FreeSessions = ses;

    unhash(ses->acHash);
    unhash(ses->clientHash);
    NumSessions--;
}

void
unhash(SessionHash *sh)
{
    unsigned int b = hash(sh->peerMac, sh->sesNum) % HASHTAB_SIZE;
    if (sh->prev) {
	sh->prev->next = sh->next;
    } else {
	Buckets[b] = sh->next;
    }

    if (sh->next) {
	sh->next->prev = sh->prev;
    }

    
    sh->next = FreeHashes;
    FreeHashes = sh;
}

void
addHash(SessionHash *sh)
{
    unsigned int b = hash(sh->peerMac, sh->sesNum) % HASHTAB_SIZE;
    sh->next = Buckets[b];
    sh->prev = NULL;
    if (sh->next) {
	sh->next->prev = sh;
    }
    Buckets[b] = sh;
}

unsigned int
hash(unsigned char const *mac, UINT16_t sesNum)
{
    unsigned int ans1 =
	((unsigned int) mac[0]) |
	(((unsigned int) mac[1]) << 8) |
	(((unsigned int) mac[2]) << 16) |
	(((unsigned int) mac[3]) << 24);
    unsigned int ans2 =
	((unsigned int) sesNum) |
	(((unsigned int) mac[4]) << 16) |
	(((unsigned int) mac[5]) << 24);
    return ans1 ^ ans2;
}

SessionHash *
findSession(unsigned char const *mac, UINT16_t sesNum)
{
    unsigned int b = hash(mac, sesNum) % HASHTAB_SIZE;
    SessionHash *sh = Buckets[b];
    while(sh) {
	if (!memcmp(mac, sh->peerMac, ETH_ALEN) && sesNum == sh->sesNum) {
	    return sh;
	}
	sh = sh->next;
    }
    return NULL;
}

void
fatalSys(char const *str)
{
    char buf[1024];
    sprintf(buf, "%.256s: %.256s", str, strerror(errno));
    printErr(buf);
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
    exit(EXIT_FAILURE);
}

void
relayLoop()
{
    fd_set readable, readableCopy;
    int maxFD;
    int i, r;
    int sock;

    
    FD_ZERO(&readable);
    maxFD = 0;
    for (i=0; i<NumInterfaces; i++) {
	sock = Interfaces[i].discoverySock;
	if (sock > maxFD) maxFD = sock;
	FD_SET(sock, &readable);
	sock = Interfaces[i].sessionSock;
	if (sock > maxFD) maxFD = sock;
	FD_SET(sock, &readable);
	if (CleanPipe[0] > maxFD) maxFD = CleanPipe[0];
	FD_SET(CleanPipe[0], &readable);
    }
    maxFD++;
    for(;;) {
	readableCopy = readable;
	for(;;) {
	    r = select(maxFD, &readableCopy, NULL, NULL, NULL);
	    if (r >= 0 || errno != EINTR) break;
	}
	if (r < 0) {
	    sysErr("select (relayLoop)");
	    continue;
	}

	
	for (i=0; i<NumInterfaces; i++) {
	    if (FD_ISSET(Interfaces[i].sessionSock, &readableCopy)) {
		relayGotSessionPacket(&Interfaces[i]);
	    }
	}

	
	for (i=0; i<NumInterfaces; i++) {
	    if (FD_ISSET(Interfaces[i].discoverySock, &readableCopy)) {
		relayGotDiscoveryPacket(&Interfaces[i]);
	    }
	}

	
	if (FD_ISSET(CleanPipe[0], &readableCopy)) {
	    char dummy;
	    CleanCounter = 0;
	    read(CleanPipe[0], &dummy, 1);
	    if (IdleTimeout) cleanSessions();
	}
    }
}

void
relayGotDiscoveryPacket(PPPoEInterface const *iface)
{
    PPPoEPacket packet;
    int size;

    if (receivePacket(iface->discoverySock, &packet, &size) < 0) {
	return;
    }
    
    if (packet.ver != 1 || packet.type != 1) {
	return;
    }

    
    if (ntohs(packet.length) + HDR_SIZE > size) {
	syslog(LOG_ERR, "Bogus PPPoE length field (%u)",
	       (unsigned int) ntohs(packet.length));
	return;
    }

    
    if (size > ntohs(packet.length) + HDR_SIZE) {
	size = ntohs(packet.length) + HDR_SIZE;
    }

    switch(packet.code) {
    case CODE_PADT:
	relayHandlePADT(iface, &packet, size);
	break;
    case CODE_PADI:
	relayHandlePADI(iface, &packet, size);
	break;
    case CODE_PADO:
	relayHandlePADO(iface, &packet, size);
	break;
    case CODE_PADR:
	relayHandlePADR(iface, &packet, size);
	break;
    case CODE_PADS:
	relayHandlePADS(iface, &packet, size);
	break;
    default:
	syslog(LOG_ERR, "Discovery packet on %s with unknown code %d",
	       iface->name, (int) packet.code);
    }
}

void
relayGotSessionPacket(PPPoEInterface const *iface)
{
    PPPoEPacket packet;
    int size;
    SessionHash *sh;
    PPPoESession *ses;

    if (receivePacket(iface->sessionSock, &packet, &size) < 0) {
	return;
    }

    
    if (packet.ver != 1 || packet.type != 1) {
	return;
    }

    
    if (packet.code != CODE_SESS) {
	syslog(LOG_ERR, "Session packet with code %d", (int) packet.code);
	return;
    }

    
    if (memcmp(packet.ethHdr.h_dest, iface->mac, ETH_ALEN)) {
	return;
    }

    
    if (ntohs(packet.length) + HDR_SIZE > size) {
	syslog(LOG_ERR, "Bogus PPPoE length field (%u)",
	       (unsigned int) ntohs(packet.length));
	return;
    }

    
    if (size > ntohs(packet.length) + HDR_SIZE) {
	size = ntohs(packet.length) + HDR_SIZE;
    }

    
    sh = findSession(packet.ethHdr.h_source, packet.session);
    if (!sh) {
	return;
    }

    
    ses = sh->ses;
    ses->epoch = Epoch;
    sh = sh->peer;
    packet.session = sh->sesNum;
    memcpy(packet.ethHdr.h_source, sh->interface->mac, ETH_ALEN);
    memcpy(packet.ethHdr.h_dest, sh->peerMac, ETH_ALEN);
#if 0
    fprintf(stderr, "Relaying %02x:%02x:%02x:%02x:%02x:%02x(%s:%d) to %02x:%02x:%02x:%02x:%02x:%02x(%s:%d)\n",
	    sh->peer->peerMac[0], sh->peer->peerMac[1], sh->peer->peerMac[2],
	    sh->peer->peerMac[3], sh->peer->peerMac[4], sh->peer->peerMac[5],
	    sh->peer->interface->name, ntohs(sh->peer->sesNum),
	    sh->peerMac[0], sh->peerMac[1], sh->peerMac[2],
	    sh->peerMac[3], sh->peerMac[4], sh->peerMac[5],
	    sh->interface->name, ntohs(sh->sesNum));
#endif
    sendPacket(NULL, sh->interface->sessionSock, &packet, size);
}

void
relayHandlePADT(PPPoEInterface const *iface,
		PPPoEPacket *packet,
		int size)
{
    SessionHash *sh;
    PPPoESession *ses;

    sh = findSession(packet->ethHdr.h_source, packet->session);
    if (!sh) {
	return;
    }
    
    sh = sh->peer;
    ses = sh->ses;
    packet->session = sh->sesNum;
    memcpy(packet->ethHdr.h_source, sh->interface->mac, ETH_ALEN);
    memcpy(packet->ethHdr.h_dest, sh->peerMac, ETH_ALEN);
    sendPacket(NULL, sh->interface->sessionSock, packet, size);

    
    freeSession(ses, "Received PADT");
}

void
relayHandlePADI(PPPoEInterface const *iface,
		PPPoEPacket *packet,
		int size)
{
    PPPoETag tag;
    unsigned char *loc;
    int i, r;

    int ifIndex;

    
    if (!iface->clientOK) {
	syslog(LOG_ERR,
	       "PADI packet from %02x:%02x:%02x:%02x:%02x:%02x on interface %s not permitted",
	       packet->ethHdr.h_source[0],
	       packet->ethHdr.h_source[1],
	       packet->ethHdr.h_source[2],
	       packet->ethHdr.h_source[3],
	       packet->ethHdr.h_source[4],
	       packet->ethHdr.h_source[5],
	       iface->name);
	return;
    }

    
    if (NOT_UNICAST(packet->ethHdr.h_source)) {
	syslog(LOG_ERR,
	       "PADI packet from %02x:%02x:%02x:%02x:%02x:%02x on interface %s not from a unicast address",
	       packet->ethHdr.h_source[0],
	       packet->ethHdr.h_source[1],
	       packet->ethHdr.h_source[2],
	       packet->ethHdr.h_source[3],
	       packet->ethHdr.h_source[4],
	       packet->ethHdr.h_source[5],
	       iface->name);
	return;
    }

    
    if (NOT_BROADCAST(packet->ethHdr.h_dest)) {
	syslog(LOG_ERR,
	       "PADI packet from %02x:%02x:%02x:%02x:%02x:%02x on interface %s not to a broadcast address",
	       packet->ethHdr.h_source[0],
	       packet->ethHdr.h_source[1],
	       packet->ethHdr.h_source[2],
	       packet->ethHdr.h_source[3],
	       packet->ethHdr.h_source[4],
	       packet->ethHdr.h_source[5],
	       iface->name);
	return;
    }

    
    ifIndex = iface - Interfaces;

    loc = findTag(packet, TAG_RELAY_SESSION_ID, &tag);
    if (!loc) {
	tag.type = htons(TAG_RELAY_SESSION_ID);
	tag.length = htons(MY_RELAY_TAG_LEN);
	memcpy(tag.payload, &ifIndex, sizeof(ifIndex));
	memcpy(tag.payload+sizeof(ifIndex), packet->ethHdr.h_source, ETH_ALEN);
	
	r = addTag(packet, &tag);
	if (r < 0) return;
	size += r;
    } else {
	return;
    }

    for (i=0; i < NumInterfaces; i++) {
	if (iface == &Interfaces[i]) continue;
	if (!Interfaces[i].acOK) continue;
	memcpy(packet->ethHdr.h_source, Interfaces[i].mac, ETH_ALEN);
	sendPacket(NULL, Interfaces[i].discoverySock, packet, size);
    }

}

void
relayHandlePADO(PPPoEInterface const *iface,
		PPPoEPacket *packet,
		int size)
{
    PPPoETag tag;
    unsigned char *loc;
    int ifIndex;
    int acIndex;

    
    if (!iface->acOK) {
	syslog(LOG_ERR,
	       "PADO packet from %02x:%02x:%02x:%02x:%02x:%02x on interface %s not permitted",
	       packet->ethHdr.h_source[0],
	       packet->ethHdr.h_source[1],
	       packet->ethHdr.h_source[2],
	       packet->ethHdr.h_source[3],
	       packet->ethHdr.h_source[4],
	       packet->ethHdr.h_source[5],
	       iface->name);
	return;
    }

    acIndex = iface - Interfaces;

    
    if (NOT_UNICAST(packet->ethHdr.h_source)) {
	syslog(LOG_ERR,
	       "PADO packet from %02x:%02x:%02x:%02x:%02x:%02x on interface %s not from a unicast address",
	       packet->ethHdr.h_source[0],
	       packet->ethHdr.h_source[1],
	       packet->ethHdr.h_source[2],
	       packet->ethHdr.h_source[3],
	       packet->ethHdr.h_source[4],
	       packet->ethHdr.h_source[5],
	       iface->name);
	return;
    }

    
    if (memcmp(packet->ethHdr.h_dest, iface->mac, ETH_ALEN)) {
	return;
    }

    
    loc = findTag(packet, TAG_RELAY_SESSION_ID, &tag);
    if (!loc) {
	syslog(LOG_ERR,
	       "PADO packet from %02x:%02x:%02x:%02x:%02x:%02x on interface %s does not have Relay-Session-Id tag",
	       packet->ethHdr.h_source[0],
	       packet->ethHdr.h_source[1],
	       packet->ethHdr.h_source[2],
	       packet->ethHdr.h_source[3],
	       packet->ethHdr.h_source[4],
	       packet->ethHdr.h_source[5],
	       iface->name);
	return;
    }

    
    if (ntohs(tag.length) != MY_RELAY_TAG_LEN) {
	syslog(LOG_ERR,
	       "PADO packet from %02x:%02x:%02x:%02x:%02x:%02x on interface %s does not have correct length Relay-Session-Id tag",
	       packet->ethHdr.h_source[0],
	       packet->ethHdr.h_source[1],
	       packet->ethHdr.h_source[2],
	       packet->ethHdr.h_source[3],
	       packet->ethHdr.h_source[4],
	       packet->ethHdr.h_source[5],
	       iface->name);
	return;
    }

    
    memcpy(&ifIndex, tag.payload, sizeof(ifIndex));

    if (ifIndex < 0 || ifIndex >= NumInterfaces ||
	!Interfaces[ifIndex].clientOK ||
	iface == &Interfaces[ifIndex]) {
	syslog(LOG_ERR,
	       "PADO packet from %02x:%02x:%02x:%02x:%02x:%02x on interface %s has invalid interface in Relay-Session-Id tag",
	       packet->ethHdr.h_source[0],
	       packet->ethHdr.h_source[1],
	       packet->ethHdr.h_source[2],
	       packet->ethHdr.h_source[3],
	       packet->ethHdr.h_source[4],
	       packet->ethHdr.h_source[5],
	       iface->name);
	return;
    }

    
    memcpy(loc+TAG_HDR_SIZE, &acIndex, sizeof(acIndex));
    memcpy(loc+TAG_HDR_SIZE+sizeof(ifIndex), packet->ethHdr.h_source, ETH_ALEN);

    
    memcpy(packet->ethHdr.h_dest, tag.payload + sizeof(ifIndex), ETH_ALEN);

    
    memcpy(packet->ethHdr.h_source, Interfaces[ifIndex].mac, ETH_ALEN);

    
    sendPacket(NULL, Interfaces[ifIndex].discoverySock, packet, size);
}

void
relayHandlePADR(PPPoEInterface const *iface,
		PPPoEPacket *packet,
		int size)
{
    PPPoETag tag;
    unsigned char *loc;
    int ifIndex;
    int cliIndex;

    
    if (!iface->clientOK) {
	syslog(LOG_ERR,
	       "PADR packet from %02x:%02x:%02x:%02x:%02x:%02x on interface %s not permitted",
	       packet->ethHdr.h_source[0],
	       packet->ethHdr.h_source[1],
	       packet->ethHdr.h_source[2],
	       packet->ethHdr.h_source[3],
	       packet->ethHdr.h_source[4],
	       packet->ethHdr.h_source[5],
	       iface->name);
	return;
    }

    cliIndex = iface - Interfaces;

    
    if (NOT_UNICAST(packet->ethHdr.h_source)) {
	syslog(LOG_ERR,
	       "PADR packet from %02x:%02x:%02x:%02x:%02x:%02x on interface %s not from a unicast address",
	       packet->ethHdr.h_source[0],
	       packet->ethHdr.h_source[1],
	       packet->ethHdr.h_source[2],
	       packet->ethHdr.h_source[3],
	       packet->ethHdr.h_source[4],
	       packet->ethHdr.h_source[5],
	       iface->name);
	return;
    }

    
    if (memcmp(packet->ethHdr.h_dest, iface->mac, ETH_ALEN)) {
	return;
    }

    
    loc = findTag(packet, TAG_RELAY_SESSION_ID, &tag);
    if (!loc) {
	syslog(LOG_ERR,
	       "PADR packet from %02x:%02x:%02x:%02x:%02x:%02x on interface %s does not have Relay-Session-Id tag",
	       packet->ethHdr.h_source[0],
	       packet->ethHdr.h_source[1],
	       packet->ethHdr.h_source[2],
	       packet->ethHdr.h_source[3],
	       packet->ethHdr.h_source[4],
	       packet->ethHdr.h_source[5],
	       iface->name);
	return;
    }

    
    if (ntohs(tag.length) != MY_RELAY_TAG_LEN) {
	syslog(LOG_ERR,
	       "PADR packet from %02x:%02x:%02x:%02x:%02x:%02x on interface %s does not have correct length Relay-Session-Id tag",
	       packet->ethHdr.h_source[0],
	       packet->ethHdr.h_source[1],
	       packet->ethHdr.h_source[2],
	       packet->ethHdr.h_source[3],
	       packet->ethHdr.h_source[4],
	       packet->ethHdr.h_source[5],
	       iface->name);
	return;
    }

    
    memcpy(&ifIndex, tag.payload, sizeof(ifIndex));

    if (ifIndex < 0 || ifIndex >= NumInterfaces ||
	!Interfaces[ifIndex].acOK ||
	iface == &Interfaces[ifIndex]) {
	syslog(LOG_ERR,
	       "PADR packet from %02x:%02x:%02x:%02x:%02x:%02x on interface %s has invalid interface in Relay-Session-Id tag",
	       packet->ethHdr.h_source[0],
	       packet->ethHdr.h_source[1],
	       packet->ethHdr.h_source[2],
	       packet->ethHdr.h_source[3],
	       packet->ethHdr.h_source[4],
	       packet->ethHdr.h_source[5],
	       iface->name);
	return;
    }

    
    memcpy(loc+TAG_HDR_SIZE, &cliIndex, sizeof(cliIndex));
    memcpy(loc+TAG_HDR_SIZE+sizeof(ifIndex), packet->ethHdr.h_source, ETH_ALEN);

    
    memcpy(packet->ethHdr.h_dest, tag.payload + sizeof(ifIndex), ETH_ALEN);

    
    memcpy(packet->ethHdr.h_source, Interfaces[ifIndex].mac, ETH_ALEN);

    
    sendPacket(NULL, Interfaces[ifIndex].discoverySock, packet, size);
}

void
relayHandlePADS(PPPoEInterface const *iface,
		PPPoEPacket *packet,
		int size)
{
    PPPoETag tag;
    unsigned char *loc;
    int ifIndex;
    int acIndex;
    PPPoESession *ses = NULL;
    SessionHash *sh;

    
    if (!iface->acOK) {
	syslog(LOG_ERR,
	       "PADS packet from %02x:%02x:%02x:%02x:%02x:%02x on interface %s not permitted",
	       packet->ethHdr.h_source[0],
	       packet->ethHdr.h_source[1],
	       packet->ethHdr.h_source[2],
	       packet->ethHdr.h_source[3],
	       packet->ethHdr.h_source[4],
	       packet->ethHdr.h_source[5],
	       iface->name);
	return;
    }

    acIndex = iface - Interfaces;

    
    if (NOT_UNICAST(packet->ethHdr.h_source)) {
	syslog(LOG_ERR,
	       "PADS packet from %02x:%02x:%02x:%02x:%02x:%02x on interface %s not from a unicast address",
	       packet->ethHdr.h_source[0],
	       packet->ethHdr.h_source[1],
	       packet->ethHdr.h_source[2],
	       packet->ethHdr.h_source[3],
	       packet->ethHdr.h_source[4],
	       packet->ethHdr.h_source[5],
	       iface->name);
	return;
    }

    
    if (memcmp(packet->ethHdr.h_dest, iface->mac, ETH_ALEN)) {
	return;
    }

    
    loc = findTag(packet, TAG_RELAY_SESSION_ID, &tag);
    if (!loc) {
	syslog(LOG_ERR,
	       "PADS packet from %02x:%02x:%02x:%02x:%02x:%02x on interface %s does not have Relay-Session-Id tag",
	       packet->ethHdr.h_source[0],
	       packet->ethHdr.h_source[1],
	       packet->ethHdr.h_source[2],
	       packet->ethHdr.h_source[3],
	       packet->ethHdr.h_source[4],
	       packet->ethHdr.h_source[5],
	       iface->name);
	return;
    }

    
    if (ntohs(tag.length) != MY_RELAY_TAG_LEN) {
	syslog(LOG_ERR,
	       "PADS packet from %02x:%02x:%02x:%02x:%02x:%02x on interface %s does not have correct length Relay-Session-Id tag",
	       packet->ethHdr.h_source[0],
	       packet->ethHdr.h_source[1],
	       packet->ethHdr.h_source[2],
	       packet->ethHdr.h_source[3],
	       packet->ethHdr.h_source[4],
	       packet->ethHdr.h_source[5],
	       iface->name);
	return;
    }

    
    memcpy(&ifIndex, tag.payload, sizeof(ifIndex));

    if (ifIndex < 0 || ifIndex >= NumInterfaces ||
	!Interfaces[ifIndex].clientOK ||
	iface == &Interfaces[ifIndex]) {
	syslog(LOG_ERR,
	       "PADS packet from %02x:%02x:%02x:%02x:%02x:%02x on interface %s has invalid interface in Relay-Session-Id tag",
	       packet->ethHdr.h_source[0],
	       packet->ethHdr.h_source[1],
	       packet->ethHdr.h_source[2],
	       packet->ethHdr.h_source[3],
	       packet->ethHdr.h_source[4],
	       packet->ethHdr.h_source[5],
	       iface->name);
	return;
    }

    if (packet->session != htons(0)) {
	
	sh = findSession(packet->ethHdr.h_source, packet->session);
	if (sh) ses = sh->ses;


	if (!ses) {
	    
	    ses = createSession(iface, &Interfaces[ifIndex],
				packet->ethHdr.h_source,
				loc + TAG_HDR_SIZE + sizeof(ifIndex), packet->session);
	    if (!ses) {
		PPPoETag hostUniq, *hu;
		if (findTag(packet, TAG_HOST_UNIQ, &hostUniq)) {
		    hu = &hostUniq;
		} else {
		    hu = NULL;
		}
		relaySendError(CODE_PADS, htons(0), &Interfaces[ifIndex],
			       loc + TAG_HDR_SIZE + sizeof(ifIndex),
			       hu, "RP-PPPoE: Relay: Unable to allocate session");
		relaySendError(CODE_PADT, packet->session, iface,
			       packet->ethHdr.h_source, NULL,
			       "RP-PPPoE: Relay: Unable to allocate session");
		return;
	    }
	}
	
	packet->session = ses->sesNum;
    }

    
    removeBytes(packet, loc, MY_RELAY_TAG_LEN + TAG_HDR_SIZE);
    size -= (MY_RELAY_TAG_LEN + TAG_HDR_SIZE);

    
    memcpy(packet->ethHdr.h_dest, tag.payload + sizeof(ifIndex), ETH_ALEN);

    
    memcpy(packet->ethHdr.h_source, Interfaces[ifIndex].mac, ETH_ALEN);

    
    sendPacket(NULL, Interfaces[ifIndex].discoverySock, packet, size);
}

void
relaySendError(unsigned char code,
	       UINT16_t session,
	       PPPoEInterface const *iface,
	       unsigned char const *mac,
	       PPPoETag const *hostUniq,
	       char const *errMsg)
{
    PPPoEPacket packet;
    PPPoETag errTag;
    int size;

    memcpy(packet.ethHdr.h_source, iface->mac, ETH_ALEN);
    memcpy(packet.ethHdr.h_dest, mac, ETH_ALEN);
    packet.ethHdr.h_proto = htons(Eth_PPPOE_Discovery);
    packet.type = 1;
    packet.ver = 1;
    packet.code = code;
    packet.session = session;
    packet.length = htons(0);
    if (hostUniq) {
	if (addTag(&packet, hostUniq) < 0) return;
    }
    errTag.type = htons(TAG_GENERIC_ERROR);
    errTag.length = htons(strlen(errMsg));
    strcpy((char *) errTag.payload, errMsg);
    if (addTag(&packet, &errTag) < 0) return;
    size = ntohs(packet.length) + HDR_SIZE;
    if (code == CODE_PADT) {
	sendPacket(NULL, iface->discoverySock, &packet, size);
    } else {
	sendPacket(NULL, iface->sessionSock, &packet, size);
    }
}

void
alarmHandler(int sig)
{
    alarm(1);
    Epoch++;
    CleanCounter++;
    if (CleanCounter == CleanPeriod) {
	write(CleanPipe[1], "", 1);
    }
}

void cleanSessions(void)
{
    PPPoESession *cur, *next;
    cur = ActiveSessions;
    while(cur) {
	next = cur->next;
	if (Epoch - cur->epoch > IdleTimeout) {
	    
	    relaySendError(CODE_PADT, cur->acHash->sesNum,
			   cur->acHash->interface,
			   cur->acHash->peerMac, NULL,
			   "RP-PPPoE: Relay: Session exceeded idle timeout");
	    relaySendError(CODE_PADT, cur->clientHash->sesNum,
			   cur->clientHash->interface,
			   cur->clientHash->peerMac, NULL,
			   "RP-PPPoE: Relay: Session exceeded idle timeout");
	    freeSession(cur, "Idle Timeout");
	}
	cur = next;
    }
}
