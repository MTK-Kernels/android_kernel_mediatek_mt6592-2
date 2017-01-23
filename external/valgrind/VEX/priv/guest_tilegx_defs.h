
/*
   This file is part of Valgrind, a dynamic binary instrumentation
   framework.

   Copyright (C) 2010-2013 Tilera Corp.

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

 

#ifndef __VEX_GUEST_TILEGX_DEFS_H
#define __VEX_GUEST_TILEGX_DEFS_H

#ifdef __tilegx__
#include "tilegx_disasm.h"
#endif


extern DisResult disInstr_TILEGX ( IRSB* irbb,
                                   Bool (*resteerOkFn) ( void *, Addr ),
                                   Bool resteerCisOk,
                                   void* callback_opaque,
                                   const UChar* guest_code,
                                   Long delta,
                                   Addr guest_IP,
                                   VexArch guest_arch,
                                   const VexArchInfo* archinfo,
                                   const VexAbiInfo* abiinfo,
                                   VexEndness host_endness_IN,
                                   Bool sigill_diag_IN );

extern IRExpr *guest_tilegx_spechelper ( const HChar * function_name,
                                         IRExpr ** args,
                                         IRStmt ** precedingStmts,
                                         Int n_precedingStmts );

extern Bool guest_tilegx_state_requires_precise_mem_exns (
  Int, Int, VexRegisterUpdates );

extern VexGuestLayout tilegxGuest_layout;


extern ULong tilegx_dirtyhelper_gen ( ULong opc,
                                      ULong rd0,
                                      ULong rd1,
                                      ULong rd2,
                                      ULong rd3 );



typedef enum {
  TILEGXCondEQ = 0,      
  TILEGXCondNE = 1,      
  TILEGXCondHS = 2,      
  TILEGXCondLO = 3,      
  TILEGXCondMI = 4,      
  TILEGXCondPL = 5,      
  TILEGXCondVS = 6,      
  TILEGXCondVC = 7,      
  TILEGXCondHI = 8,      
  TILEGXCondLS = 9,      
  TILEGXCondGE = 10,     
  TILEGXCondLT = 11,     
  TILEGXCondGT = 12,     
  TILEGXCondLE = 13,     
  TILEGXCondAL = 14,     
  TILEGXCondNV = 15      
} TILEGXCondcode;

#endif            

