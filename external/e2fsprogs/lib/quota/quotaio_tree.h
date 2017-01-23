
#ifndef _LINUX_QUOTA_TREE_H
#define _LINUX_QUOTA_TREE_H

#include <sys/types.h>

typedef u_int32_t qid_t;        

#define QT_TREEOFF	1	
#define QT_TREEDEPTH	4	
#define QT_BLKSIZE_BITS	10
#define QT_BLKSIZE (1 << QT_BLKSIZE_BITS)	

struct qt_disk_dqdbheader {
	u_int32_t dqdh_next_free;	
	u_int32_t dqdh_prev_free; 
	u_int16_t dqdh_entries; 
	u_int16_t dqdh_pad1;
	u_int32_t dqdh_pad2;
} __attribute__ ((packed));

struct dquot;
struct quota_handle;

struct qtree_fmt_operations {
	
	void (*mem2disk_dqblk)(void *disk, struct dquot *dquot);
	
	void (*disk2mem_dqblk)(struct dquot *dquot, void *disk);
	
	int (*is_id)(void *disk, struct dquot *dquot);
};

struct qtree_mem_dqinfo {
	unsigned int dqi_blocks;	
	unsigned int dqi_free_blk;	
	unsigned int dqi_free_entry;	
	unsigned int dqi_entry_size;	
	struct qtree_fmt_operations *dqi_ops;	
};

void qtree_write_dquot(struct dquot *dquot);
struct dquot *qtree_read_dquot(struct quota_handle *h, qid_t id);
void qtree_delete_dquot(struct dquot *dquot);
int qtree_entry_unused(struct qtree_mem_dqinfo *info, char *disk);
int qtree_scan_dquots(struct quota_handle *h,
		int (*process_dquot) (struct dquot *, void *), void *data);

int qtree_dqstr_in_blk(struct qtree_mem_dqinfo *info);

#endif 
