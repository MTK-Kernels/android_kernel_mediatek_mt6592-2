

/*
   This file is part of Valgrind, a dynamic binary instrumentation
   framework.

   Copyright (C) 2006-2013 OpenWorks LLP
      info@open-works.co.uk

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

#ifndef __PRIV_ASPACEMGR_H
#define __PRIV_ASPACEMGR_H


#include "pub_core_basics.h"     
#include "pub_core_vkiscnums.h"  
#include "pub_core_vki.h"        
                                 

#include "pub_core_debuglog.h"   

#include "pub_core_libcbase.h"   
                                 
                                 

#include "pub_core_libcassert.h" 

#include "pub_core_syscall.h"    
                                 
                                 

#include "pub_core_options.h"    

#include "pub_core_aspacemgr.h"  




__attribute__ ((noreturn))
extern void   ML_(am_exit) ( Int status );
__attribute__ ((noreturn))
extern void   ML_(am_barf) ( const HChar* what );
__attribute__ ((noreturn))
extern void   ML_(am_barf_toolow) ( const HChar* what );

__attribute__ ((noreturn))
extern void   ML_(am_assert_fail) ( const HChar* expr,
                                    const HChar* file,
                                    Int line, 
                                    const HChar* fn );

#define aspacem_assert(expr)                              \
  ((void) (LIKELY(expr) ? 0 :                             \
           (ML_(am_assert_fail)(#expr,                    \
                                __FILE__, __LINE__,       \
                                __PRETTY_FUNCTION__))))

extern Int    ML_(am_getpid)( void );

extern UInt   ML_(am_sprintf) ( HChar* buf, const HChar *format, ... );

extern SysRes ML_(am_do_munmap_NO_NOTIFY)(Addr start, SizeT length);

extern SysRes ML_(am_do_extend_mapping_NO_NOTIFY)( 
                 Addr  old_addr, 
                 SizeT old_len,
                 SizeT new_len 
              );
extern SysRes ML_(am_do_relocate_nooverlap_mapping_NO_NOTIFY)( 
                 Addr old_addr, Addr old_len, 
                 Addr new_addr, Addr new_len 
              );


extern SysRes ML_(am_open)  ( const HChar* pathname, Int flags, Int mode );
extern void   ML_(am_close) ( Int fd );
extern Int    ML_(am_read)  ( Int fd, void* buf, Int count);
extern Int    ML_(am_readlink) ( const HChar* path, HChar* buf, UInt bufsiz );
extern Int    ML_(am_fcntl) ( Int fd, Int cmd, Addr arg );

extern
Bool ML_(am_get_fd_d_i_m)( Int fd, 
                           ULong* dev, 
                           ULong* ino, UInt* mode );

extern
Bool ML_(am_resolve_filename) ( Int fd, HChar* buf, Int nbuf );


extern void ML_(am_do_sanity_check)( void );


void ML_(am_segnames_init)(void);
void ML_(am_show_segnames)(Int logLevel, const HChar *prefix);

Int ML_(am_allocate_segname)(const HChar *name);

void ML_(am_inc_refcount)(Int);
void ML_(am_dec_refcount)(Int);

Bool ML_(am_sane_segname)(Int fnIdx);

const HChar *ML_(am_get_segname)(Int fnIdx);

Int ML_(am_segname_get_seqnr)(Int fnIdx);

#endif   

