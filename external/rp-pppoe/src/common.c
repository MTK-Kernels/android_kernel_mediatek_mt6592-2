/***********************************************************************
*
* common.c
*
* Implementation of user-space PPPoE redirector for Linux.
*
* Common functions used by PPPoE client and server
*
* Copyright (C) 2000 by Roaring Penguin Software Inc.
*
* This program may be distributed according to the terms of the GNU
* General Public License, version 2 or (at your option) any later version.
*
* LIC: GPL
*
***********************************************************************/

static char const RCSID[] =
"$Id$";
#define _ISOC99_SOURCE 1

#define _BSD_SOURCE 1

#include "pppoe.h"


#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif

#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdarg.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <sys/types.h>
#include <pwd.h>

int IsSetID = 0;

static uid_t saved_uid = -2;
static uid_t saved_gid = -2;

int
parsePacket(PPPoEPacket *packet, ParseFunc *func, void *extra)
{
    UINT16_t len = ntohs(packet->length);
    unsigned char *curTag;
    UINT16_t tagType, tagLen;

    if (packet->ver != 1) {
	syslog(LOG_ERR, "Invalid PPPoE version (%d)", (int) packet->ver);
	return -1;
    }
    if (packet->type != 1) {
	syslog(LOG_ERR, "Invalid PPPoE type (%d)", (int) packet->type);
	return -1;
    }

    
    if (len > ETH_DATA_LEN - 6) { 
	syslog(LOG_ERR, "Invalid PPPoE packet length (%u)", len);
	return -1;
    }

    
    curTag = packet->payload;
    while(curTag - packet->payload < len) {
	
	tagType = (((UINT16_t) curTag[0]) << 8) +
	    (UINT16_t) curTag[1];
	tagLen = (((UINT16_t) curTag[2]) << 8) +
	    (UINT16_t) curTag[3];
	if (tagType == TAG_END_OF_LIST) {
	    return 0;
	}
	if ((curTag - packet->payload) + tagLen + TAG_HDR_SIZE > len) {
	    syslog(LOG_ERR, "Invalid PPPoE tag length (%u)", tagLen);
	    return -1;
	}
	func(tagType, tagLen, curTag+TAG_HDR_SIZE, extra);
	curTag = curTag + TAG_HDR_SIZE + tagLen;
    }
    return 0;
}

unsigned char *
findTag(PPPoEPacket *packet, UINT16_t type, PPPoETag *tag)
{
    UINT16_t len = ntohs(packet->length);
    unsigned char *curTag;
    UINT16_t tagType, tagLen;

    if (packet->ver != 1) {
	syslog(LOG_ERR, "Invalid PPPoE version (%d)", (int) packet->ver);
	return NULL;
    }
    if (packet->type != 1) {
	syslog(LOG_ERR, "Invalid PPPoE type (%d)", (int) packet->type);
	return NULL;
    }

    
    if (len > ETH_DATA_LEN - 6) { 
	syslog(LOG_ERR, "Invalid PPPoE packet length (%u)", len);
	return NULL;
    }

    
    curTag = packet->payload;
    while(curTag - packet->payload < len) {
	
	tagType = (((UINT16_t) curTag[0]) << 8) +
	    (UINT16_t) curTag[1];
	tagLen = (((UINT16_t) curTag[2]) << 8) +
	    (UINT16_t) curTag[3];
	if (tagType == TAG_END_OF_LIST) {
	    return NULL;
	}
	if ((curTag - packet->payload) + tagLen + TAG_HDR_SIZE > len) {
	    syslog(LOG_ERR, "Invalid PPPoE tag length (%u)", tagLen);
	    return NULL;
	}
	if (tagType == type) {
	    memcpy(tag, curTag, tagLen + TAG_HDR_SIZE);
	    return curTag;
	}
	curTag = curTag + TAG_HDR_SIZE + tagLen;
    }
    return NULL;
}

void
switchToRealID (void) {
    if (IsSetID) {
	if (saved_uid < 0) saved_uid = geteuid();
	if (saved_gid < 0) saved_gid = getegid();
	if (setegid(getgid()) < 0) {
	    printErr("setgid failed");
	    exit(EXIT_FAILURE);
	}
	if (seteuid(getuid()) < 0) {
	    printErr("seteuid failed");
	    exit(EXIT_FAILURE);
	}
    }
}

void
switchToEffectiveID (void) {
    if (IsSetID) {
	if (setegid(saved_gid) < 0) {
	    printErr("setgid failed");
	    exit(EXIT_FAILURE);
	}
	if (seteuid(saved_uid) < 0) {
	    printErr("seteuid failed");
	    exit(EXIT_FAILURE);
	}
    }
}

void
dropPrivs(void)
{
    struct passwd *pw = NULL;
    int ok = 0;
    if (geteuid() == 0) {
	pw = getpwnam("nobody");
	if (pw) {
	    if (setgid(pw->pw_gid) < 0) ok++;
	    if (setuid(pw->pw_uid) < 0) ok++;
	}
    }
    if (ok < 2 && IsSetID) {
	setegid(getgid());
	seteuid(getuid());
    }
}

void
printErr(char const *str)
{
    fprintf(stderr, "pppoe: %s\n", str);
    syslog(LOG_ERR, "%s", str);
}


char *
strDup(char const *str)
{
    char *copy = malloc(strlen(str)+1);
    if (!copy) {
	rp_fatal("strdup failed");
    }
    strcpy(copy, str);
    return copy;
}

UINT16_t
computeTCPChecksum(unsigned char *ipHdr, unsigned char *tcpHdr)
{
    UINT32_t sum = 0;
    UINT16_t count = ipHdr[2] * 256 + ipHdr[3];
    UINT16_t tmp;

    unsigned char *addr = tcpHdr;
    unsigned char pseudoHeader[12];

    
    count -= (ipHdr[0] & 0x0F) * 4;

    memcpy(pseudoHeader, ipHdr+12, 8);
    pseudoHeader[8] = 0;
    pseudoHeader[9] = ipHdr[9];
    pseudoHeader[10] = (count >> 8) & 0xFF;
    pseudoHeader[11] = (count & 0xFF);

    
    sum += * (UINT16_t *) pseudoHeader;
    sum += * ((UINT16_t *) (pseudoHeader+2));
    sum += * ((UINT16_t *) (pseudoHeader+4));
    sum += * ((UINT16_t *) (pseudoHeader+6));
    sum += * ((UINT16_t *) (pseudoHeader+8));
    sum += * ((UINT16_t *) (pseudoHeader+10));

    
    while (count > 1) {
	memcpy(&tmp, addr, sizeof(tmp));
	sum += (UINT32_t) tmp;
	addr += sizeof(tmp);
	count -= sizeof(tmp);
    }
    if (count > 0) {
	sum += (unsigned char) *addr;
    }

    while(sum >> 16) {
	sum = (sum & 0xffff) + (sum >> 16);
    }
    return (UINT16_t) ((~sum) & 0xFFFF);
}

void
clampMSS(PPPoEPacket *packet, char const *dir, int clampMss)
{
    unsigned char *tcpHdr;
    unsigned char *ipHdr;
    unsigned char *opt;
    unsigned char *endHdr;
    unsigned char *mssopt = NULL;
    UINT16_t csum;

    int len, minlen;

    
    if (packet->payload[0] & 0x01) {
        

        
        if (packet->payload[0] != 0x21) {
            
            return;
        }

        ipHdr = packet->payload + 1;
	minlen = 41;
    } else {
        

        
        if (packet->payload[0] != 0x00 ||
            packet->payload[1] != 0x21) {
            
            return;
        }

        ipHdr = packet->payload + 2;
	minlen = 42;
    }

    
    len = (int) ntohs(packet->length);
    if (len < minlen) {
	
	return;
    }

    
    if ((ipHdr[0] & 0xF0) != 0x40) {
	return;
    }

    
    if ((ipHdr[6] & 0x1F) || ipHdr[7]) {
	
	return;
    }
    
    if (ipHdr[9] != 0x06) {
	return;
    }

    
    tcpHdr = ipHdr + (ipHdr[0] & 0x0F) * 4;

    
    if (!(tcpHdr[13] & 0x02)) {
	return;
    }

    csum = computeTCPChecksum(ipHdr, tcpHdr);
    if (csum) {
	syslog(LOG_ERR, "Bad TCP checksum %x", (unsigned int) csum);

	
	return;
    }

    
    endHdr = tcpHdr + ((tcpHdr[12] & 0xF0) >> 2);
    opt = tcpHdr + 20;
    while (opt < endHdr) {
	if (!*opt) break;	
	switch(*opt) {
	case 1:
	    opt++;
	    break;

	case 2:
	    if (opt[1] != 4) {
		
		syslog(LOG_ERR,
		       "Bogus length for MSS option (%u) from %u.%u.%u.%u",
		       (unsigned int) opt[1],
		       (unsigned int) ipHdr[12],
		       (unsigned int) ipHdr[13],
		       (unsigned int) ipHdr[14],
		       (unsigned int) ipHdr[15]);
		return;
	    }
	    mssopt = opt;
	    break;
	default:
	    if (opt[1] < 2) {
		
		syslog(LOG_ERR,
		       "Bogus TCP option length (%u) from %u.%u.%u.%u",
		       (unsigned int) opt[1],
		       (unsigned int) ipHdr[12],
		       (unsigned int) ipHdr[13],
		       (unsigned int) ipHdr[14],
		       (unsigned int) ipHdr[15]);
		return;
	    }
	    opt += (opt[1]);
	    break;
	}
	
	if (mssopt) break;
    }

    
    if (mssopt) {
	unsigned mss = mssopt[2] * 256 + mssopt[3];
	if (mss <= clampMss) {
	    return;
	}

	mssopt[2] = (((unsigned) clampMss) >> 8) & 0xFF;
	mssopt[3] = ((unsigned) clampMss) & 0xFF;
    } else {
	
	return;
    }

    
    tcpHdr[16] = 0;
    tcpHdr[17] = 0;
    csum = computeTCPChecksum(ipHdr, tcpHdr);
    (* (UINT16_t *) (tcpHdr+16)) = csum;
}

void
sendPADT(PPPoEConnection *conn, char const *msg)
{
    PPPoEPacket packet;
    unsigned char *cursor = packet.payload;

    UINT16_t plen = 0;

    
    if (!conn->session) return;

    
    if (conn->discoverySocket < 0) return;

    memcpy(packet.ethHdr.h_dest, conn->peerEth, ETH_ALEN);
    memcpy(packet.ethHdr.h_source, conn->myEth, ETH_ALEN);

    packet.ethHdr.h_proto = htons(Eth_PPPOE_Discovery);
    packet.ver = 1;
    packet.type = 1;
    packet.code = CODE_PADT;
    packet.session = conn->session;

    conn->session = 0;

    
    if (conn->useHostUniq) {
	PPPoETag hostUniq;
	pid_t pid = getpid();
	hostUniq.type = htons(TAG_HOST_UNIQ);
	hostUniq.length = htons(sizeof(pid));
	memcpy(hostUniq.payload, &pid, sizeof(pid));
	memcpy(cursor, &hostUniq, sizeof(pid) + TAG_HDR_SIZE);
	cursor += sizeof(pid) + TAG_HDR_SIZE;
	plen += sizeof(pid) + TAG_HDR_SIZE;
    }

    
    if (msg) {
	PPPoETag err;
	size_t elen = strlen(msg);
	err.type = htons(TAG_GENERIC_ERROR);
	err.length = htons(elen);
	strcpy((char *) err.payload, msg);
	memcpy(cursor, &err, elen + TAG_HDR_SIZE);
	cursor += elen + TAG_HDR_SIZE;
	plen += elen + TAG_HDR_SIZE;
    }

    
    if (conn->cookie.type) {
	CHECK_ROOM(cursor, packet.payload,
		   ntohs(conn->cookie.length) + TAG_HDR_SIZE);
	memcpy(cursor, &conn->cookie, ntohs(conn->cookie.length) + TAG_HDR_SIZE);
	cursor += ntohs(conn->cookie.length) + TAG_HDR_SIZE;
	plen += ntohs(conn->cookie.length) + TAG_HDR_SIZE;
    }

    if (conn->relayId.type) {
	CHECK_ROOM(cursor, packet.payload,
		   ntohs(conn->relayId.length) + TAG_HDR_SIZE);
	memcpy(cursor, &conn->relayId, ntohs(conn->relayId.length) + TAG_HDR_SIZE);
	cursor += ntohs(conn->relayId.length) + TAG_HDR_SIZE;
	plen += ntohs(conn->relayId.length) + TAG_HDR_SIZE;
    }

    packet.length = htons(plen);
    sendPacket(conn, conn->discoverySocket, &packet, (int) (plen + HDR_SIZE));
#ifdef DEBUGGING_ENABLED
    if (conn->debugFile) {
	dumpPacket(conn->debugFile, &packet, "SENT");
	fprintf(conn->debugFile, "\n");
	fflush(conn->debugFile);
    }
#endif
    syslog(LOG_INFO,"Sent PADT");
}

void
sendPADTf(PPPoEConnection *conn, char const *fmt, ...)
{
    char msg[512];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);
    msg[511] = 0;

    sendPADT(conn, msg);
}

void
pktLogErrs(char const *pkt,
	   UINT16_t type, UINT16_t len, unsigned char *data,
	   void *extra)
{
    char const *str;
    char const *fmt = "%s: %s: %.*s";
    switch(type) {
    case TAG_SERVICE_NAME_ERROR:
	str = "Service-Name-Error";
	break;
    case TAG_AC_SYSTEM_ERROR:
	str = "System-Error";
	break;
    default:
	str = "Generic-Error";
    }

    syslog(LOG_ERR, fmt, pkt, str, (int) len, data);
    fprintf(stderr, fmt, pkt, str, (int) len, data);
    fprintf(stderr, "\n");
}

void
parseLogErrs(UINT16_t type, UINT16_t len, unsigned char *data,
	     void *extra)
{
    pktLogErrs("PADT", type, len, data, extra);
}
