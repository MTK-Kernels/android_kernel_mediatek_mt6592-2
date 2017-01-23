/*-
 * Copyright (c) 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1997
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from the Stanford/CMU enet packet filter,
 * (net/enet.c) distributed as part of 4.3BSD, and code contributed
 * to Berkeley by Steven McCanne and Van Jacobson both of Lawrence 
 * Berkeley Laboratory.
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
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *      @(#)bpf.h       7.1 (Berkeley) 5/7/91
 *
 * @(#) $Header: /tcpdump/master/libpcap/pcap/bpf.h,v 1.32 2008-12-23 20:13:29 guy Exp $ (LBL)
 */


#if !defined(_NET_BPF_H_) && !defined(_BPF_H_) && !defined(_H_BPF) && !defined(lib_pcap_bpf_h)
#define lib_pcap_bpf_h

#ifdef __cplusplus
extern "C" {
#endif

#define BPF_RELEASE 199606

#ifdef MSDOS 
typedef long          bpf_int32;
typedef unsigned long bpf_u_int32;
#else
typedef	int bpf_int32;
typedef	u_int bpf_u_int32;
#endif

#ifndef __NetBSD__
#define BPF_ALIGNMENT sizeof(bpf_int32)
#else
#define BPF_ALIGNMENT sizeof(long)
#endif
#define BPF_WORDALIGN(x) (((x)+(BPF_ALIGNMENT-1))&~(BPF_ALIGNMENT-1))

struct bpf_program {
	u_int bf_len;
	struct bpf_insn *bf_insns;
};
 

#define DLT_NULL	0	
#define DLT_EN10MB	1	
#define DLT_EN3MB	2	
#define DLT_AX25	3	
#define DLT_PRONET	4	
#define DLT_CHAOS	5	
#define DLT_IEEE802	6	
#define DLT_ARCNET	7	
#define DLT_SLIP	8	
#define DLT_PPP		9	
#define DLT_FDDI	10	

#define DLT_ATM_RFC1483	11	

#ifdef __OpenBSD__
#define DLT_RAW		14	
#else
#define DLT_RAW		12	
#endif

#if defined(__NetBSD__) || defined(__FreeBSD__)
#ifndef DLT_SLIP_BSDOS
#define DLT_SLIP_BSDOS	13	
#define DLT_PPP_BSDOS	14	
#endif
#else
#define DLT_SLIP_BSDOS	15	
#define DLT_PPP_BSDOS	16	
#endif


#if defined(__OpenBSD__) || defined(__NetBSD__) || defined(__DragonFly__) || defined(__APPLE__)
#define DLT_PFSYNC	18
#endif

#define DLT_ATM_CLIP	19	

#define DLT_REDBACK_SMARTEDGE	32

#define DLT_PPP_SERIAL	50	
#define DLT_PPP_ETHER	51	

#define DLT_SYMANTEC_FIREWALL	99


#define DLT_MATCHING_MIN	104

/*
 * This value was defined by libpcap 0.5; platforms that have defined
 * it with a different value should define it here with that value -
 * a link type of 104 in a save file will be mapped to DLT_C_HDLC,
 * whatever value that happens to be, so programs will correctly
 * handle files with that link type regardless of the value of
 * DLT_C_HDLC.
 *
 * The name DLT_C_HDLC was used by BSD/OS; we use that name for source
 * compatibility with programs written for BSD/OS.
 *
 * libpcap 0.5 defined it as DLT_CHDLC; we define DLT_CHDLC as well,
 * for source compatibility with programs written for libpcap 0.5.
 */
#define DLT_C_HDLC	104	
#define DLT_CHDLC	DLT_C_HDLC

#define DLT_IEEE802_11	105	


#define DLT_FRELAY	107

#ifdef __OpenBSD__
#define DLT_LOOP	12
#else
#define DLT_LOOP	108
#endif

#ifdef __OpenBSD__
#define DLT_ENC		13
#else
#define DLT_ENC		109
#endif


#define DLT_LINUX_SLL	113

#define DLT_LTALK	114

#define DLT_ECONET	115

#define DLT_IPFILTER	116

#define DLT_PFLOG	117

#define DLT_CISCO_IOS	118

#define DLT_PRISM_HEADER	119

#define DLT_AIRONET_HEADER	120

#ifdef __FreeBSD__
#define DLT_PFSYNC		121
#else
#define DLT_HHDLC		121
#endif

#define DLT_IP_OVER_FC		122

#define DLT_SUNATM		123	

#define DLT_RIO                 124     
#define DLT_PCI_EXP             125     
#define DLT_AURORA              126     

#define DLT_IEEE802_11_RADIO	127	

#define DLT_TZSP                128     

#define DLT_ARCNET_LINUX	129	

#define DLT_JUNIPER_MLPPP       130
#define DLT_JUNIPER_MLFR        131
#define DLT_JUNIPER_ES          132
#define DLT_JUNIPER_GGSN        133
#define DLT_JUNIPER_MFR         134
#define DLT_JUNIPER_ATM2        135
#define DLT_JUNIPER_SERVICES    136
#define DLT_JUNIPER_ATM1        137

#define DLT_APPLE_IP_OVER_IEEE1394	138

#define DLT_MTP2_WITH_PHDR	139	
#define DLT_MTP2		140	
#define DLT_MTP3		141	
#define DLT_SCCP		142	

#define DLT_DOCSIS		143

#define DLT_LINUX_IRDA		144

#define DLT_IBM_SP		145
#define DLT_IBM_SN		146

#define DLT_USER0		147
#define DLT_USER1		148
#define DLT_USER2		149
#define DLT_USER3		150
#define DLT_USER4		151
#define DLT_USER5		152
#define DLT_USER6		153
#define DLT_USER7		154
#define DLT_USER8		155
#define DLT_USER9		156
#define DLT_USER10		157
#define DLT_USER11		158
#define DLT_USER12		159
#define DLT_USER13		160
#define DLT_USER14		161
#define DLT_USER15		162

#define DLT_IEEE802_11_RADIO_AVS 163	

#define DLT_JUNIPER_MONITOR     164

#define DLT_BACNET_MS_TP	165

#define DLT_PPP_PPPD		166

#define DLT_PPP_WITH_DIRECTION	DLT_PPP_PPPD
#define DLT_LINUX_PPP_WITHDIRECTION	DLT_PPP_PPPD

#define DLT_JUNIPER_PPPOE       167
#define DLT_JUNIPER_PPPOE_ATM   168

#define DLT_GPRS_LLC		169	
#define DLT_GPF_T		170	
#define DLT_GPF_F		171	

#define DLT_GCOM_T1E1		172
#define DLT_GCOM_SERIAL		173

#define DLT_JUNIPER_PIC_PEER    174

#define DLT_ERF_ETH		175	
#define DLT_ERF_POS		176	

#define DLT_LINUX_LAPD		177

#define DLT_JUNIPER_ETHER       178
#define DLT_JUNIPER_PPP         179
#define DLT_JUNIPER_FRELAY      180
#define DLT_JUNIPER_CHDLC       181

#define DLT_MFR                 182

#define DLT_JUNIPER_VP          183

#define DLT_A429                184

#define DLT_A653_ICM            185

#define DLT_USB			186

#define DLT_BLUETOOTH_HCI_H4	187

#define DLT_IEEE802_16_MAC_CPS	188

#define DLT_USB_LINUX		189

#define DLT_CAN20B              190

#define DLT_IEEE802_15_4_LINUX	191

#define DLT_PPI			192

#define DLT_IEEE802_16_MAC_CPS_RADIO	193

#define DLT_JUNIPER_ISM         194

#define DLT_IEEE802_15_4	195

#define DLT_SITA		196

#define DLT_ERF			197

#define DLT_RAIF1		198

#define DLT_IPMB		199

#define DLT_JUNIPER_ST          200

#define DLT_BLUETOOTH_HCI_H4_WITH_PHDR	201

#define DLT_AX25_KISS		202

#define DLT_LAPD		203

#define DLT_PPP_WITH_DIR	204	
#define DLT_C_HDLC_WITH_DIR	205	
#define DLT_FRELAY_WITH_DIR	206	
#define DLT_LAPB_WITH_DIR	207	


#define DLT_IPMB_LINUX		209

#define DLT_FLEXRAY		210

#define DLT_MOST		211

#define DLT_LIN			212

#define DLT_X2E_SERIAL		213

#define DLT_X2E_XORAYA		214

#define DLT_IEEE802_15_4_NONASK_PHY	215

#define DLT_LINUX_EVDEV		216

#define DLT_GSMTAP_UM		217
#define DLT_GSMTAP_ABIS		218

#define DLT_MPLS		219

#define DLT_USB_LINUX_MMAPPED	220

#define DLT_DECT		221

/*
 * From: "Lidwa, Eric (GSFC-582.0)[SGT INC]" <eric.lidwa-1@nasa.gov>
 * Date: Mon, 11 May 2009 11:18:30 -0500
 *
 * DLT_AOS. We need it for AOS Space Data Link Protocol.
 *   I have already written dissectors for but need an OK from
 *   legal before I can submit a patch.
 *
 */
#define DLT_AOS                 222

#define DLT_WIHART		223

#define DLT_FC_2		224

#define DLT_FC_2_WITH_FRAME_DELIMS	225

#define DLT_IPNET		226

#define DLT_CAN_SOCKETCAN	227

#define DLT_IPV4		228
#define DLT_IPV6		229

#define DLT_IEEE802_15_4_NOFCS	230

#define DLT_DBUS		231

#define DLT_JUNIPER_VS			232
#define DLT_JUNIPER_SRX_E2E		233
#define DLT_JUNIPER_FIBRECHANNEL	234

#define DLT_DVB_CI		235

#define DLT_MUX27010		236

#define DLT_STANAG_5066_D_PDU	237

#define DLT_JUNIPER_ATM_CEMIC	238

#define DLT_NFLOG		239

#define DLT_NETANALYZER		240

#define DLT_NETANALYZER_TRANSPARENT	241

#define DLT_IPOIB		242

#define DLT_MPEG_2_TS		243

#define DLT_NG40		244

#define DLT_NFC_LLCP		245

#if !defined(__FreeBSD__) && !defined(__OpenBSD__) && !defined(__NetBSD__) && !defined(__DragonFly__) && !defined(__APPLE__)
#define DLT_PFSYNC		246
#endif

#define DLT_INFINIBAND		247

#define DLT_SCTP		248

#define DLT_USBPCAP		249

#define DLT_RTAC_SERIAL		250

#define DLT_BLUETOOTH_LE_LL	251

#define DLT_WIRESHARK_UPPER_PDU	252

#define DLT_MATCHING_MAX	252	

#define DLT_CLASS(x)		((x) & 0x03ff0000)

#define	DLT_CLASS_NETBSD_RAWAF	0x02240000
#define	DLT_NETBSD_RAWAF(af)	(DLT_CLASS_NETBSD_RAWAF | (af))
#define	DLT_NETBSD_RAWAF_AF(x)	((x) & 0x0000ffff)
#define	DLT_IS_NETBSD_RAWAF(x)	(DLT_CLASS(x) == DLT_CLASS_NETBSD_RAWAF)


#define BPF_CLASS(code) ((code) & 0x07)
#define		BPF_LD		0x00
#define		BPF_LDX		0x01
#define		BPF_ST		0x02
#define		BPF_STX		0x03
#define		BPF_ALU		0x04
#define		BPF_JMP		0x05
#define		BPF_RET		0x06
#define		BPF_MISC	0x07

#define BPF_SIZE(code)	((code) & 0x18)
#define		BPF_W		0x00
#define		BPF_H		0x08
#define		BPF_B		0x10
#define BPF_MODE(code)	((code) & 0xe0)
#define		BPF_IMM 	0x00
#define		BPF_ABS		0x20
#define		BPF_IND		0x40
#define		BPF_MEM		0x60
#define		BPF_LEN		0x80
#define		BPF_MSH		0xa0

#define BPF_OP(code)	((code) & 0xf0)
#define		BPF_ADD		0x00
#define		BPF_SUB		0x10
#define		BPF_MUL		0x20
#define		BPF_DIV		0x30
#define		BPF_OR		0x40
#define		BPF_AND		0x50
#define		BPF_LSH		0x60
#define		BPF_RSH		0x70
#define		BPF_NEG		0x80
#define		BPF_JA		0x00
#define		BPF_JEQ		0x10
#define		BPF_JGT		0x20
#define		BPF_JGE		0x30
#define		BPF_JSET	0x40
#define BPF_SRC(code)	((code) & 0x08)
#define		BPF_K		0x00
#define		BPF_X		0x08

#define BPF_RVAL(code)	((code) & 0x18)
#define		BPF_A		0x10

#define BPF_MISCOP(code) ((code) & 0xf8)
#define		BPF_TAX		0x00
#define		BPF_TXA		0x80

struct bpf_insn {
	u_short	code;
	u_char 	jt;
	u_char 	jf;
	bpf_u_int32 k;
};

#define BPF_STMT(code, k) { (u_short)(code), 0, 0, k }
#define BPF_JUMP(code, k, jt, jf) { (u_short)(code), jt, jf, k }

#if __STDC__ || defined(__cplusplus)
extern int bpf_validate(const struct bpf_insn *, int);
extern u_int bpf_filter(const struct bpf_insn *, const u_char *, u_int, u_int);
#else
extern int bpf_validate();
extern u_int bpf_filter();
#endif

#define BPF_MEMWORDS 16

#ifdef __cplusplus
}
#endif

#endif 
