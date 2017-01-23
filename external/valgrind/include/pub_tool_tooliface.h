

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

#ifndef __PUB_TOOL_TOOLIFACE_H
#define __PUB_TOOL_TOOLIFACE_H

#include "pub_tool_errormgr.h"   
#include "libvex.h"              


extern void (*VG_(tl_pre_clo_init)) ( void );

#define VG_DETERMINE_INTERFACE_VERSION(pre_clo_init) \
   void (*VG_(tl_pre_clo_init)) ( void ) = pre_clo_init;


typedef 
   struct {
      Addr     nraddr; 
      Addr     readdr; 
      ThreadId tid;    
   }
   VgCallbackClosure;

extern void VG_(basic_tool_funcs)(
   
   
   void  (*post_clo_init)(void),

   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   IRSB*(*instrument)(VgCallbackClosure* closure, 
                      IRSB*              sb_in, 
                      const VexGuestLayout*  layout, 
                      const VexGuestExtents* vge, 
                      const VexArchInfo*     archinfo_host,
                      IRType             gWordTy, 
                      IRType             hWordTy),

   
   
   void  (*fini)(Int)
);


#define VG_DEFAULT_TRANS_SIZEB   172

extern void VG_(details_name)                  ( const HChar* name );
extern void VG_(details_version)               ( const HChar* version );
extern void VG_(details_description)           ( const HChar* description );
extern void VG_(details_copyright_author)      ( const HChar* copyright_author );

extern void VG_(details_avg_translation_sizeB) ( UInt size );

extern void VG_(details_bug_reports_to)   ( const HChar* bug_reports_to );


extern void VG_(needs_libc_freeres) ( void );

extern void VG_(needs_core_errors) ( void );


extern void VG_(needs_tool_errors) (
   
   
   
   
   
   
   
   Bool (*eq_Error)(VgRes res, const Error* e1, const Error* e2),

   
   
   
   
   
   
   
   
   void (*before_pp_Error)(const Error* err),

   
   void (*pp_Error)(const Error* err),

   
   Bool show_ThreadIDs_for_errors,

   
   
   
   
   
   
   
   UInt (*update_extra)(const Error* err),

   
   
   Bool (*recognised_suppression)(const HChar* name, Supp* su),

   
   
   
   
   
   Bool (*read_extra_suppression_info)(Int fd, HChar** bufpp, SizeT* nBufp,
                                       Int* lineno, Supp* su),

   
   
   
   Bool (*error_matches_suppression)(const Error* err, const Supp* su),

   
   
   
   const HChar* (*get_error_name)(const Error* err),

   
   
   
   
   
   
   
   SizeT (*print_extra_suppression_info)(const Error* err,
                                         HChar* buf, Int nBuf),

   
   
   
   SizeT (*print_extra_suppression_use)(const Supp* su,
                                        HChar* buf, Int nBuf),

   
   
   
   
   
   void (*update_extra_suppression_use)(const Error* err, const Supp* su)
);

extern void VG_(needs_superblock_discards) (
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   void (*discard_superblock_info)(Addr orig_addr, VexGuestExtents extents)
);

extern void VG_(needs_command_line_options) (
   
   
   
   
   
   
   
   
   
   
   
   Bool (*process_cmd_line_option)(const HChar* argv),

   
   void (*print_usage)(void),

   
   void (*print_debug_usage)(void)
);

extern void VG_(needs_client_requests) (
   
   
   
   
   
   
   
   
   
   
   
   Bool (*handle_client_request)(ThreadId tid, UWord* arg_block, UWord* ret)
);

extern void VG_(needs_syscall_wrapper) (
               void (* pre_syscall)(ThreadId tid, UInt syscallno,
                                    UWord* args, UInt nArgs),
               void (*post_syscall)(ThreadId tid, UInt syscallno,
                                    UWord* args, UInt nArgs, SysRes res)
);

extern void VG_(needs_sanity_checks) (
   Bool(*cheap_sanity_check)(void),
   Bool(*expensive_sanity_check)(void)
);

extern void VG_(needs_print_stats) (
   
   
   void (*print_stats)(void)
);

extern void VG_(needs_info_location) (
   
   void (*info_location)(Addr)
);

extern void VG_(needs_var_info) ( void );

extern void VG_(needs_malloc_replacement)(
   void* (*pmalloc)               ( ThreadId tid, SizeT n ),
   void* (*p__builtin_new)        ( ThreadId tid, SizeT n ),
   void* (*p__builtin_vec_new)    ( ThreadId tid, SizeT n ),
   void* (*pmemalign)             ( ThreadId tid, SizeT align, SizeT n ),
   void* (*pcalloc)               ( ThreadId tid, SizeT nmemb, SizeT size1 ),
   void  (*pfree)                 ( ThreadId tid, void* p ),
   void  (*p__builtin_delete)     ( ThreadId tid, void* p ),
   void  (*p__builtin_vec_delete) ( ThreadId tid, void* p ),
   void* (*prealloc)              ( ThreadId tid, void* p, SizeT new_size ),
   SizeT (*pmalloc_usable_size)   ( ThreadId tid, void* p), 
   SizeT client_malloc_redzone_szB
);

extern void VG_(needs_xml_output) ( void );

extern void VG_(needs_final_IR_tidy_pass) ( IRSB*(*final_tidy)(IRSB*) );



typedef
   enum { Vg_CoreStartup=1, Vg_CoreSignal, Vg_CoreSysCall,
          
          
          
          
          
          Vg_CoreSysCallArgInMem,  
          Vg_CoreTranslate, Vg_CoreClientReq
   } CorePart;

void VG_(track_new_mem_startup)     (void(*f)(Addr a, SizeT len,
                                              Bool rr, Bool ww, Bool xx,
                                              ULong di_handle));
void VG_(track_new_mem_stack_signal)(void(*f)(Addr a, SizeT len, ThreadId tid));
void VG_(track_new_mem_brk)         (void(*f)(Addr a, SizeT len, ThreadId tid));
void VG_(track_new_mem_mmap)        (void(*f)(Addr a, SizeT len,
                                              Bool rr, Bool ww, Bool xx,
                                              ULong di_handle));

void VG_(track_copy_mem_remap)      (void(*f)(Addr from, Addr to, SizeT len));
void VG_(track_change_mem_mprotect) (void(*f)(Addr a, SizeT len,
                                              Bool rr, Bool ww, Bool xx));
void VG_(track_die_mem_stack_signal)(void(*f)(Addr a, SizeT len));
void VG_(track_die_mem_brk)         (void(*f)(Addr a, SizeT len));
void VG_(track_die_mem_munmap)      (void(*f)(Addr a, SizeT len));

void VG_(track_new_mem_stack_4_w_ECU)  (VG_REGPARM(2) void(*f)(Addr new_ESP, UInt ecu));
void VG_(track_new_mem_stack_8_w_ECU)  (VG_REGPARM(2) void(*f)(Addr new_ESP, UInt ecu));
void VG_(track_new_mem_stack_12_w_ECU) (VG_REGPARM(2) void(*f)(Addr new_ESP, UInt ecu));
void VG_(track_new_mem_stack_16_w_ECU) (VG_REGPARM(2) void(*f)(Addr new_ESP, UInt ecu));
void VG_(track_new_mem_stack_32_w_ECU) (VG_REGPARM(2) void(*f)(Addr new_ESP, UInt ecu));
void VG_(track_new_mem_stack_112_w_ECU)(VG_REGPARM(2) void(*f)(Addr new_ESP, UInt ecu));
void VG_(track_new_mem_stack_128_w_ECU)(VG_REGPARM(2) void(*f)(Addr new_ESP, UInt ecu));
void VG_(track_new_mem_stack_144_w_ECU)(VG_REGPARM(2) void(*f)(Addr new_ESP, UInt ecu));
void VG_(track_new_mem_stack_160_w_ECU)(VG_REGPARM(2) void(*f)(Addr new_ESP, UInt ecu));
void VG_(track_new_mem_stack_w_ECU)                  (void(*f)(Addr a, SizeT len,
                                                                       UInt ecu));

void VG_(track_new_mem_stack_4)  (VG_REGPARM(1) void(*f)(Addr new_ESP));
void VG_(track_new_mem_stack_8)  (VG_REGPARM(1) void(*f)(Addr new_ESP));
void VG_(track_new_mem_stack_12) (VG_REGPARM(1) void(*f)(Addr new_ESP));
void VG_(track_new_mem_stack_16) (VG_REGPARM(1) void(*f)(Addr new_ESP));
void VG_(track_new_mem_stack_32) (VG_REGPARM(1) void(*f)(Addr new_ESP));
void VG_(track_new_mem_stack_112)(VG_REGPARM(1) void(*f)(Addr new_ESP));
void VG_(track_new_mem_stack_128)(VG_REGPARM(1) void(*f)(Addr new_ESP));
void VG_(track_new_mem_stack_144)(VG_REGPARM(1) void(*f)(Addr new_ESP));
void VG_(track_new_mem_stack_160)(VG_REGPARM(1) void(*f)(Addr new_ESP));
void VG_(track_new_mem_stack)                  (void(*f)(Addr a, SizeT len));

void VG_(track_die_mem_stack_4)  (VG_REGPARM(1) void(*f)(Addr die_ESP));
void VG_(track_die_mem_stack_8)  (VG_REGPARM(1) void(*f)(Addr die_ESP));
void VG_(track_die_mem_stack_12) (VG_REGPARM(1) void(*f)(Addr die_ESP));
void VG_(track_die_mem_stack_16) (VG_REGPARM(1) void(*f)(Addr die_ESP));
void VG_(track_die_mem_stack_32) (VG_REGPARM(1) void(*f)(Addr die_ESP));
void VG_(track_die_mem_stack_112)(VG_REGPARM(1) void(*f)(Addr die_ESP));
void VG_(track_die_mem_stack_128)(VG_REGPARM(1) void(*f)(Addr die_ESP));
void VG_(track_die_mem_stack_144)(VG_REGPARM(1) void(*f)(Addr die_ESP));
void VG_(track_die_mem_stack_160)(VG_REGPARM(1) void(*f)(Addr die_ESP));
void VG_(track_die_mem_stack)                  (void(*f)(Addr a, SizeT len));

void VG_(track_ban_mem_stack)      (void(*f)(Addr a, SizeT len));

void VG_(track_pre_mem_read)       (void(*f)(CorePart part, ThreadId tid,
                                             const HChar* s, Addr a, SizeT size));
void VG_(track_pre_mem_read_asciiz)(void(*f)(CorePart part, ThreadId tid,
                                             const HChar* s, Addr a));
void VG_(track_pre_mem_write)      (void(*f)(CorePart part, ThreadId tid,
                                             const HChar* s, Addr a, SizeT size));
void VG_(track_post_mem_write)     (void(*f)(CorePart part, ThreadId tid,
                                             Addr a, SizeT size));

void VG_(track_pre_reg_read)  (void(*f)(CorePart part, ThreadId tid,
                                        const HChar* s, PtrdiffT guest_state_offset,
                                        SizeT size));
void VG_(track_post_reg_write)(void(*f)(CorePart part, ThreadId tid,
                                        PtrdiffT guest_state_offset,
                                        SizeT size));

void VG_(track_post_reg_write_clientcall_return)(
      void(*f)(ThreadId tid, PtrdiffT guest_state_offset, SizeT size, Addr f));



void VG_(track_start_client_code)(
        void(*f)(ThreadId tid, ULong blocks_dispatched)
     );
void VG_(track_stop_client_code)(
        void(*f)(ThreadId tid, ULong blocks_dispatched)
     );


void VG_(track_pre_thread_ll_create) (void(*f)(ThreadId tid, ThreadId child));
void VG_(track_pre_thread_first_insn)(void(*f)(ThreadId tid));
void VG_(track_pre_thread_ll_exit)   (void(*f)(ThreadId tid));


void VG_(track_pre_deliver_signal) (void(*f)(ThreadId tid, Int sigNo,
                                             Bool alt_stack));
void VG_(track_post_deliver_signal)(void(*f)(ThreadId tid, Int sigNo));

#endif   

