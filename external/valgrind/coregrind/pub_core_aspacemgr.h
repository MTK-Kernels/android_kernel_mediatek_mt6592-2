

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

#ifndef __PUB_CORE_ASPACEMGR_H
#define __PUB_CORE_ASPACEMGR_H


#include "pub_tool_aspacemgr.h"





extern Addr VG_(am_startup) ( Addr sp_at_startup );

extern Bool VG_(am_is_valid_for_aspacem_minAddr)( Addr addr,
                                                  const HChar **errmsg );



extern NSegment const* VG_(am_next_nsegment) ( const NSegment* here,
                                               Bool fwds );

extern Bool VG_(am_is_valid_for_valgrind)
   ( Addr start, SizeT len, UInt prot );

extern Bool VG_(am_is_valid_for_client_or_free_or_resvn)
   ( Addr start, SizeT len, UInt prot );

extern Bool VG_(am_addr_is_in_extensible_client_stack)( Addr addr );

extern ULong VG_(am_get_anonsize_total)( void );

extern void VG_(am_show_nsegments) ( Int logLevel, const HChar* who );



extern Bool VG_(am_do_sync_check) ( const HChar* fn, 
                                    const HChar* file, Int line );


typedef
   struct {
      enum { MFixed, MHint, MAny } rkind;
      Addr start;
      Addr len;
   }
   MapRequest;

extern Addr VG_(am_get_advisory)
   ( const MapRequest* req, Bool forClient, Bool* ok );

extern Addr VG_(am_get_advisory_client_simple) 
   ( Addr start, SizeT len, Bool* ok );

extern Bool VG_(am_covered_by_single_free_segment)
   ( Addr start, SizeT len);

extern Bool VG_(am_notify_client_mmap)
   ( Addr a, SizeT len, UInt prot, UInt flags, Int fd, Off64T offset );

extern Bool VG_(am_notify_client_shmat)( Addr a, SizeT len, UInt prot );

extern Bool VG_(am_notify_mprotect)( Addr start, SizeT len, UInt prot );

extern Bool VG_(am_notify_munmap)( Addr start, SizeT len );

extern SysRes VG_(am_do_mmap_NO_NOTIFY)
   ( Addr start, SizeT length, UInt prot, UInt flags, Int fd, Off64T offset);




extern SysRes VG_(am_mmap_file_fixed_client)
   ( Addr start, SizeT length, UInt prot, Int fd, Off64T offset );
extern SysRes VG_(am_mmap_named_file_fixed_client)
   ( Addr start, SizeT length, UInt prot, Int fd, Off64T offset, const HChar *name );

extern SysRes VG_(am_mmap_anon_fixed_client)
   ( Addr start, SizeT length, UInt prot );


extern SysRes VG_(am_mmap_anon_float_client) ( SizeT length, Int prot );

extern SysRes VG_(am_mmap_anon_float_valgrind)( SizeT cszB );

extern SysRes VG_(am_mmap_file_float_valgrind)
   ( SizeT length, UInt prot, Int fd, Off64T offset );

extern SysRes VG_(am_shared_mmap_file_float_valgrind)
   ( SizeT length, UInt prot, Int fd, Off64T offset );

extern SysRes VG_(am_mmap_client_heap) ( SizeT length, Int prot );

extern SysRes VG_(am_munmap_client)( Bool* need_discard,
                                     Addr start, SizeT length );

extern Bool VG_(am_change_ownership_v_to_c)( Addr start, SizeT len );

extern void VG_(am_set_segment_hasT)( Addr addr );


extern Bool VG_(am_create_reservation) 
   ( Addr start, SizeT length, ShrinkMode smode, SSizeT extra );

extern const NSegment *VG_(am_extend_into_adjacent_reservation_client) 
   ( Addr addr, SSizeT delta, Bool *overflow );


extern const NSegment *VG_(am_extend_map_client)( Addr addr, SizeT delta );

extern Bool VG_(am_relocate_nooverlap_client)( Bool* need_discard,
                                               Addr old_addr, SizeT old_len,
                                               Addr new_addr, SizeT new_len );


#if defined(VGP_ppc32_linux) \
    || defined(VGP_ppc64be_linux) || defined(VGP_ppc64le_linux)	\
    || defined(VGP_mips32_linux) || defined(VGP_mips64_linux) \
    || defined(VGP_arm64_linux) || defined(VGP_tilegx_linux)
# define VG_STACK_GUARD_SZB  65536  
#else
# define VG_STACK_GUARD_SZB  8192   
#endif
# define VG_DEFAULT_STACK_ACTIVE_SZB 1048576 

typedef struct _VgStack VgStack;



extern VgStack* VG_(am_alloc_VgStack)( Addr* initial_sp );

extern SizeT VG_(am_get_VgStack_unused_szB)( const VgStack* stack,
                                             SizeT limit ); 

#if defined(VGO_darwin)
typedef 
   struct {
      Bool   is_added;  
      Addr   start;
      SizeT  end;
      UInt   prot;      
      Off64T offset;    
   }
   ChangedSeg;

extern Bool VG_(get_changed_segments)(
      const HChar* when, const HChar* where, ChangedSeg* css,
      Int css_size, Int* css_used);
#endif

#endif   

