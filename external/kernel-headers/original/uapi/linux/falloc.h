#ifndef _UAPI_FALLOC_H_
#define _UAPI_FALLOC_H_

#define FALLOC_FL_KEEP_SIZE	0x01 
#define FALLOC_FL_PUNCH_HOLE	0x02 
#define FALLOC_FL_NO_HIDE_STALE	0x04 

#define FALLOC_FL_COLLAPSE_RANGE	0x08

/*
 * FALLOC_FL_ZERO_RANGE is used to convert a range of file to zeros preferably
 * without issuing data IO. Blocks should be preallocated for the regions that
 * span holes in the file, and the entire range is preferable converted to
 * unwritten extents - even though file system may choose to zero out the
 * extent or do whatever which will result in reading zeros from the range
 * while the range remains allocated for the file.
 *
 * This can be also used to preallocate blocks past EOF in the same way as
 * with fallocate. Flag FALLOC_FL_KEEP_SIZE should cause the inode
 * size to remain the same.
 */
#define FALLOC_FL_ZERO_RANGE		0x10

#endif 
