/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
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


#ifdef WIN32

#include <ws2tcpip.h>

#else

#include <netdb.h>

#endif

static char *ai_errlist[] = {
	"Success",					
	"Address family for hostname not supported",	
	"Temporary failure in name resolution",		
	"Invalid value for ai_flags",			
	"Non-recoverable failure in name resolution",	
	"ai_family not supported",			
	"Memory allocation failure", 			
	"No address associated with hostname",		
	"hostname nor servname provided, or not known",	
	"servname not supported for ai_socktype",	
	"ai_socktype not supported", 			
	"System error returned in errno", 		
	"Invalid value for hints",			
	"Resolved protocol is unknown"			
};

#ifndef EAI_MAX
#define EAI_MAX (sizeof(ai_errlist)/sizeof(ai_errlist[0]))
#endif

#ifndef gai_strerror

char *
WSAAPI gai_strerrorA(int ecode)
{
	if (ecode >= 0 && ecode < EAI_MAX)
		return ai_errlist[ecode];
	return "Unknown error";
}

#endif 
