

#define	JAN_1970	2208988800U	

struct l_fixedpt {
	u_int32_t int_part;
	u_int32_t fraction;
};

struct s_fixedpt {
	u_int16_t int_part;
	u_int16_t fraction;
};


struct ntpdata {
	u_char status;		
	u_char stratum;		
	u_char ppoll;		
	int precision:8;
	struct s_fixedpt root_delay;
	struct s_fixedpt root_dispersion;
	u_int32_t refid;
	struct l_fixedpt ref_timestamp;
	struct l_fixedpt org_timestamp;
	struct l_fixedpt rec_timestamp;
	struct l_fixedpt xmt_timestamp;
        u_int32_t key_id;
        u_int8_t  message_digest[16];
};
#define	NO_WARNING	0x00	
#define	PLUS_SEC	0x40	
#define	MINUS_SEC	0x80	
#define	ALARM		0xc0	

#define	NTPVERSION_1	0x08
#define	VERSIONMASK	0x38
#define LEAPMASK	0xc0
#define	MODEMASK	0x07

#define	MODE_UNSPEC	0	
#define	MODE_SYM_ACT	1	
#define	MODE_SYM_PAS	2	
#define	MODE_CLIENT	3	
#define	MODE_SERVER	4	
#define	MODE_BROADCAST	5	
#define	MODE_RES1	6	
#define	MODE_RES2	7	

#define	UNSPECIFIED	0
#define	PRIM_REF	1	
#define	INFO_QUERY	62	
#define	INFO_REPLY	63	
