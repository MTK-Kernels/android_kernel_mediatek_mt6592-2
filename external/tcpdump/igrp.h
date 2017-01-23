

struct igrphdr {
	u_int8_t ig_vop;	
#define IGRP_V(x)	(((x) & 0xf0) >> 4)
#define IGRP_OP(x)	((x) & 0x0f)
	u_int8_t ig_ed;		
	u_int16_t ig_as;	
	u_int16_t ig_ni;	
	u_int16_t ig_ns;	
	u_int16_t ig_nx;	
	u_int16_t ig_sum;	
};

#define IGRP_UPDATE	1
#define IGRP_REQUEST	2


struct igrprte {
	u_int8_t igr_net[3];	
	u_int8_t igr_dly[3];	
	u_int8_t igr_bw[3];	
	u_int8_t igr_mtu[2];	
	u_int8_t igr_rel;	
	u_int8_t igr_ld;	
	u_int8_t igr_hct;	
};

#define IGRP_RTE_SIZE	14	
