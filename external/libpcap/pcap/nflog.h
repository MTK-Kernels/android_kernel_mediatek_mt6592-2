/*
 * Copyright (c) 2013, Petar Alilovic,
 * Faculty of Electrical Engineering and Computing, University of Zagreb
 * All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice,
 *	 this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *	 notice, this list of conditions and the following disclaimer in the
 *	 documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#ifndef _PCAP_NFLOG_H__
#define _PCAP_NFLOG_H__

typedef struct nflog_hdr {
	u_int8_t	nflog_family;		
	u_int8_t	nflog_version;		
	u_int16_t	nflog_rid;		
} nflog_hdr_t;

typedef struct nflog_tlv {
	u_int16_t	tlv_length;		
	u_int16_t	tlv_type;		
	
} nflog_tlv_t;

typedef struct nflog_packet_hdr {
	u_int16_t	hw_protocol;	
	u_int8_t	hook;		
	u_int8_t	pad;		
} nflog_packet_hdr_t;

typedef struct nflog_hwaddr {
	u_int16_t	hw_addrlen;	
	u_int16_t	pad;		
	u_int8_t	hw_addr[8];	
} nflog_hwaddr_t;

typedef struct nflog_timestamp {
	u_int64_t	sec;
	u_int64_t	usec;
} nflog_timestamp_t;

#define NFULA_PACKET_HDR		1	
#define NFULA_MARK			2	
#define NFULA_TIMESTAMP			3	
#define NFULA_IFINDEX_INDEV		4	
#define NFULA_IFINDEX_OUTDEV		5	
#define NFULA_IFINDEX_PHYSINDEV		6	
#define NFULA_IFINDEX_PHYSOUTDEV	7	
#define NFULA_HWADDR			8	
#define NFULA_PAYLOAD			9	
#define NFULA_PREFIX			10	
#define NFULA_UID			11	
#define NFULA_SEQ			12	
#define NFULA_SEQ_GLOBAL		13	
#define NFULA_GID			14	
#define NFULA_HWTYPE			15	
#define NFULA_HWHEADER			16	
#define NFULA_HWLEN			17	

#endif
