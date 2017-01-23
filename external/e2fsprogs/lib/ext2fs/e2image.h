/*
 * e2image.h --- header file describing the ext2 image format
 *
 * Copyright (C) 2000 Theodore Ts'o.
 *
 * Note: this uses the POSIX IO interfaces, unlike most of the other
 * functions in this library.  So sue me.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Library
 * General Public License, version 2.
 * %End-Header%
 */

struct ext2_image_hdr {
	__u32	magic_number;	
	char	magic_descriptor[16]; 
	char	fs_hostname[64];
	char	fs_netaddr[32];	
	__u32	fs_netaddr_type;
	__u32	fs_device;	
	char	fs_device_name[64]; 
	char	fs_uuid[16];	
	__u32	fs_blocksize;	
	__u32	fs_reserved[8];

	__u32	image_device;	
	__u32	image_inode;	
	__u32	image_time;	
	__u32	image_reserved[8];

	__u32	offset_super;	
	__u32	offset_inode;	
	__u32	offset_inodemap; 
	__u32	offset_blockmap; 
	__u32	offset_reserved[8];
};
