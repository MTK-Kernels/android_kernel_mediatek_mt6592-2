/*
 * setversion.c		- Set a file version on an ext2 file system
 *
 * Copyright (C) 1993, 1994  Remy Card <card@masi.ibp.fr>
 *                           Laboratoire MASI, Institut Blaise Pascal
 *                           Universite Pierre et Marie Curie (Paris VI)
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Library
 * General Public License, version 2.
 * %End-Header%
 */


#if HAVE_ERRNO_H
#include <errno.h>
#endif
#include <sys/ioctl.h>

#include "e2p.h"

int setversion (int fd, unsigned long version)
{
#if HAVE_EXT2_IOCTLS
	int ver;

	ver = (int) version;
	return ioctl (fd, EXT2_IOC_SETVERSION, &ver);
#else 
	extern int errno;
	errno = EOPNOTSUPP;
	return -1;
#endif 
}
