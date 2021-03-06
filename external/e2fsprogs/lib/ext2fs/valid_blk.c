/*
 * valid_blk.c --- does the inode have valid blocks?
 *
 * Copyright 1997 by Theodore Ts'o
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Library
 * General Public License, version 2.
 * %End-Header%
 */

#include <stdio.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <string.h>
#include <time.h>

#include "ext2_fs.h"
#include "ext2fs.h"

int ext2fs_inode_has_valid_blocks2(ext2_filsys fs, struct ext2_inode *inode)
{
	if (!LINUX_S_ISDIR(inode->i_mode) && !LINUX_S_ISREG(inode->i_mode) &&
	    !LINUX_S_ISLNK(inode->i_mode))
		return 0;

	if (LINUX_S_ISLNK (inode->i_mode)) {
		if (ext2fs_file_acl_block(fs, inode) == 0) {
			
			if (inode->i_blocks == 0)
				return 0;
		} else {
			
			if (inode->i_size >= EXT2_N_BLOCKS*4)
				return 1; 
			if (inode->i_size > 4 && inode->i_block[1] == 0)
				return 1; 
			return 0; 
		}
	}
	return 1;
}

int ext2fs_inode_has_valid_blocks(struct ext2_inode *inode)
{
	return ext2fs_inode_has_valid_blocks2(NULL, inode);
}
