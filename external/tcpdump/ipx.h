
#define	IPX_SKT_NCP		0x0451
#define	IPX_SKT_SAP		0x0452
#define	IPX_SKT_RIP		0x0453
#define	IPX_SKT_NETBIOS		0x0455
#define	IPX_SKT_DIAGNOSTICS	0x0456
#define	IPX_SKT_NWLINK_DGM	0x0553	
#define	IPX_SKT_EIGRP		0x85be	

struct ipxHdr {
    u_int16_t	cksum;		
    u_int16_t	length;		
    u_int8_t	tCtl;		
    u_int8_t	pType;		
    u_int16_t	dstNet[2];	
    u_int8_t	dstNode[6];	
    u_int16_t	dstSkt;		
    u_int16_t	srcNet[2];	
    u_int8_t	srcNode[6];	
    u_int16_t	srcSkt;		
};

#define ipxSize	30

