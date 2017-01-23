
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
 * 3. Neither the name of Cisco nor of Motorola may be used
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


#ifndef __sctpConstants_h__
#define __sctpConstants_h__



#define PROTO_SIGNATURE_A 0x30000000

#define SCTP_VERSION_NUMBER 0x3

#define MAX_TSN 0xffffffff
#define MAX_SEQ 0xffff

#define SCTP_IGNORE_CWND_ON_FR 1
#define SCTP_DEF_MAX_BURST 4

#define SCTP_DATAGRAM_UNSENT 		0
#define SCTP_DATAGRAM_SENT   		1
#define SCTP_DATAGRAM_RESEND1		2 
#define SCTP_DATAGRAM_RESEND2		3 
#define SCTP_DATAGRAM_RESEND3		4 
#define SCTP_DATAGRAM_RESEND		5
#define SCTP_DATAGRAM_ACKED		10010
#define SCTP_DATAGRAM_INBOUND		10011
#define SCTP_READY_TO_TRANSMIT		10012
#define SCTP_DATAGRAM_MARKED		20010

#define MAX_FSID 64	

#define SCTP_MSGTYPE_MASK	0xff

#define SCTP_DATA		0x00
#define SCTP_INITIATION		0x01
#define SCTP_INITIATION_ACK	0x02
#define SCTP_SELECTIVE_ACK	0x03
#define SCTP_HEARTBEAT_REQUEST	0x04
#define SCTP_HEARTBEAT_ACK	0x05
#define SCTP_ABORT_ASSOCIATION	0x06
#define SCTP_SHUTDOWN		0x07
#define SCTP_SHUTDOWN_ACK	0x08
#define SCTP_OPERATION_ERR	0x09
#define SCTP_COOKIE_ECHO	0x0a
#define SCTP_COOKIE_ACK         0x0b
#define SCTP_ECN_ECHO		0x0c
#define SCTP_ECN_CWR		0x0d
#define SCTP_SHUTDOWN_COMPLETE	0x0e
#define SCTP_FORWARD_CUM_TSN    0xc0
#define SCTP_RELIABLE_CNTL      0xc1
#define SCTP_RELIABLE_CNTL_ACK  0xc2

#define SCTP_HAD_NO_TCB		0x01

#define SCTP_DATA_FRAG_MASK	0x03
#define SCTP_DATA_MIDDLE_FRAG	0x00
#define SCTP_DATA_LAST_FRAG	0x01
#define SCTP_DATA_FIRST_FRAG	0x02
#define SCTP_DATA_NOT_FRAG	0x03
#define SCTP_DATA_UNORDERED	0x04

#define SCTP_CRC_ENABLE_BIT	0x01	

#define isSCTPControl(a) (a->chunkID != SCTP_DATA)
#define isSCTPData(a) (a->chunkID == SCTP_DATA)


#define SCTP_IPV4_PARAM_TYPE    0x0005
#define SCTP_IPV6_PARAM_TYPE    0x0006
#define SCTP_RESPONDER_COOKIE   0x0007
#define SCTP_UNRECOG_PARAM	0x0008
#define SCTP_COOKIE_PRESERVE    0x0009
#define SCTP_HOSTNAME_VIA_DNS   0x000b
#define SCTP_RESTRICT_ADDR_TO	0x000c

#define SCTP_ECN_I_CAN_DO_ECN	0x8000
#define SCTP_OPERATION_SUCCEED	0x4001
#define SCTP_ERROR_NOT_EXECUTED	0x4002

#define SCTP_UNRELIABLE_STRM    0xc000
#define SCTP_ADD_IP_ADDRESS     0xc001
#define SCTP_DEL_IP_ADDRESS     0xc002
#define SCTP_STRM_FLOW_LIMIT    0xc003
#define SCTP_PARTIAL_CSUM       0xc004
#define SCTP_ERROR_CAUSE_TLV	0xc005
#define SCTP_MIT_STACK_NAME	0xc006
#define SCTP_SETADDRESS_PRIMARY 0xc007

#define SCTP_ECT_BIT		0x02
#define SCTP_CE_BIT		0x01

#define SCTP_OP_ERROR_NO_ERROR		0x0000
#define SCTP_OP_ERROR_INV_STRM		0x0001
#define SCTP_OP_ERROR_MISS_PARAM	0x0002
#define SCTP_OP_ERROR_STALE_COOKIE	0x0003
#define SCTP_OP_ERROR_NO_RESOURCE 	0x0004
#define SCTP_OP_ERROR_DNS_FAILED   	0x0005
#define SCTP_OP_ERROR_UNK_CHUNK	   	0x0006
#define SCTP_OP_ERROR_INV_PARAM		0x0007
#define SCTP_OP_ERROR_UNK_PARAM	       	0x0008
#define SCTP_OP_ERROR_NO_USERD    	0x0009
#define SCTP_OP_ERROR_COOKIE_SHUT	0x000a
#define SCTP_OP_ERROR_DELETE_LAST	0x000b
#define SCTP_OP_ERROR_RESOURCE_SHORT 	0x000c

#define SCTP_MAX_ERROR_CAUSE  12


#define HEART_BEAT_PARAM 0x0001



#define SCTP_ORDERED_DELIVERY		0x01
#define SCTP_NON_ORDERED_DELIVERY	0x02
#define SCTP_DO_CRC16			0x08
#define SCTP_MY_ADDRESS_ONLY		0x10

#define SCTP_FLEXIBLE_ADDRESS		0x20
#define SCTP_NO_HEARTBEAT		0x40

#define SCTP_STICKY_OPTIONS_MASK        0x0c

#define SCTP_DONT_FRAGMENT		0x0100
#define SCTP_FRAGMENT_OK		0x0200


#define SCTP_STATE_EMPTY		0x0000
#define SCTP_STATE_INUSE		0x0001
#define SCTP_STATE_COOKIE_WAIT		0x0002
#define SCTP_STATE_COOKIE_SENT		0x0004
#define SCTP_STATE_OPEN			0x0008
#define SCTP_STATE_SHUTDOWN		0x0010
#define SCTP_STATE_SHUTDOWN_RECV	0x0020
#define SCTP_STATE_SHUTDOWN_ACK_SENT	0x0040
#define SCTP_STATE_SHUTDOWN_PEND	0x0080
#define SCTP_STATE_MASK			0x007f
#define SCTP_ADDR_NOT_REACHABLE		1
#define SCTP_ADDR_REACHABLE		2
#define SCTP_ADDR_NOHB			4
#define SCTP_ADDR_BEING_DELETED		8

#define SCTP_DEFAULT_COOKIE_LIFE 60 

#define MAX_SCTP_STREAMS 2048


#define SCTP_STARTING_MAPARRAY 10000

#define SCTP_TIMER_INIT 	0
#define SCTP_TIMER_RECV 	1
#define SCTP_TIMER_SEND 	2
#define SCTP_TIMER_SHUTDOWN	3
#define SCTP_TIMER_HEARTBEAT	4
#define SCTP_TIMER_PMTU		5
#define SCTP_NUM_TMRS 6



#define SCTP_IPV4_ADDRESS	2
#define SCTP_IPV6_ADDRESS	4

#define SctpTimerTypeNone		0
#define SctpTimerTypeSend		1
#define SctpTimerTypeInit		2
#define SctpTimerTypeRecv		3
#define SctpTimerTypeShutdown		4
#define SctpTimerTypeHeartbeat		5
#define SctpTimerTypeCookie		6
#define SctpTimerTypeNewCookie		7
#define SctpTimerTypePathMtuRaise	8
#define SctpTimerTypeShutdownAck	9
#define SctpTimerTypeRelReq		10

#define SCTP_TIMER_START	1
#define SCTP_TIMER_STOP		2

#define SCTP_TIMER_IDLE		0x0
#define SCTP_TIMER_EXPIRED	0x1
#define SCTP_TIMER_RUNNING	0x2


#define SCTP_MAX_NET_TIMERS     6	
#define SCTP_NUMBER_TIMERS	12	


#define SCTP_MAX_STALE_COOKIES_I_COLLECT 10

#define SCTP_MAX_DUP_TSNS      20

#define SCTP_MAXATTEMPT_INIT 2
#define SCTP_MAXATTEMPT_SEND 3


#define SCTP_INIT_SEC	3
#define SCTP_INIT_NSEC	0

#define SCTP_SEND_SEC	1
#define SCTP_SEND_NSEC	0

#define SCTP_RECV_SEC	0
#define SCTP_RECV_NSEC	200000000

#define SCTP_HB_SEC	30
#define SCTP_HB_NSEC	0


#define SCTP_SHUTDOWN_SEC	0
#define SCTP_SHUTDOWN_NSEC	300000000

#define SCTP_RTO_UPPER_BOUND 60000000 
#define SCTP_RTO_UPPER_BOUND_SEC 60  
#define SCTP_RTO_LOWER_BOUND  1000000 

#define SCTP_DEF_MAX_INIT 8
#define SCTP_DEF_MAX_SEND 10

#define SCTP_DEF_PMTU_RAISE 600  
#define SCTP_DEF_PMTU_MIN   600

#define SCTP_MSEC_IN_A_SEC  1000
#define SCTP_USEC_IN_A_SEC  1000000
#define SCTP_NSEC_IN_A_SEC  1000000000


#define SCTP_EVENT_READ		0x000001
#define SCTP_EVENT_WRITE	0x000002
#define SCTP_EVENT_EXCEPT	0x000004


#define SCTP_MAX_OUTSTANDING_DG	10000



#define SCTP_MAX_READBUFFER 65536
#define SCTP_ADDRMAX 60

#define SCTP_MIN_RWND	1500

#define SCTP_WINDOW_MIN	1500	
#define SCTP_WINDOW_MAX 1048576	

#define SCTP_MAX_BUNDLE_UP 256	

#define SCTP_DEFAULT_MAXMSGREASM 1048576


#define SCTP_DEFAULT_MAXWINDOW	32768	
#define SCTP_DEFAULT_MAXSEGMENT 1500	
#ifdef LYNX
#define DEFAULT_MTU_CEILING  1500 	
#else
#define DEFAULT_MTU_CEILING  2048	
#endif
#define SCTP_DEFAULT_MINSEGMENT 512	
#define SCTP_HOW_MANY_SECRETS 2		
#define SCTP_HOW_LONG_COOKIE_LIVE 3600	

#define SCTP_NUMBER_OF_SECRETS	8	
#define SCTP_SECRET_SIZE 32		

#ifdef USE_MD5
#define SCTP_SIGNATURE_SIZE 16	
#else
#define SCTP_SIGNATURE_SIZE 20	
#endif

#define SCTP_NOTIFY_ASSOC_UP		1

#define SCTP_NOTIFY_ASSOC_DOWN		2

#define SCTP_NOTIFY_INTF_DOWN		3

#define SCTP_NOTIFY_INTF_UP		4

#define SCTP_NOTIFY_DG_FAIL		5

#define SCTP_NOTIFY_STRDATA_ERR 	6

#define SCTP_NOTIFY_ASSOC_ABORTED	7

#define SCTP_NOTIFY_PEER_OPENED_STR	8
#define SCTP_NOTIFY_STREAM_OPENED_OK	9

#define SCTP_NOTIFY_ASSOC_RESTART	10

#define SCTP_NOTIFY_HB_RESP             11

#define SCTP_NOTIFY_RELREQ_RESULT_OK		12
#define SCTP_NOTIFY_RELREQ_RESULT_FAILED	13

#define SCTP_CLOCK_GRAINULARITY 10000

#define IP_HDR_SIZE 40		

#define SCTP_NUM_FDS 3

#define SCTP_FD_IP   0
#define SCTP_FD_ICMP 1
#define SCTP_REQUEST 2


#define SCTP_DEAMON_PORT 9899

#define DEAMON_REGISTER       0x01
#define DEAMON_REGISTER_ACK   0x02
#define DEAMON_DEREGISTER     0x03
#define DEAMON_DEREGISTER_ACK 0x04
#define DEAMON_CHECKADDR_LIST 0x05

#define DEAMON_MAGIC_VER_LEN 0xff

#define SCTP_MAX_ATTEMPTS_AT_DEAMON 5
#define SCTP_TIMEOUT_IN_POLL_FOR_DEAMON 1500 

#define compare_with_wrap(a, b, M) ((a > b) && ((a - b) < (M >> 1))) || \
              ((b > a) && ((b - a) > (M >> 1)))

#ifndef TIMEVAL_TO_TIMESPEC
#define TIMEVAL_TO_TIMESPEC(tv, ts)			\
{							\
    (ts)->tv_sec  = (tv)->tv_sec;			\
    (ts)->tv_nsec = (tv)->tv_usec * 1000;		\
}
#endif

#define SCTP_NUMBER_OF_PEGS 21
#define SCTP_PEG_SACKS_SEEN 0
#define SCTP_PEG_SACKS_SENT 1
#define SCTP_PEG_TSNS_SENT  2
#define SCTP_PEG_TSNS_RCVD  3
#define SCTP_DATAGRAMS_SENT 4
#define SCTP_DATAGRAMS_RCVD 5
#define SCTP_RETRANTSN_SENT 6
#define SCTP_DUPTSN_RECVD   7
#define SCTP_HBR_RECV	    8
#define SCTP_HBA_RECV       9
#define SCTP_HB_SENT	   10
#define SCTP_DATA_DG_SENT  11
#define SCTP_DATA_DG_RECV  12
#define SCTP_TMIT_TIMER    13
#define SCTP_RECV_TIMER    14
#define SCTP_HB_TIMER      15
#define SCTP_FAST_RETRAN   16
#define SCTP_PEG_TSNS_READ 17
#define SCTP_NONE_LFT_TO   18
#define SCTP_NONE_LFT_RWND 19
#define SCTP_NONE_LFT_CWND 20



#endif

