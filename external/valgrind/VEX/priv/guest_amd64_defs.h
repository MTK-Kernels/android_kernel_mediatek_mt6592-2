

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


#ifndef __VEX_GUEST_AMD64_DEFS_H
#define __VEX_GUEST_AMD64_DEFS_H

#include "libvex_basictypes.h"
#include "libvex_emnote.h"              
#include "libvex_guest_amd64.h"         
#include "guest_generic_bb_to_IR.h"     


extern
DisResult disInstr_AMD64 ( IRSB*        irbb,
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
IRExpr* guest_amd64_spechelper ( const HChar* function_name,
                                 IRExpr** args,
                                 IRStmt** precedingStmts,
                                 Int      n_precedingStmts );

extern 
Bool guest_amd64_state_requires_precise_mem_exns ( Int, Int,
                                                   VexRegisterUpdates );

extern
VexGuestLayout amd64guest_layout;




extern ULong amd64g_calculate_rflags_all ( 
                ULong cc_op, 
                ULong cc_dep1, ULong cc_dep2, ULong cc_ndep 
             );

extern ULong amd64g_calculate_rflags_c ( 
                ULong cc_op, 
                ULong cc_dep1, ULong cc_dep2, ULong cc_ndep 
             );

extern ULong amd64g_calculate_condition ( 
                ULong cond, 
                ULong cc_op, 
                ULong cc_dep1, ULong cc_dep2, ULong cc_ndep 
             );

extern ULong amd64g_calculate_FXAM ( ULong tag, ULong dbl );

extern ULong amd64g_calculate_RCR  ( 
                ULong arg, ULong rot_amt, ULong rflags_in, Long sz 
             );

extern ULong amd64g_calculate_RCL  ( 
                ULong arg, ULong rot_amt, ULong rflags_in, Long sz 
             );

extern ULong amd64g_calculate_pclmul(ULong s1, ULong s2, ULong which);

extern ULong amd64g_check_fldcw ( ULong fpucw );

extern ULong amd64g_create_fpucw ( ULong fpround );

extern ULong amd64g_check_ldmxcsr ( ULong mxcsr );

extern ULong amd64g_create_mxcsr ( ULong sseround );

extern VexEmNote amd64g_dirtyhelper_FLDENV  ( VexGuestAMD64State*, HWord );
extern VexEmNote amd64g_dirtyhelper_FRSTOR  ( VexGuestAMD64State*, HWord );
extern VexEmNote amd64g_dirtyhelper_FRSTORS ( VexGuestAMD64State*, HWord );

extern void amd64g_dirtyhelper_FSTENV  ( VexGuestAMD64State*, HWord );
extern void amd64g_dirtyhelper_FNSAVE  ( VexGuestAMD64State*, HWord );
extern void amd64g_dirtyhelper_FNSAVES ( VexGuestAMD64State*, HWord );


extern ULong amd64g_calculate_mmx_pmaddwd  ( ULong, ULong );
extern ULong amd64g_calculate_mmx_psadbw   ( ULong, ULong );

extern ULong amd64g_calculate_sse_phminposuw ( ULong sLo, ULong sHi );

extern ULong amd64g_calc_crc32b ( ULong crcIn, ULong b );
extern ULong amd64g_calc_crc32w ( ULong crcIn, ULong w );
extern ULong amd64g_calc_crc32l ( ULong crcIn, ULong l );
extern ULong amd64g_calc_crc32q ( ULong crcIn, ULong q );

extern ULong amd64g_calc_mpsadbw ( ULong sHi, ULong sLo,
                                   ULong dHi, ULong dLo,
                                   ULong imm_and_return_control_bit );

extern ULong amd64g_calculate_pext  ( ULong, ULong );
extern ULong amd64g_calculate_pdep  ( ULong, ULong );


extern ULong amd64g_dirtyhelper_loadF80le  ( Addr );

extern void  amd64g_dirtyhelper_storeF80le ( Addr, ULong );

extern void  amd64g_dirtyhelper_CPUID_baseline ( VexGuestAMD64State* st );
extern void  amd64g_dirtyhelper_CPUID_sse3_and_cx16 ( VexGuestAMD64State* st );
extern void  amd64g_dirtyhelper_CPUID_sse42_and_cx16 ( VexGuestAMD64State* st );
extern void  amd64g_dirtyhelper_CPUID_avx_and_cx16 ( VexGuestAMD64State* st );

extern void  amd64g_dirtyhelper_FINIT ( VexGuestAMD64State* );

extern void      amd64g_dirtyhelper_FXSAVE_ALL_EXCEPT_XMM
                    ( VexGuestAMD64State*, HWord );
extern VexEmNote amd64g_dirtyhelper_FXRSTOR_ALL_EXCEPT_XMM
                    ( VexGuestAMD64State*, HWord );

extern ULong amd64g_dirtyhelper_RDTSC ( void );
extern void  amd64g_dirtyhelper_RDTSCP ( VexGuestAMD64State* st );

extern ULong amd64g_dirtyhelper_IN  ( ULong portno, ULong sz );
extern void  amd64g_dirtyhelper_OUT ( ULong portno, ULong data, 
                                      ULong sz );

extern void amd64g_dirtyhelper_SxDT ( void* address,
                                      ULong op  );

extern ULong amd64g_dirtyhelper_PCMPxSTRx ( 
          VexGuestAMD64State*,
          HWord opc4_and_imm,
          HWord gstOffL, HWord gstOffR,
          HWord edxIN, HWord eaxIN
       );

extern void amd64g_dirtyhelper_AES ( 
          VexGuestAMD64State* gst,
          HWord opc4, HWord gstOffD,
          HWord gstOffL, HWord gstOffR
       );

extern void amd64g_dirtyhelper_AESKEYGENASSIST ( 
          VexGuestAMD64State* gst,
          HWord imm8,
          HWord gstOffL, HWord gstOffR
       );









#define AMD64G_CC_SHIFT_O   11
#define AMD64G_CC_SHIFT_S   7
#define AMD64G_CC_SHIFT_Z   6
#define AMD64G_CC_SHIFT_A   4
#define AMD64G_CC_SHIFT_C   0
#define AMD64G_CC_SHIFT_P   2

#define AMD64G_CC_MASK_O    (1ULL << AMD64G_CC_SHIFT_O)
#define AMD64G_CC_MASK_S    (1ULL << AMD64G_CC_SHIFT_S)
#define AMD64G_CC_MASK_Z    (1ULL << AMD64G_CC_SHIFT_Z)
#define AMD64G_CC_MASK_A    (1ULL << AMD64G_CC_SHIFT_A)
#define AMD64G_CC_MASK_C    (1ULL << AMD64G_CC_SHIFT_C)
#define AMD64G_CC_MASK_P    (1ULL << AMD64G_CC_SHIFT_P)

#define AMD64G_FC_SHIFT_C3   14
#define AMD64G_FC_SHIFT_C2   10
#define AMD64G_FC_SHIFT_C1   9
#define AMD64G_FC_SHIFT_C0   8

#define AMD64G_FC_MASK_C3    (1ULL << AMD64G_FC_SHIFT_C3)
#define AMD64G_FC_MASK_C2    (1ULL << AMD64G_FC_SHIFT_C2)
#define AMD64G_FC_MASK_C1    (1ULL << AMD64G_FC_SHIFT_C1)
#define AMD64G_FC_MASK_C0    (1ULL << AMD64G_FC_SHIFT_C0)


enum {
    AMD64G_CC_OP_COPY=0,  
                          

    AMD64G_CC_OP_ADDB,    
    AMD64G_CC_OP_ADDW,    
    AMD64G_CC_OP_ADDL,    
    AMD64G_CC_OP_ADDQ,    

    AMD64G_CC_OP_SUBB,    
    AMD64G_CC_OP_SUBW,    
    AMD64G_CC_OP_SUBL,    
    AMD64G_CC_OP_SUBQ,    

    AMD64G_CC_OP_ADCB,    
    AMD64G_CC_OP_ADCW,    
    AMD64G_CC_OP_ADCL,    
    AMD64G_CC_OP_ADCQ,    

    AMD64G_CC_OP_SBBB,    
    AMD64G_CC_OP_SBBW,    
    AMD64G_CC_OP_SBBL,    
    AMD64G_CC_OP_SBBQ,    

    AMD64G_CC_OP_LOGICB,  
    AMD64G_CC_OP_LOGICW,  
    AMD64G_CC_OP_LOGICL,  
    AMD64G_CC_OP_LOGICQ,  

    AMD64G_CC_OP_INCB,    
    AMD64G_CC_OP_INCW,    
    AMD64G_CC_OP_INCL,    
    AMD64G_CC_OP_INCQ,    

    AMD64G_CC_OP_DECB,    
    AMD64G_CC_OP_DECW,    
    AMD64G_CC_OP_DECL,    
    AMD64G_CC_OP_DECQ,    

    AMD64G_CC_OP_SHLB,    
    AMD64G_CC_OP_SHLW,    
    AMD64G_CC_OP_SHLL,    
    AMD64G_CC_OP_SHLQ,    

    AMD64G_CC_OP_SHRB,    
    AMD64G_CC_OP_SHRW,    
    AMD64G_CC_OP_SHRL,    
    AMD64G_CC_OP_SHRQ,    

    AMD64G_CC_OP_ROLB,    
    AMD64G_CC_OP_ROLW,    
    AMD64G_CC_OP_ROLL,    
    AMD64G_CC_OP_ROLQ,    

    AMD64G_CC_OP_RORB,    
    AMD64G_CC_OP_RORW,    
    AMD64G_CC_OP_RORL,    
    AMD64G_CC_OP_RORQ,    

    AMD64G_CC_OP_UMULB,   
    AMD64G_CC_OP_UMULW,   
    AMD64G_CC_OP_UMULL,   
    AMD64G_CC_OP_UMULQ,   

    AMD64G_CC_OP_SMULB,   
    AMD64G_CC_OP_SMULW,   
    AMD64G_CC_OP_SMULL,   
    AMD64G_CC_OP_SMULQ,   

    AMD64G_CC_OP_ANDN32,  
    AMD64G_CC_OP_ANDN64,  

    AMD64G_CC_OP_BLSI32,  
    AMD64G_CC_OP_BLSI64,  

    AMD64G_CC_OP_BLSMSK32,
    AMD64G_CC_OP_BLSMSK64,

    AMD64G_CC_OP_BLSR32,  
    AMD64G_CC_OP_BLSR64,  

    AMD64G_CC_OP_NUMBER
};

typedef
   enum {
      AMD64CondO      = 0,  
      AMD64CondNO     = 1,  

      AMD64CondB      = 2,  
      AMD64CondNB     = 3,  

      AMD64CondZ      = 4,  
      AMD64CondNZ     = 5,  

      AMD64CondBE     = 6,  
      AMD64CondNBE    = 7,  

      AMD64CondS      = 8,  
      AMD64CondNS     = 9,  

      AMD64CondP      = 10, 
      AMD64CondNP     = 11, 

      AMD64CondL      = 12, 
      AMD64CondNL     = 13, 

      AMD64CondLE     = 14, 
      AMD64CondNLE    = 15, 

      AMD64CondAlways = 16  
   }
   AMD64Condcode;

#endif 

