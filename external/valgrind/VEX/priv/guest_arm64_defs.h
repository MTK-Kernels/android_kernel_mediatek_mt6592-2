
/*
   This file is part of Valgrind, a dynamic binary instrumentation
   framework.

   Copyright (C) 2013-2013 OpenWorks
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
*/

#ifndef __VEX_GUEST_ARM64_DEFS_H
#define __VEX_GUEST_ARM64_DEFS_H

#include "libvex_basictypes.h"
#include "guest_generic_bb_to_IR.h"     


extern
DisResult disInstr_ARM64 ( IRSB*        irbb,
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
IRExpr* guest_arm64_spechelper ( const HChar* function_name,
                                 IRExpr** args,
                                 IRStmt** precedingStmts,
                                 Int      n_precedingStmts );

extern 
Bool guest_arm64_state_requires_precise_mem_exns ( Int, Int,
                                                   VexRegisterUpdates );

extern
VexGuestLayout arm64Guest_layout;




extern 
ULong arm64g_calculate_flags_nzcv ( ULong cc_op, ULong cc_dep1,
                                    ULong cc_dep2, ULong cc_dep3 );

extern
ULong arm64g_calculate_flag_c ( ULong cc_op, ULong cc_dep1,
                                ULong cc_dep2, ULong cc_dep3 );

extern 
ULong arm64g_calculate_condition ( 
                                   ULong cond_n_op ,
                                   ULong cc_dep1,
                                   ULong cc_dep2, ULong cc_dep3 );




extern ULong arm64g_dirtyhelper_MRS_CNTVCT_EL0 ( void );



#define ARM64G_CC_SHIFT_N  31
#define ARM64G_CC_SHIFT_Z  30
#define ARM64G_CC_SHIFT_C  29
#define ARM64G_CC_SHIFT_V  28


enum {
   ARM64G_CC_OP_COPY=0,   

   ARM64G_CC_OP_ADD32,    

   ARM64G_CC_OP_ADD64,    

   ARM64G_CC_OP_SUB32,    

   ARM64G_CC_OP_SUB64,    

   ARM64G_CC_OP_ADC32,    

   ARM64G_CC_OP_ADC64,    

   ARM64G_CC_OP_SBC32,    

   ARM64G_CC_OP_SBC64,    

   ARM64G_CC_OP_LOGIC32,  
   ARM64G_CC_OP_LOGIC64,  


   ARM64G_CC_OP_NUMBER
};





typedef
   enum {
      ARM64CondEQ = 0,  
      ARM64CondNE = 1,  

      ARM64CondCS = 2,  
      ARM64CondCC = 3,  

      ARM64CondMI = 4,  
      ARM64CondPL = 5,  

      ARM64CondVS = 6,  
      ARM64CondVC = 7,  

      ARM64CondHI = 8,  
      ARM64CondLS = 9,  

      ARM64CondGE = 10, 
      ARM64CondLT = 11, 

      ARM64CondGT = 12, 
      ARM64CondLE = 13, 

      ARM64CondAL = 14, 
      ARM64CondNV = 15  
   }
   ARM64Condcode;

#endif 

