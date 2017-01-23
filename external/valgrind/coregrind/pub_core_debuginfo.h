

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

#ifndef __PUB_CORE_DEBUGINFO_H
#define __PUB_CORE_DEBUGINFO_H


#include "pub_tool_debuginfo.h"

extern void VG_(di_initialise) ( void );

#if defined(VGO_linux) || defined(VGO_darwin)
extern ULong VG_(di_notify_mmap)( Addr a, Bool allow_SkFileV, Int use_fd );

extern void VG_(di_notify_munmap)( Addr a, SizeT len );

extern void VG_(di_notify_mprotect)( Addr a, SizeT len, UInt prot );

extern void VG_(di_notify_pdb_debuginfo)( Int fd, Addr avma,
                                          SizeT total_size,
                                          PtrdiffT bias );

extern void VG_(di_notify_vm_protect)( Addr a, SizeT len, UInt prot );
#endif

extern void VG_(di_discard_ALL_debuginfo)( void );

extern
Bool VG_(get_fnname_raw) ( Addr a, const HChar** buf );

extern
Bool VG_(get_fnname_no_cxx_demangle) ( Addr a, const HChar** buf,
                                       const InlIPCursor* iipc );

extern
Bool VG_(get_inst_offset_in_function)( Addr a, PtrdiffT* offset );


#if defined(VGA_amd64) || defined(VGA_x86)
typedef
   struct { Addr xip; Addr xsp; Addr xbp; }
   D3UnwindRegs;
#elif defined(VGA_arm)
typedef
   struct { Addr r15; Addr r14; Addr r13; Addr r12; Addr r11; Addr r7; }
   D3UnwindRegs;
#elif defined(VGA_arm64)
typedef
   struct { Addr pc; Addr sp; Addr x30; Addr x29; } 
   D3UnwindRegs;
#elif defined(VGA_ppc32) || defined(VGA_ppc64be) || defined(VGA_ppc64le)
typedef
   UChar  
   D3UnwindRegs;
#elif defined(VGA_s390x)
typedef
   struct { Addr ia; Addr sp; Addr fp; Addr lr;}
   D3UnwindRegs;
#elif defined(VGA_mips32) || defined(VGA_mips64)
typedef
   struct { Addr pc; Addr sp; Addr fp; Addr ra; }
   D3UnwindRegs;
#elif defined(VGA_tilegx)
typedef
   struct { Addr pc; Addr sp; Addr fp; Addr lr; }
   D3UnwindRegs;
#else
#  error "Unsupported arch"
#endif

extern Bool VG_(use_CF_info) ( D3UnwindRegs* uregs,
                               Addr min_accessible,
                               Addr max_accessible );

extern UInt VG_(CF_info_generation) (void);



extern Bool VG_(use_FPO_info) ( Addr* ipP,
                                Addr* spP,
                                Addr* fpP,
                                Addr min_accessible,
                                Addr max_accessible );

typedef
   struct {
      Addr main;      
#     if defined(VGA_ppc64be) || defined(VGA_ppc64le)
      Addr tocptr;    
#     endif
#     if defined(VGA_ppc64le)
      Addr local_ep;  
#     endif
   }
   SymAVMAs;

#if defined(VGA_ppc64be) || defined(VGA_ppc64le)
# define GET_TOCPTR_AVMA(_sym_avmas)          (_sym_avmas).tocptr
# define SET_TOCPTR_AVMA(_sym_avmas, _val)    (_sym_avmas).tocptr = (_val)
#else
# define GET_TOCPTR_AVMA(_sym_avmas)          ((Addr)0)
# define SET_TOCPTR_AVMA(_sym_avmas, _val)    
#endif

#if defined(VGA_ppc64le)
# define GET_LOCAL_EP_AVMA(_sym_avmas)        (_sym_avmas).local_ep
# define SET_LOCAL_EP_AVMA(_sym_avmas, _val)  (_sym_avmas).local_ep = (_val)
#else
# define GET_LOCAL_EP_AVMA(_sym_avmas)        ((Addr)0)
# define SET_LOCAL_EP_AVMA(_sym_avmas, _val)  
#endif

Int  VG_(DebugInfo_syms_howmany) ( const DebugInfo *di );
void VG_(DebugInfo_syms_getidx)  ( const DebugInfo *di, 
                                   Int idx,
                                   SymAVMAs* ad,
                                   UInt*     size,
                                   const HChar**   pri_name,
                                   const HChar***  sec_names,
                                   Bool*     isText,
                                   Bool*     isIFunc );
extern Addr VG_(get_tocptr) ( Addr guest_code_addr );

extern
Bool VG_(lookup_symbol_SLOW)(const HChar* sopatt, const HChar* name,
                             SymAVMAs* avmas);

#endif   

