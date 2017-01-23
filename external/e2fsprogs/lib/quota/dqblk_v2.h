
#ifndef __QUOTA_DQBLK_V2_H__
#define __QUOTA_DQBLK_V2_H__

#include "quotaio_tree.h"

struct v2_mem_dqinfo {
	struct qtree_mem_dqinfo dqi_qtree;
	unsigned int dqi_flags;		
	unsigned int dqi_used_entries;	
	unsigned int dqi_data_blocks;	
};

struct v2_mem_dqblk {
	long long dqb_off;	
};

struct quotafile_ops;		

extern struct quotafile_ops quotafile_ops_2;

#endif  
