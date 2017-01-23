/*
 * Copyright (C) 1995, 1996, 1997, 1998, and 1999 WIDE Project.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */


#ifndef HAVE_ADDRINFO

#define	EAI_ADDRFAMILY	 1	
#define	EAI_AGAIN	 2	
#define	EAI_BADFLAGS	 3	
#define	EAI_FAIL	 4	
#define	EAI_FAMILY	 5	
#define	EAI_MEMORY	 6	
#define	EAI_NODATA	 7	
#define	EAI_NONAME	 8	
#define	EAI_SERVICE	 9	
#define	EAI_SOCKTYPE	10	
#define	EAI_SYSTEM	11	
#define EAI_BADHINTS	12
#define EAI_PROTOCOL	13
#define EAI_MAX		14

#define	NETDB_INTERNAL	-1	

#define	AI_PASSIVE	0x00000001 
#define	AI_CANONNAME	0x00000002 
#define	AI_NUMERICHOST	0x00000004 
#define	AI_MASK		(AI_PASSIVE | AI_CANONNAME | AI_NUMERICHOST)

#define	AI_ALL		0x00000100 
#define	AI_V4MAPPED_CFG	0x00000200 
#define	AI_ADDRCONFIG	0x00000400 
#define	AI_V4MAPPED	0x00000800 
#define	AI_DEFAULT	(AI_V4MAPPED_CFG | AI_ADDRCONFIG)

struct addrinfo {
	int	ai_flags;	
	int	ai_family;	
	int	ai_socktype;	
	int	ai_protocol;	
	size_t	ai_addrlen;	
	char	*ai_canonname;	
	struct sockaddr *ai_addr;	
	struct addrinfo *ai_next;	
};

extern void freeaddrinfo (struct addrinfo *);
extern void freehostent (struct hostent *);
extern int getnameinfo (const struct sockaddr *, size_t, char *,
			    size_t, char *, size_t, int);
extern struct hostent *getipnodebyaddr (const void *, size_t, int, int *);
extern struct hostent *getipnodebyname (const char *, int, int, int *);
extern int inet_pton (int, const char *, void *);
extern const char *inet_ntop (int, const void *, char *, size_t);
#endif 

#ifndef NI_MAXHOST
#define	NI_MAXHOST	1025
#endif
#ifndef NI_MAXSERV
#define	NI_MAXSERV	32
#endif

#ifndef NI_NOFQDN
#define	NI_NOFQDN	0x00000001
#endif
#ifndef NI_NUMERICHOST
#define	NI_NUMERICHOST	0x00000002
#endif
#ifndef NI_NAMEREQD
#define	NI_NAMEREQD	0x00000004
#endif
#ifndef NI_NUMERICSERV
#define	NI_NUMERICSERV	0x00000008
#endif
#ifndef NI_DGRAM
#define	NI_DGRAM	0x00000010
#endif
