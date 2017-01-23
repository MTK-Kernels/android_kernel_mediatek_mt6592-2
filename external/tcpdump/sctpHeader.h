
/* SCTP reference Implementation Copyright (C) 1999 Cisco And Motorola
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 4. Neither the name of Cisco nor of Motorola may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
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
 * This file is part of the SCTP reference Implementation
 *
 *
 * Please send any bug reports or fixes you make to one of the following email
 * addresses:
 *
 * rstewar1@email.mot.com
 * kmorneau@cisco.com
 * qxie1@email.mot.com
 *
 * Any bugs reported given to us we will try to fix... any fixes shared will
 * be incorperated into the next SCTP release.
 */


#ifndef __sctpHeader_h__
#define __sctpHeader_h__

#include <sctpConstants.h>

#ifdef	__cplusplus
extern "C" {
#endif


#ifdef TRU64
 #define _64BITS 1
#endif

struct sctpHeader{
  u_int16_t source;
  u_int16_t destination;
  u_int32_t verificationTag;
  u_int32_t adler32;
};


struct sctpChunkDesc{
  u_int8_t chunkID;
  u_int8_t chunkFlg;
  u_int16_t chunkLength;
};

struct sctpParamDesc{
  u_int16_t paramType;
  u_int16_t paramLength;
};


struct sctpRelChunkDesc{
  struct sctpChunkDesc chk;
  u_int32_t serialNumber;
};

struct sctpVendorSpecificParam {
  struct sctpParamDesc p;  
  u_int32_t vendorId;	   
  u_int16_t vendorSpecificType;
  u_int16_t vendorSpecificLen;
};






struct sctpInitiation{
  u_int32_t initTag;		
  u_int32_t rcvWindowCredit;	
  u_int16_t NumPreopenStreams;	
  u_int16_t MaxInboundStreams;     
  u_int32_t initialTSN;
  
};

struct sctpV4IpAddress{
  struct sctpParamDesc p;	
  u_int32_t  ipAddress;
};


struct sctpV6IpAddress{
  struct sctpParamDesc p;	
  u_int8_t  ipAddress[16];
};

struct sctpDNSName{
  struct sctpParamDesc param;
  u_int8_t name[1];
};


struct sctpCookiePreserve{
  struct sctpParamDesc p;	
  u_int32_t extraTime;
};


struct sctpTimeStamp{
  u_int32_t ts_sec;
  u_int32_t ts_usec;
};

struct cookieMessage{
  u_int32_t TieTag_curTag;		
  u_int32_t TieTag_hisTag; 		
  int32_t cookieLife;			
  struct sctpTimeStamp timeEnteringState; 
  struct sctpInitiation initAckISent;	
  u_int32_t addressWhereISent[4];	
  int32_t addrtype;			
  u_int16_t locScope;			
  u_int16_t siteScope;			
};


struct sctpUnifiedInit{
  struct sctpChunkDesc uh;
  struct sctpInitiation initm;
};

struct sctpSendableInit{
  struct sctpHeader mh;
  struct sctpUnifiedInit msg;
};



struct sctpSelectiveAck{
  u_int32_t highestConseqTSN;
  u_int32_t updatedRwnd;
  u_int16_t numberOfdesc;
  u_int16_t numDupTsns;
};

struct sctpSelectiveFrag{
  u_int16_t fragmentStart;
  u_int16_t fragmentEnd;
};


struct sctpUnifiedSack{
  struct sctpChunkDesc uh;
  struct sctpSelectiveAck sack;
};


struct sctpHBrequest {
  u_int32_t time_value_1;
  u_int32_t time_value_2;
};

struct sctpHBunified{
  struct sctpChunkDesc hdr;
  struct sctpParamDesc hb;
};


struct sctpHBsender{
  struct sctpChunkDesc hdr;
  struct sctpParamDesc hb;
  struct sctpHBrequest rtt;
  int8_t addrFmt[SCTP_ADDRMAX];
  u_int16_t userreq;
};



struct sctpUnifiedAbort{
  struct sctpChunkDesc uh;
};

struct sctpUnifiedAbortLight{
  struct sctpHeader mh;
  struct sctpChunkDesc uh;
};

struct sctpUnifiedAbortHeavy{
  struct sctpHeader mh;
  struct sctpChunkDesc uh;
  u_int16_t causeCode;
  u_int16_t causeLen;
};

struct sctpShutdown {
  u_int32_t TSN_Seen;
};

struct sctpUnifiedShutdown{
  struct sctpChunkDesc uh;
  struct sctpShutdown shut;
};

struct sctpOpErrorCause{
  u_int16_t cause;
  u_int16_t causeLen;
};

struct sctpUnifiedOpError{
  struct sctpChunkDesc uh;
  struct sctpOpErrorCause c;
};

struct sctpUnifiedStreamError{
  struct sctpHeader mh;
  struct sctpChunkDesc uh;
  struct sctpOpErrorCause c;
  u_int16_t strmNum;
  u_int16_t reserved;
};

struct staleCookieMsg{
  struct sctpHeader mh;
  struct sctpChunkDesc uh;
  struct sctpOpErrorCause c;
  u_int32_t moretime;
};


struct sctpUnifiedSingleMsg{
  struct sctpHeader mh;
  struct sctpChunkDesc uh;
};

struct sctpDataPart{
  u_int32_t TSN;
  u_int16_t streamId;
  u_int16_t sequence;
  u_int32_t payloadtype;
};

struct sctpUnifiedDatagram{
  struct sctpChunkDesc uh;
  struct sctpDataPart dp;
};

struct sctpECN_echo{
  struct sctpChunkDesc uh;
  u_int32_t Lowest_TSN;
};


struct sctpCWR{
  struct sctpChunkDesc uh;
  u_int32_t TSN_reduced_at;
};

#ifdef	__cplusplus
}
#endif

#endif
