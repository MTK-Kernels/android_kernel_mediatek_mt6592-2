

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

#ifndef __PUB_CORE_SYSWRAP_H
#define __PUB_CORE_SYSWRAP_H

#include "pub_core_basics.h"        
#include "pub_core_threadstate.h"   


extern void VG_(main_thread_wrapper_NORETURN)(ThreadId tid);

extern void VG_(client_syscall) ( ThreadId tid, UInt trc );

extern void VG_(post_syscall)   ( ThreadId tid );

extern void VG_(clear_syscallInfo) ( Int tid );

extern Bool VG_(is_in_syscall) ( Int tid );

extern void VG_(fixup_guest_state_after_syscall_interrupted)(
               ThreadId tid,
               Addr     ip, 
               SysRes   sysret,
               Bool     restart
            );

extern void VG_(reap_threads)(ThreadId self);

extern void VG_(cleanup_thread) ( ThreadArchState* );

extern void VG_(init_preopened_fds) ( void );
extern void VG_(show_open_fds) ( const HChar* when );

extern void (* VG_(address_of_m_main_shutdown_actions_NORETURN) )
            (ThreadId,VgSchedReturnCode);

#endif   


