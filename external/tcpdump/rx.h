/*
 * Copyright: (c) 2000 United States Government as represented by the
 *	Secretary of the Navy. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in
 *      the documentation and/or other materials provided with the
 *      distribution.
 *   3. The names of the authors may not be used to endorse or promote
 *      products derived from this software without specific prior
 *      written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#define FS_RX_PORT	7000
#define CB_RX_PORT	7001
#define PROT_RX_PORT	7002
#define VLDB_RX_PORT	7003
#define KAUTH_RX_PORT	7004
#define VOL_RX_PORT	7005
#define ERROR_RX_PORT	7006		
#define BOS_RX_PORT	7007

#ifndef AFSNAMEMAX
#define AFSNAMEMAX 256
#endif

#ifndef AFSOPAQUEMAX
#define AFSOPAQUEMAX 1024
#endif

#define PRNAMEMAX 64
#define VLNAMEMAX 65
#define KANAMEMAX 64
#define BOSNAMEMAX 256

#define	PRSFS_READ		1 
#define	PRSFS_WRITE		2 
#define	PRSFS_INSERT		4 
#define	PRSFS_LOOKUP		8 
#define	PRSFS_DELETE		16 
#define	PRSFS_LOCK		32 
#define	PRSFS_ADMINISTER	64 

struct rx_header {
	u_int32_t epoch;
	u_int32_t cid;
	u_int32_t callNumber;
	u_int32_t seq;
	u_int32_t serial;
	u_int8_t type;
#define RX_PACKET_TYPE_DATA		1
#define RX_PACKET_TYPE_ACK		2
#define RX_PACKET_TYPE_BUSY		3
#define RX_PACKET_TYPE_ABORT		4
#define RX_PACKET_TYPE_ACKALL		5
#define RX_PACKET_TYPE_CHALLENGE	6
#define RX_PACKET_TYPE_RESPONSE		7
#define RX_PACKET_TYPE_DEBUG		8
#define RX_PACKET_TYPE_PARAMS		9
#define RX_PACKET_TYPE_VERSION		13
	u_int8_t flags;
#define RX_CLIENT_INITIATED	1
#define RX_REQUEST_ACK		2
#define RX_LAST_PACKET		4
#define RX_MORE_PACKETS		8
#define RX_FREE_PACKET		16
#define RX_SLOW_START_OK	32
#define RX_JUMBO_PACKET		32
	u_int8_t userStatus;
	u_int8_t securityIndex;
	u_int16_t spare;		
	u_int16_t serviceId;		
};					
					
					

#define NUM_RX_FLAGS 7

#define RX_MAXACKS 255

struct rx_ackPacket {
	u_int16_t bufferSpace;		
	u_int16_t maxSkew;		
					
	u_int32_t firstPacket;		
	u_int32_t previousPacket;	
	u_int32_t serial;		
	u_int8_t reason;		
	u_int8_t nAcks;			
	u_int8_t acks[RX_MAXACKS];	
};


#define RX_ACK_TYPE_NACK	0	
#define RX_ACK_TYPE_ACK		1	
