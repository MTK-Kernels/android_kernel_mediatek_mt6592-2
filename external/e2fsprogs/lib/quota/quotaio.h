
#ifndef GUARD_QUOTAIO_H
#define GUARD_QUOTAIO_H

#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "ext2fs/ext2fs.h"
#include "dqblk_v2.h"

typedef int64_t qsize_t;	

#define MAXQUOTAS 2
#define USRQUOTA 0
#define GRPQUOTA 1

#define INITQMAGICS {\
	0xd9c01f11,	\
	0xd9c01927	\
}

#define QUOTABLOCK_BITS 10
#define QUOTABLOCK_SIZE (1 << QUOTABLOCK_BITS)
#define toqb(x) (((x) + QUOTABLOCK_SIZE - 1) >> QUOTABLOCK_BITS)

#define	QFMT_VFS_OLD 1
#define	QFMT_VFS_V0 2
#define	QFMT_VFS_V1 4

#define MAX_IQ_TIME  604800	
#define MAX_DQ_TIME  604800	

#define IOFL_INFODIRTY	0x01	

struct quotafile_ops;

struct util_dqinfo {
	time_t dqi_bgrace;	
	time_t dqi_igrace;	
	union {
		struct v2_mem_dqinfo v2_mdqi;
	} u;			
};

struct quota_file {
	ext2_filsys fs;
	ext2_ino_t ino;
	ext2_file_t e2_file;
};

struct quota_handle {
	int qh_type;		
	int qh_fmt;		
	int qh_io_flags;	
	struct quota_file qh_qf;
	unsigned int (*e2fs_read)(struct quota_file *qf, ext2_loff_t offset,
			 void *buf, unsigned int size);
	unsigned int (*e2fs_write)(struct quota_file *qf, ext2_loff_t offset,
			  void *buf, unsigned int size);
	struct quotafile_ops *qh_ops;	
	struct util_dqinfo qh_info;	
};

struct util_dqblk {
	qsize_t dqb_ihardlimit;
	qsize_t dqb_isoftlimit;
	qsize_t dqb_curinodes;
	qsize_t dqb_bhardlimit;
	qsize_t dqb_bsoftlimit;
	qsize_t dqb_curspace;
	time_t dqb_btime;
	time_t dqb_itime;
	union {
		struct v2_mem_dqblk v2_mdqb;
	} u;			
};

struct dquot {
	struct dquot *dq_next;	
	qid_t dq_id;		
	int dq_flags;		
	struct quota_handle *dq_h;	
	struct util_dqblk dq_dqb;	
};

struct quotafile_ops {
	
	int (*check_file) (struct quota_handle *h, int type, int fmt);
	
	int (*init_io) (struct quota_handle *h);
	
	int (*new_io) (struct quota_handle *h);
	
	int (*end_io) (struct quota_handle *h);
	
	int (*write_info) (struct quota_handle *h);
	
	struct dquot *(*read_dquot) (struct quota_handle *h, qid_t id);
	
	int (*commit_dquot) (struct dquot *dquot);
	
	int (*scan_dquots) (struct quota_handle *h,
			    int (*process_dquot) (struct dquot *dquot,
						  void *data),
			    void *data);
	
	int (*report) (struct quota_handle *h, int verbose);
};

extern struct quotafile_ops quotafile_ops_meta;

errcode_t quota_file_open(struct quota_handle *h, ext2_filsys fs,
			  ext2_ino_t qf_ino, int type, int fmt, int flags);


errcode_t quota_file_create(struct quota_handle *h, ext2_filsys fs,
			    int type, int fmt);

errcode_t quota_file_close(struct quota_handle *h);

struct dquot *get_empty_dquot(void);

errcode_t quota_inode_truncate(ext2_filsys fs, ext2_ino_t ino);

const char *type2name(int type);

void update_grace_times(struct dquot *q);

#define QUOTA_NAME_LEN 16

const char *quota_get_qf_name(int type, int fmt, char *buf);
const char *quota_get_qf_path(const char *mntpt, int qtype, int fmt,
			      char *path_buf, size_t path_buf_size);

#endif 
