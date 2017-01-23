/*
 * Copyright (c) 1997 Yen Yen Lim and North Dakota State University
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Yen Yen Lim and
        North Dakota State University
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * @(#) $Header: /tcpdump/master/libpcap/atmuni31.h,v 1.3 2007-10-22 19:28:58 guy Exp $ (LBL)
 */


#define VCI_PPC			0x05	
#define VCI_BCC			0x02	
#define VCI_OAMF4SC		0x03	
#define VCI_OAMF4EC		0x04	
#define VCI_METAC		0x01	
#define VCI_ILMIC		0x10	

#define CALL_PROCEED		0x02	
#define CONNECT			0x07	
#define CONNECT_ACK		0x0f	
#define SETUP			0x05	
#define RELEASE			0x4d	
#define RELEASE_DONE		0x5a	
#define RESTART			0x46	
#define RESTART_ACK		0x4e	
#define STATUS			0x7d	
#define STATUS_ENQ		0x75	
#define ADD_PARTY		0x80	
#define ADD_PARTY_ACK		0x81	
#define ADD_PARTY_REJ		0x82	
#define DROP_PARTY		0x83	
#define DROP_PARTY_ACK		0x84	

#define CAUSE			0x08	
#define ENDPT_REF		0x54	
#define AAL_PARA		0x58	
#define TRAFF_DESCRIP		0x59	
#define CONNECT_ID		0x5a	
#define QOS_PARA		0x5c	
#define B_HIGHER		0x5d	
#define B_BEARER		0x5e	
#define B_LOWER			0x5f	
#define CALLING_PARTY		0x6c	
#define CALLED_PARTY		0x70	

#define Q2931			0x09

#define PROTO_POS       0	
#define CALL_REF_POS    2	
#define MSG_TYPE_POS    5	
#define MSG_LEN_POS     7	
#define IE_BEGIN_POS    9	

#define TYPE_POS	0
#define LEN_POS		2
#define FIELD_BEGIN_POS 4
