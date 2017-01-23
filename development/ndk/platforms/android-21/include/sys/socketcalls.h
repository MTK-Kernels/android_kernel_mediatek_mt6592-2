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
#ifndef _SYS_SOCKETCALLS_H_
#define _SYS_SOCKETCALLS_H_


#define SYS_SOCKET      1               
#define SYS_BIND        2               
#define SYS_CONNECT     3               
#define SYS_LISTEN      4               
#define SYS_ACCEPT      5               
#define SYS_GETSOCKNAME 6               
#define SYS_GETPEERNAME 7               
#define SYS_SOCKETPAIR  8               
#define SYS_SEND        9               
#define SYS_RECV        10              
#define SYS_SENDTO      11              
#define SYS_RECVFROM    12              
#define SYS_SHUTDOWN    13              
#define SYS_SETSOCKOPT  14              
#define SYS_GETSOCKOPT  15              
#define SYS_SENDMSG     16              
#define SYS_RECVMSG     17              
#define SYS_ACCEPT4     18              
#define SYS_RECVMMSG    19              
#define SYS_SENDMMSG    20              

#endif 
