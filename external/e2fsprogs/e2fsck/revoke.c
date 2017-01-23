/*
 * linux/fs/revoke.c
 *
 * Written by Stephen C. Tweedie <sct@redhat.com>, 2000
 *
 * Copyright 2000 Red Hat corp --- All Rights Reserved
 *
 * This file is part of the Linux kernel and is made available under
 * the terms of the GNU General Public License, version 2, or at your
 * option, any later version, incorporated herein by reference.
 *
 * Journal revoke routines for the generic filesystem journaling code;
 * part of the ext2fs journaling system.
 *
 * Revoke is the mechanism used to prevent old log records for deleted
 * metadata from being replayed on top of newer data using the same
 * blocks.  The revoke mechanism is used in two separate places:
 *
 * + Commit: during commit we write the entire list of the current
 *   transaction's revoked blocks to the journal
 *
 * + Recovery: during recovery we record the transaction ID of all
 *   revoked blocks.  If there are multiple revoke records in the log
 *   for a single block, only the last one counts, and if there is a log
 *   entry for a block beyond the last revoke, then that log entry still
 *   gets replayed.
 *
 * We can get interactions between revokes and new log data within a
 * single transaction:
 *
 * Block is revoked and then journaled:
 *   The desired end result is the journaling of the new block, so we
 *   cancel the revoke before the transaction commits.
 *
 * Block is journaled and then revoked:
 *   The revoke must take precedence over the write of the block, so we
 *   need either to cancel the journal entry or to write the revoke
 *   later in the log than the log block.  In this case, we choose the
 *   latter: journaling a block cancels any revoke record for that block
 *   in the current transaction, so any revoke for that block in the
 *   transaction must have happened after the block was journaled and so
 *   the revoke must take precedence.
 *
 * Block is revoked and then written as data:
 *   The data write is allowed to succeed, but the revoke is _not_
 *   cancelled.  We still need to prevent old log records from
 *   overwriting the new data.  We don't even need to clear the revoke
 *   bit here.
 *
 * Revoke information on buffers is a tri-state value:
 *
 * RevokeValid clear:	no cached revoke status, need to look it up
 * RevokeValid set, Revoked clear:
 *			buffer has not been revoked, and cancel_revoke
 *			need do nothing.
 * RevokeValid set, Revoked set:
 *			buffer has been revoked.
 */

#ifndef __KERNEL__
#include "jfs_user.h"
#else
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/jbd.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/locks.h>
#include <linux/list.h>
#include <linux/smp_lock.h>
#include <linux/init.h>
#endif

static lkmem_cache_t *revoke_record_cache;
static lkmem_cache_t *revoke_table_cache;


struct jbd_revoke_record_s
{
	struct list_head  hash;
	tid_t		  sequence;	
	unsigned long	  blocknr;
};


struct jbd_revoke_table_s
{
	int		  hash_size;
	int		  hash_shift;
	struct list_head *hash_table;
};


#ifdef __KERNEL__
static void write_one_revoke_record(journal_t *, transaction_t *,
				    struct journal_head **, int *,
				    struct jbd_revoke_record_s *);
static void flush_descriptor(journal_t *, struct journal_head *, int);
#endif


static inline int hash(journal_t *journal, unsigned long block)
{
	struct jbd_revoke_table_s *table = journal->j_revoke;
	int hash_shift = table->hash_shift;

	return ((block << (hash_shift - 6)) ^
		(block >> 13) ^
		(block << (hash_shift - 12))) & (table->hash_size - 1);
}

static int insert_revoke_hash(journal_t *journal, unsigned long blocknr,
			      tid_t seq)
{
	struct list_head *hash_list;
	struct jbd_revoke_record_s *record;

#ifdef __KERNEL__
repeat:
#endif
	record = kmem_cache_alloc(revoke_record_cache, GFP_NOFS);
	if (!record)
		goto oom;

	record->sequence = seq;
	record->blocknr = blocknr;
	hash_list = &journal->j_revoke->hash_table[hash(journal, blocknr)];
	list_add(&record->hash, hash_list);
	return 0;

oom:
#ifdef __KERNEL__
	if (!journal_oom_retry)
		return -ENOMEM;
	jbd_debug(1, "ENOMEM in " __FUNCTION__ ", retrying.\n");
	current->policy |= SCHED_YIELD;
	schedule();
	goto repeat;
#else
	return -ENOMEM;
#endif
}


static struct jbd_revoke_record_s *find_revoke_record(journal_t *journal,
						      unsigned long blocknr)
{
	struct list_head *hash_list;
	struct jbd_revoke_record_s *record;

	hash_list = &journal->j_revoke->hash_table[hash(journal, blocknr)];

	record = (struct jbd_revoke_record_s *) hash_list->next;
	while (&(record->hash) != hash_list) {
		if (record->blocknr == blocknr)
			return record;
		record = (struct jbd_revoke_record_s *) record->hash.next;
	}
	return NULL;
}

int __init journal_init_revoke_caches(void)
{
	revoke_record_cache = kmem_cache_create("revoke_record",
					   sizeof(struct jbd_revoke_record_s),
					   0, SLAB_HWCACHE_ALIGN, NULL, NULL);
	if (revoke_record_cache == 0)
		return -ENOMEM;

	revoke_table_cache = kmem_cache_create("revoke_table",
					   sizeof(struct jbd_revoke_table_s),
					   0, 0, NULL, NULL);
	if (revoke_table_cache == 0) {
		kmem_cache_destroy(revoke_record_cache);
		revoke_record_cache = NULL;
		return -ENOMEM;
	}
	return 0;
}

void journal_destroy_revoke_caches(void)
{
	kmem_cache_destroy(revoke_record_cache);
	revoke_record_cache = 0;
	kmem_cache_destroy(revoke_table_cache);
	revoke_table_cache = 0;
}


int journal_init_revoke(journal_t *journal, int hash_size)
{
	int shift, tmp;

	J_ASSERT (journal->j_revoke == NULL);

	journal->j_revoke = kmem_cache_alloc(revoke_table_cache, GFP_KERNEL);
	if (!journal->j_revoke)
		return -ENOMEM;

	
	J_ASSERT ((hash_size & (hash_size-1)) == 0);

	journal->j_revoke->hash_size = hash_size;

	shift = 0;
	tmp = hash_size;
	while((tmp >>= 1UL) != 0UL)
		shift++;
	journal->j_revoke->hash_shift = shift;

	journal->j_revoke->hash_table =
		kmalloc(hash_size * sizeof(struct list_head), GFP_KERNEL);
	if (!journal->j_revoke->hash_table) {
		kmem_cache_free(revoke_table_cache, journal->j_revoke);
		journal->j_revoke = NULL;
		return -ENOMEM;
	}

	for (tmp = 0; tmp < hash_size; tmp++)
		INIT_LIST_HEAD(&journal->j_revoke->hash_table[tmp]);

	return 0;
}


void journal_destroy_revoke(journal_t *journal)
{
	struct jbd_revoke_table_s *table;
	struct list_head *hash_list;
	int i;

	table = journal->j_revoke;
	if (!table)
		return;

	for (i=0; i<table->hash_size; i++) {
		hash_list = &table->hash_table[i];
		J_ASSERT (list_empty(hash_list));
	}

	kfree(table->hash_table);
	kmem_cache_free(revoke_table_cache, table);
	journal->j_revoke = NULL;
}


#ifdef __KERNEL__


int journal_revoke(handle_t *handle, unsigned long blocknr,
		   struct buffer_head *bh_in)
{
	struct buffer_head *bh = NULL;
	journal_t *journal;
	kdev_t dev;
	int err;

	if (bh_in)
		BUFFER_TRACE(bh_in, "enter");

	journal = handle->h_transaction->t_journal;
	if (!journal_set_features(journal, 0, 0, JFS_FEATURE_INCOMPAT_REVOKE)){
		J_ASSERT (!"Cannot set revoke feature!");
		return -EINVAL;
	}

	dev = journal->j_fs_dev;
	bh = bh_in;

	if (!bh) {
		bh = get_hash_table(dev, blocknr, journal->j_blocksize);
		if (bh)
			BUFFER_TRACE(bh, "found on hash");
	}
#ifdef JBD_EXPENSIVE_CHECKING
	else {
		struct buffer_head *bh2;

		bh2 = get_hash_table(dev, blocknr, journal->j_blocksize);
		if (bh2) {
			
			if ((bh2 != bh) &&
			    test_bit(BH_RevokeValid, &bh2->b_state))
				J_ASSERT_BH(bh2, test_bit(BH_Revoked, &
							  bh2->b_state));
			__brelse(bh2);
		}
	}
#endif

	if (bh) {
		J_ASSERT_BH(bh, !test_bit(BH_Revoked, &bh->b_state));
		set_bit(BH_Revoked, &bh->b_state);
		set_bit(BH_RevokeValid, &bh->b_state);
		if (bh_in) {
			BUFFER_TRACE(bh_in, "call journal_forget");
			journal_forget(handle, bh_in);
		} else {
			BUFFER_TRACE(bh, "call brelse");
			__brelse(bh);
		}
	}

	lock_journal(journal);
	jbd_debug(2, "insert revoke for block %lu, bh_in=%p\n", blocknr, bh_in);
	err = insert_revoke_hash(journal, blocknr,
				handle->h_transaction->t_tid);
	unlock_journal(journal);
	BUFFER_TRACE(bh_in, "exit");
	return err;
}

int journal_cancel_revoke(handle_t *handle, struct journal_head *jh)
{
	struct jbd_revoke_record_s *record;
	journal_t *journal = handle->h_transaction->t_journal;
	int need_cancel;
	int did_revoke = 0;	
	struct buffer_head *bh = jh2bh(jh);

	jbd_debug(4, "journal_head %p, cancelling revoke\n", jh);

	if (test_and_set_bit(BH_RevokeValid, &bh->b_state))
		need_cancel = (test_and_clear_bit(BH_Revoked, &bh->b_state));
	else {
		need_cancel = 1;
		clear_bit(BH_Revoked, &bh->b_state);
	}

	if (need_cancel) {
		record = find_revoke_record(journal, bh->b_blocknr);
		if (record) {
			jbd_debug(4, "cancelled existing revoke on "
				  "blocknr %lu\n", bh->b_blocknr);
			list_del(&record->hash);
			kmem_cache_free(revoke_record_cache, record);
			did_revoke = 1;
		}
	}

#ifdef JBD_EXPENSIVE_CHECKING
	
	record = find_revoke_record(journal, bh->b_blocknr);
	J_ASSERT_JH(jh, record == NULL);
#endif

	if (need_cancel && !bh->b_pprev) {
		struct buffer_head *bh2;
		bh2 = get_hash_table(bh->b_dev, bh->b_blocknr, bh->b_size);
		if (bh2) {
			clear_bit(BH_Revoked, &bh2->b_state);
			__brelse(bh2);
		}
	}

	return did_revoke;
}



void journal_write_revoke_records(journal_t *journal,
				  transaction_t *transaction)
{
	struct journal_head *descriptor;
	struct jbd_revoke_record_s *record;
	struct jbd_revoke_table_s *revoke;
	struct list_head *hash_list;
	int i, offset, count;

	descriptor = NULL;
	offset = 0;
	count = 0;
	revoke = journal->j_revoke;

	for (i = 0; i < revoke->hash_size; i++) {
		hash_list = &revoke->hash_table[i];

		while (!list_empty(hash_list)) {
			record = (struct jbd_revoke_record_s *)
				hash_list->next;
			write_one_revoke_record(journal, transaction,
						&descriptor, &offset,
						record);
			count++;
			list_del(&record->hash);
			kmem_cache_free(revoke_record_cache, record);
		}
	}
	if (descriptor)
		flush_descriptor(journal, descriptor, offset);
	jbd_debug(1, "Wrote %d revoke records\n", count);
}


static void write_one_revoke_record(journal_t *journal,
				    transaction_t *transaction,
				    struct journal_head **descriptorp,
				    int *offsetp,
				    struct jbd_revoke_record_s *record)
{
	struct journal_head *descriptor;
	int offset;
	journal_header_t *header;

	if (is_journal_aborted(journal))
		return;

	descriptor = *descriptorp;
	offset = *offsetp;

	
	if (descriptor) {
		if (offset == journal->j_blocksize) {
			flush_descriptor(journal, descriptor, offset);
			descriptor = NULL;
		}
	}

	if (!descriptor) {
		descriptor = journal_get_descriptor_buffer(journal);
		if (!descriptor)
			return;
		header = (journal_header_t *) &jh2bh(descriptor)->b_data[0];
		header->h_magic     = htonl(JFS_MAGIC_NUMBER);
		header->h_blocktype = htonl(JFS_REVOKE_BLOCK);
		header->h_sequence  = htonl(transaction->t_tid);

		
		JBUFFER_TRACE(descriptor, "file as BJ_LogCtl");
		journal_file_buffer(descriptor, transaction, BJ_LogCtl);

		offset = sizeof(journal_revoke_header_t);
		*descriptorp = descriptor;
	}

	* ((unsigned int *)(&jh2bh(descriptor)->b_data[offset])) =
		htonl(record->blocknr);
	offset += 4;
	*offsetp = offset;
}


static void flush_descriptor(journal_t *journal,
			     struct journal_head *descriptor,
			     int offset)
{
	journal_revoke_header_t *header;

	if (is_journal_aborted(journal)) {
		JBUFFER_TRACE(descriptor, "brelse");
		__brelse(jh2bh(descriptor));
		return;
	}

	header = (journal_revoke_header_t *) jh2bh(descriptor)->b_data;
	header->r_count = htonl(offset);
	set_bit(BH_JWrite, &jh2bh(descriptor)->b_state);
	{
		struct buffer_head *bh = jh2bh(descriptor);
		BUFFER_TRACE(bh, "write");
		ll_rw_block (WRITE, 1, &bh);
	}
}

#endif



int journal_set_revoke(journal_t *journal,
		       unsigned long blocknr,
		       tid_t sequence)
{
	struct jbd_revoke_record_s *record;

	record = find_revoke_record(journal, blocknr);
	if (record) {
		if (tid_gt(sequence, record->sequence))
			record->sequence = sequence;
		return 0;
	}
	return insert_revoke_hash(journal, blocknr, sequence);
}


int journal_test_revoke(journal_t *journal,
			unsigned long blocknr,
			tid_t sequence)
{
	struct jbd_revoke_record_s *record;

	record = find_revoke_record(journal, blocknr);
	if (!record)
		return 0;
	if (tid_gt(sequence, record->sequence))
		return 0;
	return 1;
}


void journal_clear_revoke(journal_t *journal)
{
	int i;
	struct list_head *hash_list;
	struct jbd_revoke_record_s *record;
	struct jbd_revoke_table_s *revoke;

	revoke = journal->j_revoke;

	for (i = 0; i < revoke->hash_size; i++) {
		hash_list = &revoke->hash_table[i];
		while (!list_empty(hash_list)) {
			record = (struct jbd_revoke_record_s*) hash_list->next;
			list_del(&record->hash);
			kmem_cache_free(revoke_record_cache, record);
		}
	}
}

