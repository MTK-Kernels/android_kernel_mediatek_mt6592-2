/*
 * Copyright (c) 1992, 1994, 1996
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
 * @(#) $Header: /tcpdump/master/tcpdump/decnet.h,v 1.11 2002-12-11 07:13:50 guy Exp $ (LBL)
 */

#ifndef WIN32
typedef u_int8_t byte[1];		
#else
typedef unsigned char Byte[1];		
#define byte Byte
#endif 
typedef u_int8_t word[2];		
typedef u_int8_t longword[4];		

union etheraddress {
	u_int8_t   dne_addr[6];		
	struct {
		u_int8_t dne_hiord[4];	
		u_int8_t dne_nodeaddr[2]; 
	} dne_remote;
};

typedef union etheraddress etheraddr;	

#define HIORD 0x000400aa		

#define AREAMASK	0176000		
#define	AREASHIFT	10		
#define NODEMASK	01777		

#define DN_MAXADDL	20		
struct dn_naddr {
	u_int16_t	a_len;		
	u_int8_t a_addr[DN_MAXADDL]; 
};

struct shorthdr
  {
    byte	sh_flags;		
    word	sh_dst;			
    word	sh_src;			
    byte	sh_visits;		
  };

struct longhdr
  {
    byte	lg_flags;		
    byte	lg_darea;		
    byte	lg_dsarea;		
    etheraddr	lg_dst;			
    byte	lg_sarea;		
    byte	lg_ssarea;		
    etheraddr	lg_src;			
    byte	lg_nextl2;		
    byte	lg_visits;		
    byte	lg_service;		
    byte	lg_pt;			
  };

union routehdr
  {
    struct shorthdr rh_short;		
    struct longhdr rh_long;		
  };

#define RMF_MASK	7		
#define RMF_SHORT	2		
#define RMF_LONG	6		
#ifndef RMF_RQR
#define RMF_RQR		010		
#define RMF_RTS		020		
#define RMF_IE		040		
#endif 
#define RMF_FVER	0100		
#define RMF_PAD		0200		
#define RMF_PADMASK	0177		

#define VIS_MASK	077		

#define RMF_CTLMASK	017		
#define RMF_CTLMSG	01		
#define RMF_INIT	01		
#define RMF_VER		03		
#define RMF_TEST	05		
#define RMF_L1ROUT	07		
#define RMF_L2ROUT	011		
#define RMF_RHELLO	013		
#define RMF_EHELLO	015		

#define TI_L2ROUT	01		
#define TI_L1ROUT	02		
#define TI_ENDNODE	03		
#define TI_VERIF	04		
#define TI_BLOCK	010		

#define VE_VERS		2		
#define VE_ECO		0		
#define VE_UECO		0		

#define P3_VERS		1		
#define P3_ECO		3		
#define P3_UECO		0		

#define II_L2ROUT	01		
#define II_L1ROUT	02		
#define II_ENDNODE	03		
#define II_VERIF	04		
#define II_NOMCAST	040		
#define II_BLOCK	0100		
#define II_TYPEMASK	03		

#define TESTDATA	0252		
#define TESTLEN		1		

struct initmsgIII			
  {
    byte	inIII_flags;		
    word	inIII_src;		
    byte	inIII_info;		
    word	inIII_blksize;		
    byte	inIII_vers;		
    byte	inIII_eco;		
    byte	inIII_ueco;		
    byte	inIII_rsvd;		
  };

struct initmsg				
  {
    byte	in_flags;		
    word	in_src;			
    byte	in_info;		
    word	in_blksize;		
    byte	in_vers;		
    byte	in_eco;			
    byte	in_ueco;		
    word	in_hello;		
    byte	in_rsvd;		
  };

struct verifmsg				
  {
    byte	ve_flags;		
    word	ve_src;			
    byte	ve_fcnval;		
  };

struct testmsg				
  {
    byte	te_flags;		
    word	te_src;			
    byte	te_data;		
  };

struct l1rout				
  {
    byte	r1_flags;		
    word	r1_src;			
    byte	r1_rsvd;		
  };

struct l2rout				
  {
    byte	r2_flags;		
    word	r2_src;			
    byte	r2_rsvd;		
  };

struct rhellomsg			
  {
    byte	rh_flags;		
    byte	rh_vers;		
    byte	rh_eco;			
    byte	rh_ueco;		
    etheraddr	rh_src;			
    byte	rh_info;		
    word	rh_blksize;		
    byte	rh_priority;		
    byte	rh_area;		
    word	rh_hello;		
    byte	rh_mpd;			
  };

struct ehellomsg			
  {
    byte	eh_flags;		
    byte	eh_vers;		
    byte	eh_eco;			
    byte	eh_ueco;		
    etheraddr	eh_src;			
    byte	eh_info;		
    word	eh_blksize;		
    byte	eh_area;		
    byte	eh_seed[8];		
    etheraddr	eh_router;		
    word	eh_hello;		
    byte	eh_mpd;			
    byte	eh_data;		
  };

union controlmsg
  {
    struct initmsg	cm_init;	
    struct verifmsg	cm_ver;		
    struct testmsg	cm_test;	
    struct l1rout	cm_l1rou;	
    struct l2rout	cm_l2rout;	
    struct rhellomsg	cm_rhello;	
    struct ehellomsg	cm_ehello;	
  };

#define	RI_COST(x)	((x)&0777)
#define	RI_HOPS(x)	(((x)>>10)&037)


#define NSP_TYPEMASK 014		
#define NSP_SUBMASK 0160		
#define NSP_SUBSHFT 4			

#define MFT_DATA 0			
#define MFT_ACK  04			
#define MFT_CTL  010			

#define MFS_ILS  020			
#define MFS_BOM  040			
#define MFS_MOM  0			
#define MFS_EOM  0100			
#define MFS_INT  040			

#define MFS_DACK 0			
#define MFS_IACK 020			
#define MFS_CACK 040			

#define MFS_NOP  0			
#define MFS_CI   020			
#define MFS_CC   040			
#define MFS_DI   060			
#define MFS_DC   0100			
#define MFS_RCI  0140			

#define SGQ_ACK  0100000		
#define SGQ_NAK  0110000		
#define SGQ_OACK 0120000		
#define SGQ_ONAK 0130000		
#define SGQ_MASK 07777			
#define SGQ_OTHER 020000		
#define SGQ_DELAY 010000		

#define SGQ_EOM  0100000		

#define LSM_MASK 03			
#define LSM_NOCHANGE 0			
#define LSM_DONOTSEND 1			
#define LSM_SEND 2			

#define LSI_MASK 014			
#define LSI_DATA 0			
#define LSI_INTR 4			
#define LSI_INTM 0377			

#define COS_MASK 014			
#define COS_NONE 0			
#define COS_SEGMENT 04			
#define COS_MESSAGE 010			
#define COS_CRYPTSER 020		
#define COS_DEFAULT 1			

#define COI_MASK 3			
#define COI_32 0			
#define COI_31 1			
#define COI_40 2			
#define COI_41 3			

#define MNU_MASK 140			
#define MNU_10 000				
#define MNU_20 040				
#define MNU_ACCESS 1			
#define MNU_USRDATA 2			
#define MNU_INVKPROXY 4			
#define MNU_UICPROXY 8			

#define DC_NORESOURCES 1		
#define DC_NOLINK 41			
#define DC_COMPLETE 42			

#define DI_NOERROR 0			
#define DI_SHUT 3			
#define DI_NOUSER 4			
#define DI_INVDEST 5			
#define DI_REMRESRC 6			
#define DI_TPA 8			
#define DI_PROTOCOL 7			
#define DI_ABORT 9			
#define DI_LOCALRESRC 32		
#define DI_REMUSERRESRC 33		
#define DI_BADACCESS 34			
#define DI_BADACCNT 36			
#define DI_CONNECTABORT 38		
#define DI_TIMEDOUT 38			
#define DI_UNREACHABLE 39		
#define DI_BADIMAGE 43			
#define DI_SERVMISMATCH 54		

#define UC_OBJREJECT 0			
#define UC_USERDISCONNECT 0		
#define UC_RESOURCES 1			
#define UC_NOSUCHNODE 2			
#define UC_REMOTESHUT 3			
#define UC_NOSUCHOBJ 4			
#define UC_INVOBJFORMAT 5		
#define UC_OBJTOOBUSY 6			
#define UC_NETWORKABORT 8		
#define UC_USERABORT 9			
#define UC_INVNODEFORMAT 10		
#define UC_LOCALSHUT 11			
#define UC_ACCESSREJECT 34		
#define UC_NORESPONSE 38		
#define UC_UNREACHABLE 39		

struct nsphdr				
  {
    byte	nh_flags;		
    word	nh_dst;			
    word	nh_src;			
  };

struct seghdr				
  {
    byte	sh_flags;		
    word	sh_dst;			
    word	sh_src;			
    word	sh_seq[3];		
  };

struct minseghdr			
  {
    byte	ms_flags;		
    word	ms_dst;			
    word	ms_src;			
    word	ms_seq;			
  };

struct lsmsg				
  {
    byte	ls_lsflags;		
    byte	ls_fcval;		
  };

struct ackmsg				
  {
    byte	ak_flags;		
    word	ak_dst;			
    word	ak_src;			
    word	ak_acknum[2];		
  };

struct minackmsg			
  {
    byte	mk_flags;		
    word	mk_dst;			
    word	mk_src;			
    word	mk_acknum;		
  };

struct ciackmsg				
  {
    byte	ck_flags;		
    word	ck_dst;			
  };

struct cimsg				
  {
    byte	ci_flags;		
    word	ci_dst;			
    word	ci_src;			
    byte	ci_services;		
    byte	ci_info;		
    word	ci_segsize;		
  };

struct ccmsg				
  {
    byte	cc_flags;		
    word	cc_dst;			
    word	cc_src;			
    byte	cc_services;		
    byte	cc_info;		
    word	cc_segsize;		
    byte	cc_optlen;		
  };

struct cnmsg				
  {
    byte	cn_flags;		
    word	cn_dst;			
    word	cn_src;			
    byte	cn_services;		
    byte	cn_info;		
    word	cn_segsize;		
  };

struct dimsg				
  {
    byte	di_flags;		
    word	di_dst;			
    word	di_src;			
    word	di_reason;		
    byte	di_optlen;		
  };

struct dcmsg				
  {
    byte	dc_flags;		
    word	dc_dst;			
    word	dc_src;			
    word	dc_reason;		
  };
