/*
 * Copyright (c) 1991, 1993, 1994, 1995, 1996, 1997
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
 * OSPF support contributed by Jeffrey Honig (jch@mitchell.cit.cornell.edu)
 */
#define	OSPF_TYPE_UMD           0	
#define	OSPF_TYPE_HELLO         1	
#define	OSPF_TYPE_DD            2	
#define	OSPF_TYPE_LS_REQ        3	
#define	OSPF_TYPE_LS_UPDATE     4	
#define	OSPF_TYPE_LS_ACK        5	

                
#define OSPF_OPTION_T	0x01	
#define OSPF_OPTION_E	0x02	
#define	OSPF_OPTION_MC	0x04	
#define	OSPF_OPTION_NP	0x08	
#define	OSPF_OPTION_EA	0x10	
#define	OSPF_OPTION_L	0x10	
#define	OSPF_OPTION_DC	0x20	
#define	OSPF_OPTION_O	0x40	
#define	OSPF_OPTION_DN	0x80	

#define	OSPF_AUTH_NONE		0	
#define	OSPF_AUTH_SIMPLE	1	
#define OSPF_AUTH_SIMPLE_LEN	8	
#define OSPF_AUTH_MD5		2	
#define OSPF_AUTH_MD5_LEN	16	

#define	OSPF_DB_INIT		0x04
#define	OSPF_DB_MORE		0x02
#define	OSPF_DB_MASTER          0x01
#define OSPF_DB_RESYNC          0x08  

#define	LS_TYPE_ROUTER		1   
#define	LS_TYPE_NETWORK		2   
#define	LS_TYPE_SUM_IP		3   
#define	LS_TYPE_SUM_ABR		4   
#define	LS_TYPE_ASE		5   
#define	LS_TYPE_GROUP		6   
				    
#define	LS_TYPE_NSSA            7   
#define	LS_TYPE_OPAQUE_LL       9   
#define	LS_TYPE_OPAQUE_AL      10   
#define	LS_TYPE_OPAQUE_DW      11   

#define LS_OPAQUE_TYPE_TE       1   
#define LS_OPAQUE_TYPE_GRACE    3   
#define LS_OPAQUE_TYPE_RI       4   

#define LS_OPAQUE_TE_TLV_ROUTER 1   
#define LS_OPAQUE_TE_TLV_LINK   2   

#define LS_OPAQUE_TE_LINK_SUBTLV_LINK_TYPE             1 
#define LS_OPAQUE_TE_LINK_SUBTLV_LINK_ID               2 
#define LS_OPAQUE_TE_LINK_SUBTLV_LOCAL_IP              3 
#define LS_OPAQUE_TE_LINK_SUBTLV_REMOTE_IP             4 
#define LS_OPAQUE_TE_LINK_SUBTLV_TE_METRIC             5 
#define LS_OPAQUE_TE_LINK_SUBTLV_MAX_BW                6 
#define LS_OPAQUE_TE_LINK_SUBTLV_MAX_RES_BW            7 
#define LS_OPAQUE_TE_LINK_SUBTLV_UNRES_BW              8 
#define LS_OPAQUE_TE_LINK_SUBTLV_ADMIN_GROUP           9 
#define LS_OPAQUE_TE_LINK_SUBTLV_LINK_LOCAL_REMOTE_ID 11 
#define LS_OPAQUE_TE_LINK_SUBTLV_LINK_PROTECTION_TYPE 14 
#define LS_OPAQUE_TE_LINK_SUBTLV_INTF_SW_CAP_DESCR    15 
#define LS_OPAQUE_TE_LINK_SUBTLV_SHARED_RISK_GROUP    16 
#define LS_OPAQUE_TE_LINK_SUBTLV_BW_CONSTRAINTS       17 

#define LS_OPAQUE_TE_LINK_SUBTLV_LINK_TYPE_PTP        1  
#define LS_OPAQUE_TE_LINK_SUBTLV_LINK_TYPE_MA         2  

#define LS_OPAQUE_GRACE_TLV_PERIOD       1 
#define LS_OPAQUE_GRACE_TLV_REASON       2 
#define LS_OPAQUE_GRACE_TLV_INT_ADDRESS  3 

#define LS_OPAQUE_GRACE_TLV_REASON_UNKNOWN     0 
#define LS_OPAQUE_GRACE_TLV_REASON_SW_RESTART  1 
#define LS_OPAQUE_GRACE_TLV_REASON_SW_UPGRADE  2 
#define LS_OPAQUE_GRACE_TLV_REASON_CP_SWITCH   3 

#define LS_OPAQUE_RI_TLV_CAP             1 


#define	RLA_TYPE_ROUTER		1   
#define	RLA_TYPE_TRANSIT	2   
#define	RLA_TYPE_STUB		3   
#define RLA_TYPE_VIRTUAL	4   

#define	RLA_FLAG_B	0x01
#define	RLA_FLAG_E	0x02
#define	RLA_FLAG_W1	0x04
#define	RLA_FLAG_W2	0x08

#define	SLA_MASK_TOS		0x7f000000
#define	SLA_MASK_METRIC		0x00ffffff
#define SLA_SHIFT_TOS		24

#define	ASLA_FLAG_EXTERNAL	0x80000000
#define	ASLA_MASK_TOS		0x7f000000
#define	ASLA_SHIFT_TOS		24
#define	ASLA_MASK_METRIC	0x00ffffff

#define	MCLA_VERTEX_ROUTER	1
#define	MCLA_VERTEX_NETWORK	2

#define OSPF_LLS_EO             1  
#define OSPF_LLS_MD5            2  

#define OSPF_LLS_EO_LR		0x00000001		
#define OSPF_LLS_EO_RS		0x00000002		

struct tos_metric {
    u_int8_t tos_type;
    u_int8_t reserved;
    u_int8_t tos_metric[2];
};
struct tos_link {
    u_int8_t link_type;
    u_int8_t link_tos_count;
    u_int8_t tos_metric[2];
};
union un_tos {
    struct tos_link link;
    struct tos_metric metrics;
};

struct lsa_hdr {
    u_int16_t ls_age;
    u_int8_t ls_options;
    u_int8_t ls_type;
    union {
        struct in_addr lsa_id;
        struct { 
            u_int8_t opaque_type;
            u_int8_t opaque_id[3];
	} opaque_field;
    } un_lsa_id;
    struct in_addr ls_router;
    u_int32_t ls_seq;
    u_int16_t ls_chksum;
    u_int16_t ls_length;
};

struct lsa {
    struct lsa_hdr ls_hdr;

    
    union {
	
	struct {
	    u_int8_t rla_flags;
	    u_int8_t rla_zero[1];
	    u_int16_t rla_count;
	    struct rlalink {
		struct in_addr link_id;
		struct in_addr link_data;
                union un_tos un_tos;
	    } rla_link[1];		
	} un_rla;

	
	struct {
	    struct in_addr nla_mask;
	    struct in_addr nla_router[1];	
	} un_nla;

	
	struct {
	    struct in_addr sla_mask;
	    u_int32_t sla_tosmetric[1];	
	} un_sla;

	
	struct {
	    struct in_addr asla_mask;
	    struct aslametric {
		u_int32_t asla_tosmetric;
		struct in_addr asla_forward;
		struct in_addr asla_tag;
	    } asla_metric[1];		
	} un_asla;

	
	struct mcla {
	    u_int32_t mcla_vtype;
	    struct in_addr mcla_vid;
	} un_mcla[1];

        
        struct {
	    u_int16_t type;
	    u_int16_t length;
	    u_int8_t data[1]; 
	} un_te_lsa_tlv;

        
        struct {
	    u_int16_t type;
	    u_int16_t length;
	    u_int8_t data[1]; 
	} un_grace_tlv;

        
        struct {
	    u_int16_t type;
	    u_int16_t length;
	    u_int8_t data[1]; 
	} un_ri_tlv;

        
        struct unknown {
	    u_int8_t data[1]; 
	} un_unknown[1];

    } lsa_un;
};

#define	OSPF_AUTH_SIZE	8

struct ospfhdr {
    u_int8_t ospf_version;
    u_int8_t ospf_type;
    u_int16_t ospf_len;
    struct in_addr ospf_routerid;
    struct in_addr ospf_areaid;
    u_int16_t ospf_chksum;
    u_int16_t ospf_authtype;
    u_int8_t ospf_authdata[OSPF_AUTH_SIZE];
    union {

	
	struct {
	    struct in_addr hello_mask;
	    u_int16_t hello_helloint;
	    u_int8_t hello_options;
	    u_int8_t hello_priority;
	    u_int32_t hello_deadint;
	    struct in_addr hello_dr;
	    struct in_addr hello_bdr;
	    struct in_addr hello_neighbor[1]; 
	} un_hello;

	
	struct {
	    u_int16_t db_ifmtu;
	    u_int8_t db_options;
	    u_int8_t db_flags;
	    u_int32_t db_seq;
	    struct lsa_hdr db_lshdr[1]; 
	} un_db;

	
	struct lsr {
	    u_int8_t ls_type[4];
            union {
                struct in_addr ls_stateid;
                struct { 
                    u_int8_t opaque_type;
                    u_int8_t opaque_id[3];
                } opaque_field;
            } un_ls_stateid;
	    struct in_addr ls_router;
	} un_lsr[1];		

	
	struct {
	    u_int32_t lsu_count;
	    struct lsa lsu_lsa[1]; 
	} un_lsu;

	
	struct {
	    struct lsa_hdr lsa_lshdr[1]; 
	} un_lsa ;
    } ospf_un ;
};

#define	ospf_hello	ospf_un.un_hello
#define	ospf_db		ospf_un.un_db
#define	ospf_lsr	ospf_un.un_lsr
#define	ospf_lsu	ospf_un.un_lsu
#define	ospf_lsa	ospf_un.un_lsa

extern int ospf_print_te_lsa(const u_int8_t *, u_int);
extern int ospf_print_grace_lsa(const u_int8_t *, u_int);
