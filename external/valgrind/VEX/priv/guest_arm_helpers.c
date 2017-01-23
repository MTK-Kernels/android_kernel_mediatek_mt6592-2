

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

#include "libvex_basictypes.h"
#include "libvex_emnote.h"
#include "libvex_guest_arm.h"
#include "libvex_ir.h"
#include "libvex.h"

#include "main_util.h"
#include "main_globals.h"
#include "guest_generic_bb_to_IR.h"
#include "guest_arm_defs.h"




#define PROFILE_NZCV_FLAGS 0

#if PROFILE_NZCV_FLAGS

static UInt tab_n_eval[ARMG_CC_OP_NUMBER];
static UInt tab_z_eval[ARMG_CC_OP_NUMBER];
static UInt tab_c_eval[ARMG_CC_OP_NUMBER];
static UInt tab_v_eval[ARMG_CC_OP_NUMBER];
static UInt initted = 0;
static UInt tot_evals = 0;

static void initCounts ( void )
{
   UInt i;
   for (i = 0; i < ARMG_CC_OP_NUMBER; i++) {
      tab_n_eval[i] = tab_z_eval[i] = tab_c_eval[i] = tab_v_eval[i] = 0;
   }
   initted = 1;
}

static void showCounts ( void )
{
   UInt i;
   vex_printf("\n                 N          Z          C          V\n");
   vex_printf(  "---------------------------------------------------\n");
   for (i = 0; i < ARMG_CC_OP_NUMBER; i++) {
      vex_printf("CC_OP=%d  %9d  %9d  %9d  %9d\n",
                 i,
                 tab_n_eval[i], tab_z_eval[i],
                 tab_c_eval[i], tab_v_eval[i] );
    }
}

#define NOTE_N_EVAL(_cc_op) NOTE_EVAL(_cc_op, tab_n_eval)
#define NOTE_Z_EVAL(_cc_op) NOTE_EVAL(_cc_op, tab_z_eval)
#define NOTE_C_EVAL(_cc_op) NOTE_EVAL(_cc_op, tab_c_eval)
#define NOTE_V_EVAL(_cc_op) NOTE_EVAL(_cc_op, tab_v_eval)

#define NOTE_EVAL(_cc_op, _tab) \
   do { \
      if (!initted) initCounts(); \
      vassert( ((UInt)(_cc_op)) < ARMG_CC_OP_NUMBER); \
      _tab[(UInt)(_cc_op)]++; \
      tot_evals++; \
      if (0 == (tot_evals & 0xFFFFF)) \
        showCounts(); \
   } while (0)

#endif 


static
UInt armg_calculate_flag_n ( UInt cc_op, UInt cc_dep1,
                             UInt cc_dep2, UInt cc_dep3 )
{
#  if PROFILE_NZCV_FLAGS
   NOTE_N_EVAL(cc_op);
#  endif

   switch (cc_op) {
      case ARMG_CC_OP_COPY: {
         
         UInt nf   = (cc_dep1 >> ARMG_CC_SHIFT_N) & 1;
         return nf;
      }
      case ARMG_CC_OP_ADD: {
         
         UInt argL = cc_dep1;
         UInt argR = cc_dep2;
         UInt res  = argL + argR;
         UInt nf   = res >> 31;
         return nf;
      }
      case ARMG_CC_OP_SUB: {
         
         UInt argL = cc_dep1;
         UInt argR = cc_dep2;
         UInt res  = argL - argR;
         UInt nf   = res >> 31;
         return nf;
      }
      case ARMG_CC_OP_ADC: {
         
         UInt argL = cc_dep1;
         UInt argR = cc_dep2;
         UInt oldC = cc_dep3;
         vassert((oldC & ~1) == 0);
         UInt res  = argL + argR + oldC;
         UInt nf   = res >> 31;
         return nf;
      }
      case ARMG_CC_OP_SBB: {
         
         UInt argL = cc_dep1;
         UInt argR = cc_dep2;
         UInt oldC = cc_dep3;
         vassert((oldC & ~1) == 0);
         UInt res  = argL - argR - (oldC ^ 1);
         UInt nf   = res >> 31;
         return nf;
      }
      case ARMG_CC_OP_LOGIC: {
         
         UInt res  = cc_dep1;
         UInt nf   = res >> 31;
         return nf;
      }
      case ARMG_CC_OP_MUL: {
         
         UInt res  = cc_dep1;
         UInt nf   = res >> 31;
         return nf;
      }
      case ARMG_CC_OP_MULL: {
         
         UInt resHi32 = cc_dep2;
         UInt nf      = resHi32 >> 31;
         return nf;
      }
      default:
         
         vex_printf("armg_calculate_flag_n"
                    "( op=%u, dep1=0x%x, dep2=0x%x, dep3=0x%x )\n",
                    cc_op, cc_dep1, cc_dep2, cc_dep3 );
         vpanic("armg_calculate_flags_n");
   }
}


static
UInt armg_calculate_flag_z ( UInt cc_op, UInt cc_dep1,
                             UInt cc_dep2, UInt cc_dep3 )
{
#  if PROFILE_NZCV_FLAGS
   NOTE_Z_EVAL(cc_op);
#  endif

   switch (cc_op) {
      case ARMG_CC_OP_COPY: {
         
         UInt zf   = (cc_dep1 >> ARMG_CC_SHIFT_Z) & 1;
         return zf;
      }
      case ARMG_CC_OP_ADD: {
         
         UInt argL = cc_dep1;
         UInt argR = cc_dep2;
         UInt res  = argL + argR;
         UInt zf   = res == 0;
         return zf;
      }
      case ARMG_CC_OP_SUB: {
         
         UInt argL = cc_dep1;
         UInt argR = cc_dep2;
         UInt res  = argL - argR;
         UInt zf   = res == 0;
         return zf;
      }
      case ARMG_CC_OP_ADC: {
         
         UInt argL = cc_dep1;
         UInt argR = cc_dep2;
         UInt oldC = cc_dep3;
         vassert((oldC & ~1) == 0);
         UInt res  = argL + argR + oldC;
         UInt zf   = res == 0;
         return zf;
      }
      case ARMG_CC_OP_SBB: {
         
         UInt argL = cc_dep1;
         UInt argR = cc_dep2;
         UInt oldC = cc_dep3;
         vassert((oldC & ~1) == 0);
         UInt res  = argL - argR - (oldC ^ 1);
         UInt zf   = res == 0;
         return zf;
      }
      case ARMG_CC_OP_LOGIC: {
         
         UInt res  = cc_dep1;
         UInt zf   = res == 0;
         return zf;
      }
      case ARMG_CC_OP_MUL: {
         
         UInt res  = cc_dep1;
         UInt zf   = res == 0;
         return zf;
      }
      case ARMG_CC_OP_MULL: {
         
         UInt resLo32 = cc_dep1;
         UInt resHi32 = cc_dep2;
         UInt zf      = (resHi32|resLo32) == 0;
         return zf;
      }
      default:
         
         vex_printf("armg_calculate_flags_z"
                    "( op=%u, dep1=0x%x, dep2=0x%x, dep3=0x%x )\n",
                    cc_op, cc_dep1, cc_dep2, cc_dep3 );
         vpanic("armg_calculate_flags_z");
   }
}


UInt armg_calculate_flag_c ( UInt cc_op, UInt cc_dep1,
                             UInt cc_dep2, UInt cc_dep3 )
{
#  if PROFILE_NZCV_FLAGS
   NOTE_C_EVAL(cc_op);
#  endif

   switch (cc_op) {
      case ARMG_CC_OP_COPY: {
         
         UInt cf   = (cc_dep1 >> ARMG_CC_SHIFT_C) & 1;
         return cf;
      }
      case ARMG_CC_OP_ADD: {
         
         UInt argL = cc_dep1;
         UInt argR = cc_dep2;
         UInt res  = argL + argR;
         UInt cf   = res < argL;
         return cf;
      }
      case ARMG_CC_OP_SUB: {
         
         UInt argL = cc_dep1;
         UInt argR = cc_dep2;
         UInt cf   = argL >= argR;
         return cf;
      }
      case ARMG_CC_OP_ADC: {
         
         UInt argL = cc_dep1;
         UInt argR = cc_dep2;
         UInt oldC = cc_dep3;
         vassert((oldC & ~1) == 0);
         UInt res  = argL + argR + oldC;
         UInt cf   = oldC ? (res <= argL) : (res < argL);
         return cf;
      }
      case ARMG_CC_OP_SBB: {
         
         UInt argL = cc_dep1;
         UInt argR = cc_dep2;
         UInt oldC = cc_dep3;
         vassert((oldC & ~1) == 0);
         UInt cf   = oldC ? (argL >= argR) : (argL > argR);
         return cf;
      }
      case ARMG_CC_OP_LOGIC: {
         
         UInt shco = cc_dep2;
         vassert((shco & ~1) == 0);
         UInt cf   = shco;
         return cf;
      }
      case ARMG_CC_OP_MUL: {
         
         UInt oldC = (cc_dep3 >> 1) & 1;
         vassert((cc_dep3 & ~3) == 0);
         UInt cf   = oldC;
         return cf;
      }
      case ARMG_CC_OP_MULL: {
         
         UInt oldC    = (cc_dep3 >> 1) & 1;
         vassert((cc_dep3 & ~3) == 0);
         UInt cf      = oldC;
         return cf;
      }
      default:
         
         vex_printf("armg_calculate_flag_c"
                    "( op=%u, dep1=0x%x, dep2=0x%x, dep3=0x%x )\n",
                    cc_op, cc_dep1, cc_dep2, cc_dep3 );
         vpanic("armg_calculate_flag_c");
   }
}


UInt armg_calculate_flag_v ( UInt cc_op, UInt cc_dep1,
                             UInt cc_dep2, UInt cc_dep3 )
{
#  if PROFILE_NZCV_FLAGS
   NOTE_V_EVAL(cc_op);
#  endif

   switch (cc_op) {
      case ARMG_CC_OP_COPY: {
         
         UInt vf   = (cc_dep1 >> ARMG_CC_SHIFT_V) & 1;
         return vf;
      }
      case ARMG_CC_OP_ADD: {
         
         UInt argL = cc_dep1;
         UInt argR = cc_dep2;
         UInt res  = argL + argR;
         UInt vf   = ((res ^ argL) & (res ^ argR)) >> 31;
         return vf;
      }
      case ARMG_CC_OP_SUB: {
         
         UInt argL = cc_dep1;
         UInt argR = cc_dep2;
         UInt res  = argL - argR;
         UInt vf   = ((argL ^ argR) & (argL ^ res)) >> 31;
         return vf;
      }
      case ARMG_CC_OP_ADC: {
         
         UInt argL = cc_dep1;
         UInt argR = cc_dep2;
         UInt oldC = cc_dep3;
         vassert((oldC & ~1) == 0);
         UInt res  = argL + argR + oldC;
         UInt vf   = ((res ^ argL) & (res ^ argR)) >> 31;
         return vf;
      }
      case ARMG_CC_OP_SBB: {
         
         UInt argL = cc_dep1;
         UInt argR = cc_dep2;
         UInt oldC = cc_dep3;
         vassert((oldC & ~1) == 0);
         UInt res  = argL - argR - (oldC ^ 1);
         UInt vf   = ((argL ^ argR) & (argL ^ res)) >> 31;
         return vf;
      }
      case ARMG_CC_OP_LOGIC: {
         
         UInt oldV = cc_dep3;
         vassert((oldV & ~1) == 0);
         UInt vf   = oldV;
         return vf;
      }
      case ARMG_CC_OP_MUL: {
         
         UInt oldV = (cc_dep3 >> 0) & 1;
         vassert((cc_dep3 & ~3) == 0);
         UInt vf   = oldV;
         return vf;
      }
      case ARMG_CC_OP_MULL: {
         
         UInt oldV    = (cc_dep3 >> 0) & 1;
         vassert((cc_dep3 & ~3) == 0);
         UInt vf      = oldV;
         return vf;
      }
      default:
         
         vex_printf("armg_calculate_flag_v"
                    "( op=%u, dep1=0x%x, dep2=0x%x, dep3=0x%x )\n",
                    cc_op, cc_dep1, cc_dep2, cc_dep3 );
         vpanic("armg_calculate_flag_v");
   }
}


UInt armg_calculate_flags_nzcv ( UInt cc_op, UInt cc_dep1,
                                 UInt cc_dep2, UInt cc_dep3 )
{
   UInt f;
   UInt res = 0;
   f = armg_calculate_flag_n(cc_op, cc_dep1, cc_dep2, cc_dep3);
   res |= (f << ARMG_CC_SHIFT_N);
   f = armg_calculate_flag_z(cc_op, cc_dep1, cc_dep2, cc_dep3);
   res |= (f << ARMG_CC_SHIFT_Z);
   f = armg_calculate_flag_c(cc_op, cc_dep1, cc_dep2, cc_dep3);
   res |= (f << ARMG_CC_SHIFT_C);
   f = armg_calculate_flag_v(cc_op, cc_dep1, cc_dep2, cc_dep3);
   res |= (f << ARMG_CC_SHIFT_V);
   return res;
}


UInt armg_calculate_flag_qc ( UInt resL1, UInt resL2,
                              UInt resR1, UInt resR2 )
{
   if (resL1 != resR1 || resL2 != resR2)
      return 1;
   else
      return 0;
}

UInt armg_calculate_condition ( UInt cond_n_op ,
                                UInt cc_dep1,
                                UInt cc_dep2, UInt cc_dep3 )
{
   UInt cond  = cond_n_op >> 4;
   UInt cc_op = cond_n_op & 0xF;
   UInt nf, zf, vf, cf, inv;
   
   

   
   if (cond == ARMCondAL) return 1;

   inv  = cond & 1;

   switch (cond) {
      case ARMCondEQ:    
      case ARMCondNE:    
         zf = armg_calculate_flag_z(cc_op, cc_dep1, cc_dep2, cc_dep3);
         return inv ^ zf;

      case ARMCondHS:    
      case ARMCondLO:    
         cf = armg_calculate_flag_c(cc_op, cc_dep1, cc_dep2, cc_dep3);
         return inv ^ cf;

      case ARMCondMI:    
      case ARMCondPL:    
         nf = armg_calculate_flag_n(cc_op, cc_dep1, cc_dep2, cc_dep3);
         return inv ^ nf;

      case ARMCondVS:    
      case ARMCondVC:    
         vf = armg_calculate_flag_v(cc_op, cc_dep1, cc_dep2, cc_dep3);
         return inv ^ vf;

      case ARMCondHI:    
      case ARMCondLS:    
         cf = armg_calculate_flag_c(cc_op, cc_dep1, cc_dep2, cc_dep3);
         zf = armg_calculate_flag_z(cc_op, cc_dep1, cc_dep2, cc_dep3);
         return inv ^ (1 & (cf & ~zf));

      case ARMCondGE:    
      case ARMCondLT:    
         nf = armg_calculate_flag_n(cc_op, cc_dep1, cc_dep2, cc_dep3);
         vf = armg_calculate_flag_v(cc_op, cc_dep1, cc_dep2, cc_dep3);
         return inv ^ (1 & ~(nf ^ vf));

      case ARMCondGT:    
      case ARMCondLE:    
         nf = armg_calculate_flag_n(cc_op, cc_dep1, cc_dep2, cc_dep3);
         vf = armg_calculate_flag_v(cc_op, cc_dep1, cc_dep2, cc_dep3);
         zf = armg_calculate_flag_z(cc_op, cc_dep1, cc_dep2, cc_dep3);
         return inv ^ (1 & ~(zf | (nf ^ vf)));

      case ARMCondAL: 
      case ARMCondNV: 
      default:
         
         vex_printf("armg_calculate_condition(ARM)"
                    "( %u, %u, 0x%x, 0x%x, 0x%x )\n",
                    cond, cc_op, cc_dep1, cc_dep2, cc_dep3 );
         vpanic("armg_calculate_condition(ARM)");
   }
}




static Bool isU32 ( IRExpr* e, UInt n )
{
   return
      toBool( e->tag == Iex_Const
              && e->Iex.Const.con->tag == Ico_U32
              && e->Iex.Const.con->Ico.U32 == n );
}

IRExpr* guest_arm_spechelper ( const HChar* function_name,
                               IRExpr** args,
                               IRStmt** precedingStmts,
                               Int      n_precedingStmts )
{
#  define unop(_op,_a1) IRExpr_Unop((_op),(_a1))
#  define binop(_op,_a1,_a2) IRExpr_Binop((_op),(_a1),(_a2))
#  define mkU32(_n) IRExpr_Const(IRConst_U32(_n))
#  define mkU8(_n)  IRExpr_Const(IRConst_U8(_n))

   Int i, arity = 0;
   for (i = 0; args[i]; i++)
      arity++;
#  if 0
   vex_printf("spec request:\n");
   vex_printf("   %s  ", function_name);
   for (i = 0; i < arity; i++) {
      vex_printf("  ");
      ppIRExpr(args[i]);
   }
   vex_printf("\n");
#  endif

   

   if (vex_streq(function_name, "armg_calculate_condition")) {

      IRExpr *cond_n_op, *cc_dep1, *cc_dep2, *cc_ndep;
      vassert(arity == 4);
      cond_n_op = args[0]; 
      cc_dep1   = args[1];
      cc_dep2   = args[2];
      cc_ndep   = args[3];

      

      if (isU32(cond_n_op, (ARMCondEQ << 4) | ARMG_CC_OP_SUB)) {
         
         return unop(Iop_1Uto32,
                     binop(Iop_CmpEQ32, cc_dep1, cc_dep2));
      }
      if (isU32(cond_n_op, (ARMCondNE << 4) | ARMG_CC_OP_SUB)) {
         
         return unop(Iop_1Uto32,
                     binop(Iop_CmpNE32, cc_dep1, cc_dep2));
      }

      if (isU32(cond_n_op, (ARMCondGT << 4) | ARMG_CC_OP_SUB)) {
         return unop(Iop_1Uto32,
                     binop(Iop_CmpLT32S, cc_dep2, cc_dep1));
      }
      if (isU32(cond_n_op, (ARMCondLE << 4) | ARMG_CC_OP_SUB)) {
         
         return unop(Iop_1Uto32,
                     binop(Iop_CmpLE32S, cc_dep1, cc_dep2));
      }

      if (isU32(cond_n_op, (ARMCondLT << 4) | ARMG_CC_OP_SUB)) {
         
         return unop(Iop_1Uto32,
                     binop(Iop_CmpLT32S, cc_dep1, cc_dep2));
      }

      if (isU32(cond_n_op, (ARMCondGE << 4) | ARMG_CC_OP_SUB)) {
         return unop(Iop_1Uto32,
                     binop(Iop_CmpLE32S, cc_dep2, cc_dep1));
      }

      if (isU32(cond_n_op, (ARMCondHS << 4) | ARMG_CC_OP_SUB)) {
         return unop(Iop_1Uto32,
                     binop(Iop_CmpLE32U, cc_dep2, cc_dep1));
      }
      if (isU32(cond_n_op, (ARMCondLO << 4) | ARMG_CC_OP_SUB)) {
         
         return unop(Iop_1Uto32,
                     binop(Iop_CmpLT32U, cc_dep1, cc_dep2));
      }

      if (isU32(cond_n_op, (ARMCondLS << 4) | ARMG_CC_OP_SUB)) {
         
         return unop(Iop_1Uto32,
                     binop(Iop_CmpLE32U, cc_dep1, cc_dep2));
      }
      if (isU32(cond_n_op, (ARMCondHI << 4) | ARMG_CC_OP_SUB)) {
         return unop(Iop_1Uto32,
                     binop(Iop_CmpLT32U, cc_dep2, cc_dep1));
      }

      

      if (isU32(cond_n_op, (ARMCondHS << 4) | ARMG_CC_OP_SBB)) {
         
         
         return
            IRExpr_ITE(
               binop(Iop_CmpNE32, cc_ndep, mkU32(0)),
               
               unop(Iop_1Uto32, binop(Iop_CmpLE32U, cc_dep2, cc_dep1)),
               
               unop(Iop_1Uto32, binop(Iop_CmpLT32U, cc_dep2, cc_dep1))
            );
      }

      

      if (isU32(cond_n_op, (ARMCondEQ << 4) | ARMG_CC_OP_LOGIC)) {
         
         return unop(Iop_1Uto32,
                     binop(Iop_CmpEQ32, cc_dep1, mkU32(0)));
      }
      if (isU32(cond_n_op, (ARMCondNE << 4) | ARMG_CC_OP_LOGIC)) {
         
         return unop(Iop_1Uto32,
                     binop(Iop_CmpNE32, cc_dep1, mkU32(0)));
      }

      if (isU32(cond_n_op, (ARMCondPL << 4) | ARMG_CC_OP_LOGIC)) {
         
         return unop(Iop_1Uto32,
                     binop(Iop_CmpEQ32,
                           binop(Iop_Shr32, cc_dep1, mkU8(31)),
                           mkU32(0)));
      }
      if (isU32(cond_n_op, (ARMCondMI << 4) | ARMG_CC_OP_LOGIC)) {
         
         return unop(Iop_1Uto32,
                     binop(Iop_CmpEQ32,
                           binop(Iop_Shr32, cc_dep1, mkU8(31)),
                           mkU32(1)));
      }

      

      
      if (isU32(cond_n_op, (ARMCondEQ << 4) | ARMG_CC_OP_COPY)) {
         
         return binop(Iop_And32,
                      binop(Iop_Shr32, cc_dep1,
                            mkU8(ARMG_CC_SHIFT_Z)),
                      mkU32(1));
      }
      if (isU32(cond_n_op, (ARMCondNE << 4) | ARMG_CC_OP_COPY)) {
         
         return binop(Iop_And32,
                      binop(Iop_Xor32,
                            binop(Iop_Shr32, cc_dep1,
                                             mkU8(ARMG_CC_SHIFT_Z)),
                            mkU32(1)),
                      mkU32(1));
      }

      
      if (isU32(cond_n_op, (ARMCondMI << 4) | ARMG_CC_OP_COPY)) {
         
         return binop(Iop_And32,
                      binop(Iop_Shr32, cc_dep1,
                            mkU8(ARMG_CC_SHIFT_N)),
                      mkU32(1));
      }
      if (isU32(cond_n_op, (ARMCondPL << 4) | ARMG_CC_OP_COPY)) {
         
         return binop(Iop_And32,
                      binop(Iop_Xor32,
                            binop(Iop_Shr32, cc_dep1,
                                             mkU8(ARMG_CC_SHIFT_N)),
                            mkU32(1)),
                      mkU32(1));
      }

      
      if (isU32(cond_n_op, (ARMCondGT << 4) | ARMG_CC_OP_COPY)) {
         
         IRExpr* n = binop(Iop_Shr32, cc_dep1, mkU8(ARMG_CC_SHIFT_N));
         IRExpr* v = binop(Iop_Shr32, cc_dep1, mkU8(ARMG_CC_SHIFT_V));
         IRExpr* z = binop(Iop_Shr32, cc_dep1, mkU8(ARMG_CC_SHIFT_Z));
         return binop(Iop_Xor32,
                      binop(Iop_And32, 
                            binop(Iop_Or32, z, binop(Iop_Xor32, n, v)),
                            mkU32(1)),
                      mkU32(1));
      }
      if (isU32(cond_n_op, (ARMCondLE << 4) | ARMG_CC_OP_COPY)) {
         
         IRExpr* n = binop(Iop_Shr32, cc_dep1, mkU8(ARMG_CC_SHIFT_N));
         IRExpr* v = binop(Iop_Shr32, cc_dep1, mkU8(ARMG_CC_SHIFT_V));
         IRExpr* z = binop(Iop_Shr32, cc_dep1, mkU8(ARMG_CC_SHIFT_Z));
         return binop(Iop_Xor32,
                      binop(Iop_And32, 
                            binop(Iop_Or32, z, binop(Iop_Xor32, n, v)),
                            mkU32(1)),
                      mkU32(0));
      }

      

      if (cond_n_op->tag == Iex_RdTmp) {
         Int    j;
         IRTemp look_for = cond_n_op->Iex.RdTmp.tmp;
         Int    limit    = n_precedingStmts - 16;
         if (limit < 0) limit = 0;
         if (0) vex_printf("scanning %d .. %d\n", n_precedingStmts-1, limit);
         for (j = n_precedingStmts - 1; j >= limit; j--) {
            IRStmt* st = precedingStmts[j];
            if (st->tag == Ist_WrTmp
                && st->Ist.WrTmp.tmp == look_for
                && st->Ist.WrTmp.data->tag == Iex_Binop
                && st->Ist.WrTmp.data->Iex.Binop.op == Iop_Or32
                && isU32(st->Ist.WrTmp.data->Iex.Binop.arg2, (ARMCondAL << 4)))
               return mkU32(1);
         }
      }
   }

   

   else
   if (vex_streq(function_name, "armg_calculate_flag_c")) {

      IRExpr *cc_op, *cc_dep1, *cc_dep2, *cc_ndep;
      vassert(arity == 4);
      cc_op   = args[0]; 
      cc_dep1 = args[1];
      cc_dep2 = args[2];
      cc_ndep = args[3];

      if (isU32(cc_op, ARMG_CC_OP_LOGIC)) {
         
         
         return cc_dep2;
      }

      if (isU32(cc_op, ARMG_CC_OP_SUB)) {
         
         return unop(Iop_1Uto32,
                     binop(Iop_CmpLE32U, cc_dep2, cc_dep1));
      }

      if (isU32(cc_op, ARMG_CC_OP_SBB)) {
         
         
         return
            IRExpr_ITE(
               binop(Iop_CmpNE32, cc_ndep, mkU32(0)),
               
               unop(Iop_1Uto32, binop(Iop_CmpLE32U, cc_dep2, cc_dep1)),
               
               unop(Iop_1Uto32, binop(Iop_CmpLT32U, cc_dep2, cc_dep1))
            );
      }

   }

   

   else
   if (vex_streq(function_name, "armg_calculate_flag_v")) {

      IRExpr *cc_op, *cc_dep1, *cc_dep2, *cc_ndep;
      vassert(arity == 4);
      cc_op   = args[0]; 
      cc_dep1 = args[1];
      cc_dep2 = args[2];
      cc_ndep = args[3];

      if (isU32(cc_op, ARMG_CC_OP_LOGIC)) {
         
         
         return cc_ndep;
      }

      if (isU32(cc_op, ARMG_CC_OP_SUB)) {
         
         IRExpr* argL = cc_dep1;
         IRExpr* argR = cc_dep2;
         return
            binop(Iop_Shr32,
                  binop(Iop_And32,
                        binop(Iop_Xor32, argL, argR),
                        binop(Iop_Xor32, argL, binop(Iop_Sub32, argL, argR))
                  ),
                  mkU8(31)
            );
      }

      if (isU32(cc_op, ARMG_CC_OP_SBB)) {
         
         
         return
            binop(
               Iop_And32,
               binop(
                  Iop_And32,
                  
                  binop(Iop_Xor32, cc_dep1, cc_dep2),
                  
                  binop(Iop_Xor32,
                        cc_dep1,
                        binop(Iop_Sub32,
                              binop(Iop_Sub32, cc_dep1, cc_dep2),
                              binop(Iop_Xor32, cc_ndep, mkU32(1)))
                  )
               ),
               mkU32(1)
            );
      }

   }

#  undef unop
#  undef binop
#  undef mkU32
#  undef mkU8

   return NULL;
}



#if 0
void LibVEX_GuestARM_put_flags ( UInt flags_native,
                                 VexGuestARMState* vex_state )
{
   vassert(0); 

   
   flags_native
      &= (ARMG_CC_MASK_N | ARMG_CC_MASK_Z | ARMG_CC_MASK_V | ARMG_CC_MASK_C);
   
   vex_state->guest_CC_OP   = ARMG_CC_OP_COPY;
   vex_state->guest_CC_DEP1 = flags_native;
   vex_state->guest_CC_DEP2 = 0;
   vex_state->guest_CC_NDEP = 0;
}
#endif

UInt LibVEX_GuestARM_get_cpsr ( const VexGuestARMState* vex_state )
{
   UInt cpsr = 0;
   
   cpsr |= armg_calculate_flags_nzcv(
               vex_state->guest_CC_OP,
               vex_state->guest_CC_DEP1,
               vex_state->guest_CC_DEP2,
               vex_state->guest_CC_NDEP
            );
   vassert(0 == (cpsr & 0x0FFFFFFF));
   
   if (vex_state->guest_QFLAG32 > 0)
      cpsr |= (1 << 27);
   
   if (vex_state->guest_GEFLAG0 > 0)
      cpsr |= (1 << 16);
   if (vex_state->guest_GEFLAG1 > 0)
      cpsr |= (1 << 17);
   if (vex_state->guest_GEFLAG2 > 0)
      cpsr |= (1 << 18);
   if (vex_state->guest_GEFLAG3 > 0)
      cpsr |= (1 << 19);
   
   cpsr |= (1 << 4); 
   
   
   if (vex_state->guest_R15T & 1)
      cpsr |= (1 << 5);
   
   
   
   
   
   return cpsr;
}

void LibVEX_GuestARM_initialise ( VexGuestARMState* vex_state )
{
   vex_state->host_EvC_FAILADDR = 0;
   vex_state->host_EvC_COUNTER = 0;

   vex_state->guest_R0  = 0;
   vex_state->guest_R1  = 0;
   vex_state->guest_R2  = 0;
   vex_state->guest_R3  = 0;
   vex_state->guest_R4  = 0;
   vex_state->guest_R5  = 0;
   vex_state->guest_R6  = 0;
   vex_state->guest_R7  = 0;
   vex_state->guest_R8  = 0;
   vex_state->guest_R9  = 0;
   vex_state->guest_R10 = 0;
   vex_state->guest_R11 = 0;
   vex_state->guest_R12 = 0;
   vex_state->guest_R13 = 0;
   vex_state->guest_R14 = 0;
   vex_state->guest_R15T = 0;  

   vex_state->guest_CC_OP   = ARMG_CC_OP_COPY;
   vex_state->guest_CC_DEP1 = 0;
   vex_state->guest_CC_DEP2 = 0;
   vex_state->guest_CC_NDEP = 0;
   vex_state->guest_QFLAG32 = 0;
   vex_state->guest_GEFLAG0 = 0;
   vex_state->guest_GEFLAG1 = 0;
   vex_state->guest_GEFLAG2 = 0;
   vex_state->guest_GEFLAG3 = 0;

   vex_state->guest_EMNOTE  = EmNote_NONE;
   vex_state->guest_CMSTART = 0;
   vex_state->guest_CMLEN   = 0;
   vex_state->guest_NRADDR  = 0;
   vex_state->guest_IP_AT_SYSCALL = 0;

   vex_state->guest_D0  = 0;
   vex_state->guest_D1  = 0;
   vex_state->guest_D2  = 0;
   vex_state->guest_D3  = 0;
   vex_state->guest_D4  = 0;
   vex_state->guest_D5  = 0;
   vex_state->guest_D6  = 0;
   vex_state->guest_D7  = 0;
   vex_state->guest_D8  = 0;
   vex_state->guest_D9  = 0;
   vex_state->guest_D10 = 0;
   vex_state->guest_D11 = 0;
   vex_state->guest_D12 = 0;
   vex_state->guest_D13 = 0;
   vex_state->guest_D14 = 0;
   vex_state->guest_D15 = 0;
   vex_state->guest_D16 = 0;
   vex_state->guest_D17 = 0;
   vex_state->guest_D18 = 0;
   vex_state->guest_D19 = 0;
   vex_state->guest_D20 = 0;
   vex_state->guest_D21 = 0;
   vex_state->guest_D22 = 0;
   vex_state->guest_D23 = 0;
   vex_state->guest_D24 = 0;
   vex_state->guest_D25 = 0;
   vex_state->guest_D26 = 0;
   vex_state->guest_D27 = 0;
   vex_state->guest_D28 = 0;
   vex_state->guest_D29 = 0;
   vex_state->guest_D30 = 0;
   vex_state->guest_D31 = 0;

   vex_state->guest_FPSCR = 0;

   vex_state->guest_TPIDRURO = 0;

   
   vex_state->guest_ITSTATE = 0;

   vex_state->padding1 = 0;
}



Bool guest_arm_state_requires_precise_mem_exns (
        Int minoff, Int maxoff, VexRegisterUpdates pxControl
     )
{
   Int sp_min = offsetof(VexGuestARMState, guest_R13);
   Int sp_max = sp_min + 4 - 1;
   Int pc_min = offsetof(VexGuestARMState, guest_R15T);
   Int pc_max = pc_min + 4 - 1;

   if (maxoff < sp_min || minoff > sp_max) {
      
      if (pxControl == VexRegUpdSpAtMemAccess)
         return False; 
   } else {
      return True;
   }

   if (maxoff < pc_min || minoff > pc_max) {
      
   } else {
      return True;
   }

   Int r11_min = offsetof(VexGuestARMState, guest_R11);
   Int r11_max = r11_min + 4 - 1;

   if (maxoff < r11_min || minoff > r11_max) {
      
   } else {
      return True;
   }

   Int r7_min = offsetof(VexGuestARMState, guest_R7);
   Int r7_max = r7_min + 4 - 1;

   if (maxoff < r7_min || minoff > r7_max) {
      
   } else {
      return True;
   }

   return False;
}



#define ALWAYSDEFD(field)                           \
    { offsetof(VexGuestARMState, field),            \
      (sizeof ((VexGuestARMState*)0)->field) }

VexGuestLayout
   armGuest_layout 
      = { 
          
          .total_sizeB = sizeof(VexGuestARMState),

          
          .offset_SP = offsetof(VexGuestARMState,guest_R13),
          .sizeof_SP = 4,

          
          .offset_IP = offsetof(VexGuestARMState,guest_R15T),
          .sizeof_IP = 4,

          .n_alwaysDefd = 10,

          .alwaysDefd
             = {  ALWAYSDEFD(guest_R15T),
                  ALWAYSDEFD(guest_CC_OP),
                  ALWAYSDEFD(guest_CC_NDEP),
                  ALWAYSDEFD(guest_EMNOTE),
                  ALWAYSDEFD(guest_CMSTART),
                  ALWAYSDEFD(guest_CMLEN),
                  ALWAYSDEFD(guest_NRADDR),
                  ALWAYSDEFD(guest_IP_AT_SYSCALL),
                  ALWAYSDEFD(guest_TPIDRURO),
                  ALWAYSDEFD(guest_ITSTATE)
               }
        };


