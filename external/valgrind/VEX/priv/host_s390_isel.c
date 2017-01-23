

/*
   This file is part of Valgrind, a dynamic binary instrumentation
   framework.

   Copyright IBM Corp. 2010-2013
   Copyright (C) 2012-2013  Florian Krohm   (britzel@acm.org)

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
#include "libvex_ir.h"
#include "libvex.h"
#include "libvex_s390x_common.h"

#include "main_util.h"
#include "main_globals.h"
#include "guest_s390_defs.h"   
#include "host_generic_regs.h"
#include "host_s390_defs.h"



enum {
   GUEST_IA,
   GUEST_CC_OP,
   GUEST_CC_DEP1,
   GUEST_CC_DEP2,
   GUEST_CC_NDEP,
   GUEST_SYSNO,
   GUEST_COUNTER,
   GUEST_UNKNOWN    
};

#define NUM_TRACKED_REGS GUEST_UNKNOWN


typedef struct {
   IRTypeEnv   *type_env;

   HInstrArray *code;
   HReg        *vregmap;
   HReg        *vregmapHI;
   UInt         n_vregmap;
   UInt         vreg_ctr;
   UInt         hwcaps;

   IRExpr      *previous_bfp_rounding_mode;
   IRExpr      *previous_dfp_rounding_mode;

   ULong        old_value[NUM_TRACKED_REGS];

   
   Addr64       max_ga;
   Bool         chaining_allowed;

   Bool         old_value_valid[NUM_TRACKED_REGS];
} ISelEnv;


static HReg          s390_isel_int_expr(ISelEnv *, IRExpr *);
static s390_amode   *s390_isel_amode(ISelEnv *, IRExpr *);
static s390_amode   *s390_isel_amode_b12_b20(ISelEnv *, IRExpr *);
static s390_cc_t     s390_isel_cc(ISelEnv *, IRExpr *);
static s390_opnd_RMI s390_isel_int_expr_RMI(ISelEnv *, IRExpr *);
static void          s390_isel_int128_expr(HReg *, HReg *, ISelEnv *, IRExpr *);
static HReg          s390_isel_float_expr(ISelEnv *, IRExpr *);
static void          s390_isel_float128_expr(HReg *, HReg *, ISelEnv *, IRExpr *);
static HReg          s390_isel_dfp_expr(ISelEnv *, IRExpr *);
static void          s390_isel_dfp128_expr(HReg *, HReg *, ISelEnv *, IRExpr *);


static Int
get_guest_reg(Int offset)
{
   switch (offset) {
   case S390X_GUEST_OFFSET(guest_IA):        return GUEST_IA;
   case S390X_GUEST_OFFSET(guest_CC_OP):     return GUEST_CC_OP;
   case S390X_GUEST_OFFSET(guest_CC_DEP1):   return GUEST_CC_DEP1;
   case S390X_GUEST_OFFSET(guest_CC_DEP2):   return GUEST_CC_DEP2;
   case S390X_GUEST_OFFSET(guest_CC_NDEP):   return GUEST_CC_NDEP;
   case S390X_GUEST_OFFSET(guest_SYSNO):     return GUEST_SYSNO;
   case S390X_GUEST_OFFSET(guest_counter):   return GUEST_COUNTER;

   case S390X_GUEST_OFFSET(guest_IA)+1      ... S390X_GUEST_OFFSET(guest_IA)+7:
   case S390X_GUEST_OFFSET(guest_CC_OP)+1   ... S390X_GUEST_OFFSET(guest_CC_OP)+7:
   case S390X_GUEST_OFFSET(guest_CC_DEP1)+1 ... S390X_GUEST_OFFSET(guest_CC_DEP1)+7:
   case S390X_GUEST_OFFSET(guest_CC_DEP2)+1 ... S390X_GUEST_OFFSET(guest_CC_DEP2)+7:
   case S390X_GUEST_OFFSET(guest_CC_NDEP)+1 ... S390X_GUEST_OFFSET(guest_CC_NDEP)+7:
   case S390X_GUEST_OFFSET(guest_SYSNO)+1   ... S390X_GUEST_OFFSET(guest_SYSNO)+7:
      
   case S390X_GUEST_OFFSET(guest_counter)+1 ... S390X_GUEST_OFFSET(guest_counter)+3:
   case S390X_GUEST_OFFSET(guest_counter)+5 ... S390X_GUEST_OFFSET(guest_counter)+7:
      vpanic("partial update of this guest state register is not allowed");
      break;

   default: break;
   }

   return GUEST_UNKNOWN;
}

static void
addInstr(ISelEnv *env, s390_insn *insn)
{
   addHInstr(env->code, insn);

   if (vex_traceflags & VEX_TRACE_VCODE) {
      vex_printf("%s\n", s390_insn_as_string(insn));
   }
}


static __inline__ IRExpr *
mkU64(ULong value)
{
   return IRExpr_Const(IRConst_U64(value));
}



static HReg
lookupIRTemp(ISelEnv *env, IRTemp tmp)
{
   vassert(tmp < env->n_vregmap);
   vassert(! hregIsInvalid(env->vregmap[tmp]));

   return env->vregmap[tmp];
}


static void
lookupIRTemp128(HReg *hi, HReg *lo, ISelEnv *env, IRTemp tmp)
{
   vassert(tmp < env->n_vregmap);
   vassert(! hregIsInvalid(env->vregmapHI[tmp]));

   *lo = env->vregmap[tmp];
   *hi = env->vregmapHI[tmp];
}


static __inline__ HReg
mkVRegI(UInt ix)
{
   return mkHReg(True, HRcInt64, 0, ix);
}

static __inline__ HReg
newVRegI(ISelEnv *env)
{
   return mkVRegI(env->vreg_ctr++);
}


static __inline__ HReg
mkVRegF(UInt ix)
{
   return mkHReg(True, HRcFlt64, 0, ix);
}

static __inline__ HReg
newVRegF(ISelEnv *env)
{
   return mkVRegF(env->vreg_ctr++);
}


static __inline__ HReg
make_gpr(UInt regno)
{
   return s390_hreg_gpr(regno);
}


static __inline__ HReg
make_fpr(UInt regno)
{
   return s390_hreg_fpr(regno);
}



static __inline__ Bool
ulong_fits_unsigned_12bit(ULong val)
{
   return (val & 0xFFFu) == val;
}


static __inline__ Bool
ulong_fits_signed_20bit(ULong val)
{
   ULong v = val & 0xFFFFFu;

   v = (Long)(v << 44) >> 44;  

   return val == v;
}


static __inline__ Bool
ulong_fits_signed_8bit(ULong val)
{
   ULong v = val & 0xFFu;

   v = (Long)(v << 56) >> 56;  

   return val == v;
}

static s390_amode *
s390_isel_amode_wrk(ISelEnv *env, IRExpr *expr,
                    Bool select_b12_b20_only __attribute__((unused)))
{
   if (expr->tag == Iex_Binop && expr->Iex.Binop.op == Iop_Add64) {
      IRExpr *arg1 = expr->Iex.Binop.arg1;
      IRExpr *arg2 = expr->Iex.Binop.arg2;

      
      if (arg1->tag == Iex_Const) {
         IRExpr *tmp;
         tmp  = arg1;
         arg1 = arg2;
         arg2 = tmp;
      }

      
      if (arg2->tag == Iex_Const && arg2->Iex.Const.con->tag == Ico_U64) {
         ULong value = arg2->Iex.Const.con->Ico.U64;

         if (ulong_fits_unsigned_12bit(value)) {
            return s390_amode_b12((Int)value, s390_isel_int_expr(env, arg1));
         }
         if (ulong_fits_signed_20bit(value)) {
            return s390_amode_b20((Int)value, s390_isel_int_expr(env, arg1));
         }
      }
   }

   return s390_amode_b12(0, s390_isel_int_expr(env, expr));
}


static s390_amode *
s390_isel_amode(ISelEnv *env, IRExpr *expr)
{
   s390_amode *am;

   
   vassert(typeOfIRExpr(env->type_env, expr) == Ity_I64);

   am = s390_isel_amode_wrk(env, expr,  False);

   
   vassert(s390_amode_is_sane(am));

   return am;
}


static s390_amode *
s390_isel_amode_b12_b20(ISelEnv *env, IRExpr *expr)
{
   s390_amode *am;

   
   vassert(typeOfIRExpr(env->type_env, expr) == Ity_I64);

   am = s390_isel_amode_wrk(env, expr,  True);

   
   vassert(s390_amode_is_sane(am) &&
           (am->tag == S390_AMODE_B12 || am->tag == S390_AMODE_B20));

   return am;
}



#define order_commutative_operands(left, right)                   \
        do {                                                      \
          if (left->tag == Iex_Const || left->tag == Iex_Load ||  \
              left->tag == Iex_Get) {                             \
            IRExpr *tmp;                                          \
            tmp   = left;                                         \
            left  = right;                                        \
            right = tmp;                                          \
          }                                                       \
        } while (0)


static s390_insn *
s390_opnd_copy(UChar size, HReg dst, s390_opnd_RMI opnd)
{
   switch (opnd.tag) {
   case S390_OPND_AMODE:
      return s390_insn_load(size, dst, opnd.variant.am);

   case S390_OPND_REG:
      return s390_insn_move(size, dst, opnd.variant.reg);

   case S390_OPND_IMMEDIATE:
      return s390_insn_load_immediate(size, dst, opnd.variant.imm);

   default:
      vpanic("s390_opnd_copy");
   }
}


static __inline__ s390_opnd_RMI
s390_opnd_reg(HReg reg)
{
   s390_opnd_RMI opnd;

   opnd.tag  = S390_OPND_REG;
   opnd.variant.reg = reg;

   return opnd;
}


static __inline__ s390_opnd_RMI
s390_opnd_imm(ULong value)
{
   s390_opnd_RMI opnd;

   opnd.tag  = S390_OPND_IMMEDIATE;
   opnd.variant.imm = value;

   return opnd;
}


static Bool
s390_expr_is_const_zero(IRExpr *expr)
{
   ULong value;

   if (expr->tag == Iex_Const) {
      switch (expr->Iex.Const.con->tag) {
      case Ico_U1:  value = expr->Iex.Const.con->Ico.U1;  break;
      case Ico_U8:  value = expr->Iex.Const.con->Ico.U8;  break;
      case Ico_U16: value = expr->Iex.Const.con->Ico.U16; break;
      case Ico_U32: value = expr->Iex.Const.con->Ico.U32; break;
      case Ico_U64: value = expr->Iex.Const.con->Ico.U64; break;
      default:
         vpanic("s390_expr_is_const_zero");
      }
      return value == 0;
   }

   return 0;
}


static ULong
get_const_value_as_ulong(const IRConst *con)
{
   ULong value;

   switch (con->tag) {
   case Ico_U1:  value = con->Ico.U1;  return ((Long)(value << 63) >> 63);
   case Ico_U8:  value = con->Ico.U8;  return ((Long)(value << 56) >> 56);
   case Ico_U16: value = con->Ico.U16; return ((Long)(value << 48) >> 48);
   case Ico_U32: value = con->Ico.U32; return ((Long)(value << 32) >> 32);
   case Ico_U64: return con->Ico.U64;
   default:
      vpanic("get_const_value_as_ulong");
   }
}


static void
doHelperCall(UInt *stackAdjustAfterCall,
             RetLoc *retloc,
             ISelEnv *env, IRExpr *guard,
             IRCallee *callee, IRType retTy, IRExpr **args)
{
   UInt n_args, i, argreg, size;
   Addr64 target;
   HReg tmpregs[S390_NUM_GPRPARMS];
   s390_cc_t cc;

   
   *stackAdjustAfterCall = 0;
   *retloc               = mk_RetLoc_INVALID();

   UInt nVECRETs = 0;
   UInt nBBPTRs  = 0;

   n_args = 0;
   for (i = 0; args[i]; i++)
      ++n_args;

   if (n_args > S390_NUM_GPRPARMS) {
      vpanic("doHelperCall: too many arguments");
   }

   Int arg_errors = 0;
   for (i = 0; i < n_args; ++i) {
      if (UNLIKELY(args[i]->tag == Iex_VECRET)) {
         nVECRETs++;
      } else if (UNLIKELY(args[i]->tag == Iex_BBPTR)) {
         nBBPTRs++;
      } else {
         IRType type = typeOfIRExpr(env->type_env, args[i]);
         if (type != Ity_I64) {
            ++arg_errors;
            vex_printf("calling %s: argument #%d has type ", callee->name, i);
            ppIRType(type);
            vex_printf("; Ity_I64 is required\n");
         }
      }
   }

   if (arg_errors)
      vpanic("cannot continue due to errors in argument passing");

   
   vassert(nBBPTRs == 0 || nBBPTRs == 1);
   vassert(nVECRETs == 0);

   argreg = 0;

   
   for (i = 0; i < n_args; i++) {
      IRExpr *arg = args[i];
      if (UNLIKELY(arg->tag == Iex_BBPTR)) {
         
         tmpregs[argreg] = newVRegI(env);
         addInstr(env, s390_insn_move(sizeof(ULong), tmpregs[argreg],
                                      s390_hreg_guest_state_pointer()));
      } else {
         tmpregs[argreg] = s390_isel_int_expr(env, args[i]);
      }
      argreg++;
   }

   
   cc = S390_CC_ALWAYS;
   if (guard) {
      if (guard->tag == Iex_Const
          && guard->Iex.Const.con->tag == Ico_U1
          && guard->Iex.Const.con->Ico.U1 == True) {
         
      } else {
         cc = s390_isel_cc(env, guard);
      }
   }

   for (i = 0; i < argreg; i++) {
      HReg finalreg;

      finalreg = make_gpr(s390_gprno_from_arg_index(i));
      size = sizeofIRType(Ity_I64);
      addInstr(env, s390_insn_move(size, finalreg, tmpregs[i]));
   }

   target = (Addr)callee->addr;

   vassert(*stackAdjustAfterCall == 0);
   vassert(is_RetLoc_INVALID(*retloc));
   switch (retTy) {
   case Ity_INVALID:
      
      *retloc = mk_RetLoc_simple(RLPri_None);
      break;
   case Ity_I64: case Ity_I32: case Ity_I16: case Ity_I8:
      *retloc = mk_RetLoc_simple(RLPri_Int);
      break;
   default:
      vex_printf("calling %s: return type is ", callee->name);
      ppIRType(retTy);
      vex_printf("; an integer type is required\n");
      vassert(0);
   }

   
   addInstr(env, s390_insn_helper_call(cc, target, n_args,
                                       callee->name, *retloc));
}



static void 
set_bfp_rounding_mode_in_fpc(ISelEnv *env, IRExpr *irrm)
{
   vassert(typeOfIRExpr(env->type_env, irrm) == Ity_I32);

   
   if (env->previous_bfp_rounding_mode &&
       env->previous_bfp_rounding_mode->tag == Iex_RdTmp &&
       irrm->tag == Iex_RdTmp &&
       env->previous_bfp_rounding_mode->Iex.RdTmp.tmp == irrm->Iex.RdTmp.tmp) {
      
      return;
   }

   
   env->previous_bfp_rounding_mode = irrm;

   HReg ir = s390_isel_int_expr(env, irrm);

   HReg mode = newVRegI(env);

   addInstr(env, s390_insn_load_immediate(4, mode, 4));
   addInstr(env, s390_insn_alu(4, S390_ALU_SUB, mode, s390_opnd_reg(ir)));
   addInstr(env, s390_insn_alu(4, S390_ALU_AND, mode, s390_opnd_imm(3)));

   addInstr(env, s390_insn_set_fpc_bfprm(4, mode));
}


static s390_bfp_round_t
get_bfp_rounding_mode(ISelEnv *env, IRExpr *irrm)
{
   if (irrm->tag == Iex_Const) {          
      vassert(irrm->Iex.Const.con->tag == Ico_U32);
      IRRoundingMode mode = irrm->Iex.Const.con->Ico.U32;

      switch (mode) {
      case Irrm_NEAREST:  return S390_BFP_ROUND_NEAREST_EVEN;
      case Irrm_ZERO:     return S390_BFP_ROUND_ZERO;
      case Irrm_PosINF:   return S390_BFP_ROUND_POSINF;
      case Irrm_NegINF:   return S390_BFP_ROUND_NEGINF;
      default:
         vpanic("get_bfp_rounding_mode");
      }
   }

   set_bfp_rounding_mode_in_fpc(env, irrm);
   return S390_BFP_ROUND_PER_FPC;
}



static void
set_dfp_rounding_mode_in_fpc(ISelEnv *env, IRExpr *irrm)
{
   vassert(typeOfIRExpr(env->type_env, irrm) == Ity_I32);

   
   if (env->previous_dfp_rounding_mode &&
       env->previous_dfp_rounding_mode->tag == Iex_RdTmp &&
       irrm->tag == Iex_RdTmp &&
       env->previous_dfp_rounding_mode->Iex.RdTmp.tmp == irrm->Iex.RdTmp.tmp) {
      
      return;
   }

   
   env->previous_dfp_rounding_mode = irrm;

   HReg ir = s390_isel_int_expr(env, irrm);

   HReg mode = newVRegI(env);

   addInstr(env, s390_insn_move(4, mode, ir));
   addInstr(env, s390_insn_alu(4, S390_ALU_LSH, mode, s390_opnd_imm(1)));
   addInstr(env, s390_insn_alu(4, S390_ALU_AND, mode, s390_opnd_imm(2)));
   addInstr(env, s390_insn_alu(4, S390_ALU_XOR, mode, s390_opnd_reg(ir)));

   addInstr(env, s390_insn_set_fpc_dfprm(4, mode));
}


static s390_dfp_round_t
get_dfp_rounding_mode(ISelEnv *env, IRExpr *irrm)
{
   if (irrm->tag == Iex_Const) {          
      vassert(irrm->Iex.Const.con->tag == Ico_U32);
      IRRoundingMode mode = irrm->Iex.Const.con->Ico.U32;

      switch (mode) {
      case Irrm_NEAREST:
         return S390_DFP_ROUND_NEAREST_EVEN_8;
      case Irrm_NegINF:
         return S390_DFP_ROUND_NEGINF_11;
      case Irrm_PosINF:
         return S390_DFP_ROUND_POSINF_10;
      case Irrm_ZERO:
         return S390_DFP_ROUND_ZERO_9;
      case Irrm_NEAREST_TIE_AWAY_0:
         return S390_DFP_ROUND_NEAREST_TIE_AWAY_0_12;
      case Irrm_PREPARE_SHORTER:
          return S390_DFP_ROUND_PREPARE_SHORT_15;
      case Irrm_AWAY_FROM_ZERO:
         return S390_DFP_ROUND_AWAY_0;
      case Irrm_NEAREST_TIE_TOWARD_0:
         return S390_DFP_ROUND_NEAREST_TIE_TOWARD_0;
      default:
         vpanic("get_dfp_rounding_mode");
      }
   }

   set_dfp_rounding_mode_in_fpc(env, irrm);
   return S390_DFP_ROUND_PER_FPC_0;
}



static HReg
convert_s390_to_vex_bfpcc(ISelEnv *env, HReg cc_s390)
{
   HReg cc0, cc1, b2, b6, cc_vex;

   cc0 = newVRegI(env);
   addInstr(env, s390_insn_move(4, cc0, cc_s390));
   addInstr(env, s390_insn_alu(4, S390_ALU_AND, cc0, s390_opnd_imm(1)));

   cc1 = newVRegI(env);
   addInstr(env, s390_insn_move(4, cc1, cc_s390));
   addInstr(env, s390_insn_alu(4, S390_ALU_RSH, cc1, s390_opnd_imm(1)));

   b2 = newVRegI(env);
   addInstr(env, s390_insn_move(4, b2, cc0));
   addInstr(env, s390_insn_alu(4, S390_ALU_AND, b2, s390_opnd_reg(cc1)));
   addInstr(env, s390_insn_alu(4, S390_ALU_LSH, b2, s390_opnd_imm(2)));

   b6 = newVRegI(env);
   addInstr(env, s390_insn_move(4, b6, cc0));
   addInstr(env, s390_insn_alu(4, S390_ALU_SUB, b6, s390_opnd_reg(cc1)));
   addInstr(env, s390_insn_alu(4, S390_ALU_ADD, b6, s390_opnd_imm(1)));
   addInstr(env, s390_insn_alu(4, S390_ALU_AND, b6, s390_opnd_imm(1)));
   addInstr(env, s390_insn_alu(4, S390_ALU_LSH, b6, s390_opnd_imm(6)));

   cc_vex = newVRegI(env);
   addInstr(env, s390_insn_move(4, cc_vex, cc0));
   addInstr(env, s390_insn_alu(4, S390_ALU_OR, cc_vex, s390_opnd_reg(b2)));
   addInstr(env, s390_insn_alu(4, S390_ALU_OR, cc_vex, s390_opnd_reg(b6)));

   return cc_vex;
}

static HReg
convert_s390_to_vex_dfpcc(ISelEnv *env, HReg cc_s390)
{
   
   return convert_s390_to_vex_bfpcc(env, cc_s390);
}


static void
s390_isel_int128_expr_wrk(HReg *dst_hi, HReg *dst_lo, ISelEnv *env,
                          IRExpr *expr)
{
   IRType ty = typeOfIRExpr(env->type_env, expr);

   vassert(ty == Ity_I128);


   
   if (expr->tag == Iex_RdTmp) {
      lookupIRTemp128(dst_hi, dst_lo, env, expr->Iex.RdTmp.tmp);
      return;
   }

   if (expr->tag == Iex_Binop) {
      IRExpr *arg1 = expr->Iex.Binop.arg1;
      IRExpr *arg2 = expr->Iex.Binop.arg2;
      Bool is_signed_multiply, is_signed_divide;

      switch (expr->Iex.Binop.op) {
      case Iop_MullU64:
         is_signed_multiply = False;
         goto do_multiply64;

      case Iop_MullS64:
         is_signed_multiply = True;
         goto do_multiply64;

      case Iop_DivModU128to64:
         is_signed_divide = False;
         goto do_divide64;

      case Iop_DivModS128to64:
         is_signed_divide = True;
         goto do_divide64;

      case Iop_64HLto128:
         *dst_hi = s390_isel_int_expr(env, arg1);
         *dst_lo = s390_isel_int_expr(env, arg2);
         return;

      case Iop_DivModS64to64: {
         HReg r10, r11, h1;
         s390_opnd_RMI op2;

         h1  = s390_isel_int_expr(env, arg1);       
         op2 = s390_isel_int_expr_RMI(env, arg2);   

         
         r10  = make_gpr(10);
         r11  = make_gpr(11);

         
         addInstr(env, s390_insn_move(8, r11, h1));

         
         addInstr(env, s390_insn_divs(8, r10, r11, op2));

         *dst_hi = newVRegI(env);
         *dst_lo = newVRegI(env);
         addInstr(env, s390_insn_move(8, *dst_hi, r10));
         addInstr(env, s390_insn_move(8, *dst_lo, r11));
         return;
      }

      default:
         break;

      do_multiply64: {
            HReg r10, r11, h1;
            s390_opnd_RMI op2;

            order_commutative_operands(arg1, arg2);

            h1   = s390_isel_int_expr(env, arg1);       
            op2  = s390_isel_int_expr_RMI(env, arg2);   

            
            r10  = make_gpr(10);
            r11  = make_gpr(11);

            
            addInstr(env, s390_insn_move(8, r11, h1));

            
            addInstr(env, s390_insn_mul(8, r10, r11, op2, is_signed_multiply));

            *dst_hi = newVRegI(env);
            *dst_lo = newVRegI(env);
            addInstr(env, s390_insn_move(8, *dst_hi, r10));
            addInstr(env, s390_insn_move(8, *dst_lo, r11));
            return;
         }

      do_divide64: {
         HReg r10, r11, hi, lo;
         s390_opnd_RMI op2;

         s390_isel_int128_expr(&hi, &lo, env, arg1);
         op2  = s390_isel_int_expr_RMI(env, arg2);   

         
         r10  = make_gpr(10);
         r11  = make_gpr(11);

         addInstr(env, s390_insn_move(8, r10, hi));
         addInstr(env, s390_insn_move(8, r11, lo));

         
         addInstr(env, s390_insn_div(8, r10, r11, op2, is_signed_divide));

         *dst_hi = newVRegI(env);
         *dst_lo = newVRegI(env);
         addInstr(env, s390_insn_move(8, *dst_hi, r10));
         addInstr(env, s390_insn_move(8, *dst_lo, r11));
         return;
      }
      }
   }

   vpanic("s390_isel_int128_expr");
}


static void
s390_isel_int128_expr(HReg *dst_hi, HReg *dst_lo, ISelEnv *env, IRExpr *expr)
{
   s390_isel_int128_expr_wrk(dst_hi, dst_lo, env, expr);

   
   vassert(hregIsVirtual(*dst_hi));
   vassert(hregIsVirtual(*dst_lo));
   vassert(hregClass(*dst_hi) == HRcInt64);
   vassert(hregClass(*dst_lo) == HRcInt64);
}




static HReg
s390_isel_int_expr_wrk(ISelEnv *env, IRExpr *expr)
{
   IRType ty = typeOfIRExpr(env->type_env, expr);
   UChar size;
   s390_bfp_conv_t conv;
   s390_dfp_conv_t dconv;

   vassert(ty == Ity_I8 || ty == Ity_I16 || ty == Ity_I32 || ty == Ity_I64);

   size = sizeofIRType(ty);   

   switch (expr->tag) {

      
   case Iex_RdTmp:
      
      return lookupIRTemp(env, expr->Iex.RdTmp.tmp);

      
   case Iex_Load: {
      HReg        dst = newVRegI(env);
      s390_amode *am  = s390_isel_amode(env, expr->Iex.Load.addr);

      if (expr->Iex.Load.end != Iend_BE)
         goto irreducible;

      addInstr(env, s390_insn_load(size, dst, am));

      return dst;
   }

      
   case Iex_Binop: {
      IRExpr *arg1 = expr->Iex.Binop.arg1;
      IRExpr *arg2 = expr->Iex.Binop.arg2;
      HReg h1, res;
      s390_alu_t opkind;
      s390_opnd_RMI op2, value, opnd;
      s390_insn *insn;
      Bool is_commutative, is_signed_multiply, is_signed_divide;

      is_commutative = True;

      switch (expr->Iex.Binop.op) {
      case Iop_MullU8:
      case Iop_MullU16:
      case Iop_MullU32:
         is_signed_multiply = False;
         goto do_multiply;

      case Iop_MullS8:
      case Iop_MullS16:
      case Iop_MullS32:
         is_signed_multiply = True;
         goto do_multiply;

      do_multiply: {
            HReg r10, r11;
            UInt arg_size = size / 2;

            order_commutative_operands(arg1, arg2);

            h1   = s390_isel_int_expr(env, arg1);       
            op2  = s390_isel_int_expr_RMI(env, arg2);   

            
            r10  = make_gpr(10);
            r11  = make_gpr(11);

            
            addInstr(env, s390_insn_move(arg_size, r11, h1));

            
            addInstr(env, s390_insn_mul(arg_size, r10, r11, op2, is_signed_multiply));

            res  = newVRegI(env);
            addInstr(env, s390_insn_move(arg_size, res, r10));
            value = s390_opnd_imm(arg_size * 8);
            addInstr(env, s390_insn_alu(size, S390_ALU_LSH, res, value));
            value = s390_opnd_imm((((ULong)1) << arg_size * 8) - 1);
            addInstr(env, s390_insn_alu(size, S390_ALU_AND, r11, value));
            opnd = s390_opnd_reg(r11);
            addInstr(env, s390_insn_alu(size, S390_ALU_OR,  res, opnd));
            return res;
         }

      case Iop_DivModS64to32:
         is_signed_divide = True;
         goto do_divide;

      case Iop_DivModU64to32:
         is_signed_divide = False;
         goto do_divide;

      do_divide: {
            HReg r10, r11;

            h1   = s390_isel_int_expr(env, arg1);       
            op2  = s390_isel_int_expr_RMI(env, arg2);   

            
            r10  = make_gpr(10);
            r11  = make_gpr(11);

            addInstr(env, s390_insn_move(8, r10, h1));
            addInstr(env, s390_insn_move(8, r11, h1));
            value = s390_opnd_imm(32);
            addInstr(env, s390_insn_alu(8, S390_ALU_RSH, r10, value));

            
            addInstr(env, s390_insn_div(4, r10, r11, op2, is_signed_divide));

            res  = newVRegI(env);
            addInstr(env, s390_insn_move(8, res, r10));
            value = s390_opnd_imm(32);
            addInstr(env, s390_insn_alu(8, S390_ALU_LSH, res, value));
            value = s390_opnd_imm((((ULong)1) << 32) - 1);
            addInstr(env, s390_insn_alu(8, S390_ALU_AND, r11, value));
            opnd = s390_opnd_reg(r11);
            addInstr(env, s390_insn_alu(8, S390_ALU_OR,  res, opnd));
            return res;
         }

      case Iop_F32toI32S:  conv = S390_BFP_F32_TO_I32;  goto do_convert;
      case Iop_F32toI64S:  conv = S390_BFP_F32_TO_I64;  goto do_convert;
      case Iop_F32toI32U:  conv = S390_BFP_F32_TO_U32;  goto do_convert;
      case Iop_F32toI64U:  conv = S390_BFP_F32_TO_U64;  goto do_convert;
      case Iop_F64toI32S:  conv = S390_BFP_F64_TO_I32;  goto do_convert;
      case Iop_F64toI64S:  conv = S390_BFP_F64_TO_I64;  goto do_convert;
      case Iop_F64toI32U:  conv = S390_BFP_F64_TO_U32;  goto do_convert;
      case Iop_F64toI64U:  conv = S390_BFP_F64_TO_U64;  goto do_convert;
      case Iop_F128toI32S: conv = S390_BFP_F128_TO_I32; goto do_convert_128;
      case Iop_F128toI64S: conv = S390_BFP_F128_TO_I64; goto do_convert_128;
      case Iop_F128toI32U: conv = S390_BFP_F128_TO_U32; goto do_convert_128;
      case Iop_F128toI64U: conv = S390_BFP_F128_TO_U64; goto do_convert_128;

      case Iop_D64toI32S:  dconv = S390_DFP_D64_TO_I32;  goto do_convert_dfp;
      case Iop_D64toI64S:  dconv = S390_DFP_D64_TO_I64;  goto do_convert_dfp;
      case Iop_D64toI32U:  dconv = S390_DFP_D64_TO_U32;  goto do_convert_dfp;
      case Iop_D64toI64U:  dconv = S390_DFP_D64_TO_U64;  goto do_convert_dfp;
      case Iop_D128toI32S: dconv = S390_DFP_D128_TO_I32; goto do_convert_dfp128;
      case Iop_D128toI64S: dconv = S390_DFP_D128_TO_I64; goto do_convert_dfp128;
      case Iop_D128toI32U: dconv = S390_DFP_D128_TO_U32; goto do_convert_dfp128;
      case Iop_D128toI64U: dconv = S390_DFP_D128_TO_U64; goto do_convert_dfp128;

      do_convert: {
         s390_bfp_round_t rounding_mode;

         res  = newVRegI(env);
         h1   = s390_isel_float_expr(env, arg2);   

         rounding_mode = get_bfp_rounding_mode(env, arg1);
         addInstr(env, s390_insn_bfp_convert(size, conv, res, h1,
                                             rounding_mode));
         return res;
      }

      do_convert_128: {
         s390_bfp_round_t rounding_mode;
         HReg op_hi, op_lo, f13, f15;

         res = newVRegI(env);
         s390_isel_float128_expr(&op_hi, &op_lo, env, arg2); 

         
         f13 = make_fpr(13);
         f15 = make_fpr(15);

         
         addInstr(env, s390_insn_move(8, f13, op_hi));
         addInstr(env, s390_insn_move(8, f15, op_lo));

         rounding_mode = get_bfp_rounding_mode(env, arg1);
         addInstr(env, s390_insn_bfp128_convert_from(size, conv, res,
                                                     INVALID_HREG, f13, f15,
                                                     rounding_mode));
         return res;
      }

      do_convert_dfp: {
            s390_dfp_round_t rounding_mode;

            res  = newVRegI(env);
            h1   = s390_isel_dfp_expr(env, arg2);   

            rounding_mode = get_dfp_rounding_mode(env, arg1);
            addInstr(env, s390_insn_dfp_convert(size, dconv, res, h1,
                                                rounding_mode));
            return res;
         }

      do_convert_dfp128: {
            s390_dfp_round_t rounding_mode;
            HReg op_hi, op_lo, f13, f15;

            res = newVRegI(env);
            s390_isel_dfp128_expr(&op_hi, &op_lo, env, arg2); 

            
            f13 = make_fpr(13);
            f15 = make_fpr(15);

            
            addInstr(env, s390_insn_move(8, f13, op_hi));
            addInstr(env, s390_insn_move(8, f15, op_lo));

            rounding_mode = get_dfp_rounding_mode(env, arg1);
            addInstr(env, s390_insn_dfp128_convert_from(size, dconv, res,
                                                        INVALID_HREG, f13,
                                                        f15, rounding_mode));
            return res;
         }

      case Iop_8HLto16:
      case Iop_16HLto32:
      case Iop_32HLto64: {
         HReg h2;
         UInt arg_size = size / 2;

         res  = newVRegI(env);
         h1   = s390_isel_int_expr(env, arg1);   
         h2   = s390_isel_int_expr(env, arg2);   

         addInstr(env, s390_insn_move(arg_size, res, h1));
         value = s390_opnd_imm(arg_size * 8);
         addInstr(env, s390_insn_alu(size, S390_ALU_LSH, res, value));
         value = s390_opnd_imm((((ULong)1) << arg_size * 8) - 1);
         addInstr(env, s390_insn_alu(size, S390_ALU_AND, h2, value));
         opnd = s390_opnd_reg(h2);
         addInstr(env, s390_insn_alu(size, S390_ALU_OR,  res, opnd));
         return res;
      }

      case Iop_Max32U: {
         
         res = newVRegI(env);
         h1  = s390_isel_int_expr(env, arg1);
         op2 = s390_isel_int_expr_RMI(env, arg2);

         addInstr(env, s390_insn_move(size, res, h1));
         addInstr(env, s390_insn_compare(size, res, op2, False ));
         addInstr(env, s390_insn_cond_move(size, S390_CC_L, res, op2));
         return res;
      }

      case Iop_CmpF32:
      case Iop_CmpF64: {
         HReg cc_s390, h2;

         h1 = s390_isel_float_expr(env, arg1);
         h2 = s390_isel_float_expr(env, arg2);
         cc_s390 = newVRegI(env);

         size = (expr->Iex.Binop.op == Iop_CmpF32) ? 4 : 8;

         addInstr(env, s390_insn_bfp_compare(size, cc_s390, h1, h2));

         return convert_s390_to_vex_bfpcc(env, cc_s390);
      }

      case Iop_CmpF128: {
         HReg op1_hi, op1_lo, op2_hi, op2_lo, f12, f13, f14, f15, cc_s390;

         s390_isel_float128_expr(&op1_hi, &op1_lo, env, arg1); 
         s390_isel_float128_expr(&op2_hi, &op2_lo, env, arg2); 
         cc_s390 = newVRegI(env);

         
         f12 = make_fpr(12);
         f13 = make_fpr(13);
         f14 = make_fpr(14);
         f15 = make_fpr(15);

         
         addInstr(env, s390_insn_move(8, f12, op1_hi));
         addInstr(env, s390_insn_move(8, f14, op1_lo));

         
         addInstr(env, s390_insn_move(8, f13, op2_hi));
         addInstr(env, s390_insn_move(8, f15, op2_lo));

         res = newVRegI(env);
         addInstr(env, s390_insn_bfp128_compare(16, cc_s390, f12, f14, f13, f15));

         return convert_s390_to_vex_bfpcc(env, cc_s390);
      }

      case Iop_CmpD64:
      case Iop_CmpExpD64: {
         HReg cc_s390, h2;
         s390_dfp_cmp_t cmp;

         h1 = s390_isel_dfp_expr(env, arg1);
         h2 = s390_isel_dfp_expr(env, arg2);
         cc_s390 = newVRegI(env);

         switch(expr->Iex.Binop.op) {
         case Iop_CmpD64:    cmp = S390_DFP_COMPARE; break;
         case Iop_CmpExpD64: cmp = S390_DFP_COMPARE_EXP; break;
         default: goto irreducible;
         }
         addInstr(env, s390_insn_dfp_compare(8, cmp, cc_s390, h1, h2));

         return convert_s390_to_vex_dfpcc(env, cc_s390);
      }

      case Iop_CmpD128:
      case Iop_CmpExpD128: {
         HReg op1_hi, op1_lo, op2_hi, op2_lo, f12, f13, f14, f15, cc_s390;
         s390_dfp_cmp_t cmp;

         s390_isel_dfp128_expr(&op1_hi, &op1_lo, env, arg1); 
         s390_isel_dfp128_expr(&op2_hi, &op2_lo, env, arg2); 
         cc_s390 = newVRegI(env);

         
         f12 = make_fpr(12);
         f13 = make_fpr(13);
         f14 = make_fpr(14);
         f15 = make_fpr(15);

         
         addInstr(env, s390_insn_move(8, f12, op1_hi));
         addInstr(env, s390_insn_move(8, f14, op1_lo));

         
         addInstr(env, s390_insn_move(8, f13, op2_hi));
         addInstr(env, s390_insn_move(8, f15, op2_lo));

         switch(expr->Iex.Binop.op) {
         case Iop_CmpD128:    cmp = S390_DFP_COMPARE; break;
         case Iop_CmpExpD128: cmp = S390_DFP_COMPARE_EXP; break;
         default: goto irreducible;
         }
         addInstr(env, s390_insn_dfp128_compare(16, cmp, cc_s390, f12, f14,
                                                f13, f15));

         return convert_s390_to_vex_dfpcc(env, cc_s390);
      }

      case Iop_Add8:
      case Iop_Add16:
      case Iop_Add32:
      case Iop_Add64:
         opkind = S390_ALU_ADD;
         break;

      case Iop_Sub8:
      case Iop_Sub16:
      case Iop_Sub32:
      case Iop_Sub64:
         opkind = S390_ALU_SUB;
         is_commutative = False;
         break;

      case Iop_And8:
      case Iop_And16:
      case Iop_And32:
      case Iop_And64:
         opkind = S390_ALU_AND;
         break;

      case Iop_Or8:
      case Iop_Or16:
      case Iop_Or32:
      case Iop_Or64:
         opkind = S390_ALU_OR;
         break;

      case Iop_Xor8:
      case Iop_Xor16:
      case Iop_Xor32:
      case Iop_Xor64:
         opkind = S390_ALU_XOR;
         break;

      case Iop_Shl8:
      case Iop_Shl16:
      case Iop_Shl32:
      case Iop_Shl64:
         opkind = S390_ALU_LSH;
         is_commutative = False;
         break;

      case Iop_Shr8:
      case Iop_Shr16:
      case Iop_Shr32:
      case Iop_Shr64:
         opkind = S390_ALU_RSH;
         is_commutative = False;
         break;

      case Iop_Sar8:
      case Iop_Sar16:
      case Iop_Sar32:
      case Iop_Sar64:
         opkind = S390_ALU_RSHA;
         is_commutative = False;
         break;

      default:
         goto irreducible;
      }

      
      if (opkind == S390_ALU_SUB && s390_expr_is_const_zero(arg1)) {
         res  = newVRegI(env);
         op2  = s390_isel_int_expr_RMI(env, arg2);   
         insn = s390_insn_unop(size, S390_NEGATE, res, op2);
         addInstr(env, insn);

         return res;
      }

      if (is_commutative) {
         order_commutative_operands(arg1, arg2);
      }

      h1   = s390_isel_int_expr(env, arg1);       
      op2  = s390_isel_int_expr_RMI(env, arg2);   
      res  = newVRegI(env);

      switch (expr->Iex.Binop.op) {
      case Iop_Shr8:
         insn = s390_insn_unop(4, S390_ZERO_EXTEND_8, res, s390_opnd_reg(h1));
         break;
      case Iop_Shr16:
         insn = s390_insn_unop(4, S390_ZERO_EXTEND_16, res, s390_opnd_reg(h1));
         break;
      case Iop_Sar8:
         insn = s390_insn_unop(4, S390_SIGN_EXTEND_8, res, s390_opnd_reg(h1));
         break;
      case Iop_Sar16:
         insn = s390_insn_unop(4, S390_SIGN_EXTEND_16, res, s390_opnd_reg(h1));
         break;
      default:
         insn = s390_insn_move(size, res, h1);
         break;
      }
      addInstr(env, insn);

      insn = s390_insn_alu(size, opkind, res, op2);

      addInstr(env, insn);

      return res;
   }

      
   case Iex_Unop: {
      static s390_opnd_RMI mask  = { S390_OPND_IMMEDIATE };
      static s390_opnd_RMI shift = { S390_OPND_IMMEDIATE };
      s390_opnd_RMI opnd;
      s390_insn    *insn;
      IRExpr *arg;
      HReg    dst, h1;
      IROp    unop, binop;

      arg = expr->Iex.Unop.arg;

      

      unop  = expr->Iex.Unop.op;
      binop = arg->Iex.Binop.op;

      if ((arg->tag == Iex_Binop &&
           ((unop == Iop_64to32 &&
             (binop == Iop_MullS32 || binop == Iop_MullU32)) ||
            (unop == Iop_128to64 &&
             (binop == Iop_MullS64 || binop == Iop_MullU64))))) {
         h1   = s390_isel_int_expr(env, arg->Iex.Binop.arg1);     
         opnd = s390_isel_int_expr_RMI(env, arg->Iex.Binop.arg2); 
         dst  = newVRegI(env);     
         addInstr(env, s390_insn_move(size, dst, h1));
         addInstr(env, s390_insn_alu(size, S390_ALU_MUL, dst, opnd));

         return dst;
      }

      if (unop == Iop_ReinterpF64asI64 || unop == Iop_ReinterpF32asI32) {
         dst = newVRegI(env);
         h1  = s390_isel_float_expr(env, arg);     
         addInstr(env, s390_insn_move(size, dst, h1));

         return dst;
      }

      if (unop == Iop_ReinterpD64asI64) {
         dst = newVRegI(env);
         h1  = s390_isel_dfp_expr(env, arg);     
         addInstr(env, s390_insn_move(size, dst, h1));

         return dst;
      }

      if (unop == Iop_ExtractExpD64 || unop == Iop_ExtractSigD64) {
         s390_dfp_unop_t dfpop;
         switch(unop) {
         case Iop_ExtractExpD64: dfpop = S390_DFP_EXTRACT_EXP_D64; break;
         case Iop_ExtractSigD64: dfpop = S390_DFP_EXTRACT_SIG_D64; break;
         default: goto irreducible;
         }
         dst = newVRegI(env);
         h1  = s390_isel_dfp_expr(env, arg);     
         addInstr(env, s390_insn_dfp_unop(size, dfpop, dst, h1));
         return dst;
      }

      if (unop == Iop_ExtractExpD128 || unop == Iop_ExtractSigD128) {
         s390_dfp_unop_t dfpop;
         HReg op_hi, op_lo, f13, f15;

         switch(unop) {
         case Iop_ExtractExpD128: dfpop = S390_DFP_EXTRACT_EXP_D128; break;
         case Iop_ExtractSigD128: dfpop = S390_DFP_EXTRACT_SIG_D128; break;
         default: goto irreducible;
         }
         dst = newVRegI(env);
         s390_isel_dfp128_expr(&op_hi, &op_lo, env, arg); 

         
         f13 = make_fpr(13);
         f15 = make_fpr(15);

         
         addInstr(env, s390_insn_move(8, f13, op_hi));
         addInstr(env, s390_insn_move(8, f15, op_lo));

         addInstr(env, s390_insn_dfp128_unop(size, dfpop, dst, f13, f15));
         return dst;
      }

      
      if (typeOfIRExpr(env->type_env, arg) == Ity_I1) {
         s390_cc_t cond = s390_isel_cc(env, arg);
         dst = newVRegI(env);     
         addInstr(env, s390_insn_cc2bool(dst, cond));

         switch (unop) {
         case Iop_1Uto8:
         case Iop_1Uto32:
            
            mask.variant.imm = 1;
            addInstr(env, s390_insn_alu(4, S390_ALU_AND,  dst, mask));
            break;

         case Iop_1Uto64:
            
            mask.variant.imm = 1;
            addInstr(env, s390_insn_alu(8, S390_ALU_AND,  dst, mask));
            break;

         case Iop_1Sto8:
         case Iop_1Sto16:
         case Iop_1Sto32:
            shift.variant.imm = 31;
            addInstr(env, s390_insn_alu(4, S390_ALU_LSH,  dst, shift));
            addInstr(env, s390_insn_alu(4, S390_ALU_RSHA, dst, shift));
            break;

         case Iop_1Sto64:
            shift.variant.imm = 63;
            addInstr(env, s390_insn_alu(8, S390_ALU_LSH,  dst, shift));
            addInstr(env, s390_insn_alu(8, S390_ALU_RSHA, dst, shift));
            break;

         default:
            goto irreducible;
         }

         return dst;
      }

      

      if (unop == Iop_128to64) {
         HReg dst_hi, dst_lo;

         s390_isel_int128_expr(&dst_hi, &dst_lo, env, arg);
         return dst_lo;
      }

      if (unop == Iop_128HIto64) {
         HReg dst_hi, dst_lo;

         s390_isel_int128_expr(&dst_hi, &dst_lo, env, arg);
         return dst_hi;
      }

      dst  = newVRegI(env);     
      opnd = s390_isel_int_expr_RMI(env, arg);     

      switch (unop) {
      case Iop_8Uto16:
      case Iop_8Uto32:
      case Iop_8Uto64:
         insn = s390_insn_unop(size, S390_ZERO_EXTEND_8, dst, opnd);
         break;

      case Iop_16Uto32:
      case Iop_16Uto64:
         insn = s390_insn_unop(size, S390_ZERO_EXTEND_16, dst, opnd);
         break;

      case Iop_32Uto64:
         insn = s390_insn_unop(size, S390_ZERO_EXTEND_32, dst, opnd);
         break;

      case Iop_8Sto16:
      case Iop_8Sto32:
      case Iop_8Sto64:
         insn = s390_insn_unop(size, S390_SIGN_EXTEND_8, dst, opnd);
         break;

      case Iop_16Sto32:
      case Iop_16Sto64:
         insn = s390_insn_unop(size, S390_SIGN_EXTEND_16, dst, opnd);
         break;

      case Iop_32Sto64:
         insn = s390_insn_unop(size, S390_SIGN_EXTEND_32, dst, opnd);
         break;

      case Iop_64to8:
      case Iop_64to16:
      case Iop_64to32:
      case Iop_32to8:
      case Iop_32to16:
      case Iop_16to8:
         insn = s390_opnd_copy(8, dst, opnd);
         break;

      case Iop_64HIto32:
         addInstr(env, s390_opnd_copy(8, dst, opnd));
         shift.variant.imm = 32;
         insn = s390_insn_alu(8, S390_ALU_RSH, dst, shift);
         break;

      case Iop_32HIto16:
         addInstr(env, s390_opnd_copy(4, dst, opnd));
         shift.variant.imm = 16;
         insn = s390_insn_alu(4, S390_ALU_RSH, dst, shift);
         break;

      case Iop_16HIto8:
         addInstr(env, s390_opnd_copy(2, dst, opnd));
         shift.variant.imm = 8;
         insn = s390_insn_alu(2, S390_ALU_RSH, dst, shift);
         break;

      case Iop_Not8:
      case Iop_Not16:
      case Iop_Not32:
      case Iop_Not64:
         
         mask.variant.imm = ~(ULong)0;
         addInstr(env, s390_opnd_copy(size, dst, opnd));
         insn = s390_insn_alu(size, S390_ALU_XOR, dst, mask);
         break;

      case Iop_Left8:
      case Iop_Left16:
      case Iop_Left32:
      case Iop_Left64:
         addInstr(env, s390_insn_unop(size, S390_NEGATE, dst, opnd));
         insn = s390_insn_alu(size, S390_ALU_OR, dst, opnd);
         break;

      case Iop_CmpwNEZ32:
      case Iop_CmpwNEZ64: {
         addInstr(env, s390_insn_unop(size, S390_NEGATE, dst, opnd));
         addInstr(env, s390_insn_alu(size, S390_ALU_OR,  dst, opnd));
         shift.variant.imm = (unop == Iop_CmpwNEZ32) ? 31 : 63;
         addInstr(env, s390_insn_alu(size, S390_ALU_RSHA,  dst, shift));
         return dst;
      }

      case Iop_Clz64: {
         HReg r10, r11;

         r10  = make_gpr(10);
         r11  = make_gpr(11);

         addInstr(env, s390_insn_clz(8, r10, r11, opnd));
         addInstr(env, s390_insn_move(8, dst, r10));
         return dst;
      }

      default:
         goto irreducible;
      }

      addInstr(env, insn);

      return dst;
   }

      
   case Iex_Get: {
      HReg dst = newVRegI(env);
      s390_amode *am = s390_amode_for_guest_state(expr->Iex.Get.offset);

      vassert(size <= 8);

      addInstr(env, s390_insn_load(size, dst, am));

      return dst;
   }

   case Iex_GetI:
      
      break;

      
   case Iex_CCall: {
      HReg dst = newVRegI(env);
      HReg ret = make_gpr(S390_REGNO_RETURN_VALUE);
      UInt   addToSp = 0;
      RetLoc rloc    = mk_RetLoc_INVALID();

      doHelperCall(&addToSp, &rloc, env, NULL, expr->Iex.CCall.cee,
                   expr->Iex.CCall.retty, expr->Iex.CCall.args);
      vassert(is_sane_RetLoc(rloc));
      vassert(rloc.pri == RLPri_Int);
      vassert(addToSp == 0);
      addInstr(env, s390_insn_move(sizeof(ULong), dst, ret));

      return dst;
   }

      

   case Iex_Const: {
      ULong value;
      HReg  dst = newVRegI(env);
      const IRConst *con = expr->Iex.Const.con;

      
      switch (con->tag) {
      case Ico_U64: value = con->Ico.U64; break;
      case Ico_U32: value = con->Ico.U32; break;
      case Ico_U16: value = con->Ico.U16; break;
      case Ico_U8:  value = con->Ico.U8;  break;
      default:      vpanic("s390_isel_int_expr: invalid constant");
      }

      addInstr(env, s390_insn_load_immediate(size, dst, value));

      return dst;
   }

      
   case Iex_ITE: {
      IRExpr *cond_expr;
      HReg dst, r1;
      s390_opnd_RMI r0;

      cond_expr = expr->Iex.ITE.cond;

      vassert(typeOfIRExpr(env->type_env, cond_expr) == Ity_I1);

      dst  = newVRegI(env);
      r0   = s390_isel_int_expr_RMI(env, expr->Iex.ITE.iffalse);
      r1   = s390_isel_int_expr(env, expr->Iex.ITE.iftrue);
      size = sizeofIRType(typeOfIRExpr(env->type_env, expr->Iex.ITE.iftrue));

      s390_cc_t cc = s390_isel_cc(env, cond_expr);

      addInstr(env, s390_insn_move(size, dst, r1));
      addInstr(env, s390_insn_cond_move(size, s390_cc_invert(cc), dst, r0));
      return dst;
   }

   default:
      break;
   }

   
 irreducible:
   ppIRExpr(expr);
   vpanic("s390_isel_int_expr: cannot reduce tree");
}


static HReg
s390_isel_int_expr(ISelEnv *env, IRExpr *expr)
{
   HReg dst = s390_isel_int_expr_wrk(env, expr);

   
   vassert(hregClass(dst) == HRcInt64);
   vassert(hregIsVirtual(dst));

   return dst;
}


static s390_opnd_RMI
s390_isel_int_expr_RMI(ISelEnv *env, IRExpr *expr)
{
   IRType ty = typeOfIRExpr(env->type_env, expr);
   s390_opnd_RMI dst;

   vassert(ty == Ity_I8 || ty == Ity_I16 || ty == Ity_I32 ||
           ty == Ity_I64);

   if (expr->tag == Iex_Load) {
      dst.tag = S390_OPND_AMODE;
      dst.variant.am = s390_isel_amode(env, expr->Iex.Load.addr);
   } else if (expr->tag == Iex_Get) {
      dst.tag = S390_OPND_AMODE;
      dst.variant.am = s390_amode_for_guest_state(expr->Iex.Get.offset);
   } else if (expr->tag == Iex_Const) {
      ULong value;

      switch (expr->Iex.Const.con->tag) {
      case Ico_U1:  value = expr->Iex.Const.con->Ico.U1;  break;
      case Ico_U8:  value = expr->Iex.Const.con->Ico.U8;  break;
      case Ico_U16: value = expr->Iex.Const.con->Ico.U16; break;
      case Ico_U32: value = expr->Iex.Const.con->Ico.U32; break;
      case Ico_U64: value = expr->Iex.Const.con->Ico.U64; break;
      default:
         vpanic("s390_isel_int_expr_RMI");
      }

      dst.tag = S390_OPND_IMMEDIATE;
      dst.variant.imm = value;
   } else {
      dst.tag = S390_OPND_REG;
      dst.variant.reg = s390_isel_int_expr(env, expr);
   }

   return dst;
}


static void
s390_isel_float128_expr_wrk(HReg *dst_hi, HReg *dst_lo, ISelEnv *env,
                            IRExpr *expr)
{
   IRType ty = typeOfIRExpr(env->type_env, expr);

   vassert(ty == Ity_F128);

   switch (expr->tag) {
   case Iex_RdTmp:
      
      lookupIRTemp128(dst_hi, dst_lo, env, expr->Iex.RdTmp.tmp);
      return;

      
   case Iex_Load: {
      IRExpr *addr_hi, *addr_lo;
      s390_amode *am_hi, *am_lo;

      if (expr->Iex.Load.end != Iend_BE)
         goto irreducible;

      addr_hi = expr->Iex.Load.addr;
      addr_lo = IRExpr_Binop(Iop_Add64, addr_hi, mkU64(8));

      am_hi  = s390_isel_amode(env, addr_hi);
      am_lo  = s390_isel_amode(env, addr_lo);

      *dst_hi = newVRegF(env);
      *dst_lo = newVRegF(env);
      addInstr(env, s390_insn_load(8, *dst_hi, am_hi));
      addInstr(env, s390_insn_load(8, *dst_hi, am_lo));
      return;
   }


      
   case Iex_Get:
      vpanic("Iex_Get with F128 data");

      
   case Iex_Qop:
      vpanic("Iex_Qop with F128 data");

      
   case Iex_Triop: {
      IRTriop *triop = expr->Iex.Triop.details;
      IROp    op     = triop->op;
      IRExpr *left   = triop->arg2;
      IRExpr *right  = triop->arg3;
      s390_bfp_binop_t bfpop;
      HReg op1_hi, op1_lo, op2_hi, op2_lo, f12, f13, f14, f15;

      s390_isel_float128_expr(&op1_hi, &op1_lo, env, left);  
      s390_isel_float128_expr(&op2_hi, &op2_lo, env, right); 

      
      f12 = make_fpr(12);
      f13 = make_fpr(13);
      f14 = make_fpr(14);
      f15 = make_fpr(15);

      
      addInstr(env, s390_insn_move(8, f12, op1_hi));
      addInstr(env, s390_insn_move(8, f14, op1_lo));

      
      addInstr(env, s390_insn_move(8, f13, op2_hi));
      addInstr(env, s390_insn_move(8, f15, op2_lo));

      switch (op) {
      case Iop_AddF128: bfpop = S390_BFP_ADD; break;
      case Iop_SubF128: bfpop = S390_BFP_SUB; break;
      case Iop_MulF128: bfpop = S390_BFP_MUL; break;
      case Iop_DivF128: bfpop = S390_BFP_DIV; break;
      default:
         goto irreducible;
      }

      set_bfp_rounding_mode_in_fpc(env, triop->arg1);
      addInstr(env, s390_insn_bfp128_binop(16, bfpop, f12, f14, f13, f15));

      
      *dst_hi = newVRegF(env);
      *dst_lo = newVRegF(env);
      addInstr(env, s390_insn_move(8, *dst_hi, f12));
      addInstr(env, s390_insn_move(8, *dst_lo, f14));

      return;
   }

      
   case Iex_Binop: {
      switch (expr->Iex.Binop.op) {
      case Iop_SqrtF128: {
         HReg op_hi, op_lo, f12, f13, f14, f15;

         
         f12 = make_fpr(12);
         f13 = make_fpr(13);
         f14 = make_fpr(14);
         f15 = make_fpr(15);

         s390_isel_float128_expr(&op_hi, &op_lo, env, expr->Iex.Binop.arg2);

         
         addInstr(env, s390_insn_move(8, f13, op_hi));
         addInstr(env, s390_insn_move(8, f15, op_lo));

         set_bfp_rounding_mode_in_fpc(env, expr->Iex.Binop.arg1);
         addInstr(env, s390_insn_bfp128_unop(16, S390_BFP_SQRT, f12, f14,
                                             f13, f15));

         
         *dst_hi = newVRegF(env);
         *dst_lo = newVRegF(env);
         addInstr(env, s390_insn_move(8, *dst_hi, f12));
         addInstr(env, s390_insn_move(8, *dst_lo, f14));
         return;
      }

      case Iop_F64HLtoF128:
         *dst_hi = s390_isel_float_expr(env, expr->Iex.Binop.arg1);
         *dst_lo = s390_isel_float_expr(env, expr->Iex.Binop.arg2);
         return;

      case Iop_D32toF128:
      case Iop_D64toF128: {
         IRExpr *irrm;
         IRExpr *left;
         s390_dfp_round_t rm;
         HReg h1; 
         HReg f0, f2, f4, r1; 
         s390_fp_conv_t fpconv;

         switch (expr->Iex.Binop.op) {
         case Iop_D32toF128:
            fpconv = S390_FP_D32_TO_F128;
            break;
         case Iop_D64toF128:
            fpconv = S390_FP_D64_TO_F128;
            break;
         default: goto irreducible;
         }

         f4 = make_fpr(4); 
         f0 = make_fpr(0); 
         f2 = make_fpr(2); 
         r1 = make_gpr(1); 
         irrm = expr->Iex.Binop.arg1;
         left = expr->Iex.Binop.arg2;
         rm = get_dfp_rounding_mode(env, irrm);
         h1 = s390_isel_dfp_expr(env, left);
         addInstr(env, s390_insn_move(8, f4, h1));
         addInstr(env, s390_insn_fp128_convert(16, fpconv, f0, f2,
                                               f4, INVALID_HREG, r1, rm));
         
         *dst_hi = newVRegF(env);
         *dst_lo = newVRegF(env);
         addInstr(env, s390_insn_move(8, *dst_hi, f0));
         addInstr(env, s390_insn_move(8, *dst_lo, f2));

         return;
      }

      case Iop_D128toF128: {
         IRExpr *irrm;
         IRExpr *left;
         s390_dfp_round_t rm;
         HReg op_hi, op_lo;
         HReg f0, f2, f4, f6, r1; 

         f4 = make_fpr(4); 
         f6 = make_fpr(6); 
         f0 = make_fpr(0); 
         f2 = make_fpr(2); 
         r1 = make_gpr(1); 

         irrm = expr->Iex.Binop.arg1;
         left = expr->Iex.Binop.arg2;
         rm = get_dfp_rounding_mode(env, irrm);
         s390_isel_dfp128_expr(&op_hi, &op_lo, env, left);
         
         addInstr(env, s390_insn_move(8, f4, op_hi));
         addInstr(env, s390_insn_move(8, f6, op_lo));
         addInstr(env, s390_insn_fp128_convert(16, S390_FP_D128_TO_F128, f0, f2,
                                               f4, f6, r1, rm));
         
         *dst_hi = newVRegF(env);
         *dst_lo = newVRegF(env);
         addInstr(env, s390_insn_move(8, *dst_hi, f0));
         addInstr(env, s390_insn_move(8, *dst_lo, f2));

         return;
      }

      default:
         goto irreducible;
      }
   }

      
   case Iex_Unop: {
      IRExpr *left = expr->Iex.Unop.arg;
      s390_bfp_unop_t bfpop;
      s390_bfp_conv_t conv;
      HReg op_hi, op_lo, op, f12, f13, f14, f15;

      
      f12 = make_fpr(12);
      f13 = make_fpr(13);
      f14 = make_fpr(14);
      f15 = make_fpr(15);

      switch (expr->Iex.Unop.op) {
      case Iop_NegF128:
         if (left->tag == Iex_Unop &&
             (left->Iex.Unop.op == Iop_AbsF32 ||
              left->Iex.Unop.op == Iop_AbsF64))
            bfpop = S390_BFP_NABS;
         else
            bfpop = S390_BFP_NEG;
         goto float128_opnd;
      case Iop_AbsF128:     bfpop = S390_BFP_ABS;         goto float128_opnd;
      case Iop_I32StoF128:  conv = S390_BFP_I32_TO_F128;  goto convert_int;
      case Iop_I64StoF128:  conv = S390_BFP_I64_TO_F128;  goto convert_int;
      case Iop_I32UtoF128:  conv = S390_BFP_U32_TO_F128;  goto convert_int;
      case Iop_I64UtoF128:  conv = S390_BFP_U64_TO_F128;  goto convert_int;
      case Iop_F32toF128:   conv = S390_BFP_F32_TO_F128;  goto convert_float;
      case Iop_F64toF128:   conv = S390_BFP_F64_TO_F128;  goto convert_float;
      default:
         goto irreducible;
      }

   float128_opnd:
      s390_isel_float128_expr(&op_hi, &op_lo, env, left);

      
      addInstr(env, s390_insn_move(8, f13, op_hi));
      addInstr(env, s390_insn_move(8, f15, op_lo));

      addInstr(env, s390_insn_bfp128_unop(16, bfpop, f12, f14, f13, f15));
      goto move_dst;

   convert_float:
      op  = s390_isel_float_expr(env, left);
      addInstr(env, s390_insn_bfp128_convert_to(16, conv, f12, f14, op));
      goto move_dst;

   convert_int:
      op  = s390_isel_int_expr(env, left);
      addInstr(env, s390_insn_bfp128_convert_to(16, conv, f12, f14, op));
      goto move_dst;

   move_dst:
      
      *dst_hi = newVRegF(env);
      *dst_lo = newVRegF(env);
      addInstr(env, s390_insn_move(8, *dst_hi, f12));
      addInstr(env, s390_insn_move(8, *dst_lo, f14));
      return;
   }

   default:
      goto irreducible;
   }

   
 irreducible:
   ppIRExpr(expr);
   vpanic("s390_isel_float128_expr: cannot reduce tree");
}

static void
s390_isel_float128_expr(HReg *dst_hi, HReg *dst_lo, ISelEnv *env, IRExpr *expr)
{
   s390_isel_float128_expr_wrk(dst_hi, dst_lo, env, expr);

   
   vassert(hregIsVirtual(*dst_hi));
   vassert(hregIsVirtual(*dst_lo));
   vassert(hregClass(*dst_hi) == HRcFlt64);
   vassert(hregClass(*dst_lo) == HRcFlt64);
}



static HReg
s390_isel_float_expr_wrk(ISelEnv *env, IRExpr *expr)
{
   IRType ty = typeOfIRExpr(env->type_env, expr);
   UChar size;

   vassert(ty == Ity_F32 || ty == Ity_F64);

   size = sizeofIRType(ty);

   switch (expr->tag) {
   case Iex_RdTmp:
      
      return lookupIRTemp(env, expr->Iex.RdTmp.tmp);

      
   case Iex_Load: {
      HReg        dst = newVRegF(env);
      s390_amode *am  = s390_isel_amode(env, expr->Iex.Load.addr);

      if (expr->Iex.Load.end != Iend_BE)
         goto irreducible;

      addInstr(env, s390_insn_load(size, dst, am));

      return dst;
   }

      
   case Iex_Get: {
      HReg dst = newVRegF(env);
      s390_amode *am = s390_amode_for_guest_state(expr->Iex.Get.offset);

      addInstr(env, s390_insn_load(size, dst, am));

      return dst;
   }

      

   case Iex_Const: {
      ULong value;
      HReg  dst = newVRegF(env);
      const IRConst *con = expr->Iex.Const.con;

      
      switch (con->tag) {
      case Ico_F32i: value = con->Ico.F32i; break;
      case Ico_F64i: value = con->Ico.F64i; break;
      default:       vpanic("s390_isel_float_expr: invalid constant");
      }

      if (value != 0) vpanic("cannot load immediate floating point constant");

      addInstr(env, s390_insn_load_immediate(size, dst, value));

      return dst;
   }

      
   case Iex_Qop: {
      HReg op1, op2, op3, dst;
      s390_bfp_triop_t bfpop;

      op3 = s390_isel_float_expr(env, expr->Iex.Qop.details->arg2);
      op2 = s390_isel_float_expr(env, expr->Iex.Qop.details->arg3);
      op1 = s390_isel_float_expr(env, expr->Iex.Qop.details->arg4);
      dst = newVRegF(env);
      addInstr(env, s390_insn_move(size, dst, op1));

      switch (expr->Iex.Qop.details->op) {
      case Iop_MAddF32:
      case Iop_MAddF64:  bfpop = S390_BFP_MADD; break;
      case Iop_MSubF32:
      case Iop_MSubF64:  bfpop = S390_BFP_MSUB; break;

      default:
         goto irreducible;
      }

      set_bfp_rounding_mode_in_fpc(env, expr->Iex.Qop.details->arg1);
      addInstr(env, s390_insn_bfp_triop(size, bfpop, dst, op2, op3));
      return dst;
   }

      
   case Iex_Triop: {
      IRTriop *triop = expr->Iex.Triop.details;
      IROp    op     = triop->op;
      IRExpr *left   = triop->arg2;
      IRExpr *right  = triop->arg3;
      s390_bfp_binop_t bfpop;
      HReg h1, op2, dst;

      h1   = s390_isel_float_expr(env, left);  
      op2  = s390_isel_float_expr(env, right); 
      dst  = newVRegF(env);
      addInstr(env, s390_insn_move(size, dst, h1));
      switch (op) {
      case Iop_AddF32:
      case Iop_AddF64:  bfpop = S390_BFP_ADD; break;
      case Iop_SubF32:
      case Iop_SubF64:  bfpop = S390_BFP_SUB; break;
      case Iop_MulF32:
      case Iop_MulF64:  bfpop = S390_BFP_MUL; break;
      case Iop_DivF32:
      case Iop_DivF64:  bfpop = S390_BFP_DIV; break;

      default:
         goto irreducible;
      }

      set_bfp_rounding_mode_in_fpc(env, triop->arg1);
      addInstr(env, s390_insn_bfp_binop(size, bfpop, dst, op2));
      return dst;
   }

      
   case Iex_Binop: {
      IROp    op   = expr->Iex.Binop.op;
      IRExpr *irrm = expr->Iex.Binop.arg1;
      IRExpr *left = expr->Iex.Binop.arg2;
      HReg h1, dst;
      s390_bfp_conv_t  conv;
      s390_fp_conv_t fpconv;

      switch (op) {
      case Iop_SqrtF32:
      case Iop_SqrtF64:
         h1  = s390_isel_float_expr(env, left);
         dst = newVRegF(env);
         set_bfp_rounding_mode_in_fpc(env, irrm);
         addInstr(env, s390_insn_bfp_unop(size, S390_BFP_SQRT, dst, h1));
         return dst;

      case Iop_F64toF32:  conv = S390_BFP_F64_TO_F32; goto convert_float;
      case Iop_I32StoF32: conv = S390_BFP_I32_TO_F32; goto convert_int;
      case Iop_I32UtoF32: conv = S390_BFP_U32_TO_F32; goto convert_int;
      case Iop_I64StoF32: conv = S390_BFP_I64_TO_F32; goto convert_int;
      case Iop_I64StoF64: conv = S390_BFP_I64_TO_F64; goto convert_int;
      case Iop_I64UtoF32: conv = S390_BFP_U64_TO_F32; goto convert_int;
      case Iop_I64UtoF64: conv = S390_BFP_U64_TO_F64; goto convert_int;
      case Iop_D32toF32:  fpconv = S390_FP_D32_TO_F32;  goto convert_dfp;
      case Iop_D32toF64:  fpconv = S390_FP_D32_TO_F64;  goto convert_dfp;
      case Iop_D64toF32:  fpconv = S390_FP_D64_TO_F32;  goto convert_dfp;
      case Iop_D64toF64:  fpconv = S390_FP_D64_TO_F64;  goto convert_dfp;
      case Iop_D128toF32: fpconv = S390_FP_D128_TO_F32; goto convert_dfp128;
      case Iop_D128toF64: fpconv = S390_FP_D128_TO_F64; goto convert_dfp128;

      convert_float:
         h1 = s390_isel_float_expr(env, left);
         goto convert;

      convert_int:
         h1 = s390_isel_int_expr(env, left);
         goto convert;

      convert: {
         s390_bfp_round_t rounding_mode;
         dst = newVRegF(env);
         if (s390_host_has_fpext) {
            rounding_mode = get_bfp_rounding_mode(env, irrm);
         } else {
            set_bfp_rounding_mode_in_fpc(env, irrm);
            rounding_mode = S390_BFP_ROUND_PER_FPC;
         }
         addInstr(env, s390_insn_bfp_convert(size, conv, dst, h1,
                                             rounding_mode));
         return dst;
      }

      convert_dfp: {
         s390_dfp_round_t rm;
         HReg f0, f4, r1; 

         f4 = make_fpr(4); 
         f0 = make_fpr(0); 
         r1 = make_gpr(1); 
         h1 = s390_isel_dfp_expr(env, left);
         dst = newVRegF(env);
         rm = get_dfp_rounding_mode(env, irrm);
         
         addInstr(env, s390_insn_move(8, f4, h1));
         addInstr(env, s390_insn_fp_convert(size, fpconv, f0, f4, r1, rm));
         
         addInstr(env, s390_insn_move(8, dst, f0));
         return dst;
      }

      convert_dfp128: {
         s390_dfp_round_t rm;
         HReg op_hi, op_lo;
         HReg f0, f4, f6, r1; 

         f4 = make_fpr(4); 
         f6 = make_fpr(6); 
         f0 = make_fpr(0); 
         r1 = make_gpr(1); 
         s390_isel_dfp128_expr(&op_hi, &op_lo, env, left);
         dst = newVRegF(env);
         rm = get_dfp_rounding_mode(env, irrm);
         
         addInstr(env, s390_insn_move(8, f4, op_hi));
         addInstr(env, s390_insn_move(8, f6, op_lo));
         addInstr(env, s390_insn_fp128_convert(16, fpconv, f0, INVALID_HREG,
                                               f4, f6, r1, rm));
         
         addInstr(env, s390_insn_move(8, dst, f0));
         return dst;
      }

      default:
         goto irreducible;

      case Iop_F128toF64:
      case Iop_F128toF32: {
         HReg op_hi, op_lo, f12, f13, f14, f15;
         s390_bfp_round_t rounding_mode;

         conv = op == Iop_F128toF32 ? S390_BFP_F128_TO_F32
                                    : S390_BFP_F128_TO_F64;

         s390_isel_float128_expr(&op_hi, &op_lo, env, left);

         
         f12 = make_fpr(12);
         f13 = make_fpr(13);
         f14 = make_fpr(14);
         f15 = make_fpr(15);

         
         addInstr(env, s390_insn_move(8, f13, op_hi));
         addInstr(env, s390_insn_move(8, f15, op_lo));

         

         if (s390_host_has_fpext) {
            rounding_mode = get_bfp_rounding_mode(env, irrm);
         } else {
            set_bfp_rounding_mode_in_fpc(env, irrm);
            rounding_mode = S390_BFP_ROUND_PER_FPC;
         }

         addInstr(env, s390_insn_bfp128_convert_from(size, conv, f12, f14,
                                                     f13, f15, rounding_mode));
         dst = newVRegF(env);
         addInstr(env, s390_insn_move(8, dst, f12));

         return dst;
      }
      }
   }

      
   case Iex_Unop: {
      IROp    op   = expr->Iex.Unop.op;
      IRExpr *left = expr->Iex.Unop.arg;
      s390_bfp_unop_t bfpop;
      s390_bfp_conv_t conv;
      HReg h1, dst;

      if (op == Iop_F128HItoF64 || op == Iop_F128LOtoF64) {
         HReg dst_hi, dst_lo;

         s390_isel_float128_expr(&dst_hi, &dst_lo, env, left);
         return op == Iop_F128LOtoF64 ? dst_lo : dst_hi;
      }

      if (op == Iop_ReinterpI64asF64 || op == Iop_ReinterpI32asF32) {
         dst = newVRegF(env);
         h1  = s390_isel_int_expr(env, left);     
         addInstr(env, s390_insn_move(size, dst, h1));

         return dst;
      }

      switch (op) {
      case Iop_NegF32:
      case Iop_NegF64:
         if (left->tag == Iex_Unop &&
             (left->Iex.Unop.op == Iop_AbsF32 ||
              left->Iex.Unop.op == Iop_AbsF64))
            bfpop = S390_BFP_NABS;
         else
            bfpop = S390_BFP_NEG;
         break;

      case Iop_AbsF32:
      case Iop_AbsF64:
         bfpop = S390_BFP_ABS;
         break;

      case Iop_I32StoF64:  conv = S390_BFP_I32_TO_F64;  goto convert_int1;
      case Iop_I32UtoF64:  conv = S390_BFP_U32_TO_F64;  goto convert_int1;
      case Iop_F32toF64:   conv = S390_BFP_F32_TO_F64;  goto convert_float1;

      convert_float1:
         h1 = s390_isel_float_expr(env, left);
         goto convert1;

      convert_int1:
         h1 = s390_isel_int_expr(env, left);
         goto convert1;

      convert1:
         dst = newVRegF(env);
         addInstr(env, s390_insn_bfp_convert(size, conv, dst, h1,
                                             S390_BFP_ROUND_NEAREST_EVEN));
         return dst;

      default:
         goto irreducible;
      }

      
      h1  = s390_isel_float_expr(env, left);
      dst = newVRegF(env);
      addInstr(env, s390_insn_bfp_unop(size, bfpop, dst, h1));
      return dst;
   }

   default:
      goto irreducible;
   }

   
 irreducible:
   ppIRExpr(expr);
   vpanic("s390_isel_float_expr: cannot reduce tree");
}


static HReg
s390_isel_float_expr(ISelEnv *env, IRExpr *expr)
{
   HReg dst = s390_isel_float_expr_wrk(env, expr);

   
   vassert(hregClass(dst) == HRcFlt64);
   vassert(hregIsVirtual(dst));

   return dst;
}


static void
s390_isel_dfp128_expr_wrk(HReg *dst_hi, HReg *dst_lo, ISelEnv *env,
                          IRExpr *expr)
{
   IRType ty = typeOfIRExpr(env->type_env, expr);

   vassert(ty == Ity_D128);

   switch (expr->tag) {
   case Iex_RdTmp:
      
      lookupIRTemp128(dst_hi, dst_lo, env, expr->Iex.RdTmp.tmp);
      return;

      
   case Iex_Load: {
      IRExpr *addr_hi, *addr_lo;
      s390_amode *am_hi, *am_lo;

      if (expr->Iex.Load.end != Iend_BE)
         goto irreducible;

      addr_hi = expr->Iex.Load.addr;
      addr_lo = IRExpr_Binop(Iop_Add64, addr_hi, mkU64(8));

      am_hi  = s390_isel_amode(env, addr_hi);
      am_lo  = s390_isel_amode(env, addr_lo);

      *dst_hi = newVRegF(env);
      *dst_lo = newVRegF(env);
      addInstr(env, s390_insn_load(8, *dst_hi, am_hi));
      addInstr(env, s390_insn_load(8, *dst_hi, am_lo));
      return;
   }

      
   case Iex_Get:
      vpanic("Iex_Get with D128 data");

      
   case Iex_Qop:
      vpanic("Iex_Qop with D128 data");

      
   case Iex_Triop: {
      IRTriop *triop = expr->Iex.Triop.details;
      IROp    op     = triop->op;
      IRExpr *irrm   = triop->arg1;
      IRExpr *left   = triop->arg2;
      IRExpr *right  = triop->arg3;
      s390_dfp_round_t rounding_mode;
      s390_dfp_binop_t dfpop;
      HReg op1_hi, op1_lo, op2_hi, op2_lo, f9, f11, f12, f13, f14, f15;

      f9  = make_fpr(9);
      f11 = make_fpr(11);
      f12 = make_fpr(12);
      f13 = make_fpr(13);
      f14 = make_fpr(14);
      f15 = make_fpr(15);

      switch (op) {
      case Iop_AddD128:       dfpop = S390_DFP_ADD;      goto evaluate_dfp128;
      case Iop_SubD128:       dfpop = S390_DFP_SUB;      goto evaluate_dfp128;
      case Iop_MulD128:       dfpop = S390_DFP_MUL;      goto evaluate_dfp128;
      case Iop_DivD128:       dfpop = S390_DFP_DIV;      goto evaluate_dfp128;
      case Iop_QuantizeD128:  dfpop = S390_DFP_QUANTIZE; goto evaluate_dfp128;

      evaluate_dfp128: {
         
         s390_isel_dfp128_expr(&op1_hi, &op1_lo, env, left);
         
         addInstr(env, s390_insn_move(8, f9,  op1_hi));
         addInstr(env, s390_insn_move(8, f11, op1_lo));

         
         s390_isel_dfp128_expr(&op2_hi, &op2_lo, env, right);
         
         addInstr(env, s390_insn_move(8, f12, op2_hi));
         addInstr(env, s390_insn_move(8, f14, op2_lo));

         if (s390_host_has_fpext || op == Iop_QuantizeD128) {
            rounding_mode = get_dfp_rounding_mode(env, irrm);
         } else {
            set_dfp_rounding_mode_in_fpc(env, irrm);
            rounding_mode = S390_DFP_ROUND_PER_FPC_0;
         }
         addInstr(env, s390_insn_dfp128_binop(16, dfpop, f13, f15, f9, f11,
                                              f12, f14, rounding_mode));
         
         *dst_hi = newVRegF(env);
         *dst_lo = newVRegF(env);
         addInstr(env, s390_insn_move(8, *dst_hi, f13));
         addInstr(env, s390_insn_move(8, *dst_lo, f15));
         return;
      }

      case Iop_SignificanceRoundD128: {
         
         HReg op1 = s390_isel_int_expr(env, left);
         
         s390_isel_dfp128_expr(&op2_hi, &op2_lo, env, right);
         
         addInstr(env, s390_insn_move(8, f12, op2_hi));
         addInstr(env, s390_insn_move(8, f14, op2_lo));

         rounding_mode = get_dfp_rounding_mode(env, irrm);
         addInstr(env, s390_insn_dfp128_reround(16, f13, f15, op1, f12, f14,
                                                rounding_mode));
         
         *dst_hi = newVRegF(env);
         *dst_lo = newVRegF(env);
         addInstr(env, s390_insn_move(8, *dst_hi, f13));
         addInstr(env, s390_insn_move(8, *dst_lo, f15));
         return;
      }

      default:
         goto irreducible;
      }
   }

      
   case Iex_Binop: {

      switch (expr->Iex.Binop.op) {
      case Iop_D64HLtoD128:
         *dst_hi = s390_isel_dfp_expr(env, expr->Iex.Binop.arg1);
         *dst_lo = s390_isel_dfp_expr(env, expr->Iex.Binop.arg2);
         return;

      case Iop_ShlD128:
      case Iop_ShrD128:
      case Iop_InsertExpD128: {
         HReg op1_hi, op1_lo, op2, f9, f11, f13, f15;
         s390_dfp_intop_t intop;
         IRExpr *dfp_op;
         IRExpr *int_op;

         switch (expr->Iex.Binop.op) {
         case Iop_ShlD128:       
            intop = S390_DFP_SHIFT_LEFT;
            dfp_op = expr->Iex.Binop.arg1;
            int_op = expr->Iex.Binop.arg2;
            break;
         case Iop_ShrD128:       
            intop = S390_DFP_SHIFT_RIGHT;
            dfp_op = expr->Iex.Binop.arg1;
            int_op = expr->Iex.Binop.arg2;
            break;
         case Iop_InsertExpD128: 
            intop = S390_DFP_INSERT_EXP;
            int_op = expr->Iex.Binop.arg1;
            dfp_op = expr->Iex.Binop.arg2;
            break;
         default: goto irreducible;
         }

         
         f9  = make_fpr(9); 
         f11 = make_fpr(11);

         f13 = make_fpr(13); 
         f15 = make_fpr(15);

         
         s390_isel_dfp128_expr(&op1_hi, &op1_lo, env, dfp_op);
         
         addInstr(env, s390_insn_move(8, f9,  op1_hi));
         addInstr(env, s390_insn_move(8, f11, op1_lo));

         op2 = s390_isel_int_expr(env, int_op);  

         addInstr(env,
                  s390_insn_dfp128_intop(16, intop, f13, f15, op2, f9, f11));

         
         *dst_hi = newVRegF(env);
         *dst_lo = newVRegF(env);
         addInstr(env, s390_insn_move(8, *dst_hi, f13));
         addInstr(env, s390_insn_move(8, *dst_lo, f15));
         return;
      }

      case Iop_F32toD128:
      case Iop_F64toD128: {
         IRExpr *irrm;
         IRExpr *left;
         s390_dfp_round_t rm;
         HReg h1; 
         HReg f0, f2, f4, r1; 
         s390_fp_conv_t fpconv;

         switch (expr->Iex.Binop.op) {
         case Iop_F32toD128:       
            fpconv = S390_FP_F32_TO_D128;
            break;
         case Iop_F64toD128:       
            fpconv = S390_FP_F64_TO_D128;
            break;
         default: goto irreducible;
         }

         f4 = make_fpr(4); 
         f0 = make_fpr(0); 
         f2 = make_fpr(2); 
         r1 = make_gpr(1); 
         irrm = expr->Iex.Binop.arg1;
         left = expr->Iex.Binop.arg2;
         rm = get_dfp_rounding_mode(env, irrm);
         h1 = s390_isel_float_expr(env, left);
         addInstr(env, s390_insn_move(8, f4, h1));
         addInstr(env, s390_insn_fp128_convert(16, fpconv, f0, f2,
                                               f4, INVALID_HREG, r1, rm));
         
         *dst_hi = newVRegF(env);
         *dst_lo = newVRegF(env);
         addInstr(env, s390_insn_move(8, *dst_hi, f0));
         addInstr(env, s390_insn_move(8, *dst_lo, f2));

         return;
      }

      case Iop_F128toD128: {
         IRExpr *irrm;
         IRExpr *left;
         s390_dfp_round_t rm;
         HReg op_hi, op_lo;
         HReg f0, f2, f4, f6, r1; 

         f4 = make_fpr(4); 
         f6 = make_fpr(6); 
         f0 = make_fpr(0); 
         f2 = make_fpr(2); 
         r1 = make_gpr(1); 

         irrm = expr->Iex.Binop.arg1;
         left = expr->Iex.Binop.arg2;
         rm = get_dfp_rounding_mode(env, irrm);
         s390_isel_float128_expr(&op_hi, &op_lo, env, left);
         
         addInstr(env, s390_insn_move(8, f4, op_hi));
         addInstr(env, s390_insn_move(8, f6, op_lo));
         addInstr(env, s390_insn_fp128_convert(16, S390_FP_F128_TO_D128, f0, f2,
                                               f4, f6, r1, rm));
         
         *dst_hi = newVRegF(env);
         *dst_lo = newVRegF(env);
         addInstr(env, s390_insn_move(8, *dst_hi, f0));
         addInstr(env, s390_insn_move(8, *dst_lo, f2));

         return;
      }

      default:
         goto irreducible;
      }
   }

      
   case Iex_Unop: {
      IRExpr *left = expr->Iex.Unop.arg;
      s390_dfp_conv_t conv;
      HReg op, f12, f14;

      
      f12 = make_fpr(12);
      f14 = make_fpr(14);

      switch (expr->Iex.Unop.op) {
      case Iop_D64toD128:   conv = S390_DFP_D64_TO_D128;  goto convert_dfp;
      case Iop_I32StoD128:  conv = S390_DFP_I32_TO_D128;  goto convert_int;
      case Iop_I64StoD128:  conv = S390_DFP_I64_TO_D128;  goto convert_int;
      case Iop_I32UtoD128:  conv = S390_DFP_U32_TO_D128;  goto convert_int;
      case Iop_I64UtoD128:  conv = S390_DFP_U64_TO_D128;  goto convert_int;
      default:
         goto irreducible;
      }

   convert_dfp:
      op  = s390_isel_dfp_expr(env, left);
      addInstr(env, s390_insn_dfp128_convert_to(16, conv, f12, f14, op));
      goto move_dst;

   convert_int:
      op  = s390_isel_int_expr(env, left);
      addInstr(env, s390_insn_dfp128_convert_to(16, conv, f12, f14, op));
      goto move_dst;

   move_dst:
      
      *dst_hi = newVRegF(env);
      *dst_lo = newVRegF(env);
      addInstr(env, s390_insn_move(8, *dst_hi, f12));
      addInstr(env, s390_insn_move(8, *dst_lo, f14));
      return;
   }

   default:
      goto irreducible;
   }

   
 irreducible:
   ppIRExpr(expr);
   vpanic("s390_isel_dfp128_expr_wrk: cannot reduce tree");

}


static void
s390_isel_dfp128_expr(HReg *dst_hi, HReg *dst_lo, ISelEnv *env, IRExpr *expr)
{
   s390_isel_dfp128_expr_wrk(dst_hi, dst_lo, env, expr);

   
   vassert(hregIsVirtual(*dst_hi));
   vassert(hregIsVirtual(*dst_lo));
   vassert(hregClass(*dst_hi) == HRcFlt64);
   vassert(hregClass(*dst_lo) == HRcFlt64);
}



static HReg
s390_isel_dfp_expr_wrk(ISelEnv *env, IRExpr *expr)
{
   IRType ty = typeOfIRExpr(env->type_env, expr);
   UChar size;

   vassert(ty == Ity_D64 || ty == Ity_D32);

   size = sizeofIRType(ty);

   switch (expr->tag) {
   case Iex_RdTmp:
      
      return lookupIRTemp(env, expr->Iex.RdTmp.tmp);

      
   case Iex_Load: {
      HReg        dst = newVRegF(env);
      s390_amode *am  = s390_isel_amode(env, expr->Iex.Load.addr);

      if (expr->Iex.Load.end != Iend_BE)
         goto irreducible;

      addInstr(env, s390_insn_load(size, dst, am));

      return dst;
   }

      
   case Iex_Get: {
      HReg dst = newVRegF(env);
      s390_amode *am = s390_amode_for_guest_state(expr->Iex.Get.offset);

      addInstr(env, s390_insn_load(size, dst, am));

      return dst;
   }

      
   case Iex_Binop: {
      IROp    op   = expr->Iex.Binop.op;
      IRExpr *irrm = expr->Iex.Binop.arg1;
      IRExpr *left = expr->Iex.Binop.arg2;
      HReg h1, dst;
      s390_dfp_conv_t  conv;
      s390_fp_conv_t  fpconv;

      switch (op) {
      case Iop_D64toD32:  conv = S390_DFP_D64_TO_D32; goto convert_dfp;
      case Iop_I64StoD64: conv = S390_DFP_I64_TO_D64; goto convert_int;
      case Iop_I64UtoD64: conv = S390_DFP_U64_TO_D64; goto convert_int;
      case Iop_F32toD32:  fpconv = S390_FP_F32_TO_D32; goto convert_bfp;
      case Iop_F32toD64:  fpconv = S390_FP_F32_TO_D64; goto convert_bfp;
      case Iop_F64toD32:  fpconv = S390_FP_F64_TO_D32; goto convert_bfp;
      case Iop_F64toD64:  fpconv = S390_FP_F64_TO_D64; goto convert_bfp;
      case Iop_F128toD32: fpconv = S390_FP_F128_TO_D32; goto convert_bfp128;
      case Iop_F128toD64: fpconv = S390_FP_F128_TO_D64; goto convert_bfp128;

      convert_dfp:
         h1 = s390_isel_dfp_expr(env, left);
         goto convert;

      convert_int:
         h1 = s390_isel_int_expr(env, left);
         goto convert;

      convert: {
            s390_dfp_round_t rounding_mode;
            dst = newVRegF(env);
            if (s390_host_has_fpext) {
               rounding_mode = get_dfp_rounding_mode(env, irrm);
            } else {
               set_dfp_rounding_mode_in_fpc(env, irrm);
               rounding_mode = S390_DFP_ROUND_PER_FPC_0;
            }
            addInstr(env, s390_insn_dfp_convert(size, conv, dst, h1,
                                                rounding_mode));
            return dst;
         }

      convert_bfp: {
         s390_dfp_round_t rm;
         HReg f0, f4, r1; 

         f4 = make_fpr(4); 
         f0 = make_fpr(0); 
         r1 = make_gpr(1); 
         h1 = s390_isel_float_expr(env, left);
         dst = newVRegF(env);
         rm = get_dfp_rounding_mode(env, irrm);
         
         addInstr(env, s390_insn_move(8, f4, h1));
         addInstr(env, s390_insn_fp_convert(size, fpconv, f0, f4, r1, rm));
         
         addInstr(env, s390_insn_move(8, dst, f0));
         return dst;
      }

      convert_bfp128: {
         s390_dfp_round_t rm;
         HReg op_hi, op_lo;
         HReg f0, f4, f6, r1; 

         f4 = make_fpr(4); 
         f6 = make_fpr(6); 
         f0 = make_fpr(0); 
         r1 = make_gpr(1); 
         s390_isel_float128_expr(&op_hi, &op_lo, env, left);
         dst = newVRegF(env);
         rm = get_dfp_rounding_mode(env, irrm);
         
         addInstr(env, s390_insn_move(8, f4, op_hi));
         addInstr(env, s390_insn_move(8, f6, op_lo));
         addInstr(env, s390_insn_fp128_convert(16, fpconv, f0, INVALID_HREG,
                                               f4, f6, r1, rm));
         
         addInstr(env, s390_insn_move(8, dst, f0));
         return dst;
      }

      case Iop_D128toD64: {
         HReg op_hi, op_lo, f12, f13, f14, f15;
         s390_dfp_round_t rounding_mode;

         conv = S390_DFP_D128_TO_D64;

         s390_isel_dfp128_expr(&op_hi, &op_lo, env, left);

         
         f12 = make_fpr(12);
         f13 = make_fpr(13);
         f14 = make_fpr(14);
         f15 = make_fpr(15);

         
         addInstr(env, s390_insn_move(8, f13, op_hi));
         addInstr(env, s390_insn_move(8, f15, op_lo));

         
 
         if (s390_host_has_fpext) {
            rounding_mode = get_dfp_rounding_mode(env, irrm);
         } else {
            set_dfp_rounding_mode_in_fpc(env, irrm);
            rounding_mode = S390_DFP_ROUND_PER_FPC_0;
         }
         addInstr(env, s390_insn_dfp128_convert_from(size, conv, f12, f14,
                                                     f13, f15, rounding_mode));
         dst = newVRegF(env);
         addInstr(env, s390_insn_move(8, dst, f12));

         return dst;
      }

      case Iop_ShlD64:
      case Iop_ShrD64:
      case Iop_InsertExpD64: {
         HReg op2;
         HReg op3;
         IRExpr *dfp_op;
         IRExpr *int_op;
         s390_dfp_intop_t intop;

         switch (expr->Iex.Binop.op) {
         case Iop_ShlD64:       
            intop = S390_DFP_SHIFT_LEFT;
            dfp_op = expr->Iex.Binop.arg1;
            int_op = expr->Iex.Binop.arg2;
            break;
         case Iop_ShrD64:       
            intop = S390_DFP_SHIFT_RIGHT;
            dfp_op = expr->Iex.Binop.arg1;
            int_op = expr->Iex.Binop.arg2;
            break;
         case Iop_InsertExpD64: 
            intop = S390_DFP_INSERT_EXP;
            int_op = expr->Iex.Binop.arg1;
            dfp_op = expr->Iex.Binop.arg2;
            break;
         default: goto irreducible;
         }

         op2 = s390_isel_int_expr(env, int_op);
         op3 = s390_isel_dfp_expr(env, dfp_op);
         dst = newVRegF(env);

         addInstr(env, s390_insn_dfp_intop(size, intop, dst, op2, op3));
         return dst;
      }

      default:
         goto irreducible;
      }
   }

      
   case Iex_Unop: {
      IROp    op   = expr->Iex.Unop.op;
      IRExpr *left = expr->Iex.Unop.arg;
      s390_dfp_conv_t conv;
      HReg h1, dst;

      if (op == Iop_D128HItoD64 || op == Iop_D128LOtoD64) {
         HReg dst_hi, dst_lo;

         s390_isel_dfp128_expr(&dst_hi, &dst_lo, env, left);
         return op == Iop_D128LOtoD64 ? dst_lo : dst_hi;
      }

      if (op == Iop_ReinterpI64asD64) {
         dst = newVRegF(env);
         h1  = s390_isel_int_expr(env, left);     
         addInstr(env, s390_insn_move(size, dst, h1));

         return dst;
      }

      switch (op) {
      case Iop_D32toD64:  conv = S390_DFP_D32_TO_D64;  goto convert_dfp1;
      case Iop_I32StoD64: conv = S390_DFP_I32_TO_D64;  goto convert_int1;
      case Iop_I32UtoD64: conv = S390_DFP_U32_TO_D64;  goto convert_int1;

      convert_dfp1:
         h1 = s390_isel_dfp_expr(env, left);
         goto convert1;

      convert_int1:
         h1 = s390_isel_int_expr(env, left);
         goto convert1;

      convert1:
         dst = newVRegF(env);
         addInstr(env, s390_insn_dfp_convert(size, conv, dst, h1,
                                             S390_DFP_ROUND_NEAREST_EVEN_4));
         return dst;

      default:
         goto irreducible;
      }
   }

      
   case Iex_Triop: {
      IRTriop *triop = expr->Iex.Triop.details;
      IROp    op     = triop->op;
      IRExpr *irrm   = triop->arg1;
      IRExpr *left   = triop->arg2;
      IRExpr *right  = triop->arg3;
      s390_dfp_round_t rounding_mode;
      s390_dfp_binop_t dfpop;
      HReg op2, op3, dst;

      switch (op) {
      case Iop_AddD64:      dfpop = S390_DFP_ADD;      goto evaluate_dfp;
      case Iop_SubD64:      dfpop = S390_DFP_SUB;      goto evaluate_dfp;
      case Iop_MulD64:      dfpop = S390_DFP_MUL;      goto evaluate_dfp;
      case Iop_DivD64:      dfpop = S390_DFP_DIV;      goto evaluate_dfp;
      case Iop_QuantizeD64: dfpop = S390_DFP_QUANTIZE; goto evaluate_dfp;

      evaluate_dfp: {
         op2  = s390_isel_dfp_expr(env, left);  
         op3  = s390_isel_dfp_expr(env, right); 
         dst  = newVRegF(env);
         if (s390_host_has_fpext || dfpop == S390_DFP_QUANTIZE) {
            rounding_mode = get_dfp_rounding_mode(env, irrm);
         } else {
            set_dfp_rounding_mode_in_fpc(env, irrm);
            rounding_mode = S390_DFP_ROUND_PER_FPC_0;
         }
         addInstr(env, s390_insn_dfp_binop(size, dfpop, dst, op2, op3,
                                           rounding_mode));
         return dst;
      }

      case Iop_SignificanceRoundD64:
         op2  = s390_isel_int_expr(env, left);  
         op3  = s390_isel_dfp_expr(env, right); 
         dst  = newVRegF(env);
         rounding_mode = get_dfp_rounding_mode(env, irrm);
         addInstr(env, s390_insn_dfp_reround(size, dst, op2, op3,
                                             rounding_mode));
         return dst;

      default:
         goto irreducible;
      }
   }

   default:
      goto irreducible;
   }

   
 irreducible:
   ppIRExpr(expr);
   vpanic("s390_isel_dfp_expr: cannot reduce tree");
}

static HReg
s390_isel_dfp_expr(ISelEnv *env, IRExpr *expr)
{
   HReg dst = s390_isel_dfp_expr_wrk(env, expr);

   
   vassert(hregClass(dst) == HRcFlt64);
   vassert(hregIsVirtual(dst));

   return dst;
}



static s390_cc_t
s390_isel_cc(ISelEnv *env, IRExpr *cond)
{
   UChar size;

   vassert(typeOfIRExpr(env->type_env, cond) == Ity_I1);

   
   if (cond->tag == Iex_Const) {
      vassert(cond->Iex.Const.con->tag == Ico_U1);
      vassert(cond->Iex.Const.con->Ico.U1 == True
              || cond->Iex.Const.con->Ico.U1 == False);

      return cond->Iex.Const.con->Ico.U1 == True ? S390_CC_ALWAYS : S390_CC_NEVER;
   }

   
   if (cond->tag == Iex_RdTmp) {
      IRTemp tmp = cond->Iex.RdTmp.tmp;
      HReg   reg = lookupIRTemp(env, tmp);

      
      if (typeOfIRTemp(env->type_env, tmp) == Ity_I1)
         size = 4;
      else
         size = sizeofIRType(typeOfIRTemp(env->type_env, tmp));
      addInstr(env, s390_insn_test(size, s390_opnd_reg(reg)));
      return S390_CC_NE;
   }

   
   if (cond->tag == Iex_Unop) {
      IRExpr *arg = cond->Iex.Unop.arg;

      switch (cond->Iex.Unop.op) {
      case Iop_Not1:  
         
         return s390_cc_invert(s390_isel_cc(env, arg));

         
      case Iop_32to1:
      case Iop_64to1: {
         HReg dst = newVRegI(env);
         HReg h1  = s390_isel_int_expr(env, arg);

         size = sizeofIRType(typeOfIRExpr(env->type_env, arg));

         addInstr(env, s390_insn_move(size, dst, h1));
         addInstr(env, s390_insn_alu(size, S390_ALU_AND, dst, s390_opnd_imm(1)));
         addInstr(env, s390_insn_test(size, s390_opnd_reg(dst)));
         return S390_CC_NE;
      }

      case Iop_CmpNEZ8:
      case Iop_CmpNEZ16: {
         s390_opnd_RMI src;
         s390_unop_t   op;
         HReg dst;

         op  = (cond->Iex.Unop.op == Iop_CmpNEZ8) ? S390_ZERO_EXTEND_8
            : S390_ZERO_EXTEND_16;
         dst = newVRegI(env);
         src = s390_isel_int_expr_RMI(env, arg);
         addInstr(env, s390_insn_unop(4, op, dst, src));
         addInstr(env, s390_insn_test(4, s390_opnd_reg(dst)));
         return S390_CC_NE;
      }

      case Iop_CmpNEZ32:
      case Iop_CmpNEZ64: {
         s390_opnd_RMI src;

         src = s390_isel_int_expr_RMI(env, arg);
         size = sizeofIRType(typeOfIRExpr(env->type_env, arg));
         addInstr(env, s390_insn_test(size, src));
         return S390_CC_NE;
      }

      default:
         goto fail;
      }
   }

   
   if (cond->tag == Iex_Binop) {
      IRExpr *arg1 = cond->Iex.Binop.arg1;
      IRExpr *arg2 = cond->Iex.Binop.arg2;
      HReg reg1, reg2;

      size = sizeofIRType(typeOfIRExpr(env->type_env, arg1));

      switch (cond->Iex.Binop.op) {
         s390_unop_t op;
         s390_cc_t   result;

      case Iop_CmpEQ8:
      case Iop_CasCmpEQ8:
         op     = S390_ZERO_EXTEND_8;
         result = S390_CC_E;
         goto do_compare_ze;

      case Iop_CmpNE8:
      case Iop_CasCmpNE8:
         op     = S390_ZERO_EXTEND_8;
         result = S390_CC_NE;
         goto do_compare_ze;

      case Iop_CmpEQ16:
      case Iop_CasCmpEQ16:
         op     = S390_ZERO_EXTEND_16;
         result = S390_CC_E;
         goto do_compare_ze;

      case Iop_CmpNE16:
      case Iop_CasCmpNE16:
         op     = S390_ZERO_EXTEND_16;
         result = S390_CC_NE;
         goto do_compare_ze;

      do_compare_ze: {
            s390_opnd_RMI op1, op2;

            op1  = s390_isel_int_expr_RMI(env, arg1);
            reg1 = newVRegI(env);
            addInstr(env, s390_insn_unop(4, op, reg1, op1));

            op2  = s390_isel_int_expr_RMI(env, arg2);
            reg2 = newVRegI(env);
            addInstr(env, s390_insn_unop(4, op, reg2, op2));  

            op2 = s390_opnd_reg(reg2);
            addInstr(env, s390_insn_compare(4, reg1, op2, False));

            return result;
         }

      case Iop_CmpEQ32:
      case Iop_CmpEQ64:
      case Iop_CasCmpEQ32:
      case Iop_CasCmpEQ64:
         result = S390_CC_E;
         goto do_compare;

      case Iop_CmpNE32:
      case Iop_CmpNE64:
      case Iop_CasCmpNE32:
      case Iop_CasCmpNE64:
         result = S390_CC_NE;
         goto do_compare;

      do_compare: {
            HReg op1;
            s390_opnd_RMI op2;

            order_commutative_operands(arg1, arg2);

            op1 = s390_isel_int_expr(env, arg1);
            op2 = s390_isel_int_expr_RMI(env, arg2);

            addInstr(env, s390_insn_compare(size, op1, op2, False));

            return result;
         }

      case Iop_CmpLT32S:
      case Iop_CmpLE32S:
      case Iop_CmpLT64S:
      case Iop_CmpLE64S: {
         HReg op1;
         s390_opnd_RMI op2;

         op1 = s390_isel_int_expr(env, arg1);
         op2 = s390_isel_int_expr_RMI(env, arg2);

         addInstr(env, s390_insn_compare(size, op1, op2, True));

         return (cond->Iex.Binop.op == Iop_CmpLT32S ||
                 cond->Iex.Binop.op == Iop_CmpLT64S) ? S390_CC_L : S390_CC_LE;
      }

      case Iop_CmpLT32U:
      case Iop_CmpLE32U:
      case Iop_CmpLT64U:
      case Iop_CmpLE64U: {
         HReg op1;
         s390_opnd_RMI op2;

         op1 = s390_isel_int_expr(env, arg1);
         op2 = s390_isel_int_expr_RMI(env, arg2);

         addInstr(env, s390_insn_compare(size, op1, op2, False));

         return (cond->Iex.Binop.op == Iop_CmpLT32U ||
                 cond->Iex.Binop.op == Iop_CmpLT64U) ? S390_CC_L : S390_CC_LE;
      }

      default:
         goto fail;
      }
   }

 fail:
   ppIRExpr(cond);
   vpanic("s390_isel_cc: unexpected operator");
}



static void
s390_isel_stmt(ISelEnv *env, IRStmt *stmt)
{
   if (vex_traceflags & VEX_TRACE_VCODE) {
      vex_printf("\n -- ");
      ppIRStmt(stmt);
      vex_printf("\n");
   }

   switch (stmt->tag) {

      
   case Ist_Store: {
      IRType tyd = typeOfIRExpr(env->type_env, stmt->Ist.Store.data);
      s390_amode *am;
      HReg src;

      if (stmt->Ist.Store.end != Iend_BE) goto stmt_fail;

      am = s390_isel_amode(env, stmt->Ist.Store.addr);

      switch (tyd) {
      case Ity_I8:
      case Ity_I16:
      case Ity_I32:
      case Ity_I64:
         
         if (am->tag == S390_AMODE_B12 &&
             stmt->Ist.Store.data->tag == Iex_Const) {
            ULong value =
               get_const_value_as_ulong(stmt->Ist.Store.data->Iex.Const.con);
            addInstr(env, s390_insn_mimm(sizeofIRType(tyd), am, value));
            return;
         }
         
         if (am->tag == S390_AMODE_B12 &&
             stmt->Ist.Store.data->tag == Iex_Get) {
            UInt offset = stmt->Ist.Store.data->Iex.Get.offset;
            s390_amode *from = s390_amode_for_guest_state(offset);
            addInstr(env, s390_insn_memcpy(sizeofIRType(tyd), am, from));
            return;
         }
         
         src = s390_isel_int_expr(env, stmt->Ist.Store.data);
         break;

      case Ity_F32:
      case Ity_F64:
         src = s390_isel_float_expr(env, stmt->Ist.Store.data);
         break;

      case Ity_D32:
      case Ity_D64:
         src = s390_isel_dfp_expr(env, stmt->Ist.Store.data);
         break;

      case Ity_F128:
      case Ity_D128:
         
         vpanic("Ist_Store with 128-bit floating point data");

      default:
         goto stmt_fail;
      }

      addInstr(env, s390_insn_store(sizeofIRType(tyd), am, src));
      return;
   }

      
   case Ist_Put: {
      IRType tyd = typeOfIRExpr(env->type_env, stmt->Ist.Put.data);
      HReg src;
      s390_amode *am;
      ULong new_value, old_value, difference;


      Int offset = stmt->Ist.Put.offset;

      Int guest_reg = get_guest_reg(offset);

      if (guest_reg == GUEST_UNKNOWN) goto not_special;

      if (stmt->Ist.Put.data->tag != Iex_Const) {
         
         env->old_value_valid[guest_reg] = False;
         goto not_special;
      }

      
      if (tyd != Ity_I64)
         goto not_special;

      

      old_value = env->old_value[guest_reg];
      new_value = stmt->Ist.Put.data->Iex.Const.con->Ico.U64;
      env->old_value[guest_reg] = new_value;

      Bool old_value_is_valid = env->old_value_valid[guest_reg];
      env->old_value_valid[guest_reg] = True;

      if (old_value_is_valid && new_value == old_value) {
         return;
      }

      if (old_value_is_valid == False) goto not_special;

      difference = new_value - old_value;

      if (s390_host_has_gie && ulong_fits_signed_8bit(difference)) {
         am = s390_amode_for_guest_state(offset);
         addInstr(env, s390_insn_madd(sizeofIRType(tyd), am,
                                      (difference & 0xFF), new_value));
         return;
      }

      
      if ((old_value >> 32) == (new_value >> 32)) {
         am = s390_amode_for_guest_state(offset + 4);
         addInstr(env, s390_insn_mimm(4, am, new_value & 0xFFFFFFFF));
         return;
      }

      

   not_special:
      am = s390_amode_for_guest_state(offset);

      switch (tyd) {
      case Ity_I8:
      case Ity_I16:
      case Ity_I32:
      case Ity_I64:
         if (am->tag == S390_AMODE_B12 &&
             stmt->Ist.Put.data->tag == Iex_Const) {
            ULong value =
               get_const_value_as_ulong(stmt->Ist.Put.data->Iex.Const.con);
            addInstr(env, s390_insn_mimm(sizeofIRType(tyd), am, value));
            return;
         }
         
         if (am->tag == S390_AMODE_B12 &&
             stmt->Ist.Put.data->tag == Iex_Load) {
            if (stmt->Ist.Put.data->Iex.Load.end != Iend_BE) goto stmt_fail;
            IRExpr *data = stmt->Ist.Put.data->Iex.Load.addr;
            s390_amode *from = s390_isel_amode(env, data);
            UInt size = sizeofIRType(tyd);

            if (from->tag == S390_AMODE_B12) {
               
               addInstr(env, s390_insn_memcpy(size, am, from));
               return;
            }

            src = newVRegI(env);
            addInstr(env, s390_insn_load(size, src, from));
            break;
         }
         
         if (am->tag == S390_AMODE_B12 &&
             stmt->Ist.Put.data->tag == Iex_Get) {
            UInt put_offset = am->d;
            UInt get_offset = stmt->Ist.Put.data->Iex.Get.offset;
            UInt size = sizeofIRType(tyd);
            
            if (put_offset + size <= get_offset ||
                get_offset + size <= put_offset) {
               s390_amode *from = s390_amode_for_guest_state(get_offset);
               addInstr(env, s390_insn_memcpy(size, am, from));
               return;
            }
            goto no_memcpy_put;
         }
         
no_memcpy_put:
         src = s390_isel_int_expr(env, stmt->Ist.Put.data);
         break;

      case Ity_F32:
      case Ity_F64:
         src = s390_isel_float_expr(env, stmt->Ist.Put.data);
         break;

      case Ity_F128:
      case Ity_D128:
         
         vpanic("Ist_Put with 128-bit floating point data");

      case Ity_D32:
      case Ity_D64:
         src = s390_isel_dfp_expr(env, stmt->Ist.Put.data);
         break;

      default:
         goto stmt_fail;
      }

      addInstr(env, s390_insn_store(sizeofIRType(tyd), am, src));
      return;
   }

      
   case Ist_WrTmp: {
      IRTemp tmp = stmt->Ist.WrTmp.tmp;
      IRType tyd = typeOfIRTemp(env->type_env, tmp);
      HReg src, dst;

      switch (tyd) {
      case Ity_I128: {
         HReg dst_hi, dst_lo, res_hi, res_lo;

         s390_isel_int128_expr(&res_hi, &res_lo, env, stmt->Ist.WrTmp.data);
         lookupIRTemp128(&dst_hi, &dst_lo, env, tmp);

         addInstr(env, s390_insn_move(8, dst_hi, res_hi));
         addInstr(env, s390_insn_move(8, dst_lo, res_lo));
         return;
      }

      case Ity_I8:
      case Ity_I16:
      case Ity_I32:
      case Ity_I64:
         src = s390_isel_int_expr(env, stmt->Ist.WrTmp.data);
         dst = lookupIRTemp(env, tmp);
         break;

      case Ity_I1: {
         s390_cc_t cond = s390_isel_cc(env, stmt->Ist.WrTmp.data);
         dst = lookupIRTemp(env, tmp);
         addInstr(env, s390_insn_cc2bool(dst, cond));
         return;
      }

      case Ity_F32:
      case Ity_F64:
         src = s390_isel_float_expr(env, stmt->Ist.WrTmp.data);
         dst = lookupIRTemp(env, tmp);
         break;

      case Ity_F128: {
         HReg dst_hi, dst_lo, res_hi, res_lo;

         s390_isel_float128_expr(&res_hi, &res_lo, env, stmt->Ist.WrTmp.data);
         lookupIRTemp128(&dst_hi, &dst_lo, env, tmp);

         addInstr(env, s390_insn_move(8, dst_hi, res_hi));
         addInstr(env, s390_insn_move(8, dst_lo, res_lo));
         return;
      }

      case Ity_D32:
      case Ity_D64:
         src = s390_isel_dfp_expr(env, stmt->Ist.WrTmp.data);
         dst = lookupIRTemp(env, tmp);
         break;

      case Ity_D128: {
         HReg dst_hi, dst_lo, res_hi, res_lo;

         s390_isel_dfp128_expr(&res_hi, &res_lo, env, stmt->Ist.WrTmp.data);
         lookupIRTemp128(&dst_hi, &dst_lo, env, tmp);

         addInstr(env, s390_insn_move(8, dst_hi, res_hi));
         addInstr(env, s390_insn_move(8, dst_lo, res_lo));
         return;
      }

      default:
         goto stmt_fail;
      }

      addInstr(env, s390_insn_move(sizeofIRType(tyd), dst, src));
      return;
   }

      
   case Ist_Dirty: {
      IRType   retty;
      IRDirty* d = stmt->Ist.Dirty.details;
      HReg dst;
      RetLoc rloc    = mk_RetLoc_INVALID();
      UInt   addToSp = 0;
      Int i;

      for (i = 0; i < d->nFxState; ++i) {
         vassert(d->fxState[i].nRepeats == 0 && d->fxState[i].repeatLen == 0);
         if ((d->fxState[i].fx == Ifx_Write || d->fxState[i].fx == Ifx_Modify)) {
            Int guest_reg = get_guest_reg(d->fxState[i].offset);
            if (guest_reg != GUEST_UNKNOWN)
               env->old_value_valid[guest_reg] = False;
         }
      }

      if (d->tmp == IRTemp_INVALID) {
         
         retty = Ity_INVALID;
         doHelperCall(&addToSp, &rloc, env, d->guard,  d->cee, retty,
                      d->args);
         vassert(is_sane_RetLoc(rloc));
         vassert(rloc.pri == RLPri_None);
         vassert(addToSp == 0);

         return;
      }

      retty = typeOfIRTemp(env->type_env, d->tmp);
      if (retty == Ity_I64 || retty == Ity_I32
          || retty == Ity_I16 || retty == Ity_I8) {
         
         HReg ret = make_gpr(S390_REGNO_RETURN_VALUE);

         dst = lookupIRTemp(env, d->tmp);
         doHelperCall(&addToSp, &rloc, env, d->guard,  d->cee, retty,
                      d->args);
         vassert(is_sane_RetLoc(rloc));
         vassert(rloc.pri == RLPri_Int);
         vassert(addToSp == 0);
         addInstr(env, s390_insn_move(sizeof(ULong), dst, ret));

         return;
      }
      break;
   }

   case Ist_CAS:
      if (stmt->Ist.CAS.details->oldHi == IRTemp_INVALID) {
         IRCAS *cas = stmt->Ist.CAS.details;
         s390_amode *op2 = s390_isel_amode_b12_b20(env, cas->addr);
         HReg op3 = s390_isel_int_expr(env, cas->dataLo);  
         HReg op1 = s390_isel_int_expr(env, cas->expdLo);  
         HReg old = lookupIRTemp(env, cas->oldLo);

         if (typeOfIRTemp(env->type_env, cas->oldLo) == Ity_I32) {
            addInstr(env, s390_insn_cas(4, op1, op2, op3, old));
         } else {
            addInstr(env, s390_insn_cas(8, op1, op2, op3, old));
         }
         return;
      } else {
         IRCAS *cas = stmt->Ist.CAS.details;
         s390_amode *op2 = s390_isel_amode_b12_b20(env, cas->addr);
         HReg r8, r9, r10, r11, r1;
         HReg op3_high = s390_isel_int_expr(env, cas->dataHi);  
         HReg op3_low  = s390_isel_int_expr(env, cas->dataLo);  
         HReg op1_high = s390_isel_int_expr(env, cas->expdHi);  
         HReg op1_low  = s390_isel_int_expr(env, cas->expdLo);  
         HReg old_low  = lookupIRTemp(env, cas->oldLo);
         HReg old_high = lookupIRTemp(env, cas->oldHi);

         r8 = make_gpr(8);
         r9 = make_gpr(9);
         addInstr(env, s390_insn_move(8, r8, op1_high));
         addInstr(env, s390_insn_move(8, r9, op1_low));

         r10 = make_gpr(10);
         r11 = make_gpr(11);
         addInstr(env, s390_insn_move(8, r10, op3_high));
         addInstr(env, s390_insn_move(8, r11, op3_low));

         
         r1 = make_gpr(1);

         if (typeOfIRTemp(env->type_env, cas->oldLo) == Ity_I32) {
            addInstr(env, s390_insn_cdas(4, r8, r9, op2, r10, r11,
                                         old_high, old_low, r1));
         } else {
            addInstr(env, s390_insn_cdas(8, r8, r9, op2, r10, r11,
                                         old_high, old_low, r1));
         }
         addInstr(env, s390_insn_move(8, op1_high, r8));
         addInstr(env, s390_insn_move(8, op1_low,  r9));
         addInstr(env, s390_insn_move(8, op3_high, r10));
         addInstr(env, s390_insn_move(8, op3_low,  r11));
         return;
      }
      break;

      
   case Ist_Exit: {
      s390_cc_t cond;
      IRConstTag tag = stmt->Ist.Exit.dst->tag;

      if (tag != Ico_U64)
         vpanic("s390_isel_stmt: Ist_Exit: dst is not a 64-bit value");

      s390_amode *guest_IA = s390_amode_for_guest_state(stmt->Ist.Exit.offsIP);
      cond = s390_isel_cc(env, stmt->Ist.Exit.guard);

      
      if (stmt->Ist.Exit.jk == Ijk_Boring) {
         if (env->chaining_allowed) {
            
            Bool to_fast_entry
               = ((Addr64)stmt->Ist.Exit.dst->Ico.U64) > env->max_ga;
            if (0) vex_printf("%s", to_fast_entry ? "Y" : ",");
            addInstr(env, s390_insn_xdirect(cond, stmt->Ist.Exit.dst->Ico.U64,
                                            guest_IA, to_fast_entry));
         } else {
            
            HReg dst = s390_isel_int_expr(env,
                                          IRExpr_Const(stmt->Ist.Exit.dst));
            addInstr(env, s390_insn_xassisted(cond, dst, guest_IA, Ijk_Boring));
         }
         return;
      }

      
      switch (stmt->Ist.Exit.jk) {
      case Ijk_EmFail:
      case Ijk_EmWarn:
      case Ijk_NoDecode:
      case Ijk_InvalICache:
      case Ijk_Sys_syscall:
      case Ijk_ClientReq:
      case Ijk_NoRedir:
      case Ijk_Yield:
      case Ijk_SigTRAP: {
         HReg dst = s390_isel_int_expr(env, IRExpr_Const(stmt->Ist.Exit.dst));
         addInstr(env, s390_insn_xassisted(cond, dst, guest_IA,
                                           stmt->Ist.Exit.jk));
         return;
      }
      default:
         break;
      }

      
      goto stmt_fail;
   }

      
   case Ist_MBE:
      switch (stmt->Ist.MBE.event) {
         case Imbe_Fence:
            addInstr(env, s390_insn_mfence());
            return;
         default:
            break;
      }
      break;

      

   case Ist_PutI:    
   case Ist_IMark:   
   case Ist_NoOp:    
   case Ist_AbiHint: 
      return;

   default:
      break;
   }

 stmt_fail:
   ppIRStmt(stmt);
   vpanic("s390_isel_stmt");
}



static void
iselNext(ISelEnv *env, IRExpr *next, IRJumpKind jk, Int offsIP)
{
   if (vex_traceflags & VEX_TRACE_VCODE) {
      vex_printf("\n-- PUT(%d) = ", offsIP);
      ppIRExpr(next);
      vex_printf("; exit-");
      ppIRJumpKind(jk);
      vex_printf("\n");
   }

   s390_amode *guest_IA = s390_amode_for_guest_state(offsIP);

   
   if (next->tag == Iex_Const) {
      IRConst *cdst = next->Iex.Const.con;
      vassert(cdst->tag == Ico_U64);
      if (jk == Ijk_Boring || jk == Ijk_Call) {
         
         if (env->chaining_allowed) {
            
            Bool to_fast_entry
               = ((Addr64)cdst->Ico.U64) > env->max_ga;
            if (0) vex_printf("%s", to_fast_entry ? "X" : ".");
            addInstr(env, s390_insn_xdirect(S390_CC_ALWAYS, cdst->Ico.U64,
                                            guest_IA, to_fast_entry));
         } else {
            
            HReg dst = s390_isel_int_expr(env, next);
            addInstr(env, s390_insn_xassisted(S390_CC_ALWAYS, dst, guest_IA,
                                              Ijk_Boring));
         }
         return;
      }
   }

   
   switch (jk) {
   case Ijk_Boring:
   case Ijk_Ret:
   case Ijk_Call: {
      HReg dst = s390_isel_int_expr(env, next);
      if (env->chaining_allowed) {
         addInstr(env, s390_insn_xindir(S390_CC_ALWAYS, dst, guest_IA));
      } else {
         addInstr(env, s390_insn_xassisted(S390_CC_ALWAYS, dst, guest_IA,
                                           Ijk_Boring));
      }
      return;
   }
   default:
      break;
   }

   
   switch (jk) {
   case Ijk_EmFail:
   case Ijk_EmWarn:
   case Ijk_NoDecode:
   case Ijk_InvalICache:
   case Ijk_Sys_syscall:
   case Ijk_ClientReq:
   case Ijk_NoRedir:
   case Ijk_Yield:
   case Ijk_SigTRAP: {
      HReg dst = s390_isel_int_expr(env, next);
      addInstr(env, s390_insn_xassisted(S390_CC_ALWAYS, dst, guest_IA, jk));
      return;
   }
   default:
      break;
   }

   vpanic("iselNext");
}




HInstrArray *
iselSB_S390(const IRSB *bb, VexArch arch_host, const VexArchInfo *archinfo_host,
            const VexAbiInfo *vbi, Int offset_host_evcheck_counter,
            Int offset_host_evcheck_fail_addr, Bool chaining_allowed,
            Bool add_profinc, Addr max_ga)
{
   UInt     i, j;
   HReg     hreg, hregHI;
   ISelEnv *env;
   UInt     hwcaps_host = archinfo_host->hwcaps;

   
   vassert((VEX_HWCAPS_S390X(hwcaps_host) & ~(VEX_HWCAPS_S390X_ALL)) == 0);

   
   vassert(archinfo_host->endness == VexEndnessBE);

   
   env = LibVEX_Alloc_inline(sizeof(ISelEnv));
   env->vreg_ctr = 0;

   
   env->code = newHInstrArray();

   
   env->type_env = bb->tyenv;

   
   for (i = 0; i < NUM_TRACKED_REGS; ++i) {
      env->old_value[i] = 0;  
      env->old_value_valid[i] = False;
   }

   vassert(bb->tyenv->types_used >= 0);

   env->n_vregmap = bb->tyenv->types_used;
   env->vregmap   = LibVEX_Alloc_inline(env->n_vregmap * sizeof(HReg));
   env->vregmapHI = LibVEX_Alloc_inline(env->n_vregmap * sizeof(HReg));

   env->previous_bfp_rounding_mode = NULL;
   env->previous_dfp_rounding_mode = NULL;

   
   env->hwcaps    = hwcaps_host;

   env->max_ga = max_ga;
   env->chaining_allowed = chaining_allowed;

   j = 0;
   for (i = 0; i < env->n_vregmap; i++) {
      hregHI = hreg = INVALID_HREG;
      switch (bb->tyenv->types[i]) {
      case Ity_I1:
      case Ity_I8:
      case Ity_I16:
      case Ity_I32:
      case Ity_I64:
         hreg = mkVRegI(j++);
         break;

      case Ity_I128:
         hreg   = mkVRegI(j++);
         hregHI = mkVRegI(j++);
         break;

      case Ity_F32:
      case Ity_F64:
      case Ity_D32:
      case Ity_D64:
         hreg = mkVRegF(j++);
         break;

      case Ity_F128:
      case Ity_D128:
         hreg   = mkVRegF(j++);
         hregHI = mkVRegF(j++);
         break;

      case Ity_V128: 
      default:
         ppIRType(bb->tyenv->types[i]);
         vpanic("iselSB_S390: IRTemp type");
      }

      env->vregmap[i]   = hreg;
      env->vregmapHI[i] = hregHI;
   }
   env->vreg_ctr = j;

   
   s390_amode *counter, *fail_addr;
   counter   = s390_amode_for_guest_state(offset_host_evcheck_counter);
   fail_addr = s390_amode_for_guest_state(offset_host_evcheck_fail_addr);
   addInstr(env, s390_insn_evcheck(counter, fail_addr));

   if (add_profinc) {
      addInstr(env, s390_insn_profinc());
   }

   
   for (i = 0; i < bb->stmts_used; i++)
      if (bb->stmts[i])
         s390_isel_stmt(env, bb->stmts[i]);

   iselNext(env, bb->next, bb->jumpkind, bb->offsIP);

   
   env->code->n_vregs = env->vreg_ctr;

   return env->code;
}

