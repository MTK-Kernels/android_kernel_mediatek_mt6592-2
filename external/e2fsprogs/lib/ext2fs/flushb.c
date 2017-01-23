/*
 * flushb.c --- Hides system-dependent information for both syncing a
 * 	device to disk and to flush any buffers from disk cache.
 *
 * Copyright (C) 2000 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Library
 * General Public License, version 2.
 * %End-Header%
 */

#include <stdio.h>
#if HAVE_ERRNO_H
#include <errno.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#if HAVE_SYS_MOUNT_H
#include <sys/param.h>
#include <sys/mount.h>		
#endif

#include "ext2_fs.h"
#include "ext2fs.h"

#ifdef __linux__
#ifndef BLKFLSBUF
#define BLKFLSBUF	_IO(0x12,97)	
#endif
#ifndef FDFLUSH
#define FDFLUSH		_IO(2,0x4b)	
#endif
#endif

errcode_t ext2fs_sync_device(int fd, int flushb)
{
	if (fsync (fd) == -1)
		return errno;

	if (flushb) {

#ifdef BLKFLSBUF
		if (ioctl (fd, BLKFLSBUF, 0) == 0)
			return 0;
#elif defined(__linux__)
#warning BLKFLSBUF not defined
#endif
#ifdef FDFLUSH
		return ioctl(fd, FDFLUSH, 0);   
#elif defined(__linux__)
#warning FDFLUSH not defined
#endif
	}
	return 0;
}
