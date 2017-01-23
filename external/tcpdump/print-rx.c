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

#ifndef lint
static const char rcsid[] _U_ =
    "@(#) $Header: /tcpdump/master/tcpdump/print-rx.c,v 1.42 2008-07-01 07:44:50 guy Exp $";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tcpdump-stdinc.h>

#include "interface.h"
#include "addrtoname.h"
#include "extract.h"

#include "rx.h"

#include "ip.h"

static const struct tok rx_types[] = {
	{ RX_PACKET_TYPE_DATA,		"data" },
	{ RX_PACKET_TYPE_ACK,		"ack" },
	{ RX_PACKET_TYPE_BUSY,		"busy" },
	{ RX_PACKET_TYPE_ABORT,		"abort" },
	{ RX_PACKET_TYPE_ACKALL,	"ackall" },
	{ RX_PACKET_TYPE_CHALLENGE,	"challenge" },
	{ RX_PACKET_TYPE_RESPONSE,	"response" },
	{ RX_PACKET_TYPE_DEBUG,		"debug" },
	{ RX_PACKET_TYPE_PARAMS,	"params" },
	{ RX_PACKET_TYPE_VERSION,	"version" },
	{ 0,				NULL },
};

static struct double_tok {
	int flag;		
	int packetType;		
	const char *s;		
} rx_flags[] = {
	{ RX_CLIENT_INITIATED,	0,			"client-init" },
	{ RX_REQUEST_ACK,	0,			"req-ack" },
	{ RX_LAST_PACKET,	0,			"last-pckt" },
	{ RX_MORE_PACKETS,	0,			"more-pckts" },
	{ RX_FREE_PACKET,	0,			"free-pckt" },
	{ RX_SLOW_START_OK,	RX_PACKET_TYPE_ACK,	"slow-start" },
	{ RX_JUMBO_PACKET,	RX_PACKET_TYPE_DATA,	"jumbogram" }
};

static const struct tok fs_req[] = {
	{ 130,		"fetch-data" },
	{ 131,		"fetch-acl" },
	{ 132,		"fetch-status" },
	{ 133,		"store-data" },
	{ 134,		"store-acl" },
	{ 135,		"store-status" },
	{ 136,		"remove-file" },
	{ 137,		"create-file" },
	{ 138,		"rename" },
	{ 139,		"symlink" },
	{ 140,		"link" },
	{ 141,		"makedir" },
	{ 142,		"rmdir" },
	{ 143,		"oldsetlock" },
	{ 144,		"oldextlock" },
	{ 145,		"oldrellock" },
	{ 146,		"get-stats" },
	{ 147,		"give-cbs" },
	{ 148,		"get-vlinfo" },
	{ 149,		"get-vlstats" },
	{ 150,		"set-vlstats" },
	{ 151,		"get-rootvl" },
	{ 152,		"check-token" },
	{ 153,		"get-time" },
	{ 154,		"nget-vlinfo" },
	{ 155,		"bulk-stat" },
	{ 156,		"setlock" },
	{ 157,		"extlock" },
	{ 158,		"rellock" },
	{ 159,		"xstat-ver" },
	{ 160,		"get-xstat" },
	{ 161,		"dfs-lookup" },
	{ 162,		"dfs-flushcps" },
	{ 163,		"dfs-symlink" },
	{ 220,		"residency" },
	{ 65536,        "inline-bulk-status" },
	{ 65537,        "fetch-data-64" },
	{ 65538,        "store-data-64" },
	{ 65539,        "give-up-all-cbs" },
	{ 65540,        "get-caps" },
	{ 65541,        "cb-rx-conn-addr" },
	{ 0,		NULL },
};

static const struct tok cb_req[] = {
	{ 204,		"callback" },
	{ 205,		"initcb" },
	{ 206,		"probe" },
	{ 207,		"getlock" },
	{ 208,		"getce" },
	{ 209,		"xstatver" },
	{ 210,		"getxstat" },
	{ 211,		"initcb2" },
	{ 212,		"whoareyou" },
	{ 213,		"initcb3" },
	{ 214,		"probeuuid" },
	{ 215,		"getsrvprefs" },
	{ 216,		"getcellservdb" },
	{ 217,		"getlocalcell" },
	{ 218,		"getcacheconf" },
	{ 65536,        "getce64" },
	{ 65537,        "getcellbynum" },
	{ 65538,        "tellmeaboutyourself" },
	{ 0,		NULL },
};

static const struct tok pt_req[] = {
	{ 500,		"new-user" },
	{ 501,		"where-is-it" },
	{ 502,		"dump-entry" },
	{ 503,		"add-to-group" },
	{ 504,		"name-to-id" },
	{ 505,		"id-to-name" },
	{ 506,		"delete" },
	{ 507,		"remove-from-group" },
	{ 508,		"get-cps" },
	{ 509,		"new-entry" },
	{ 510,		"list-max" },
	{ 511,		"set-max" },
	{ 512,		"list-entry" },
	{ 513,		"change-entry" },
	{ 514,		"list-elements" },
	{ 515,		"same-mbr-of" },
	{ 516,		"set-fld-sentry" },
	{ 517,		"list-owned" },
	{ 518,		"get-cps2" },
	{ 519,		"get-host-cps" },
	{ 520,		"update-entry" },
	{ 521,		"list-entries" },
	{ 530,		"list-super-groups" },
	{ 0,		NULL },
};

static const struct tok vldb_req[] = {
	{ 501,		"create-entry" },
	{ 502,		"delete-entry" },
	{ 503,		"get-entry-by-id" },
	{ 504,		"get-entry-by-name" },
	{ 505,		"get-new-volume-id" },
	{ 506,		"replace-entry" },
	{ 507,		"update-entry" },
	{ 508,		"setlock" },
	{ 509,		"releaselock" },
	{ 510,		"list-entry" },
	{ 511,		"list-attrib" },
	{ 512,		"linked-list" },
	{ 513,		"get-stats" },
	{ 514,		"probe" },
	{ 515,		"get-addrs" },
	{ 516,		"change-addr" },
	{ 517,		"create-entry-n" },
	{ 518,		"get-entry-by-id-n" },
	{ 519,		"get-entry-by-name-n" },
	{ 520,		"replace-entry-n" },
	{ 521,		"list-entry-n" },
	{ 522,		"list-attrib-n" },
	{ 523,		"linked-list-n" },
	{ 524,		"update-entry-by-name" },
	{ 525,		"create-entry-u" },
	{ 526,		"get-entry-by-id-u" },
	{ 527,		"get-entry-by-name-u" },
	{ 528,		"replace-entry-u" },
	{ 529,		"list-entry-u" },
	{ 530,		"list-attrib-u" },
	{ 531,		"linked-list-u" },
	{ 532,		"regaddr" },
	{ 533,		"get-addrs-u" },
	{ 534,		"list-attrib-n2" },
	{ 0,		NULL },
};

static const struct tok kauth_req[] = {
	{ 1,		"auth-old" },
	{ 21,		"authenticate" },
	{ 22,		"authenticate-v2" },
	{ 2,		"change-pw" },
	{ 3,		"get-ticket-old" },
	{ 23,		"get-ticket" },
	{ 4,		"set-pw" },
	{ 5,		"set-fields" },
	{ 6,		"create-user" },
	{ 7,		"delete-user" },
	{ 8,		"get-entry" },
	{ 9,		"list-entry" },
	{ 10,		"get-stats" },
	{ 11,		"debug" },
	{ 12,		"get-pw" },
	{ 13,		"get-random-key" },
	{ 14,		"unlock" },
	{ 15,		"lock-status" },
	{ 0,		NULL },
};

static const struct tok vol_req[] = {
	{ 100,		"create-volume" },
	{ 101,		"delete-volume" },
	{ 102,		"restore" },
	{ 103,		"forward" },
	{ 104,		"end-trans" },
	{ 105,		"clone" },
	{ 106,		"set-flags" },
	{ 107,		"get-flags" },
	{ 108,		"trans-create" },
	{ 109,		"dump" },
	{ 110,		"get-nth-volume" },
	{ 111,		"set-forwarding" },
	{ 112,		"get-name" },
	{ 113,		"get-status" },
	{ 114,		"sig-restore" },
	{ 115,		"list-partitions" },
	{ 116,		"list-volumes" },
	{ 117,		"set-id-types" },
	{ 118,		"monitor" },
	{ 119,		"partition-info" },
	{ 120,		"reclone" },
	{ 121,		"list-one-volume" },
	{ 122,		"nuke" },
	{ 123,		"set-date" },
	{ 124,		"x-list-volumes" },
	{ 125,		"x-list-one-volume" },
	{ 126,		"set-info" },
	{ 127,		"x-list-partitions" },
	{ 128,		"forward-multiple" },
	{ 65536,	"convert-ro" },
	{ 65537,	"get-size" },
	{ 65538,	"dump-v2" },
	{ 0,		NULL },
};

static const struct tok bos_req[] = {
	{ 80,		"create-bnode" },
	{ 81,		"delete-bnode" },
	{ 82,		"set-status" },
	{ 83,		"get-status" },
	{ 84,		"enumerate-instance" },
	{ 85,		"get-instance-info" },
	{ 86,		"get-instance-parm" },
	{ 87,		"add-superuser" },
	{ 88,		"delete-superuser" },
	{ 89,		"list-superusers" },
	{ 90,		"list-keys" },
	{ 91,		"add-key" },
	{ 92,		"delete-key" },
	{ 93,		"set-cell-name" },
	{ 94,		"get-cell-name" },
	{ 95,		"get-cell-host" },
	{ 96,		"add-cell-host" },
	{ 97,		"delete-cell-host" },
	{ 98,		"set-t-status" },
	{ 99,		"shutdown-all" },
	{ 100,		"restart-all" },
	{ 101,		"startup-all" },
	{ 102,		"set-noauth-flag" },
	{ 103,		"re-bozo" },
	{ 104,		"restart" },
	{ 105,		"start-bozo-install" },
	{ 106,		"uninstall" },
	{ 107,		"get-dates" },
	{ 108,		"exec" },
	{ 109,		"prune" },
	{ 110,		"set-restart-time" },
	{ 111,		"get-restart-time" },
	{ 112,		"start-bozo-log" },
	{ 113,		"wait-all" },
	{ 114,		"get-instance-strings" },
	{ 115,		"get-restricted" },
	{ 116,		"set-restricted" },
	{ 0,		NULL },
};

static const struct tok ubik_req[] = {
	{ 10000,	"vote-beacon" },
	{ 10001,	"vote-debug-old" },
	{ 10002,	"vote-sdebug-old" },
	{ 10003,	"vote-getsyncsite" },
	{ 10004,	"vote-debug" },
	{ 10005,	"vote-sdebug" },
	{ 10006,	"vote-xdebug" },
	{ 10007,	"vote-xsdebug" },
	{ 20000,	"disk-begin" },
	{ 20001,	"disk-commit" },
	{ 20002,	"disk-lock" },
	{ 20003,	"disk-write" },
	{ 20004,	"disk-getversion" },
	{ 20005,	"disk-getfile" },
	{ 20006,	"disk-sendfile" },
	{ 20007,	"disk-abort" },
	{ 20008,	"disk-releaselocks" },
	{ 20009,	"disk-truncate" },
	{ 20010,	"disk-probe" },
	{ 20011,	"disk-writev" },
	{ 20012,	"disk-interfaceaddr" },
	{ 20013,	"disk-setversion" },
	{ 0,		NULL },
};

#define VOTE_LOW	10000
#define VOTE_HIGH	10007
#define DISK_LOW	20000
#define DISK_HIGH	20013

static const struct tok cb_types[] = {
	{ 1,		"exclusive" },
	{ 2,		"shared" },
	{ 3,		"dropped" },
	{ 0,		NULL },
};

static const struct tok ubik_lock_types[] = {
	{ 1,		"read" },
	{ 2,		"write" },
	{ 3,		"wait" },
	{ 0,		NULL },
};

static const char *voltype[] = { "read-write", "read-only", "backup" };

static const struct tok afs_fs_errors[] = {
	{ 101,		"salvage volume" },
	{ 102, 		"no such vnode" },
	{ 103, 		"no such volume" },
	{ 104, 		"volume exist" },
	{ 105, 		"no service" },
	{ 106, 		"volume offline" },
	{ 107, 		"voline online" },
	{ 108, 		"diskfull" },
	{ 109, 		"diskquota exceeded" },
	{ 110, 		"volume busy" },
	{ 111, 		"volume moved" },
	{ 112, 		"AFS IO error" },
	{ -100,		"restarting fileserver" },
	{ 0,		NULL }
};


static const struct tok rx_ack_reasons[] = {
	{ 1,		"ack requested" },
	{ 2,		"duplicate packet" },
	{ 3,		"out of sequence" },
	{ 4,		"exceeds window" },
	{ 5,		"no buffer space" },
	{ 6,		"ping" },
	{ 7,		"ping response" },
	{ 8,		"delay" },
	{ 9,		"idle" },
	{ 0,		NULL },
};


struct rx_cache_entry {
	u_int32_t	callnum;	
	struct in_addr	client;		
	struct in_addr	server;		
	int		dport;		
	u_short		serviceId;	
	u_int32_t	opcode;		
};

#define RX_CACHE_SIZE	64

static struct rx_cache_entry	rx_cache[RX_CACHE_SIZE];

static int	rx_cache_next = 0;
static int	rx_cache_hint = 0;
static void	rx_cache_insert(const u_char *, const struct ip *, int);
static int	rx_cache_find(const struct rx_header *, const struct ip *,
			      int, int32_t *);

static void fs_print(const u_char *, int);
static void fs_reply_print(const u_char *, int, int32_t);
static void acl_print(u_char *, int, u_char *);
static void cb_print(const u_char *, int);
static void cb_reply_print(const u_char *, int, int32_t);
static void prot_print(const u_char *, int);
static void prot_reply_print(const u_char *, int, int32_t);
static void vldb_print(const u_char *, int);
static void vldb_reply_print(const u_char *, int, int32_t);
static void kauth_print(const u_char *, int);
static void kauth_reply_print(const u_char *, int, int32_t);
static void vol_print(const u_char *, int);
static void vol_reply_print(const u_char *, int, int32_t);
static void bos_print(const u_char *, int);
static void bos_reply_print(const u_char *, int, int32_t);
static void ubik_print(const u_char *);
static void ubik_reply_print(const u_char *, int, int32_t);

static void rx_ack_print(const u_char *, int);

static int is_ubik(u_int32_t);


void
rx_print(register const u_char *bp, int length, int sport, int dport,
	 u_char *bp2)
{
	register struct rx_header *rxh;
	int i;
	int32_t opcode;

	if (snapend - bp < (int)sizeof (struct rx_header)) {
		printf(" [|rx] (%d)", length);
		return;
	}

	rxh = (struct rx_header *) bp;

	printf(" rx %s", tok2str(rx_types, "type %d", rxh->type));

	if (vflag) {
		int firstflag = 0;

		if (vflag > 1)
			printf(" cid %08x call# %d",
			       (int) EXTRACT_32BITS(&rxh->cid),
			       (int) EXTRACT_32BITS(&rxh->callNumber));

		printf(" seq %d ser %d",
		       (int) EXTRACT_32BITS(&rxh->seq),
		       (int) EXTRACT_32BITS(&rxh->serial));

		if (vflag > 2)
			printf(" secindex %d serviceid %hu",
				(int) rxh->securityIndex,
				EXTRACT_16BITS(&rxh->serviceId));

		if (vflag > 1)
			for (i = 0; i < NUM_RX_FLAGS; i++) {
				if (rxh->flags & rx_flags[i].flag &&
				    (!rx_flags[i].packetType ||
				     rxh->type == rx_flags[i].packetType)) {
					if (!firstflag) {
						firstflag = 1;
						printf(" ");
					} else {
						printf(",");
					}
					printf("<%s>", rx_flags[i].s);
				}
			}
	}


	if (rxh->type == RX_PACKET_TYPE_DATA &&
	    EXTRACT_32BITS(&rxh->seq) == 1 &&
	    rxh->flags & RX_CLIENT_INITIATED) {


		rx_cache_insert(bp, (const struct ip *) bp2, dport);

		switch (dport) {
			case FS_RX_PORT:	
				fs_print(bp, length);
				break;
			case CB_RX_PORT:	
				cb_print(bp, length);
				break;
			case PROT_RX_PORT:	
				prot_print(bp, length);
				break;
			case VLDB_RX_PORT:	
				vldb_print(bp, length);
				break;
			case KAUTH_RX_PORT:	
				kauth_print(bp, length);
				break;
			case VOL_RX_PORT:	
				vol_print(bp, length);
				break;
			case BOS_RX_PORT:	
				bos_print(bp, length);
				break;
			default:
				;
		}


	} else if (((rxh->type == RX_PACKET_TYPE_DATA &&
					EXTRACT_32BITS(&rxh->seq) == 1) ||
		    rxh->type == RX_PACKET_TYPE_ABORT) &&
		   (rxh->flags & RX_CLIENT_INITIATED) == 0 &&
		   rx_cache_find(rxh, (const struct ip *) bp2,
				 sport, &opcode)) {

		switch (sport) {
			case FS_RX_PORT:	
				fs_reply_print(bp, length, opcode);
				break;
			case CB_RX_PORT:	
				cb_reply_print(bp, length, opcode);
				break;
			case PROT_RX_PORT:	
				prot_reply_print(bp, length, opcode);
				break;
			case VLDB_RX_PORT:	
				vldb_reply_print(bp, length, opcode);
				break;
			case KAUTH_RX_PORT:	
				kauth_reply_print(bp, length, opcode);
				break;
			case VOL_RX_PORT:	
				vol_reply_print(bp, length, opcode);
				break;
			case BOS_RX_PORT:	
				bos_reply_print(bp, length, opcode);
				break;
			default:
				;
		}


	} else if (rxh->type == RX_PACKET_TYPE_ACK)
		rx_ack_print(bp, length);


	printf(" (%d)", length);
}


static void
rx_cache_insert(const u_char *bp, const struct ip *ip, int dport)
{
	struct rx_cache_entry *rxent;
	const struct rx_header *rxh = (const struct rx_header *) bp;

	if (snapend - bp + 1 <= (int)(sizeof(struct rx_header) + sizeof(int32_t)))
		return;

	rxent = &rx_cache[rx_cache_next];

	if (++rx_cache_next >= RX_CACHE_SIZE)
		rx_cache_next = 0;

	rxent->callnum = rxh->callNumber;
	rxent->client = ip->ip_src;
	rxent->server = ip->ip_dst;
	rxent->dport = dport;
	rxent->serviceId = rxh->serviceId;
	rxent->opcode = EXTRACT_32BITS(bp + sizeof(struct rx_header));
}


static int
rx_cache_find(const struct rx_header *rxh, const struct ip *ip, int sport,
	      int32_t *opcode)
{
	int i;
	struct rx_cache_entry *rxent;
	u_int32_t clip = ip->ip_dst.s_addr;
	u_int32_t sip = ip->ip_src.s_addr;

	

	i = rx_cache_hint;
	do {
		rxent = &rx_cache[i];
		if (rxent->callnum == rxh->callNumber &&
		    rxent->client.s_addr == clip &&
		    rxent->server.s_addr == sip &&
		    rxent->serviceId == rxh->serviceId &&
		    rxent->dport == sport) {

			

			rx_cache_hint = i;
			*opcode = rxent->opcode;
			return(1);
		}
		if (++i >= RX_CACHE_SIZE)
			i = 0;
	} while (i != rx_cache_hint);

	
	return(0);
}


#define FIDOUT() { unsigned long n1, n2, n3; \
			TCHECK2(bp[0], sizeof(int32_t) * 3); \
			n1 = EXTRACT_32BITS(bp); \
			bp += sizeof(int32_t); \
			n2 = EXTRACT_32BITS(bp); \
			bp += sizeof(int32_t); \
			n3 = EXTRACT_32BITS(bp); \
			bp += sizeof(int32_t); \
			printf(" fid %d/%d/%d", (int) n1, (int) n2, (int) n3); \
		}

#define STROUT(MAX) { unsigned int i; \
			TCHECK2(bp[0], sizeof(int32_t)); \
			i = EXTRACT_32BITS(bp); \
			if (i > (MAX)) \
				goto trunc; \
			bp += sizeof(int32_t); \
			printf(" \""); \
			if (fn_printn(bp, i, snapend)) \
				goto trunc; \
			printf("\""); \
			bp += ((i + sizeof(int32_t) - 1) / sizeof(int32_t)) * sizeof(int32_t); \
		}

#define INTOUT() { int i; \
			TCHECK2(bp[0], sizeof(int32_t)); \
			i = (int) EXTRACT_32BITS(bp); \
			bp += sizeof(int32_t); \
			printf(" %d", i); \
		}

#define UINTOUT() { unsigned long i; \
			TCHECK2(bp[0], sizeof(int32_t)); \
			i = EXTRACT_32BITS(bp); \
			bp += sizeof(int32_t); \
			printf(" %lu", i); \
		}

#define UINT64OUT() { u_int64_t i; \
			TCHECK2(bp[0], sizeof(u_int64_t)); \
			i = EXTRACT_64BITS(bp); \
			bp += sizeof(u_int64_t); \
			printf(" %" PRIu64, i); \
		}

#define DATEOUT() { time_t t; struct tm *tm; char str[256]; \
			TCHECK2(bp[0], sizeof(int32_t)); \
			t = (time_t) EXTRACT_32BITS(bp); \
			bp += sizeof(int32_t); \
			tm = localtime(&t); \
			strftime(str, 256, "%Y/%m/%d %T", tm); \
			printf(" %s", str); \
		}

#define STOREATTROUT() { unsigned long mask, i; \
			TCHECK2(bp[0], (sizeof(int32_t)*6)); \
			mask = EXTRACT_32BITS(bp); bp += sizeof(int32_t); \
			if (mask) printf (" StoreStatus"); \
		        if (mask & 1) { printf(" date"); DATEOUT(); } \
			else bp += sizeof(int32_t); \
			i = EXTRACT_32BITS(bp); bp += sizeof(int32_t); \
		        if (mask & 2) printf(" owner %lu", i);  \
			i = EXTRACT_32BITS(bp); bp += sizeof(int32_t); \
		        if (mask & 4) printf(" group %lu", i); \
			i = EXTRACT_32BITS(bp); bp += sizeof(int32_t); \
		        if (mask & 8) printf(" mode %lo", i & 07777); \
			i = EXTRACT_32BITS(bp); bp += sizeof(int32_t); \
		        if (mask & 16) printf(" segsize %lu", i); \
			 \
		        if (mask & 1024) printf(" fsync");  \
		}

#define UBIK_VERSIONOUT() {int32_t epoch; int32_t counter; \
			TCHECK2(bp[0], sizeof(int32_t) * 2); \
			epoch = EXTRACT_32BITS(bp); \
			bp += sizeof(int32_t); \
			counter = EXTRACT_32BITS(bp); \
			bp += sizeof(int32_t); \
			printf(" %d.%d", epoch, counter); \
		}

#define AFSUUIDOUT() {u_int32_t temp; int i; \
			TCHECK2(bp[0], 11*sizeof(u_int32_t)); \
			temp = EXTRACT_32BITS(bp); \
			bp += sizeof(u_int32_t); \
			printf(" %08x", temp); \
			temp = EXTRACT_32BITS(bp); \
			bp += sizeof(u_int32_t); \
			printf("%04x", temp); \
			temp = EXTRACT_32BITS(bp); \
			bp += sizeof(u_int32_t); \
			printf("%04x", temp); \
			for (i = 0; i < 8; i++) { \
				temp = EXTRACT_32BITS(bp); \
				bp += sizeof(u_int32_t); \
				printf("%02x", (unsigned char) temp); \
			} \
		}


#define VECOUT(MAX) { u_char *sp; \
			u_char s[AFSNAMEMAX]; \
			int k; \
			if ((MAX) + 1 > sizeof(s)) \
				goto trunc; \
			TCHECK2(bp[0], (MAX) * sizeof(int32_t)); \
			sp = s; \
			for (k = 0; k < (MAX); k++) { \
				*sp++ = (u_char) EXTRACT_32BITS(bp); \
				bp += sizeof(int32_t); \
			} \
			s[(MAX)] = '\0'; \
			printf(" \""); \
			fn_print(s, NULL); \
			printf("\""); \
		}

#define DESTSERVEROUT() { unsigned long n1, n2, n3; \
			TCHECK2(bp[0], sizeof(int32_t) * 3); \
			n1 = EXTRACT_32BITS(bp); \
			bp += sizeof(int32_t); \
			n2 = EXTRACT_32BITS(bp); \
			bp += sizeof(int32_t); \
			n3 = EXTRACT_32BITS(bp); \
			bp += sizeof(int32_t); \
			printf(" server %d:%d:%d", (int) n1, (int) n2, (int) n3); \
		}


static void
fs_print(register const u_char *bp, int length)
{
	int fs_op;
	unsigned long i;

	if (length <= (int)sizeof(struct rx_header))
		return;

	if (snapend - bp + 1 <= (int)(sizeof(struct rx_header) + sizeof(int32_t))) {
		goto trunc;
	}


	fs_op = EXTRACT_32BITS(bp + sizeof(struct rx_header));

	printf(" fs call %s", tok2str(fs_req, "op#%d", fs_op));


	bp += sizeof(struct rx_header) + 4;


	switch (fs_op) {
		case 130:	
			FIDOUT();
			printf(" offset");
			UINTOUT();
			printf(" length");
			UINTOUT();
			break;
		case 131:	
		case 132:	
		case 143:	
		case 144:	
		case 145:	
		case 156:	
		case 157:	
		case 158:	
			FIDOUT();
			break;
		case 135:	
			FIDOUT();
			STOREATTROUT();
			break;
		case 133:	
			FIDOUT();
			STOREATTROUT();
			printf(" offset");
			UINTOUT();
			printf(" length");
			UINTOUT();
			printf(" flen");
			UINTOUT();
			break;
		case 134:	
		{
			char a[AFSOPAQUEMAX+1];
			FIDOUT();
			TCHECK2(bp[0], 4);
			i = EXTRACT_32BITS(bp);
			bp += sizeof(int32_t);
			TCHECK2(bp[0], i);
			i = min(AFSOPAQUEMAX, i);
			strncpy(a, (char *) bp, i);
			a[i] = '\0';
			acl_print((u_char *) a, sizeof(a), (u_char *) a + i);
			break;
		}
		case 137:	
		case 141:	
			FIDOUT();
			STROUT(AFSNAMEMAX);
			STOREATTROUT();
			break;
		case 136:	
		case 142:	
			FIDOUT();
			STROUT(AFSNAMEMAX);
			break;
		case 138:	
			printf(" old");
			FIDOUT();
			STROUT(AFSNAMEMAX);
			printf(" new");
			FIDOUT();
			STROUT(AFSNAMEMAX);
			break;
		case 139:	
			FIDOUT();
			STROUT(AFSNAMEMAX);
			printf(" link to");
			STROUT(AFSNAMEMAX);
			break;
		case 140:	
			FIDOUT();
			STROUT(AFSNAMEMAX);
			printf(" link to");
			FIDOUT();
			break;
		case 148:	
			STROUT(AFSNAMEMAX);
			break;
		case 149:	
		case 150:	
			printf(" volid");
			UINTOUT();
			break;
		case 154:	
			printf(" volname");
			STROUT(AFSNAMEMAX);
			break;
		case 155:	
		case 65536:     
		{
			unsigned long j;
			TCHECK2(bp[0], 4);
			j = EXTRACT_32BITS(bp);
			bp += sizeof(int32_t);

			for (i = 0; i < j; i++) {
				FIDOUT();
				if (i != j - 1)
					printf(",");
			}
			if (j == 0)
				printf(" <none!>");
		}
		case 65537:	
			FIDOUT();
			printf(" offset");
			UINT64OUT();
			printf(" length");
			UINT64OUT();
			break;
		case 65538:	
			FIDOUT();
			STOREATTROUT();
			printf(" offset");
			UINT64OUT();
			printf(" length");
			UINT64OUT();
			printf(" flen");
			UINT64OUT();
			break;
		case 65541:    
			printf(" addr");
			UINTOUT();
		default:
			;
	}

	return;

trunc:
	printf(" [|fs]");
}


static void
fs_reply_print(register const u_char *bp, int length, int32_t opcode)
{
	unsigned long i;
	struct rx_header *rxh;

	if (length <= (int)sizeof(struct rx_header))
		return;

	rxh = (struct rx_header *) bp;


	printf(" fs reply %s", tok2str(fs_req, "op#%d", opcode));

	bp += sizeof(struct rx_header);


	if (rxh->type == RX_PACKET_TYPE_DATA) {
		switch (opcode) {
		case 131:	
		{
			char a[AFSOPAQUEMAX+1];
			TCHECK2(bp[0], 4);
			i = EXTRACT_32BITS(bp);
			bp += sizeof(int32_t);
			TCHECK2(bp[0], i);
			i = min(AFSOPAQUEMAX, i);
			strncpy(a, (char *) bp, i);
			a[i] = '\0';
			acl_print((u_char *) a, sizeof(a), (u_char *) a + i);
			break;
		}
		case 137:	
		case 141:	
			printf(" new");
			FIDOUT();
			break;
		case 151:	
			printf(" root volume");
			STROUT(AFSNAMEMAX);
			break;
		case 153:	
			DATEOUT();
			break;
		default:
			;
		}
	} else if (rxh->type == RX_PACKET_TYPE_ABORT) {
		int i;

		TCHECK2(bp[0], sizeof(int32_t));
		i = (int) EXTRACT_32BITS(bp);
		bp += sizeof(int32_t);

		printf(" error %s", tok2str(afs_fs_errors, "#%d", i));
	} else {
		printf(" strange fs reply of type %d", rxh->type);
	}

	return;

trunc:
	printf(" [|fs]");
}


static void
acl_print(u_char *s, int maxsize, u_char *end)
{
	int pos, neg, acl;
	int n, i;
	char *user;
	char fmt[1024];

	if ((user = (char *)malloc(maxsize)) == NULL)
		return;

	if (sscanf((char *) s, "%d %d\n%n", &pos, &neg, &n) != 2)
		goto finish;

	s += n;

	if (s > end)
		goto finish;


#define ACLOUT(acl) \
	if (acl & PRSFS_READ) \
		printf("r"); \
	if (acl & PRSFS_LOOKUP) \
		printf("l"); \
	if (acl & PRSFS_INSERT) \
		printf("i"); \
	if (acl & PRSFS_DELETE) \
		printf("d"); \
	if (acl & PRSFS_WRITE) \
		printf("w"); \
	if (acl & PRSFS_LOCK) \
		printf("k"); \
	if (acl & PRSFS_ADMINISTER) \
		printf("a");

	for (i = 0; i < pos; i++) {
		snprintf(fmt, sizeof(fmt), "%%%ds %%d\n%%n", maxsize - 1);
		if (sscanf((char *) s, fmt, user, &acl, &n) != 2)
			goto finish;
		s += n;
		printf(" +{");
		fn_print((u_char *)user, NULL);
		printf(" ");
		ACLOUT(acl);
		printf("}");
		if (s > end)
			goto finish;
	}

	for (i = 0; i < neg; i++) {
		snprintf(fmt, sizeof(fmt), "%%%ds %%d\n%%n", maxsize - 1);
		if (sscanf((char *) s, fmt, user, &acl, &n) != 2)
			goto finish;
		s += n;
		printf(" -{");
		fn_print((u_char *)user, NULL);
		printf(" ");
		ACLOUT(acl);
		printf("}");
		if (s > end)
			goto finish;
	}

finish:
	free(user);
	return;
}

#undef ACLOUT


static void
cb_print(register const u_char *bp, int length)
{
	int cb_op;
	unsigned long i;

	if (length <= (int)sizeof(struct rx_header))
		return;

	if (snapend - bp + 1 <= (int)(sizeof(struct rx_header) + sizeof(int32_t))) {
		goto trunc;
	}


	cb_op = EXTRACT_32BITS(bp + sizeof(struct rx_header));

	printf(" cb call %s", tok2str(cb_req, "op#%d", cb_op));

	bp += sizeof(struct rx_header) + 4;


	switch (cb_op) {
		case 204:		
		{
			unsigned long j, t;
			TCHECK2(bp[0], 4);
			j = EXTRACT_32BITS(bp);
			bp += sizeof(int32_t);

			for (i = 0; i < j; i++) {
				FIDOUT();
				if (i != j - 1)
					printf(",");
			}

			if (j == 0)
				printf(" <none!>");

			j = EXTRACT_32BITS(bp);
			bp += sizeof(int32_t);

			if (j != 0)
				printf(";");

			for (i = 0; i < j; i++) {
				printf(" ver");
				INTOUT();
				printf(" expires");
				DATEOUT();
				TCHECK2(bp[0], 4);
				t = EXTRACT_32BITS(bp);
				bp += sizeof(int32_t);
				tok2str(cb_types, "type %d", t);
			}
		}
		case 214: {
			printf(" afsuuid");
			AFSUUIDOUT();
			break;
		}
		default:
			;
	}

	return;

trunc:
	printf(" [|cb]");
}


static void
cb_reply_print(register const u_char *bp, int length, int32_t opcode)
{
	struct rx_header *rxh;

	if (length <= (int)sizeof(struct rx_header))
		return;

	rxh = (struct rx_header *) bp;


	printf(" cb reply %s", tok2str(cb_req, "op#%d", opcode));

	bp += sizeof(struct rx_header);


	if (rxh->type == RX_PACKET_TYPE_DATA)
		switch (opcode) {
		case 213:	
			AFSUUIDOUT();
			break;
		default:
		;
		}
	else {
		printf(" errcode");
		INTOUT();
	}

	return;

trunc:
	printf(" [|cb]");
}


static void
prot_print(register const u_char *bp, int length)
{
	unsigned long i;
	int pt_op;

	if (length <= (int)sizeof(struct rx_header))
		return;

	if (snapend - bp + 1 <= (int)(sizeof(struct rx_header) + sizeof(int32_t))) {
		goto trunc;
	}


	pt_op = EXTRACT_32BITS(bp + sizeof(struct rx_header));

	printf(" pt");

	if (is_ubik(pt_op)) {
		ubik_print(bp);
		return;
	}

	printf(" call %s", tok2str(pt_req, "op#%d", pt_op));


	bp += sizeof(struct rx_header) + 4;

	switch (pt_op) {
		case 500:	
			STROUT(PRNAMEMAX);
			printf(" id");
			INTOUT();
			printf(" oldid");
			INTOUT();
			break;
		case 501:	
		case 506:	
		case 508:	
		case 512:	
		case 514:	
		case 517:	
		case 518:	
		case 519:	
		case 530:	
			printf(" id");
			INTOUT();
			break;
		case 502:	
			printf(" pos");
			INTOUT();
			break;
		case 503:	
		case 507:	
		case 515:	
			printf(" uid");
			INTOUT();
			printf(" gid");
			INTOUT();
			break;
		case 504:	
		{
			unsigned long j;
			TCHECK2(bp[0], 4);
			j = EXTRACT_32BITS(bp);
			bp += sizeof(int32_t);


			for (i = 0; i < j; i++) {
				VECOUT(PRNAMEMAX);
			}
			if (j == 0)
				printf(" <none!>");
		}
			break;
		case 505:	
		{
			unsigned long j;
			printf(" ids:");
			TCHECK2(bp[0], 4);
			i = EXTRACT_32BITS(bp);
			bp += sizeof(int32_t);
			for (j = 0; j < i; j++)
				INTOUT();
			if (j == 0)
				printf(" <none!>");
		}
			break;
		case 509:	
			STROUT(PRNAMEMAX);
			printf(" flag");
			INTOUT();
			printf(" oid");
			INTOUT();
			break;
		case 511:	
			printf(" id");
			INTOUT();
			printf(" gflag");
			INTOUT();
			break;
		case 513:	
			printf(" id");
			INTOUT();
			STROUT(PRNAMEMAX);
			printf(" oldid");
			INTOUT();
			printf(" newid");
			INTOUT();
			break;
		case 520:	
			printf(" id");
			INTOUT();
			STROUT(PRNAMEMAX);
			break;
		default:
			;
	}


	return;

trunc:
	printf(" [|pt]");
}


static void
prot_reply_print(register const u_char *bp, int length, int32_t opcode)
{
	struct rx_header *rxh;
	unsigned long i;

	if (length < (int)sizeof(struct rx_header))
		return;

	rxh = (struct rx_header *) bp;


	printf(" pt");

	if (is_ubik(opcode)) {
		ubik_reply_print(bp, length, opcode);
		return;
	}

	printf(" reply %s", tok2str(pt_req, "op#%d", opcode));

	bp += sizeof(struct rx_header);


	if (rxh->type == RX_PACKET_TYPE_DATA)
		switch (opcode) {
		case 504:		
		{
			unsigned long j;
			printf(" ids:");
			TCHECK2(bp[0], 4);
			i = EXTRACT_32BITS(bp);
			bp += sizeof(int32_t);
			for (j = 0; j < i; j++)
				INTOUT();
			if (j == 0)
				printf(" <none!>");
		}
			break;
		case 505:		
		{
			unsigned long j;
			TCHECK2(bp[0], 4);
			j = EXTRACT_32BITS(bp);
			bp += sizeof(int32_t);


			for (i = 0; i < j; i++) {
				VECOUT(PRNAMEMAX);
			}
			if (j == 0)
				printf(" <none!>");
		}
			break;
		case 508:		
		case 514:		
		case 517:		
		case 518:		
		case 519:		
		{
			unsigned long j;
			TCHECK2(bp[0], 4);
			j = EXTRACT_32BITS(bp);
			bp += sizeof(int32_t);
			for (i = 0; i < j; i++) {
				INTOUT();
			}
			if (j == 0)
				printf(" <none!>");
		}
			break;
		case 510:		
			printf(" maxuid");
			INTOUT();
			printf(" maxgid");
			INTOUT();
			break;
		default:
			;
		}
	else {
		printf(" errcode");
		INTOUT();
	}

	return;

trunc:
	printf(" [|pt]");
}


static void
vldb_print(register const u_char *bp, int length)
{
	int vldb_op;
	unsigned long i;

	if (length <= (int)sizeof(struct rx_header))
		return;

	if (snapend - bp + 1 <= (int)(sizeof(struct rx_header) + sizeof(int32_t))) {
		goto trunc;
	}


	vldb_op = EXTRACT_32BITS(bp + sizeof(struct rx_header));

	printf(" vldb");

	if (is_ubik(vldb_op)) {
		ubik_print(bp);
		return;
	}
	printf(" call %s", tok2str(vldb_req, "op#%d", vldb_op));


	bp += sizeof(struct rx_header) + 4;

	switch (vldb_op) {
		case 501:	
		case 517:	
			VECOUT(VLNAMEMAX);
			break;
		case 502:	
		case 503:	
		case 507:	
		case 508:	
		case 509:	
		case 518:	
			printf(" volid");
			INTOUT();
			TCHECK2(bp[0], sizeof(int32_t));
			i = EXTRACT_32BITS(bp);
			bp += sizeof(int32_t);
			if (i <= 2)
				printf(" type %s", voltype[i]);
			break;
		case 504:	
		case 519:	
		case 524:	
		case 527:	
			STROUT(VLNAMEMAX);
			break;
		case 505:	
			printf(" bump");
			INTOUT();
			break;
		case 506:	
		case 520:	
			printf(" volid");
			INTOUT();
			TCHECK2(bp[0], sizeof(int32_t));
			i = EXTRACT_32BITS(bp);
			bp += sizeof(int32_t);
			if (i <= 2)
				printf(" type %s", voltype[i]);
			VECOUT(VLNAMEMAX);
			break;
		case 510:	
		case 521:	
			printf(" index");
			INTOUT();
			break;
		default:
			;
	}

	return;

trunc:
	printf(" [|vldb]");
}


static void
vldb_reply_print(register const u_char *bp, int length, int32_t opcode)
{
	struct rx_header *rxh;
	unsigned long i;

	if (length < (int)sizeof(struct rx_header))
		return;

	rxh = (struct rx_header *) bp;


	printf(" vldb");

	if (is_ubik(opcode)) {
		ubik_reply_print(bp, length, opcode);
		return;
	}

	printf(" reply %s", tok2str(vldb_req, "op#%d", opcode));

	bp += sizeof(struct rx_header);


	if (rxh->type == RX_PACKET_TYPE_DATA)
		switch (opcode) {
		case 510:	
			printf(" count");
			INTOUT();
			printf(" nextindex");
			INTOUT();
		case 503:	
		case 504:	
		{	unsigned long nservers, j;
			VECOUT(VLNAMEMAX);
			TCHECK2(bp[0], sizeof(int32_t));
			bp += sizeof(int32_t);
			printf(" numservers");
			TCHECK2(bp[0], sizeof(int32_t));
			nservers = EXTRACT_32BITS(bp);
			bp += sizeof(int32_t);
			printf(" %lu", nservers);
			printf(" servers");
			for (i = 0; i < 8; i++) {
				TCHECK2(bp[0], sizeof(int32_t));
				if (i < nservers)
					printf(" %s",
					   intoa(((struct in_addr *) bp)->s_addr));
				bp += sizeof(int32_t);
			}
			printf(" partitions");
			for (i = 0; i < 8; i++) {
				TCHECK2(bp[0], sizeof(int32_t));
				j = EXTRACT_32BITS(bp);
				if (i < nservers && j <= 26)
					printf(" %c", 'a' + (int)j);
				else if (i < nservers)
					printf(" %lu", j);
				bp += sizeof(int32_t);
			}
			TCHECK2(bp[0], 8 * sizeof(int32_t));
			bp += 8 * sizeof(int32_t);
			printf(" rwvol");
			UINTOUT();
			printf(" rovol");
			UINTOUT();
			printf(" backup");
			UINTOUT();
		}
			break;
		case 505:	
			printf(" newvol");
			UINTOUT();
			break;
		case 521:	
		case 529:	
			printf(" count");
			INTOUT();
			printf(" nextindex");
			INTOUT();
		case 518:	
		case 519:	
		{	unsigned long nservers, j;
			VECOUT(VLNAMEMAX);
			printf(" numservers");
			TCHECK2(bp[0], sizeof(int32_t));
			nservers = EXTRACT_32BITS(bp);
			bp += sizeof(int32_t);
			printf(" %lu", nservers);
			printf(" servers");
			for (i = 0; i < 13; i++) {
				TCHECK2(bp[0], sizeof(int32_t));
				if (i < nservers)
					printf(" %s",
					   intoa(((struct in_addr *) bp)->s_addr));
				bp += sizeof(int32_t);
			}
			printf(" partitions");
			for (i = 0; i < 13; i++) {
				TCHECK2(bp[0], sizeof(int32_t));
				j = EXTRACT_32BITS(bp);
				if (i < nservers && j <= 26)
					printf(" %c", 'a' + (int)j);
				else if (i < nservers)
					printf(" %lu", j);
				bp += sizeof(int32_t);
			}
			TCHECK2(bp[0], 13 * sizeof(int32_t));
			bp += 13 * sizeof(int32_t);
			printf(" rwvol");
			UINTOUT();
			printf(" rovol");
			UINTOUT();
			printf(" backup");
			UINTOUT();
		}
			break;
		case 526:	
		case 527:	
		{	unsigned long nservers, j;
			VECOUT(VLNAMEMAX);
			printf(" numservers");
			TCHECK2(bp[0], sizeof(int32_t));
			nservers = EXTRACT_32BITS(bp);
			bp += sizeof(int32_t);
			printf(" %lu", nservers);
			printf(" servers");
			for (i = 0; i < 13; i++) {
				if (i < nservers) {
					printf(" afsuuid");
					AFSUUIDOUT();
				} else {
					TCHECK2(bp[0], 44);
					bp += 44;
				}
			}
			TCHECK2(bp[0], 4 * 13);
			bp += 4 * 13;
			printf(" partitions");
			for (i = 0; i < 13; i++) {
				TCHECK2(bp[0], sizeof(int32_t));
				j = EXTRACT_32BITS(bp);
				if (i < nservers && j <= 26)
					printf(" %c", 'a' + (int)j);
				else if (i < nservers)
					printf(" %lu", j);
				bp += sizeof(int32_t);
			}
			TCHECK2(bp[0], 13 * sizeof(int32_t));
			bp += 13 * sizeof(int32_t);
			printf(" rwvol");
			UINTOUT();
			printf(" rovol");
			UINTOUT();
			printf(" backup");
			UINTOUT();
		}
		default:
			;
		}

	else {
		printf(" errcode");
		INTOUT();
	}

	return;

trunc:
	printf(" [|vldb]");
}


static void
kauth_print(register const u_char *bp, int length)
{
	int kauth_op;

	if (length <= (int)sizeof(struct rx_header))
		return;

	if (snapend - bp + 1 <= (int)(sizeof(struct rx_header) + sizeof(int32_t))) {
		goto trunc;
	}


	kauth_op = EXTRACT_32BITS(bp + sizeof(struct rx_header));

	printf(" kauth");

	if (is_ubik(kauth_op)) {
		ubik_print(bp);
		return;
	}


	printf(" call %s", tok2str(kauth_req, "op#%d", kauth_op));


	bp += sizeof(struct rx_header) + 4;

	switch (kauth_op) {
		case 1:		;
		case 21:	
		case 22:	
		case 2:		
		case 5:		
		case 6:		
		case 7:		
		case 8:		
		case 14:	
		case 15:	
			printf(" principal");
			STROUT(KANAMEMAX);
			STROUT(KANAMEMAX);
			break;
		case 3:		
		case 23:	
		{
			int i;
			printf(" kvno");
			INTOUT();
			printf(" domain");
			STROUT(KANAMEMAX);
			TCHECK2(bp[0], sizeof(int32_t));
			i = (int) EXTRACT_32BITS(bp);
			bp += sizeof(int32_t);
			TCHECK2(bp[0], i);
			bp += i;
			printf(" principal");
			STROUT(KANAMEMAX);
			STROUT(KANAMEMAX);
			break;
		}
		case 4:		
			printf(" principal");
			STROUT(KANAMEMAX);
			STROUT(KANAMEMAX);
			printf(" kvno");
			INTOUT();
			break;
		case 12:	
			printf(" name");
			STROUT(KANAMEMAX);
			break;
		default:
			;
	}

	return;

trunc:
	printf(" [|kauth]");
}


static void
kauth_reply_print(register const u_char *bp, int length, int32_t opcode)
{
	struct rx_header *rxh;

	if (length <= (int)sizeof(struct rx_header))
		return;

	rxh = (struct rx_header *) bp;


	printf(" kauth");

	if (is_ubik(opcode)) {
		ubik_reply_print(bp, length, opcode);
		return;
	}

	printf(" reply %s", tok2str(kauth_req, "op#%d", opcode));

	bp += sizeof(struct rx_header);


	if (rxh->type == RX_PACKET_TYPE_DATA)
		
		;
	else {
		printf(" errcode");
		INTOUT();
	}

	return;

trunc:
	printf(" [|kauth]");
}


static void
vol_print(register const u_char *bp, int length)
{
	int vol_op;

	if (length <= (int)sizeof(struct rx_header))
		return;

	if (snapend - bp + 1 <= (int)(sizeof(struct rx_header) + sizeof(int32_t))) {
		goto trunc;
	}


	vol_op = EXTRACT_32BITS(bp + sizeof(struct rx_header));

	printf(" vol call %s", tok2str(vol_req, "op#%d", vol_op));

	bp += sizeof(struct rx_header) + 4;

	switch (vol_op) {
		case 100:	
			printf(" partition");
			UINTOUT();
			printf(" name");
			STROUT(AFSNAMEMAX);
			printf(" type");
			UINTOUT();
			printf(" parent");
			UINTOUT();
			break;
		case 101:	
		case 107:	
			printf(" trans");
			UINTOUT();
			break;
		case 102:	
			printf(" totrans");
			UINTOUT();
			printf(" flags");
			UINTOUT();
			break;
		case 103:	
			printf(" fromtrans");
			UINTOUT();
			printf(" fromdate");
			DATEOUT();
			DESTSERVEROUT();
			printf(" desttrans");
			INTOUT();
			break;
		case 104:	
			printf(" trans");
			UINTOUT();
			break;
		case 105:	
			printf(" trans");
			UINTOUT();
			printf(" purgevol");
			UINTOUT();
			printf(" newtype");
			UINTOUT();
			printf(" newname");
			STROUT(AFSNAMEMAX);
			break;
		case 106:	
			printf(" trans");
			UINTOUT();
			printf(" flags");
			UINTOUT();
			break;
		case 108:	
			printf(" vol");
			UINTOUT();
			printf(" partition");
			UINTOUT();
			printf(" flags");
			UINTOUT();
			break;
		case 109:	
		case 655537:	
			printf(" fromtrans");
			UINTOUT();
			printf(" fromdate");
			DATEOUT();
			break;
		case 110:	
			printf(" index");
			UINTOUT();
			break;
		case 111:	
			printf(" tid");
			UINTOUT();
			printf(" newsite");
			UINTOUT();
			break;
		case 112:	
		case 113:	
			printf(" tid");
			break;
		case 114:	
			printf(" name");
			STROUT(AFSNAMEMAX);
			printf(" type");
			UINTOUT();
			printf(" pid");
			UINTOUT();
			printf(" cloneid");
			UINTOUT();
			break;
		case 116:	
			printf(" partition");
			UINTOUT();
			printf(" flags");
			UINTOUT();
			break;
		case 117:	
			printf(" tid");
			UINTOUT();
			printf(" name");
			STROUT(AFSNAMEMAX);
			printf(" type");
			UINTOUT();
			printf(" pid");
			UINTOUT();
			printf(" clone");
			UINTOUT();
			printf(" backup");
			UINTOUT();
			break;
		case 119:	
			printf(" name");
			STROUT(AFSNAMEMAX);
			break;
		case 120:	
			printf(" tid");
			UINTOUT();
			break;
		case 121:	
		case 122:	
		case 124:	
		case 125:	
		case 65536:	
			printf(" partid");
			UINTOUT();
			printf(" volid");
			UINTOUT();
			break;
		case 123:	
			printf(" tid");
			UINTOUT();
			printf(" date");
			DATEOUT();
			break;
		case 126:	
			printf(" tid");
			UINTOUT();
			break;
		case 128:	
			printf(" fromtrans");
			UINTOUT();
			printf(" fromdate");
			DATEOUT();
			{
				unsigned long i, j;
				TCHECK2(bp[0], 4);
				j = EXTRACT_32BITS(bp);
				bp += sizeof(int32_t);
				for (i = 0; i < j; i++) {
					DESTSERVEROUT();
					if (i != j - 1)
						printf(",");
				}
				if (j == 0)
					printf(" <none!>");
			}
			break;
		case 65538:	
			printf(" fromtrans");
			UINTOUT();
			printf(" fromdate");
			DATEOUT();
			printf(" flags");
			UINTOUT();
			break;
		default:
			;
	}
	return;

trunc:
	printf(" [|vol]");
}


static void
vol_reply_print(register const u_char *bp, int length, int32_t opcode)
{
	struct rx_header *rxh;

	if (length <= (int)sizeof(struct rx_header))
		return;

	rxh = (struct rx_header *) bp;


	printf(" vol reply %s", tok2str(vol_req, "op#%d", opcode));

	bp += sizeof(struct rx_header);


	if (rxh->type == RX_PACKET_TYPE_DATA) {
		switch (opcode) {
			case 100:	
				printf(" volid");
				UINTOUT();
				printf(" trans");
				UINTOUT();
				break;
			case 104:	
				UINTOUT();
				break;
			case 105:	
				printf(" newvol");
				UINTOUT();
				break;
			case 107:	
				UINTOUT();
				break;
			case 108:	
				printf(" trans");
				UINTOUT();
				break;
			case 110:	
				printf(" volume");
				UINTOUT();
				printf(" partition");
				UINTOUT();
				break;
			case 112:	
				STROUT(AFSNAMEMAX);
				break;
			case 113:	
				printf(" volid");
				UINTOUT();
				printf(" nextuniq");
				UINTOUT();
				printf(" type");
				UINTOUT();
				printf(" parentid");
				UINTOUT();
				printf(" clone");
				UINTOUT();
				printf(" backup");
				UINTOUT();
				printf(" restore");
				UINTOUT();
				printf(" maxquota");
				UINTOUT();
				printf(" minquota");
				UINTOUT();
				printf(" owner");
				UINTOUT();
				printf(" create");
				DATEOUT();
				printf(" access");
				DATEOUT();
				printf(" update");
				DATEOUT();
				printf(" expire");
				DATEOUT();
				printf(" backup");
				DATEOUT();
				printf(" copy");
				DATEOUT();
				break;
			case 115:	
				break;
			case 116:	
			case 121:	
				{
					unsigned long i, j;
					TCHECK2(bp[0], 4);
					j = EXTRACT_32BITS(bp);
					bp += sizeof(int32_t);
					for (i = 0; i < j; i++) {
						printf(" name");
						VECOUT(32);
						printf(" volid");
						UINTOUT();
						printf(" type");
						bp += sizeof(int32_t) * 21;
						if (i != j - 1)
							printf(",");
					}
					if (j == 0)
						printf(" <none!>");
				}
				break;
				

			default:
				;
		}
	} else {
		printf(" errcode");
		INTOUT();
	}

	return;

trunc:
	printf(" [|vol]");
}


static void
bos_print(register const u_char *bp, int length)
{
	int bos_op;

	if (length <= (int)sizeof(struct rx_header))
		return;

	if (snapend - bp + 1 <= (int)(sizeof(struct rx_header) + sizeof(int32_t))) {
		goto trunc;
	}


	bos_op = EXTRACT_32BITS(bp + sizeof(struct rx_header));

	printf(" bos call %s", tok2str(bos_req, "op#%d", bos_op));


	bp += sizeof(struct rx_header) + 4;

	switch (bos_op) {
		case 80:	
			printf(" type");
			STROUT(BOSNAMEMAX);
			printf(" instance");
			STROUT(BOSNAMEMAX);
			break;
		case 81:	
		case 83:	
		case 85:	
		case 87:	
		case 88:	
		case 93:	
		case 96:	
		case 97:	
		case 104:	
		case 106:	
		case 108:	
		case 112:	
		case 114:	
			STROUT(BOSNAMEMAX);
			break;
		case 82:	
		case 98:	
			STROUT(BOSNAMEMAX);
			printf(" status");
			INTOUT();
			break;
		case 86:	
			STROUT(BOSNAMEMAX);
			printf(" num");
			INTOUT();
			break;
		case 84:	
		case 89:	
		case 90:	
		case 91:	
		case 92:	
		case 95:	
			INTOUT();
			break;
		case 105:	
			STROUT(BOSNAMEMAX);
			printf(" size");
			INTOUT();
			printf(" flags");
			INTOUT();
			printf(" date");
			INTOUT();
			break;
		default:
			;
	}

	return;

trunc:
	printf(" [|bos]");
}


static void
bos_reply_print(register const u_char *bp, int length, int32_t opcode)
{
	struct rx_header *rxh;

	if (length <= (int)sizeof(struct rx_header))
		return;

	rxh = (struct rx_header *) bp;


	printf(" bos reply %s", tok2str(bos_req, "op#%d", opcode));

	bp += sizeof(struct rx_header);


	if (rxh->type == RX_PACKET_TYPE_DATA)
		
		;
	else {
		printf(" errcode");
		INTOUT();
	}

	return;

trunc:
	printf(" [|bos]");
}


static int
is_ubik(u_int32_t opcode)
{
	if ((opcode >= VOTE_LOW && opcode <= VOTE_HIGH) ||
	    (opcode >= DISK_LOW && opcode <= DISK_HIGH))
		return(1);
	else
		return(0);
}


static void
ubik_print(register const u_char *bp)
{
	int ubik_op;
	int32_t temp;


	ubik_op = EXTRACT_32BITS(bp + sizeof(struct rx_header));

	printf(" ubik call %s", tok2str(ubik_req, "op#%d", ubik_op));


	bp += sizeof(struct rx_header) + 4;

	switch (ubik_op) {
		case 10000:		
			TCHECK2(bp[0], 4);
			temp = EXTRACT_32BITS(bp);
			bp += sizeof(int32_t);
			printf(" syncsite %s", temp ? "yes" : "no");
			printf(" votestart");
			DATEOUT();
			printf(" dbversion");
			UBIK_VERSIONOUT();
			printf(" tid");
			UBIK_VERSIONOUT();
			break;
		case 10003:		
			printf(" site");
			UINTOUT();
			break;
		case 20000:		
		case 20001:		
		case 20007:		
		case 20008:		
		case 20010:		
			printf(" tid");
			UBIK_VERSIONOUT();
			break;
		case 20002:		
			printf(" tid");
			UBIK_VERSIONOUT();
			printf(" file");
			INTOUT();
			printf(" pos");
			INTOUT();
			printf(" length");
			INTOUT();
			temp = EXTRACT_32BITS(bp);
			bp += sizeof(int32_t);
			tok2str(ubik_lock_types, "type %d", temp);
			break;
		case 20003:		
			printf(" tid");
			UBIK_VERSIONOUT();
			printf(" file");
			INTOUT();
			printf(" pos");
			INTOUT();
			break;
		case 20005:		
			printf(" file");
			INTOUT();
			break;
		case 20006:		
			printf(" file");
			INTOUT();
			printf(" length");
			INTOUT();
			printf(" dbversion");
			UBIK_VERSIONOUT();
			break;
		case 20009:		
			printf(" tid");
			UBIK_VERSIONOUT();
			printf(" file");
			INTOUT();
			printf(" length");
			INTOUT();
			break;
		case 20012:		
			printf(" tid");
			UBIK_VERSIONOUT();
			printf(" oldversion");
			UBIK_VERSIONOUT();
			printf(" newversion");
			UBIK_VERSIONOUT();
			break;
		default:
			;
	}

	return;

trunc:
	printf(" [|ubik]");
}


static void
ubik_reply_print(register const u_char *bp, int length, int32_t opcode)
{
	struct rx_header *rxh;

	if (length < (int)sizeof(struct rx_header))
		return;

	rxh = (struct rx_header *) bp;


	printf(" ubik reply %s", tok2str(ubik_req, "op#%d", opcode));

	bp += sizeof(struct rx_header);


	if (rxh->type == RX_PACKET_TYPE_DATA)
		switch (opcode) {
		case 10000:		
			printf(" vote no");
			break;
		case 20004:		
			printf(" dbversion");
			UBIK_VERSIONOUT();
			break;
		default:
			;
		}


	else
		switch (opcode) {
		case 10000:		
			printf(" vote yes until");
			DATEOUT();
			break;
		default:
			printf(" errcode");
			INTOUT();
		}

	return;

trunc:
	printf(" [|ubik]");
}


static void
rx_ack_print(register const u_char *bp, int length)
{
	struct rx_ackPacket *rxa;
	int i, start, last;
	u_int32_t firstPacket;

	if (length < (int)sizeof(struct rx_header))
		return;

	bp += sizeof(struct rx_header);


	TCHECK2(bp[0], sizeof(struct rx_ackPacket) - RX_MAXACKS);

	rxa = (struct rx_ackPacket *) bp;
	bp += (sizeof(struct rx_ackPacket) - RX_MAXACKS);


	if (vflag > 2)
		printf(" bufspace %d maxskew %d",
		       (int) EXTRACT_16BITS(&rxa->bufferSpace),
		       (int) EXTRACT_16BITS(&rxa->maxSkew));

	firstPacket = EXTRACT_32BITS(&rxa->firstPacket);
	printf(" first %d serial %d reason %s",
	       firstPacket, EXTRACT_32BITS(&rxa->serial),
	       tok2str(rx_ack_reasons, "#%d", (int) rxa->reason));


	if (rxa->nAcks != 0) {

		TCHECK2(bp[0], rxa->nAcks);


		for (i = 0, start = last = -2; i < rxa->nAcks; i++)
			if (rxa->acks[i] == RX_ACK_TYPE_ACK) {


				if (last == -2) {
					printf(" acked %d",
					       firstPacket + i);
					start = i;
				}


				else if (last != i - 1) {
					printf(",%d", firstPacket + i);
					start = i;
				}


				last = i;

			} else if (last == i - 1 && start != last)
				printf("-%d", firstPacket + i - 1);


		if (last == i - 1 && start != last)
			printf("-%d", firstPacket + i - 1);


		for (i = 0, start = last = -2; i < rxa->nAcks; i++)
			if (rxa->acks[i] == RX_ACK_TYPE_NACK) {
				if (last == -2) {
					printf(" nacked %d",
					       firstPacket + i);
					start = i;
				} else if (last != i - 1) {
					printf(",%d", firstPacket + i);
					start = i;
				}
				last = i;
			} else if (last == i - 1 && start != last)
				printf("-%d", firstPacket + i - 1);

		if (last == i - 1 && start != last)
			printf("-%d", firstPacket + i - 1);

		bp += rxa->nAcks;
	}



#define TRUNCRET(n)	if (snapend - bp + 1 <= n) return;

	if (vflag > 1) {
		TRUNCRET(4);
		printf(" ifmtu");
		INTOUT();

		TRUNCRET(4);
		printf(" maxmtu");
		INTOUT();

		TRUNCRET(4);
		printf(" rwind");
		INTOUT();

		TRUNCRET(4);
		printf(" maxpackets");
		INTOUT();
	}

	return;

trunc:
	printf(" [|ack]");
}
#undef TRUNCRET
