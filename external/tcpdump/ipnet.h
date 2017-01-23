typedef struct ipnet_hdr {
	uint8_t		iph_version;
	uint8_t		iph_family;
	uint16_t	iph_htype;
	uint32_t	iph_pktlen;
	uint32_t	iph_ifindex;
	uint32_t	iph_grifindex;
	uint32_t	iph_zsrc;
	uint32_t	iph_zdst;
} ipnet_hdr_t;

#define	IPH_AF_INET	2		
#define	IPH_AF_INET6	26		
