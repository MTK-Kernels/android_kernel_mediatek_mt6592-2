/*
 * VMware vSockets Driver
 *
 * Copyright (C) 2007-2013 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation version 2 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#ifndef _UAPI_VM_SOCKETS_H
#define _UAPI_VM_SOCKETS_H

#include <linux/socket.h>


#define SO_VM_SOCKETS_BUFFER_SIZE 0


#define SO_VM_SOCKETS_BUFFER_MIN_SIZE 1

/* Option name for STREAM socket maximum buffer size.  Use as the option name
 * in setsockopt(3) or getsockopt(3) to set or get an unsigned long long
 * that specifies the maximum size allowed for the buffer underlying a
 * vSockets STREAM socket.
 */

#define SO_VM_SOCKETS_BUFFER_MAX_SIZE 2


#define SO_VM_SOCKETS_PEER_HOST_VM_ID 3


#define SO_VM_SOCKETS_TRUSTED 5


#define SO_VM_SOCKETS_CONNECT_TIMEOUT 6


#define SO_VM_SOCKETS_NONBLOCK_TXRX 7


#define VMADDR_CID_ANY -1U


#define VMADDR_PORT_ANY -1U


#define VMADDR_CID_HYPERVISOR 0


#define VMADDR_CID_RESERVED 1


#define VMADDR_CID_HOST 2


#define VM_SOCKETS_INVALID_VERSION -1U


#define VM_SOCKETS_VERSION_EPOCH(_v) (((_v) & 0xFF000000) >> 24)


#define VM_SOCKETS_VERSION_MAJOR(_v) (((_v) & 0x00FF0000) >> 16)


#define VM_SOCKETS_VERSION_MINOR(_v) (((_v) & 0x0000FFFF))


struct sockaddr_vm {
	__kernel_sa_family_t svm_family;
	unsigned short svm_reserved1;
	unsigned int svm_port;
	unsigned int svm_cid;
	unsigned char svm_zero[sizeof(struct sockaddr) -
			       sizeof(sa_family_t) -
			       sizeof(unsigned short) -
			       sizeof(unsigned int) - sizeof(unsigned int)];
};

#define IOCTL_VM_SOCKETS_GET_LOCAL_CID		_IO(7, 0xb9)

#endif 
