

/*
   This file is part of Valgrind, a dynamic binary instrumentation
   framework.

   Copyright (C) 2005-2013 Apple Inc.
      Greg Parker  gparker@apple.com

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

#if defined(VGO_darwin)

#include "pub_core_basics.h"
#include "pub_core_vki.h"
#include "pub_core_vkiscnums.h"
#include "pub_core_threadstate.h"
#include "pub_core_aspacemgr.h"
#include "pub_core_xarray.h"
#include "pub_core_clientstate.h"
#include "pub_core_debuglog.h"
#include "pub_core_debuginfo.h"    
#include "pub_core_transtab.h"     
#include "pub_core_libcbase.h"
#include "pub_core_libcassert.h"
#include "pub_core_libcfile.h"
#include "pub_core_libcprint.h"
#include "pub_core_libcproc.h"
#include "pub_core_libcsignal.h"
#include "pub_core_machine.h"      
#include "pub_core_mallocfree.h"
#include "pub_core_options.h"
#include "pub_core_oset.h"
#include "pub_core_scheduler.h"
#include "pub_core_sigframe.h"      
#include "pub_core_signals.h"
#include "pub_core_syscall.h"
#include "pub_core_syswrap.h"
#include "pub_core_tooliface.h"
#include "pub_core_wordfm.h"

#include "priv_types_n_macros.h"
#include "priv_syswrap-generic.h"   
#include "priv_syswrap-darwin.h"    
#include "priv_syswrap-main.h"

#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <semaphore.h>

#define msgh_request_port      msgh_remote_port
#define msgh_reply_port        msgh_local_port
#define BOOTSTRAP_MAX_NAME_LEN                  128
typedef HChar name_t[BOOTSTRAP_MAX_NAME_LEN];

typedef uint64_t mig_addr_t;


static mach_port_t vg_host_port = 0;
static mach_port_t vg_task_port = 0;
static mach_port_t vg_bootstrap_port = 0;

static VgSchedReturnCode thread_wrapper(Word  tidW)
{
   VgSchedReturnCode ret;
   ThreadId     tid = (ThreadId)tidW;
   ThreadState* tst = VG_(get_ThreadState)(tid);

   VG_(debugLog)(1, "syswrap-darwin", 
                    "thread_wrapper(tid=%lld): entry\n", 
                    (ULong)tidW);

   vg_assert(tst->status == VgTs_Init);

   
   VG_(acquire_BigLock)(tid, "thread_wrapper");

   if (0)
      VG_(printf)("thread tid %d started: stack = %p\n",
                  tid, &tid);

   
   tst->err_disablement_level = 0;

   VG_TRACK(pre_thread_first_insn, tid);

   tst->os_state.lwpid = VG_(gettid)();
   tst->os_state.threadgroup = VG_(getpid)();


   ret = VG_(scheduler)(tid);

   vg_assert(VG_(is_exiting)(tid));
   
   vg_assert(tst->status == VgTs_Runnable);
   vg_assert(VG_(is_running_thread)(tid));

   VG_(debugLog)(1, "syswrap-darwin", 
                    "thread_wrapper(tid=%lld): done\n", 
                    (ULong)tidW);

   
   return ret;
}




Addr allocstack ( ThreadId tid )
{
   ThreadState* tst = VG_(get_ThreadState)(tid);
   VgStack*     stack;
   Addr         initial_SP;


   if (tst->os_state.valgrind_stack_base == 0)
      vg_assert(tst->os_state.valgrind_stack_init_SP == 0);

   if (tst->os_state.valgrind_stack_base != 0)
      vg_assert(tst->os_state.valgrind_stack_init_SP != 0);

   

   if (tst->os_state.valgrind_stack_base == 0) {
      stack = VG_(am_alloc_VgStack)( &initial_SP );
      if (stack) {
         tst->os_state.valgrind_stack_base    = (Addr)stack;
         tst->os_state.valgrind_stack_init_SP = initial_SP;
      }
   }

   VG_(debugLog)( 2, "syswrap-darwin", "stack for tid %d at %p; init_SP=%p\n",
                   tid, 
                   (void*)tst->os_state.valgrind_stack_base, 
                   (void*)tst->os_state.valgrind_stack_init_SP );

   vg_assert(VG_IS_32_ALIGNED(tst->os_state.valgrind_stack_init_SP));
   
   return tst->os_state.valgrind_stack_init_SP;
}


void find_stack_segment(ThreadId tid, Addr sp)
{
   ML_(guess_and_register_stack) (sp, VG_(get_ThreadState)(tid));
}


static void run_a_thread_NORETURN ( Word tidW )
{
   Int               c;
   VgSchedReturnCode src;
   ThreadId          tid = (ThreadId)tidW;
   ThreadState*      tst;

   VG_(debugLog)(1, "syswrap-darwin", 
                    "run_a_thread_NORETURN(tid=%lld): pre-thread_wrapper\n",
                    (ULong)tidW);

   tst = VG_(get_ThreadState)(tid);
   vg_assert(tst);

   
   src = thread_wrapper(tid);  

   VG_(debugLog)(1, "syswrap-darwin", 
                    "run_a_thread_NORETURN(tid=%lld): post-thread_wrapper\n",
                    (ULong)tidW);

   c = VG_(count_living_threads)();
   vg_assert(c >= 1); 

   
   VG_TRACK( pre_thread_ll_exit, tid );

   if (tst->err_disablement_level > 0) {
      VG_(umsg)(
         "WARNING: exiting thread has error reporting disabled.\n"
         "WARNING: possibly as a result of some mistake in the use\n"
         "WARNING: of the VALGRIND_DISABLE_ERROR_REPORTING macros.\n"
      );
      VG_(debugLog)(
         1, "syswrap-linux", 
            "run_a_thread_NORETURN(tid=%lld): "
            "WARNING: exiting thread has err_disablement_level = %u\n",
            (ULong)tidW, tst->err_disablement_level
      );
   }
   tst->err_disablement_level = 0;

   if (c == 1) {

      VG_(debugLog)(1, "syswrap-darwin", 
                       "run_a_thread_NORETURN(tid=%lld): "
                          "last one standing\n",
                          (ULong)tidW);

      ( * VG_(address_of_m_main_shutdown_actions_NORETURN) ) (tid, src);

   } else {

      mach_msg_header_t msg;

      VG_(debugLog)(1, "syswrap-darwin", 
                       "run_a_thread_NORETURN(tid=%lld): "
                          "not last one standing\n",
                          (ULong)tidW);

      

      
      VG_(exit_thread)(tid);
      vg_assert(tst->status == VgTs_Zombie);

      

      
      msg.msgh_bits = MACH_MSGH_BITS(17, MACH_MSG_TYPE_MAKE_SEND_ONCE);
      msg.msgh_request_port = VG_(gettid)();
      msg.msgh_reply_port = 0;
      msg.msgh_id = 3600;  
      
      tst->status = VgTs_Empty;
      
      
      
      mach_msg(&msg, MACH_SEND_MSG|MACH_MSG_OPTION_NONE, 
               sizeof(msg), 0, 0, MACH_MSG_TIMEOUT_NONE, 0);
      
      
      
      VG_(core_panic)("Thread exit failed?\n");
   }
   
   
   vg_assert(0);
}


void VG_(main_thread_wrapper_NORETURN)(ThreadId tid)
{
   Addr sp;
   VG_(debugLog)(1, "syswrap-darwin", 
                    "entering VG_(main_thread_wrapper_NORETURN)\n");

   sp = allocstack(tid);

   vg_assert2(sp != 0, "Cannot allocate main thread's stack.");

   
   vg_assert( VG_(count_living_threads)() == 1 );
   
   call_on_new_stack_0_1( 
      (Addr)sp,             
      0,                     
      run_a_thread_NORETURN,  
      (Word)tid              
   );

   
   vg_assert(0);
}


void start_thread_NORETURN ( Word arg )
{
   ThreadState* tst = (ThreadState*)arg;
   ThreadId     tid = tst->tid;

   run_a_thread_NORETURN ( (Word)tid );
   
   vg_assert(0);
}


void VG_(cleanup_thread) ( ThreadArchState* arch )
{
}  



static WordFM* decaying_string_table = NULL; 

static Word decaying_string_table_cmp ( UWord s1, UWord s2 ) {
   return (Word)VG_(strcmp)( (HChar*)s1, (HChar*)s2 );
}

static void log_decaying ( const HChar* format, ... ) PRINTF_CHECK(1, 2);
static void log_decaying ( const HChar* format, ... )
{
   
   HChar buf[256];
   VG_(memset)(buf, 0, sizeof(buf));
   va_list vargs;
   va_start(vargs,format);
   (void) VG_(vsnprintf)(buf, sizeof(buf), format, vargs);
   va_end(vargs);
   buf[sizeof(buf)-1] = 0;

   
   if (!decaying_string_table) {
      decaying_string_table
         = VG_(newFM)( VG_(malloc), "syswrap-darwin.pd.1",
                       VG_(free), decaying_string_table_cmp );
   }

   const HChar* key = NULL;
   UWord        val = 0;
   if (!VG_(lookupFM)(decaying_string_table,
                      (UWord*)&key, &val, (UWord)&buf[0])) {
      
      
      vg_assert(key == NULL && val == 0);
      key = VG_(strdup)("syswrap-darwin.pd.2", buf);
      VG_(addToFM)(decaying_string_table, (UWord)key, (UWord)0);
   }

   vg_assert(key != NULL && key != &buf[0]);

   
   
   val++;
   Bool b = VG_(addToFM)(decaying_string_table, (UWord)key, (UWord)val);
   vg_assert(b);
   
   if (-1 != VG_(log2)( (UInt)val )) {
      if (val == 1)
         VG_(dmsg)("%s\n", key);
      else
         VG_(dmsg)("%s (repeated %lu times)\n", key, val);
   }
}



typedef struct OpenPort
{
   mach_port_t port;
   mach_port_type_t type;         
   Int send_count;                
   HChar *name;                   
   ExeContext *where;             
   struct OpenPort *next, *prev;
} OpenPort;

#define PORT_STRLEN (2+2*sizeof(mach_port_t))

static OpenPort *allocated_ports;

static Int allocated_port_count = 0;

static void port_create_vanilla(mach_port_t port)
{
   OpenPort* op
     = VG_(calloc)("syswrap-darwin.port_create_vanilla", sizeof(OpenPort), 1);
   op->port = port;
   
   op->next = allocated_ports;
   if (allocated_ports) allocated_ports->prev = op;
   allocated_ports = op;
   allocated_port_count++;
}

__attribute__((unused))
static Bool port_exists(mach_port_t port)
{
   OpenPort *i;

   
   i = allocated_ports;
   while (i) {
      if (i->port == port) {
         return True;
      }
      i = i->next;
   }
   
   return False;
}

static OpenPort *info_for_port(mach_port_t port)
{
   OpenPort *i;
   if (!port) return NULL;

   i = allocated_ports;
   while (i) {
      if (i->port == port) {
         return i;
      }
      i = i->next;
   }

   return NULL;
}


__private_extern__ void assign_port_name(mach_port_t port, const HChar *name)
{
   OpenPort *i;
   if (!port) return;
   vg_assert(name);

   i = info_for_port(port);
   vg_assert(i);

   if (i->name) VG_(free)(i->name);
   i->name = 
       VG_(malloc)("syswrap-darwin.mach-port-name", 
                   VG_(strlen)(name) + PORT_STRLEN + 1);
   VG_(sprintf)(i->name, name, port);
}


static const HChar *name_for_port(mach_port_t port)
{
   static HChar buf[8 + PORT_STRLEN + 1];
   OpenPort *i;

   
   if (port == VG_(gettid)()) return "mach_thread_self()";
   if (port == 0) return "NULL";

   i = allocated_ports;
   while (i) {
      if (i->port == port) {
         return i->name;
      }
      i = i->next;
   }

   VG_(sprintf)(buf, "NONPORT-%#x", port);
   return buf;
}


static
void record_port_mod_refs(mach_port_t port, mach_port_type_t right, Int delta)
{
   OpenPort *i = allocated_ports;
   if (!port) return;

   while(i) {
      if(i->port == port) {
         vg_assert(right != MACH_PORT_TYPE_DEAD_NAME);
         if (right & MACH_PORT_TYPE_SEND) {
            
            if (delta == INT_MIN) delta = -i->send_count; 
            i->send_count += delta;
            if (i->send_count > 0) i->type |= MACH_PORT_TYPE_SEND;
            else i->type &= ~MACH_PORT_TYPE_SEND;
         } 
         right = right & ~MACH_PORT_TYPE_SEND;
         if (right) {
            
            if (delta > 0) {
               i->type |= right;
            } else if (delta < 0) {
               i->type &= ~right;
            }
         }

         if (i->type != 0) return;

         
         
         if(i->prev)
            i->prev->next = i->next;
         else
            allocated_ports = i->next;
         if(i->next)
            i->next->prev = i->prev;
         if(i->name) 
            VG_(free) (i->name);
         VG_(free) (i);
         allocated_port_count--;
         return;
      }
      i = i->next;
   }

   VG_(printf)("UNKNOWN Mach port modified (port %#x delta %d)\n", port, delta);
}

static 
void record_port_insert_rights(mach_port_t port, mach_msg_type_name_t type)
{
   switch (type) {
   case MACH_MSG_TYPE_PORT_NAME:
      
      break;
   case MACH_MSG_TYPE_PORT_RECEIVE:
      
      record_port_mod_refs(port, MACH_PORT_TYPE_RECEIVE, 1);
      break;
   case MACH_MSG_TYPE_PORT_SEND:
      
      record_port_mod_refs(port, MACH_PORT_TYPE_SEND, 1);
      break;
   case MACH_MSG_TYPE_PORT_SEND_ONCE:
      
      record_port_mod_refs(port, MACH_PORT_TYPE_SEND_ONCE, 1);
      break;
   default:
      vg_assert(0);
      break;
   }
}

static 
void record_port_dealloc(mach_port_t port)
{
   
   record_port_mod_refs(port, MACH_PORT_TYPE_SEND_RIGHTS, -1);
}

static 
void record_port_destroy(mach_port_t port)
{
   
   record_port_mod_refs(port, MACH_PORT_TYPE_ALL_RIGHTS, INT_MIN);
}


void record_named_port(ThreadId tid, mach_port_t port, 
                       mach_port_right_t right, const HChar *name)
{
   OpenPort *i;
   if (!port) return;

   
   i = allocated_ports;
   while (i) {
      if (i->port == port) {
         if (right != -1) record_port_mod_refs(port, MACH_PORT_TYPE(right), 1);
         return;
      }
      i = i->next;
   }

   
   if (i == NULL) {
      i = VG_(malloc)("syswrap-darwin.mach-port", sizeof(OpenPort));

      i->prev = NULL;
      i->next = allocated_ports;
      if(allocated_ports) allocated_ports->prev = i;
      allocated_ports = i;
      allocated_port_count++;

      i->port = port;
      i->where = (tid == -1) ? NULL : VG_(record_ExeContext)(tid, 0);
      i->name = NULL;
      if (right != -1) {
         i->type = MACH_PORT_TYPE(right);
         i->send_count = (right == MACH_PORT_RIGHT_SEND) ? 1 : 0;
      } else {
         i->type = 0;
         i->send_count = 0;
      }
      
      assign_port_name(port, name);
   }
}


static void record_unnamed_port(ThreadId tid, mach_port_t port, mach_port_right_t right)
{
   record_named_port(tid, port, right, "unnamed-%p");
}


void VG_(show_open_ports)(void)
{
   OpenPort *i;
   
   VG_(message)(Vg_UserMsg, 
                "MACH PORTS: %d open at exit.\n", allocated_port_count);

   for (i = allocated_ports; i; i = i->next) {
      if (i->name) {
         VG_(message)(Vg_UserMsg, "Open Mach port 0x%x: %s\n", i->port,
                      i->name);
      } else {
         VG_(message)(Vg_UserMsg, "Open Mach port 0x%x\n", i->port);
      }

      if (i->where) {
         VG_(pp_ExeContext)(i->where);
         VG_(message)(Vg_UserMsg, "\n");
      }
   }

   VG_(message)(Vg_UserMsg, "\n");
}



typedef
   enum { CheckAlways=1, CheckEvery20, CheckNever }
   CheckHowOften;

static const HChar* show_CheckHowOften ( CheckHowOften cho ) {
   switch (cho) {
      case CheckAlways:   return "Always ";
      case CheckEvery20:  return "Every20";
      case CheckNever:    return "Never  ";
      default: vg_assert(0);
   }
}

typedef
   struct {
      CheckHowOften cho;
      const HChar*  key1;
      const HChar*  key2;
      UWord         key3;
      ULong         n_checks;
      ULong         n_mappings_added;
      ULong         n_mappings_removed;
   }
   SyncStats;

static Bool cmp_eqkeys_SyncStats ( SyncStats* ss1, SyncStats* ss2 ) {
   return ss1->key3 == ss2->key3
          && 0 == VG_(strcmp)(ss1->key1, ss2->key1)
          && 0 == VG_(strcmp)(ss1->key2, ss2->key2);
}

#define N_SYNCSTATS 1000
static Int       syncstats_used = 0;
static SyncStats syncstats[N_SYNCSTATS];

static ULong n_syncsRequested = 0; 
static ULong n_syncsPerformed = 0; 


static
void update_syncstats ( CheckHowOften cho,
                        const HChar* key1, const HChar* key2,
                        UWord key3,
                        UInt n_mappings_added, UInt n_mappings_removed )
{
   SyncStats dummy = { CheckAlways, key1, key2, key3, 0, 0, 0 };
   Int i;
   for (i = 0; i < syncstats_used; i++) {
      if (cmp_eqkeys_SyncStats(&syncstats[i], &dummy))
         break;
   }
   vg_assert(i >= 0 && i <= syncstats_used);
   if (i == syncstats_used) {
      
      vg_assert(syncstats_used < N_SYNCSTATS);
      syncstats_used++;
      syncstats[i] = dummy;
      syncstats[i].cho = cho;
   }
   vg_assert(cmp_eqkeys_SyncStats(&syncstats[i], &dummy));
   syncstats[i].n_checks++;
   syncstats[i].n_mappings_added   += (ULong)n_mappings_added;
   syncstats[i].n_mappings_removed += (ULong)n_mappings_removed;
   
   static UInt reorder_ctr = 0;
   if (i > 0 && 0 == (1 & reorder_ctr++)) {
      SyncStats tmp = syncstats[i-1];
      syncstats[i-1] = syncstats[i];
      syncstats[i] = tmp;
   }
}


static void maybe_show_syncstats ( void )
{
   Int i;

   
   if (0 == (n_syncsRequested & 0xFF)) {
      VG_(printf)("Resync filter: %'llu requested, %'llu performed (%llu%%)\n",
                  n_syncsRequested, n_syncsPerformed,
                  (100 * n_syncsPerformed) / 
                     (n_syncsRequested == 0 ? 1 : n_syncsRequested));
      for (i = 0; i < syncstats_used; i++) {
         if (i >= 40) break; 
         VG_(printf)("  [%3d] (%s) upd %6llu  diff %4llu+,%3llu-"
                     "  %s %s 0x%08llx\n",
                     i, show_CheckHowOften(syncstats[i].cho),
                     syncstats[i].n_checks, 
                     syncstats[i].n_mappings_added,
                     syncstats[i].n_mappings_removed,
                     syncstats[i].key1, syncstats[i].key2,
                     (ULong)syncstats[i].key3);
      }
      if (i < syncstats_used) {
        VG_(printf)("  and %d more entries not shown.\n", syncstats_used - i);
      }
      VG_(printf)("\n");
   }
}


Bool ML_(sync_mappings)(const HChar* when, const HChar* where, UWord num)
{
   
   
   
   
   
   
   
   
   
   
   

   if (VG_(clo_resync_filter) >= 2)
      maybe_show_syncstats();

   n_syncsRequested++;

   
   
   
   
   
   ChangedSeg* css = NULL;
   Int         css_size;
   Int         css_used;
   Int         i;
   Bool        ok;

   
   
   
   
   
   
   
   
   
   
   
   
   

#  define STREQ(_s1, _s2) (0 == VG_(strcmp)((_s1),(_s2)))
   Bool when_in    = STREQ(when,  "in");
   Bool when_after = STREQ(when,  "after");
   Bool where_mmr  = STREQ(where, "mach_msg_receive");
   Bool where_mmrU = STREQ(where, "mach_msg_receive-UNHANDLED");
   Bool where_iuct = STREQ(where, "iokit_user_client_trap");
   Bool where_MwcN = STREQ(where, "ML_(wqthread_continue_NORETURN)");
   Bool where_woQR = STREQ(where, "workq_ops(QUEUE_REQTHREADS)");
   Bool where_woTR = STREQ(where, "workq_ops(THREAD_RETURN)");
   Bool where_ke64 = STREQ(where, "kevent64");
#  undef STREQ

   vg_assert(
      1 >= ( (where_mmr ? 1 : 0) + (where_mmrU ? 1 : 0) 
             + (where_iuct ? 1 : 0) + (where_MwcN ? 1 : 0)
             + (where_woQR ? 1 : 0) + (where_woTR ? 1 : 0)
             + (where_ke64 ? 1 : 0)
   ));
   
   
   vg_assert(when_in    == True || when_in    == False);
   vg_assert(when_after == True || when_after == False);

   CheckHowOften check = CheckAlways;

#  if DARWIN_VERS == DARWIN_10_9 && VG_WORDSIZE == 8
   
   if (when_after && where_mmr) {
      
      switch (num) {
         case 0x00000000: 
            check = CheckEvery20;
            break;
         default:
            break;
      }
   }
   else
   if (when_after && where_mmrU) {
      
      switch (num) {
         case 0x00000000: 
         case 0x00000001: 
         case 0x00000002: 
         case 0x00000003: 
         
         case 0x000072d9: 
         case 0x000072cb: 
         case 0x000074d5: 
            check = CheckEvery20;
            break;
         default:
            break;
      }
   }
   else
   if (when_in && where_MwcN && num == 0x00000000) {
      
      
      check = CheckEvery20;
   }
   else
   if (when_after && where_woQR && num == 0x00000000) {
      
      
      check = CheckEvery20;
   }
   else
   if (when_after && where_woTR && num == 0x00000000) {
      
      
      check = CheckEvery20;
   }
   else
   if (when_after && where_ke64 && num == 0x00000000) {
      
      
      check = CheckEvery20;
   }
   
#  endif 

#  if DARWIN_VERS == DARWIN_10_10 && VG_WORDSIZE == 8
   
   if (when_after && where_mmr) {
      
      switch (num) {
         case 0x00000000: 
            check = CheckEvery20;
            break;
         default:
            break;
      }
   }
   else
   if (when_after && where_mmrU) {
      
      switch (num) {
         case 0x00000000: 
         case 0x0000004f: 
         case 0x00000b95: 
         case 0x00000ba5: 
         case 0x0000157f: 
         case 0x0000157d: 
         case 0x0000333d: 
         case 0x0000333f: 
         case 0x000072cd: 
         case 0x000072ae: 
         case 0x000072ec: 
         case 0x77303074: 
         case 0x10000000: 
            check = CheckEvery20;
            break;
         default:
            break;
      }
   }
   else
   if (when_in && where_MwcN && num == 0x00000000) {
      
      
      check = CheckEvery20;
   }
   else
   if (when_after && where_woQR && num == 0x00000000) {
      
      
      check = CheckEvery20;
   }
   else
   if (when_after && where_woTR && num == 0x00000000) {
      
      
      check = CheckEvery20;
   }
   else
   if (when_after && where_ke64 && num == 0x00000000) {
      
      
      check = CheckEvery20;
   }
   
#  endif 

   {
     static UInt ctr1k = 0;
     ctr1k++;
     if ((ctr1k % 1000) == 0)
        check = CheckAlways;
   }

   
   if (VG_(clo_resync_filter) == 0)
      check = CheckAlways;

   switch (check) {
      case CheckAlways:
         break;
      case CheckEvery20: {
         
         static UInt ctr10 = 0;
         ctr10++;
         if ((ctr10 % 20) != 0) return False;
         break;
      }
      case CheckNever:
         return False;
      default:
         vg_assert(0);
   }
   
   

   if (0 || VG_(clo_trace_syscalls)) {
       VG_(debugLog)(0, "syswrap-darwin",
                     "sync_mappings (%s) (\"%s\", \"%s\", 0x%llx)\n", 
                     show_CheckHowOften(check), when, where, (ULong)num);
   }

   
   
   css_size = 16;
   ok = False;
   while (!ok) {
      VG_(free)(css);   
      css = VG_(calloc)("sys_wrap.sync_mappings",
                        css_size, sizeof(ChangedSeg));
      ok = VG_(get_changed_segments)(when, where, css, css_size, &css_used);
      css_size *= 2;
   } 

   UInt css_added = 0, css_removed = 0;

   
   for (i = 0; i < css_used; i++) {
      ChangedSeg* cs = &css[i];
      if (cs->is_added) {
         css_added++;
         ML_(notify_core_and_tool_of_mmap)(
               cs->start, cs->end - cs->start + 1,
               cs->prot, VKI_MAP_PRIVATE, 0, cs->offset);
         
      } else {
         css_removed++;
         ML_(notify_core_and_tool_of_munmap)(
               cs->start, cs->end - cs->start + 1);
      }
      if (VG_(clo_trace_syscalls)) {
          if (cs->is_added) {
             VG_(debugLog)(0, "syswrap-darwin",
                "  added region 0x%010lx..0x%010lx prot %u at %s (%s)\n", 
                cs->start, cs->end + 1, (UInt)cs->prot, where, when);
	  } else {
             VG_(debugLog)(0, "syswrap-darwin",
                "  removed region 0x%010lx..0x%010lx at %s (%s)\n", 
                cs->start, cs->end + 1, where, when);
	  }
      }
   }

   VG_(free)(css);

   if (0)
      VG_(debugLog)(0, "syswrap-darwin", "SYNC: %d  %s  %s\n",
                    css_used, when, where);

   
   n_syncsPerformed++;
   update_syncstats(check, when, where, num, css_added, css_removed);

   return css_used > 0;
}


#define PRE(name)       DEFN_PRE_TEMPLATE(darwin, name)
#define POST(name)      DEFN_POST_TEMPLATE(darwin, name)

#define PRE_FN(name)    vgSysWrap_darwin_##name##_before
#define POST_FN(name)   vgSysWrap_darwin_##name##_after

#define CALL_PRE(name) PRE_FN(name)(tid, layout, arrghs, status, flags)
#define CALL_POST(name) POST_FN(name)(tid, arrghs, status)

#if VG_WORDSIZE == 4
# if defined(VGA_x86)
#  define LOHI64(lo,hi)   ( ((ULong)(UInt)(lo)) | (((ULong)(UInt)(hi)) << 32) )
# else
#  error unknown architecture
# endif
#endif

#define MACH_THREAD ((Addr)VG_(get_ThreadState)(tid)->os_state.lwpid)

#define AFTER VG_(get_ThreadState)(tid)->os_state.post_mach_trap_fn

#define MACH_ARG(x) VG_(get_ThreadState)(tid)->os_state.mach_args.x
#define MACH_REMOTE VG_(get_ThreadState)(tid)->os_state.remote_port
#define MACH_MSGH_ID VG_(get_ThreadState)(tid)->os_state.msgh_id


PRE(ioctl)
{
   *flags |= SfMayBlock;

   
   switch (ARG2 ) {
   case VKI_TIOCSCTTY:
   case VKI_TIOCEXCL:
   case VKI_TIOCSBRK:
   case VKI_TIOCCBRK:
   case VKI_TIOCPTYGRANT:
   case VKI_TIOCPTYUNLK:
   case VKI_DTRACEHIOC_REMOVE: 
      PRINT("ioctl ( %ld, 0x%lx )",ARG1,ARG2);
      PRE_REG_READ2(long, "ioctl",
                    unsigned int, fd, unsigned int, request);
      return;
   default:
      PRINT("ioctl ( %ld, 0x%lx, %#lx )",ARG1,ARG2,ARG3);
      PRE_REG_READ3(long, "ioctl",
                    unsigned int, fd, unsigned int, request, unsigned long, arg);
   }

   switch (ARG2 ) {
   case VKI_TIOCGWINSZ:
      PRE_MEM_WRITE( "ioctl(TIOCGWINSZ)", ARG3, sizeof(struct vki_winsize) );
      break;
   case VKI_TIOCSWINSZ:
      PRE_MEM_READ( "ioctl(TIOCSWINSZ)",  ARG3, sizeof(struct vki_winsize) );
      break;
   case VKI_TIOCMBIS:
      PRE_MEM_READ( "ioctl(TIOCMBIS)",    ARG3, sizeof(unsigned int) );
      break;
   case VKI_TIOCMBIC:
      PRE_MEM_READ( "ioctl(TIOCMBIC)",    ARG3, sizeof(unsigned int) );
      break;
   case VKI_TIOCMSET:
      PRE_MEM_READ( "ioctl(TIOCMSET)",    ARG3, sizeof(unsigned int) );
      break;
   case VKI_TIOCMGET:
      PRE_MEM_WRITE( "ioctl(TIOCMGET)",   ARG3, sizeof(unsigned int) );
      break;
   case VKI_TIOCGPGRP:
      
      PRE_MEM_WRITE( "ioctl(TIOCGPGRP)", ARG3, sizeof(vki_pid_t) );
      break;
   case VKI_TIOCSPGRP:
      
      PRE_MEM_WRITE( "ioctl(TIOCGPGRP)", ARG3, sizeof(vki_pid_t) );
      break;
   case VKI_FIONBIO:
      PRE_MEM_READ( "ioctl(FIONBIO)",    ARG3, sizeof(int) );
      break;
   case VKI_FIOASYNC:
      PRE_MEM_READ( "ioctl(FIOASYNC)",   ARG3, sizeof(int) );
      break;
   case VKI_FIONREAD:                
      PRE_MEM_WRITE( "ioctl(FIONREAD)",  ARG3, sizeof(int) );
      break;


      
      
   case VKI_SIOCGIFFLAGS:        
      PRE_MEM_RASCIIZ( "ioctl(SIOCGIFFLAGS)",
                     (Addr)((struct vki_ifreq *)ARG3)->vki_ifr_name );
      PRE_MEM_WRITE( "ioctl(SIOCGIFFLAGS)", ARG3, sizeof(struct vki_ifreq));
      break;
   case VKI_SIOCGIFMTU:          
      PRE_MEM_RASCIIZ( "ioctl(SIOCGIFMTU)",
                     (Addr)((struct vki_ifreq *)ARG3)->vki_ifr_name );
      PRE_MEM_WRITE( "ioctl(SIOCGIFMTU)", ARG3, sizeof(struct vki_ifreq));
      break;
   case VKI_SIOCGIFADDR:         
      PRE_MEM_RASCIIZ( "ioctl(SIOCGIFADDR)",
                     (Addr)((struct vki_ifreq *)ARG3)->vki_ifr_name );
      PRE_MEM_WRITE( "ioctl(SIOCGIFADDR)", ARG3, sizeof(struct vki_ifreq));
      break;
   case VKI_SIOCGIFNETMASK:      
      PRE_MEM_RASCIIZ( "ioctl(SIOCGIFNETMASK)",
                     (Addr)((struct vki_ifreq *)ARG3)->vki_ifr_name );
      PRE_MEM_WRITE( "ioctl(SIOCGIFNETMASK)", ARG3, sizeof(struct vki_ifreq));
      break;
   case VKI_SIOCGIFMETRIC:       
      PRE_MEM_RASCIIZ( "ioctl(SIOCGIFMETRIC)",
                     (Addr)((struct vki_ifreq *)ARG3)->vki_ifr_name );
      PRE_MEM_WRITE( "ioctl(SIOCGIFMETRIC)", ARG3, sizeof(struct vki_ifreq));
      break;
   case VKI_SIOCGIFDSTADDR:      
      PRE_MEM_RASCIIZ( "ioctl(SIOCGIFDSTADDR)",
                     (Addr)((struct vki_ifreq *)ARG3)->vki_ifr_name );
      PRE_MEM_WRITE( "ioctl(SIOCGIFDSTADDR)", ARG3, sizeof(struct vki_ifreq));
      break;
   case VKI_SIOCGIFBRDADDR:      
      PRE_MEM_RASCIIZ( "ioctl(SIOCGIFBRDADDR)",
                     (Addr)((struct vki_ifreq *)ARG3)->vki_ifr_name );
      PRE_MEM_WRITE( "ioctl(SIOCGIFBRDADDR)", ARG3, sizeof(struct vki_ifreq));
      break;
   case VKI_SIOCGIFCONF:         
      PRE_MEM_READ( "ioctl(SIOCGIFCONF)",
                    (Addr)&((struct vki_ifconf *)ARG3)->ifc_len,
                    sizeof(((struct vki_ifconf *)ARG3)->ifc_len));
      PRE_MEM_READ( "ioctl(SIOCGIFCONF)",
                    (Addr)&((struct vki_ifconf *)ARG3)->vki_ifc_buf,
                    sizeof(((struct vki_ifconf *)ARG3)->vki_ifc_buf));
      if ( ARG3 ) {
         
         
         struct vki_ifconf *ifc = (struct vki_ifconf *) ARG3;
         PRE_MEM_WRITE( "ioctl(SIOCGIFCONF).ifc_buf",
                        (Addr)(ifc->vki_ifc_buf), ifc->ifc_len );
      }
      break;
                    
   case VKI_SIOCSIFFLAGS:        
      PRE_MEM_RASCIIZ( "ioctl(SIOCSIFFLAGS)",
                     (Addr)((struct vki_ifreq *)ARG3)->vki_ifr_name );
      PRE_MEM_READ( "ioctl(SIOCSIFFLAGS)",
                     (Addr)&((struct vki_ifreq *)ARG3)->vki_ifr_flags,
                     sizeof(((struct vki_ifreq *)ARG3)->vki_ifr_flags) );
      break;
   case VKI_SIOCSIFADDR:         
   case VKI_SIOCSIFDSTADDR:      
   case VKI_SIOCSIFBRDADDR:      
   case VKI_SIOCSIFNETMASK:      
      PRE_MEM_RASCIIZ( "ioctl(SIOCSIF*ADDR)",
                     (Addr)((struct vki_ifreq *)ARG3)->vki_ifr_name );
      PRE_MEM_READ( "ioctl(SIOCSIF*ADDR)",
                     (Addr)&((struct vki_ifreq *)ARG3)->ifr_addr,
                     sizeof(((struct vki_ifreq *)ARG3)->ifr_addr) );
      break;
   case VKI_SIOCSIFMETRIC:       
      PRE_MEM_RASCIIZ( "ioctl(SIOCSIFMETRIC)",
                     (Addr)((struct vki_ifreq *)ARG3)->vki_ifr_name );
      PRE_MEM_READ( "ioctl(SIOCSIFMETRIC)",
                     (Addr)&((struct vki_ifreq *)ARG3)->vki_ifr_metric,
                     sizeof(((struct vki_ifreq *)ARG3)->vki_ifr_metric) );
      break;
   case VKI_SIOCSIFMTU:          
      PRE_MEM_RASCIIZ( "ioctl(SIOCSIFMTU)",
                     (Addr)((struct vki_ifreq *)ARG3)->vki_ifr_name );
      PRE_MEM_READ( "ioctl(SIOCSIFMTU)",
                     (Addr)&((struct vki_ifreq *)ARG3)->vki_ifr_mtu,
                     sizeof(((struct vki_ifreq *)ARG3)->vki_ifr_mtu) );
      break;
      
#ifdef VKI_SIOCADDRT
   case VKI_SIOCADDRT:           
   case VKI_SIOCDELRT:           
      PRE_MEM_READ( "ioctl(SIOCADDRT/DELRT)", ARG3, 
                    sizeof(struct vki_rtentry));
      break;
#endif

   case VKI_SIOCGPGRP:
      PRE_MEM_WRITE( "ioctl(SIOCGPGRP)", ARG3, sizeof(int) );
      break;
   case VKI_SIOCSPGRP:
      PRE_MEM_READ( "ioctl(SIOCSPGRP)", ARG3, sizeof(int) );
      
      break;

   case VKI_FIODTYPE: 
      PRE_MEM_WRITE( "ioctl(FIONREAD)", ARG3, sizeof(int) );
      break;

   case VKI_DTRACEHIOC_ADDDOF: 
       break;

       
   case VKI_TIOCGETA:
       PRE_MEM_WRITE( "ioctl(TIOCGETA)", ARG3, sizeof(struct vki_termios) );
       break;
   case VKI_TIOCSETA:
       PRE_MEM_READ( "ioctl(TIOCSETA)", ARG3, sizeof(struct vki_termios) );
       break;
   case VKI_TIOCGETD:
       PRE_MEM_WRITE( "ioctl(TIOCGETD)", ARG3, sizeof(int) );
       break;
   case VKI_TIOCSETD:
       PRE_MEM_READ( "ioctl(TIOCSETD)", ARG3, sizeof(int) );
       break;
   case VKI_TIOCPTYGNAME:
       PRE_MEM_WRITE( "ioctl(TIOCPTYGNAME)", ARG3, 128 );
       break;

   
   case VKI_FIOCLEX:
       break;
   case VKI_FIONCLEX:
       break;

   default: 
      ML_(PRE_unknown_ioctl)(tid, ARG2, ARG3);
      break;
   }
}


POST(ioctl)
{
   vg_assert(SUCCESS);
   switch (ARG2 ) {
   case VKI_TIOCGWINSZ:
      POST_MEM_WRITE( ARG3, sizeof(struct vki_winsize) );
      break;
   case VKI_TIOCSWINSZ:
   case VKI_TIOCMBIS:
   case VKI_TIOCMBIC:
   case VKI_TIOCMSET:
      break;
   case VKI_TIOCMGET:
      POST_MEM_WRITE( ARG3, sizeof(unsigned int) );
      break;
   case VKI_TIOCGPGRP:
      
      POST_MEM_WRITE( ARG3, sizeof(vki_pid_t) );
      break;
   case VKI_TIOCSPGRP:
      
      POST_MEM_WRITE( ARG3, sizeof(vki_pid_t) );
      break;
   case VKI_TIOCSCTTY:
      break;
   case VKI_FIONBIO:
      break;
   case VKI_FIOASYNC:
      break;
   case VKI_FIONREAD:                
      POST_MEM_WRITE( ARG3, sizeof(int) );
      break;

      
   case VKI_SIOCGIFFLAGS:        
      POST_MEM_WRITE( (Addr)&((struct vki_ifreq *)ARG3)->vki_ifr_flags,
                      sizeof(((struct vki_ifreq *)ARG3)->vki_ifr_flags) );
      break;
   case VKI_SIOCGIFMTU:          
      POST_MEM_WRITE( (Addr)&((struct vki_ifreq *)ARG3)->vki_ifr_mtu,
                      sizeof(((struct vki_ifreq *)ARG3)->vki_ifr_mtu) );
      break;
   case VKI_SIOCGIFADDR:         
   case VKI_SIOCGIFDSTADDR:      
   case VKI_SIOCGIFBRDADDR:      
   case VKI_SIOCGIFNETMASK:      
      POST_MEM_WRITE(
                (Addr)&((struct vki_ifreq *)ARG3)->ifr_addr,
                sizeof(((struct vki_ifreq *)ARG3)->ifr_addr) );
      break;
   case VKI_SIOCGIFMETRIC:       
      POST_MEM_WRITE(
                (Addr)&((struct vki_ifreq *)ARG3)->vki_ifr_metric,
                sizeof(((struct vki_ifreq *)ARG3)->vki_ifr_metric) );
      break;
   case VKI_SIOCGIFCONF:         
      if (RES == 0 && ARG3 ) {
         struct vki_ifconf *ifc = (struct vki_ifconf *) ARG3;
         if (ifc->vki_ifc_buf != NULL)
            POST_MEM_WRITE( (Addr)(ifc->vki_ifc_buf), ifc->ifc_len );
      }
      break;
                    
   case VKI_SIOCSIFFLAGS:        
   case VKI_SIOCSIFDSTADDR:      
   case VKI_SIOCSIFBRDADDR:      
   case VKI_SIOCSIFNETMASK:      
   case VKI_SIOCSIFMETRIC:       
   case VKI_SIOCSIFADDR:         
   case VKI_SIOCSIFMTU:          
      break;

#ifdef VKI_SIOCADDRT
      
   case VKI_SIOCADDRT:           
   case VKI_SIOCDELRT:           
      break;
#endif

   case VKI_SIOCGPGRP:
      POST_MEM_WRITE(ARG3, sizeof(int));
      break;
   case VKI_SIOCSPGRP:
      break;

   case VKI_FIODTYPE: 
      POST_MEM_WRITE( ARG3, sizeof(int) );
      break;

   case VKI_DTRACEHIOC_REMOVE: 
   case VKI_DTRACEHIOC_ADDDOF: 
       break;

       
   case VKI_TIOCGETA:
       POST_MEM_WRITE( ARG3, sizeof(struct vki_termios));
       break;
   case VKI_TIOCSETA:
       break;
   case VKI_TIOCGETD:
       POST_MEM_WRITE( ARG3, sizeof(int) );
       break;
   case VKI_TIOCSETD:
       break;
   case VKI_TIOCPTYGNAME:
       POST_MEM_WRITE( ARG3, 128);
       break;
   case VKI_TIOCSBRK:           
   case VKI_TIOCCBRK:           
   case VKI_TIOCPTYGRANT:
   case VKI_TIOCPTYUNLK:
       break;

   default:
      break;
   }
}


static const HChar *name_for_fcntl(UWord cmd) {
#define F(n) case VKI_##n: return #n
   switch (cmd) {
      F(F_CHKCLEAN);
      F(F_RDAHEAD);
      F(F_NOCACHE);
      F(F_FULLFSYNC);
      F(F_FREEZE_FS);
      F(F_THAW_FS);
      F(F_GLOBAL_NOCACHE);
      F(F_PREALLOCATE);
      F(F_SETSIZE);
      F(F_RDADVISE);
#     if DARWIN_VERS < DARWIN_10_9
      F(F_READBOOTSTRAP);
      F(F_WRITEBOOTSTRAP);
#     endif
      F(F_LOG2PHYS);
      F(F_GETPATH);
      F(F_PATHPKG_CHECK);
      F(F_ADDSIGS);
#     if DARWIN_VERS >= DARWIN_10_9
      F(F_ADDFILESIGS);
#     endif
   default:
      return "UNKNOWN";
   }
#undef F
}

PRE(fcntl)
{
   switch (ARG2) {
   
   case VKI_F_GETFD:
   case VKI_F_GETFL:
   case VKI_F_GETOWN:
      PRINT("fcntl ( %ld, %ld )", ARG1,ARG2);
      PRE_REG_READ2(long, "fcntl", unsigned int, fd, unsigned int, cmd);
      break;

   
   case VKI_F_DUPFD:
   case VKI_F_SETFD:
   case VKI_F_SETFL:
   case VKI_F_SETOWN:
      PRINT("fcntl[ARG3=='arg'] ( %ld, %ld, %ld )", ARG1,ARG2,ARG3);
      PRE_REG_READ3(long, "fcntl",
                    unsigned int, fd, unsigned int, cmd, unsigned long, arg);
      break;

   
   case VKI_F_GETLK:
   case VKI_F_SETLK:
   case VKI_F_SETLKW:
      PRINT("fcntl[ARG3=='lock'] ( %ld, %ld, %#lx )", ARG1,ARG2,ARG3);
      PRE_REG_READ3(long, "fcntl",
                    unsigned int, fd, unsigned int, cmd,
                    struct flock64 *, lock);
      
      if (ARG2 == VKI_F_SETLKW) 
         *flags |= SfMayBlock;
      break;
#  if DARWIN_VERS >= DARWIN_10_10
   case VKI_F_SETLKWTIMEOUT:
      PRINT("fcntl[ARG3=='locktimeout'] ( %ld, %ld, %#lx )", ARG1,ARG2,ARG3);
      PRE_REG_READ3(long, "fcntl",
                    unsigned int, fd, unsigned int, cmd,
                    struct flocktimeout *, lock);
      *flags |= SfMayBlock;
      break;
#  endif

       
   case VKI_F_CHKCLEAN:
   case VKI_F_RDAHEAD:
   case VKI_F_NOCACHE:
   case VKI_F_FULLFSYNC:
   case VKI_F_FREEZE_FS:
   case VKI_F_THAW_FS:
   case VKI_F_GLOBAL_NOCACHE:
      PRINT("fcntl ( %ld, %s )", ARG1, name_for_fcntl(ARG1));
      PRE_REG_READ2(long, "fcntl", unsigned int, fd, unsigned int, cmd);
      break;

       
   case VKI_F_PREALLOCATE:
      PRINT("fcntl ( %ld, %s, %#lx )", ARG1, name_for_fcntl(ARG2), ARG3);
      PRE_REG_READ3(long, "fcntl",
                    unsigned int, fd, unsigned int, cmd,
                    struct fstore *, fstore);
      {
         struct vki_fstore *fstore = (struct vki_fstore *)ARG3;
         PRE_FIELD_READ( "fcntl(F_PREALLOCATE, fstore->fst_flags)", 
                         fstore->fst_flags );
         PRE_FIELD_READ( "fcntl(F_PREALLOCATE, fstore->fst_flags)", 
                         fstore->fst_posmode );
         PRE_FIELD_READ( "fcntl(F_PREALLOCATE, fstore->fst_flags)", 
                         fstore->fst_offset );
         PRE_FIELD_READ( "fcntl(F_PREALLOCATE, fstore->fst_flags)", 
                         fstore->fst_length );
         PRE_FIELD_WRITE( "fcntl(F_PREALLOCATE, fstore->fst_bytesalloc)", 
                          fstore->fst_bytesalloc);
      }
      break;

       
   case VKI_F_SETSIZE:
      PRINT("fcntl ( %ld, %s, %#lx )", ARG1, name_for_fcntl(ARG2), ARG3);
      PRE_REG_READ3(long, "fcntl",
                    unsigned int, fd, unsigned int, cmd,
                    vki_off_t *, offset);
      break;

       
   case VKI_F_RDADVISE:
      PRINT("fcntl ( %ld, %s, %#lx )", ARG1, name_for_fcntl(ARG2), ARG3);
      PRE_REG_READ3(long, "fcntl",
                    unsigned int, fd, unsigned int, cmd,
                    struct vki_radvisory *, radvisory);
      {
         struct vki_radvisory *radvisory = (struct vki_radvisory *)ARG3;
         PRE_FIELD_READ( "fcntl(F_PREALLOCATE, radvisory->ra_offset)", 
                         radvisory->ra_offset );
         PRE_FIELD_READ( "fcntl(F_PREALLOCATE, radvisory->ra_count)", 
                         radvisory->ra_count );
      }
      break;

#  if DARWIN_VERS < DARWIN_10_9
       
   case VKI_F_READBOOTSTRAP:
   case VKI_F_WRITEBOOTSTRAP:
      PRINT("fcntl ( %ld, %s, %#lx )", ARG1, name_for_fcntl(ARG2), ARG3);
      PRE_REG_READ3(long, "fcntl",
                    unsigned int, fd, unsigned int, cmd,
                    struct fbootstraptransfer *, bootstrap);
      PRE_MEM_READ( "fcntl(F_READ/WRITEBOOTSTRAP, bootstrap)", 
                    ARG3, sizeof(struct vki_fbootstraptransfer) );
      break;
#  endif

       
   case VKI_F_LOG2PHYS:
      PRINT("fcntl ( %ld, %s, %#lx )", ARG1, name_for_fcntl(ARG2), ARG3);
      PRE_REG_READ3(long, "fcntl",
                    unsigned int, fd, unsigned int, cmd,
                    struct log2phys *, l2p);
      PRE_MEM_WRITE( "fcntl(F_LOG2PHYS, l2p)", 
                     ARG3, sizeof(struct vki_log2phys) );
      break;

       
   case VKI_F_GETPATH:
      PRINT("fcntl ( %ld, %s, %#lx )", ARG1, name_for_fcntl(ARG2), ARG3);
      PRE_REG_READ3(long, "fcntl",
                    unsigned int, fd, unsigned int, cmd,
                    char *, pathbuf);
      PRE_MEM_WRITE( "fcntl(F_GETPATH, pathbuf)", 
                     ARG3, VKI_MAXPATHLEN );
      break;

       
   case VKI_F_PATHPKG_CHECK:
      PRINT("fcntl ( %ld, %s, %#lx '%s')", ARG1, name_for_fcntl(ARG2), ARG3,
          (char *)ARG3);
      PRE_REG_READ3(long, "fcntl",
                    unsigned int, fd, unsigned int, cmd,
                    char *, pathbuf);
      PRE_MEM_RASCIIZ( "fcntl(F_PATHPKG_CHECK, pathbuf)", ARG3);
      break;

   case VKI_F_ADDSIGS: 
      PRINT("fcntl ( %ld, %s )", ARG1, name_for_fcntl(ARG2));
      PRE_REG_READ3(long, "fcntl",
                    unsigned int, fd, unsigned int, cmd,
                    vki_fsignatures_t *, sigs);

      {
         vki_fsignatures_t *fsigs = (vki_fsignatures_t*)ARG3;
         PRE_FIELD_READ( "fcntl(F_ADDSIGS, fsigs->fs_blob_start)",
                         fsigs->fs_blob_start);
         PRE_FIELD_READ( "fcntl(F_ADDSIGS, fsigs->fs_blob_size)",
                         fsigs->fs_blob_size);

         if (fsigs->fs_blob_start)
            PRE_MEM_READ( "fcntl(F_ADDSIGS, fsigs->fs_blob_start)",
                          (Addr)fsigs->fs_blob_start, fsigs->fs_blob_size);
      }
      break;

   case VKI_F_ADDFILESIGS: 
      PRINT("fcntl ( %ld, %s )", ARG1, name_for_fcntl(ARG2));
      PRE_REG_READ3(long, "fcntl",
                    unsigned int, fd, unsigned int, cmd,
                    vki_fsignatures_t *, sigs);

      {
         vki_fsignatures_t *fsigs = (vki_fsignatures_t*)ARG3;
         PRE_FIELD_READ( "fcntl(F_ADDFILESIGS, fsigs->fs_blob_start)",
                         fsigs->fs_blob_start);
         PRE_FIELD_READ( "fcntl(F_ADDFILESIGS, fsigs->fs_blob_size)",
                         fsigs->fs_blob_size);
      }
      break;

   default:
      PRINT("fcntl ( %ld, %ld [??] )", ARG1, ARG2);
      log_decaying("UNKNOWN fcntl %ld!", ARG2);
      break;
   }
}

POST(fcntl)
{
   vg_assert(SUCCESS);
   switch (ARG2) {
   case VKI_F_DUPFD:
      if (!ML_(fd_allowed)(RES, "fcntl(DUPFD)", tid, True)) {
         VG_(close)(RES);
         SET_STATUS_Failure( VKI_EMFILE );
      } else {
         if (VG_(clo_track_fds))
            ML_(record_fd_open_named)(tid, RES);
      }
      break;

   case VKI_F_GETFD:
   case VKI_F_GETFL:
   case VKI_F_GETOWN:
   case VKI_F_SETFD:
   case VKI_F_SETFL:
   case VKI_F_SETOWN:
   case VKI_F_GETLK:
   case VKI_F_SETLK:
   case VKI_F_SETLKW:
#  if DARWIN_VERS >= DARWIN_10_10
   case VKI_F_SETLKWTIMEOUT:
       break;
#  endif

   case VKI_F_PREALLOCATE:
      {
         struct vki_fstore *fstore = (struct vki_fstore *)ARG3;
         POST_FIELD_WRITE( fstore->fst_bytesalloc );
      }
      break;

   case VKI_F_LOG2PHYS:
      POST_MEM_WRITE( ARG3, sizeof(struct vki_log2phys) );
      break;

   case VKI_F_GETPATH:
      POST_MEM_WRITE( ARG3, 1+VG_(strlen)((char *)ARG3) );
      PRINT("\"%s\"", (char*)ARG3);
      break;

   default:
      
      break;
   }
}


PRE(futimes)
{
   PRINT("futimes ( %ld, %#lx )", ARG1,ARG2);
   PRE_REG_READ2(long, "futimes", int, fd, struct timeval *, tvp);
   if (!ML_(fd_allowed)(ARG1, "futimes", tid, False)) {
      SET_STATUS_Failure( VKI_EBADF );
   } else if (ARG2 != 0) {
      PRE_timeval_READ( "futimes(tvp[0])", ARG2 );
      PRE_timeval_READ( "futimes(tvp[1])", ARG2+sizeof(struct vki_timeval) );
   }
}

PRE(semget)
{
   PRINT("semget ( %ld, %ld, %ld )",ARG1,ARG2,ARG3);
   PRE_REG_READ3(long, "semget", vki_key_t, key, int, nsems, int, semflg);
}

PRE(semop)
{
   *flags |= SfMayBlock;
   PRINT("semop ( %ld, %#lx, %lu )",ARG1,ARG2,ARG3);
   PRE_REG_READ3(long, "semop",
                 int, semid, struct sembuf *, sops, vki_size_t, nsoops);
   ML_(generic_PRE_sys_semop)(tid, ARG1,ARG2,ARG3);
}

PRE(semctl)
{
   switch (ARG3) {
   case VKI_IPC_STAT:
   case VKI_IPC_SET:
      PRINT("semctl ( %ld, %ld, %ld, %#lx )",ARG1,ARG2,ARG3,ARG4);
      PRE_REG_READ4(long, "semctl",
                    int, semid, int, semnum, int, cmd, struct semid_ds *, arg);
      break;
   case VKI_GETALL:
   case VKI_SETALL:
      PRINT("semctl ( %ld, %ld, %ld, %#lx )",ARG1,ARG2,ARG3,ARG4);
      PRE_REG_READ4(long, "semctl",
                    int, semid, int, semnum, int, cmd, unsigned short *, arg);
      break;
   case VKI_SETVAL:
      PRINT("semctl ( %ld, %ld, %ld, %#lx )",ARG1,ARG2,ARG3,ARG4);
      PRE_REG_READ4(long, "semctl",
                    int, semid, int, semnum, int, cmd, int, arg);
      break;
   default:
      PRINT("semctl ( %ld, %ld, %ld )",ARG1,ARG2,ARG3);
      PRE_REG_READ3(long, "semctl",
                    int, semid, int, semnum, int, cmd);
      break;
   }
   ML_(generic_PRE_sys_semctl)(tid, ARG1,ARG2,ARG3,ARG4);
}
POST(semctl)
{
   ML_(generic_POST_sys_semctl)(tid, RES,ARG1,ARG2,ARG3,ARG4);
}

PRE(sem_open)
{
   if (ARG2 & VKI_O_CREAT) {
      
      PRINT("sem_open ( %#lx(%s), %ld, %ld, %ld )",
            ARG1,(char*)ARG1,ARG2,ARG3,ARG4);
      PRE_REG_READ4(vki_sem_t *, "sem_open",
                    const char *, name, int, oflag, vki_mode_t, mode,
                    unsigned int, value);
   } else {
      
      PRINT("sem_open ( %#lx(%s), %ld )",ARG1,(char*)ARG1,ARG2);
      PRE_REG_READ2(vki_sem_t *, "sem_open",
                    const char *, name, int, oflag);
   }
   PRE_MEM_RASCIIZ( "sem_open(name)", ARG1 );

   
   *flags |= SfMayBlock;
}

PRE(sem_close)
{
   PRINT("sem_close( %#lx )", ARG1);
   PRE_REG_READ1(int, "sem_close", vki_sem_t *, sem);
}

PRE(sem_unlink)
{
   PRINT("sem_unlink(  %#lx(%s) )", ARG1,(char*)ARG1);
   PRE_REG_READ1(int, "sem_unlink", const char *, name);
   PRE_MEM_RASCIIZ( "sem_unlink(name)", ARG1 );
}

PRE(sem_post)
{
   PRINT("sem_post( %#lx )", ARG1);
   PRE_REG_READ1(int, "sem_post", vki_sem_t *, sem);
   *flags |= SfMayBlock;
}

PRE(sem_destroy)
{
  PRINT("sem_destroy( %#lx )", ARG1);
  PRE_REG_READ1(int, "sem_destroy", vki_sem_t *, sem);
  PRE_MEM_READ("sem_destroy(sem)", ARG1, sizeof(vki_sem_t));
}

PRE(sem_init)
{
  PRINT("sem_init( %#lx, %ld, %ld )", ARG1, ARG2, ARG3);
  PRE_REG_READ3(int, "sem_init", vki_sem_t *, sem,
                int, pshared, unsigned int, value);
  PRE_MEM_WRITE("sem_init(sem)", ARG1, sizeof(vki_sem_t));
}

POST(sem_init)
{
  POST_MEM_WRITE(ARG1, sizeof(vki_sem_t));
}

PRE(sem_wait)
{
   PRINT("sem_wait( %#lx )", ARG1);
   PRE_REG_READ1(int, "sem_wait", vki_sem_t *, sem);
   *flags |= SfMayBlock;
}

PRE(sem_trywait)
{
   PRINT("sem_trywait( %#lx )", ARG1);
   PRE_REG_READ1(int, "sem_trywait", vki_sem_t *, sem);
   *flags |= SfMayBlock;
}

PRE(kqueue)
{
    PRINT("kqueue()");
}

POST(kqueue)
{
   if (!ML_(fd_allowed)(RES, "kqueue", tid, True)) {
      VG_(close)(RES);
      SET_STATUS_Failure( VKI_EMFILE );
   } else {
      if (VG_(clo_track_fds)) {
         ML_(record_fd_open_with_given_name)(tid, RES, NULL);
      }
   }
}

PRE(fileport_makeport)
{
    PRINT("fileport_makeport(fd:%#lx, portnamep:%#lx) FIXME",
      ARG1, ARG2);
}

PRE(guarded_open_np)
{
    PRINT("guarded_open_np(path:%#lx(%s), guard:%#lx, guardflags:%#lx, flags:%#lx) FIXME",
      ARG1, (char*)ARG1, ARG2, ARG3, ARG4);
}

PRE(guarded_kqueue_np)
{
    PRINT("guarded_kqueue_np(guard:%#lx, guardflags:%#lx) FIXME",
      ARG1, ARG2);
}

POST(guarded_kqueue_np)
{
   if (!ML_(fd_allowed)(RES, "guarded_kqueue_np", tid, True)) {
      VG_(close)(RES);
      SET_STATUS_Failure( VKI_EMFILE );
   } else {
      if (VG_(clo_track_fds)) {
         ML_(record_fd_open_with_given_name)(tid, RES, NULL);
      }
   }
}

PRE(guarded_close_np)
{
    PRINT("guarded_close_np(fd:%#lx, guard:%#lx) FIXME",
      ARG1, ARG2);
}

PRE(change_fdguard_np)
{
    PRINT("change_fdguard_np(fd:%#lx, guard:%#lx, guardflags:%#lx, nguard:%#lx, nguardflags:%#lx, fdflagsp:%#lx) FIXME",
      ARG1, ARG2, ARG3, ARG4, ARG5, ARG6);
}

PRE(connectx)
{
    PRINT("connectx(s:%#lx, src:%#lx, srclen:%#lx, dsts:%#lx, dstlen:%#lx, ifscope:%#lx, aid:%#lx, out_cid:%#lx) FIXME",
      ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7, ARG8);
}

PRE(disconnectx)
{
    PRINT("disconnectx(s:%#lx, aid:%#lx, cid:%#lx) FIXME",
      ARG1, ARG2, ARG3);
}


PRE(kevent)
{
   PRINT("kevent( %ld, %#lx, %ld, %#lx, %ld, %#lx )", 
         ARG1, ARG2, ARG3, ARG4, ARG5, ARG6);
   PRE_REG_READ6(int,"kevent", int,kq, 
                 const struct vki_kevent *,changelist, int,nchanges, 
                 struct vki_kevent *,eventlist, int,nevents, 
                 const struct vki_timespec *,timeout);

   if (ARG3) PRE_MEM_READ ("kevent(changelist)", 
                           ARG2, ARG3 * sizeof(struct vki_kevent));
   if (ARG5) PRE_MEM_WRITE("kevent(eventlist)", 
                           ARG4, ARG5 * sizeof(struct vki_kevent));
   if (ARG6) PRE_MEM_READ ("kevent(timeout)", 
                           ARG6, sizeof(struct vki_timespec));

   *flags |= SfMayBlock;
}

POST(kevent)
{
   PRINT("kevent ret %ld dst %#lx (%zu)", RES, ARG4, sizeof(struct vki_kevent));
   if (RES > 0) POST_MEM_WRITE(ARG4, RES * sizeof(struct vki_kevent));
}


PRE(kevent64)
{
   PRINT("kevent64( %ld, %#lx, %ld, %#lx, %ld, %#lx )",
         ARG1, ARG2, ARG3, ARG4, ARG5, ARG6);
   PRE_REG_READ6(int,"kevent64", int,kq,
                 const struct vki_kevent64 *,changelist, int,nchanges,
                 struct vki_kevent64 *,eventlist, int,nevents,
                 const struct vki_timespec *,timeout);

   if (ARG3) PRE_MEM_READ ("kevent64(changelist)",
                           ARG2, ARG3 * sizeof(struct vki_kevent64));
   if (ARG5) PRE_MEM_WRITE("kevent64(eventlist)",
                           ARG4, ARG5 * sizeof(struct vki_kevent64));
   if (ARG6) PRE_MEM_READ ("kevent64(timeout)",
                           ARG6, sizeof(struct vki_timespec));

   *flags |= SfMayBlock;
}

POST(kevent64)
{
   PRINT("kevent64 ret %ld dst %#lx (%zu)", RES, ARG4, sizeof(struct vki_kevent64));
   if (RES > 0) {
      ML_(sync_mappings)("after", "kevent64", 0);
      POST_MEM_WRITE(ARG4, RES * sizeof(struct vki_kevent64));
   }
}


Addr pthread_starter = 0;
Addr wqthread_starter = 0;
SizeT pthread_structsize = 0;

PRE(bsdthread_register)
{
   PRINT("bsdthread_register( %#lx, %#lx, %lu )", ARG1, ARG2, ARG3);
   PRE_REG_READ3(int,"__bsdthread_register", void *,"threadstart", 
                 void *,"wqthread", size_t,"pthsize");

   pthread_starter = ARG1;
   wqthread_starter = ARG2;
   pthread_structsize = ARG3;
   ARG1 = (Word)&pthread_hijack_asm;
   ARG2 = (Word)&wqthread_hijack_asm;
}

PRE(workq_open)
{
   PRINT("workq_open()");
   PRE_REG_READ0(int, "workq_open");

   
   
}

static const HChar *workqop_name(int op)
{
   switch (op) {
   case VKI_WQOPS_QUEUE_ADD:        return "QUEUE_ADD";
   case VKI_WQOPS_QUEUE_REMOVE:     return "QUEUE_REMOVE";
   case VKI_WQOPS_THREAD_RETURN:    return "THREAD_RETURN";
   case VKI_WQOPS_THREAD_SETCONC:   return "THREAD_SETCONC";
   case VKI_WQOPS_QUEUE_NEWSPISUPP: return "QUEUE_NEWSPISUPP";
   case VKI_WQOPS_QUEUE_REQTHREADS: return "QUEUE_REQTHREADS";
   default: return "?";
   }
}


PRE(workq_ops)
{
   PRINT("workq_ops( %ld(%s), %#lx, %ld )", ARG1, workqop_name(ARG1), ARG2,
      ARG3);
   PRE_REG_READ3(int,"workq_ops", int,"options", void *,"item", 
                 int,"priority");

   switch (ARG1) {
   case VKI_WQOPS_QUEUE_ADD:
   case VKI_WQOPS_QUEUE_REMOVE:
      
      
      break;
   case VKI_WQOPS_QUEUE_NEWSPISUPP:
      
      
      break;
   case VKI_WQOPS_QUEUE_REQTHREADS:
      
      *flags |= SfMayBlock; 
      break;
   case VKI_WQOPS_THREAD_RETURN: {
      
      
      
      
      
      
      
      
      
      
      ThreadState *tst = VG_(get_ThreadState)(tid);
      tst->os_state.wq_jmpbuf_valid = True;
      *flags |= SfMayBlock;  
      break;
   }
   default:
      VG_(printf)("UNKNOWN workq_ops option %ld\n", ARG1);
      break;
   }
}
POST(workq_ops)
{
   ThreadState *tst = VG_(get_ThreadState)(tid);
   tst->os_state.wq_jmpbuf_valid = False;
   switch (ARG1) {
      case VKI_WQOPS_THREAD_RETURN:
         ML_(sync_mappings)("after", "workq_ops(THREAD_RETURN)", 0);
         break;
      case VKI_WQOPS_QUEUE_REQTHREADS:
         ML_(sync_mappings)("after", "workq_ops(QUEUE_REQTHREADS)", 0);
         break;
      default:
         break;
   }
}



PRE(__mac_syscall)
{
   PRINT("__mac_syscall( %#lx(%s), %ld, %#lx )",
         ARG1, (HChar*)ARG1, ARG2, ARG3);
   PRE_REG_READ3(int,"__mac_syscall", char *,"policy", 
                 int,"call", void *,"arg");

   
   
}


PRE(exit)
{
   ThreadId     t;
   ThreadState* tst;

   PRINT("darwin exit( %ld )", ARG1);
   PRE_REG_READ1(void, "exit", int, status);

   tst = VG_(get_ThreadState)(tid);

   for (t = 1; t < VG_N_THREADS; t++) {
      if ( 
           VG_(threads)[t].status == VgTs_Empty 
           
         )
         continue;

      VG_(threads)[t].exitreason = VgSrc_ExitProcess;
      VG_(threads)[t].os_state.exitcode = ARG1;

      if (t != tid)
         VG_(get_thread_out_of_syscall)(t);     
   }

   
   SET_STATUS_Success(0);
}


PRE(sigaction)
{
   PRINT("sigaction ( %ld, %#lx, %#lx )", ARG1,ARG2,ARG3);
   PRE_REG_READ3(long, "sigaction",
                 int, signum, vki_sigaction_toK_t *, act,
                 vki_sigaction_fromK_t *, oldact);

   if (ARG2 != 0) {
      vki_sigaction_toK_t *sa = (vki_sigaction_toK_t *)ARG2;
      PRE_MEM_READ( "sigaction(act->sa_handler)",
                    (Addr)&sa->ksa_handler, sizeof(sa->ksa_handler));
      PRE_MEM_READ( "sigaction(act->sa_mask)",
                    (Addr)&sa->sa_mask, sizeof(sa->sa_mask));
      PRE_MEM_READ( "sigaction(act->sa_flags)",
                    (Addr)&sa->sa_flags, sizeof(sa->sa_flags));
   }
   if (ARG3 != 0)
      PRE_MEM_WRITE( "sigaction(oldact)",
                     ARG3, sizeof(vki_sigaction_fromK_t));

   SET_STATUS_from_SysRes(
      VG_(do_sys_sigaction)(ARG1, (const vki_sigaction_toK_t *)ARG2,
                                  (vki_sigaction_fromK_t *)ARG3)
   );
}
POST(sigaction)
{
   vg_assert(SUCCESS);
   if (RES == 0 && ARG3 != 0)
      POST_MEM_WRITE( ARG3, sizeof(vki_sigaction_fromK_t));
}


PRE(__pthread_kill)
{
   PRINT("__pthread_kill ( %ld, %ld )", ARG1, ARG2);
   PRE_REG_READ2(long, "__pthread_kill", vki_pthread_t*, thread, int, sig);
}


PRE(__pthread_sigmask)
{
   
   
   
   log_decaying("UNKNOWN __pthread_sigmask is unsupported.");
   SET_STATUS_Success( 0 );
}


PRE(__pthread_canceled)
{
   *flags |= SfMayBlock; 
   PRINT("__pthread_canceled ( %ld )", ARG1);
   PRE_REG_READ1(long, "__pthread_canceled", void*, arg1);
}


PRE(__pthread_markcancel)
{
   *flags |= SfMayBlock; 
   PRINT("__pthread_markcancel ( %#lx )", ARG1);
   PRE_REG_READ1(long, "__pthread_markcancel", void*, arg1);
   
}


PRE(__disable_threadsignal)
{
   vki_sigset_t set;
   PRINT("__disable_threadsignal(%ld, %ld, %ld)", ARG1, ARG2, ARG3);

   VG_(sigfillset)( &set );
   SET_STATUS_from_SysRes(
      VG_(do_sys_sigprocmask) ( tid, VKI_SIG_BLOCK, &set, NULL )
   );

   if (SUCCESS)
      *flags |= SfPollAfter;
}


PRE(__pthread_chdir)
{
    PRINT("__pthread_chdir ( %#lx(%s) )", ARG1, (char*)ARG1);
    PRE_REG_READ1(long, "__pthread_chdir", const char *, path);
    PRE_MEM_RASCIIZ( "__pthread_chdir(path)", ARG1 );
}



PRE(__pthread_fchdir)
{
    PRINT("__pthread_fchdir ( %ld )", ARG1);
    PRE_REG_READ1(long, "__pthread_fchdir", unsigned int, fd);
}


PRE(kdebug_trace)
{
   PRINT("kdebug_trace(%ld, %ld, %ld, %ld, %ld, %ld)", 
         ARG1, ARG2, ARG3, ARG4, ARG5, ARG6);
}


PRE(seteuid)
{
    PRINT("seteuid(%ld)", ARG1);
    PRE_REG_READ1(long, "seteuid", vki_uid_t, "uid");
}


PRE(setegid)
{
    PRINT("setegid(%ld)", ARG1);
    PRE_REG_READ1(long, "setegid", vki_uid_t, "uid");
}

PRE(settid)
{
    PRINT("settid(%ld, %ld)", ARG1, ARG2);
    PRE_REG_READ2(long, "settid", vki_uid_t, "uid", vki_gid_t, "gid");
}

PRE(gettid)
{
    PRINT("gettid()");
    PRE_REG_READ0(long, gettid);
}

PRE(watchevent)
{
    PRINT("watchevent(%#lx, %#lx)", ARG1, ARG2);
    PRE_REG_READ2(long, "watchevent",
        vki_eventreq *, "event", unsigned int, "eventmask");

    PRE_MEM_READ("watchevent(event)", ARG1, sizeof(vki_eventreq));
    PRE_MEM_READ("watchevent(eventmask)", ARG2, sizeof(unsigned int));
    *flags |= SfMayBlock;
}

#define WAITEVENT_FAST_POLL ((Addr)(struct timeval *)-1)
PRE(waitevent)
{
   PRINT("waitevent(%#lx, %#lx)", ARG1, ARG2);
   PRE_REG_READ2(long, "waitevent",
      vki_eventreq *, "event", struct timeval *, "timeout");
   PRE_MEM_WRITE("waitevent(event)", ARG1, sizeof(vki_eventreq));

   if (ARG2  &&  ARG2 != WAITEVENT_FAST_POLL) {
      PRE_timeval_READ("waitevent(timeout)", ARG2);
   }

   
   *flags |= SfMayBlock;
}

POST(waitevent)
{
   POST_MEM_WRITE(ARG1, sizeof(vki_eventreq));
}

PRE(modwatch)
{
   PRINT("modwatch(%#lx, %#lx)", ARG1, ARG2);
   PRE_REG_READ2(long, "modwatch",
      vki_eventreq *, "event", unsigned int, "eventmask");

   PRE_MEM_READ("modwatch(event)", ARG1, sizeof(vki_eventreq));
   PRE_MEM_READ("modwatch(eventmask)", ARG2, sizeof(unsigned int));
}

PRE(getxattr)
{
   PRINT("getxattr(%#lx(%s), %#lx(%s), %#lx, %lu, %lu, %ld)",
         ARG1, (char *)ARG1, ARG2, (char *)ARG2, ARG3, ARG4, ARG5, ARG6);

   PRE_REG_READ6(vki_ssize_t, "getxattr",
                const char *, path, char *, name, void *, value,
                vki_size_t, size, uint32_t, position, int, options);
   PRE_MEM_RASCIIZ("getxattr(path)", ARG1);
   PRE_MEM_RASCIIZ("getxattr(name)", ARG2);
   if (ARG3)
      PRE_MEM_WRITE( "getxattr(value)", ARG3, ARG4);
}

POST(getxattr)
{
   vg_assert((vki_ssize_t)RES >= 0);
   if (ARG3)
      POST_MEM_WRITE(ARG3, (vki_ssize_t)RES);
}

PRE(fgetxattr)
{
   PRINT("fgetxattr(%ld, %#lx(%s), %#lx, %lu, %lu, %ld)",
      ARG1, ARG2, (char *)ARG2, ARG3, ARG4, ARG5, ARG6);

   PRE_REG_READ6(vki_ssize_t, "fgetxattr",
                 int, fd, char *, name, void *, value,
                 vki_size_t, size, uint32_t, position, int, options);
   PRE_MEM_RASCIIZ("getxattr(name)", ARG2);
   PRE_MEM_WRITE( "getxattr(value)", ARG3, ARG4);
}

POST(fgetxattr)
{
   vg_assert((vki_ssize_t)RES >= 0);
   POST_MEM_WRITE(ARG3, (vki_ssize_t)RES);
}

PRE(setxattr)
{
   PRINT("setxattr ( %#lx(%s), %#lx(%s), %#lx, %lu, %lu, %ld )", 
         ARG1, (char *)ARG1, ARG2, (char*)ARG2, ARG3, ARG4, ARG5, ARG6 );
   PRE_REG_READ6(int, "setxattr", 
                 const char *,"path", char *,"name", void *,"value", 
                 vki_size_t,"size", uint32_t,"position", int,"options" );
   
   PRE_MEM_RASCIIZ( "setxattr(path)", ARG1 );
   PRE_MEM_RASCIIZ( "setxattr(name)", ARG2 );
   PRE_MEM_READ( "setxattr(value)", ARG3, ARG4 );
}


PRE(fsetxattr)
{
   PRINT( "fsetxattr ( %ld, %#lx(%s), %#lx, %lu, %lu, %ld )", 
          ARG1, ARG2, (char*)ARG2, ARG3, ARG4, ARG5, ARG6 );
   PRE_REG_READ6(int, "fsetxattr", 
                 int,"fd", char *,"name", void *,"value", 
                 vki_size_t,"size", uint32_t,"position", int,"options" );
   
   PRE_MEM_RASCIIZ( "fsetxattr(name)", ARG2 );
   PRE_MEM_READ( "fsetxattr(value)", ARG3, ARG4 );
}


PRE(removexattr)
{
   PRINT( "removexattr ( %#lx(%s), %#lx(%s), %ld )",
          ARG1, (HChar*)ARG1, ARG2, (HChar*)ARG2, ARG3 );
   PRE_REG_READ3(int, "removexattr",
                 const char*, "path", char*, "attrname", int, "options");
   PRE_MEM_RASCIIZ( "removexattr(path)", ARG1 );
   PRE_MEM_RASCIIZ( "removexattr(attrname)", ARG2 );
}


PRE(fremovexattr)
{
   PRINT( "fremovexattr ( %ld, %#lx(%s), %ld )",
          ARG1, ARG2, (HChar*)ARG2, ARG3 );
   PRE_REG_READ3(int, "fremovexattr",
                 int, "fd", char*, "attrname", int, "options");
   PRE_MEM_RASCIIZ( "removexattr(attrname)", ARG2 );
}


PRE(listxattr)
{
   PRINT( "listxattr ( %#lx(%s), %#lx, %lu, %ld )", 
          ARG1, (char *)ARG1, ARG2, ARG3, ARG4 );
   PRE_REG_READ4 (long, "listxattr", 
                 const char *,"path", char *,"namebuf", 
                 vki_size_t,"size", int,"options" );
   
   PRE_MEM_RASCIIZ( "listxattr(path)", ARG1 );
   PRE_MEM_WRITE( "listxattr(namebuf)", ARG2, ARG3 );
   *flags |= SfMayBlock;
}
POST(listxattr)
{
   vg_assert(SUCCESS);
   vg_assert((vki_ssize_t)RES >= 0);
   POST_MEM_WRITE( ARG2, (vki_ssize_t)RES );
}


PRE(flistxattr)
{
   PRINT( "flistxattr ( %ld, %#lx, %lu, %ld )", 
          ARG1, ARG2, ARG3, ARG4 );
   PRE_REG_READ4 (long, "flistxattr", 
                  int, "fd", char *,"namebuf", 
                 vki_size_t,"size", int,"options" );
   PRE_MEM_WRITE( "flistxattr(namebuf)", ARG2, ARG3 );
   *flags |= SfMayBlock;
}
POST(flistxattr)
{
   vg_assert(SUCCESS);
   vg_assert((vki_ssize_t)RES >= 0);
   POST_MEM_WRITE( ARG2, (vki_ssize_t)RES );
}


PRE(shmat)
{
   UWord arg2tmp;
   PRINT("shmat ( %ld, %#lx, %ld )",ARG1,ARG2,ARG3);
   PRE_REG_READ3(long, "shmat",
                 int, shmid, const void *, shmaddr, int, shmflg);
   arg2tmp = ML_(generic_PRE_sys_shmat)(tid, ARG1,ARG2,ARG3);
   if (arg2tmp == 0)
      SET_STATUS_Failure( VKI_EINVAL );
   else
      ARG2 = arg2tmp;  
}
POST(shmat)
{
   ML_(generic_POST_sys_shmat)(tid, RES,ARG1,ARG2,ARG3);
}

PRE(shmctl)
{
   PRINT("shmctl ( %ld, %ld, %#lx )",ARG1,ARG2,ARG3);
   PRE_REG_READ3(long, "shmctl",
                 int, shmid, int, cmd, struct vki_shmid_ds *, buf);
   ML_(generic_PRE_sys_shmctl)(tid, ARG1,ARG2,ARG3);
}
POST(shmctl)
{
   ML_(generic_POST_sys_shmctl)(tid, RES,ARG1,ARG2,ARG3);
}

PRE(shmdt)
{
   PRINT("shmdt ( %#lx )",ARG1);
   PRE_REG_READ1(long, "shmdt", const void *, shmaddr);
   if (!ML_(generic_PRE_sys_shmdt)(tid, ARG1))
      SET_STATUS_Failure( VKI_EINVAL );
}
POST(shmdt)
{
   ML_(generic_POST_sys_shmdt)(tid, RES,ARG1);
}

PRE(shmget)
{
   PRINT("shmget ( %ld, %ld, %ld )",ARG1,ARG2,ARG3);
   PRE_REG_READ3(long, "shmget", vki_key_t, key, vki_size_t, size, int, shmflg);
}

PRE(shm_open)
{
   PRINT("shm_open(%#lx(%s), %ld, %ld)", ARG1, (char *)ARG1, ARG2, ARG3);
   PRE_REG_READ3(long, "shm_open",
                 const char *,"name", int,"flags", vki_mode_t,"mode");

   PRE_MEM_RASCIIZ( "shm_open(filename)", ARG1 );

   *flags |= SfMayBlock;
}
POST(shm_open)
{
   vg_assert(SUCCESS);
   if (!ML_(fd_allowed)(RES, "shm_open", tid, True)) {
      VG_(close)(RES);
      SET_STATUS_Failure( VKI_EMFILE );
   } else {
      if (VG_(clo_track_fds))
         ML_(record_fd_open_with_given_name)(tid, RES, (char*)ARG1);
   }
}

PRE(shm_unlink)
{
   *flags |= SfMayBlock;
   PRINT("shm_unlink ( %#lx(%s) )", ARG1,(char*)ARG1);
   PRE_REG_READ1(long, "shm_unlink", const char *, pathname);
   PRE_MEM_RASCIIZ( "shm_unlink(pathname)", ARG1 );
}
POST(shm_unlink)
{
   ML_(sync_mappings)("after", "shm_unlink", 0);
}

PRE(stat_extended)
{
   PRINT("stat_extended( %#lx(%s), %#lx, %#lx, %#lx )",
      ARG1, (char *)ARG1, ARG2, ARG3, ARG4);
   PRE_REG_READ4(int, "stat_extended", char *, file_name, struct stat *, buf, 
                 void *, fsacl, vki_size_t *, fsacl_size);
   PRE_MEM_RASCIIZ( "stat_extended(file_name)",  ARG1 );
   PRE_MEM_WRITE(   "stat_extended(buf)",        ARG2, sizeof(struct vki_stat) );
   if (ML_(safe_to_deref)( (void*)ARG4, sizeof(vki_size_t) ))
      PRE_MEM_WRITE("stat_extended(fsacl)",      ARG3, *(vki_size_t *)ARG4 );
   PRE_MEM_READ(    "stat_extended(fsacl_size)", ARG4, sizeof(vki_size_t) );
}
POST(stat_extended)
{
   POST_MEM_WRITE( ARG2, sizeof(struct vki_stat) );
   if (ML_(safe_to_deref)( (void*)ARG4, sizeof(vki_size_t) ))
      POST_MEM_WRITE( ARG3, *(vki_size_t *)ARG4 );
   POST_MEM_WRITE( ARG4, sizeof(vki_size_t) );
}


PRE(lstat_extended)
{
   PRINT("lstat_extended( %#lx(%s), %#lx, %#lx, %#lx )",
      ARG1, (char *)ARG1, ARG2, ARG3, ARG4);
   PRE_REG_READ4(int, "lstat_extended", char *, file_name, struct stat *, buf, 
                 void *, fsacl, vki_size_t *, fsacl_size);
   PRE_MEM_RASCIIZ( "lstat_extended(file_name)",  ARG1 );
   PRE_MEM_WRITE(   "lstat_extended(buf)",        ARG2, sizeof(struct vki_stat) );
   if (ML_(safe_to_deref)( (void*)ARG4, sizeof(vki_size_t) ))
      PRE_MEM_WRITE("lstat_extended(fsacl)",      ARG3, *(vki_size_t *)ARG4 );
   PRE_MEM_READ(    "lstat_extended(fsacl_size)", ARG4, sizeof(vki_size_t) );
}
POST(lstat_extended)
{
   POST_MEM_WRITE( ARG2, sizeof(struct vki_stat) );
   if (ML_(safe_to_deref)( (void*)ARG4, sizeof(vki_size_t) ))
      POST_MEM_WRITE( ARG3, *(vki_size_t *)ARG4 );
   POST_MEM_WRITE( ARG4, sizeof(vki_size_t) );
}


PRE(fstat_extended)
{
   PRINT("fstat_extended( %ld, %#lx, %#lx, %#lx )",
      ARG1, ARG2, ARG3, ARG4);
   PRE_REG_READ4(int, "fstat_extended", int, fd, struct stat *, buf, 
                 void *, fsacl, vki_size_t *, fsacl_size);
   PRE_MEM_WRITE(   "fstat_extended(buf)",        ARG2, sizeof(struct vki_stat) );
   if (ML_(safe_to_deref)( (void*)ARG4, sizeof(vki_size_t) ))
      PRE_MEM_WRITE("fstat_extended(fsacl)",      ARG3, *(vki_size_t *)ARG4 );
   PRE_MEM_READ(    "fstat_extended(fsacl_size)", ARG4, sizeof(vki_size_t) );
}
POST(fstat_extended)
{
   POST_MEM_WRITE( ARG2, sizeof(struct vki_stat) );
   if (ML_(safe_to_deref)( (void*)ARG4, sizeof(vki_size_t) ))
      POST_MEM_WRITE( ARG3, *(vki_size_t *)ARG4 );
   POST_MEM_WRITE( ARG4, sizeof(vki_size_t) );
}


PRE(stat64_extended)
{
   PRINT("stat64_extended( %#lx(%s), %#lx, %#lx, %#lx )",
      ARG1, (char *)ARG1, ARG2, ARG3, ARG4);
   PRE_REG_READ4(int, "stat64_extended", char *, file_name, struct stat64 *, buf, 
                 void *, fsacl, vki_size_t *, fsacl_size);
   PRE_MEM_RASCIIZ( "stat64_extended(file_name)",  ARG1 );
   PRE_MEM_WRITE(   "stat64_extended(buf)",        ARG2, sizeof(struct vki_stat64) );
   if (ML_(safe_to_deref)( (void*)ARG4, sizeof(vki_size_t) ))
      PRE_MEM_WRITE("stat64_extended(fsacl)",      ARG3, *(vki_size_t *)ARG4 );
   PRE_MEM_READ(    "stat64_extended(fsacl_size)", ARG4, sizeof(vki_size_t) );
}
POST(stat64_extended)
{
   POST_MEM_WRITE( ARG2, sizeof(struct vki_stat64) );
   if (ML_(safe_to_deref)( (void*)ARG4, sizeof(vki_size_t) ))
      POST_MEM_WRITE( ARG3, *(vki_size_t *)ARG4 );
   POST_MEM_WRITE( ARG4, sizeof(vki_size_t) );
}


PRE(lstat64_extended)
{
   PRINT("lstat64_extended( %#lx(%s), %#lx, %#lx, %#lx )",
      ARG1, (char *)ARG1, ARG2, ARG3, ARG4);
   PRE_REG_READ4(int, "lstat64_extended", char *, file_name, struct stat64 *, buf, 
                 void *, fsacl, vki_size_t *, fsacl_size);
   PRE_MEM_RASCIIZ( "lstat64_extended(file_name)",  ARG1 );
   PRE_MEM_WRITE(   "lstat64_extended(buf)",        ARG2, sizeof(struct vki_stat64) );
   if (ML_(safe_to_deref)( (void*)ARG4, sizeof(vki_size_t) ))
      PRE_MEM_WRITE(   "lstat64_extended(fsacl)",   ARG3, *(vki_size_t *)ARG4 );
   PRE_MEM_READ(    "lstat64_extended(fsacl_size)", ARG4, sizeof(vki_size_t) );
}
POST(lstat64_extended)
{
   POST_MEM_WRITE( ARG2, sizeof(struct vki_stat64) );
   if (ML_(safe_to_deref)( (void*)ARG4, sizeof(vki_size_t) ))
      POST_MEM_WRITE( ARG3, *(vki_size_t *)ARG4 );
   POST_MEM_WRITE( ARG4, sizeof(vki_size_t) );
}


PRE(fstat64_extended)
{
   PRINT("fstat64_extended( %ld, %#lx, %#lx, %#lx )",
      ARG1, ARG2, ARG3, ARG4);
   PRE_REG_READ4(int, "fstat64_extended", int, fd, struct stat64 *, buf, 
                 void *, fsacl, vki_size_t *, fsacl_size);
   PRE_MEM_WRITE(   "fstat64_extended(buf)",        ARG2, sizeof(struct vki_stat64) );
   if (ML_(safe_to_deref)( (void*)ARG4, sizeof(vki_size_t) ))
      PRE_MEM_WRITE("fstat64_extended(fsacl)",      ARG3, *(vki_size_t *)ARG4 );
   PRE_MEM_READ(    "fstat64_extended(fsacl_size)", ARG4, sizeof(vki_size_t) );
}
POST(fstat64_extended)
{
   POST_MEM_WRITE( ARG2, sizeof(struct vki_stat64) );
   if (ML_(safe_to_deref)( (void*)ARG4, sizeof(vki_size_t) ))
      POST_MEM_WRITE( ARG3, *(vki_size_t *)ARG4 );
   POST_MEM_WRITE( ARG4, sizeof(vki_size_t) );
}


PRE(fchmod_extended)
{
   PRINT("fchmod_extended ( %ld, %ld, %ld, %ld, %#lx )",
         ARG1, ARG2, ARG3, ARG4, ARG5);
   PRE_REG_READ5(long, "fchmod_extended", 
                 unsigned int, fildes, 
                 uid_t, uid,
                 gid_t, gid,
                 vki_mode_t, mode,
                 void* , xsecurity);
   if (ARG5) {
      PRE_MEM_READ( "fchmod_extended(xsecurity)", ARG5, 
                    sizeof(struct vki_kauth_filesec) );
   }
}

PRE(chmod_extended)
{
   PRINT("chmod_extended ( %#lx(%s), %ld, %ld, %ld, %#lx )",
         ARG1, ARG1 ? (HChar*)ARG1 : "(null)", ARG2, ARG3, ARG4, ARG5);
   PRE_REG_READ5(long, "chmod_extended", 
                 unsigned int, fildes, 
                 uid_t, uid,
                 gid_t, gid,
                 vki_mode_t, mode,
                 void* , xsecurity);
   PRE_MEM_RASCIIZ("chmod_extended(path)", ARG1);
   if (ARG5) {
      PRE_MEM_READ( "chmod_extended(xsecurity)", ARG5, 
                    sizeof(struct vki_kauth_filesec) );
   }
}

PRE(open_extended)
{
   PRINT("open_extended ( %#lx(%s), 0x%lx, %ld, %ld, %ld, %#lx )",
         ARG1, ARG1 ? (HChar*)ARG1 : "(null)",
	 ARG2, ARG3, ARG4, ARG5, ARG6);
   PRE_REG_READ6(long, "open_extended", 
                 char*, path,
                 int,   flags,
                 uid_t, uid,
                 gid_t, gid,
                 vki_mode_t, mode,
                 void* , xsecurity);
   PRE_MEM_RASCIIZ("open_extended(path)", ARG1);
   if (ARG6)
      PRE_MEM_READ( "open_extended(xsecurity)", ARG6, 
                    sizeof(struct vki_kauth_filesec) );
}

PRE(access_extended)
{
   PRINT("access_extended( %#lx(%s), %lu, %#lx, %lu )",
      ARG1, (char *)ARG1, ARG2, ARG3, ARG4);
   
   
   
   PRE_REG_READ4(int, "access_extended", void *, entries, vki_size_t, size, 
                 vki_errno_t *, results, vki_uid_t *, uid);
   PRE_MEM_READ("access_extended(entries)", ARG1, ARG2 );

   
   
   
}
POST(access_extended)
{
   
   
   
   
   
   struct vki_accessx_descriptor* entries = (struct vki_accessx_descriptor*)ARG1;
   SizeT size = ARG2;
   Int n_descs = (size - 2) / sizeof(struct accessx_descriptor);
   Int i;         
   Int u;         
                  
   vg_assert(n_descs > 0);

   
   
   for (i = 0; True; i++) {
      
      
      if (i >= n_descs)
         break;

      
      
      
      
      u = entries[i].ad_name_offset / sizeof(struct vki_accessx_descriptor);
      if (u == 0) {
         vg_assert(i != 0);
         continue;
      }

      
      
      if (u < n_descs)
         n_descs = u;
   }

   
   vg_assert(n_descs <= VKI_ACCESSX_MAX_DESCRIPTORS);

   POST_MEM_WRITE( ARG3, n_descs * sizeof(vki_errno_t) );
}


PRE(chflags)
{
   PRINT("chflags ( %#lx(%s), %lu )", ARG1, (char *)ARG1, ARG2);
   PRE_REG_READ2(int, "chflags", const char *,path, unsigned int,flags);
   PRE_MEM_RASCIIZ("chflags(path)", ARG1);

   
}

PRE(fchflags)
{
   PRINT("fchflags ( %ld, %lu )", ARG1, ARG2);
   PRE_REG_READ2(int, "fchflags", int,fd, unsigned int,flags);

   
}

PRE(stat64)
{
   PRINT("stat64 ( %#lx(%s), %#lx )", ARG1, (char *)ARG1, ARG2);
   PRE_REG_READ2(long, "stat", const char *,path, struct stat64 *,buf);
   PRE_MEM_RASCIIZ("stat64(path)", ARG1);
   PRE_MEM_WRITE( "stat64(buf)", ARG2, sizeof(struct vki_stat64) );
}
POST(stat64)
{
   POST_MEM_WRITE( ARG2, sizeof(struct vki_stat64) );
}

PRE(lstat64)
{
   PRINT("lstat64 ( %#lx(%s), %#lx )", ARG1, (char *)ARG1, ARG2);
   PRE_REG_READ2(long, "stat", const char *,path, struct stat64 *,buf);
   PRE_MEM_RASCIIZ("lstat64(path)", ARG1);
   PRE_MEM_WRITE( "lstat64(buf)", ARG2, sizeof(struct vki_stat64) );
}
POST(lstat64)
{
   POST_MEM_WRITE( ARG2, sizeof(struct vki_stat64) );
}

PRE(fstat64)
{
   PRINT("fstat64 ( %ld, %#lx )", ARG1,ARG2);
   PRE_REG_READ2(long, "fstat", unsigned int, fd, struct stat64 *, buf);
   PRE_MEM_WRITE( "fstat64(buf)", ARG2, sizeof(struct vki_stat64) );
}
POST(fstat64)
{
   POST_MEM_WRITE( ARG2, sizeof(struct vki_stat64) );
}

PRE(getfsstat)
{
   PRINT("getfsstat(%#lx, %ld, %ld)", ARG1, ARG2, ARG3);
   PRE_REG_READ3(int, "getfsstat",
                 struct vki_statfs *, buf, int, bufsize, int, flags);
   if (ARG1) {
      
      PRE_MEM_WRITE("getfsstat(buf)", ARG1, ARG2);
   }
}
POST(getfsstat)
{
   if (ARG1) {
      
      POST_MEM_WRITE(ARG1, RES * sizeof(struct vki_statfs));
   }
}

PRE(getfsstat64)
{
   PRINT("getfsstat64(%#lx, %ld, %ld)", ARG1, ARG2, ARG3);
   PRE_REG_READ3(int, "getfsstat64",
                 struct vki_statfs64 *, buf, int, bufsize, int, flags);
   if (ARG1) {
      
      PRE_MEM_WRITE("getfsstat64(buf)", ARG1, ARG2);
   }
}
POST(getfsstat64)
{
   if (ARG1) {
      
      POST_MEM_WRITE(ARG1, RES * sizeof(struct vki_statfs64));
   }
}

PRE(mount)
{
   
   
   
   *flags |= SfMayBlock;
   PRINT("sys_mount( %#lx(%s), %#lx(%s), %#lx, %#lx )",
         ARG1,(char*)ARG1, ARG2,(char*)ARG2, ARG3, ARG4);
   PRE_REG_READ4(long, "mount",
                 const char *, type, const char *, dir,
                 int, flags, void *, data);
   PRE_MEM_RASCIIZ( "mount(type)", ARG1);
   PRE_MEM_RASCIIZ( "mount(dir)", ARG2);
}


static void scan_attrlist(ThreadId tid, struct vki_attrlist *attrList, 
                          void *attrBuf, SizeT attrBufSize, 
                          void (*fn)(ThreadId, void *attrData, SizeT size)
                          )
{
   typedef struct {
      uint32_t attrBit;
      int32_t attrSize;
   } attrspec;
   static const attrspec commonattr[] = {
      
#if DARWIN_VERS >= DARWIN_10_6
      { ATTR_CMN_RETURNED_ATTRS,  sizeof(attribute_set_t) }, 
#endif
      { ATTR_CMN_NAME,            -1 }, 
      { ATTR_CMN_DEVID,           sizeof(dev_t) }, 
      { ATTR_CMN_FSID,            sizeof(fsid_t) }, 
      { ATTR_CMN_OBJTYPE,         sizeof(fsobj_type_t) }, 
      { ATTR_CMN_OBJTAG,          sizeof(fsobj_tag_t) }, 
      { ATTR_CMN_OBJID,           sizeof(fsobj_id_t) }, 
      { ATTR_CMN_OBJPERMANENTID,  sizeof(fsobj_id_t) }, 
      { ATTR_CMN_PAROBJID,        sizeof(fsobj_id_t) }, 
      { ATTR_CMN_SCRIPT,          sizeof(text_encoding_t) }, 
      { ATTR_CMN_CRTIME,          sizeof(struct timespec) }, 
      { ATTR_CMN_MODTIME,         sizeof(struct timespec) }, 
      { ATTR_CMN_CHGTIME,         sizeof(struct timespec) }, 
      { ATTR_CMN_ACCTIME,         sizeof(struct timespec) }, 
      { ATTR_CMN_BKUPTIME,        sizeof(struct timespec) }, 
      { ATTR_CMN_FNDRINFO,        32  }, 
      { ATTR_CMN_OWNERID,         sizeof(uid_t) }, 
      { ATTR_CMN_GRPID,           sizeof(gid_t) }, 
      { ATTR_CMN_ACCESSMASK,      sizeof(uint32_t) }, 
      { ATTR_CMN_NAMEDATTRCOUNT,  sizeof(uint32_t) }, 
      { ATTR_CMN_NAMEDATTRLIST,   -1 }, 
      { ATTR_CMN_FLAGS,           sizeof(uint32_t) }, 
      { ATTR_CMN_USERACCESS,      sizeof(uint32_t) }, 
      { ATTR_CMN_EXTENDED_SECURITY, -1 }, 
      { ATTR_CMN_UUID,            sizeof(guid_t) }, 
      { ATTR_CMN_GRPUUID,         sizeof(guid_t) }, 
      { ATTR_CMN_FILEID,          sizeof(uint64_t) }, 
      { ATTR_CMN_PARENTID,        sizeof(uint64_t) }, 
#if DARWIN_VERS >= DARWIN_10_6
      { ATTR_CMN_FULLPATH,        -1 }, 
#endif
#if DARWIN_VERS >= DARWIN_10_8
      { ATTR_CMN_ADDEDTIME,       -1 }, 
#endif
      { 0,                        0 }
   };
   static const attrspec volattr[] = {
      
      { ATTR_VOL_INFO,            0 }, 
      { ATTR_VOL_FSTYPE,          sizeof(uint32_t) }, 
      { ATTR_VOL_SIGNATURE,       sizeof(uint32_t) }, 
      { ATTR_VOL_SIZE,            sizeof(off_t) }, 
      { ATTR_VOL_SPACEFREE,       sizeof(off_t) }, 
      { ATTR_VOL_SPACEAVAIL,      sizeof(off_t) }, 
      { ATTR_VOL_MINALLOCATION,   sizeof(off_t) }, 
      { ATTR_VOL_ALLOCATIONCLUMP, sizeof(off_t) }, 
      { ATTR_VOL_IOBLOCKSIZE,     sizeof(uint32_t) }, 
      { ATTR_VOL_OBJCOUNT,        sizeof(uint32_t) }, 
      { ATTR_VOL_FILECOUNT,       sizeof(uint32_t) }, 
      { ATTR_VOL_DIRCOUNT,        sizeof(uint32_t) }, 
      { ATTR_VOL_MAXOBJCOUNT,     sizeof(uint32_t) }, 
      { ATTR_VOL_MOUNTPOINT,      -1 }, 
      { ATTR_VOL_NAME,            -1 }, 
      { ATTR_VOL_MOUNTFLAGS,      sizeof(uint32_t) }, 
      { ATTR_VOL_MOUNTEDDEVICE,   -1 }, 
      { ATTR_VOL_ENCODINGSUSED,   sizeof(uint64_t) }, 
      { ATTR_VOL_CAPABILITIES,    sizeof(vol_capabilities_attr_t) }, 
#if DARWIN_VERS >= DARWIN_10_6
      { ATTR_VOL_UUID,            sizeof(uuid_t) }, 
#endif
      { ATTR_VOL_ATTRIBUTES,      sizeof(vol_attributes_attr_t) }, 
      { 0,                        0 }
   };
   static const attrspec dirattr[] = {
      
      { ATTR_DIR_LINKCOUNT,       sizeof(uint32_t) }, 
      { ATTR_DIR_ENTRYCOUNT,      sizeof(uint32_t) }, 
      { ATTR_DIR_MOUNTSTATUS,     sizeof(uint32_t) }, 
      { 0,                        0 }
   };
   static const attrspec fileattr[] = {
      
      { ATTR_FILE_LINKCOUNT,      sizeof(uint32_t) }, 
      { ATTR_FILE_TOTALSIZE,      sizeof(off_t) }, 
      { ATTR_FILE_ALLOCSIZE,      sizeof(off_t) }, 
      { ATTR_FILE_IOBLOCKSIZE,    sizeof(uint32_t) }, 
      { ATTR_FILE_CLUMPSIZE,      sizeof(uint32_t) }, 
      { ATTR_FILE_DEVTYPE,        sizeof(uint32_t) }, 
      { ATTR_FILE_FILETYPE,       sizeof(uint32_t) }, 
      { ATTR_FILE_FORKCOUNT,      sizeof(uint32_t) }, 
      { ATTR_FILE_FORKLIST,       -1 }, 
      { ATTR_FILE_DATALENGTH,     sizeof(off_t) }, 
      { ATTR_FILE_DATAALLOCSIZE,  sizeof(off_t) }, 
      { ATTR_FILE_DATAEXTENTS,    sizeof(extentrecord) }, 
      { ATTR_FILE_RSRCLENGTH,     sizeof(off_t) }, 
      { ATTR_FILE_RSRCALLOCSIZE,  sizeof(off_t) }, 
      { ATTR_FILE_RSRCEXTENTS,    sizeof(extentrecord) }, 
      { 0,                        0 }
   };
   static const attrspec forkattr[] = {
      
      { ATTR_FORK_TOTALSIZE,      sizeof(off_t) }, 
      { ATTR_FORK_ALLOCSIZE,      sizeof(off_t) }, 
      { 0,                        0 }
   };

   static const attrspec *attrdefs[5] = { 
      commonattr, volattr, dirattr, fileattr, forkattr 
   };
   attrgroup_t a[5];
   uint8_t *d, *dend;
   int g, i;

   vg_assert(attrList->bitmapcount == 5);
   VG_(memcpy)(a, &attrList->commonattr, sizeof(a));
   d = attrBuf;
   dend = d + attrBufSize;

#if DARWIN_VERS >= DARWIN_10_6
   
   if (a[0] & ATTR_CMN_RETURNED_ATTRS) {
       
       a[0] &= ~ATTR_CMN_RETURNED_ATTRS;
       fn(tid, d, sizeof(attribute_set_t));
       VG_(memcpy)(a, d, sizeof(a));
   }
#endif

   for (g = 0; g < 5; g++) {
      for (i = 0; attrdefs[g][i].attrBit; i++) {
         uint32_t bit = attrdefs[g][i].attrBit;
         int32_t size = attrdefs[g][i].attrSize;

         if (a[g] & bit) {
             a[g] &= ~bit;  
            if (size == -1) {
               attrreference_t *ref = (attrreference_t *)d;
               size = MIN(sizeof(attrreference_t), dend - d);
               fn(tid, d, size);
               if (size >= sizeof(attrreference_t)  &&  
                   d + ref->attr_dataoffset < dend) 
               {
                  fn(tid, d + ref->attr_dataoffset, 
                     MIN(ref->attr_length, dend - (d + ref->attr_dataoffset)));
               }
               d += size;
            } 
            else {
               size = MIN(size, dend - d);
               fn(tid, d, size);
               d += size;
            }
            
            if ((uintptr_t)d % 4) d += 4 - ((uintptr_t)d % 4);
            if (d > dend) d = dend;
         }
      }

      
      if (a[g] != 0) {
         VG_(message)(Vg_UserMsg, "UNKNOWN attrlist flags %d:0x%x\n", g, a[g]);
      }
   }
}

static void get1attr(ThreadId tid, void *attrData, SizeT attrDataSize)
{
   POST_MEM_WRITE((Addr)attrData, attrDataSize);
}

static void set1attr(ThreadId tid, void *attrData, SizeT attrDataSize)
{
   PRE_MEM_READ("setattrlist(attrBuf value)", (Addr)attrData, attrDataSize);
}

PRE(getattrlist)
{
   PRINT("getattrlist(%#lx(%s), %#lx, %#lx, %lu, %lu)", 
         ARG1, (char *)ARG1, ARG2, ARG3, ARG4, ARG5);
   PRE_REG_READ5(int, "getattrlist", 
                 const char *,path, struct vki_attrlist *,attrList, 
                 void *,attrBuf, vki_size_t,attrBufSize, unsigned int,options);
   PRE_MEM_RASCIIZ("getattrlist(path)", ARG1);
   PRE_MEM_READ("getattrlist(attrList)", ARG2, sizeof(struct vki_attrlist));
   PRE_MEM_WRITE("getattrlist(attrBuf)", ARG3, ARG4);
}

POST(getattrlist) 
{
   if (ARG4 > sizeof(vki_uint32_t)) {
      
      vki_uint32_t *sizep = (vki_uint32_t *)ARG3;
      POST_MEM_WRITE(ARG3, sizeof(vki_uint32_t));
      if (ARG5 & FSOPT_REPORT_FULLSIZE) {
         
      } else {
         
      }
      scan_attrlist(tid, (struct vki_attrlist *)ARG2, sizep+1, MIN(*sizep, ARG4), &get1attr);
   }
}


PRE(setattrlist)
{
   PRINT("setattrlist(%#lx(%s), %#lx, %#lx, %lu, %lu)", 
         ARG1, (char *)ARG1, ARG2, ARG3, ARG4, ARG5);
   PRE_REG_READ5(int, "setattrlist", 
                 const char *,path, struct vki_attrlist *,attrList, 
                 void *,attrBuf, vki_size_t,attrBufSize, unsigned int,options);
   PRE_MEM_RASCIIZ("setattrlist(path)", ARG1);
   PRE_MEM_READ("setattrlist(attrList)", ARG2, sizeof(struct vki_attrlist));
   scan_attrlist(tid, (struct vki_attrlist *)ARG2, (void*)ARG3, ARG4, &set1attr);
}


PRE(getdirentriesattr)
{
   PRINT("getdirentriesattr(%ld, %#lx, %#lx, %ld, %#lx, %#lx, %#lx, %ld)", 
         ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7, ARG8);
   PRE_REG_READ8(int, "getdirentriesattr", 
                 int,fd, struct vki_attrlist *,attrList, 
                 void *,attrBuf, size_t,attrBufSize, 
                 unsigned int *,count, unsigned int *,basep, 
                 unsigned int *,newState, unsigned int,options);
   PRE_MEM_READ("getdirentriesattr(attrList)", 
                ARG2, sizeof(struct vki_attrlist));
   PRE_MEM_WRITE("getdirentriesattr(attrBuf)", ARG3, ARG4);
   PRE_MEM_READ("getdirentriesattr(count)", ARG5, sizeof(unsigned int));
   PRE_MEM_WRITE("getdirentriesattr(count)", ARG5, sizeof(unsigned int));
   PRE_MEM_WRITE("getdirentriesattr(basep)", ARG6, sizeof(unsigned int));
   PRE_MEM_WRITE("getdirentriesattr(newState)", ARG7, sizeof(unsigned int));
}
POST(getdirentriesattr) 
{
   char *p, *end;
   unsigned int count;
   unsigned int i;

   POST_MEM_WRITE(ARG5, sizeof(unsigned int));
   POST_MEM_WRITE(ARG6, sizeof(unsigned int));
   POST_MEM_WRITE(ARG7, sizeof(unsigned int));

   
   count = *(unsigned int *)ARG5;
   p = (char *)ARG3;
   end = (char *)ARG3 + ARG4;
   for (i = 0; i < count; i++) {
      vg_assert(p < end);  
      p += *(unsigned int *)p;
   }

   POST_MEM_WRITE(ARG3, p - (char *)ARG3);

   PRINT("got %d records, %ld/%lu bytes\n",
         count, (Addr)p-(Addr)ARG3, ARG4);
}


PRE(fsgetpath)
{
#if VG_WORDSIZE == 4
   PRINT("fsgetpath(%#lx, %ld, %#lx {%u,%u}, %llu)", 
         ARG1, ARG2, ARG3,
         ((unsigned int *)ARG3)[0], ((unsigned int *)ARG3)[1],
         LOHI64(ARG4, ARG5));
   PRE_REG_READ5(ssize_t, "fsgetpath", 
                 void*,"buf", size_t,"bufsize", 
                 fsid_t *,"fsid",
                 vki_uint32_t, "objid_low32", vki_uint32_t, "objid_high32");
#else
   PRINT("fsgetpath(%#lx, %ld, %#lx {%u,%u}, %lu)", 
         ARG1, ARG2, ARG3,
         ((unsigned int *)ARG3)[0],
         ((unsigned int *)ARG3)[1], ARG4);
   PRE_REG_READ4(ssize_t, "fsgetpath", 
                 void*,"buf", size_t,"bufsize", 
                 fsid_t *,"fsid", uint64_t,"objid");
#endif
   PRE_MEM_READ("fsgetpath(fsid)", ARG3, sizeof(fsid_t));
   PRE_MEM_WRITE("fsgetpath(buf)", ARG1, ARG2);
}

POST(fsgetpath)
{
   POST_MEM_WRITE(ARG1, RES);
}

PRE(audit_session_self)
{
  PRINT("audit_session_self()");
}

POST(audit_session_self)
{
  record_named_port(tid, RES, MACH_PORT_RIGHT_SEND, "audit-session-%p");
  PRINT("audit-session %#lx", RES);
}

PRE(exchangedata)
{
   PRINT("exchangedata(%#lx(%s), %#lx(%s), %lu)",
         ARG1, (char*)ARG1, ARG2, (char*)ARG2, ARG3);
   PRE_REG_READ3(int, "exchangedata", 
                 char *, path1, char *, path2, unsigned long, options);
   PRE_MEM_RASCIIZ( "exchangedata(path1)", ARG1 );
   PRE_MEM_RASCIIZ( "exchangedata(path2)", ARG2 );
}

PRE(fsctl)
{
   PRINT("fsctl ( %#lx(%s), %ld, %#lx, %ld )",
      ARG1, (char *)ARG1, ARG2, ARG3, ARG4);
   PRE_REG_READ4( long, "fsctl", 
                  char *,"path", unsigned int,"request", 
                  void *,"data", unsigned int,"options");
   
   PRE_MEM_RASCIIZ( "fsctl(path)", ARG1 );

   switch (ARG2) {
   case VKI_afpfsByteRangeLock2FSCTL: {
      struct vki_ByteRangeLockPB2 *pb = (struct vki_ByteRangeLockPB2 *)ARG3;
      PRE_FIELD_READ("fsctl(afpfsByteRangeLock2, pb->offset)", 
                     pb->offset);
      PRE_FIELD_READ("fsctl(afpfsByteRangeLock2, pb->length)", 
                     pb->length);
      PRE_FIELD_READ("fsctl(afpfsByteRangeLock2, pb->unLockFlag)", 
                     pb->unLockFlag);
      PRE_FIELD_READ("fsctl(afpfsByteRangeLock2, pb->startEndFlag)", 
                     pb->startEndFlag);
      PRE_FIELD_READ("fsctl(afpfsByteRangeLock2, pb->fd)", 
                     pb->fd);

      PRE_FIELD_WRITE("fsctl(afpfsByteRangeLock2, pb->retRangeStart)", 
                      pb->retRangeStart);

      
      break;
   }
   case VKI_FSIOC_SYNC_VOLUME:
       PRE_MEM_READ( "fsctl(FSIOC_SYNC_VOLUME)", ARG3, sizeof(int) );
       break;

   default:
      
      ML_(PRE_unknown_ioctl)(tid, ARG2, ARG3);
      break;
   }
}

POST(fsctl)
{
   switch (ARG2) {
   case VKI_afpfsByteRangeLock2FSCTL: {
      struct vki_ByteRangeLockPB2 *pb = (struct vki_ByteRangeLockPB2 *)ARG3;
      POST_FIELD_WRITE(pb->retRangeStart);
      break;
   }
   case VKI_FSIOC_SYNC_VOLUME:
       break;

   default:
      
      ML_(POST_unknown_ioctl)(tid, RES, ARG2, ARG3);
      break;
   }
}

PRE(initgroups)
{
    PRINT("initgroups(%s, %#lx, %lu)", (char *)ARG1, ARG2, ARG3);
    PRE_REG_READ3(long, "initgroups",
        int, setlen, vki_gid_t *, gidset, vki_uid_t, gmuid);
    PRE_MEM_READ("gidset", ARG2, ARG1 * sizeof(vki_gid_t));
}


static void pre_argv_envp(Addr a, ThreadId tid, const HChar* s1, const HChar* s2)
{
   while (True) {
      Addr a_deref;
      Addr* a_p = (Addr*)a;
      PRE_MEM_READ( s1, (Addr)a_p, sizeof(Addr) );
      a_deref = *a_p;
      if (0 == a_deref)
         break;
      PRE_MEM_RASCIIZ( s2, a_deref );
      a += sizeof(char*);
   }
}
static SysRes simple_pre_exec_check ( const HChar* exe_name,
                                      Bool trace_this_child )
{
   Int fd, ret;
   SysRes res;
   Bool setuid_allowed;

   
   res = VG_(open)(exe_name, VKI_O_RDONLY, 0);
   if (sr_isError(res)) {
      return res;
   }
   fd = sr_Res(res);
   VG_(close)(fd);

   
   
   
   setuid_allowed = trace_this_child  ? False  : True;
   ret = VG_(check_executable)(NULL,
                               exe_name, setuid_allowed);
   if (0 != ret) {
      return VG_(mk_SysRes_Error)(ret);
   }
   return VG_(mk_SysRes_Success)(0);
}
PRE(posix_spawn)
{
   HChar*       path = NULL;       
   HChar**      envp = NULL;
   HChar**      argv = NULL;
   HChar**      arg2copy;
   HChar*       launcher_basename = NULL;
   Int          i, j, tot_args;
   SysRes       res;
   Bool         trace_this_child;

   PRINT("posix_spawn( %#lx, %#lx(%s), %#lx, %#lx, %#lx )",
         ARG1, ARG2, ARG2 ? (HChar*)ARG2 : "(null)", ARG3, ARG4, ARG5 );

   

   PRE_REG_READ5(int, "posix_spawn", vki_pid_t*, pid, char*, path,
                 void*, file_actions, char**, argv, char**, envp );
   PRE_MEM_WRITE("posix_spawn(pid)", ARG1, sizeof(vki_pid_t) );
   PRE_MEM_RASCIIZ("posix_spawn(path)", ARG2);
   
   if (ARG4 != 0)
      pre_argv_envp( ARG4, tid, "posix_spawn(argv)",
                                "posix_spawn(argv[i])" );
   if (ARG5 != 0)
      pre_argv_envp( ARG5, tid, "posix_spawn(envp)",
                                "posix_spawn(envp[i])" );

   if (0)
   VG_(printf)("posix_spawn( %#lx, %#lx(%s), %#lx, %#lx, %#lx )\n",
         ARG1, ARG2, ARG2 ? (HChar*)ARG2 : "(null)", ARG3, ARG4, ARG5 );


   
   if (ARG2 == 0 
       || !VG_(am_is_valid_for_client)( ARG2, 1, VKI_PROT_READ )) {
      SET_STATUS_Failure( VKI_EFAULT );
      return;
   }

   
   { 
     
     const HChar** child_argv = (const HChar**)ARG4;
     if (child_argv && child_argv[0] == NULL)
        child_argv = NULL;
     trace_this_child = VG_(should_we_trace_this_child)( (HChar*)ARG2, child_argv );
   }

   
   
   
   res = simple_pre_exec_check( (const HChar*)ARG2, trace_this_child );
   if (sr_isError(res)) {
      SET_STATUS_Failure( sr_Err(res) );
      return;
   }

   if (trace_this_child
       && (VG_(name_of_launcher) == NULL
           || VG_(name_of_launcher)[0] != '/')) {
      SET_STATUS_Failure( VKI_ECHILD ); 
      return;
   }

   
   VG_(debugLog)(1, "syswrap", "Posix_spawn of %s\n", (HChar*)ARG2);


   
   
   if (trace_this_child) {

      
      path = VG_(name_of_launcher);
      
      
      
      vg_assert(path);
      vg_assert(path[0] == '/');
      launcher_basename = path;

   } else {
      path = (HChar*)ARG2;
   }

   
   
   
   
   
   
   
   
   
   
   
   if (ARG5 == 0) {
      envp = NULL;
   } else {
      envp = VG_(env_clone)( (HChar**)ARG5 );
      vg_assert(envp);
      VG_(env_remove_valgrind_env_stuff)( envp );
   }

   if (trace_this_child) {
      
      VG_(env_setenv)( &envp, VALGRIND_LIB, VG_(libdir));
   }

   
   
   
   
   
   
   
   
   if (!trace_this_child) {
      argv = (HChar**)ARG4;
   } else {
      vg_assert( VG_(args_for_valgrind) );
      vg_assert( VG_(args_for_valgrind_noexecpass) >= 0 );
      vg_assert( VG_(args_for_valgrind_noexecpass)
                   <= VG_(sizeXA)( VG_(args_for_valgrind) ) );
      
      
      tot_args = 1;
      
      tot_args += VG_(sizeXA)( VG_(args_for_valgrind) );
      tot_args -= VG_(args_for_valgrind_noexecpass);
      
      tot_args++;
      
      arg2copy = (HChar**)ARG4;
      if (arg2copy && arg2copy[0]) {
         for (i = 1; arg2copy[i]; i++)
            tot_args++;
      }
      
      argv = VG_(malloc)( "di.syswrap.pre_sys_execve.1",
                          (tot_args+1) * sizeof(HChar*) );
      
      j = 0;
      argv[j++] = launcher_basename;
      for (i = 0; i < VG_(sizeXA)( VG_(args_for_valgrind) ); i++) {
         if (i < VG_(args_for_valgrind_noexecpass))
            continue;
         argv[j++] = * (HChar**) VG_(indexXA)( VG_(args_for_valgrind), i );
      }
      argv[j++] = (HChar*)ARG2;
      if (arg2copy && arg2copy[0])
         for (i = 1; arg2copy[i]; i++)
            argv[j++] = arg2copy[i];
      argv[j++] = NULL;
      
      vg_assert(j == tot_args+1);
   }


   if (0) {
      HChar **cpp;
      VG_(printf)("posix_spawn: %s\n", path);
      for (cpp = argv; cpp && *cpp; cpp++)
         VG_(printf)("argv: %s\n", *cpp);
      if (1)
         for (cpp = envp; cpp && *cpp; cpp++)
            VG_(printf)("env: %s\n", *cpp);
   }

   ARG2 = (UWord)path;
   ARG4 = (UWord)argv;
   ARG5 = (UWord)envp;

   
   *flags |= SfMayBlock;
}
POST(posix_spawn)
{
   vg_assert(SUCCESS);
   if (ARG1 != 0) {
      POST_MEM_WRITE( ARG1, sizeof(vki_pid_t) );
   }
}


PRE(socket)
{
   PRINT("socket ( %ld, %ld, %ld )",ARG1,ARG2,ARG3);
   PRE_REG_READ3(long, "socket", int, domain, int, type, int, protocol);
}

POST(socket)
{
   SysRes r;
   vg_assert(SUCCESS);
   r = ML_(generic_POST_sys_socket)(tid, VG_(mk_SysRes_Success)(RES));
   SET_STATUS_from_SysRes(r);
}


PRE(setsockopt)
{
   PRINT("setsockopt ( %ld, %ld, %ld, %#lx, %ld )",
      ARG1,ARG2,ARG3,ARG4,ARG5);
   PRE_REG_READ5(long, "setsockopt",
                 int, s, int, level, int, optname,
                 const void *, optval, vki_socklen_t, optlen);
   ML_(generic_PRE_sys_setsockopt)(tid, ARG1,ARG2,ARG3,ARG4,ARG5);
}


PRE(getsockopt)
{
   Addr optval_p = ARG4;
   Addr optlen_p = ARG5;
   PRINT("getsockopt ( %ld, %ld, %ld, %#lx, %#lx )",
      ARG1,ARG2,ARG3,ARG4,ARG5);
   PRE_REG_READ5(long, "getsockopt",
                 int, s, int, level, int, optname,
                 void *, optval, vki_socklen_t *, optlen);
   
   if (optval_p != (Addr)NULL) {
      ML_(buf_and_len_pre_check) ( tid, optval_p, optlen_p,
                                   "socketcall.getsockopt(optval)",
                                   "socketcall.getsockopt(optlen)" );
   }
   
}

POST(getsockopt)
{
   Addr optval_p = ARG4;
   Addr optlen_p = ARG5;
   vg_assert(SUCCESS);
   if (optval_p != (Addr)NULL) {
      ML_(buf_and_len_post_check) ( tid, VG_(mk_SysRes_Success)(RES),
                                    optval_p, optlen_p,
                                    "socketcall.getsockopt(optlen_out)" );
   
   }
}


PRE(connect)
{
   *flags |= SfMayBlock;
   PRINT("connect ( %ld, %#lx, %ld )",ARG1,ARG2,ARG3);
   PRE_REG_READ3(long, "connect",
                 int, sockfd, struct sockaddr *, serv_addr, int, addrlen);
   ML_(generic_PRE_sys_connect)(tid, ARG1,ARG2,ARG3);
}


PRE(accept)
{
   *flags |= SfMayBlock;
   PRINT("accept ( %ld, %#lx, %ld )",ARG1,ARG2,ARG3);
   PRE_REG_READ3(long, "accept",
                 int, s, struct sockaddr *, addr, int, *addrlen);
   ML_(generic_PRE_sys_accept)(tid, ARG1,ARG2,ARG3);
}

POST(accept)
{
   SysRes r;
   vg_assert(SUCCESS);
   r = ML_(generic_POST_sys_accept)(tid, VG_(mk_SysRes_Success)(RES),
                                         ARG1,ARG2,ARG3);
   SET_STATUS_from_SysRes(r);
}

PRE(mkfifo)
{
   *flags |= SfMayBlock;
   PRINT("mkfifo ( %#lx(%s), %lld )",ARG1,(char *)ARG1,(ULong)ARG2);
   PRE_REG_READ2(long, "mkfifo", const char *, path, vki_mode_t, mode);
   PRE_MEM_RASCIIZ( "mkfifo(path)", ARG1 );
}

POST(mkfifo)
{
   vg_assert(SUCCESS);
   if (!ML_(fd_allowed)(RES, "mkfifo", tid, True)) {
      VG_(close)(RES);
      SET_STATUS_Failure( VKI_EMFILE );
   } else {
      if (VG_(clo_track_fds))
         ML_(record_fd_open_with_given_name)(tid, RES, (HChar*)ARG1);
   }
}

PRE(sendto)
{
   *flags |= SfMayBlock;
   PRINT("sendto ( %ld, %s, %ld, %lu, %#lx, %ld )",
      ARG1,(char *)ARG2,ARG3,ARG4,ARG5,ARG6);
   PRE_REG_READ6(long, "sendto",
                 int, s, const void *, msg, int, len, 
                 unsigned int, flags, 
                 const struct sockaddr *, to, int, tolen);
   ML_(generic_PRE_sys_sendto)(tid, ARG1,ARG2,ARG3,ARG4,ARG5,ARG6);
}

PRE(sendfile)
{
#if VG_WORDSIZE == 4
   PRINT("sendfile(%ld, %ld, %llu, %#lx, %#lx, %ld)",
         ARG1, ARG2, LOHI64(ARG3, ARG4), ARG5, ARG6, ARG7);

   PRE_REG_READ7(long, "sendfile",
      int, fromfd, int, tofd,
      vki_uint32_t, offset_low32, vki_uint32_t, offset_high32,
      vki_uint64_t *, nwritten, struct sf_hdtr *, sf_header, int, flags);
   PRE_MEM_WRITE("sendfile(nwritten)", ARG5, sizeof(vki_uint64_t));
   if (ARG6) PRE_MEM_WRITE("sendfile(sf_header)", ARG6, sizeof(struct sf_hdtr));
#else
   PRINT("sendfile(%ld, %ld, %ld, %#lx, %#lx, %ld)",
      ARG1, ARG2, ARG3, ARG4, ARG5, ARG6);

   PRE_REG_READ6(long, "sendfile",
      int, fromfd, int, tofd,
      vki_uint64_t, offset, 
      vki_uint64_t *, nwritten, struct sf_hdtr *, sf_header, int, flags);
   PRE_MEM_WRITE("sendfile(nwritten)", ARG4, sizeof(vki_uint64_t));
   if (ARG5) PRE_MEM_WRITE("sendfile(sf_header)", ARG5, sizeof(struct sf_hdtr));
#endif

   *flags |= SfMayBlock;
}
POST(sendfile)
{
#if VG_WORDSIZE == 4
   POST_MEM_WRITE(ARG5, sizeof(vki_uint64_t));
   if (ARG6) POST_MEM_WRITE(ARG6, sizeof(struct sf_hdtr));
#else
   POST_MEM_WRITE(ARG4, sizeof(vki_uint64_t));
   if (ARG5) POST_MEM_WRITE(ARG5, sizeof(struct sf_hdtr));
#endif
}

PRE(recvfrom)
{
   *flags |= SfMayBlock;
   PRINT("recvfrom ( %ld, %#lx, %ld, %lu, %#lx, %#lx )",
      ARG1,ARG2,ARG3,ARG4,ARG5,ARG6);
   PRE_REG_READ6(long, "recvfrom",
                 int, s, void *, buf, int, len, unsigned int, flags,
                 struct sockaddr *, from, int *, fromlen);
   ML_(generic_PRE_sys_recvfrom)(tid, ARG1,ARG2,ARG3,ARG4,ARG5,ARG6);
}

POST(recvfrom)
{
   vg_assert(SUCCESS);
   ML_(generic_POST_sys_recvfrom)(tid, VG_(mk_SysRes_Success)(RES),
                                       ARG1,ARG2,ARG3,ARG4,ARG5,ARG6);
}


PRE(sendmsg)
{
   *flags |= SfMayBlock;
   PRINT("sendmsg ( %ld, %#lx, %ld )",ARG1,ARG2,ARG3);
   PRE_REG_READ3(long, "sendmsg",
                 int, s, const struct msghdr *, msg, int, flags);
   ML_(generic_PRE_sys_sendmsg)(tid, "msg", (struct vki_msghdr *)ARG2);
}


PRE(recvmsg)
{
   *flags |= SfMayBlock;
   PRINT("recvmsg ( %ld, %#lx, %ld )",ARG1,ARG2,ARG3);
   PRE_REG_READ3(long, "recvmsg", int, s, struct msghdr *, msg, int, flags);
   ML_(generic_PRE_sys_recvmsg)(tid, "msg", (struct vki_msghdr *)ARG2);
}

POST(recvmsg)
{
   ML_(generic_POST_sys_recvmsg)(tid, "msg", (struct vki_msghdr *)ARG2, RES);
}


PRE(shutdown)
{
   *flags |= SfMayBlock;
   PRINT("shutdown ( %ld, %ld )",ARG1,ARG2);
   PRE_REG_READ2(int, "shutdown", int, s, int, how);
}


PRE(bind)
{
   PRINT("bind ( %ld, %#lx, %ld )",ARG1,ARG2,ARG3);
   PRE_REG_READ3(long, "bind",
                 int, sockfd, struct sockaddr *, my_addr, int, addrlen);
   ML_(generic_PRE_sys_bind)(tid, ARG1,ARG2,ARG3);
}


PRE(listen)
{
   PRINT("listen ( %ld, %ld )",ARG1,ARG2);
   PRE_REG_READ2(long, "listen", int, s, int, backlog);
}


PRE(getsockname)
{
   PRINT("getsockname ( %ld, %#lx, %#lx )",ARG1,ARG2,ARG3);
   PRE_REG_READ3(long, "getsockname",
                 int, s, struct sockaddr *, name, int *, namelen);
   ML_(generic_PRE_sys_getsockname)(tid, ARG1,ARG2,ARG3);
}

POST(getsockname)
{
   vg_assert(SUCCESS);
   ML_(generic_POST_sys_getsockname)(tid, VG_(mk_SysRes_Success)(RES),
                                          ARG1,ARG2,ARG3);
}


PRE(getpeername)
{
   PRINT("getpeername ( %ld, %#lx, %#lx )",ARG1,ARG2,ARG3);
   PRE_REG_READ3(long, "getpeername",
                 int, s, struct sockaddr *, name, int *, namelen);
   ML_(generic_PRE_sys_getpeername)(tid, ARG1,ARG2,ARG3);
}

POST(getpeername)
{
   vg_assert(SUCCESS);
   ML_(generic_POST_sys_getpeername)(tid, VG_(mk_SysRes_Success)(RES),
                                          ARG1,ARG2,ARG3);
}


PRE(socketpair)
{
   PRINT("socketpair ( %ld, %ld, %ld, %#lx )",ARG1,ARG2,ARG3,ARG4);
   PRE_REG_READ4(long, "socketpair",
                 int, d, int, type, int, protocol, int *, sv);
   ML_(generic_PRE_sys_socketpair)(tid, ARG1,ARG2,ARG3,ARG4);
}

POST(socketpair)
{
   vg_assert(SUCCESS);
   ML_(generic_POST_sys_socketpair)(tid, VG_(mk_SysRes_Success)(RES),
                                         ARG1,ARG2,ARG3,ARG4);
}


PRE(gethostuuid)
{
   PRINT("gethostuuid ( %#lx, %#lx )", ARG1, ARG2);
   PRE_REG_READ2(int,"gethostuuid", 
                 char *,"uuid_buf", 
                 const struct vki_timespec *,"timeout");

   PRE_MEM_WRITE("uuid_buf", ARG1, 16);
   PRE_MEM_READ("timeout", ARG2, sizeof(struct vki_timespec));

   *flags |= SfMayBlock;
}


POST(gethostuuid)
{
   POST_MEM_WRITE(ARG1, 16);
}

PRE(pipe)
{
   PRINT("pipe ( )");
   PRE_REG_READ0(int, "pipe");
}

POST(pipe)
{
   Int p0, p1;
   vg_assert(SUCCESS);
   p0 = RES;
   p1 = RESHI;

   if (!ML_(fd_allowed)(p0, "pipe", tid, True) ||
       !ML_(fd_allowed)(p1, "pipe", tid, True)) {
      VG_(close)(p0);
      VG_(close)(p1);
      SET_STATUS_Failure( VKI_EMFILE );
   } else {
      if (VG_(clo_track_fds)) {
         ML_(record_fd_open_nameless)(tid, p0);
         ML_(record_fd_open_nameless)(tid, p1);
      }
   }
}


PRE(getlogin)
{
   PRINT("getlogin ( %#lx, %ld )", ARG1, ARG2);
   PRE_REG_READ2(long, "getlogin", 
                 char *,"namebuf", unsigned int,"namelen");

   PRE_MEM_WRITE("getlogin(namebuf)", ARG1, ARG2);
}

POST(getlogin)
{
   POST_MEM_WRITE(ARG1, ARG2);
}


PRE(ptrace)
{
   PRINT("ptrace ( %ld, %ld, %#lx, %ld )", ARG1, ARG2, ARG3, ARG4);
   PRE_REG_READ4(long, "ptrace", 
                 int,"request", vki_pid_t,"pid", 
                 vki_caddr_t,"addr", int,"data");
    
   

   
}


PRE(issetugid)
{
   PRINT("issetugid ( )");
   PRE_REG_READ0(long, "issetugid");
}


PRE(getdtablesize)
{
   PRINT("getdtablesize ( )");
   PRE_REG_READ0(long, "getdtablesize");
}

POST(getdtablesize)
{
   
   if (RES > VG_(fd_hard_limit)) SET_STATUS_Success(VG_(fd_hard_limit));
}

PRE(lseek)
{
   PRINT("lseek ( %ld, %ld, %ld )", ARG1,ARG2,ARG3);
   PRE_REG_READ4(vki_off_t, "lseek",
                 unsigned int,fd, int,offset_hi, int,offset_lo, 
                 unsigned int,whence);
}


PRE(pathconf)
{
   PRINT("pathconf(%#lx(%s), %ld)", ARG1,(char *)ARG1,ARG2);
   PRE_REG_READ2(long,"pathconf", const char *,"path", int,"name");
   PRE_MEM_RASCIIZ("pathconf(path)", ARG1);
}


PRE(fpathconf)
{
   PRINT("fpathconf(%ld, %ld)", ARG1,ARG2);
   PRE_REG_READ2(long,"fpathconf", int,"fd", int,"name");

   if (!ML_(fd_allowed)(ARG1, "fpathconf", tid, False))
      SET_STATUS_Failure( VKI_EBADF );
}


PRE(getdirentries)
{
   PRINT("getdirentries(%ld, %#lx, %ld, %#lx)", ARG1, ARG2, ARG3, ARG4);
   PRE_REG_READ4(int, "getdirentries", 
                 int, fd, char *, buf, int, nbytes, long *, basep);
   PRE_MEM_WRITE("getdirentries(basep)", ARG4, sizeof(long));
   PRE_MEM_WRITE("getdirentries(buf)", ARG2, ARG3);
}

POST(getdirentries) 
{
   POST_MEM_WRITE(ARG4, sizeof(long));
   
   POST_MEM_WRITE(ARG2, RES);
}


PRE(getdirentries64)
{
   PRINT("getdirentries64(%ld, %#lx, %lu, %#lx)", ARG1, ARG2, ARG3, ARG4);
   PRE_REG_READ4(vki_ssize_t, "getdirentries", 
                 int,fd, char *,buf, vki_size_t,nbytes, vki_off_t *,basep);
   PRE_MEM_WRITE("getdirentries64(buf)", ARG2, ARG3);
}
POST(getdirentries64) 
{
   
   POST_MEM_WRITE(ARG2, RES);
}


PRE(statfs64)
{
   PRINT("statfs64 ( %#lx(%s), %#lx )",ARG1,(char *)ARG1,ARG2);
   PRE_REG_READ2(long, "statfs64", const char *, path, struct statfs64 *, buf);
   PRE_MEM_RASCIIZ( "statfs64(path)", ARG1 );
   PRE_MEM_WRITE( "statfs64(buf)", ARG2, sizeof(struct vki_statfs64) );
}
POST(statfs64)
{
   POST_MEM_WRITE( ARG2, sizeof(struct vki_statfs64) );
}


PRE(fstatfs64)
{
   PRINT("fstatfs64 ( %ld, %#lx )",ARG1,ARG2);
   PRE_REG_READ2(long, "fstatfs64",
                 unsigned int, fd, struct statfs *, buf);
   PRE_MEM_WRITE( "fstatfs64(buf)", ARG2, sizeof(struct vki_statfs64) );
}
POST(fstatfs64)
{
   POST_MEM_WRITE( ARG2, sizeof(struct vki_statfs64) );
}

PRE(csops)
{
   PRINT("csops ( %ld, %#lx, %#lx, %lu )", ARG1, ARG2, ARG3, ARG4);
   PRE_REG_READ4(int, "csops",
                 vki_pid_t, pid, uint32_t, ops,
                 void *, useraddr, vki_size_t, usersize);

   PRE_MEM_WRITE( "csops(useraddr)", ARG3, ARG4 );

   
   
   if (!ARG1 || VG_(getpid)() == ARG1) {
      switch (ARG2) {
      case VKI_CS_OPS_MARKINVALID:
      case VKI_CS_OPS_MARKHARD:
      case VKI_CS_OPS_MARKKILL:
         SET_STATUS_Success(0);
      }
   }
}
POST(csops)
{
   POST_MEM_WRITE( ARG3, ARG4 );
}

PRE(auditon)
{
   PRINT("auditon ( %ld, %#lx, %ld )", ARG1, ARG2, ARG3);
   PRE_REG_READ3(int,"auditon", 
                 int,"cmd", void*,"data", unsigned int,"length");

   switch (ARG1) {

   case VKI_A_SETPOLICY: 
   case VKI_A_SETKMASK:
   case VKI_A_SETQCTRL:
   case VKI_A_SETCOND:
   case VKI_A_SETCLASS:
   case VKI_A_SETPMASK:
   case VKI_A_SETFSIZE:
#if DARWIN_VERS >= DARWIN_10_6
   case VKI_A_SENDTRIGGER:
#endif
      
      PRE_MEM_READ("auditon(data)", ARG2, ARG3);
      break;

   case VKI_A_GETKMASK:
   case VKI_A_GETPOLICY:
   case VKI_A_GETQCTRL:
   case VKI_A_GETFSIZE:
   case VKI_A_GETCOND:
      
      // GrP fixme be precise about what gets written
      PRE_MEM_WRITE("auditon(data)", ARG2, ARG3);
      break;


   case VKI_A_GETCLASS:
   case VKI_A_GETPINFO:
   case VKI_A_GETPINFO_ADDR:
#if DARWIN_VERS >= DARWIN_10_6
   case VKI_A_GETSINFO_ADDR:
#endif
      
      // GrP fixme be precise about what gets read and written
      PRE_MEM_READ("auditon(data)", ARG2, ARG3);
      PRE_MEM_WRITE("auditon(data)", ARG2, ARG3);
      break;

   case VKI_A_SETKAUDIT:
   case VKI_A_SETSTAT:
   case VKI_A_SETUMASK:
   case VKI_A_SETSMASK:
   case VKI_A_GETKAUDIT:
   case VKI_A_GETCWD:
   case VKI_A_GETCAR:
   case VKI_A_GETSTAT:
      
      break;

   default:
      VG_(message)(Vg_UserMsg, "UNKNOWN auditon cmd %ld\n", ARG1);
      break;
   }
}
POST(auditon)
{
   switch (ARG1) {

   case VKI_A_SETPOLICY: 
   case VKI_A_SETKMASK:
   case VKI_A_SETQCTRL:
   case VKI_A_SETCOND:
   case VKI_A_SETCLASS:
   case VKI_A_SETPMASK:
   case VKI_A_SETFSIZE:
#if DARWIN_VERS >= DARWIN_10_6
   case VKI_A_SENDTRIGGER:
#endif
      
      break;

   case VKI_A_GETKMASK:
   case VKI_A_GETPOLICY:
   case VKI_A_GETQCTRL:
   case VKI_A_GETFSIZE:
   case VKI_A_GETCOND:
      
      // GrP fixme be precise about what gets written
      POST_MEM_WRITE(ARG2, ARG3);
      break;


   case VKI_A_GETCLASS:
   case VKI_A_GETPINFO:
   case VKI_A_GETPINFO_ADDR:
#if DARWIN_VERS >= DARWIN_10_6
   case VKI_A_GETSINFO_ADDR:
#endif
      
      // GrP fixme be precise about what gets read and written
      POST_MEM_WRITE(ARG2, ARG3);
      break;

   case VKI_A_SETKAUDIT:
   case VKI_A_SETSTAT:
   case VKI_A_SETUMASK:
   case VKI_A_SETSMASK:
   case VKI_A_GETKAUDIT:
   case VKI_A_GETCWD:
   case VKI_A_GETCAR:
   case VKI_A_GETSTAT:
      
      break;

   default:
      break;
   }    
}


PRE(mmap)
{
   
   if (0) VG_(am_do_sync_check)("(PRE_MMAP)",__FILE__,__LINE__);

#if VG_WORDSIZE == 4
   PRINT("mmap ( %#lx, %lu, %ld, %ld, %ld, %lld )",
         ARG1, ARG2, ARG3, ARG4, ARG5, LOHI64(ARG6, ARG7) );
   PRE_REG_READ7(Addr, "mmap",
                 Addr,start, vki_size_t,length, int,prot, int,flags, int,fd, 
                 unsigned long,offset_hi, unsigned long,offset_lo);
   
   
   
   
#else
   PRINT("mmap ( %#lx, %lu, %ld, %ld, %ld, %ld )",
         ARG1, ARG2, ARG3, ARG4, ARG5, ARG6 );
   PRE_REG_READ6(long, "mmap",
                 Addr,start, vki_size_t,length, int,prot, int,flags, int,fd, 
                 Off64T,offset);
   

#endif

   
}

POST(mmap)
{
   if (RES != -1) {
      ML_(notify_core_and_tool_of_mmap)(RES, ARG2, ARG3, ARG4, ARG5, ARG6);
      
      VG_(di_notify_mmap)( (Addr)RES, False,
                           -1 );
      ML_(sync_mappings)("after", "mmap", 0);
   }
}


static void common_PRE_sysctl (
               
               ThreadId tid, SyscallStatus* status, UWord* flags,
               
               Bool is_kern_dot_userstack,
               UWord oldp, UWord oldlenp,
               UWord newp, UWord newlen )
{
   if (oldlenp) {
      
      PRE_MEM_WRITE("sysctl(oldlenp)", oldlenp, sizeof(size_t));
      if (oldp) {
         
         PRE_MEM_READ("sysctl(oldlenp)", oldlenp, sizeof(size_t));
         PRE_MEM_WRITE("sysctl(oldp)", oldp, *(size_t*)oldlenp);
      }
   }
   if (newp) {
      PRE_MEM_READ("sysctl(newp)", newp, newlen);
   }

   
   
        
   if (is_kern_dot_userstack) {
      
      
      
      if (newp || newlen) {
         SET_STATUS_Failure(VKI_EPERM); 
      } else {
         Addr* t_oldp = (Addr*)oldp;
         size_t* t_oldlenp = (size_t*)oldlenp;
         if (t_oldlenp) {
            
            
            
            
            Addr stack_end = VG_(clstk_end)+1;
            size_t oldlen = *t_oldlenp;
            
            *t_oldlenp = sizeof(Addr);
            if (t_oldp && oldlen >= sizeof(Addr)) {
               
               *t_oldp = stack_end;
               SET_STATUS_Success(0);
            } else {
               
               
               if (t_oldp) VG_(memcpy)(t_oldp, &stack_end, oldlen);
               SET_STATUS_Failure(VKI_ENOMEM);
            }
         }
      }
   }

   if (!SUCCESS && !FAILURE) {
      
      *flags |= SfPostOnFail;
   }
}


PRE(__sysctl)
{
   UWord name    = ARG1;
   UWord namelen = ARG2;
   UWord oldp    = ARG3;
   UWord oldlenp = ARG4;
   UWord newp    = ARG5;
   UWord newlen  = ARG6;

   PRINT( "__sysctl ( %#lx, %ld, %#lx, %#lx, %#lx, %ld )", 
          name, namelen, oldp, oldlenp, newp, newlen );

   PRE_REG_READ6(int, "__sysctl", int*, name, unsigned int, namelen, 
                 void*, oldp, vki_size_t *, oldlenp, 
                 void*, newp, vki_size_t *, newlenp);

   PRE_MEM_READ("sysctl(name)", name, namelen);  

   if (VG_(clo_trace_syscalls)) {
      UInt i;
      Int* t_name = (Int*)name;
      VG_(printf)(" mib: [ ");
      for (i = 0; i < namelen; i++) {
         VG_(printf)("%d ", t_name[i]);
      }
      VG_(printf)("]");
   }

   Int vKI_KERN_USRSTACKXX
      = VG_WORDSIZE == 4 ? VKI_KERN_USRSTACK32 : VKI_KERN_USRSTACK64; 
   Bool is_kern_dot_userstack
      = name && namelen == 2
        && ((Int*)name)[0] == VKI_CTL_KERN
        && ((Int*)name)[1] == vKI_KERN_USRSTACKXX;

   common_PRE_sysctl( tid,status,flags,
                      is_kern_dot_userstack, oldp, oldlenp, newp, newlen );
}

POST(__sysctl)
{
   UWord oldp    = ARG3;
   UWord oldlenp = ARG4;

   if (SUCCESS || ERR == VKI_ENOMEM) {
      
      if (oldlenp) {
         POST_MEM_WRITE(oldlenp, sizeof(size_t));
      }
      if (oldp && oldlenp) {
         POST_MEM_WRITE(oldp, *(size_t*)oldlenp);
      }
   }
}


PRE(sigpending)
{
   PRINT( "sigpending ( %#lx )", ARG1 );
   PRE_REG_READ1(long, "sigpending", vki_sigset_t *, set);
   PRE_MEM_WRITE( "sigpending(set)", ARG1, sizeof(vki_sigset_t));
}
POST(sigpending)
{
   POST_MEM_WRITE( ARG1, sizeof(vki_sigset_t) ) ;
}


PRE(sigprocmask)
{
   UWord arg1;
   PRINT("sigprocmask ( %ld, %#lx, %#lx )", ARG1, ARG2, ARG3);
   PRE_REG_READ3(long, "sigprocmask",
                 int, how, vki_sigset_t *, set, vki_sigset_t *, oldset);
   if (ARG2 != 0)
      PRE_MEM_READ( "sigprocmask(set)", ARG2, sizeof(vki_sigset_t));
   if (ARG3 != 0)
      PRE_MEM_WRITE( "sigprocmask(oldset)", ARG3, sizeof(vki_sigset_t));

   arg1 = ARG1;
   if (ARG2 == 0  
       && ARG1 != VKI_SIG_BLOCK
       && ARG1 != VKI_SIG_UNBLOCK && ARG1 != VKI_SIG_SETMASK) {
      arg1 = VKI_SIG_SETMASK;
   }
   SET_STATUS_from_SysRes(
      VG_(do_sys_sigprocmask) ( tid, arg1, (vki_sigset_t*)ARG2,
                                           (vki_sigset_t*)ARG3 )
   );

   if (SUCCESS)
      *flags |= SfPollAfter;
}

POST(sigprocmask)
{
   vg_assert(SUCCESS);
   if (RES == 0 && ARG3 != 0)
      POST_MEM_WRITE( ARG3, sizeof(vki_sigset_t));
}


PRE(sigsuspend)
{
   *flags |= SfMayBlock;
   PRINT("sigsuspend ( mask=0x%08lx )", ARG1 );
   PRE_REG_READ1(int, "sigsuspend", int, sigmask);
}


#if DARWIN_VERS >= DARWIN_10_6
PRE(proc_info)
{
#if VG_WORDSIZE == 4
   PRINT("proc_info(%d, %d, %u, %llu, %#lx, %d)",
         (Int)ARG1, (Int)ARG2, (UInt)ARG3, LOHI64(ARG4,ARG5), ARG6, (Int)ARG7);
   PRE_REG_READ7(int, "proc_info",
                 int, callnum, int, pid, unsigned int, flavor,
                 vki_uint32_t, arg_low32,
                 vki_uint32_t, arg_high32,
                 void*, buffer, int, buffersize);
   PRE_MEM_WRITE("proc_info(buffer)", ARG6, ARG7);
#else
   PRINT("proc_info(%d, %d, %u, %llu, %#lx, %d)",
         (Int)ARG1, (Int)ARG2, (UInt)ARG3, (ULong)ARG4, ARG5, (Int)ARG6);
   PRE_REG_READ6(int, "proc_info",
                 int, callnum, int, pid, unsigned int, flavor,
                 unsigned long long int, arg,
                 void*, buffer, int, buffersize);
   PRE_MEM_WRITE("proc_info(buffer)", ARG5, ARG6);
#endif
}

POST(proc_info)
{
#if VG_WORDSIZE == 4
   vg_assert(SUCCESS);

   
   if (ARG1 == 5 && ARG3 == 2 && LOHI64(ARG4,ARG5) == 0 )
   {
       const HChar* new_name = (const HChar*) ARG6;
       if (new_name) {    
          ThreadState* tst = VG_(get_ThreadState)(tid);
          SizeT new_len = VG_(strlen)(new_name);
           
          
          tst->thread_name =
             VG_(realloc)("syscall(proc_info)", tst->thread_name, new_len + 1);
          VG_(strcpy)(tst->thread_name, new_name);
       }
   }
    
   POST_MEM_WRITE(ARG6, ARG7);
#else
   vg_assert(SUCCESS);

   
   if (ARG1 == 5 && ARG3 == 2 && ARG4 == 0 )
   {
      const HChar* new_name = (const HChar*) ARG5;
      if (new_name) {    
         ThreadState* tst = VG_(get_ThreadState)(tid);
         SizeT new_len = VG_(strlen)(new_name);
            
         
         tst->thread_name =
            VG_(realloc)("syscall(proc_info)", tst->thread_name, new_len + 1);
         VG_(strcpy)(tst->thread_name, new_name);
       }
   }

   POST_MEM_WRITE(ARG5, ARG6);
#endif
}

#endif 


// aio_return() is called we can mark the memory written asynchronously by
// aio_read() as having been written.  We don't have to do this for
static OSet* aiocbp_table = NULL;
static Bool aio_init_done = False;

static void aio_init(void)
{
   aiocbp_table = VG_(OSetWord_Create)(VG_(malloc), "syswrap.aio", VG_(free));
   aio_init_done = True;
}

static Bool was_a_successful_aio_read = False;

PRE(aio_return)
{
   struct vki_aiocb* aiocbp = (struct vki_aiocb*)ARG1;
   
   
   PRINT( "aio_return ( %#lx )", ARG1 );
   PRE_REG_READ1(long, "aio_return", struct vki_aiocb*, aiocbp);

   if (!aio_init_done) aio_init();
   was_a_successful_aio_read = VG_(OSetWord_Remove)(aiocbp_table, (UWord)aiocbp);
}
POST(aio_return)
{
   
   // so mark the buffer as written.  If we didn't find it, it must have been
   
   // aiocbp).  Either way, the buffer won't have been written so we don't
   // have to mark the buffer as written.
   if (was_a_successful_aio_read) {
      struct vki_aiocb* aiocbp = (struct vki_aiocb*)ARG1;
      POST_MEM_WRITE((Addr)aiocbp->aio_buf, aiocbp->aio_nbytes);
      was_a_successful_aio_read = False;
   }
}

PRE(aio_suspend)
{
   
   
   PRINT( "aio_suspend ( %#lx )", ARG1 );
   PRE_REG_READ3(long, "aio_suspend",
                 const struct vki_aiocb *, aiocbp, int, nent,
                 const struct vki_timespec *, timeout);
   if (ARG2 > 0)
      PRE_MEM_READ("aio_suspend(list)", ARG1, ARG2 * sizeof(struct vki_aiocb *));
   if (ARG3)
      PRE_MEM_READ ("aio_suspend(timeout)", ARG3, sizeof(struct vki_timespec));
}

PRE(aio_error)
{
   
   
   PRINT( "aio_error ( %#lx )", ARG1 );
   PRE_REG_READ1(long, "aio_error", struct vki_aiocb*, aiocbp);
}

PRE(aio_read)
{
   struct vki_aiocb* aiocbp = (struct vki_aiocb*)ARG1;

   PRINT( "aio_read ( %#lx )", ARG1 );
   PRE_REG_READ1(long, "aio_read", struct vki_aiocb*, aiocbp);
   PRE_MEM_READ( "aio_read(aiocbp)", ARG1, sizeof(struct vki_aiocb));

   if (ML_(safe_to_deref)(aiocbp, sizeof(struct vki_aiocb))) {
      if (ML_(fd_allowed)(aiocbp->aio_fildes, "aio_read", tid, False)) {
         PRE_MEM_WRITE("aio_read(aiocbp->aio_buf)",
                       (Addr)aiocbp->aio_buf, aiocbp->aio_nbytes);
      } else {
         SET_STATUS_Failure( VKI_EBADF );
      }
   } else {
      SET_STATUS_Failure( VKI_EINVAL );
   }
}
POST(aio_read)
{
   
   
   
   struct vki_aiocb* aiocbp = (struct vki_aiocb*)ARG1;
   if (!aio_init_done) aio_init();
   
   
   
   VG_(OSetWord_Insert)(aiocbp_table, (UWord)aiocbp);
}

PRE(aio_write)
{
   struct vki_aiocb* aiocbp = (struct vki_aiocb*)ARG1;

   PRINT( "aio_write ( %#lx )", ARG1 );
   PRE_REG_READ1(long, "aio_write", struct vki_aiocb*, aiocbp);
   PRE_MEM_READ( "aio_write(aiocbp)", ARG1, sizeof(struct vki_aiocb));

   if (ML_(safe_to_deref)(aiocbp, sizeof(struct vki_aiocb))) {
      if (ML_(fd_allowed)(aiocbp->aio_fildes, "aio_write", tid, False)) {
         PRE_MEM_READ("aio_write(aiocbp->aio_buf)",
                      (Addr)aiocbp->aio_buf, aiocbp->aio_nbytes);
      } else {
         SET_STATUS_Failure( VKI_EBADF );
      }
   } else {
      SET_STATUS_Failure( VKI_EINVAL );
   }
}


static size_t desc_size(mach_msg_descriptor_t *desc)
{
   switch (desc->type.type) {
   case MACH_MSG_PORT_DESCRIPTOR:          return sizeof(desc->port);
   case MACH_MSG_OOL_DESCRIPTOR:           return sizeof(desc->out_of_line);
   case MACH_MSG_OOL_VOLATILE_DESCRIPTOR:  return sizeof(desc->out_of_line);
   case MACH_MSG_OOL_PORTS_DESCRIPTOR:     return sizeof(desc->ool_ports);
   default: 
      VG_(printf)("UNKNOWN mach message descriptor %d\n", desc->type.type);
      return sizeof(desc->type); 
   }
}


static void assign_port_names(mach_msg_ool_ports_descriptor_t *desc, 
                              const char *name)
{
   mach_msg_size_t i;
   mach_port_t *ports = (mach_port_t *)desc->address;
   for (i = 0; i < desc->count; i++) {
      assign_port_name(ports[i], name);
   }
}


static void import_complex_message(ThreadId tid, mach_msg_header_t *mh)
{
   mach_msg_body_t *body;
   mach_msg_size_t count, i;
   uint8_t *p;
   mach_msg_descriptor_t *desc;
   
   vg_assert(mh->msgh_bits & MACH_MSGH_BITS_COMPLEX);
   
   body = (mach_msg_body_t *)(mh+1);
   count = body->msgh_descriptor_count;
   p = (uint8_t *)(body+1);
   
   for (i = 0; i < count; i++) {
      desc = (mach_msg_descriptor_t *)p;
      p += desc_size(desc);
      
      switch (desc->type.type) {
      case MACH_MSG_PORT_DESCRIPTOR:
         
         record_unnamed_port(tid, desc->port.name, -1);
         record_port_insert_rights(desc->port.name, desc->port.disposition);
         PRINT("got port %s;\n", name_for_port(desc->port.name));
         break;

      case MACH_MSG_OOL_DESCRIPTOR:
      case MACH_MSG_OOL_VOLATILE_DESCRIPTOR:
         
         
         
         
         
         if (desc->out_of_line.size > 0) {
            Addr start = VG_PGROUNDDN((Addr)desc->out_of_line.address);
            Addr end = VG_PGROUNDUP((Addr)desc->out_of_line.address + 
                                    (Addr)desc->out_of_line.size);
            PRINT("got ool mem %p..%p;\n", desc->out_of_line.address, 
                  (char*)desc->out_of_line.address+desc->out_of_line.size);

            ML_(notify_core_and_tool_of_mmap)(
               start, end - start, VKI_PROT_READ|VKI_PROT_WRITE, 
               VKI_MAP_PRIVATE, -1, 0);
         }
         
         break;

      case MACH_MSG_OOL_PORTS_DESCRIPTOR:
         
         
         PRINT("got %d ool ports %p..%#lx", desc->ool_ports.count, desc->ool_ports.address, (Addr)desc->ool_ports.address+desc->ool_ports.count*sizeof(mach_port_t));

         if (desc->ool_ports.count > 0) {
            Addr start = VG_PGROUNDDN((Addr)desc->ool_ports.address);
            Addr end = VG_PGROUNDUP((Addr)desc->ool_ports.address + desc->ool_ports.count * sizeof(mach_port_t));
            mach_port_t *ports = (mach_port_t *)desc->ool_ports.address;

            ML_(notify_core_and_tool_of_mmap)(
               start, end - start, VKI_PROT_READ|VKI_PROT_WRITE, 
               VKI_MAP_PRIVATE, -1, 0);

            PRINT(":");
            for (i = 0; i < desc->ool_ports.count; i++) {
               record_unnamed_port(tid, ports[i], -1);
               record_port_insert_rights(ports[i], desc->port.disposition);
               PRINT(" %s", name_for_port(ports[i]));
            }
         }
         PRINT(";\n");
         break;

      default:
         VG_(printf)("UNKNOWN Mach descriptor type %u!\n", desc->type.type);
         break;
      }
   }
}


static void pre_port_desc_read(ThreadId tid, mach_msg_port_descriptor_t *desc2)
{
#pragma pack(4)
   struct {
      mach_port_t name;
      mach_msg_size_t pad1;
      uint16_t pad2;
      uint8_t disposition;
      uint8_t type;
   } *desc = (void*)desc2;
#pragma pack()

   PRE_FIELD_READ("msg->desc.port.name",        desc->name);
   PRE_FIELD_READ("msg->desc.port.disposition", desc->disposition);
   PRE_FIELD_READ("msg->desc.port.type",        desc->type);
}


static void pre_ool_desc_read(ThreadId tid, mach_msg_ool_descriptor_t *desc2)
{
#pragma pack(4)
   struct {
      Addr address;
#if VG_WORDSIZE != 8
      mach_msg_size_t size;
#endif
      uint8_t deallocate;
      uint8_t copy;
      uint8_t pad1;
      uint8_t type;
#if VG_WORDSIZE == 8
      mach_msg_size_t size;
#endif
   } *desc = (void*)desc2;
#pragma pack()

   PRE_FIELD_READ("msg->desc.out_of_line.address",    desc->address);
   PRE_FIELD_READ("msg->desc.out_of_line.size",       desc->size);
   PRE_FIELD_READ("msg->desc.out_of_line.deallocate", desc->deallocate);
   PRE_FIELD_READ("msg->desc.out_of_line.copy",       desc->copy);
   PRE_FIELD_READ("msg->desc.out_of_line.type",       desc->type);
}

static void pre_oolports_desc_read(ThreadId tid, 
                                   mach_msg_ool_ports_descriptor_t *desc2)
{
#pragma pack(4)
   struct {
      Addr address;
#if VG_WORDSIZE != 8
      mach_msg_size_t size;
#endif
      uint8_t deallocate;
      uint8_t copy;
      uint8_t disposition;
      uint8_t type;
#if VG_WORDSIZE == 8
      mach_msg_size_t size;
#endif
   } *desc = (void*)desc2;
#pragma pack()

   PRE_FIELD_READ("msg->desc.ool_ports.address",     desc->address);
   PRE_FIELD_READ("msg->desc.ool_ports.size",        desc->size);
   PRE_FIELD_READ("msg->desc.ool_ports.deallocate",  desc->deallocate);
   PRE_FIELD_READ("msg->desc.ool_ports.copy",        desc->copy);
   PRE_FIELD_READ("msg->desc.ool_ports.disposition", desc->disposition);
   PRE_FIELD_READ("msg->desc.ool_ports.type",        desc->type);
}


static size_t export_complex_message(ThreadId tid, mach_msg_header_t *mh)
{
   mach_msg_body_t *body;
   mach_msg_size_t count, i;
   uint8_t *p;
   mach_msg_descriptor_t *desc;
   
   vg_assert(mh->msgh_bits & MACH_MSGH_BITS_COMPLEX);
   
   body = (mach_msg_body_t *)(mh+1);
   PRE_MEM_READ("msg->msgh_descriptor_count)", (Addr)body, sizeof(*body));

   count = body->msgh_descriptor_count;
   p = (uint8_t *)(body+1);
   
   for (i = 0; i < count; i++) {
      desc = (mach_msg_descriptor_t *)p;
      p += desc_size(desc);
      
      switch (desc->type.type) {
      case MACH_MSG_PORT_DESCRIPTOR:
         
         pre_port_desc_read(tid, &desc->port);
         break;

      case MACH_MSG_OOL_DESCRIPTOR:
      case MACH_MSG_OOL_VOLATILE_DESCRIPTOR:
         
         
         
         
         pre_ool_desc_read(tid, &desc->out_of_line);

         if (desc->out_of_line.deallocate  &&  desc->out_of_line.size > 0) {
            vm_size_t size = desc->out_of_line.size;
            Addr start = VG_PGROUNDDN((Addr)desc->out_of_line.address);
            Addr end = VG_PGROUNDUP((Addr)desc->out_of_line.address + size);
            PRINT("kill ool mem %p..%#lx; ", desc->out_of_line.address, 
                  (Addr)desc->out_of_line.address + size);
            ML_(notify_core_and_tool_of_munmap)(start, end - start);
         }
         break;

      case MACH_MSG_OOL_PORTS_DESCRIPTOR:
         
         
         
         pre_oolports_desc_read(tid, &desc->ool_ports);

         if (desc->ool_ports.deallocate  &&  desc->ool_ports.count > 0) {
            vm_size_t size = desc->ool_ports.count * sizeof(mach_port_t);
            Addr start = VG_PGROUNDDN((Addr)desc->ool_ports.address);
            Addr end = VG_PGROUNDUP((Addr)desc->ool_ports.address + size);
            PRINT("kill ool port array %p..%#lx; ", desc->ool_ports.address, 
                  (Addr)desc->ool_ports.address + size);
            ML_(notify_core_and_tool_of_munmap)(start, end - start);
         }
         break;
      default:
         VG_(printf)("UNKNOWN Mach descriptor type %u!\n", desc->type.type);
         break;
      }
   }

   return (size_t)((Addr)p - (Addr)body);
}




POST(host_info)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      NDR_record_t NDR;
      kern_return_t RetCode;
      mach_msg_type_number_t host_info_outCnt;
      integer_t host_info_out[14];
   } Reply;
#pragma pack()

   Reply *reply = (Reply *)ARG1;

   if (reply->RetCode) PRINT("mig return %d", reply->RetCode);
}

PRE(host_info)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      NDR_record_t NDR;
      host_flavor_t flavor;
      mach_msg_type_number_t host_info_outCnt;
   } Request;
#pragma pack()

   Request *req = (Request *)ARG1;

   PRINT("host_info(mach_host_self(), flavor %d)", req->flavor);

   AFTER = POST_FN(host_info);
}


POST(host_page_size)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      NDR_record_t NDR;
      kern_return_t RetCode;
      vm_size_t out_page_size;
   } Reply;
#pragma pack()

   Reply *reply = (Reply *)ARG1;

   if (!reply->RetCode) {
     PRINT("page size %llu", (ULong)reply->out_page_size);
   } else {
      PRINT("mig return %d", reply->RetCode);
   }
}

PRE(host_page_size)
{
   PRINT("host_page_size(mach_host_self(), ...)");
    
   AFTER = POST_FN(host_page_size);
}


POST(host_get_io_master)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      
      mach_msg_body_t msgh_body;
      mach_msg_port_descriptor_t io_master;
      
   } Reply;
#pragma pack()

   Reply *reply = (Reply *)ARG1;

   assign_port_name(reply->io_master.name, "io_master-%p");
   PRINT("%s", name_for_port(reply->io_master.name));
}

PRE(host_get_io_master)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
   } Request;
#pragma pack()

   

   PRINT("host_get_io_master(mach_host_self())");

   AFTER = POST_FN(host_get_io_master);
}


POST(host_get_clock_service)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      
      mach_msg_body_t msgh_body;
      mach_msg_port_descriptor_t clock_serv;
      
   } Reply;
#pragma pack()

   Reply *reply = (Reply *)ARG1;

   assign_port_name(reply->clock_serv.name, "clock-%p");
   PRINT("%s", name_for_port(reply->clock_serv.name));
}

PRE(host_get_clock_service)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      NDR_record_t NDR;
      clock_id_t clock_id;
   } Request;
#pragma pack()

   Request *req = (Request *)ARG1;

   PRINT("host_get_clock_service(mach_host_self(), %d)", req->clock_id);

   AFTER = POST_FN(host_get_clock_service);
}


PRE(host_request_notification)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      
      mach_msg_body_t msgh_body;
      mach_msg_port_descriptor_t notify_port;
      
      NDR_record_t NDR;
      host_flavor_t notify_type;
   } Request;
#pragma pack()

   Request *req = (Request *)ARG1;

   if (MACH_REMOTE == mach_task_self()) { 
      if (req->notify_type == 0) {
         PRINT("host_request_notification(mach_host_self(), %s, %s)", 
               "HOST_NOTIFY_CALENDAR_CHANGE", 
               name_for_port(req->notify_port.name));
      } else {
         PRINT("host_request_notification(mach_host_self(), %d, %s)",
               req->notify_type, 
               name_for_port(req->notify_port.name));
      } 
   } else {
      PRINT("host_request_notification(%s, %d, %s)",
            name_for_port(MACH_REMOTE), 
            req->notify_type, 
            name_for_port(req->notify_port.name));
   }

    
   assign_port_name(req->notify_port.name, "host_notify-%p");
}




PRE(mach_port_set_context)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      NDR_record_t NDR;
      mach_port_name_t name;
      mach_vm_address_t context;
   } Request;
#pragma pack()

   Request *req = (Request *)ARG1;

   PRINT("mach_port_set_context(%s, %s, 0x%llx)",
        name_for_port(MACH_REMOTE), 
        name_for_port(req->name), req->context);

   AFTER = POST_FN(mach_port_set_context);
}

POST(mach_port_set_context)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      NDR_record_t NDR;
      kern_return_t RetCode;
   } Reply;
#pragma pack()
}


PRE(task_get_exception_ports)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      NDR_record_t NDR;
      exception_mask_t exception_mask;
   } Request;
#pragma pack()

   PRINT("task_get_exception_ports(BOGUS)");
   AFTER = POST_FN(task_get_exception_ports);
}

POST(task_get_exception_ports)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      
      mach_msg_body_t msgh_body;
      mach_msg_port_descriptor_t old_handlers[32];
      
      NDR_record_t NDR;
      mach_msg_type_number_t masksCnt;
      exception_mask_t masks[32];
      exception_behavior_t old_behaviors[32];
      thread_state_flavor_t old_flavors[32];
   } Reply;
#pragma pack()
}



PRE(mach_port_type)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      NDR_record_t NDR;
      mach_port_name_t name;
   } Request;
#pragma pack()

   Request *req = (Request *)ARG1;

   PRINT("mach_port_type(%s, %s, ...)", 
         name_for_port(MACH_REMOTE), name_for_port(req->name));

   AFTER = POST_FN(mach_port_type);
}

POST(mach_port_type)
{
}


PRE(mach_port_extract_member)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      NDR_record_t NDR;
      mach_port_name_t name;
      mach_port_name_t pset;
   } Request;
#pragma pack()

   Request *req = (Request *)ARG1;

   PRINT("mach_port_extract_member(%s, 0x%x, 0x%x)", 
         name_for_port(MACH_REMOTE), 
         req->name, req->pset);

   AFTER = POST_FN(mach_port_extract_member);

   
}

POST(mach_port_extract_member)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      NDR_record_t NDR;
      kern_return_t RetCode;
   } Reply;
#pragma pack()

   Reply *reply = (Reply *)ARG1;

   if (reply->RetCode) PRINT("mig return %d", reply->RetCode);
}


PRE(mach_port_allocate)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      NDR_record_t NDR;
      mach_port_right_t right;
   } Request;
#pragma pack()

   Request *req = (Request *)ARG1;

   PRINT("mach_port_allocate(mach_task_self(), %d, ...)", req->right);

   MACH_ARG(mach_port_allocate.right) = req->right;

   AFTER = POST_FN(mach_port_allocate);
}

POST(mach_port_allocate)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      NDR_record_t NDR;
      kern_return_t RetCode;
      mach_port_name_t name;
   } Reply;
#pragma pack()

   Reply *reply = (Reply *)ARG1;

   if (!reply->RetCode) {
      if (MACH_REMOTE == vg_task_port) {
         
         
         record_unnamed_port(tid, reply->name, MACH_ARG(mach_port_allocate.right));
         PRINT("got port 0x%x", reply->name);
      } else {
         VG_(printf)("UNKNOWN inserted port 0x%x into remote task\n", reply->name);
      }
   } else {
      PRINT("mig return %d", reply->RetCode);
   }
}


PRE(mach_port_deallocate)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      NDR_record_t NDR;
      mach_port_name_t name;
   } Request;
#pragma pack()

   Request *req = (Request *)ARG1;

   PRINT("mach_port_deallocate(%s, %s)", 
         name_for_port(MACH_REMOTE), 
         name_for_port(req->name));

   MACH_ARG(mach_port.port) = req->name;

   AFTER = POST_FN(mach_port_deallocate);

   
   
   *flags &= ~SfMayBlock;
}

POST(mach_port_deallocate)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      NDR_record_t NDR;
      kern_return_t RetCode;
   } Reply;
#pragma pack()
   
   Reply *reply = (Reply *)ARG1;

   if (!reply->RetCode) {
      if (MACH_REMOTE == vg_task_port) {
         
         record_port_dealloc(MACH_ARG(mach_port.port));
      } else {
         VG_(printf)("UNKNOWN remote port dealloc\n");
      }
   } else {
      PRINT("mig return %d", reply->RetCode);
   }
}


PRE(mach_port_get_refs)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      NDR_record_t NDR;
      mach_port_name_t name;
      mach_port_right_t right;
   } Request;
#pragma pack()

   Request *req = (Request *)ARG1;

   PRINT("mach_port_get_refs(%s, %s, 0x%x)", 
         name_for_port(MACH_REMOTE), 
         name_for_port(req->name), req->right);

   MACH_ARG(mach_port_mod_refs.port) = req->name;
   MACH_ARG(mach_port_mod_refs.right) = req->right;
    
   AFTER = POST_FN(mach_port_get_refs);
}

POST(mach_port_get_refs)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      NDR_record_t NDR;
      kern_return_t RetCode;
      mach_port_urefs_t refs;
   } Reply;
#pragma pack()
   
   Reply *reply = (Reply *)ARG1;

   if (!reply->RetCode) {
      PRINT("got refs=%d", reply->refs);
   } else {
      PRINT("mig return %d", reply->RetCode);
   }
}


PRE(mach_port_mod_refs)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      NDR_record_t NDR;
      mach_port_name_t name;
      mach_port_right_t right;
      mach_port_delta_t delta;
   } Request;
#pragma pack()

   Request *req = (Request *)ARG1;

   PRINT("mach_port_mod_refs(%s, %s, 0x%x, 0x%x)", 
         name_for_port(MACH_REMOTE), 
         name_for_port(req->name), req->right, req->delta);

   MACH_ARG(mach_port_mod_refs.port) = req->name;
   MACH_ARG(mach_port_mod_refs.right) = req->right;
   MACH_ARG(mach_port_mod_refs.delta) = req->delta;
    
   AFTER = POST_FN(mach_port_mod_refs);

   
   
   *flags &= ~SfMayBlock;
}

POST(mach_port_mod_refs)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      NDR_record_t NDR;
      kern_return_t RetCode;
   } Reply;
#pragma pack()
   
   Reply *reply = (Reply *)ARG1;

   if (!reply->RetCode) {
      if (MACH_REMOTE == vg_task_port) {
         
         record_port_mod_refs(MACH_ARG(mach_port_mod_refs.port), 
                              MACH_PORT_TYPE(MACH_ARG(mach_port_mod_refs.right)), 
                              MACH_ARG(mach_port_mod_refs.delta));
      } else {
         VG_(printf)("UNKNOWN remote port mod refs\n");
      }
   } else {
      PRINT("mig return %d", reply->RetCode);
   }
}


PRE(mach_port_get_set_status)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      NDR_record_t NDR;
      mach_port_name_t name;
   } Request;
#pragma pack()

   Request *req = (Request *)ARG1;

   PRINT("mach_port_get_set_status(%s, %s)", 
         name_for_port(MACH_REMOTE), 
         name_for_port(req->name));

   AFTER = POST_FN(mach_port_get_set_status);
}

POST(mach_port_get_set_status)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      
      mach_msg_body_t msgh_body;
      mach_msg_ool_descriptor_t members;
      
      NDR_record_t NDR;
      mach_msg_type_number_t membersCnt;
      mach_msg_trailer_t trailer;
   } Reply;
#pragma pack()

   

   
}


PRE(mach_port_move_member)
{
#pragma pack(4)
    typedef struct {
        mach_msg_header_t Head;
        NDR_record_t NDR;
        mach_port_name_t member;
        mach_port_name_t after;
    } Request;
#pragma pack()

    Request *req = (Request *)ARG1;

    PRINT("mach_port_move_member(%s, %s, %s)", 
          name_for_port(MACH_REMOTE), 
          name_for_port(req->member), 
          name_for_port(req->after));
    AFTER = POST_FN(mach_port_move_member);
}

POST(mach_port_move_member)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      NDR_record_t NDR;
      kern_return_t RetCode;
      mach_msg_trailer_t trailer;
   } Reply;
#pragma pack()

   Reply *reply = (Reply *)ARG1;

   if (!reply->RetCode) {
      
   } else {
      PRINT("mig return %d", reply->RetCode);
   }
}


PRE(mach_port_destroy)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      NDR_record_t NDR;
      mach_port_name_t name;
   } Request;
#pragma pack()

   Request *req = (Request *)ARG1;

   PRINT("mach_port_destroy(%s, %s)", 
         name_for_port(MACH_REMOTE), 
         name_for_port(req->name));

   MACH_ARG(mach_port.port) = req->name;

   AFTER = POST_FN(mach_port_destroy);

   
   
   *flags &= ~SfMayBlock;
}

POST(mach_port_destroy)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      NDR_record_t NDR;
      kern_return_t RetCode;
   } Reply;
#pragma pack()
   
   Reply *reply = (Reply *)ARG1;

   if (!reply->RetCode) {
      if (MACH_REMOTE == vg_task_port) {
         
         record_port_destroy(MACH_ARG(mach_port.port));
      } else {
         VG_(printf)("UNKNOWN remote port destroy\n");
      }
   } else {
      PRINT("mig return %d", reply->RetCode);
   }
}


PRE(mach_port_request_notification)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      
      mach_msg_body_t msgh_body;
      mach_msg_port_descriptor_t notify;
      
      NDR_record_t NDR;
      mach_port_name_t name;
      mach_msg_id_t msgid;
      mach_port_mscount_t sync;
   } Request;
#pragma pack()

   Request *req = (Request *)ARG1;

   PRINT("mach_port_request_notification(%s, %s, %d, %d, %d, %d, ...)", 
         name_for_port(MACH_REMOTE), 
         name_for_port(req->name), req->msgid, req->sync, 
         req->notify.name, req->notify.disposition);

   AFTER = POST_FN(mach_port_request_notification);
}

POST(mach_port_request_notification)
{
   
}


PRE(mach_port_insert_right)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      
      mach_msg_body_t msgh_body;
      mach_msg_port_descriptor_t poly;
      
      NDR_record_t NDR;
      mach_port_name_t name;
   } Request;
#pragma pack()

   Request *req = (Request *)ARG1;

   PRINT("mach_port_insert_right(%s, %s, %d, %d)", 
         name_for_port(MACH_REMOTE), 
         name_for_port(req->name), req->poly.name, req->poly.disposition);

   AFTER = POST_FN(mach_port_insert_right);

   if (MACH_REMOTE == mach_task_self()) {
      
      
   } else {
      VG_(printf)("UNKNOWN mach_port_insert_right into remote task!\n");
      
   }

   
}

POST(mach_port_insert_right)
{
}


PRE(mach_port_extract_right)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      NDR_record_t NDR;
      mach_port_name_t name;
      mach_msg_type_name_t msgt_name;
   } Request;
#pragma pack()

   Request *req = (Request *)ARG1;
   
   PRINT("mach_port_extract_right(%s, %s, %d)", 
         name_for_port(MACH_REMOTE), 
         name_for_port(req->name), req->msgt_name);
   
   AFTER = POST_FN(mach_port_extract_right);
   
   
}

POST(mach_port_extract_right)
{
   
}


PRE(mach_port_get_attributes)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      NDR_record_t NDR;
      mach_port_name_t name;
      mach_port_flavor_t flavor;
      mach_msg_type_number_t port_info_outCnt;
   } Request;
#pragma pack()

   Request *req = (Request *)ARG1;

   PRINT("mach_port_get_attributes(%s, %s, %d, ..., %d)", 
         name_for_port(MACH_REMOTE), 
         name_for_port(req->name), req->flavor, req->port_info_outCnt);

   AFTER = POST_FN(mach_port_get_attributes);
}

POST(mach_port_get_attributes)
{
}


PRE(mach_port_set_attributes)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      NDR_record_t NDR;
      mach_port_name_t name;
      mach_port_flavor_t flavor;
      mach_msg_type_number_t port_infoCnt;
      integer_t port_info[10];
   } Request;
#pragma pack()

   Request *req = (Request *)ARG1;

   PRINT("mach_port_set_attributes(%s, %s, %d, ..., %d)", 
        name_for_port(MACH_REMOTE), 
        name_for_port(req->name), req->flavor, req->port_infoCnt);

   AFTER = POST_FN(mach_port_set_attributes);
}

POST(mach_port_set_attributes)
{
}


PRE(mach_port_insert_member)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      NDR_record_t NDR;
      mach_port_name_t name;
      mach_port_name_t pset;
   } Request;
#pragma pack()

   Request *req = (Request *)ARG1;

   PRINT("mach_port_insert_member(%s, 0x%x, 0x%x)", 
         name_for_port(MACH_REMOTE), req->name, req->pset);

   AFTER = POST_FN(mach_port_insert_member);

   
}

POST(mach_port_insert_member)
{
}


PRE(task_get_special_port)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      NDR_record_t NDR;
      int which_port;
   } Request;
#pragma pack()
   
   Request *req = (Request *)ARG1;
   
   switch (req->which_port) {
   case TASK_KERNEL_PORT:
      PRINT("task_get_special_port(%s, TASK_KERNEL_PORT)", 
            name_for_port(MACH_REMOTE));
      break;
   case TASK_HOST_PORT:
      PRINT("task_get_special_port(%s, TASK_HOST_PORT)", 
            name_for_port(MACH_REMOTE));
      break;
   case TASK_BOOTSTRAP_PORT:
      PRINT("task_get_special_port(%s, TASK_BOOTSTRAP_PORT)", 
            name_for_port(MACH_REMOTE));
      break;
#if DARWIN_VERS < DARWIN_10_8
   
   case TASK_WIRED_LEDGER_PORT:
      PRINT("task_get_special_port(%s, TASK_WIRED_LEDGER_PORT)", 
            name_for_port(MACH_REMOTE));
      break;
   case TASK_PAGED_LEDGER_PORT:
      PRINT("task_get_special_port(%s, TASK_PAGED_LEDGER_PORT)", 
            name_for_port(MACH_REMOTE));
      break;
#endif
   default:
      PRINT("task_get_special_port(%s, %d)", 
            name_for_port(MACH_REMOTE), req->which_port);
      break;
   }
   
   MACH_ARG(task_get_special_port.which_port) = req->which_port;
   
   AFTER = POST_FN(task_get_special_port);
}

POST(task_get_special_port)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      
      mach_msg_body_t msgh_body;
      mach_msg_port_descriptor_t special_port;
      
   } Reply;
#pragma pack()
   
   Reply *reply = (Reply *)ARG1;

   PRINT("got port %#x ", reply->special_port.name);

   switch (MACH_ARG(task_get_special_port.which_port)) {
   case TASK_BOOTSTRAP_PORT:
      vg_bootstrap_port = reply->special_port.name;
      assign_port_name(reply->special_port.name, "bootstrap");
      break;
   case TASK_KERNEL_PORT:
      assign_port_name(reply->special_port.name, "kernel");
      break;
   case TASK_HOST_PORT:
      assign_port_name(reply->special_port.name, "host");
      break;
#if DARWIN_VERS < DARWIN_10_8
   
   case TASK_WIRED_LEDGER_PORT:
      assign_port_name(reply->special_port.name, "wired-ledger");
      break;
   case TASK_PAGED_LEDGER_PORT:
      assign_port_name(reply->special_port.name, "paged-ledger");
      break;
#endif
   default:
      assign_port_name(reply->special_port.name, "special-%p");
      break;
   }

   PRINT("%s", name_for_port(reply->special_port.name));
}


PRE(semaphore_create)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      NDR_record_t NDR;
      int policy;
      int value;
   } Request;
#pragma pack()

   Request *req = (Request *)ARG1;

   PRINT("semaphore_create(%s, ..., %d, %d)",
         name_for_port(MACH_REMOTE), req->policy, req->value);

   AFTER = POST_FN(semaphore_create);
}

POST(semaphore_create)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      
      mach_msg_body_t msgh_body;
      mach_msg_port_descriptor_t semaphore;
      
      mach_msg_trailer_t trailer;
   } Reply;
#pragma pack()

   Reply *reply = (Reply *)ARG1;

   assign_port_name(reply->semaphore.name, "semaphore-%p");
   PRINT("%s", name_for_port(reply->semaphore.name));
}


PRE(semaphore_destroy)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      
      mach_msg_body_t msgh_body;
      mach_msg_port_descriptor_t semaphore;
      
      mach_msg_trailer_t trailer;
   } Request;
#pragma pack()

   Request *req = (Request *)ARG1;

   PRINT("semaphore_destroy(%s, %s)", 
         name_for_port(MACH_REMOTE), name_for_port(req->semaphore.name));

   record_port_destroy(req->semaphore.name);

   AFTER = POST_FN(semaphore_destroy);
}

POST(semaphore_destroy)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      NDR_record_t NDR;
      kern_return_t RetCode;
      mach_msg_trailer_t trailer;
   } Reply;
#pragma pack()

   Reply *reply = (Reply *)ARG1;        
   if (!reply->RetCode) {
   } else {
      PRINT("mig return %d", reply->RetCode);
   }
}

PRE(task_policy_set)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      NDR_record_t NDR;
      task_policy_flavor_t flavor;
      mach_msg_type_number_t policy_infoCnt;
      integer_t policy_info[16];
   } Request;
#pragma pack()

   Request *req = (Request *)ARG1;

   PRINT("task_policy_set(%s) flavor:%d", name_for_port(MACH_REMOTE), req->flavor);

   AFTER = POST_FN(task_policy_set);
}

POST(task_policy_set)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      NDR_record_t NDR;
      kern_return_t RetCode;
   } Reply;
#pragma pack()

   Reply *reply = (Reply *)ARG1;        
   if (!reply->RetCode) {
   } else {
      PRINT("mig return %d", reply->RetCode);
   }
}


PRE(mach_ports_register)
{
#pragma pack(4)
    typedef struct {
       mach_msg_header_t Head;
       
       mach_msg_body_t msgh_body;
       mach_msg_ool_ports_descriptor_t init_port_set;
       
       NDR_record_t NDR;
       mach_msg_type_number_t init_port_setCnt;
    } Request;
#pragma pack()
    
    
    
    PRINT("mach_ports_register(%s)", name_for_port(MACH_REMOTE));
    
    AFTER = POST_FN(mach_ports_register);
}

POST(mach_ports_register)
{
#pragma pack(4)
    typedef struct {
       mach_msg_header_t Head;
       NDR_record_t NDR;
       kern_return_t RetCode;
    } Reply;
#pragma pack()
    
    Reply *reply = (Reply *)ARG1;
    if (!reply->RetCode) {
    } else {
        PRINT("mig return %d", reply->RetCode);
    }
}


PRE(mach_ports_lookup)
{
#pragma pack(4)
   typedef struct {
       mach_msg_header_t Head;
   } Request;
#pragma pack()

   

   PRINT("mach_ports_lookup(%s)", name_for_port(MACH_REMOTE));

   AFTER = POST_FN(mach_ports_lookup);
}

POST(mach_ports_lookup)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      
      mach_msg_body_t msgh_body;
      mach_msg_ool_ports_descriptor_t init_port_set;
      
      NDR_record_t NDR;
      mach_msg_type_number_t init_port_setCnt;
   } Reply;
#pragma pack()

    
}


PRE(task_info)
{
#pragma pack(4)
    typedef struct {
        mach_msg_header_t Head;
        NDR_record_t NDR;
        task_flavor_t flavor;
        mach_msg_type_number_t task_info_outCnt;
    } Request;
#pragma pack()
    
    Request *req = (Request *)ARG1;
    
    PRINT("task_info(%s) flavor:%d", name_for_port(MACH_REMOTE), req->flavor);
    
    AFTER = POST_FN(task_info);
}

POST(task_info)
{
#pragma pack(4)
    typedef struct {
        mach_msg_header_t Head;
        NDR_record_t NDR;
        kern_return_t RetCode;
        mach_msg_type_number_t task_info_outCnt;
        integer_t task_info_out[52];
    } Reply;
#pragma pack()
    
    Reply *reply = (Reply *)ARG1;
    if (!reply->RetCode) {
    } else {
        PRINT("mig return %d", reply->RetCode);
    }
}


PRE(task_threads)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
   } Request;
#pragma pack()

   

   PRINT("task_threads(%s)", name_for_port(MACH_REMOTE));

   AFTER = POST_FN(task_threads);
}

POST(task_threads)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      
      mach_msg_body_t msgh_body;
      mach_msg_ool_ports_descriptor_t act_list;
      
      NDR_record_t NDR;
      mach_msg_type_number_t act_listCnt;
      mach_msg_trailer_t trailer;
   } Reply;
#pragma pack()

   Reply *reply = (Reply *)ARG1;

   if (MACH_REMOTE == vg_task_port) {
      assign_port_names(&reply->act_list, "thread-%p");
   } else {
      assign_port_names(&reply->act_list, "remote-thread-%p");
   }
}


PRE(task_suspend)
{
   PRINT("task_suspend(%s)", name_for_port(MACH_REMOTE));

   if (MACH_REMOTE == vg_task_port) {
      
      vg_assert(0);
   } else {
      
   }

   AFTER = POST_FN(task_suspend);
}

POST(task_suspend)
{
}


PRE(task_resume)
{
   PRINT("task_resume(%s)", name_for_port(MACH_REMOTE));

   if (MACH_REMOTE == vg_task_port) {
      
      vg_assert(0);
   } else {
      
   }

   AFTER = POST_FN(task_resume);
}

POST(task_resume)
{
}


PRE(vm_allocate)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      NDR_record_t NDR;
      vm_address_t address;
      vm_size_t size;
      int flags;
   } Request;
#pragma pack()

   Request *req = (Request *)ARG1;

   PRINT("vm_allocate (%s, at %#llx, size %lld, flags %#x)", 
         name_for_port(MACH_REMOTE), 
         (ULong)req->address, (ULong)req->size, req->flags);

   MACH_ARG(vm_allocate.size) = req->size;
   MACH_ARG(vm_allocate.flags) = req->flags;

   AFTER = POST_FN(vm_allocate);
}

POST(vm_allocate)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      NDR_record_t NDR;
      kern_return_t RetCode;
      vm_address_t address;
      mach_msg_trailer_t trailer;
   } Reply;
#pragma pack()
   
   Reply *reply = (Reply *)ARG1;
   
   if (!reply->RetCode) {
      if (MACH_REMOTE == vg_task_port) {
        PRINT("allocated at %#llx", (ULong)reply->address);
         
         if (MACH_ARG(vm_allocate.size)) {
            ML_(notify_core_and_tool_of_mmap)(
                  reply->address, MACH_ARG(vm_allocate.size), 
                  VKI_PROT_READ|VKI_PROT_WRITE, VKI_MAP_ANON, -1, 0);
         }
      } else {
         PRINT("allocated at %#llx in remote task %s",
               (ULong)reply->address, 
               name_for_port(MACH_REMOTE));
      }
   } else {
      PRINT("mig return %d", reply->RetCode);
   }
}


PRE(vm_deallocate)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      NDR_record_t NDR;
      vm_address_t address;
      vm_size_t size;
   } Request;
#pragma pack()
   
   Request *req = (Request *)ARG1;
   
   PRINT("vm_deallocate(%s, at %#llx, size %lld)", 
         name_for_port(MACH_REMOTE), 
         (ULong)req->address, (ULong)req->size);
   
   MACH_ARG(vm_deallocate.address) = req->address;
   MACH_ARG(vm_deallocate.size) = req->size;
   
   AFTER = POST_FN(vm_deallocate);

   
   
   *flags &= ~SfMayBlock;
}

POST(vm_deallocate)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      NDR_record_t NDR;
      kern_return_t RetCode;
      mach_msg_trailer_t trailer;
   } Reply;
#pragma pack()
   
   Reply *reply = (Reply *)ARG1;
   
   if (!reply->RetCode) {
      if (MACH_REMOTE == vg_task_port) {
         if (MACH_ARG(vm_deallocate.size)) {
            Addr start = VG_PGROUNDDN(MACH_ARG(vm_deallocate.address));
            Addr end = VG_PGROUNDUP(MACH_ARG(vm_deallocate.address) + 
                                    MACH_ARG(vm_deallocate.size));
            
            ML_(notify_core_and_tool_of_munmap)(start, end - start);
         }
      }
   } else {
      PRINT("mig return %d", reply->RetCode);
   }
}
   

PRE(vm_protect)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      NDR_record_t NDR;
      vm_address_t address;
      vm_size_t size;
      boolean_t set_maximum;
      vm_prot_t new_protection;
   } Request;
#pragma pack()
   
   Request *req = (Request *)ARG1;
   
   PRINT("vm_protect(%s, at %#llx, size %lld, set_max %d, prot %d)", 
         name_for_port(MACH_REMOTE),
         (ULong)req->address, (ULong)req->size, 
         req->set_maximum, req->new_protection);
   
   MACH_ARG(vm_protect.address) = req->address;
   MACH_ARG(vm_protect.size) = req->size;
   MACH_ARG(vm_protect.set_maximum) = req->set_maximum;
   MACH_ARG(vm_protect.new_protection) = req->new_protection;
   
   AFTER = POST_FN(vm_protect);
}

POST(vm_protect)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      NDR_record_t NDR;
      kern_return_t RetCode;
      mach_msg_trailer_t trailer;
   } Reply;
#pragma pack()
   
   Reply *reply = (Reply *)ARG1;
   
   if (!reply->RetCode) {
      if (MACH_REMOTE == vg_task_port) {
         Addr start = VG_PGROUNDDN(MACH_ARG(vm_protect.address));
         Addr end = VG_PGROUNDUP(MACH_ARG(vm_protect.address) + 
                                 MACH_ARG(vm_protect.size));
         UInt prot = MACH_ARG(vm_protect.new_protection);
         if (MACH_ARG(vm_protect.set_maximum)) {
             
             VG_(printf)("UNKNOWN vm_protect set maximum");
            
         } else {
            ML_(notify_core_and_tool_of_mprotect)(start, end-start, prot);
            VG_(di_notify_vm_protect)(start, end-start, prot);
         }
      }
   } else {
      PRINT("mig return %d", reply->RetCode);
   }
}


PRE(vm_inherit)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      NDR_record_t NDR;
      vm_address_t address;
      vm_size_t size;
      vm_inherit_t new_inheritance;
   } Request;
#pragma pack()
   
   Request *req = (Request *)ARG1;
   
   PRINT("vm_inherit(%s, at %#llx, size %lld, value %d)", 
         name_for_port(MACH_REMOTE), 
         (ULong)req->address, (ULong)req->size, 
         req->new_inheritance);
   
   AFTER = POST_FN(vm_inherit);
}

POST(vm_inherit)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      NDR_record_t NDR;
      kern_return_t RetCode;
      mach_msg_trailer_t trailer;
   } Reply;
#pragma pack()
   
   Reply *reply = (Reply *)ARG1;
   
   if (!reply->RetCode) {
      if (MACH_REMOTE == vg_task_port) {
         
      }
   } else {
      PRINT("mig return %d", reply->RetCode);
   }
}


PRE(vm_read)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      NDR_record_t NDR;
      vm_address_t address;
      vm_size_t size;
   } Request;
#pragma pack()

   Request *req = (Request *)ARG1;

   PRINT("vm_read(from %s at %#llx size %llu)", 
         name_for_port(MACH_REMOTE),
         (ULong)req->address, (ULong)req->size);
   
   MACH_ARG(vm_read.addr) = req->address;
   MACH_ARG(vm_read.size) = req->size;

   AFTER = POST_FN(vm_read);
}

POST(vm_read)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      
      mach_msg_body_t msgh_body;
      mach_msg_ool_descriptor_t data;
      
      NDR_record_t NDR;
      mach_msg_type_number_t dataCnt;
   } Reply;
#pragma pack()

   

   if (MACH_REMOTE == vg_task_port) {
      
      
   }
}



PRE(mach_vm_read)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      NDR_record_t NDR;
      mach_vm_address_t address;
      mach_vm_size_t size;
   } Request;
#pragma pack()

   Request *req = (Request *)ARG1;

   PRINT("mach_vm_read(from %s at 0x%llx size %llu)", 
         name_for_port(MACH_REMOTE), req->address, req->size);
   
   MACH_ARG(mach_vm_read.addr) = req->address;
   MACH_ARG(mach_vm_read.size) = req->size;

   AFTER = POST_FN(mach_vm_read);
}

POST(mach_vm_read)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      
      mach_msg_body_t msgh_body;
      mach_msg_ool_descriptor_t data;
      
      NDR_record_t NDR;
      mach_msg_type_number_t dataCnt;
   } Reply;
#pragma pack()

   

   if (MACH_REMOTE == vg_task_port) {
      
      
   }
}


PRE(vm_read_overwrite)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      NDR_record_t NDR;
      vm_address_t address;
      vm_size_t size;
      vm_address_t data;
   } Request;
#pragma pack()

   Request *req = (Request *)ARG1;

   PRINT("vm_read_overwrite(from %s at %#llx size %llu to %#llx)", 
         name_for_port(MACH_REMOTE),
         (ULong)req->address, (ULong)req->size, (ULong)req->data);
   
   MACH_ARG(vm_read_overwrite.addr) = req->address;
   MACH_ARG(vm_read_overwrite.size) = req->size;
   MACH_ARG(vm_read_overwrite.data) = req->data;

   PRE_MEM_WRITE("vm_read_overwrite(data)", req->data, req->size);

   AFTER = POST_FN(vm_read_overwrite);
}

POST(vm_read_overwrite)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      NDR_record_t NDR;
      kern_return_t RetCode;
      vm_size_t outsize;
   } Reply;
#pragma pack()

   Reply *reply = (Reply *)ARG1;

   if (reply->RetCode) {
       PRINT("mig return %d", reply->RetCode);
   } else {
      PRINT("read %llu bytes", (unsigned long long)reply->outsize);
      if (MACH_REMOTE == vg_task_port) {
         
         
         POST_MEM_WRITE(MACH_ARG(vm_read_overwrite.data), reply->outsize);
      } else {
         
         POST_MEM_WRITE(MACH_ARG(vm_read_overwrite.data), reply->outsize);
      }
   }
}


PRE(vm_copy)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      NDR_record_t NDR;
      vm_address_t source_address;
      vm_size_t size;
      vm_address_t dest_address;
   } Request;
#pragma pack()
   
   Request *req = (Request *)ARG1;
   
   PRINT("vm_copy(%s, %#llx, %lld, %#llx)", 
         name_for_port(MACH_REMOTE), 
         (ULong)req->source_address,
         (ULong)req->size, (ULong)req->dest_address);

   MACH_ARG(vm_copy.src) = req->source_address;
   MACH_ARG(vm_copy.dst) = req->dest_address;
   MACH_ARG(vm_copy.size) = req->size;
   
   AFTER = POST_FN(vm_copy);
}

POST(vm_copy)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      NDR_record_t NDR;
      kern_return_t RetCode;
      mach_msg_trailer_t trailer;
   } Reply;
#pragma pack()
   
   Reply *reply = (Reply *)ARG1;
   
   if (!reply->RetCode) {
      if (MACH_REMOTE == vg_task_port) {
         
         
      }
   } else {
      PRINT("mig return %d", reply->RetCode);
   }
}


PRE(vm_map)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      
      mach_msg_body_t msgh_body;
      mach_msg_port_descriptor_t object;
      
      NDR_record_t NDR;
      vm_address_t address;
      vm_size_t size;
      vm_address_t mask;
      int flags;
      vm_offset_t offset;
      boolean_t copy;
      vm_prot_t cur_protection;
      vm_prot_t max_protection;
      vm_inherit_t inheritance;
   } Request;
#pragma pack()

   Request *req = (Request *)ARG1;

   
   PRINT("vm_map(in %s, at %#llx, size %lld, from %s ...)", 
         name_for_port(MACH_REMOTE), 
         (ULong)req->address, (ULong)req->size, 
         name_for_port(req->object.name));

   MACH_ARG(vm_map.size) = req->size;
   MACH_ARG(vm_map.copy) = req->copy;
   MACH_ARG(vm_map.protection) = (req->cur_protection & req->max_protection);

   AFTER = POST_FN(vm_map);
}

POST(vm_map)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      NDR_record_t NDR;
      kern_return_t RetCode;
      vm_address_t address;
      mach_msg_trailer_t trailer;
   } Reply;
#pragma pack()

   Reply *reply = (Reply *)ARG1;

   if (!reply->RetCode) {
      
     PRINT("mapped at %#llx", (ULong)reply->address);
      
      ML_(notify_core_and_tool_of_mmap)(
            reply->address, VG_PGROUNDUP(MACH_ARG(vm_map.size)), 
            MACH_ARG(vm_map.protection), VKI_MAP_SHARED, -1, 0);
      
   } else {
      PRINT("mig return %d", reply->RetCode);
   }
}


PRE(vm_remap)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      
      mach_msg_body_t msgh_body;
      mach_msg_port_descriptor_t src_task;
      
      NDR_record_t NDR;
      vm_address_t target_address;
      vm_size_t size;
      vm_address_t mask;
      boolean_t anywhere;
      vm_address_t src_address;
      boolean_t copy;
      vm_inherit_t inheritance;
   } Request;
#pragma pack()

   Request *req = (Request *)ARG1;

   

   if (VG_(clo_trace_syscalls)) {
      mach_port_name_t source_task = req->src_task.name;
      if (source_task == mach_task_self()) {
         PRINT("vm_remap(mach_task_self(), "
               "to %#llx size %lld, from mach_task_self() at %#llx, ...)",
               (ULong)req->target_address,
               (ULong)req->size, (ULong)req->src_address);
      } else {
         PRINT("vm_remap(mach_task_self(), "
               "to %#llx size %lld, from task %u at %#llx, ...)",
               (ULong)req->target_address, (ULong)req->size, 
               source_task, (ULong)req->src_address);
      }
   }

   
   
   MACH_ARG(vm_remap.size) = req->size;
   

   AFTER = POST_FN(vm_remap);
}

POST(vm_remap)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      NDR_record_t NDR;
      kern_return_t RetCode;
      vm_address_t target_address;
      vm_prot_t cur_protection;
      vm_prot_t max_protection;
      mach_msg_trailer_t trailer;
   } Reply;
#pragma pack()

   Reply *reply = (Reply *)ARG1;

   if (!reply->RetCode) {
      
      UInt prot = reply->cur_protection & reply->max_protection;
      
      PRINT("mapped at %#llx", (ULong)reply->target_address);
      ML_(notify_core_and_tool_of_mmap)(
            reply->target_address, VG_PGROUNDUP(MACH_ARG(vm_remap.size)), 
            prot, VKI_MAP_SHARED, -1, 0);
      
      
   } else {
      PRINT("mig return %d", reply->RetCode);
   }
}


PRE(mach_make_memory_entry_64)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      
      mach_msg_body_t msgh_body;
      mach_msg_port_descriptor_t parent_entry;
      
      NDR_record_t NDR;
      memory_object_size_t size;
      memory_object_offset_t offset;
      vm_prot_t permission;
   } Request;
#pragma pack()

   Request *req = (Request *)ARG1;

   PRINT("mach_make_memory_entry_64(%s, %llu, %llu, %d, ..., %u)", 
         name_for_port(MACH_REMOTE), 
         req->size, req->offset, req->permission, req->parent_entry.type);

   AFTER = POST_FN(mach_make_memory_entry_64);
}

POST(mach_make_memory_entry_64)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      mach_msg_body_t msgh_body;
      mach_msg_port_descriptor_t object;
      NDR_record_t NDR;
      memory_object_size_t size;
   } Reply;
#pragma pack()

   Reply *reply = (Reply *)ARG1;

   if (reply->Head.msgh_bits & MACH_MSGH_BITS_COMPLEX) {
      assign_port_name(reply->object.name, "memory-%p");
      PRINT("%s", name_for_port(reply->object.name));
   }
}


PRE(vm_purgable_control)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      NDR_record_t NDR;
      vm_address_t address;
      vm_purgable_t control;
      int state;
   } Request;
#pragma pack()

   Request *req = (Request *)ARG1;

   PRINT("vm_purgable_control(%s, %#llx, %d, %d)", 
         name_for_port(MACH_REMOTE), 
         (ULong)req->address, req->control, req->state);

   

   AFTER = POST_FN(vm_purgable_control);
}

POST(vm_purgable_control)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      NDR_record_t NDR;
      kern_return_t RetCode;
      int state;
   } Reply;
#pragma pack()

   Reply *reply = (Reply *)ARG1;

   if (!reply->RetCode) {
   } else {
      PRINT("mig return %d", reply->RetCode);
   }
}


PRE(mach_vm_purgable_control)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      NDR_record_t NDR;
      mach_vm_address_t address;
      vm_purgable_t control;
      int state;
   } Request;
#pragma pack()

   Request *req = (Request *)ARG1;

   PRINT("mach_vm_purgable_control(%s, 0x%llx, %d, %d)", 
         name_for_port(MACH_REMOTE), 
         (unsigned long long)req->address, req->control, req->state);

   

   AFTER = POST_FN(mach_vm_purgable_control);
}

POST(mach_vm_purgable_control)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      NDR_record_t NDR;
      kern_return_t RetCode;
      int state;
   } Reply;
#pragma pack()

   Reply *reply = (Reply *)ARG1;

   if (!reply->RetCode) {
   } else {
      PRINT("mig return %d", reply->RetCode);
   }
}


PRE(mach_vm_allocate)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      NDR_record_t NDR;
      mach_vm_address_t address;
      mach_vm_size_t size;
      int flags;
   } Request;
#pragma pack()

   Request *req = (Request *)ARG1;

   PRINT("mach_vm_allocate (%s, at 0x%llx, size %lld, flags 0x%x)", 
         name_for_port(MACH_REMOTE), 
         req->address, req->size, req->flags);

   MACH_ARG(mach_vm_allocate.size) = req->size;
   MACH_ARG(mach_vm_allocate.flags) = req->flags;

   AFTER = POST_FN(mach_vm_allocate);
}

POST(mach_vm_allocate)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      NDR_record_t NDR;
      kern_return_t RetCode;
      mach_vm_address_t address;
      mach_msg_trailer_t trailer;
   } Reply;
#pragma pack()
   
   Reply *reply = (Reply *)ARG1;
   
   if (!reply->RetCode) {
      if (MACH_REMOTE == vg_task_port) {
         PRINT("allocated at 0x%llx", reply->address);
         
         if (MACH_ARG(mach_vm_allocate.size)) {
            ML_(notify_core_and_tool_of_mmap)(
                  reply->address, MACH_ARG(mach_vm_allocate.size), 
                  VKI_PROT_READ|VKI_PROT_WRITE, VKI_MAP_ANON, -1, 0);
         }
      } else {
         PRINT("allocated at 0x%llx in remote task %s", reply->address, 
               name_for_port(MACH_REMOTE));
      }
   } else {
      PRINT("mig return %d", reply->RetCode);
   }
}


PRE(mach_vm_deallocate)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      NDR_record_t NDR;
      mach_vm_address_t address;
      mach_vm_size_t size;
   } Request;
#pragma pack()
   
   Request *req = (Request *)ARG1;
   
   PRINT("mach_vm_deallocate(%s, at 0x%llx, size %lld)", 
         name_for_port(MACH_REMOTE), 
         req->address, req->size);
   
   MACH_ARG(mach_vm_deallocate.address) = req->address;
   MACH_ARG(mach_vm_deallocate.size) = req->size;
   
   AFTER = POST_FN(mach_vm_deallocate);

   
   
   *flags &= ~SfMayBlock;
}

POST(mach_vm_deallocate)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      NDR_record_t NDR;
      kern_return_t RetCode;
      mach_msg_trailer_t trailer;
   } Reply;
#pragma pack()
   
   Reply *reply = (Reply *)ARG1;
   
   if (!reply->RetCode) {
      if (MACH_REMOTE == vg_task_port) {
         if (MACH_ARG(mach_vm_deallocate.size)) {
            Addr start = VG_PGROUNDDN(MACH_ARG(mach_vm_deallocate.address));
            Addr end = VG_PGROUNDUP(MACH_ARG(mach_vm_deallocate.address) + 
                                    MACH_ARG(mach_vm_deallocate.size));
            
            ML_(notify_core_and_tool_of_munmap)(start, end - start);
         }
      }
   } else {
      PRINT("mig return %d", reply->RetCode);
   }
}
   

PRE(mach_vm_protect)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      NDR_record_t NDR;
      mach_vm_address_t address;
      mach_vm_size_t size;
      boolean_t set_maximum;
      vm_prot_t new_protection;
   } Request;
#pragma pack()
   
   Request *req = (Request *)ARG1;
   
   PRINT("mach_vm_protect(%s, at 0x%llx, size %lld, set_max %d, prot %d)", 
         name_for_port(MACH_REMOTE), req->address, req->size, 
         req->set_maximum, req->new_protection);
   
   MACH_ARG(mach_vm_protect.address) = req->address;
   MACH_ARG(mach_vm_protect.size) = req->size;
   MACH_ARG(mach_vm_protect.set_maximum) = req->set_maximum;
   MACH_ARG(mach_vm_protect.new_protection) = req->new_protection;
   
   AFTER = POST_FN(mach_vm_protect);
}

POST(mach_vm_protect)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      NDR_record_t NDR;
      kern_return_t RetCode;
      mach_msg_trailer_t trailer;
   } Reply;
#pragma pack()
   
   Reply *reply = (Reply *)ARG1;
   
   if (!reply->RetCode) {
      if (MACH_REMOTE == vg_task_port) {
         Addr start = VG_PGROUNDDN(MACH_ARG(mach_vm_protect.address));
         Addr end = VG_PGROUNDUP(MACH_ARG(mach_vm_protect.address) + 
                                 MACH_ARG(mach_vm_protect.size));
         UInt prot = MACH_ARG(mach_vm_protect.new_protection);
         if (MACH_ARG(mach_vm_protect.set_maximum)) {
            
            
         } else {
            ML_(notify_core_and_tool_of_mprotect)(start, end-start, prot);
         }
      }
   } else {
      PRINT("mig return %d", reply->RetCode);
   }
}


PRE(mach_vm_inherit)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      NDR_record_t NDR;
      mach_vm_address_t address;
      mach_vm_size_t size;
      vm_inherit_t new_inheritance;
   } Request;
#pragma pack()

   Request *req = (Request *)ARG1;
   
   PRINT("mach_vm_inherit(to %s, at 0x%llx, size %llu, value %u)", 
         name_for_port(MACH_REMOTE), 
         req->address, req->size, req->new_inheritance);

   AFTER = POST_FN(mach_vm_inherit);
}

POST(mach_vm_inherit)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      NDR_record_t NDR;
      kern_return_t RetCode;
      mach_msg_trailer_t trailer;
   } Reply;
#pragma pack()
   
   Reply *reply = (Reply *)ARG1;
   
   if (!reply->RetCode) {
      
      
   } else {
      PRINT("mig return %d", reply->RetCode);
   }
}


PRE(mach_vm_copy)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      NDR_record_t NDR;
      mach_vm_address_t source_address;
      mach_vm_size_t size;
      mach_vm_address_t dest_address;
   } Request;
#pragma pack()
   
   Request *req = (Request *)ARG1;
   
   PRINT("mach_vm_copy(%s, 0x%llx, %llu, 0x%llx)", 
         name_for_port(MACH_REMOTE), 
         req->source_address, req->size, req->dest_address);
   
   
   
   
   
   
   AFTER = POST_FN(mach_vm_copy);
}

POST(mach_vm_copy)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      NDR_record_t NDR;
      kern_return_t RetCode;
      mach_msg_trailer_t trailer;
   } Reply;
#pragma pack()
   
   Reply *reply = (Reply *)ARG1;
   
   if (!reply->RetCode) {
      if (MACH_REMOTE == vg_task_port) {
         
         
      }
   } else {
      PRINT("mig return %d", reply->RetCode);
   }
}

PRE(mach_vm_read_overwrite)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      NDR_record_t NDR;
      mach_vm_address_t address;
      mach_vm_size_t size;
      mach_vm_address_t data;
   } Request;
#pragma pack()

   Request *req = (Request *)ARG1;

   PRINT("mach_vm_read_overwrite(%s, 0x%llx, %llu, 0x%llx)",
         name_for_port(MACH_REMOTE),
         req->address, req->size, req->data);

   AFTER = POST_FN(mach_vm_read_overwrite);
}

POST(mach_vm_read_overwrite)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      NDR_record_t NDR;
      kern_return_t RetCode;
      mach_vm_size_t outsize;
   } Reply;
#pragma pack()

   Reply *reply = (Reply *)ARG1;

   if (!reply->RetCode) {
      if (MACH_REMOTE == vg_task_port) {
         
         
      }
   } else {
      PRINT("mig return %d", reply->RetCode);
   }
}

PRE(mach_vm_map)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      
      mach_msg_body_t msgh_body;
      mach_msg_port_descriptor_t object;
      
      NDR_record_t NDR;
      mach_vm_address_t address;
      mach_vm_size_t size;
      mach_vm_address_t mask;
      int flags;
      memory_object_offset_t offset;
      boolean_t copy;
      vm_prot_t cur_protection;
      vm_prot_t max_protection;
      vm_inherit_t inheritance;
   } Request;
#pragma pack()

   Request *req = (Request *)ARG1;

   
   PRINT("mach_vm_map(in %s->%s at 0x%llx, size %llu, cur_prot:%x max_prot:%x ...)", 
         name_for_port(req->Head.msgh_remote_port), 
         name_for_port(req->object.name),
         req->address, req->size, 
         req->cur_protection,
         req->max_protection);

   MACH_ARG(mach_vm_map.size) = req->size;
   MACH_ARG(mach_vm_map.copy) = req->copy;
   MACH_ARG(mach_vm_map.protection) = 
      (req->cur_protection & req->max_protection);

   AFTER = POST_FN(mach_vm_map);
}

POST(mach_vm_map)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      NDR_record_t NDR;
      kern_return_t RetCode;
      mach_vm_address_t address;
      mach_msg_trailer_t trailer;
   } Reply;
#pragma pack()

   Reply *reply = (Reply *)ARG1;

   if (!reply->RetCode) {
      
      PRINT("mapped at 0x%llx", reply->address);
#     if 0
      
      ML_(notify_core_and_tool_of_mmap)(
            reply->address, VG_PGROUNDUP(MACH_ARG(mach_vm_map.size)), 
            MACH_ARG(mach_vm_map.protection), VKI_MAP_SHARED, -1, 0);
      
#     else
      ML_(sync_mappings)("after", "mach_vm_map", 0);
#     endif
   } else {
      PRINT("mig return %d", reply->RetCode);
   }
}


PRE(mach_vm_remap)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      
      mach_msg_body_t msgh_body;
      mach_msg_port_descriptor_t src_task;
      
      NDR_record_t NDR;
      mach_vm_address_t target_address;
      mach_vm_size_t size;
      mach_vm_offset_t mask;
      int flags;
      mach_vm_address_t src_address;
      boolean_t copy;
      vm_inherit_t inheritance;
   } Request;
#pragma pack()

   Request *req = (Request *)ARG1;

   
   PRINT("mach_vm_remap(in %s, at 0x%llx, size %llu, from %s ...)",
         name_for_port(MACH_REMOTE),
         req->target_address, req->size,
         name_for_port(req->src_task.name));

   MACH_ARG(mach_vm_remap.size) = req->size;
   MACH_ARG(mach_vm_remap.copy) = req->copy;

   AFTER = POST_FN(mach_vm_remap);
}

POST(mach_vm_remap)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      NDR_record_t NDR;
      kern_return_t RetCode;
      mach_vm_address_t target_address;
      vm_prot_t cur_protection;
      vm_prot_t max_protection;
   } Reply;
#pragma pack()

   Reply *reply = (Reply *)ARG1;

   if (!reply->RetCode) {
      
      PRINT("mapped at 0x%llx", reply->target_address);
      
      ML_(notify_core_and_tool_of_mmap)(
            reply->target_address, VG_PGROUNDUP(MACH_ARG(mach_vm_remap.size)),
            reply->cur_protection, VKI_MAP_SHARED, -1, 0);
      
   } else {
      PRINT("mig return %d", reply->RetCode);
   }
}


PRE(mach_vm_region_recurse)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      NDR_record_t NDR;
      mach_vm_address_t address;
      natural_t nesting_depth;
      mach_msg_type_number_t infoCnt;
   } Request;
#pragma pack()

   Request *req = (Request *)ARG1;

   PRINT("mach_vm_region_recurse(in %s, at 0x%llx, depth %u, count %u)", 
         name_for_port(MACH_REMOTE), 
         req->address, req->nesting_depth, req->infoCnt);

   AFTER = POST_FN(mach_vm_region_recurse);
}

POST(mach_vm_region_recurse)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      NDR_record_t NDR;
      kern_return_t RetCode;
      mach_vm_address_t address;
      mach_vm_size_t size;
      natural_t nesting_depth;
      mach_msg_type_number_t infoCnt;
      int info[19];
   } Reply;
#pragma pack()

   Reply *reply = (Reply *)ARG1;

   if (!reply->RetCode) {
       PRINT("got region at 0x%llx, size %llu, depth %u, count %u", 
             reply->address, reply->size, 
             reply->nesting_depth, reply->infoCnt);
       
   } else {
      PRINT("mig return %d", reply->RetCode);
   }
}





POST(thread_terminate)
{
}


PRE(thread_terminate)
{
   mach_msg_header_t *mh = (mach_msg_header_t *)ARG1;
   Bool self_terminate = (mh->msgh_request_port == MACH_THREAD);

   PRINT("thread_terminate(%s)", name_for_port(mh->msgh_request_port));

   AFTER = POST_FN(thread_terminate);

   if (self_terminate) {
      
      
      ThreadState *tst = VG_(get_ThreadState)(tid);
      tst->exitreason = VgSrc_ExitThread;
      tst->os_state.exitcode = 0;  
      
      
      
      
      
      
      SET_STATUS_from_SysRes(
         VG_(mk_SysRes_x86_darwin)(
            VG_DARWIN_SYSCALL_CLASS_MACH,
            False, 0, 0
         )
      );
      *flags &= ~SfMayBlock;  
   } else {
      
      
      
      
      
      
   }
}


POST(thread_create)
{
}


PRE(thread_create)
{
   PRINT("thread_create(mach_task_self(), ...)");

   AFTER = POST_FN(thread_create);

   
   VG_(core_panic)("thread_create() unimplemented");
}


PRE(thread_create_running)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      NDR_record_t NDR;
      thread_state_flavor_t flavor;
      mach_msg_type_number_t new_stateCnt;
      natural_t new_state[144];
   } Request;
#pragma pack()
   
   Request *req;
   thread_state_t regs;
   ThreadState *new_thread;
   
   PRINT("thread_create_running(mach_task_self(), ...)");
   
   
   
   
   req = (Request *)ARG1;
   regs = (thread_state_t)req->new_state;
   
   
   new_thread = build_thread(regs, req->flavor, req->new_stateCnt);
   
   
   hijack_thread_state(regs, req->flavor, req->new_stateCnt, new_thread);

   AFTER = POST_FN(thread_create_running);
}


POST(thread_create_running)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      
      mach_msg_body_t msgh_body;
      mach_msg_port_descriptor_t child_act;
      
   } Reply;
#pragma pack()
   
   Reply *reply = (Reply *)ARG1;

   assign_port_name(reply->child_act.name, "thread-%p");
   PRINT("%s", name_for_port(reply->child_act.name));
}


PRE(bsdthread_create)
{
   ThreadState *tst;

   PRINT("bsdthread_create( %#lx, %#lx, %#lx, %#lx, %#lx )", 
         ARG1, ARG2, ARG3, ARG4, ARG5);
   PRE_REG_READ5(pthread_t,"bsdthread_create", 
                 void *,"func", void *,"func_arg", void *,"stack", 
                 pthread_t,"thread", unsigned int,"flags");

   
   
   
   
   tst = VG_(get_ThreadState)(VG_(alloc_ThreadState)());
   allocstack(tst->tid);

   tst->os_state.func_arg = (Addr)ARG2;
   ARG2 = (Word)tst;

   
   
   semaphore_create(mach_task_self(), &tst->os_state.child_go, 
                    SYNC_POLICY_FIFO, 0);
   semaphore_create(mach_task_self(), &tst->os_state.child_done, 
                    SYNC_POLICY_FIFO, 0);
}

POST(bsdthread_create)
{ 
   
   
   
   
   

   ThreadState *tst = (ThreadState *)ARG2;
   semaphore_signal(tst->os_state.child_go);
   semaphore_wait(tst->os_state.child_done);
   semaphore_destroy(mach_task_self(), tst->os_state.child_go);
   semaphore_destroy(mach_task_self(), tst->os_state.child_done);

   
   
   

   
   
   
   
   vg_assert(VG_(owns_BigLock_LL)(tid));
   VG_TRACK ( pre_thread_ll_create, tid, tst->tid );
}


PRE(bsdthread_terminate)
{
   ThreadState *tst;

   PRINT("bsdthread_terminate( %#lx, %lx, %s, %s )", 
         ARG1, ARG2, name_for_port(ARG3), name_for_port(ARG4));
   PRE_REG_READ4(int,"bsdthread_terminate", 
                 void *,"freeaddr", size_t,"freesize", 
                 mach_port_t,"kport", mach_port_t,"joinsem");

   
   
   if (ARG4) semaphore_signal((semaphore_t)ARG4);
   if (ARG1  &&  ARG2) {
       ML_(notify_core_and_tool_of_munmap)(ARG1, ARG2);
#      if DARWIN_VERS >= DARWIN_10_8
       VG_(do_syscall2)(__NR_munmap, ARG1, ARG2);
#      else
       vm_deallocate(mach_task_self(), (vm_address_t)ARG1, (vm_size_t)ARG2);
#      endif
   }

   
   
   tst = VG_(get_ThreadState)(tid);
   tst->exitreason = VgSrc_ExitThread;
   tst->os_state.exitcode = 0;  
   SET_STATUS_Success(0);
}


POST(thread_suspend)
{
}

PRE(thread_suspend)
{
   mach_msg_header_t *mh = (mach_msg_header_t *)ARG1;
   Bool self_suspend = (mh->msgh_request_port == MACH_THREAD);

   PRINT("thread_suspend(%s)", name_for_port(mh->msgh_request_port));

   AFTER = POST_FN(thread_suspend);

   if (self_suspend) {
       
       
       
       *flags |= SfMayBlock;
   } else {
       
       
       
       *flags &= ~SfMayBlock;
   }
}


POST(thread_resume)
{
}

PRE(thread_resume)
{
   mach_msg_header_t *mh = (mach_msg_header_t *)ARG1;
   Bool self_resume = (mh->msgh_request_port == MACH_THREAD);

   PRINT("thread_resume(%s)", name_for_port(mh->msgh_request_port));

   AFTER = POST_FN(thread_resume);

   if (self_resume) {
       
       
       vg_assert(0);
   } else {
       
       
       
       *flags &= ~SfMayBlock;
   }
}


POST(thread_get_state)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      NDR_record_t NDR;
      kern_return_t RetCode;
      mach_msg_type_number_t old_stateCnt;
      natural_t old_state[144];
      mach_msg_trailer_t trailer;
   } Reply;
#pragma pack()

   Reply *reply = (Reply *)ARG1;
   
   thread_state_flavor_t flavor = MACH_ARG(thread_get_state.flavor);

   if (!reply->RetCode) {
      thread_state_from_vex((thread_state_t)reply->old_state, 
                             flavor, reply->old_stateCnt, 
                             &VG_(get_ThreadState)(tid)->arch.vex);
   } else {
      PRINT("mig return %d", reply->RetCode);
   }
}

PRE(thread_get_state)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      NDR_record_t NDR;
      thread_state_flavor_t flavor;
      mach_msg_type_number_t old_stateCnt;
   } Request;
#pragma pack()
    
   Request *req = (Request *)ARG1;
   

   
   PRINT("thread_get_state(%s, %d)", 
         name_for_port(req->Head.msgh_request_port), req->flavor);

   
   MACH_ARG(thread_get_state.thread) = req->Head.msgh_request_port;
   MACH_ARG(thread_get_state.flavor) = req->flavor;

   AFTER = POST_FN(thread_get_state);
}


PRE(thread_policy)
{
   mach_msg_header_t *mh = (mach_msg_header_t *)ARG1;
   

   
      PRINT("thread_policy(%s, ...)", name_for_port(mh->msgh_request_port));

   AFTER = POST_FN(thread_policy);
}

POST(thread_policy)
{
}


PRE(thread_policy_set)
{
   mach_msg_header_t *mh = (mach_msg_header_t *)ARG1;

   PRINT("thread_policy_set(%s, ...)", name_for_port(mh->msgh_request_port));

   AFTER = POST_FN(thread_policy_set);
}

POST(thread_policy_set)
{
}


PRE(thread_info)
{
   mach_msg_header_t *mh = (mach_msg_header_t *)ARG1;

   PRINT("thread_info(%s, ...)", name_for_port(mh->msgh_request_port));
   

   AFTER = POST_FN(thread_info);
}

POST(thread_info)
{
   
}





POST(bootstrap_register)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      NDR_record_t NDR;
      kern_return_t RetCode;
      mach_msg_trailer_t trailer;
   } Reply;
#pragma pack()

   Reply *reply = (Reply *)ARG1;

   if (reply->RetCode) PRINT("mig return %d", reply->RetCode);
}

PRE(bootstrap_register)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      
      mach_msg_body_t msgh_body;
      mach_msg_port_descriptor_t service_port;
      
      NDR_record_t NDR;
      name_t service_name;
   } Request;
#pragma pack()

   Request *req = (Request *)ARG1;

   PRINT("bootstrap_register(port 0x%x, \"%s\")",
         req->service_port.name, req->service_name);

   if (!port_exists(req->service_port.name)) {
      port_create_vanilla(req->service_port.name);
   }

   assign_port_name(req->service_port.name, req->service_name);

   AFTER = POST_FN(bootstrap_register);
}


POST(bootstrap_look_up)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      
      mach_msg_body_t msgh_body;
      mach_msg_port_descriptor_t service_port;
      
      mach_msg_trailer_t trailer;
   } Reply;
#pragma pack()

   Reply *reply = (Reply *)ARG1;

   if ((reply->Head.msgh_bits & MACH_MSGH_BITS_COMPLEX)  &&  
       reply->service_port.name) 
   {
       assign_port_name(reply->service_port.name, 
                        MACH_ARG(bootstrap_look_up.service_name));
       PRINT("%s", name_for_port(reply->service_port.name));
   } else {
       PRINT("not found");
   }
   VG_(free)(MACH_ARG(bootstrap_look_up.service_name));
}

PRE(bootstrap_look_up)
{
#pragma pack(4)
   typedef struct {
      mach_msg_header_t Head;
      NDR_record_t NDR;
      name_t service_name;
   } Request;
#pragma pack()

   Request *req = (Request *)ARG1;

   PRINT("bootstrap_look_up(\"%s\")", req->service_name);

   MACH_ARG(bootstrap_look_up.service_name) =
      VG_(strdup)("syswrap-darwin.bootstrap-name", req->service_name);

   AFTER = POST_FN(bootstrap_look_up);
}




POST(mach_msg_receive)
{
   mach_msg_header_t *mh = (mach_msg_header_t *)ARG1;

   
   
   

   
   ML_(sync_mappings)("after", "mach_msg_receive", mh->msgh_id);
}

PRE(mach_msg_receive)
{
   mach_msg_header_t *mh = (mach_msg_header_t *)ARG1;
   
   PRINT("mach_msg_receive(port %s)", name_for_port(mh->msgh_reply_port));
   
   AFTER = POST_FN(mach_msg_receive);

   
   
   *flags |= SfMayBlock;
}


PRE(mach_msg_bootstrap)
{
   

   mach_msg_header_t *mh = (mach_msg_header_t *)ARG1;
   
   switch (mh->msgh_id) {
   case 403:
      CALL_PRE(bootstrap_register);
      return;
   case 404:
      CALL_PRE(bootstrap_look_up);
      return;
      
   default:
      PRINT("UNHANDLED bootstrap message [id %d, to %s, reply 0x%x]\n", 
            mh->msgh_id, name_for_port(mh->msgh_request_port),
            mh->msgh_reply_port);
      return;
   }
}


PRE(mach_msg_host)
{
   

   mach_msg_header_t *mh = (mach_msg_header_t *)ARG1;

   switch (mh->msgh_id) {
   case 200:
      CALL_PRE(host_info);
      return;
   case 202:
      CALL_PRE(host_page_size);
      return;
   case 205:
      CALL_PRE(host_get_io_master);
      return;
   case 206:
      CALL_PRE(host_get_clock_service);
      return;
   case 217:
      CALL_PRE(host_request_notification);
      return;

   default:
      
      log_decaying("UNKNOWN host message [id %d, to %s, reply 0x%x]", 
                   mh->msgh_id, name_for_port(mh->msgh_request_port), 
                   mh->msgh_reply_port);
      return;
   }
} 

PRE(mach_msg_task)
{
   

   mach_msg_header_t *mh = (mach_msg_header_t *)ARG1;

   switch (mh->msgh_id) {
   case 3201:
      CALL_PRE(mach_port_type);
      return;
   case 3204:
      CALL_PRE(mach_port_allocate);
      return;
   case 3205:
      CALL_PRE(mach_port_destroy);
      return;
   case 3206:
      CALL_PRE(mach_port_deallocate);
      return;
   case 3207:
      CALL_PRE(mach_port_get_refs);
      return;
   case 3208:
      CALL_PRE(mach_port_mod_refs);
      return;
   case 3211:
      CALL_PRE(mach_port_get_set_status);
      return;
   case 3212:
      CALL_PRE(mach_port_move_member);
      return;
   case 3213:
      CALL_PRE(mach_port_request_notification);
      return;
   case 3214:
      CALL_PRE(mach_port_insert_right);
      return;
   case 3215:
      CALL_PRE(mach_port_extract_right);
      return;
   case 3217:
      CALL_PRE(mach_port_get_attributes);
      return;
   case 3218:
      CALL_PRE(mach_port_set_attributes);
      return;
   case 3226:
      CALL_PRE(mach_port_insert_member);
      return;
   case 3227:
      CALL_PRE(mach_port_extract_member);
      return;

   case 3229:
      CALL_PRE(mach_port_set_context);
      return;

   case 3402:
      CALL_PRE(task_threads);
      return;
   case 3403:
      CALL_PRE(mach_ports_register);
      return;
   case 3404:
      CALL_PRE(mach_ports_lookup);
      return;

   case 3405:
      CALL_PRE(task_info);
      return;

   case 3407:
      CALL_PRE(task_suspend);
      return;
   case 3408:
      CALL_PRE(task_resume);
      return;
      
   case 3409:
      CALL_PRE(task_get_special_port);
      return;
   case 3411:
      CALL_PRE(thread_create);
      return;
   case 3412:
      CALL_PRE(thread_create_running);
      return;

   case 3414:
      CALL_PRE(task_get_exception_ports);
      return;
      
   case 3418:
      CALL_PRE(semaphore_create);
      return;
   case 3419:
      CALL_PRE(semaphore_destroy);
      return;
   case 3420:
      CALL_PRE(task_policy_set);
      return;
      
   case 3801:
      CALL_PRE(vm_allocate);
      return;
   case 3802:
      CALL_PRE(vm_deallocate);
      return;
   case 3803:
      CALL_PRE(vm_protect);
      return;
   case 3804:
      CALL_PRE(vm_inherit);
      return;
   case 3805:
      CALL_PRE(vm_read);
      return;
   case 3808:
      CALL_PRE(vm_copy);
      return;
   case 3809:
      CALL_PRE(vm_read_overwrite);
      return;
   case 3812:
      CALL_PRE(vm_map);
      return;
   case 3814:
      CALL_PRE(vm_remap);
      return;
   case 3825:
      CALL_PRE(mach_make_memory_entry_64);
      return;
   case 3830:
      CALL_PRE(vm_purgable_control);
      return;

   case 4800:
      CALL_PRE(mach_vm_allocate);
      return;
   case 4801:
      CALL_PRE(mach_vm_deallocate);
      return;
   case 4802:
      CALL_PRE(mach_vm_protect);
      return;
   case 4803:
      CALL_PRE(mach_vm_inherit);
      return;
   case 4804:
      CALL_PRE(mach_vm_read);
      return;
   case 4807:
      CALL_PRE(mach_vm_copy);
      return;
   case 4808:
      CALL_PRE(mach_vm_read_overwrite);
      return;
   case 4811:
      CALL_PRE(mach_vm_map);
      return;
   case 4813:
      CALL_PRE(mach_vm_remap);
      return;
   case 4815:
      CALL_PRE(mach_vm_region_recurse);
      return;
   case 4817:
      CALL_PRE(mach_make_memory_entry_64);
      return;
   case 4818:
      CALL_PRE(mach_vm_purgable_control);
      return;

   default:
      
      log_decaying("UNKNOWN task message [id %d, to %s, reply 0x%x]",
                   mh->msgh_id, name_for_port(mh->msgh_remote_port),
                   mh->msgh_reply_port);
      return;
   }
} 


PRE(mach_msg_thread)
{
   

   mach_msg_header_t *mh = (mach_msg_header_t *)ARG1;

   switch (mh->msgh_id) {
   case 3600: 
      CALL_PRE(thread_terminate);
      return;
   case 3603:
      CALL_PRE(thread_get_state);
      return;
   case 3605: 
      CALL_PRE(thread_suspend);
      return;
   case 3606:
      CALL_PRE(thread_resume);
      return;
   case 3612: 
      CALL_PRE(thread_info);
      return;
   case 3616: 
      CALL_PRE(thread_policy);
      return;
   case 3617: 
      CALL_PRE(thread_policy_set);
      return;
   default:
      
      VG_(printf)("UNKNOWN thread message [id %d, to %s, reply 0x%x]\n", 
                  mh->msgh_id, name_for_port(mh->msgh_request_port), 
                  mh->msgh_reply_port);
      return;
   }
}


static int is_thread_port(mach_port_t port)
{
   if (port == 0) return False;

   return VG_(lwpid_to_vgtid)(port) != VG_INVALID_THREADID;
}


static int is_task_port(mach_port_t port)
{
   if (port == 0) return False;

   if (port == vg_task_port) return True;

   return (0 == VG_(strncmp)("task-", name_for_port(port), 5));
}



PRE(mach_msg)
{
   mach_msg_header_t *mh = (mach_msg_header_t *)ARG1;
   mach_msg_option_t option = (mach_msg_option_t)ARG2;
   
   mach_msg_size_t rcv_size = (mach_msg_size_t)ARG4;
   
   size_t complex_header_size = 0;

   PRE_REG_READ7(long, "mach_msg", 
                 mach_msg_header_t*,"msg", mach_msg_option_t,"option", 
                 mach_msg_size_t,"send_size", mach_msg_size_t,"rcv_size", 
                 mach_port_t,"rcv_name", mach_msg_timeout_t,"timeout", 
                 mach_port_t,"notify");

   
   AFTER = NULL;

   
   *flags |= SfMayBlock;

   if (option & MACH_SEND_MSG) {
      
      PRE_MEM_READ("mach_msg(msg.msgh_bits)", 
                   (Addr)&mh->msgh_bits, sizeof(mh->msgh_bits));
      
      PRE_MEM_READ("mach_msg(msg.msgh_remote_port)", 
                   (Addr)&mh->msgh_remote_port, sizeof(mh->msgh_remote_port));
      PRE_MEM_READ("mach_msg(msg.msgh_local_port)", 
                   (Addr)&mh->msgh_local_port, sizeof(mh->msgh_local_port));
      
      PRE_MEM_READ("mach_msg(msg.msgh_id)", 
                   (Addr)&mh->msgh_id, sizeof(mh->msgh_id));
      
      if (mh->msgh_bits & MACH_MSGH_BITS_COMPLEX) {
         
         complex_header_size = export_complex_message(tid, mh);
      }

      
      
      
#     if DARWIN_VERS == DARWIN_10_10
      if (mh->msgh_bits & MACH_SEND_TRAILER) {
         log_decaying("UNKNOWN mach_msg unhandled MACH_SEND_TRAILER option");
      }
#     else
      vg_assert(! (mh->msgh_bits & MACH_SEND_TRAILER));
#     endif

      MACH_REMOTE = mh->msgh_remote_port;
      MACH_MSGH_ID = mh->msgh_id;
   }

   if (option & MACH_RCV_MSG) {
      
      PRE_MEM_WRITE("mach_msg(receive buffer)", (Addr)mh, rcv_size);
   }

   

   if (!(option & MACH_SEND_MSG)) {
      
      CALL_PRE(mach_msg_receive);
      return;
   }
   else if (mh->msgh_request_port == vg_host_port) {
      
      CALL_PRE(mach_msg_host);
      return;
   }
   else if (is_task_port(mh->msgh_request_port)) {
      
      CALL_PRE(mach_msg_task);
      return;
   }
   else if (mh->msgh_request_port == vg_bootstrap_port) {
      
      CALL_PRE(mach_msg_bootstrap);
      return;
   }
   else if (is_thread_port(mh->msgh_request_port)) {
      
      CALL_PRE(mach_msg_thread);
      return;
   }
   else {
      
      
#if 0
      Bool do_mapping_update = False;
      
      
      switch (mh->msgh_id) {
         
         case 29008: 

         
         case 29000:
         case 29822:
         case 29820: 
         case 29809: 
         case 29800: 
         case 29873:
         case 29876: 

         
         case 29624:
         case 29625:
         case 29506:
         case 29504:
         case 29509:
         case 29315:
         case 29236:
         case 29473:
         case 29268:
         case 29237: 
         case 29360:
         case 29301:
         case 29287:
         case 29568:
         case 29570: 
         case 29211:
         case 29569: 
         case 29374:
         case 29246:
         case 29239:
         case 29272:
            if (mh->msgh_id == 29820 ||
               mh->msgh_id == 29876)
               do_mapping_update = True;

            PRINT("com.apple.windowserver.active service mach_msg [id %d, to %s, reply 0x%x]",
               mh->msgh_id, name_for_port(mh->msgh_request_port),
               mh->msgh_reply_port);
            break;

         
         case 13024:
            PRINT("com.apple.FontServerservice mach_msg [id %d, to %s, reply 0x%x]",
               mh->msgh_id, name_for_port(mh->msgh_request_port),
               mh->msgh_reply_port);
            break;

         
         case 78945698:
         case 78945701:
         case 78945695: 
         case 78945694:
         case 78945700:
            if (mh->msgh_id == 78945695)
               do_mapping_update = False;
            PRINT("com.apple.system.notification_center mach_msg [id %d, to %s, reply 0x%x]",
               mh->msgh_id, name_for_port(mh->msgh_request_port),
               mh->msgh_reply_port);
            break;

         
         case 10000:
         case 10019:
         case 10002: 
         case 10003: 
         case 14007:
         case 13000:
         case 13001:
         case 13011:
         case 13016: 
            if (mh->msgh_id == 10002|| 
                mh->msgh_id == 10003)
               do_mapping_update = True;
            PRINT("com.apple.CoreServices.coreservicesd mach_msg [id %d, to %s, reply 0x%x]",
               mh->msgh_id, name_for_port(mh->msgh_request_port),
               mh->msgh_reply_port);
            break;

         
         case 118:
            PRINT("com.apple.system.logger mach_msg [id %d, to %s, reply 0x%x]",
               mh->msgh_id, name_for_port(mh->msgh_request_port),
               mh->msgh_reply_port);
            break;

         
         case 1999646836: 
            if (mh->msgh_id == 1999646836)
               do_mapping_update = True;
            PRINT("om.apple.coreservices.launchservicesd mach_msg [id %d, to %s, reply 0x%x]",
               mh->msgh_id, name_for_port(mh->msgh_request_port),
               mh->msgh_reply_port);
            break;

         
         case 33012:
            PRINT("com.apple.ocspd mach_msg [id %d, to %s, reply 0x%x]",
               mh->msgh_id, name_for_port(mh->msgh_request_port),
               mh->msgh_reply_port);

         default:
            
            do_mapping_update = True;
            PRINT("UNHANDLED mach_msg [id %d, to %s, reply 0x%x]",
               mh->msgh_id, name_for_port(mh->msgh_request_port),
               mh->msgh_reply_port);
      }

      
      if (do_mapping_update)
         AFTER = POST_FN(mach_msg_unhandled);
      else
         AFTER = POST_FN(mach_msg_unhandled_check);
#else
      AFTER = POST_FN(mach_msg_unhandled);
#endif

      
      
      return;
   }
}

POST(mach_msg)
{
   mach_msg_header_t *mh = (mach_msg_header_t *)ARG1;
   mach_msg_option_t option = (mach_msg_option_t)ARG2;

   if (option & MACH_RCV_MSG) {
      if (RES != 0) {
         
         
      } else {
         mach_msg_trailer_t *mt = 
             (mach_msg_trailer_t *)((Addr)mh + round_msg(mh->msgh_size));
           
         
         
         POST_MEM_WRITE((Addr)mh, 
                        round_msg(mh->msgh_size) + mt->msgh_trailer_size);
         
         if (mh->msgh_bits & MACH_MSGH_BITS_COMPLEX) {
             
             import_complex_message(tid, mh);
         }
      }
   }
   
   
   if (AFTER) {
      (*AFTER)(tid, arrghs, status);
   }
}


POST(mach_msg_unhandled)
{
   mach_msg_header_t *mh = (mach_msg_header_t *)ARG1;
   ML_(sync_mappings)("after", "mach_msg_receive-UNHANDLED", mh->msgh_id);
}

POST(mach_msg_unhandled_check)
{
   if (ML_(sync_mappings)("after", "mach_msg_receive (unhandled_check)", 0))
      PRINT("mach_msg_unhandled_check tid:%d missed mapping change()", tid);
}



PRE(mach_reply_port)
{
   PRINT("mach_reply_port()");
}

POST(mach_reply_port)
{
   record_named_port(tid, RES, MACH_PORT_RIGHT_RECEIVE, "reply-%p");
   PRINT("reply port %s", name_for_port(RES));
}


PRE(mach_thread_self)
{
   PRINT("mach_thread_self()");
}

POST(mach_thread_self)
{
   record_named_port(tid, RES, MACH_PORT_RIGHT_SEND, "thread-%p");
   PRINT("thread %#lx", RES);
}


PRE(mach_host_self)
{
   PRINT("mach_host_self()");
}

POST(mach_host_self)
{
   vg_host_port = RES;
   record_named_port(tid, RES, MACH_PORT_RIGHT_SEND, "mach_host_self()");
   PRINT("host %#lx", RES);
}


PRE(mach_task_self)
{
   PRINT("mach_task_self()");
}

POST(mach_task_self)
{
   vg_task_port = RES;
   record_named_port(tid, RES, MACH_PORT_RIGHT_SEND, "mach_task_self()");
   PRINT("task %#lx", RES);
}


PRE(syscall_thread_switch)
{
   PRINT("syscall_thread_switch(%s, %ld, %ld)",
      name_for_port(ARG1), ARG2, ARG3);
   PRE_REG_READ3(long, "syscall_thread_switch", 
                 mach_port_t,"thread", int,"option", natural_t,"timeout");

   *flags |= SfMayBlock;
}


PRE(semaphore_signal)
{
   PRINT("semaphore_signal(%s)", name_for_port(ARG1));
   PRE_REG_READ1(long, "semaphore_signal", semaphore_t,"semaphore");
}


PRE(semaphore_signal_all)
{
   PRINT("semaphore_signal_all(%s)", name_for_port(ARG1));
   PRE_REG_READ1(long, "semaphore_signal_all", semaphore_t,"semaphore");
}


PRE(semaphore_signal_thread)
{
   PRINT("semaphore_signal_thread(%s, %s)", 
         name_for_port(ARG1), name_for_port(ARG2));
   PRE_REG_READ2(long, "semaphore_signal_thread", 
                 semaphore_t,"semaphore", mach_port_t,"thread");
}


PRE(semaphore_wait)
{
   PRINT("semaphore_wait(%s)", name_for_port(ARG1));
   PRE_REG_READ1(long, "semaphore_signal", semaphore_t,"semaphore");

   *flags |= SfMayBlock;
}


PRE(semaphore_wait_signal)
{
   PRINT("semaphore_wait_signal(%s, %s)", 
         name_for_port(ARG1), name_for_port(ARG2));
   PRE_REG_READ2(long, "semaphore_wait_signal", 
                 semaphore_t,"wait_semaphore", 
                 semaphore_t,"signal_semaphore");

   *flags |= SfMayBlock;
}


PRE(semaphore_timedwait)
{
   PRINT("semaphore_timedwait(%s, %g seconds)",
         name_for_port(ARG1), ARG2+ARG3/1000000000.0);
   PRE_REG_READ3(long, "semaphore_wait_signal", 
                 semaphore_t,"semaphore", 
                 int,"wait_time_hi", 
                 int,"wait_time_lo");
   
   *flags |= SfMayBlock;
}


PRE(semaphore_timedwait_signal)
{
   PRINT("semaphore_wait_signal(wait %s, signal %s, %g seconds)",
         name_for_port(ARG1), name_for_port(ARG2), ARG3+ARG4/1000000000.0);
   PRE_REG_READ4(long, "semaphore_wait_signal", 
                 semaphore_t,"wait_semaphore", 
                 semaphore_t,"signal_semaphore", 
                 int,"wait_time_hi", 
                 int,"wait_time_lo");

   *flags |= SfMayBlock;
}


PRE(__semwait_signal)
{
   PRINT("__semwait_signal(wait %s, signal %s, %ld, %ld, %lds:%ldns)", 
         name_for_port(ARG1), name_for_port(ARG2), ARG3, ARG4, ARG5, ARG6);
   PRE_REG_READ6(long, "__semwait_signal", 
                 int,"cond_sem", int,"mutex_sem",
                 int,"timeout", int,"relative", 
                 vki_time_t,"tv_sec", int,"tv_nsec");

   *flags |= SfMayBlock;
}


PRE(__thread_selfid)
{
   PRINT("__thread_selfid ()");
   PRE_REG_READ0(vki_uint64_t, "__thread_selfid");
}

PRE(task_for_pid)
{
   PRINT("task_for_pid(%s, %ld, %#lx)", name_for_port(ARG1), ARG2, ARG3);
   PRE_REG_READ3(long, "task_for_pid", 
                 mach_port_t,"target", 
                 vki_pid_t, "pid", mach_port_t *,"task");
   PRE_MEM_WRITE("task_for_pid(task)", ARG3, sizeof(mach_port_t));
}

POST(task_for_pid)
{
   mach_port_t task;

   POST_MEM_WRITE(ARG3, sizeof(mach_port_t));

   task = *(mach_port_t *)ARG3;
   record_named_port(tid, task, MACH_PORT_RIGHT_SEND, "task-%p");
   PRINT("task 0x%x", task);
}


PRE(pid_for_task)
{
   PRINT("pid_for_task(%s, %#lx)", name_for_port(ARG1), ARG2);
   PRE_REG_READ2(long, "task_for_pid", mach_port_t,"task", vki_pid_t *,"pid");
   PRE_MEM_WRITE("task_for_pid(pid)", ARG2, sizeof(vki_pid_t));
}

POST(pid_for_task)
{
   vki_pid_t pid;

   POST_MEM_WRITE(ARG2, sizeof(vki_pid_t));

   pid = *(vki_pid_t *)ARG2;
   PRINT("pid %u", pid);
}


PRE(mach_timebase_info)
{
   PRINT("mach_timebase_info(%#lx)", ARG1);
   PRE_REG_READ1(long, "mach_timebase_info", void *,"info");
   PRE_MEM_WRITE("mach_timebase_info(info)", ARG1, sizeof(struct vki_mach_timebase_info));
}

POST(mach_timebase_info)
{
   POST_MEM_WRITE(ARG1, sizeof(struct vki_mach_timebase_info));
}


PRE(mach_wait_until)
{
#if VG_WORDSIZE == 8
   PRINT("mach_wait_until(%lu)", ARG1);
   PRE_REG_READ1(long, "mach_wait_until", 
                 unsigned long long,"deadline");
#else
   PRINT("mach_wait_until(%llu)", LOHI64(ARG1, ARG2));
   PRE_REG_READ2(long, "mach_wait_until", 
                 int,"deadline_hi", int,"deadline_lo");
#endif   
   *flags |= SfMayBlock;
}


PRE(mk_timer_create)
{
   PRINT("mk_timer_create()");
   PRE_REG_READ0(long, "mk_timer_create");
}

POST(mk_timer_create)
{
   record_named_port(tid, RES, MACH_PORT_RIGHT_SEND, "mk_timer-%p");
}


PRE(mk_timer_destroy)
{
   PRINT("mk_timer_destroy(%s)", name_for_port(ARG1));
   PRE_REG_READ1(long, "mk_timer_destroy", mach_port_t,"name");

   
   
   *flags &= ~SfMayBlock;
}

POST(mk_timer_destroy)
{
   
   record_port_destroy(ARG1);
}


PRE(mk_timer_arm)
{
#if VG_WORDSIZE == 8
   PRINT("mk_timer_arm(%s, %lu)", name_for_port(ARG1), ARG2);
   PRE_REG_READ2(long, "mk_timer_arm", mach_port_t,"name", 
                 unsigned long,"expire_time");
#else
   PRINT("mk_timer_arm(%s, %llu)", name_for_port(ARG1), LOHI64(ARG2, ARG3));
   PRE_REG_READ3(long, "mk_timer_arm", mach_port_t,"name", 
                 int,"expire_time_hi", int,"expire_time_lo");
#endif
}


PRE(mk_timer_cancel)
{
   PRINT("mk_timer_cancel(%s, %#lx)", name_for_port(ARG1), ARG2);
   PRE_REG_READ2(long, "mk_timer_cancel", 
                 mach_port_t,"name", Addr,"result_time");
   if (ARG2) {
      PRE_MEM_WRITE("mk_timer_cancel(result_time)", ARG2,sizeof(vki_uint64_t));
   }
}

POST(mk_timer_cancel)
{
   if (ARG2) {
      POST_MEM_WRITE(ARG2, sizeof(vki_uint64_t));
   }
}


PRE(iokit_user_client_trap)
{
   PRINT("iokit_user_client_trap(%s, %ld, %lx, %lx, %lx, %lx, %lx, %lx)",
         name_for_port(ARG1), ARG2, ARG3, ARG4, ARG5, ARG6, ARG7, ARG8);
   PRE_REG_READ8(kern_return_t, "iokit_user_client_trap", 
                 mach_port_t,connect, unsigned int,index, 
                 uintptr_t,p1, uintptr_t,p2, uintptr_t,p3, 
                 uintptr_t,p4, uintptr_t,p5, uintptr_t,p6);

   
   
}

POST(iokit_user_client_trap)
{
   ML_(sync_mappings)("after", "iokit_user_client_trap", ARG2);
}


PRE(swtch)
{
   PRINT("swtch ( )");
   PRE_REG_READ0(long, "swtch");

   *flags |= SfMayBlock;
}


PRE(swtch_pri)
{
   PRINT("swtch_pri ( %ld )", ARG1);
   PRE_REG_READ1(long, "swtch_pri", int,"pri");

   *flags |= SfMayBlock;
}


PRE(FAKE_SIGRETURN)
{
   

   PRINT("FAKE_SIGRETURN ( )");

   vg_assert(VG_(is_valid_tid)(tid));
   vg_assert(tid >= 1 && tid < VG_N_THREADS);
   vg_assert(VG_(is_running_thread)(tid));

   VG_(sigframe_destroy)(tid, True);

   *flags |= SfNoWriteResult;
   SET_STATUS_Success(0);

   
   *flags |= SfPollAfter;
}


PRE(sigreturn)
{

   PRINT("sigreturn ( uctx=%#lx, infostyle=%#lx )", ARG1, ARG2);

   vg_assert(VG_(is_valid_tid)(tid));
   vg_assert(tid >= 1 && tid < VG_N_THREADS);
   vg_assert(VG_(is_running_thread)(tid));

   if (ARG2 == VKI_UC_SET_ALT_STACK) {
      VG_(debugLog)(0, "syswrap-darwin",
                       "WARNING: Ignoring sigreturn( ..., "
                       "UC_SET_ALT_STACK );\n");
      SET_STATUS_Success(0);
      return;
   }
   if (ARG2 == VKI_UC_RESET_ALT_STACK) {
      
      VG_(debugLog)(0, "syswrap-darwin",
                       "WARNING: Ignoring sigreturn( ..., "
                       "UC_RESET_ALT_STACK );\n");
      SET_STATUS_Success(0);
      return;
   }

   
   VG_(debugLog)(0, "syswrap-darwin",
                    "WARNING: Ignoring sigreturn( uctx=..., 0 );\n");
   VG_(debugLog)(0, "syswrap-darwin",
                    "WARNING: Thread/program/Valgrind "
                    "will likely segfault now.\n");
   VG_(debugLog)(0, "syswrap-darwin",
                    "WARNING: Please file a bug report at "
                    "http://www.valgrind.org.\n");
   SET_STATUS_Failure( VKI_ENOSYS );
}



#if defined(VGA_x86)
static VexGuestX86SegDescr* alloc_zeroed_x86_LDT ( void )
{
   Int nbytes = VEX_GUEST_X86_LDT_NENT * sizeof(VexGuestX86SegDescr);
   return VG_(calloc)("syswrap-darwin.ldt", nbytes, 1);
}
#endif

PRE(thread_fast_set_cthread_self)
{
   PRINT("thread_fast_set_cthread_self ( %#lx )", ARG1);
   PRE_REG_READ1(void, "thread_fast_set_cthread_self", struct pthread_t *, self);

#if defined(VGA_x86)
   
   {
      VexGuestX86SegDescr *ldt;
      ThreadState *tst = VG_(get_ThreadState)(tid);
      ldt = (VexGuestX86SegDescr *)tst->arch.vex.guest_LDT;
      if (!ldt) {
         ldt = alloc_zeroed_x86_LDT();
         tst->arch.vex.guest_LDT = (HWord)ldt;
      }
      VG_(memset)(&ldt[6], 0, sizeof(ldt[6]));
      ldt[6].LdtEnt.Bits.LimitLow = 1;
      ldt[6].LdtEnt.Bits.LimitHi = 0;
      ldt[6].LdtEnt.Bits.BaseLow = ARG1 & 0xffff;
      ldt[6].LdtEnt.Bits.BaseMid = (ARG1 >> 16) & 0xff;
      ldt[6].LdtEnt.Bits.BaseHi = (ARG1 >> 24) & 0xff;
      ldt[6].LdtEnt.Bits.Pres = 1; 
      ldt[6].LdtEnt.Bits.Dpl = 3; 
      ldt[6].LdtEnt.Bits.Type = 0x12; 
      ldt[6].LdtEnt.Bits.Granularity = 1;  
      ldt[6].LdtEnt.Bits.Default_Big = 1;  
      
      tst->os_state.pthread = ARG1;
      tst->arch.vex.guest_GS = 0x37;

      
      
      
      
      
      
      SET_STATUS_from_SysRes(
         VG_(mk_SysRes_x86_darwin)(
            VG_DARWIN_SYSNO_CLASS(__NR_thread_fast_set_cthread_self),
            False, 0, 0x37
         )
      );
   }

#elif defined(VGA_amd64)
   
   {
      ThreadState *tst = VG_(get_ThreadState)(tid);
      tst->os_state.pthread = ARG1;
      tst->arch.vex.guest_GS_CONST = ARG1;
      
      
      SET_STATUS_from_SysRes(
         VG_(mk_SysRes_amd64_darwin)(
            VG_DARWIN_SYSNO_CLASS(__NR_thread_fast_set_cthread_self),
            False, 0, 0x60
         )
      );
   }

#else
#error unknown architecture
#endif
}



#if DARWIN_VERS >= DARWIN_10_7

PRE(getaudit_addr)
{
   PRINT("getaudit_addr(%#lx, %lu)", ARG1, ARG2);
   PRE_REG_READ1(void*, "auditinfo_addr", int, "length");
   PRE_MEM_WRITE("getaudit_addr(auditinfo_addr)", ARG1, ARG2);
}
POST(getaudit_addr)
{
   POST_MEM_WRITE(ARG1, ARG2);
}

PRE(psynch_mutexwait)
{
   PRINT("psynch_mutexwait(BOGUS)");
   *flags |= SfMayBlock;
}
POST(psynch_mutexwait)
{
}

PRE(psynch_mutexdrop)
{
   PRINT("psynch_mutexdrop(BOGUS)");
   *flags |= SfMayBlock;
}
POST(psynch_mutexdrop)
{
}

PRE(psynch_cvbroad)
{
   PRINT("psynch_cvbroad(BOGUS)");
}
POST(psynch_cvbroad)
{
}

PRE(psynch_cvsignal)
{
   PRINT("psynch_cvsignal(BOGUS)");
}
POST(psynch_cvsignal)
{
}

PRE(psynch_cvwait)
{
   PRINT("psynch_cvwait(BOGUS)");
   *flags |= SfMayBlock;
}
POST(psynch_cvwait)
{
}

PRE(psynch_rw_rdlock)
{
   PRINT("psynch_rw_rdlock(BOGUS)");
   *flags |= SfMayBlock;
}
POST(psynch_rw_rdlock)
{
}

PRE(psynch_rw_wrlock)
{
   PRINT("psynch_rw_wrlock(BOGUS)");
   *flags |= SfMayBlock;
}
POST(psynch_rw_wrlock)
{
}

PRE(psynch_rw_unlock)
{
   PRINT("psynch_rw_unlock(BOGUS)");
}
POST(psynch_rw_unlock)
{
}

PRE(psynch_cvclrprepost)
{
   PRINT("psynch_cvclrprepost(BOGUS)");
   *flags |= SfMayBlock;
}
POST(psynch_cvclrprepost)
{
}

#endif 




static void munge_wwl(UWord* a1, UWord* a2, ULong* a3,
                      UWord aRG1, UWord aRG2, UWord aRG3, UWord aRG4)
{
#  if defined(VGA_x86)
   *a1 = aRG1; *a2 = aRG2; *a3 = LOHI64(aRG3,aRG4);
#  else
   *a1 = aRG1; *a2 = aRG2; *a3 = aRG3;
#  endif
}

static void munge_wll(UWord* a1, ULong* a2, ULong* a3,
                      UWord aRG1, UWord aRG2, UWord aRG3,
                      UWord aRG4, UWord aRG5)
{
#  if defined(VGA_x86)
   *a1 = aRG1; *a2 = LOHI64(aRG2,aRG3); *a3 = LOHI64(aRG4,aRG5);
#  else
   *a1 = aRG1; *a2 = aRG2; *a3 = aRG3;
#  endif
}

static void munge_wwlw(UWord* a1, UWord* a2, ULong* a3, UWord* a4,
                       UWord aRG1, UWord aRG2, UWord aRG3,
                       UWord aRG4, UWord aRG5)
{
#  if defined(VGA_x86)
   *a1 = aRG1; *a2 = aRG2; *a3 = LOHI64(aRG3,aRG4); *a4 = aRG5;
#  else
   *a1 = aRG1; *a2 = aRG2; *a3 = aRG3; *a4 = aRG4;
#  endif
}

static void munge_wwwl(UWord* a1, UWord* a2, UWord* a3, ULong* a4,
                       UWord aRG1, UWord aRG2, UWord aRG3,
                       UWord aRG4, UWord aRG5)
{
#  if defined(VGA_x86)
   *a1 = aRG1; *a2 = aRG2; *a3 = aRG3; *a4 = LOHI64(aRG4,aRG5);
#  else
   *a1 = aRG1; *a2 = aRG2; *a3 = aRG3; *a4 = aRG4;
#  endif
}

static void munge_wllww(UWord* a1, ULong* a2, ULong* a3, UWord* a4, UWord* a5,
                        UWord aRG1, UWord aRG2, UWord aRG3,
                        UWord aRG4, UWord aRG5, UWord aRG6, UWord aRG7)
{
#  if defined(VGA_x86)
   *a1 = aRG1; *a2 = LOHI64(aRG2,aRG3); *a3 = LOHI64(aRG4,aRG5);
   *a4 = aRG6; *a5 = aRG7;
#  else
   *a1 = aRG1; *a2 = aRG2; *a3 = aRG3; *a4 = aRG4; *a5 = aRG5;
#  endif
}

static void munge_wwllww(UWord* a1, UWord* a2, ULong* a3,
                         ULong* a4, UWord* a5, UWord* a6,
                         UWord aRG1, UWord aRG2, UWord aRG3, UWord aRG4,
                         UWord aRG5, UWord aRG6, UWord aRG7, UWord aRG8)
{
#  if defined(VGA_x86)
   *a1 = aRG1; *a2 = aRG2;
   *a3 = LOHI64(aRG3,aRG4); *a4 = LOHI64(aRG5,aRG6);
   *a5 = aRG7; *a6 = aRG8;
#  else
   *a1 = aRG1; *a2 = aRG2; *a3 = aRG3; *a4 = aRG4; *a5 = aRG5; *a6 = aRG6;
#  endif
}

#if DARWIN_VERS >= DARWIN_10_8

PRE(kernelrpc_mach_vm_allocate_trap)
{
   UWord a1; UWord a2; ULong a3; UWord a4;
   munge_wwlw(&a1, &a2, &a3, &a4, ARG1, ARG2, ARG3, ARG4, ARG5);
   PRINT("kernelrpc_mach_vm_allocate_trap"
         "(target:%s, address:%p, size:%#llx, flags:%#lx)",
         name_for_port(a1), *(void**)a2, a3, a4);
   PRE_MEM_WRITE("kernelrpc_mach_vm_allocate_trap(address)",
                 a2, sizeof(void*));
}
POST(kernelrpc_mach_vm_allocate_trap)
{
   UWord a1; UWord a2; ULong a3; UWord a4;
   munge_wwlw(&a1, &a2, &a3, &a4, ARG1, ARG2, ARG3, ARG4, ARG5);
   PRINT("address:%p size:%#llx", *(void**)a2, a3);
   if (ML_(safe_to_deref)((void*)a2, sizeof(void*))) {
      POST_MEM_WRITE(a2, sizeof(void*));
   }
   if (a1 == mach_task_self()) {
#     if 1
      ML_(sync_mappings)("POST(kernelrpc_mach_vm_allocate_trap)", "??", 0);
#     else
      ML_(notify_core_and_tool_of_mmap)(
         *(UWord*)a2, a3,
         VKI_PROT_READ|VKI_PROT_WRITE, VKI_MAP_ANON, -1, 0);
#     endif
   }
}

PRE(kernelrpc_mach_vm_deallocate_trap)
{
   UWord a1; ULong a2; ULong a3;
   munge_wll(&a1, &a2, &a3, ARG1, ARG2, ARG3, ARG4, ARG5);
   PRINT("kernelrpc_mach_vm_deallocate_trap"
         "(target:%#lx, address:%#llx, size:%#llx)", a1, a2, a3);
}
POST(kernelrpc_mach_vm_deallocate_trap)
{
   UWord a1; ULong a2; ULong a3;
   munge_wll(&a1, &a2, &a3, ARG1, ARG2, ARG3, ARG4, ARG5);
   
   
   
   if (a3)
      ML_(notify_core_and_tool_of_munmap)(a2, a3);
}

PRE(kernelrpc_mach_vm_protect_trap)
{
   UWord a1; ULong a2; ULong a3; UWord a4; UWord a5;
   munge_wllww(&a1, &a2, &a3, &a4, &a5,
               ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7);
   PRINT("kernelrpc_mach_vm_protect_trap"
         "(task:%#lx, address:%#llx, size:%#llx,"
         " set_maximum:%#lx, new_prot:%#lx)", a1, a2, a3, a4, a5);
}
POST(kernelrpc_mach_vm_protect_trap)
{
   UWord a1; ULong a2; ULong a3; UWord a4; UWord a5;
   munge_wllww(&a1, &a2, &a3, &a4, &a5,
               ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7);
   if (a1 == mach_task_self()) {
      ML_(notify_core_and_tool_of_mprotect)((Addr)a2, (SizeT)a3, (Int)a5);
      VG_(di_notify_vm_protect)((Addr)a2, (SizeT)a3, (UInt)a5);
   }
}

PRE(kernelrpc_mach_port_allocate_trap)
{
   
   PRINT("kernelrpc_mach_port_allocate_trap(task:%#lx, mach_port_right_t:%#lx)",
         ARG1, ARG2);
   PRE_MEM_WRITE("kernelrpc_mach_port_allocate_trap(name)",
                 ARG3, sizeof(mach_port_name_t));
}
POST(kernelrpc_mach_port_allocate_trap)
{
   
   POST_MEM_WRITE(ARG3, sizeof(mach_port_name_t));
   PRINT(", name:%#x", *(mach_port_name_t*)ARG3);
   record_unnamed_port(tid, *(mach_port_name_t *)ARG3, ARG2);
}

PRE(kernelrpc_mach_port_destroy_trap)
{
   
   PRINT("kernelrpc_mach_port_destroy_trap(task:%#lx, name:%#lx)", ARG1, ARG2);
   record_port_destroy(ARG2);
}

PRE(kernelrpc_mach_port_deallocate_trap)
{
   
   PRINT("kernelrpc_mach_port_deallocate_trap(task:%#lx, name:%#lx ) FIXME",
         ARG1, ARG2);
}
POST(kernelrpc_mach_port_deallocate_trap)
{
   
}

PRE(kernelrpc_mach_port_mod_refs_trap)
{
   
   PRINT("kernelrpc_mach_port_mod_refs_trap"
         "(task:%#lx, name:%#lx, right:%#lx refs:%#lx) FIXME",
         ARG1, ARG2, ARG3, ARG4);
}

PRE(kernelrpc_mach_port_move_member_trap)
{
   
   PRINT("kernelrpc_mach_port_move_member_trap"
         "(task:%#lx, name:%#lx, after:%#lx ) FIXME",
         ARG1, ARG2, ARG3);
}

PRE(kernelrpc_mach_port_insert_right_trap)
{
   
   PRINT("kernelrpc_mach_port_insert_right_trap(FIXME)"
         "(%lx,%lx,%lx,%lx)", ARG1, ARG2, ARG3, ARG4);
}

PRE(kernelrpc_mach_port_insert_member_trap)
{
   
   PRINT("kernelrpc_mach_port_insert_member_trap(FIXME)"
         "(%lx,%lx,%lx)", ARG1, ARG2, ARG3);
}

PRE(kernelrpc_mach_port_extract_member_trap)
{
   
   PRINT("kernelrpc_mach_port_extract_member_trap(FIXME)"
         "(%lx,%lx,%lx)", ARG1, ARG2, ARG3);
}

PRE(iopolicysys)
{
   
   PRINT("iopolicysys(FIXME)(0x%lx, 0x%lx, 0x%lx)", ARG1, ARG2, ARG3);
   
}
POST(iopolicysys)
{
   
}

PRE(process_policy)
{
   
   PRINT("process_policy(FIXME)("
         "scope:0x%lx, action:0x%lx, policy:0x%lx, policy_subtype:0x%lx,"
         " attr:%lx, target_pid:%lx, target_threadid:%lx)",
         ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7);
   
}
POST(process_policy)
{
   
}

#endif 



#if DARWIN_VERS >= DARWIN_10_9

PRE(kernelrpc_mach_vm_map_trap)
{
   UWord a1; UWord a2; ULong a3; ULong a4; UWord a5; UWord a6;
   munge_wwllww(&a1, &a2, &a3, &a4, &a5, &a6,
                ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7, ARG8);
   PRINT("kernelrpc_mach_vm_map_trap"
         "(target:%#lx, address:%p, size:%#llx,"
         " mask:%#llx, flags:%#lx, cur_prot:%#lx)",
         a1, *(void**)a2, a3, a4, a5, a6);
   PRE_MEM_WRITE("kernelrpc_mach_vm_map_trap(address)", a2, sizeof(void*));
}
POST(kernelrpc_mach_vm_map_trap)
{
   UWord a1; UWord a2; ULong a3; ULong a4; UWord a5; UWord a6;
   munge_wwllww(&a1, &a2, &a3, &a4, &a5, &a6,
                ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7, ARG8);
   PRINT("-> address:%p", *(void**)a2);
   if (ML_(safe_to_deref)((void*)a2, sizeof(void*))) {
      POST_MEM_WRITE(a2, sizeof(void*));
   }
   ML_(notify_core_and_tool_of_mmap)(
      *(mach_vm_address_t*)a2, a3,
      VKI_PROT_READ|VKI_PROT_WRITE, VKI_MAP_ANON, -1, 0);
   
}

PRE(kernelrpc_mach_port_construct_trap)
{
   UWord a1; UWord a2; ULong a3; UWord a4;
   munge_wwlw(&a1, &a2, &a3, &a4, ARG1, ARG2, ARG3, ARG4, ARG5);
   PRINT("kernelrpc_mach_port_construct_trap(FIXME)"
         "(%lx,%lx,%llx,%lx)", a1, a2, a3, a4);
}

PRE(kernelrpc_mach_port_destruct_trap)
{
   UWord a1; UWord a2; UWord a3; ULong a4;
   munge_wwwl(&a1, &a2, &a3, &a4, ARG1, ARG2, ARG3, ARG4, ARG5);
   PRINT("kernelrpc_mach_port_destruct_trap(FIXME)"
         "(%lx,%lx,%lx,%llx)", a1, a2, a3, a4);
}

PRE(kernelrpc_mach_port_guard_trap)
{
   UWord a1; UWord a2; ULong a3; UWord a4;
   munge_wwlw(&a1, &a2, &a3, &a4, ARG1, ARG2, ARG3, ARG4, ARG5);
   PRINT("kernelrpc_mach_port_guard_trap(FIXME)"
         "(%lx,%lx,%llx,%lx)", a1, a2, a3, a4);
}

PRE(kernelrpc_mach_port_unguard_trap)
{
   
   UWord a1; UWord a2; ULong a3;
   munge_wwl(&a1, &a2, &a3, ARG1, ARG2, ARG3, ARG4);
   PRINT("kernelrpc_mach_port_unguard_trap(FIXME)"
         "(%lx,%lx,%llx)", a1, a2, a3);
}

#endif 



#if DARWIN_VERS >= DARWIN_10_10

PRE(necp_match_policy)
{
   
   
   PRINT("necp_match_policy(FIXME)(%lx,%ld, %lx)", ARG1, ARG2, ARG3);
   PRE_REG_READ3(int, "necp_match_policy", uint8_t*, parameters,
                 size_t, parameters_size, struct necp_aggregate_result*,
                 returned_result);
   PRE_MEM_READ("necp_match_policy(returned_result)", ARG1, ARG2);
}
POST(necp_match_policy)
{
   POST_MEM_WRITE(ARG3, sizeof(struct vki_necp_aggregate_result));
}

PRE(sysctlbyname)
{
   UWord name    = ARG1;
   UWord namelen = ARG2;
   UWord oldp    = ARG3;
   UWord oldlenp = ARG4;
   UWord newp    = ARG5;
   UWord newlen  = ARG6;

   PRINT( "sysctlbyname ( %#lx,%ld, %#lx,%#lx, %#lx,%ld )", 
          name, namelen, oldp, oldlenp, newp, newlen );

   PRE_REG_READ6(int, "sysctlbyname", char*, name, size_t, namelen,
                 void*, oldp, vki_size_t *, oldlenp, 
                 void*, newp, vki_size_t *, newlenp);

   
   PRE_MEM_READ("sysctlbyname(name)", name, namelen);

   if (VG_(clo_trace_syscalls)) {
      UInt i;
      const HChar* t_name = (const HChar*)name;
      VG_(printf)(" name: ");
      for (i = 0; i < namelen; i++) {
         VG_(printf)("%c", t_name[i]);
      }
      VG_(printf)(" ");
   }
  
   Bool is_kern_dot_userstack
      = False;

   common_PRE_sysctl( tid,status,flags,
                      is_kern_dot_userstack, oldp, oldlenp, newp, newlen );
}
POST(sysctlbyname)
{
   UWord oldp    = ARG3;
   UWord oldlenp = ARG4;

   if (SUCCESS || ERR == VKI_ENOMEM) {
      
      if (oldlenp) {
         POST_MEM_WRITE(oldlenp, sizeof(size_t));
      }
      if (oldp && oldlenp) {
         POST_MEM_WRITE(oldp, *(size_t*)oldlenp);
      }
   }
}

PRE(getattrlistbulk)
{
   
   
   
   
   PRINT("getattrlistbulk(FIXME)(%ld, %lx, %lx,%lu, %lu)",
         ARG1, ARG2, ARG3, ARG4, ARG5);
   PRE_REG_READ5(int, "getattrlistbulk", int, dirfd, void*, list,
                 void*, attributeBuffer, size_t, bufferSize,
                 uint32_t, options_lo32);
   PRE_MEM_READ("getattrlistbulk(alist)", ARG2, sizeof(struct vki_attrlist));
   PRE_MEM_WRITE("getattrlistbulk(attributeBuffer)", ARG3, ARG4);
}
POST(getattrlistbulk)
{
   
   
   
   
   vg_assert(SUCCESS);
   if (ARG3 && /* "at least one output element was written" */RES > 0)
      POST_MEM_WRITE(ARG3, ARG4);
}

PRE(readlinkat)
{
    Word  saved = SYSNO;
    
    PRINT("readlinkat ( %ld, %#lx(%s), %#lx, %llu )", ARG1,ARG2,(char*)ARG2,ARG3,(ULong)ARG4);
    PRE_REG_READ4(long, "readlinkat",
                  int, dfd, const char *, path, char *, buf, int, bufsiz);
    PRE_MEM_RASCIIZ( "readlinkat(path)", ARG2 );
    PRE_MEM_WRITE( "readlinkat(buf)", ARG3,ARG4 );
    
    {
        
        SET_STATUS_from_SysRes( VG_(do_syscall4)(saved, ARG1, ARG2, ARG3, ARG4));
    }
    
    if (SUCCESS && RES > 0)
        POST_MEM_WRITE( ARG3, RES );
}

PRE(bsdthread_ctl)
{
   
   
   PRINT("bsdthread_ctl(FIXME)(%lx,%lx,%lx,%lx)", ARG1, ARG2, ARG3, ARG4);
   PRE_REG_READ4(int, "bsdthreadctl",
                 void*, cmd, void*, arg1, void*, arg2, void*, arg3);
}

PRE(guarded_open_dprotected_np)
{
    PRINT("guarded_open_dprotected_np("
        "path:%#lx(%s), guard:%#lx, guardflags:%#lx, flags:%#lx, "
        "dpclass:%#lx, dpflags: %#lx) FIXME",
        ARG1, (char*)ARG1, ARG2, ARG3, ARG4, ARG5, ARG6);
}

PRE(guarded_write_np)
{
    PRINT("guarded_write_np(fd:%ld, guard:%#lx, cbuf:%#lx, nbyte:%llu) FIXME",
        ARG1, ARG2, ARG3, (ULong)ARG4);
}

PRE(guarded_pwrite_np)
{
    PRINT("guarded_pwrite_np(fd:%ld, guard:%#lx, buf:%#lx, nbyte:%llu, offset:%lld) FIXME",
        ARG1, ARG2, ARG3, (ULong)ARG4, (Long)ARG5);
}

PRE(guarded_writev_np)
{
    PRINT("guarded_writev_np(fd:%ld, guard:%#lx, iovp:%#lx, iovcnt:%llu) FIXME",
        ARG1, ARG2, ARG3, (ULong)ARG4);
}

#endif 




#define MACX_(sysno, name) \
           WRAPPER_ENTRY_X_(darwin, VG_DARWIN_SYSNO_INDEX(sysno), name) 

#define MACXY(sysno, name) \
           WRAPPER_ENTRY_XY(darwin, VG_DARWIN_SYSNO_INDEX(sysno), name)

#define _____(sysno) GENX_(sysno, sys_ni_syscall)  

const SyscallTableEntry ML_(syscall_table)[] = {
   MACX_(__NR_exit,        exit), 
   GENX_(__NR_fork,        sys_fork), 
   GENXY(__NR_read,        sys_read), 
   GENX_(__NR_write,       sys_write), 
   GENXY(__NR_open,        sys_open), 
   GENXY(__NR_close,       sys_close), 
   GENXY(__NR_wait4,       sys_wait4), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(8)),     
   GENX_(__NR_link,        sys_link), 
   GENX_(__NR_unlink,      sys_unlink), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(11)),    
   GENX_(__NR_chdir,       sys_chdir), 
   GENX_(__NR_fchdir,      sys_fchdir), 
   GENX_(__NR_mknod,       sys_mknod), 
   GENX_(__NR_chmod,       sys_chmod), 
   GENX_(__NR_chown,       sys_chown), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(17)),    
   MACXY(__NR_getfsstat,   getfsstat), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(19)),    
   GENX_(__NR_getpid,      sys_getpid),     
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(21)),    
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(22)),    
   GENX_(__NR_setuid,      sys_setuid), 
   GENX_(__NR_getuid,      sys_getuid), 
   GENX_(__NR_geteuid,     sys_geteuid), 
   MACX_(__NR_ptrace,      ptrace), 
   MACXY(__NR_recvmsg,     recvmsg), 
   MACX_(__NR_sendmsg,     sendmsg), 
   MACXY(__NR_recvfrom,    recvfrom), 
   MACXY(__NR_accept,      accept), 
   MACXY(__NR_getpeername, getpeername), 
   MACXY(__NR_getsockname, getsockname), 
   GENX_(__NR_access,      sys_access), 
   MACX_(__NR_chflags,     chflags), 
   MACX_(__NR_fchflags,    fchflags), 
   GENX_(__NR_sync,        sys_sync), 
   GENX_(__NR_kill,        sys_kill), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(38)),    
   GENX_(__NR_getppid,     sys_getppid), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(40)),    
   GENXY(__NR_dup,         sys_dup), 
   MACXY(__NR_pipe,        pipe), 
   GENX_(__NR_getegid,     sys_getegid), 
#if DARWIN_VERS >= DARWIN_10_7
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(44)),    
#else
#endif
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(45)),    
   MACXY(__NR_sigaction,   sigaction), 
   GENX_(__NR_getgid,      sys_getgid), 
   MACXY(__NR_sigprocmask, sigprocmask), 
   MACXY(__NR_getlogin,    getlogin), 
   MACXY(__NR_sigpending,  sigpending),
   GENXY(__NR_sigaltstack, sys_sigaltstack), 
   MACXY(__NR_ioctl,       ioctl), 
   GENX_(__NR_symlink,     sys_symlink),   
   GENX_(__NR_readlink,    sys_readlink), 
   GENX_(__NR_execve,      sys_execve), 
   GENX_(__NR_umask,       sys_umask),     
   GENX_(__NR_chroot,      sys_chroot), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(62)),    
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(63)),    
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(64)),    
   GENX_(__NR_msync,       sys_msync), 
   GENX_(__NR_vfork,       sys_fork),              
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(67)),    
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(68)),    
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(69)),    
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(70)),    
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(71)),    
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(72)),    
   GENXY(__NR_munmap,      sys_munmap), 
   GENXY(__NR_mprotect,    sys_mprotect), 
   GENX_(__NR_madvise,     sys_madvise), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(76)),    
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(77)),    
   GENXY(__NR_mincore,     sys_mincore), 
   GENXY(__NR_getgroups,   sys_getgroups), 
   GENX_(__NR_getpgrp,     sys_getpgrp), 
   GENX_(__NR_setpgid,     sys_setpgid), 
   GENXY(__NR_setitimer,   sys_setitimer), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(84)),    
   GENXY(__NR_getitimer,   sys_getitimer), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(87)),    
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(88)),    
   MACXY(__NR_getdtablesize, getdtablesize), 
   GENXY(__NR_dup2,        sys_dup2), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(91)),    
   MACXY(__NR_fcntl,       fcntl), 
   GENX_(__NR_select,      sys_select), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(94)),    
   GENX_(__NR_fsync,       sys_fsync), 
   GENX_(__NR_setpriority, sys_setpriority), 
   MACXY(__NR_socket,      socket), 
   MACX_(__NR_connect,     connect), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(99)),    
   GENX_(__NR_getpriority, sys_getpriority),   
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(101)),   
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(102)),   
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(103)),   
   MACX_(__NR_bind,        bind), 
   MACX_(__NR_setsockopt,  setsockopt), 
   MACX_(__NR_listen,      listen), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(107)),   
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(108)),   
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(109)),   
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(110)),   
   MACX_(__NR_sigsuspend,  sigsuspend),            
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(112)),   
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(113)),   
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(114)),   
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(115)),   
   GENXY(__NR_gettimeofday, sys_gettimeofday), 
   GENXY(__NR_getrusage,   sys_getrusage), 
   MACXY(__NR_getsockopt,  getsockopt), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(119)),   
   GENXY(__NR_readv,       sys_readv),        
   GENX_(__NR_writev,      sys_writev), 
   GENX_(__NR_fchown,      sys_fchown), 
   GENX_(__NR_fchmod,      sys_fchmod), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(125)),   
   GENX_(__NR_rename,      sys_rename), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(129)),   
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(130)),   
   GENX_(__NR_flock,       sys_flock), 
   MACXY(__NR_mkfifo,      mkfifo),
   MACX_(__NR_sendto,      sendto), 
   MACX_(__NR_shutdown,    shutdown), 
   MACXY(__NR_socketpair,  socketpair), 
   GENX_(__NR_mkdir,       sys_mkdir), 
   GENX_(__NR_rmdir,       sys_rmdir), 
   GENX_(__NR_utimes,      sys_utimes), 
   MACX_(__NR_futimes,     futimes), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(141)),   
   MACXY(__NR_gethostuuid, gethostuuid), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(143)),   
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(144)),   
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(145)),   
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(146)),   
   GENX_(__NR_setsid,      sys_setsid), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(148)),   
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(149)),   
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(150)),   
   GENXY(__NR_pread,       sys_pread64), 
   GENX_(__NR_pwrite,      sys_pwrite64), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(156)),   
   GENXY(__NR_statfs,      sys_statfs), 
   GENXY(__NR_fstatfs,     sys_fstatfs), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(160)),   
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(162)),   
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(163)),   
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(164)),   
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(166)),   
   MACX_(__NR_mount,       mount), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(168)),   
   MACXY(__NR_csops,       csops),                 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(170)),   
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(171)),   
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(172)),   
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(174)),   
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(175)),   
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(177)),   
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(178)),   
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(179)),   
   MACX_(__NR_kdebug_trace, kdebug_trace),     
   GENX_(__NR_setgid,      sys_setgid), 
   MACX_(__NR_setegid,     setegid), 
   MACX_(__NR_seteuid,     seteuid), 
   MACX_(__NR_sigreturn,   sigreturn), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(186)),   
#if DARWIN_VERS >= DARWIN_10_6
#else
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(187)),   
#endif
   GENXY(__NR_stat,        sys_newstat), 
   GENXY(__NR_fstat,       sys_newfstat), 
   GENXY(__NR_lstat,       sys_newlstat), 
   MACX_(__NR_pathconf,    pathconf), 
   MACX_(__NR_fpathconf,   fpathconf), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(193)),   
   GENXY(__NR_getrlimit,   sys_getrlimit), 
   GENX_(__NR_setrlimit,   sys_setrlimit), 
   MACXY(__NR_getdirentries, getdirentries), 
   MACXY(__NR_mmap,        mmap), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(198)),   
   MACX_(__NR_lseek,       lseek), 
   GENX_(__NR_truncate,    sys_truncate64),   
   GENX_(__NR_ftruncate,   sys_ftruncate64), 
   MACXY(__NR___sysctl,    __sysctl), 
   GENX_(__NR_mlock,       sys_mlock), 
   GENX_(__NR_munlock,     sys_munlock), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(213)),   
#if DARWIN_VERS >= DARWIN_10_6
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(214)),   
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(215)),   
#else
#endif
   MACXY(__NR_getattrlist, getattrlist),   
   MACX_(__NR_setattrlist, setattrlist), 
   MACXY(__NR_getdirentriesattr, getdirentriesattr), 
   MACX_(__NR_exchangedata,      exchangedata), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(224)),   
   GENX_(__NR_delete,      sys_unlink), 
#if DARWIN_VERS >= DARWIN_10_6
#else
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(228)),   
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(229)),   
#endif
   GENXY(__NR_poll,        sys_poll), 
   MACX_(__NR_watchevent,  watchevent), 
   MACXY(__NR_waitevent,   waitevent), 
   MACX_(__NR_modwatch,    modwatch), 
   MACXY(__NR_getxattr,    getxattr), 
   MACXY(__NR_fgetxattr,   fgetxattr), 
   MACX_(__NR_setxattr,    setxattr), 
   MACX_(__NR_fsetxattr,   fsetxattr), 
   MACX_(__NR_removexattr, removexattr), 
   MACX_(__NR_fremovexattr, fremovexattr), 
   MACXY(__NR_listxattr,   listxattr),    
   MACXY(__NR_flistxattr,  flistxattr), 
   MACXY(__NR_fsctl,       fsctl), 
   MACX_(__NR_initgroups,  initgroups), 
   MACXY(__NR_posix_spawn, posix_spawn), 
#if DARWIN_VERS >= DARWIN_10_6
#else
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(245)),   
#endif
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(246)),   
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(249)),   
   MACXY(__NR_semctl,      semctl), 
   MACX_(__NR_semget,      semget), 
   MACX_(__NR_semop,       semop), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(257)),   
   MACXY(__NR_shmat,       shmat), 
   MACXY(__NR_shmctl,      shmctl), 
   MACXY(__NR_shmdt,       shmdt), 
   MACX_(__NR_shmget,      shmget), 
   MACXY(__NR_shm_open,    shm_open), 
   MACXY(__NR_shm_unlink,  shm_unlink), 
   MACX_(__NR_sem_open,    sem_open), 
   MACX_(__NR_sem_close,   sem_close), 
   MACX_(__NR_sem_unlink,  sem_unlink), 
   MACX_(__NR_sem_wait,    sem_wait), 
   MACX_(__NR_sem_trywait, sem_trywait), 
   MACX_(__NR_sem_post,    sem_post), 
   
   
   MACXY(__NR_sem_init,    sem_init), 
   MACX_(__NR_sem_destroy, sem_destroy), 
   MACX_(__NR_open_extended,  open_extended),    
   MACXY(__NR_stat_extended,  stat_extended), 
   MACXY(__NR_lstat_extended, lstat_extended),   
   MACXY(__NR_fstat_extended, fstat_extended), 
   MACX_(__NR_chmod_extended, chmod_extended), 
   MACX_(__NR_fchmod_extended,fchmod_extended), 
   MACXY(__NR_access_extended,access_extended), 
   MACX_(__NR_settid,         settid), 
#if DARWIN_VERS >= DARWIN_10_8
   MACX_(__NR_gettid, gettid),  
#endif
#if DARWIN_VERS >= DARWIN_10_6
#else
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(296)),   
#endif
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(297)),   
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(298)),   
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(299)),   
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(300)),   
   MACXY(__NR_psynch_mutexwait, psynch_mutexwait), 
   MACXY(__NR_psynch_mutexdrop, psynch_mutexdrop), 
   MACXY(__NR_psynch_cvbroad,   psynch_cvbroad),   
   MACXY(__NR_psynch_cvsignal,  psynch_cvsignal),  
   MACXY(__NR_psynch_cvwait,    psynch_cvwait),    
   MACXY(__NR_psynch_rw_rdlock, psynch_rw_rdlock), 
   MACXY(__NR_psynch_rw_wrlock, psynch_rw_wrlock), 
   MACXY(__NR_psynch_rw_unlock, psynch_rw_unlock), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(309)),   
   MACXY(__NR_psynch_cvclrprepost, psynch_cvclrprepost), 
   MACXY(__NR_aio_return,     aio_return), 
   MACX_(__NR_aio_suspend,    aio_suspend), 
   MACX_(__NR_aio_error,      aio_error), 
   MACXY(__NR_aio_read,       aio_read), 
   MACX_(__NR_aio_write,      aio_write), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(321)),   

#if DARWIN_VERS >= DARWIN_10_8
   MACXY(__NR_iopolicysys, iopolicysys), 
   MACXY(__NR_process_policy, process_policy),
#else
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(322)),   
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(323)),   
#endif
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(326)),   
   MACX_(__NR_issetugid,               issetugid), 
   MACX_(__NR___pthread_kill,          __pthread_kill),
   MACX_(__NR___pthread_sigmask,       __pthread_sigmask), 
   MACX_(__NR___disable_threadsignal,  __disable_threadsignal), 
   MACX_(__NR___pthread_markcancel,    __pthread_markcancel), 
   MACX_(__NR___pthread_canceled,      __pthread_canceled),
   MACX_(__NR___semwait_signal,        __semwait_signal), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(335)),   
#if DARWIN_VERS >= DARWIN_10_6
   MACXY(__NR_proc_info,               proc_info),  
#endif
   MACXY(__NR_sendfile,    sendfile), 
   MACXY(__NR_stat64,      stat64), 
   MACXY(__NR_fstat64,     fstat64), 
   MACXY(__NR_lstat64,     lstat64),    
   MACXY(__NR_stat64_extended,  stat64_extended), 
   MACXY(__NR_lstat64_extended, lstat64_extended), 
   MACXY(__NR_fstat64_extended, fstat64_extended),
   MACXY(__NR_getdirentries64, getdirentries64), 
   MACXY(__NR_statfs64,    statfs64), 
   MACXY(__NR_fstatfs64,   fstatfs64), 
   MACXY(__NR_getfsstat64, getfsstat64), 
   MACX_(__NR___pthread_chdir,  __pthread_chdir),
   MACX_(__NR___pthread_fchdir, __pthread_fchdir),
   MACXY(__NR_auditon,     auditon), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(352)),   
#if DARWIN_VERS >= DARWIN_10_7
   MACXY(__NR_getaudit_addr, getaudit_addr),
#endif
   MACXY(__NR_bsdthread_create,     bsdthread_create),   
   MACX_(__NR_bsdthread_terminate,  bsdthread_terminate), 
   MACXY(__NR_kqueue,      kqueue), 
   MACXY(__NR_kevent,      kevent), 
   GENX_(__NR_lchown,      sys_lchown), 
   MACX_(__NR_bsdthread_register, bsdthread_register), 
   MACX_(__NR_workq_open,  workq_open), 
   MACXY(__NR_workq_ops,   workq_ops), 
#if DARWIN_VERS >= DARWIN_10_6
   MACXY(__NR_kevent64,      kevent64), 
#else
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(369)),   
#endif
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(370)),   
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(371)),   
#if DARWIN_VERS >= DARWIN_10_6
   MACX_(__NR___thread_selfid, __thread_selfid), 
#else
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(372)),   
#endif
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(373)),   
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(374)),   
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(375)),   
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(376)),   
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(377)),   
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(378)),   
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(379)),   
   MACX_(__NR___mac_syscall, __mac_syscall),
   
   GENXY(__NR_read_nocancel,     sys_read),
   GENX_(__NR_write_nocancel,    sys_write),
   GENXY(__NR_open_nocancel,     sys_open),
   GENXY(__NR_close_nocancel,    sys_close),
   GENXY(__NR_wait4_nocancel,    sys_wait4),   
   MACXY(__NR_recvmsg_nocancel,  recvmsg),
   MACX_(__NR_sendmsg_nocancel,  sendmsg),
   MACXY(__NR_recvfrom_nocancel, recvfrom),
   MACXY(__NR_accept_nocancel,   accept),
   GENX_(__NR_msync_nocancel,    sys_msync),
   MACXY(__NR_fcntl_nocancel,    fcntl),
   GENX_(__NR_select_nocancel,   sys_select),
   GENX_(__NR_fsync_nocancel,    sys_fsync),
   MACX_(__NR_connect_nocancel,  connect),
   MACX_(__NR_sigsuspend_nocancel, sigsuspend),
   GENXY(__NR_readv_nocancel,    sys_readv),
   GENX_(__NR_writev_nocancel,   sys_writev),
   MACX_(__NR_sendto_nocancel,   sendto),
   GENXY(__NR_pread_nocancel,    sys_pread64),
   GENX_(__NR_pwrite_nocancel,   sys_pwrite64),
   GENXY(__NR_poll_nocancel,     sys_poll),
   MACX_(__NR_sem_wait_nocancel, sem_wait), 
   MACX_(__NR___semwait_signal_nocancel, __semwait_signal), 
#if DARWIN_VERS >= DARWIN_10_6
   MACXY(__NR_fsgetpath, fsgetpath), 
   MACXY(__NR_audit_session_self, audit_session_self),
#endif
#if DARWIN_VERS >= DARWIN_10_9
    MACX_(__NR_fileport_makeport, fileport_makeport),
    MACX_(__NR_guarded_open_np, guarded_open_np),
    MACX_(__NR_guarded_close_np, guarded_close_np),
    MACX_(__NR_guarded_kqueue_np, guarded_kqueue_np),
    MACX_(__NR_change_fdguard_np, change_fdguard_np),
    MACX_(__NR_connectx, connectx),
    MACX_(__NR_disconnectx, disconnectx),
#endif
#if DARWIN_VERS >= DARWIN_10_10
   MACXY(__NR_sysctlbyname,        sysctlbyname),       
   MACXY(__NR_necp_match_policy,   necp_match_policy),  
   MACXY(__NR_getattrlistbulk,     getattrlistbulk),    
   MACX_(__NR_readlinkat,          readlinkat),         
   MACX_(__NR_bsdthread_ctl,       bsdthread_ctl),      
   MACX_(__NR_guarded_open_dprotected_np, guarded_open_dprotected_np),
   MACX_(__NR_guarded_write_np, guarded_write_np),
   MACX_(__NR_guarded_pwrite_np, guarded_pwrite_np),
   MACX_(__NR_guarded_writev_np, guarded_writev_np),
#endif
   MACX_(__NR_DARWIN_FAKE_SIGRETURN, FAKE_SIGRETURN)
};



const SyscallTableEntry ML_(mach_trap_table)[] = {
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(0)), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(1)), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(2)), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(3)), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(4)), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(5)), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(6)), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(7)), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(8)), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(9)), 

#  if DARWIN_VERS >= DARWIN_10_8
   MACXY(__NR_kernelrpc_mach_vm_allocate_trap, kernelrpc_mach_vm_allocate_trap),
#  else
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(10)), 
#  endif

   _____(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(11)), 

#  if DARWIN_VERS >= DARWIN_10_8
   MACXY(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(12), kernelrpc_mach_vm_deallocate_trap),
#  else
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(12)), 
#  endif

   _____(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(13)), 

#  if DARWIN_VERS >= DARWIN_10_8
   MACXY(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(14), kernelrpc_mach_vm_protect_trap),
#  endif

#  if DARWIN_VERS >= DARWIN_10_9
   MACXY(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(15), kernelrpc_mach_vm_map_trap),
#  endif

#  if DARWIN_VERS < DARWIN_10_8
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(14)), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(15)), 
#  endif

#  if DARWIN_VERS >= DARWIN_10_8
   MACXY(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(16), kernelrpc_mach_port_allocate_trap),
   MACX_(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(17), kernelrpc_mach_port_destroy_trap),
   MACX_(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(18), kernelrpc_mach_port_deallocate_trap),
   MACX_(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(19), kernelrpc_mach_port_mod_refs_trap),
   MACX_(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(20), kernelrpc_mach_port_move_member_trap),
   MACX_(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(21), kernelrpc_mach_port_insert_right_trap),
   MACX_(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(22), kernelrpc_mach_port_insert_member_trap),
   MACX_(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(23), kernelrpc_mach_port_extract_member_trap),
#  else
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(16)), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(17)), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(18)), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(19)), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(20)), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(21)), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(22)), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(23)), 
#  endif

#  if DARWIN_VERS >= DARWIN_10_9
   MACX_(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(24), kernelrpc_mach_port_construct_trap),
   MACX_(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(25), kernelrpc_mach_port_destruct_trap),
#  else
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(24)), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(25)),
#  endif

   MACXY(__NR_mach_reply_port, mach_reply_port), 
   MACXY(__NR_thread_self_trap, mach_thread_self), 
   MACXY(__NR_task_self_trap, mach_task_self), 
   MACXY(__NR_host_self_trap, mach_host_self), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(30)), 
   MACXY(__NR_mach_msg_trap, mach_msg), 
   MACX_(__NR_semaphore_signal_trap, semaphore_signal), 
   MACX_(__NR_semaphore_signal_all_trap, semaphore_signal_all), 
   MACX_(__NR_semaphore_signal_thread_trap, semaphore_signal_thread), 
   MACX_(__NR_semaphore_wait_trap, semaphore_wait), 
   MACX_(__NR_semaphore_wait_signal_trap, semaphore_wait_signal), 
   MACX_(__NR_semaphore_timedwait_trap, semaphore_timedwait), 
   MACX_(__NR_semaphore_timedwait_signal_trap, semaphore_timedwait_signal), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(40)),    

#if defined(VGA_x86)
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(42)), 
#else
#  if DARWIN_VERS >= DARWIN_10_9
   MACX_(__NR_kernelrpc_mach_port_guard_trap, kernelrpc_mach_port_guard_trap),
   MACX_(__NR_kernelrpc_mach_port_unguard_trap, kernelrpc_mach_port_unguard_trap),
#  endif
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(43)), 
#endif

   MACXY(__NR_task_for_pid, task_for_pid), 
   MACXY(__NR_pid_for_task, pid_for_task), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(47)), 
#if defined(VGA_x86)
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(50)), 
#else
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(48)), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(49)), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(50)), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(51)), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(52)), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(53)), 
#endif
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(54)), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(55)), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(56)), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(57)), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(58)), 
   MACX_(__NR_swtch_pri, swtch_pri), 
   MACX_(__NR_swtch, swtch),   
   MACX_(__NR_syscall_thread_switch, syscall_thread_switch), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(63)), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(64)), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(65)), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(66)), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(67)), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(68)), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(69)), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(70)), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(71)), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(72)), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(73)), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(74)), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(75)), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(76)), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(77)), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(78)), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(79)), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(80)),   
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(81)), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(82)), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(83)), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(84)), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(85)), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(86)), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(87)), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(88)), 
   MACXY(__NR_mach_timebase_info, mach_timebase_info), 
   MACX_(__NR_mach_wait_until, mach_wait_until), 
   MACXY(__NR_mk_timer_create, mk_timer_create), 
   MACXY(__NR_mk_timer_destroy, mk_timer_destroy), 
   MACX_(__NR_mk_timer_arm, mk_timer_arm), 
   MACXY(__NR_mk_timer_cancel, mk_timer_cancel), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(95)), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(96)), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(97)), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(98)), 
   _____(VG_DARWIN_SYSCALL_CONSTRUCT_MACH(99)), 
   MACXY(__NR_iokit_user_client_trap, iokit_user_client_trap), 
};



#if defined(VGA_x86)
const SyscallTableEntry ML_(mdep_trap_table)[] = {
   MACX_(__NR_thread_fast_set_cthread_self, thread_fast_set_cthread_self), 
};
#elif defined(VGA_amd64)
const SyscallTableEntry ML_(mdep_trap_table)[] = {
   MACX_(__NR_thread_fast_set_cthread_self, thread_fast_set_cthread_self), 
};
#else
#error unknown architecture
#endif

const UInt ML_(syscall_table_size) = 
            sizeof(ML_(syscall_table)) / sizeof(ML_(syscall_table)[0]);

const UInt ML_(mach_trap_table_size) = 
            sizeof(ML_(mach_trap_table)) / sizeof(ML_(mach_trap_table)[0]);

const UInt ML_(mdep_trap_table_size) = 
            sizeof(ML_(mdep_trap_table)) / sizeof(ML_(mdep_trap_table)[0]);

#endif 

