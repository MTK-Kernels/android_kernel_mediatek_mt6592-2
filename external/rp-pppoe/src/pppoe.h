/***********************************************************************
*
* pppoe.h
*
* Declaration of various PPPoE constants
*
* Copyright (C) 2000 Roaring Penguin Software Inc.
*
* This program may be distributed according to the terms of the GNU
* General Public License, version 2 or (at your option) any later version.
*
* LIC: GPL
*
* $Id$
*
***********************************************************************/

#include "config.h"

extern int IsSetID;

#if defined(HAVE_NETPACKET_PACKET_H) || defined(HAVE_LINUX_IF_PACKET_H)
#define _POSIX_SOURCE 1 
#endif

#include <stdio.h>		
#include <sys/types.h>		

#undef USE_LINUX_PACKET
#undef USE_BPF

#if defined(HAVE_NETPACKET_PACKET_H) || defined(HAVE_LINUX_IF_PACKET_H)
#define USE_LINUX_PACKET 1
#elif defined(HAVE_SYS_DLPI_H)
#define USE_DLPI
#elif defined(HAVE_NET_BPF_H)
#define USE_BPF 1
#endif

#if !defined(USE_BPF) && !defined(USE_LINUX_PACKET) && !defined(USE_DLPI)
#error Unknown method for accessing raw Ethernet frames
#endif

#ifdef HAVE_SYS_CDEFS_H
#include <sys/cdefs.h>
#endif

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#if defined(HAVE_LINUX_IF_H)
#include <linux/if.h>
#elif defined(HAVE_NET_IF_H)
#include <net/if.h>
#endif

#ifdef HAVE_NET_IF_TYPES_H
#include <net/if_types.h>
#endif

#ifdef HAVE_NET_IF_DL_H
#include <net/if_dl.h>
#endif

#if defined(__OpenBSD__)
#include <net/ppp_defs.h>
#include <net/if_ppp.h>
#endif

#ifdef USE_BPF
extern int bpfSize;
struct PPPoEPacketStruct;
void sessionDiscoveryPacket(struct PPPoEPacketStruct *packet);
#define BPF_BUFFER_IS_EMPTY (bpfSize <= 0)
#define BPF_BUFFER_HAS_DATA (bpfSize > 0)
#define ethhdr ether_header
#define h_dest ether_dhost
#define h_source ether_shost
#define h_proto ether_type
#define	ETH_DATA_LEN ETHERMTU
#define	ETH_ALEN ETHER_ADDR_LEN
#else
#undef USE_BPF
#define BPF_BUFFER_IS_EMPTY 1
#define BPF_BUFFER_HAS_DATA 0
#endif

#ifdef USE_DLPI
#include <sys/ethernet.h>
#define ethhdr ether_header
#define	ETH_DATA_LEN ETHERMTU
#define	ETH_ALEN ETHERADDRL
#define h_dest ether_dhost.ether_addr_octet
#define h_source ether_shost.ether_addr_octet
#define h_proto ether_type

#define         MAXDLBUF        8192
#define         MAXDLADDR       1024
#define         MAXWAIT         15
#define         OFFADDR(s, n)   (u_char*)((char*)(s) + (int)(n))
#define         CASERET(s)      case s:  return ("s")

#endif

#if SIZEOF_UNSIGNED_SHORT == 2
typedef unsigned short UINT16_t;
#elif SIZEOF_UNSIGNED_INT == 2
typedef unsigned int UINT16_t;
#else
#error Could not find a 16-bit integer type
#endif

#if SIZEOF_UNSIGNED_SHORT == 4
typedef unsigned short UINT32_t;
#elif SIZEOF_UNSIGNED_INT == 4
typedef unsigned int UINT32_t;
#elif SIZEOF_UNSIGNED_LONG == 4
typedef unsigned long UINT32_t;
#else
#error Could not find a 32-bit integer type
#endif

#ifdef HAVE_LINUX_IF_ETHER_H
#include <linux/if_ether.h>
#endif

#include <netinet/in.h>

#ifdef HAVE_NETINET_IF_ETHER_H
#include <sys/types.h>

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifndef HAVE_SYS_DLPI_H
#include <netinet/if_ether.h>
#endif
#endif



#define ETH_PPPOE_DISCOVERY 0x8863
#define ETH_PPPOE_SESSION   0x8864

extern UINT16_t Eth_PPPOE_Discovery;
extern UINT16_t Eth_PPPOE_Session;

extern void switchToRealID(void);
extern void switchToEffectiveID(void);
extern void dropPrivs(void);

#define CODE_PADI           0x09
#define CODE_PADO           0x07
#define CODE_PADR           0x19
#define CODE_PADS           0x65
#define CODE_PADT           0xA7

#define CODE_PADM           0xD3
#define CODE_PADN           0xD4

#define CODE_SESS           0x00

#define TAG_END_OF_LIST        0x0000
#define TAG_SERVICE_NAME       0x0101
#define TAG_AC_NAME            0x0102
#define TAG_HOST_UNIQ          0x0103
#define TAG_AC_COOKIE          0x0104
#define TAG_VENDOR_SPECIFIC    0x0105
#define TAG_RELAY_SESSION_ID   0x0110
#define TAG_SERVICE_NAME_ERROR 0x0201
#define TAG_AC_SYSTEM_ERROR    0x0202
#define TAG_GENERIC_ERROR      0x0203

#define TAG_HURL               0x111
#define TAG_MOTM               0x112
#define TAG_IP_ROUTE_ADD       0x121

#define STATE_SENT_PADI     0
#define STATE_RECEIVED_PADO 1
#define STATE_SENT_PADR     2
#define STATE_SESSION       3
#define STATE_TERMINATED    4

#define MAX_PADI_ATTEMPTS 3

#define PADI_TIMEOUT 5

#define STATE_WAITFOR_FRAME_ADDR 0
#define STATE_DROP_PROTO         1
#define STATE_BUILDING_PACKET    2

#define FRAME_ESC    0x7D
#define FRAME_FLAG   0x7E
#define FRAME_ADDR   0xFF
#define FRAME_CTRL   0x03
#define FRAME_ENC    0x20

#define IPV4ALEN     4
#define SMALLBUF   256

typedef struct PPPoEPacketStruct {
    struct ethhdr ethHdr;	
#ifdef PACK_BITFIELDS_REVERSED
    unsigned int type:4;	
    unsigned int ver:4;		
#else
    unsigned int ver:4;		
    unsigned int type:4;	
#endif
    unsigned int code:8;	
    unsigned int session:16;	
    unsigned int length:16;	
    unsigned char payload[ETH_DATA_LEN]; 
} PPPoEPacket;

#define PPPOE_OVERHEAD 6  
#define HDR_SIZE (sizeof(struct ethhdr) + PPPOE_OVERHEAD)
#define MAX_PPPOE_PAYLOAD (ETH_DATA_LEN - PPPOE_OVERHEAD)
#define MAX_PPPOE_MTU (MAX_PPPOE_PAYLOAD - 2)


typedef struct PPPoETagStruct {
    unsigned int type:16;	
    unsigned int length:16;	
    unsigned char payload[ETH_DATA_LEN]; 
} PPPoETag;
#define TAG_HDR_SIZE 4

#define READ_CHUNK 4096

typedef void ParseFunc(UINT16_t type,
		       UINT16_t len,
		       unsigned char *data,
		       void *extra);

#define PPPINITFCS16    0xffff  


typedef struct PPPoEConnectionStruct {
    int discoveryState;		
    int discoverySocket;	
    int sessionSocket;		
    unsigned char myEth[ETH_ALEN]; 
    unsigned char peerEth[ETH_ALEN]; 
    UINT16_t session;		
    char *ifName;		
    char *serviceName;		
    char *acName;		
    int synchronous;		
    int useHostUniq;		
    int printACNames;		
    int skipDiscovery;		
    int noDiscoverySocket;	
    int killSession;		
    FILE *debugFile;		
    int numPADOs;		
    PPPoETag cookie;		
    PPPoETag relayId;		
    int PADSHadError;           
    int discoveryTimeout;       
} PPPoEConnection;

struct PacketCriteria {
    PPPoEConnection *conn;
    int acNameOK;
    int serviceNameOK;
    int seenACName;
    int seenServiceName;
};

UINT16_t etherType(PPPoEPacket *packet);
int openInterface(char const *ifname, UINT16_t type, unsigned char *hwaddr);
int sendPacket(PPPoEConnection *conn, int sock, PPPoEPacket *pkt, int size);
int receivePacket(int sock, PPPoEPacket *pkt, int *size);
void fatalSys(char const *str);
void rp_fatal(char const *str);
void printErr(char const *str);
void sysErr(char const *str);
#ifdef DEBUGGING_ENABLED
void dumpPacket(FILE *fp, PPPoEPacket *packet, char const *dir);
void dumpHex(FILE *fp, unsigned char const *buf, int len);
#endif
int parsePacket(PPPoEPacket *packet, ParseFunc *func, void *extra);
void parseLogErrs(UINT16_t typ, UINT16_t len, unsigned char *data, void *xtra);
void pktLogErrs(char const *pkt, UINT16_t typ, UINT16_t len, unsigned char *data, void *xtra);
void syncReadFromPPP(PPPoEConnection *conn, PPPoEPacket *packet);
void asyncReadFromPPP(PPPoEConnection *conn, PPPoEPacket *packet);
void asyncReadFromEth(PPPoEConnection *conn, int sock, int clampMss);
void syncReadFromEth(PPPoEConnection *conn, int sock, int clampMss);
char *strDup(char const *str);
void sendPADT(PPPoEConnection *conn, char const *msg);
void sendPADTf(PPPoEConnection *conn, char const *fmt, ...);

void sendSessionPacket(PPPoEConnection *conn,
		       PPPoEPacket *packet, int len);
void initPPP(void);
void clampMSS(PPPoEPacket *packet, char const *dir, int clampMss);
UINT16_t computeTCPChecksum(unsigned char *ipHdr, unsigned char *tcpHdr);
UINT16_t pppFCS16(UINT16_t fcs, unsigned char *cp, int len);
void discovery(PPPoEConnection *conn);
unsigned char *findTag(PPPoEPacket *packet, UINT16_t tagType,
		       PPPoETag *tag);

#define SET_STRING(var, val) do { if (var) free(var); var = strDup(val); } while(0);

#define CHECK_ROOM(cursor, start, len) \
do {\
    if (((cursor)-(start))+(len) > MAX_PPPOE_PAYLOAD) { \
        syslog(LOG_ERR, "Would create too-long packet"); \
        return; \
    } \
} while(0)

#define NOT_UNICAST(e) ((e[0] & 0x01) != 0)
#define BROADCAST(e) ((e[0] & e[1] & e[2] & e[3] & e[4] & e[5]) == 0xFF)
#define NOT_BROADCAST(e) ((e[0] & e[1] & e[2] & e[3] & e[4] & e[5]) != 0xFF)
