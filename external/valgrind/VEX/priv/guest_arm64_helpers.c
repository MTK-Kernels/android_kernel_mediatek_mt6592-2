

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

#include "libvex_basictypes.h"
#include "libvex_emnote.h"
#include "libvex_guest_arm64.h"
#include "libvex_ir.h"
#include "libvex.h"

#include "main_util.h"
#include "main_globals.h"
#include "guest_generic_bb_to_IR.h"
#include "guest_arm64_defs.h"




#define PROFILE_NZCV_FLAGS 0

#if PROFILE_NZCV_FLAGS

static UInt tab_eval[ARM64G_CC_OP_NUMBER][16];
static UInt initted = 0;
static UInt tot_evals = 0;

static void initCounts ( void )
{
   UInt i, j;
   for (i = 0; i < ARM64G_CC_OP_NUMBER; i++) {
      for (j = 0; j < 16; j++) {
         tab_eval[i][j] = 0;
      }
   }
   initted = 1;
}

static void showCounts ( void )
{
   const HChar* nameCC[16]
      = { "EQ", "NE", "CS", "CC", "MI", "PL", "VS", "VC",
          "HI", "LS", "GE", "LT", "GT", "LE", "AL", "NV" };
   UInt i, j;
   ULong sum = 0;
   vex_printf("\nCC_OP          0         1         2         3    "
              "     4         5         6\n");
   vex_printf(  "--------------------------------------------------"
              "--------------------------\n");
   for (j = 0; j < 16; j++) {
      vex_printf("%2d %s  ", j, nameCC[j]);
      for (i = 0; i < ARM64G_CC_OP_NUMBER; i++) {
         vex_printf("%9d ", tab_eval[i][j]);
         sum += tab_eval[i][j];
      }
      vex_printf("\n");
   }
   vex_printf("(In total %llu calls)\n", sum);
}

#define NOTE_EVAL(_cc_op, _cond) \
   do { \
      if (!initted) initCounts(); \
      vassert( ((UInt)(_cc_op)) < ARM64G_CC_OP_NUMBER); \
      vassert( ((UInt)(_cond)) < 16); \
      tab_eval[(UInt)(_cc_op)][(UInt)(cond)]++;  \
      tot_evals++; \
      if (0 == (tot_evals & 0x7FFF)) \
        showCounts(); \
   } while (0)

#endif 


static
ULong arm64g_calculate_flag_n ( ULong cc_op, ULong cc_dep1,
                                ULong cc_dep2, ULong cc_dep3 )
{
   switch (cc_op) {
      case ARM64G_CC_OP_COPY: {
         
         ULong nf   = (cc_dep1 >> ARM64G_CC_SHIFT_N) & 1;
         return nf;
      }
      case ARM64G_CC_OP_ADD32: {
         
         UInt  argL = (UInt)cc_dep1;
         UInt  argR = (UInt)cc_dep2;
         UInt  res  = argL + argR;
         ULong nf   = (ULong)(res >> 31);
         return nf;
      }
      case ARM64G_CC_OP_ADD64: {
         
         ULong argL = cc_dep1;
         ULong argR = cc_dep2;
         ULong res  = argL + argR;
         ULong nf   = (ULong)(res >> 63);
         return nf;
      }
      case ARM64G_CC_OP_SUB32: {
         
         UInt  argL = (UInt)cc_dep1;
         UInt  argR = (UInt)cc_dep2;
         UInt  res  = argL - argR;
         ULong nf   = (ULong)(res >> 31);
         return nf;
      }
      case ARM64G_CC_OP_SUB64: {
         
         ULong argL = cc_dep1;
         ULong argR = cc_dep2;
         ULong res  = argL - argR;
         ULong nf   = res >> 63;
         return nf;
      }
      case ARM64G_CC_OP_ADC32: {
         
         UInt  argL = cc_dep1;
         UInt  argR = cc_dep2;
         UInt  oldC = cc_dep3;
         vassert((oldC & ~1) == 0);
         UInt  res  = argL + argR + oldC;
         ULong nf   = (ULong)(res >> 31);
         return nf;
      }
      case ARM64G_CC_OP_ADC64: {
         
         ULong argL = cc_dep1;
         ULong argR = cc_dep2;
         ULong oldC = cc_dep3;
         vassert((oldC & ~1) == 0);
         ULong res  = argL + argR + oldC;
         ULong nf   = res >> 63;
         return nf;
      }
      case ARM64G_CC_OP_SBC32: {
         
         UInt  argL = cc_dep1;
         UInt  argR = cc_dep2;
         UInt  oldC = cc_dep3;
         vassert((oldC & ~1) == 0);
         UInt  res  = argL - argR - (oldC ^ 1);
         ULong nf   = (ULong)(res >> 31);
         return nf;
      }
      case ARM64G_CC_OP_SBC64: {
         
         ULong argL = cc_dep1;
         ULong argR = cc_dep2;
         ULong oldC = cc_dep3;
         vassert((oldC & ~1) == 0);
         ULong res  = argL - argR - (oldC ^ 1);
         ULong nf   = res >> 63;
         return nf;
      }
      case ARM64G_CC_OP_LOGIC32: {
         
         UInt  res = (UInt)cc_dep1;
         ULong nf  = res >> 31;
         return nf;
      }
      case ARM64G_CC_OP_LOGIC64: {
         
         ULong res = cc_dep1;
         ULong nf  = res >> 63;
         return nf;
      }
      default:
         
         vex_printf("arm64g_calculate_flag_n"
                    "( op=%llu, dep1=0x%llx, dep2=0x%llx, dep3=0x%llx )\n",
                    cc_op, cc_dep1, cc_dep2, cc_dep3 );
         vpanic("arm64g_calculate_flag_n");
   }
}


static
ULong arm64g_calculate_flag_z ( ULong cc_op, ULong cc_dep1,
                                ULong cc_dep2, ULong cc_dep3 )
{
   switch (cc_op) {
      case ARM64G_CC_OP_COPY: {
         
         ULong zf   = (cc_dep1 >> ARM64G_CC_SHIFT_Z) & 1;
         return zf;
      }
      case ARM64G_CC_OP_ADD32: {
         
         UInt  argL = (UInt)cc_dep1;
         UInt  argR = (UInt)cc_dep2;
         UInt  res  = argL + argR;
         ULong zf   = res == 0;
         return zf;
      }
      case ARM64G_CC_OP_ADD64: {
         
         ULong argL = cc_dep1;
         ULong argR = cc_dep2;
         ULong res  = argL + argR;
         ULong zf   = res == 0;
         return zf;
      }
      case ARM64G_CC_OP_SUB32: {
         
         UInt  argL = (UInt)cc_dep1;
         UInt  argR = (UInt)cc_dep2;
         UInt  res  = argL - argR;
         ULong zf   = res == 0;
         return zf;
      }
      case ARM64G_CC_OP_SUB64: {
         
         ULong argL = cc_dep1;
         ULong argR = cc_dep2;
         ULong res  = argL - argR;
         ULong zf   = res == 0;
         return zf;
      }
      case ARM64G_CC_OP_ADC32: {
         
         UInt  argL = cc_dep1;
         UInt  argR = cc_dep2;
         UInt  oldC = cc_dep3;
         vassert((oldC & ~1) == 0);
         UInt  res  = argL + argR + oldC;
         ULong zf   = res == 0;
         return zf;
      }
      case ARM64G_CC_OP_ADC64: {
         
         ULong argL = cc_dep1;
         ULong argR = cc_dep2;
         ULong oldC = cc_dep3;
         vassert((oldC & ~1) == 0);
         ULong res  = argL + argR + oldC;
         ULong zf   = res == 0;
         return zf;
      }
      case ARM64G_CC_OP_SBC32: {
         
         UInt  argL = cc_dep1;
         UInt  argR = cc_dep2;
         UInt  oldC = cc_dep3;
         vassert((oldC & ~1) == 0);
         UInt  res  = argL - argR - (oldC ^ 1);
         ULong zf   = res == 0;
         return zf;
      }
      case ARM64G_CC_OP_SBC64: {
         
         ULong argL = cc_dep1;
         ULong argR = cc_dep2;
         ULong oldC = cc_dep3;
         vassert((oldC & ~1) == 0);
         ULong res  = argL - argR - (oldC ^ 1);
         ULong zf   = res == 0;
         return zf;
      }
      case ARM64G_CC_OP_LOGIC32: {
         
         UInt  res  = (UInt)cc_dep1;
         ULong zf   = res == 0;
         return zf;
      }
      case ARM64G_CC_OP_LOGIC64: {
         
         ULong res  = cc_dep1;
         ULong zf   = res == 0;
         return zf;
      }
      default:
         
         vex_printf("arm64g_calculate_flag_z"
                    "( op=%llu, dep1=0x%llx, dep2=0x%llx, dep3=0x%llx )\n",
                    cc_op, cc_dep1, cc_dep2, cc_dep3 );
         vpanic("arm64g_calculate_flag_z");
   }
}


ULong arm64g_calculate_flag_c ( ULong cc_op, ULong cc_dep1,
                                ULong cc_dep2, ULong cc_dep3 )
{
   switch (cc_op) {
      case ARM64G_CC_OP_COPY: {
         
         ULong cf = (cc_dep1 >> ARM64G_CC_SHIFT_C) & 1;
         return cf;
      }
      case ARM64G_CC_OP_ADD32: {
         
         UInt  argL = (UInt)cc_dep1;
         UInt  argR = (UInt)cc_dep2;
         UInt  res  = argL + argR;
         ULong cf   = res < argL;
         return cf;
      }
      case ARM64G_CC_OP_ADD64: {
         
         ULong argL = cc_dep1;
         ULong argR = cc_dep2;
         ULong res  = argL + argR;
         ULong cf   = res < argL;
         return cf;
      }
      case ARM64G_CC_OP_SUB32: {
         
         UInt  argL = (UInt)cc_dep1;
         UInt  argR = (UInt)cc_dep2;
         ULong cf   = argL >= argR;
         return cf;
      }
      case ARM64G_CC_OP_SUB64: {
         
         ULong argL = cc_dep1;
         ULong argR = cc_dep2;
         ULong cf   = argL >= argR;
         return cf;
      }
      case ARM64G_CC_OP_ADC32: {
         
         UInt  argL = cc_dep1;
         UInt  argR = cc_dep2;
         UInt  oldC = cc_dep3;
         vassert((oldC & ~1) == 0);
         UInt  res  = argL + argR + oldC;
         ULong cf   = oldC ? (res <= argL) : (res < argL);
         return cf;
      }
      case ARM64G_CC_OP_ADC64: {
         
         ULong argL = cc_dep1;
         ULong argR = cc_dep2;
         ULong oldC = cc_dep3;
         vassert((oldC & ~1) == 0);
         ULong res  = argL + argR + oldC;
         ULong cf   = oldC ? (res <= argL) : (res < argL);
         return cf;
      }
      case ARM64G_CC_OP_SBC32: {
         
         UInt  argL = cc_dep1;
         UInt  argR = cc_dep2;
         UInt  oldC = cc_dep3;
         vassert((oldC & ~1) == 0);
         ULong cf   = oldC ? (argL >= argR) : (argL > argR);
         return cf;
      }
      case ARM64G_CC_OP_SBC64: {
         
         ULong argL = cc_dep1;
         ULong argR = cc_dep2;
         ULong oldC = cc_dep3;
         vassert((oldC & ~1) == 0);
         ULong cf   = oldC ? (argL >= argR) : (argL > argR);
         return cf;
      }
      case ARM64G_CC_OP_LOGIC32:
      case ARM64G_CC_OP_LOGIC64: {
         
         return 0; 
      }
      default:
         
         vex_printf("arm64g_calculate_flag_c"
                    "( op=%llu, dep1=0x%llx, dep2=0x%llx, dep3=0x%llx )\n",
                    cc_op, cc_dep1, cc_dep2, cc_dep3 );
         vpanic("arm64g_calculate_flag_c");
   }
}


static
ULong arm64g_calculate_flag_v ( ULong cc_op, ULong cc_dep1,
                                ULong cc_dep2, ULong cc_dep3 )
{
   switch (cc_op) {
      case ARM64G_CC_OP_COPY: {
         
         ULong vf   = (cc_dep1 >> ARM64G_CC_SHIFT_V) & 1;
         return vf;
      }
      case ARM64G_CC_OP_ADD32: {
         
         UInt  argL = (UInt)cc_dep1;
         UInt  argR = (UInt)cc_dep2;
         UInt  res  = argL + argR;
         ULong vf   = (ULong)(((res ^ argL) & (res ^ argR)) >> 31);
         return vf;
      }
      case ARM64G_CC_OP_ADD64: {
         
         ULong argL = cc_dep1;
         ULong argR = cc_dep2;
         ULong res  = argL + argR;
         ULong vf   = ((res ^ argL) & (res ^ argR)) >> 63;
         return vf;
      }
      case ARM64G_CC_OP_SUB32: {
         
         UInt  argL = (UInt)cc_dep1;
         UInt  argR = (UInt)cc_dep2;
         UInt  res  = argL - argR;
         ULong vf   = (ULong)(((argL ^ argR) & (argL ^ res)) >> 31);
         return vf;
      }
      case ARM64G_CC_OP_SUB64: {
         
         ULong argL = cc_dep1;
         ULong argR = cc_dep2;
         ULong res  = argL - argR;
         ULong vf   = (((argL ^ argR) & (argL ^ res))) >> 63;
         return vf;
      }
      case ARM64G_CC_OP_ADC32: {
         
         UInt  argL = cc_dep1;
         UInt  argR = cc_dep2;
         UInt  oldC = cc_dep3;
         vassert((oldC & ~1) == 0);
         UInt  res  = argL + argR + oldC;
         ULong vf   = (ULong)(((res ^ argL) & (res ^ argR)) >> 31);
         return vf;
      }
      case ARM64G_CC_OP_ADC64: {
         
         ULong argL = cc_dep1;
         ULong argR = cc_dep2;
         ULong oldC = cc_dep3;
         vassert((oldC & ~1) == 0);
         ULong res  = argL + argR + oldC;
         ULong vf   = ((res ^ argL) & (res ^ argR)) >> 63;
         return vf;
      }
      case ARM64G_CC_OP_SBC32: {
         
         UInt  argL = cc_dep1;
         UInt  argR = cc_dep2;
         UInt  oldC = cc_dep3;
         vassert((oldC & ~1) == 0);
         UInt  res  = argL - argR - (oldC ^ 1);
         ULong vf   = (ULong)(((argL ^ argR) & (argL ^ res)) >> 31);
         return vf;
      }
      case ARM64G_CC_OP_SBC64: {
         
         ULong argL = cc_dep1;
         ULong argR = cc_dep2;
         ULong oldC = cc_dep3;
         vassert((oldC & ~1) == 0);
         ULong res  = argL - argR - (oldC ^ 1);
         ULong vf   = ((argL ^ argR) & (argL ^ res)) >> 63;
         return vf;
      }
      case ARM64G_CC_OP_LOGIC32:
      case ARM64G_CC_OP_LOGIC64: {
         
         return 0; 
      }
      default:
         
         vex_printf("arm64g_calculate_flag_v"
                    "( op=%llu, dep1=0x%llx, dep2=0x%llx, dep3=0x%llx )\n",
                    cc_op, cc_dep1, cc_dep2, cc_dep3 );
         vpanic("arm64g_calculate_flag_v");
   }
}


ULong arm64g_calculate_flags_nzcv ( ULong cc_op, ULong cc_dep1,
                                    ULong cc_dep2, ULong cc_dep3 )
{
   ULong f;
   ULong res = 0;
   f = 1 & arm64g_calculate_flag_n(cc_op, cc_dep1, cc_dep2, cc_dep3);
   res |= (f << ARM64G_CC_SHIFT_N);
   f = 1 & arm64g_calculate_flag_z(cc_op, cc_dep1, cc_dep2, cc_dep3);
   res |= (f << ARM64G_CC_SHIFT_Z);
   f = 1 & arm64g_calculate_flag_c(cc_op, cc_dep1, cc_dep2, cc_dep3);
   res |= (f << ARM64G_CC_SHIFT_C);
   f = 1 & arm64g_calculate_flag_v(cc_op, cc_dep1, cc_dep2, cc_dep3);
   res |= (f << ARM64G_CC_SHIFT_V);
   return res;
}


ULong arm64g_calculate_condition ( 
                                   ULong cond_n_op ,
                                   ULong cc_dep1,
                                   ULong cc_dep2, ULong cc_dep3 )
{
   ULong cond  = cond_n_op >> 4;
   ULong cc_op = cond_n_op & 0xF;
   ULong inv   = cond & 1;
   ULong nf, zf, vf, cf;

#  if PROFILE_NZCV_FLAGS
   NOTE_EVAL(cc_op, cond);
#  endif

   
   

   switch (cond) {
      case ARM64CondEQ:    
      case ARM64CondNE:    
         zf = arm64g_calculate_flag_z(cc_op, cc_dep1, cc_dep2, cc_dep3);
         return inv ^ zf;

      case ARM64CondCS:    
      case ARM64CondCC:    
         cf = arm64g_calculate_flag_c(cc_op, cc_dep1, cc_dep2, cc_dep3);
         return inv ^ cf;

      case ARM64CondMI:    
      case ARM64CondPL:    
         nf = arm64g_calculate_flag_n(cc_op, cc_dep1, cc_dep2, cc_dep3);
         return inv ^ nf;

      case ARM64CondVS:    
      case ARM64CondVC:    
         vf = arm64g_calculate_flag_v(cc_op, cc_dep1, cc_dep2, cc_dep3);
         return inv ^ vf;

      case ARM64CondHI:    
      case ARM64CondLS:    
         cf = arm64g_calculate_flag_c(cc_op, cc_dep1, cc_dep2, cc_dep3);
         zf = arm64g_calculate_flag_z(cc_op, cc_dep1, cc_dep2, cc_dep3);
         return inv ^ (1 & (cf & ~zf));

      case ARM64CondGE:    
      case ARM64CondLT:    
         nf = arm64g_calculate_flag_n(cc_op, cc_dep1, cc_dep2, cc_dep3);
         vf = arm64g_calculate_flag_v(cc_op, cc_dep1, cc_dep2, cc_dep3);
         return inv ^ (1 & ~(nf ^ vf));

      case ARM64CondGT:    
      case ARM64CondLE:    
         nf = arm64g_calculate_flag_n(cc_op, cc_dep1, cc_dep2, cc_dep3);
         vf = arm64g_calculate_flag_v(cc_op, cc_dep1, cc_dep2, cc_dep3);
         zf = arm64g_calculate_flag_z(cc_op, cc_dep1, cc_dep2, cc_dep3);
         return inv ^ (1 & ~(zf | (nf ^ vf)));

      case ARM64CondAL:    
      case ARM64CondNV:    
         return 1;

      default:
         
         vex_printf("arm64g_calculate_condition(ARM64)"
                    "( %llu, %llu, 0x%llx, 0x%llx, 0x%llx )\n",
                    cond, cc_op, cc_dep1, cc_dep2, cc_dep3 );
         vpanic("armg_calculate_condition(ARM64)");
   }
}


ULong arm64g_dirtyhelper_MRS_CNTVCT_EL0 ( void )
{
#  if defined(__aarch64__) && !defined(__arm__)
   ULong w = 0x5555555555555555ULL; /* overwritten */
   __asm__ __volatile__("mrs %0, cntvct_el0" : "=r"(w));
   return w;
#  else
   return 0ULL;
#  endif
}




static Bool isU64 ( IRExpr* e, ULong n )
{
   return
      toBool( e->tag == Iex_Const
              && e->Iex.Const.con->tag == Ico_U64
              && e->Iex.Const.con->Ico.U64 == n );
}

IRExpr* guest_arm64_spechelper ( const HChar* function_name,
                                 IRExpr** args,
                                 IRStmt** precedingStmts,
                                 Int      n_precedingStmts )
{
#  define unop(_op,_a1) IRExpr_Unop((_op),(_a1))
#  define binop(_op,_a1,_a2) IRExpr_Binop((_op),(_a1),(_a2))
#  define mkU64(_n) IRExpr_Const(IRConst_U64(_n))
#  define mkU8(_n)  IRExpr_Const(IRConst_U8(_n))

   Int i, arity = 0;
   for (i = 0; args[i]; i++)
      arity++;

   

   if (vex_streq(function_name, "arm64g_calculate_condition")) {

      IRExpr *cond_n_op, *cc_dep1, *cc_dep2  ; 
      vassert(arity == 4);
      cond_n_op = args[0]; 
      cc_dep1   = args[1];
      cc_dep2   = args[2];
      

      

      
      if (isU64(cond_n_op, (ARM64CondEQ << 4) | ARM64G_CC_OP_SUB64)) {
         
         return unop(Iop_1Uto64,
                     binop(Iop_CmpEQ64, cc_dep1, cc_dep2));
      }
      if (isU64(cond_n_op, (ARM64CondNE << 4) | ARM64G_CC_OP_SUB64)) {
         
         return unop(Iop_1Uto64,
                     binop(Iop_CmpNE64, cc_dep1, cc_dep2));
      }

      
      if (isU64(cond_n_op, (ARM64CondCS << 4) | ARM64G_CC_OP_SUB64)) {
         return unop(Iop_1Uto64,
                     binop(Iop_CmpLE64U, cc_dep2, cc_dep1));
      }
      if (isU64(cond_n_op, (ARM64CondCC << 4) | ARM64G_CC_OP_SUB64)) {
         
         return unop(Iop_1Uto64,
                     binop(Iop_CmpLT64U, cc_dep1, cc_dep2));
      }

      
      if (isU64(cond_n_op, (ARM64CondLS << 4) | ARM64G_CC_OP_SUB64)) {
         
         return unop(Iop_1Uto64,
                     binop(Iop_CmpLE64U, cc_dep1, cc_dep2));
      }
      if (isU64(cond_n_op, (ARM64CondHI << 4) | ARM64G_CC_OP_SUB64)) {
         return unop(Iop_1Uto64,
                     binop(Iop_CmpLT64U, cc_dep2, cc_dep1));
      }

      
      if (isU64(cond_n_op, (ARM64CondLT << 4) | ARM64G_CC_OP_SUB64)) {
         
         return unop(Iop_1Uto64,
                     binop(Iop_CmpLT64S, cc_dep1, cc_dep2));
      }
      if (isU64(cond_n_op, (ARM64CondGE << 4) | ARM64G_CC_OP_SUB64)) {
         return unop(Iop_1Uto64,
                     binop(Iop_CmpLE64S, cc_dep2, cc_dep1));
      }

      
      if (isU64(cond_n_op, (ARM64CondGT << 4) | ARM64G_CC_OP_SUB64)) {
         return unop(Iop_1Uto64,
                     binop(Iop_CmpLT64S, cc_dep2, cc_dep1));
      }
      if (isU64(cond_n_op, (ARM64CondLE << 4) | ARM64G_CC_OP_SUB64)) {
         
         return unop(Iop_1Uto64,
                     binop(Iop_CmpLE64S, cc_dep1, cc_dep2));
      }

      

      
      if (isU64(cond_n_op, (ARM64CondEQ << 4) | ARM64G_CC_OP_SUB32)) {
         
         return unop(Iop_1Uto64,
                     binop(Iop_CmpEQ32, unop(Iop_64to32, cc_dep1),
                                        unop(Iop_64to32, cc_dep2)));
      }
      if (isU64(cond_n_op, (ARM64CondNE << 4) | ARM64G_CC_OP_SUB32)) {
         
         return unop(Iop_1Uto64,
                     binop(Iop_CmpNE32, unop(Iop_64to32, cc_dep1),
                                        unop(Iop_64to32, cc_dep2)));
      }

      
      if (isU64(cond_n_op, (ARM64CondCS << 4) | ARM64G_CC_OP_SUB32)) {
         return unop(Iop_1Uto64,
                     binop(Iop_CmpLE32U, unop(Iop_64to32, cc_dep2),
                                         unop(Iop_64to32, cc_dep1)));
      }
      if (isU64(cond_n_op, (ARM64CondCC << 4) | ARM64G_CC_OP_SUB32)) {
         
         return unop(Iop_1Uto64,
                     binop(Iop_CmpLT32U, unop(Iop_64to32, cc_dep1),
                                         unop(Iop_64to32, cc_dep2)));
      }

      
      if (isU64(cond_n_op, (ARM64CondLS << 4) | ARM64G_CC_OP_SUB32)) {
         
         return unop(Iop_1Uto64,
                     binop(Iop_CmpLE32U, unop(Iop_64to32, cc_dep1),
                                         unop(Iop_64to32, cc_dep2)));
      }
      if (isU64(cond_n_op, (ARM64CondHI << 4) | ARM64G_CC_OP_SUB32)) {
         return unop(Iop_1Uto64,
                     binop(Iop_CmpLT32U, unop(Iop_64to32, cc_dep2),
                                         unop(Iop_64to32, cc_dep1)));
      }

      
      if (isU64(cond_n_op, (ARM64CondLT << 4) | ARM64G_CC_OP_SUB32)) {
         
         return unop(Iop_1Uto64,
                     binop(Iop_CmpLT32S, unop(Iop_64to32, cc_dep1),
                                         unop(Iop_64to32, cc_dep2)));
      }
      if (isU64(cond_n_op, (ARM64CondGE << 4) | ARM64G_CC_OP_SUB32)) {
         return unop(Iop_1Uto64,
                     binop(Iop_CmpLE32S, unop(Iop_64to32, cc_dep2),
                                         unop(Iop_64to32, cc_dep1)));
      }

      
      if (isU64(cond_n_op, (ARM64CondGT << 4) | ARM64G_CC_OP_SUB32)) {
         return unop(Iop_1Uto64,
                     binop(Iop_CmpLT32S, unop(Iop_64to32, cc_dep2), 
                                         unop(Iop_64to32, cc_dep1)));
      }
      if (isU64(cond_n_op, (ARM64CondLE << 4) | ARM64G_CC_OP_SUB32)) {
         
         return unop(Iop_1Uto64,
                     binop(Iop_CmpLE32S, unop(Iop_64to32, cc_dep1),
                                         unop(Iop_64to32, cc_dep2)));
      }


      

      if (isU64(cond_n_op, (ARM64CondEQ << 4) | ARM64G_CC_OP_COPY)) {
         
         return binop(Iop_And64,
                      binop(Iop_Shr64, cc_dep1,
                                       mkU8(ARM64G_CC_SHIFT_Z)),
                      mkU64(1));
      }
      if (isU64(cond_n_op, (ARM64CondNE << 4) | ARM64G_CC_OP_COPY)) {
         
         return binop(Iop_And64,
                      binop(Iop_Xor64,
                            binop(Iop_Shr64, cc_dep1,
                                             mkU8(ARM64G_CC_SHIFT_Z)),
                            mkU64(1)),
                      mkU64(1));
      }

   }


#  undef unop
#  undef binop
#  undef mkU64
#  undef mkU8

   return NULL;
}




ULong LibVEX_GuestARM64_get_nzcv ( const VexGuestARM64State* vex_state )
{
   ULong nzcv = 0;
   
   nzcv |= arm64g_calculate_flags_nzcv(
               vex_state->guest_CC_OP,
               vex_state->guest_CC_DEP1,
               vex_state->guest_CC_DEP2,
               vex_state->guest_CC_NDEP
            );
   vassert(0 == (nzcv & 0xFFFFFFFF0FFFFFFFULL));
   return nzcv;
}

ULong LibVEX_GuestARM64_get_fpsr ( const VexGuestARM64State* vex_state )
{
   UInt w32 = vex_state->guest_QCFLAG[0] | vex_state->guest_QCFLAG[1]
              | vex_state->guest_QCFLAG[2] | vex_state->guest_QCFLAG[3];
   ULong fpsr = 0;
   
   if (w32 != 0)
      fpsr |= (1 << 27);
   return fpsr;
}

void LibVEX_GuestARM64_set_fpsr ( VexGuestARM64State* vex_state,
                                  ULong fpsr )
{
   
   vex_state->guest_QCFLAG[0] = (UInt)((fpsr >> 27) & 1);
   vex_state->guest_QCFLAG[1] = 0;
   vex_state->guest_QCFLAG[2] = 0;
   vex_state->guest_QCFLAG[3] = 0;
}

void LibVEX_GuestARM64_initialise ( VexGuestARM64State* vex_state )
{
   vex_bzero(vex_state, sizeof(*vex_state));
   vex_state->guest_CC_OP   = ARM64G_CC_OP_COPY;
}



Bool guest_arm64_state_requires_precise_mem_exns (
        Int minoff, Int maxoff, VexRegisterUpdates pxControl
     )
{
   Int xsp_min = offsetof(VexGuestARM64State, guest_XSP);
   Int xsp_max = xsp_min + 8 - 1;
   Int pc_min  = offsetof(VexGuestARM64State, guest_PC);
   Int pc_max  = pc_min + 8 - 1;

   if (maxoff < xsp_min || minoff > xsp_max) {
      
      if (pxControl == VexRegUpdSpAtMemAccess)
         return False; 
   } else {
      return True;
   }

   if (maxoff < pc_min || minoff > pc_max) {
      
   } else {
      return True;
   }

   
   Int x29_min = offsetof(VexGuestARM64State, guest_X29);
   Int x29_max = x29_min + 8 - 1;

   if (maxoff < x29_min || minoff > x29_max) {
      
   } else {
      return True;
   }

   
   Int x30_min = offsetof(VexGuestARM64State, guest_X30);
   Int x30_max = x30_min + 8 - 1;

   if (maxoff < x30_min || minoff > x30_max) {
      
   } else {
      return True;
   }

   return False;
}


#define ALWAYSDEFD(field)                             \
    { offsetof(VexGuestARM64State, field),            \
      (sizeof ((VexGuestARM64State*)0)->field) }
VexGuestLayout
   arm64Guest_layout 
      = { 
          
          .total_sizeB = sizeof(VexGuestARM64State),

          
          .offset_SP = offsetof(VexGuestARM64State,guest_XSP),
          .sizeof_SP = 8,

          
          .offset_IP = offsetof(VexGuestARM64State,guest_PC),
          .sizeof_IP = 8,

          .n_alwaysDefd = 9,

          .alwaysDefd
             = {  ALWAYSDEFD(guest_PC),
                  ALWAYSDEFD(guest_CC_OP),
                  ALWAYSDEFD(guest_CC_NDEP),
                  ALWAYSDEFD(guest_EMNOTE),
                  ALWAYSDEFD(guest_CMSTART),
                  ALWAYSDEFD(guest_CMLEN),
                  ALWAYSDEFD(guest_NRADDR),
                  ALWAYSDEFD(guest_IP_AT_SYSCALL),
                  ALWAYSDEFD(guest_TPIDR_EL0)
               }
        };


