
/*
 * Copyright (c) 1989, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)if_types.h	8.3 (Berkeley) 4/28/95
 */

#ifndef _NET_IF_TYPES_H_
#define _NET_IF_TYPES_H_


#define	IFT_OTHER	0x1		
#define	IFT_1822	0x2		
#define	IFT_HDH1822	0x3		
#define	IFT_X25DDN	0x4		
#define	IFT_X25		0x5		
#define	IFT_ETHER	0x6		
#define	IFT_ISO88023	0x7		
#define	IFT_ISO88024	0x8		
#define	IFT_ISO88025	0x9		
#define	IFT_ISO88026	0xa		
#define	IFT_STARLAN	0xb
#define	IFT_P10		0xc		
#define	IFT_P80		0xd		
#define	IFT_HY		0xe		
#define	IFT_FDDI	0xf
#define	IFT_LAPB	0x10
#define	IFT_SDLC	0x11
#define	IFT_T1		0x12
#define	IFT_CEPT	0x13		
#define	IFT_ISDNBASIC	0x14
#define	IFT_ISDNPRIMARY	0x15
#define	IFT_PTPSERIAL	0x16		
#define	IFT_PPP		0x17		
#define	IFT_LOOP	0x18		
#define	IFT_EON		0x19		
#define	IFT_XETHER	0x1a		
#define	IFT_NSIP	0x1b		
#define	IFT_SLIP	0x1c		
#define	IFT_ULTRA	0x1d		
#define	IFT_DS3		0x1e		
#define	IFT_SIP		0x1f		
#define	IFT_FRELAY	0x20		
#define	IFT_RS232	0x21
#define	IFT_PARA	0x22		
#define	IFT_ARCNET	0x23
#define	IFT_ARCNETPLUS	0x24
#define	IFT_ATM		0x25		
#define	IFT_MIOX25	0x26
#define	IFT_SONET	0x27		
#define	IFT_X25PLE	0x28
#define	IFT_ISO88022LLC	0x29
#define	IFT_LOCALTALK	0x2a
#define	IFT_SMDSDXI	0x2b
#define	IFT_FRELAYDCE	0x2c		
#define	IFT_V35		0x2d
#define	IFT_HSSI	0x2e
#define	IFT_HIPPI	0x2f
#define	IFT_MODEM	0x30		
#define	IFT_AAL5	0x31		
#define	IFT_SONETPATH	0x32
#define	IFT_SONETVT	0x33
#define	IFT_SMDSICIP	0x34		
#define	IFT_PROPVIRTUAL	0x35		
#define	IFT_PROPMUX	0x36		
#define IFT_IEEE80212		   0x37 
#define IFT_FIBRECHANNEL	   0x38 
#define IFT_HIPPIINTERFACE	   0x39 
#define IFT_FRAMERELAYINTERCONNECT 0x3a 
#define IFT_AFLANE8023		   0x3b 
#define IFT_AFLANE8025		   0x3c 
#define IFT_CCTEMUL		   0x3d 
#define IFT_FASTETHER		   0x3e 
#define IFT_ISDN		   0x3f 
#define IFT_V11			   0x40 
#define IFT_V36			   0x41 
#define IFT_G703AT64K		   0x42 
#define IFT_G703AT2MB		   0x43 
#define IFT_QLLC		   0x44 
#define IFT_FASTETHERFX		   0x45 
#define IFT_CHANNEL		   0x46 
#define IFT_IEEE80211		   0x47 
#define IFT_IBM370PARCHAN	   0x48 
#define IFT_ESCON		   0x49 
#define IFT_DLSW		   0x4a 
#define IFT_ISDNS		   0x4b 
#define IFT_ISDNU		   0x4c 
#define IFT_LAPD		   0x4d 
#define IFT_IPSWITCH		   0x4e 
#define IFT_RSRB		   0x4f 
#define IFT_ATMLOGICAL		   0x50 
#define IFT_DS0			   0x51 
#define IFT_DS0BUNDLE		   0x52 
#define IFT_BSC			   0x53 
#define IFT_ASYNC		   0x54 
#define IFT_CNR			   0x55 
#define IFT_ISO88025DTR		   0x56 
#define IFT_EPLRS		   0x57 
#define IFT_ARAP		   0x58 
#define IFT_PROPCNLS		   0x59 
#define IFT_HOSTPAD		   0x5a 
#define IFT_TERMPAD		   0x5b 
#define IFT_FRAMERELAYMPI	   0x5c 
#define IFT_X213		   0x5d 
#define IFT_ADSL		   0x5e 
#define IFT_RADSL		   0x5f 
#define IFT_SDSL		   0x60 
#define IFT_VDSL		   0x61 
#define IFT_ISO88025CRFPINT	   0x62 
#define IFT_MYRINET		   0x63 
#define IFT_VOICEEM		   0x64 
#define IFT_VOICEFXO		   0x65 
#define IFT_VOICEFXS		   0x66 
#define IFT_VOICEENCAP		   0x67 
#define IFT_VOICEOVERIP		   0x68 
#define IFT_ATMDXI		   0x69 
#define IFT_ATMFUNI		   0x6a 
#define IFT_ATMIMA		   0x6b 
#define IFT_PPPMULTILINKBUNDLE	   0x6c 
#define IFT_IPOVERCDLC		   0x6d 
#define IFT_IPOVERCLAW		   0x6e 
#define IFT_STACKTOSTACK	   0x6f 
#define IFT_VIRTUALIPADDRESS	   0x70 
#define IFT_MPC			   0x71 
#define IFT_IPOVERATM		   0x72 
#define IFT_ISO88025FIBER	   0x73 
#define IFT_TDLC		   0x74 
#define IFT_GIGABITETHERNET	   0x75 
#define IFT_HDLC		   0x76 
#define IFT_LAPF		   0x77 
#define IFT_V37			   0x78 
#define IFT_X25MLP		   0x79 
#define IFT_X25HUNTGROUP	   0x7a 
#define IFT_TRANSPHDLC		   0x7b 
#define IFT_INTERLEAVE		   0x7c 
#define IFT_FAST		   0x7d 
#define IFT_IP			   0x7e 
#define IFT_DOCSCABLEMACLAYER	   0x7f 
#define IFT_DOCSCABLEDOWNSTREAM	   0x80 
#define IFT_DOCSCABLEUPSTREAM	   0x81 
#define IFT_A12MPPSWITCH	   0x82	
#define IFT_TUNNEL		   0x83	
#define IFT_COFFEE		   0x84	
#define IFT_CES			   0x85	
#define IFT_ATMSUBINTERFACE	   0x86	
#define IFT_L2VLAN		   0x87	
#define IFT_L3IPVLAN		   0x88	
#define IFT_L3IPXVLAN		   0x89	
#define IFT_DIGITALPOWERLINE	   0x8a	
#define IFT_MEDIAMAILOVERIP	   0x8b	
#define IFT_DTM			   0x8c	
#define IFT_DCN			   0x8d	
#define IFT_IPFORWARD		   0x8e	
#define IFT_MSDSL		   0x8f	
#define IFT_IEEE1394		   0x90	
#define IFT_IFGSN		   0x91	
#define IFT_DVBRCCMACLAYER	   0x92	
#define IFT_DVBRCCDOWNSTREAM	   0x93	
#define IFT_DVBRCCUPSTREAM	   0x94	
#define IFT_ATMVIRTUAL		   0x95	
#define IFT_MPLSTUNNEL		   0x96	
#define IFT_SRP			   0x97	
#define IFT_VOICEOVERATM	   0x98	
#define IFT_VOICEOVERFRAMERELAY	   0x99	
#define IFT_IDSL		   0x9a	
#define IFT_COMPOSITELINK	   0x9b	
#define IFT_SS7SIGLINK		   0x9c	
#define IFT_PROPWIRELESSP2P	   0x9d	
#define IFT_FRFORWARD		   0x9e	
#define IFT_RFC1483		   0x9f	
#define IFT_USB			   0xa0	
#define IFT_IEEE8023ADLAG	   0xa1	
#define IFT_BGPPOLICYACCOUNTING	   0xa2	
#define IFT_FRF16MFRBUNDLE	   0xa3	
#define IFT_H323GATEKEEPER	   0xa4	
#define IFT_H323PROXY		   0xa5	
#define IFT_MPLS		   0xa6	
#define IFT_MFSIGLINK		   0xa7	
#define IFT_HDSL2		   0xa8	
#define IFT_SHDSL		   0xa9	
#define IFT_DS1FDL		   0xaa	
#define IFT_POS			   0xab	
#define IFT_DVBASILN		   0xac	
#define IFT_DVBASIOUT		   0xad	
#define IFT_PLC			   0xae	
#define IFT_NFAS		   0xaf	
#define IFT_TR008		   0xb0	
#define IFT_GR303RDT		   0xb1	
#define IFT_GR303IDT		   0xb2	
#define IFT_ISUP		   0xb3	
#define IFT_PROPDOCSWIRELESSMACLAYER	   0xb4	
#define IFT_PROPDOCSWIRELESSDOWNSTREAM	   0xb5	
#define IFT_PROPDOCSWIRELESSUPSTREAM	   0xb6	
#define IFT_HIPERLAN2		   0xb7	
#define IFT_PROPBWAP2MP		   0xb8	
#define IFT_SONETOVERHEADCHANNEL   0xb9	
#define IFT_DIGITALWRAPPEROVERHEADCHANNEL  0xba	
#define IFT_AAL2		   0xbb	
#define IFT_RADIOMAC		   0xbc	
#define IFT_ATMRADIO		   0xbd	
#define IFT_IMT			   0xbe 
#define IFT_MVL			   0xbf 
#define IFT_REACHDSL		   0xc0 
#define IFT_FRDLCIENDPT		   0xc1 
#define IFT_ATMVCIENDPT		   0xc2 
#define IFT_OPTICALCHANNEL	   0xc3 
#define IFT_OPTICALTRANSPORT	   0xc4 
#define IFT_PROPATM		   0xc5 
#define IFT_VOICEOVERCABLE	   0xc6 
#define IFT_INFINIBAND		   0xc7 
#define IFT_TELINK		   0xc8 
#define IFT_Q2931		   0xc9 
#define IFT_VIRTUALTG		   0xca 
#define IFT_SIPTG		   0xcb 
#define IFT_SIPSIG		   0xcc 
#define IFT_DOCSCABLEUPSTREAMCHANNEL 0xcd 
#define IFT_ECONET		   0xce 
#define IFT_PON155		   0xcf 
#define IFT_PON622		   0xd0 */
#define IFT_BRIDGE		   0xd1 
#define IFT_LINEGROUP		   0xd2 
#define IFT_VOICEEMFGD		   0xd3 
#define IFT_VOICEFGDEANA	   0xd4 
#define IFT_VOICEDID		   0xd5 
#define IFT_STF			   0xd7	

#define IFT_GIF		0xf0
#define IFT_PVC		0xf1
#define IFT_FAITH	0xf2
#define IFT_PFLOG	0xf5		
#define IFT_PFSYNC	0xf6		

#endif 
