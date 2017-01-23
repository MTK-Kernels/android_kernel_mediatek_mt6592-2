/*
 * Copyright (c) 1999 - 2005 NetGroup, Politecnico di Torino (Italy)
 * Copyright (c) 2005 - 2008 CACE Technologies, Davis (California)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Politecnico di Torino, CACE Technologies 
 * nor the names of its contributors may be used to endorse or promote 
 * products derived from this software without specific prior written 
 * permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef lint
static const char rcsid[] _U_ =
    "@(#) $Header: /tcpdump/master/libpcap/pcap-win32.c,v 1.42 2008-05-21 22:15:25 gianluca Exp $ (LBL)";
#endif

#include <pcap-int.h>
#include <Packet32.h>
#ifdef __MINGW32__
#ifdef __MINGW64__
#include <ntddndis.h>
#else  
#include <ddk/ntddndis.h>
#include <ddk/ndis.h>
#endif 
#else 
#include <ntddndis.h>
#endif 
#ifdef HAVE_DAG_API
#include <dagnew.h>
#include <dagapi.h>
#endif 
#ifdef __MINGW32__
int* _errno();
#define errno (*_errno())
#endif 

static int pcap_setfilter_win32_npf(pcap_t *, struct bpf_program *);
static int pcap_setfilter_win32_dag(pcap_t *, struct bpf_program *);
static int pcap_getnonblock_win32(pcap_t *, char *);
static int pcap_setnonblock_win32(pcap_t *, int, char *);

#define	WIN32_DEFAULT_USER_BUFFER_SIZE 256000

#define	WIN32_DEFAULT_KERNEL_BUFFER_SIZE 1000000

#define SWAPS(_X) ((_X & 0xff) << 8) | (_X >> 8)

struct pcap_win {
	int nonblock;

#ifdef HAVE_DAG_API
	int	dag_fcs_bits;	
#endif
};

struct bpf_hdr {
	struct timeval	bh_tstamp;	
	bpf_u_int32	bh_caplen;	
	bpf_u_int32	bh_datalen;	
	u_short		bh_hdrlen;	
};

CRITICAL_SECTION g_PcapCompileCriticalSection;

BOOL WINAPI DllMain(
  HANDLE hinstDLL,
  DWORD dwReason,
  LPVOID lpvReserved
)
{
	if (dwReason == DLL_PROCESS_ATTACH)
	{
		InitializeCriticalSection(&g_PcapCompileCriticalSection);
	}

	return TRUE;
}

int 
wsockinit()
{
	WORD wVersionRequested;
	WSADATA wsaData;
	int err;
	wVersionRequested = MAKEWORD( 1, 1); 
	err = WSAStartup( wVersionRequested, &wsaData );
	if ( err != 0 )
	{
		return -1;
	}
	return 0;
}


static int
pcap_stats_win32(pcap_t *p, struct pcap_stat *ps)
{

	if(PacketGetStats(p->adapter, (struct bpf_stat*)ps) != TRUE){
		snprintf(p->errbuf, PCAP_ERRBUF_SIZE, "PacketGetStats error: %s", pcap_win32strerror());
		return -1;
	}

	return 0;
}

static int
pcap_setbuff_win32(pcap_t *p, int dim)
{
	if(PacketSetBuff(p->adapter,dim)==FALSE)
	{
		snprintf(p->errbuf, PCAP_ERRBUF_SIZE, "driver error: not enough memory to allocate the kernel buffer");
		return -1;
	}
	return 0;
}

static int
pcap_setmode_win32(pcap_t *p, int mode)
{
	if(PacketSetMode(p->adapter,mode)==FALSE)
	{
		snprintf(p->errbuf, PCAP_ERRBUF_SIZE, "driver error: working mode not recognized");
		return -1;
	}

	return 0;
}

static int
pcap_setmintocopy_win32(pcap_t *p, int size)
{
	if(PacketSetMinToCopy(p->adapter, size)==FALSE)
	{
		snprintf(p->errbuf, PCAP_ERRBUF_SIZE, "driver error: unable to set the requested mintocopy size");
		return -1;
	}
	return 0;
}

static Adapter *
pcap_getadapter_win32(pcap_t *p)
{
	return p->adapter;
}

static int
pcap_read_win32_npf(pcap_t *p, int cnt, pcap_handler callback, u_char *user)
{
	int cc;
	int n = 0;
	register u_char *bp, *ep;

	cc = p->cc;
	if (p->cc == 0) {
		if (p->break_loop) {
			p->break_loop = 0;
			return (-2);
		}

	    
		if(PacketReceivePacket(p->adapter,p->Packet,TRUE)==FALSE){
			snprintf(p->errbuf, PCAP_ERRBUF_SIZE, "read error: PacketReceivePacket failed");
			return (-1);
		}
			
		cc = p->Packet->ulBytesReceived;

		bp = p->Packet->Buffer;
	} 
	else
		bp = p->bp;

#define bhp ((struct bpf_hdr *)bp)
	ep = bp + cc;
	while (1) {
		register int caplen, hdrlen;

		if (p->break_loop) {
			if (n == 0) {
				p->break_loop = 0;
				return (-2);
			} else {
				p->bp = bp;
				p->cc = ep - bp;
				return (n);
			}
		}
		if (bp >= ep)
			break;

		caplen = bhp->bh_caplen;
		hdrlen = bhp->bh_hdrlen;

		(*callback)(user, (struct pcap_pkthdr*)bp, bp + hdrlen);
		bp += Packet_WORDALIGN(caplen + hdrlen);
		if (++n >= cnt && !PACKET_COUNT_IS_UNLIMITED(cnt)) {
			p->bp = bp;
			p->cc = ep - bp;
			return (n);
		}
	}
#undef bhp
	p->cc = 0;
	return (n);
}

#ifdef HAVE_DAG_API
static int
pcap_read_win32_dag(pcap_t *p, int cnt, pcap_handler callback, u_char *user)
{
	struct pcap_win *pw = p->priv;
	u_char *dp = NULL;
	int	packet_len = 0, caplen = 0;
	struct pcap_pkthdr	pcap_header;
	u_char *endofbuf;
	int n = 0;
	dag_record_t *header;
	unsigned erf_record_len;
	ULONGLONG ts;
	int cc;
	unsigned swt;
	unsigned dfp = p->adapter->DagFastProcess;

	cc = p->cc;
	if (cc == 0) 
	{
	    
		if(PacketReceivePacket(p->adapter, p->Packet, TRUE)==FALSE){
			snprintf(p->errbuf, PCAP_ERRBUF_SIZE, "read error: PacketReceivePacket failed");
			return (-1);
		}

		cc = p->Packet->ulBytesReceived;
		if(cc == 0)
			
			return 0;
		header = (dag_record_t*)p->adapter->DagBuffer;
	} 
	else
		header = (dag_record_t*)p->bp;
	
	endofbuf = (char*)header + cc;
	
	do
	{
		erf_record_len = SWAPS(header->rlen);
		if((char*)header + erf_record_len > endofbuf)
			break;

		
		pw->stat.ps_recv++;
		
		
		dp = ((u_char *)header) + dag_record_size;

		
		switch(header->type) 
		{
		case TYPE_ATM:
			packet_len = ATM_SNAPLEN;
			caplen = ATM_SNAPLEN;
			dp += 4;

			break;

		case TYPE_ETH:
			swt = SWAPS(header->wlen);
			packet_len = swt - (pw->dag_fcs_bits);
			caplen = erf_record_len - dag_record_size - 2;
			if (caplen > packet_len)
			{
				caplen = packet_len;
			}
			dp += 2;
			
			break;
		
		case TYPE_HDLC_POS:
			swt = SWAPS(header->wlen);
			packet_len = swt - (pw->dag_fcs_bits);
			caplen = erf_record_len - dag_record_size;
			if (caplen > packet_len)
			{
				caplen = packet_len;
			}
			
			break;
		}
		
		if(caplen > p->snapshot)
			caplen = p->snapshot;

		if (p->break_loop) 
		{
			if (n == 0) 
			{
				p->break_loop = 0;
				return (-2);
			} 
			else 
			{
				p->bp = (char*)header;
				p->cc = endofbuf - (char*)header;
				return (n);
			}
		}

		if(!dfp)
		{
			
			ts = header->ts;
			pcap_header.ts.tv_sec = (int)(ts >> 32);
			ts = (ts & 0xffffffffi64) * 1000000;
			ts += 0x80000000; 
			pcap_header.ts.tv_usec = (int)(ts >> 32);
			if (pcap_header.ts.tv_usec >= 1000000) {
				pcap_header.ts.tv_usec -= 1000000;
				pcap_header.ts.tv_sec++;
			}
		}
		
		
		if (p->fcode.bf_insns) 
		{
			if (bpf_filter(p->fcode.bf_insns, dp, packet_len, caplen) == 0) 
			{
				
				header = (dag_record_t*)((char*)header + erf_record_len);
				continue;
			}
		}
		
		
		pcap_header.caplen = caplen;
		pcap_header.len = packet_len;
		
		
		(*callback)(user, &pcap_header, dp);
		
		
		header = (dag_record_t*)((char*)header + erf_record_len);

		
		if (++n >= cnt && !PACKET_COUNT_IS_UNLIMITED(cnt)) 
		{
			p->bp = (char*)header;
			p->cc = endofbuf - (char*)header;
			return (n);
		}
	}
	while((u_char*)header < endofbuf);
	
  return 1;
}
#endif 

static int 
pcap_inject_win32(pcap_t *p, const void *buf, size_t size){
	LPPACKET PacketToSend;

	PacketToSend=PacketAllocatePacket();
	
	if (PacketToSend == NULL)
	{
		snprintf(p->errbuf, PCAP_ERRBUF_SIZE, "send error: PacketAllocatePacket failed");
		return -1;
	}
	
	PacketInitPacket(PacketToSend,(PVOID)buf,size);
	if(PacketSendPacket(p->adapter,PacketToSend,TRUE) == FALSE){
		snprintf(p->errbuf, PCAP_ERRBUF_SIZE, "send error: PacketSendPacket failed");
		PacketFreePacket(PacketToSend);
		return -1;
	}

	PacketFreePacket(PacketToSend);

	return size;
}

static void
pcap_cleanup_win32(pcap_t *p)
{
	if (p->adapter != NULL) {
		PacketCloseAdapter(p->adapter);
		p->adapter = NULL;
	}
	if (p->Packet) {
		PacketFreePacket(p->Packet);
		p->Packet = NULL;
	}
	pcap_cleanup_live_common(p);
}

static int
pcap_activate_win32(pcap_t *p)
{
	struct pcap_win *pw = p->priv;
	NetType type;

	if (p->opt.rfmon) {
		return (PCAP_ERROR_RFMON_NOTSUP);
	}

	
	wsockinit();

	p->adapter = PacketOpenAdapter(p->opt.source);
	
	if (p->adapter == NULL)
	{
		
		snprintf(p->errbuf, PCAP_ERRBUF_SIZE, "Error opening adapter: %s", pcap_win32strerror());
		return PCAP_ERROR;
	}
	
	
	if(PacketGetNetType (p->adapter,&type) == FALSE)
	{
		snprintf(p->errbuf, PCAP_ERRBUF_SIZE, "Cannot determine the network type: %s", pcap_win32strerror());
		goto bad;
	}
	
	
	switch (type.LinkType) 
	{
	case NdisMediumWan:
		p->linktype = DLT_EN10MB;
		break;
		
	case NdisMedium802_3:
		p->linktype = DLT_EN10MB;
		p->dlt_list = (u_int *) malloc(sizeof(u_int) * 2);
		if (p->dlt_list != NULL) {
			p->dlt_list[0] = DLT_EN10MB;
			p->dlt_list[1] = DLT_DOCSIS;
			p->dlt_count = 2;
		}
		break;
		
	case NdisMediumFddi:
		p->linktype = DLT_FDDI;
		break;
		
	case NdisMedium802_5:			
		p->linktype = DLT_IEEE802;	
		break;
		
	case NdisMediumArcnetRaw:
		p->linktype = DLT_ARCNET;
		break;
		
	case NdisMediumArcnet878_2:
		p->linktype = DLT_ARCNET;
		break;
		
	case NdisMediumAtm:
		p->linktype = DLT_ATM_RFC1483;
		break;
		
	case NdisMediumCHDLC:
		p->linktype = DLT_CHDLC;
		break;

	case NdisMediumPPPSerial:
		p->linktype = DLT_PPP_SERIAL;
		break;

	case NdisMediumNull:
		p->linktype = DLT_NULL;
		break;

	case NdisMediumBare80211:
		p->linktype = DLT_IEEE802_11;
		break;

	case NdisMediumRadio80211:
		p->linktype = DLT_IEEE802_11_RADIO;
		break;

	case NdisMediumPpi:
		p->linktype = DLT_PPI;
		break;

	default:
		p->linktype = DLT_EN10MB;			
		break;
	}

	
	if (p->opt.promisc) 
	{

		if (PacketSetHwFilter(p->adapter,NDIS_PACKET_TYPE_PROMISCUOUS) == FALSE)
		{
			snprintf(p->errbuf, PCAP_ERRBUF_SIZE, "failed to set hardware filter to promiscuous mode");
			goto bad;
		}
	}
	else 
	{
		if (PacketSetHwFilter(p->adapter,NDIS_PACKET_TYPE_ALL_LOCAL) == FALSE)
		{
			snprintf(p->errbuf, PCAP_ERRBUF_SIZE, "failed to set hardware filter to non-promiscuous mode");
			goto bad;
		}
	}

	
	p->bufsize = WIN32_DEFAULT_USER_BUFFER_SIZE;

	
	if((p->Packet = PacketAllocatePacket())==NULL)
	{
		snprintf(p->errbuf, PCAP_ERRBUF_SIZE, "failed to allocate the PACKET structure");
		goto bad;
	}

	if(!(p->adapter->Flags & INFO_FLAG_DAG_CARD))
	{
	 	if (p->opt.buffer_size == 0)
	 		p->opt.buffer_size = WIN32_DEFAULT_KERNEL_BUFFER_SIZE;

		if(PacketSetBuff(p->adapter,p->opt.buffer_size)==FALSE)
		{
			snprintf(p->errbuf, PCAP_ERRBUF_SIZE, "driver error: not enough memory to allocate the kernel buffer");
			goto bad;
		}
		
		p->buffer = (u_char *)malloc(p->bufsize);
		if (p->buffer == NULL) 
		{
			snprintf(p->errbuf, PCAP_ERRBUF_SIZE, "malloc: %s", pcap_strerror(errno));
			goto bad;
		}
		
		PacketInitPacket(p->Packet,(BYTE*)p->buffer,p->bufsize);
		
		if (p-opt.immediate)
		{
			
			if(PacketSetMinToCopy(p->adapter,0)==FALSE)
			{
				snprintf(p->errbuf, PCAP_ERRBUF_SIZE,"Error calling PacketSetMinToCopy: %s", pcap_win32strerror());
				goto bad;
			}
		}
		else
		{
			
			if(PacketSetMinToCopy(p->adapter,16000)==FALSE)
			{
				snprintf(p->errbuf, PCAP_ERRBUF_SIZE,"Error calling PacketSetMinToCopy: %s", pcap_win32strerror());
				goto bad;
			}
		}
	}
	else
#ifdef HAVE_DAG_API
	{
		LONG	status;
		HKEY	dagkey;
		DWORD	lptype;
		DWORD	lpcbdata;
		int		postype = 0;
		char	keyname[512];
		
		snprintf(keyname, sizeof(keyname), "%s\\CardParams\\%s", 
			"SYSTEM\\CurrentControlSet\\Services\\DAG",
			strstr(_strlwr(p->opt.source), "dag"));
		do
		{
			status = RegOpenKeyEx(HKEY_LOCAL_MACHINE, keyname, 0, KEY_READ, &dagkey);
			if(status != ERROR_SUCCESS)
				break;
			
			status = RegQueryValueEx(dagkey,
				"PosType",
				NULL,
				&lptype,
				(char*)&postype,
				&lpcbdata);
			
			if(status != ERROR_SUCCESS)
			{
				postype = 0;
			}
			
			RegCloseKey(dagkey);
		}
		while(FALSE);
		
		
		p->snapshot = PacketSetSnapLen(p->adapter, snaplen);
		
		pw->dag_fcs_bits = p->adapter->DagFcsLen;
	}
#else
	goto bad;
#endif 
	
	PacketSetReadTimeout(p->adapter, p->opt.timeout);
	
#ifdef HAVE_DAG_API
	if(p->adapter->Flags & INFO_FLAG_DAG_CARD)
	{
		
		p->read_op = pcap_read_win32_dag;
		p->setfilter_op = pcap_setfilter_win32_dag;
	}
	else
	{
#endif 
		
		p->read_op = pcap_read_win32_npf;
		p->setfilter_op = pcap_setfilter_win32_npf;
#ifdef HAVE_DAG_API
	}
#endif 
	p->setdirection_op = NULL;	
	    
	p->inject_op = pcap_inject_win32;
	p->set_datalink_op = NULL;	
	p->getnonblock_op = pcap_getnonblock_win32;
	p->setnonblock_op = pcap_setnonblock_win32;
	p->stats_op = pcap_stats_win32;
	p->setbuff_op = pcap_setbuff_win32;
	p->setmode_op = pcap_setmode_win32;
	p->setmintocopy_op = pcap_setmintocopy_win32;
	p->getadapter_op = pcap_getadapter_win32;
	p->cleanup_op = pcap_cleanup_win32;

	return (0);
bad:
	pcap_cleanup_win32(p);
	return (PCAP_ERROR);
}

pcap_t *
pcap_create_interface(const char *device, char *ebuf)
{
	pcap_t *p;

	if (strlen(device) == 1)
	{
		size_t length;
		char* deviceAscii;

		length = wcslen((wchar_t*)device);

		deviceAscii = (char*)malloc(length + 1);

		if (deviceAscii == NULL)
		{
			snprintf(ebuf, PCAP_ERRBUF_SIZE, "Malloc failed");
			return NULL;
		}

		snprintf(deviceAscii, length + 1, "%ws", (wchar_t*)device);
		p = pcap_create_common(deviceAscii, ebuf, sizeof (struct pcap_win));
		free(deviceAscii);
	}
	else
	{
		p = pcap_create_common(device, ebuf, sizeof (struct pcap_win));
	}

	if (p == NULL)
		return (NULL);

	p->activate_op = pcap_activate_win32;
	return (p);
}

static int
pcap_setfilter_win32_npf(pcap_t *p, struct bpf_program *fp)
{
	if(PacketSetBpf(p->adapter,fp)==FALSE){
		snprintf(p->errbuf, PCAP_ERRBUF_SIZE, "Driver error: cannot set bpf filter: %s", pcap_win32strerror());
		return (-1);
	}

	p->cc = 0;
	return (0);
}

static int 
pcap_setfilter_win32_dag(pcap_t *p, struct bpf_program *fp) {
	
	if(!fp) 
	{
		strncpy(p->errbuf, "setfilter: No filter specified", sizeof(p->errbuf));
		return -1;
	}
	
	
	if (install_bpf_program(p, fp) < 0) 
	{
		snprintf(p->errbuf, sizeof(p->errbuf),
			"setfilter, unable to install the filter: %s", pcap_strerror(errno));
		return -1;
	}
	
	return (0);
}

static int
pcap_getnonblock_win32(pcap_t *p, char *errbuf)
{
	struct pcap_win *pw = p->priv;

	return (pw->nonblock);
}

static int
pcap_setnonblock_win32(pcap_t *p, int nonblock, char *errbuf)
{
	struct pcap_win *pw = p->priv;
	int newtimeout;

	if (nonblock) {
		newtimeout = -1;
	} else {
		newtimeout = p->opt.timeout;
	}
	if (!PacketSetReadTimeout(p->adapter, newtimeout)) {
		snprintf(errbuf, PCAP_ERRBUF_SIZE,
		    "PacketSetReadTimeout: %s", pcap_win32strerror());
		return (-1);
	}
	pw->nonblock = (newtimeout == -1);
	return (0);
}

int
pcap_platform_finddevs(pcap_if_t **alldevsp, char *errbuf)
{
	return (0);
}
