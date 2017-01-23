/*
 * $Id: options.h,v 1.1 2004/11/14 07:26:26 paulus Exp $
 *
 * Copyright (C) 1996 Lars Fenneberg
 *
 * See the file COPYRIGHT for the respective terms and conditions. 
 * If the file is missing contact me at lf@elemental.net 
 * and I'll send you a copy.
 *
 */

#define OPTION_LEN	64

#define OT_STR		(1<<0)	  
#define OT_INT		(1<<1)	  
#define OT_SRV		(1<<2)	  
#define OT_AUO		(1<<3)    

#define OT_ANY		((unsigned int)~0) 

#define ST_UNDEF	(1<<0)	  

typedef struct _option {
	char name[OPTION_LEN];	  
	int type, status;	  
	void *val;		  
} OPTION;

static SERVER acctserver = {0};
static SERVER authserver = {0};

int default_tries = 4;
int default_timeout = 60;

static OPTION config_options[] = {
{"config_file",		OT_STR, ST_UNDEF, NULL},
{"auth_order",	 	OT_AUO, ST_UNDEF, NULL},
{"login_tries",	 	OT_INT, ST_UNDEF, &default_tries},
{"login_timeout",	OT_INT, ST_UNDEF, &default_timeout},
{"nologin",		OT_STR, ST_UNDEF, "/etc/nologin"},
{"issue",		OT_STR, ST_UNDEF, "/etc/radiusclient/issue"},
{"authserver",		OT_SRV, ST_UNDEF, &authserver},
{"acctserver",		OT_SRV, ST_UNDEF, &acctserver},
{"servers",		OT_STR, ST_UNDEF, NULL},
{"dictionary",		OT_STR, ST_UNDEF, NULL},
{"login_radius",	OT_STR, ST_UNDEF, "/usr/sbin/login.radius"},
{"seqfile",		OT_STR, ST_UNDEF, NULL},
{"mapfile",		OT_STR, ST_UNDEF, NULL},
{"default_realm",	OT_STR, ST_UNDEF, NULL},
{"radius_timeout",	OT_INT, ST_UNDEF, NULL},
{"radius_retries",	OT_INT,	ST_UNDEF, NULL},
{"nas_identifier",      OT_STR, ST_UNDEF, ""},
{"bindaddr",            OT_STR, ST_UNDEF, NULL},
{"login_local",		OT_STR, ST_UNDEF, NULL},
};

static int num_options = ((sizeof(config_options))/(sizeof(config_options[0])));
