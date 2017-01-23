

/*
   This file is part of Valgrind, a dynamic binary instrumentation
   framework.

   Copyright (C) 2004-2013 OpenWorks LLP
      info@open-works.net

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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   The GNU General Public License is contained in the file COPYING.

   Neither the names of the U.S. Department of Energy nor the
   University of California nor the names of its contributors may be
   used to endorse or promote products derived from this software
   without prior written permission.
*/


#ifndef __VEX_GUEST_X86_DEFS_H
#define __VEX_GUEST_X86_DEFS_H

#include "libvex_basictypes.h"
#include "libvex_guest_x86.h"           
#include "libvex_emnote.h"              
#include "guest_generic_bb_to_IR.h"     


extern
DisResult disInstr_X86 ( IRSB*        irbb,
                         Bool         (*resteerOkFn) ( void*, Addr ),
                         Bool         resteerCisOk,
                         void*        callback_opaque,
                         const UChar* guest_code,
                         Long         delta,
                         Addr         guest_IP,
                         VexArch      guest_arch,
                         const VexArchInfo* archinfo,
                         const VexAbiInfo*  abiinfo,
                         VexEndness   host_endness,
                         Bool         sigill_diag );

extern
IRExpr* guest_x86_spechelper ( const HChar* function_name,
                               IRExpr** args,
                               IRStmt** precedingStmts,
                               Int      n_precedingStmts );

extern 
Bool guest_x86_state_requires_precise_mem_exns ( Int, Int,
                                                 VexRegisterUpdates );

extern
VexGuestLayout x86guest_layout;




extern UInt  x86g_calculate_eflags_all ( 
                UInt cc_op, UInt cc_dep1, UInt cc_dep2, UInt cc_ndep 
             );

VEX_REGPARM(3)
extern UInt  x86g_calculate_eflags_c ( 
                UInt cc_op, UInt cc_dep1, UInt cc_dep2, UInt cc_ndep 
             );

extern UInt  x86g_calculate_condition ( 
                UInt cond, 
                UInt cc_op, 
                UInt cc_dep1, UInt cc_dep2, UInt cc_ndep 
             );

extern UInt  x86g_calculate_FXAM ( UInt tag, ULong dbl );

extern ULong x86g_calculate_RCR ( 
                UInt arg, UInt rot_amt, UInt eflags_in, UInt sz 
             );
extern ULong x86g_calculate_RCL ( 
                UInt arg, UInt rot_amt, UInt eflags_in, UInt sz 
             );

extern UInt x86g_calculate_daa_das_aaa_aas ( UInt AX_and_flags, UInt opcode );

extern UInt x86g_calculate_aad_aam ( UInt AX_and_flags, UInt opcode );

extern ULong x86g_check_fldcw ( UInt fpucw );

extern UInt  x86g_create_fpucw ( UInt fpround );

extern ULong x86g_check_ldmxcsr ( UInt mxcsr );

extern UInt  x86g_create_mxcsr ( UInt sseround );


extern 
ULong x86g_use_seg_selector ( HWord ldt, HWord gdt, 
                              UInt seg_selector, UInt virtual_addr );

extern ULong x86g_calculate_mmx_pmaddwd  ( ULong, ULong );
extern ULong x86g_calculate_mmx_psadbw   ( ULong, ULong );



extern ULong x86g_dirtyhelper_loadF80le  ( Addr );

extern void  x86g_dirtyhelper_storeF80le ( Addr, ULong );

extern void  x86g_dirtyhelper_CPUID_sse0 ( VexGuestX86State* );
extern void  x86g_dirtyhelper_CPUID_mmxext ( VexGuestX86State* );
extern void  x86g_dirtyhelper_CPUID_sse1 ( VexGuestX86State* );
extern void  x86g_dirtyhelper_CPUID_sse2 ( VexGuestX86State* );

extern void  x86g_dirtyhelper_FINIT ( VexGuestX86State* );

extern void  x86g_dirtyhelper_FXSAVE ( VexGuestX86State*, HWord );
extern void  x86g_dirtyhelper_FSAVE  ( VexGuestX86State*, HWord );
extern void  x86g_dirtyhelper_FSTENV ( VexGuestX86State*, HWord );

extern ULong x86g_dirtyhelper_RDTSC ( void );

extern UInt x86g_dirtyhelper_IN  ( UInt portno, UInt sz );
extern void x86g_dirtyhelper_OUT ( UInt portno, UInt data, 
                                   UInt sz );

extern void x86g_dirtyhelper_SxDT ( void* address,
                                    UInt op  );

extern VexEmNote
            x86g_dirtyhelper_FXRSTOR ( VexGuestX86State*, HWord );

extern VexEmNote
            x86g_dirtyhelper_FRSTOR ( VexGuestX86State*, HWord );

extern VexEmNote 
            x86g_dirtyhelper_FLDENV ( VexGuestX86State*, HWord );



#define X86G_CC_SHIFT_O   11
#define X86G_CC_SHIFT_S   7
#define X86G_CC_SHIFT_Z   6
#define X86G_CC_SHIFT_A   4
#define X86G_CC_SHIFT_C   0
#define X86G_CC_SHIFT_P   2

#define X86G_CC_MASK_O    (1 << X86G_CC_SHIFT_O)
#define X86G_CC_MASK_S    (1 << X86G_CC_SHIFT_S)
#define X86G_CC_MASK_Z    (1 << X86G_CC_SHIFT_Z)
#define X86G_CC_MASK_A    (1 << X86G_CC_SHIFT_A)
#define X86G_CC_MASK_C    (1 << X86G_CC_SHIFT_C)
#define X86G_CC_MASK_P    (1 << X86G_CC_SHIFT_P)

#define X86G_FC_SHIFT_C3   14
#define X86G_FC_SHIFT_C2   10
#define X86G_FC_SHIFT_C1   9
#define X86G_FC_SHIFT_C0   8

#define X86G_FC_MASK_C3    (1 << X86G_FC_SHIFT_C3)
#define X86G_FC_MASK_C2    (1 << X86G_FC_SHIFT_C2)
#define X86G_FC_MASK_C1    (1 << X86G_FC_SHIFT_C1)
#define X86G_FC_MASK_C0    (1 << X86G_FC_SHIFT_C0)


enum {
    X86G_CC_OP_COPY=0,  
                        

    X86G_CC_OP_ADDB,    
    X86G_CC_OP_ADDW,    
    X86G_CC_OP_ADDL,    

    X86G_CC_OP_SUBB,    
    X86G_CC_OP_SUBW,    
    X86G_CC_OP_SUBL,    

    X86G_CC_OP_ADCB,    
    X86G_CC_OP_ADCW,    
    X86G_CC_OP_ADCL,    

    X86G_CC_OP_SBBB,    
    X86G_CC_OP_SBBW,    
    X86G_CC_OP_SBBL,    

    X86G_CC_OP_LOGICB,  
    X86G_CC_OP_LOGICW,  
    X86G_CC_OP_LOGICL,  

    X86G_CC_OP_INCB,    
    X86G_CC_OP_INCW,    
    X86G_CC_OP_INCL,    

    X86G_CC_OP_DECB,    
    X86G_CC_OP_DECW,    
    X86G_CC_OP_DECL,    

    X86G_CC_OP_SHLB,    
    X86G_CC_OP_SHLW,    
    X86G_CC_OP_SHLL,    

    X86G_CC_OP_SHRB,    
    X86G_CC_OP_SHRW,    
    X86G_CC_OP_SHRL,    

    X86G_CC_OP_ROLB,    
    X86G_CC_OP_ROLW,    
    X86G_CC_OP_ROLL,    

    X86G_CC_OP_RORB,    
    X86G_CC_OP_RORW,    
    X86G_CC_OP_RORL,    

    X86G_CC_OP_UMULB,   
    X86G_CC_OP_UMULW,   
    X86G_CC_OP_UMULL,   

    X86G_CC_OP_SMULB,   
    X86G_CC_OP_SMULW,   
    X86G_CC_OP_SMULL,   

    X86G_CC_OP_NUMBER
};

typedef
   enum {
      X86CondO      = 0,  
      X86CondNO     = 1,  

      X86CondB      = 2,  
      X86CondNB     = 3,  

      X86CondZ      = 4,  
      X86CondNZ     = 5,  

      X86CondBE     = 6,  
      X86CondNBE    = 7,  

      X86CondS      = 8,  
      X86CondNS     = 9,  

      X86CondP      = 10, 
      X86CondNP     = 11, 

      X86CondL      = 12, 
      X86CondNL     = 13, 

      X86CondLE     = 14, 
      X86CondNLE    = 15, 

      X86CondAlways = 16  
   }
   X86Condcode;

#endif 

