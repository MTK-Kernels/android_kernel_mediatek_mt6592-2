

/*
   This file is part of Valgrind, a dynamic binary instrumentation
   framework.

   Copyright (C) 2000-2013 Julian Seward
      jseward@acm.org

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307, USA.

   The GNU General Public License is contained in the file COPYING.
*/

#ifndef __PUB_CORE_LIBCFILE_H
#define __PUB_CORE_LIBCFILE_H


#include "pub_tool_libcfile.h"

extern Int VG_(safe_fd) ( Int oldfd );
extern Int VG_(fcntl)   ( Int fd, Int cmd, Addr arg );

extern Bool VG_(resolve_filename) ( Int fd, const HChar** buf );

extern Long VG_(fsize) ( Int fd );

extern SysRes VG_(getxattr) ( const HChar* file_name, const HChar* attr_name,
                              Addr attr_value, SizeT attr_value_len );

extern Bool VG_(is_dir) ( const HChar* f );

#define VG_CLO_DEFAULT_LOGPORT 1500

extern Int VG_(connect_via_socket)( const HChar* str );

extern UInt   VG_(htonl) ( UInt x );
extern UInt   VG_(ntohl) ( UInt x );
extern UShort VG_(htons) ( UShort x );
extern UShort VG_(ntohs) ( UShort x );

extern Int VG_(socket) ( Int domain, Int type, Int protocol );

extern Int VG_(write_socket)( Int sd, const void *msg, Int count );
extern Int VG_(getsockname) ( Int sd, struct vki_sockaddr *name, Int *namelen );
extern Int VG_(getpeername) ( Int sd, struct vki_sockaddr *name, Int *namelen );
extern Int VG_(getsockopt)  ( Int sd, Int level, Int optname, 
                              void *optval, Int *optlen );
extern Int VG_(setsockopt)  ( Int sd, Int level, Int optname,
                              void *optval, Int optlen );

extern Int VG_(access) ( const HChar* path, Bool irusr, Bool iwusr,
                                            Bool ixusr );

extern Int VG_(check_executable)(Bool* is_setuid,
                                 const HChar* f, Bool allow_setuid);

extern SysRes VG_(pread) ( Int fd, void* buf, Int count, OffT offset );

extern SizeT VG_(mkstemp_fullname_bufsz) ( SizeT part_of_name_len );

/* Create and open (-rw------) a tmp file name incorporating said arg.
   Returns -1 on failure, else the fd of the file.  The file name is
   written to the memory pointed to be fullname. The number of bytes written
   is equal to VG_(mkstemp_fullname_bufsz)(VG_(strlen)(part_of_name)). */
extern Int VG_(mkstemp) ( const HChar* part_of_name, HChar* fullname );

extern Bool VG_(record_startup_wd) ( void );

#endif   

