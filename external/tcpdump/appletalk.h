/*
 * Copyright (c) 1988, 1989, 1990, 1993, 1994, 1995, 1996
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * AppleTalk protocol formats (courtesy Bill Croft of Stanford/SUMEX).
 *
 * @(#) $Header: /tcpdump/master/tcpdump/appletalk.h,v 1.16 2004-05-01 09:41:50 hannes Exp $ (LBL)
 */

struct LAP {
	u_int8_t	dst;
	u_int8_t	src;
	u_int8_t	type;
};
#define lapShortDDP	1	
#define lapDDP		2	
#define lapKLAP		'K'	


struct atDDP {
	u_int16_t	length;
	u_int16_t	checksum;
	u_int16_t	dstNet;
	u_int16_t	srcNet;
	u_int8_t	dstNode;
	u_int8_t	srcNode;
	u_int8_t	dstSkt;
	u_int8_t	srcSkt;
	u_int8_t	type;
};

struct atShortDDP {
	u_int16_t	length;
	u_int8_t	dstSkt;
	u_int8_t	srcSkt;
	u_int8_t	type;
};

#define	ddpMaxWKS	0x7F
#define	ddpMaxData	586
#define	ddpLengthMask	0x3FF
#define	ddpHopShift	10
#define	ddpSize		13	
#define	ddpSSize	5
#define	ddpWKS		128	
#define	ddpRTMP		1	
#define	ddpRTMPrequest	5	
#define	ddpNBP		2	
#define	ddpATP		3	
#define	ddpECHO		4	
#define	ddpIP		22	
#define	ddpARP		23	
#define ddpEIGRP        88      
#define	ddpKLAP		0x4b	



struct atATP {
	u_int8_t	control;
	u_int8_t	bitmap;
	u_int16_t	transID;
	int32_t userData;
};

#define	atpReqCode	0x40
#define	atpRspCode	0x80
#define	atpRelCode	0xC0
#define	atpXO		0x20
#define	atpEOM		0x10
#define	atpSTS		0x08
#define	atpFlagMask	0x3F
#define	atpControlMask	0xF8
#define	atpMaxNum	8
#define	atpMaxData	578



struct atEcho {
	u_int8_t	echoFunction;
	u_int8_t	*echoData;
};

#define echoSkt		4		
#define echoSize	1		
#define echoRequest	1		
#define echoReply	2		



struct atNBP {
	u_int8_t	control;
	u_int8_t	id;
};

struct atNBPtuple {
	u_int16_t	net;
	u_int8_t	node;
	u_int8_t	skt;
	u_int8_t	enumerator;
};

#define	nbpBrRq		0x10
#define	nbpLkUp		0x20
#define	nbpLkUpReply	0x30

#define	nbpNIS		2
#define	nbpTupleMax	15

#define	nbpHeaderSize	2
#define nbpTupleSize	5

#define nbpSkt		2		



#define	rtmpSkt		1	
#define	rtmpSize	4	
#define	rtmpTupleSize	3



struct zipHeader {
	u_int8_t	command;
	u_int8_t	netcount;
};

#define	zipHeaderSize	2
#define	zipQuery	1
#define	zipReply	2
#define	zipTakedown	3
#define	zipBringup	4
#define	ddpZIP		6
#define	zipSkt		6
#define	GetMyZone	7
#define	GetZoneList	8

#define atalk_port(p) \
	(((unsigned)((p) - 16512) < 128) || \
	 ((unsigned)((p) - 200) < 128) || \
	 ((unsigned)((p) - 768) < 128))
