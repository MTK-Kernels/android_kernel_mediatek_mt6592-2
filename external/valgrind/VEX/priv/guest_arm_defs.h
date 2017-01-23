
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
*/


#ifndef __VEX_GUEST_ARM_DEFS_H
#define __VEX_GUEST_ARM_DEFS_H

#include "libvex_basictypes.h"
#include "guest_generic_bb_to_IR.h"     


extern
DisResult disInstr_ARM ( IRSB*        irbb,
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
IRExpr* guest_arm_spechelper ( const HChar* function_name,
                               IRExpr** args,
                               IRStmt** precedingStmts,
                               Int      n_precedingStmts );

extern 
Bool guest_arm_state_requires_precise_mem_exns ( Int, Int,
                                                 VexRegisterUpdates );

extern
VexGuestLayout armGuest_layout;




extern 
UInt armg_calculate_flags_nzcv ( UInt cc_op, UInt cc_dep1,
                                 UInt cc_dep2, UInt cc_dep3 );

extern 
UInt armg_calculate_flag_c ( UInt cc_op, UInt cc_dep1,
                             UInt cc_dep2, UInt cc_dep3 );

extern 
UInt armg_calculate_flag_v ( UInt cc_op, UInt cc_dep1,
                             UInt cc_dep2, UInt cc_dep3 );

extern 
UInt armg_calculate_condition ( UInt cond_n_op ,
                                UInt cc_dep1,
                                UInt cc_dep2, UInt cc_dep3 );

extern 
UInt armg_calculate_flag_qc ( UInt resL1, UInt resL2,
                              UInt resR1, UInt resR2 );



#define ARMG_CC_SHIFT_N  31
#define ARMG_CC_SHIFT_Z  30
#define ARMG_CC_SHIFT_C  29
#define ARMG_CC_SHIFT_V  28
#define ARMG_CC_SHIFT_Q  27

#define ARMG_CC_MASK_N    (1 << ARMG_CC_SHIFT_N)
#define ARMG_CC_MASK_Z    (1 << ARMG_CC_SHIFT_Z)
#define ARMG_CC_MASK_C    (1 << ARMG_CC_SHIFT_C)
#define ARMG_CC_MASK_V    (1 << ARMG_CC_SHIFT_V)
#define ARMG_CC_MASK_Q    (1 << ARMG_CC_SHIFT_Q)


enum {
   ARMG_CC_OP_COPY=0,  

   ARMG_CC_OP_ADD,     

   ARMG_CC_OP_SUB,     

   ARMG_CC_OP_ADC,     

   ARMG_CC_OP_SBB,     

   ARMG_CC_OP_LOGIC,   

   ARMG_CC_OP_MUL,     

   ARMG_CC_OP_MULL,    

   ARMG_CC_OP_NUMBER
};





typedef
   enum {
      ARMCondEQ     = 0,  
      ARMCondNE     = 1,  

      ARMCondHS     = 2,  
      ARMCondLO     = 3,  

      ARMCondMI     = 4,  
      ARMCondPL     = 5,  

      ARMCondVS     = 6,  
      ARMCondVC     = 7,  

      ARMCondHI     = 8,  
      ARMCondLS     = 9,  

      ARMCondGE     = 10, 
      ARMCondLT     = 11, 

      ARMCondGT     = 12, 
      ARMCondLE     = 13, 

      ARMCondAL     = 14, 
      ARMCondNV     = 15  
   }
   ARMCondcode;

#endif 

