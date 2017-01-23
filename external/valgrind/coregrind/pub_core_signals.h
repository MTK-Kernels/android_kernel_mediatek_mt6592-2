

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

#ifndef __PUB_CORE_SIGNALS_H
#define __PUB_CORE_SIGNALS_H


#include "pub_tool_signals.h"       
#include "pub_core_vki.h"           

extern Int VG_(max_signal);

extern const HChar *VG_(signame)(Int sigNo);

#define VG_SIGVGKILL       (VG_(max_signal)-0)
#define VG_SIGVGRTUSERMAX  (VG_(max_signal)-1)

extern void VG_(sigstartup_actions) ( void );

extern void VG_(poll_signals) ( ThreadId );

extern SysRes VG_(do_sys_sigaltstack) ( ThreadId tid, vki_stack_t* ss,
                                                      vki_stack_t* oss );
extern SysRes VG_(do_sys_sigaction)   ( Int signo, 
                                        const vki_sigaction_toK_t* new_act, 
                                        vki_sigaction_fromK_t* old_act );
extern SysRes VG_(do_sys_sigprocmask) ( ThreadId tid, Int how, 
                                        vki_sigset_t* set,
                                        vki_sigset_t* oldset );

extern void VG_(clear_out_queued_signals) 
                  ( ThreadId tid,  vki_sigset_t* saved_mask );

extern void VG_(kill_self)(Int sigNo);

extern void VG_(synth_fault)        (ThreadId tid);
extern void VG_(synth_fault_mapping)(ThreadId tid, Addr addr);
extern void VG_(synth_fault_perms)  (ThreadId tid, Addr addr);
extern void VG_(synth_sigill)       (ThreadId tid, Addr addr);
extern void VG_(synth_sigtrap)      (ThreadId tid);
extern void VG_(synth_sigbus)       (ThreadId tid);
extern void VG_(synth_sigfpe)       (ThreadId tid, UInt code);

extern Bool VG_(extend_stack)(ThreadId tid, Addr addr);

extern void VG_(set_default_handler)(Int sig);

#endif   

