

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

#ifndef __PUB_CORE_SCHEDULER_H
#define __PUB_CORE_SCHEDULER_H

#include "pub_core_basics.h"        
#include "pub_core_threadstate.h"   


extern ThreadId VG_(alloc_ThreadState)(void);

extern void VG_(exit_thread)(ThreadId tid);

extern void VG_(get_thread_out_of_syscall)(ThreadId tid);

extern void VG_(nuke_all_threads_except) ( ThreadId me,
                                           VgSchedReturnCode reason );

extern void VG_(acquire_BigLock) ( ThreadId tid, const HChar* who );

extern void VG_(acquire_BigLock_LL) ( const HChar* who );

extern void VG_(release_BigLock) ( ThreadId tid,
                                   ThreadStatus state, const HChar* who );

extern void VG_(release_BigLock_LL) ( const HChar* who );

extern Bool VG_(owns_BigLock_LL) ( ThreadId tid );

extern void VG_(vg_yield)(void);

extern VgSchedReturnCode VG_(scheduler) ( ThreadId tid );

extern ThreadId VG_(scheduler_init_phase1) ( void );

extern void VG_(scheduler_init_phase2) ( ThreadId main_tid, 
                                         Addr     clstack_end, 
                                         SizeT    clstack_size );

extern void VG_(disable_vgdb_poll) (void );
extern void VG_(force_vgdb_poll) ( void );

extern void VG_(print_scheduler_stats) ( void );

extern Bool VG_(in_generated_code);

extern void VG_(sanity_check_general) ( Bool force_expensive );

#endif   

