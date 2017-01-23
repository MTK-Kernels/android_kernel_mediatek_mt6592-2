
#ifndef __QUOTA_QUOTAIO_H__
#define __QUOTA_QUOTAIO_H__

#include "ext2fs/ext2_fs.h"
#include "ext2fs/ext2fs.h"
#include "quotaio.h"
#include "../e2fsck/dict.h"

typedef struct quota_ctx *quota_ctx_t;

struct quota_ctx {
	ext2_filsys	fs;
	dict_t		*quota_dict[MAXQUOTAS];
};

errcode_t quota_init_context(quota_ctx_t *qctx, ext2_filsys fs, int qtype);
void quota_data_inodes(quota_ctx_t qctx, struct ext2_inode *inode, ext2_ino_t ino,
		int adjust);
void quota_data_add(quota_ctx_t qctx, struct ext2_inode *inode, ext2_ino_t ino,
		qsize_t space);
void quota_data_sub(quota_ctx_t qctx, struct ext2_inode *inode, ext2_ino_t ino,
		qsize_t space);
errcode_t quota_write_inode(quota_ctx_t qctx, int qtype);
errcode_t quota_update_limits(quota_ctx_t qctx, ext2_ino_t qf_ino, int type);
errcode_t quota_compute_usage(quota_ctx_t qctx);
void quota_release_context(quota_ctx_t *qctx);

errcode_t quota_remove_inode(ext2_filsys fs, int qtype);
int quota_file_exists(ext2_filsys fs, int qtype, int fmt);
void quota_set_sb_inum(ext2_filsys fs, ext2_ino_t ino, int qtype);
errcode_t quota_compare_and_update(quota_ctx_t qctx, int qtype,
				   int *usage_inconsistent);

#endif  
