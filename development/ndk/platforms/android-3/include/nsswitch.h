
/*-
 * Copyright (c) 1997, 1998, 1999, 2004 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Luke Mewburn.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _NSSWITCH_H
#define _NSSWITCH_H	1

#include <sys/types.h>
#include <stdarg.h>

#define	NSS_MODULE_INTERFACE_VERSION	0

#ifndef _PATH_NS_CONF
#define _PATH_NS_CONF	"/etc/nsswitch.conf"
#endif

#define	NS_CONTINUE	0
#define	NS_RETURN	1

	
#define	NS_SUCCESS	(1<<0)		
#define	NS_UNAVAIL	(1<<1)		
#define	NS_NOTFOUND	(1<<2)		
#define	NS_TRYAGAIN	(1<<3)		
#define	NS_STATUSMASK	0x000000ff	

	
#define	NS_FORCEALL	(1<<8)		

#define NSSRC_FILES	"files"		
#define	NSSRC_DNS	"dns"		
#define	NSSRC_NIS	"nis"		
#define	NSSRC_COMPAT	"compat"	

#define NSDB_HOSTS		"hosts"
#define NSDB_GROUP		"group"
#define NSDB_GROUP_COMPAT	"group_compat"
#define NSDB_NETGROUP		"netgroup"
#define NSDB_NETWORKS		"networks"
#define NSDB_PASSWD		"passwd"
#define NSDB_PASSWD_COMPAT	"passwd_compat"
#define NSDB_SHELLS		"shells"

#define NSDB_ALIASES		"aliases"
#define NSDB_AUTH		"auth"
#define NSDB_AUTOMOUNT		"automount"
#define NSDB_BOOTPARAMS		"bootparams"
#define NSDB_ETHERS		"ethers"
#define NSDB_EXPORTS		"exports"
#define NSDB_NETMASKS		"netmasks"
#define NSDB_PHONES		"phones"
#define NSDB_PRINTCAP		"printcap"
#define NSDB_PROTOCOLS		"protocols"
#define NSDB_REMOTE		"remote"
#define NSDB_RPC		"rpc"
#define NSDB_SENDMAILVARS	"sendmailvars"
#define NSDB_SERVICES		"services"
#define NSDB_TERMCAP		"termcap"
#define NSDB_TTYS		"ttys"

typedef	int (*nss_method)(void *, void *, va_list);

typedef struct {
	const char	 *src;
	nss_method	 callback;
	void		 *cb_data;
} ns_dtab;

#define NS_FILES_CB(F,C)	{ NSSRC_FILES,	F,	__UNCONST(C) },
#define NS_COMPAT_CB(F,C)	{ NSSRC_COMPAT,	F,	__UNCONST(C) },

#ifdef HESIOD
#   define NS_DNS_CB(F,C)	{ NSSRC_DNS,	F,	__UNCONST(C) },
#else
#   define NS_DNS_CB(F,C)
#endif

#ifdef YP
#   define NS_NIS_CB(F,C)	{ NSSRC_NIS,	F,	__UNCONST(C) },
#else
#   define NS_NIS_CB(F,C)
#endif

typedef struct {
	const char	*name;
	uint32_t	 flags;
} ns_src;


#if 0 
extern const ns_src __nsdefaultsrc[];
extern const ns_src __nsdefaultcompat[];
extern const ns_src __nsdefaultcompat_forceall[];
extern const ns_src __nsdefaultfiles[];
extern const ns_src __nsdefaultfiles_forceall[];
extern const ns_src __nsdefaultnis[];
extern const ns_src __nsdefaultnis_forceall[];
#endif 

typedef struct {
	const char	*database;
	const char	*name;
	nss_method	 method;
	void		*mdata;
} ns_mtab;

typedef	void (*nss_module_unregister_fn)(ns_mtab *, u_int);
typedef	ns_mtab *(*nss_module_register_fn)(const char *, u_int *,
					   nss_module_unregister_fn *);

#ifdef _NS_PRIVATE


typedef struct {
	const char	*name;		
	ns_src		*srclist;	
	u_int		 srclistsize;	
} ns_dbt;

typedef struct {
	const char	*name;		
	void		*handle;	
	ns_mtab		*mtab;		
	u_int		 mtabsize;	
					
	nss_module_unregister_fn unregister;
} ns_mod;

#endif 


#include <sys/cdefs.h>

__BEGIN_DECLS
int	nsdispatch(void *, const ns_dtab [], const char *,
			const char *, const ns_src [], ...);

#ifdef _NS_PRIVATE
int		 _nsdbtaddsrc(ns_dbt *, const ns_src *);
void		 _nsdbtdump(const ns_dbt *);
int		 _nsdbtput(const ns_dbt *);
void		 _nsyyerror(const char *);
int		 _nsyylex(void);
#endif 

__END_DECLS

#endif 
