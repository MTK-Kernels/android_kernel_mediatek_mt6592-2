/*
 * Copyright (C) 2008 The Android Open Source Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#ifndef _SYS_MOUNT_H
#define _SYS_MOUNT_H

#include <sys/cdefs.h>
#include <sys/ioctl.h>

__BEGIN_DECLS

#define MS_RDONLY        1      
#define MS_NOSUID        2      
#define MS_NODEV         4      
#define MS_NOEXEC        8      
#define MS_SYNCHRONOUS  16      
#define MS_REMOUNT      32      
#define MS_MANDLOCK     64      
#define MS_DIRSYNC      128     
#define MS_NOATIME      1024    
#define MS_NODIRATIME   2048    
#define MS_BIND         4096
#define MS_MOVE         8192
#define MS_REC          16384
#define MS_VERBOSE      32768
#define MS_POSIXACL     (1<<16) 
#define MS_ONE_SECOND   (1<<17) 
#define MS_ACTIVE       (1<<30)
#define MS_NOUSER       (1<<31)

#define MS_RMT_MASK     (MS_RDONLY|MS_SYNCHRONOUS|MS_MANDLOCK|MS_NOATIME|MS_NODIRATIME)

#define MS_MGC_VAL 0xC0ED0000
#define MS_MGC_MSK 0xffff0000

#define MNT_FORCE	1	
#define MNT_DETACH	2	
#define MNT_EXPIRE	4	

#define BLKROSET   _IO(0x12, 93) 
#define BLKROGET   _IO(0x12, 94) 
#define BLKRRPART  _IO(0x12, 95) 
#define BLKGETSIZE _IO(0x12, 96) 
#define BLKFLSBUF  _IO(0x12, 97) 
#define BLKRASET   _IO(0x12, 98) 
#define BLKRAGET   _IO(0x12, 99) 

extern int mount(const char *, const char *,
		   const char *, unsigned long,
		   const void *);
extern int umount(const char *);
extern int umount2(const char *, int);

#if 0 
extern int pivot_root(const char *, const char *);
#endif 

__END_DECLS

#endif 
