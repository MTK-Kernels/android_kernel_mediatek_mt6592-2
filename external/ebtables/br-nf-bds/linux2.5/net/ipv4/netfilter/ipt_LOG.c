#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/ip.h>
#include <linux/spinlock.h>
#include <net/icmp.h>
#include <net/udp.h>
#include <net/tcp.h>
#include <linux/netfilter_ipv4/ip_tables.h>

struct in_device;
#include <net/route.h>
#include <linux/netfilter_ipv4/ipt_LOG.h>

#if 0
#define DEBUGP printk
#else
#define DEBUGP(format, args...)
#endif

struct esphdr {
	__u32   spi;
}; 
        
static spinlock_t log_lock = SPIN_LOCK_UNLOCKED;

static void dump_packet(const struct ipt_log_info *info,
			struct iphdr *iph, unsigned int len, int recurse)
{
	void *protoh = (u_int32_t *)iph + iph->ihl;
	unsigned int datalen = len - iph->ihl * 4;

	
	printk("SRC=%u.%u.%u.%u DST=%u.%u.%u.%u ",
	       NIPQUAD(iph->saddr), NIPQUAD(iph->daddr));

	
	printk("LEN=%u TOS=0x%02X PREC=0x%02X TTL=%u ID=%u ",
	       ntohs(iph->tot_len), iph->tos & IPTOS_TOS_MASK,
	       iph->tos & IPTOS_PREC_MASK, iph->ttl, ntohs(iph->id));

	
	if (ntohs(iph->frag_off) & IP_CE)
		printk("CE ");
	if (ntohs(iph->frag_off) & IP_DF)
		printk("DF ");
	if (ntohs(iph->frag_off) & IP_MF)
		printk("MF ");

	
	if (ntohs(iph->frag_off) & IP_OFFSET)
		printk("FRAG:%u ", ntohs(iph->frag_off) & IP_OFFSET);

	if ((info->logflags & IPT_LOG_IPOPT)
	    && iph->ihl * 4 != sizeof(struct iphdr)) {
		unsigned int i;

		
		printk("OPT (");
		for (i = sizeof(struct iphdr); i < iph->ihl * 4; i++)
			printk("%02X", ((u_int8_t *)iph)[i]);
		printk(") ");
	}

	switch (iph->protocol) {
	case IPPROTO_TCP: {
		struct tcphdr *tcph = protoh;

		
		printk("PROTO=TCP ");

		if (ntohs(iph->frag_off) & IP_OFFSET)
			break;

		
		if (datalen < sizeof (*tcph)) {
			printk("INCOMPLETE [%u bytes] ", datalen);
			break;
		}

		
		printk("SPT=%u DPT=%u ",
		       ntohs(tcph->source), ntohs(tcph->dest));
		
		if (info->logflags & IPT_LOG_TCPSEQ)
			printk("SEQ=%u ACK=%u ",
			       ntohl(tcph->seq), ntohl(tcph->ack_seq));
		
		printk("WINDOW=%u ", ntohs(tcph->window));
		
		printk("RES=0x%02x ", (u_int8_t)(ntohl(tcp_flag_word(tcph) & TCP_RESERVED_BITS) >> 22));
		
		if (tcph->cwr)
			printk("CWR ");
		if (tcph->ece)
			printk("ECE ");
		if (tcph->urg)
			printk("URG ");
		if (tcph->ack)
			printk("ACK ");
		if (tcph->psh)
			printk("PSH ");
		if (tcph->rst)
			printk("RST ");
		if (tcph->syn)
			printk("SYN ");
		if (tcph->fin)
			printk("FIN ");
		
		printk("URGP=%u ", ntohs(tcph->urg_ptr));

		if ((info->logflags & IPT_LOG_TCPOPT)
		    && tcph->doff * 4 != sizeof(struct tcphdr)) {
			unsigned int i;

			
			printk("OPT (");
			for (i =sizeof(struct tcphdr); i < tcph->doff * 4; i++)
				printk("%02X", ((u_int8_t *)tcph)[i]);
			printk(") ");
		}
		break;
	}
	case IPPROTO_UDP: {
		struct udphdr *udph = protoh;

		
		printk("PROTO=UDP ");

		if (ntohs(iph->frag_off) & IP_OFFSET)
			break;

		
		if (datalen < sizeof (*udph)) {
			printk("INCOMPLETE [%u bytes] ", datalen);
			break;
		}

		
		printk("SPT=%u DPT=%u LEN=%u ",
		       ntohs(udph->source), ntohs(udph->dest),
		       ntohs(udph->len));
		break;
	}
	case IPPROTO_ICMP: {
		struct icmphdr *icmph = protoh;
		static size_t required_len[NR_ICMP_TYPES+1]
			= { [ICMP_ECHOREPLY] = 4,
			    [ICMP_DEST_UNREACH]
			    = 8 + sizeof(struct iphdr) + 8,
			    [ICMP_SOURCE_QUENCH]
			    = 8 + sizeof(struct iphdr) + 8,
			    [ICMP_REDIRECT]
			    = 8 + sizeof(struct iphdr) + 8,
			    [ICMP_ECHO] = 4,
			    [ICMP_TIME_EXCEEDED]
			    = 8 + sizeof(struct iphdr) + 8,
			    [ICMP_PARAMETERPROB]
			    = 8 + sizeof(struct iphdr) + 8,
			    [ICMP_TIMESTAMP] = 20,
			    [ICMP_TIMESTAMPREPLY] = 20,
			    [ICMP_ADDRESS] = 12,
			    [ICMP_ADDRESSREPLY] = 12 };

		
		printk("PROTO=ICMP ");

		if (ntohs(iph->frag_off) & IP_OFFSET)
			break;

		
		if (datalen < 4) {
			printk("INCOMPLETE [%u bytes] ", datalen);
			break;
		}

		
		printk("TYPE=%u CODE=%u ", icmph->type, icmph->code);

		
		if (icmph->type <= NR_ICMP_TYPES
		    && required_len[icmph->type]
		    && datalen < required_len[icmph->type]) {
			printk("INCOMPLETE [%u bytes] ", datalen);
			break;
		}

		switch (icmph->type) {
		case ICMP_ECHOREPLY:
		case ICMP_ECHO:
			
			printk("ID=%u SEQ=%u ",
			       ntohs(icmph->un.echo.id),
			       ntohs(icmph->un.echo.sequence));
			break;

		case ICMP_PARAMETERPROB:
			
			printk("PARAMETER=%u ",
			       ntohl(icmph->un.gateway) >> 24);
			break;
		case ICMP_REDIRECT:
			
			printk("GATEWAY=%u.%u.%u.%u ", NIPQUAD(icmph->un.gateway));
			
		case ICMP_DEST_UNREACH:
		case ICMP_SOURCE_QUENCH:
		case ICMP_TIME_EXCEEDED:
			
			if (recurse) {
				printk("[");
				dump_packet(info,
					    (struct iphdr *)(icmph + 1),
					    datalen-sizeof(struct icmphdr),
					    0);
				printk("] ");
			}

			
			if (icmph->type == ICMP_DEST_UNREACH
			    && icmph->code == ICMP_FRAG_NEEDED)
				printk("MTU=%u ", ntohs(icmph->un.frag.mtu));
		}
		break;
	}
	
	case IPPROTO_AH:
	case IPPROTO_ESP: {
		struct esphdr *esph = protoh;
		int esp= (iph->protocol==IPPROTO_ESP);

		
		printk("PROTO=%s ",esp? "ESP" : "AH");

		if (ntohs(iph->frag_off) & IP_OFFSET)
			break;

		
		if (datalen < sizeof (*esph)) {
			printk("INCOMPLETE [%u bytes] ", datalen);
			break;
		}

		
		printk("SPI=0x%x ", ntohl(esph->spi) );
		break;
	}
	
	default:
		printk("PROTO=%u ", iph->protocol);
	}

	
	
	
	
	
	
	
	

	
	
	
}

static unsigned int
ipt_log_target(struct sk_buff **pskb,
	       unsigned int hooknum,
	       const struct net_device *in,
	       const struct net_device *out,
	       const void *targinfo,
	       void *userinfo)
{
	struct iphdr *iph = (*pskb)->nh.iph;
	const struct ipt_log_info *loginfo = targinfo;
	char level_string[4] = "< >";

	level_string[1] = '0' + (loginfo->level % 8);
	spin_lock_bh(&log_lock);
	printk(level_string);
	printk("%sIN=%s ", loginfo->prefix, in ? in->name : "");
	if ((*pskb)->nf_bridge) {
		struct net_device *physindev = (*pskb)->nf_bridge->physindev;
		struct net_device *physoutdev = (*pskb)->nf_bridge->physoutdev;

		if (physindev && in != physindev)
			printk("PHYSIN=%s ", physindev->name);
		printk("OUT=%s ", out ? out->name : "");
		if (physoutdev && out != physoutdev)
			printk("PHYSOUT=%s ", physoutdev->name);
	}

	if (in && !out) {
		
		printk("MAC=");
		if ((*pskb)->dev && (*pskb)->dev->hard_header_len && (*pskb)->mac.raw != (void*)iph) {
			int i;
			unsigned char *p = (*pskb)->mac.raw;
			for (i = 0; i < (*pskb)->dev->hard_header_len; i++,p++)
				printk("%02x%c", *p,
				       i==(*pskb)->dev->hard_header_len - 1
				       ? ' ':':');
		} else
			printk(" ");
	}

	dump_packet(loginfo, iph, (*pskb)->len, 1);
	printk("\n");
	spin_unlock_bh(&log_lock);

	return IPT_CONTINUE;
}

static int ipt_log_checkentry(const char *tablename,
			      const struct ipt_entry *e,
			      void *targinfo,
			      unsigned int targinfosize,
			      unsigned int hook_mask)
{
	const struct ipt_log_info *loginfo = targinfo;

	if (targinfosize != IPT_ALIGN(sizeof(struct ipt_log_info))) {
		DEBUGP("LOG: targinfosize %u != %u\n",
		       targinfosize, IPT_ALIGN(sizeof(struct ipt_log_info)));
		return 0;
	}

	if (loginfo->level >= 8) {
		DEBUGP("LOG: level %u >= 8\n", loginfo->level);
		return 0;
	}

	if (loginfo->prefix[sizeof(loginfo->prefix)-1] != '\0') {
		DEBUGP("LOG: prefix term %i\n",
		       loginfo->prefix[sizeof(loginfo->prefix)-1]);
		return 0;
	}

	return 1;
}

static struct ipt_target ipt_log_reg
= { { NULL, NULL }, "LOG", ipt_log_target, ipt_log_checkentry, NULL, 
    THIS_MODULE };

static int __init init(void)
{
	if (ipt_register_target(&ipt_log_reg))
		return -EINVAL;

	return 0;
}

static void __exit fini(void)
{
	ipt_unregister_target(&ipt_log_reg);
}

module_init(init);
module_exit(fini);
MODULE_LICENSE("GPL");
