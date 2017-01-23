/*
 * Copyright (c) 2003,2004 Cluster File Systems, Inc, info@clusterfs.com
 * Written by Alex Tomas <alex@clusterfs.com>
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Library
 * General Public License, version 2.
 * %End-Header%
 */

#ifndef _LINUX_EXT3_EXTENTS
#define _LINUX_EXT3_EXTENTS


struct ext3_extent {
	__u32	ee_block;	
	__u16	ee_len;		
	__u16	ee_start_hi;	
	__u32	ee_start;	
};

struct ext3_extent_idx {
	__u32	ei_block;	
	__u32	ei_leaf;	
	__u16	ei_leaf_hi;	
	__u16	ei_unused;
};

struct ext3_extent_header {
	__u16	eh_magic;	
	__u16	eh_entries;	
	__u16	eh_max;		
	__u16	eh_depth;	
	__u32	eh_generation;	
};

#define EXT3_EXT_MAGIC		0xf30a

struct ext3_ext_path {
	__u32				p_block;
	__u16				p_depth;
	struct ext3_extent		*p_ext;
	struct ext3_extent_idx		*p_idx;
	struct ext3_extent_header	*p_hdr;
	struct buffer_head		*p_bh;
};

#define EXT_INIT_MAX_LEN	(1UL << 15)
#define EXT_UNINIT_MAX_LEN	(EXT_INIT_MAX_LEN - 1)

#define EXT_FIRST_EXTENT(__hdr__) \
	((struct ext3_extent *) (((char *) (__hdr__)) +		\
				 sizeof(struct ext3_extent_header)))
#define EXT_FIRST_INDEX(__hdr__) \
	((struct ext3_extent_idx *) (((char *) (__hdr__)) +	\
				     sizeof(struct ext3_extent_header)))
#define EXT_HAS_FREE_INDEX(__path__) \
	((__path__)->p_hdr->eh_entries < (__path__)->p_hdr->eh_max)
#define EXT_LAST_EXTENT(__hdr__) \
	(EXT_FIRST_EXTENT((__hdr__)) + (__hdr__)->eh_entries - 1)
#define EXT_LAST_INDEX(__hdr__) \
	(EXT_FIRST_INDEX((__hdr__)) + (__hdr__)->eh_entries - 1)
#define EXT_MAX_EXTENT(__hdr__) \
	(EXT_FIRST_EXTENT((__hdr__)) + (__hdr__)->eh_max - 1)
#define EXT_MAX_INDEX(__hdr__) \
	(EXT_FIRST_INDEX((__hdr__)) + (__hdr__)->eh_max - 1)

#endif 

