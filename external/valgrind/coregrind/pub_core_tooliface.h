

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

#ifndef __PUB_CORE_TOOLIFACE_H
#define __PUB_CORE_TOOLIFACE_H

#include "pub_tool_tooliface.h"


#define VG_TDICT_CALL(fn, args...) \
   ( vg_assert2(VG_(tdict).fn, \
                "you forgot to set VgToolInterface function '" #fn "'"), \
     VG_(tdict).fn(args) )

#define VG_TRACK(fn, args...) 			\
   do {						\
      if (VG_(tdict).track_##fn)		\
	 VG_(tdict).track_##fn(args);           \
   } while(0)



typedef
   struct {
      const HChar* name;
      const HChar* version;
      const HChar* description;
      const HChar* copyright_author;
      const HChar* bug_reports_to;
      UInt  avg_translation_sizeB;
   }
   VgDetails;

extern VgDetails VG_(details);


typedef
   struct {
      Bool libc_freeres;
      Bool core_errors;
      Bool tool_errors;
      Bool superblock_discards;
      Bool command_line_options;
      Bool client_requests;
      Bool syscall_wrapper;
      Bool sanity_checks;
      Bool print_stats;
      Bool info_location;
      Bool var_info;
      Bool malloc_replacement;
      Bool xml_output;
      Bool final_IR_tidy_pass;
   } 
   VgNeeds;

extern VgNeeds VG_(needs);


typedef struct {
   
   
   void  (*tool_pre_clo_init) (void);
   void  (*tool_post_clo_init)(void);
   IRSB* (*tool_instrument)   (VgCallbackClosure*,
                               IRSB*, 
                               const VexGuestLayout*, const VexGuestExtents*, 
                               const VexArchInfo*, IRType, IRType);
   void  (*tool_fini)         (Int);

   
   
   
   
   Bool  (*tool_eq_Error)                  (VgRes, const Error*, const Error*);
   void  (*tool_before_pp_Error)           (const Error*);
   void  (*tool_pp_Error)                  (const Error*);
   Bool  tool_show_ThreadIDs_for_errors;
   UInt  (*tool_update_extra)                (const Error*);
   Bool  (*tool_recognised_suppression)      (const HChar*, Supp*);
   Bool  (*tool_read_extra_suppression_info) (Int, HChar**, SizeT*, Int*,
                                              Supp*);
   Bool  (*tool_error_matches_suppression)   (const Error*, const Supp*);
   const HChar* (*tool_get_error_name)       (const Error*);
   SizeT (*tool_get_extra_suppression_info)  (const Error*,HChar*,Int);
   SizeT (*tool_print_extra_suppression_use) (const Supp*,HChar*,Int);
   void  (*tool_update_extra_suppression_use) (const Error*, const Supp*);

   
   void (*tool_discard_superblock_info)(Addr, VexGuestExtents);

   
   Bool (*tool_process_cmd_line_option)(const HChar*);
   void (*tool_print_usage)            (void);
   void (*tool_print_debug_usage)      (void);

   
   Bool (*tool_handle_client_request)(ThreadId, UWord*, UWord*);

   
   void (*tool_pre_syscall) (ThreadId, UInt, UWord*, UInt);
   void (*tool_post_syscall)(ThreadId, UInt, UWord*, UInt, SysRes);

   
   Bool (*tool_cheap_sanity_check)(void);
   Bool (*tool_expensive_sanity_check)(void);

   
   void (*tool_print_stats)(void);

   
   void (*tool_info_location)(Addr a);

   
   void* (*tool_malloc)              (ThreadId, SizeT);
   void* (*tool___builtin_new)       (ThreadId, SizeT);
   void* (*tool___builtin_vec_new)   (ThreadId, SizeT);
   void* (*tool_memalign)            (ThreadId, SizeT, SizeT);
   void* (*tool_calloc)              (ThreadId, SizeT, SizeT);
   void  (*tool_free)                (ThreadId, void*);
   void  (*tool___builtin_delete)    (ThreadId, void*);
   void  (*tool___builtin_vec_delete)(ThreadId, void*);
   void* (*tool_realloc)             (ThreadId, void*, SizeT);
   SizeT (*tool_malloc_usable_size)  (ThreadId, void*);
   SizeT tool_client_redzone_szB;

   
   IRSB* (*tool_final_IR_tidy_pass)  (IRSB*);

   
   

   
   void (*track_new_mem_startup)     (Addr, SizeT, Bool, Bool, Bool, ULong);
   void (*track_new_mem_stack_signal)(Addr, SizeT, ThreadId);
   void (*track_new_mem_brk)         (Addr, SizeT, ThreadId);
   void (*track_new_mem_mmap)        (Addr, SizeT, Bool, Bool, Bool, ULong);

   void (*track_copy_mem_remap)      (Addr src, Addr dst, SizeT);
   void (*track_change_mem_mprotect) (Addr, SizeT, Bool, Bool, Bool);
   void (*track_die_mem_stack_signal)(Addr, SizeT);
   void (*track_die_mem_brk)         (Addr, SizeT);
   void (*track_die_mem_munmap)      (Addr, SizeT);

   void VG_REGPARM(2) (*track_new_mem_stack_4_w_ECU)  (Addr,UInt);
   void VG_REGPARM(2) (*track_new_mem_stack_8_w_ECU)  (Addr,UInt);
   void VG_REGPARM(2) (*track_new_mem_stack_12_w_ECU) (Addr,UInt);
   void VG_REGPARM(2) (*track_new_mem_stack_16_w_ECU) (Addr,UInt);
   void VG_REGPARM(2) (*track_new_mem_stack_32_w_ECU) (Addr,UInt);
   void VG_REGPARM(2) (*track_new_mem_stack_112_w_ECU)(Addr,UInt);
   void VG_REGPARM(2) (*track_new_mem_stack_128_w_ECU)(Addr,UInt);
   void VG_REGPARM(2) (*track_new_mem_stack_144_w_ECU)(Addr,UInt);
   void VG_REGPARM(2) (*track_new_mem_stack_160_w_ECU)(Addr,UInt);
   void (*track_new_mem_stack_w_ECU)(Addr,SizeT,UInt);

   void VG_REGPARM(1) (*track_new_mem_stack_4)  (Addr);
   void VG_REGPARM(1) (*track_new_mem_stack_8)  (Addr);
   void VG_REGPARM(1) (*track_new_mem_stack_12) (Addr);
   void VG_REGPARM(1) (*track_new_mem_stack_16) (Addr);
   void VG_REGPARM(1) (*track_new_mem_stack_32) (Addr);
   void VG_REGPARM(1) (*track_new_mem_stack_112)(Addr);
   void VG_REGPARM(1) (*track_new_mem_stack_128)(Addr);
   void VG_REGPARM(1) (*track_new_mem_stack_144)(Addr);
   void VG_REGPARM(1) (*track_new_mem_stack_160)(Addr);
   void (*track_new_mem_stack)(Addr,SizeT);

   void VG_REGPARM(1) (*track_die_mem_stack_4)  (Addr);
   void VG_REGPARM(1) (*track_die_mem_stack_8)  (Addr);
   void VG_REGPARM(1) (*track_die_mem_stack_12) (Addr);
   void VG_REGPARM(1) (*track_die_mem_stack_16) (Addr);
   void VG_REGPARM(1) (*track_die_mem_stack_32) (Addr);
   void VG_REGPARM(1) (*track_die_mem_stack_112)(Addr);
   void VG_REGPARM(1) (*track_die_mem_stack_128)(Addr);
   void VG_REGPARM(1) (*track_die_mem_stack_144)(Addr);
   void VG_REGPARM(1) (*track_die_mem_stack_160)(Addr);
   void (*track_die_mem_stack)(Addr, SizeT);

   void (*track_ban_mem_stack)(Addr, SizeT);

   void (*track_pre_mem_read)       (CorePart, ThreadId, const HChar*, Addr, SizeT);
   void (*track_pre_mem_read_asciiz)(CorePart, ThreadId, const HChar*, Addr);
   void (*track_pre_mem_write)      (CorePart, ThreadId, const HChar*, Addr, SizeT);
   void (*track_post_mem_write)     (CorePart, ThreadId, Addr, SizeT);

   void (*track_pre_reg_read)  (CorePart, ThreadId, const HChar*, PtrdiffT, SizeT);
   void (*track_post_reg_write)(CorePart, ThreadId,               PtrdiffT, SizeT);
   void (*track_post_reg_write_clientcall_return)(ThreadId, PtrdiffT, SizeT,
                                                  Addr);

   void (*track_start_client_code)(ThreadId, ULong);
   void (*track_stop_client_code) (ThreadId, ULong);

   void (*track_pre_thread_ll_create)(ThreadId, ThreadId);
   void (*track_pre_thread_first_insn)(ThreadId);
   void (*track_pre_thread_ll_exit)  (ThreadId);

   void (*track_pre_deliver_signal) (ThreadId, Int sigNo, Bool);
   void (*track_post_deliver_signal)(ThreadId, Int sigNo);

} VgToolInterface;

extern VgToolInterface VG_(tdict);


Bool VG_(sanity_check_needs) ( const HChar** failmsg );

#endif   

