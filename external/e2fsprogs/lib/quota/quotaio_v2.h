
#ifndef GUARD_QUOTAIO_V2_H
#define GUARD_QUOTAIO_V2_H

#include <sys/types.h>
#include "quotaio.h"

#define V2_DQINFOOFF		sizeof(struct v2_disk_dqheader)
#define V2_VERSION 1

struct v2_disk_dqheader {
	u_int32_t dqh_magic;	
	u_int32_t dqh_version;	
} __attribute__ ((packed));

#define V2_DQF_MASK  0x0000	

struct v2_disk_dqinfo {
	u_int32_t dqi_bgrace;	
	u_int32_t dqi_igrace;	
	u_int32_t dqi_flags;	
	u_int32_t dqi_blocks;	
	u_int32_t dqi_free_blk;	
	u_int32_t dqi_free_entry;	
} __attribute__ ((packed));

struct v2r1_disk_dqblk {
	u_int32_t dqb_id;	
	u_int32_t dqb_pad;
	u_int64_t dqb_ihardlimit;	
	u_int64_t dqb_isoftlimit;	
	u_int64_t dqb_curinodes;	
	u_int64_t dqb_bhardlimit;	
	u_int64_t dqb_bsoftlimit;	
	u_int64_t dqb_curspace;	
	u_int64_t dqb_btime;	
	u_int64_t dqb_itime;	
} __attribute__ ((packed));

#endif
