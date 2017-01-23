/*
 * blkidP.h - Internal interfaces for libblkid
 *
 * Copyright (C) 2001 Andreas Dilger
 * Copyright (C) 2003 Theodore Ts'o
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 * %End-Header%
 */

#ifndef _BLKID_BLKIDP_H
#define _BLKID_BLKIDP_H

#include <sys/types.h>
#include <stdio.h>

#include <blkid/blkid.h>

#include <blkid/list.h>

#ifdef __GNUC__
#define __BLKID_ATTR(x) __attribute__(x)
#else
#define __BLKID_ATTR(x)
#endif


struct blkid_struct_dev
{
	struct list_head	bid_devs;	
	struct list_head	bid_tags;	
	blkid_cache		bid_cache;	
	char			*bid_name;	
	char			*bid_type;	
	int			bid_pri;	
	dev_t			bid_devno;	
	time_t			bid_time;	
	unsigned int		bid_flags;	
	char			*bid_label;	
	char			*bid_uuid;	
};

#define BLKID_BID_FL_VERIFIED	0x0001	
#define BLKID_BID_FL_INVALID	0x0004	

struct blkid_struct_tag
{
	struct list_head	bit_tags;	
	struct list_head	bit_names;	
	char			*bit_name;	
	char			*bit_val;	
	blkid_dev		bit_dev;	
};
typedef struct blkid_struct_tag *blkid_tag;

#define BLKID_PROBE_MIN		2

#define BLKID_PROBE_INTERVAL	200

struct blkid_struct_cache
{
	struct list_head	bic_devs;	
	struct list_head	bic_tags;	
	time_t			bic_time;	
	time_t			bic_ftime; 	
	unsigned int		bic_flags;	
	char			*bic_filename;	
};

#define BLKID_BIC_FL_PROBED	0x0002	
#define BLKID_BIC_FL_CHANGED	0x0004	

extern char *blkid_strdup(const char *s);
extern char *blkid_strndup(const char *s, const int length);

#define BLKID_CACHE_FILE "/etc/blkid.tab"

#define BLKID_ERR_IO	 5
#define BLKID_ERR_PROC	 9
#define BLKID_ERR_MEM	12
#define BLKID_ERR_CACHE	14
#define BLKID_ERR_DEV	19
#define BLKID_ERR_PARAM	22
#define BLKID_ERR_BIG	27

#define BLKID_PRI_DM	40
#define BLKID_PRI_EVMS	30
#define BLKID_PRI_LVM	20
#define BLKID_PRI_MD	10

#if defined(TEST_PROGRAM) && !defined(CONFIG_BLKID_DEBUG)
#define CONFIG_BLKID_DEBUG
#endif

#define DEBUG_CACHE	0x0001
#define DEBUG_DUMP	0x0002
#define DEBUG_DEV	0x0004
#define DEBUG_DEVNAME	0x0008
#define DEBUG_DEVNO	0x0010
#define DEBUG_PROBE	0x0020
#define DEBUG_READ	0x0040
#define DEBUG_RESOLVE	0x0080
#define DEBUG_SAVE	0x0100
#define DEBUG_TAG	0x0200
#define DEBUG_INIT	0x8000
#define DEBUG_ALL	0xFFFF

#ifdef CONFIG_BLKID_DEBUG
#include <stdio.h>
extern int	blkid_debug_mask;
#define DBG(m,x)	if ((m) & blkid_debug_mask) x;
#else
#define DBG(m,x)
#endif

#ifdef CONFIG_BLKID_DEBUG
extern void blkid_debug_dump_dev(blkid_dev dev);
extern void blkid_debug_dump_tag(blkid_tag tag);
#endif

struct dir_list {
	char	*name;
	struct dir_list *next;
};
extern void blkid__scan_dir(char *, dev_t, struct dir_list **, char **);

extern blkid_loff_t blkid_llseek(int fd, blkid_loff_t offset, int whence);

extern void blkid_read_cache(blkid_cache cache);

extern int blkid_flush_cache(blkid_cache cache);

extern void blkid_free_tag(blkid_tag tag);
extern blkid_tag blkid_find_tag_dev(blkid_dev dev, const char *type);
extern int blkid_set_tag(blkid_dev dev, const char *name,
			 const char *value, const int vlength);

extern blkid_dev blkid_new_dev(void);
extern void blkid_free_dev(blkid_dev dev);

#ifdef __cplusplus
}
#endif

#endif 
