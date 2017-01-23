/*
 * linux/include/linux/jbd.h
 *
 * Written by Stephen C. Tweedie <sct@redhat.com>
 *
 * Copyright 1998-2000 Red Hat, Inc --- All Rights Reserved
 *
 * This file is part of the Linux kernel and is made available under
 * the terms of the GNU General Public License, version 2, or at your
 * option, any later version, incorporated herein by reference.
 *
 * Definitions for transaction data structures for the buffer cache
 * filesystem journaling support.
 */

#ifndef _LINUX_JBD_H
#define _LINUX_JBD_H

#if defined(CONFIG_JBD) || defined(CONFIG_JBD_MODULE) || !defined(__KERNEL__)

#ifndef __KERNEL__
#include "jfs_compat.h"
#define JFS_DEBUG
#define jfs_debug jbd_debug
#else

#include <linux/journal-head.h>
#include <linux/stddef.h>
#include <asm/semaphore.h>
#endif

#ifndef __GNUC__
#define __FUNCTION__ ""
#endif

#define journal_oom_retry 1

#ifdef __STDC__
#ifdef CONFIG_JBD_DEBUG
#define JBD_EXPENSIVE_CHECKING
extern int journal_enable_debug;

#define jbd_debug(n, f, a...)						\
	do {								\
		if ((n) <= journal_enable_debug) {			\
			printk (KERN_DEBUG "(%s, %d): %s: ",		\
				__FILE__, __LINE__, __FUNCTION__);	\
		  	printk (f, ## a);				\
		}							\
	} while (0)
#else
#ifdef __GNUC__
#define jbd_debug(f, a...)	
#else
#define jbd_debug(f, ...)	
#endif
#endif
#else
#define jbd_debug(x)		
#endif

extern void * __jbd_kmalloc (char *where, size_t size, int flags, int retry);
#define jbd_kmalloc(size, flags) \
	__jbd_kmalloc(__FUNCTION__, (size), (flags), journal_oom_retry)
#define jbd_rep_kmalloc(size, flags) \
	__jbd_kmalloc(__FUNCTION__, (size), (flags), 1)

#define JFS_MIN_JOURNAL_BLOCKS 1024

#ifdef __KERNEL__
typedef struct handle_s		handle_t;	
typedef struct journal_s	journal_t;	
#endif


#define JFS_MAGIC_NUMBER 0xc03b3998U 



#define JFS_DESCRIPTOR_BLOCK	1
#define JFS_COMMIT_BLOCK	2
#define JFS_SUPERBLOCK_V1	3
#define JFS_SUPERBLOCK_V2	4
#define JFS_REVOKE_BLOCK	5

typedef struct journal_header_s
{
	__u32		h_magic;
	__u32		h_blocktype;
	__u32		h_sequence;
} journal_header_t;

#define JBD2_CRC32_CHKSUM   1
#define JBD2_MD5_CHKSUM     2
#define JBD2_SHA1_CHKSUM    3

#define JBD2_CRC32_CHKSUM_SIZE 4

#define JBD2_CHECKSUM_BYTES (32 / sizeof(__u32))
struct commit_header {
	__u32		h_magic;
	__u32		h_blocktype;
	__u32		h_sequence;
	unsigned char	h_chksum_type;
	unsigned char	h_chksum_size;
	unsigned char	h_padding[2];
	__u32		h_chksum[JBD2_CHECKSUM_BYTES];
	__u64		h_commit_sec;
	__u32		h_commit_nsec;
};

typedef struct journal_block_tag_s
{
	__u32		t_blocknr;	
	__u32		t_flags;	
	__u32		t_blocknr_high; 
} journal_block_tag_t;

#define JBD_TAG_SIZE64 (sizeof(journal_block_tag_t))
#define JBD_TAG_SIZE32 (8)

typedef struct journal_revoke_header_s
{
	journal_header_t r_header;
	int		 r_count;	
} journal_revoke_header_t;


#define JFS_FLAG_ESCAPE		1	
#define JFS_FLAG_SAME_UUID	2	
#define JFS_FLAG_DELETED	4	
#define JFS_FLAG_LAST_TAG	8	


typedef struct journal_superblock_s
{
	journal_header_t s_header;

	
	__u32	s_blocksize;		
	__u32	s_maxlen;		
	__u32	s_first;		

	
	__u32	s_sequence;		
	__u32	s_start;		

	
	__s32	s_errno;

	
	__u32	s_feature_compat; 	
	__u32	s_feature_incompat; 	
	__u32	s_feature_ro_compat; 	
	__u8	s_uuid[16];		

	__u32	s_nr_users;		

	__u32	s_dynsuper;		

	__u32	s_max_transaction;	
	__u32	s_max_trans_data;	

	__u32	s_padding[44];

	__u8	s_users[16*48];		
} journal_superblock_t;

#define JFS_HAS_COMPAT_FEATURE(j,mask)					\
	((j)->j_format_version >= 2 &&					\
	 ((j)->j_superblock->s_feature_compat & ext2fs_cpu_to_be32((mask))))
#define JFS_HAS_RO_COMPAT_FEATURE(j,mask)				\
	((j)->j_format_version >= 2 &&					\
	 ((j)->j_superblock->s_feature_ro_compat & ext2fs_cpu_to_be32((mask))))
#define JFS_HAS_INCOMPAT_FEATURE(j,mask)				\
	((j)->j_format_version >= 2 &&					\
	 ((j)->j_superblock->s_feature_incompat & ext2fs_cpu_to_be32((mask))))

#define JFS_FEATURE_COMPAT_CHECKSUM	0x00000001

#define JFS_FEATURE_INCOMPAT_REVOKE	0x00000001

#define JFS_FEATURE_INCOMPAT_REVOKE		0x00000001
#define JFS_FEATURE_INCOMPAT_64BIT		0x00000002
#define JFS_FEATURE_INCOMPAT_ASYNC_COMMIT	0x00000004

#define JFS_KNOWN_COMPAT_FEATURES	0
#define JFS_KNOWN_ROCOMPAT_FEATURES	0
#define JFS_KNOWN_INCOMPAT_FEATURES	(JFS_FEATURE_INCOMPAT_REVOKE|\
					 JFS_FEATURE_INCOMPAT_ASYNC_COMMIT|\
					 JFS_FEATURE_INCOMPAT_64BIT)

#ifdef __KERNEL__

#include <linux/fs.h>
#include <linux/sched.h>

#define JBD_ASSERTIONS
#ifdef JBD_ASSERTIONS
#define J_ASSERT(assert)						\
do {									\
	if (!(assert)) {						\
		printk (KERN_EMERG					\
			"Assertion failure in %s() at %s:%d: \"%s\"\n",	\
			__FUNCTION__, __FILE__, __LINE__, # assert);	\
		BUG();							\
	}								\
} while (0)

#if defined(CONFIG_BUFFER_DEBUG)
void buffer_assertion_failure(struct buffer_head *bh);
#define J_ASSERT_BH(bh, expr)						\
	do {								\
		if (!(expr))						\
			buffer_assertion_failure(bh);			\
		J_ASSERT(expr);						\
	} while (0)
#define J_ASSERT_JH(jh, expr)	J_ASSERT_BH(jh2bh(jh), expr)
#else
#define J_ASSERT_BH(bh, expr)	J_ASSERT(expr)
#define J_ASSERT_JH(jh, expr)	J_ASSERT(expr)
#endif

#else
#define J_ASSERT(assert)
#endif		

enum jbd_state_bits {
	BH_JWrite
	  = BH_PrivateStart,	/* 1 if being written to log (@@@ DEBUGGING) */
	BH_Freed,		
	BH_Revoked,		
	BH_RevokeValid,		
	BH_JBDDirty,		
};

static inline int buffer_jbd(struct buffer_head *bh)
{
	return __buffer_state(bh, JBD);
}

static inline struct buffer_head *jh2bh(struct journal_head *jh)
{
	return jh->b_bh;
}

static inline struct journal_head *bh2jh(struct buffer_head *bh)
{
	return bh->b_private;
}

struct jbd_revoke_table_s;


struct handle_s
{
	
	transaction_t	      * h_transaction;

	
	int			h_buffer_credits;

	
	int			h_ref;

	int			h_err;

	
	unsigned int	h_sync:		1;	
	unsigned int	h_jdata:	1;	
	unsigned int	h_aborted:	1;	
};



struct transaction_s
{
	
	journal_t *		t_journal;

	
	tid_t			t_tid;

	
	enum {
		T_RUNNING,
		T_LOCKED,
		T_RUNDOWN,
		T_FLUSH,
		T_COMMIT,
		T_FINISHED
	}			t_state;

	
	unsigned long		t_log_start;

	
	struct inode *		t_ilist;

	
	int			t_nr_buffers;

	struct journal_head *	t_reserved_list;

	struct journal_head *	t_buffers;

	struct journal_head *	t_sync_datalist;

	/*
	 * Doubly-linked circular list of all writepage data buffers
	 * still to be written before this transaction can be committed.
	 * Protected by journal_datalist_lock.
	 */
	struct journal_head *	t_async_datalist;

	struct journal_head *	t_forget;

	
	struct journal_head *	t_checkpoint_list;

	struct journal_head *	t_iobuf_list;

	struct journal_head *	t_shadow_list;

	/* Doubly-linked circular list of control buffers being written
           to the log. */
	struct journal_head *	t_log_list;

	
	int			t_updates;

	int			t_outstanding_credits;

	
	transaction_t		*t_cpnext, *t_cpprev;

	unsigned long		t_expires;

	
	int t_handle_count;
};



struct journal_s
{
	
	unsigned long		j_flags;

	int			j_errno;

	
	struct buffer_head *	j_sb_buffer;
	journal_superblock_t *	j_superblock;

	
	int			j_format_version;

	
	int			j_barrier_count;

	
	struct semaphore	j_barrier;

	
	transaction_t *		j_running_transaction;

	
	transaction_t *		j_committing_transaction;

	
	transaction_t *		j_checkpoint_transactions;

	wait_queue_head_t	j_wait_transaction_locked;

	
	wait_queue_head_t	j_wait_logspace;

	
	wait_queue_head_t	j_wait_done_commit;

	
	wait_queue_head_t	j_wait_checkpoint;

	
	wait_queue_head_t	j_wait_commit;

	
	wait_queue_head_t	j_wait_updates;

	
	struct semaphore 	j_checkpoint_sem;

	
	struct semaphore	j_sem;

	
	unsigned long		j_head;

	unsigned long		j_tail;

	
	unsigned long		j_free;

	unsigned long		j_first, j_last;

	kdev_t			j_dev;
	int			j_blocksize;
	unsigned int		j_blk_offset;

	kdev_t			j_fs_dev;

	
	unsigned int		j_maxlen;

	struct inode *		j_inode;

	
	tid_t			j_tail_sequence;
	
	tid_t			j_transaction_sequence;
	
	tid_t			j_commit_sequence;
	
	tid_t			j_commit_request;


	__u8			j_uuid[16];

	
	struct task_struct *	j_task;

	int			j_max_transaction_buffers;

	unsigned long		j_commit_interval;

	
	struct timer_list *	j_commit_timer;
	int			j_commit_timer_active;

	
	struct list_head	j_all_journals;

	struct jbd_revoke_table_s *j_revoke;

	
	unsigned int		j_failed_commit;
};

#define JFS_UNMOUNT	0x001	
#define JFS_ABORT	0x002	
#define JFS_ACK_ERR	0x004	
#define JFS_FLUSHED	0x008	
#define JFS_LOADED	0x010	


extern void __journal_unfile_buffer(struct journal_head *);
extern void journal_unfile_buffer(struct journal_head *);
extern void __journal_refile_buffer(struct journal_head *);
extern void journal_refile_buffer(struct journal_head *);
extern void __journal_file_buffer(struct journal_head *, transaction_t *, int);
extern void __journal_free_buffer(struct journal_head *bh);
extern void journal_file_buffer(struct journal_head *, transaction_t *, int);
extern void __journal_clean_data_list(transaction_t *transaction);

extern struct journal_head * journal_get_descriptor_buffer(journal_t *);
extern unsigned long journal_next_log_block(journal_t *);

extern void journal_commit_transaction(journal_t *);

int __journal_clean_checkpoint_list(journal_t *journal);
extern void journal_remove_checkpoint(struct journal_head *);
extern void __journal_remove_checkpoint(struct journal_head *);
extern void journal_insert_checkpoint(struct journal_head *, transaction_t *);
extern void __journal_insert_checkpoint(struct journal_head *,transaction_t *);

extern int
journal_write_metadata_buffer(transaction_t	  *transaction,
			      struct journal_head  *jh_in,
			      struct journal_head **jh_out,
			      int		   blocknr);

extern void		__wait_on_journal (journal_t *);


static inline void lock_journal(journal_t *journal)
{
	down(&journal->j_sem);
}

static inline int try_lock_journal(journal_t * journal)
{
	return down_trylock(&journal->j_sem);
}

static inline void unlock_journal(journal_t * journal)
{
	up(&journal->j_sem);
}


static inline handle_t *journal_current_handle(void)
{
	return current->journal_info;
}


extern handle_t *journal_start(journal_t *, int nblocks);
extern handle_t *journal_try_start(journal_t *, int nblocks);
extern int	 journal_restart (handle_t *, int nblocks);
extern int	 journal_extend (handle_t *, int nblocks);
extern int	 journal_get_write_access (handle_t *, struct buffer_head *);
extern int	 journal_get_create_access (handle_t *, struct buffer_head *);
extern int	 journal_get_undo_access (handle_t *, struct buffer_head *);
extern int	 journal_dirty_data (handle_t *,
				struct buffer_head *, int async);
extern int	 journal_dirty_metadata (handle_t *, struct buffer_head *);
extern void	 journal_release_buffer (handle_t *, struct buffer_head *);
extern void	 journal_forget (handle_t *, struct buffer_head *);
extern void	 journal_sync_buffer (struct buffer_head *);
extern int	 journal_flushpage(journal_t *, struct page *, unsigned long);
extern int	 journal_try_to_free_buffers(journal_t *, struct page *, int);
extern int	 journal_stop(handle_t *);
extern int	 journal_flush (journal_t *);

extern void	 journal_lock_updates (journal_t *);
extern void	 journal_unlock_updates (journal_t *);

extern journal_t * journal_init_dev(kdev_t dev, kdev_t fs_dev,
				int start, int len, int bsize);
extern journal_t * journal_init_inode (struct inode *);
extern int	   journal_update_format (journal_t *);
extern int	   journal_check_used_features
		   (journal_t *, unsigned long, unsigned long, unsigned long);
extern int	   journal_check_available_features
		   (journal_t *, unsigned long, unsigned long, unsigned long);
extern int	   journal_set_features
		   (journal_t *, unsigned long, unsigned long, unsigned long);
extern int	   journal_create     (journal_t *);
extern int	   journal_load       (journal_t *journal);
extern void	   journal_destroy    (journal_t *);
extern int	   journal_recover    (journal_t *journal);
extern int	   journal_wipe       (journal_t *, int);
extern int	   journal_skip_recovery (journal_t *);
extern void	   journal_update_superblock (journal_t *, int);
extern void	   __journal_abort      (journal_t *);
extern void	   journal_abort      (journal_t *, int);
extern int	   journal_errno      (journal_t *);
extern void	   journal_ack_err    (journal_t *);
extern int	   journal_clear_err  (journal_t *);
extern unsigned long journal_bmap(journal_t *journal, unsigned long blocknr);
extern int	    journal_force_commit(journal_t *journal);

extern struct journal_head
		*journal_add_journal_head(struct buffer_head *bh);
extern void	journal_remove_journal_head(struct buffer_head *bh);
extern void	__journal_remove_journal_head(struct buffer_head *bh);
extern void	journal_unlock_journal_head(struct journal_head *jh);

#define JOURNAL_REVOKE_DEFAULT_HASH 256
extern int	   journal_init_revoke(journal_t *, int);
extern void	   journal_destroy_revoke_caches(void);
extern int	   journal_init_revoke_caches(void);

extern void	   journal_destroy_revoke(journal_t *);
extern int	   journal_revoke (handle_t *,
				unsigned long, struct buffer_head *);
extern int	   journal_cancel_revoke(handle_t *, struct journal_head *);
extern void	   journal_write_revoke_records(journal_t *, transaction_t *);

extern int	   journal_set_revoke(journal_t *, unsigned long, tid_t);
extern int	   journal_test_revoke(journal_t *, unsigned long, tid_t);
extern void	   journal_clear_revoke(journal_t *);
extern void	   journal_brelse_array(struct buffer_head *b[], int n);


extern int	log_space_left (journal_t *); 
extern tid_t	log_start_commit (journal_t *, transaction_t *);
extern void	log_wait_commit (journal_t *, tid_t);
extern int	log_do_checkpoint (journal_t *, int);

extern void	log_wait_for_space(journal_t *, int nblocks);
extern void	__journal_drop_transaction(journal_t *, transaction_t *);
extern int	cleanup_journal_tail(journal_t *);

extern void shrink_journal_memory(void);


#define jbd_ENOSYS() \
do {								      \
	printk (KERN_ERR "JBD unimplemented function " __FUNCTION__); \
	current->state = TASK_UNINTERRUPTIBLE;			      \
	schedule();						      \
} while (1)


static inline int is_journal_aborted(journal_t *journal)
{
	return journal->j_flags & JFS_ABORT;
}

static inline int is_handle_aborted(handle_t *handle)
{
	if (handle->h_aborted)
		return 1;
	return is_journal_aborted(handle->h_transaction->t_journal);
}

static inline void journal_abort_handle(handle_t *handle)
{
	handle->h_aborted = 1;
}

#ifndef BUG
#define BUG() do { \
        printk("kernel BUG at %s:%d!\n", __FILE__, __LINE__); \
	* ((char *) 0) = 0; \
 } while (0)
#endif 

#else

extern int	   journal_recover    (journal_t *journal);
extern int	   journal_skip_recovery (journal_t *);

extern int	   journal_init_revoke(journal_t *, int);
extern void	   journal_destroy_revoke_caches(void);
extern int	   journal_init_revoke_caches(void);

extern int	   journal_set_revoke(journal_t *, unsigned long, tid_t);
extern int	   journal_test_revoke(journal_t *, unsigned long, tid_t);
extern void	   journal_clear_revoke(journal_t *);
extern void	   journal_brelse_array(struct buffer_head *b[], int n);

extern void	   journal_destroy_revoke(journal_t *);
#endif 

static inline int tid_gt(tid_t x, tid_t y) EXT2FS_ATTR((unused));
static inline int tid_geq(tid_t x, tid_t y) EXT2FS_ATTR((unused));


static inline int tid_gt(tid_t x, tid_t y)
{
	int difference = (x - y);
	return (difference > 0);
}

static inline int tid_geq(tid_t x, tid_t y)
{
	int difference = (x - y);
	return (difference >= 0);
}

extern int journal_blocks_per_page(struct inode *inode);


#define BJ_None		0	
#define BJ_SyncData	1	
#define BJ_AsyncData	2	
#define BJ_Metadata	3	
#define BJ_Forget	4	
#define BJ_IO		5	
#define BJ_Shadow	6	
#define BJ_LogCtl	7	
#define BJ_Reserved	8	
#define BJ_Types	9

extern int jbd_blocks_per_page(struct inode *inode);

#ifdef __KERNEL__

extern spinlock_t jh_splice_lock;
#define SPLICE_LOCK(expr1, expr2)				\
	({							\
		int ret = (expr1);				\
		if (ret) {					\
			spin_lock(&jh_splice_lock);		\
			ret = (expr1) && (expr2);		\
			spin_unlock(&jh_splice_lock);		\
		}						\
		ret;						\
	})


static inline int buffer_jlist_eq(struct buffer_head *bh, int list)
{
	return SPLICE_LOCK(buffer_jbd(bh), bh2jh(bh)->b_jlist == list);
}

static inline int buffer_jdirty(struct buffer_head *bh)
{
	return buffer_jbd(bh) && __buffer_state(bh, JBDDirty);
}

static inline int buffer_jbd_data(struct buffer_head *bh)
{
	return SPLICE_LOCK(buffer_jbd(bh),
			bh2jh(bh)->b_jlist == BJ_SyncData ||
			bh2jh(bh)->b_jlist == BJ_AsyncData);
}

#ifdef CONFIG_SMP
#define assert_spin_locked(lock)	J_ASSERT(spin_is_locked(lock))
#else
#define assert_spin_locked(lock)	do {} while(0)
#endif

#define buffer_trace_init(bh)	do {} while (0)
#define print_buffer_fields(bh)	do {} while (0)
#define print_buffer_trace(bh)	do {} while (0)
#define BUFFER_TRACE(bh, info)	do {} while (0)
#define BUFFER_TRACE2(bh, bh2, info)	do {} while (0)
#define JBUFFER_TRACE(jh, info)	do {} while (0)

#endif	

#endif	


#if defined(__KERNEL__) && !(defined(CONFIG_JBD) || defined(CONFIG_JBD_MODULE))

#define J_ASSERT(expr)			do {} while (0)
#define J_ASSERT_BH(bh, expr)		do {} while (0)
#define buffer_jbd(bh)			0
#define buffer_jlist_eq(bh, val)	0
#define journal_buffer_journal_lru(bh)	0

#endif	
#endif	
