

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

#include "libvex_basictypes.h"
#include "libvex_ir.h"
#include "libvex.h"

#include "ir_match.h"
#include "main_util.h"
#include "main_globals.h"
#include "host_generic_regs.h"
#include "host_generic_simd64.h"
#include "host_ppc_defs.h"

#define HRcGPR(_mode64) ((_mode64) ? HRcInt64 : HRcInt32)







static IRExpr* unop ( IROp op, IRExpr* a )
{
   return IRExpr_Unop(op, a);
}

static IRExpr* mkU32 ( UInt i )
{
   return IRExpr_Const(IRConst_U32(i));
}

static IRExpr* bind ( Int binder )
{
   return IRExpr_Binder(binder);
}

static Bool isZeroU8 ( IRExpr* e )
{
   return e->tag == Iex_Const
          && e->Iex.Const.con->tag == Ico_U8
          && e->Iex.Const.con->Ico.U8 == 0;
}



 
typedef
   struct {
      
      IRTypeEnv* type_env;
                              
      HReg*    vregmapLo;     
      HReg*    vregmapMedLo;  
      HReg*    vregmapMedHi;  
      HReg*    vregmapHi;     
      Int      n_vregmap;

      
      UInt         hwcaps;

      Bool         mode64;

      const VexAbiInfo*  vbi;   

      Bool         chainingAllowed;
      Addr64       max_ga;

      
      HInstrArray* code;
      Int          vreg_ctr;

      IRExpr*      previous_rm;
   }
   ISelEnv;
 
 
static HReg lookupIRTemp ( ISelEnv* env, IRTemp tmp )
{
   vassert(tmp >= 0);
   vassert(tmp < env->n_vregmap);
   return env->vregmapLo[tmp];
}

static void lookupIRTempPair ( HReg* vrHI, HReg* vrLO,
                               ISelEnv* env, IRTemp tmp )
{
   vassert(tmp >= 0);
   vassert(tmp < env->n_vregmap);
   vassert(! hregIsInvalid(env->vregmapMedLo[tmp]));
   *vrLO = env->vregmapLo[tmp];
   *vrHI = env->vregmapMedLo[tmp];
}

static void lookupIRTempQuad ( HReg* vrHi, HReg* vrMedHi, HReg* vrMedLo,
                               HReg* vrLo, ISelEnv* env, IRTemp tmp )
{
   vassert(!env->mode64);
   vassert(tmp >= 0);
   vassert(tmp < env->n_vregmap);
   vassert(! hregIsInvalid(env->vregmapMedLo[tmp]));
   *vrHi    = env->vregmapHi[tmp];
   *vrMedHi = env->vregmapMedHi[tmp];
   *vrMedLo = env->vregmapMedLo[tmp];
   *vrLo    = env->vregmapLo[tmp];
}

static void addInstr ( ISelEnv* env, PPCInstr* instr )
{
   addHInstr(env->code, instr);
   if (vex_traceflags & VEX_TRACE_VCODE) {
      ppPPCInstr(instr, env->mode64);
      vex_printf("\n");
   }
}

static HReg newVRegI ( ISelEnv* env )
{
   HReg reg
      = mkHReg(True, HRcGPR(env->mode64), 0, env->vreg_ctr);
   env->vreg_ctr++;
   return reg;
}

static HReg newVRegF ( ISelEnv* env )
{
   HReg reg = mkHReg(True, HRcFlt64, 0, env->vreg_ctr);
   env->vreg_ctr++;
   return reg;
}

static HReg newVRegV ( ISelEnv* env )
{
   HReg reg = mkHReg(True, HRcVec128, 0, env->vreg_ctr);
   env->vreg_ctr++;
   return reg;
}



static HReg          iselWordExpr_R_wrk ( ISelEnv* env, IRExpr* e,
                                          IREndness IEndianess );
static HReg          iselWordExpr_R     ( ISelEnv* env, IRExpr* e,
                                          IREndness IEndianess );

static PPCRH*        iselWordExpr_RH_wrk ( ISelEnv* env, 
                                           Bool syned, IRExpr* e,
                                           IREndness IEndianess );
static PPCRH*        iselWordExpr_RH     ( ISelEnv* env, 
                                           Bool syned, IRExpr* e,
                                           IREndness IEndianess );

static PPCRI*        iselWordExpr_RI_wrk ( ISelEnv* env, IRExpr* e,
                                           IREndness IEndianess );
static PPCRI*        iselWordExpr_RI     ( ISelEnv* env, IRExpr* e,
                                           IREndness IEndianess );

static PPCRH*        iselWordExpr_RH5u_wrk ( ISelEnv* env, IRExpr* e,
                                             IREndness IEndianess );
static PPCRH*        iselWordExpr_RH5u     ( ISelEnv* env, IRExpr* e,
                                             IREndness IEndianess );

static PPCRH*        iselWordExpr_RH6u_wrk ( ISelEnv* env, IRExpr* e,
                                             IREndness IEndianess );
static PPCRH*        iselWordExpr_RH6u     ( ISelEnv* env, IRExpr* e,
                                             IREndness IEndianess );

static PPCAMode*     iselWordExpr_AMode_wrk ( ISelEnv* env, IRExpr* e,
                                              IRType xferTy,
                                              IREndness IEndianess );
static PPCAMode*     iselWordExpr_AMode     ( ISelEnv* env, IRExpr* e,
                                              IRType xferTy,
                                              IREndness IEndianess );

static void iselInt128Expr_to_32x4_wrk ( HReg* rHi, HReg* rMedHi,
                                         HReg* rMedLo, HReg* rLo,
                                         ISelEnv* env, IRExpr* e,
                                         IREndness IEndianess );
static void iselInt128Expr_to_32x4     ( HReg* rHi, HReg* rMedHi,
                                         HReg* rMedLo, HReg* rLo,
                                         ISelEnv* env, IRExpr* e,
                                         IREndness IEndianess );


static void          iselInt64Expr_wrk ( HReg* rHi, HReg* rLo,
                                         ISelEnv* env, IRExpr* e,
                                         IREndness IEndianess );
static void          iselInt64Expr     ( HReg* rHi, HReg* rLo,
                                         ISelEnv* env, IRExpr* e,
                                         IREndness IEndianess );

static void          iselInt128Expr_wrk ( HReg* rHi, HReg* rLo, 
                                          ISelEnv* env, IRExpr* e,
                                          IREndness IEndianess );

static void          iselInt128Expr     ( HReg* rHi, HReg* rLo, 
                                          ISelEnv* env, IRExpr* e,
                                          IREndness IEndianess );

static PPCCondCode   iselCondCode_wrk ( ISelEnv* env, IRExpr* e,
                                        IREndness IEndianess );
static PPCCondCode   iselCondCode     ( ISelEnv* env, IRExpr* e,
                                        IREndness IEndianess );

static HReg          iselDblExpr_wrk ( ISelEnv* env, IRExpr* e,
                                       IREndness IEndianess );
static HReg          iselDblExpr     ( ISelEnv* env, IRExpr* e,
                                       IREndness IEndianess );

static HReg          iselFltExpr_wrk ( ISelEnv* env, IRExpr* e,
                                       IREndness IEndianess );
static HReg          iselFltExpr     ( ISelEnv* env, IRExpr* e,
                                       IREndness IEndianess );

static HReg          iselVecExpr_wrk ( ISelEnv* env, IRExpr* e,
                                       IREndness IEndianess );
static HReg          iselVecExpr     ( ISelEnv* env, IRExpr* e,
                                       IREndness IEndianess );

static HReg          iselDfp32Expr_wrk ( ISelEnv* env, IRExpr* e,
                                         IREndness IEndianess );
static HReg          iselDfp32Expr     ( ISelEnv* env, IRExpr* e,
                                         IREndness IEndianess );
static HReg          iselDfp64Expr_wrk ( ISelEnv* env, IRExpr* e,
                                         IREndness IEndianess );
static HReg          iselDfp64Expr     ( ISelEnv* env, IRExpr* e,
                                         IREndness IEndianess );

static void iselDfp128Expr_wrk ( HReg* rHi, HReg* rLo, ISelEnv* env,
                                 IRExpr* e, IREndness IEndianess );
static void iselDfp128Expr     ( HReg* rHi, HReg* rLo, ISelEnv* env,
                                 IRExpr* e, IREndness IEndianess );



static PPCInstr* mk_iMOVds_RR ( HReg r_dst, HReg r_src )
{
   vassert(hregClass(r_dst) == hregClass(r_src));
   vassert(hregClass(r_src) ==  HRcInt32 ||
           hregClass(r_src) ==  HRcInt64);
   return PPCInstr_Alu(Palu_OR, r_dst, r_src, PPCRH_Reg(r_src));
}


static void add_to_sp ( ISelEnv* env, UInt n )
{
   HReg sp = StackFramePtr(env->mode64);
   vassert(n <= 1024 && (n%16) == 0);
   addInstr(env, PPCInstr_Alu( Palu_ADD, sp, sp,
                               PPCRH_Imm(True,toUShort(n)) ));
}

static void sub_from_sp ( ISelEnv* env, UInt n )
{
   HReg sp = StackFramePtr(env->mode64);
   vassert(n <= 1024 && (n%16) == 0);
   addInstr(env, PPCInstr_Alu( Palu_SUB, sp, sp,
                               PPCRH_Imm(True,toUShort(n)) ));
}

static HReg get_sp_aligned16 ( ISelEnv* env )
{
   HReg       r = newVRegI(env);
   HReg align16 = newVRegI(env);
   addInstr(env, mk_iMOVds_RR(r, StackFramePtr(env->mode64)));
   
   addInstr(env, PPCInstr_Alu( Palu_ADD, r, r,
                               PPCRH_Imm(True,toUShort(16)) ));
   
   addInstr(env,
            PPCInstr_LI(align16, 0xFFFFFFFFFFFFFFF0ULL, env->mode64));
   addInstr(env, PPCInstr_Alu(Palu_AND, r,r, PPCRH_Reg(align16)));
   return r;
}



static HReg mk_LoadRR32toFPR ( ISelEnv* env,
                               HReg r_srcHi, HReg r_srcLo )
{
   HReg fr_dst = newVRegF(env);
   PPCAMode *am_addr0, *am_addr1;

   vassert(!env->mode64);
   vassert(hregClass(r_srcHi) == HRcInt32);
   vassert(hregClass(r_srcLo) == HRcInt32);

   sub_from_sp( env, 16 );        
   am_addr0 = PPCAMode_IR( 0, StackFramePtr(env->mode64) );
   am_addr1 = PPCAMode_IR( 4, StackFramePtr(env->mode64) );

   
   addInstr(env, PPCInstr_Store( 4, am_addr0, r_srcHi, env->mode64 ));
   addInstr(env, PPCInstr_Store( 4, am_addr1, r_srcLo, env->mode64 ));

   
   addInstr(env, PPCInstr_FpLdSt(True, 8, fr_dst, am_addr0));
   
   add_to_sp( env, 16 );          
   return fr_dst;
}

static HReg mk_LoadR64toFPR ( ISelEnv* env, HReg r_src )
{
   HReg fr_dst = newVRegF(env);
   PPCAMode *am_addr0;

   vassert(env->mode64);
   vassert(hregClass(r_src) == HRcInt64);

   sub_from_sp( env, 16 );        
   am_addr0 = PPCAMode_IR( 0, StackFramePtr(env->mode64) );

   
   addInstr(env, PPCInstr_Store( 8, am_addr0, r_src, env->mode64 ));

   
   addInstr(env, PPCInstr_FpLdSt(True, 8, fr_dst, am_addr0));
   
   add_to_sp( env, 16 );          
   return fr_dst;
}



static PPCAMode* advance4 ( ISelEnv* env, PPCAMode* am )
{
   PPCAMode* am4 = dopyPPCAMode( am );
   if (am4->tag == Pam_IR 
       && am4->Pam.IR.index + 4 <= 32767) {
      am4->Pam.IR.index += 4;
   } else {
      vpanic("advance4(ppc,host)");
   }
   return am4;
}


static
PPCAMode* genGuestArrayOffset ( ISelEnv* env, IRRegArray* descr,
                                IRExpr* off, Int bias, IREndness IEndianess )
{
   HReg rtmp, roff;
   Int  elemSz = sizeofIRType(descr->elemTy);
   Int  nElems = descr->nElems;
   Int  shift  = 0;


   if (nElems != 16 && nElems != 32)
      vpanic("genGuestArrayOffset(ppc host)(1)");

   switch (elemSz) {
      case 4:  shift = 2; break;
      case 8:  shift = 3; break;
      default: vpanic("genGuestArrayOffset(ppc host)(2)");
   }

   if (bias < -100 || bias > 100) 
      vpanic("genGuestArrayOffset(ppc host)(3)");
   if (descr->base < 0 || descr->base > 5000) 
      vpanic("genGuestArrayOffset(ppc host)(4)");

   roff = iselWordExpr_R(env, off, IEndianess);
   rtmp = newVRegI(env);
   addInstr(env, PPCInstr_Alu(
                    Palu_ADD, 
                    rtmp, roff, 
                    PPCRH_Imm(True, toUShort(bias))));
   addInstr(env, PPCInstr_Alu(
                    Palu_AND, 
                    rtmp, rtmp, 
                    PPCRH_Imm(False, toUShort(nElems-1))));
   addInstr(env, PPCInstr_Shft(
                    Pshft_SHL, 
                    env->mode64 ? False : True,
                    rtmp, rtmp, 
                    PPCRH_Imm(False, toUShort(shift))));
   addInstr(env, PPCInstr_Alu(
                    Palu_ADD, 
                    rtmp, rtmp, 
                    PPCRH_Imm(True, toUShort(descr->base))));
   return
      PPCAMode_RR( GuestStatePtr(env->mode64), rtmp );
}



static
Bool mightRequireFixedRegs ( IRExpr* e )
{
   switch (e->tag) {
   case Iex_RdTmp: case Iex_Const: case Iex_Get: 
      return False;
   default:
      return True;
   }
}



static
void doHelperCall ( UInt*   stackAdjustAfterCall,
                    RetLoc* retloc,
                    ISelEnv* env,
                    IRExpr* guard,
                    IRCallee* cee, IRType retTy, IRExpr** args,
                    IREndness IEndianess)
{
   PPCCondCode cc;
   HReg        argregs[PPC_N_REGPARMS];
   HReg        tmpregs[PPC_N_REGPARMS];
   Bool        go_fast;
   Int         n_args, i, argreg;
   UInt        argiregs;
   Bool        mode64 = env->mode64;

   
   *stackAdjustAfterCall = 0;
   *retloc               = mk_RetLoc_INVALID();

   UInt nVECRETs = 0;
   UInt nBBPTRs  = 0;



   n_args = 0;
   for (i = 0; args[i]; i++)
      n_args++;

   if (n_args > PPC_N_REGPARMS) {
      vpanic("doHelperCall(PPC): cannot currently handle > 8 args");
      
   }

   vassert(PPC_N_REGPARMS == 8);
   
   argregs[0] = hregPPC_GPR3(mode64);
   argregs[1] = hregPPC_GPR4(mode64);
   argregs[2] = hregPPC_GPR5(mode64);
   argregs[3] = hregPPC_GPR6(mode64);
   argregs[4] = hregPPC_GPR7(mode64);
   argregs[5] = hregPPC_GPR8(mode64);
   argregs[6] = hregPPC_GPR9(mode64);
   argregs[7] = hregPPC_GPR10(mode64);
   argiregs = 0;

   tmpregs[0] = tmpregs[1] = tmpregs[2] =
   tmpregs[3] = tmpregs[4] = tmpregs[5] =
   tmpregs[6] = tmpregs[7] = INVALID_HREG;


   go_fast = True;

   if (retTy == Ity_V128 || retTy == Ity_V256)
      go_fast = False;

   if (go_fast && guard) {
      if (guard->tag == Iex_Const 
          && guard->Iex.Const.con->tag == Ico_U1
          && guard->Iex.Const.con->Ico.U1 == True) {
         
      } else {
         
         go_fast = False;
      }
   }

   if (go_fast) {
      for (i = 0; i < n_args; i++) {
         IRExpr* arg = args[i];
         if (UNLIKELY(arg->tag == Iex_BBPTR)) {
            
         } 
         else if (UNLIKELY(arg->tag == Iex_VECRET)) {
            vpanic("doHelperCall(PPC): invalid IR");
         }
         else if (mightRequireFixedRegs(arg)) {
            go_fast = False;
            break;
         }
      }
   }


   if (go_fast) {

      
      argreg = 0;

      for (i = 0; i < n_args; i++) {
         IRExpr* arg = args[i];
         vassert(argreg < PPC_N_REGPARMS);

         if (arg->tag == Iex_BBPTR) {
            argiregs |= (1 << (argreg+3));
            addInstr(env, mk_iMOVds_RR( argregs[argreg],
                                        GuestStatePtr(mode64) ));
            argreg++;
         } else {
            vassert(arg->tag != Iex_VECRET);
            IRType ty = typeOfIRExpr(env->type_env, arg);
            vassert(ty == Ity_I32 || ty == Ity_I64);
            if (!mode64) {
               if (ty == Ity_I32) { 
                  argiregs |= (1 << (argreg+3));
                  addInstr(env,
                           mk_iMOVds_RR( argregs[argreg],
                                         iselWordExpr_R(env, arg,
							IEndianess) ));
               } else { 
                  HReg rHi, rLo;
                  if ((argreg%2) == 1)
                                 
                     argreg++;   
                  vassert(argreg < PPC_N_REGPARMS-1);
                  iselInt64Expr(&rHi,&rLo, env, arg, IEndianess);
                  argiregs |= (1 << (argreg+3));
                  addInstr(env, mk_iMOVds_RR( argregs[argreg++], rHi ));
                  argiregs |= (1 << (argreg+3));
                  addInstr(env, mk_iMOVds_RR( argregs[argreg], rLo));
               }
            } else { 
               argiregs |= (1 << (argreg+3));
               addInstr(env, mk_iMOVds_RR( argregs[argreg],
                                           iselWordExpr_R(env, arg,
                                                          IEndianess) ));
            }
            argreg++;
         } 
      }

      
      cc = mk_PPCCondCode( Pct_ALWAYS, Pcf_NONE );

   } else {

      
      argreg = 0;

      HReg r_vecRetAddr = INVALID_HREG;
      if (retTy == Ity_V128) {
         r_vecRetAddr = newVRegI(env);
         sub_from_sp(env, 512);
         addInstr(env, mk_iMOVds_RR( r_vecRetAddr, StackFramePtr(mode64) ));
         sub_from_sp(env, 512);
      }
      else if (retTy == Ity_V256) {
         vassert(0); 
         r_vecRetAddr = newVRegI(env);
         sub_from_sp(env, 512);
         addInstr(env, mk_iMOVds_RR( r_vecRetAddr, StackFramePtr(mode64) ));
         sub_from_sp(env, 512);
      }

      vassert(n_args >= 0 && n_args <= 8);
      for (i = 0; i < n_args; i++) {
         IRExpr* arg = args[i];
         vassert(argreg < PPC_N_REGPARMS);
         if (UNLIKELY(arg->tag == Iex_BBPTR)) {
            tmpregs[argreg] = newVRegI(env);
            addInstr(env, mk_iMOVds_RR( tmpregs[argreg],
                                        GuestStatePtr(mode64) ));
            nBBPTRs++;
         }
         else if (UNLIKELY(arg->tag == Iex_VECRET)) {
            vassert(!hregIsInvalid(r_vecRetAddr));
            tmpregs[i] = r_vecRetAddr;
            nVECRETs++;
         }
         else {
            IRType ty = typeOfIRExpr(env->type_env, arg);
            vassert(ty == Ity_I32 || ty == Ity_I64);
            if (!mode64) {
               if (ty == Ity_I32) { 
                  tmpregs[argreg] = iselWordExpr_R(env, arg, IEndianess);
               } else { 
                  HReg rHi, rLo;
                  if ((argreg%2) == 1)
                                
                     argreg++;  
                  vassert(argreg < PPC_N_REGPARMS-1);
                  iselInt64Expr(&rHi,&rLo, env, arg, IEndianess);
                  tmpregs[argreg++] = rHi;
                  tmpregs[argreg]   = rLo;
               }
            } else { 
               tmpregs[argreg] = iselWordExpr_R(env, arg, IEndianess);
            }
         }
         argreg++;
      }

      cc = mk_PPCCondCode( Pct_ALWAYS, Pcf_NONE );
      if (guard) {
         if (guard->tag == Iex_Const 
             && guard->Iex.Const.con->tag == Ico_U1
             && guard->Iex.Const.con->Ico.U1 == True) {
            
         } else {
            cc = iselCondCode( env, guard, IEndianess );
         }
      }

      
      for (i = 0; i < argreg; i++) {
         if (hregIsInvalid(tmpregs[i]))  
            continue;
         argiregs |= (1 << (i+3));
         addInstr( env, mk_iMOVds_RR( argregs[i], tmpregs[i] ) );
      }

   }

   if (retTy == Ity_V128 || retTy == Ity_V256) {
      vassert(nVECRETs == 1);
   } else {
      vassert(nVECRETs == 0);
   }

   vassert(nBBPTRs == 0 || nBBPTRs == 1);

   vassert(*stackAdjustAfterCall == 0);
   vassert(is_RetLoc_INVALID(*retloc));
   switch (retTy) {
      case Ity_INVALID:
         
         *retloc = mk_RetLoc_simple(RLPri_None);
         break;
      case Ity_I64:
         *retloc = mk_RetLoc_simple(mode64 ? RLPri_Int : RLPri_2Int);
         break;
      case Ity_I32: case Ity_I16: case Ity_I8:
         *retloc = mk_RetLoc_simple(RLPri_Int);
         break;
      case Ity_V128:
         *retloc = mk_RetLoc_spRel(RLPri_V128SpRel, 512);
         *stackAdjustAfterCall = 1024;
         break;
      case Ity_V256:
         vassert(0); 
         
         *retloc = mk_RetLoc_spRel(RLPri_V256SpRel, 512);
         *stackAdjustAfterCall = 1024;
         break;
      default:
         vassert(0);
   }


   Addr64 target = mode64 ? (Addr)cee->addr
                          : toUInt((Addr)(cee->addr));
   addInstr(env, PPCInstr_Call( cc, target, argiregs, *retloc ));
}




static HReg roundModeIRtoPPC ( ISelEnv* env, HReg r_rmIR )
{
   HReg r_rmPPC = newVRegI(env);
   HReg r_tmp1  = newVRegI(env);
   HReg r_tmp2  = newVRegI(env);

   vassert(hregClass(r_rmIR) == HRcGPR(env->mode64));

   
   
   
   
   

   addInstr(env, PPCInstr_Shft(Pshft_SHL, True,
                               r_tmp1, r_rmIR, PPCRH_Imm(False,1)));

   addInstr( env, PPCInstr_Alu( Palu_AND,
                                r_tmp2, r_tmp1, PPCRH_Imm( False, 3 ) ) );

   addInstr( env, PPCInstr_Alu( Palu_XOR,
                                r_rmPPC, r_rmIR, PPCRH_Reg( r_tmp2 ) ) );

   return r_rmPPC;
}


static
void _set_FPU_rounding_mode ( ISelEnv* env, IRExpr* mode, Bool dfp_rm,
                              IREndness IEndianess )
{
   HReg fr_src = newVRegF(env);
   HReg r_src;

   vassert(typeOfIRExpr(env->type_env,mode) == Ity_I32);
   
   
   if (env->previous_rm
       && env->previous_rm->tag == Iex_RdTmp
       && mode->tag == Iex_RdTmp
       && env->previous_rm->Iex.RdTmp.tmp == mode->Iex.RdTmp.tmp) {
      
      vassert(typeOfIRExpr(env->type_env, env->previous_rm) == Ity_I32);
      return;
   }

   
   env->previous_rm = mode;


   
   r_src = roundModeIRtoPPC( env, iselWordExpr_R(env, mode, IEndianess) );

   
   if (env->mode64) {
      if (dfp_rm) {
         HReg r_tmp1 = newVRegI( env );
         addInstr( env,
                   PPCInstr_Shft( Pshft_SHL, False,
                                  r_tmp1, r_src, PPCRH_Imm( False, 32 ) ) );
         fr_src = mk_LoadR64toFPR( env, r_tmp1 );
      } else {
         fr_src = mk_LoadR64toFPR( env, r_src ); 
      }
   } else {
      if (dfp_rm) {
         HReg r_zero = newVRegI( env );
         addInstr( env, PPCInstr_LI( r_zero, 0, env->mode64 ) );
         fr_src = mk_LoadRR32toFPR( env, r_src, r_zero );
      } else {
         fr_src = mk_LoadRR32toFPR( env, r_src, r_src ); 
      }
   }

   
   addInstr(env, PPCInstr_FpLdFPSCR( fr_src, dfp_rm ));
}

static void set_FPU_rounding_mode ( ISelEnv* env, IRExpr* mode,
                                    IREndness IEndianess )
{
   _set_FPU_rounding_mode(env, mode, False, IEndianess);
}

static void set_FPU_DFP_rounding_mode ( ISelEnv* env, IRExpr* mode,
                                        IREndness IEndianess )
{
   _set_FPU_rounding_mode(env, mode, True, IEndianess);
}



static HReg generate_zeroes_V128 ( ISelEnv* env )
{
   HReg dst = newVRegV(env);
   addInstr(env, PPCInstr_AvBinary(Pav_XOR, dst, dst, dst));
   return dst;
}

static HReg generate_ones_V128 ( ISelEnv* env )
{
   HReg dst = newVRegV(env);
   PPCVI5s * src = PPCVI5s_Imm(-1);
   addInstr(env, PPCInstr_AvSplat(8, dst, src));
   return dst;
}


static HReg mk_AvDuplicateRI( ISelEnv* env, IRExpr* e, IREndness IEndianess )
{
   HReg   r_src;
   HReg   dst = newVRegV(env);
   PPCRI* ri  = iselWordExpr_RI(env, e, IEndianess);
   IRType ty  = typeOfIRExpr(env->type_env,e);
   UInt   sz  = (ty == Ity_I8) ? 8 : (ty == Ity_I16) ? 16 : 32;
   vassert(ty == Ity_I8 || ty == Ity_I16 || ty == Ity_I32);

   
   if (ri->tag == Pri_Imm) {
      Int simm32 = (Int)ri->Pri.Imm;

      
      if (simm32 >= -32 && simm32 <= 31) {
         Char simm6 = (Char)simm32;
         if (simm6 > 15) {           
            HReg v1 = newVRegV(env);
            HReg v2 = newVRegV(env);
            addInstr(env, PPCInstr_AvSplat(sz, v1, PPCVI5s_Imm(-16)));
            addInstr(env, PPCInstr_AvSplat(sz, v2, PPCVI5s_Imm(simm6-16)));
            addInstr(env,
               (sz== 8) ? PPCInstr_AvBin8x16(Pav_SUBU, dst, v2, v1) :
               (sz==16) ? PPCInstr_AvBin16x8(Pav_SUBU, dst, v2, v1)
                        : PPCInstr_AvBin32x4(Pav_SUBU, dst, v2, v1) );
            return dst;
         }
         if (simm6 < -16) {          
            HReg v1 = newVRegV(env);
            HReg v2 = newVRegV(env);
            addInstr(env, PPCInstr_AvSplat(sz, v1, PPCVI5s_Imm(-16)));
            addInstr(env, PPCInstr_AvSplat(sz, v2, PPCVI5s_Imm(simm6+16)));
            addInstr(env,
               (sz== 8) ? PPCInstr_AvBin8x16(Pav_ADDU, dst, v2, v1) :
               (sz==16) ? PPCInstr_AvBin16x8(Pav_ADDU, dst, v2, v1)
                        : PPCInstr_AvBin32x4(Pav_ADDU, dst, v2, v1) );
            return dst;
         }
         
         addInstr(env, PPCInstr_AvSplat(sz, dst, PPCVI5s_Imm(simm6)));
         return dst;
      }

      
      r_src = newVRegI(env);
      addInstr(env, PPCInstr_LI(r_src, (Long)simm32, env->mode64));
   }
   else {
      r_src = ri->Pri.Reg;
   }

   {
      
      HReg r_aligned16;
      PPCAMode *am_offset, *am_offset_zero;

      sub_from_sp( env, 32 );     
      
      r_aligned16 = get_sp_aligned16( env );

      Int i;
      Int stride = (sz == 8) ? 1 : (sz == 16) ? 2 : 4;
      UChar num_bytes_to_store = stride;
      am_offset_zero = PPCAMode_IR( 0, r_aligned16 );
      am_offset = am_offset_zero;
      for (i = 0; i < 16; i+=stride, am_offset = PPCAMode_IR( i, r_aligned16)) {
         addInstr(env, PPCInstr_Store( num_bytes_to_store, am_offset, r_src, env->mode64 ));
      }

      
      addInstr(env, PPCInstr_AvLdSt( True, 16, dst, am_offset_zero ) );
      add_to_sp( env, 32 );       

      return dst;
   }
}


static HReg isNan ( ISelEnv* env, HReg vSrc, IREndness IEndianess )
{
   HReg zeros, msk_exp, msk_mnt, expt, mnts, vIsNan;
 
   vassert(hregClass(vSrc) == HRcVec128);

   zeros   = mk_AvDuplicateRI(env, mkU32(0), IEndianess);
   msk_exp = mk_AvDuplicateRI(env, mkU32(0x7F800000), IEndianess);
   msk_mnt = mk_AvDuplicateRI(env, mkU32(0x7FFFFF), IEndianess);
   expt    = newVRegV(env);
   mnts    = newVRegV(env);
   vIsNan  = newVRegV(env); 


   addInstr(env, PPCInstr_AvBinary(Pav_AND, expt, vSrc, msk_exp));
   addInstr(env, PPCInstr_AvBin32x4(Pav_CMPEQU, expt, expt, msk_exp));
   addInstr(env, PPCInstr_AvBinary(Pav_AND, mnts, vSrc, msk_mnt));
   addInstr(env, PPCInstr_AvBin32x4(Pav_CMPGTU, mnts, mnts, zeros));
   addInstr(env, PPCInstr_AvBinary(Pav_AND, vIsNan, expt, mnts));
   return vIsNan;
}




static HReg iselWordExpr_R ( ISelEnv* env, IRExpr* e, IREndness IEndianess )
{
   HReg r = iselWordExpr_R_wrk(env, e, IEndianess);
   
#  if 0
   vex_printf("\n"); ppIRExpr(e); vex_printf("\n");
#  endif

   vassert(hregClass(r) == HRcGPR(env->mode64));
   vassert(hregIsVirtual(r));
   return r;
}

static HReg iselWordExpr_R_wrk ( ISelEnv* env, IRExpr* e,
                                 IREndness IEndianess )
{
   Bool mode64 = env->mode64;
   MatchInfo mi;
   DECLARE_PATTERN(p_32to1_then_1Uto8);

   IRType ty = typeOfIRExpr(env->type_env,e);
   vassert(ty == Ity_I8 || ty == Ity_I16 ||
           ty == Ity_I32 || ((ty == Ity_I64) && mode64));

   switch (e->tag) {

   
   case Iex_RdTmp:
      return lookupIRTemp(env, e->Iex.RdTmp.tmp);

   
   case Iex_Load: {
      HReg      r_dst;
      PPCAMode* am_addr;
      if (e->Iex.Load.end != IEndianess)
         goto irreducible;
      r_dst   = newVRegI(env);
      am_addr = iselWordExpr_AMode( env, e->Iex.Load.addr, ty,
                                    IEndianess );
      addInstr(env, PPCInstr_Load( toUChar(sizeofIRType(ty)), 
                                   r_dst, am_addr, mode64 ));
      return r_dst;
      
   }

   
   case Iex_Binop: {
      PPCAluOp  aluOp;
      PPCShftOp shftOp;

      
      switch (e->Iex.Binop.op) {
      case Iop_Add8: case Iop_Add16: case Iop_Add32: case Iop_Add64:
         aluOp = Palu_ADD; break;
      case Iop_Sub8: case Iop_Sub16: case Iop_Sub32: case Iop_Sub64:
         aluOp = Palu_SUB; break;
      case Iop_And8: case Iop_And16: case Iop_And32: case Iop_And64:
         aluOp = Palu_AND; break;
      case Iop_Or8:  case Iop_Or16:  case Iop_Or32:  case Iop_Or64:
         aluOp = Palu_OR; break;
      case Iop_Xor8: case Iop_Xor16: case Iop_Xor32: case Iop_Xor64:
         aluOp = Palu_XOR; break;
      default:
         aluOp = Palu_INVALID; break;
      }
      if (aluOp != Palu_INVALID) {
         HReg   r_dst   = newVRegI(env);
         HReg   r_srcL  = iselWordExpr_R(env, e->Iex.Binop.arg1, IEndianess);
         PPCRH* ri_srcR = NULL;
         
         switch (aluOp) {
         case Palu_ADD: case Palu_SUB:
            ri_srcR = iselWordExpr_RH(env, True, 
                                      e->Iex.Binop.arg2, IEndianess);
            break;
         case Palu_AND: case Palu_OR: case Palu_XOR:
            ri_srcR = iselWordExpr_RH(env, False,
                                      e->Iex.Binop.arg2, IEndianess);
            break;
         default:
            vpanic("iselWordExpr_R_wrk-aluOp-arg2");
         }
         addInstr(env, PPCInstr_Alu(aluOp, r_dst, r_srcL, ri_srcR));
         return r_dst;
      }

      
      switch (e->Iex.Binop.op) {
      case Iop_Shl8: case Iop_Shl16: case Iop_Shl32: case Iop_Shl64:
         shftOp = Pshft_SHL; break;
      case Iop_Shr8: case Iop_Shr16: case Iop_Shr32: case Iop_Shr64:
         shftOp = Pshft_SHR; break;
      case Iop_Sar8: case Iop_Sar16: case Iop_Sar32: case Iop_Sar64:
         shftOp = Pshft_SAR; break;
      default:
         shftOp = Pshft_INVALID; break;
      }
      
      if (shftOp != Pshft_INVALID) {
         HReg   r_dst   = newVRegI(env);
         HReg   r_srcL  = iselWordExpr_R(env, e->Iex.Binop.arg1, IEndianess);
         PPCRH* ri_srcR = NULL;
         
         switch (shftOp) {
         case Pshft_SHL: case Pshft_SHR: case Pshft_SAR:
            if (!mode64)
               ri_srcR = iselWordExpr_RH5u(env, e->Iex.Binop.arg2, IEndianess);
            else
               ri_srcR = iselWordExpr_RH6u(env, e->Iex.Binop.arg2, IEndianess);
            break;
         default:
            vpanic("iselIntExpr_R_wrk-shftOp-arg2");
         }
         
         if (shftOp == Pshft_SHR || shftOp == Pshft_SAR) {
            if (ty == Ity_I8 || ty == Ity_I16) {
               PPCRH* amt = PPCRH_Imm(False,
                                      toUShort(ty == Ity_I8 ? 24 : 16));
               HReg   tmp = newVRegI(env);
               addInstr(env, PPCInstr_Shft(Pshft_SHL,
                                           True,
                                           tmp, r_srcL, amt));
               addInstr(env, PPCInstr_Shft(shftOp,
                                           True,
                                           tmp, tmp,    amt));
               r_srcL = tmp;
               vassert(0); 
            }
         }
         if (ty == Ity_I64) {
            vassert(mode64);
            addInstr(env, PPCInstr_Shft(shftOp, False,
                                        r_dst, r_srcL, ri_srcR));
         } else {
            addInstr(env, PPCInstr_Shft(shftOp, True,
                                        r_dst, r_srcL, ri_srcR));
         }
         return r_dst;
      }

      
      if (e->Iex.Binop.op == Iop_DivS32 || 
          e->Iex.Binop.op == Iop_DivU32 ||
          e->Iex.Binop.op == Iop_DivS32E ||
          e->Iex.Binop.op == Iop_DivU32E) {
         Bool syned  = toBool((e->Iex.Binop.op == Iop_DivS32) || (e->Iex.Binop.op == Iop_DivS32E));
         HReg r_dst  = newVRegI(env);
         HReg r_srcL = iselWordExpr_R(env, e->Iex.Binop.arg1, IEndianess);
         HReg r_srcR = iselWordExpr_R(env, e->Iex.Binop.arg2, IEndianess);
         addInstr( env,
                      PPCInstr_Div( ( ( e->Iex.Binop.op == Iop_DivU32E )
                                             || ( e->Iex.Binop.op == Iop_DivS32E ) ) ? True
                                                                                     : False,
                                    syned,
                                    True,
                                    r_dst,
                                    r_srcL,
                                    r_srcR ) );
         return r_dst;
      }
      if (e->Iex.Binop.op == Iop_DivS64 || 
          e->Iex.Binop.op == Iop_DivU64 || e->Iex.Binop.op == Iop_DivS64E
          || e->Iex.Binop.op == Iop_DivU64E ) {
         Bool syned  = toBool((e->Iex.Binop.op == Iop_DivS64) ||(e->Iex.Binop.op == Iop_DivS64E));
         HReg r_dst  = newVRegI(env);
         HReg r_srcL = iselWordExpr_R(env, e->Iex.Binop.arg1, IEndianess);
         HReg r_srcR = iselWordExpr_R(env, e->Iex.Binop.arg2, IEndianess);
         vassert(mode64);
         addInstr( env,
                      PPCInstr_Div( ( ( e->Iex.Binop.op == Iop_DivS64E )
                                             || ( e->Iex.Binop.op
                                                      == Iop_DivU64E ) ) ? True
                                                                         : False,
                                    syned,
                                    False,
                                    r_dst,
                                    r_srcL,
                                    r_srcR ) );
         return r_dst;
      }

      
      if (e->Iex.Binop.op == Iop_Mul32
          || e->Iex.Binop.op == Iop_Mul64) {
         Bool syned       = False;
         Bool sz32        = (e->Iex.Binop.op != Iop_Mul64);
         HReg r_dst       = newVRegI(env);
         HReg r_srcL      = iselWordExpr_R(env, e->Iex.Binop.arg1, IEndianess);
         HReg r_srcR      = iselWordExpr_R(env, e->Iex.Binop.arg2, IEndianess);
         addInstr(env, PPCInstr_MulL(syned, False, sz32,
                                     r_dst, r_srcL, r_srcR));
         return r_dst;
      }      

      
      if (mode64
          && (e->Iex.Binop.op == Iop_MullU32
              || e->Iex.Binop.op == Iop_MullS32)) {
         HReg tLo    = newVRegI(env);
         HReg tHi    = newVRegI(env);
         HReg r_dst  = newVRegI(env);
         Bool syned  = toBool(e->Iex.Binop.op == Iop_MullS32);
         HReg r_srcL = iselWordExpr_R(env, e->Iex.Binop.arg1, IEndianess);
         HReg r_srcR = iselWordExpr_R(env, e->Iex.Binop.arg2, IEndianess);
         addInstr(env, PPCInstr_MulL(False, 
                                     False, True,
                                     tLo, r_srcL, r_srcR));
         addInstr(env, PPCInstr_MulL(syned,
                                     True, True,
                                     tHi, r_srcL, r_srcR));
         addInstr(env, PPCInstr_Shft(Pshft_SHL, False,
                                     r_dst, tHi, PPCRH_Imm(False,32)));
         addInstr(env, PPCInstr_Alu(Palu_OR,
                                    r_dst, r_dst, PPCRH_Reg(tLo)));
         return r_dst;
      }

      
      if (e->Iex.Binop.op == Iop_CmpORD32S
          || e->Iex.Binop.op == Iop_CmpORD32U) {
         Bool   syned = toBool(e->Iex.Binop.op == Iop_CmpORD32S);
         HReg   dst   = newVRegI(env);
         HReg   srcL  = iselWordExpr_R(env, e->Iex.Binop.arg1, IEndianess);
         PPCRH* srcR  = iselWordExpr_RH(env, syned, e->Iex.Binop.arg2,
                                        IEndianess);
         addInstr(env, PPCInstr_Cmp(syned, True,
                                    7, srcL, srcR));
         addInstr(env, PPCInstr_MfCR(dst));
         addInstr(env, PPCInstr_Alu(Palu_AND, dst, dst,
                                    PPCRH_Imm(False,7<<1)));
         return dst;
      }

      if (e->Iex.Binop.op == Iop_CmpORD64S
          || e->Iex.Binop.op == Iop_CmpORD64U) {
         Bool   syned = toBool(e->Iex.Binop.op == Iop_CmpORD64S);
         HReg   dst   = newVRegI(env);
         HReg   srcL  = iselWordExpr_R(env, e->Iex.Binop.arg1, IEndianess);
         PPCRH* srcR  = iselWordExpr_RH(env, syned, e->Iex.Binop.arg2,
                                        IEndianess);
         vassert(mode64);
         addInstr(env, PPCInstr_Cmp(syned, False,
                                    7, srcL, srcR));
         addInstr(env, PPCInstr_MfCR(dst));
         addInstr(env, PPCInstr_Alu(Palu_AND, dst, dst,
                                    PPCRH_Imm(False,7<<1)));
         return dst;
      }

      if (e->Iex.Binop.op == Iop_Max32U) {
         HReg        r1   = iselWordExpr_R(env, e->Iex.Binop.arg1, IEndianess);
         HReg        r2   = iselWordExpr_R(env, e->Iex.Binop.arg2, IEndianess);
         HReg        rdst = newVRegI(env);
         PPCCondCode cc   = mk_PPCCondCode( Pct_TRUE, Pcf_7LT );
         addInstr(env, mk_iMOVds_RR(rdst, r1));
         addInstr(env, PPCInstr_Cmp(False, True,
                                    7, rdst, PPCRH_Reg(r2)));
         addInstr(env, PPCInstr_CMov(cc, rdst, PPCRI_Reg(r2)));
         return rdst;
      }

      if (e->Iex.Binop.op == Iop_32HLto64) {
         HReg   r_Hi  = iselWordExpr_R(env, e->Iex.Binop.arg1, IEndianess);
         HReg   r_Lo  = iselWordExpr_R(env, e->Iex.Binop.arg2, IEndianess);
         HReg   r_Tmp = newVRegI(env);
         HReg   r_dst = newVRegI(env);
         HReg   msk   = newVRegI(env);
         vassert(mode64);
         
         addInstr(env, PPCInstr_Shft(Pshft_SHL, False,
                                     r_dst, r_Hi, PPCRH_Imm(False,32)));
         addInstr(env, PPCInstr_LI(msk, 0xFFFFFFFF, mode64));
         addInstr(env, PPCInstr_Alu( Palu_AND, r_Tmp, r_Lo,
                                     PPCRH_Reg(msk) ));
         addInstr(env, PPCInstr_Alu( Palu_OR, r_dst, r_dst,
                                     PPCRH_Reg(r_Tmp) ));
         return r_dst;
      }

      if ((e->Iex.Binop.op == Iop_CmpF64) ||
          (e->Iex.Binop.op == Iop_CmpD64) ||
          (e->Iex.Binop.op == Iop_CmpD128)) {
         HReg fr_srcL;
         HReg fr_srcL_lo;
         HReg fr_srcR;
         HReg fr_srcR_lo;

         HReg r_ccPPC   = newVRegI(env);
         HReg r_ccIR    = newVRegI(env);
         HReg r_ccIR_b0 = newVRegI(env);
         HReg r_ccIR_b2 = newVRegI(env);
         HReg r_ccIR_b6 = newVRegI(env);

         if (e->Iex.Binop.op == Iop_CmpF64) {
            fr_srcL = iselDblExpr(env, e->Iex.Binop.arg1, IEndianess);
            fr_srcR = iselDblExpr(env, e->Iex.Binop.arg2, IEndianess);
            addInstr(env, PPCInstr_FpCmp(r_ccPPC, fr_srcL, fr_srcR));

         } else if (e->Iex.Binop.op == Iop_CmpD64) {
            fr_srcL = iselDfp64Expr(env, e->Iex.Binop.arg1, IEndianess);
            fr_srcR = iselDfp64Expr(env, e->Iex.Binop.arg2, IEndianess);
            addInstr(env, PPCInstr_Dfp64Cmp(r_ccPPC, fr_srcL, fr_srcR));

         } else {    
            iselDfp128Expr(&fr_srcL, &fr_srcL_lo, env, e->Iex.Binop.arg1,
                           IEndianess);
            iselDfp128Expr(&fr_srcR, &fr_srcR_lo, env, e->Iex.Binop.arg2,
                           IEndianess);
            addInstr(env, PPCInstr_Dfp128Cmp(r_ccPPC, fr_srcL, fr_srcL_lo,
                                             fr_srcR, fr_srcR_lo));
         }


         
         addInstr(env, PPCInstr_Shft(Pshft_SHR, True,
                                     r_ccIR_b0, r_ccPPC,
                                     PPCRH_Imm(False,0x3)));
         addInstr(env, PPCInstr_Alu(Palu_OR,  r_ccIR_b0,
                                    r_ccPPC,   PPCRH_Reg(r_ccIR_b0)));
         addInstr(env, PPCInstr_Alu(Palu_AND, r_ccIR_b0,
                                    r_ccIR_b0, PPCRH_Imm(False,0x1)));
         
         
         addInstr(env, PPCInstr_Shft(Pshft_SHL, True,
                                     r_ccIR_b2, r_ccPPC,
                                     PPCRH_Imm(False,0x2)));
         addInstr(env, PPCInstr_Alu(Palu_AND, r_ccIR_b2,
                                    r_ccIR_b2, PPCRH_Imm(False,0x4)));

         
         addInstr(env, PPCInstr_Shft(Pshft_SHR, True,
                                     r_ccIR_b6, r_ccPPC,
                                     PPCRH_Imm(False,0x1)));
         addInstr(env, PPCInstr_Alu(Palu_OR,  r_ccIR_b6,
                                    r_ccPPC, PPCRH_Reg(r_ccIR_b6)));
         addInstr(env, PPCInstr_Shft(Pshft_SHL, True,
                                     r_ccIR_b6, r_ccIR_b6,
                                     PPCRH_Imm(False,0x6)));
         addInstr(env, PPCInstr_Alu(Palu_AND, r_ccIR_b6,
                                    r_ccIR_b6, PPCRH_Imm(False,0x40)));

         
         addInstr(env, PPCInstr_Alu(Palu_OR, r_ccIR,
                                    r_ccIR_b0, PPCRH_Reg(r_ccIR_b2)));
         addInstr(env, PPCInstr_Alu(Palu_OR, r_ccIR,
                                    r_ccIR,    PPCRH_Reg(r_ccIR_b6)));
         return r_ccIR;
      }

      if ( e->Iex.Binop.op == Iop_F64toI32S ||
               e->Iex.Binop.op == Iop_F64toI32U ) {
         
         HReg      r1      = StackFramePtr(env->mode64);
         PPCAMode* zero_r1 = PPCAMode_IR( 0, r1 );
         HReg      fsrc    = iselDblExpr(env, e->Iex.Binop.arg2, IEndianess);
         HReg      ftmp    = newVRegF(env);
         HReg      idst    = newVRegI(env);

         
         set_FPU_rounding_mode( env, e->Iex.Binop.arg1, IEndianess );

         sub_from_sp( env, 16 );
         addInstr(env, PPCInstr_FpCftI(False, True,
                                       e->Iex.Binop.op == Iop_F64toI32S ? True
                                                                     : False,
                                       True,
                                       ftmp, fsrc));
         addInstr(env, PPCInstr_FpSTFIW(r1, ftmp));
         addInstr(env, PPCInstr_Load(4, idst, zero_r1, mode64));

         
         if (mode64)
            addInstr(env, PPCInstr_Unary(Pun_EXTSW, idst, idst));

         add_to_sp( env, 16 );

         
         
         return idst;
      }

      if (e->Iex.Binop.op == Iop_F64toI64S || e->Iex.Binop.op == Iop_F64toI64U ) {
         if (mode64) {
            HReg      r1      = StackFramePtr(env->mode64);
            PPCAMode* zero_r1 = PPCAMode_IR( 0, r1 );
            HReg      fsrc    = iselDblExpr(env, e->Iex.Binop.arg2,
                                            IEndianess);
            HReg      idst    = newVRegI(env);         
            HReg      ftmp    = newVRegF(env);

            
            set_FPU_rounding_mode( env, e->Iex.Binop.arg1, IEndianess );

            sub_from_sp( env, 16 );
            addInstr(env, PPCInstr_FpCftI(False, False,
                                          ( e->Iex.Binop.op == Iop_F64toI64S ) ? True
                                                                            : False,
                                          True, ftmp, fsrc));
            addInstr(env, PPCInstr_FpLdSt(False, 8, ftmp, zero_r1));
            addInstr(env, PPCInstr_Load(8, idst, zero_r1, True));
            add_to_sp( env, 16 );

            
            
            return idst;
         }
      }

      if (e->Iex.Binop.op == Iop_D64toI64S ) {
         HReg      r1      = StackFramePtr(env->mode64);
         PPCAMode* zero_r1 = PPCAMode_IR( 0, r1 );
         HReg      fr_src  = iselDfp64Expr(env, e->Iex.Binop.arg2, IEndianess);
         HReg      idst    = newVRegI(env);
         HReg      ftmp    = newVRegF(env);

         
         set_FPU_DFP_rounding_mode( env, e->Iex.Binop.arg1, IEndianess );
         addInstr(env, PPCInstr_Dfp64Unary(Pfp_DCTFIX, ftmp, fr_src));
         sub_from_sp( env, 16 );
         addInstr(env, PPCInstr_FpLdSt(False, 8, ftmp, zero_r1));
         addInstr(env, PPCInstr_Load(8, idst, zero_r1, mode64));

         add_to_sp( env, 16 );

         
         
         return idst;
      }

      if (e->Iex.Binop.op == Iop_D128toI64S ) {
         PPCFpOp fpop = Pfp_DCTFIXQ;
         HReg r_srcHi = newVRegF(env);
         HReg r_srcLo = newVRegF(env);
         HReg idst    = newVRegI(env);
         HReg ftmp    = newVRegF(env);
         PPCAMode* zero_r1 = PPCAMode_IR( 0, StackFramePtr(env->mode64) );

         set_FPU_DFP_rounding_mode( env, e->Iex.Binop.arg1, IEndianess );
         iselDfp128Expr(&r_srcHi, &r_srcLo, env, e->Iex.Binop.arg2,
                        IEndianess);
         addInstr(env, PPCInstr_DfpD128toD64(fpop, ftmp, r_srcHi, r_srcLo));

         
         sub_from_sp( env, 16 );
         addInstr(env, PPCInstr_FpLdSt(False, 8, ftmp, zero_r1));
         addInstr(env, PPCInstr_Load(8, idst, zero_r1, True));
         add_to_sp( env, 16 );
         return idst;
      }
      break;
   }

   
   case Iex_Unop: {
      IROp op_unop = e->Iex.Unop.op;

      
      DEFINE_PATTERN(p_32to1_then_1Uto8,
                     unop(Iop_1Uto8,unop(Iop_32to1,bind(0))));
      if (matchIRExpr(&mi,p_32to1_then_1Uto8,e)) {
         IRExpr* expr32 = mi.bindee[0];
         HReg r_dst = newVRegI(env);
         HReg r_src = iselWordExpr_R(env, expr32, IEndianess);
         addInstr(env, PPCInstr_Alu(Palu_AND, r_dst,
                                    r_src, PPCRH_Imm(False,1)));
         return r_dst;
      }

      
      {
         DECLARE_PATTERN(p_LDbe16_then_16Uto32);
         DEFINE_PATTERN(p_LDbe16_then_16Uto32,
                        unop(Iop_16Uto32,
                             IRExpr_Load(IEndianess,Ity_I16,bind(0))) );
         if (matchIRExpr(&mi,p_LDbe16_then_16Uto32,e)) {
            HReg r_dst = newVRegI(env);
            PPCAMode* amode
               = iselWordExpr_AMode( env, mi.bindee[0], Ity_I16,
                                     IEndianess );
            addInstr(env, PPCInstr_Load(2,r_dst,amode, mode64));
            return r_dst;
         }
      }

      switch (op_unop) {
      case Iop_8Uto16:
      case Iop_8Uto32:
      case Iop_8Uto64:
      case Iop_16Uto32:
      case Iop_16Uto64: {
         HReg   r_dst = newVRegI(env);
         HReg   r_src = iselWordExpr_R(env, e->Iex.Unop.arg, IEndianess);
         UShort mask  = toUShort(op_unop==Iop_16Uto64 ? 0xFFFF :
                                 op_unop==Iop_16Uto32 ? 0xFFFF : 0xFF);
         addInstr(env, PPCInstr_Alu(Palu_AND,r_dst,r_src,
                                    PPCRH_Imm(False,mask)));
         return r_dst;
      }
      case Iop_32Uto64: {
         HReg r_dst = newVRegI(env);
         HReg r_src = iselWordExpr_R(env, e->Iex.Unop.arg, IEndianess);
         vassert(mode64);
         addInstr(env,
                  PPCInstr_Shft(Pshft_SHL, False,
                                r_dst, r_src, PPCRH_Imm(False,32)));
         addInstr(env,
                  PPCInstr_Shft(Pshft_SHR, False,
                                r_dst, r_dst, PPCRH_Imm(False,32)));
         return r_dst;
      }
      case Iop_8Sto16:
      case Iop_8Sto32:
      case Iop_16Sto32: {
         HReg   r_dst = newVRegI(env);
         HReg   r_src = iselWordExpr_R(env, e->Iex.Unop.arg, IEndianess);
         UShort amt   = toUShort(op_unop==Iop_16Sto32 ? 16 : 24);
         addInstr(env,
                  PPCInstr_Shft(Pshft_SHL, True,
                                r_dst, r_src, PPCRH_Imm(False,amt)));
         addInstr(env,
                  PPCInstr_Shft(Pshft_SAR, True,
                                r_dst, r_dst, PPCRH_Imm(False,amt)));
         return r_dst;
      }
      case Iop_8Sto64:
      case Iop_16Sto64: {
         HReg   r_dst = newVRegI(env);
         HReg   r_src = iselWordExpr_R(env, e->Iex.Unop.arg, IEndianess);
         UShort amt   = toUShort(op_unop==Iop_8Sto64  ? 56 : 48);
         vassert(mode64);
         addInstr(env,
                  PPCInstr_Shft(Pshft_SHL, False,
                                r_dst, r_src, PPCRH_Imm(False,amt)));
         addInstr(env,
                  PPCInstr_Shft(Pshft_SAR, False,
                                r_dst, r_dst, PPCRH_Imm(False,amt)));
         return r_dst;
      }
      case Iop_32Sto64: {
         HReg   r_dst = newVRegI(env);
         HReg   r_src = iselWordExpr_R(env, e->Iex.Unop.arg, IEndianess);
	 vassert(mode64);
         addInstr(env,
                  PPCInstr_Shft(Pshft_SAR, True,
                                r_dst, r_src, PPCRH_Imm(False,0)));
         return r_dst;
      }
      case Iop_Not8:
      case Iop_Not16:
      case Iop_Not32:
      case Iop_Not64: {
         if (op_unop == Iop_Not64) vassert(mode64);
         HReg r_dst = newVRegI(env);
         HReg r_src = iselWordExpr_R(env, e->Iex.Unop.arg, IEndianess);
         addInstr(env, PPCInstr_Unary(Pun_NOT,r_dst,r_src));
         return r_dst;
      }
      case Iop_64HIto32: {
         if (!mode64) {
            HReg rHi, rLo;
            iselInt64Expr(&rHi,&rLo, env, e->Iex.Unop.arg, IEndianess);
            return rHi; 
         } else {
            HReg   r_dst = newVRegI(env);
            HReg   r_src = iselWordExpr_R(env, e->Iex.Unop.arg, IEndianess);
            addInstr(env,
                     PPCInstr_Shft(Pshft_SHR, False,
                                   r_dst, r_src, PPCRH_Imm(False,32)));
            return r_dst;
         }
      }
      case Iop_64to32: {
         if (!mode64) {
            HReg rHi, rLo;
            iselInt64Expr(&rHi,&rLo, env, e->Iex.Unop.arg, IEndianess);
            return rLo; 
         } else {
            
            return iselWordExpr_R(env, e->Iex.Unop.arg, IEndianess);
         }
      }
      case Iop_64to16: {
         if (mode64) { 
            return iselWordExpr_R(env, e->Iex.Unop.arg, IEndianess);
         }
         break; 
      }
      case Iop_16HIto8:
      case Iop_32HIto16: {
         HReg   r_dst = newVRegI(env);
         HReg   r_src = iselWordExpr_R(env, e->Iex.Unop.arg, IEndianess);
         UShort shift = toUShort(op_unop == Iop_16HIto8 ? 8 : 16);
         addInstr(env,
                  PPCInstr_Shft(Pshft_SHR, True,
                                r_dst, r_src, PPCRH_Imm(False,shift)));
         return r_dst;
      }
      case Iop_128HIto64: 
         if (mode64) {
            HReg rHi, rLo;
            iselInt128Expr(&rHi,&rLo, env, e->Iex.Unop.arg, IEndianess);
            return rHi; 
         }
         break;
      case Iop_128to64:
         if (mode64) {
            HReg rHi, rLo;
            iselInt128Expr(&rHi,&rLo, env, e->Iex.Unop.arg, IEndianess);
            return rLo; 
         }
         break;
      case Iop_1Uto64:
      case Iop_1Uto32:
      case Iop_1Uto8:
         if ((op_unop != Iop_1Uto64) || mode64) {
            HReg        r_dst = newVRegI(env);
            PPCCondCode cond  = iselCondCode(env, e->Iex.Unop.arg, IEndianess);
            addInstr(env, PPCInstr_Set(cond,r_dst));
            return r_dst;
         }
         break;
      case Iop_1Sto8:
      case Iop_1Sto16:
      case Iop_1Sto32: {
         
         HReg        r_dst = newVRegI(env);
         PPCCondCode cond  = iselCondCode(env, e->Iex.Unop.arg, IEndianess);
         addInstr(env, PPCInstr_Set(cond,r_dst));
         addInstr(env,
                  PPCInstr_Shft(Pshft_SHL, True,
                                r_dst, r_dst, PPCRH_Imm(False,31)));
         addInstr(env,
                  PPCInstr_Shft(Pshft_SAR, True,
                                r_dst, r_dst, PPCRH_Imm(False,31)));
         return r_dst;
      }
      case Iop_1Sto64: 
         if (mode64) {
            
            HReg        r_dst = newVRegI(env);
            PPCCondCode cond  = iselCondCode(env, e->Iex.Unop.arg, IEndianess);
            addInstr(env, PPCInstr_Set(cond,r_dst));
            addInstr(env, PPCInstr_Shft(Pshft_SHL, False,
                                        r_dst, r_dst, PPCRH_Imm(False,63)));
            addInstr(env, PPCInstr_Shft(Pshft_SAR, False,
                                        r_dst, r_dst, PPCRH_Imm(False,63)));
            return r_dst;
         }
         break;
      case Iop_Clz32:
      case Iop_Clz64: {
         HReg r_src, r_dst;
         PPCUnaryOp op_clz = (op_unop == Iop_Clz32) ? Pun_CLZ32 :
                                                      Pun_CLZ64;
         if (op_unop == Iop_Clz64 && !mode64)
            goto irreducible;
         
         r_dst = newVRegI(env);
         r_src = iselWordExpr_R(env, e->Iex.Unop.arg, IEndianess);
         addInstr(env, PPCInstr_Unary(op_clz,r_dst,r_src));
         return r_dst;
      }

      case Iop_Left8:
      case Iop_Left16:
      case Iop_Left32: 
      case Iop_Left64: {
         HReg r_src, r_dst;
         if (op_unop == Iop_Left64 && !mode64)
            goto irreducible;
         r_dst = newVRegI(env);
         r_src = iselWordExpr_R(env, e->Iex.Unop.arg, IEndianess);
         addInstr(env, PPCInstr_Unary(Pun_NEG,r_dst,r_src));
         addInstr(env, PPCInstr_Alu(Palu_OR, r_dst, r_dst, PPCRH_Reg(r_src)));
         return r_dst;
      }

      case Iop_CmpwNEZ32: {
         HReg r_dst = newVRegI(env);
         HReg r_src = iselWordExpr_R(env, e->Iex.Unop.arg, IEndianess);
         addInstr(env, PPCInstr_Unary(Pun_NEG,r_dst,r_src));
         addInstr(env, PPCInstr_Alu(Palu_OR, r_dst, r_dst, PPCRH_Reg(r_src)));
         addInstr(env, PPCInstr_Shft(Pshft_SAR, True, 
                                     r_dst, r_dst, PPCRH_Imm(False, 31)));
         return r_dst;
      }

      case Iop_CmpwNEZ64: {
         HReg r_dst = newVRegI(env);
         HReg r_src = iselWordExpr_R(env, e->Iex.Unop.arg, IEndianess);
         if (!mode64) goto irreducible;
         addInstr(env, PPCInstr_Unary(Pun_NEG,r_dst,r_src));
         addInstr(env, PPCInstr_Alu(Palu_OR, r_dst, r_dst, PPCRH_Reg(r_src)));
         addInstr(env, PPCInstr_Shft(Pshft_SAR, False, 
                                     r_dst, r_dst, PPCRH_Imm(False, 63)));
         return r_dst;
      }

      case Iop_V128to32: {
         HReg        r_aligned16;
         HReg        dst  = newVRegI(env);
         HReg        vec  = iselVecExpr(env, e->Iex.Unop.arg, IEndianess);
         PPCAMode *am_off0, *am_off_word0;
         sub_from_sp( env, 32 );     

         
         r_aligned16 = get_sp_aligned16( env );
         am_off0  = PPCAMode_IR( 0, r_aligned16 );

         if (IEndianess == Iend_LE)
            am_off_word0 = am_off0;
         else
            am_off_word0 = PPCAMode_IR( 12,r_aligned16 );

         
         addInstr(env,
                  PPCInstr_AvLdSt( False, 16, vec, am_off0 ));
         addInstr(env,
                  PPCInstr_Load( 4, dst, am_off_word0, mode64 ));

         add_to_sp( env, 32 );       
         return dst;
      }

      case Iop_V128to64:
      case Iop_V128HIto64: 
         if (mode64) {
            HReg     r_aligned16;
            HReg     dst = newVRegI(env);
            HReg     vec = iselVecExpr(env, e->Iex.Unop.arg, IEndianess);
            PPCAMode *am_off0, *am_off8, *am_off_arg;
            sub_from_sp( env, 32 );     

            
            r_aligned16 = get_sp_aligned16( env );
            am_off0 = PPCAMode_IR( 0, r_aligned16 );
            am_off8 = PPCAMode_IR( 8 ,r_aligned16 );

            
            addInstr(env,
                     PPCInstr_AvLdSt( False, 16, vec, am_off0 ));
            if (IEndianess == Iend_LE) {
               if (op_unop == Iop_V128HIto64)
                  am_off_arg = am_off8;
               else
                  am_off_arg = am_off0;
            } else {
               if (op_unop == Iop_V128HIto64)
                  am_off_arg = am_off0;
               else
                  am_off_arg = am_off8;
            }
            addInstr(env,
                     PPCInstr_Load( 
                        8, dst, 
                        am_off_arg,
                        mode64 ));

            add_to_sp( env, 32 );       
            return dst;
         }
         break;
      case Iop_16to8:
      case Iop_32to8:
      case Iop_32to16:
      case Iop_64to8:
         
         return iselWordExpr_R(env, e->Iex.Unop.arg, IEndianess);
         
      
      case Iop_ReinterpF64asI64: 
         if (mode64) {
            PPCAMode *am_addr;
            HReg fr_src = iselDblExpr(env, e->Iex.Unop.arg, IEndianess);
            HReg r_dst  = newVRegI(env);

            sub_from_sp( env, 16 );     
            am_addr = PPCAMode_IR( 0, StackFramePtr(mode64) );

            
            addInstr(env, PPCInstr_FpLdSt( False, 8,
                                           fr_src, am_addr ));
            
            addInstr(env, PPCInstr_Load( 8, r_dst, am_addr, mode64 ));

            add_to_sp( env, 16 );       
            return r_dst;
         }
         break;

      
      case Iop_ReinterpF32asI32: {
         PPCAMode *am_addr;
         HReg fr_src = iselFltExpr(env, e->Iex.Unop.arg, IEndianess);
         HReg r_dst  = newVRegI(env);

         sub_from_sp( env, 16 );     
         am_addr = PPCAMode_IR( 0, StackFramePtr(mode64) );

         
         addInstr(env, PPCInstr_FpLdSt( False, 4,
                                        fr_src, am_addr ));
         
         addInstr(env, PPCInstr_Load( 4, r_dst, am_addr, mode64 ));

         add_to_sp( env, 16 );       
         return r_dst;
      }
      break;

      case Iop_ReinterpD64asI64:
         if (mode64) {
            PPCAMode *am_addr;
            HReg fr_src = iselDfp64Expr(env, e->Iex.Unop.arg, IEndianess);
            HReg r_dst  = newVRegI(env);

            sub_from_sp( env, 16 );     
            am_addr = PPCAMode_IR( 0, StackFramePtr(mode64) );

            
            addInstr(env, PPCInstr_FpLdSt( False, 8,
                                           fr_src, am_addr ));
            
            addInstr(env, PPCInstr_Load( 8, r_dst, am_addr, mode64 ));
            add_to_sp( env, 16 );       
            return r_dst;
         } 
         break;

      case Iop_BCDtoDPB: {
         
         if (!mode64) break;

         PPCCondCode cc;
         UInt        argiregs;
         HReg        argregs[1];
         HReg        r_dst  = newVRegI(env);
         Int         argreg;

         argiregs = 0;
         argreg = 0;
         argregs[0] = hregPPC_GPR3(mode64);

         argiregs |= (1 << (argreg+3));
         addInstr(env, mk_iMOVds_RR( argregs[argreg++],
                                     iselWordExpr_R(env, e->Iex.Unop.arg,
                                                    IEndianess) ) );

         cc = mk_PPCCondCode( Pct_ALWAYS, Pcf_NONE );
         if (IEndianess == Iend_LE) {
             addInstr(env, PPCInstr_Call( cc, (Addr)h_calc_BCDtoDPB,
                                          argiregs,
                                          mk_RetLoc_simple(RLPri_Int)) );
         } else {
             HWord*      fdescr;
             fdescr = (HWord*)h_calc_BCDtoDPB;
             addInstr(env, PPCInstr_Call( cc, (Addr64)(fdescr[0]),
                                          argiregs,
                                          mk_RetLoc_simple(RLPri_Int)) );
         }

         addInstr(env, mk_iMOVds_RR(r_dst, argregs[0]));
         return r_dst;
      }

      case Iop_DPBtoBCD: {
         
         if (!mode64) break;

         PPCCondCode cc;
         UInt        argiregs;
         HReg        argregs[1];
         HReg        r_dst  = newVRegI(env);
         Int         argreg;

         argiregs = 0;
         argreg = 0;
         argregs[0] = hregPPC_GPR3(mode64);

         argiregs |= (1 << (argreg+3));
         addInstr(env, mk_iMOVds_RR( argregs[argreg++],
                                     iselWordExpr_R(env, e->Iex.Unop.arg,
                                                    IEndianess) ) );

         cc = mk_PPCCondCode( Pct_ALWAYS, Pcf_NONE );

        if (IEndianess == Iend_LE) {
            addInstr(env, PPCInstr_Call( cc, (Addr)h_calc_DPBtoBCD,
                                         argiregs, 
                                         mk_RetLoc_simple(RLPri_Int) ) );
	} else {
            HWord*      fdescr;
            fdescr = (HWord*)h_calc_DPBtoBCD;
            addInstr(env, PPCInstr_Call( cc, (Addr64)(fdescr[0]),
                                         argiregs,
                                         mk_RetLoc_simple(RLPri_Int) ) );
         }

         addInstr(env, mk_iMOVds_RR(r_dst, argregs[0]));
         return r_dst;
      }

      default: 
         break;
      }

     switch (e->Iex.Unop.op) {
        case Iop_ExtractExpD64: {

            HReg fr_dst = newVRegI(env);
            HReg fr_src = iselDfp64Expr(env, e->Iex.Unop.arg, IEndianess);
            HReg tmp    = newVRegF(env);
            PPCAMode* zero_r1 = PPCAMode_IR( 0, StackFramePtr(env->mode64) );
            addInstr(env, PPCInstr_Dfp64Unary(Pfp_DXEX, tmp, fr_src));

            
            sub_from_sp( env, 16 );
            addInstr(env, PPCInstr_FpLdSt(False, 8, tmp, zero_r1));
            addInstr(env, PPCInstr_Load(8, fr_dst, zero_r1, env->mode64));
            add_to_sp( env, 16 );
            return fr_dst;
         }
         case Iop_ExtractExpD128: {
            HReg fr_dst = newVRegI(env);
            HReg r_srcHi;
            HReg r_srcLo;
            HReg tmp    = newVRegF(env);
            PPCAMode* zero_r1 = PPCAMode_IR( 0, StackFramePtr(env->mode64) );

            iselDfp128Expr(&r_srcHi, &r_srcLo, env, e->Iex.Unop.arg,
                           IEndianess);
            addInstr(env, PPCInstr_ExtractExpD128(Pfp_DXEXQ, tmp,
                                                  r_srcHi, r_srcLo));

            sub_from_sp( env, 16 );
            addInstr(env, PPCInstr_FpLdSt(False, 8, tmp, zero_r1));
            addInstr(env, PPCInstr_Load(8, fr_dst, zero_r1, env->mode64));
            add_to_sp( env, 16 );
            return fr_dst;
         }
         default: 
            break;
      }

      break;
   }

   
   case Iex_Get: {
      if (ty == Ity_I8  || ty == Ity_I16 ||
          ty == Ity_I32 || ((ty == Ity_I64) && mode64)) {
         HReg r_dst = newVRegI(env);
         PPCAMode* am_addr = PPCAMode_IR( e->Iex.Get.offset,
                                          GuestStatePtr(mode64) );
         addInstr(env, PPCInstr_Load( toUChar(sizeofIRType(ty)), 
                                      r_dst, am_addr, mode64 ));
         return r_dst;
      }
      break;
   }

   case Iex_GetI: {
      PPCAMode* src_am
         = genGuestArrayOffset( env, e->Iex.GetI.descr,
                                e->Iex.GetI.ix, e->Iex.GetI.bias,
                                IEndianess );
      HReg r_dst = newVRegI(env);
      if (mode64 && ty == Ity_I64) {
         addInstr(env, PPCInstr_Load( toUChar(8),
                                      r_dst, src_am, mode64 ));
         return r_dst;
      }
      if ((!mode64) && ty == Ity_I32) {
         addInstr(env, PPCInstr_Load( toUChar(4),
                                      r_dst, src_am, mode64 ));
         return r_dst;
      }
      break;
   }

   
   case Iex_CCall: {
      vassert(ty == e->Iex.CCall.retty); 

      if (!(ty == Ity_I32 || (mode64 && ty == Ity_I64)))
         goto irreducible;

      
      UInt   addToSp = 0;
      RetLoc rloc    = mk_RetLoc_INVALID();
      doHelperCall( &addToSp, &rloc, env, NULL,
                    e->Iex.CCall.cee, e->Iex.CCall.retty, e->Iex.CCall.args,
                    IEndianess );
      vassert(is_sane_RetLoc(rloc));
      vassert(rloc.pri == RLPri_Int);
      vassert(addToSp == 0);

      
      HReg r_dst = newVRegI(env);
      addInstr(env, mk_iMOVds_RR(r_dst, hregPPC_GPR3(mode64)));
      return r_dst;
   }
      
   
   
   case Iex_Const: {
      Long l;
      HReg r_dst = newVRegI(env);
      IRConst* con = e->Iex.Const.con;
      switch (con->tag) {
         case Ico_U64: if (!mode64) goto irreducible;
                       l = (Long)            con->Ico.U64; break;
         case Ico_U32: l = (Long)(Int)       con->Ico.U32; break;
         case Ico_U16: l = (Long)(Int)(Short)con->Ico.U16; break;
         case Ico_U8:  l = (Long)(Int)(Char )con->Ico.U8;  break;
         default:      vpanic("iselIntExpr_R.const(ppc)");
      }
      addInstr(env, PPCInstr_LI(r_dst, (ULong)l, mode64));
      return r_dst;
   }

   
   case Iex_ITE: { 
      if ((ty == Ity_I8  || ty == Ity_I16 ||
           ty == Ity_I32 || ((ty == Ity_I64) && mode64)) &&
          typeOfIRExpr(env->type_env,e->Iex.ITE.cond) == Ity_I1) {
         PPCRI* r1    = iselWordExpr_RI(env, e->Iex.ITE.iftrue, IEndianess);
         HReg   r0    = iselWordExpr_R(env, e->Iex.ITE.iffalse, IEndianess);
         HReg   r_dst = newVRegI(env);
         addInstr(env, mk_iMOVds_RR(r_dst,r0));
         PPCCondCode cc = iselCondCode(env, e->Iex.ITE.cond, IEndianess);
         addInstr(env, PPCInstr_CMov(cc, r_dst, r1));
         return r_dst;
      }
      break;
   }
      
   default: 
      break;
   } 


   
 irreducible:
   ppIRExpr(e);
   vpanic("iselIntExpr_R(ppc): cannot reduce tree");
}





static Bool uInt_fits_in_16_bits ( UInt u ) 
{
   
   Int i = u & 0xFFFF;
   i <<= 16;
   i >>= 16;
   return toBool(u == (UInt)i);
}

static Bool uLong_fits_in_16_bits ( ULong u ) 
{
   
   Long i = u & 0xFFFFULL;
   i <<= 48;
   i >>= 48;
   return toBool(u == (ULong)i);
}

static Bool uLong_is_4_aligned ( ULong u )
{
   return toBool((u & 3ULL) == 0);
}

static Bool sane_AMode ( ISelEnv* env, PPCAMode* am )
{
   Bool mode64 = env->mode64;
   switch (am->tag) {
   case Pam_IR:
      return toBool( hregClass(am->Pam.IR.base) == HRcGPR(mode64) && 
                     hregIsVirtual(am->Pam.IR.base) && 
                     uInt_fits_in_16_bits(am->Pam.IR.index) );
   case Pam_RR:
      return toBool( hregClass(am->Pam.RR.base) == HRcGPR(mode64) && 
                     hregIsVirtual(am->Pam.RR.base) &&
                     hregClass(am->Pam.RR.index) == HRcGPR(mode64) &&
                     hregIsVirtual(am->Pam.RR.index) );
   default:
      vpanic("sane_AMode: unknown ppc amode tag");
   }
}

static 
PPCAMode* iselWordExpr_AMode ( ISelEnv* env, IRExpr* e, IRType xferTy,
                               IREndness IEndianess )
{
   PPCAMode* am = iselWordExpr_AMode_wrk(env, e, xferTy, IEndianess);
   vassert(sane_AMode(env, am));
   return am;
}

static PPCAMode* iselWordExpr_AMode_wrk ( ISelEnv* env, IRExpr* e,
                                          IRType xferTy, IREndness IEndianess )
{
   IRType ty = typeOfIRExpr(env->type_env,e);

   if (env->mode64) {

      Bool aligned4imm = toBool(xferTy == Ity_I32 || xferTy == Ity_I64);

      vassert(ty == Ity_I64);
   
      
      if (e->tag == Iex_Binop 
          && e->Iex.Binop.op == Iop_Add64
          && e->Iex.Binop.arg2->tag == Iex_Const
          && e->Iex.Binop.arg2->Iex.Const.con->tag == Ico_U64
          && (aligned4imm  ? uLong_is_4_aligned(e->Iex.Binop.arg2
                                                 ->Iex.Const.con->Ico.U64)
                           : True)
          && uLong_fits_in_16_bits(e->Iex.Binop.arg2
                                    ->Iex.Const.con->Ico.U64)) {
         return PPCAMode_IR( (Int)e->Iex.Binop.arg2->Iex.Const.con->Ico.U64,
                             iselWordExpr_R(env, e->Iex.Binop.arg1,
                                            IEndianess) );
      }
      
      
      if (e->tag == Iex_Binop 
          && e->Iex.Binop.op == Iop_Add64) {
         HReg r_base = iselWordExpr_R(env, e->Iex.Binop.arg1, IEndianess);
         HReg r_idx  = iselWordExpr_R(env, e->Iex.Binop.arg2, IEndianess);
         return PPCAMode_RR( r_idx, r_base );
      }

   } else {

      vassert(ty == Ity_I32);
   
      
      if (e->tag == Iex_Binop 
          && e->Iex.Binop.op == Iop_Add32
          && e->Iex.Binop.arg2->tag == Iex_Const
          && e->Iex.Binop.arg2->Iex.Const.con->tag == Ico_U32
          && uInt_fits_in_16_bits(e->Iex.Binop.arg2
                                   ->Iex.Const.con->Ico.U32)) {
         return PPCAMode_IR( (Int)e->Iex.Binop.arg2->Iex.Const.con->Ico.U32,
                             iselWordExpr_R(env, e->Iex.Binop.arg1,
                                            IEndianess) );
      }
      
      
      if (e->tag == Iex_Binop 
          && e->Iex.Binop.op == Iop_Add32) {
         HReg r_base = iselWordExpr_R(env, e->Iex.Binop.arg1, IEndianess);
         HReg r_idx  = iselWordExpr_R(env, e->Iex.Binop.arg2, IEndianess);
         return PPCAMode_RR( r_idx, r_base );
      }

   }

   return PPCAMode_IR( 0, iselWordExpr_R(env,e,IEndianess) );
}




static PPCRH* iselWordExpr_RH ( ISelEnv* env, Bool syned, IRExpr* e,
                                IREndness IEndianess )
{
  PPCRH* ri = iselWordExpr_RH_wrk(env, syned, e, IEndianess);
   
   switch (ri->tag) {
   case Prh_Imm:
      vassert(ri->Prh.Imm.syned == syned);
      if (syned)
         vassert(ri->Prh.Imm.imm16 != 0x8000);
      return ri;
   case Prh_Reg:
      vassert(hregClass(ri->Prh.Reg.reg) == HRcGPR(env->mode64));
      vassert(hregIsVirtual(ri->Prh.Reg.reg));
      return ri;
   default:
      vpanic("iselIntExpr_RH: unknown ppc RH tag");
   }
}

static PPCRH* iselWordExpr_RH_wrk ( ISelEnv* env, Bool syned, IRExpr* e,
                                    IREndness IEndianess )
{
   ULong u;
   Long  l;
   IRType ty = typeOfIRExpr(env->type_env,e);
   vassert(ty == Ity_I8  || ty == Ity_I16 ||
           ty == Ity_I32 || ((ty == Ity_I64) && env->mode64));

   
   if (e->tag == Iex_Const) {
      IRConst* con = e->Iex.Const.con;
      
      switch (con->tag) {
      
      case Ico_U64: vassert(env->mode64);
                    u =              con->Ico.U64; break;
      case Ico_U32: u = 0xFFFFFFFF & con->Ico.U32; break;
      case Ico_U16: u = 0x0000FFFF & con->Ico.U16; break;
      case Ico_U8:  u = 0x000000FF & con->Ico.U8; break;
      default:      vpanic("iselIntExpr_RH.Iex_Const(ppch)");
      }
      l = (Long)u;
      
      if (!syned && u <= 65535) {
         return PPCRH_Imm(False, toUShort(u & 0xFFFF));
      }
      if (syned && l >= -32767 && l <= 32767) {
         return PPCRH_Imm(True, toUShort(u & 0xFFFF));
      }
      
   }

   
   return PPCRH_Reg( iselWordExpr_R ( env, e, IEndianess ) );
}




static PPCRI* iselWordExpr_RI ( ISelEnv* env, IRExpr* e, IREndness IEndianess )
{
   PPCRI* ri = iselWordExpr_RI_wrk(env, e, IEndianess);
   
   switch (ri->tag) {
   case Pri_Imm:
      return ri;
   case Pri_Reg:
      vassert(hregClass(ri->Pri.Reg) == HRcGPR(env->mode64));
      vassert(hregIsVirtual(ri->Pri.Reg));
      return ri;
   default:
      vpanic("iselIntExpr_RI: unknown ppc RI tag");
   }
}

static PPCRI* iselWordExpr_RI_wrk ( ISelEnv* env, IRExpr* e,
                                    IREndness IEndianess )
{
   Long  l;
   IRType ty = typeOfIRExpr(env->type_env,e);
   vassert(ty == Ity_I8  || ty == Ity_I16 ||
           ty == Ity_I32 || ((ty == Ity_I64) && env->mode64));

   
   if (e->tag == Iex_Const) {
      IRConst* con = e->Iex.Const.con;
      switch (con->tag) {
      case Ico_U64: vassert(env->mode64);
                    l = (Long)            con->Ico.U64; break;
      case Ico_U32: l = (Long)(Int)       con->Ico.U32; break;
      case Ico_U16: l = (Long)(Int)(Short)con->Ico.U16; break;
      case Ico_U8:  l = (Long)(Int)(Char )con->Ico.U8;  break;
      default:      vpanic("iselIntExpr_RI.Iex_Const(ppch)");
      }
      return PPCRI_Imm((ULong)l);
   }

   
   return PPCRI_Reg( iselWordExpr_R ( env, e, IEndianess ) );
}




static PPCRH* iselWordExpr_RH5u ( ISelEnv* env, IRExpr* e,
                                  IREndness IEndianess )
{
   PPCRH* ri;
   vassert(!env->mode64);
   ri = iselWordExpr_RH5u_wrk(env, e, IEndianess);
   
   switch (ri->tag) {
   case Prh_Imm:
      vassert(ri->Prh.Imm.imm16 >= 1 && ri->Prh.Imm.imm16 <= 31);
      vassert(!ri->Prh.Imm.syned);
      return ri;
   case Prh_Reg:
      vassert(hregClass(ri->Prh.Reg.reg) == HRcGPR(env->mode64));
      vassert(hregIsVirtual(ri->Prh.Reg.reg));
      return ri;
   default:
      vpanic("iselIntExpr_RH5u: unknown ppc RI tag");
   }
}

static PPCRH* iselWordExpr_RH5u_wrk ( ISelEnv* env, IRExpr* e,
                                      IREndness IEndianess )
{
   IRType ty = typeOfIRExpr(env->type_env,e);
   vassert(ty == Ity_I8);

   
   if (e->tag == Iex_Const
       && e->Iex.Const.con->tag == Ico_U8
       && e->Iex.Const.con->Ico.U8 >= 1
       && e->Iex.Const.con->Ico.U8 <= 31) {
      return PPCRH_Imm(False, e->Iex.Const.con->Ico.U8);
   }

   
   return PPCRH_Reg( iselWordExpr_R ( env, e, IEndianess ) );
}




static PPCRH* iselWordExpr_RH6u ( ISelEnv* env, IRExpr* e,
                                  IREndness IEndianess )
{
   PPCRH* ri; 
   vassert(env->mode64);
   ri = iselWordExpr_RH6u_wrk(env, e, IEndianess);
   
   switch (ri->tag) {
   case Prh_Imm:
      vassert(ri->Prh.Imm.imm16 >= 1 && ri->Prh.Imm.imm16 <= 63);
      vassert(!ri->Prh.Imm.syned);
      return ri;
   case Prh_Reg:
      vassert(hregClass(ri->Prh.Reg.reg) == HRcGPR(env->mode64));
      vassert(hregIsVirtual(ri->Prh.Reg.reg));
      return ri;
   default:
      vpanic("iselIntExpr_RH6u: unknown ppc64 RI tag");
   }
}

static PPCRH* iselWordExpr_RH6u_wrk ( ISelEnv* env, IRExpr* e,
                                      IREndness IEndianess )
{
   IRType ty = typeOfIRExpr(env->type_env,e);
   vassert(ty == Ity_I8);

   
   if (e->tag == Iex_Const
       && e->Iex.Const.con->tag == Ico_U8
       && e->Iex.Const.con->Ico.U8 >= 1
       && e->Iex.Const.con->Ico.U8 <= 63) {
      return PPCRH_Imm(False, e->Iex.Const.con->Ico.U8);
   }

   
   return PPCRH_Reg( iselWordExpr_R ( env, e, IEndianess ) );
}




static PPCCondCode iselCondCode ( ISelEnv* env, IRExpr* e,
                                  IREndness IEndianess )
{
   
   return iselCondCode_wrk(env,e, IEndianess);
}

static PPCCondCode iselCondCode_wrk ( ISelEnv* env, IRExpr* e,
                                      IREndness IEndianess )
{
   vassert(e);
   vassert(typeOfIRExpr(env->type_env,e) == Ity_I1);

   
   if (e->tag == Iex_Const && e->Iex.Const.con->Ico.U1 == True) {
      
      HReg r_zero = newVRegI(env);
      addInstr(env, PPCInstr_LI(r_zero, 0, env->mode64));
      addInstr(env, PPCInstr_Cmp(False, True,
                                 7, r_zero, PPCRH_Reg(r_zero)));
      return mk_PPCCondCode( Pct_TRUE, Pcf_7EQ );
   }

   
   if (e->tag == Iex_Unop && e->Iex.Unop.op == Iop_Not1) {
      
      PPCCondCode cond = iselCondCode(env, e->Iex.Unop.arg, IEndianess);
      cond.test = invertCondTest(cond.test);
      return cond;
   }

   

   
   if (e->tag == Iex_Unop &&
       (e->Iex.Unop.op == Iop_32to1 || e->Iex.Unop.op == Iop_64to1)) {
      HReg src = iselWordExpr_R(env, e->Iex.Unop.arg, IEndianess);
      HReg tmp = newVRegI(env);
      
      addInstr(env, PPCInstr_Alu(Palu_AND, tmp,
                                 src, PPCRH_Imm(False,1)));
      addInstr(env, PPCInstr_Cmp(False, True,
                                 7, tmp, PPCRH_Imm(False,1)));
      return mk_PPCCondCode( Pct_TRUE, Pcf_7EQ );
   }

   

   
   
   
   if (e->tag == Iex_Unop
       && e->Iex.Unop.op == Iop_CmpNEZ8) {
      HReg arg = iselWordExpr_R(env, e->Iex.Unop.arg, IEndianess);
      HReg tmp = newVRegI(env);
      addInstr(env, PPCInstr_Alu(Palu_AND, tmp, arg,
                                 PPCRH_Imm(False,0xFF)));
      addInstr(env, PPCInstr_Cmp(False, True,
                                 7, tmp, PPCRH_Imm(False,0)));
      return mk_PPCCondCode( Pct_FALSE, Pcf_7EQ );
   }

   

   
   if (e->tag == Iex_Unop
       && e->Iex.Unop.op == Iop_CmpNEZ32) {
      HReg r1 = iselWordExpr_R(env, e->Iex.Unop.arg, IEndianess);
      addInstr(env, PPCInstr_Cmp(False, True,
                                 7, r1, PPCRH_Imm(False,0)));
      return mk_PPCCondCode( Pct_FALSE, Pcf_7EQ );
   }

   

   
   if (e->tag == Iex_Binop 
       && (e->Iex.Binop.op == Iop_CmpEQ32
           || e->Iex.Binop.op == Iop_CmpNE32
           || e->Iex.Binop.op == Iop_CmpLT32S
           || e->Iex.Binop.op == Iop_CmpLT32U
           || e->Iex.Binop.op == Iop_CmpLE32S
           || e->Iex.Binop.op == Iop_CmpLE32U)) {
      Bool syned = (e->Iex.Binop.op == Iop_CmpLT32S ||
                    e->Iex.Binop.op == Iop_CmpLE32S);
      HReg   r1  = iselWordExpr_R(env, e->Iex.Binop.arg1, IEndianess);
      PPCRH* ri2 = iselWordExpr_RH(env, syned, e->Iex.Binop.arg2, IEndianess);
      addInstr(env, PPCInstr_Cmp(syned, True,
                                 7, r1, ri2));

      switch (e->Iex.Binop.op) {
      case Iop_CmpEQ32:  return mk_PPCCondCode( Pct_TRUE,  Pcf_7EQ );
      case Iop_CmpNE32:  return mk_PPCCondCode( Pct_FALSE, Pcf_7EQ );
      case Iop_CmpLT32U: case Iop_CmpLT32S:
         return mk_PPCCondCode( Pct_TRUE,  Pcf_7LT );
      case Iop_CmpLE32U: case Iop_CmpLE32S:
         return mk_PPCCondCode( Pct_FALSE, Pcf_7GT );
      default: vpanic("iselCondCode(ppc): CmpXX32");
      }
   }

   

   
   if (e->tag == Iex_Unop 
       && e->Iex.Unop.op == Iop_CmpNEZ64) {
      if (!env->mode64) {
         HReg hi, lo;
         HReg tmp = newVRegI(env);
         iselInt64Expr( &hi, &lo, env, e->Iex.Unop.arg, IEndianess );
         addInstr(env, PPCInstr_Alu(Palu_OR, tmp, lo, PPCRH_Reg(hi)));
         addInstr(env, PPCInstr_Cmp(False, True,
                                    7, tmp,PPCRH_Imm(False,0)));
         return mk_PPCCondCode( Pct_FALSE, Pcf_7EQ );
      } else {  
         HReg r_src = iselWordExpr_R(env, e->Iex.Unop.arg, IEndianess);
         addInstr(env, PPCInstr_Cmp(False, False,
                                    7, r_src,PPCRH_Imm(False,0)));
         return mk_PPCCondCode( Pct_FALSE, Pcf_7EQ );
      }
   }

   

   
   if (e->tag == Iex_Binop 
       && (e->Iex.Binop.op == Iop_CmpEQ64
           || e->Iex.Binop.op == Iop_CmpNE64
           || e->Iex.Binop.op == Iop_CmpLT64S
           || e->Iex.Binop.op == Iop_CmpLT64U
           || e->Iex.Binop.op == Iop_CmpLE64S
           || e->Iex.Binop.op == Iop_CmpLE64U)) {
      Bool   syned = (e->Iex.Binop.op == Iop_CmpLT64S ||
                      e->Iex.Binop.op == Iop_CmpLE64S);
      HReg    r1 = iselWordExpr_R(env, e->Iex.Binop.arg1, IEndianess);
      PPCRH* ri2 = iselWordExpr_RH(env, syned, e->Iex.Binop.arg2, IEndianess);
      vassert(env->mode64);
      addInstr(env, PPCInstr_Cmp(syned, False,
                                 7, r1, ri2));

      switch (e->Iex.Binop.op) {
      case Iop_CmpEQ64:  return mk_PPCCondCode( Pct_TRUE,  Pcf_7EQ );
      case Iop_CmpNE64:  return mk_PPCCondCode( Pct_FALSE, Pcf_7EQ );
      case Iop_CmpLT64U: return mk_PPCCondCode( Pct_TRUE,  Pcf_7LT );
      case Iop_CmpLE64U: return mk_PPCCondCode( Pct_FALSE, Pcf_7GT );
      default: vpanic("iselCondCode(ppc): CmpXX64");
      }
   }

   

   
   
   
   if (e->tag == Iex_Binop
       && e->Iex.Binop.op == Iop_CmpNE8
       && isZeroU8(e->Iex.Binop.arg2)) {
      HReg arg = iselWordExpr_R(env, e->Iex.Binop.arg1, IEndianess);
      HReg tmp = newVRegI(env);
      addInstr(env, PPCInstr_Alu(Palu_AND, tmp, arg,
                                 PPCRH_Imm(False,0xFF)));
      addInstr(env, PPCInstr_Cmp(False, True,
                                 7, tmp, PPCRH_Imm(False,0)));
      return mk_PPCCondCode( Pct_FALSE, Pcf_7EQ );
   }

   
   if (e->tag == Iex_RdTmp) {
      HReg r_src      = lookupIRTemp(env, e->Iex.RdTmp.tmp);
      HReg src_masked = newVRegI(env);
      addInstr(env,
               PPCInstr_Alu(Palu_AND, src_masked,
                            r_src, PPCRH_Imm(False,1)));
      addInstr(env,
               PPCInstr_Cmp(False, True,
                            7, src_masked, PPCRH_Imm(False,1)));
      return mk_PPCCondCode( Pct_TRUE, Pcf_7EQ );
   }

   vex_printf("iselCondCode(ppc): No such tag(%u)\n", e->tag);
   ppIRExpr(e);
   vpanic("iselCondCode(ppc)");
}




static void iselInt128Expr ( HReg* rHi, HReg* rLo,
                             ISelEnv* env, IRExpr* e, IREndness IEndianess )
{
   vassert(env->mode64);
   iselInt128Expr_wrk(rHi, rLo, env, e, IEndianess);
#  if 0
   vex_printf("\n"); ppIRExpr(e); vex_printf("\n");
#  endif
   vassert(hregClass(*rHi) == HRcGPR(env->mode64));
   vassert(hregIsVirtual(*rHi));
   vassert(hregClass(*rLo) == HRcGPR(env->mode64));
   vassert(hregIsVirtual(*rLo));
}

static void iselInt128Expr_wrk ( HReg* rHi, HReg* rLo,
                                 ISelEnv* env, IRExpr* e, IREndness IEndianess )
{
   vassert(e);
   vassert(typeOfIRExpr(env->type_env,e) == Ity_I128);

   
   if (e->tag == Iex_RdTmp) {
      lookupIRTempPair( rHi, rLo, env, e->Iex.RdTmp.tmp);
      return;
   }

   
   if (e->tag == Iex_Binop) {
      switch (e->Iex.Binop.op) {
      
      case Iop_MullU64:
      case Iop_MullS64: {
         HReg     tLo     = newVRegI(env);
         HReg     tHi     = newVRegI(env);
         Bool     syned   = toBool(e->Iex.Binop.op == Iop_MullS64);
         HReg     r_srcL  = iselWordExpr_R(env, e->Iex.Binop.arg1, IEndianess);
         HReg     r_srcR  = iselWordExpr_R(env, e->Iex.Binop.arg2, IEndianess);
         addInstr(env, PPCInstr_MulL(False, 
                                     False, False,
                                     tLo, r_srcL, r_srcR));
         addInstr(env, PPCInstr_MulL(syned,
                                     True, False,
                                     tHi, r_srcL, r_srcR));
         *rHi = tHi;
         *rLo = tLo;
         return;
      }

      
      case Iop_64HLto128:
         *rHi = iselWordExpr_R(env, e->Iex.Binop.arg1, IEndianess);
         *rLo = iselWordExpr_R(env, e->Iex.Binop.arg2, IEndianess);
         return;
      default: 
         break;
      }
   } 


   
   if (e->tag == Iex_Unop) {
      switch (e->Iex.Unop.op) {
      default:
         break;
      }
   } 

   vex_printf("iselInt128Expr(ppc64): No such tag(%u)\n", e->tag);
   ppIRExpr(e);
   vpanic("iselInt128Expr(ppc64)");
}



static void iselInt128Expr_to_32x4 ( HReg* rHi, HReg* rMedHi, HReg* rMedLo,
                                     HReg* rLo, ISelEnv* env, IRExpr* e,
                                     IREndness IEndianess )
{
   vassert(!env->mode64);
   iselInt128Expr_to_32x4_wrk(rHi, rMedHi, rMedLo, rLo, env, e, IEndianess);
#  if 0
   vex_printf("\n"); ppIRExpr(e); vex_printf("\n");
#  endif
   vassert(hregClass(*rHi) == HRcInt32);
   vassert(hregIsVirtual(*rHi));
   vassert(hregClass(*rMedHi) == HRcInt32);
   vassert(hregIsVirtual(*rMedHi));
   vassert(hregClass(*rMedLo) == HRcInt32);
   vassert(hregIsVirtual(*rMedLo));
   vassert(hregClass(*rLo) == HRcInt32);
   vassert(hregIsVirtual(*rLo));
}

static void iselInt128Expr_to_32x4_wrk ( HReg* rHi, HReg* rMedHi,
                                         HReg* rMedLo, HReg* rLo,
                                         ISelEnv* env, IRExpr* e,
                                         IREndness IEndianess )
{
   vassert(e);
   vassert(typeOfIRExpr(env->type_env,e) == Ity_I128);

   
   if (e->tag == Iex_RdTmp) {
      lookupIRTempQuad( rHi, rMedHi, rMedLo, rLo, env, e->Iex.RdTmp.tmp);
      return;
   }

   if (e->tag == Iex_Binop) {

      IROp op_binop = e->Iex.Binop.op;
      switch (op_binop) {
      case Iop_64HLto128:
         iselInt64Expr(rHi, rMedHi, env, e->Iex.Binop.arg1, IEndianess);
         iselInt64Expr(rMedLo, rLo, env, e->Iex.Binop.arg2, IEndianess);
         return;
      default:
         vex_printf("iselInt128Expr_to_32x4_wrk: Binop case 0x%x not found\n",
                    op_binop);
         break;
      }
   } 

   vex_printf("iselInt128Expr_to_32x4_wrk: e->tag 0x%x not found\n", e->tag);
   return;
}


static void iselInt64Expr ( HReg* rHi, HReg* rLo,
                            ISelEnv* env, IRExpr* e,
                            IREndness IEndianess )
{
   vassert(!env->mode64);
   iselInt64Expr_wrk(rHi, rLo, env, e, IEndianess);
#  if 0
   vex_printf("\n"); ppIRExpr(e); vex_printf("\n");
#  endif
   vassert(hregClass(*rHi) == HRcInt32);
   vassert(hregIsVirtual(*rHi));
   vassert(hregClass(*rLo) == HRcInt32);
   vassert(hregIsVirtual(*rLo));
}

static void iselInt64Expr_wrk ( HReg* rHi, HReg* rLo,
                                ISelEnv* env, IRExpr* e,
                                IREndness IEndianess )
{
   vassert(e);
   vassert(typeOfIRExpr(env->type_env,e) == Ity_I64);

   
   if (e->tag == Iex_Load && e->Iex.Load.end == IEndianess) {
      HReg tLo    = newVRegI(env);
      HReg tHi    = newVRegI(env);
      HReg r_addr = iselWordExpr_R(env, e->Iex.Load.addr, IEndianess);
      vassert(!env->mode64);
      addInstr(env, PPCInstr_Load( 4,
                                   tHi, PPCAMode_IR( 0, r_addr ), 
                                   False) );
      addInstr(env, PPCInstr_Load( 4, 
                                   tLo, PPCAMode_IR( 4, r_addr ), 
                                   False) );
      *rHi = tHi;
      *rLo = tLo;
      return;
   }

   
   if (e->tag == Iex_Const) {
      ULong w64 = e->Iex.Const.con->Ico.U64;
      UInt  wHi = ((UInt)(w64 >> 32)) & 0xFFFFFFFF;
      UInt  wLo = ((UInt)w64) & 0xFFFFFFFF;
      HReg  tLo = newVRegI(env);
      HReg  tHi = newVRegI(env);
      vassert(e->Iex.Const.con->tag == Ico_U64);
      addInstr(env, PPCInstr_LI(tHi, (Long)(Int)wHi, False));
      addInstr(env, PPCInstr_LI(tLo, (Long)(Int)wLo, False));
      *rHi = tHi;
      *rLo = tLo;
      return;
   }

   
   if (e->tag == Iex_RdTmp) {
      lookupIRTempPair( rHi, rLo, env, e->Iex.RdTmp.tmp);
      return;
   }

   
   if (e->tag == Iex_Get) {
      PPCAMode* am_addr = PPCAMode_IR( e->Iex.Get.offset,
                                       GuestStatePtr(False) );
      PPCAMode* am_addr4 = advance4(env, am_addr);
      HReg tLo = newVRegI(env);
      HReg tHi = newVRegI(env);
      addInstr(env, PPCInstr_Load( 4, tHi, am_addr,  False ));
      addInstr(env, PPCInstr_Load( 4, tLo, am_addr4, False ));
      *rHi = tHi;
      *rLo = tLo;
      return;
   }

   
   if (e->tag == Iex_ITE) { 
      HReg e0Lo, e0Hi, eXLo, eXHi;
      iselInt64Expr(&eXHi, &eXLo, env, e->Iex.ITE.iftrue, IEndianess);
      iselInt64Expr(&e0Hi, &e0Lo, env, e->Iex.ITE.iffalse, IEndianess);
      HReg tLo = newVRegI(env);
      HReg tHi = newVRegI(env);
      addInstr(env, mk_iMOVds_RR(tHi,e0Hi));
      addInstr(env, mk_iMOVds_RR(tLo,e0Lo));
      PPCCondCode cc = iselCondCode(env, e->Iex.ITE.cond, IEndianess);
      addInstr(env, PPCInstr_CMov(cc,tHi,PPCRI_Reg(eXHi)));
      addInstr(env, PPCInstr_CMov(cc,tLo,PPCRI_Reg(eXLo)));
      *rHi = tHi;
      *rLo = tLo;
      return;
   }

   
   if (e->tag == Iex_Binop) {
      IROp op_binop = e->Iex.Binop.op;
      switch (op_binop) {
         
         case Iop_MullU32:
         case Iop_MullS32: {
            HReg     tLo     = newVRegI(env);
            HReg     tHi     = newVRegI(env);
            Bool     syned   = toBool(op_binop == Iop_MullS32);
            HReg     r_srcL  = iselWordExpr_R(env, e->Iex.Binop.arg1,
                                              IEndianess);
            HReg     r_srcR  = iselWordExpr_R(env, e->Iex.Binop.arg2,
                                              IEndianess);
            addInstr(env, PPCInstr_MulL(False, 
                                        False, True,
                                        tLo, r_srcL, r_srcR));
            addInstr(env, PPCInstr_MulL(syned,
                                        True, True,
                                        tHi, r_srcL, r_srcR));
            *rHi = tHi;
            *rLo = tLo;
            return;
         }

         
         case Iop_Or64:
         case Iop_And64:
         case Iop_Xor64: {
            HReg xLo, xHi, yLo, yHi;
            HReg tLo = newVRegI(env);
            HReg tHi = newVRegI(env);
            PPCAluOp op = (op_binop == Iop_Or64) ? Palu_OR :
                          (op_binop == Iop_And64) ? Palu_AND : Palu_XOR;
            iselInt64Expr(&xHi, &xLo, env, e->Iex.Binop.arg1, IEndianess);
            iselInt64Expr(&yHi, &yLo, env, e->Iex.Binop.arg2, IEndianess);
            addInstr(env, PPCInstr_Alu(op, tHi, xHi, PPCRH_Reg(yHi)));
            addInstr(env, PPCInstr_Alu(op, tLo, xLo, PPCRH_Reg(yLo)));
            *rHi = tHi;
            *rLo = tLo;
            return;
         }

         
         case Iop_Add64: {
            HReg xLo, xHi, yLo, yHi;
            HReg tLo = newVRegI(env);
            HReg tHi = newVRegI(env);
            iselInt64Expr(&xHi, &xLo, env, e->Iex.Binop.arg1, IEndianess);
            iselInt64Expr(&yHi, &yLo, env, e->Iex.Binop.arg2, IEndianess);
            addInstr(env, PPCInstr_AddSubC( True, True ,
                                            tLo, xLo, yLo));
            addInstr(env, PPCInstr_AddSubC( True, False,
                                            tHi, xHi, yHi));
            *rHi = tHi;
            *rLo = tLo;
            return;
         }

         
         case Iop_32HLto64:
            *rHi = iselWordExpr_R(env, e->Iex.Binop.arg1, IEndianess);
            *rLo = iselWordExpr_R(env, e->Iex.Binop.arg2, IEndianess);
            return;

         
         case Iop_F64toI64S: case Iop_F64toI64U: {
            HReg      tLo     = newVRegI(env);
            HReg      tHi     = newVRegI(env);
            HReg      r1      = StackFramePtr(env->mode64);
            PPCAMode* zero_r1 = PPCAMode_IR( 0, r1 );
            PPCAMode* four_r1 = PPCAMode_IR( 4, r1 );
            HReg      fsrc    = iselDblExpr(env, e->Iex.Binop.arg2,
                                            IEndianess);
            HReg      ftmp    = newVRegF(env);

            vassert(!env->mode64);
            
            set_FPU_rounding_mode( env, e->Iex.Binop.arg1, IEndianess );

            sub_from_sp( env, 16 );
            addInstr(env, PPCInstr_FpCftI(False, False,
                                          (op_binop == Iop_F64toI64S) ? True : False,
                                          True, ftmp, fsrc));
            addInstr(env, PPCInstr_FpLdSt(False, 8, ftmp, zero_r1));
            addInstr(env, PPCInstr_Load(4, tHi, zero_r1, False));
            addInstr(env, PPCInstr_Load(4, tLo, four_r1, False));
            add_to_sp( env, 16 );

            
            
            *rHi = tHi;
            *rLo = tLo;
            return;
         }
         case Iop_D64toI64S: {
            HReg      tLo     = newVRegI(env);
            HReg      tHi     = newVRegI(env);
            HReg      r1      = StackFramePtr(env->mode64);
            PPCAMode* zero_r1 = PPCAMode_IR( 0, r1 );
            PPCAMode* four_r1 = PPCAMode_IR( 4, r1 );
            HReg fr_src = iselDfp64Expr(env, e->Iex.Binop.arg2, IEndianess);
            HReg tmp    = newVRegF(env);

            vassert(!env->mode64);
            set_FPU_DFP_rounding_mode( env, e->Iex.Binop.arg1, IEndianess );
            addInstr(env, PPCInstr_Dfp64Unary(Pfp_DCTFIX, tmp, fr_src));

            sub_from_sp( env, 16 );
            addInstr(env, PPCInstr_FpLdSt(False, 8, tmp, zero_r1));
            addInstr(env, PPCInstr_Load(4, tHi, zero_r1, False));
            addInstr(env, PPCInstr_Load(4, tLo, four_r1, False));
            add_to_sp( env, 16 );
            *rHi = tHi;
            *rLo = tLo;
            return;
         }
         case Iop_D128toI64S: {
            PPCFpOp fpop = Pfp_DCTFIXQ;
            HReg r_srcHi = newVRegF(env);
            HReg r_srcLo = newVRegF(env);
            HReg tLo     = newVRegI(env);
            HReg tHi     = newVRegI(env);
            HReg ftmp    = newVRegF(env);
            PPCAMode* zero_r1 = PPCAMode_IR( 0, StackFramePtr(env->mode64) );
            PPCAMode* four_r1 = PPCAMode_IR( 4, StackFramePtr(env->mode64) );

            set_FPU_DFP_rounding_mode( env, e->Iex.Binop.arg1, IEndianess );
            iselDfp128Expr(&r_srcHi, &r_srcLo, env, e->Iex.Binop.arg2,
                           IEndianess);
            addInstr(env, PPCInstr_DfpD128toD64(fpop, ftmp, r_srcHi, r_srcLo));

            
            sub_from_sp( env, 16 );
            addInstr(env, PPCInstr_FpLdSt(False, 8, ftmp, zero_r1));
            addInstr(env, PPCInstr_Load(4, tHi, zero_r1, False));
            addInstr(env, PPCInstr_Load(4, tLo, four_r1, False));
            add_to_sp( env, 16 );
            *rHi = tHi;
            *rLo = tLo;
            return;
         }
         default: 
            break;
      }
   } 


   
   if (e->tag == Iex_Unop) {
      switch (e->Iex.Unop.op) {

      
      case Iop_CmpwNEZ64: {
         HReg argHi, argLo;
         HReg tmp1  = newVRegI(env);
         HReg tmp2  = newVRegI(env);
         iselInt64Expr(&argHi, &argLo, env, e->Iex.Unop.arg, IEndianess);
         
         addInstr(env, PPCInstr_Alu(Palu_OR, tmp1, argHi, PPCRH_Reg(argLo)));
         
         addInstr(env, PPCInstr_Unary(Pun_NEG,tmp2,tmp1));
         addInstr(env, PPCInstr_Alu(Palu_OR, tmp2, tmp2, PPCRH_Reg(tmp1)));
         addInstr(env, PPCInstr_Shft(Pshft_SAR, True, 
                                     tmp2, tmp2, PPCRH_Imm(False, 31)));
         *rHi = tmp2;
         *rLo = tmp2; 
         return;
      }

      
      case Iop_Left64: {
         HReg argHi, argLo;
         HReg zero32 = newVRegI(env);
         HReg resHi  = newVRegI(env);
         HReg resLo  = newVRegI(env);
         iselInt64Expr(&argHi, &argLo, env, e->Iex.Unop.arg, IEndianess);
         vassert(env->mode64 == False);
         addInstr(env, PPCInstr_LI(zero32, 0, env->mode64));
         
         addInstr(env, PPCInstr_AddSubC( False, True,
                                         resLo, zero32, argLo ));
         addInstr(env, PPCInstr_AddSubC( False, False,
                                         resHi, zero32, argHi ));
         
         addInstr(env, PPCInstr_Alu(Palu_OR, resLo, resLo, PPCRH_Reg(argLo)));
         addInstr(env, PPCInstr_Alu(Palu_OR, resHi, resHi, PPCRH_Reg(argHi)));
         *rHi = resHi;
         *rLo = resLo;
         return;
      }

      
      case Iop_32Sto64: {
         HReg tHi = newVRegI(env);
         HReg src = iselWordExpr_R(env, e->Iex.Unop.arg, IEndianess);
         addInstr(env, PPCInstr_Shft(Pshft_SAR, True,
                                     tHi, src, PPCRH_Imm(False,31)));
         *rHi = tHi;
         *rLo = src;
         return;
      }
      case Iop_ExtractExpD64: {
         HReg tmp    = newVRegF(env);
         HReg fr_src = iselDfp64Expr(env, e->Iex.Unop.arg, IEndianess);
         HReg      tLo     = newVRegI(env);
         HReg      tHi     = newVRegI(env);
         PPCAMode* zero_r1 = PPCAMode_IR( 0, StackFramePtr(env->mode64) );
         PPCAMode* four_r1 = PPCAMode_IR( 4, StackFramePtr(env->mode64) );

         addInstr(env, PPCInstr_Dfp64Unary(Pfp_DXEX, tmp, fr_src));

         
         sub_from_sp( env, 16 );
         addInstr(env, PPCInstr_FpLdSt(False, 8, tmp, zero_r1));
         addInstr(env, PPCInstr_Load(4, tHi, zero_r1, False));
         addInstr(env, PPCInstr_Load(4, tLo, four_r1, False));
         add_to_sp( env, 16 );
         *rHi = tHi;
         *rLo = tLo;
         return;
      }
      case Iop_ExtractExpD128: {
         HReg      r_srcHi;
         HReg      r_srcLo;
         HReg      tmp     = newVRegF(env);
         HReg      tLo     = newVRegI(env);
         HReg      tHi     = newVRegI(env);
         PPCAMode* zero_r1 = PPCAMode_IR( 0, StackFramePtr(env->mode64) );
         PPCAMode* four_r1 = PPCAMode_IR( 4, StackFramePtr(env->mode64) );

         iselDfp128Expr(&r_srcHi, &r_srcLo, env, e->Iex.Unop.arg, IEndianess);
         addInstr(env, PPCInstr_ExtractExpD128(Pfp_DXEXQ, tmp,
                                                  r_srcHi, r_srcLo));

         
         sub_from_sp( env, 16 );
         addInstr(env, PPCInstr_FpLdSt(False, 8, tmp, zero_r1));
         addInstr(env, PPCInstr_Load(4, tHi, zero_r1, False));
         addInstr(env, PPCInstr_Load(4, tLo, four_r1, False));
         add_to_sp( env, 16 );
         *rHi = tHi;
         *rLo = tLo;
         return;
      }

      
      case Iop_32Uto64: {
         HReg tHi = newVRegI(env);
         HReg tLo = iselWordExpr_R(env, e->Iex.Unop.arg, IEndianess);
         addInstr(env, PPCInstr_LI(tHi, 0, False));
         *rHi = tHi;
         *rLo = tLo;
         return;
      }

      case Iop_128to64: {
         HReg r_Hi    = INVALID_HREG;
         HReg r_MedHi = INVALID_HREG;
         HReg r_MedLo = INVALID_HREG;
         HReg r_Lo    = INVALID_HREG;

         iselInt128Expr_to_32x4(&r_Hi, &r_MedHi, &r_MedLo, &r_Lo,
                                env, e->Iex.Unop.arg, IEndianess);
         *rHi = r_MedLo;
         *rLo = r_Lo;
         return;
      }

      case Iop_128HIto64: {
         HReg r_Hi    = INVALID_HREG;
         HReg r_MedHi = INVALID_HREG;
         HReg r_MedLo = INVALID_HREG;
         HReg r_Lo    = INVALID_HREG;

         iselInt128Expr_to_32x4(&r_Hi, &r_MedHi, &r_MedLo, &r_Lo,
                                env, e->Iex.Unop.arg, IEndianess);
         *rHi = r_Hi;
         *rLo = r_MedHi;
         return;
      }

      
      case Iop_V128HIto64:
      case Iop_V128to64: {
         HReg r_aligned16;
         Int  off = e->Iex.Unop.op==Iop_V128HIto64 ? 0 : 8;
         HReg tLo = newVRegI(env);
         HReg tHi = newVRegI(env);
         HReg vec = iselVecExpr(env, e->Iex.Unop.arg, IEndianess);
         PPCAMode *am_off0, *am_offLO, *am_offHI;
         sub_from_sp( env, 32 );     
         
         
         r_aligned16 = get_sp_aligned16( env );
         am_off0  = PPCAMode_IR( 0,     r_aligned16 );
         am_offHI = PPCAMode_IR( off,   r_aligned16 );
         am_offLO = PPCAMode_IR( off+4, r_aligned16 );
         
         
         addInstr(env,
                  PPCInstr_AvLdSt( False, 16, vec, am_off0 ));
         
         
         addInstr(env,
                  PPCInstr_Load( 4, tHi, am_offHI, False ));
         addInstr(env,
                  PPCInstr_Load( 4, tLo, am_offLO, False ));
         
         add_to_sp( env, 32 );       
         *rHi = tHi;
         *rLo = tLo;
         return;
      }

      
      case Iop_1Sto64: {
         HReg tLo = newVRegI(env);
         HReg tHi = newVRegI(env);
         PPCCondCode cond = iselCondCode(env, e->Iex.Unop.arg, IEndianess);
         addInstr(env, PPCInstr_Set(cond,tLo));
         addInstr(env, PPCInstr_Shft(Pshft_SHL, True,
                                     tLo, tLo, PPCRH_Imm(False,31)));
         addInstr(env, PPCInstr_Shft(Pshft_SAR, True,
                                     tLo, tLo, PPCRH_Imm(False,31)));
         addInstr(env, mk_iMOVds_RR(tHi, tLo));
         *rHi = tHi;
         *rLo = tLo;
         return;
      }

      case Iop_Not64: {
         HReg xLo, xHi;
         HReg tmpLo = newVRegI(env);
         HReg tmpHi = newVRegI(env);
         iselInt64Expr(&xHi, &xLo, env, e->Iex.Unop.arg, IEndianess);
         addInstr(env, PPCInstr_Unary(Pun_NOT,tmpLo,xLo));
         addInstr(env, PPCInstr_Unary(Pun_NOT,tmpHi,xHi));
         *rHi = tmpHi;
         *rLo = tmpLo;
         return;
      }

      
      case Iop_ReinterpF64asI64: {
         PPCAMode *am_addr0, *am_addr1;
         HReg fr_src  = iselDblExpr(env, e->Iex.Unop.arg, IEndianess);
         HReg r_dstLo = newVRegI(env);
         HReg r_dstHi = newVRegI(env);
         
         sub_from_sp( env, 16 );     
         am_addr0 = PPCAMode_IR( 0, StackFramePtr(False) );
         am_addr1 = PPCAMode_IR( 4, StackFramePtr(False) );

         
         addInstr(env, PPCInstr_FpLdSt( False, 8,
                                        fr_src, am_addr0 ));
         
         
         addInstr(env, PPCInstr_Load( 4, r_dstHi,
                                      am_addr0, False ));
         addInstr(env, PPCInstr_Load( 4, r_dstLo,
                                      am_addr1, False ));
         *rHi = r_dstHi;
         *rLo = r_dstLo;
         
         add_to_sp( env, 16 );       
         return;
      }

      case Iop_ReinterpD64asI64: {
         HReg fr_src  = iselDfp64Expr(env, e->Iex.Unop.arg, IEndianess);
         PPCAMode *am_addr0, *am_addr1;
         HReg r_dstLo = newVRegI(env);
         HReg r_dstHi = newVRegI(env);


         sub_from_sp( env, 16 );     
         am_addr0 = PPCAMode_IR( 0, StackFramePtr(False) );
         am_addr1 = PPCAMode_IR( 4, StackFramePtr(False) );

         
         addInstr(env, PPCInstr_FpLdSt( False, 8,
                                        fr_src, am_addr0 ));

         
         addInstr(env, PPCInstr_Load( 4, r_dstHi,
                                      am_addr0, False ));
         addInstr(env, PPCInstr_Load( 4, r_dstLo,
                                      am_addr1, False ));
         *rHi = r_dstHi;
         *rLo = r_dstLo;

         add_to_sp( env, 16 );       

         return;
      }

      case Iop_BCDtoDPB: {
         PPCCondCode cc;
         UInt        argiregs;
         HReg        argregs[2];
         Int         argreg;
         HReg        tLo = newVRegI(env);
         HReg        tHi = newVRegI(env);
         HReg        tmpHi;
         HReg        tmpLo;
         Bool        mode64 = env->mode64;

         argregs[0] = hregPPC_GPR3(mode64);
         argregs[1] = hregPPC_GPR4(mode64);

         argiregs = 0;
         argreg = 0;

         iselInt64Expr( &tmpHi, &tmpLo, env, e->Iex.Unop.arg, IEndianess );

         argiregs |= ( 1 << (argreg+3 ) );
         addInstr( env, mk_iMOVds_RR( argregs[argreg++], tmpHi ) );

         argiregs |= ( 1 << (argreg+3 ) );
         addInstr( env, mk_iMOVds_RR( argregs[argreg], tmpLo ) );

         cc = mk_PPCCondCode( Pct_ALWAYS, Pcf_NONE );

         if (IEndianess == Iend_LE) {
             addInstr( env, PPCInstr_Call( cc, (Addr)h_calc_BCDtoDPB,
                                           argiregs,
                                           mk_RetLoc_simple(RLPri_2Int) ) );
         } else {
             Addr64 target;
             target = mode64 ? (Addr)h_calc_BCDtoDPB :
               toUInt( (Addr)h_calc_BCDtoDPB );
             addInstr( env, PPCInstr_Call( cc, target,
                                           argiregs,
                                           mk_RetLoc_simple(RLPri_2Int) ) );
         }

         addInstr( env, mk_iMOVds_RR( tHi, argregs[argreg-1] ) );
         addInstr( env, mk_iMOVds_RR( tLo, argregs[argreg] ) );

         *rHi = tHi;
         *rLo = tLo;
         return;
      }

      case Iop_DPBtoBCD: {
         PPCCondCode cc;
         UInt        argiregs;
         HReg        argregs[2];
         Int         argreg;
         HReg        tLo = newVRegI(env);
         HReg        tHi = newVRegI(env);
         HReg        tmpHi;
         HReg        tmpLo;
         Bool        mode64 = env->mode64;

         argregs[0] = hregPPC_GPR3(mode64);
         argregs[1] = hregPPC_GPR4(mode64);

         argiregs = 0;
         argreg = 0;

         iselInt64Expr(&tmpHi, &tmpLo, env, e->Iex.Unop.arg, IEndianess);

         argiregs |= (1 << (argreg+3));
         addInstr(env, mk_iMOVds_RR( argregs[argreg++], tmpHi ));

         argiregs |= (1 << (argreg+3));
         addInstr(env, mk_iMOVds_RR( argregs[argreg], tmpLo));

         cc = mk_PPCCondCode( Pct_ALWAYS, Pcf_NONE );

         if (IEndianess == Iend_LE) {
             addInstr(env, PPCInstr_Call( cc, (Addr)h_calc_DPBtoBCD,
                                          argiregs,
                                          mk_RetLoc_simple(RLPri_2Int) ) );
         } else {
             Addr64 target;
             target = mode64 ? (Addr)h_calc_DPBtoBCD :
               toUInt( (Addr)h_calc_DPBtoBCD );
             addInstr(env, PPCInstr_Call( cc, target, argiregs,
                                          mk_RetLoc_simple(RLPri_2Int) ) );
         }

         addInstr(env, mk_iMOVds_RR(tHi, argregs[argreg-1]));
         addInstr(env, mk_iMOVds_RR(tLo, argregs[argreg]));

         *rHi = tHi;
         *rLo = tLo;
         return;
      }

      default:
         break;
      }
   } 

   vex_printf("iselInt64Expr(ppc): No such tag(%u)\n", e->tag);
   ppIRExpr(e);
   vpanic("iselInt64Expr(ppc)");
}




static HReg iselFltExpr ( ISelEnv* env, IRExpr* e, IREndness IEndianess )
{
  HReg r = iselFltExpr_wrk( env, e, IEndianess );
#  if 0
   vex_printf("\n"); ppIRExpr(e); vex_printf("\n");
#  endif
   vassert(hregClass(r) == HRcFlt64); 
   vassert(hregIsVirtual(r));
   return r;
}

static HReg iselFltExpr_wrk ( ISelEnv* env, IRExpr* e, IREndness IEndianess )
{
   Bool        mode64 = env->mode64;

   IRType ty = typeOfIRExpr(env->type_env,e);
   vassert(ty == Ity_F32);

   if (e->tag == Iex_RdTmp) {
      return lookupIRTemp(env, e->Iex.RdTmp.tmp);
   }

   if (e->tag == Iex_Load && e->Iex.Load.end == IEndianess) {
      PPCAMode* am_addr;
      HReg r_dst = newVRegF(env);
      vassert(e->Iex.Load.ty == Ity_F32);
      am_addr = iselWordExpr_AMode(env, e->Iex.Load.addr, Ity_F32,
                                   IEndianess);
      addInstr(env, PPCInstr_FpLdSt(True, 4, r_dst, am_addr));
      return r_dst;
   }

   if (e->tag == Iex_Get) {
      HReg r_dst = newVRegF(env);
      PPCAMode* am_addr = PPCAMode_IR( e->Iex.Get.offset,
                                       GuestStatePtr(env->mode64) );
      addInstr(env, PPCInstr_FpLdSt( True, 4, r_dst, am_addr ));
      return r_dst;
   }

   if (e->tag == Iex_Unop && e->Iex.Unop.op == Iop_TruncF64asF32) {
      /* This is quite subtle.  The only way to do the relevant
         truncation is to do a single-precision store and then a
         double precision load to get it back into a register.  The
         problem is, if the data is then written to memory a second
         time, as in

            STbe(...) = TruncF64asF32(...)

         then will the second truncation further alter the value?  The
         answer is no: flds (as generated here) followed by fsts
         (generated for the STbe) is the identity function on 32-bit
         floats, so we are safe.

         Another upshot of this is that if iselStmt can see the
         entirety of

            STbe(...) = TruncF64asF32(arg)

         then it can short circuit having to deal with TruncF64asF32
         individually; instead just compute arg into a 64-bit FP
         register and do 'fsts' (since that itself does the
         truncation).

         We generate pretty poor code here (should be ok both for
         32-bit and 64-bit mode); but it is expected that for the most
         part the latter optimisation will apply and hence this code
         will not often be used.
      */
      HReg      fsrc    = iselDblExpr(env, e->Iex.Unop.arg, IEndianess);
      HReg      fdst    = newVRegF(env);
      PPCAMode* zero_r1 = PPCAMode_IR( 0, StackFramePtr(env->mode64) );

      sub_from_sp( env, 16 );
      
      addInstr(env, PPCInstr_FpLdSt( False, 4,
                                     fsrc, zero_r1 ));
      
      addInstr(env, PPCInstr_FpLdSt( True, 4,
                                     fdst, zero_r1 ));
      add_to_sp( env, 16 );
      return fdst;
   }

   if (e->tag == Iex_Binop && e->Iex.Binop.op == Iop_I64UtoF32) {
      if (mode64) {
         HReg fdst = newVRegF(env);
         HReg isrc = iselWordExpr_R(env, e->Iex.Binop.arg2, IEndianess);
         HReg r1   = StackFramePtr(env->mode64);
         PPCAMode* zero_r1 = PPCAMode_IR( 0, r1 );

         
         set_FPU_rounding_mode( env, e->Iex.Binop.arg1, IEndianess );

         sub_from_sp( env, 16 );

         addInstr(env, PPCInstr_Store(8, zero_r1, isrc, True));
         addInstr(env, PPCInstr_FpLdSt(True, 8, fdst, zero_r1));
         addInstr(env, PPCInstr_FpCftI(True, False, 
                                       False, False,
                                       fdst, fdst));

         add_to_sp( env, 16 );

         
         
         return fdst;
      } else {
         
         HReg fdst = newVRegF(env);
         HReg isrcHi, isrcLo;
         HReg r1   = StackFramePtr(env->mode64);
         PPCAMode* zero_r1 = PPCAMode_IR( 0, r1 );
         PPCAMode* four_r1 = PPCAMode_IR( 4, r1 );

         iselInt64Expr(&isrcHi, &isrcLo, env, e->Iex.Binop.arg2, IEndianess);

         
         set_FPU_rounding_mode( env, e->Iex.Binop.arg1, IEndianess );

         sub_from_sp( env, 16 );

         addInstr(env, PPCInstr_Store(4, zero_r1, isrcHi, False));
         addInstr(env, PPCInstr_Store(4, four_r1, isrcLo, False));
         addInstr(env, PPCInstr_FpLdSt(True, 8, fdst, zero_r1));
         addInstr(env, PPCInstr_FpCftI(True, False, 
                                       False, False,
                                       fdst, fdst));

         add_to_sp( env, 16 );

         
         
         return fdst;
      }

   }

   vex_printf("iselFltExpr(ppc): No such tag(%u)\n", e->tag);
   ppIRExpr(e);
   vpanic("iselFltExpr_wrk(ppc)");
}





static HReg iselDblExpr ( ISelEnv* env, IRExpr* e, IREndness IEndianess )
{
   HReg r = iselDblExpr_wrk( env, e, IEndianess );
#  if 0
   vex_printf("\n"); ppIRExpr(e); vex_printf("\n");
#  endif
   vassert(hregClass(r) == HRcFlt64);
   vassert(hregIsVirtual(r));
   return r;
}

static HReg iselDblExpr_wrk ( ISelEnv* env, IRExpr* e, IREndness IEndianess )
{
   Bool mode64 = env->mode64;
   IRType ty = typeOfIRExpr(env->type_env,e);
   vassert(e);
   vassert(ty == Ity_F64);

   if (e->tag == Iex_RdTmp) {
      return lookupIRTemp(env, e->Iex.RdTmp.tmp);
   }

   
   if (e->tag == Iex_Const) {
      union { UInt u32x2[2]; ULong u64; Double f64; } u;
      vassert(sizeof(u) == 8);
      vassert(sizeof(u.u64) == 8);
      vassert(sizeof(u.f64) == 8);
      vassert(sizeof(u.u32x2) == 8);

      if (e->Iex.Const.con->tag == Ico_F64) {
         u.f64 = e->Iex.Const.con->Ico.F64;
      }
      else if (e->Iex.Const.con->tag == Ico_F64i) {
         u.u64 = e->Iex.Const.con->Ico.F64i;
      }
      else
         vpanic("iselDblExpr(ppc): const");

      if (!mode64) {
         HReg r_srcHi = newVRegI(env);
         HReg r_srcLo = newVRegI(env);
         addInstr(env, PPCInstr_LI(r_srcHi, u.u32x2[0], mode64));
         addInstr(env, PPCInstr_LI(r_srcLo, u.u32x2[1], mode64));
         return mk_LoadRR32toFPR( env, r_srcHi, r_srcLo );
      } else { 
         HReg r_src = newVRegI(env);
         addInstr(env, PPCInstr_LI(r_src, u.u64, mode64));
         return mk_LoadR64toFPR( env, r_src );         
      }
   }

   
   if (e->tag == Iex_Load && e->Iex.Load.end == IEndianess) {
      HReg r_dst = newVRegF(env);
      PPCAMode* am_addr;
      vassert(e->Iex.Load.ty == Ity_F64);
      am_addr = iselWordExpr_AMode(env, e->Iex.Load.addr, Ity_F64,
                                   IEndianess);
      addInstr(env, PPCInstr_FpLdSt(True, 8, r_dst, am_addr));
      return r_dst;
   }

   
   if (e->tag == Iex_Get) {
      HReg r_dst = newVRegF(env);
      PPCAMode* am_addr = PPCAMode_IR( e->Iex.Get.offset,
                                       GuestStatePtr(mode64) );
      addInstr(env, PPCInstr_FpLdSt( True, 8, r_dst, am_addr ));
      return r_dst;
   }

   
   if (e->tag == Iex_Qop) {
      PPCFpOp fpop = Pfp_INVALID;
      switch (e->Iex.Qop.details->op) {
         case Iop_MAddF64:    fpop = Pfp_MADDD; break;
         case Iop_MAddF64r32: fpop = Pfp_MADDS; break;
         case Iop_MSubF64:    fpop = Pfp_MSUBD; break;
         case Iop_MSubF64r32: fpop = Pfp_MSUBS; break;
         default: break;
      }
      if (fpop != Pfp_INVALID) {
         HReg r_dst  = newVRegF(env);
         HReg r_srcML  = iselDblExpr(env, e->Iex.Qop.details->arg2,
                                     IEndianess);
         HReg r_srcMR  = iselDblExpr(env, e->Iex.Qop.details->arg3,
                                     IEndianess);
         HReg r_srcAcc = iselDblExpr(env, e->Iex.Qop.details->arg4,
                                     IEndianess);
         set_FPU_rounding_mode( env, e->Iex.Qop.details->arg1, IEndianess );
         addInstr(env, PPCInstr_FpMulAcc(fpop, r_dst, 
                                               r_srcML, r_srcMR, r_srcAcc));
         return r_dst;
      }
   }

   if (e->tag == Iex_Triop) {
      IRTriop *triop = e->Iex.Triop.details;
      PPCFpOp fpop = Pfp_INVALID;
      switch (triop->op) {
         case Iop_AddF64:    fpop = Pfp_ADDD; break;
         case Iop_SubF64:    fpop = Pfp_SUBD; break;
         case Iop_MulF64:    fpop = Pfp_MULD; break;
         case Iop_DivF64:    fpop = Pfp_DIVD; break;
         case Iop_AddF64r32: fpop = Pfp_ADDS; break;
         case Iop_SubF64r32: fpop = Pfp_SUBS; break;
         case Iop_MulF64r32: fpop = Pfp_MULS; break;
         case Iop_DivF64r32: fpop = Pfp_DIVS; break;
         default: break;
      }
      if (fpop != Pfp_INVALID) {
         HReg r_dst  = newVRegF(env);
         HReg r_srcL = iselDblExpr(env, triop->arg2, IEndianess);
         HReg r_srcR = iselDblExpr(env, triop->arg3, IEndianess);
         set_FPU_rounding_mode( env, triop->arg1, IEndianess );
         addInstr(env, PPCInstr_FpBinary(fpop, r_dst, r_srcL, r_srcR));
         return r_dst;
      }
   }

   if (e->tag == Iex_Binop) {
      PPCFpOp fpop = Pfp_INVALID;
      switch (e->Iex.Binop.op) {
      case Iop_SqrtF64:   fpop = Pfp_SQRT;   break;
      default: break;
      }
      if (fpop == Pfp_SQRT) {
         HReg fr_dst = newVRegF(env);
         HReg fr_src = iselDblExpr(env, e->Iex.Binop.arg2, IEndianess);
         set_FPU_rounding_mode( env, e->Iex.Binop.arg1, IEndianess );
         addInstr(env, PPCInstr_FpUnary(fpop, fr_dst, fr_src));
         return fr_dst;
      }
   }

   if (e->tag == Iex_Binop) {

      if (e->Iex.Binop.op == Iop_RoundF64toF32) {
         HReg r_dst = newVRegF(env);
         HReg r_src = iselDblExpr(env, e->Iex.Binop.arg2, IEndianess);
         set_FPU_rounding_mode( env, e->Iex.Binop.arg1, IEndianess );
         addInstr(env, PPCInstr_FpRSP(r_dst, r_src));
         
         return r_dst;
      }

      if (e->Iex.Binop.op == Iop_I64StoF64 || e->Iex.Binop.op == Iop_I64UtoF64) {
         if (mode64) {
            HReg fdst = newVRegF(env);
            HReg isrc = iselWordExpr_R(env, e->Iex.Binop.arg2, IEndianess);
            HReg r1   = StackFramePtr(env->mode64);
            PPCAMode* zero_r1 = PPCAMode_IR( 0, r1 );

            
            set_FPU_rounding_mode( env, e->Iex.Binop.arg1, IEndianess );

            sub_from_sp( env, 16 );

            addInstr(env, PPCInstr_Store(8, zero_r1, isrc, True));
            addInstr(env, PPCInstr_FpLdSt(True, 8, fdst, zero_r1));
            addInstr(env, PPCInstr_FpCftI(True, False, 
                                          e->Iex.Binop.op == Iop_I64StoF64,
                                          True,
                                          fdst, fdst));

            add_to_sp( env, 16 );

            
            
            return fdst;
         } else {
            
            HReg fdst = newVRegF(env);
            HReg isrcHi, isrcLo;
            HReg r1   = StackFramePtr(env->mode64);
            PPCAMode* zero_r1 = PPCAMode_IR( 0, r1 );
            PPCAMode* four_r1 = PPCAMode_IR( 4, r1 );

            iselInt64Expr(&isrcHi, &isrcLo, env, e->Iex.Binop.arg2,
                          IEndianess);

            
            set_FPU_rounding_mode( env, e->Iex.Binop.arg1, IEndianess );

            sub_from_sp( env, 16 );

            addInstr(env, PPCInstr_Store(4, zero_r1, isrcHi, False));
            addInstr(env, PPCInstr_Store(4, four_r1, isrcLo, False));
            addInstr(env, PPCInstr_FpLdSt(True, 8, fdst, zero_r1));
            addInstr(env, PPCInstr_FpCftI(True, False, 
                                          e->Iex.Binop.op == Iop_I64StoF64,
                                          True,
                                          fdst, fdst));

            add_to_sp( env, 16 );

            
            
            return fdst;
         }
      }

   }

   if (e->tag == Iex_Unop) {
      PPCFpOp fpop = Pfp_INVALID;
      switch (e->Iex.Unop.op) {
         case Iop_NegF64:     fpop = Pfp_NEG; break;
         case Iop_AbsF64:     fpop = Pfp_ABS; break;
         case Iop_RSqrtEst5GoodF64:      fpop = Pfp_RSQRTE; break;
         case Iop_RoundF64toF64_NegINF:  fpop = Pfp_FRIM; break;
         case Iop_RoundF64toF64_PosINF:  fpop = Pfp_FRIP; break;
         case Iop_RoundF64toF64_NEAREST: fpop = Pfp_FRIN; break;
         case Iop_RoundF64toF64_ZERO:    fpop = Pfp_FRIZ; break;
         default: break;
      }
      if (fpop != Pfp_INVALID) {
         HReg fr_dst = newVRegF(env);
         HReg fr_src = iselDblExpr(env, e->Iex.Unop.arg, IEndianess);
         addInstr(env, PPCInstr_FpUnary(fpop, fr_dst, fr_src));
         return fr_dst;
      }
   }

   if (e->tag == Iex_Unop) {
      switch (e->Iex.Unop.op) {
         case Iop_ReinterpI64asF64: {
            if (!mode64) {
               HReg r_srcHi, r_srcLo;
               iselInt64Expr( &r_srcHi, &r_srcLo, env, e->Iex.Unop.arg,
                               IEndianess);
               return mk_LoadRR32toFPR( env, r_srcHi, r_srcLo );
            } else {
               HReg r_src = iselWordExpr_R(env, e->Iex.Unop.arg, IEndianess);
               return mk_LoadR64toFPR( env, r_src );
            }
         }

         case Iop_F32toF64: {
            if (e->Iex.Unop.arg->tag == Iex_Unop &&
                     e->Iex.Unop.arg->Iex.Unop.op == Iop_ReinterpI32asF32 ) {
               e = e->Iex.Unop.arg;

               HReg src = iselWordExpr_R(env, e->Iex.Unop.arg, IEndianess);
               HReg fr_dst = newVRegF(env);
               PPCAMode *am_addr;

               sub_from_sp( env, 16 );        
               am_addr = PPCAMode_IR( 0, StackFramePtr(env->mode64) );

               
               addInstr(env, PPCInstr_Store( 4, am_addr, src, env->mode64 ));

               
               
               addInstr(env, PPCInstr_FpLdSt(True, 4, fr_dst, am_addr));

               add_to_sp( env, 16 );          
               return fr_dst;
            }


            
            HReg res = iselFltExpr(env, e->Iex.Unop.arg, IEndianess);
            return res;
         }
         default: 
            break;
      }
   }

   
   if (e->tag == Iex_ITE) { 
      if (ty == Ity_F64
          && typeOfIRExpr(env->type_env,e->Iex.ITE.cond) == Ity_I1) {
         HReg fr1    = iselDblExpr(env, e->Iex.ITE.iftrue, IEndianess);
         HReg fr0    = iselDblExpr(env, e->Iex.ITE.iffalse, IEndianess);
         HReg fr_dst = newVRegF(env);
         addInstr(env, PPCInstr_FpUnary( Pfp_MOV, fr_dst, fr0 ));
         PPCCondCode cc = iselCondCode(env, e->Iex.ITE.cond, IEndianess);
         addInstr(env, PPCInstr_FpCMov( cc, fr_dst, fr1 ));
         return fr_dst;
      }
   }

   vex_printf("iselDblExpr(ppc): No such tag(%u)\n", e->tag);
   ppIRExpr(e);
   vpanic("iselDblExpr_wrk(ppc)");
}

static HReg iselDfp32Expr(ISelEnv* env, IRExpr* e, IREndness IEndianess)
{
   HReg r = iselDfp32Expr_wrk( env, e, IEndianess );
   vassert(hregClass(r) == HRcFlt64);
   vassert( hregIsVirtual(r) );
   return r;
}

static HReg iselDfp32Expr_wrk(ISelEnv* env, IRExpr* e, IREndness IEndianess)
{
   Bool mode64 = env->mode64;
   IRType ty = typeOfIRExpr( env->type_env, e );

   vassert( e );
   vassert( ty == Ity_D32 );

   
   if (e->tag == Iex_Get) {
      HReg r_dst = newVRegF( env );
      PPCAMode* am_addr = PPCAMode_IR( e->Iex.Get.offset,
                                       GuestStatePtr(mode64) );
      addInstr( env, PPCInstr_FpLdSt( True, 8, r_dst, am_addr ) );
      return r_dst;
   }

   
   if (e->tag == Iex_Load && e->Iex.Load.end == IEndianess) {
      PPCAMode* am_addr;
      HReg r_dst = newVRegF(env);
      vassert(e->Iex.Load.ty == Ity_D32);
      am_addr = iselWordExpr_AMode(env, e->Iex.Load.addr, Ity_D32,
                                   IEndianess);
      addInstr(env, PPCInstr_FpLdSt(True, 4, r_dst, am_addr));
      return r_dst;
   }

   
   if (e->tag == Iex_Binop) {
      if (e->Iex.Binop.op == Iop_D64toD32) {
         HReg fr_dst = newVRegF(env);
         HReg fr_src = iselDfp64Expr(env, e->Iex.Binop.arg2, IEndianess);
         set_FPU_DFP_rounding_mode( env, e->Iex.Binop.arg1, IEndianess );
         addInstr(env, PPCInstr_Dfp64Unary(Pfp_DRSP, fr_dst, fr_src));
         return fr_dst;
      }
   }

   ppIRExpr( e );
   vpanic( "iselDfp32Expr_wrk(ppc)" );
}

static HReg iselDfp64Expr(ISelEnv* env, IRExpr* e, IREndness IEndianess)
{
   HReg r = iselDfp64Expr_wrk( env, e, IEndianess );
   vassert(hregClass(r) == HRcFlt64);
   vassert( hregIsVirtual(r) );
   return r;
}

static HReg iselDfp64Expr_wrk(ISelEnv* env, IRExpr* e, IREndness IEndianess)
{
   Bool mode64 = env->mode64;
   IRType ty = typeOfIRExpr( env->type_env, e );
   HReg r_dstHi, r_dstLo;

   vassert( e );
   vassert( ty == Ity_D64 );

   if (e->tag == Iex_RdTmp) {
      return lookupIRTemp( env, e->Iex.RdTmp.tmp );
   }

   
   if (e->tag == Iex_Get) {
      HReg r_dst = newVRegF( env );
      PPCAMode* am_addr = PPCAMode_IR( e->Iex.Get.offset,
                                       GuestStatePtr(mode64) );
      addInstr( env, PPCInstr_FpLdSt( True, 8, r_dst, am_addr ) );
      return r_dst;
   }

   if (e->tag == Iex_Load && e->Iex.Load.end == IEndianess) {
      PPCAMode* am_addr;
      HReg r_dst = newVRegF(env);
      vassert(e->Iex.Load.ty == Ity_D64);
      am_addr = iselWordExpr_AMode(env, e->Iex.Load.addr, Ity_D64,
                                   IEndianess);
      addInstr(env, PPCInstr_FpLdSt(True, 8, r_dst, am_addr));
      return r_dst;
   }

   
   if (e->tag == Iex_Qop) {
      HReg r_dst = newVRegF( env );
      return r_dst;
   }

   if (e->tag == Iex_Unop) {
      HReg fr_dst = newVRegF(env);
      switch (e->Iex.Unop.op) {
      case Iop_ReinterpI64asD64: {
         if (!mode64) {
            HReg r_srcHi, r_srcLo;
            iselInt64Expr( &r_srcHi, &r_srcLo, env, e->Iex.Unop.arg,
                           IEndianess);
            return mk_LoadRR32toFPR( env, r_srcHi, r_srcLo );
         } else {
            HReg r_src = iselWordExpr_R(env, e->Iex.Unop.arg, IEndianess);
            return mk_LoadR64toFPR( env, r_src );
         }
      }
      case Iop_D32toD64: {
         HReg fr_src = iselDfp32Expr(env, e->Iex.Unop.arg, IEndianess);
         addInstr(env, PPCInstr_Dfp64Unary(Pfp_DCTDP, fr_dst, fr_src));
         return fr_dst;
      }
      case Iop_D128HItoD64:
         iselDfp128Expr( &r_dstHi, &r_dstLo, env, e->Iex.Unop.arg,
                         IEndianess );
         return r_dstHi;
      case Iop_D128LOtoD64:
         iselDfp128Expr( &r_dstHi, &r_dstLo, env, e->Iex.Unop.arg,
                         IEndianess );
         return r_dstLo;
      case Iop_InsertExpD64: {
         HReg fr_srcL = iselDblExpr(env, e->Iex.Binop.arg1, IEndianess);
         HReg fr_srcR = iselDblExpr(env, e->Iex.Binop.arg2, IEndianess);

         addInstr(env, PPCInstr_Dfp64Binary(Pfp_DIEX, fr_dst, fr_srcL,
					    fr_srcR));
         return fr_dst;
       }
      default:
         vex_printf( "ERROR: iselDfp64Expr_wrk, UNKNOWN unop case %d\n",
                     e->Iex.Unop.op );
      }
   }

   if (e->tag == Iex_Binop) {
      PPCFpOp fpop = Pfp_INVALID;
      HReg fr_dst = newVRegF(env);

      switch (e->Iex.Binop.op) {
      case Iop_D128toD64:     fpop = Pfp_DRDPQ;  break;
      case Iop_D64toD32:      fpop = Pfp_DRSP;   break;
      case Iop_I64StoD64:     fpop = Pfp_DCFFIX; break;
      case Iop_RoundD64toInt: fpop = Pfp_DRINTN; break;
      default: break;
      }
      if (fpop == Pfp_DRDPQ) {
         HReg r_srcHi = newVRegF(env);
         HReg r_srcLo = newVRegF(env);

         set_FPU_DFP_rounding_mode( env, e->Iex.Binop.arg1, IEndianess );
         iselDfp128Expr(&r_srcHi, &r_srcLo, env, e->Iex.Binop.arg2,
                        IEndianess);
         addInstr(env, PPCInstr_DfpD128toD64(fpop, fr_dst, r_srcHi, r_srcLo));
         return fr_dst;

      } else if (fpop == Pfp_DRINTN) {
         HReg fr_src = newVRegF(env);
         PPCRI* r_rmc = iselWordExpr_RI(env, e->Iex.Binop.arg1, IEndianess);

         fr_src = iselDfp64Expr(env, e->Iex.Binop.arg2, IEndianess);
         addInstr(env, PPCInstr_DfpRound(fr_dst, fr_src, r_rmc));
         return fr_dst;

      } else if (fpop == Pfp_DRSP) {
         HReg fr_src = iselDfp64Expr(env, e->Iex.Binop.arg2, IEndianess);
         set_FPU_DFP_rounding_mode( env, e->Iex.Binop.arg1, IEndianess );
         addInstr(env, PPCInstr_Dfp64Unary(fpop, fr_dst, fr_src));
         return fr_dst;

      } else if (fpop == Pfp_DCFFIX) {
         HReg fr_src = newVRegF(env);
         PPCAMode* zero_r1 = PPCAMode_IR( 0, StackFramePtr(env->mode64) );

         set_FPU_DFP_rounding_mode( env, e->Iex.Binop.arg1, IEndianess );
         sub_from_sp( env, 16 );

         
         if (mode64) {
           HReg tmp = iselWordExpr_R(env, e->Iex.Binop.arg2, IEndianess);

           addInstr(env, PPCInstr_Store(8, zero_r1, tmp, True));
         } else {
            HReg tmpHi, tmpLo;
            PPCAMode* four_r1 = PPCAMode_IR( 4, StackFramePtr(env->mode64) );

            iselInt64Expr(&tmpHi, &tmpLo, env, e->Iex.Binop.arg2,
                          IEndianess);
            addInstr(env, PPCInstr_Store(4, zero_r1, tmpHi, False));
            addInstr(env, PPCInstr_Store(4, four_r1, tmpLo, False));
         }

         addInstr(env, PPCInstr_FpLdSt(True, 8,  fr_src, zero_r1));
         addInstr(env, PPCInstr_Dfp64Unary(fpop, fr_dst, fr_src));
         add_to_sp( env, 16 );
         return fr_dst;
      }

      switch (e->Iex.Binop.op) {
      
      case Iop_ShlD64: fpop = Pfp_DSCLI; break;
      case Iop_ShrD64: fpop = Pfp_DSCRI; break;
      default: break;
      }
      if (fpop != Pfp_INVALID) {
         HReg fr_src = iselDfp64Expr(env, e->Iex.Binop.arg1, IEndianess);
         PPCRI* shift = iselWordExpr_RI(env, e->Iex.Binop.arg2, IEndianess);

         
         vassert(shift->tag == Pri_Imm);

         addInstr(env, PPCInstr_DfpShift(fpop, fr_dst, fr_src, shift));
         return fr_dst;
      }

      switch (e->Iex.Binop.op) {
      case Iop_InsertExpD64:
         fpop = Pfp_DIEX;
         break;
      default: 	break;
      }
      if (fpop != Pfp_INVALID) {
         HReg fr_srcL = newVRegF(env);
         HReg fr_srcR = iselDfp64Expr(env, e->Iex.Binop.arg2, IEndianess);
         PPCAMode* zero_r1 = PPCAMode_IR( 0, StackFramePtr(env->mode64) );
         sub_from_sp( env, 16 );

         if (env->mode64) {
            
            HReg tmp = iselWordExpr_R(env, e->Iex.Binop.arg1, IEndianess);

            addInstr(env, PPCInstr_Store(8, zero_r1, tmp, True));
         } else {
            
            HReg tmpHi;
            HReg tmpLo;
            PPCAMode* four_r1 = PPCAMode_IR( 4, StackFramePtr(env->mode64) );

            iselInt64Expr(&tmpHi, &tmpLo, env, e->Iex.Binop.arg1,
                          IEndianess);
            addInstr(env, PPCInstr_Store(4, zero_r1, tmpHi, False));
            addInstr(env, PPCInstr_Store(4, four_r1, tmpLo, False));
         }
         addInstr(env, PPCInstr_FpLdSt(True, 8, fr_srcL, zero_r1));
         addInstr(env, PPCInstr_Dfp64Binary(fpop, fr_dst, fr_srcL,
                                            fr_srcR));
         add_to_sp( env, 16 );
         return fr_dst;
      }
   }

   if (e->tag == Iex_Triop) {
      IRTriop *triop = e->Iex.Triop.details;
      PPCFpOp fpop = Pfp_INVALID;

      switch (triop->op) {
      case Iop_AddD64:
         fpop = Pfp_DFPADD;
         break;
      case Iop_SubD64:
         fpop = Pfp_DFPSUB;
         break;
      case Iop_MulD64:
         fpop = Pfp_DFPMUL;
         break;
      case Iop_DivD64:
         fpop = Pfp_DFPDIV;
         break;
      default:
         break;
      }
      if (fpop != Pfp_INVALID) {
         HReg r_dst = newVRegF( env );
         HReg r_srcL = iselDfp64Expr( env, triop->arg2, IEndianess );
         HReg r_srcR = iselDfp64Expr( env, triop->arg3, IEndianess );

         set_FPU_DFP_rounding_mode( env, triop->arg1, IEndianess );
         addInstr( env, PPCInstr_Dfp64Binary( fpop, r_dst, r_srcL, r_srcR ) );
         return r_dst;
      }

      switch (triop->op) {
      case Iop_QuantizeD64:          fpop = Pfp_DQUA;  break;
      case Iop_SignificanceRoundD64: fpop = Pfp_RRDTR; break;
      default: break;
      }
      if (fpop == Pfp_DQUA) {
         HReg r_dst = newVRegF(env);
         HReg r_srcL = iselDfp64Expr(env, triop->arg2, IEndianess);
         HReg r_srcR = iselDfp64Expr(env, triop->arg3, IEndianess);
         PPCRI* rmc  = iselWordExpr_RI(env, triop->arg1, IEndianess);
         addInstr(env, PPCInstr_DfpQuantize(fpop, r_dst, r_srcL, r_srcR,
                                            rmc));
         return r_dst;

      } else if (fpop == Pfp_RRDTR) {
         HReg r_dst = newVRegF(env);
         HReg r_srcL = newVRegF(env);
         HReg r_srcR = iselDfp64Expr(env, triop->arg3, IEndianess);
         PPCRI* rmc  = iselWordExpr_RI(env, triop->arg1, IEndianess);
         PPCAMode* zero_r1 = PPCAMode_IR( 0, StackFramePtr(env->mode64) );
         HReg i8_val = iselWordExpr_R(env, triop->arg2, IEndianess);

         
         sub_from_sp( env, 16 );
         if (mode64)
            addInstr(env, PPCInstr_Store(8, zero_r1, i8_val, True));
         else
            addInstr(env, PPCInstr_Store(4, zero_r1, i8_val, False));

         addInstr(env, PPCInstr_FpLdSt(True, 8, r_srcL, zero_r1));
         add_to_sp( env, 16 );

         
         addInstr(env, PPCInstr_DfpQuantize(fpop, r_dst, r_srcL, r_srcR, rmc));
         return r_dst;
      }
   }

   ppIRExpr( e );
   vpanic( "iselDfp64Expr_wrk(ppc)" );
}

static void iselDfp128Expr(HReg* rHi, HReg* rLo, ISelEnv* env, IRExpr* e,
                           IREndness IEndianess)
{
   iselDfp128Expr_wrk( rHi, rLo, env, e, IEndianess );
   vassert( hregIsVirtual(*rHi) );
   vassert( hregIsVirtual(*rLo) );
}

static void iselDfp128Expr_wrk(HReg* rHi, HReg *rLo, ISelEnv* env, IRExpr* e,
                               IREndness IEndianess)
{
   vassert( e );
   vassert( typeOfIRExpr(env->type_env,e) == Ity_D128 );

   
   if (e->tag == Iex_RdTmp) {
      lookupIRTempPair( rHi, rLo, env, e->Iex.RdTmp.tmp );
      return;
   }

   if (e->tag == Iex_Unop) {
      HReg r_dstHi = newVRegF(env);
      HReg r_dstLo = newVRegF(env);

      if (e->Iex.Unop.op == Iop_I64StoD128) {
         HReg fr_src = newVRegF(env);
         PPCAMode* zero_r1 = PPCAMode_IR( 0, StackFramePtr(env->mode64) );

         
         if (env->mode64) {
            HReg tmp   = iselWordExpr_R(env, e->Iex.Unop.arg, IEndianess);
            addInstr(env, PPCInstr_Store(8, zero_r1, tmp, True));
         } else {
            HReg tmpHi, tmpLo;
            PPCAMode* four_r1 = PPCAMode_IR( 4, StackFramePtr(env->mode64) );

            iselInt64Expr(&tmpHi, &tmpLo, env, e->Iex.Unop.arg,
                          IEndianess);
            addInstr(env, PPCInstr_Store(4, zero_r1, tmpHi, False));
            addInstr(env, PPCInstr_Store(4, four_r1, tmpLo, False));
         }

         addInstr(env, PPCInstr_FpLdSt(True, 8, fr_src, zero_r1));
         addInstr(env, PPCInstr_DfpI64StoD128(Pfp_DCFFIXQ, r_dstHi, r_dstLo,
                                              fr_src));
      }

      if (e->Iex.Unop.op == Iop_D64toD128) {
         HReg r_src = iselDfp64Expr(env, e->Iex.Unop.arg, IEndianess);

         addInstr(env, PPCInstr_Dfp128Unary(Pfp_DCTQPQ, r_dstHi, r_dstLo,
                                            r_src, r_src));
      }
      *rHi = r_dstHi;
      *rLo = r_dstLo;
      return;
   }

   
   if (e->tag == Iex_Binop) {
      HReg r_srcHi;
      HReg r_srcLo;

      switch (e->Iex.Binop.op) {
      case Iop_D64HLtoD128:
         r_srcHi = iselDfp64Expr( env, e->Iex.Binop.arg1, IEndianess );
         r_srcLo = iselDfp64Expr( env, e->Iex.Binop.arg2, IEndianess );
         *rHi = r_srcHi;
         *rLo = r_srcLo;
         return;
         break;
      case Iop_D128toD64: {
         PPCFpOp fpop = Pfp_DRDPQ;
         HReg fr_dst  = newVRegF(env);

         set_FPU_DFP_rounding_mode( env, e->Iex.Binop.arg1, IEndianess );
         iselDfp128Expr(&r_srcHi, &r_srcLo, env, e->Iex.Binop.arg2,
                        IEndianess);
         addInstr(env, PPCInstr_DfpD128toD64(fpop, fr_dst, r_srcHi, r_srcLo));

         *rHi = fr_dst;
         *rLo = fr_dst;
         return;
      }
      case Iop_ShlD128: 
      case Iop_ShrD128: {
         HReg fr_dst_hi = newVRegF(env);  
         HReg fr_dst_lo = newVRegF(env);
         PPCRI* shift = iselWordExpr_RI(env, e->Iex.Binop.arg2, IEndianess);
         PPCFpOp fpop = Pfp_DSCLIQ;  

         iselDfp128Expr(&r_srcHi, &r_srcLo, env, e->Iex.Binop.arg1,
                        IEndianess);

         if (e->Iex.Binop.op == Iop_ShrD128)
            fpop = Pfp_DSCRIQ;

         addInstr(env, PPCInstr_DfpShift128(fpop, fr_dst_hi, fr_dst_lo,
                                            r_srcHi, r_srcLo, shift));

         *rHi = fr_dst_hi;
         *rLo = fr_dst_lo;
         return;
      }
      case Iop_RoundD128toInt: {
         HReg r_dstHi = newVRegF(env);
         HReg r_dstLo = newVRegF(env);
         PPCRI* r_rmc = iselWordExpr_RI(env, e->Iex.Binop.arg1, IEndianess);

         
         iselDfp128Expr(&r_srcHi, &r_srcLo, env, e->Iex.Binop.arg2,
                        IEndianess);

         addInstr(env, PPCInstr_DfpRound128(r_dstHi, r_dstLo,
                                            r_srcHi, r_srcLo, r_rmc));
         *rHi = r_dstHi;
         *rLo = r_dstLo;
         return;
      }
      case Iop_InsertExpD128: {
         HReg r_dstHi = newVRegF(env);
         HReg r_dstLo = newVRegF(env);
         HReg r_srcL  = newVRegF(env);
         PPCAMode* zero_r1 = PPCAMode_IR( 0, StackFramePtr(env->mode64) );
         r_srcHi = newVRegF(env);
         r_srcLo = newVRegF(env);

         iselDfp128Expr(&r_srcHi, &r_srcLo, env, e->Iex.Binop.arg2,
                        IEndianess);

         
         if (env->mode64) {
            HReg tmp = iselWordExpr_R(env, e->Iex.Binop.arg1, IEndianess);
            addInstr(env, PPCInstr_Store(8, zero_r1, tmp, True));
         } else {
            HReg tmpHi, tmpLo;
            PPCAMode* four_r1 = PPCAMode_IR( 4, StackFramePtr(env->mode64) );

            iselInt64Expr(&tmpHi, &tmpLo, env, e->Iex.Unop.arg,
                          IEndianess);
            addInstr(env, PPCInstr_Store(4, zero_r1, tmpHi, False));
            addInstr(env, PPCInstr_Store(4, four_r1, tmpLo, False));
         }

         addInstr(env, PPCInstr_FpLdSt(True, 8, r_srcL, zero_r1));
         addInstr(env, PPCInstr_InsertExpD128(Pfp_DIEXQ,
                                              r_dstHi, r_dstLo,
                                              r_srcL, r_srcHi, r_srcLo));
         *rHi = r_dstHi;
         *rLo = r_dstLo;
         return;
      }
      default:
         vex_printf( "ERROR: iselDfp128Expr_wrk, UNKNOWN binop case %d\n",
                     e->Iex.Binop.op );
         break;
      }
   }

   if (e->tag == Iex_Triop) {
      IRTriop *triop = e->Iex.Triop.details;
      PPCFpOp fpop = Pfp_INVALID;
      HReg r_dstHi = newVRegF(env);
      HReg r_dstLo = newVRegF(env);

      switch (triop->op) {
      case Iop_AddD128:
         fpop = Pfp_DFPADDQ;
         break;
      case Iop_SubD128:
         fpop = Pfp_DFPSUBQ;
         break;
      case Iop_MulD128:
         fpop = Pfp_DFPMULQ;
         break;
      case Iop_DivD128:
         fpop = Pfp_DFPDIVQ;
         break;
      default:
         break;
      }

      if (fpop != Pfp_INVALID) {
         HReg r_srcRHi = newVRegV( env );
         HReg r_srcRLo = newVRegV( env );

         
         iselDfp128Expr( &r_dstHi, &r_dstLo, env, triop->arg2, IEndianess );
         iselDfp128Expr( &r_srcRHi, &r_srcRLo, env, triop->arg3, IEndianess );
         set_FPU_DFP_rounding_mode( env, triop->arg1, IEndianess );
         addInstr( env,
                   PPCInstr_Dfp128Binary( fpop, r_dstHi, r_dstLo,
                                          r_srcRHi, r_srcRLo ) );
         *rHi = r_dstHi;
         *rLo = r_dstLo;
         return;
      }
      switch (triop->op) {
      case Iop_QuantizeD128:          fpop = Pfp_DQUAQ;  break;
      case Iop_SignificanceRoundD128: fpop = Pfp_DRRNDQ; break;
      default: break;
      }
      if (fpop == Pfp_DQUAQ) {
         HReg r_srcHi = newVRegF(env);
         HReg r_srcLo = newVRegF(env);
         PPCRI* rmc = iselWordExpr_RI(env, triop->arg1, IEndianess);

         
         iselDfp128Expr(&r_dstHi, &r_dstLo, env, triop->arg2, IEndianess);
         iselDfp128Expr(&r_srcHi, &r_srcLo, env, triop->arg3, IEndianess);

         
         addInstr(env, PPCInstr_DfpQuantize128(fpop, r_dstHi, r_dstLo,
                                               r_srcHi, r_srcLo, rmc));
        *rHi = r_dstHi;
        *rLo = r_dstLo;
         return;

      } else if (fpop == Pfp_DRRNDQ) {
         HReg r_srcHi = newVRegF(env);
         HReg r_srcLo = newVRegF(env);
         PPCRI* rmc = iselWordExpr_RI(env, triop->arg1, IEndianess);
         PPCAMode* zero_r1 = PPCAMode_IR( 0, StackFramePtr(env->mode64) );
         PPCAMode* four_r1 = PPCAMode_IR( 4, StackFramePtr(env->mode64) );
         HReg i8_val = iselWordExpr_R(env, triop->arg2, IEndianess);
         HReg r_zero = newVRegI( env );

         iselDfp128Expr(&r_srcHi, &r_srcLo, env, triop->arg3, IEndianess);

         
         sub_from_sp( env, 16 );

         if (env->mode64)
            addInstr(env, PPCInstr_Store(4, four_r1, i8_val, True));
         else
            addInstr(env, PPCInstr_Store(4, four_r1, i8_val, False));

         addInstr( env, PPCInstr_LI( r_zero, 0, env->mode64 ) );
         addInstr(env, PPCInstr_FpLdSt(True, 8, r_dstHi, zero_r1));
         addInstr(env, PPCInstr_FpLdSt(True, 8, r_dstLo, zero_r1));

         add_to_sp( env, 16 );

         
         addInstr(env, PPCInstr_DfpQuantize128(fpop, r_dstHi, r_dstLo,
                                               r_srcHi, r_srcLo, rmc));
         *rHi = r_dstHi;
         *rLo = r_dstLo;
         return;
      }
 }

   ppIRExpr( e );
   vpanic( "iselDfp128Expr(ppc64)" );
}



static HReg iselVecExpr ( ISelEnv* env, IRExpr* e, IREndness IEndianess )
{
   HReg r = iselVecExpr_wrk( env, e, IEndianess );
#  if 0
   vex_printf("\n"); ppIRExpr(e); vex_printf("\n");
#  endif
   vassert(hregClass(r) == HRcVec128);
   vassert(hregIsVirtual(r));
   return r;
}

static HReg iselVecExpr_wrk ( ISelEnv* env, IRExpr* e, IREndness IEndianess )
{
   Bool mode64 = env->mode64;
   PPCAvOp op = Pav_INVALID;
   PPCAvFpOp fpop = Pavfp_INVALID;
   IRType  ty = typeOfIRExpr(env->type_env,e);
   vassert(e);
   vassert(ty == Ity_V128);

   if (e->tag == Iex_RdTmp) {
      return lookupIRTemp(env, e->Iex.RdTmp.tmp);
   }

   if (e->tag == Iex_Get) {
      HReg dst = newVRegV(env);
      addInstr(env,
               PPCInstr_AvLdSt( True, 16, dst,
                                PPCAMode_IR( e->Iex.Get.offset,
                                             GuestStatePtr(mode64) )));
      return dst;
   }

   if (e->tag == Iex_Load && e->Iex.Load.end == IEndianess) {

      HReg Vhi   = newVRegV(env);
      HReg Vlo   = newVRegV(env);
      HReg Vp    = newVRegV(env);
      HReg v_dst = newVRegV(env);
      HReg rB;
      HReg rB_plus_15 = newVRegI(env);

      vassert(e->Iex.Load.ty == Ity_V128);
      rB = iselWordExpr_R( env, e->Iex.Load.addr, IEndianess );

      
      addInstr(env, PPCInstr_AvLdSt( True, 16, Vhi,
                                     PPCAMode_IR(0, rB)) );

      if (IEndianess == Iend_LE)
         
         addInstr(env, PPCInstr_AvSh( False, Vp,
                                      PPCAMode_IR(0, rB)) );
      else
         
         addInstr(env, PPCInstr_AvSh( True, Vp,
                                      PPCAMode_IR(0, rB)) );

      
      addInstr(env, PPCInstr_Alu( Palu_ADD, rB_plus_15,
                                  rB, PPCRH_Imm(True, toUShort(15))) );

      
      addInstr(env, PPCInstr_AvLdSt( True, 16, Vlo,
                                     PPCAMode_IR(0, rB_plus_15)) );

      if (IEndianess == Iend_LE)
         
         addInstr(env, PPCInstr_AvPerm( v_dst, Vlo, Vhi, Vp ));
      else
         
         addInstr(env, PPCInstr_AvPerm( v_dst, Vhi, Vlo, Vp ));

      return v_dst;
   }

   if (e->tag == Iex_Unop) {
      switch (e->Iex.Unop.op) {

      case Iop_NotV128: {
         HReg arg = iselVecExpr(env, e->Iex.Unop.arg, IEndianess);
         HReg dst = newVRegV(env);
         addInstr(env, PPCInstr_AvUnary(Pav_NOT, dst, arg));
         return dst;
      }

      case Iop_CmpNEZ8x16: {
         HReg arg  = iselVecExpr(env, e->Iex.Unop.arg, IEndianess);
         HReg zero = newVRegV(env);
         HReg dst  = newVRegV(env);
         addInstr(env, PPCInstr_AvBinary(Pav_XOR, zero, zero, zero));
         addInstr(env, PPCInstr_AvBin8x16(Pav_CMPEQU, dst, arg, zero));
         addInstr(env, PPCInstr_AvUnary(Pav_NOT, dst, dst));
         return dst;
      }

      case Iop_CmpNEZ16x8: {
         HReg arg  = iselVecExpr(env, e->Iex.Unop.arg, IEndianess);
         HReg zero = newVRegV(env);
         HReg dst  = newVRegV(env);
         addInstr(env, PPCInstr_AvBinary(Pav_XOR, zero, zero, zero));
         addInstr(env, PPCInstr_AvBin16x8(Pav_CMPEQU, dst, arg, zero));
         addInstr(env, PPCInstr_AvUnary(Pav_NOT, dst, dst));
         return dst;
      }

      case Iop_CmpNEZ32x4: {
         HReg arg  = iselVecExpr(env, e->Iex.Unop.arg, IEndianess);
         HReg zero = newVRegV(env);
         HReg dst  = newVRegV(env);
         addInstr(env, PPCInstr_AvBinary(Pav_XOR, zero, zero, zero));
         addInstr(env, PPCInstr_AvBin32x4(Pav_CMPEQU, dst, arg, zero));
         addInstr(env, PPCInstr_AvUnary(Pav_NOT, dst, dst));
         return dst;
      }

      case Iop_CmpNEZ64x2: {
         HReg arg  = iselVecExpr(env, e->Iex.Unop.arg, IEndianess);
         HReg zero = newVRegV(env);
         HReg dst  = newVRegV(env);
         addInstr(env, PPCInstr_AvBinary(Pav_XOR, zero, zero, zero));
         addInstr(env, PPCInstr_AvBin64x2(Pav_CMPEQU, dst, arg, zero));
         addInstr(env, PPCInstr_AvUnary(Pav_NOT, dst, dst));
         return dst;
      }

      case Iop_RecipEst32Fx4: fpop = Pavfp_RCPF;    goto do_32Fx4_unary;
      case Iop_RSqrtEst32Fx4: fpop = Pavfp_RSQRTF;  goto do_32Fx4_unary;
      case Iop_I32UtoFx4:     fpop = Pavfp_CVTU2F;  goto do_32Fx4_unary;
      case Iop_I32StoFx4:     fpop = Pavfp_CVTS2F;  goto do_32Fx4_unary;
      case Iop_QFtoI32Ux4_RZ: fpop = Pavfp_QCVTF2U; goto do_32Fx4_unary;
      case Iop_QFtoI32Sx4_RZ: fpop = Pavfp_QCVTF2S; goto do_32Fx4_unary;
      case Iop_RoundF32x4_RM: fpop = Pavfp_ROUNDM;  goto do_32Fx4_unary;
      case Iop_RoundF32x4_RP: fpop = Pavfp_ROUNDP;  goto do_32Fx4_unary;
      case Iop_RoundF32x4_RN: fpop = Pavfp_ROUNDN;  goto do_32Fx4_unary;
      case Iop_RoundF32x4_RZ: fpop = Pavfp_ROUNDZ;  goto do_32Fx4_unary;
      do_32Fx4_unary:
      {
         HReg arg = iselVecExpr(env, e->Iex.Unop.arg, IEndianess);
         HReg dst = newVRegV(env);
         addInstr(env, PPCInstr_AvUn32Fx4(fpop, dst, arg));
         return dst;
      }

      case Iop_32UtoV128: {
         HReg r_aligned16, r_zeros;
         HReg r_src = iselWordExpr_R(env, e->Iex.Unop.arg, IEndianess);
         HReg   dst = newVRegV(env);
         PPCAMode *am_off0, *am_off4, *am_off8, *am_off12;
         sub_from_sp( env, 32 );     

         
         r_aligned16 = get_sp_aligned16( env );
         am_off0  = PPCAMode_IR( 0,  r_aligned16 );
         am_off4  = PPCAMode_IR( 4,  r_aligned16 );
         am_off8  = PPCAMode_IR( 8,  r_aligned16 );
         am_off12 = PPCAMode_IR( 12, r_aligned16 );

         
         r_zeros = newVRegI(env);
         addInstr(env, PPCInstr_LI(r_zeros, 0x0, mode64));
         if (IEndianess == Iend_LE)
            addInstr(env, PPCInstr_Store( 4, am_off0, r_src, mode64 ));
         else
            addInstr(env, PPCInstr_Store( 4, am_off0, r_zeros, mode64 ));
         addInstr(env, PPCInstr_Store( 4, am_off4, r_zeros, mode64 ));
         addInstr(env, PPCInstr_Store( 4, am_off8, r_zeros, mode64 ));

         
         if (IEndianess == Iend_LE)
            addInstr(env, PPCInstr_Store( 4, am_off12, r_zeros, mode64 ));
         else
            addInstr(env, PPCInstr_Store( 4, am_off12, r_src, mode64 ));

         
         if (IEndianess == Iend_LE)
            addInstr(env, PPCInstr_AvLdSt( True, 4, dst, am_off0 ));
         else
            addInstr(env, PPCInstr_AvLdSt( True, 4, dst, am_off12 ));

         add_to_sp( env, 32 );       
         return dst;
      }

      case Iop_Dup8x16:
      case Iop_Dup16x8:
      case Iop_Dup32x4:
         return mk_AvDuplicateRI(env, e->Iex.Unop.arg, IEndianess);

      case Iop_CipherSV128: op = Pav_CIPHERSUBV128; goto do_AvCipherV128Un;
      do_AvCipherV128Un: {
         HReg arg = iselVecExpr(env, e->Iex.Unop.arg, IEndianess);
         HReg dst = newVRegV(env);
         addInstr(env, PPCInstr_AvCipherV128Unary(op, dst, arg));
         return dst;
      }

      case Iop_Clz8x16: op = Pav_ZEROCNTBYTE;   goto do_zerocnt;
      case Iop_Clz16x8: op = Pav_ZEROCNTHALF;   goto do_zerocnt;
      case Iop_Clz32x4: op = Pav_ZEROCNTWORD;   goto do_zerocnt;
      case Iop_Clz64x2: op = Pav_ZEROCNTDBL;    goto do_zerocnt;
      case Iop_PwBitMtxXpose64x2: op = Pav_BITMTXXPOSE;  goto do_zerocnt;
      do_zerocnt:
      {
        HReg arg = iselVecExpr(env, e->Iex.Unop.arg, IEndianess);
        HReg dst = newVRegV(env);
        addInstr(env, PPCInstr_AvUnary(op, dst, arg));
        return dst;
      }

      default:
         break;
      } 
   } 

   if (e->tag == Iex_Binop) {
      switch (e->Iex.Binop.op) {

      case Iop_64HLtoV128: {
         if (!mode64) {
            HReg     r3, r2, r1, r0, r_aligned16;
            PPCAMode *am_off0, *am_off4, *am_off8, *am_off12;
            HReg     dst = newVRegV(env);
            
            sub_from_sp( env, 32 );        
            
            
            r_aligned16 = get_sp_aligned16( env );
            am_off0  = PPCAMode_IR( 0,  r_aligned16 );
            am_off4  = PPCAMode_IR( 4,  r_aligned16 );
            am_off8  = PPCAMode_IR( 8,  r_aligned16 );
            am_off12 = PPCAMode_IR( 12, r_aligned16 );
            
            
            iselInt64Expr(&r1, &r0, env, e->Iex.Binop.arg2, IEndianess);
            addInstr(env, PPCInstr_Store( 4, am_off12, r0, mode64 ));
            addInstr(env, PPCInstr_Store( 4, am_off8,  r1, mode64 ));
            
            iselInt64Expr(&r3, &r2, env, e->Iex.Binop.arg1, IEndianess);
            addInstr(env, PPCInstr_Store( 4, am_off4, r2, mode64 ));
            addInstr(env, PPCInstr_Store( 4, am_off0, r3, mode64 ));
            
            
            addInstr(env, PPCInstr_AvLdSt(True, 16, dst, am_off0));
            
            add_to_sp( env, 32 );          
            return dst;
         } else {
            HReg     rHi = iselWordExpr_R(env, e->Iex.Binop.arg1, IEndianess);
            HReg     rLo = iselWordExpr_R(env, e->Iex.Binop.arg2, IEndianess);
            HReg     dst = newVRegV(env);
            HReg     r_aligned16;
            PPCAMode *am_off0, *am_off8;
            
            sub_from_sp( env, 32 );        
            
            
            r_aligned16 = get_sp_aligned16( env );
            am_off0  = PPCAMode_IR( 0,  r_aligned16 );
            am_off8  = PPCAMode_IR( 8,  r_aligned16 );
            
            
            if (IEndianess == Iend_LE) {
               addInstr(env, PPCInstr_Store( 8, am_off0, rLo, mode64 ));
               addInstr(env, PPCInstr_Store( 8, am_off8, rHi, mode64 ));
            } else {
               addInstr(env, PPCInstr_Store( 8, am_off0, rHi, mode64 ));
               addInstr(env, PPCInstr_Store( 8, am_off8, rLo, mode64 ));
            }
            
            addInstr(env, PPCInstr_AvLdSt(True, 16, dst, am_off0));
            
            add_to_sp( env, 32 );          
            return dst;
         }
      }

      case Iop_Max32Fx4:   fpop = Pavfp_MAXF;   goto do_32Fx4;
      case Iop_Min32Fx4:   fpop = Pavfp_MINF;   goto do_32Fx4;
      case Iop_CmpEQ32Fx4: fpop = Pavfp_CMPEQF; goto do_32Fx4;
      case Iop_CmpGT32Fx4: fpop = Pavfp_CMPGTF; goto do_32Fx4;
      case Iop_CmpGE32Fx4: fpop = Pavfp_CMPGEF; goto do_32Fx4;
      do_32Fx4:
      {
         HReg argL = iselVecExpr(env, e->Iex.Binop.arg1, IEndianess);
         HReg argR = iselVecExpr(env, e->Iex.Binop.arg2, IEndianess);
         HReg dst = newVRegV(env);
         addInstr(env, PPCInstr_AvBin32Fx4(fpop, dst, argL, argR));
         return dst;
      }

      case Iop_CmpLE32Fx4: {
         HReg argL = iselVecExpr(env, e->Iex.Binop.arg1, IEndianess);
         HReg argR = iselVecExpr(env, e->Iex.Binop.arg2, IEndianess);
         HReg dst = newVRegV(env);
         
         HReg isNanLR = newVRegV(env);
         HReg isNanL = isNan(env, argL, IEndianess);
         HReg isNanR = isNan(env, argR, IEndianess);
         addInstr(env, PPCInstr_AvBinary(Pav_OR, isNanLR,
                                         isNanL, isNanR));

         addInstr(env, PPCInstr_AvBin32Fx4(Pavfp_CMPGTF, dst,
                                           argL, argR));
         addInstr(env, PPCInstr_AvBinary(Pav_OR, dst, dst, isNanLR));
         addInstr(env, PPCInstr_AvUnary(Pav_NOT, dst, dst));
         return dst;
      }

      case Iop_AndV128:    op = Pav_AND;      goto do_AvBin;
      case Iop_OrV128:     op = Pav_OR;       goto do_AvBin;
      case Iop_XorV128:    op = Pav_XOR;      goto do_AvBin;
      do_AvBin: {
         HReg arg1 = iselVecExpr(env, e->Iex.Binop.arg1, IEndianess);
         HReg arg2 = iselVecExpr(env, e->Iex.Binop.arg2, IEndianess);
         HReg dst  = newVRegV(env);
         addInstr(env, PPCInstr_AvBinary(op, dst, arg1, arg2));
         return dst;
      }

      case Iop_Shl8x16:    op = Pav_SHL;    goto do_AvBin8x16;
      case Iop_Shr8x16:    op = Pav_SHR;    goto do_AvBin8x16;
      case Iop_Sar8x16:    op = Pav_SAR;    goto do_AvBin8x16;
      case Iop_Rol8x16:    op = Pav_ROTL;   goto do_AvBin8x16;
      case Iop_InterleaveHI8x16: op = Pav_MRGHI;  goto do_AvBin8x16;
      case Iop_InterleaveLO8x16: op = Pav_MRGLO;  goto do_AvBin8x16;
      case Iop_Add8x16:    op = Pav_ADDU;   goto do_AvBin8x16;
      case Iop_QAdd8Ux16:  op = Pav_QADDU;  goto do_AvBin8x16;
      case Iop_QAdd8Sx16:  op = Pav_QADDS;  goto do_AvBin8x16;
      case Iop_Sub8x16:    op = Pav_SUBU;   goto do_AvBin8x16;
      case Iop_QSub8Ux16:  op = Pav_QSUBU;  goto do_AvBin8x16;
      case Iop_QSub8Sx16:  op = Pav_QSUBS;  goto do_AvBin8x16;
      case Iop_Avg8Ux16:   op = Pav_AVGU;   goto do_AvBin8x16;
      case Iop_Avg8Sx16:   op = Pav_AVGS;   goto do_AvBin8x16;
      case Iop_Max8Ux16:   op = Pav_MAXU;   goto do_AvBin8x16;
      case Iop_Max8Sx16:   op = Pav_MAXS;   goto do_AvBin8x16;
      case Iop_Min8Ux16:   op = Pav_MINU;   goto do_AvBin8x16;
      case Iop_Min8Sx16:   op = Pav_MINS;   goto do_AvBin8x16;
      case Iop_MullEven8Ux16: op = Pav_OMULU;  goto do_AvBin8x16;
      case Iop_MullEven8Sx16: op = Pav_OMULS;  goto do_AvBin8x16;
      case Iop_CmpEQ8x16:  op = Pav_CMPEQU; goto do_AvBin8x16;
      case Iop_CmpGT8Ux16: op = Pav_CMPGTU; goto do_AvBin8x16;
      case Iop_CmpGT8Sx16: op = Pav_CMPGTS; goto do_AvBin8x16;
      case Iop_PolynomialMulAdd8x16: op = Pav_POLYMULADD; goto do_AvBin8x16;
      do_AvBin8x16: {
         HReg arg1 = iselVecExpr(env, e->Iex.Binop.arg1, IEndianess);
         HReg arg2 = iselVecExpr(env, e->Iex.Binop.arg2, IEndianess);
         HReg dst  = newVRegV(env);
         addInstr(env, PPCInstr_AvBin8x16(op, dst, arg1, arg2));
         return dst;
      }

      case Iop_Shl16x8:    op = Pav_SHL;    goto do_AvBin16x8;
      case Iop_Shr16x8:    op = Pav_SHR;    goto do_AvBin16x8;
      case Iop_Sar16x8:    op = Pav_SAR;    goto do_AvBin16x8;
      case Iop_Rol16x8:    op = Pav_ROTL;   goto do_AvBin16x8;
      case Iop_NarrowBin16to8x16:    op = Pav_PACKUU;  goto do_AvBin16x8;
      case Iop_QNarrowBin16Uto8Ux16: op = Pav_QPACKUU; goto do_AvBin16x8;
      case Iop_QNarrowBin16Sto8Sx16: op = Pav_QPACKSS; goto do_AvBin16x8;
      case Iop_InterleaveHI16x8:  op = Pav_MRGHI;  goto do_AvBin16x8;
      case Iop_InterleaveLO16x8:  op = Pav_MRGLO;  goto do_AvBin16x8;
      case Iop_Add16x8:    op = Pav_ADDU;   goto do_AvBin16x8;
      case Iop_QAdd16Ux8:  op = Pav_QADDU;  goto do_AvBin16x8;
      case Iop_QAdd16Sx8:  op = Pav_QADDS;  goto do_AvBin16x8;
      case Iop_Sub16x8:    op = Pav_SUBU;   goto do_AvBin16x8;
      case Iop_QSub16Ux8:  op = Pav_QSUBU;  goto do_AvBin16x8;
      case Iop_QSub16Sx8:  op = Pav_QSUBS;  goto do_AvBin16x8;
      case Iop_Avg16Ux8:   op = Pav_AVGU;   goto do_AvBin16x8;
      case Iop_Avg16Sx8:   op = Pav_AVGS;   goto do_AvBin16x8;
      case Iop_Max16Ux8:   op = Pav_MAXU;   goto do_AvBin16x8;
      case Iop_Max16Sx8:   op = Pav_MAXS;   goto do_AvBin16x8;
      case Iop_Min16Ux8:   op = Pav_MINU;   goto do_AvBin16x8;
      case Iop_Min16Sx8:   op = Pav_MINS;   goto do_AvBin16x8;
      case Iop_MullEven16Ux8: op = Pav_OMULU;  goto do_AvBin16x8;
      case Iop_MullEven16Sx8: op = Pav_OMULS;  goto do_AvBin16x8;
      case Iop_CmpEQ16x8:  op = Pav_CMPEQU; goto do_AvBin16x8;
      case Iop_CmpGT16Ux8: op = Pav_CMPGTU; goto do_AvBin16x8;
      case Iop_CmpGT16Sx8: op = Pav_CMPGTS; goto do_AvBin16x8;
      case Iop_PolynomialMulAdd16x8: op = Pav_POLYMULADD; goto do_AvBin16x8;
      do_AvBin16x8: {
         HReg arg1 = iselVecExpr(env, e->Iex.Binop.arg1, IEndianess);
         HReg arg2 = iselVecExpr(env, e->Iex.Binop.arg2, IEndianess);
         HReg dst  = newVRegV(env);
         addInstr(env, PPCInstr_AvBin16x8(op, dst, arg1, arg2));
         return dst;
      }

      case Iop_Shl32x4:    op = Pav_SHL;    goto do_AvBin32x4;
      case Iop_Shr32x4:    op = Pav_SHR;    goto do_AvBin32x4;
      case Iop_Sar32x4:    op = Pav_SAR;    goto do_AvBin32x4;
      case Iop_Rol32x4:    op = Pav_ROTL;   goto do_AvBin32x4;
      case Iop_NarrowBin32to16x8:    op = Pav_PACKUU;  goto do_AvBin32x4;
      case Iop_QNarrowBin32Uto16Ux8: op = Pav_QPACKUU; goto do_AvBin32x4;
      case Iop_QNarrowBin32Sto16Sx8: op = Pav_QPACKSS; goto do_AvBin32x4;
      case Iop_InterleaveHI32x4:  op = Pav_MRGHI;  goto do_AvBin32x4;
      case Iop_InterleaveLO32x4:  op = Pav_MRGLO;  goto do_AvBin32x4;
      case Iop_Add32x4:    op = Pav_ADDU;   goto do_AvBin32x4;
      case Iop_QAdd32Ux4:  op = Pav_QADDU;  goto do_AvBin32x4;
      case Iop_QAdd32Sx4:  op = Pav_QADDS;  goto do_AvBin32x4;
      case Iop_Sub32x4:    op = Pav_SUBU;   goto do_AvBin32x4;
      case Iop_QSub32Ux4:  op = Pav_QSUBU;  goto do_AvBin32x4;
      case Iop_QSub32Sx4:  op = Pav_QSUBS;  goto do_AvBin32x4;
      case Iop_Avg32Ux4:   op = Pav_AVGU;   goto do_AvBin32x4;
      case Iop_Avg32Sx4:   op = Pav_AVGS;   goto do_AvBin32x4;
      case Iop_Max32Ux4:   op = Pav_MAXU;   goto do_AvBin32x4;
      case Iop_Max32Sx4:   op = Pav_MAXS;   goto do_AvBin32x4;
      case Iop_Min32Ux4:   op = Pav_MINU;   goto do_AvBin32x4;
      case Iop_Min32Sx4:   op = Pav_MINS;   goto do_AvBin32x4;
      case Iop_Mul32x4:    op = Pav_MULU;   goto do_AvBin32x4;
      case Iop_MullEven32Ux4: op = Pav_OMULU;  goto do_AvBin32x4;
      case Iop_MullEven32Sx4: op = Pav_OMULS;  goto do_AvBin32x4;
      case Iop_CmpEQ32x4:  op = Pav_CMPEQU; goto do_AvBin32x4;
      case Iop_CmpGT32Ux4: op = Pav_CMPGTU; goto do_AvBin32x4;
      case Iop_CmpGT32Sx4: op = Pav_CMPGTS; goto do_AvBin32x4;
      case Iop_CatOddLanes32x4:  op = Pav_CATODD;  goto do_AvBin32x4;
      case Iop_CatEvenLanes32x4: op = Pav_CATEVEN; goto do_AvBin32x4;
      case Iop_PolynomialMulAdd32x4: op = Pav_POLYMULADD; goto do_AvBin32x4;
      do_AvBin32x4: {
         HReg arg1 = iselVecExpr(env, e->Iex.Binop.arg1, IEndianess);
         HReg arg2 = iselVecExpr(env, e->Iex.Binop.arg2, IEndianess);
         HReg dst  = newVRegV(env);
         addInstr(env, PPCInstr_AvBin32x4(op, dst, arg1, arg2));
         return dst;
      }

      case Iop_Shl64x2:    op = Pav_SHL;    goto do_AvBin64x2;
      case Iop_Shr64x2:    op = Pav_SHR;    goto do_AvBin64x2;
      case Iop_Sar64x2:    op = Pav_SAR;    goto do_AvBin64x2;
      case Iop_Rol64x2:    op = Pav_ROTL;   goto do_AvBin64x2;
      case Iop_NarrowBin64to32x4:    op = Pav_PACKUU;  goto do_AvBin64x2;
      case Iop_QNarrowBin64Sto32Sx4: op = Pav_QPACKSS; goto do_AvBin64x2;
      case Iop_QNarrowBin64Uto32Ux4: op = Pav_QPACKUU; goto do_AvBin64x2;
      case Iop_InterleaveHI64x2:  op = Pav_MRGHI;  goto do_AvBin64x2;
      case Iop_InterleaveLO64x2:  op = Pav_MRGLO;  goto do_AvBin64x2;
      case Iop_Add64x2:    op = Pav_ADDU;   goto do_AvBin64x2;
      case Iop_Sub64x2:    op = Pav_SUBU;   goto do_AvBin64x2;
      case Iop_Max64Ux2:   op = Pav_MAXU;   goto do_AvBin64x2;
      case Iop_Max64Sx2:   op = Pav_MAXS;   goto do_AvBin64x2;
      case Iop_Min64Ux2:   op = Pav_MINU;   goto do_AvBin64x2;
      case Iop_Min64Sx2:   op = Pav_MINS;   goto do_AvBin64x2;
      case Iop_CmpEQ64x2:  op = Pav_CMPEQU; goto do_AvBin64x2;
      case Iop_CmpGT64Ux2: op = Pav_CMPGTU; goto do_AvBin64x2;
      case Iop_CmpGT64Sx2: op = Pav_CMPGTS; goto do_AvBin64x2;
      case Iop_PolynomialMulAdd64x2: op = Pav_POLYMULADD; goto do_AvBin64x2;
      do_AvBin64x2: {
         HReg arg1 = iselVecExpr(env, e->Iex.Binop.arg1, IEndianess);
         HReg arg2 = iselVecExpr(env, e->Iex.Binop.arg2, IEndianess);
         HReg dst  = newVRegV(env);
         addInstr(env, PPCInstr_AvBin64x2(op, dst, arg1, arg2));
         return dst;
      }

      case Iop_ShlN8x16: op = Pav_SHL; goto do_AvShift8x16;
      case Iop_SarN8x16: op = Pav_SAR; goto do_AvShift8x16;
      do_AvShift8x16: {
         HReg r_src  = iselVecExpr(env, e->Iex.Binop.arg1, IEndianess);
         HReg dst    = newVRegV(env);
         HReg v_shft = mk_AvDuplicateRI(env, e->Iex.Binop.arg2, IEndianess);
         addInstr(env, PPCInstr_AvBin8x16(op, dst, r_src, v_shft));
         return dst;
      }

      case Iop_ShlN16x8: op = Pav_SHL; goto do_AvShift16x8;
      case Iop_ShrN16x8: op = Pav_SHR; goto do_AvShift16x8;
      case Iop_SarN16x8: op = Pav_SAR; goto do_AvShift16x8;
      do_AvShift16x8: {
         HReg r_src  = iselVecExpr(env, e->Iex.Binop.arg1, IEndianess);
         HReg dst    = newVRegV(env);
         HReg v_shft = mk_AvDuplicateRI(env, e->Iex.Binop.arg2, IEndianess);
         addInstr(env, PPCInstr_AvBin16x8(op, dst, r_src, v_shft));
         return dst;
      }

      case Iop_ShlN32x4: op = Pav_SHL; goto do_AvShift32x4;
      case Iop_ShrN32x4: op = Pav_SHR; goto do_AvShift32x4;
      case Iop_SarN32x4: op = Pav_SAR; goto do_AvShift32x4;
      do_AvShift32x4: {
         HReg r_src  = iselVecExpr(env, e->Iex.Binop.arg1, IEndianess);
         HReg dst    = newVRegV(env);
         HReg v_shft = mk_AvDuplicateRI(env, e->Iex.Binop.arg2, IEndianess);
         addInstr(env, PPCInstr_AvBin32x4(op, dst, r_src, v_shft));
         return dst;
      }

      case Iop_ShlN64x2: op = Pav_SHL; goto do_AvShift64x2;
      case Iop_ShrN64x2: op = Pav_SHR; goto do_AvShift64x2;
      case Iop_SarN64x2: op = Pav_SAR; goto do_AvShift64x2;
      do_AvShift64x2: {
         HReg r_src  = iselVecExpr(env, e->Iex.Binop.arg1, IEndianess);
         HReg dst    = newVRegV(env);
         HReg v_shft = mk_AvDuplicateRI(env, e->Iex.Binop.arg2, IEndianess);
         addInstr(env, PPCInstr_AvBin64x2(op, dst, r_src, v_shft));
         return dst;
      }

      case Iop_ShrV128: op = Pav_SHR; goto do_AvShiftV128;
      case Iop_ShlV128: op = Pav_SHL; goto do_AvShiftV128;
      do_AvShiftV128: {
         HReg dst    = newVRegV(env);
         HReg r_src  = iselVecExpr(env, e->Iex.Binop.arg1, IEndianess);
         HReg v_shft = mk_AvDuplicateRI(env, e->Iex.Binop.arg2, IEndianess);
         
         addInstr(env, PPCInstr_AvBinary(op, dst, r_src, v_shft));
         return dst;
      }

      case Iop_Perm8x16: {
         HReg dst   = newVRegV(env);
         HReg v_src = iselVecExpr(env, e->Iex.Binop.arg1, IEndianess);
         HReg v_ctl = iselVecExpr(env, e->Iex.Binop.arg2, IEndianess);
         addInstr(env, PPCInstr_AvPerm(dst, v_src, v_src, v_ctl));
         return dst;
      }

      case Iop_CipherV128:  op = Pav_CIPHERV128;   goto do_AvCipherV128;
      case Iop_CipherLV128: op = Pav_CIPHERLV128;  goto do_AvCipherV128;
      case Iop_NCipherV128: op = Pav_NCIPHERV128;  goto do_AvCipherV128;
      case Iop_NCipherLV128:op = Pav_NCIPHERLV128; goto do_AvCipherV128;
      do_AvCipherV128: {
         HReg arg1 = iselVecExpr(env, e->Iex.Binop.arg1, IEndianess);
         HReg arg2 = iselVecExpr(env, e->Iex.Binop.arg2, IEndianess);
         HReg dst  = newVRegV(env);
         addInstr(env, PPCInstr_AvCipherV128Binary(op, dst, arg1, arg2));
         return dst;
      }

      case Iop_SHA256:op = Pav_SHA256; goto do_AvHashV128;
      case Iop_SHA512:op = Pav_SHA512; goto do_AvHashV128;
      do_AvHashV128: {
         HReg arg1 = iselVecExpr(env, e->Iex.Binop.arg1, IEndianess);
         HReg dst  = newVRegV(env);
         PPCRI* s_field = iselWordExpr_RI(env, e->Iex.Binop.arg2, IEndianess);
         addInstr(env, PPCInstr_AvHashV128Binary(op, dst, arg1, s_field));
         return dst;
      }
      default:
         break;
      } 
   } 

   if (e->tag == Iex_Triop) {
      IRTriop *triop = e->Iex.Triop.details;
      switch (triop->op) {
      case Iop_BCDAdd:op = Pav_BCDAdd; goto do_AvBCDV128;
      case Iop_BCDSub:op = Pav_BCDSub; goto do_AvBCDV128;
      do_AvBCDV128: {
         HReg arg1 = iselVecExpr(env, triop->arg1, IEndianess);
         HReg arg2 = iselVecExpr(env, triop->arg2, IEndianess);
         HReg dst  = newVRegV(env);
         PPCRI* ps = iselWordExpr_RI(env, triop->arg3, IEndianess);
         addInstr(env, PPCInstr_AvBCDV128Trinary(op, dst, arg1, arg2, ps));
         return dst;
      }

      case Iop_Add32Fx4: fpop = Pavfp_ADDF; goto do_32Fx4_with_rm;
      case Iop_Sub32Fx4: fpop = Pavfp_SUBF; goto do_32Fx4_with_rm;
      case Iop_Mul32Fx4: fpop = Pavfp_MULF; goto do_32Fx4_with_rm;
      do_32Fx4_with_rm:
      {
         HReg argL = iselVecExpr(env, triop->arg2, IEndianess);
         HReg argR = iselVecExpr(env, triop->arg3, IEndianess);
         HReg dst  = newVRegV(env);
         set_FPU_rounding_mode(env, triop->arg1, IEndianess);
         addInstr(env, PPCInstr_AvBin32Fx4(fpop, dst, argL, argR));
         return dst;
      }

      default:
         break;
      } 
   } 


   if (e->tag == Iex_Const ) {
      vassert(e->Iex.Const.con->tag == Ico_V128);
      if (e->Iex.Const.con->Ico.V128 == 0x0000) {
         return generate_zeroes_V128(env);
      } 
      else if (e->Iex.Const.con->Ico.V128 == 0xffff) {
         return generate_ones_V128(env);
      }
   }

   vex_printf("iselVecExpr(ppc) (subarch = %s): can't reduce\n",
              LibVEX_ppVexHwCaps(mode64 ? VexArchPPC64 : VexArchPPC32,
                                 env->hwcaps));
   ppIRExpr(e);
   vpanic("iselVecExpr_wrk(ppc)");
}



static void iselStmt ( ISelEnv* env, IRStmt* stmt, IREndness IEndianess )
{
   Bool mode64 = env->mode64;
   if (vex_traceflags & VEX_TRACE_VCODE) {
      vex_printf("\n -- ");
      ppIRStmt(stmt);
      vex_printf("\n");
   }

   switch (stmt->tag) {

   
   case Ist_Store: {
      IRType    tya   = typeOfIRExpr(env->type_env, stmt->Ist.Store.addr);
      IRType    tyd   = typeOfIRExpr(env->type_env, stmt->Ist.Store.data);
      IREndness end   = stmt->Ist.Store.end;

      if (end != IEndianess)
         goto stmt_fail;
      if (!mode64 && (tya != Ity_I32))
         goto stmt_fail;
      if (mode64 && (tya != Ity_I64))
         goto stmt_fail;

      if (tyd == Ity_I8 || tyd == Ity_I16 || tyd == Ity_I32 ||
          (mode64 && (tyd == Ity_I64))) {
         PPCAMode* am_addr
            = iselWordExpr_AMode(env, stmt->Ist.Store.addr, tyd,
                                 IEndianess);
         HReg r_src = iselWordExpr_R(env, stmt->Ist.Store.data, IEndianess);
         addInstr(env, PPCInstr_Store( toUChar(sizeofIRType(tyd)), 
                                       am_addr, r_src, mode64 ));
         return;
      }
      if (tyd == Ity_F64) {
         PPCAMode* am_addr
            = iselWordExpr_AMode(env, stmt->Ist.Store.addr, tyd,
                                 IEndianess);
         HReg fr_src = iselDblExpr(env, stmt->Ist.Store.data, IEndianess);
         addInstr(env,
                  PPCInstr_FpLdSt(False, 8, fr_src, am_addr));
         return;
      }
      if (tyd == Ity_F32) {
         PPCAMode* am_addr
            = iselWordExpr_AMode(env, stmt->Ist.Store.addr, tyd,
                                 IEndianess);
         HReg fr_src = iselFltExpr(env, stmt->Ist.Store.data, IEndianess);
         addInstr(env,
                  PPCInstr_FpLdSt(False, 4, fr_src, am_addr));
         return;
      }
      if (tyd == Ity_D64) {
         PPCAMode* am_addr
            = iselWordExpr_AMode(env, stmt->Ist.Store.addr, tyd,
                                 IEndianess);
         HReg fr_src = iselDfp64Expr(env, stmt->Ist.Store.data, IEndianess);
         addInstr(env,
                  PPCInstr_FpLdSt(False, 8, fr_src, am_addr));
         return;
      }
      if (tyd == Ity_D32) {
         PPCAMode* am_addr
            = iselWordExpr_AMode(env, stmt->Ist.Store.addr, tyd,
                                 IEndianess);
         HReg fr_src = iselDfp32Expr(env, stmt->Ist.Store.data, IEndianess);
         addInstr(env,
                  PPCInstr_FpLdSt(False, 4, fr_src, am_addr));
         return;
      }
      if (tyd == Ity_V128) {
         PPCAMode* am_addr
            = iselWordExpr_AMode(env, stmt->Ist.Store.addr, tyd,
                                 IEndianess);
         HReg v_src = iselVecExpr(env, stmt->Ist.Store.data, IEndianess);
         addInstr(env,
                  PPCInstr_AvLdSt(False, 16, v_src, am_addr));
         return;
      }
      if (tyd == Ity_I64 && !mode64) {
         HReg rHi32, rLo32;
         HReg r_addr = iselWordExpr_R(env, stmt->Ist.Store.addr, IEndianess);
         iselInt64Expr( &rHi32, &rLo32, env, stmt->Ist.Store.data,
                        IEndianess );
         addInstr(env, PPCInstr_Store( 4,
                                       PPCAMode_IR( 0, r_addr ), 
                                       rHi32,
                                       False) );
         addInstr(env, PPCInstr_Store( 4, 
                                       PPCAMode_IR( 4, r_addr ), 
                                       rLo32,
                                       False) );
         return;
      }
      break;
   }

   
   case Ist_Put: {
      IRType ty = typeOfIRExpr(env->type_env, stmt->Ist.Put.data);
      if (ty == Ity_I8  || ty == Ity_I16 ||
          ty == Ity_I32 || ((ty == Ity_I64) && mode64)) {
         HReg r_src = iselWordExpr_R(env, stmt->Ist.Put.data, IEndianess);
         PPCAMode* am_addr = PPCAMode_IR( stmt->Ist.Put.offset,
                                          GuestStatePtr(mode64) );
         addInstr(env, PPCInstr_Store( toUChar(sizeofIRType(ty)), 
                                       am_addr, r_src, mode64 ));
         return;
      }
      if (!mode64 && ty == Ity_I64) {
         HReg rHi, rLo;
         PPCAMode* am_addr  = PPCAMode_IR( stmt->Ist.Put.offset,
                                           GuestStatePtr(mode64) );
         PPCAMode* am_addr4 = advance4(env, am_addr);
         iselInt64Expr(&rHi,&rLo, env, stmt->Ist.Put.data, IEndianess);
         addInstr(env, PPCInstr_Store( 4, am_addr,  rHi, mode64 ));
         addInstr(env, PPCInstr_Store( 4, am_addr4, rLo, mode64 ));
         return;
     }
     if (ty == Ity_V128) {
         HReg v_src = iselVecExpr(env, stmt->Ist.Put.data, IEndianess);
         PPCAMode* am_addr  = PPCAMode_IR( stmt->Ist.Put.offset,
                                           GuestStatePtr(mode64) );
         addInstr(env,
                  PPCInstr_AvLdSt(False, 16, v_src, am_addr));
         return;
      }
      if (ty == Ity_F64) {
         HReg fr_src = iselDblExpr(env, stmt->Ist.Put.data, IEndianess);
         PPCAMode* am_addr = PPCAMode_IR( stmt->Ist.Put.offset,
                                          GuestStatePtr(mode64) );
         addInstr(env, PPCInstr_FpLdSt( False, 8,
                                        fr_src, am_addr ));
         return;
      }
      if (ty == Ity_D32) {
         
         HReg fr_src = iselDfp32Expr( env, stmt->Ist.Put.data, IEndianess );
         PPCAMode* am_addr = PPCAMode_IR( stmt->Ist.Put.offset,
                                          GuestStatePtr(mode64) );
         addInstr( env, PPCInstr_FpLdSt( False, 8,
                                         fr_src, am_addr ) );
         return;
      }
      if (ty == Ity_D64) {
         HReg fr_src = iselDfp64Expr( env, stmt->Ist.Put.data, IEndianess );
         PPCAMode* am_addr = PPCAMode_IR( stmt->Ist.Put.offset,
                                          GuestStatePtr(mode64) );
         addInstr( env, PPCInstr_FpLdSt( False, 8, fr_src, am_addr ) );
         return;
      }
      break;
   }
      
   
   case Ist_PutI: {
      IRPutI *puti = stmt->Ist.PutI.details;

      PPCAMode* dst_am
         = genGuestArrayOffset(
              env, puti->descr, 
              puti->ix, puti->bias,
              IEndianess );
      IRType ty = typeOfIRExpr(env->type_env, puti->data);
      if (mode64 && ty == Ity_I64) {
         HReg r_src = iselWordExpr_R(env, puti->data, IEndianess);
         addInstr(env, PPCInstr_Store( toUChar(8),
                                       dst_am, r_src, mode64 ));
         return;
      }
      if ((!mode64) && ty == Ity_I32) {
         HReg r_src = iselWordExpr_R(env, puti->data, IEndianess);
         addInstr(env, PPCInstr_Store( toUChar(4),
                                       dst_am, r_src, mode64 ));
         return;
      }
      break;
   }

   
   case Ist_WrTmp: {
      IRTemp tmp = stmt->Ist.WrTmp.tmp;
      IRType ty = typeOfIRTemp(env->type_env, tmp);
      if (ty == Ity_I8  || ty == Ity_I16 ||
          ty == Ity_I32 || ((ty == Ity_I64) && mode64)) {
         HReg r_dst = lookupIRTemp(env, tmp);
         HReg r_src = iselWordExpr_R(env, stmt->Ist.WrTmp.data, IEndianess);
         addInstr(env, mk_iMOVds_RR( r_dst, r_src ));
         return;
      }
      if (!mode64 && ty == Ity_I64) {
         HReg r_srcHi, r_srcLo, r_dstHi, r_dstLo;

         iselInt64Expr(&r_srcHi,&r_srcLo, env, stmt->Ist.WrTmp.data,
                       IEndianess);
         lookupIRTempPair( &r_dstHi, &r_dstLo, env, tmp);
         addInstr(env, mk_iMOVds_RR(r_dstHi, r_srcHi) );
         addInstr(env, mk_iMOVds_RR(r_dstLo, r_srcLo) );
         return;
      }
      if (mode64 && ty == Ity_I128) {
         HReg r_srcHi, r_srcLo, r_dstHi, r_dstLo;
         iselInt128Expr(&r_srcHi,&r_srcLo, env, stmt->Ist.WrTmp.data,
                        IEndianess);
         lookupIRTempPair( &r_dstHi, &r_dstLo, env, tmp);
         addInstr(env, mk_iMOVds_RR(r_dstHi, r_srcHi) );
         addInstr(env, mk_iMOVds_RR(r_dstLo, r_srcLo) );
         return;
      }
      if (!mode64 && ty == Ity_I128) {
         HReg r_srcHi, r_srcMedHi, r_srcMedLo, r_srcLo;
         HReg r_dstHi, r_dstMedHi, r_dstMedLo, r_dstLo;

         iselInt128Expr_to_32x4(&r_srcHi, &r_srcMedHi,
                                &r_srcMedLo, &r_srcLo,
                                env, stmt->Ist.WrTmp.data, IEndianess);

         lookupIRTempQuad( &r_dstHi, &r_dstMedHi, &r_dstMedLo,
                           &r_dstLo, env, tmp);

         addInstr(env, mk_iMOVds_RR(r_dstHi,    r_srcHi) );
         addInstr(env, mk_iMOVds_RR(r_dstMedHi, r_srcMedHi) );
         addInstr(env, mk_iMOVds_RR(r_dstMedLo, r_srcMedLo) );
         addInstr(env, mk_iMOVds_RR(r_dstLo,    r_srcLo) );
         return;
      }
      if (ty == Ity_I1) {
         PPCCondCode cond = iselCondCode(env, stmt->Ist.WrTmp.data,
                                         IEndianess);
         HReg r_dst = lookupIRTemp(env, tmp);
         addInstr(env, PPCInstr_Set(cond, r_dst));
         return;
      }
      if (ty == Ity_F64) {
         HReg fr_dst = lookupIRTemp(env, tmp);
         HReg fr_src = iselDblExpr(env, stmt->Ist.WrTmp.data, IEndianess);
         addInstr(env, PPCInstr_FpUnary(Pfp_MOV, fr_dst, fr_src));
         return;
      }
      if (ty == Ity_F32) {
         HReg fr_dst = lookupIRTemp(env, tmp);
         HReg fr_src = iselFltExpr(env, stmt->Ist.WrTmp.data, IEndianess);
         addInstr(env, PPCInstr_FpUnary(Pfp_MOV, fr_dst, fr_src));
         return;
      }
      if (ty == Ity_D32) {
         HReg fr_dst = lookupIRTemp(env, tmp);
         HReg fr_src = iselDfp32Expr(env, stmt->Ist.WrTmp.data, IEndianess);
         addInstr(env, PPCInstr_Dfp64Unary(Pfp_MOV, fr_dst, fr_src));
         return;
      }
      if (ty == Ity_V128) {
         HReg v_dst = lookupIRTemp(env, tmp);
         HReg v_src = iselVecExpr(env, stmt->Ist.WrTmp.data, IEndianess);
         addInstr(env, PPCInstr_AvUnary(Pav_MOV, v_dst, v_src));
         return;
      }
      if (ty == Ity_D64) {
         HReg fr_dst = lookupIRTemp( env, tmp );
         HReg fr_src = iselDfp64Expr( env, stmt->Ist.WrTmp.data, IEndianess );
         addInstr( env, PPCInstr_Dfp64Unary( Pfp_MOV, fr_dst, fr_src ) );
         return;
      }
      if (ty == Ity_D128) {
         HReg fr_srcHi, fr_srcLo, fr_dstHi, fr_dstLo;
	 
         lookupIRTempPair( &fr_dstHi, &fr_dstLo, env, tmp );
         iselDfp128Expr( &fr_srcHi, &fr_srcLo, env, stmt->Ist.WrTmp.data,
                         IEndianess );
         addInstr( env, PPCInstr_Dfp64Unary( Pfp_MOV, fr_dstHi, fr_srcHi ) );
         addInstr( env, PPCInstr_Dfp64Unary( Pfp_MOV, fr_dstLo, fr_srcLo ) );
         return;
      }
      break;
   }

   
   case Ist_LLSC: {
      IRTemp res    = stmt->Ist.LLSC.result;
      IRType tyRes  = typeOfIRTemp(env->type_env, res);
      IRType tyAddr = typeOfIRExpr(env->type_env, stmt->Ist.LLSC.addr);

      if (stmt->Ist.LLSC.end != IEndianess)
         goto stmt_fail;
      if (!mode64 && (tyAddr != Ity_I32))
         goto stmt_fail;
      if (mode64 && (tyAddr != Ity_I64))
         goto stmt_fail;

      if (stmt->Ist.LLSC.storedata == NULL) {
         
         HReg r_addr = iselWordExpr_R( env, stmt->Ist.LLSC.addr, IEndianess );
         HReg r_dst  = lookupIRTemp(env, res);
         if (tyRes == Ity_I8) {
            addInstr(env, PPCInstr_LoadL( 1, r_dst, r_addr, mode64 ));
            return;
         }
         if (tyRes == Ity_I16) {
            addInstr(env, PPCInstr_LoadL( 2, r_dst, r_addr, mode64 ));
            return;
         }
         if (tyRes == Ity_I32) {
            addInstr(env, PPCInstr_LoadL( 4, r_dst, r_addr, mode64 ));
            return;
         }
         if (tyRes == Ity_I64 && mode64) {
            addInstr(env, PPCInstr_LoadL( 8, r_dst, r_addr, mode64 ));
            return;
         }
         ;
      } else {
         
         HReg   r_res  = lookupIRTemp(env, res); 
         HReg   r_a    = iselWordExpr_R(env, stmt->Ist.LLSC.addr, IEndianess);
         HReg   r_src  = iselWordExpr_R(env, stmt->Ist.LLSC.storedata,
                                        IEndianess);
         HReg   r_tmp  = newVRegI(env);
         IRType tyData = typeOfIRExpr(env->type_env,
                                      stmt->Ist.LLSC.storedata);
         vassert(tyRes == Ity_I1);
         if (tyData == Ity_I8 || tyData == Ity_I16 || tyData == Ity_I32 ||
            (tyData == Ity_I64 && mode64)) {
            int size = 0;

            if (tyData == Ity_I64)
               size = 8;
            else if (tyData == Ity_I32)
               size = 4;
            else if (tyData == Ity_I16)
               size = 2;
            else if (tyData == Ity_I8)
               size = 1;

            addInstr(env, PPCInstr_StoreC( size,
                                           r_a, r_src, mode64 ));
            addInstr(env, PPCInstr_MfCR( r_tmp ));
            addInstr(env, PPCInstr_Shft(
                             Pshft_SHR,
                             env->mode64 ? False : True
                                ,
                             r_tmp, r_tmp, 
                             PPCRH_Imm(False, 29)));
            addInstr(env, PPCInstr_Alu(
                             Palu_AND, 
                             r_res, r_tmp, 
                             PPCRH_Imm(False, 1)));
            return;
         }
         
      }
      goto stmt_fail;
      
   }

   
   case Ist_Dirty: {
      IRDirty* d = stmt->Ist.Dirty.details;

      
      IRType retty = Ity_INVALID;
      if (d->tmp != IRTemp_INVALID)
         retty = typeOfIRTemp(env->type_env, d->tmp);

      Bool retty_ok = False;
      switch (retty) {
         case Ity_INVALID: 
         case Ity_V128:
         case Ity_I64: case Ity_I32: case Ity_I16: case Ity_I8:
            retty_ok = True; break;
         default:
            break;
      }
      if (!retty_ok)
         break; 

      UInt   addToSp = 0;
      RetLoc rloc    = mk_RetLoc_INVALID();
      doHelperCall( &addToSp, &rloc, env, d->guard, d->cee, retty, d->args,
                    IEndianess );
      vassert(is_sane_RetLoc(rloc));

      
      switch (retty) {
         case Ity_INVALID: {
            
            vassert(d->tmp == IRTemp_INVALID);
            vassert(rloc.pri == RLPri_None);
            vassert(addToSp == 0);
            return;
         }
         case Ity_I32: case Ity_I16: case Ity_I8: {
            HReg r_dst = lookupIRTemp(env, d->tmp);
            addInstr(env, mk_iMOVds_RR(r_dst, hregPPC_GPR3(mode64)));
            vassert(rloc.pri == RLPri_Int);
            vassert(addToSp == 0);
            return;
         }
         case Ity_I64:
            if (mode64) {
               HReg r_dst = lookupIRTemp(env, d->tmp);
               addInstr(env, mk_iMOVds_RR(r_dst, hregPPC_GPR3(mode64)));
               vassert(rloc.pri == RLPri_Int);
               vassert(addToSp == 0);
            } else {
               HReg r_dstHi = INVALID_HREG;
               HReg r_dstLo = INVALID_HREG;
               lookupIRTempPair( &r_dstHi, &r_dstLo, env, d->tmp);
               addInstr(env, mk_iMOVds_RR(r_dstHi, hregPPC_GPR3(mode64)));
               addInstr(env, mk_iMOVds_RR(r_dstLo, hregPPC_GPR4(mode64)));
               vassert(rloc.pri == RLPri_2Int);
               vassert(addToSp == 0);
            }
            return;
         case Ity_V128: {
            vassert(rloc.pri == RLPri_V128SpRel);
            vassert(addToSp >= 16);
            HReg      dst = lookupIRTemp(env, d->tmp);
            PPCAMode* am  = PPCAMode_IR(rloc.spOff, StackFramePtr(mode64));
            addInstr(env, PPCInstr_AvLdSt( True, 16, dst, am ));
            add_to_sp(env, addToSp);
            return;
         }
         default:
            
            vassert(0);
      }
   }

   
   case Ist_MBE:
      switch (stmt->Ist.MBE.event) {
         case Imbe_Fence:
            addInstr(env, PPCInstr_MFence());
            return;
         default:
            break;
      }
      break;

   
   
   case Ist_IMark:
       return;

   
   case Ist_AbiHint:
       return;

   
   
   case Ist_NoOp:
       return;

   
   case Ist_Exit: {
      IRConst* dst = stmt->Ist.Exit.dst;
      if (!mode64 && dst->tag != Ico_U32)
         vpanic("iselStmt(ppc): Ist_Exit: dst is not a 32-bit value");
      if (mode64 && dst->tag != Ico_U64)
         vpanic("iselStmt(ppc64): Ist_Exit: dst is not a 64-bit value");

      PPCCondCode cc    = iselCondCode(env, stmt->Ist.Exit.guard, IEndianess);
      PPCAMode*   amCIA = PPCAMode_IR(stmt->Ist.Exit.offsIP,
                                      hregPPC_GPR31(mode64));

      
      if (stmt->Ist.Exit.jk == Ijk_Boring
          || stmt->Ist.Exit.jk == Ijk_Call
          ) {
         if (env->chainingAllowed) {
            
            Bool toFastEP
               = mode64
               ? (((Addr64)stmt->Ist.Exit.dst->Ico.U64) > (Addr64)env->max_ga)
               : (((Addr32)stmt->Ist.Exit.dst->Ico.U32) > (Addr32)env->max_ga);
            if (0) vex_printf("%s", toFastEP ? "Y" : ",");
            addInstr(env, PPCInstr_XDirect(
                             mode64 ? (Addr64)stmt->Ist.Exit.dst->Ico.U64
                                    : (Addr64)stmt->Ist.Exit.dst->Ico.U32,
                             amCIA, cc, toFastEP));
         } else {
            
            HReg r = iselWordExpr_R(env, IRExpr_Const(stmt->Ist.Exit.dst),
                                    IEndianess);
            addInstr(env, PPCInstr_XAssisted(r, amCIA, cc, Ijk_Boring));
         }
         return;
      }

      
      switch (stmt->Ist.Exit.jk) {
         
         case Ijk_ClientReq:
         case Ijk_EmFail:
         case Ijk_EmWarn:
         case Ijk_NoDecode:
         case Ijk_NoRedir:
         case Ijk_SigBUS:
         case Ijk_SigTRAP:
         case Ijk_Sys_syscall:
         case Ijk_InvalICache:
         {
            HReg r = iselWordExpr_R(env, IRExpr_Const(stmt->Ist.Exit.dst),
                                    IEndianess);
            addInstr(env, PPCInstr_XAssisted(r, amCIA, cc,
                                             stmt->Ist.Exit.jk));
            return;
         }
         default:
            break;
      }

      
      goto stmt_fail;
   }

   default: break;
   }
  stmt_fail:
   ppIRStmt(stmt);
   vpanic("iselStmt(ppc)");
}
 


static void iselNext ( ISelEnv* env,
                       IRExpr* next, IRJumpKind jk, Int offsIP,
                       IREndness IEndianess)
{
   if (vex_traceflags & VEX_TRACE_VCODE) {
      vex_printf( "\n-- PUT(%d) = ", offsIP);
      ppIRExpr( next );
      vex_printf( "; exit-");
      ppIRJumpKind(jk);
      vex_printf( "\n");
   }

   PPCCondCode always = mk_PPCCondCode( Pct_ALWAYS, Pcf_NONE );

   
   if (next->tag == Iex_Const) {
      IRConst* cdst = next->Iex.Const.con;
      vassert(cdst->tag == (env->mode64 ? Ico_U64 :Ico_U32));
      if (jk == Ijk_Boring || jk == Ijk_Call) {
         
         PPCAMode* amCIA = PPCAMode_IR(offsIP, hregPPC_GPR31(env->mode64));
         if (env->chainingAllowed) {
            
            Bool toFastEP
               = env->mode64
               ? (((Addr64)cdst->Ico.U64) > (Addr64)env->max_ga)
               : (((Addr32)cdst->Ico.U32) > (Addr32)env->max_ga);
            if (0) vex_printf("%s", toFastEP ? "X" : ".");
            addInstr(env, PPCInstr_XDirect(
                             env->mode64 ? (Addr64)cdst->Ico.U64
                                         : (Addr64)cdst->Ico.U32,
                             amCIA, always, toFastEP));
         } else {
            
            HReg r = iselWordExpr_R(env, next, IEndianess);
            addInstr(env, PPCInstr_XAssisted(r, amCIA, always,
                                             Ijk_Boring));
         }
         return;
      }
   }

   
   switch (jk) {
      case Ijk_Boring: case Ijk_Ret: case Ijk_Call: {
         HReg       r     = iselWordExpr_R(env, next, IEndianess);
         PPCAMode*  amCIA = PPCAMode_IR(offsIP, hregPPC_GPR31(env->mode64));
         if (env->chainingAllowed) {
            addInstr(env, PPCInstr_XIndir(r, amCIA, always));
         } else {
            addInstr(env, PPCInstr_XAssisted(r, amCIA, always,
                                             Ijk_Boring));
         }
         return;
      }
      default:
         break;
   }

   
   switch (jk) {
      
      case Ijk_ClientReq:
      case Ijk_EmFail:
      case Ijk_EmWarn:
      case Ijk_NoDecode:
      case Ijk_NoRedir:
      case Ijk_SigBUS:
      case Ijk_SigTRAP:
      case Ijk_Sys_syscall:
      case Ijk_InvalICache:
      {
         HReg      r     = iselWordExpr_R(env, next, IEndianess);
         PPCAMode* amCIA = PPCAMode_IR(offsIP, hregPPC_GPR31(env->mode64));
         addInstr(env, PPCInstr_XAssisted(r, amCIA, always, jk));
         return;
      }
      default:
         break;
   }

   vex_printf( "\n-- PUT(%d) = ", offsIP);
   ppIRExpr( next );
   vex_printf( "; exit-");
   ppIRJumpKind(jk);
   vex_printf( "\n");
   vassert(0); 
}



HInstrArray* iselSB_PPC ( const IRSB* bb,
                          VexArch      arch_host,
                          const VexArchInfo* archinfo_host,
                          const VexAbiInfo*  vbi,
                          Int offs_Host_EvC_Counter,
                          Int offs_Host_EvC_FailAddr,
                          Bool chainingAllowed,
                          Bool addProfInc,
                          Addr max_ga)

{
   Int       i, j;
   HReg      hregLo, hregMedLo, hregMedHi, hregHi;
   ISelEnv*  env;
   UInt      hwcaps_host = archinfo_host->hwcaps;
   Bool      mode64 = False;
   UInt      mask32, mask64;
   PPCAMode *amCounter, *amFailAddr;
   IREndness IEndianess;

   vassert(arch_host == VexArchPPC32 || arch_host == VexArchPPC64);
   mode64 = arch_host == VexArchPPC64;

   
   mask32 = VEX_HWCAPS_PPC32_F | VEX_HWCAPS_PPC32_V
            | VEX_HWCAPS_PPC32_FX | VEX_HWCAPS_PPC32_GX | VEX_HWCAPS_PPC32_VX
            | VEX_HWCAPS_PPC32_DFP | VEX_HWCAPS_PPC32_ISA2_07;


   mask64 = VEX_HWCAPS_PPC64_V | VEX_HWCAPS_PPC64_FX
            | VEX_HWCAPS_PPC64_GX | VEX_HWCAPS_PPC64_VX | VEX_HWCAPS_PPC64_DFP
            | VEX_HWCAPS_PPC64_ISA2_07;

   if (mode64) {
      vassert((hwcaps_host & mask32) == 0);
   } else {
      vassert((hwcaps_host & mask64) == 0);
   }

   
   vassert((archinfo_host->endness == VexEndnessBE) ||
	   (archinfo_host->endness == VexEndnessLE));

   if (archinfo_host->endness == VexEndnessBE)
     IEndianess = Iend_BE;
   else
     IEndianess = Iend_LE;

   
   env = LibVEX_Alloc_inline(sizeof(ISelEnv));
   env->vreg_ctr = 0;

   
   env->mode64 = mode64;

   
   env->code = newHInstrArray();

   
   env->type_env = bb->tyenv;

   env->n_vregmap = bb->tyenv->types_used;
   env->vregmapLo    = LibVEX_Alloc_inline(env->n_vregmap * sizeof(HReg));
   env->vregmapMedLo = LibVEX_Alloc_inline(env->n_vregmap * sizeof(HReg));
   if (mode64) {
      env->vregmapMedHi = NULL;
      env->vregmapHi    = NULL;
   } else {
      env->vregmapMedHi = LibVEX_Alloc_inline(env->n_vregmap * sizeof(HReg));
      env->vregmapHi    = LibVEX_Alloc_inline(env->n_vregmap * sizeof(HReg));
   }

   
   env->chainingAllowed = chainingAllowed;
   env->max_ga          = max_ga;
   env->hwcaps          = hwcaps_host;
   env->previous_rm     = NULL;
   env->vbi             = vbi;

   j = 0;
   for (i = 0; i < env->n_vregmap; i++) {
      hregLo = hregMedLo = hregMedHi = hregHi = INVALID_HREG;
      switch (bb->tyenv->types[i]) {
      case Ity_I1:
      case Ity_I8:
      case Ity_I16:
      case Ity_I32:
         if (mode64) {
            hregLo = mkHReg(True, HRcInt64, 0, j++);
         } else {
            hregLo = mkHReg(True, HRcInt32, 0, j++);
         }
         break;
      case Ity_I64:  
         if (mode64) {
            hregLo    = mkHReg(True, HRcInt64, 0, j++);
         } else {
            hregLo    = mkHReg(True, HRcInt32, 0, j++);
            hregMedLo = mkHReg(True, HRcInt32, 0, j++);
         }
         break;
      case Ity_I128:
         if (mode64) {
            hregLo    = mkHReg(True, HRcInt64, 0, j++);
            hregMedLo = mkHReg(True, HRcInt64, 0, j++);
         } else {
            hregLo    = mkHReg(True, HRcInt32, 0, j++);
            hregMedLo = mkHReg(True, HRcInt32, 0, j++);
            hregMedHi = mkHReg(True, HRcInt32, 0, j++);
            hregHi    = mkHReg(True, HRcInt32, 0, j++);
         }
         break;
      case Ity_F32:
      case Ity_F64:
         hregLo = mkHReg(True, HRcFlt64, 0, j++);
         break;
      case Ity_V128:
         hregLo = mkHReg(True, HRcVec128, 0, j++);
         break;
      case Ity_D32:
      case Ity_D64:
         hregLo = mkHReg(True, HRcFlt64, 0, j++);
         break;
      case Ity_D128:
         hregLo    = mkHReg(True, HRcFlt64, 0, j++);
         hregMedLo = mkHReg(True, HRcFlt64, 0, j++);
         break;
      default:
         ppIRType(bb->tyenv->types[i]);
         vpanic("iselBB(ppc): IRTemp type");
      }
      env->vregmapLo[i]    = hregLo;
      env->vregmapMedLo[i] = hregMedLo;
      if (!mode64) {
         env->vregmapMedHi[i] = hregMedHi;
         env->vregmapHi[i]    = hregHi;
      }
   }
   env->vreg_ctr = j;

   
   amCounter  = PPCAMode_IR(offs_Host_EvC_Counter, hregPPC_GPR31(mode64));
   amFailAddr = PPCAMode_IR(offs_Host_EvC_FailAddr, hregPPC_GPR31(mode64));
   addInstr(env, PPCInstr_EvCheck(amCounter, amFailAddr));

   if (addProfInc) {
      addInstr(env, PPCInstr_ProfInc());
   }

   
   for (i = 0; i < bb->stmts_used; i++)
      iselStmt(env, bb->stmts[i], IEndianess);

   iselNext(env, bb->next, bb->jumpkind, bb->offsIP, IEndianess);

   
   env->code->n_vregs = env->vreg_ctr;
   return env->code;
}


