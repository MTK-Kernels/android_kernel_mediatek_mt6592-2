

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



#ifndef __VEX_GUEST_PPC_DEFS_H
#define __VEX_GUEST_PPC_DEFS_H

#include "libvex_basictypes.h"
#include "libvex_guest_ppc32.h"         
#include "libvex_guest_ppc64.h"         
#include "guest_generic_bb_to_IR.h"     


extern
DisResult disInstr_PPC ( IRSB*        irbb,
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
IRExpr* guest_ppc32_spechelper ( const HChar* function_name,
                                 IRExpr** args,
                                 IRStmt** precedingStmts,
                                 Int      n_precedingStmts );

extern
IRExpr* guest_ppc64_spechelper ( const HChar* function_name,
                                 IRExpr** args,
                                 IRStmt** precedingStmts,
                                 Int      n_precedingStmts );

extern 
Bool guest_ppc32_state_requires_precise_mem_exns ( Int, Int,
                                                   VexRegisterUpdates );

extern 
Bool guest_ppc64_state_requires_precise_mem_exns ( Int, Int,
                                                   VexRegisterUpdates );

extern
VexGuestLayout ppc32Guest_layout;

extern
VexGuestLayout ppc64Guest_layout;


typedef
   enum {
      PPCrm_NEAREST = 0,
      PPCrm_NegINF  = 1,
      PPCrm_PosINF  = 2,
      PPCrm_ZERO    = 3
   } PPCRoundingMode;

typedef
   enum {
      PPCcr_LT = 0x8,
      PPCcr_GT = 0x4,
      PPCcr_EQ = 0x2,
      PPCcr_UN = 0x1
   }
   PPCCmpF64Result;

enum {
    PPCG_FLAG_OP_ADD=0,   
    PPCG_FLAG_OP_ADDE,    
    PPCG_FLAG_OP_DIVW,    
    PPCG_FLAG_OP_DIVWU,   
    PPCG_FLAG_OP_MULLW,   
    PPCG_FLAG_OP_NEG,     
    PPCG_FLAG_OP_SUBF,    
    PPCG_FLAG_OP_SUBFC,   
    PPCG_FLAG_OP_SUBFE,   
    PPCG_FLAG_OP_SUBFI,   
    PPCG_FLAG_OP_SRAW,    
    PPCG_FLAG_OP_SRAWI,   
    PPCG_FLAG_OP_SRAD,    
    PPCG_FLAG_OP_SRADI,   
    PPCG_FLAG_OP_DIVDE,   
    PPCG_FLAG_OP_DIVWEU,  
    PPCG_FLAG_OP_DIVWE,   
    PPCG_FLAG_OP_DIVDEU,  
    PPCG_FLAG_OP_MULLD,   
   PPCG_FLAG_OP_NUMBER
};






extern ULong ppcg_dirtyhelper_MFTB ( void );

extern UInt ppc32g_dirtyhelper_MFSPR_268_269 ( UInt );

extern UInt ppc32g_dirtyhelper_MFSPR_287 ( void );

extern void ppc32g_dirtyhelper_LVS ( VexGuestPPC32State* gst,
                                     UInt vD_idx, UInt sh,
                                     UInt shift_right );

extern void ppc64g_dirtyhelper_LVS ( VexGuestPPC64State* gst,
                                     UInt vD_idx, UInt sh,
                                     UInt shift_right,
                                     UInt endness );

#endif 

