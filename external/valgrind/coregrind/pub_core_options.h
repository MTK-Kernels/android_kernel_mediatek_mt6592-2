

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

#ifndef __PUB_CORE_OPTIONS_H
#define __PUB_CORE_OPTIONS_H

#include "pub_tool_options.h"
#include "pub_core_xarray.h"

extern Bool  VG_(clo_error_limit);
extern Int   VG_(clo_error_exitcode);

extern HChar *VG_(clo_error_markers)[2];

typedef 
   enum { 
      Vg_VgdbNo,   
      Vg_VgdbYes,  
      Vg_VgdbFull, 
                   
                   
   } 
   VgVgdb;
extern VgVgdb VG_(clo_vgdb);
extern Int VG_(clo_vgdb_poll);

typedef
   enum {                       
      VgdbStopAt_Startup,       
      VgdbStopAt_Exit,          
      VgdbStopAt_ValgrindAbExit 
   }
   VgdbStopAt;
#define VgdbStopAt2S(a) (1 << (a))
#define VgdbStopAtiS(a,s) ((s) & VgdbStopAt2S(a))
extern UInt VG_(clo_vgdb_stop_at); 

extern const HChar *VG_(clo_vgdb_prefix);

extern Bool  VG_(clo_vgdb_shadow_registers);

extern Bool  VG_(clo_db_attach);
extern const HChar* VG_(clo_db_command);
extern Int   VG_(clo_gen_suppressions);
extern Int   VG_(clo_sanity_level);
extern Bool  VG_(clo_demangle);
extern const HChar* VG_(clo_soname_synonyms);
extern Bool  VG_(clo_trace_children);
extern const HChar* VG_(clo_trace_children_skip);
extern const HChar* VG_(clo_trace_children_skip_by_arg);
extern Bool  VG_(clo_child_silent_after_fork);

extern const HChar* VG_(clo_log_fname_expanded);
extern const HChar* VG_(clo_xml_fname_expanded);

extern Bool  VG_(clo_time_stamp);

extern Int   VG_(clo_input_fd);

extern Bool  VG_(clo_default_supp);

extern XArray *VG_(clo_suppressions);

extern XArray *VG_(clo_fullpath_after);

extern const HChar* VG_(clo_extra_debuginfo_path);

extern const HChar* VG_(clo_debuginfo_server);

extern Bool VG_(clo_allow_mismatched_debuginfo);

extern UChar VG_(clo_trace_flags);

extern Bool  VG_(clo_profyle_sbs);
extern UChar VG_(clo_profyle_flags);
extern ULong VG_(clo_profyle_interval);

extern Int   VG_(clo_trace_notbelow);
extern Int   VG_(clo_trace_notabove);
extern Bool  VG_(clo_trace_syscalls);
extern Bool  VG_(clo_trace_signals);
extern Bool  VG_(clo_trace_symtab);
extern const HChar* VG_(clo_trace_symtab_patt);
extern Bool  VG_(clo_trace_cfi);
extern Bool  VG_(clo_debug_dump_syms);
extern Bool  VG_(clo_debug_dump_line);
extern Bool  VG_(clo_debug_dump_frames);
extern Bool  VG_(clo_trace_redir);
enum FairSchedType { disable_fair_sched, enable_fair_sched, try_fair_sched };
extern enum FairSchedType VG_(clo_fair_sched);
extern Bool  VG_(clo_trace_sched);
extern Bool  VG_(clo_profile_heap);
#define MAX_REDZONE_SZB 128
#define MAX_CLO_REDZONE_SZB 4096
#define CORE_REDZONE_DEFAULT_SZB 4
extern Int VG_(clo_core_redzone_size);
extern Int VG_(clo_redzone_size);
extern Int   VG_(clo_dump_error);

typedef
   enum {
      SimHint_lax_ioctls,
      SimHint_fuse_compatible,
      SimHint_enable_outer,
      SimHint_no_inner_prefix,
      SimHint_no_nptl_pthread_stackcache
   }
   SimHint;

#define SimHint2S(a) (1 << (a))
#define SimHintiS(h,s) ((s) & SimHint2S(h))
extern UInt VG_(clo_sim_hints);

extern Bool VG_(clo_sym_offsets);
extern Bool VG_(clo_read_inline_info);
extern Bool VG_(clo_read_var_info);
extern const HChar* VG_(clo_prefix_to_strip);

/* An array of strings harvested from --require-text-symbol= 
   flags.

   Each string specifies a pair: a soname pattern and a text symbol
   name pattern, separated by a colon.  The patterns can be written
   using the normal "?" and "*" wildcards.  For example:
   ":*libc.so*:foo?bar".

   These flags take effect when reading debuginfo from objects.  If an
   object is loaded and the object's soname matches the soname
   component of one of the specified pairs, then Valgrind will examine
   all the text symbol names in the object.  If none of them match the
   symbol name component of that same specification, then the run is
   aborted, with an error message.

   The purpose of this is to support reliable usage of marked-up
   libraries.  For example, suppose we have a version of GCC's
   libgomp.so which has been marked up with annotations to support
   Helgrind.  It is only too easy and confusing to load the 'wrong'
   libgomp.so into the application.  So the idea is: add a text symbol
   in the marked-up library (eg), "annotated_for_helgrind_3_6", and
   then give the flag

     --require-text-symbol=:*libgomp*so*:annotated_for_helgrind_3_6

   so that when libgomp.so is loaded, we scan the symbol table, and if
   the symbol isn't present the run is aborted, rather than continuing
   silently with the un-marked-up library.  Note that you should put
   the entire flag in quotes to stop shells messing up the * and ?
   wildcards. */
extern XArray *VG_(clo_req_tsyms);

extern Bool  VG_(clo_track_fds);

extern Bool  VG_(clo_run_libc_freeres);

extern Bool VG_(clo_show_emwarns);

extern Word VG_(clo_max_stackframe);
extern Word VG_(clo_main_stacksize);

#define MAX_THREADS_DEFAULT 500
extern UInt VG_(clo_max_threads);

extern Int VG_(clo_merge_recursive_frames);

extern UInt VG_(clo_num_transtab_sectors);

extern UInt VG_(clo_avg_transtab_entry_size);

extern Addr VG_(clo_aspacem_minAddr);

extern Word VG_(clo_valgrind_stacksize);

extern Bool VG_(clo_wait_for_gdb);

typedef 
   enum { 
      Vg_SmcNone,  
      Vg_SmcStack, 
                   
      Vg_SmcAll,   
      Vg_SmcAllNonFile 
                   
   } 
   VgSmc;

extern VgSmc VG_(clo_smc_check);

typedef
   enum {
      KernelVariant_bproc,
      KernelVariant_android_no_hw_tls,
      KernelVariant_android_gpu_sgx5xx,
      KernelVariant_android_gpu_adreno3xx
   }
   KernelVariant;
#define KernelVariant2S(v) (1 << (v))
#define KernelVariantiS(v,s) ((s) & KernelVariant2S(v))
extern UInt VG_(clo_kernel_variant);

extern Bool VG_(clo_dsymutil);

extern Bool VG_(should_we_trace_this_child) ( const HChar* child_exe_name,
                                              const HChar** child_argv );

extern Bool VG_(clo_sigill_diag);

extern UInt VG_(clo_unw_stack_scan_thresh);

extern UInt VG_(clo_unw_stack_scan_frames);

extern UInt VG_(clo_resync_filter);

#endif   

