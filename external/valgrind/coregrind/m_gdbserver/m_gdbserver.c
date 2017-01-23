

/*
   This file is part of Valgrind, a dynamic binary instrumentation
   framework.

   Copyright (C) 2011-2013 Philippe Waroquiers

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

#include "pub_core_basics.h"
#include "pub_core_vki.h"
#include "pub_core_debuglog.h"
#include "pub_core_libcproc.h"
#include "pub_core_libcprint.h"
#include "pub_core_mallocfree.h"
#include "pub_core_threadstate.h"
#include "pub_core_gdbserver.h"
#include "pub_core_options.h"
#include "pub_core_transtab.h"
#include "pub_core_hashtable.h"
#include "pub_core_xarray.h"
#include "pub_core_libcassert.h"
#include "pub_core_libcbase.h"
#include "pub_core_libcsignal.h"
#include "pub_core_signals.h"
#include "pub_core_machine.h"     
#include "pub_core_debuginfo.h"
#include "pub_core_scheduler.h"
#include "pub_core_syswrap.h"

#include "server.h"

Int VG_(dyn_vgdb_error);

VG_REGPARM(1)
void VG_(helperc_CallDebugger) ( HWord iaddr );
VG_REGPARM(1)
void VG_(helperc_invalidate_if_not_gdbserved) ( Addr addr );
static void invalidate_current_ip (ThreadId tid, const HChar *who);

typedef
   enum {
      init_reason,    
      vgdb_reason,    
      core_reason,    
      break_reason,   
      watch_reason,   
      signal_reason,  
      exit_reason}    
    CallReason;

static const HChar* ppCallReason(CallReason reason)
{
   switch (reason) {
   case init_reason:    return "init_reason";
   case vgdb_reason:    return "vgdb_reason";
   case core_reason:    return "core_reason";
   case break_reason:   return "break_reason";
   case watch_reason:   return "watch_reason";
   case signal_reason:  return "signal_reason";
   case exit_reason:    return "exit_reason";
   default: vg_assert (0);
   }
}

static Addr ignore_this_break_once = 0;


static void call_gdbserver ( ThreadId tid , CallReason reason);

static HChar* sym (Addr addr, Bool is_code)
{
   static HChar *buf[2];
   static int w = 0;
   PtrdiffT offset;
   if (w == 2) w = 0;

   if (is_code) {
      const HChar *name;
      name = VG_(describe_IP) (addr, NULL);
      if (buf[w]) VG_(free)(buf[w]);
      buf[w] = VG_(strdup)("gdbserver sym", name);
   } else {
      const HChar *name;
      VG_(get_datasym_and_offset) (addr, &name, &offset);
      if (buf[w]) VG_(free)(buf[w]);
      buf[w] = VG_(strdup)("gdbserver sym", name);
   }
   return buf[w++];
}

static int gdbserver_called = 0;
static int gdbserver_exited = 0;

static void* gs_alloc (const HChar* cc, SizeT sz)
{
   return VG_(malloc)(cc, sz);
}
static void gs_free (void* ptr)
{
   VG_(free)(ptr);
}

typedef
   enum {
     GS_break,
     GS_jump
   }
   GS_Kind;

typedef
   struct _GS_Address {
      struct _GS_Address* next;
      Addr    addr;
      GS_Kind kind;
   }
   GS_Address;

static VgHashTable *gs_addresses = NULL;

static Addr HT_addr ( Addr addr )
{
#if defined(VGA_arm)
  return addr & ~(Addr)1;
#else
  return addr;
#endif
}

static void add_gs_address (Addr addr, GS_Kind kind, const HChar* from)
{
   GS_Address *p;

   p = VG_(malloc)(from, sizeof(GS_Address));
   p->addr = HT_addr (addr);
   p->kind = kind;
   VG_(HT_add_node)(gs_addresses, p);
   if (VG_(clo_vgdb) != Vg_VgdbFull)
      VG_(discard_translations) (addr, 2, from);
}

static void remove_gs_address (GS_Address* g, const HChar* from)
{
   VG_(HT_remove) (gs_addresses, g->addr);
   
   if (VG_(clo_vgdb) != Vg_VgdbFull)
      VG_(discard_translations) (g->addr, 2, from);
   VG_(free) (g);
}

const HChar* VG_(ppPointKind) (PointKind kind)
{
   switch(kind) {
   case software_breakpoint: return "software_breakpoint";
   case hardware_breakpoint: return "hardware_breakpoint";
   case write_watchpoint:    return "write_watchpoint";
   case read_watchpoint:     return "read_watchpoint";
   case access_watchpoint:   return "access_watchpoint";
   default:                  return "???wrong PointKind";
   }
}

typedef
   struct _GS_Watch {
      Addr    addr;
      SizeT   len;
      PointKind kind;
   }
   GS_Watch;

static XArray* gs_watches = NULL;

static inline GS_Watch* index_gs_watches(Word i)
{
   return *(GS_Watch **) VG_(indexXA) (gs_watches, i);
}

static GS_Watch* lookup_gs_watch (Addr addr, SizeT len, PointKind kind,
                                  Word* g_ix)
{
   const Word n_elems = VG_(sizeXA) (gs_watches);
   Word i;
   GS_Watch *g;

   for (i = 0; i < n_elems; i++) {
      g = index_gs_watches(i);
      if (g->addr == addr && g->len == len && g->kind == kind) {
         
         *g_ix = i;
         return g;
      }
   }

   
   *g_ix = -1;
   return NULL;
}


static void breakpoint (Bool insert, CORE_ADDR addr)
{
   GS_Address *g;

   g = VG_(HT_lookup) (gs_addresses, (UWord)HT_addr(addr));
   if (insert) {
      
      if (g == NULL) {
         add_gs_address (addr, GS_break, "m_gdbserver breakpoint insert");
      } else {
         g->kind = GS_break;
      }
   } else {
      
      if (g != NULL && g->kind == GS_break) {
         if (valgrind_single_stepping()) {
            
            g->kind = GS_jump;
         } else {
            remove_gs_address (g, "m_gdbserver breakpoint remove");
         }
      } else {
         dlog (1, "remove break addr %p %s\n",
               C2v(addr), (g == NULL ? 
                           "NULL" : 
                           (g->kind == GS_jump ? "GS_jump" : "GS_break")));
      }
   }
}

static Bool (*tool_watchpoint) (PointKind kind, 
                                Bool insert, 
                                Addr addr,
                                SizeT len) = NULL;
void VG_(needs_watchpoint) (Bool (*watchpoint) (PointKind kind, 
                                                Bool insert, 
                                                Addr addr,
                                                SizeT len))
{
   tool_watchpoint = watchpoint;
}
     
Bool VG_(gdbserver_point) (PointKind kind, Bool insert,
                           CORE_ADDR addr, int len)
{
   Bool res;
   GS_Watch *g;
   Word g_ix;
   Bool is_code = kind == software_breakpoint || kind == hardware_breakpoint;

   dlog(1, "%s %s at addr %p %s\n",
        (insert ? "insert" : "remove"), 
        VG_(ppPointKind) (kind),
        C2v(addr), 
        sym(addr, is_code));

   if (is_code) {
      breakpoint (insert, addr);
      return True;
   }

   vg_assert (kind == access_watchpoint 
              || kind == read_watchpoint 
              || kind == write_watchpoint);

   if (tool_watchpoint == NULL)
      return False;

   res = (*tool_watchpoint) (kind, insert, addr, len);
   if (!res) 
      return False; 

   
   

   g = lookup_gs_watch (addr, len, kind, &g_ix);
   if (insert) {
      if (g == NULL) {
         g = VG_(malloc)("gdbserver_point watchpoint", sizeof(GS_Watch));
         g->addr = addr;
         g->len  = len;
         g->kind = kind;
         VG_(addToXA)(gs_watches, &g);
      } else {
         dlog(1, 
              "VG_(gdbserver_point) addr %p len %d kind %s already inserted\n",
               C2v(addr), len, VG_(ppPointKind) (kind));
      }
   } else {
      if (g != NULL) {
         VG_(removeIndexXA) (gs_watches, g_ix);
         VG_(free) (g);
      } else {
         dlog(1, 
              "VG_(gdbserver_point) addr %p len %d kind %s already deleted?\n",
              C2v(addr), len, VG_(ppPointKind) (kind));
      }
   }  
   return True;
}

Bool VG_(has_gdbserver_breakpoint) (Addr addr)
{
   GS_Address *g;
   if (!gdbserver_called)
      return False;
   g = VG_(HT_lookup) (gs_addresses, (UWord)HT_addr(addr));
   return (g != NULL && g->kind == GS_break);
}

Bool VG_(is_watched)(PointKind kind, Addr addr, Int szB)
{
   Word n_elems;
   GS_Watch* g;
   Word i;
   Bool watched = False;
   const ThreadId tid = VG_(running_tid);

   if (!gdbserver_called)
      return False;

   n_elems = VG_(sizeXA) (gs_watches);

   Addr to = addr + szB; 

   vg_assert (kind == access_watchpoint 
              || kind == read_watchpoint 
              || kind == write_watchpoint);
   dlog(1, "tid %d VG_(is_watched) %s addr %p szB %d\n",
        tid, VG_(ppPointKind) (kind), C2v(addr), szB);

   for (i = 0; i < n_elems; i++) {
      g = index_gs_watches(i);
      switch (g->kind) {
      case software_breakpoint:
      case hardware_breakpoint:
         break;
      case access_watchpoint:
      case read_watchpoint:
      case write_watchpoint:
         if (to <= g->addr || addr >= (g->addr + g->len))
            
            continue;

         watched = True; 

         if (kind == access_watchpoint
             || g->kind == access_watchpoint
             || g->kind == kind) {

            /* set the watchpoint stop address to the first read or written. */
            if (g->addr <= addr) {
               VG_(set_watchpoint_stop_address) (addr);
            } else {
               VG_(set_watchpoint_stop_address) (g->addr);
            }

            if (kind == write_watchpoint) {
               valgrind_set_single_stepping (True);
               invalidate_current_ip (tid, "m_gdbserver write watchpoint");
            } else {
               call_gdbserver (tid, watch_reason);
               VG_(set_watchpoint_stop_address) ((Addr) 0);
            }
            return True; 
         }
         break;
      default:
         vg_assert (0);
      }
   }
   return watched;
}

static VgVgdb VG_(gdbserver_instrumentation_needed) (const VexGuestExtents* vge)
{
   GS_Address* g;
   int e;

   if (!gdbserver_called)
      return Vg_VgdbNo;

   if (valgrind_single_stepping()) {
      dlog(2, "gdbserver_instrumentation_needed due to single stepping\n");
      return Vg_VgdbYes;
   }

   if (VG_(clo_vgdb) == Vg_VgdbYes && VG_(HT_count_nodes) (gs_addresses) == 0)
      return Vg_VgdbNo;

   VG_(HT_ResetIter) (gs_addresses);
   while ((g = VG_(HT_Next) (gs_addresses))) {
      for (e = 0; e < vge->n_used; e++) {
         if (g->addr >= HT_addr(vge->base[e]) 
             && g->addr < HT_addr(vge->base[e]) + vge->len[e]) {
            dlog(2,
                 "gdbserver_instrumentation_needed %p %s reason %s\n",
                 C2v(g->addr), sym(g->addr,  True),
                 (g->kind == GS_jump ? "GS_jump" : "GS_break"));
            return Vg_VgdbYes;
         }
      }
   }

   if (VG_(clo_vgdb) == Vg_VgdbFull) {
      dlog(4, "gdbserver_instrumentation_needed"
           " due to VG_(clo_vgdb) == Vg_VgdbFull\n");
      return Vg_VgdbFull;
   }


   return Vg_VgdbNo;
}

static void clear_gdbserved_addresses(Bool clear_only_jumps)
{
   GS_Address** ag;
   UInt n_elems;
   int i;

   dlog(1,
        "clear_gdbserved_addresses: scanning hash table nodes %d\n", 
        VG_(HT_count_nodes) (gs_addresses));
   ag = (GS_Address**) VG_(HT_to_array) (gs_addresses, &n_elems);
   for (i = 0; i < n_elems; i++)
      if (!clear_only_jumps || ag[i]->kind == GS_jump)
         remove_gs_address (ag[i], "clear_gdbserved_addresses");
   VG_(free) (ag);
}

static void clear_watched_addresses(void)
{
   GS_Watch* g;
   const Word n_elems = VG_(sizeXA) (gs_watches);
   Word i;

   dlog(1,
        "clear_watched_addresses: %ld elements\n", 
        n_elems);
   
   for (i = 0; i < n_elems; i++) {
      g = index_gs_watches(i);
      if (!VG_(gdbserver_point) (g->kind,
                                  False,
                                 g->addr,
                                 g->len)) {
         vg_assert (0);
      }
   }

   VG_(deleteXA) (gs_watches);
   gs_watches = NULL;
}

static void invalidate_if_jump_not_yet_gdbserved (Addr addr, const HChar* from)
{
   if (VG_(HT_lookup) (gs_addresses, (UWord)HT_addr(addr)))
      return;
   add_gs_address (addr, GS_jump, from);
}

static void invalidate_current_ip (ThreadId tid, const HChar *who)
{
   invalidate_if_jump_not_yet_gdbserved (VG_(get_IP) (tid), who);
}

Bool VG_(gdbserver_init_done) (void)
{
   return gdbserver_called > 0;
}

Bool VG_(gdbserver_stop_at) (VgdbStopAt stopat)
{
   return gdbserver_called > 0 && VgdbStopAtiS(stopat, VG_(clo_vgdb_stop_at));
}

void VG_(gdbserver_prerun_action) (ThreadId tid)
{
   
   
   if (VG_(dyn_vgdb_error) == 0 
       || VgdbStopAtiS(VgdbStopAt_Startup, VG_(clo_vgdb_stop_at))) {
      VG_(umsg)("(action at startup) vgdb me ... \n");
      VG_(gdbserver)(tid); 
   } else {
      if (VG_(gdbserver_activity) (tid))
         VG_(gdbserver) (tid);
   }
}

static void gdbserver_cleanup_in_child_after_fork(ThreadId me)
{
   dlog(1, "thread %d gdbserver_cleanup_in_child_after_fork pid %d\n",
        me, VG_(getpid) ());

   
   remote_finish(reset_after_fork);

   if (gdbserver_called) {
      gdbserver_called = 0;
      vg_assert (gs_addresses != NULL);
      vg_assert (gs_watches != NULL);
      clear_gdbserved_addresses( False);
      VG_(HT_destruct) (gs_addresses, VG_(free));
      gs_addresses = NULL;
      clear_watched_addresses();
   } else {
      vg_assert (gs_addresses == NULL);
      vg_assert (gs_watches == NULL);
   }

   
   if (VG_(clo_trace_children)) {
      VG_(gdbserver_prerun_action) (me);
   }
}

static void call_gdbserver ( ThreadId tid , CallReason reason)
{
   ThreadState*     tst = VG_(get_ThreadState)(tid);
   int stepping;
   Addr saved_pc;

   dlog(1, 
        "entering call_gdbserver %s ... pid %d tid %d status %s "
        "sched_jmpbuf_valid %d\n",
        ppCallReason (reason),
        VG_(getpid) (), tid, VG_(name_of_ThreadStatus)(tst->status),
        tst->sched_jmpbuf_valid);

   if (reason == exit_reason) {
      server_main();
      return;
   }

   vg_assert(VG_(is_valid_tid)(tid));
   saved_pc = VG_(get_IP) (tid);

   if (gdbserver_exited) {
      dlog(0, "call_gdbserver called when gdbserver_exited %d\n",
           gdbserver_exited);
      return;
   }

   if (gdbserver_called == 0) {
      vg_assert (gs_addresses == NULL);
      vg_assert (gs_watches == NULL);
      gs_addresses = VG_(HT_construct)( "gdbserved_addresses" );
      gs_watches = VG_(newXA)(gs_alloc,
                              "gdbserved_watches",
                              gs_free,
                              sizeof(GS_Watch*));
      VG_(atfork)(NULL, NULL, gdbserver_cleanup_in_child_after_fork);
   }
   vg_assert (gs_addresses != NULL);
   vg_assert (gs_watches != NULL);
   
   gdbserver_called++;

   
   if (gdbserver_called == 1)
      gdbserver_init();

   if (reason == init_reason || gdbserver_called == 1)
      remote_open(VG_(clo_vgdb_prefix));

   if (reason == init_reason) {
      return;
   }

   stepping = valgrind_single_stepping();

   server_main();

   ignore_this_break_once = valgrind_get_ignore_break_once();
   if (ignore_this_break_once)
      dlog(1, "!!! will ignore_this_break_once %s\n", 
           sym(ignore_this_break_once,  True));
      

   if (valgrind_single_stepping()) {
      if (!stepping && tid != 0) {
         invalidate_current_ip (tid, "m_gdbserver single step");
      }
   } else {
      if (stepping)
         clear_gdbserved_addresses( True);
   }
   
   if (gdbserver_called > 1)
      VG_(sanity_check_general) ( False);

   if (VG_(get_IP) (tid) != saved_pc) {
      dlog(1, "tid %d %s PC changed from %s to %s\n",
           tid, VG_(name_of_ThreadStatus) (tst->status),
           sym(saved_pc,  True),
           sym(VG_(get_IP) (tid),  True));
      if (tst->status == VgTs_Yielding) {
         SysRes sres;
         VG_(memset)(&sres, 0, sizeof(SysRes));
         VG_(acquire_BigLock)(tid, "gdbsrv VG_MINIMAL_LONGJMP");
      }
      if (tst->sched_jmpbuf_valid) {
         
         VG_MINIMAL_LONGJMP(tst->sched_jmpbuf);
      }
      
   }
   
}

static volatile int busy = 0;

void VG_(gdbserver) ( ThreadId tid )
{
   busy++;
   if (tid != 0) {
      call_gdbserver (tid, core_reason);
   } else {
      if (gdbserver_called == 0) {
         dlog(1, "VG_(gdbserver) called to terminate, nothing to terminate\n");
      } else if (gdbserver_exited) {
         dlog(0, "VG_(gdbserver) called to terminate again %d\n",
              gdbserver_exited);
      } else {
         gdbserver_terminate();
         gdbserver_exited++;
      }
   }
   busy--;
}

static int interrupts_while_busy = 0;

static int interrupts_non_busy = 0;

static int interrupts_non_interruptible = 0;


static void give_control_back_to_vgdb(void)
{
   if (VG_(kill)(VG_(getpid)(), VKI_SIGSTOP) != 0)
      vg_assert2(0, "SIGSTOP for vgdb could not be generated\n");

   vg_assert2(0, 
              "vgdb did not took control. Did you kill vgdb ?\n"
              "busy %d vgdb_interrupted_tid %d\n",
              busy, vgdb_interrupted_tid);
}

void VG_(invoke_gdbserver) ( int check )
{

   int n_tid;

   vg_assert (check == 0x8BADF00D);

   if (busy) {
      interrupts_while_busy++;
      give_control_back_to_vgdb();
   }
   interrupts_non_busy++;

   for (n_tid = 1; n_tid < VG_N_THREADS; n_tid++) {
      switch (VG_(threads)[n_tid].status) {
      
      case VgTs_WaitSys:
      case VgTs_Yielding:
         if (vgdb_interrupted_tid == 0) vgdb_interrupted_tid = n_tid;
         break;

      case VgTs_Empty:     
      case VgTs_Zombie:
         break;

      
      case VgTs_Init:
      case VgTs_Runnable:
         interrupts_non_interruptible++;
         VG_(force_vgdb_poll) ();
         give_control_back_to_vgdb();

      default:             vg_assert(0);
      }
   }

   dlog(1, "invoke_gdbserver running_tid %d vgdb_interrupted_tid %d\n",
        VG_(running_tid), vgdb_interrupted_tid);
   call_gdbserver (vgdb_interrupted_tid, vgdb_reason);
   vgdb_interrupted_tid = 0;
   dlog(1,
        "exit invoke_gdbserver running_tid %d\n", VG_(running_tid));
   give_control_back_to_vgdb();

   vg_assert2(0, "end of invoke_gdbserver reached");

}

Bool VG_(gdbserver_activity) (ThreadId tid)
{
   Bool ret;
   busy++;
   if (!gdbserver_called)
      call_gdbserver (tid, init_reason);
   switch (remote_desc_activity("VG_(gdbserver_activity)")) {
   case 0: ret = False; break;
   case 1: ret = True; break;
   case 2: 
      remote_finish(reset_after_error);
      call_gdbserver (tid, init_reason); 
      ret = False; 
      break;
   default: vg_assert (0);
   }
   busy--;
   return ret;
}

static void dlog_signal (const HChar *who, const vki_siginfo_t *info,
                         ThreadId tid)
{
   dlog(1, "VG core calling %s "
        "vki_nr %d %s gdb_nr %d %s tid %d\n", 
        who,
        info->si_signo, VG_(signame)(info->si_signo),
        target_signal_from_host (info->si_signo),
        target_signal_to_name(target_signal_from_host (info->si_signo)), 
        tid);

}

void VG_(gdbserver_report_fatal_signal) (const vki_siginfo_t *info,
                                         ThreadId tid)
{
   dlog_signal("VG_(gdbserver_report_fatal_signal)", info, tid);

   if (remote_connected()) {
      dlog(1, "already connected, assuming already reported\n");
      return;
   }

   VG_(umsg)("(action on fatal signal) vgdb me ... \n");

   
   gdbserver_signal_encountered (info);

   
   call_gdbserver (tid, signal_reason);
   
}

Bool VG_(gdbserver_report_signal) (vki_siginfo_t *info, ThreadId tid)
{
   dlog_signal("VG_(gdbserver_report_signal)", info, tid);

   if (!remote_connected()) {
      dlog(1, "not connected => pass\n");
      return True;
   }
   if (pass_signals[target_signal_from_host(info->si_signo)]) {
      dlog(1, "pass_signals => pass\n");
      return True;
   }
   
   
   gdbserver_signal_encountered (info);

   call_gdbserver (tid, signal_reason);
   
   
   if (gdbserver_deliver_signal (info)) {
      dlog(1, "gdbserver deliver signal\n");
      return True;
   } else {
      dlog(1, "gdbserver ignore signal\n");
      return False;
   }
}

void VG_(gdbserver_exit) (ThreadId tid, VgSchedReturnCode tids_schedretcode)
{
   dlog(1, "VG core calling VG_(gdbserver_exit) tid %d will exit\n", tid);
   if (remote_connected()) {
      
      switch(tids_schedretcode) {
      case VgSrc_None:
         vg_assert (0);
      case VgSrc_ExitThread:
      case VgSrc_ExitProcess:
         gdbserver_process_exit_encountered ('W', VG_(threads)[tid].os_state.exitcode);
         call_gdbserver (tid, exit_reason);
         break;
      case VgSrc_FatalSig:
         gdbserver_process_exit_encountered ('X', VG_(threads)[tid].os_state.fatalsig);
         call_gdbserver (tid, exit_reason);
         break;
      default:
         vg_assert(0);
      }
   } else {
      dlog(1, "not connected\n");
   }

   
   VG_(gdbserver) (0);
}

VG_REGPARM(1)
void VG_(helperc_CallDebugger) ( HWord iaddr )
{
   GS_Address* g;

   
   
   if (!gdbserver_called)
      return;

   if (valgrind_single_stepping() ||
       ((g = VG_(HT_lookup) (gs_addresses, (UWord)HT_addr(iaddr))) &&
        (g->kind == GS_break))) {
      if (iaddr == HT_addr(ignore_this_break_once)) {
         dlog(1, "ignoring ignore_this_break_once %s\n", 
              sym(ignore_this_break_once,  True));
         ignore_this_break_once = 0;
      } else {
         call_gdbserver (VG_(get_running_tid)(), break_reason);
      }
   }
}

static void VG_(invalidate_if_not_gdbserved) (Addr addr)
{
   if (valgrind_single_stepping())
      invalidate_if_jump_not_yet_gdbserved
         (addr, "gdbserver target jump (instrument)");
}

VG_REGPARM(1)
void VG_(helperc_invalidate_if_not_gdbserved) ( Addr addr )
{
   if (valgrind_single_stepping())
      invalidate_if_jump_not_yet_gdbserved
         (addr, "gdbserver target jump (runtime)");
}

static void VG_(add_stmt_call_invalidate_if_not_gdbserved)
     ( IRSB* sb_in,
       const VexGuestLayout* layout, 
       const VexGuestExtents* vge,
       IRTemp jmp, 
       IRSB* irsb)
{
   
   void*    fn;
   const HChar*   nm;
   IRExpr** args;
   Int      nargs;
   IRDirty* di;

   fn    = &VG_(helperc_invalidate_if_not_gdbserved);
   nm    = "VG_(helperc_invalidate_if_not_gdbserved)";
   args  = mkIRExprVec_1(IRExpr_RdTmp (jmp));
   nargs = 1;
   
   di = unsafeIRDirty_0_N( nargs, nm, 
                           VG_(fnptr_to_fnentry)( fn ), args );

   di->nFxState = 0;

   addStmtToIRSB(irsb, IRStmt_Dirty(di));
}

static void VG_(add_stmt_call_gdbserver) 
     (IRSB* sb_in,                
      const VexGuestLayout* layout, 
      const VexGuestExtents* vge,
      IRType gWordTy, IRType hWordTy,
      Addr  iaddr,                
      UChar delta,                
      IRSB* irsb)                 
{
   void*    fn;
   const HChar*   nm;
   IRExpr** args;
   Int      nargs;
   IRDirty* di;


   vg_assert(delta <= 1);
   addStmtToIRSB(irsb, IRStmt_Put(layout->offset_IP ,
                                  mkIRExpr_HWord(iaddr + (Addr)delta)));

   fn    = &VG_(helperc_CallDebugger);
   nm    = "VG_(helperc_CallDebugger)";
   args  = mkIRExprVec_1(mkIRExpr_HWord (iaddr));
   nargs = 1;
   
   di = unsafeIRDirty_0_N( nargs, nm, 
                           VG_(fnptr_to_fnentry)( fn ), args );

   /* Note: in fact, a debugger call can read whatever register
      or memory. It can also write whatever register or memory.
      So, in theory, we have to indicate the whole universe
      can be read and modified. It is however not critical
      to indicate precisely what is being read/written
      as such indications are needed for tool error detection
      and we do not want to have errors being detected for
      gdb interactions. */
   
   di->nFxState = 2;
   di->fxState[0].fx        = Ifx_Read;
   di->fxState[0].offset    = layout->offset_SP;
   di->fxState[0].size      = layout->sizeof_SP;
   di->fxState[0].nRepeats  = 0;
   di->fxState[0].repeatLen = 0;
   di->fxState[1].fx        = Ifx_Modify;
   di->fxState[1].offset    = layout->offset_IP;
   di->fxState[1].size      = layout->sizeof_IP;
   di->fxState[1].nRepeats  = 0;
   di->fxState[1].repeatLen = 0;

   addStmtToIRSB(irsb, IRStmt_Dirty(di));

}


static void VG_(add_stmt_call_invalidate_exit_target_if_not_gdbserved)
   (IRSB* sb_in,
    const VexGuestLayout* layout,
    const VexGuestExtents* vge,
    IRType gWordTy,
    IRSB* irsb)
{
   if (sb_in->next->tag == Iex_Const) {
     VG_(invalidate_if_not_gdbserved) (gWordTy == Ity_I64 ?
                                       sb_in->next->Iex.Const.con->Ico.U64 
                                       : sb_in->next->Iex.Const.con->Ico.U32);
   } else if (sb_in->next->tag == Iex_RdTmp) {
     VG_(add_stmt_call_invalidate_if_not_gdbserved)
       (sb_in, layout, vge, sb_in->next->Iex.RdTmp.tmp, irsb);
   } else {
     vg_assert (0); 
   }
}

IRSB* VG_(instrument_for_gdbserver_if_needed)
     (IRSB* sb_in,
      const VexGuestLayout* layout,
      const VexGuestExtents* vge,
      IRType gWordTy, IRType hWordTy)
{
   IRSB* sb_out;
   Int i;
   const VgVgdb instr_needed = VG_(gdbserver_instrumentation_needed) (vge);

   if (instr_needed == Vg_VgdbNo)
     return sb_in;


   
   sb_out = deepCopyIRSBExceptStmts(sb_in);

   for (i = 0; i < sb_in->stmts_used; i++) {
      IRStmt* st = sb_in->stmts[i];
      
      if (!st || st->tag == Ist_NoOp) continue;
      
      if (st->tag == Ist_Exit && instr_needed == Vg_VgdbYes) {
        VG_(invalidate_if_not_gdbserved) 
          (hWordTy == Ity_I64 ? 
           st->Ist.Exit.dst->Ico.U64 : 
           st->Ist.Exit.dst->Ico.U32);
      }
      addStmtToIRSB( sb_out, st );
      if (st->tag == Ist_IMark) {
         
         switch (instr_needed) {
         case Vg_VgdbNo: vg_assert (0);
         case Vg_VgdbYes:
         case Vg_VgdbFull:
            VG_(add_stmt_call_gdbserver) ( sb_in, layout, vge,
                                           gWordTy, hWordTy,
                                           st->Ist.IMark.addr,
                                           st->Ist.IMark.delta,
                                           sb_out);
            break;
         default: vg_assert (0);
         }
      }
   }

   if (instr_needed == Vg_VgdbYes) {
      VG_(add_stmt_call_invalidate_exit_target_if_not_gdbserved) (sb_in,
                                                                  layout, vge,
                                                                  gWordTy,
                                                                  sb_out);
   }

   return sb_out;
}

struct mon_out_buf {
   HChar buf[DATASIZ+1];
   int next;
   UInt ret;
};

static void mon_out (HChar c, void *opaque)
{
   struct mon_out_buf *b = (struct mon_out_buf *) opaque;
   b->ret++;
   b->buf[b->next] = c;
   b->next++;
   if (b->next == DATASIZ) {
      b->buf[b->next] = '\0';
      monitor_output(b->buf);
      b->next = 0;
   }
}
UInt VG_(gdb_printf) ( const HChar *format, ... )
{
   struct mon_out_buf b;

   b.next = 0;
   b.ret = 0;
   
   va_list vargs;
   va_start(vargs, format);
   VG_(vcbprintf) (mon_out, &b, format, vargs);
   va_end(vargs);
   
   if (b.next > 0) {
      b.buf[b.next] = '\0';
      monitor_output(b.buf);
   }
   return b.ret;
}

Int VG_(keyword_id) (const HChar* keywords, const HChar* input_word,
                     kwd_report_error report)
{
   const Int il = (input_word == NULL ? 0 : VG_(strlen) (input_word));
   HChar  iw[il+1];
   HChar  kwds[VG_(strlen)(keywords)+1];
   HChar  *kwdssaveptr;

   HChar* kw; 
   Int   kwl;
   Int   kpos = -1;

   Int pass; 
   

   Int pass1needed = 0;

   Int partial_match = -1;
   Int full_match = -1;

   if (input_word == NULL) {
      iw[0] = 0;
      partial_match = 0; 
   } else {
      VG_(strcpy) (iw, input_word);
   }

   for (pass = 0; pass < 2; pass++) {
      VG_(strcpy) (kwds, keywords);
      if (pass == 1)
         VG_(gdb_printf) ("%s can match", 
                          (il == 0 ? "<empty string>" : iw));
      for (kw = VG_(strtok_r) (kwds, " ", &kwdssaveptr); 
           kw != NULL; 
           kw = VG_(strtok_r) (NULL, " ", &kwdssaveptr)) {
         kwl = VG_(strlen) (kw);
         kpos++;
         
         if (il > kwl) {
            ; 
         } else if (il == kwl) {
            if (VG_(strcmp) (kw, iw) == 0) {
               
               if (pass == 1)
                  VG_(gdb_printf) (" %s", kw);
               if (full_match != -1)
                  pass1needed++;
               full_match = kpos;
            }
         } else {
            
            if (VG_(strncmp) (iw, kw, il) == 0) {
               
               if (pass == 1)
                  VG_(gdb_printf) (" %s", kw);
               if (partial_match != -1)
                  pass1needed++;
               partial_match = kpos;
            }
         }
      }
      
      if (pass1needed == 0) {
         if (full_match != -1) {
            return full_match;
         } else {
            if (report == kwd_report_all && partial_match == -1) {
               VG_(gdb_printf) ("%s does not match any of '%s'\n", 
                                iw, keywords);
            }
            return partial_match;
         }
      }

      
      if (pass == 1 || report == kwd_report_none) {
         if (report != kwd_report_none) {
            VG_(gdb_printf) ("\n");
         }
         if (partial_match != -1 || full_match != -1)
            return -2;
         else
            return -1;
      }
   }
   
   vg_assert (0);
}

static Bool is_zero_x (const HChar *s)
{
   if (strlen (s) >= 3 && s[0] == '0' && s[1] == 'x')
      return True;
   else
      return False;
}

static Bool is_zero_b (const HChar *s)
{
   if (strlen (s) >= 3 && s[0] == '0' && s[1] == 'b')
      return True;
   else
      return False;
}

Bool VG_(strtok_get_address_and_size) (Addr* address, 
                                       SizeT* szB, 
                                       HChar **ssaveptr)
{
   HChar* wa;
   HChar* ws;
   HChar* endptr;
   const HChar *ppc;

   wa = VG_(strtok_r) (NULL, " ", ssaveptr);
   ppc = wa;
   if (ppc == NULL || !VG_(parse_Addr) (&ppc, address)) {
      VG_(gdb_printf) ("missing or malformed address\n");
      *address = (Addr) 0;
      *szB = 0;
      return False;
   }
   ws = VG_(strtok_r) (NULL, " ", ssaveptr);
   if (ws == NULL) {
       ;
   } else if (is_zero_x (ws)) {
      *szB = VG_(strtoull16) (ws, &endptr);
   } else if (is_zero_b (ws)) {
      Int j;
      HChar *parsews = ws;
      Int n_bits = VG_(strlen) (ws) - 2;
      *szB = 0;
      ws = NULL; 
      for (j = 0; j < n_bits; j++) {
         if      ('0' == parsews[j+2]) {  }
         else if ('1' == parsews[j+2]) *szB |= (1 << (n_bits-j-1));
         else {
            
            ws = parsews;
            endptr = ws + j + 2;
            break;
         }
      }
   } else {
      *szB = VG_(strtoull10) (ws, &endptr);
   }

   if (ws != NULL && *endptr != '\0') {
      VG_(gdb_printf) ("malformed integer, expecting "
                       "hex 0x..... or dec ...... or binary .....b\n");
      *address = (Addr) 0;
      *szB = 0;
      return False;
   }
   return True;
}

void VG_(gdbserver_status_output)(void)
{
   const int nr_gdbserved_addresses 
      = (gs_addresses == NULL ? -1 : VG_(HT_count_nodes) (gs_addresses));
   const int nr_watchpoints
      = (gs_watches == NULL ? -1 : (int) VG_(sizeXA) (gs_watches));
   remote_utils_output_status();
   VG_(umsg)
      ("nr of calls to gdbserver: %d\n"
       "single stepping %d\n"
       "interrupts intr_tid %d gs_non_busy %d gs_busy %d tid_non_intr %d\n"
       "gdbserved addresses %d (-1 = not initialized)\n"
       "watchpoints %d (-1 = not initialized)\n"
       "vgdb-error %d\n"
       "hostvisibility %s\n",
       gdbserver_called,
       valgrind_single_stepping(),
       
       vgdb_interrupted_tid, 
       interrupts_non_busy, 
       interrupts_while_busy,
       interrupts_non_interruptible,
       
       nr_gdbserved_addresses,
       nr_watchpoints,
       VG_(dyn_vgdb_error),
       hostvisibility ? "yes" : "no");
}
