/*
 * e2fsck.h
 *
 * Copyright (C) 1993, 1994 Theodore Ts'o.  This file may be
 * redistributed under the terms of the GNU Public License.
 *
 */

#ifndef _E2FSCK_H
#define _E2FSCK_H

#include <stdio.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdlib.h>
#include <time.h>
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_SETJMP_H
#include <setjmp.h>
#endif

#if EXT2_FLAT_INCLUDES
#include "ext2_fs.h"
#include "ext2fs.h"
#include "blkid.h"
#else
#include "ext2fs/ext2_fs.h"
#include "ext2fs/ext2fs.h"
#include "blkid/blkid.h"
#endif

#include "profile.h"
#include "prof_err.h"

#ifdef ENABLE_NLS
#include <libintl.h>
#include <locale.h>
#define _(a) (gettext (a))
#ifdef gettext_noop
#define N_(a) gettext_noop (a)
#else
#define N_(a) (a)
#endif
#define P_(singular, plural, n) (ngettext (singular, plural, n))
#ifndef NLS_CAT_NAME
#define NLS_CAT_NAME "e2fsprogs"
#endif
#ifndef LOCALEDIR
#define LOCALEDIR "/usr/share/locale"
#endif
#else
#define _(a) (a)
#define N_(a) a
#define P_(singular, plural, n) ((n) == 1 ? (singular) : (plural))
#endif

#ifdef __GNUC__
#define E2FSCK_ATTR(x) __attribute__(x)
#else
#define E2FSCK_ATTR(x)
#endif

#include "quota/mkquota.h"

#define FSCK_OK          0	
#define FSCK_NONDESTRUCT 1	
#define FSCK_REBOOT      2	
#define FSCK_UNCORRECTED 4	
#define FSCK_ERROR       8	
#define FSCK_USAGE       16	
#define FSCK_CANCELED	 32	
#define FSCK_LIBRARY     128	

#define E2FSCK_CURRENT_REV	1

struct dir_info {
	ext2_ino_t		ino;	
	ext2_ino_t		dotdot;	
	ext2_ino_t		parent; 
};


struct dx_dir_info {
	ext2_ino_t		ino; 		
	int			numblocks;	
	int			hashversion;
	short			depth;		
	struct dx_dirblock_info	*dx_block; 	
};

#define DX_DIRBLOCK_ROOT	1
#define DX_DIRBLOCK_LEAF	2
#define DX_DIRBLOCK_NODE	3
#define DX_DIRBLOCK_CORRUPT	4
#define DX_DIRBLOCK_CLEARED	8

struct dx_dirblock_info {
	int		type;
	blk64_t		phys;
	int		flags;
	blk64_t		parent;
	ext2_dirhash_t	min_hash;
	ext2_dirhash_t	max_hash;
	ext2_dirhash_t	node_min_hash;
	ext2_dirhash_t	node_max_hash;
};

#define DX_FLAG_REFERENCED	1
#define DX_FLAG_DUP_REF		2
#define DX_FLAG_FIRST		4
#define DX_FLAG_LAST		8

#define RESOURCE_TRACK

#ifdef RESOURCE_TRACK
struct resource_track {
	struct timeval time_start;
	struct timeval user_start;
	struct timeval system_start;
	void	*brk_start;
	unsigned long long bytes_read;
	unsigned long long bytes_written;
};
#endif

#define E2F_OPT_READONLY	0x0001
#define E2F_OPT_PREEN		0x0002
#define E2F_OPT_YES		0x0004
#define E2F_OPT_NO		0x0008
#define E2F_OPT_TIME		0x0010
#define E2F_OPT_TIME2		0x0020
#define E2F_OPT_CHECKBLOCKS	0x0040
#define E2F_OPT_DEBUG		0x0080
#define E2F_OPT_FORCE		0x0100
#define E2F_OPT_WRITECHECK	0x0200
#define E2F_OPT_COMPRESS_DIRS	0x0400
#define E2F_OPT_FRAGCHECK	0x0800
#define E2F_OPT_JOURNAL_ONLY	0x1000 
#define E2F_OPT_DISCARD		0x2000

#define E2F_FLAG_ABORT		0x0001 
#define E2F_FLAG_CANCEL		0x0002 
#define E2F_FLAG_SIGNAL_MASK	0x0003
#define E2F_FLAG_RESTART	0x0004 
#define E2F_FLAG_RESTART_LATER	0x0008 

#define E2F_FLAG_SETJMP_OK	0x0010 

#define E2F_FLAG_PROG_BAR	0x0020 
#define E2F_FLAG_PROG_SUPPRESS	0x0040 
#define E2F_FLAG_JOURNAL_INODE	0x0080 
#define E2F_FLAG_SB_SPECIFIED	0x0100 
#define E2F_FLAG_RESTARTED	0x0200 
#define E2F_FLAG_RESIZE_INODE	0x0400 
#define E2F_FLAG_GOT_DEVSIZE	0x0800 
#define E2F_FLAG_EXITING	0x1000 
#define E2F_FLAG_TIME_INSANE	0x2000 

#define E2F_RESET_FLAGS (E2F_FLAG_TIME_INSANE)

#define E2F_PASS_1	1
#define E2F_PASS_2	2
#define E2F_PASS_3	3
#define E2F_PASS_4	4
#define E2F_PASS_5	5
#define E2F_PASS_1B	6

typedef struct ea_refcount *ext2_refcount_t;

typedef struct e2fsck_struct *e2fsck_t;

#define MAX_EXTENT_DEPTH_COUNT 5

struct e2fsck_struct {
	ext2_filsys fs;
	const char *program_name;
	char *filesystem_name;
	char *device_name;
	char *io_options;
	FILE	*logf;
	char	*log_fn;
	int	flags;		
	int	options;
	int	blocksize;	
	blk64_t	use_superblock;	
	blk64_t	superblock;	
	blk64_t	num_blocks;	
	blk64_t free_blocks;
	ino_t	free_inodes;
	int	mount_flags;
	blkid_cache blkid;	

#ifdef HAVE_SETJMP_H
	jmp_buf	abort_loc;
#endif
	unsigned long abort_code;

	int (*progress)(e2fsck_t ctx, int pass, unsigned long cur,
			unsigned long max);

	ext2fs_inode_bitmap inode_used_map; 
	ext2fs_inode_bitmap inode_bad_map; 
	ext2fs_inode_bitmap inode_dir_map; 
	ext2fs_inode_bitmap inode_bb_map; 
	ext2fs_inode_bitmap inode_imagic_map; 
	ext2fs_inode_bitmap inode_reg_map; 

	ext2fs_block_bitmap block_found_map; 
	ext2fs_block_bitmap block_dup_map; 
	ext2fs_block_bitmap block_ea_map; 

	ext2_icount_t	inode_count;
	ext2_icount_t inode_link_info;

	ext2_refcount_t	refcount;
	ext2_refcount_t refcount_extra;

	int *invalid_inode_bitmap_flag;
	int *invalid_block_bitmap_flag;
	int *invalid_inode_table_flag;
	int invalid_bitmaps;	

	char *block_buf;

	ext2_ino_t stashed_ino;
	struct ext2_inode *stashed_inode;

	ext2_ino_t lost_and_found;
	int bad_lost_and_found;

	struct dir_info_db	*dir_info;

	int		dx_dir_info_count;
	int		dx_dir_info_size;
	struct dx_dir_info *dx_dir_info;

	ext2_u32_list	dirs_to_hash;

	int process_inode_size;
	int inode_buffer_blocks;
	unsigned int htree_slack_percentage;

	io_channel	journal_io;
	char	*journal_name;

	quota_ctx_t qctx;
#ifdef RESOURCE_TRACK
	struct resource_track	global_rtrack;
#endif

	int progress_fd;
	int progress_pos;
	int progress_last_percent;
	unsigned int progress_last_time;
	int interactive;	
	char start_meta[2], stop_meta[2];

	
	__u32 fs_directory_count;
	__u32 fs_regular_count;
	__u32 fs_blockdev_count;
	__u32 fs_chardev_count;
	__u32 fs_links_count;
	__u32 fs_symlinks_count;
	__u32 fs_fast_symlinks_count;
	__u32 fs_fifo_count;
	__u32 fs_total_count;
	__u32 fs_badblocks_count;
	__u32 fs_sockets_count;
	__u32 fs_ind_count;
	__u32 fs_dind_count;
	__u32 fs_tind_count;
	__u32 fs_fragmented;
	__u32 fs_fragmented_dir;
	__u32 large_files;
	__u32 fs_ext_attr_inodes;
	__u32 fs_ext_attr_blocks;
	__u32 extent_depth_count[MAX_EXTENT_DEPTH_COUNT];

	
	time_t now;
	time_t time_fudge;	
	int ext_attr_ver;
	profile_t	profile;
	int blocks_per_page;

	void *priv_data;
};

typedef __u32 region_addr_t;
typedef struct region_struct *region_t;

#ifndef HAVE_STRNLEN
#define strnlen(str, x) e2fsck_strnlen((str),(x))
extern int e2fsck_strnlen(const char * s, int count);
#endif


extern void e2fsck_pass1(e2fsck_t ctx);
extern void e2fsck_pass1_dupblocks(e2fsck_t ctx, char *block_buf);
extern void e2fsck_pass2(e2fsck_t ctx);
extern void e2fsck_pass3(e2fsck_t ctx);
extern void e2fsck_pass4(e2fsck_t ctx);
extern void e2fsck_pass5(e2fsck_t ctx);

extern errcode_t e2fsck_allocate_context(e2fsck_t *ret);
extern errcode_t e2fsck_reset_context(e2fsck_t ctx);
extern void e2fsck_free_context(e2fsck_t ctx);
extern int e2fsck_run(e2fsck_t ctx);


extern void read_bad_blocks_file(e2fsck_t ctx, const char *bad_blocks_file,
				 int replace_bad_blocks);

extern __u32 crc32_be(__u32 crc, unsigned char const *p, size_t len);

extern void e2fsck_add_dir_info(e2fsck_t ctx, ext2_ino_t ino, ext2_ino_t parent);
extern void e2fsck_free_dir_info(e2fsck_t ctx);
extern int e2fsck_get_num_dirinfo(e2fsck_t ctx);
extern struct dir_info_iter *e2fsck_dir_info_iter_begin(e2fsck_t ctx);
extern struct dir_info *e2fsck_dir_info_iter(e2fsck_t ctx,
					     struct dir_info_iter *);
extern void e2fsck_dir_info_iter_end(e2fsck_t ctx, struct dir_info_iter *);
extern int e2fsck_dir_info_set_parent(e2fsck_t ctx, ext2_ino_t ino,
				      ext2_ino_t parent);
extern int e2fsck_dir_info_set_dotdot(e2fsck_t ctx, ext2_ino_t ino,
				      ext2_ino_t dotdot);
extern int e2fsck_dir_info_get_parent(e2fsck_t ctx, ext2_ino_t ino,
				      ext2_ino_t *parent);
extern int e2fsck_dir_info_get_dotdot(e2fsck_t ctx, ext2_ino_t ino,
				      ext2_ino_t *dotdot);

extern void e2fsck_add_dx_dir(e2fsck_t ctx, ext2_ino_t ino, int num_blocks);
extern struct dx_dir_info *e2fsck_get_dx_dir_info(e2fsck_t ctx, ext2_ino_t ino);
extern void e2fsck_free_dx_dir_info(e2fsck_t ctx);
extern int e2fsck_get_num_dx_dirinfo(e2fsck_t ctx);
extern struct dx_dir_info *e2fsck_dx_dir_info_iter(e2fsck_t ctx, int *control);

extern errcode_t ea_refcount_create(int size, ext2_refcount_t *ret);
extern void ea_refcount_free(ext2_refcount_t refcount);
extern errcode_t ea_refcount_fetch(ext2_refcount_t refcount, blk64_t blk, int *ret);
extern errcode_t ea_refcount_increment(ext2_refcount_t refcount,
				       blk64_t blk, int *ret);
extern errcode_t ea_refcount_decrement(ext2_refcount_t refcount,
				       blk64_t blk, int *ret);
extern errcode_t ea_refcount_store(ext2_refcount_t refcount,
				   blk64_t blk, int count);
extern blk_t ext2fs_get_refcount_size(ext2_refcount_t refcount);
extern void ea_refcount_intr_begin(ext2_refcount_t refcount);
extern blk64_t ea_refcount_intr_next(ext2_refcount_t refcount, int *ret);

extern const char *ehandler_operation(const char *op);
extern void ehandler_init(io_channel channel);

extern errcode_t e2fsck_check_ext3_journal(e2fsck_t ctx);
extern errcode_t e2fsck_run_ext3_journal(e2fsck_t ctx);
extern void e2fsck_move_ext3_journal(e2fsck_t ctx);
extern int e2fsck_fix_ext3_journal_hint(e2fsck_t ctx);

extern void set_up_logging(e2fsck_t ctx);

extern void e2fsck_hide_quota(e2fsck_t ctx);

extern void e2fsck_setup_tdb_icount(e2fsck_t ctx, int flags,
				    ext2_icount_t *ret);
extern void e2fsck_use_inode_shortcuts(e2fsck_t ctx, int use_shortcuts);
extern int e2fsck_pass1_check_device_inode(ext2_filsys fs,
					   struct ext2_inode *inode);
extern int e2fsck_pass1_check_symlink(ext2_filsys fs, ext2_ino_t ino,
				      struct ext2_inode *inode, char *buf);
extern void e2fsck_clear_inode(e2fsck_t ctx, ext2_ino_t ino,
			       struct ext2_inode *inode, int restart_flag,
			       const char *source);

extern int e2fsck_process_bad_inode(e2fsck_t ctx, ext2_ino_t dir,
				    ext2_ino_t ino, char *buf);

extern int e2fsck_reconnect_file(e2fsck_t ctx, ext2_ino_t inode);
extern errcode_t e2fsck_expand_directory(e2fsck_t ctx, ext2_ino_t dir,
					 int num, int gauranteed_size);
extern ext2_ino_t e2fsck_get_lost_and_found(e2fsck_t ctx, int fix);
extern errcode_t e2fsck_adjust_inode_count(e2fsck_t ctx, ext2_ino_t ino,
					   int adj);


extern region_t region_create(region_addr_t min, region_addr_t max);
extern void region_free(region_t region);
extern int region_allocate(region_t region, region_addr_t start, int n);

errcode_t e2fsck_rehash_dir(e2fsck_t ctx, ext2_ino_t ino);
void e2fsck_rehash_directories(e2fsck_t ctx);

void sigcatcher_setup(void);

void check_super_block(e2fsck_t ctx);
int check_backup_super_block(e2fsck_t ctx);
void check_resize_inode(e2fsck_t ctx);

extern void *e2fsck_allocate_memory(e2fsck_t ctx, unsigned int size,
				    const char *description);
extern int ask(e2fsck_t ctx, const char * string, int def);
extern int ask_yn(e2fsck_t ctx, const char * string, int def);
extern void fatal_error(e2fsck_t ctx, const char * fmt_string);
extern void log_out(e2fsck_t ctx, const char *fmt, ...)
	E2FSCK_ATTR((format(printf, 2, 3)));
extern void log_err(e2fsck_t ctx, const char *fmt, ...)
	E2FSCK_ATTR((format(printf, 2, 3)));
extern void e2fsck_read_bitmaps(e2fsck_t ctx);
extern void e2fsck_write_bitmaps(e2fsck_t ctx);
extern void preenhalt(e2fsck_t ctx);
extern char *string_copy(e2fsck_t ctx, const char *str, int len);
extern errcode_t e2fsck_zero_blocks(ext2_filsys fs, blk_t blk, int num,
				    blk_t *ret_blk, int *ret_count);
extern int fs_proc_check(const char *fs_name);
extern int check_for_modules(const char *fs_name);
#ifdef RESOURCE_TRACK
extern void print_resource_track(e2fsck_t ctx,
				 const char *desc,
				 struct resource_track *track,
				 io_channel channel);
extern void init_resource_track(struct resource_track *track,
				io_channel channel);
#else
#define print_resource_track(ctx, desc, track, channel) do { } while (0)
#define init_resource_track(track, channel) do { } while (0)
#endif
extern int inode_has_valid_blocks(struct ext2_inode *inode);
extern void e2fsck_read_inode(e2fsck_t ctx, unsigned long ino,
			      struct ext2_inode * inode, const char * proc);
extern void e2fsck_read_inode_full(e2fsck_t ctx, unsigned long ino,
				   struct ext2_inode *inode,
				   const int bufsize, const char *proc);
extern void e2fsck_write_inode(e2fsck_t ctx, unsigned long ino,
			       struct ext2_inode * inode, const char * proc);
extern void e2fsck_write_inode_full(e2fsck_t ctx, unsigned long ino,
                               struct ext2_inode * inode, int bufsize,
                               const char *proc);
#ifdef MTRACE
extern void mtrace_print(char *mesg);
#endif
extern blk64_t get_backup_sb(e2fsck_t ctx, ext2_filsys fs,
			   const char *name, io_manager manager);
extern int ext2_file_type(unsigned int mode);
extern int write_all(int fd, char *buf, size_t count);
void dump_mmp_msg(struct mmp_struct *mmp, const char *msg);
errcode_t e2fsck_mmp_update(ext2_filsys fs);

extern void e2fsck_set_bitmap_type(ext2_filsys fs,
				   unsigned int default_type,
				   const char *profile_name,
				   unsigned int *old_type);
extern errcode_t e2fsck_allocate_inode_bitmap(ext2_filsys fs,
					      const char *descr,
					      int default_type,
					      const char *profile_name,
					      ext2fs_inode_bitmap *ret);
extern errcode_t e2fsck_allocate_block_bitmap(ext2_filsys fs,
					      const char *descr,
					      int default_type,
					      const char *profile_name,
					      ext2fs_block_bitmap *ret);
extern errcode_t e2fsck_allocate_subcluster_bitmap(ext2_filsys fs,
						   const char *descr,
						   int default_type,
						   const char *profile_name,
						   ext2fs_block_bitmap *ret);

extern void e2fsck_clear_progbar(e2fsck_t ctx);
extern int e2fsck_simple_progress(e2fsck_t ctx, const char *label,
				  float percent, unsigned int dpynum);
#endif 
