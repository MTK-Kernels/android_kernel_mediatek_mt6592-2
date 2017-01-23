

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

#ifndef __PUB_TOOL_MACHINE_H
#define __PUB_TOOL_MACHINE_H

#include "pub_tool_basics.h"           
#include "libvex.h"                    

#if defined(VGP_x86_linux)
#  define VG_MIN_INSTR_SZB          1  
#  define VG_MAX_INSTR_SZB         16  
#  define VG_CLREQ_SZB             14  
                                       
#  define VG_STACK_REDZONE_SZB      0  

#elif defined(VGP_amd64_linux)
#  define VG_MIN_INSTR_SZB          1
#  define VG_MAX_INSTR_SZB         16
#  define VG_CLREQ_SZB             19
#  define VG_STACK_REDZONE_SZB    128

#elif defined(VGP_ppc32_linux)
#  define VG_MIN_INSTR_SZB          4
#  define VG_MAX_INSTR_SZB          4 
#  define VG_CLREQ_SZB             20
#  define VG_STACK_REDZONE_SZB      0

#elif defined(VGP_ppc64be_linux)  || defined(VGP_ppc64le_linux)
#  define VG_MIN_INSTR_SZB          4
#  define VG_MAX_INSTR_SZB          4 
#  define VG_CLREQ_SZB             20
#  define VG_STACK_REDZONE_SZB    288  
                                       
                                       

#elif defined(VGP_arm_linux)
#  define VG_MIN_INSTR_SZB          2
#  define VG_MAX_INSTR_SZB          4 
#  define VG_CLREQ_SZB             20
#  define VG_STACK_REDZONE_SZB      0

#elif defined(VGP_arm64_linux)
#  define VG_MIN_INSTR_SZB          4
#  define VG_MAX_INSTR_SZB          4 
#  define VG_CLREQ_SZB             20
#  define VG_STACK_REDZONE_SZB      0

#elif defined(VGP_s390x_linux)
#  define VG_MIN_INSTR_SZB          2
#  define VG_MAX_INSTR_SZB          6
#  define VG_CLREQ_SZB             10
#  define VG_STACK_REDZONE_SZB      0  

#elif defined(VGP_x86_darwin)
#  define VG_MIN_INSTR_SZB          1  
#  define VG_MAX_INSTR_SZB         16  
#  define VG_CLREQ_SZB             14  
                                       
#  define VG_STACK_REDZONE_SZB      0  

#elif defined(VGP_amd64_darwin)
#  define VG_MIN_INSTR_SZB          1
#  define VG_MAX_INSTR_SZB         16
#  define VG_CLREQ_SZB             19
#  define VG_STACK_REDZONE_SZB    128

#elif defined(VGP_mips32_linux)
#  define VG_MIN_INSTR_SZB          4
#  define VG_MAX_INSTR_SZB          4 
#  define VG_CLREQ_SZB             20
#  define VG_STACK_REDZONE_SZB      0

#elif defined(VGP_mips64_linux)
#  define VG_MIN_INSTR_SZB          4
#  define VG_MAX_INSTR_SZB          4 
#  define VG_CLREQ_SZB             20
#  define VG_STACK_REDZONE_SZB      0

#elif defined(VGP_tilegx_linux)
#  define VG_MIN_INSTR_SZB          8
#  define VG_MAX_INSTR_SZB          8
#  define VG_CLREQ_SZB             24
#  define VG_STACK_REDZONE_SZB      0

#else
#  error Unknown platform
#endif

Addr VG_(get_IP) ( ThreadId tid );
Addr VG_(get_SP) ( ThreadId tid );


void
VG_(get_shadow_regs_area) ( ThreadId tid, 
                            UChar* dst,
                            Int shadowNo, PtrdiffT offset, SizeT size );
void
VG_(set_shadow_regs_area) ( ThreadId tid, 
                            Int shadowNo, PtrdiffT offset, SizeT size,
                            const UChar* src );

extern void VG_(apply_to_GP_regs)(void (*f)(ThreadId tid,
                                            const HChar* regname, UWord val));

extern void VG_(thread_stack_reset_iter) ( ThreadId* tid );
extern Bool VG_(thread_stack_next)       ( ThreadId* tid,
                                           Addr* stack_min, 
                                           Addr* stack_max );

extern Addr VG_(thread_get_stack_max) ( ThreadId tid );

extern SizeT VG_(thread_get_stack_size) ( ThreadId tid );

extern Addr VG_(thread_get_altstack_min) ( ThreadId tid );

extern SizeT VG_(thread_get_altstack_size) ( ThreadId tid );

extern void* VG_(fnptr_to_fnentry)( void* );

extern Int VG_(machine_get_size_of_largest_guest_register) ( void );

extern void VG_(machine_get_VexArchInfo)( VexArch*,
                                          VexArchInfo* );

#endif   

