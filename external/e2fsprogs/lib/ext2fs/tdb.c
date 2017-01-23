 /*
   trivial database library - standalone version

   Copyright (C) Andrew Tridgell              1999-2005
   Copyright (C) Jeremy Allison               2000-2006
   Copyright (C) Paul `Rusty' Russell         2000

     ** NOTE! The following LGPL license applies to the tdb
     ** library. This does NOT imply that all of Samba is released
     ** under the LGPL

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifdef CONFIG_STAND_ALONE
#define HAVE_MMAP
#define HAVE_STRDUP
#define HAVE_SYS_MMAN_H
#define HAVE_UTIME_H
#define HAVE_UTIME
#endif
#define _XOPEN_SOURCE 600

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>
#include <errno.h>
#include <string.h>
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#ifdef HAVE_UTIME_H
#include <utime.h>
#endif
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>

#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif

#ifndef MAP_FILE
#define MAP_FILE 0
#endif

#ifndef MAP_FAILED
#define MAP_FAILED ((void *)-1)
#endif

#ifndef HAVE_STRDUP
#define strdup rep_strdup
static char *rep_strdup(const char *s)
{
	char *ret;
	int length;
	if (!s)
		return NULL;

	if (!length)
		length = strlen(s);

	ret = malloc(length + 1);
	if (ret) {
		strncpy(ret, s, length);
		ret[length] = '\0';
	}
	return ret;
}
#endif

#ifndef PRINTF_ATTRIBUTE
#if (__GNUC__ >= 3) && (__GNUC_MINOR__ >= 1 )
#define PRINTF_ATTRIBUTE(a1, a2) __attribute__ ((format (__printf__, a1, a2)))
#else
#define PRINTF_ATTRIBUTE(a1, a2)
#endif
#endif

typedef int bool;

#include "tdb.h"

static TDB_DATA tdb_null;

#ifndef u32
#define u32 unsigned
#endif

typedef u32 tdb_len_t;
typedef u32 tdb_off_t;

#ifndef offsetof
#define offsetof(t,f) ((unsigned int)&((t *)0)->f)
#endif

#define TDB_MAGIC_FOOD "TDB file\n"
#define TDB_VERSION (0x26011967 + 6)
#define TDB_MAGIC (0x26011999U)
#define TDB_FREE_MAGIC (~TDB_MAGIC)
#define TDB_DEAD_MAGIC (0xFEE1DEAD)
#define TDB_RECOVERY_MAGIC (0xf53bc0e7U)
#define TDB_ALIGNMENT 4
#define MIN_REC_SIZE (2*sizeof(struct list_struct) + TDB_ALIGNMENT)
#define DEFAULT_HASH_SIZE 131
#define FREELIST_TOP (sizeof(struct tdb_header))
#define TDB_ALIGN(x,a) (((x) + (a)-1) & ~((a)-1))
#define TDB_BYTEREV(x) (((((x)&0xff)<<24)|((x)&0xFF00)<<8)|(((x)>>8)&0xFF00)|((x)>>24))
#define TDB_DEAD(r) ((r)->magic == TDB_DEAD_MAGIC)
#define TDB_BAD_MAGIC(r) ((r)->magic != TDB_MAGIC && !TDB_DEAD(r))
#define TDB_HASH_TOP(hash) (FREELIST_TOP + (BUCKET(hash)+1)*sizeof(tdb_off_t))
#define TDB_HASHTABLE_SIZE(tdb) ((tdb->header.hash_size+1)*sizeof(tdb_off_t))
#define TDB_DATA_START(hash_size) TDB_HASH_TOP(hash_size-1)
#define TDB_RECOVERY_HEAD offsetof(struct tdb_header, recovery_start)
#define TDB_SEQNUM_OFS    offsetof(struct tdb_header, sequence_number)
#define TDB_PAD_BYTE 0x42
#define TDB_PAD_U32  0x42424242

#define TDB_LOG(x) tdb->log.log_fn x

#define GLOBAL_LOCK      0
#define ACTIVE_LOCK      4
#define TRANSACTION_LOCK 8

#ifndef SAFE_FREE
#define SAFE_FREE(x) do { if ((x) != NULL) {free(x); (x)=NULL;} } while(0)
#endif

#define BUCKET(hash) ((hash) % tdb->header.hash_size)

#define DOCONV() (tdb->flags & TDB_CONVERT)
#define CONVERT(x) (DOCONV() ? tdb_convert(&x, sizeof(x)) : &x)


struct list_struct {
	tdb_off_t next; 
	tdb_len_t rec_len; 
	tdb_len_t key_len; 
	tdb_len_t data_len; 
	u32 full_hash; 
	u32 magic;   
};


struct tdb_header {
	char magic_food[32]; 
	u32 version; 
	u32 hash_size; 
	tdb_off_t rwlocks; 
	tdb_off_t recovery_start; 
	tdb_off_t sequence_number; 
	tdb_off_t reserved[29];
};

struct tdb_lock_type {
	int list;
	u32 count;
	u32 ltype;
};

struct tdb_traverse_lock {
	struct tdb_traverse_lock *next;
	u32 off;
	u32 hash;
	int lock_rw;
};


struct tdb_methods {
	int (*tdb_read)(struct tdb_context *, tdb_off_t , void *, tdb_len_t , int );
	int (*tdb_write)(struct tdb_context *, tdb_off_t, const void *, tdb_len_t);
	void (*next_hash_chain)(struct tdb_context *, u32 *);
	int (*tdb_oob)(struct tdb_context *, tdb_off_t , int );
	int (*tdb_expand_file)(struct tdb_context *, tdb_off_t , tdb_off_t );
	int (*tdb_brlock)(struct tdb_context *, tdb_off_t , int, int, int, size_t);
};

struct tdb_context {
	char *name; 
	void *map_ptr; 
	int fd; 
	tdb_len_t map_size; 
	int read_only; 
	int traverse_read; 
	struct tdb_lock_type global_lock;
	int num_lockrecs;
	struct tdb_lock_type *lockrecs; 
	enum TDB_ERROR ecode; 
	struct tdb_header header; 
	u32 flags; 
	struct tdb_traverse_lock travlocks; 
	struct tdb_context *next; 
	dev_t device;	
	ino_t inode;	
	struct tdb_logging_context log;
	unsigned int (*hash_fn)(TDB_DATA *key);
	int open_flags; 
	unsigned int num_locks; 
	const struct tdb_methods *methods;
	struct tdb_transaction *transaction;
	int page_size;
	int max_dead_records;
	bool have_transaction_lock;
};


static int tdb_munmap(struct tdb_context *tdb);
static void tdb_mmap(struct tdb_context *tdb);
static int tdb_lock(struct tdb_context *tdb, int list, int ltype);
static int tdb_unlock(struct tdb_context *tdb, int list, int ltype);
static int tdb_brlock(struct tdb_context *tdb, tdb_off_t offset, int rw_type, int lck_type, int probe, size_t len);
static int tdb_transaction_lock(struct tdb_context *tdb, int ltype);
static int tdb_transaction_unlock(struct tdb_context *tdb);
static int tdb_brlock_upgrade(struct tdb_context *tdb, tdb_off_t offset, size_t len);
static int tdb_write_lock_record(struct tdb_context *tdb, tdb_off_t off);
static int tdb_write_unlock_record(struct tdb_context *tdb, tdb_off_t off);
static int tdb_ofs_read(struct tdb_context *tdb, tdb_off_t offset, tdb_off_t *d);
static int tdb_ofs_write(struct tdb_context *tdb, tdb_off_t offset, tdb_off_t *d);
static void *tdb_convert(void *buf, u32 size);
static int tdb_free(struct tdb_context *tdb, tdb_off_t offset, struct list_struct *rec);
static tdb_off_t tdb_allocate(struct tdb_context *tdb, tdb_len_t length, struct list_struct *rec);
static int tdb_ofs_read(struct tdb_context *tdb, tdb_off_t offset, tdb_off_t *d);
static int tdb_ofs_write(struct tdb_context *tdb, tdb_off_t offset, tdb_off_t *d);
static int tdb_lock_record(struct tdb_context *tdb, tdb_off_t off);
static int tdb_unlock_record(struct tdb_context *tdb, tdb_off_t off);
static int tdb_rec_read(struct tdb_context *tdb, tdb_off_t offset, struct list_struct *rec);
static int tdb_rec_write(struct tdb_context *tdb, tdb_off_t offset, struct list_struct *rec);
static int tdb_do_delete(struct tdb_context *tdb, tdb_off_t rec_ptr, struct list_struct *rec);
static unsigned char *tdb_alloc_read(struct tdb_context *tdb, tdb_off_t offset, tdb_len_t len);
static int tdb_parse_data(struct tdb_context *tdb, TDB_DATA key,
		   tdb_off_t offset, tdb_len_t len,
		   int (*parser)(TDB_DATA key, TDB_DATA data,
				 void *private_data),
		   void *private_data);
static tdb_off_t tdb_find_lock_hash(struct tdb_context *tdb, TDB_DATA key, u32 hash, int locktype,
			   struct list_struct *rec);
static void tdb_io_init(struct tdb_context *tdb);
static int tdb_expand(struct tdb_context *tdb, tdb_off_t size);
static int tdb_rec_free_read(struct tdb_context *tdb, tdb_off_t off,
		      struct list_struct *rec);



enum TDB_ERROR tdb_error(struct tdb_context *tdb)
{
	return tdb->ecode;
}

static struct tdb_errname {
	enum TDB_ERROR ecode; const char *estring;
} emap[] = { {TDB_SUCCESS, "Success"},
	     {TDB_ERR_CORRUPT, "Corrupt database"},
	     {TDB_ERR_IO, "IO Error"},
	     {TDB_ERR_LOCK, "Locking error"},
	     {TDB_ERR_OOM, "Out of memory"},
	     {TDB_ERR_EXISTS, "Record exists"},
	     {TDB_ERR_NOLOCK, "Lock exists on other keys"},
	     {TDB_ERR_EINVAL, "Invalid parameter"},
	     {TDB_ERR_NOEXIST, "Record does not exist"},
	     {TDB_ERR_RDONLY, "write not permitted"} };

const char *tdb_errorstr(struct tdb_context *tdb)
{
	u32 i;
	for (i = 0; i < sizeof(emap) / sizeof(struct tdb_errname); i++)
		if (tdb->ecode == emap[i].ecode)
			return emap[i].estring;
	return "Invalid error code";
}


#define TDB_MARK_LOCK 0x80000000

int tdb_brlock(struct tdb_context *tdb, tdb_off_t offset,
	       int rw_type, int lck_type, int probe, size_t len)
{
	struct flock fl;
	int ret;

	if (tdb->flags & TDB_NOLOCK) {
		return 0;
	}

	if ((rw_type == F_WRLCK) && (tdb->read_only || tdb->traverse_read)) {
		tdb->ecode = TDB_ERR_RDONLY;
		return -1;
	}

	fl.l_type = rw_type;
	fl.l_whence = SEEK_SET;
	fl.l_start = offset;
	fl.l_len = len;
	fl.l_pid = 0;

	do {
		ret = fcntl(tdb->fd,lck_type,&fl);
	} while (ret == -1 && errno == EINTR);

	if (ret == -1) {
		if (!probe && lck_type != F_SETLK) {
			
			tdb->ecode = TDB_ERR_LOCK;
			TDB_LOG((tdb, TDB_DEBUG_TRACE,"tdb_brlock failed (fd=%d) at offset %d rw_type=%d lck_type=%d len=%d\n",
				 tdb->fd, offset, rw_type, lck_type, (int)len));
		}
		return TDB_ERRCODE(TDB_ERR_LOCK, -1);
	}
	return 0;
}


int tdb_brlock_upgrade(struct tdb_context *tdb, tdb_off_t offset, size_t len)
{
	int count = 1000;
	while (count--) {
		struct timeval tv;
		if (tdb_brlock(tdb, offset, F_WRLCK, F_SETLKW, 1, len) == 0) {
			return 0;
		}
		if (errno != EDEADLK) {
			break;
		}
		
		tv.tv_sec = 0;
		tv.tv_usec = 1;
		select(0, NULL, NULL, NULL, &tv);
	}
	TDB_LOG((tdb, TDB_DEBUG_TRACE,"tdb_brlock_upgrade failed at offset %d\n", offset));
	return -1;
}


static int _tdb_lock(struct tdb_context *tdb, int list, int ltype, int op)
{
	struct tdb_lock_type *new_lck;
	int i;
	bool mark_lock = ((ltype & TDB_MARK_LOCK) == TDB_MARK_LOCK);

	ltype &= ~TDB_MARK_LOCK;

	
	if (tdb->global_lock.count &&
	    (ltype == tdb->global_lock.ltype || ltype == F_RDLCK)) {
		return 0;
	}

	if (tdb->global_lock.count) {
		return TDB_ERRCODE(TDB_ERR_LOCK, -1);
	}

	if (list < -1 || list >= (int)tdb->header.hash_size) {
		TDB_LOG((tdb, TDB_DEBUG_ERROR,"tdb_lock: invalid list %d for ltype=%d\n",
			   list, ltype));
		return -1;
	}
	if (tdb->flags & TDB_NOLOCK)
		return 0;

	for (i=0; i<tdb->num_lockrecs; i++) {
		if (tdb->lockrecs[i].list == list) {
			if (tdb->lockrecs[i].count == 0) {
				TDB_LOG((tdb, TDB_DEBUG_ERROR, "tdb_lock: "
					 "lck->count == 0 for list %d", list));
			}
			tdb->lockrecs[i].count++;
			return 0;
		}
	}

	new_lck = (struct tdb_lock_type *)realloc(
		tdb->lockrecs,
		sizeof(*tdb->lockrecs) * (tdb->num_lockrecs+1));
	if (new_lck == NULL) {
		errno = ENOMEM;
		return -1;
	}
	tdb->lockrecs = new_lck;

	if (!mark_lock &&
	    tdb->methods->tdb_brlock(tdb,FREELIST_TOP+4*list, ltype, op,
				     0, 1)) {
		return -1;
	}

	tdb->num_locks++;

	tdb->lockrecs[tdb->num_lockrecs].list = list;
	tdb->lockrecs[tdb->num_lockrecs].count = 1;
	tdb->lockrecs[tdb->num_lockrecs].ltype = ltype;
	tdb->num_lockrecs += 1;

	return 0;
}

int tdb_lock(struct tdb_context *tdb, int list, int ltype)
{
	int ret;
	ret = _tdb_lock(tdb, list, ltype, F_SETLKW);
	if (ret) {
		TDB_LOG((tdb, TDB_DEBUG_ERROR, "tdb_lock failed on list %d "
			 "ltype=%d (%s)\n",  list, ltype, strerror(errno)));
	}
	return ret;
}

int tdb_lock_nonblock(struct tdb_context *tdb, int list, int ltype)
{
	return _tdb_lock(tdb, list, ltype, F_SETLK);
}


int tdb_unlock(struct tdb_context *tdb, int list, int ltype)
{
	int ret = -1;
	int i;
	struct tdb_lock_type *lck = NULL;
	bool mark_lock = ((ltype & TDB_MARK_LOCK) == TDB_MARK_LOCK);

	ltype &= ~TDB_MARK_LOCK;

	
	if (tdb->global_lock.count &&
	    (ltype == tdb->global_lock.ltype || ltype == F_RDLCK)) {
		return 0;
	}

	if (tdb->global_lock.count) {
		return TDB_ERRCODE(TDB_ERR_LOCK, -1);
	}

	if (tdb->flags & TDB_NOLOCK)
		return 0;

	
	if (list < -1 || list >= (int)tdb->header.hash_size) {
		TDB_LOG((tdb, TDB_DEBUG_ERROR, "tdb_unlock: list %d invalid (%d)\n", list, tdb->header.hash_size));
		return ret;
	}

	for (i=0; i<tdb->num_lockrecs; i++) {
		if (tdb->lockrecs[i].list == list) {
			lck = &tdb->lockrecs[i];
			break;
		}
	}

	if ((lck == NULL) || (lck->count == 0)) {
		TDB_LOG((tdb, TDB_DEBUG_ERROR, "tdb_unlock: count is 0\n"));
		return -1;
	}

	if (lck->count > 1) {
		lck->count--;
		return 0;
	}


	if (mark_lock) {
		ret = 0;
	} else {
		ret = tdb->methods->tdb_brlock(tdb, FREELIST_TOP+4*list, F_UNLCK,
					       F_SETLKW, 0, 1);
	}
	tdb->num_locks--;


	if (tdb->num_lockrecs > 1) {
		*lck = tdb->lockrecs[tdb->num_lockrecs-1];
	}
	tdb->num_lockrecs -= 1;


	if (tdb->num_lockrecs == 0) {
		SAFE_FREE(tdb->lockrecs);
	}

	if (ret)
		TDB_LOG((tdb, TDB_DEBUG_ERROR, "tdb_unlock: An error occurred unlocking!\n"));
	return ret;
}

int tdb_transaction_lock(struct tdb_context *tdb, int ltype)
{
	if (tdb->have_transaction_lock || tdb->global_lock.count) {
		return 0;
	}
	if (tdb->methods->tdb_brlock(tdb, TRANSACTION_LOCK, ltype,
				     F_SETLKW, 0, 1) == -1) {
		TDB_LOG((tdb, TDB_DEBUG_ERROR, "tdb_transaction_lock: failed to get transaction lock\n"));
		tdb->ecode = TDB_ERR_LOCK;
		return -1;
	}
	tdb->have_transaction_lock = 1;
	return 0;
}

int tdb_transaction_unlock(struct tdb_context *tdb)
{
	int ret;
	if (!tdb->have_transaction_lock) {
		return 0;
	}
	ret = tdb->methods->tdb_brlock(tdb, TRANSACTION_LOCK, F_UNLCK, F_SETLKW, 0, 1);
	if (ret == 0) {
		tdb->have_transaction_lock = 0;
	}
	return ret;
}




static int _tdb_lockall(struct tdb_context *tdb, int ltype, int op)
{
	bool mark_lock = ((ltype & TDB_MARK_LOCK) == TDB_MARK_LOCK);

	ltype &= ~TDB_MARK_LOCK;

	
	if (tdb->read_only || tdb->traverse_read)
		return TDB_ERRCODE(TDB_ERR_LOCK, -1);

	if (tdb->global_lock.count && tdb->global_lock.ltype == ltype) {
		tdb->global_lock.count++;
		return 0;
	}

	if (tdb->global_lock.count) {
		
		return TDB_ERRCODE(TDB_ERR_LOCK, -1);
	}

	if (tdb->num_locks != 0) {
		
		return TDB_ERRCODE(TDB_ERR_LOCK, -1);
	}

	if (!mark_lock &&
	    tdb->methods->tdb_brlock(tdb, FREELIST_TOP, ltype, op,
				     0, 4*tdb->header.hash_size)) {
		if (op == F_SETLKW) {
			TDB_LOG((tdb, TDB_DEBUG_ERROR, "tdb_lockall failed (%s)\n", strerror(errno)));
		}
		return -1;
	}

	tdb->global_lock.count = 1;
	tdb->global_lock.ltype = ltype;

	return 0;
}



static int _tdb_unlockall(struct tdb_context *tdb, int ltype)
{
	bool mark_lock = ((ltype & TDB_MARK_LOCK) == TDB_MARK_LOCK);

	ltype &= ~TDB_MARK_LOCK;

	
	if (tdb->read_only || tdb->traverse_read) {
		return TDB_ERRCODE(TDB_ERR_LOCK, -1);
	}

	if (tdb->global_lock.ltype != ltype || tdb->global_lock.count == 0) {
		return TDB_ERRCODE(TDB_ERR_LOCK, -1);
	}

	if (tdb->global_lock.count > 1) {
		tdb->global_lock.count--;
		return 0;
	}

	if (!mark_lock &&
	    tdb->methods->tdb_brlock(tdb, FREELIST_TOP, F_UNLCK, F_SETLKW,
				     0, 4*tdb->header.hash_size)) {
		TDB_LOG((tdb, TDB_DEBUG_ERROR, "tdb_unlockall failed (%s)\n", strerror(errno)));
		return -1;
	}

	tdb->global_lock.count = 0;
	tdb->global_lock.ltype = 0;

	return 0;
}

int tdb_lockall(struct tdb_context *tdb)
{
	return _tdb_lockall(tdb, F_WRLCK, F_SETLKW);
}

int tdb_lockall_mark(struct tdb_context *tdb)
{
	return _tdb_lockall(tdb, F_WRLCK | TDB_MARK_LOCK, F_SETLKW);
}

int tdb_lockall_unmark(struct tdb_context *tdb)
{
	return _tdb_unlockall(tdb, F_WRLCK | TDB_MARK_LOCK);
}

int tdb_lockall_nonblock(struct tdb_context *tdb)
{
	return _tdb_lockall(tdb, F_WRLCK, F_SETLK);
}

int tdb_unlockall(struct tdb_context *tdb)
{
	return _tdb_unlockall(tdb, F_WRLCK);
}

int tdb_lockall_read(struct tdb_context *tdb)
{
	return _tdb_lockall(tdb, F_RDLCK, F_SETLKW);
}

int tdb_lockall_read_nonblock(struct tdb_context *tdb)
{
	return _tdb_lockall(tdb, F_RDLCK, F_SETLK);
}

int tdb_unlockall_read(struct tdb_context *tdb)
{
	return _tdb_unlockall(tdb, F_RDLCK);
}

int tdb_chainlock(struct tdb_context *tdb, TDB_DATA key)
{
	return tdb_lock(tdb, BUCKET(tdb->hash_fn(&key)), F_WRLCK);
}

int tdb_chainlock_nonblock(struct tdb_context *tdb, TDB_DATA key)
{
	return tdb_lock_nonblock(tdb, BUCKET(tdb->hash_fn(&key)), F_WRLCK);
}

int tdb_chainlock_mark(struct tdb_context *tdb, TDB_DATA key)
{
	return tdb_lock(tdb, BUCKET(tdb->hash_fn(&key)), F_WRLCK | TDB_MARK_LOCK);
}

int tdb_chainlock_unmark(struct tdb_context *tdb, TDB_DATA key)
{
	return tdb_unlock(tdb, BUCKET(tdb->hash_fn(&key)), F_WRLCK | TDB_MARK_LOCK);
}

int tdb_chainunlock(struct tdb_context *tdb, TDB_DATA key)
{
	return tdb_unlock(tdb, BUCKET(tdb->hash_fn(&key)), F_WRLCK);
}

int tdb_chainlock_read(struct tdb_context *tdb, TDB_DATA key)
{
	return tdb_lock(tdb, BUCKET(tdb->hash_fn(&key)), F_RDLCK);
}

int tdb_chainunlock_read(struct tdb_context *tdb, TDB_DATA key)
{
	return tdb_unlock(tdb, BUCKET(tdb->hash_fn(&key)), F_RDLCK);
}



int tdb_lock_record(struct tdb_context *tdb, tdb_off_t off)
{
	return off ? tdb->methods->tdb_brlock(tdb, off, F_RDLCK, F_SETLKW, 0, 1) : 0;
}

int tdb_write_lock_record(struct tdb_context *tdb, tdb_off_t off)
{
	struct tdb_traverse_lock *i;
	for (i = &tdb->travlocks; i; i = i->next)
		if (i->off == off)
			return -1;
	return tdb->methods->tdb_brlock(tdb, off, F_WRLCK, F_SETLK, 1, 1);
}

int tdb_write_unlock_record(struct tdb_context *tdb, tdb_off_t off)
{
	return tdb->methods->tdb_brlock(tdb, off, F_UNLCK, F_SETLK, 0, 1);
}

int tdb_unlock_record(struct tdb_context *tdb, tdb_off_t off)
{
	struct tdb_traverse_lock *i;
	u32 count = 0;

	if (off == 0)
		return 0;
	for (i = &tdb->travlocks; i; i = i->next)
		if (i->off == off)
			count++;
	return (count == 1 ? tdb->methods->tdb_brlock(tdb, off, F_UNLCK, F_SETLKW, 0, 1) : 0);
}


static int tdb_oob(struct tdb_context *tdb, tdb_off_t len, int probe)
{
	struct stat st;
	if (len <= tdb->map_size)
		return 0;
	if (tdb->flags & TDB_INTERNAL) {
		if (!probe) {
			
			tdb->ecode = TDB_ERR_IO;
			TDB_LOG((tdb, TDB_DEBUG_FATAL,"tdb_oob len %d beyond internal malloc size %d\n",
				 (int)len, (int)tdb->map_size));
		}
		return TDB_ERRCODE(TDB_ERR_IO, -1);
	}

	if (fstat(tdb->fd, &st) == -1) {
		return TDB_ERRCODE(TDB_ERR_IO, -1);
	}

	if (st.st_size < (size_t)len) {
		if (!probe) {
			
			tdb->ecode = TDB_ERR_IO;
			TDB_LOG((tdb, TDB_DEBUG_FATAL,"tdb_oob len %d beyond eof at %d\n",
				 (int)len, (int)st.st_size));
		}
		return TDB_ERRCODE(TDB_ERR_IO, -1);
	}

	
	if (tdb_munmap(tdb) == -1)
		return TDB_ERRCODE(TDB_ERR_IO, -1);
	tdb->map_size = st.st_size;
	tdb_mmap(tdb);
	return 0;
}

static int tdb_write(struct tdb_context *tdb, tdb_off_t off,
		     const void *buf, tdb_len_t len)
{
	if (len == 0) {
		return 0;
	}

	if (tdb->read_only || tdb->traverse_read) {
		tdb->ecode = TDB_ERR_RDONLY;
		return -1;
	}

	if (tdb->methods->tdb_oob(tdb, off + len, 0) != 0)
		return -1;

	if (tdb->map_ptr) {
		memcpy(off + (char *)tdb->map_ptr, buf, len);
	} else if (pwrite(tdb->fd, buf, len, off) != (ssize_t)len) {
		
		tdb->ecode = TDB_ERR_IO;
		TDB_LOG((tdb, TDB_DEBUG_FATAL,"tdb_write failed at %d len=%d (%s)\n",
			   off, len, strerror(errno)));
		return TDB_ERRCODE(TDB_ERR_IO, -1);
	}
	return 0;
}

void *tdb_convert(void *buf, u32 size)
{
	u32 i, *p = (u32 *)buf;
	for (i = 0; i < size / 4; i++)
		p[i] = TDB_BYTEREV(p[i]);
	return buf;
}


static int tdb_read(struct tdb_context *tdb, tdb_off_t off, void *buf,
		    tdb_len_t len, int cv)
{
	if (tdb->methods->tdb_oob(tdb, off + len, 0) != 0) {
		return -1;
	}

	if (tdb->map_ptr) {
		memcpy(buf, off + (char *)tdb->map_ptr, len);
	} else {
		ssize_t ret = pread(tdb->fd, buf, len, off);
		if (ret != (ssize_t)len) {
			
			tdb->ecode = TDB_ERR_IO;
			TDB_LOG((tdb, TDB_DEBUG_FATAL,"tdb_read failed at %d "
				 "len=%d ret=%d (%s) map_size=%d\n",
				 (int)off, (int)len, (int)ret, strerror(errno),
				 (int)tdb->map_size));
			return TDB_ERRCODE(TDB_ERR_IO, -1);
		}
	}
	if (cv) {
		tdb_convert(buf, len);
	}
	return 0;
}



static void tdb_next_hash_chain(struct tdb_context *tdb, u32 *chain)
{
	u32 h = *chain;
	if (tdb->map_ptr) {
		for (;h < tdb->header.hash_size;h++) {
			if (0 != *(u32 *)(TDB_HASH_TOP(h) + (unsigned char *)tdb->map_ptr)) {
				break;
			}
		}
	} else {
		u32 off=0;
		for (;h < tdb->header.hash_size;h++) {
			if (tdb_ofs_read(tdb, TDB_HASH_TOP(h), &off) != 0 || off != 0) {
				break;
			}
		}
	}
	(*chain) = h;
}


int tdb_munmap(struct tdb_context *tdb)
{
	if (tdb->flags & TDB_INTERNAL)
		return 0;

#ifdef HAVE_MMAP
	if (tdb->map_ptr) {
		int ret = munmap(tdb->map_ptr, tdb->map_size);
		if (ret != 0)
			return ret;
	}
#endif
	tdb->map_ptr = NULL;
	return 0;
}

void tdb_mmap(struct tdb_context *tdb)
{
	if (tdb->flags & TDB_INTERNAL)
		return;

#ifdef HAVE_MMAP
	if (!(tdb->flags & TDB_NOMMAP)) {
		tdb->map_ptr = mmap(NULL, tdb->map_size,
				    PROT_READ|(tdb->read_only? 0:PROT_WRITE),
				    MAP_SHARED|MAP_FILE, tdb->fd, 0);


		if (tdb->map_ptr == MAP_FAILED) {
			tdb->map_ptr = NULL;
			TDB_LOG((tdb, TDB_DEBUG_WARNING, "tdb_mmap failed for size %d (%s)\n",
				 tdb->map_size, strerror(errno)));
		}
	} else {
		tdb->map_ptr = NULL;
	}
#else
	tdb->map_ptr = NULL;
#endif
}

static int tdb_expand_file(struct tdb_context *tdb, tdb_off_t size, tdb_off_t addition)
{
	char buf[1024];

	if (tdb->read_only || tdb->traverse_read) {
		tdb->ecode = TDB_ERR_RDONLY;
		return -1;
	}

	if (ftruncate(tdb->fd, size+addition) == -1) {
		char b = 0;
		if (pwrite(tdb->fd,  &b, 1, (size+addition) - 1) != 1) {
			TDB_LOG((tdb, TDB_DEBUG_FATAL, "expand_file to %d failed (%s)\n",
				 size+addition, strerror(errno)));
			return -1;
		}
	}

	memset(buf, TDB_PAD_BYTE, sizeof(buf));
	while (addition) {
		int n = addition>sizeof(buf)?sizeof(buf):addition;
		int ret = pwrite(tdb->fd, buf, n, size);
		if (ret != n) {
			TDB_LOG((tdb, TDB_DEBUG_FATAL, "expand_file write of %d failed (%s)\n",
				   n, strerror(errno)));
			return -1;
		}
		addition -= n;
		size += n;
	}
	return 0;
}


int tdb_expand(struct tdb_context *tdb, tdb_off_t size)
{
	struct list_struct rec;
	tdb_off_t offset;

	if (tdb_lock(tdb, -1, F_WRLCK) == -1) {
		TDB_LOG((tdb, TDB_DEBUG_ERROR, "lock failed in tdb_expand\n"));
		return -1;
	}

	
	tdb->methods->tdb_oob(tdb, tdb->map_size + 1, 1);

	size = TDB_ALIGN(tdb->map_size + size*10, tdb->page_size) - tdb->map_size;

	if (!(tdb->flags & TDB_INTERNAL))
		tdb_munmap(tdb);


	
	if (!(tdb->flags & TDB_INTERNAL)) {
		if (tdb->methods->tdb_expand_file(tdb, tdb->map_size, size) != 0)
			goto fail;
	}

	tdb->map_size += size;

	if (tdb->flags & TDB_INTERNAL) {
		char *new_map_ptr = (char *)realloc(tdb->map_ptr,
						    tdb->map_size);
		if (!new_map_ptr) {
			tdb->map_size -= size;
			goto fail;
		}
		tdb->map_ptr = new_map_ptr;
	} else {

		
		tdb_mmap(tdb);
	}

	
	memset(&rec,'\0',sizeof(rec));
	rec.rec_len = size - sizeof(rec);

	
	offset = tdb->map_size - size;
	if (tdb_free(tdb, offset, &rec) == -1)
		goto fail;

	tdb_unlock(tdb, -1, F_WRLCK);
	return 0;
 fail:
	tdb_unlock(tdb, -1, F_WRLCK);
	return -1;
}

int tdb_ofs_read(struct tdb_context *tdb, tdb_off_t offset, tdb_off_t *d)
{
	return tdb->methods->tdb_read(tdb, offset, (char*)d, sizeof(*d), DOCONV());
}

int tdb_ofs_write(struct tdb_context *tdb, tdb_off_t offset, tdb_off_t *d)
{
	tdb_off_t off = *d;
	return tdb->methods->tdb_write(tdb, offset, CONVERT(off), sizeof(*d));
}


unsigned char *tdb_alloc_read(struct tdb_context *tdb, tdb_off_t offset, tdb_len_t len)
{
	unsigned char *buf;

	
	if (len == 0) {
		len = 1;
	}

	if (!(buf = (unsigned char *)malloc(len))) {
		
		tdb->ecode = TDB_ERR_OOM;
		TDB_LOG((tdb, TDB_DEBUG_ERROR,"tdb_alloc_read malloc failed len=%d (%s)\n",
			   len, strerror(errno)));
		return TDB_ERRCODE(TDB_ERR_OOM, buf);
	}
	if (tdb->methods->tdb_read(tdb, offset, buf, len, 0) == -1) {
		SAFE_FREE(buf);
		return NULL;
	}
	return buf;
}


int tdb_parse_data(struct tdb_context *tdb, TDB_DATA key,
		   tdb_off_t offset, tdb_len_t len,
		   int (*parser)(TDB_DATA key, TDB_DATA data,
				 void *private_data),
		   void *private_data)
{
	TDB_DATA data;
	int result;

	data.dsize = len;

	if ((tdb->transaction == NULL) && (tdb->map_ptr != NULL)) {
		if (tdb->methods->tdb_oob(tdb, offset+len, 0) != 0) {
			return -1;
		}
		data.dptr = offset + (unsigned char *)tdb->map_ptr;
		return parser(key, data, private_data);
	}

	if (!(data.dptr = tdb_alloc_read(tdb, offset, len))) {
		return -1;
	}

	result = parser(key, data, private_data);
	free(data.dptr);
	return result;
}

int tdb_rec_read(struct tdb_context *tdb, tdb_off_t offset, struct list_struct *rec)
{
	if (tdb->methods->tdb_read(tdb, offset, rec, sizeof(*rec),DOCONV()) == -1)
		return -1;
	if (TDB_BAD_MAGIC(rec)) {
		
		tdb->ecode = TDB_ERR_CORRUPT;
		TDB_LOG((tdb, TDB_DEBUG_FATAL,"tdb_rec_read bad magic 0x%x at offset=%d\n", rec->magic, offset));
		return TDB_ERRCODE(TDB_ERR_CORRUPT, -1);
	}
	return tdb->methods->tdb_oob(tdb, rec->next+sizeof(*rec), 0);
}

int tdb_rec_write(struct tdb_context *tdb, tdb_off_t offset, struct list_struct *rec)
{
	struct list_struct r = *rec;
	return tdb->methods->tdb_write(tdb, offset, CONVERT(r), sizeof(r));
}

static const struct tdb_methods io_methods = {
	tdb_read,
	tdb_write,
	tdb_next_hash_chain,
	tdb_oob,
	tdb_expand_file,
	tdb_brlock
};

void tdb_io_init(struct tdb_context *tdb)
{
	tdb->methods = &io_methods;
}



struct tdb_transaction_el {
	struct tdb_transaction_el *next, *prev;
	tdb_off_t offset;
	tdb_len_t length;
	unsigned char *data;
};

struct tdb_transaction {
	u32 *hash_heads;

	
	const struct tdb_methods *io_methods;

	struct tdb_transaction_el *elements, *elements_last;

	int transaction_error;

	int nesting;

	
	tdb_len_t old_map_size;
};


static int transaction_read(struct tdb_context *tdb, tdb_off_t off, void *buf,
			    tdb_len_t len, int cv)
{
	struct tdb_transaction_el *el;

	
	for (el=tdb->transaction->elements_last;el;el=el->prev) {
		tdb_len_t partial;

		if (off+len <= el->offset) {
			continue;
		}
		if (off >= el->offset + el->length) {
			continue;
		}

		if (off < el->offset) {
			partial = el->offset - off;
			if (transaction_read(tdb, off, buf, partial, cv) != 0) {
				goto fail;
			}
			len -= partial;
			off += partial;
			buf = (void *)(partial + (char *)buf);
		}
		if (off + len <= el->offset + el->length) {
			partial = len;
		} else {
			partial = el->offset + el->length - off;
		}
		memcpy(buf, el->data + (off - el->offset), partial);
		if (cv) {
			tdb_convert(buf, len);
		}
		len -= partial;
		off += partial;
		buf = (void *)(partial + (char *)buf);

		if (len != 0 && transaction_read(tdb, off, buf, len, cv) != 0) {
			goto fail;
		}

		return 0;
	}

	
	return tdb->transaction->io_methods->tdb_read(tdb, off, buf, len, cv);

fail:
	TDB_LOG((tdb, TDB_DEBUG_FATAL, "transaction_read: failed at off=%d len=%d\n", off, len));
	tdb->ecode = TDB_ERR_IO;
	tdb->transaction->transaction_error = 1;
	return -1;
}


static int transaction_write(struct tdb_context *tdb, tdb_off_t off,
			     const void *buf, tdb_len_t len)
{
	struct tdb_transaction_el *el, *best_el=NULL;

	if (len == 0) {
		return 0;
	}

	if (len == sizeof(tdb_off_t) && off >= FREELIST_TOP &&
	    off < FREELIST_TOP+TDB_HASHTABLE_SIZE(tdb)) {
		u32 chain = (off-FREELIST_TOP) / sizeof(tdb_off_t);
		memcpy(&tdb->transaction->hash_heads[chain], buf, len);
	}

	
	for (el=tdb->transaction->elements_last;el;el=el->prev) {
		tdb_len_t partial;

		if (best_el == NULL && off == el->offset+el->length) {
			best_el = el;
		}

		if (off+len <= el->offset) {
			continue;
		}
		if (off >= el->offset + el->length) {
			continue;
		}

		if (off < el->offset) {
			partial = el->offset - off;
			if (transaction_write(tdb, off, buf, partial) != 0) {
				goto fail;
			}
			len -= partial;
			off += partial;
			buf = (const void *)(partial + (const char *)buf);
		}
		if (off + len <= el->offset + el->length) {
			partial = len;
		} else {
			partial = el->offset + el->length - off;
		}
		memcpy(el->data + (off - el->offset), buf, partial);
		len -= partial;
		off += partial;
		buf = (const void *)(partial + (const char *)buf);

		if (len != 0 && transaction_write(tdb, off, buf, len) != 0) {
			goto fail;
		}

		return 0;
	}

	
	if (best_el && best_el->offset + best_el->length == off &&
	    (off+len < tdb->transaction->old_map_size ||
	     off > tdb->transaction->old_map_size)) {
		unsigned char *data = best_el->data;
		el = best_el;
		el->data = (unsigned char *)realloc(el->data,
						    el->length + len);
		if (el->data == NULL) {
			tdb->ecode = TDB_ERR_OOM;
			tdb->transaction->transaction_error = 1;
			el->data = data;
			return -1;
		}
		if (buf) {
			memcpy(el->data + el->length, buf, len);
		} else {
			memset(el->data + el->length, TDB_PAD_BYTE, len);
		}
		el->length += len;
		return 0;
	}

	
	el = (struct tdb_transaction_el *)malloc(sizeof(*el));
	if (el == NULL) {
		tdb->ecode = TDB_ERR_OOM;
		tdb->transaction->transaction_error = 1;
		return -1;
	}
	el->next = NULL;
	el->prev = tdb->transaction->elements_last;
	el->offset = off;
	el->length = len;
	el->data = (unsigned char *)malloc(len);
	if (el->data == NULL) {
		free(el);
		tdb->ecode = TDB_ERR_OOM;
		tdb->transaction->transaction_error = 1;
		return -1;
	}
	if (buf) {
		memcpy(el->data, buf, len);
	} else {
		memset(el->data, TDB_PAD_BYTE, len);
	}
	if (el->prev) {
		el->prev->next = el;
	} else {
		tdb->transaction->elements = el;
	}
	tdb->transaction->elements_last = el;
	return 0;

fail:
	TDB_LOG((tdb, TDB_DEBUG_FATAL, "transaction_write: failed at off=%d len=%d\n", off, len));
	tdb->ecode = TDB_ERR_IO;
	tdb->transaction->transaction_error = 1;
	return -1;
}

static void transaction_next_hash_chain(struct tdb_context *tdb, u32 *chain)
{
	u32 h = *chain;
	for (;h < tdb->header.hash_size;h++) {
		
		if (0 != tdb->transaction->hash_heads[h+1]) {
			break;
		}
	}
	(*chain) = h;
}

static int transaction_oob(struct tdb_context *tdb, tdb_off_t len, int probe)
{
	if (len <= tdb->map_size) {
		return 0;
	}
	return TDB_ERRCODE(TDB_ERR_IO, -1);
}

static int transaction_expand_file(struct tdb_context *tdb, tdb_off_t size,
				   tdb_off_t addition)
{
	if (transaction_write(tdb, size, NULL, addition) != 0) {
		return -1;
	}

	return 0;
}

static int transaction_brlock(struct tdb_context *tdb, tdb_off_t offset,
			      int rw_type, int lck_type, int probe, size_t len)
{
	return 0;
}

static const struct tdb_methods transaction_methods = {
	transaction_read,
	transaction_write,
	transaction_next_hash_chain,
	transaction_oob,
	transaction_expand_file,
	transaction_brlock
};


int tdb_transaction_start(struct tdb_context *tdb)
{
	
	if (tdb->read_only || (tdb->flags & TDB_INTERNAL) || tdb->traverse_read) {
		TDB_LOG((tdb, TDB_DEBUG_ERROR, "tdb_transaction_start: cannot start a transaction on a read-only or internal db\n"));
		tdb->ecode = TDB_ERR_EINVAL;
		return -1;
	}

	
	if (tdb->transaction != NULL) {
		tdb->transaction->nesting++;
		TDB_LOG((tdb, TDB_DEBUG_TRACE, "tdb_transaction_start: nesting %d\n",
			 tdb->transaction->nesting));
		return 0;
	}

	if (tdb->num_locks != 0 || tdb->global_lock.count) {
		TDB_LOG((tdb, TDB_DEBUG_ERROR, "tdb_transaction_start: cannot start a transaction with locks held\n"));
		tdb->ecode = TDB_ERR_LOCK;
		return -1;
	}

	if (tdb->travlocks.next != NULL) {
		TDB_LOG((tdb, TDB_DEBUG_ERROR, "tdb_transaction_start: cannot start a transaction within a traverse\n"));
		tdb->ecode = TDB_ERR_LOCK;
		return -1;
	}

	tdb->transaction = (struct tdb_transaction *)
		calloc(sizeof(struct tdb_transaction), 1);
	if (tdb->transaction == NULL) {
		tdb->ecode = TDB_ERR_OOM;
		return -1;
	}

	if (tdb_transaction_lock(tdb, F_WRLCK) == -1) {
		SAFE_FREE(tdb->transaction);
		return -1;
	}

	if (tdb_brlock(tdb, FREELIST_TOP, F_RDLCK, F_SETLKW, 0, 0) == -1) {
		TDB_LOG((tdb, TDB_DEBUG_ERROR, "tdb_transaction_start: failed to get hash locks\n"));
		tdb->ecode = TDB_ERR_LOCK;
		goto fail;
	}

	tdb->transaction->hash_heads = (u32 *)
		calloc(tdb->header.hash_size+1, sizeof(u32));
	if (tdb->transaction->hash_heads == NULL) {
		tdb->ecode = TDB_ERR_OOM;
		goto fail;
	}
	if (tdb->methods->tdb_read(tdb, FREELIST_TOP, tdb->transaction->hash_heads,
				   TDB_HASHTABLE_SIZE(tdb), 0) != 0) {
		TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_transaction_start: failed to read hash heads\n"));
		tdb->ecode = TDB_ERR_IO;
		goto fail;
	}

	tdb->methods->tdb_oob(tdb, tdb->map_size + 1, 1);
	tdb->transaction->old_map_size = tdb->map_size;

	tdb->transaction->io_methods = tdb->methods;
	tdb->methods = &transaction_methods;

	if (transaction_write(tdb, FREELIST_TOP, tdb->transaction->hash_heads,
			      TDB_HASHTABLE_SIZE(tdb)) != 0) {
		TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_transaction_start: failed to prime hash table\n"));
		tdb->ecode = TDB_ERR_IO;
		tdb->methods = tdb->transaction->io_methods;
		goto fail;
	}

	return 0;

fail:
	tdb_brlock(tdb, FREELIST_TOP, F_UNLCK, F_SETLKW, 0, 0);
	tdb_transaction_unlock(tdb);
	SAFE_FREE(tdb->transaction->hash_heads);
	SAFE_FREE(tdb->transaction);
	return -1;
}


int tdb_transaction_cancel(struct tdb_context *tdb)
{
	if (tdb->transaction == NULL) {
		TDB_LOG((tdb, TDB_DEBUG_ERROR, "tdb_transaction_cancel: no transaction\n"));
		return -1;
	}

	if (tdb->transaction->nesting != 0) {
		tdb->transaction->transaction_error = 1;
		tdb->transaction->nesting--;
		return 0;
	}

	tdb->map_size = tdb->transaction->old_map_size;

	
	while (tdb->transaction->elements) {
		struct tdb_transaction_el *el = tdb->transaction->elements;
		tdb->transaction->elements = el->next;
		free(el->data);
		free(el);
	}

	
	if (tdb->global_lock.count != 0) {
		tdb_brlock(tdb, FREELIST_TOP, F_UNLCK, F_SETLKW, 0, 4*tdb->header.hash_size);
		tdb->global_lock.count = 0;
	}

	
	if (tdb->num_locks != 0) {
		int i;
		for (i=0;i<tdb->num_lockrecs;i++) {
			tdb_brlock(tdb,FREELIST_TOP+4*tdb->lockrecs[i].list,
				   F_UNLCK,F_SETLKW, 0, 1);
		}
		tdb->num_locks = 0;
		tdb->num_lockrecs = 0;
		SAFE_FREE(tdb->lockrecs);
	}

	
	tdb->methods = tdb->transaction->io_methods;

	tdb_brlock(tdb, FREELIST_TOP, F_UNLCK, F_SETLKW, 0, 0);
	tdb_transaction_unlock(tdb);
	SAFE_FREE(tdb->transaction->hash_heads);
	SAFE_FREE(tdb->transaction);

	return 0;
}

static int transaction_sync(struct tdb_context *tdb, tdb_off_t offset, tdb_len_t length)
{
	if (fsync(tdb->fd) != 0) {
		tdb->ecode = TDB_ERR_IO;
		TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_transaction: fsync failed\n"));
		return -1;
	}
#if defined(HAVE_MSYNC) && defined(MS_SYNC)
	if (tdb->map_ptr) {
		tdb_off_t moffset = offset & ~(tdb->page_size-1);
		if (msync(moffset + (char *)tdb->map_ptr,
			  length + (offset - moffset), MS_SYNC) != 0) {
			tdb->ecode = TDB_ERR_IO;
			TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_transaction: msync failed - %s\n",
				 strerror(errno)));
			return -1;
		}
	}
#endif
	return 0;
}


static tdb_len_t tdb_recovery_size(struct tdb_context *tdb)
{
	struct tdb_transaction_el *el;
	tdb_len_t recovery_size = 0;

	recovery_size = sizeof(u32);
	for (el=tdb->transaction->elements;el;el=el->next) {
		if (el->offset >= tdb->transaction->old_map_size) {
			continue;
		}
		recovery_size += 2*sizeof(tdb_off_t) + el->length;
	}

	return recovery_size;
}

static int tdb_recovery_allocate(struct tdb_context *tdb,
				 tdb_len_t *recovery_size,
				 tdb_off_t *recovery_offset,
				 tdb_len_t *recovery_max_size)
{
	struct list_struct rec;
	const struct tdb_methods *methods = tdb->transaction->io_methods;
	tdb_off_t recovery_head;

	if (tdb_ofs_read(tdb, TDB_RECOVERY_HEAD, &recovery_head) == -1) {
		TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_recovery_allocate: failed to read recovery head\n"));
		return -1;
	}

	rec.rec_len = 0;

	if (recovery_head != 0 &&
	    methods->tdb_read(tdb, recovery_head, &rec, sizeof(rec), DOCONV()) == -1) {
		TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_recovery_allocate: failed to read recovery record\n"));
		return -1;
	}

	*recovery_size = tdb_recovery_size(tdb);

	if (recovery_head != 0 && *recovery_size <= rec.rec_len) {
		
		*recovery_max_size = rec.rec_len;
		*recovery_offset = recovery_head;
		return 0;
	}

	if (recovery_head != 0) {
		if (tdb_free(tdb, recovery_head, &rec) == -1) {
			TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_recovery_allocate: failed to free previous recovery area\n"));
			return -1;
		}
	}

	
	*recovery_size = tdb_recovery_size(tdb);

	
	*recovery_max_size = TDB_ALIGN(sizeof(rec) + *recovery_size, tdb->page_size) - sizeof(rec);
	*recovery_offset = tdb->map_size;
	recovery_head = *recovery_offset;

	if (methods->tdb_expand_file(tdb, tdb->transaction->old_map_size,
				     (tdb->map_size - tdb->transaction->old_map_size) +
				     sizeof(rec) + *recovery_max_size) == -1) {
		TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_recovery_allocate: failed to create recovery area\n"));
		return -1;
	}

	
	methods->tdb_oob(tdb, tdb->map_size + 1, 1);

	tdb->transaction->old_map_size = tdb->map_size;

	CONVERT(recovery_head);
	if (methods->tdb_write(tdb, TDB_RECOVERY_HEAD,
			       &recovery_head, sizeof(tdb_off_t)) == -1) {
		TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_recovery_allocate: failed to write recovery head\n"));
		return -1;
	}

	return 0;
}


static int transaction_setup_recovery(struct tdb_context *tdb,
				      tdb_off_t *magic_offset)
{
	struct tdb_transaction_el *el;
	tdb_len_t recovery_size;
	unsigned char *data, *p;
	const struct tdb_methods *methods = tdb->transaction->io_methods;
	struct list_struct *rec;
	tdb_off_t recovery_offset, recovery_max_size;
	tdb_off_t old_map_size = tdb->transaction->old_map_size;
	u32 magic, tailer;

	if (tdb_recovery_allocate(tdb, &recovery_size,
				  &recovery_offset, &recovery_max_size) == -1) {
		return -1;
	}

	data = (unsigned char *)malloc(recovery_size + sizeof(*rec));
	if (data == NULL) {
		tdb->ecode = TDB_ERR_OOM;
		return -1;
	}

	rec = (struct list_struct *)data;
	memset(rec, 0, sizeof(*rec));

	rec->magic    = 0;
	rec->data_len = recovery_size;
	rec->rec_len  = recovery_max_size;
	rec->key_len  = old_map_size;
	CONVERT(rec);

	p = data + sizeof(*rec);
	for (el=tdb->transaction->elements;el;el=el->next) {
		if (el->offset >= old_map_size) {
			continue;
		}
		if (el->offset + el->length > tdb->transaction->old_map_size) {
			TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_transaction_setup_recovery: transaction data over new region boundary\n"));
			free(data);
			tdb->ecode = TDB_ERR_CORRUPT;
			return -1;
		}
		memcpy(p, &el->offset, 4);
		memcpy(p+4, &el->length, 4);
		if (DOCONV()) {
			tdb_convert(p, 8);
		}
		if (methods->tdb_read(tdb, el->offset, p + 8, el->length, 0) != 0) {
			free(data);
			tdb->ecode = TDB_ERR_IO;
			return -1;
		}
		p += 8 + el->length;
	}

	
	tailer = sizeof(*rec) + recovery_max_size;
	memcpy(p, &tailer, 4);
	CONVERT(p);

	
	if (methods->tdb_write(tdb, recovery_offset, data, sizeof(*rec) + recovery_size) == -1) {
		TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_transaction_setup_recovery: failed to write recovery data\n"));
		free(data);
		tdb->ecode = TDB_ERR_IO;
		return -1;
	}

	if (transaction_sync(tdb, recovery_offset, sizeof(*rec) + recovery_size) == -1) {
		free(data);
		return -1;
	}

	free(data);

	magic = TDB_RECOVERY_MAGIC;
	CONVERT(magic);

	*magic_offset = recovery_offset + offsetof(struct list_struct, magic);

	if (methods->tdb_write(tdb, *magic_offset, &magic, sizeof(magic)) == -1) {
		TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_transaction_setup_recovery: failed to write recovery magic\n"));
		tdb->ecode = TDB_ERR_IO;
		return -1;
	}

	
	if (transaction_sync(tdb, *magic_offset, sizeof(magic)) == -1) {
		return -1;
	}

	return 0;
}

int tdb_transaction_commit(struct tdb_context *tdb)
{
	const struct tdb_methods *methods;
	tdb_off_t magic_offset = 0;
	u32 zero = 0;

	if (tdb->transaction == NULL) {
		TDB_LOG((tdb, TDB_DEBUG_ERROR, "tdb_transaction_commit: no transaction\n"));
		return -1;
	}

	if (tdb->transaction->transaction_error) {
		tdb->ecode = TDB_ERR_IO;
		tdb_transaction_cancel(tdb);
		TDB_LOG((tdb, TDB_DEBUG_ERROR, "tdb_transaction_commit: transaction error pending\n"));
		return -1;
	}

	if (tdb->transaction->nesting != 0) {
		tdb->transaction->nesting--;
		return 0;
	}

	
	if (tdb->transaction->elements == NULL) {
		tdb_transaction_cancel(tdb);
		return 0;
	}

	methods = tdb->transaction->io_methods;

	if (tdb->num_locks || tdb->global_lock.count) {
		tdb->ecode = TDB_ERR_LOCK;
		TDB_LOG((tdb, TDB_DEBUG_ERROR, "tdb_transaction_commit: locks pending on commit\n"));
		tdb_transaction_cancel(tdb);
		return -1;
	}

	
	if (tdb_brlock_upgrade(tdb, FREELIST_TOP, 0) == -1) {
		TDB_LOG((tdb, TDB_DEBUG_ERROR, "tdb_transaction_start: failed to upgrade hash locks\n"));
		tdb->ecode = TDB_ERR_LOCK;
		tdb_transaction_cancel(tdb);
		return -1;
	}

	if (tdb_brlock(tdb, GLOBAL_LOCK, F_WRLCK, F_SETLKW, 0, 1) == -1) {
		TDB_LOG((tdb, TDB_DEBUG_ERROR, "tdb_transaction_commit: failed to get global lock\n"));
		tdb->ecode = TDB_ERR_LOCK;
		tdb_transaction_cancel(tdb);
		return -1;
	}

	if (!(tdb->flags & TDB_NOSYNC)) {
		
		if (transaction_setup_recovery(tdb, &magic_offset) == -1) {
			TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_transaction_commit: failed to setup recovery data\n"));
			tdb_brlock(tdb, GLOBAL_LOCK, F_UNLCK, F_SETLKW, 0, 1);
			tdb_transaction_cancel(tdb);
			return -1;
		}
	}

	
	if (tdb->map_size != tdb->transaction->old_map_size) {
		if (methods->tdb_expand_file(tdb, tdb->transaction->old_map_size,
					     tdb->map_size -
					     tdb->transaction->old_map_size) == -1) {
			tdb->ecode = TDB_ERR_IO;
			TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_transaction_commit: expansion failed\n"));
			tdb_brlock(tdb, GLOBAL_LOCK, F_UNLCK, F_SETLKW, 0, 1);
			tdb_transaction_cancel(tdb);
			return -1;
		}
		tdb->map_size = tdb->transaction->old_map_size;
		methods->tdb_oob(tdb, tdb->map_size + 1, 1);
	}

	
	while (tdb->transaction->elements) {
		struct tdb_transaction_el *el = tdb->transaction->elements;

		if (methods->tdb_write(tdb, el->offset, el->data, el->length) == -1) {
			TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_transaction_commit: write failed during commit\n"));

			/* we've overwritten part of the data and
			   possibly expanded the file, so we need to
			   run the crash recovery code */
			tdb->methods = methods;
			tdb_transaction_recover(tdb);

			tdb_transaction_cancel(tdb);
			tdb_brlock(tdb, GLOBAL_LOCK, F_UNLCK, F_SETLKW, 0, 1);

			TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_transaction_commit: write failed\n"));
			return -1;
		}
		tdb->transaction->elements = el->next;
		free(el->data);
		free(el);
	}

	if (!(tdb->flags & TDB_NOSYNC)) {
		
		if (transaction_sync(tdb, 0, tdb->map_size) == -1) {
			return -1;
		}

		
		if (methods->tdb_write(tdb, magic_offset, &zero, 4) == -1) {
			TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_transaction_commit: failed to remove recovery magic\n"));
			return -1;
		}

		
		if (transaction_sync(tdb, magic_offset, 4) == -1) {
			return -1;
		}
	}

	tdb_brlock(tdb, GLOBAL_LOCK, F_UNLCK, F_SETLKW, 0, 1);


#ifdef HAVE_UTIME
	utime(tdb->name, NULL);
#endif

	tdb_transaction_cancel(tdb);
	return 0;
}


int tdb_transaction_recover(struct tdb_context *tdb)
{
	tdb_off_t recovery_head, recovery_eof;
	unsigned char *data, *p;
	u32 zero = 0;
	struct list_struct rec;

	
	if (tdb_ofs_read(tdb, TDB_RECOVERY_HEAD, &recovery_head) == -1) {
		TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_transaction_recover: failed to read recovery head\n"));
		tdb->ecode = TDB_ERR_IO;
		return -1;
	}

	if (recovery_head == 0) {
		
		return 0;
	}

	
	if (tdb->methods->tdb_read(tdb, recovery_head, &rec,
				   sizeof(rec), DOCONV()) == -1) {
		TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_transaction_recover: failed to read recovery record\n"));
		tdb->ecode = TDB_ERR_IO;
		return -1;
	}

	if (rec.magic != TDB_RECOVERY_MAGIC) {
		
		return 0;
	}

	if (tdb->read_only) {
		TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_transaction_recover: attempt to recover read only database\n"));
		tdb->ecode = TDB_ERR_CORRUPT;
		return -1;
	}

	recovery_eof = rec.key_len;

	data = (unsigned char *)malloc(rec.data_len);
	if (data == NULL) {
		TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_transaction_recover: failed to allocate recovery data\n"));
		tdb->ecode = TDB_ERR_OOM;
		return -1;
	}

	
	if (tdb->methods->tdb_read(tdb, recovery_head + sizeof(rec), data,
				   rec.data_len, 0) == -1) {
		TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_transaction_recover: failed to read recovery data\n"));
		tdb->ecode = TDB_ERR_IO;
		return -1;
	}

	
	p = data;
	while (p+8 < data + rec.data_len) {
		u32 ofs, len;
		if (DOCONV()) {
			tdb_convert(p, 8);
		}
		memcpy(&ofs, p, 4);
		memcpy(&len, p+4, 4);

		if (tdb->methods->tdb_write(tdb, ofs, p+8, len) == -1) {
			free(data);
			TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_transaction_recover: failed to recover %d bytes at offset %d\n", len, ofs));
			tdb->ecode = TDB_ERR_IO;
			return -1;
		}
		p += 8 + len;
	}

	free(data);

	if (transaction_sync(tdb, 0, tdb->map_size) == -1) {
		TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_transaction_recover: failed to sync recovery\n"));
		tdb->ecode = TDB_ERR_IO;
		return -1;
	}

	
	if (recovery_eof <= recovery_head) {
		if (tdb_ofs_write(tdb, TDB_RECOVERY_HEAD, &zero) == -1) {
			TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_transaction_recover: failed to remove recovery head\n"));
			tdb->ecode = TDB_ERR_IO;
			return -1;
		}
	}

	
	if (tdb_ofs_write(tdb, recovery_head + offsetof(struct list_struct, magic),
			  &zero) == -1) {
		TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_transaction_recover: failed to remove recovery magic\n"));
		tdb->ecode = TDB_ERR_IO;
		return -1;
	}

	
	tdb_munmap(tdb);
	if (ftruncate(tdb->fd, recovery_eof) != 0) {
		TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_transaction_recover: failed to reduce to recovery size\n"));
		tdb->ecode = TDB_ERR_IO;
		return -1;
	}
	tdb->map_size = recovery_eof;
	tdb_mmap(tdb);

	if (transaction_sync(tdb, 0, recovery_eof) == -1) {
		TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_transaction_recover: failed to sync2 recovery\n"));
		tdb->ecode = TDB_ERR_IO;
		return -1;
	}

	TDB_LOG((tdb, TDB_DEBUG_TRACE, "tdb_transaction_recover: recovered %d byte database\n",
		 recovery_eof));

	
	return 0;
}


static int tdb_rec_free_read(struct tdb_context *tdb, tdb_off_t off, struct list_struct *rec)
{
	if (tdb->methods->tdb_read(tdb, off, rec, sizeof(*rec),DOCONV()) == -1)
		return -1;

	if (rec->magic == TDB_MAGIC) {
		TDB_LOG((tdb, TDB_DEBUG_WARNING, "tdb_rec_free_read non-free magic 0x%x at offset=%d - fixing\n",
			 rec->magic, off));
		rec->magic = TDB_FREE_MAGIC;
		if (tdb->methods->tdb_write(tdb, off, rec, sizeof(*rec)) == -1)
			return -1;
	}

	if (rec->magic != TDB_FREE_MAGIC) {
		
		tdb->ecode = TDB_ERR_CORRUPT;
		TDB_LOG((tdb, TDB_DEBUG_WARNING, "tdb_rec_free_read bad magic 0x%x at offset=%d\n",
			   rec->magic, off));
		return TDB_ERRCODE(TDB_ERR_CORRUPT, -1);
	}
	if (tdb->methods->tdb_oob(tdb, rec->next+sizeof(*rec), 0) != 0)
		return -1;
	return 0;
}



static int remove_from_freelist(struct tdb_context *tdb, tdb_off_t off, tdb_off_t next)
{
	tdb_off_t last_ptr, i;

	
	last_ptr = FREELIST_TOP;
	while (tdb_ofs_read(tdb, last_ptr, &i) != -1 && i != 0) {
		if (i == off) {
			
			return tdb_ofs_write(tdb, last_ptr, &next);
		}
		
		last_ptr = i;
	}
	TDB_LOG((tdb, TDB_DEBUG_FATAL,"remove_from_freelist: not on list at off=%d\n", off));
	return TDB_ERRCODE(TDB_ERR_CORRUPT, -1);
}


static int update_tailer(struct tdb_context *tdb, tdb_off_t offset,
			 const struct list_struct *rec)
{
	tdb_off_t totalsize;

	
	totalsize = sizeof(*rec) + rec->rec_len;
	return tdb_ofs_write(tdb, offset + totalsize - sizeof(tdb_off_t),
			 &totalsize);
}

int tdb_free(struct tdb_context *tdb, tdb_off_t offset, struct list_struct *rec)
{
	tdb_off_t right, left;

	
	if (tdb_lock(tdb, -1, F_WRLCK) != 0)
		return -1;

	
	if (update_tailer(tdb, offset, rec) != 0) {
		TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_free: update_tailer failed!\n"));
		goto fail;
	}

	
	right = offset + sizeof(*rec) + rec->rec_len;
	if (right + sizeof(*rec) <= tdb->map_size) {
		struct list_struct r;

		if (tdb->methods->tdb_read(tdb, right, &r, sizeof(r), DOCONV()) == -1) {
			TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_free: right read failed at %u\n", right));
			goto left;
		}

		
		if (r.magic == TDB_FREE_MAGIC) {
			if (remove_from_freelist(tdb, right, r.next) == -1) {
				TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_free: right free failed at %u\n", right));
				goto left;
			}
			rec->rec_len += sizeof(r) + r.rec_len;
		}
	}

left:
	
	left = offset - sizeof(tdb_off_t);
	if (left > TDB_DATA_START(tdb->header.hash_size)) {
		struct list_struct l;
		tdb_off_t leftsize;

		
		if (tdb_ofs_read(tdb, left, &leftsize) == -1) {
			TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_free: left offset read failed at %u\n", left));
			goto update;
		}

		
		if (leftsize == 0 || leftsize == TDB_PAD_U32) {
			goto update;
		}

		left = offset - leftsize;

		
		if (tdb->methods->tdb_read(tdb, left, &l, sizeof(l), DOCONV()) == -1) {
			TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_free: left read failed at %u (%u)\n", left, leftsize));
			goto update;
		}

		
		if (l.magic == TDB_FREE_MAGIC) {
			if (remove_from_freelist(tdb, left, l.next) == -1) {
				TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_free: left free failed at %u\n", left));
				goto update;
			} else {
				offset = left;
				rec->rec_len += leftsize;
			}
		}
	}

update:
	if (update_tailer(tdb, offset, rec) == -1) {
		TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_free: update_tailer failed at %u\n", offset));
		goto fail;
	}

	
	rec->magic = TDB_FREE_MAGIC;

	if (tdb_ofs_read(tdb, FREELIST_TOP, &rec->next) == -1 ||
	    tdb_rec_write(tdb, offset, rec) == -1 ||
	    tdb_ofs_write(tdb, FREELIST_TOP, &offset) == -1) {
		TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_free record write failed at offset=%d\n", offset));
		goto fail;
	}

	
	tdb_unlock(tdb, -1, F_WRLCK);
	return 0;

 fail:
	tdb_unlock(tdb, -1, F_WRLCK);
	return -1;
}


static tdb_off_t tdb_allocate_ofs(struct tdb_context *tdb, tdb_len_t length, tdb_off_t rec_ptr,
				struct list_struct *rec, tdb_off_t last_ptr)
{
	struct list_struct newrec;
	tdb_off_t newrec_ptr;

	memset(&newrec, '\0', sizeof(newrec));

	
	if (rec->rec_len > length + MIN_REC_SIZE) {
		
		length = TDB_ALIGN(length, TDB_ALIGNMENT);

		
		newrec.rec_len = rec->rec_len - (sizeof(*rec) + length);
		newrec_ptr = rec_ptr + sizeof(*rec) + length;

		
		rec->rec_len = length;
	} else {
		newrec_ptr = 0;
	}

	
	if (tdb_ofs_write(tdb, last_ptr, &rec->next) == -1) {
		return 0;
	}

	rec->magic = TDB_MAGIC;
	if (tdb_rec_write(tdb, rec_ptr, rec) == -1) {
		return 0;
	}

	
	if (newrec_ptr) {
		if (update_tailer(tdb, rec_ptr, rec) == -1) {
			return 0;
		}

		
		if (tdb_free(tdb, newrec_ptr, &newrec) == -1) {
			return 0;
		}
	}

	
	return rec_ptr;
}

tdb_off_t tdb_allocate(struct tdb_context *tdb, tdb_len_t length, struct list_struct *rec)
{
	tdb_off_t rec_ptr, last_ptr, newrec_ptr;
	struct {
		tdb_off_t rec_ptr, last_ptr;
		tdb_len_t rec_len;
	} bestfit;

	if (tdb_lock(tdb, -1, F_WRLCK) == -1)
		return 0;

	
	length += sizeof(tdb_off_t);

 again:
	last_ptr = FREELIST_TOP;

	
	if (tdb_ofs_read(tdb, FREELIST_TOP, &rec_ptr) == -1)
		goto fail;

	bestfit.rec_ptr = 0;
	bestfit.last_ptr = 0;
	bestfit.rec_len = 0;

	while (rec_ptr) {
		if (tdb_rec_free_read(tdb, rec_ptr, rec) == -1) {
			goto fail;
		}

		if (rec->rec_len >= length) {
			if (bestfit.rec_ptr == 0 ||
			    rec->rec_len < bestfit.rec_len) {
				bestfit.rec_len = rec->rec_len;
				bestfit.rec_ptr = rec_ptr;
				bestfit.last_ptr = last_ptr;
				if (bestfit.rec_len < 2*length) {
					break;
				}
			}
		}

		
		last_ptr = rec_ptr;
		rec_ptr = rec->next;
	}

	if (bestfit.rec_ptr != 0) {
		if (tdb_rec_free_read(tdb, bestfit.rec_ptr, rec) == -1) {
			goto fail;
		}

		newrec_ptr = tdb_allocate_ofs(tdb, length, bestfit.rec_ptr, rec, bestfit.last_ptr);
		tdb_unlock(tdb, -1, F_WRLCK);
		return newrec_ptr;
	}

	if (tdb_expand(tdb, length + sizeof(*rec)) == 0)
		goto again;
 fail:
	tdb_unlock(tdb, -1, F_WRLCK);
	return 0;
}



static int seen_insert(struct tdb_context *mem_tdb, tdb_off_t rec_ptr)
{
	TDB_DATA key, data;

	memset(&data, '\0', sizeof(data));
	key.dptr = (unsigned char *)&rec_ptr;
	key.dsize = sizeof(rec_ptr);
	return tdb_store(mem_tdb, key, data, TDB_INSERT);
}

int tdb_validate_freelist(struct tdb_context *tdb, int *pnum_entries)
{
	struct tdb_context *mem_tdb = NULL;
	struct list_struct rec;
	tdb_off_t rec_ptr, last_ptr;
	int ret = -1;

	*pnum_entries = 0;

	mem_tdb = tdb_open("flval", tdb->header.hash_size,
				TDB_INTERNAL, O_RDWR, 0600);
	if (!mem_tdb) {
		return -1;
	}

	if (tdb_lock(tdb, -1, F_WRLCK) == -1) {
		tdb_close(mem_tdb);
		return 0;
	}

	last_ptr = FREELIST_TOP;

	
	if (seen_insert(mem_tdb, last_ptr) == -1) {
		ret = TDB_ERRCODE(TDB_ERR_CORRUPT, -1);
		goto fail;
	}

	
	if (tdb_ofs_read(tdb, FREELIST_TOP, &rec_ptr) == -1) {
		goto fail;
	}

	while (rec_ptr) {


		if (seen_insert(mem_tdb, rec_ptr)) {
			ret = TDB_ERRCODE(TDB_ERR_CORRUPT, -1);
			goto fail;
		}

		if (tdb_rec_free_read(tdb, rec_ptr, &rec) == -1) {
			goto fail;
		}

		
		last_ptr = rec_ptr;
		rec_ptr = rec.next;
		*pnum_entries += 1;
	}

	ret = 0;

  fail:

	tdb_close(mem_tdb);
	tdb_unlock(tdb, -1, F_WRLCK);
	return ret;
}


static int tdb_next_lock(struct tdb_context *tdb, struct tdb_traverse_lock *tlock,
			 struct list_struct *rec)
{
	int want_next = (tlock->off != 0);

	
	for (; tlock->hash < tdb->header.hash_size; tlock->hash++) {
		if (!tlock->off && tlock->hash != 0) {
			tdb->methods->next_hash_chain(tdb, &tlock->hash);
			if (tlock->hash == tdb->header.hash_size) {
				continue;
			}
		}

		if (tdb_lock(tdb, tlock->hash, tlock->lock_rw) == -1)
			return -1;

		
		if (!tlock->off) {
			if (tdb_ofs_read(tdb, TDB_HASH_TOP(tlock->hash),
				     &tlock->off) == -1)
				goto fail;
		} else {
			
			if (tdb_unlock_record(tdb, tlock->off) != 0)
				goto fail;
		}

		if (want_next) {
			
			if (tdb_rec_read(tdb, tlock->off, rec) == -1)
				goto fail;
			tlock->off = rec->next;
		}

		
		while( tlock->off) {
			tdb_off_t current;
			if (tdb_rec_read(tdb, tlock->off, rec) == -1)
				goto fail;

			
			if (tlock->off == rec->next) {
				TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_next_lock: loop detected.\n"));
				goto fail;
			}

			if (!TDB_DEAD(rec)) {
				
				if (tdb_lock_record(tdb, tlock->off) != 0)
					goto fail;
				return tlock->off;
			}

			
			current = tlock->off;
			tlock->off = rec->next;
			if (!(tdb->read_only || tdb->traverse_read) &&
			    tdb_do_delete(tdb, current, rec) != 0)
				goto fail;
		}
		tdb_unlock(tdb, tlock->hash, tlock->lock_rw);
		want_next = 0;
	}
	
	return TDB_ERRCODE(TDB_SUCCESS, 0);

 fail:
	tlock->off = 0;
	if (tdb_unlock(tdb, tlock->hash, tlock->lock_rw) != 0)
		TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_next_lock: On error unlock failed!\n"));
	return -1;
}

static int tdb_traverse_internal(struct tdb_context *tdb,
				 tdb_traverse_func fn, void *private_data,
				 struct tdb_traverse_lock *tl)
{
	TDB_DATA key, dbuf;
	struct list_struct rec;
	int ret, count = 0;

	tl->next = tdb->travlocks.next;

	
	tdb->travlocks.next = tl;

	
	while ((ret = tdb_next_lock(tdb, tl, &rec)) > 0) {
		count++;
		
		key.dptr = tdb_alloc_read(tdb, tl->off + sizeof(rec),
					  rec.key_len + rec.data_len);
		if (!key.dptr) {
			ret = -1;
			if (tdb_unlock(tdb, tl->hash, tl->lock_rw) != 0)
				goto out;
			if (tdb_unlock_record(tdb, tl->off) != 0)
				TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_traverse: key.dptr == NULL and unlock_record failed!\n"));
			goto out;
		}
		key.dsize = rec.key_len;
		dbuf.dptr = key.dptr + rec.key_len;
		dbuf.dsize = rec.data_len;

		
		if (tdb_unlock(tdb, tl->hash, tl->lock_rw) != 0) {
			ret = -1;
			SAFE_FREE(key.dptr);
			goto out;
		}
		if (fn && fn(tdb, key, dbuf, private_data)) {
			
			ret = count;
			if (tdb_unlock_record(tdb, tl->off) != 0) {
				TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_traverse: unlock_record failed!\n"));;
				ret = -1;
			}
			SAFE_FREE(key.dptr);
			goto out;
		}
		SAFE_FREE(key.dptr);
	}
out:
	tdb->travlocks.next = tl->next;
	if (ret < 0)
		return -1;
	else
		return count;
}


int tdb_traverse_read(struct tdb_context *tdb,
		      tdb_traverse_func fn, void *private_data)
{
	struct tdb_traverse_lock tl = { NULL, 0, 0, F_RDLCK };
	int ret;

	if (tdb_transaction_lock(tdb, F_RDLCK)) {
		return -1;
	}

	tdb->traverse_read++;
	ret = tdb_traverse_internal(tdb, fn, private_data, &tl);
	tdb->traverse_read--;

	tdb_transaction_unlock(tdb);

	return ret;
}

int tdb_traverse(struct tdb_context *tdb,
		 tdb_traverse_func fn, void *private_data)
{
	struct tdb_traverse_lock tl = { NULL, 0, 0, F_WRLCK };
	int ret;

	if (tdb->read_only || tdb->traverse_read) {
		return tdb_traverse_read(tdb, fn, private_data);
	}

	if (tdb_transaction_lock(tdb, F_WRLCK)) {
		return -1;
	}

	ret = tdb_traverse_internal(tdb, fn, private_data, &tl);

	tdb_transaction_unlock(tdb);

	return ret;
}


TDB_DATA tdb_firstkey(struct tdb_context *tdb)
{
	TDB_DATA key;
	struct list_struct rec;

	
	if (tdb_unlock_record(tdb, tdb->travlocks.off) != 0)
		return tdb_null;
	tdb->travlocks.off = tdb->travlocks.hash = 0;
	tdb->travlocks.lock_rw = F_RDLCK;

	
	if (tdb_next_lock(tdb, &tdb->travlocks, &rec) <= 0)
		return tdb_null;
	
	key.dsize = rec.key_len;
	key.dptr =tdb_alloc_read(tdb,tdb->travlocks.off+sizeof(rec),key.dsize);

	
	if (tdb_unlock(tdb, tdb->travlocks.hash, tdb->travlocks.lock_rw) != 0)
		TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_firstkey: error occurred while tdb_unlocking!\n"));
	return key;
}

TDB_DATA tdb_nextkey(struct tdb_context *tdb, TDB_DATA oldkey)
{
	u32 oldhash;
	TDB_DATA key = tdb_null;
	struct list_struct rec;
	unsigned char *k = NULL;

	
	if (tdb->travlocks.off) {
		if (tdb_lock(tdb,tdb->travlocks.hash,tdb->travlocks.lock_rw))
			return tdb_null;
		if (tdb_rec_read(tdb, tdb->travlocks.off, &rec) == -1
		    || !(k = tdb_alloc_read(tdb,tdb->travlocks.off+sizeof(rec),
					    rec.key_len))
		    || memcmp(k, oldkey.dptr, oldkey.dsize) != 0) {
			
			if (tdb_unlock_record(tdb, tdb->travlocks.off) != 0) {
				SAFE_FREE(k);
				return tdb_null;
			}
			if (tdb_unlock(tdb, tdb->travlocks.hash, tdb->travlocks.lock_rw) != 0) {
				SAFE_FREE(k);
				return tdb_null;
			}
			tdb->travlocks.off = 0;
		}

		SAFE_FREE(k);
	}

	if (!tdb->travlocks.off) {
		
		tdb->travlocks.off = tdb_find_lock_hash(tdb, oldkey, tdb->hash_fn(&oldkey), tdb->travlocks.lock_rw, &rec);
		if (!tdb->travlocks.off)
			return tdb_null;
		tdb->travlocks.hash = BUCKET(rec.full_hash);
		if (tdb_lock_record(tdb, tdb->travlocks.off) != 0) {
			TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_nextkey: lock_record failed (%s)!\n", strerror(errno)));
			return tdb_null;
		}
	}
	oldhash = tdb->travlocks.hash;

	if (tdb_next_lock(tdb, &tdb->travlocks, &rec) > 0) {
		key.dsize = rec.key_len;
		key.dptr = tdb_alloc_read(tdb, tdb->travlocks.off+sizeof(rec),
					  key.dsize);
		
		if (tdb_unlock(tdb, tdb->travlocks.hash, tdb->travlocks.lock_rw) != 0)
			TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_nextkey: WARNING tdb_unlock failed!\n"));
	}
	
	if (tdb_unlock(tdb, BUCKET(oldhash), tdb->travlocks.lock_rw) != 0)
		TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_nextkey: WARNING tdb_unlock failed!\n"));
	return key;
}


static tdb_off_t tdb_dump_record(struct tdb_context *tdb, int hash,
				 tdb_off_t offset)
{
	struct list_struct rec;
	tdb_off_t tailer_ofs, tailer;

	if (tdb->methods->tdb_read(tdb, offset, (char *)&rec,
				   sizeof(rec), DOCONV()) == -1) {
		printf("ERROR: failed to read record at %u\n", offset);
		return 0;
	}

	printf(" rec: hash=%d offset=0x%08x next=0x%08x rec_len=%d "
	       "key_len=%d data_len=%d full_hash=0x%x magic=0x%x\n",
	       hash, offset, rec.next, rec.rec_len, rec.key_len, rec.data_len,
	       rec.full_hash, rec.magic);

	tailer_ofs = offset + sizeof(rec) + rec.rec_len - sizeof(tdb_off_t);

	if (tdb_ofs_read(tdb, tailer_ofs, &tailer) == -1) {
		printf("ERROR: failed to read tailer at %u\n", tailer_ofs);
		return rec.next;
	}

	if (tailer != rec.rec_len + sizeof(rec)) {
		printf("ERROR: tailer does not match record! tailer=%u totalsize=%u\n",
				(unsigned int)tailer, (unsigned int)(rec.rec_len + sizeof(rec)));
	}
	return rec.next;
}

static int tdb_dump_chain(struct tdb_context *tdb, int i)
{
	tdb_off_t rec_ptr, top;

	top = TDB_HASH_TOP(i);

	if (tdb_lock(tdb, i, F_WRLCK) != 0)
		return -1;

	if (tdb_ofs_read(tdb, top, &rec_ptr) == -1)
		return tdb_unlock(tdb, i, F_WRLCK);

	if (rec_ptr)
		printf("hash=%d\n", i);

	while (rec_ptr) {
		rec_ptr = tdb_dump_record(tdb, i, rec_ptr);
	}

	return tdb_unlock(tdb, i, F_WRLCK);
}

void tdb_dump_all(struct tdb_context *tdb)
{
	int i;
	for (i=0;i<tdb->header.hash_size;i++) {
		tdb_dump_chain(tdb, i);
	}
	printf("freelist:\n");
	tdb_dump_chain(tdb, -1);
}

int tdb_printfreelist(struct tdb_context *tdb)
{
	int ret;
	long total_free = 0;
	tdb_off_t offset, rec_ptr;
	struct list_struct rec;

	if ((ret = tdb_lock(tdb, -1, F_WRLCK)) != 0)
		return ret;

	offset = FREELIST_TOP;

	
	if (tdb_ofs_read(tdb, offset, &rec_ptr) == -1) {
		tdb_unlock(tdb, -1, F_WRLCK);
		return 0;
	}

	printf("freelist top=[0x%08x]\n", rec_ptr );
	while (rec_ptr) {
		if (tdb->methods->tdb_read(tdb, rec_ptr, (char *)&rec,
					   sizeof(rec), DOCONV()) == -1) {
			tdb_unlock(tdb, -1, F_WRLCK);
			return -1;
		}

		if (rec.magic != TDB_FREE_MAGIC) {
			printf("bad magic 0x%08x in free list\n", rec.magic);
			tdb_unlock(tdb, -1, F_WRLCK);
			return -1;
		}

		printf("entry offset=[0x%08x], rec.rec_len = [0x%08x (%d)] (end = 0x%08x)\n",
		       rec_ptr, rec.rec_len, rec.rec_len, rec_ptr + rec.rec_len);
		total_free += rec.rec_len;

		
		rec_ptr = rec.next;
	}
	printf("total rec_len = [0x%08x (%d)]\n", (int)total_free,
               (int)total_free);

	return tdb_unlock(tdb, -1, F_WRLCK);
}


void tdb_increment_seqnum_nonblock(struct tdb_context *tdb)
{
	tdb_off_t seqnum=0;

	if (!(tdb->flags & TDB_SEQNUM)) {
		return;
	}

	tdb_ofs_read(tdb, TDB_SEQNUM_OFS, &seqnum);
	seqnum++;
	tdb_ofs_write(tdb, TDB_SEQNUM_OFS, &seqnum);
}

static void tdb_increment_seqnum(struct tdb_context *tdb)
{
	if (!(tdb->flags & TDB_SEQNUM)) {
		return;
	}

	if (tdb_brlock(tdb, TDB_SEQNUM_OFS, F_WRLCK, F_SETLKW, 1, 1) != 0) {
		return;
	}

	tdb_increment_seqnum_nonblock(tdb);

	tdb_brlock(tdb, TDB_SEQNUM_OFS, F_UNLCK, F_SETLKW, 1, 1);
}

static int tdb_key_compare(TDB_DATA key, TDB_DATA data, void *private_data)
{
	return memcmp(data.dptr, key.dptr, data.dsize);
}

static tdb_off_t tdb_find(struct tdb_context *tdb, TDB_DATA key, u32 hash,
			struct list_struct *r)
{
	tdb_off_t rec_ptr;

	
	if (tdb_ofs_read(tdb, TDB_HASH_TOP(hash), &rec_ptr) == -1)
		return 0;

	
	while (rec_ptr) {
		if (tdb_rec_read(tdb, rec_ptr, r) == -1)
			return 0;

		if (!TDB_DEAD(r) && hash==r->full_hash
		    && key.dsize==r->key_len
		    && tdb_parse_data(tdb, key, rec_ptr + sizeof(*r),
				      r->key_len, tdb_key_compare,
				      NULL) == 0) {
			return rec_ptr;
		}
		rec_ptr = r->next;
	}
	return TDB_ERRCODE(TDB_ERR_NOEXIST, 0);
}

tdb_off_t tdb_find_lock_hash(struct tdb_context *tdb, TDB_DATA key, u32 hash, int locktype,
			   struct list_struct *rec)
{
	u32 rec_ptr;

	if (tdb_lock(tdb, BUCKET(hash), locktype) == -1)
		return 0;
	if (!(rec_ptr = tdb_find(tdb, key, hash, rec)))
		tdb_unlock(tdb, BUCKET(hash), locktype);
	return rec_ptr;
}


static int tdb_update_hash(struct tdb_context *tdb, TDB_DATA key, u32 hash, TDB_DATA dbuf)
{
	struct list_struct rec;
	tdb_off_t rec_ptr;

	
	if (!(rec_ptr = tdb_find(tdb, key, hash, &rec)))
		return -1;

	
	if (rec.rec_len < key.dsize + dbuf.dsize + sizeof(tdb_off_t)) {
		tdb->ecode = TDB_SUCCESS; 
		return -1;
	}

	if (tdb->methods->tdb_write(tdb, rec_ptr + sizeof(rec) + rec.key_len,
		      dbuf.dptr, dbuf.dsize) == -1)
		return -1;

	if (dbuf.dsize != rec.data_len) {
		
		rec.data_len = dbuf.dsize;
		return tdb_rec_write(tdb, rec_ptr, &rec);
	}

	return 0;
}

TDB_DATA tdb_fetch(struct tdb_context *tdb, TDB_DATA key)
{
	tdb_off_t rec_ptr;
	struct list_struct rec;
	TDB_DATA ret;
	u32 hash;

	
	hash = tdb->hash_fn(&key);
	if (!(rec_ptr = tdb_find_lock_hash(tdb,key,hash,F_RDLCK,&rec)))
		return tdb_null;

	ret.dptr = tdb_alloc_read(tdb, rec_ptr + sizeof(rec) + rec.key_len,
				  rec.data_len);
	ret.dsize = rec.data_len;
	tdb_unlock(tdb, BUCKET(rec.full_hash), F_RDLCK);
	return ret;
}


int tdb_parse_record(struct tdb_context *tdb, TDB_DATA key,
		     int (*parser)(TDB_DATA key, TDB_DATA data,
				   void *private_data),
		     void *private_data)
{
	tdb_off_t rec_ptr;
	struct list_struct rec;
	int ret;
	u32 hash;

	
	hash = tdb->hash_fn(&key);

	if (!(rec_ptr = tdb_find_lock_hash(tdb,key,hash,F_RDLCK,&rec))) {
		return TDB_ERRCODE(TDB_ERR_NOEXIST, 0);
	}

	ret = tdb_parse_data(tdb, key, rec_ptr + sizeof(rec) + rec.key_len,
			     rec.data_len, parser, private_data);

	tdb_unlock(tdb, BUCKET(rec.full_hash), F_RDLCK);

	return ret;
}

static int tdb_exists_hash(struct tdb_context *tdb, TDB_DATA key, u32 hash)
{
	struct list_struct rec;

	if (tdb_find_lock_hash(tdb, key, hash, F_RDLCK, &rec) == 0)
		return 0;
	tdb_unlock(tdb, BUCKET(rec.full_hash), F_RDLCK);
	return 1;
}

int tdb_exists(struct tdb_context *tdb, TDB_DATA key)
{
	u32 hash = tdb->hash_fn(&key);
	return tdb_exists_hash(tdb, key, hash);
}

int tdb_do_delete(struct tdb_context *tdb, tdb_off_t rec_ptr, struct list_struct*rec)
{
	tdb_off_t last_ptr, i;
	struct list_struct lastrec;

	if (tdb->read_only || tdb->traverse_read) return -1;

	if (tdb_write_lock_record(tdb, rec_ptr) == -1) {
		
		rec->magic = TDB_DEAD_MAGIC;
		return tdb_rec_write(tdb, rec_ptr, rec);
	}
	if (tdb_write_unlock_record(tdb, rec_ptr) != 0)
		return -1;

	
	if (tdb_ofs_read(tdb, TDB_HASH_TOP(rec->full_hash), &i) == -1)
		return -1;
	for (last_ptr = 0; i != rec_ptr; last_ptr = i, i = lastrec.next)
		if (tdb_rec_read(tdb, i, &lastrec) == -1)
			return -1;

	
	if (last_ptr == 0)
		last_ptr = TDB_HASH_TOP(rec->full_hash);
	if (tdb_ofs_write(tdb, last_ptr, &rec->next) == -1)
		return -1;

	
	if (tdb_free(tdb, rec_ptr, rec) == -1)
		return -1;
	return 0;
}

static int tdb_count_dead(struct tdb_context *tdb, u32 hash)
{
	int res = 0;
	tdb_off_t rec_ptr;
	struct list_struct rec;

	
	if (tdb_ofs_read(tdb, TDB_HASH_TOP(hash), &rec_ptr) == -1)
		return 0;

	while (rec_ptr) {
		if (tdb_rec_read(tdb, rec_ptr, &rec) == -1)
			return 0;

		if (rec.magic == TDB_DEAD_MAGIC) {
			res += 1;
		}
		rec_ptr = rec.next;
	}
	return res;
}

static int tdb_purge_dead(struct tdb_context *tdb, u32 hash)
{
	int res = -1;
	struct list_struct rec;
	tdb_off_t rec_ptr;

	if (tdb_lock(tdb, -1, F_WRLCK) == -1) {
		return -1;
	}

	
	if (tdb_ofs_read(tdb, TDB_HASH_TOP(hash), &rec_ptr) == -1)
		goto fail;

	while (rec_ptr) {
		tdb_off_t next;

		if (tdb_rec_read(tdb, rec_ptr, &rec) == -1) {
			goto fail;
		}

		next = rec.next;

		if (rec.magic == TDB_DEAD_MAGIC
		    && tdb_do_delete(tdb, rec_ptr, &rec) == -1) {
			goto fail;
		}
		rec_ptr = next;
	}
	res = 0;
 fail:
	tdb_unlock(tdb, -1, F_WRLCK);
	return res;
}

static int tdb_delete_hash(struct tdb_context *tdb, TDB_DATA key, u32 hash)
{
	tdb_off_t rec_ptr;
	struct list_struct rec;
	int ret;

	if (tdb->max_dead_records != 0) {


		if (tdb_lock(tdb, BUCKET(hash), F_WRLCK) == -1)
			return -1;

		if (tdb_count_dead(tdb, hash) >= tdb->max_dead_records) {
			tdb_purge_dead(tdb, hash);
		}

		if (!(rec_ptr = tdb_find(tdb, key, hash, &rec))) {
			tdb_unlock(tdb, BUCKET(hash), F_WRLCK);
			return -1;
		}

		rec.magic = TDB_DEAD_MAGIC;
		ret = tdb_rec_write(tdb, rec_ptr, &rec);
	}
	else {
		if (!(rec_ptr = tdb_find_lock_hash(tdb, key, hash, F_WRLCK,
						   &rec)))
			return -1;

		ret = tdb_do_delete(tdb, rec_ptr, &rec);
	}

	if (ret == 0) {
		tdb_increment_seqnum(tdb);
	}

	if (tdb_unlock(tdb, BUCKET(rec.full_hash), F_WRLCK) != 0)
		TDB_LOG((tdb, TDB_DEBUG_WARNING, "tdb_delete: WARNING tdb_unlock failed!\n"));
	return ret;
}

int tdb_delete(struct tdb_context *tdb, TDB_DATA key)
{
	u32 hash = tdb->hash_fn(&key);
	return tdb_delete_hash(tdb, key, hash);
}

static tdb_off_t tdb_find_dead(struct tdb_context *tdb, u32 hash,
			       struct list_struct *r, tdb_len_t length)
{
	tdb_off_t rec_ptr;

	
	if (tdb_ofs_read(tdb, TDB_HASH_TOP(hash), &rec_ptr) == -1)
		return 0;

	
	while (rec_ptr) {
		if (tdb_rec_read(tdb, rec_ptr, r) == -1)
			return 0;

		if (TDB_DEAD(r) && r->rec_len >= length) {
			return rec_ptr;
		}
		rec_ptr = r->next;
	}
	return 0;
}

int tdb_store(struct tdb_context *tdb, TDB_DATA key, TDB_DATA dbuf, int flag)
{
	struct list_struct rec;
	u32 hash;
	tdb_off_t rec_ptr;
	char *p = NULL;
	int ret = -1;

	if (tdb->read_only || tdb->traverse_read) {
		tdb->ecode = TDB_ERR_RDONLY;
		return -1;
	}

	
	hash = tdb->hash_fn(&key);
	if (tdb_lock(tdb, BUCKET(hash), F_WRLCK) == -1)
		return -1;

	
	if (flag == TDB_INSERT) {
		if (tdb_exists_hash(tdb, key, hash)) {
			tdb->ecode = TDB_ERR_EXISTS;
			goto fail;
		}
	} else {
		
		if (tdb_update_hash(tdb, key, hash, dbuf) == 0) {
			goto done;
		}
		if (tdb->ecode == TDB_ERR_NOEXIST &&
		    flag == TDB_MODIFY) {
			goto fail;
		}
	}
	
	tdb->ecode = TDB_SUCCESS;

	if (flag != TDB_INSERT)
		tdb_delete_hash(tdb, key, hash);


	if (!(p = (char *)malloc(key.dsize + dbuf.dsize))) {
		tdb->ecode = TDB_ERR_OOM;
		goto fail;
	}

	memcpy(p, key.dptr, key.dsize);
	if (dbuf.dsize)
		memcpy(p+key.dsize, dbuf.dptr, dbuf.dsize);

	if (tdb->max_dead_records != 0) {
		rec_ptr = tdb_find_dead(
			tdb, hash, &rec,
			key.dsize + dbuf.dsize + sizeof(tdb_off_t));

		if (rec_ptr != 0) {
			rec.key_len = key.dsize;
			rec.data_len = dbuf.dsize;
			rec.full_hash = hash;
			rec.magic = TDB_MAGIC;
			if (tdb_rec_write(tdb, rec_ptr, &rec) == -1
			    || tdb->methods->tdb_write(
				    tdb, rec_ptr + sizeof(rec),
				    p, key.dsize + dbuf.dsize) == -1) {
				goto fail;
			}
			goto done;
		}
	}


	if (tdb_lock(tdb, -1, F_WRLCK) == -1) {
		goto fail;
	}

	if ((tdb->max_dead_records != 0)
	    && (tdb_purge_dead(tdb, hash) == -1)) {
		tdb_unlock(tdb, -1, F_WRLCK);
		goto fail;
	}

	
	rec_ptr = tdb_allocate(tdb, key.dsize + dbuf.dsize, &rec);

	tdb_unlock(tdb, -1, F_WRLCK);

	if (rec_ptr == 0) {
		goto fail;
	}

	
	if (tdb_ofs_read(tdb, TDB_HASH_TOP(hash), &rec.next) == -1)
		goto fail;

	rec.key_len = key.dsize;
	rec.data_len = dbuf.dsize;
	rec.full_hash = hash;
	rec.magic = TDB_MAGIC;

	
	if (tdb_rec_write(tdb, rec_ptr, &rec) == -1
	    || tdb->methods->tdb_write(tdb, rec_ptr+sizeof(rec), p, key.dsize+dbuf.dsize)==-1
	    || tdb_ofs_write(tdb, TDB_HASH_TOP(hash), &rec_ptr) == -1) {
		
		goto fail;
	}

 done:
	ret = 0;
 fail:
	if (ret == 0) {
		tdb_increment_seqnum(tdb);
	}

	SAFE_FREE(p);
	tdb_unlock(tdb, BUCKET(hash), F_WRLCK);
	return ret;
}


int tdb_append(struct tdb_context *tdb, TDB_DATA key, TDB_DATA new_dbuf)
{
	u32 hash;
	TDB_DATA dbuf;
	int ret = -1;

	
	hash = tdb->hash_fn(&key);
	if (tdb_lock(tdb, BUCKET(hash), F_WRLCK) == -1)
		return -1;

	dbuf = tdb_fetch(tdb, key);

	if (dbuf.dptr == NULL) {
		dbuf.dptr = (unsigned char *)malloc(new_dbuf.dsize);
	} else {
		unsigned char *new_dptr = (unsigned char *)realloc(dbuf.dptr,
						     dbuf.dsize + new_dbuf.dsize);
		if (new_dptr == NULL) {
			free(dbuf.dptr);
		}
		dbuf.dptr = new_dptr;
	}

	if (dbuf.dptr == NULL) {
		tdb->ecode = TDB_ERR_OOM;
		goto failed;
	}

	memcpy(dbuf.dptr + dbuf.dsize, new_dbuf.dptr, new_dbuf.dsize);
	dbuf.dsize += new_dbuf.dsize;

	ret = tdb_store(tdb, key, dbuf, 0);

failed:
	tdb_unlock(tdb, BUCKET(hash), F_WRLCK);
	SAFE_FREE(dbuf.dptr);
	return ret;
}


const char *tdb_name(struct tdb_context *tdb)
{
	return tdb->name;
}

int tdb_fd(struct tdb_context *tdb)
{
	return tdb->fd;
}

tdb_log_func tdb_log_fn(struct tdb_context *tdb)
{
	return tdb->log.log_fn;
}


int tdb_get_seqnum(struct tdb_context *tdb)
{
	tdb_off_t seqnum=0;

	tdb_ofs_read(tdb, TDB_SEQNUM_OFS, &seqnum);
	return seqnum;
}

int tdb_hash_size(struct tdb_context *tdb)
{
	return tdb->header.hash_size;
}

size_t tdb_map_size(struct tdb_context *tdb)
{
	return tdb->map_size;
}

int tdb_get_flags(struct tdb_context *tdb)
{
	return tdb->flags;
}


void tdb_enable_seqnum(struct tdb_context *tdb)
{
	tdb->flags |= TDB_SEQNUM;
}


static struct tdb_context *tdbs = NULL;


static unsigned int default_tdb_hash(TDB_DATA *key)
{
	u32 value;	
	u32   i;	

	
	for (value = 0, i=0; i < key->dsize; i++)
		value = value * 256 + key->dptr[i] + (value >> 24) * 241;

	return value;
}


static int tdb_new_database(struct tdb_context *tdb, int hash_size)
{
	struct tdb_header *newdb;
	int size, ret = -1;

	
	size = sizeof(struct tdb_header) + (hash_size+1)*sizeof(tdb_off_t);
	if (!(newdb = (struct tdb_header *)calloc(size, 1)))
		return TDB_ERRCODE(TDB_ERR_OOM, -1);

	
	newdb->version = TDB_VERSION;
	newdb->hash_size = hash_size;
	if (tdb->flags & TDB_INTERNAL) {
		tdb->map_size = size;
		tdb->map_ptr = (char *)newdb;
		memcpy(&tdb->header, newdb, sizeof(tdb->header));
		
		CONVERT(*newdb);
		return 0;
	}
	if (lseek(tdb->fd, 0, SEEK_SET) == -1)
		goto fail;

	if (ftruncate(tdb->fd, 0) == -1)
		goto fail;

	
	CONVERT(*newdb);
	memcpy(&tdb->header, newdb, sizeof(tdb->header));
	
	memcpy(newdb->magic_food, TDB_MAGIC_FOOD, strlen(TDB_MAGIC_FOOD)+1);
	if (write(tdb->fd, newdb, size) != size) {
		ret = -1;
	} else {
		ret = 0;
	}

  fail:
	SAFE_FREE(newdb);
	return ret;
}



static int tdb_already_open(dev_t device,
			    ino_t ino)
{
	struct tdb_context *i;

	for (i = tdbs; i; i = i->next) {
		if (i->device == device && i->inode == ino) {
			return 1;
		}
	}

	return 0;
}

struct tdb_context *tdb_open(const char *name, int hash_size, int tdb_flags,
		      int open_flags, mode_t mode)
{
	return tdb_open_ex(name, hash_size, tdb_flags, open_flags, mode, NULL, NULL);
}

static void null_log_fn(struct tdb_context *tdb, enum tdb_debug_level level, const char *fmt, ...) PRINTF_ATTRIBUTE(3, 4);
static void null_log_fn(struct tdb_context *tdb, enum tdb_debug_level level, const char *fmt, ...)
{
}


struct tdb_context *tdb_open_ex(const char *name, int hash_size, int tdb_flags,
				int open_flags, mode_t mode,
				const struct tdb_logging_context *log_ctx,
				tdb_hash_func hash_fn)
{
	struct tdb_context *tdb;
	struct stat st;
	int rev = 0, locked = 0;
	unsigned char *vp;
	u32 vertest;

	if (!(tdb = (struct tdb_context *)calloc(1, sizeof *tdb))) {
		
		errno = ENOMEM;
		goto fail;
	}
	tdb_io_init(tdb);
	tdb->fd = -1;
	tdb->name = NULL;
	tdb->map_ptr = NULL;
	tdb->flags = tdb_flags;
	tdb->open_flags = open_flags;
	if (log_ctx) {
		tdb->log = *log_ctx;
	} else {
		tdb->log.log_fn = null_log_fn;
		tdb->log.log_private = NULL;
	}
	tdb->hash_fn = hash_fn ? hash_fn : default_tdb_hash;

	
	tdb->page_size = sysconf(_SC_PAGESIZE);
	if (tdb->page_size <= 0) {
		tdb->page_size = 0x2000;
	}

	if ((open_flags & O_ACCMODE) == O_WRONLY) {
		TDB_LOG((tdb, TDB_DEBUG_ERROR, "tdb_open_ex: can't open tdb %s write-only\n",
			 name));
		errno = EINVAL;
		goto fail;
	}

	if (hash_size == 0)
		hash_size = DEFAULT_HASH_SIZE;
	if ((open_flags & O_ACCMODE) == O_RDONLY) {
		tdb->read_only = 1;
		
		tdb->flags |= TDB_NOLOCK;
		tdb->flags &= ~TDB_CLEAR_IF_FIRST;
	}

	
	if (tdb->flags & TDB_INTERNAL) {
		tdb->flags |= (TDB_NOLOCK | TDB_NOMMAP);
		tdb->flags &= ~TDB_CLEAR_IF_FIRST;
		if (tdb_new_database(tdb, hash_size) != 0) {
			TDB_LOG((tdb, TDB_DEBUG_ERROR, "tdb_open_ex: tdb_new_database failed!"));
			goto fail;
		}
		goto internal;
	}

	if ((tdb->fd = open(name, open_flags, mode)) == -1) {
		TDB_LOG((tdb, TDB_DEBUG_WARNING, "tdb_open_ex: could not open file %s: %s\n",
			 name, strerror(errno)));
		goto fail;	
	}

	
	if (tdb->methods->tdb_brlock(tdb, GLOBAL_LOCK, F_WRLCK, F_SETLKW, 0, 1) == -1) {
		TDB_LOG((tdb, TDB_DEBUG_ERROR, "tdb_open_ex: failed to get global lock on %s: %s\n",
			 name, strerror(errno)));
		goto fail;	
	}

	
	if ((tdb_flags & TDB_CLEAR_IF_FIRST) &&
	    (locked = (tdb->methods->tdb_brlock(tdb, ACTIVE_LOCK, F_WRLCK, F_SETLK, 0, 1) == 0))) {
		open_flags |= O_CREAT;
		if (ftruncate(tdb->fd, 0) == -1) {
			TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_open_ex: "
				 "failed to truncate %s: %s\n",
				 name, strerror(errno)));
			goto fail; 
		}
	}

	if (read(tdb->fd, &tdb->header, sizeof(tdb->header)) != sizeof(tdb->header)
	    || strcmp(tdb->header.magic_food, TDB_MAGIC_FOOD) != 0
	    || (tdb->header.version != TDB_VERSION
		&& !(rev = (tdb->header.version==TDB_BYTEREV(TDB_VERSION))))) {
		
		if (!(open_flags & O_CREAT) || tdb_new_database(tdb, hash_size) == -1) {
			errno = EIO; 
			goto fail;
		}
		rev = (tdb->flags & TDB_CONVERT);
	}
	vp = (unsigned char *)&tdb->header.version;
	vertest = (((u32)vp[0]) << 24) | (((u32)vp[1]) << 16) |
		  (((u32)vp[2]) << 8) | (u32)vp[3];
	tdb->flags |= (vertest==TDB_VERSION) ? TDB_BIGENDIAN : 0;
	if (!rev)
		tdb->flags &= ~TDB_CONVERT;
	else {
		tdb->flags |= TDB_CONVERT;
		tdb_convert(&tdb->header, sizeof(tdb->header));
	}
	if (fstat(tdb->fd, &st) == -1)
		goto fail;

	if (tdb->header.rwlocks != 0) {
		TDB_LOG((tdb, TDB_DEBUG_ERROR, "tdb_open_ex: spinlocks no longer supported\n"));
		goto fail;
	}

	
	if (tdb_already_open(st.st_dev, st.st_ino)) {
		TDB_LOG((tdb, TDB_DEBUG_ERROR, "tdb_open_ex: "
			 "%s (%d,%d) is already open in this process\n",
			 name, (int)st.st_dev, (int)st.st_ino));
		errno = EBUSY;
		goto fail;
	}

	if (!(tdb->name = (char *)strdup(name))) {
		errno = ENOMEM;
		goto fail;
	}

	tdb->map_size = st.st_size;
	tdb->device = st.st_dev;
	tdb->inode = st.st_ino;
	tdb->max_dead_records = 0;
	tdb_mmap(tdb);
	if (locked) {
		if (tdb->methods->tdb_brlock(tdb, ACTIVE_LOCK, F_UNLCK, F_SETLK, 0, 1) == -1) {
			TDB_LOG((tdb, TDB_DEBUG_ERROR, "tdb_open_ex: "
				 "failed to take ACTIVE_LOCK on %s: %s\n",
				 name, strerror(errno)));
			goto fail;
		}

	}


	if (tdb_flags & TDB_CLEAR_IF_FIRST) {
		
		if (tdb->methods->tdb_brlock(tdb, ACTIVE_LOCK, F_RDLCK, F_SETLKW, 0, 1) == -1)
			goto fail;
	}

	
	if (tdb_transaction_recover(tdb) == -1) {
		goto fail;
	}

 internal:
	if (tdb->methods->tdb_brlock(tdb, GLOBAL_LOCK, F_UNLCK, F_SETLKW, 0, 1) == -1)
		goto fail;
	tdb->next = tdbs;
	tdbs = tdb;
	return tdb;

 fail:
	{ int save_errno = errno;

	if (!tdb)
		return NULL;

	if (tdb->map_ptr) {
		if (tdb->flags & TDB_INTERNAL)
			SAFE_FREE(tdb->map_ptr);
		else
			tdb_munmap(tdb);
	}
	SAFE_FREE(tdb->name);
	if (tdb->fd != -1)
		if (close(tdb->fd) != 0)
			TDB_LOG((tdb, TDB_DEBUG_ERROR, "tdb_open_ex: failed to close tdb->fd on error!\n"));
	SAFE_FREE(tdb);
	errno = save_errno;
	return NULL;
	}
}


void tdb_set_max_dead(struct tdb_context *tdb, int max_dead)
{
	tdb->max_dead_records = max_dead;
}

int tdb_close(struct tdb_context *tdb)
{
	struct tdb_context **i;
	int ret = 0;

	if (tdb->transaction) {
		tdb_transaction_cancel(tdb);
	}

	if (tdb->map_ptr) {
		if (tdb->flags & TDB_INTERNAL)
			SAFE_FREE(tdb->map_ptr);
		else
			tdb_munmap(tdb);
	}
	SAFE_FREE(tdb->name);
	if (tdb->fd != -1)
		ret = close(tdb->fd);
	SAFE_FREE(tdb->lockrecs);

	
	for (i = &tdbs; *i; i = &(*i)->next) {
		if (*i == tdb) {
			*i = tdb->next;
			break;
		}
	}

	memset(tdb, 0, sizeof(*tdb));
	SAFE_FREE(tdb);

	return ret;
}

void tdb_set_logging_function(struct tdb_context *tdb,
                              const struct tdb_logging_context *log_ctx)
{
        tdb->log = *log_ctx;
}

void *tdb_get_logging_private(struct tdb_context *tdb)
{
	return tdb->log.log_private;
}

int tdb_reopen(struct tdb_context *tdb)
{
	struct stat st;

	if (tdb->flags & TDB_INTERNAL) {
		return 0; 
	}

	if (tdb->num_locks != 0 || tdb->global_lock.count) {
		TDB_LOG((tdb, TDB_DEBUG_ERROR, "tdb_reopen: reopen not allowed with locks held\n"));
		goto fail;
	}

	if (tdb->transaction != 0) {
		TDB_LOG((tdb, TDB_DEBUG_ERROR, "tdb_reopen: reopen not allowed inside a transaction\n"));
		goto fail;
	}

	if (tdb_munmap(tdb) != 0) {
		TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_reopen: munmap failed (%s)\n", strerror(errno)));
		goto fail;
	}
	if (close(tdb->fd) != 0)
		TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_reopen: WARNING closing tdb->fd failed!\n"));
	tdb->fd = open(tdb->name, tdb->open_flags & ~(O_CREAT|O_TRUNC), 0);
	if (tdb->fd == -1) {
		TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_reopen: open failed (%s)\n", strerror(errno)));
		goto fail;
	}
	if ((tdb->flags & TDB_CLEAR_IF_FIRST) &&
	    (tdb->methods->tdb_brlock(tdb, ACTIVE_LOCK, F_RDLCK, F_SETLKW, 0, 1) == -1)) {
		TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_reopen: failed to obtain active lock\n"));
		goto fail;
	}
	if (fstat(tdb->fd, &st) != 0) {
		TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_reopen: fstat failed (%s)\n", strerror(errno)));
		goto fail;
	}
	if (st.st_ino != tdb->inode || st.st_dev != tdb->device) {
		TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_reopen: file dev/inode has changed!\n"));
		goto fail;
	}
	tdb_mmap(tdb);

	return 0;

fail:
	tdb_close(tdb);
	return -1;
}

int tdb_reopen_all(int parent_longlived)
{
	struct tdb_context *tdb;

	for (tdb=tdbs; tdb; tdb = tdb->next) {
		if (parent_longlived) {
			
			tdb->flags &= ~TDB_CLEAR_IF_FIRST;
		}

		if (tdb_reopen(tdb) != 0)
			return -1;
	}

	return 0;
}
