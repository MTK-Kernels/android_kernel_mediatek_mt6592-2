

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


/* TODO:

   All Puts to CC_OP/CC_DEP1/CC_DEP2/CC_NDEP should really be checked
   to ensure a 32-bit value is being written.

   FUCOMI(P): what happens to A and S flags?  Currently are forced
      to zero.

   x87 FP Limitations:

   * all arithmetic done at 64 bits

   * no FP exceptions, except for handling stack over/underflow

   * FP rounding mode observed only for float->int conversions
     and int->float conversions which could lose accuracy, and
     for float-to-float rounding.  For all other operations, 
     round-to-nearest is used, regardless.

   * some of the FCOM cases could do with testing -- not convinced
     that the args are the right way round.

   * FSAVE does not re-initialise the FPU; it should do

   * FINIT not only initialises the FPU environment, it also
     zeroes all the FP registers.  It should leave the registers
     unchanged.

   SAHF should cause eflags[1] == 1, and in fact it produces 0.  As
   per Intel docs this bit has no meaning anyway.  Since PUSHF is the
   only way to observe eflags[1], a proper fix would be to make that
   bit be set by PUSHF.

   The state of %eflags.AC (alignment check, bit 18) is recorded by
   the simulation (viz, if you set it with popf then a pushf produces
   the value you set it to), but it is otherwise ignored.  In
   particular, setting it to 1 does NOT cause alignment checking to
   happen.  Programs that set it to 1 and then rely on the resulting
   SIGBUSs to inform them of misaligned accesses will not work.

   Implementation of sysenter is necessarily partial.  sysenter is a
   kind of system call entry.  When doing a sysenter, the return
   address is not known -- that is something that is beyond Vex's
   knowledge.  So the generated IR forces a return to the scheduler,
   which can do what it likes to simulate the systenter, but it MUST
   set this thread's guest_EIP field with the continuation address
   before resuming execution.  If that doesn't happen, the thread will
   jump to address zero, which is probably fatal.

   This module uses global variables and so is not MT-safe (if that
   should ever become relevant).

   The delta values are 32-bit ints, not 64-bit ints.  That means
   this module may not work right if run on a 64-bit host.  That should
   be fixed properly, really -- if anyone ever wants to use Vex to
   translate x86 code for execution on a 64-bit host.

   casLE (implementation of lock-prefixed insns) and rep-prefixed
   insns: the side-exit back to the start of the insn is done with
   Ijk_Boring.  This is quite wrong, it should be done with
   Ijk_NoRedir, since otherwise the side exit, which is intended to
   restart the instruction for whatever reason, could go somewhere
   entirely else.  Doing it right (with Ijk_NoRedir jumps) would make
   no-redir jumps performance critical, at least for rep-prefixed
   instructions, since all iterations thereof would involve such a
   jump.  It's not such a big deal with casLE since the side exit is
   only taken if the CAS fails, that is, the location is contended,
   which is relatively unlikely.

   XXXX: Nov 2009: handling of SWP on ARM suffers from the same
   problem.

   Note also, the test for CAS success vs failure is done using
   Iop_CasCmp{EQ,NE}{8,16,32,64} rather than the ordinary
   Iop_Cmp{EQ,NE} equivalents.  This is so as to tell Memcheck that it
   shouldn't definedness-check these comparisons.  See
   COMMENT_ON_CasCmpEQ in memcheck/mc_translate.c for
   background/rationale.
*/





#include "libvex_basictypes.h"
#include "libvex_ir.h"
#include "libvex.h"
#include "libvex_guest_x86.h"

#include "main_util.h"
#include "main_globals.h"
#include "guest_generic_bb_to_IR.h"
#include "guest_generic_x87.h"
#include "guest_x86_defs.h"




static VexEndness host_endness;

static const UChar* guest_code;

static Addr32 guest_EIP_bbstart;

static Addr32 guest_EIP_curr_instr;

static IRSB* irsb;



#define DIP(format, args...)           \
   if (vex_traceflags & VEX_TRACE_FE)  \
      vex_printf(format, ## args)

#define DIS(buf, format, args...)      \
   if (vex_traceflags & VEX_TRACE_FE)  \
      vex_sprintf(buf, format, ## args)



#define OFFB_EAX       offsetof(VexGuestX86State,guest_EAX)
#define OFFB_EBX       offsetof(VexGuestX86State,guest_EBX)
#define OFFB_ECX       offsetof(VexGuestX86State,guest_ECX)
#define OFFB_EDX       offsetof(VexGuestX86State,guest_EDX)
#define OFFB_ESP       offsetof(VexGuestX86State,guest_ESP)
#define OFFB_EBP       offsetof(VexGuestX86State,guest_EBP)
#define OFFB_ESI       offsetof(VexGuestX86State,guest_ESI)
#define OFFB_EDI       offsetof(VexGuestX86State,guest_EDI)

#define OFFB_EIP       offsetof(VexGuestX86State,guest_EIP)

#define OFFB_CC_OP     offsetof(VexGuestX86State,guest_CC_OP)
#define OFFB_CC_DEP1   offsetof(VexGuestX86State,guest_CC_DEP1)
#define OFFB_CC_DEP2   offsetof(VexGuestX86State,guest_CC_DEP2)
#define OFFB_CC_NDEP   offsetof(VexGuestX86State,guest_CC_NDEP)

#define OFFB_FPREGS    offsetof(VexGuestX86State,guest_FPREG[0])
#define OFFB_FPTAGS    offsetof(VexGuestX86State,guest_FPTAG[0])
#define OFFB_DFLAG     offsetof(VexGuestX86State,guest_DFLAG)
#define OFFB_IDFLAG    offsetof(VexGuestX86State,guest_IDFLAG)
#define OFFB_ACFLAG    offsetof(VexGuestX86State,guest_ACFLAG)
#define OFFB_FTOP      offsetof(VexGuestX86State,guest_FTOP)
#define OFFB_FC3210    offsetof(VexGuestX86State,guest_FC3210)
#define OFFB_FPROUND   offsetof(VexGuestX86State,guest_FPROUND)

#define OFFB_CS        offsetof(VexGuestX86State,guest_CS)
#define OFFB_DS        offsetof(VexGuestX86State,guest_DS)
#define OFFB_ES        offsetof(VexGuestX86State,guest_ES)
#define OFFB_FS        offsetof(VexGuestX86State,guest_FS)
#define OFFB_GS        offsetof(VexGuestX86State,guest_GS)
#define OFFB_SS        offsetof(VexGuestX86State,guest_SS)
#define OFFB_LDT       offsetof(VexGuestX86State,guest_LDT)
#define OFFB_GDT       offsetof(VexGuestX86State,guest_GDT)

#define OFFB_SSEROUND  offsetof(VexGuestX86State,guest_SSEROUND)
#define OFFB_XMM0      offsetof(VexGuestX86State,guest_XMM0)
#define OFFB_XMM1      offsetof(VexGuestX86State,guest_XMM1)
#define OFFB_XMM2      offsetof(VexGuestX86State,guest_XMM2)
#define OFFB_XMM3      offsetof(VexGuestX86State,guest_XMM3)
#define OFFB_XMM4      offsetof(VexGuestX86State,guest_XMM4)
#define OFFB_XMM5      offsetof(VexGuestX86State,guest_XMM5)
#define OFFB_XMM6      offsetof(VexGuestX86State,guest_XMM6)
#define OFFB_XMM7      offsetof(VexGuestX86State,guest_XMM7)

#define OFFB_EMNOTE    offsetof(VexGuestX86State,guest_EMNOTE)

#define OFFB_CMSTART   offsetof(VexGuestX86State,guest_CMSTART)
#define OFFB_CMLEN     offsetof(VexGuestX86State,guest_CMLEN)
#define OFFB_NRADDR    offsetof(VexGuestX86State,guest_NRADDR)

#define OFFB_IP_AT_SYSCALL offsetof(VexGuestX86State,guest_IP_AT_SYSCALL)



#define R_EAX 0
#define R_ECX 1
#define R_EDX 2
#define R_EBX 3
#define R_ESP 4
#define R_EBP 5
#define R_ESI 6
#define R_EDI 7

#define R_AL (0+R_EAX)
#define R_AH (4+R_EAX)

#define R_ES 0
#define R_CS 1
#define R_SS 2
#define R_DS 3
#define R_FS 4
#define R_GS 5


static void stmt ( IRStmt* st )
{
   addStmtToIRSB( irsb, st );
}

static IRTemp newTemp ( IRType ty )
{
   vassert(isPlausibleIRType(ty));
   return newIRTemp( irsb->tyenv, ty );
}


static UInt extend_s_8to32( UInt x )
{
   return (UInt)((Int)(x << 24) >> 24);
}

static UInt extend_s_16to32 ( UInt x )
{
  return (UInt)((Int)(x << 16) >> 16);
}

static UChar getIByte ( Int delta )
{
   return guest_code[delta];
}

static Int gregOfRM ( UChar mod_reg_rm )
{
   return (Int)( (mod_reg_rm >> 3) & 7 );
}

static Bool epartIsReg ( UChar mod_reg_rm )
{
   return toBool(0xC0 == (mod_reg_rm & 0xC0));
}

static Int eregOfRM ( UChar mod_reg_rm )
{
   return (Int)(mod_reg_rm & 0x7);
}


static UChar getUChar ( Int delta )
{
   UChar v = guest_code[delta+0];
   return toUChar(v);
}

static UInt getUDisp16 ( Int delta )
{
   UInt v = guest_code[delta+1]; v <<= 8;
   v |= guest_code[delta+0];
   return v & 0xFFFF;
}

static UInt getUDisp32 ( Int delta )
{
   UInt v = guest_code[delta+3]; v <<= 8;
   v |= guest_code[delta+2]; v <<= 8;
   v |= guest_code[delta+1]; v <<= 8;
   v |= guest_code[delta+0];
   return v;
}

static UInt getUDisp ( Int size, Int delta )
{
   switch (size) {
      case 4: return getUDisp32(delta);
      case 2: return getUDisp16(delta);
      case 1: return (UInt)getUChar(delta);
      default: vpanic("getUDisp(x86)");
   }
   return 0; 
}


static UInt getSDisp8 ( Int delta )
{
   return extend_s_8to32( (UInt) (guest_code[delta]) );
}

static UInt getSDisp16 ( Int delta0 )
{
   const UChar* eip = &guest_code[delta0];
   UInt d = *eip++;
   d |= ((*eip++) << 8);
   return extend_s_16to32(d);
}

static UInt getSDisp ( Int size, Int delta )
{
   switch (size) {
      case 4: return getUDisp32(delta);
      case 2: return getSDisp16(delta);
      case 1: return getSDisp8(delta);
      default: vpanic("getSDisp(x86)");
  }
  return 0; 
}




static IRType szToITy ( Int n )
{
   switch (n) {
      case 1: return Ity_I8;
      case 2: return Ity_I16;
      case 4: return Ity_I32;
      default: vpanic("szToITy(x86)");
   }
}

static Int integerGuestRegOffset ( Int sz, UInt archreg )
{
   vassert(archreg < 8);

   
   vassert(host_endness == VexEndnessLE);

   if (sz == 4 || sz == 2 || (sz == 1 && archreg < 4)) {
      switch (archreg) {
         case R_EAX: return OFFB_EAX;
         case R_EBX: return OFFB_EBX;
         case R_ECX: return OFFB_ECX;
         case R_EDX: return OFFB_EDX;
         case R_ESI: return OFFB_ESI;
         case R_EDI: return OFFB_EDI;
         case R_ESP: return OFFB_ESP;
         case R_EBP: return OFFB_EBP;
         default: vpanic("integerGuestRegOffset(x86,le)(4,2)");
      }
   }

   vassert(archreg >= 4 && archreg < 8 && sz == 1);
   switch (archreg-4) {
      case R_EAX: return 1+ OFFB_EAX;
      case R_EBX: return 1+ OFFB_EBX;
      case R_ECX: return 1+ OFFB_ECX;
      case R_EDX: return 1+ OFFB_EDX;
      default: vpanic("integerGuestRegOffset(x86,le)(1h)");
   }

   
   vpanic("integerGuestRegOffset(x86,le)");
}

static Int segmentGuestRegOffset ( UInt sreg )
{
   switch (sreg) {
      case R_ES: return OFFB_ES;
      case R_CS: return OFFB_CS;
      case R_SS: return OFFB_SS;
      case R_DS: return OFFB_DS;
      case R_FS: return OFFB_FS;
      case R_GS: return OFFB_GS;
      default: vpanic("segmentGuestRegOffset(x86)");
   }
}

static Int xmmGuestRegOffset ( UInt xmmreg )
{
   switch (xmmreg) {
      case 0: return OFFB_XMM0;
      case 1: return OFFB_XMM1;
      case 2: return OFFB_XMM2;
      case 3: return OFFB_XMM3;
      case 4: return OFFB_XMM4;
      case 5: return OFFB_XMM5;
      case 6: return OFFB_XMM6;
      case 7: return OFFB_XMM7;
      default: vpanic("xmmGuestRegOffset");
   }
}


static Int xmmGuestRegLane16offset ( UInt xmmreg, Int laneno )
{
   
   vassert(host_endness == VexEndnessLE);
   vassert(laneno >= 0 && laneno < 8);
   return xmmGuestRegOffset( xmmreg ) + 2 * laneno;
}

static Int xmmGuestRegLane32offset ( UInt xmmreg, Int laneno )
{
   
   vassert(host_endness == VexEndnessLE);
   vassert(laneno >= 0 && laneno < 4);
   return xmmGuestRegOffset( xmmreg ) + 4 * laneno;
}

static Int xmmGuestRegLane64offset ( UInt xmmreg, Int laneno )
{
   
   vassert(host_endness == VexEndnessLE);
   vassert(laneno >= 0 && laneno < 2);
   return xmmGuestRegOffset( xmmreg ) + 8 * laneno;
}

static IRExpr* getIReg ( Int sz, UInt archreg )
{
   vassert(sz == 1 || sz == 2 || sz == 4);
   vassert(archreg < 8);
   return IRExpr_Get( integerGuestRegOffset(sz,archreg),
                      szToITy(sz) );
}

static void putIReg ( Int sz, UInt archreg, IRExpr* e )
{
   IRType ty = typeOfIRExpr(irsb->tyenv, e);
   switch (sz) {
      case 1: vassert(ty == Ity_I8); break;
      case 2: vassert(ty == Ity_I16); break;
      case 4: vassert(ty == Ity_I32); break;
      default: vpanic("putIReg(x86)");
   }
   vassert(archreg < 8);
   stmt( IRStmt_Put(integerGuestRegOffset(sz,archreg), e) );
}

static IRExpr* getSReg ( UInt sreg )
{
   return IRExpr_Get( segmentGuestRegOffset(sreg), Ity_I16 );
}

static void putSReg ( UInt sreg, IRExpr* e )
{
   vassert(typeOfIRExpr(irsb->tyenv,e) == Ity_I16);
   stmt( IRStmt_Put( segmentGuestRegOffset(sreg), e ) );
}

static IRExpr* getXMMReg ( UInt xmmreg )
{
   return IRExpr_Get( xmmGuestRegOffset(xmmreg), Ity_V128 );
}

static IRExpr* getXMMRegLane64 ( UInt xmmreg, Int laneno )
{
   return IRExpr_Get( xmmGuestRegLane64offset(xmmreg,laneno), Ity_I64 );
}

static IRExpr* getXMMRegLane64F ( UInt xmmreg, Int laneno )
{
   return IRExpr_Get( xmmGuestRegLane64offset(xmmreg,laneno), Ity_F64 );
}

static IRExpr* getXMMRegLane32 ( UInt xmmreg, Int laneno )
{
   return IRExpr_Get( xmmGuestRegLane32offset(xmmreg,laneno), Ity_I32 );
}

static IRExpr* getXMMRegLane32F ( UInt xmmreg, Int laneno )
{
   return IRExpr_Get( xmmGuestRegLane32offset(xmmreg,laneno), Ity_F32 );
}

static void putXMMReg ( UInt xmmreg, IRExpr* e )
{
   vassert(typeOfIRExpr(irsb->tyenv,e) == Ity_V128);
   stmt( IRStmt_Put( xmmGuestRegOffset(xmmreg), e ) );
}

static void putXMMRegLane64 ( UInt xmmreg, Int laneno, IRExpr* e )
{
   vassert(typeOfIRExpr(irsb->tyenv,e) == Ity_I64);
   stmt( IRStmt_Put( xmmGuestRegLane64offset(xmmreg,laneno), e ) );
}

static void putXMMRegLane64F ( UInt xmmreg, Int laneno, IRExpr* e )
{
   vassert(typeOfIRExpr(irsb->tyenv,e) == Ity_F64);
   stmt( IRStmt_Put( xmmGuestRegLane64offset(xmmreg,laneno), e ) );
}

static void putXMMRegLane32F ( UInt xmmreg, Int laneno, IRExpr* e )
{
   vassert(typeOfIRExpr(irsb->tyenv,e) == Ity_F32);
   stmt( IRStmt_Put( xmmGuestRegLane32offset(xmmreg,laneno), e ) );
}

static void putXMMRegLane32 ( UInt xmmreg, Int laneno, IRExpr* e )
{
   vassert(typeOfIRExpr(irsb->tyenv,e) == Ity_I32);
   stmt( IRStmt_Put( xmmGuestRegLane32offset(xmmreg,laneno), e ) );
}

static void putXMMRegLane16 ( UInt xmmreg, Int laneno, IRExpr* e )
{
   vassert(typeOfIRExpr(irsb->tyenv,e) == Ity_I16);
   stmt( IRStmt_Put( xmmGuestRegLane16offset(xmmreg,laneno), e ) );
}

static void assign ( IRTemp dst, IRExpr* e )
{
   stmt( IRStmt_WrTmp(dst, e) );
}

static void storeLE ( IRExpr* addr, IRExpr* data )
{
   stmt( IRStmt_Store(Iend_LE, addr, data) );
}

static IRExpr* unop ( IROp op, IRExpr* a )
{
   return IRExpr_Unop(op, a);
}

static IRExpr* binop ( IROp op, IRExpr* a1, IRExpr* a2 )
{
   return IRExpr_Binop(op, a1, a2);
}

static IRExpr* triop ( IROp op, IRExpr* a1, IRExpr* a2, IRExpr* a3 )
{
   return IRExpr_Triop(op, a1, a2, a3);
}

static IRExpr* mkexpr ( IRTemp tmp )
{
   return IRExpr_RdTmp(tmp);
}

static IRExpr* mkU8 ( UInt i )
{
   vassert(i < 256);
   return IRExpr_Const(IRConst_U8( (UChar)i ));
}

static IRExpr* mkU16 ( UInt i )
{
   vassert(i < 65536);
   return IRExpr_Const(IRConst_U16( (UShort)i ));
}

static IRExpr* mkU32 ( UInt i )
{
   return IRExpr_Const(IRConst_U32(i));
}

static IRExpr* mkU64 ( ULong i )
{
   return IRExpr_Const(IRConst_U64(i));
}

static IRExpr* mkU ( IRType ty, UInt i )
{
   if (ty == Ity_I8)  return mkU8(i);
   if (ty == Ity_I16) return mkU16(i);
   if (ty == Ity_I32) return mkU32(i);
   vpanic("mkU(x86)");
}

static IRExpr* mkV128 ( UShort mask )
{
   return IRExpr_Const(IRConst_V128(mask));
}

static IRExpr* loadLE ( IRType ty, IRExpr* addr )
{
   return IRExpr_Load(Iend_LE, ty, addr);
}

static IROp mkSizedOp ( IRType ty, IROp op8 )
{
   Int adj;
   vassert(ty == Ity_I8 || ty == Ity_I16 || ty == Ity_I32);
   vassert(op8 == Iop_Add8 || op8 == Iop_Sub8 
           || op8 == Iop_Mul8 
           || op8 == Iop_Or8 || op8 == Iop_And8 || op8 == Iop_Xor8
           || op8 == Iop_Shl8 || op8 == Iop_Shr8 || op8 == Iop_Sar8
           || op8 == Iop_CmpEQ8 || op8 == Iop_CmpNE8
           || op8 == Iop_CasCmpNE8
           || op8 == Iop_ExpCmpNE8
           || op8 == Iop_Not8);
   adj = ty==Ity_I8 ? 0 : (ty==Ity_I16 ? 1 : 2);
   return adj + op8;
}

static IROp mkWidenOp ( Int szSmall, Int szBig, Bool signd )
{
   if (szSmall == 1 && szBig == 4) {
      return signd ? Iop_8Sto32 : Iop_8Uto32;
   }
   if (szSmall == 1 && szBig == 2) {
      return signd ? Iop_8Sto16 : Iop_8Uto16;
   }
   if (szSmall == 2 && szBig == 4) {
      return signd ? Iop_16Sto32 : Iop_16Uto32;
   }
   vpanic("mkWidenOp(x86,guest)");
}

static IRExpr* mkAnd1 ( IRExpr* x, IRExpr* y )
{
   vassert(typeOfIRExpr(irsb->tyenv,x) == Ity_I1);
   vassert(typeOfIRExpr(irsb->tyenv,y) == Ity_I1);
   return unop(Iop_32to1, 
               binop(Iop_And32, 
                     unop(Iop_1Uto32,x), 
                     unop(Iop_1Uto32,y)));
}

static void casLE ( IRExpr* addr, IRExpr* expVal, IRExpr* newVal,
                    Addr32 restart_point )
{
   IRCAS* cas;
   IRType tyE    = typeOfIRExpr(irsb->tyenv, expVal);
   IRType tyN    = typeOfIRExpr(irsb->tyenv, newVal);
   IRTemp oldTmp = newTemp(tyE);
   IRTemp expTmp = newTemp(tyE);
   vassert(tyE == tyN);
   vassert(tyE == Ity_I32 || tyE == Ity_I16 || tyE == Ity_I8);
   assign(expTmp, expVal);
   cas = mkIRCAS( IRTemp_INVALID, oldTmp, Iend_LE, addr, 
                  NULL, mkexpr(expTmp), NULL, newVal );
   stmt( IRStmt_CAS(cas) );
   stmt( IRStmt_Exit(
            binop( mkSizedOp(tyE,Iop_CasCmpNE8),
                   mkexpr(oldTmp), mkexpr(expTmp) ),
            Ijk_Boring, 
            IRConst_U32( restart_point ),
            OFFB_EIP
         ));
}




static IRExpr* mk_x86g_calculate_eflags_all ( void )
{
   IRExpr** args
      = mkIRExprVec_4( IRExpr_Get(OFFB_CC_OP,   Ity_I32),
                       IRExpr_Get(OFFB_CC_DEP1, Ity_I32),
                       IRExpr_Get(OFFB_CC_DEP2, Ity_I32),
                       IRExpr_Get(OFFB_CC_NDEP, Ity_I32) );
   IRExpr* call
      = mkIRExprCCall(
           Ity_I32,
           0, 
           "x86g_calculate_eflags_all", &x86g_calculate_eflags_all,
           args
        );
   call->Iex.CCall.cee->mcx_mask = (1<<0) | (1<<3);
   return call;
}

static IRExpr* mk_x86g_calculate_condition ( X86Condcode cond )
{
   IRExpr** args
      = mkIRExprVec_5( mkU32(cond),
                       IRExpr_Get(OFFB_CC_OP,  Ity_I32),
                       IRExpr_Get(OFFB_CC_DEP1, Ity_I32),
                       IRExpr_Get(OFFB_CC_DEP2, Ity_I32),
                       IRExpr_Get(OFFB_CC_NDEP, Ity_I32) );
   IRExpr* call
      = mkIRExprCCall(
           Ity_I32,
           0, 
           "x86g_calculate_condition", &x86g_calculate_condition,
           args
        );
   call->Iex.CCall.cee->mcx_mask = (1<<0) | (1<<1) | (1<<4);
   return unop(Iop_32to1, call);
}

static IRExpr* mk_x86g_calculate_eflags_c ( void )
{
   IRExpr** args
      = mkIRExprVec_4( IRExpr_Get(OFFB_CC_OP,   Ity_I32),
                       IRExpr_Get(OFFB_CC_DEP1, Ity_I32),
                       IRExpr_Get(OFFB_CC_DEP2, Ity_I32),
                       IRExpr_Get(OFFB_CC_NDEP, Ity_I32) );
   IRExpr* call
      = mkIRExprCCall(
           Ity_I32,
           3, 
           "x86g_calculate_eflags_c", &x86g_calculate_eflags_c,
           args
        );
   call->Iex.CCall.cee->mcx_mask = (1<<0) | (1<<3);
   return call;
}




static Bool isAddSub ( IROp op8 )
{
   return toBool(op8 == Iop_Add8 || op8 == Iop_Sub8);
}

static Bool isLogic ( IROp op8 )
{
   return toBool(op8 == Iop_And8 || op8 == Iop_Or8 || op8 == Iop_Xor8);
}

static IRExpr* widenUto32 ( IRExpr* e )
{
   switch (typeOfIRExpr(irsb->tyenv,e)) {
      case Ity_I32: return e;
      case Ity_I16: return unop(Iop_16Uto32,e);
      case Ity_I8:  return unop(Iop_8Uto32,e);
      default: vpanic("widenUto32");
   }
}

static IRExpr* widenSto32 ( IRExpr* e )
{
   switch (typeOfIRExpr(irsb->tyenv,e)) {
      case Ity_I32: return e;
      case Ity_I16: return unop(Iop_16Sto32,e);
      case Ity_I8:  return unop(Iop_8Sto32,e);
      default: vpanic("widenSto32");
   }
}

static IRExpr* narrowTo ( IRType dst_ty, IRExpr* e )
{
   IRType src_ty = typeOfIRExpr(irsb->tyenv,e);
   if (src_ty == dst_ty)
      return e;
   if (src_ty == Ity_I32 && dst_ty == Ity_I16)
      return unop(Iop_32to16, e);
   if (src_ty == Ity_I32 && dst_ty == Ity_I8)
      return unop(Iop_32to8, e);

   vex_printf("\nsrc, dst tys are: ");
   ppIRType(src_ty);
   vex_printf(", ");
   ppIRType(dst_ty);
   vex_printf("\n");
   vpanic("narrowTo(x86)");
}



static 
void setFlags_DEP1_DEP2 ( IROp op8, IRTemp dep1, IRTemp dep2, IRType ty )
{
   Int ccOp = ty==Ity_I8 ? 0 : (ty==Ity_I16 ? 1 : 2);

   vassert(ty == Ity_I8 || ty == Ity_I16 || ty == Ity_I32);

   switch (op8) {
      case Iop_Add8: ccOp += X86G_CC_OP_ADDB;   break;
      case Iop_Sub8: ccOp += X86G_CC_OP_SUBB;   break;
      default:       ppIROp(op8);
                     vpanic("setFlags_DEP1_DEP2(x86)");
   }
   stmt( IRStmt_Put( OFFB_CC_OP,   mkU32(ccOp)) );
   stmt( IRStmt_Put( OFFB_CC_DEP1, widenUto32(mkexpr(dep1))) );
   stmt( IRStmt_Put( OFFB_CC_DEP2, widenUto32(mkexpr(dep2))) );
   stmt( IRStmt_Put( OFFB_CC_NDEP, mkU32(0) ));
}



static 
void setFlags_DEP1 ( IROp op8, IRTemp dep1, IRType ty )
{
   Int ccOp = ty==Ity_I8 ? 0 : (ty==Ity_I16 ? 1 : 2);

   vassert(ty == Ity_I8 || ty == Ity_I16 || ty == Ity_I32);

   switch (op8) {
      case Iop_Or8:
      case Iop_And8:
      case Iop_Xor8: ccOp += X86G_CC_OP_LOGICB; break;
      default:       ppIROp(op8);
                     vpanic("setFlags_DEP1(x86)");
   }
   stmt( IRStmt_Put( OFFB_CC_OP,   mkU32(ccOp)) );
   stmt( IRStmt_Put( OFFB_CC_DEP1, widenUto32(mkexpr(dep1))) );
   stmt( IRStmt_Put( OFFB_CC_DEP2, mkU32(0)) );
   stmt( IRStmt_Put( OFFB_CC_NDEP, mkU32(0) ));
}



static void setFlags_DEP1_DEP2_shift ( IROp    op32,
                                       IRTemp  res,
                                       IRTemp  resUS,
                                       IRType  ty,
                                       IRTemp  guard )
{
   Int ccOp = ty==Ity_I8 ? 2 : (ty==Ity_I16 ? 1 : 0);

   vassert(ty == Ity_I8 || ty == Ity_I16 || ty == Ity_I32);
   vassert(guard);

   switch (op32) {
      case Iop_Shr32:
      case Iop_Sar32: ccOp = X86G_CC_OP_SHRL - ccOp; break;
      case Iop_Shl32: ccOp = X86G_CC_OP_SHLL - ccOp; break;
      default:        ppIROp(op32);
                      vpanic("setFlags_DEP1_DEP2_shift(x86)");
   }

   
   IRTemp guardB = newTemp(Ity_I1);
   assign( guardB, binop(Iop_CmpNE8, mkexpr(guard), mkU8(0)) );

   
   stmt( IRStmt_Put( OFFB_CC_OP,
                     IRExpr_ITE( mkexpr(guardB),
                                 mkU32(ccOp),
                                 IRExpr_Get(OFFB_CC_OP,Ity_I32) ) ));
   stmt( IRStmt_Put( OFFB_CC_DEP1,
                     IRExpr_ITE( mkexpr(guardB),
                                 widenUto32(mkexpr(res)),
                                 IRExpr_Get(OFFB_CC_DEP1,Ity_I32) ) ));
   stmt( IRStmt_Put( OFFB_CC_DEP2, 
                     IRExpr_ITE( mkexpr(guardB),
                                 widenUto32(mkexpr(resUS)),
                                 IRExpr_Get(OFFB_CC_DEP2,Ity_I32) ) ));
   stmt( IRStmt_Put( OFFB_CC_NDEP,
                     IRExpr_ITE( mkexpr(guardB),
                                 mkU32(0),
                                 IRExpr_Get(OFFB_CC_NDEP,Ity_I32) ) ));
}



static void setFlags_INC_DEC ( Bool inc, IRTemp res, IRType ty )
{
   Int ccOp = inc ? X86G_CC_OP_INCB : X86G_CC_OP_DECB;
   
   ccOp += ty==Ity_I8 ? 0 : (ty==Ity_I16 ? 1 : 2);
   vassert(ty == Ity_I8 || ty == Ity_I16 || ty == Ity_I32);

   stmt( IRStmt_Put( OFFB_CC_NDEP, mk_x86g_calculate_eflags_c()) );
   stmt( IRStmt_Put( OFFB_CC_OP,   mkU32(ccOp)) );
   stmt( IRStmt_Put( OFFB_CC_DEP1, widenUto32(mkexpr(res))) );
   stmt( IRStmt_Put( OFFB_CC_DEP2, mkU32(0)) );
}



static
void setFlags_MUL ( IRType ty, IRTemp arg1, IRTemp arg2, UInt base_op )
{
   switch (ty) {
      case Ity_I8:
         stmt( IRStmt_Put( OFFB_CC_OP, mkU32(base_op+0) ) );
         break;
      case Ity_I16:
         stmt( IRStmt_Put( OFFB_CC_OP, mkU32(base_op+1) ) );
         break;
      case Ity_I32:
         stmt( IRStmt_Put( OFFB_CC_OP, mkU32(base_op+2) ) );
         break;
      default:
         vpanic("setFlags_MUL(x86)");
   }
   stmt( IRStmt_Put( OFFB_CC_DEP1, widenUto32(mkexpr(arg1)) ));
   stmt( IRStmt_Put( OFFB_CC_DEP2, widenUto32(mkexpr(arg2)) ));
   stmt( IRStmt_Put( OFFB_CC_NDEP, mkU32(0) ));
}




static const HChar* name_X86Condcode ( X86Condcode cond )
{
   switch (cond) {
      case X86CondO:      return "o";
      case X86CondNO:     return "no";
      case X86CondB:      return "b";
      case X86CondNB:     return "nb";
      case X86CondZ:      return "z";
      case X86CondNZ:     return "nz";
      case X86CondBE:     return "be";
      case X86CondNBE:    return "nbe";
      case X86CondS:      return "s";
      case X86CondNS:     return "ns";
      case X86CondP:      return "p";
      case X86CondNP:     return "np";
      case X86CondL:      return "l";
      case X86CondNL:     return "nl";
      case X86CondLE:     return "le";
      case X86CondNLE:    return "nle";
      case X86CondAlways: return "ALWAYS";
      default: vpanic("name_X86Condcode");
   }
}

static 
X86Condcode positiveIse_X86Condcode ( X86Condcode  cond,
                                      Bool*        needInvert )
{
   vassert(cond >= X86CondO && cond <= X86CondNLE);
   if (cond & 1) {
      *needInvert = True;
      return cond-1;
   } else {
      *needInvert = False;
      return cond;
   }
}



static void helper_ADC ( Int sz,
                         IRTemp tres, IRTemp ta1, IRTemp ta2,
                         
                         IRTemp taddr, IRTemp texpVal, Addr32 restart_point )
{
   UInt    thunkOp;
   IRType  ty    = szToITy(sz);
   IRTemp  oldc  = newTemp(Ity_I32);
   IRTemp  oldcn = newTemp(ty);
   IROp    plus  = mkSizedOp(ty, Iop_Add8);
   IROp    xor   = mkSizedOp(ty, Iop_Xor8);

   vassert(typeOfIRTemp(irsb->tyenv, tres) == ty);
   vassert(sz == 1 || sz == 2 || sz == 4);
   thunkOp = sz==4 ? X86G_CC_OP_ADCL 
                   : (sz==2 ? X86G_CC_OP_ADCW : X86G_CC_OP_ADCB);

   
   assign( oldc,  binop(Iop_And32,
                        mk_x86g_calculate_eflags_c(),
                        mkU32(1)) );

   assign( oldcn, narrowTo(ty, mkexpr(oldc)) );

   assign( tres, binop(plus,
                       binop(plus,mkexpr(ta1),mkexpr(ta2)),
                       mkexpr(oldcn)) );

   if (taddr != IRTemp_INVALID) {
      if (texpVal == IRTemp_INVALID) {
         vassert(restart_point == 0);
         storeLE( mkexpr(taddr), mkexpr(tres) );
      } else {
         vassert(typeOfIRTemp(irsb->tyenv, texpVal) == ty);
         
         casLE( mkexpr(taddr),
                mkexpr(texpVal), mkexpr(tres), restart_point );
      }
   }

   stmt( IRStmt_Put( OFFB_CC_OP,   mkU32(thunkOp) ) );
   stmt( IRStmt_Put( OFFB_CC_DEP1, widenUto32(mkexpr(ta1)) ));
   stmt( IRStmt_Put( OFFB_CC_DEP2, widenUto32(binop(xor, mkexpr(ta2), 
                                                         mkexpr(oldcn)) )) );
   stmt( IRStmt_Put( OFFB_CC_NDEP, mkexpr(oldc) ) );
}


static void helper_SBB ( Int sz,
                         IRTemp tres, IRTemp ta1, IRTemp ta2,
                         
                         IRTemp taddr, IRTemp texpVal, Addr32 restart_point )
{
   UInt    thunkOp;
   IRType  ty    = szToITy(sz);
   IRTemp  oldc  = newTemp(Ity_I32);
   IRTemp  oldcn = newTemp(ty);
   IROp    minus = mkSizedOp(ty, Iop_Sub8);
   IROp    xor   = mkSizedOp(ty, Iop_Xor8);

   vassert(typeOfIRTemp(irsb->tyenv, tres) == ty);
   vassert(sz == 1 || sz == 2 || sz == 4);
   thunkOp = sz==4 ? X86G_CC_OP_SBBL 
                   : (sz==2 ? X86G_CC_OP_SBBW : X86G_CC_OP_SBBB);

   
   assign( oldc, binop(Iop_And32,
                       mk_x86g_calculate_eflags_c(),
                       mkU32(1)) );

   assign( oldcn, narrowTo(ty, mkexpr(oldc)) );

   assign( tres, binop(minus,
                       binop(minus,mkexpr(ta1),mkexpr(ta2)),
                       mkexpr(oldcn)) );

   if (taddr != IRTemp_INVALID) {
      if (texpVal == IRTemp_INVALID) {
         vassert(restart_point == 0);
         storeLE( mkexpr(taddr), mkexpr(tres) );
      } else {
         vassert(typeOfIRTemp(irsb->tyenv, texpVal) == ty);
         
         casLE( mkexpr(taddr),
                mkexpr(texpVal), mkexpr(tres), restart_point );
      }
   }

   stmt( IRStmt_Put( OFFB_CC_OP,   mkU32(thunkOp) ) );
   stmt( IRStmt_Put( OFFB_CC_DEP1, widenUto32(mkexpr(ta1) )) );
   stmt( IRStmt_Put( OFFB_CC_DEP2, widenUto32(binop(xor, mkexpr(ta2), 
                                                         mkexpr(oldcn)) )) );
   stmt( IRStmt_Put( OFFB_CC_NDEP, mkexpr(oldc) ) );
}



static const HChar* nameGrp1 ( Int opc_aux )
{
   static const HChar* grp1_names[8] 
     = { "add", "or", "adc", "sbb", "and", "sub", "xor", "cmp" };
   if (opc_aux < 0 || opc_aux > 7) vpanic("nameGrp1(x86)");
   return grp1_names[opc_aux];
}

static const HChar* nameGrp2 ( Int opc_aux )
{
   static const HChar* grp2_names[8] 
     = { "rol", "ror", "rcl", "rcr", "shl", "shr", "shl", "sar" };
   if (opc_aux < 0 || opc_aux > 7) vpanic("nameGrp2(x86)");
   return grp2_names[opc_aux];
}

static const HChar* nameGrp4 ( Int opc_aux )
{
   static const HChar* grp4_names[8] 
     = { "inc", "dec", "???", "???", "???", "???", "???", "???" };
   if (opc_aux < 0 || opc_aux > 1) vpanic("nameGrp4(x86)");
   return grp4_names[opc_aux];
}

static const HChar* nameGrp5 ( Int opc_aux )
{
   static const HChar* grp5_names[8] 
     = { "inc", "dec", "call*", "call*", "jmp*", "jmp*", "push", "???" };
   if (opc_aux < 0 || opc_aux > 6) vpanic("nameGrp5(x86)");
   return grp5_names[opc_aux];
}

static const HChar* nameGrp8 ( Int opc_aux )
{
   static const HChar* grp8_names[8] 
     = { "???", "???", "???", "???", "bt", "bts", "btr", "btc" };
   if (opc_aux < 4 || opc_aux > 7) vpanic("nameGrp8(x86)");
   return grp8_names[opc_aux];
}

static const HChar* nameIReg ( Int size, Int reg )
{
   static const HChar* ireg32_names[8] 
     = { "%eax", "%ecx", "%edx", "%ebx", 
         "%esp", "%ebp", "%esi", "%edi" };
   static const HChar* ireg16_names[8] 
     = { "%ax", "%cx", "%dx", "%bx", "%sp", "%bp", "%si", "%di" };
   static const HChar* ireg8_names[8] 
     = { "%al", "%cl", "%dl", "%bl", 
         "%ah{sp}", "%ch{bp}", "%dh{si}", "%bh{di}" };
   if (reg < 0 || reg > 7) goto bad;
   switch (size) {
      case 4: return ireg32_names[reg];
      case 2: return ireg16_names[reg];
      case 1: return ireg8_names[reg];
   }
  bad:
   vpanic("nameIReg(X86)");
   return NULL; 
}

static const HChar* nameSReg ( UInt sreg )
{
   switch (sreg) {
      case R_ES: return "%es";
      case R_CS: return "%cs";
      case R_SS: return "%ss";
      case R_DS: return "%ds";
      case R_FS: return "%fs";
      case R_GS: return "%gs";
      default: vpanic("nameSReg(x86)");
   }
}

static const HChar* nameMMXReg ( Int mmxreg )
{
   static const HChar* mmx_names[8] 
     = { "%mm0", "%mm1", "%mm2", "%mm3", "%mm4", "%mm5", "%mm6", "%mm7" };
   if (mmxreg < 0 || mmxreg > 7) vpanic("nameMMXReg(x86,guest)");
   return mmx_names[mmxreg];
}

static const HChar* nameXMMReg ( Int xmmreg )
{
   static const HChar* xmm_names[8] 
     = { "%xmm0", "%xmm1", "%xmm2", "%xmm3", 
         "%xmm4", "%xmm5", "%xmm6", "%xmm7" };
   if (xmmreg < 0 || xmmreg > 7) vpanic("name_of_xmm_reg");
   return xmm_names[xmmreg];
}
 
static const HChar* nameMMXGran ( Int gran )
{
   switch (gran) {
      case 0: return "b";
      case 1: return "w";
      case 2: return "d";
      case 3: return "q";
      default: vpanic("nameMMXGran(x86,guest)");
   }
}

static HChar nameISize ( Int size )
{
   switch (size) {
      case 4: return 'l';
      case 2: return 'w';
      case 1: return 'b';
      default: vpanic("nameISize(x86)");
   }
}



static void jmp_lit( DisResult* dres,
                     IRJumpKind kind, Addr32 d32 )
{
   vassert(dres->whatNext    == Dis_Continue);
   vassert(dres->len         == 0);
   vassert(dres->continueAt  == 0);
   vassert(dres->jk_StopHere == Ijk_INVALID);
   dres->whatNext    = Dis_StopHere;
   dres->jk_StopHere = kind;
   stmt( IRStmt_Put( OFFB_EIP, mkU32(d32) ) );
}

static void jmp_treg( DisResult* dres,
                      IRJumpKind kind, IRTemp t )
{
   vassert(dres->whatNext    == Dis_Continue);
   vassert(dres->len         == 0);
   vassert(dres->continueAt  == 0);
   vassert(dres->jk_StopHere == Ijk_INVALID);
   dres->whatNext    = Dis_StopHere;
   dres->jk_StopHere = kind;
   stmt( IRStmt_Put( OFFB_EIP, mkexpr(t) ) );
}

static 
void jcc_01( DisResult* dres,
             X86Condcode cond, Addr32 d32_false, Addr32 d32_true )
{
   Bool        invert;
   X86Condcode condPos;
   vassert(dres->whatNext    == Dis_Continue);
   vassert(dres->len         == 0);
   vassert(dres->continueAt  == 0);
   vassert(dres->jk_StopHere == Ijk_INVALID);
   dres->whatNext    = Dis_StopHere;
   dres->jk_StopHere = Ijk_Boring;
   condPos = positiveIse_X86Condcode ( cond, &invert );
   if (invert) {
      stmt( IRStmt_Exit( mk_x86g_calculate_condition(condPos),
                         Ijk_Boring,
                         IRConst_U32(d32_false),
                         OFFB_EIP ) );
      stmt( IRStmt_Put( OFFB_EIP, mkU32(d32_true) ) );
   } else {
      stmt( IRStmt_Exit( mk_x86g_calculate_condition(condPos),
                         Ijk_Boring,
                         IRConst_U32(d32_true),
                         OFFB_EIP ) );
      stmt( IRStmt_Put( OFFB_EIP, mkU32(d32_false) ) );
   }
}



static 
const HChar* sorbTxt ( UChar sorb )
{
   switch (sorb) {
      case 0:    return ""; 
      case 0x3E: return "%ds";
      case 0x26: return "%es:";
      case 0x64: return "%fs:";
      case 0x65: return "%gs:";
      default: vpanic("sorbTxt(x86,guest)");
   }
}


static
IRExpr* handleSegOverride ( UChar sorb, IRExpr* virtual )
{
   Int    sreg;
   IRType hWordTy;
   IRTemp ldt_ptr, gdt_ptr, seg_selector, r64;

   if (sorb == 0)
      
      return virtual;

   switch (sorb) {
      case 0x3E: sreg = R_DS; break;
      case 0x26: sreg = R_ES; break;
      case 0x64: sreg = R_FS; break;
      case 0x65: sreg = R_GS; break;
      default: vpanic("handleSegOverride(x86,guest)");
   }

   hWordTy = sizeof(HWord)==4 ? Ity_I32 : Ity_I64;

   seg_selector = newTemp(Ity_I32);
   ldt_ptr      = newTemp(hWordTy);
   gdt_ptr      = newTemp(hWordTy);
   r64          = newTemp(Ity_I64);

   assign( seg_selector, unop(Iop_16Uto32, getSReg(sreg)) );
   assign( ldt_ptr, IRExpr_Get( OFFB_LDT, hWordTy ));
   assign( gdt_ptr, IRExpr_Get( OFFB_GDT, hWordTy ));

   assign( 
      r64, 
      mkIRExprCCall( 
         Ity_I64, 
         0, 
         "x86g_use_seg_selector", 
         &x86g_use_seg_selector, 
         mkIRExprVec_4( mkexpr(ldt_ptr), mkexpr(gdt_ptr), 
                        mkexpr(seg_selector), virtual)
      )
   );

   stmt( 
      IRStmt_Exit(
         binop(Iop_CmpNE32, unop(Iop_64HIto32, mkexpr(r64)), mkU32(0)),
         Ijk_MapFail,
         IRConst_U32( guest_EIP_curr_instr ),
         OFFB_EIP
      )
   );

   
   return unop(Iop_64to32, mkexpr(r64));
}



static IRTemp disAMode_copy2tmp ( IRExpr* addr32 )
{
   IRTemp tmp = newTemp(Ity_I32);
   assign( tmp, addr32 );
   return tmp;
}

static 
IRTemp disAMode ( Int* len, UChar sorb, Int delta, HChar* buf )
{
   UChar mod_reg_rm = getIByte(delta);
   delta++;

   buf[0] = (UChar)0;

   mod_reg_rm &= 0xC7;                      
   mod_reg_rm  = toUChar(mod_reg_rm | (mod_reg_rm >> 3));  
                                            
   mod_reg_rm &= 0x1F;                      
   switch (mod_reg_rm) {

      case 0x00: case 0x01: case 0x02: case 0x03: 
        case 0x06: case 0x07:
         { UChar rm = mod_reg_rm;
           DIS(buf, "%s(%s)", sorbTxt(sorb), nameIReg(4,rm));
           *len = 1;
           return disAMode_copy2tmp(
                  handleSegOverride(sorb, getIReg(4,rm)));
         }

      case 0x08: case 0x09: case 0x0A: case 0x0B: 
       case 0x0D: case 0x0E: case 0x0F:
         { UChar rm = toUChar(mod_reg_rm & 7);
           UInt  d  = getSDisp8(delta);
           DIS(buf, "%s%d(%s)", sorbTxt(sorb), (Int)d, nameIReg(4,rm));
           *len = 2;
           return disAMode_copy2tmp(
                  handleSegOverride(sorb,
                     binop(Iop_Add32,getIReg(4,rm),mkU32(d))));
         }

      case 0x10: case 0x11: case 0x12: case 0x13: 
       case 0x15: case 0x16: case 0x17:
         { UChar rm = toUChar(mod_reg_rm & 7);
           UInt  d  = getUDisp32(delta);
           DIS(buf, "%s0x%x(%s)", sorbTxt(sorb), (Int)d, nameIReg(4,rm));
           *len = 5;
           return disAMode_copy2tmp(
                  handleSegOverride(sorb,
                     binop(Iop_Add32,getIReg(4,rm),mkU32(d))));
         }

      
      case 0x18: case 0x19: case 0x1A: case 0x1B:
      case 0x1C: case 0x1D: case 0x1E: case 0x1F:
         vpanic("disAMode(x86): not an addr!");

      case 0x05: 
         { UInt d = getUDisp32(delta);
           *len = 5;
           DIS(buf, "%s(0x%x)", sorbTxt(sorb), d);
           return disAMode_copy2tmp( 
                     handleSegOverride(sorb, mkU32(d)));
         }

      case 0x04: {
         UChar sib     = getIByte(delta);
         UChar scale   = toUChar((sib >> 6) & 3);
         UChar index_r = toUChar((sib >> 3) & 7);
         UChar base_r  = toUChar(sib & 7);
         delta++;

         if (index_r != R_ESP && base_r != R_EBP) {
            DIS(buf, "%s(%s,%s,%d)", sorbTxt(sorb), 
                      nameIReg(4,base_r), nameIReg(4,index_r), 1<<scale);
            *len = 2;
            return
               disAMode_copy2tmp( 
               handleSegOverride(sorb,
                  binop(Iop_Add32, 
                        getIReg(4,base_r),
                        binop(Iop_Shl32, getIReg(4,index_r),
                              mkU8(scale)))));
         }

         if (index_r != R_ESP && base_r == R_EBP) {
            UInt d = getUDisp32(delta);
            DIS(buf, "%s0x%x(,%s,%d)", sorbTxt(sorb), d, 
                      nameIReg(4,index_r), 1<<scale);
            *len = 6;
            return
               disAMode_copy2tmp(
               handleSegOverride(sorb, 
                  binop(Iop_Add32,
                        binop(Iop_Shl32, getIReg(4,index_r), mkU8(scale)),
                        mkU32(d))));
         }

         if (index_r == R_ESP && base_r != R_EBP) {
            DIS(buf, "%s(%s,,)", sorbTxt(sorb), nameIReg(4,base_r));
            *len = 2;
            return disAMode_copy2tmp(
                   handleSegOverride(sorb, getIReg(4,base_r)));
         }

         if (index_r == R_ESP && base_r == R_EBP) {
            UInt d = getUDisp32(delta);
            DIS(buf, "%s0x%x(,,)", sorbTxt(sorb), d);
            *len = 6;
            return disAMode_copy2tmp(
                   handleSegOverride(sorb, mkU32(d)));
         }
         
         vassert(0);
      }

      case 0x0C: {
         UChar sib     = getIByte(delta);
         UChar scale   = toUChar((sib >> 6) & 3);
         UChar index_r = toUChar((sib >> 3) & 7);
         UChar base_r  = toUChar(sib & 7);
         UInt  d       = getSDisp8(delta+1);

         if (index_r == R_ESP) {
            DIS(buf, "%s%d(%s,,)", sorbTxt(sorb), 
                                   (Int)d, nameIReg(4,base_r));
            *len = 3;
            return disAMode_copy2tmp(
                   handleSegOverride(sorb, 
                      binop(Iop_Add32, getIReg(4,base_r), mkU32(d)) ));
         } else {
            DIS(buf, "%s%d(%s,%s,%d)", sorbTxt(sorb), (Int)d, 
                     nameIReg(4,base_r), nameIReg(4,index_r), 1<<scale);
            *len = 3;
            return 
                disAMode_copy2tmp(
                handleSegOverride(sorb,
                  binop(Iop_Add32,
                        binop(Iop_Add32, 
                              getIReg(4,base_r), 
                              binop(Iop_Shl32, 
                                    getIReg(4,index_r), mkU8(scale))),
                        mkU32(d))));
         }
	 
         vassert(0);
      }

      case 0x14: {
         UChar sib     = getIByte(delta);
         UChar scale   = toUChar((sib >> 6) & 3);
         UChar index_r = toUChar((sib >> 3) & 7);
         UChar base_r  = toUChar(sib & 7);
         UInt d        = getUDisp32(delta+1);

         if (index_r == R_ESP) {
            DIS(buf, "%s%d(%s,,)", sorbTxt(sorb), 
                                   (Int)d, nameIReg(4,base_r));
            *len = 6;
            return disAMode_copy2tmp(
                   handleSegOverride(sorb, 
                      binop(Iop_Add32, getIReg(4,base_r), mkU32(d)) ));
         } else {
            DIS(buf, "%s%d(%s,%s,%d)", sorbTxt(sorb), (Int)d, 
                     nameIReg(4,base_r), nameIReg(4,index_r), 1<<scale);
            *len = 6;
            return 
                disAMode_copy2tmp(
                handleSegOverride(sorb,
                  binop(Iop_Add32,
                        binop(Iop_Add32, 
                              getIReg(4,base_r), 
                              binop(Iop_Shl32, 
                                    getIReg(4,index_r), mkU8(scale))),
                        mkU32(d))));
         }
	 
         vassert(0);
      }

      default:
         vpanic("disAMode(x86)");
         return 0; 
   }
}



static UInt lengthAMode ( Int delta )
{
   UChar mod_reg_rm = getIByte(delta); delta++;

   mod_reg_rm &= 0xC7;               
   mod_reg_rm  = toUChar(mod_reg_rm | (mod_reg_rm >> 3));  
                                     
   mod_reg_rm &= 0x1F;               
   switch (mod_reg_rm) {

      
      case 0x00: case 0x01: case 0x02: case 0x03: 
        case 0x06: case 0x07:
         return 1;

       
      case 0x08: case 0x09: case 0x0A: case 0x0B: 
       case 0x0D: case 0x0E: case 0x0F:
         return 2;

      
      case 0x10: case 0x11: case 0x12: case 0x13: 
       case 0x15: case 0x16: case 0x17:
         return 5;

      
      case 0x18: case 0x19: case 0x1A: case 0x1B:
      case 0x1C: case 0x1D: case 0x1E: case 0x1F:
         return 1;

      
      case 0x05: return 5;

      
      case 0x04: {
         UChar sib    = getIByte(delta);
         UChar base_r = toUChar(sib & 7);
         if (base_r == R_EBP) return 6; else return 2;
      }
      
      case 0x0C: return 3;

      
      case 0x14: return 6;

      default:
         vpanic("lengthAMode");
         return 0; 
   }
}


static
UInt dis_op2_E_G ( UChar       sorb,
                   Bool        addSubCarry,
                   IROp        op8, 
                   Bool        keep,
                   Int         size, 
                   Int         delta0,
                   const HChar* t_x86opc )
{
   HChar   dis_buf[50];
   Int     len;
   IRType  ty   = szToITy(size);
   IRTemp  dst1 = newTemp(ty);
   IRTemp  src  = newTemp(ty);
   IRTemp  dst0 = newTemp(ty);
   UChar   rm   = getUChar(delta0);
   IRTemp  addr = IRTemp_INVALID;

   if (addSubCarry) {
      vassert(op8 == Iop_Add8 || op8 == Iop_Sub8);
      vassert(keep);
   }

   if (epartIsReg(rm)) {
      if ((op8 == Iop_Xor8 || (op8 == Iop_Sub8 && addSubCarry))
          && gregOfRM(rm) == eregOfRM(rm)) {
         putIReg(size, gregOfRM(rm), mkU(ty,0));
      }
      assign( dst0, getIReg(size,gregOfRM(rm)) );
      assign( src,  getIReg(size,eregOfRM(rm)) );

      if (addSubCarry && op8 == Iop_Add8) {
         helper_ADC( size, dst1, dst0, src,
                     IRTemp_INVALID, IRTemp_INVALID, 0 );
         putIReg(size, gregOfRM(rm), mkexpr(dst1));
      } else
      if (addSubCarry && op8 == Iop_Sub8) {
         helper_SBB( size, dst1, dst0, src,
                     IRTemp_INVALID, IRTemp_INVALID, 0 );
         putIReg(size, gregOfRM(rm), mkexpr(dst1));
      } else {
         assign( dst1, binop(mkSizedOp(ty,op8), mkexpr(dst0), mkexpr(src)) );
         if (isAddSub(op8))
            setFlags_DEP1_DEP2(op8, dst0, src, ty);
         else
            setFlags_DEP1(op8, dst1, ty);
         if (keep)
            putIReg(size, gregOfRM(rm), mkexpr(dst1));
      }

      DIP("%s%c %s,%s\n", t_x86opc, nameISize(size), 
                          nameIReg(size,eregOfRM(rm)),
                          nameIReg(size,gregOfRM(rm)));
      return 1+delta0;
   } else {
      
      addr = disAMode ( &len, sorb, delta0, dis_buf);
      assign( dst0, getIReg(size,gregOfRM(rm)) );
      assign( src,  loadLE(szToITy(size), mkexpr(addr)) );

      if (addSubCarry && op8 == Iop_Add8) {
         helper_ADC( size, dst1, dst0, src,
                     IRTemp_INVALID, IRTemp_INVALID, 0 );
         putIReg(size, gregOfRM(rm), mkexpr(dst1));
      } else
      if (addSubCarry && op8 == Iop_Sub8) {
         helper_SBB( size, dst1, dst0, src,
                     IRTemp_INVALID, IRTemp_INVALID, 0 );
         putIReg(size, gregOfRM(rm), mkexpr(dst1));
      } else {
         assign( dst1, binop(mkSizedOp(ty,op8), mkexpr(dst0), mkexpr(src)) );
         if (isAddSub(op8))
            setFlags_DEP1_DEP2(op8, dst0, src, ty);
         else
            setFlags_DEP1(op8, dst1, ty);
         if (keep)
            putIReg(size, gregOfRM(rm), mkexpr(dst1));
      }

      DIP("%s%c %s,%s\n", t_x86opc, nameISize(size), 
                          dis_buf,nameIReg(size,gregOfRM(rm)));
      return len+delta0;
   }
}



static
UInt dis_op2_G_E ( UChar       sorb,
                   Bool        locked,
                   Bool        addSubCarry,
                   IROp        op8, 
                   Bool        keep,
                   Int         size, 
                   Int         delta0,
                   const HChar* t_x86opc )
{
   HChar   dis_buf[50];
   Int     len;
   IRType  ty   = szToITy(size);
   IRTemp  dst1 = newTemp(ty);
   IRTemp  src  = newTemp(ty);
   IRTemp  dst0 = newTemp(ty);
   UChar   rm   = getIByte(delta0);
   IRTemp  addr = IRTemp_INVALID;

   if (addSubCarry) {
      vassert(op8 == Iop_Add8 || op8 == Iop_Sub8);
      vassert(keep);
   }

   if (epartIsReg(rm)) {
      if ((op8 == Iop_Xor8 || (op8 == Iop_Sub8 && addSubCarry))
          && gregOfRM(rm) == eregOfRM(rm)) {
         putIReg(size, eregOfRM(rm), mkU(ty,0));
      }
      assign(dst0, getIReg(size,eregOfRM(rm)));
      assign(src,  getIReg(size,gregOfRM(rm)));

      if (addSubCarry && op8 == Iop_Add8) {
         helper_ADC( size, dst1, dst0, src,
                     IRTemp_INVALID, IRTemp_INVALID, 0 );
         putIReg(size, eregOfRM(rm), mkexpr(dst1));
      } else
      if (addSubCarry && op8 == Iop_Sub8) {
         helper_SBB( size, dst1, dst0, src,
                     IRTemp_INVALID, IRTemp_INVALID, 0 );
         putIReg(size, eregOfRM(rm), mkexpr(dst1));
      } else {
         assign(dst1, binop(mkSizedOp(ty,op8), mkexpr(dst0), mkexpr(src)));
         if (isAddSub(op8))
            setFlags_DEP1_DEP2(op8, dst0, src, ty);
         else
            setFlags_DEP1(op8, dst1, ty);
         if (keep)
            putIReg(size, eregOfRM(rm), mkexpr(dst1));
      }

      DIP("%s%c %s,%s\n", t_x86opc, nameISize(size), 
                          nameIReg(size,gregOfRM(rm)),
                          nameIReg(size,eregOfRM(rm)));
      return 1+delta0;
   }

       
   {
      addr = disAMode ( &len, sorb, delta0, dis_buf);
      assign(dst0, loadLE(ty,mkexpr(addr)));
      assign(src,  getIReg(size,gregOfRM(rm)));

      if (addSubCarry && op8 == Iop_Add8) {
         if (locked) {
            
            helper_ADC( size, dst1, dst0, src,
                        addr, dst0, guest_EIP_curr_instr );
         } else {
            
            helper_ADC( size, dst1, dst0, src,
                        addr, IRTemp_INVALID, 0 );
         }
      } else
      if (addSubCarry && op8 == Iop_Sub8) {
         if (locked) {
            
            helper_SBB( size, dst1, dst0, src,
                        addr, dst0, guest_EIP_curr_instr );
         } else {
            
            helper_SBB( size, dst1, dst0, src,
                        addr, IRTemp_INVALID, 0 );
         }
      } else {
         assign(dst1, binop(mkSizedOp(ty,op8), mkexpr(dst0), mkexpr(src)));
         if (keep) {
            if (locked) {
               if (0) vex_printf("locked case\n" );
               casLE( mkexpr(addr),
                      mkexpr(dst0), 
                      mkexpr(dst1), guest_EIP_curr_instr );
            } else {
               if (0) vex_printf("nonlocked case\n");
               storeLE(mkexpr(addr), mkexpr(dst1));
            }
         }
         if (isAddSub(op8))
            setFlags_DEP1_DEP2(op8, dst0, src, ty);
         else
            setFlags_DEP1(op8, dst1, ty);
      }

      DIP("%s%c %s,%s\n", t_x86opc, nameISize(size), 
                          nameIReg(size,gregOfRM(rm)), dis_buf);
      return len+delta0;
   }
}


static
UInt dis_mov_E_G ( UChar       sorb,
                   Int         size, 
                   Int         delta0 )
{
   Int len;
   UChar rm = getIByte(delta0);
   HChar dis_buf[50];

   if (epartIsReg(rm)) {
      putIReg(size, gregOfRM(rm), getIReg(size, eregOfRM(rm)));
      DIP("mov%c %s,%s\n", nameISize(size), 
                           nameIReg(size,eregOfRM(rm)),
                           nameIReg(size,gregOfRM(rm)));
      return 1+delta0;
   }

       
   {
      IRTemp addr = disAMode ( &len, sorb, delta0, dis_buf );
      putIReg(size, gregOfRM(rm), loadLE(szToITy(size), mkexpr(addr)));
      DIP("mov%c %s,%s\n", nameISize(size), 
                           dis_buf,nameIReg(size,gregOfRM(rm)));
      return delta0+len;
   }
}


static
UInt dis_mov_G_E ( UChar       sorb,
                   Int         size, 
                   Int         delta0 )
{
   Int len;
   UChar rm = getIByte(delta0);
   HChar dis_buf[50];

   if (epartIsReg(rm)) {
      putIReg(size, eregOfRM(rm), getIReg(size, gregOfRM(rm)));
      DIP("mov%c %s,%s\n", nameISize(size), 
                           nameIReg(size,gregOfRM(rm)),
                           nameIReg(size,eregOfRM(rm)));
      return 1+delta0;
   }

       
   {
      IRTemp addr = disAMode ( &len, sorb, delta0, dis_buf);
      storeLE( mkexpr(addr), getIReg(size, gregOfRM(rm)) );
      DIP("mov%c %s,%s\n", nameISize(size), 
                           nameIReg(size,gregOfRM(rm)), dis_buf);
      return len+delta0;
   }
}


static
UInt dis_op_imm_A ( Int    size,
                    Bool   carrying,
                    IROp   op8,
                    Bool   keep,
                    Int    delta,
                    const HChar* t_x86opc )
{
   IRType ty   = szToITy(size);
   IRTemp dst0 = newTemp(ty);
   IRTemp src  = newTemp(ty);
   IRTemp dst1 = newTemp(ty);
   UInt lit    = getUDisp(size,delta);
   assign(dst0, getIReg(size,R_EAX));
   assign(src,  mkU(ty,lit));

   if (isAddSub(op8) && !carrying) {
      assign(dst1, binop(mkSizedOp(ty,op8), mkexpr(dst0), mkexpr(src)) );
      setFlags_DEP1_DEP2(op8, dst0, src, ty);
   } 
   else
   if (isLogic(op8)) {
      vassert(!carrying);
      assign(dst1, binop(mkSizedOp(ty,op8), mkexpr(dst0), mkexpr(src)) );
      setFlags_DEP1(op8, dst1, ty);
   } 
   else
   if (op8 == Iop_Add8 && carrying) {
      helper_ADC( size, dst1, dst0, src,
                  IRTemp_INVALID, IRTemp_INVALID, 0 );
   }
   else
   if (op8 == Iop_Sub8 && carrying) {
      helper_SBB( size, dst1, dst0, src,
                  IRTemp_INVALID, IRTemp_INVALID, 0 );
   }
   else
      vpanic("dis_op_imm_A(x86,guest)");

   if (keep)
      putIReg(size, R_EAX, mkexpr(dst1));

   DIP("%s%c $0x%x, %s\n", t_x86opc, nameISize(size), 
                           lit, nameIReg(size,R_EAX));
   return delta+size;
}


static
UInt dis_movx_E_G ( UChar      sorb,
                    Int delta, Int szs, Int szd, Bool sign_extend )
{
   UChar rm = getIByte(delta);
   if (epartIsReg(rm)) {
      if (szd == szs) {
         
         putIReg(szd, gregOfRM(rm),
                           getIReg(szs,eregOfRM(rm)));
      } else {
         
         putIReg(szd, gregOfRM(rm),
                      unop(mkWidenOp(szs,szd,sign_extend), 
                           getIReg(szs,eregOfRM(rm))));
      }
      DIP("mov%c%c%c %s,%s\n", sign_extend ? 's' : 'z',
                               nameISize(szs), nameISize(szd),
                               nameIReg(szs,eregOfRM(rm)),
                               nameIReg(szd,gregOfRM(rm)));
      return 1+delta;
   }

       
   {
      Int    len;
      HChar  dis_buf[50];
      IRTemp addr = disAMode ( &len, sorb, delta, dis_buf );
      if (szd == szs) {
         
         putIReg(szd, gregOfRM(rm),
                           loadLE(szToITy(szs),mkexpr(addr)));
      } else {
         
         putIReg(szd, gregOfRM(rm),
                      unop(mkWidenOp(szs,szd,sign_extend), 
                           loadLE(szToITy(szs),mkexpr(addr))));
      }
      DIP("mov%c%c%c %s,%s\n", sign_extend ? 's' : 'z',
                               nameISize(szs), nameISize(szd),
                               dis_buf, nameIReg(szd,gregOfRM(rm)));
      return len+delta;
   }
}


static
void codegen_div ( Int sz, IRTemp t, Bool signed_divide )
{
   IROp   op    = signed_divide ? Iop_DivModS64to32 : Iop_DivModU64to32;
   IRTemp src64 = newTemp(Ity_I64);
   IRTemp dst64 = newTemp(Ity_I64);
   switch (sz) {
      case 4:
         assign( src64, binop(Iop_32HLto64, 
                              getIReg(4,R_EDX), getIReg(4,R_EAX)) );
         assign( dst64, binop(op, mkexpr(src64), mkexpr(t)) );
         putIReg( 4, R_EAX, unop(Iop_64to32,mkexpr(dst64)) );
         putIReg( 4, R_EDX, unop(Iop_64HIto32,mkexpr(dst64)) );
         break;
      case 2: {
         IROp widen3264 = signed_divide ? Iop_32Sto64 : Iop_32Uto64;
         IROp widen1632 = signed_divide ? Iop_16Sto32 : Iop_16Uto32;
         assign( src64, unop(widen3264,
                             binop(Iop_16HLto32, 
                                   getIReg(2,R_EDX), getIReg(2,R_EAX))) );
         assign( dst64, binop(op, mkexpr(src64), unop(widen1632,mkexpr(t))) );
         putIReg( 2, R_EAX, unop(Iop_32to16,unop(Iop_64to32,mkexpr(dst64))) );
         putIReg( 2, R_EDX, unop(Iop_32to16,unop(Iop_64HIto32,mkexpr(dst64))) );
         break;
      }
      case 1: {
         IROp widen3264 = signed_divide ? Iop_32Sto64 : Iop_32Uto64;
         IROp widen1632 = signed_divide ? Iop_16Sto32 : Iop_16Uto32;
         IROp widen816  = signed_divide ? Iop_8Sto16  : Iop_8Uto16;
         assign( src64, unop(widen3264, unop(widen1632, getIReg(2,R_EAX))) );
         assign( dst64, 
                 binop(op, mkexpr(src64), 
                           unop(widen1632, unop(widen816, mkexpr(t)))) );
         putIReg( 1, R_AL, unop(Iop_16to8, unop(Iop_32to16,
                           unop(Iop_64to32,mkexpr(dst64)))) );
         putIReg( 1, R_AH, unop(Iop_16to8, unop(Iop_32to16,
                           unop(Iop_64HIto32,mkexpr(dst64)))) );
         break;
      }
      default: vpanic("codegen_div(x86)");
   }
}


static 
UInt dis_Grp1 ( UChar sorb, Bool locked,
                Int delta, UChar modrm, 
                Int am_sz, Int d_sz, Int sz, UInt d32 )
{
   Int     len;
   HChar   dis_buf[50];
   IRType  ty   = szToITy(sz);
   IRTemp  dst1 = newTemp(ty);
   IRTemp  src  = newTemp(ty);
   IRTemp  dst0 = newTemp(ty);
   IRTemp  addr = IRTemp_INVALID;
   IROp    op8  = Iop_INVALID;
   UInt    mask = sz==1 ? 0xFF : (sz==2 ? 0xFFFF : 0xFFFFFFFF);

   switch (gregOfRM(modrm)) {
      case 0: op8 = Iop_Add8; break;  case 1: op8 = Iop_Or8;  break;
      case 2: break;  
      case 3: break;  
      case 4: op8 = Iop_And8; break;  case 5: op8 = Iop_Sub8; break;
      case 6: op8 = Iop_Xor8; break;  case 7: op8 = Iop_Sub8; break;
      
      default: vpanic("dis_Grp1: unhandled case");
   }

   if (epartIsReg(modrm)) {
      vassert(am_sz == 1);

      assign(dst0, getIReg(sz,eregOfRM(modrm)));
      assign(src,  mkU(ty,d32 & mask));

      if (gregOfRM(modrm) == 2 ) {
         helper_ADC( sz, dst1, dst0, src,
                     IRTemp_INVALID, IRTemp_INVALID, 0 );
      } else 
      if (gregOfRM(modrm) == 3 ) {
         helper_SBB( sz, dst1, dst0, src,
                     IRTemp_INVALID, IRTemp_INVALID, 0 );
      } else {
         assign(dst1, binop(mkSizedOp(ty,op8), mkexpr(dst0), mkexpr(src)));
         if (isAddSub(op8))
            setFlags_DEP1_DEP2(op8, dst0, src, ty);
         else
            setFlags_DEP1(op8, dst1, ty);
      }

      if (gregOfRM(modrm) < 7)
         putIReg(sz, eregOfRM(modrm), mkexpr(dst1));

      delta += (am_sz + d_sz);
      DIP("%s%c $0x%x, %s\n", nameGrp1(gregOfRM(modrm)), nameISize(sz), d32, 
                              nameIReg(sz,eregOfRM(modrm)));
   } else {
      addr = disAMode ( &len, sorb, delta, dis_buf);

      assign(dst0, loadLE(ty,mkexpr(addr)));
      assign(src, mkU(ty,d32 & mask));

      if (gregOfRM(modrm) == 2 ) {
         if (locked) {
            
            helper_ADC( sz, dst1, dst0, src,
                       addr, dst0, guest_EIP_curr_instr );
         } else {
            
            helper_ADC( sz, dst1, dst0, src,
                        addr, IRTemp_INVALID, 0 );
         }
      } else 
      if (gregOfRM(modrm) == 3 ) {
         if (locked) {
            
            helper_SBB( sz, dst1, dst0, src,
                       addr, dst0, guest_EIP_curr_instr );
         } else {
            
            helper_SBB( sz, dst1, dst0, src,
                        addr, IRTemp_INVALID, 0 );
         }
      } else {
         assign(dst1, binop(mkSizedOp(ty,op8), mkexpr(dst0), mkexpr(src)));
         if (gregOfRM(modrm) < 7) {
            if (locked) {
               casLE( mkexpr(addr), mkexpr(dst0), 
                                    mkexpr(dst1),
                                    guest_EIP_curr_instr );
            } else {
               storeLE(mkexpr(addr), mkexpr(dst1));
            }
         }
         if (isAddSub(op8))
            setFlags_DEP1_DEP2(op8, dst0, src, ty);
         else
            setFlags_DEP1(op8, dst1, ty);
      }

      delta += (len+d_sz);
      DIP("%s%c $0x%x, %s\n", nameGrp1(gregOfRM(modrm)), nameISize(sz),
                              d32, dis_buf);
   }
   return delta;
}



static
UInt dis_Grp2 ( UChar sorb,
                Int delta, UChar modrm,
                Int am_sz, Int d_sz, Int sz, IRExpr* shift_expr,
                const HChar* shift_expr_txt, Bool* decode_OK )
{
   
   HChar  dis_buf[50];
   Int    len;
   Bool   isShift, isRotate, isRotateC;
   IRType ty    = szToITy(sz);
   IRTemp dst0  = newTemp(ty);
   IRTemp dst1  = newTemp(ty);
   IRTemp addr  = IRTemp_INVALID;

   *decode_OK = True;

   vassert(sz == 1 || sz == 2 || sz == 4);

   
   if (epartIsReg(modrm)) {
      assign(dst0, getIReg(sz, eregOfRM(modrm)));
      delta += (am_sz + d_sz);
   } else {
      addr = disAMode ( &len, sorb, delta, dis_buf);
      assign(dst0, loadLE(ty,mkexpr(addr)));
      delta += len + d_sz;
   }

   isShift = False;
   switch (gregOfRM(modrm)) { case 4: case 5: case 6: case 7: isShift = True; }

   isRotate = False;
   switch (gregOfRM(modrm)) { case 0: case 1: isRotate = True; }

   isRotateC = False;
   switch (gregOfRM(modrm)) { case 2: case 3: isRotateC = True; }

   if (!isShift && !isRotate && !isRotateC) {
      
      vpanic("dis_Grp2(Reg): unhandled case(x86)");
   }

   if (isRotateC) {
      Bool     left = toBool(gregOfRM(modrm) == 2);
      IRTemp   r64  = newTemp(Ity_I64);
      IRExpr** args 
         = mkIRExprVec_4( widenUto32(mkexpr(dst0)), 
                          widenUto32(shift_expr),   
                          widenUto32(mk_x86g_calculate_eflags_all()),
                          mkU32(sz) );
      assign( r64, mkIRExprCCall(
                      Ity_I64, 
                      0, 
                      left ? "x86g_calculate_RCL" : "x86g_calculate_RCR", 
                      left ? &x86g_calculate_RCL  : &x86g_calculate_RCR,
                      args
                   )
            );
      
      assign( dst1, narrowTo(ty, unop(Iop_64to32, mkexpr(r64))) );
      stmt( IRStmt_Put( OFFB_CC_OP,   mkU32(X86G_CC_OP_COPY) ));
      stmt( IRStmt_Put( OFFB_CC_DEP1, unop(Iop_64HIto32, mkexpr(r64)) ));
      stmt( IRStmt_Put( OFFB_CC_DEP2, mkU32(0) ));
      stmt( IRStmt_Put( OFFB_CC_NDEP, mkU32(0) ));
   }

   if (isShift) {

      IRTemp pre32     = newTemp(Ity_I32);
      IRTemp res32     = newTemp(Ity_I32);
      IRTemp res32ss   = newTemp(Ity_I32);
      IRTemp shift_amt = newTemp(Ity_I8);
      IROp   op32;

      switch (gregOfRM(modrm)) { 
         case 4: op32 = Iop_Shl32; break;
         case 5: op32 = Iop_Shr32; break;
         case 6: op32 = Iop_Shl32; break;
         case 7: op32 = Iop_Sar32; break;
         
         default: vpanic("dis_Grp2:shift"); break;
      }


      
      assign( shift_amt, binop(Iop_And8, shift_expr, mkU8(31)) );

      
      assign( pre32, op32==Iop_Sar32 ? widenSto32(mkexpr(dst0))
                                     : widenUto32(mkexpr(dst0)) );

      
      assign( res32, binop(op32, mkexpr(pre32), mkexpr(shift_amt)) );

      
      assign( res32ss,
              binop(op32,
                    mkexpr(pre32), 
                    binop(Iop_And8,
                          binop(Iop_Sub8,
                                mkexpr(shift_amt), mkU8(1)),
                          mkU8(31))) );

      
      setFlags_DEP1_DEP2_shift(op32, res32, res32ss, ty, shift_amt);

      
      assign( dst1, narrowTo(ty, mkexpr(res32)) );

   } 

   else 
   if (isRotate) {
      Int    ccOp      = ty==Ity_I8 ? 0 : (ty==Ity_I16 ? 1 : 2);
      Bool   left      = toBool(gregOfRM(modrm) == 0);
      IRTemp rot_amt   = newTemp(Ity_I8);
      IRTemp rot_amt32 = newTemp(Ity_I8);
      IRTemp oldFlags  = newTemp(Ity_I32);

      
      assign(rot_amt32, binop(Iop_And8, shift_expr, mkU8(31)));

      if (ty == Ity_I32)
         assign(rot_amt, mkexpr(rot_amt32));
      else
         assign(rot_amt, binop(Iop_And8, mkexpr(rot_amt32), mkU8(8*sz-1)));

      if (left) {

         
         assign(dst1, 
            binop( mkSizedOp(ty,Iop_Or8),
                   binop( mkSizedOp(ty,Iop_Shl8), 
                          mkexpr(dst0),
                          mkexpr(rot_amt)
                   ),
                   binop( mkSizedOp(ty,Iop_Shr8), 
                          mkexpr(dst0), 
                          binop(Iop_Sub8,mkU8(8*sz), mkexpr(rot_amt))
                   )
            )
         );
         ccOp += X86G_CC_OP_ROLB;

      } else { 

         
         assign(dst1, 
            binop( mkSizedOp(ty,Iop_Or8),
                   binop( mkSizedOp(ty,Iop_Shr8), 
                          mkexpr(dst0),
                          mkexpr(rot_amt)
                   ),
                   binop( mkSizedOp(ty,Iop_Shl8), 
                          mkexpr(dst0), 
                          binop(Iop_Sub8,mkU8(8*sz), mkexpr(rot_amt))
                   )
            )
         );
         ccOp += X86G_CC_OP_RORB;

      }


      assign(oldFlags, mk_x86g_calculate_eflags_all());

      
      IRTemp rot_amt32b = newTemp(Ity_I1);
      assign(rot_amt32b, binop(Iop_CmpNE8, mkexpr(rot_amt32), mkU8(0)) );

      
      stmt( IRStmt_Put( OFFB_CC_OP,
                        IRExpr_ITE( mkexpr(rot_amt32b),
                                    mkU32(ccOp),
                                    IRExpr_Get(OFFB_CC_OP,Ity_I32) ) ));
      stmt( IRStmt_Put( OFFB_CC_DEP1, 
                        IRExpr_ITE( mkexpr(rot_amt32b),
                                    widenUto32(mkexpr(dst1)),
                                    IRExpr_Get(OFFB_CC_DEP1,Ity_I32) ) ));
      stmt( IRStmt_Put( OFFB_CC_DEP2, 
                        IRExpr_ITE( mkexpr(rot_amt32b),
                                    mkU32(0),
                                    IRExpr_Get(OFFB_CC_DEP2,Ity_I32) ) ));
      stmt( IRStmt_Put( OFFB_CC_NDEP, 
                        IRExpr_ITE( mkexpr(rot_amt32b),
                                    mkexpr(oldFlags),
                                    IRExpr_Get(OFFB_CC_NDEP,Ity_I32) ) ));
   } 

   
   if (epartIsReg(modrm)) {
      putIReg(sz, eregOfRM(modrm), mkexpr(dst1));
      if (vex_traceflags & VEX_TRACE_FE) {
         vex_printf("%s%c ",
                    nameGrp2(gregOfRM(modrm)), nameISize(sz) );
         if (shift_expr_txt)
            vex_printf("%s", shift_expr_txt);
         else
            ppIRExpr(shift_expr);
         vex_printf(", %s\n", nameIReg(sz,eregOfRM(modrm)));
      }
   } else {
      storeLE(mkexpr(addr), mkexpr(dst1));
      if (vex_traceflags & VEX_TRACE_FE) {
         vex_printf("%s%c ",
                    nameGrp2(gregOfRM(modrm)), nameISize(sz) );
         if (shift_expr_txt)
            vex_printf("%s", shift_expr_txt);
         else
            ppIRExpr(shift_expr);
         vex_printf(", %s\n", dis_buf);
      }
   }
   return delta;
}


static
UInt dis_Grp8_Imm ( UChar sorb,
                    Bool locked,
                    Int delta, UChar modrm,
                    Int am_sz, Int sz, UInt src_val,
                    Bool* decode_OK )
{

   IRType ty     = szToITy(sz);
   IRTemp t2     = newTemp(Ity_I32);
   IRTemp t2m    = newTemp(Ity_I32);
   IRTemp t_addr = IRTemp_INVALID;
   HChar  dis_buf[50];
   UInt   mask;

   
   *decode_OK = True;

   switch (sz) {
      case 2:  src_val &= 15; break;
      case 4:  src_val &= 31; break;
      default: *decode_OK = False; return delta;
   }

   
   switch (gregOfRM(modrm)) {
      case 4:   mask = 0;               break;
      case 5:  mask = 1 << src_val;    break;
      case 6:  mask = ~(1 << src_val); break;
      case 7:  mask = 1 << src_val;    break;
      default: *decode_OK = False; return delta;
   }

   if (epartIsReg(modrm)) {
      vassert(am_sz == 1);
      assign( t2, widenUto32(getIReg(sz, eregOfRM(modrm))) );
      delta += (am_sz + 1);
      DIP("%s%c $0x%x, %s\n", nameGrp8(gregOfRM(modrm)), nameISize(sz),
                              src_val, nameIReg(sz,eregOfRM(modrm)));
   } else {
      Int len;
      t_addr = disAMode ( &len, sorb, delta, dis_buf);
      delta  += (len+1);
      assign( t2, widenUto32(loadLE(ty, mkexpr(t_addr))) );
      DIP("%s%c $0x%x, %s\n", nameGrp8(gregOfRM(modrm)), nameISize(sz),
                              src_val, dis_buf);
   }

   
   switch (gregOfRM(modrm)) {
      case 4: 
         break;
      case 5: 
         assign( t2m, binop(Iop_Or32, mkU32(mask), mkexpr(t2)) );
         break;
      case 6: 
         assign( t2m, binop(Iop_And32, mkU32(mask), mkexpr(t2)) );
         break;
      case 7: 
         assign( t2m, binop(Iop_Xor32, mkU32(mask), mkexpr(t2)) );
         break;
      default: 
          
         vassert(0);
   }

   if (gregOfRM(modrm) != 4 ) {
      if (epartIsReg(modrm)) {
         putIReg(sz, eregOfRM(modrm), narrowTo(ty, mkexpr(t2m)));
      } else {
         if (locked) {
            casLE( mkexpr(t_addr),
                   narrowTo(ty, mkexpr(t2)),
                   narrowTo(ty, mkexpr(t2m)),
                   guest_EIP_curr_instr );
         } else {
            storeLE(mkexpr(t_addr), narrowTo(ty, mkexpr(t2m)));
         }
      }
   }

   
   
   stmt( IRStmt_Put( OFFB_CC_OP,   mkU32(X86G_CC_OP_COPY) ));
   stmt( IRStmt_Put( OFFB_CC_DEP2, mkU32(0) ));
   stmt( IRStmt_Put( 
            OFFB_CC_DEP1,
            binop(Iop_And32,
                  binop(Iop_Shr32, mkexpr(t2), mkU8(src_val)),
                  mkU32(1))
       ));
   stmt( IRStmt_Put( OFFB_CC_NDEP, mkU32(0) ));

   return delta;
}


static void codegen_mulL_A_D ( Int sz, Bool syned, 
                               IRTemp tmp, const HChar* tmp_txt )
{
   IRType ty = szToITy(sz);
   IRTemp t1 = newTemp(ty);

   assign( t1, getIReg(sz, R_EAX) );

   switch (ty) {
      case Ity_I32: {
         IRTemp res64   = newTemp(Ity_I64);
         IRTemp resHi   = newTemp(Ity_I32);
         IRTemp resLo   = newTemp(Ity_I32);
         IROp   mulOp   = syned ? Iop_MullS32 : Iop_MullU32;
         UInt   tBaseOp = syned ? X86G_CC_OP_SMULB : X86G_CC_OP_UMULB;
         setFlags_MUL ( Ity_I32, t1, tmp, tBaseOp );
         assign( res64, binop(mulOp, mkexpr(t1), mkexpr(tmp)) );
         assign( resHi, unop(Iop_64HIto32,mkexpr(res64)));
         assign( resLo, unop(Iop_64to32,mkexpr(res64)));
         putIReg(4, R_EDX, mkexpr(resHi));
         putIReg(4, R_EAX, mkexpr(resLo));
         break;
      }
      case Ity_I16: {
         IRTemp res32   = newTemp(Ity_I32);
         IRTemp resHi   = newTemp(Ity_I16);
         IRTemp resLo   = newTemp(Ity_I16);
         IROp   mulOp   = syned ? Iop_MullS16 : Iop_MullU16;
         UInt   tBaseOp = syned ? X86G_CC_OP_SMULB : X86G_CC_OP_UMULB;
         setFlags_MUL ( Ity_I16, t1, tmp, tBaseOp );
         assign( res32, binop(mulOp, mkexpr(t1), mkexpr(tmp)) );
         assign( resHi, unop(Iop_32HIto16,mkexpr(res32)));
         assign( resLo, unop(Iop_32to16,mkexpr(res32)));
         putIReg(2, R_EDX, mkexpr(resHi));
         putIReg(2, R_EAX, mkexpr(resLo));
         break;
      }
      case Ity_I8: {
         IRTemp res16   = newTemp(Ity_I16);
         IRTemp resHi   = newTemp(Ity_I8);
         IRTemp resLo   = newTemp(Ity_I8);
         IROp   mulOp   = syned ? Iop_MullS8 : Iop_MullU8;
         UInt   tBaseOp = syned ? X86G_CC_OP_SMULB : X86G_CC_OP_UMULB;
         setFlags_MUL ( Ity_I8, t1, tmp, tBaseOp );
         assign( res16, binop(mulOp, mkexpr(t1), mkexpr(tmp)) );
         assign( resHi, unop(Iop_16HIto8,mkexpr(res16)));
         assign( resLo, unop(Iop_16to8,mkexpr(res16)));
         putIReg(2, R_EAX, mkexpr(res16));
         break;
      }
      default:
         vpanic("codegen_mulL_A_D(x86)");
   }
   DIP("%s%c %s\n", syned ? "imul" : "mul", nameISize(sz), tmp_txt);
}


static 
UInt dis_Grp3 ( UChar sorb, Bool locked, Int sz, Int delta, Bool* decode_OK )
{
   UInt    d32;
   UChar   modrm;
   HChar   dis_buf[50];
   Int     len;
   IRTemp  addr;
   IRType  ty = szToITy(sz);
   IRTemp  t1 = newTemp(ty);
   IRTemp dst1, src, dst0;

   *decode_OK = True; 

   modrm = getIByte(delta);

   if (locked && (gregOfRM(modrm) != 2 && gregOfRM(modrm) != 3)) {
      
      *decode_OK = False;
      return delta;
   }

   if (epartIsReg(modrm)) {
      switch (gregOfRM(modrm)) {
         case 0: { 
            delta++; d32 = getUDisp(sz, delta); delta += sz;
            dst1 = newTemp(ty);
            assign(dst1, binop(mkSizedOp(ty,Iop_And8),
                               getIReg(sz,eregOfRM(modrm)),
                               mkU(ty,d32)));
            setFlags_DEP1( Iop_And8, dst1, ty );
            DIP("test%c $0x%x, %s\n", nameISize(sz), d32, 
                                      nameIReg(sz, eregOfRM(modrm)));
            break;
         }
         case 1: 
           *decode_OK = False;
           break;
         case 2: 
            delta++;
            putIReg(sz, eregOfRM(modrm),
                        unop(mkSizedOp(ty,Iop_Not8),
                             getIReg(sz, eregOfRM(modrm))));
            DIP("not%c %s\n", nameISize(sz), nameIReg(sz, eregOfRM(modrm)));
            break;
         case 3: 
            delta++;
            dst0 = newTemp(ty);
            src  = newTemp(ty);
            dst1 = newTemp(ty);
            assign(dst0, mkU(ty,0));
            assign(src,  getIReg(sz,eregOfRM(modrm)));
            assign(dst1, binop(mkSizedOp(ty,Iop_Sub8), mkexpr(dst0), mkexpr(src)));
            setFlags_DEP1_DEP2(Iop_Sub8, dst0, src, ty);
            putIReg(sz, eregOfRM(modrm), mkexpr(dst1));
            DIP("neg%c %s\n", nameISize(sz), nameIReg(sz, eregOfRM(modrm)));
            break;
         case 4: 
            delta++;
            src = newTemp(ty);
            assign(src, getIReg(sz,eregOfRM(modrm)));
            codegen_mulL_A_D ( sz, False, src, nameIReg(sz,eregOfRM(modrm)) );
            break;
         case 5: 
            delta++;
            src = newTemp(ty);
            assign(src, getIReg(sz,eregOfRM(modrm)));
            codegen_mulL_A_D ( sz, True, src, nameIReg(sz,eregOfRM(modrm)) );
            break;
         case 6: 
            delta++;
            assign( t1, getIReg(sz, eregOfRM(modrm)) );
            codegen_div ( sz, t1, False );
            DIP("div%c %s\n", nameISize(sz), nameIReg(sz, eregOfRM(modrm)));
            break;
         case 7: 
            delta++;
            assign( t1, getIReg(sz, eregOfRM(modrm)) );
            codegen_div ( sz, t1, True );
            DIP("idiv%c %s\n", nameISize(sz), nameIReg(sz, eregOfRM(modrm)));
            break;
         default: 
            
            vpanic("Grp3(x86)");
      }
   } else {
      addr = disAMode ( &len, sorb, delta, dis_buf );
      t1   = newTemp(ty);
      delta += len;
      assign(t1, loadLE(ty,mkexpr(addr)));
      switch (gregOfRM(modrm)) {
         case 0: { 
            d32 = getUDisp(sz, delta); delta += sz;
            dst1 = newTemp(ty);
            assign(dst1, binop(mkSizedOp(ty,Iop_And8),
                               mkexpr(t1), mkU(ty,d32)));
            setFlags_DEP1( Iop_And8, dst1, ty );
            DIP("test%c $0x%x, %s\n", nameISize(sz), d32, dis_buf);
            break;
         }
         case 1: 
           
           *decode_OK = False;
           break;
         case 2: 
            dst1 = newTemp(ty);
            assign(dst1, unop(mkSizedOp(ty,Iop_Not8), mkexpr(t1)));
            if (locked) {
               casLE( mkexpr(addr), mkexpr(t1), mkexpr(dst1),
                                    guest_EIP_curr_instr );
            } else {
               storeLE( mkexpr(addr), mkexpr(dst1) );
            }
            DIP("not%c %s\n", nameISize(sz), dis_buf);
            break;
         case 3: 
            dst0 = newTemp(ty);
            src  = newTemp(ty);
            dst1 = newTemp(ty);
            assign(dst0, mkU(ty,0));
            assign(src,  mkexpr(t1));
            assign(dst1, binop(mkSizedOp(ty,Iop_Sub8),
                               mkexpr(dst0), mkexpr(src)));
            if (locked) {
               casLE( mkexpr(addr), mkexpr(t1), mkexpr(dst1),
                                    guest_EIP_curr_instr );
            } else {
               storeLE( mkexpr(addr), mkexpr(dst1) );
            }
            setFlags_DEP1_DEP2(Iop_Sub8, dst0, src, ty);
            DIP("neg%c %s\n", nameISize(sz), dis_buf);
            break;
         case 4: 
            codegen_mulL_A_D ( sz, False, t1, dis_buf );
            break;
         case 5: 
            codegen_mulL_A_D ( sz, True, t1, dis_buf );
            break;
         case 6: 
            codegen_div ( sz, t1, False );
            DIP("div%c %s\n", nameISize(sz), dis_buf);
            break;
         case 7: 
            codegen_div ( sz, t1, True );
            DIP("idiv%c %s\n", nameISize(sz), dis_buf);
            break;
         default: 
            
            vpanic("Grp3(x86)");
      }
   }
   return delta;
}


static
UInt dis_Grp4 ( UChar sorb, Bool locked, Int delta, Bool* decode_OK )
{
   Int   alen;
   UChar modrm;
   HChar dis_buf[50];
   IRType ty = Ity_I8;
   IRTemp t1 = newTemp(ty);
   IRTemp t2 = newTemp(ty);

   *decode_OK = True;

   modrm = getIByte(delta);

   if (locked && (gregOfRM(modrm) != 0 && gregOfRM(modrm) != 1)) {
      
      *decode_OK = False;
      return delta;
   }

   if (epartIsReg(modrm)) {
      assign(t1, getIReg(1, eregOfRM(modrm)));
      switch (gregOfRM(modrm)) {
         case 0: 
            assign(t2, binop(Iop_Add8, mkexpr(t1), mkU8(1)));
            putIReg(1, eregOfRM(modrm), mkexpr(t2));
            setFlags_INC_DEC( True, t2, ty );
            break;
         case 1: 
            assign(t2, binop(Iop_Sub8, mkexpr(t1), mkU8(1)));
            putIReg(1, eregOfRM(modrm), mkexpr(t2));
            setFlags_INC_DEC( False, t2, ty );
            break;
         default: 
            *decode_OK = False;
            return delta;
      }
      delta++;
      DIP("%sb %s\n", nameGrp4(gregOfRM(modrm)),
                      nameIReg(1, eregOfRM(modrm)));
   } else {
      IRTemp addr = disAMode ( &alen, sorb, delta, dis_buf );
      assign( t1, loadLE(ty, mkexpr(addr)) );
      switch (gregOfRM(modrm)) {
         case 0: 
            assign(t2, binop(Iop_Add8, mkexpr(t1), mkU8(1)));
            if (locked) {
               casLE( mkexpr(addr), mkexpr(t1), mkexpr(t2), 
                      guest_EIP_curr_instr );
            } else {
               storeLE( mkexpr(addr), mkexpr(t2) );
            }
            setFlags_INC_DEC( True, t2, ty );
            break;
         case 1: 
            assign(t2, binop(Iop_Sub8, mkexpr(t1), mkU8(1)));
            if (locked) {
               casLE( mkexpr(addr), mkexpr(t1), mkexpr(t2), 
                      guest_EIP_curr_instr );
            } else {
               storeLE( mkexpr(addr), mkexpr(t2) );
            }
            setFlags_INC_DEC( False, t2, ty );
            break;
         default: 
            *decode_OK = False;
            return delta;
      }
      delta += alen;
      DIP("%sb %s\n", nameGrp4(gregOfRM(modrm)), dis_buf);
   }
   return delta;
}


static
UInt dis_Grp5 ( UChar sorb, Bool locked, Int sz, Int delta, 
                DisResult* dres, Bool* decode_OK )
{
   Int     len;
   UChar   modrm;
   HChar   dis_buf[50];
   IRTemp  addr = IRTemp_INVALID;
   IRType  ty = szToITy(sz);
   IRTemp  t1 = newTemp(ty);
   IRTemp  t2 = IRTemp_INVALID;

   *decode_OK = True;

   modrm = getIByte(delta);

   if (locked && (gregOfRM(modrm) != 0 && gregOfRM(modrm) != 1)) {
      
      *decode_OK = False;
      return delta;
   }

   if (epartIsReg(modrm)) {
      assign(t1, getIReg(sz,eregOfRM(modrm)));
      switch (gregOfRM(modrm)) {
         case 0:  
            vassert(sz == 2 || sz == 4);
            t2 = newTemp(ty);
            assign(t2, binop(mkSizedOp(ty,Iop_Add8),
                             mkexpr(t1), mkU(ty,1)));
            setFlags_INC_DEC( True, t2, ty );
            putIReg(sz,eregOfRM(modrm),mkexpr(t2));
            break;
         case 1:  
            vassert(sz == 2 || sz == 4);
            t2 = newTemp(ty);
            assign(t2, binop(mkSizedOp(ty,Iop_Sub8),
                             mkexpr(t1), mkU(ty,1)));
            setFlags_INC_DEC( False, t2, ty );
            putIReg(sz,eregOfRM(modrm),mkexpr(t2));
            break;
         case 2: 
            vassert(sz == 4);
            t2 = newTemp(Ity_I32);
            assign(t2, binop(Iop_Sub32, getIReg(4,R_ESP), mkU32(4)));
            putIReg(4, R_ESP, mkexpr(t2));
            storeLE( mkexpr(t2), mkU32(guest_EIP_bbstart+delta+1));
            jmp_treg(dres, Ijk_Call, t1);
            vassert(dres->whatNext == Dis_StopHere);
            break;
         case 4: 
            vassert(sz == 4);
            jmp_treg(dres, Ijk_Boring, t1);
            vassert(dres->whatNext == Dis_StopHere);
            break;
         case 6: 
            vassert(sz == 4 || sz == 2);
            t2 = newTemp(Ity_I32);
            assign( t2, binop(Iop_Sub32,getIReg(4,R_ESP),mkU32(sz)) );
            putIReg(4, R_ESP, mkexpr(t2) );
            storeLE( mkexpr(t2), mkexpr(t1) );
            break;
         default: 
            *decode_OK = False;
            return delta;
      }
      delta++;
      DIP("%s%c %s\n", nameGrp5(gregOfRM(modrm)),
                       nameISize(sz), nameIReg(sz, eregOfRM(modrm)));
   } else {
      addr = disAMode ( &len, sorb, delta, dis_buf );
      assign(t1, loadLE(ty,mkexpr(addr)));
      switch (gregOfRM(modrm)) {
         case 0:  
            t2 = newTemp(ty);
            assign(t2, binop(mkSizedOp(ty,Iop_Add8),
                             mkexpr(t1), mkU(ty,1)));
            if (locked) {
               casLE( mkexpr(addr),
                      mkexpr(t1), mkexpr(t2), guest_EIP_curr_instr );
            } else {
               storeLE(mkexpr(addr),mkexpr(t2));
            }
            setFlags_INC_DEC( True, t2, ty );
            break;
         case 1:  
            t2 = newTemp(ty);
            assign(t2, binop(mkSizedOp(ty,Iop_Sub8),
                             mkexpr(t1), mkU(ty,1)));
            if (locked) {
               casLE( mkexpr(addr),
                      mkexpr(t1), mkexpr(t2), guest_EIP_curr_instr );
            } else {
               storeLE(mkexpr(addr),mkexpr(t2));
            }
            setFlags_INC_DEC( False, t2, ty );
            break;
         case 2: 
            vassert(sz == 4);
            t2 = newTemp(Ity_I32);
            assign(t2, binop(Iop_Sub32, getIReg(4,R_ESP), mkU32(4)));
            putIReg(4, R_ESP, mkexpr(t2));
            storeLE( mkexpr(t2), mkU32(guest_EIP_bbstart+delta+len));
            jmp_treg(dres, Ijk_Call, t1);
            vassert(dres->whatNext == Dis_StopHere);
            break;
         case 4: 
            vassert(sz == 4);
            jmp_treg(dres, Ijk_Boring, t1);
            vassert(dres->whatNext == Dis_StopHere);
            break;
         case 6: 
            vassert(sz == 4 || sz == 2);
            t2 = newTemp(Ity_I32);
            assign( t2, binop(Iop_Sub32,getIReg(4,R_ESP),mkU32(sz)) );
            putIReg(4, R_ESP, mkexpr(t2) );
            storeLE( mkexpr(t2), mkexpr(t1) );
            break;
         default: 
            *decode_OK = False;
            return delta;
      }
      delta += len;
      DIP("%s%c %s\n", nameGrp5(gregOfRM(modrm)),
                       nameISize(sz), dis_buf);
   }
   return delta;
}



static
void dis_string_op_increment(Int sz, Int t_inc)
{
   if (sz == 4 || sz == 2) {
      assign( t_inc, 
              binop(Iop_Shl32, IRExpr_Get( OFFB_DFLAG, Ity_I32 ),
                               mkU8(sz/2) ) );
   } else {
      assign( t_inc, 
              IRExpr_Get( OFFB_DFLAG, Ity_I32 ) );
   }
}

static
void dis_string_op( void (*dis_OP)( Int, IRTemp ), 
                    Int sz, const HChar* name, UChar sorb )
{
   IRTemp t_inc = newTemp(Ity_I32);
   vassert(sorb == 0); 
   dis_string_op_increment(sz, t_inc);
   dis_OP( sz, t_inc );
   DIP("%s%c\n", name, nameISize(sz));
}

static 
void dis_MOVS ( Int sz, IRTemp t_inc )
{
   IRType ty = szToITy(sz);
   IRTemp td = newTemp(Ity_I32);   
   IRTemp ts = newTemp(Ity_I32);   

   assign( td, getIReg(4, R_EDI) );
   assign( ts, getIReg(4, R_ESI) );

   storeLE( mkexpr(td), loadLE(ty,mkexpr(ts)) );

   putIReg( 4, R_EDI, binop(Iop_Add32, mkexpr(td), mkexpr(t_inc)) );
   putIReg( 4, R_ESI, binop(Iop_Add32, mkexpr(ts), mkexpr(t_inc)) );
}

static 
void dis_LODS ( Int sz, IRTemp t_inc )
{
   IRType ty = szToITy(sz);
   IRTemp ts = newTemp(Ity_I32);   

   assign( ts, getIReg(4, R_ESI) );

   putIReg( sz, R_EAX, loadLE(ty, mkexpr(ts)) );

   putIReg( 4, R_ESI, binop(Iop_Add32, mkexpr(ts), mkexpr(t_inc)) );
}

static 
void dis_STOS ( Int sz, IRTemp t_inc )
{
   IRType ty = szToITy(sz);
   IRTemp ta = newTemp(ty);        
   IRTemp td = newTemp(Ity_I32);   

   assign( ta, getIReg(sz, R_EAX) );
   assign( td, getIReg(4, R_EDI) );

   storeLE( mkexpr(td), mkexpr(ta) );

   putIReg( 4, R_EDI, binop(Iop_Add32, mkexpr(td), mkexpr(t_inc)) );
}

static 
void dis_CMPS ( Int sz, IRTemp t_inc )
{
   IRType ty  = szToITy(sz);
   IRTemp tdv = newTemp(ty);      
   IRTemp tsv = newTemp(ty);      
   IRTemp td  = newTemp(Ity_I32); 
   IRTemp ts  = newTemp(Ity_I32); 

   assign( td, getIReg(4, R_EDI) );
   assign( ts, getIReg(4, R_ESI) );

   assign( tdv, loadLE(ty,mkexpr(td)) );
   assign( tsv, loadLE(ty,mkexpr(ts)) );

   setFlags_DEP1_DEP2 ( Iop_Sub8, tsv, tdv, ty );

   putIReg(4, R_EDI, binop(Iop_Add32, mkexpr(td), mkexpr(t_inc)) );
   putIReg(4, R_ESI, binop(Iop_Add32, mkexpr(ts), mkexpr(t_inc)) );
}

static 
void dis_SCAS ( Int sz, IRTemp t_inc )
{
   IRType ty  = szToITy(sz);
   IRTemp ta  = newTemp(ty);       
   IRTemp td  = newTemp(Ity_I32);  
   IRTemp tdv = newTemp(ty);       

   assign( ta, getIReg(sz, R_EAX) );
   assign( td, getIReg(4, R_EDI) );

   assign( tdv, loadLE(ty,mkexpr(td)) );
   setFlags_DEP1_DEP2 ( Iop_Sub8, ta, tdv, ty );

   putIReg(4, R_EDI, binop(Iop_Add32, mkexpr(td), mkexpr(t_inc)) );
}


static 
void dis_REP_op ( DisResult* dres,
                  X86Condcode cond,
                  void (*dis_OP)(Int, IRTemp),
                  Int sz, Addr32 eip, Addr32 eip_next, const HChar* name )
{
   IRTemp t_inc = newTemp(Ity_I32);
   IRTemp tc    = newTemp(Ity_I32);  

   assign( tc, getIReg(4,R_ECX) );

   stmt( IRStmt_Exit( binop(Iop_CmpEQ32,mkexpr(tc),mkU32(0)),
                      Ijk_Boring,
                      IRConst_U32(eip_next), OFFB_EIP ) );

   putIReg(4, R_ECX, binop(Iop_Sub32, mkexpr(tc), mkU32(1)) );

   dis_string_op_increment(sz, t_inc);
   dis_OP (sz, t_inc);

   if (cond == X86CondAlways) {
      jmp_lit(dres, Ijk_Boring, eip);
      vassert(dres->whatNext == Dis_StopHere);
   } else {
      stmt( IRStmt_Exit( mk_x86g_calculate_condition(cond),
                         Ijk_Boring,
                         IRConst_U32(eip), OFFB_EIP ) );
      jmp_lit(dres, Ijk_Boring, eip_next);
      vassert(dres->whatNext == Dis_StopHere);
   }
   DIP("%s%c\n", name, nameISize(sz));
}



static
UInt dis_mul_E_G ( UChar       sorb,
                   Int         size, 
                   Int         delta0 )
{
   Int    alen;
   HChar  dis_buf[50];
   UChar  rm = getIByte(delta0);
   IRType ty = szToITy(size);
   IRTemp te = newTemp(ty);
   IRTemp tg = newTemp(ty);
   IRTemp resLo = newTemp(ty);

   assign( tg, getIReg(size, gregOfRM(rm)) );
   if (epartIsReg(rm)) {
      assign( te, getIReg(size, eregOfRM(rm)) );
   } else {
      IRTemp addr = disAMode( &alen, sorb, delta0, dis_buf );
      assign( te, loadLE(ty,mkexpr(addr)) );
   }

   setFlags_MUL ( ty, te, tg, X86G_CC_OP_SMULB );

   assign( resLo, binop( mkSizedOp(ty, Iop_Mul8), mkexpr(te), mkexpr(tg) ) );

   putIReg(size, gregOfRM(rm), mkexpr(resLo) );

   if (epartIsReg(rm)) {
      DIP("imul%c %s, %s\n", nameISize(size), 
                             nameIReg(size,eregOfRM(rm)),
                             nameIReg(size,gregOfRM(rm)));
      return 1+delta0;
   } else {
      DIP("imul%c %s, %s\n", nameISize(size), 
                             dis_buf, nameIReg(size,gregOfRM(rm)));
      return alen+delta0;
   }
}


static
UInt dis_imul_I_E_G ( UChar       sorb,
                      Int         size, 
                      Int         delta,
                      Int         litsize )
{
   Int    d32, alen;
   HChar  dis_buf[50];
   UChar  rm = getIByte(delta);
   IRType ty = szToITy(size);
   IRTemp te = newTemp(ty);
   IRTemp tl = newTemp(ty);
   IRTemp resLo = newTemp(ty);

   vassert(size == 1 || size == 2 || size == 4);

   if (epartIsReg(rm)) {
      assign(te, getIReg(size, eregOfRM(rm)));
      delta++;
   } else {
      IRTemp addr = disAMode( &alen, sorb, delta, dis_buf );
      assign(te, loadLE(ty, mkexpr(addr)));
      delta += alen;
   }
   d32 = getSDisp(litsize,delta);
   delta += litsize;

   if (size == 1) d32 &= 0xFF;
   if (size == 2) d32 &= 0xFFFF;

   assign(tl, mkU(ty,d32));

   assign( resLo, binop( mkSizedOp(ty, Iop_Mul8), mkexpr(te), mkexpr(tl) ));

   setFlags_MUL ( ty, te, tl, X86G_CC_OP_SMULB );

   putIReg(size, gregOfRM(rm), mkexpr(resLo));

   DIP("imul %d, %s, %s\n", d32, 
       ( epartIsReg(rm) ? nameIReg(size,eregOfRM(rm)) : dis_buf ),
       nameIReg(size,gregOfRM(rm)) );
   return delta;
}


static IRTemp gen_LZCNT ( IRType ty, IRTemp src )
{
   vassert(ty == Ity_I32 || ty == Ity_I16);

   IRTemp src32 = newTemp(Ity_I32);
   assign(src32, widenUto32( mkexpr(src) ));

   IRTemp src32x = newTemp(Ity_I32);
   assign(src32x, 
          binop(Iop_Shl32, mkexpr(src32),
                           mkU8(32 - 8 * sizeofIRType(ty))));

   
   
   IRTemp res32 = newTemp(Ity_I32);
   assign(res32,
          IRExpr_ITE(
             binop(Iop_CmpEQ32, mkexpr(src32x), mkU32(0)),
             mkU32(8 * sizeofIRType(ty)),
             unop(Iop_Clz32, mkexpr(src32x))
   ));

   IRTemp res = newTemp(ty);
   assign(res, narrowTo(ty, mkexpr(res32)));
   return res;
}





static void put_emwarn ( IRExpr* e  )
{
   vassert(typeOfIRExpr(irsb->tyenv, e) == Ity_I32);
   stmt( IRStmt_Put( OFFB_EMNOTE, e ) );
}


static IRExpr* mkQNaN64 ( void )
{
   return IRExpr_Const(IRConst_F64i(0x7FF8000000000000ULL));
}


static IRExpr* get_ftop ( void )
{
   return IRExpr_Get( OFFB_FTOP, Ity_I32 );
}

static void put_ftop ( IRExpr* e )
{
   vassert(typeOfIRExpr(irsb->tyenv, e) == Ity_I32);
   stmt( IRStmt_Put( OFFB_FTOP, e ) );
}


static IRExpr* get_C3210 ( void )
{
   return IRExpr_Get( OFFB_FC3210, Ity_I32 );
}

static void put_C3210 ( IRExpr* e )
{
   stmt( IRStmt_Put( OFFB_FC3210, e ) );
}

static IRExpr*  get_fpround ( void )
{
   return IRExpr_Get( OFFB_FPROUND, Ity_I32 );
}

static void put_fpround ( IRExpr*  e )
{
   stmt( IRStmt_Put( OFFB_FPROUND, e ) );
}


static IRExpr*  get_roundingmode ( void )
{
   return binop( Iop_And32, get_fpround(), mkU32(3) );
}

static IRExpr*  get_FAKE_roundingmode ( void )
{
   return mkU32(Irrm_NEAREST);
}




static void put_ST_TAG ( Int i, IRExpr* value )
{
   IRRegArray* descr;
   vassert(typeOfIRExpr(irsb->tyenv, value) == Ity_I8);
   descr = mkIRRegArray( OFFB_FPTAGS, Ity_I8, 8 );
   stmt( IRStmt_PutI( mkIRPutI(descr, get_ftop(), i, value) ) );
}


static IRExpr* get_ST_TAG ( Int i )
{
   IRRegArray* descr = mkIRRegArray( OFFB_FPTAGS, Ity_I8, 8 );
   return IRExpr_GetI( descr, get_ftop(), i );
}




static void put_ST_UNCHECKED ( Int i, IRExpr* value )
{
   IRRegArray* descr;
   vassert(typeOfIRExpr(irsb->tyenv, value) == Ity_F64);
   descr = mkIRRegArray( OFFB_FPREGS, Ity_F64, 8 );
   stmt( IRStmt_PutI( mkIRPutI(descr, get_ftop(), i, value) ) );
   
   put_ST_TAG(i, mkU8(1));
}


static void put_ST ( Int i, IRExpr* value )
{
   put_ST_UNCHECKED(
      i,
      IRExpr_ITE( binop(Iop_CmpNE8, get_ST_TAG(i), mkU8(0)),
                  
                  mkQNaN64(),
                  
                  value
      )
   );
}



static IRExpr* get_ST_UNCHECKED ( Int i )
{
   IRRegArray* descr = mkIRRegArray( OFFB_FPREGS, Ity_F64, 8 );
   return IRExpr_GetI( descr, get_ftop(), i );
}



static IRExpr* get_ST ( Int i )
{
   return
      IRExpr_ITE( binop(Iop_CmpNE8, get_ST_TAG(i), mkU8(0)),
                  
                  get_ST_UNCHECKED(i),
                  
                  mkQNaN64());
}



static void maybe_put_ST ( IRTemp cond, Int i, IRExpr* value )
{
   
   
   

   IRTemp old_tag = newTemp(Ity_I8);
   assign(old_tag, get_ST_TAG(i));
   IRTemp new_tag = newTemp(Ity_I8);
   assign(new_tag,
          IRExpr_ITE(mkexpr(cond), mkU8(1), mkexpr(old_tag)));

   IRTemp old_val = newTemp(Ity_F64);
   assign(old_val, get_ST_UNCHECKED(i));
   IRTemp new_val = newTemp(Ity_F64);
   assign(new_val,
          IRExpr_ITE(mkexpr(cond),
                     IRExpr_ITE(binop(Iop_CmpNE8, mkexpr(old_tag), mkU8(0)),
                                
                                mkQNaN64(),
                                
                                value),
                     mkexpr(old_val)));

   put_ST_UNCHECKED(i, mkexpr(new_val));
   
   
   put_ST_TAG(i, mkexpr(new_tag));
}


static void fp_push ( void )
{
   put_ftop( binop(Iop_Sub32, get_ftop(), mkU32(1)) );
}


static void maybe_fp_push ( IRTemp cond )
{
   put_ftop( binop(Iop_Sub32, get_ftop(), unop(Iop_1Uto32,mkexpr(cond))) );
}


static void fp_pop ( void )
{
   put_ST_TAG(0, mkU8(0));
   put_ftop( binop(Iop_Add32, get_ftop(), mkU32(1)) );
}

static void set_C2 ( IRExpr* e )
{
   IRExpr* cleared = binop(Iop_And32, get_C3210(), mkU32(~X86G_FC_MASK_C2));
   put_C3210( binop(Iop_Or32,
                    cleared,
                    binop(Iop_Shl32, e, mkU8(X86G_FC_SHIFT_C2))) );
}

static IRTemp math_IS_TRIG_ARG_FINITE_AND_IN_RANGE ( IRTemp d64 )
{
   IRTemp i64 = newTemp(Ity_I64);
   assign(i64, unop(Iop_ReinterpF64asI64, mkexpr(d64)) );
   IRTemp exponent = newTemp(Ity_I32);
   assign(exponent,
          binop(Iop_And32,
                binop(Iop_Shr32, unop(Iop_64HIto32, mkexpr(i64)), mkU8(20)),
                mkU32(0x7FF)));
   IRTemp in_range_and_finite = newTemp(Ity_I1);
   assign(in_range_and_finite,
          binop(Iop_CmpLE32U, mkexpr(exponent), mkU32(0x43D)));
   return in_range_and_finite;
}

static IRExpr* get_FPU_sw ( void )
{
   return
      unop(Iop_32to16,
           binop(Iop_Or32,
                 binop(Iop_Shl32, 
                       binop(Iop_And32, get_ftop(), mkU32(7)), 
                             mkU8(11)),
                       binop(Iop_And32, get_C3210(), mkU32(0x4700))
      ));
}



static
void fp_do_op_mem_ST_0 ( IRTemp addr, const HChar* op_txt, HChar* dis_buf, 
                         IROp op, Bool dbl )
{
   DIP("f%s%c %s\n", op_txt, dbl?'l':'s', dis_buf);
   if (dbl) {
      put_ST_UNCHECKED(0, 
         triop( op, 
                get_FAKE_roundingmode(), 
                get_ST(0), 
                loadLE(Ity_F64,mkexpr(addr))
         ));
   } else {
      put_ST_UNCHECKED(0, 
         triop( op, 
                get_FAKE_roundingmode(), 
                get_ST(0), 
                unop(Iop_F32toF64, loadLE(Ity_F32,mkexpr(addr)))
         ));
   }
}


static
void fp_do_oprev_mem_ST_0 ( IRTemp addr, const HChar* op_txt, HChar* dis_buf,
                            IROp op, Bool dbl )
{
   DIP("f%s%c %s\n", op_txt, dbl?'l':'s', dis_buf);
   if (dbl) {
      put_ST_UNCHECKED(0, 
         triop( op, 
                get_FAKE_roundingmode(), 
                loadLE(Ity_F64,mkexpr(addr)),
                get_ST(0)
         ));
   } else {
      put_ST_UNCHECKED(0, 
         triop( op, 
                get_FAKE_roundingmode(), 
                unop(Iop_F32toF64, loadLE(Ity_F32,mkexpr(addr))),
                get_ST(0)
         ));
   }
}


static
void fp_do_op_ST_ST ( const HChar* op_txt, IROp op, UInt st_src, UInt st_dst,
                      Bool pop_after )
{
   DIP("f%s%s st(%d), st(%d)\n", op_txt, pop_after?"p":"", 
                                 (Int)st_src, (Int)st_dst );
   put_ST_UNCHECKED( 
      st_dst, 
      triop( op, 
             get_FAKE_roundingmode(), 
             get_ST(st_dst), 
             get_ST(st_src) ) 
   );
   if (pop_after)
      fp_pop();
}

static
void fp_do_oprev_ST_ST ( const HChar* op_txt, IROp op, UInt st_src,
                         UInt st_dst, Bool pop_after )
{
   DIP("f%s%s st(%d), st(%d)\n", op_txt, pop_after?"p":"",
                                 (Int)st_src, (Int)st_dst );
   put_ST_UNCHECKED( 
      st_dst, 
      triop( op, 
             get_FAKE_roundingmode(), 
             get_ST(st_src), 
             get_ST(st_dst) ) 
   );
   if (pop_after)
      fp_pop();
}

static void fp_do_ucomi_ST0_STi ( UInt i, Bool pop_after )
{
   DIP("fucomi%s %%st(0),%%st(%d)\n", pop_after ? "p" : "", (Int)i );
   stmt( IRStmt_Put( OFFB_CC_OP,   mkU32(X86G_CC_OP_COPY) ));
   stmt( IRStmt_Put( OFFB_CC_DEP2, mkU32(0) ));
   stmt( IRStmt_Put( OFFB_CC_DEP1,
                     binop( Iop_And32,
                            binop(Iop_CmpF64, get_ST(0), get_ST(i)),
                            mkU32(0x45)
       )));
   stmt( IRStmt_Put( OFFB_CC_NDEP, mkU32(0) ));
   if (pop_after)
      fp_pop();
}


static
UInt dis_FPU ( Bool* decode_ok, UChar sorb, Int delta )
{
   Int    len;
   UInt   r_src, r_dst;
   HChar  dis_buf[50];
   IRTemp t1, t2;

   UChar first_opcode = getIByte(delta-1);
   UChar modrm        = getIByte(delta+0);

   

   if (first_opcode == 0xD8) {
      if (modrm < 0xC0) {

         IRTemp addr = disAMode( &len, sorb, delta, dis_buf );
         delta += len;

         switch (gregOfRM(modrm)) {

            case 0: 
               fp_do_op_mem_ST_0 ( addr, "add", dis_buf, Iop_AddF64, False );
               break;

            case 1: 
               fp_do_op_mem_ST_0 ( addr, "mul", dis_buf, Iop_MulF64, False );
               break;

            case 2: 
               DIP("fcoms %s\n", dis_buf);
               
               put_C3210( 
                   binop( Iop_And32,
                          binop(Iop_Shl32, 
                                binop(Iop_CmpF64, 
                                      get_ST(0),
                                      unop(Iop_F32toF64, 
                                           loadLE(Ity_F32,mkexpr(addr)))),
                                mkU8(8)),
                          mkU32(0x4500)
                   ));
               break;  

            case 3: 
               DIP("fcomps %s\n", dis_buf);
               
               put_C3210( 
                   binop( Iop_And32,
                          binop(Iop_Shl32, 
                                binop(Iop_CmpF64, 
                                      get_ST(0),
                                      unop(Iop_F32toF64, 
                                           loadLE(Ity_F32,mkexpr(addr)))),
                                mkU8(8)),
                          mkU32(0x4500)
                   ));
               fp_pop();
               break;  

            case 4: 
               fp_do_op_mem_ST_0 ( addr, "sub", dis_buf, Iop_SubF64, False );
               break;

            case 5: 
               fp_do_oprev_mem_ST_0 ( addr, "subr", dis_buf, Iop_SubF64, False );
               break;

            case 6: 
               fp_do_op_mem_ST_0 ( addr, "div", dis_buf, Iop_DivF64, False );
               break;

            case 7: 
               fp_do_oprev_mem_ST_0 ( addr, "divr", dis_buf, Iop_DivF64, False );
               break;

            default:
               vex_printf("unhandled opc_aux = 0x%2x\n", gregOfRM(modrm));
               vex_printf("first_opcode == 0xD8\n");
               goto decode_fail;
         }
      } else {
         delta++;
         switch (modrm) {

            case 0xC0 ... 0xC7: 
               fp_do_op_ST_ST ( "add", Iop_AddF64, modrm - 0xC0, 0, False );
               break;

            case 0xC8 ... 0xCF: 
               fp_do_op_ST_ST ( "mul", Iop_MulF64, modrm - 0xC8, 0, False );
               break;

            
            case 0xD0 ... 0xD7: 
               r_dst = (UInt)modrm - 0xD0;
               DIP("fcom %%st(0),%%st(%d)\n", (Int)r_dst);
               
               put_C3210( 
                   binop( Iop_And32,
                          binop(Iop_Shl32, 
                                binop(Iop_CmpF64, get_ST(0), get_ST(r_dst)),
                                mkU8(8)),
                          mkU32(0x4500)
                   ));
               break;

            
            case 0xD8 ... 0xDF: 
               r_dst = (UInt)modrm - 0xD8;
               DIP("fcomp %%st(0),%%st(%d)\n", (Int)r_dst);
               
               put_C3210( 
                   binop( Iop_And32,
                          binop(Iop_Shl32, 
                                binop(Iop_CmpF64, get_ST(0), get_ST(r_dst)),
                                mkU8(8)),
                          mkU32(0x4500)
                   ));
               fp_pop();
               break;

            case 0xE0 ... 0xE7: 
               fp_do_op_ST_ST ( "sub", Iop_SubF64, modrm - 0xE0, 0, False );
               break;

            case 0xE8 ... 0xEF: 
               fp_do_oprev_ST_ST ( "subr", Iop_SubF64, modrm - 0xE8, 0, False );
               break;

            case 0xF0 ... 0xF7: 
               fp_do_op_ST_ST ( "div", Iop_DivF64, modrm - 0xF0, 0, False );
               break;

            case 0xF8 ... 0xFF: 
               fp_do_oprev_ST_ST ( "divr", Iop_DivF64, modrm - 0xF8, 0, False );
               break;

            default:
               goto decode_fail;
         }
      }
   }

   
   else
   if (first_opcode == 0xD9) {
      if (modrm < 0xC0) {

         IRTemp addr = disAMode( &len, sorb, delta, dis_buf );
         delta += len;

         switch (gregOfRM(modrm)) {

            case 0: 
               DIP("flds %s\n", dis_buf);
               fp_push();
               put_ST(0, unop(Iop_F32toF64,
                              loadLE(Ity_F32, mkexpr(addr))));
               break;

            case 2: 
               DIP("fsts %s\n", dis_buf);
               storeLE(mkexpr(addr),
                       binop(Iop_F64toF32, get_roundingmode(), get_ST(0)));
               break;

            case 3: 
               DIP("fstps %s\n", dis_buf);
               storeLE(mkexpr(addr), 
                       binop(Iop_F64toF32, get_roundingmode(), get_ST(0)));
               fp_pop();
               break;

            case 4: { 
               IRTemp   ew = newTemp(Ity_I32);
               IRDirty* d  = unsafeIRDirty_0_N ( 
                                0, 
                                "x86g_dirtyhelper_FLDENV", 
                                &x86g_dirtyhelper_FLDENV,
                                mkIRExprVec_2( IRExpr_BBPTR(), mkexpr(addr) )
                             );
               d->tmp   = ew;
               
               d->mFx   = Ifx_Read;
               d->mAddr = mkexpr(addr);
               d->mSize = 28;

               
               d->nFxState = 4;
               vex_bzero(&d->fxState, sizeof(d->fxState));

               d->fxState[0].fx     = Ifx_Write;
               d->fxState[0].offset = OFFB_FTOP;
               d->fxState[0].size   = sizeof(UInt);

               d->fxState[1].fx     = Ifx_Write;
               d->fxState[1].offset = OFFB_FPTAGS;
               d->fxState[1].size   = 8 * sizeof(UChar);

               d->fxState[2].fx     = Ifx_Write;
               d->fxState[2].offset = OFFB_FPROUND;
               d->fxState[2].size   = sizeof(UInt);

               d->fxState[3].fx     = Ifx_Write;
               d->fxState[3].offset = OFFB_FC3210;
               d->fxState[3].size   = sizeof(UInt);

               stmt( IRStmt_Dirty(d) );

               put_emwarn( mkexpr(ew) );
               stmt( 
                  IRStmt_Exit(
                     binop(Iop_CmpNE32, mkexpr(ew), mkU32(0)),
                     Ijk_EmWarn,
                     IRConst_U32( ((Addr32)guest_EIP_bbstart)+delta),
                     OFFB_EIP
                  )
               );

               DIP("fldenv %s\n", dis_buf);
               break;
            }

            case 5: {
               
               IRTemp t64 = newTemp(Ity_I64);
               IRTemp ew = newTemp(Ity_I32);
               DIP("fldcw %s\n", dis_buf);
               assign( t64, mkIRExprCCall(
                               Ity_I64, 0, 
                               "x86g_check_fldcw",
                               &x86g_check_fldcw, 
                               mkIRExprVec_1( 
                                  unop( Iop_16Uto32, 
                                        loadLE(Ity_I16, mkexpr(addr)))
                               )
                            )
                     );

               put_fpround( unop(Iop_64to32, mkexpr(t64)) );
               assign( ew, unop(Iop_64HIto32, mkexpr(t64) ) );
               put_emwarn( mkexpr(ew) );
               stmt( 
                  IRStmt_Exit(
                     binop(Iop_CmpNE32, mkexpr(ew), mkU32(0)),
                     Ijk_EmWarn,
                     IRConst_U32( ((Addr32)guest_EIP_bbstart)+delta),
                     OFFB_EIP
                  )
               );
               break;
            }

            case 6: { 
               IRDirty* d = unsafeIRDirty_0_N ( 
                               0, 
                               "x86g_dirtyhelper_FSTENV", 
                               &x86g_dirtyhelper_FSTENV,
                               mkIRExprVec_2( IRExpr_BBPTR(), mkexpr(addr) )
                            );
               
               d->mFx   = Ifx_Write;
               d->mAddr = mkexpr(addr);
               d->mSize = 28;

               
               d->nFxState = 4;
               vex_bzero(&d->fxState, sizeof(d->fxState));

               d->fxState[0].fx     = Ifx_Read;
               d->fxState[0].offset = OFFB_FTOP;
               d->fxState[0].size   = sizeof(UInt);

               d->fxState[1].fx     = Ifx_Read;
               d->fxState[1].offset = OFFB_FPTAGS;
               d->fxState[1].size   = 8 * sizeof(UChar);

               d->fxState[2].fx     = Ifx_Read;
               d->fxState[2].offset = OFFB_FPROUND;
               d->fxState[2].size   = sizeof(UInt);

               d->fxState[3].fx     = Ifx_Read;
               d->fxState[3].offset = OFFB_FC3210;
               d->fxState[3].size   = sizeof(UInt);

               stmt( IRStmt_Dirty(d) );

               DIP("fnstenv %s\n", dis_buf);
               break;
            }

            case 7: 
               
               DIP("fnstcw %s\n", dis_buf);
               storeLE(
                  mkexpr(addr), 
                  unop( Iop_32to16, 
                        mkIRExprCCall(
                           Ity_I32, 0,
                           "x86g_create_fpucw", &x86g_create_fpucw, 
                           mkIRExprVec_1( get_fpround() ) 
                        ) 
                  ) 
               );
               break;

            default:
               vex_printf("unhandled opc_aux = 0x%2x\n", gregOfRM(modrm));
               vex_printf("first_opcode == 0xD9\n");
               goto decode_fail;
         }

      } else {
         delta++;
         switch (modrm) {

            case 0xC0 ... 0xC7: 
               r_src = (UInt)modrm - 0xC0;
               DIP("fld %%st(%d)\n", (Int)r_src);
               t1 = newTemp(Ity_F64);
               assign(t1, get_ST(r_src));
               fp_push();
               put_ST(0, mkexpr(t1));
               break;

            case 0xC8 ... 0xCF: 
               r_src = (UInt)modrm - 0xC8;
               DIP("fxch %%st(%d)\n", (Int)r_src);
               t1 = newTemp(Ity_F64);
               t2 = newTemp(Ity_F64);
               assign(t1, get_ST(0));
               assign(t2, get_ST(r_src));
               put_ST_UNCHECKED(0, mkexpr(t2));
               put_ST_UNCHECKED(r_src, mkexpr(t1));
               break;

            case 0xE0: 
               DIP("fchs\n");
               put_ST_UNCHECKED(0, unop(Iop_NegF64, get_ST(0)));
               break;

            case 0xE1: 
               DIP("fabs\n");
               put_ST_UNCHECKED(0, unop(Iop_AbsF64, get_ST(0)));
               break;

            case 0xE4: 
               DIP("ftst\n");
               
               put_C3210( 
                   binop( Iop_And32,
                          binop(Iop_Shl32, 
                                binop(Iop_CmpF64, 
                                      get_ST(0),
                                      IRExpr_Const(IRConst_F64i(0x0ULL))),
                                mkU8(8)),
                          mkU32(0x4500)
                   ));
               break;

            case 0xE5: { 
               IRExpr** args
                  = mkIRExprVec_2( unop(Iop_8Uto32, get_ST_TAG(0)),
                                   unop(Iop_ReinterpF64asI64, 
                                        get_ST_UNCHECKED(0)) );
               put_C3210(mkIRExprCCall(
                            Ity_I32, 
                            0, 
                            "x86g_calculate_FXAM", &x86g_calculate_FXAM,
                            args
                        ));
               DIP("fxam\n");
               break;
            }

            case 0xE8: 
               DIP("fld1\n");
               fp_push();
               
               put_ST(0, IRExpr_Const(IRConst_F64i(0x3ff0000000000000ULL)));
               break;

            case 0xE9: 
               DIP("fldl2t\n");
               fp_push();
               
               put_ST(0, IRExpr_Const(IRConst_F64i(0x400a934f0979a371ULL)));
               break;

            case 0xEA: 
               DIP("fldl2e\n");
               fp_push();
               
               put_ST(0, IRExpr_Const(IRConst_F64i(0x3ff71547652b82feULL)));
               break;

            case 0xEB: 
               DIP("fldpi\n");
               fp_push();
               
               put_ST(0, IRExpr_Const(IRConst_F64i(0x400921fb54442d18ULL)));
               break;

            case 0xEC: 
               DIP("fldlg2\n");
               fp_push();
               
               put_ST(0, IRExpr_Const(IRConst_F64i(0x3fd34413509f79ffULL)));
               break;

            case 0xED: 
               DIP("fldln2\n");
               fp_push();
               
               put_ST(0, IRExpr_Const(IRConst_F64i(0x3fe62e42fefa39efULL)));
               break;

            case 0xEE: 
               DIP("fldz\n");
               fp_push();
               
               put_ST(0, IRExpr_Const(IRConst_F64i(0x0000000000000000ULL)));
               break;

            case 0xF0: 
               DIP("f2xm1\n");
               put_ST_UNCHECKED(0, 
                  binop(Iop_2xm1F64, 
                        get_FAKE_roundingmode(), 
                        get_ST(0)));
               break;

            case 0xF1: 
               DIP("fyl2x\n");
               put_ST_UNCHECKED(1, 
                  triop(Iop_Yl2xF64,
                        get_FAKE_roundingmode(), 
                        get_ST(1), 
                        get_ST(0)));
               fp_pop();
               break;

            case 0xF2: { 
               DIP("fptan\n");
               IRTemp argD = newTemp(Ity_F64);
               assign(argD, get_ST(0));
               IRTemp argOK = math_IS_TRIG_ARG_FINITE_AND_IN_RANGE(argD);
               IRTemp resD = newTemp(Ity_F64);
               assign(resD,
                  IRExpr_ITE(
                     mkexpr(argOK), 
                     binop(Iop_TanF64,
                           get_FAKE_roundingmode(), 
                           mkexpr(argD)),
                     mkexpr(argD))
               );
               put_ST_UNCHECKED(0, mkexpr(resD));
               maybe_fp_push(argOK);
               maybe_put_ST(argOK, 0,
                            IRExpr_Const(IRConst_F64(1.0)));
               set_C2( binop(Iop_Xor32,
                             unop(Iop_1Uto32, mkexpr(argOK)), 
                             mkU32(1)) );
               break;
            }

            case 0xF3: 
               DIP("fpatan\n");
               put_ST_UNCHECKED(1, 
                  triop(Iop_AtanF64,
                        get_FAKE_roundingmode(), 
                        get_ST(1), 
                        get_ST(0)));
               fp_pop();
               break;

            case 0xF4: { 
               IRTemp argF = newTemp(Ity_F64);
               IRTemp sigF = newTemp(Ity_F64);
               IRTemp expF = newTemp(Ity_F64);
               IRTemp argI = newTemp(Ity_I64);
               IRTemp sigI = newTemp(Ity_I64);
               IRTemp expI = newTemp(Ity_I64);
               DIP("fxtract\n");
               assign( argF, get_ST(0) );
               assign( argI, unop(Iop_ReinterpF64asI64, mkexpr(argF)));
               assign( sigI, 
                       mkIRExprCCall(
                          Ity_I64, 0, 
                          "x86amd64g_calculate_FXTRACT", 
                          &x86amd64g_calculate_FXTRACT, 
                          mkIRExprVec_2( mkexpr(argI), 
                                         mkIRExpr_HWord(0) )) 
               );
               assign( expI, 
                       mkIRExprCCall(
                          Ity_I64, 0, 
                          "x86amd64g_calculate_FXTRACT", 
                          &x86amd64g_calculate_FXTRACT, 
                          mkIRExprVec_2( mkexpr(argI), 
                                         mkIRExpr_HWord(1) )) 
               );
               assign( sigF, unop(Iop_ReinterpI64asF64, mkexpr(sigI)) );
               assign( expF, unop(Iop_ReinterpI64asF64, mkexpr(expI)) );
               
               put_ST_UNCHECKED(0, mkexpr(expF) );
               fp_push();
               
               put_ST(0, mkexpr(sigF) );
               break;
            }

            case 0xF5: { 
               IRTemp a1 = newTemp(Ity_F64);
               IRTemp a2 = newTemp(Ity_F64);
               DIP("fprem1\n");
               assign( a1, get_ST(0) );
               assign( a2, get_ST(1) );
               put_ST_UNCHECKED(0, 
                  triop(Iop_PRem1F64,
                        get_FAKE_roundingmode(), 
                        mkexpr(a1), 
                        mkexpr(a2)));
               put_C3210(
                  triop(Iop_PRem1C3210F64,
                        get_FAKE_roundingmode(), 
                        mkexpr(a1), 
                        mkexpr(a2)) );
               break;
            }

            case 0xF7: 
               DIP("fprem\n");
               put_ftop( binop(Iop_Add32, get_ftop(), mkU32(1)) );
               break;

            case 0xF8: { 
               IRTemp a1 = newTemp(Ity_F64);
               IRTemp a2 = newTemp(Ity_F64);
               DIP("fprem\n");
               assign( a1, get_ST(0) );
               assign( a2, get_ST(1) );
               put_ST_UNCHECKED(0, 
                  triop(Iop_PRemF64,
                        get_FAKE_roundingmode(), 
                        mkexpr(a1), 
                        mkexpr(a2)));
               put_C3210( 
                  triop(Iop_PRemC3210F64,
                        get_FAKE_roundingmode(), 
                        mkexpr(a1), 
                        mkexpr(a2)) );
               break;
            }

            case 0xF9: 
               DIP("fyl2xp1\n");
               put_ST_UNCHECKED(1, 
                  triop(Iop_Yl2xp1F64,
                        get_FAKE_roundingmode(), 
                        get_ST(1), 
                        get_ST(0)));
               fp_pop();
               break;

            case 0xFA: 
               DIP("fsqrt\n");
               put_ST_UNCHECKED(0, 
                  binop(Iop_SqrtF64, 
                        get_FAKE_roundingmode(), 
                        get_ST(0)));
               break;

            case 0xFB: { 
               DIP("fsincos\n");
               IRTemp argD = newTemp(Ity_F64);
               assign(argD, get_ST(0));
               IRTemp argOK = math_IS_TRIG_ARG_FINITE_AND_IN_RANGE(argD);
               IRTemp resD = newTemp(Ity_F64);
               assign(resD,
                  IRExpr_ITE(
                     mkexpr(argOK), 
                     binop(Iop_SinF64,
                           get_FAKE_roundingmode(), 
                           mkexpr(argD)),
                     mkexpr(argD))
               );
               put_ST_UNCHECKED(0, mkexpr(resD));
               maybe_fp_push(argOK);
               maybe_put_ST(argOK, 0,
                  binop(Iop_CosF64,
                        get_FAKE_roundingmode(), 
                        mkexpr(argD)));
               set_C2( binop(Iop_Xor32,
                             unop(Iop_1Uto32, mkexpr(argOK)), 
                             mkU32(1)) );
               break;
            }

            case 0xFC: 
               DIP("frndint\n");
               put_ST_UNCHECKED(0,
                  binop(Iop_RoundF64toInt, get_roundingmode(), get_ST(0)) );
               break;

            case 0xFD: 
               DIP("fscale\n");
               put_ST_UNCHECKED(0, 
                  triop(Iop_ScaleF64,
                        get_FAKE_roundingmode(), 
                        get_ST(0), 
                        get_ST(1)));
               break;

            case 0xFE:   
            case 0xFF: { 
               Bool isSIN = modrm == 0xFE;
               DIP("%s\n", isSIN ? "fsin" : "fcos");
               IRTemp argD = newTemp(Ity_F64);
               assign(argD, get_ST(0));
               IRTemp argOK = math_IS_TRIG_ARG_FINITE_AND_IN_RANGE(argD);
               IRTemp resD = newTemp(Ity_F64);
               assign(resD,
                  IRExpr_ITE(
                     mkexpr(argOK), 
                     binop(isSIN ? Iop_SinF64 : Iop_CosF64,
                           get_FAKE_roundingmode(), 
                           mkexpr(argD)),
                     mkexpr(argD))
               );
               put_ST_UNCHECKED(0, mkexpr(resD));
               set_C2( binop(Iop_Xor32,
                             unop(Iop_1Uto32, mkexpr(argOK)), 
                             mkU32(1)) );
               break;
            }

            default:
               goto decode_fail;
         }
      }
   }

   
   else
   if (first_opcode == 0xDA) {

      if (modrm < 0xC0) {

         IROp   fop;
         IRTemp addr = disAMode( &len, sorb, delta, dis_buf );
         delta += len;
         switch (gregOfRM(modrm)) {

            case 0:  
               DIP("fiaddl %s\n", dis_buf);
               fop = Iop_AddF64;
               goto do_fop_m32;

            case 1:  
               DIP("fimull %s\n", dis_buf);
               fop = Iop_MulF64;
               goto do_fop_m32;

            case 2: 
               DIP("ficoml %s\n", dis_buf);
               
               put_C3210( 
                   binop( Iop_And32,
                          binop(Iop_Shl32, 
                                binop(Iop_CmpF64, 
                                      get_ST(0),
                                      unop(Iop_I32StoF64, 
                                           loadLE(Ity_I32,mkexpr(addr)))),
                                mkU8(8)),
                          mkU32(0x4500)
                   ));
               break;

            case 3: 
               DIP("ficompl %s\n", dis_buf);
               
               put_C3210( 
                   binop( Iop_And32,
                          binop(Iop_Shl32, 
                                binop(Iop_CmpF64, 
                                      get_ST(0),
                                      unop(Iop_I32StoF64, 
                                           loadLE(Ity_I32,mkexpr(addr)))),
                                mkU8(8)),
                          mkU32(0x4500)
                   ));
               fp_pop();
               break;

            case 4:  
               DIP("fisubl %s\n", dis_buf);
               fop = Iop_SubF64;
               goto do_fop_m32;

            case 5:  
               DIP("fisubrl %s\n", dis_buf);
               fop = Iop_SubF64;
               goto do_foprev_m32;

            case 6:  
               DIP("fidivl %s\n", dis_buf);
               fop = Iop_DivF64;
               goto do_fop_m32;

            case 7:  
               DIP("fidivrl %s\n", dis_buf);
               fop = Iop_DivF64;
               goto do_foprev_m32;

            do_fop_m32:
               put_ST_UNCHECKED(0, 
                  triop(fop, 
                        get_FAKE_roundingmode(), 
                        get_ST(0),
                        unop(Iop_I32StoF64,
                             loadLE(Ity_I32, mkexpr(addr)))));
               break;

            do_foprev_m32:
               put_ST_UNCHECKED(0, 
                  triop(fop, 
                        get_FAKE_roundingmode(), 
                        unop(Iop_I32StoF64,
                             loadLE(Ity_I32, mkexpr(addr))),
                        get_ST(0)));
               break;

            default:
               vex_printf("unhandled opc_aux = 0x%2x\n", gregOfRM(modrm));
               vex_printf("first_opcode == 0xDA\n");
               goto decode_fail;
         }

      } else {

         delta++;
         switch (modrm) {

            case 0xC0 ... 0xC7: 
               r_src = (UInt)modrm - 0xC0;
               DIP("fcmovb %%st(%d), %%st(0)\n", (Int)r_src);
               put_ST_UNCHECKED(0, 
                                IRExpr_ITE( 
                                    mk_x86g_calculate_condition(X86CondB),
                                    get_ST(r_src), get_ST(0)) );
               break;

            case 0xC8 ... 0xCF: 
               r_src = (UInt)modrm - 0xC8;
               DIP("fcmovz %%st(%d), %%st(0)\n", (Int)r_src);
               put_ST_UNCHECKED(0, 
                                IRExpr_ITE( 
                                    mk_x86g_calculate_condition(X86CondZ),
                                    get_ST(r_src), get_ST(0)) );
               break;

            case 0xD0 ... 0xD7: 
               r_src = (UInt)modrm - 0xD0;
               DIP("fcmovbe %%st(%d), %%st(0)\n", (Int)r_src);
               put_ST_UNCHECKED(0, 
                                IRExpr_ITE( 
                                    mk_x86g_calculate_condition(X86CondBE),
                                    get_ST(r_src), get_ST(0)) );
               break;

            case 0xD8 ... 0xDF: 
               r_src = (UInt)modrm - 0xD8;
               DIP("fcmovu %%st(%d), %%st(0)\n", (Int)r_src);
               put_ST_UNCHECKED(0, 
                                IRExpr_ITE( 
                                    mk_x86g_calculate_condition(X86CondP),
                                    get_ST(r_src), get_ST(0)) );
               break;

            case 0xE9: 
               DIP("fucompp %%st(0),%%st(1)\n");
               
               put_C3210( 
                   binop( Iop_And32,
                          binop(Iop_Shl32, 
                                binop(Iop_CmpF64, get_ST(0), get_ST(1)),
                                mkU8(8)),
                          mkU32(0x4500)
                   ));
               fp_pop();
               fp_pop();
               break;

            default:
               goto decode_fail;
         }

      }
   }

   
   else
   if (first_opcode == 0xDB) {
      if (modrm < 0xC0) {

         IRTemp addr = disAMode( &len, sorb, delta, dis_buf );
         delta += len;

         switch (gregOfRM(modrm)) {

            case 0: 
               DIP("fildl %s\n", dis_buf);
               fp_push();
               put_ST(0, unop(Iop_I32StoF64,
                              loadLE(Ity_I32, mkexpr(addr))));
               break;

            case 1: 
               DIP("fisttpl %s\n", dis_buf);
               storeLE( mkexpr(addr), 
                        binop(Iop_F64toI32S, mkU32(Irrm_ZERO), get_ST(0)) );
               fp_pop();
               break;

            case 2: 
               DIP("fistl %s\n", dis_buf);
               storeLE( mkexpr(addr), 
                        binop(Iop_F64toI32S, get_roundingmode(), get_ST(0)) );
               break;

            case 3: 
               DIP("fistpl %s\n", dis_buf);
               storeLE( mkexpr(addr), 
                        binop(Iop_F64toI32S, get_roundingmode(), get_ST(0)) );
               fp_pop();
               break;

            case 5: { 
               IRTemp   val  = newTemp(Ity_I64);
               IRExpr** args = mkIRExprVec_1 ( mkexpr(addr) );

               IRDirty* d = unsafeIRDirty_1_N ( 
                               val, 
                               0, 
                               "x86g_dirtyhelper_loadF80le", 
                               &x86g_dirtyhelper_loadF80le, 
                               args 
                            );
               
               d->mFx   = Ifx_Read;
               d->mAddr = mkexpr(addr);
               d->mSize = 10;

               
               stmt( IRStmt_Dirty(d) );
               fp_push();
               put_ST(0, unop(Iop_ReinterpI64asF64, mkexpr(val)));

               DIP("fldt %s\n", dis_buf);
               break;
            }

            case 7: { 
               
               IRExpr** args 
                  = mkIRExprVec_2( mkexpr(addr), 
                                   unop(Iop_ReinterpF64asI64, get_ST(0)) );

               IRDirty* d = unsafeIRDirty_0_N ( 
                               0, 
                               "x86g_dirtyhelper_storeF80le", 
                               &x86g_dirtyhelper_storeF80le,
                               args 
                            );
               
               d->mFx   = Ifx_Write;
               d->mAddr = mkexpr(addr);
               d->mSize = 10;

               
               stmt( IRStmt_Dirty(d) );
               fp_pop();

               DIP("fstpt\n %s", dis_buf);
               break;
            }

            default:
               vex_printf("unhandled opc_aux = 0x%2x\n", gregOfRM(modrm));
               vex_printf("first_opcode == 0xDB\n");
               goto decode_fail;
         }

      } else {

         delta++;
         switch (modrm) {

            case 0xC0 ... 0xC7: 
               r_src = (UInt)modrm - 0xC0;
               DIP("fcmovnb %%st(%d), %%st(0)\n", (Int)r_src);
               put_ST_UNCHECKED(0, 
                                IRExpr_ITE( 
                                    mk_x86g_calculate_condition(X86CondNB),
                                    get_ST(r_src), get_ST(0)) );
               break;

            case 0xC8 ... 0xCF: 
               r_src = (UInt)modrm - 0xC8;
               DIP("fcmovnz %%st(%d), %%st(0)\n", (Int)r_src);
               put_ST_UNCHECKED(0, 
                                IRExpr_ITE( 
                                    mk_x86g_calculate_condition(X86CondNZ),
                                    get_ST(r_src), get_ST(0)) );
               break;

            case 0xD0 ... 0xD7: 
               r_src = (UInt)modrm - 0xD0;
               DIP("fcmovnbe %%st(%d), %%st(0)\n", (Int)r_src);
               put_ST_UNCHECKED(0, 
                                IRExpr_ITE( 
                                    mk_x86g_calculate_condition(X86CondNBE),
                                    get_ST(r_src), get_ST(0)) );
               break;

            case 0xD8 ... 0xDF: 
               r_src = (UInt)modrm - 0xD8;
               DIP("fcmovnu %%st(%d), %%st(0)\n", (Int)r_src);
               put_ST_UNCHECKED(0, 
                                IRExpr_ITE( 
                                    mk_x86g_calculate_condition(X86CondNP),
                                    get_ST(r_src), get_ST(0)) );
               break;

            case 0xE2:
               DIP("fnclex\n");
               break;

            case 0xE3: {
               IRDirty* d  = unsafeIRDirty_0_N ( 
                                0, 
                                "x86g_dirtyhelper_FINIT", 
                                &x86g_dirtyhelper_FINIT,
                                mkIRExprVec_1(IRExpr_BBPTR())
                             );

               
               d->nFxState = 5;
               vex_bzero(&d->fxState, sizeof(d->fxState));

               d->fxState[0].fx     = Ifx_Write;
               d->fxState[0].offset = OFFB_FTOP;
               d->fxState[0].size   = sizeof(UInt);

               d->fxState[1].fx     = Ifx_Write;
               d->fxState[1].offset = OFFB_FPREGS;
               d->fxState[1].size   = 8 * sizeof(ULong);

               d->fxState[2].fx     = Ifx_Write;
               d->fxState[2].offset = OFFB_FPTAGS;
               d->fxState[2].size   = 8 * sizeof(UChar);

               d->fxState[3].fx     = Ifx_Write;
               d->fxState[3].offset = OFFB_FPROUND;
               d->fxState[3].size   = sizeof(UInt);

               d->fxState[4].fx     = Ifx_Write;
               d->fxState[4].offset = OFFB_FC3210;
               d->fxState[4].size   = sizeof(UInt);

               stmt( IRStmt_Dirty(d) );

               DIP("fninit\n");
               break;
            }

            case 0xE8 ... 0xEF: 
               fp_do_ucomi_ST0_STi( (UInt)modrm - 0xE8, False );
               break;

            case 0xF0 ... 0xF7: 
               fp_do_ucomi_ST0_STi( (UInt)modrm - 0xF0, False );
               break;

            default:
               goto decode_fail;
         }
      }
   }

   
   else
   if (first_opcode == 0xDC) {
      if (modrm < 0xC0) {

         IRTemp addr = disAMode( &len, sorb, delta, dis_buf );
         delta += len;

         switch (gregOfRM(modrm)) {

            case 0: 
               fp_do_op_mem_ST_0 ( addr, "add", dis_buf, Iop_AddF64, True );
               break;

            case 1: 
               fp_do_op_mem_ST_0 ( addr, "mul", dis_buf, Iop_MulF64, True );
               break;

            case 2: 
               DIP("fcoml %s\n", dis_buf);
               
               put_C3210( 
                   binop( Iop_And32,
                          binop(Iop_Shl32, 
                                binop(Iop_CmpF64, 
                                      get_ST(0),
                                      loadLE(Ity_F64,mkexpr(addr))),
                                mkU8(8)),
                          mkU32(0x4500)
                   ));
               break;  

            case 3: 
               DIP("fcompl %s\n", dis_buf);
               
               put_C3210( 
                   binop( Iop_And32,
                          binop(Iop_Shl32, 
                                binop(Iop_CmpF64, 
                                      get_ST(0),
                                      loadLE(Ity_F64,mkexpr(addr))),
                                mkU8(8)),
                          mkU32(0x4500)
                   ));
               fp_pop();
               break;  

            case 4: 
               fp_do_op_mem_ST_0 ( addr, "sub", dis_buf, Iop_SubF64, True );
               break;

            case 5: 
               fp_do_oprev_mem_ST_0 ( addr, "subr", dis_buf, Iop_SubF64, True );
               break;

            case 6: 
               fp_do_op_mem_ST_0 ( addr, "div", dis_buf, Iop_DivF64, True );
               break;

            case 7: 
               fp_do_oprev_mem_ST_0 ( addr, "divr", dis_buf, Iop_DivF64, True );
               break;

            default:
               vex_printf("unhandled opc_aux = 0x%2x\n", gregOfRM(modrm));
               vex_printf("first_opcode == 0xDC\n");
               goto decode_fail;
         }

      } else {

         delta++;
         switch (modrm) {

            case 0xC0 ... 0xC7: 
               fp_do_op_ST_ST ( "add", Iop_AddF64, 0, modrm - 0xC0, False );
               break;

            case 0xC8 ... 0xCF: 
               fp_do_op_ST_ST ( "mul", Iop_MulF64, 0, modrm - 0xC8, False );
               break;

            case 0xE0 ... 0xE7: 
               fp_do_oprev_ST_ST ( "subr", Iop_SubF64, 0, modrm - 0xE0, False );
               break;

            case 0xE8 ... 0xEF: 
               fp_do_op_ST_ST ( "sub", Iop_SubF64, 0, modrm - 0xE8, False );
               break;

            case 0xF0 ... 0xF7: 
               fp_do_oprev_ST_ST ( "divr", Iop_DivF64, 0, modrm - 0xF0, False );
               break;

            case 0xF8 ... 0xFF: 
               fp_do_op_ST_ST ( "div", Iop_DivF64, 0, modrm - 0xF8, False );
               break;

            default:
               goto decode_fail;
         }

      }
   }

   
   else
   if (first_opcode == 0xDD) {

      if (modrm < 0xC0) {

         IRTemp addr = disAMode( &len, sorb, delta, dis_buf );
         delta += len;

         switch (gregOfRM(modrm)) {

            case 0: 
               DIP("fldl %s\n", dis_buf);
               fp_push();
               put_ST(0, loadLE(Ity_F64, mkexpr(addr)));
               break;

            case 1: 
               DIP("fistppll %s\n", dis_buf);
               storeLE( mkexpr(addr), 
                        binop(Iop_F64toI64S, mkU32(Irrm_ZERO), get_ST(0)) );
               fp_pop();
               break;

            case 2: 
               DIP("fstl %s\n", dis_buf);
               storeLE(mkexpr(addr), get_ST(0));
               break;

            case 3: 
               DIP("fstpl %s\n", dis_buf);
               storeLE(mkexpr(addr), get_ST(0));
               fp_pop();
               break;

            case 4: { 
               IRTemp   ew = newTemp(Ity_I32);
               IRDirty* d  = unsafeIRDirty_0_N ( 
                                0, 
                                "x86g_dirtyhelper_FRSTOR", 
                                &x86g_dirtyhelper_FRSTOR,
                                mkIRExprVec_2( IRExpr_BBPTR(), mkexpr(addr) )
                             );
               d->tmp   = ew;
               
               d->mFx   = Ifx_Read;
               d->mAddr = mkexpr(addr);
               d->mSize = 108;

               
               d->nFxState = 5;
               vex_bzero(&d->fxState, sizeof(d->fxState));

               d->fxState[0].fx     = Ifx_Write;
               d->fxState[0].offset = OFFB_FTOP;
               d->fxState[0].size   = sizeof(UInt);

               d->fxState[1].fx     = Ifx_Write;
               d->fxState[1].offset = OFFB_FPREGS;
               d->fxState[1].size   = 8 * sizeof(ULong);

               d->fxState[2].fx     = Ifx_Write;
               d->fxState[2].offset = OFFB_FPTAGS;
               d->fxState[2].size   = 8 * sizeof(UChar);

               d->fxState[3].fx     = Ifx_Write;
               d->fxState[3].offset = OFFB_FPROUND;
               d->fxState[3].size   = sizeof(UInt);

               d->fxState[4].fx     = Ifx_Write;
               d->fxState[4].offset = OFFB_FC3210;
               d->fxState[4].size   = sizeof(UInt);

               stmt( IRStmt_Dirty(d) );

               put_emwarn( mkexpr(ew) );
               stmt( 
                  IRStmt_Exit(
                     binop(Iop_CmpNE32, mkexpr(ew), mkU32(0)),
                     Ijk_EmWarn,
                     IRConst_U32( ((Addr32)guest_EIP_bbstart)+delta),
                     OFFB_EIP
                  )
               );

               DIP("frstor %s\n", dis_buf);
               break;
            }

            case 6: { 
               IRDirty* d = unsafeIRDirty_0_N ( 
                               0, 
                               "x86g_dirtyhelper_FSAVE", 
                               &x86g_dirtyhelper_FSAVE,
                               mkIRExprVec_2( IRExpr_BBPTR(), mkexpr(addr) )
                            );
               
               d->mFx   = Ifx_Write;
               d->mAddr = mkexpr(addr);
               d->mSize = 108;

               
               d->nFxState = 5;
               vex_bzero(&d->fxState, sizeof(d->fxState));

               d->fxState[0].fx     = Ifx_Read;
               d->fxState[0].offset = OFFB_FTOP;
               d->fxState[0].size   = sizeof(UInt);

               d->fxState[1].fx     = Ifx_Read;
               d->fxState[1].offset = OFFB_FPREGS;
               d->fxState[1].size   = 8 * sizeof(ULong);

               d->fxState[2].fx     = Ifx_Read;
               d->fxState[2].offset = OFFB_FPTAGS;
               d->fxState[2].size   = 8 * sizeof(UChar);

               d->fxState[3].fx     = Ifx_Read;
               d->fxState[3].offset = OFFB_FPROUND;
               d->fxState[3].size   = sizeof(UInt);

               d->fxState[4].fx     = Ifx_Read;
               d->fxState[4].offset = OFFB_FC3210;
               d->fxState[4].size   = sizeof(UInt);

               stmt( IRStmt_Dirty(d) );

               DIP("fnsave %s\n", dis_buf);
               break;
            }

            case 7: { 
               IRExpr* sw = get_FPU_sw();
               vassert(typeOfIRExpr(irsb->tyenv, sw) == Ity_I16);
               storeLE( mkexpr(addr), sw );
               DIP("fnstsw %s\n", dis_buf);
               break;
            }

            default:
               vex_printf("unhandled opc_aux = 0x%2x\n", gregOfRM(modrm));
               vex_printf("first_opcode == 0xDD\n");
               goto decode_fail;
         }
      } else {
         delta++;
         switch (modrm) {

            case 0xC0 ... 0xC7: 
               r_dst = (UInt)modrm - 0xC0;
               DIP("ffree %%st(%d)\n", (Int)r_dst);
               put_ST_TAG ( r_dst, mkU8(0) );
               break;

            case 0xD0 ... 0xD7: 
               r_dst = (UInt)modrm - 0xD0;
               DIP("fst %%st(0),%%st(%d)\n", (Int)r_dst);
               put_ST_UNCHECKED(r_dst, get_ST(0));
               break;

            case 0xD8 ... 0xDF: 
               r_dst = (UInt)modrm - 0xD8;
               DIP("fstp %%st(0),%%st(%d)\n", (Int)r_dst);
               put_ST_UNCHECKED(r_dst, get_ST(0));
               fp_pop();
               break;

            case 0xE0 ... 0xE7: 
               r_dst = (UInt)modrm - 0xE0;
               DIP("fucom %%st(0),%%st(%d)\n", (Int)r_dst);
               
               put_C3210( 
                   binop( Iop_And32,
                          binop(Iop_Shl32, 
                                binop(Iop_CmpF64, get_ST(0), get_ST(r_dst)),
                                mkU8(8)),
                          mkU32(0x4500)
                   ));
               break;

            case 0xE8 ... 0xEF: 
               r_dst = (UInt)modrm - 0xE8;
               DIP("fucomp %%st(0),%%st(%d)\n", (Int)r_dst);
               
               put_C3210( 
                   binop( Iop_And32,
                          binop(Iop_Shl32, 
                                binop(Iop_CmpF64, get_ST(0), get_ST(r_dst)),
                                mkU8(8)),
                          mkU32(0x4500)
                   ));
               fp_pop();
               break;

            default:
               goto decode_fail;
         }
      }
   }

   
   else
   if (first_opcode == 0xDE) {

      if (modrm < 0xC0) {

         IROp   fop;
         IRTemp addr = disAMode( &len, sorb, delta, dis_buf );
         delta += len;

         switch (gregOfRM(modrm)) {

            case 0:  
               DIP("fiaddw %s\n", dis_buf);
               fop = Iop_AddF64;
               goto do_fop_m16;

            case 1:  
               DIP("fimulw %s\n", dis_buf);
               fop = Iop_MulF64;
               goto do_fop_m16;

            case 2: 
               DIP("ficomw %s\n", dis_buf);
               
               put_C3210( 
                   binop( Iop_And32,
                          binop(Iop_Shl32, 
                                binop(Iop_CmpF64, 
                                      get_ST(0),
                                      unop(Iop_I32StoF64, 
                                         unop(Iop_16Sto32,
                                           loadLE(Ity_I16,mkexpr(addr))))),
                                mkU8(8)),
                          mkU32(0x4500)
                   ));
               break;

            case 3: 
               DIP("ficompw %s\n", dis_buf);
               
               put_C3210( 
                   binop( Iop_And32,
                          binop(Iop_Shl32, 
                                binop(Iop_CmpF64, 
                                      get_ST(0),
                                      unop(Iop_I32StoF64, 
                                         unop(Iop_16Sto32,
                                              loadLE(Ity_I16,mkexpr(addr))))),
                                mkU8(8)),
                          mkU32(0x4500)
                   ));
               fp_pop();
               break;

            case 4:  
               DIP("fisubw %s\n", dis_buf);
               fop = Iop_SubF64;
               goto do_fop_m16;

            case 5:  
               DIP("fisubrw %s\n", dis_buf);
               fop = Iop_SubF64;
               goto do_foprev_m16;

            case 6:  
               DIP("fisubw %s\n", dis_buf);
               fop = Iop_DivF64;
               goto do_fop_m16;

            case 7:  
               DIP("fidivrw %s\n", dis_buf);
               fop = Iop_DivF64;
               goto do_foprev_m16;

            do_fop_m16:
               put_ST_UNCHECKED(0, 
                  triop(fop, 
                        get_FAKE_roundingmode(), 
                        get_ST(0),
                        unop(Iop_I32StoF64,
                             unop(Iop_16Sto32, 
                                  loadLE(Ity_I16, mkexpr(addr))))));
               break;

            do_foprev_m16:
               put_ST_UNCHECKED(0, 
                  triop(fop, 
                        get_FAKE_roundingmode(), 
                        unop(Iop_I32StoF64,
                             unop(Iop_16Sto32, 
                                  loadLE(Ity_I16, mkexpr(addr)))),
                        get_ST(0)));
               break;

            default:
               vex_printf("unhandled opc_aux = 0x%2x\n", gregOfRM(modrm));
               vex_printf("first_opcode == 0xDE\n");
               goto decode_fail;
         }

      } else {

         delta++;
         switch (modrm) {

            case 0xC0 ... 0xC7: 
               fp_do_op_ST_ST ( "add", Iop_AddF64, 0, modrm - 0xC0, True );
               break;

            case 0xC8 ... 0xCF: 
               fp_do_op_ST_ST ( "mul", Iop_MulF64, 0, modrm - 0xC8, True );
               break;

            case 0xD9: 
               DIP("fuompp %%st(0),%%st(1)\n");
               
               put_C3210( 
                   binop( Iop_And32,
                          binop(Iop_Shl32, 
                                binop(Iop_CmpF64, get_ST(0), get_ST(1)),
                                mkU8(8)),
                          mkU32(0x4500)
                   ));
               fp_pop();
               fp_pop();
               break;

            case 0xE0 ... 0xE7: 
               fp_do_oprev_ST_ST ( "subr", Iop_SubF64, 0,  modrm - 0xE0, True );
               break;

            case 0xE8 ... 0xEF: 
               fp_do_op_ST_ST ( "sub", Iop_SubF64, 0,  modrm - 0xE8, True );
               break;

            case 0xF0 ... 0xF7: 
               fp_do_oprev_ST_ST ( "divr", Iop_DivF64, 0, modrm - 0xF0, True );
               break;

            case 0xF8 ... 0xFF: 
               fp_do_op_ST_ST ( "div", Iop_DivF64, 0, modrm - 0xF8, True );
               break;

            default: 
               goto decode_fail;
         }

      }
   }

   
   else
   if (first_opcode == 0xDF) {

      if (modrm < 0xC0) {

         IRTemp addr = disAMode( &len, sorb, delta, dis_buf );
         delta += len;

         switch (gregOfRM(modrm)) {

            case 0: 
               DIP("fildw %s\n", dis_buf);
               fp_push();
               put_ST(0, unop(Iop_I32StoF64,
                              unop(Iop_16Sto32,
                                   loadLE(Ity_I16, mkexpr(addr)))));
               break;

            case 1: 
               DIP("fisttps %s\n", dis_buf);
               storeLE( mkexpr(addr), 
                        binop(Iop_F64toI16S, mkU32(Irrm_ZERO), get_ST(0)) );
               fp_pop();
               break;

            case 2: 
               DIP("fistp %s\n", dis_buf);
               storeLE( mkexpr(addr), 
                        binop(Iop_F64toI16S, get_roundingmode(), get_ST(0)) );
               break;

            case 3: 
               DIP("fistps %s\n", dis_buf);
               storeLE( mkexpr(addr), 
                        binop(Iop_F64toI16S, get_roundingmode(), get_ST(0)) );
               fp_pop();
               break;

            case 5: 
               DIP("fildll %s\n", dis_buf);
               fp_push();
               put_ST(0, binop(Iop_I64StoF64,
                               get_roundingmode(),
                               loadLE(Ity_I64, mkexpr(addr))));
               break;

            case 7: 
               DIP("fistpll %s\n", dis_buf);
               storeLE( mkexpr(addr), 
                        binop(Iop_F64toI64S, get_roundingmode(), get_ST(0)) );
               fp_pop();
               break;

            default:
               vex_printf("unhandled opc_aux = 0x%2x\n", gregOfRM(modrm));
               vex_printf("first_opcode == 0xDF\n");
               goto decode_fail;
         }

      } else {

         delta++;
         switch (modrm) {

            case 0xC0: 
               DIP("ffreep %%st(%d)\n", 0);
               put_ST_TAG ( 0, mkU8(0) );
               fp_pop();
               break;

            case 0xE0: 
               DIP("fnstsw %%ax\n");
               
               if (0) {
                  putIReg(2, R_EAX, get_FPU_sw());
               } else {
                  /* So a somewhat lame kludge is to make it very
                     clear to Memcheck that the value is written to
                     both %AH and %AL.  This generates marginally
                     worse code, but I don't think it matters much. */
                  IRTemp t16 = newTemp(Ity_I16);
                  assign(t16, get_FPU_sw());
                  putIReg( 1, R_AL, unop(Iop_16to8, mkexpr(t16)) );
                  putIReg( 1, R_AH, unop(Iop_16HIto8, mkexpr(t16)) );
               }
               break;

            case 0xE8 ... 0xEF: 
               fp_do_ucomi_ST0_STi( (UInt)modrm - 0xE8, True );
               break;

            case 0xF0 ... 0xF7: 
               
               fp_do_ucomi_ST0_STi( (UInt)modrm - 0xF0, True );
               break;

            default: 
               goto decode_fail;
         }
      }

   }

   else
   vpanic("dis_FPU(x86): invalid primary opcode");

   *decode_ok = True;
   return delta;

  decode_fail:
   *decode_ok = False;
   return delta;
}




static void do_MMX_preamble ( void )
{
   Int         i;
   IRRegArray* descr = mkIRRegArray( OFFB_FPTAGS, Ity_I8, 8 );
   IRExpr*     zero  = mkU32(0);
   IRExpr*     tag1  = mkU8(1);
   put_ftop(zero);
   for (i = 0; i < 8; i++)
      stmt( IRStmt_PutI( mkIRPutI(descr, zero, i, tag1) ) );
}

static void do_EMMS_preamble ( void )
{
   Int         i;
   IRRegArray* descr = mkIRRegArray( OFFB_FPTAGS, Ity_I8, 8 );
   IRExpr*     zero  = mkU32(0);
   IRExpr*     tag0  = mkU8(0);
   put_ftop(zero);
   for (i = 0; i < 8; i++)
      stmt( IRStmt_PutI( mkIRPutI(descr, zero, i, tag0) ) );
}


static IRExpr* getMMXReg ( UInt archreg )
{
   vassert(archreg < 8);
   return IRExpr_Get( OFFB_FPREGS + 8 * archreg, Ity_I64 );
}


static void putMMXReg ( UInt archreg, IRExpr* e )
{
   vassert(archreg < 8);
   vassert(typeOfIRExpr(irsb->tyenv,e) == Ity_I64);
   stmt( IRStmt_Put( OFFB_FPREGS + 8 * archreg, e ) );
}



static 
UInt dis_MMXop_regmem_to_reg ( UChar  sorb,
                               Int    delta,
                               UChar  opc,
                               const HChar* name,
                               Bool   show_granularity )
{
   HChar   dis_buf[50];
   UChar   modrm = getIByte(delta);
   Bool    isReg = epartIsReg(modrm);
   IRExpr* argL  = NULL;
   IRExpr* argR  = NULL;
   IRExpr* argG  = NULL;
   IRExpr* argE  = NULL;
   IRTemp  res   = newTemp(Ity_I64);

   Bool    invG  = False;
   IROp    op    = Iop_INVALID;
   void*   hAddr = NULL;
   Bool    eLeft = False;
   const HChar*  hName = NULL;

#  define XXX(_name) do { hAddr = &_name; hName = #_name; } while (0)

   switch (opc) {
      
      case 0xFC: op = Iop_Add8x8; break;
      case 0xFD: op = Iop_Add16x4; break;
      case 0xFE: op = Iop_Add32x2; break;

      case 0xEC: op = Iop_QAdd8Sx8; break;
      case 0xED: op = Iop_QAdd16Sx4; break;

      case 0xDC: op = Iop_QAdd8Ux8; break;
      case 0xDD: op = Iop_QAdd16Ux4; break;

      case 0xF8: op = Iop_Sub8x8;  break;
      case 0xF9: op = Iop_Sub16x4; break;
      case 0xFA: op = Iop_Sub32x2; break;

      case 0xE8: op = Iop_QSub8Sx8; break;
      case 0xE9: op = Iop_QSub16Sx4; break;

      case 0xD8: op = Iop_QSub8Ux8; break;
      case 0xD9: op = Iop_QSub16Ux4; break;

      case 0xE5: op = Iop_MulHi16Sx4; break;
      case 0xD5: op = Iop_Mul16x4; break;
      case 0xF5: XXX(x86g_calculate_mmx_pmaddwd); break;

      case 0x74: op = Iop_CmpEQ8x8; break;
      case 0x75: op = Iop_CmpEQ16x4; break;
      case 0x76: op = Iop_CmpEQ32x2; break;

      case 0x64: op = Iop_CmpGT8Sx8; break;
      case 0x65: op = Iop_CmpGT16Sx4; break;
      case 0x66: op = Iop_CmpGT32Sx2; break;

      case 0x6B: op = Iop_QNarrowBin32Sto16Sx4; eLeft = True; break;
      case 0x63: op = Iop_QNarrowBin16Sto8Sx8;  eLeft = True; break;
      case 0x67: op = Iop_QNarrowBin16Sto8Ux8;  eLeft = True; break;

      case 0x68: op = Iop_InterleaveHI8x8;  eLeft = True; break;
      case 0x69: op = Iop_InterleaveHI16x4; eLeft = True; break;
      case 0x6A: op = Iop_InterleaveHI32x2; eLeft = True; break;

      case 0x60: op = Iop_InterleaveLO8x8;  eLeft = True; break;
      case 0x61: op = Iop_InterleaveLO16x4; eLeft = True; break;
      case 0x62: op = Iop_InterleaveLO32x2; eLeft = True; break;

      case 0xDB: op = Iop_And64; break;
      case 0xDF: op = Iop_And64; invG = True; break;
      case 0xEB: op = Iop_Or64; break;
      case 0xEF: 
                 op = Iop_Xor64; break;

      
      case 0xE0: op = Iop_Avg8Ux8;    break;
      case 0xE3: op = Iop_Avg16Ux4;   break;
      case 0xEE: op = Iop_Max16Sx4;   break;
      case 0xDE: op = Iop_Max8Ux8;    break;
      case 0xEA: op = Iop_Min16Sx4;   break;
      case 0xDA: op = Iop_Min8Ux8;    break;
      case 0xE4: op = Iop_MulHi16Ux4; break;
      case 0xF6: XXX(x86g_calculate_mmx_psadbw); break;

      
      case 0xD4: op = Iop_Add64; break;
      case 0xFB: op = Iop_Sub64; break;

      default: 
         vex_printf("\n0x%x\n", (Int)opc);
         vpanic("dis_MMXop_regmem_to_reg");
   }

#  undef XXX

   argG = getMMXReg(gregOfRM(modrm));
   if (invG)
      argG = unop(Iop_Not64, argG);

   if (isReg) {
      delta++;
      argE = getMMXReg(eregOfRM(modrm));
   } else {
      Int    len;
      IRTemp addr = disAMode( &len, sorb, delta, dis_buf );
      delta += len;
      argE = loadLE(Ity_I64, mkexpr(addr));
   }

   if (eLeft) {
      argL = argE;
      argR = argG;
   } else {
      argL = argG;
      argR = argE;
   }

   if (op != Iop_INVALID) {
      vassert(hName == NULL);
      vassert(hAddr == NULL);
      assign(res, binop(op, argL, argR));
   } else {
      vassert(hName != NULL);
      vassert(hAddr != NULL);
      assign( res, 
              mkIRExprCCall(
                 Ity_I64, 
                 0, hName, hAddr,
                 mkIRExprVec_2( argL, argR )
              ) 
            );
   }

   putMMXReg( gregOfRM(modrm), mkexpr(res) );

   DIP("%s%s %s, %s\n", 
       name, show_granularity ? nameMMXGran(opc & 3) : "",
       ( isReg ? nameMMXReg(eregOfRM(modrm)) : dis_buf ),
       nameMMXReg(gregOfRM(modrm)) );

   return delta;
}



static UInt dis_MMX_shiftG_byE ( UChar sorb, Int delta, 
                                 const HChar* opname, IROp op )
{
   HChar   dis_buf[50];
   Int     alen, size;
   IRTemp  addr;
   Bool    shl, shr, sar;
   UChar   rm   = getIByte(delta);
   IRTemp  g0   = newTemp(Ity_I64);
   IRTemp  g1   = newTemp(Ity_I64);
   IRTemp  amt  = newTemp(Ity_I32);
   IRTemp  amt8 = newTemp(Ity_I8);

   if (epartIsReg(rm)) {
      assign( amt, unop(Iop_64to32, getMMXReg(eregOfRM(rm))) );
      DIP("%s %s,%s\n", opname,
                        nameMMXReg(eregOfRM(rm)),
                        nameMMXReg(gregOfRM(rm)) );
      delta++;
   } else {
      addr = disAMode ( &alen, sorb, delta, dis_buf );
      assign( amt, loadLE(Ity_I32, mkexpr(addr)) );
      DIP("%s %s,%s\n", opname,
                        dis_buf,
                        nameMMXReg(gregOfRM(rm)) );
      delta += alen;
   }
   assign( g0,   getMMXReg(gregOfRM(rm)) );
   assign( amt8, unop(Iop_32to8, mkexpr(amt)) );

   shl = shr = sar = False;
   size = 0;
   switch (op) {
      case Iop_ShlN16x4: shl = True; size = 32; break;
      case Iop_ShlN32x2: shl = True; size = 32; break;
      case Iop_Shl64:    shl = True; size = 64; break;
      case Iop_ShrN16x4: shr = True; size = 16; break;
      case Iop_ShrN32x2: shr = True; size = 32; break;
      case Iop_Shr64:    shr = True; size = 64; break;
      case Iop_SarN16x4: sar = True; size = 16; break;
      case Iop_SarN32x2: sar = True; size = 32; break;
      default: vassert(0);
   }

   if (shl || shr) {
     assign( 
        g1,
        IRExpr_ITE(
           binop(Iop_CmpLT32U,mkexpr(amt),mkU32(size)),
           binop(op, mkexpr(g0), mkexpr(amt8)),
           mkU64(0)
        )
     );
   } else 
   if (sar) {
     assign( 
        g1,
        IRExpr_ITE(
           binop(Iop_CmpLT32U,mkexpr(amt),mkU32(size)),
           binop(op, mkexpr(g0), mkexpr(amt8)),
           binop(op, mkexpr(g0), mkU8(size-1))
        )
     );
   } else {
      
      vassert(0);
   }

   putMMXReg( gregOfRM(rm), mkexpr(g1) );
   return delta;
}



static 
UInt dis_MMX_shiftE_imm ( Int delta, const HChar* opname, IROp op )
{
   Bool    shl, shr, sar;
   UChar   rm   = getIByte(delta);
   IRTemp  e0   = newTemp(Ity_I64);
   IRTemp  e1   = newTemp(Ity_I64);
   UChar   amt, size;
   vassert(epartIsReg(rm));
   vassert(gregOfRM(rm) == 2 
           || gregOfRM(rm) == 4 || gregOfRM(rm) == 6);
   amt = getIByte(delta+1);
   delta += 2;
   DIP("%s $%d,%s\n", opname,
                      (Int)amt,
                      nameMMXReg(eregOfRM(rm)) );

   assign( e0, getMMXReg(eregOfRM(rm)) );

   shl = shr = sar = False;
   size = 0;
   switch (op) {
      case Iop_ShlN16x4: shl = True; size = 16; break;
      case Iop_ShlN32x2: shl = True; size = 32; break;
      case Iop_Shl64:    shl = True; size = 64; break;
      case Iop_SarN16x4: sar = True; size = 16; break;
      case Iop_SarN32x2: sar = True; size = 32; break;
      case Iop_ShrN16x4: shr = True; size = 16; break;
      case Iop_ShrN32x2: shr = True; size = 32; break;
      case Iop_Shr64:    shr = True; size = 64; break;
      default: vassert(0);
   }

   if (shl || shr) {
      assign( e1, amt >= size 
                     ? mkU64(0)
                     : binop(op, mkexpr(e0), mkU8(amt))
      );
   } else 
   if (sar) {
      assign( e1, amt >= size 
                     ? binop(op, mkexpr(e0), mkU8(size-1))
                     : binop(op, mkexpr(e0), mkU8(amt))
      );
   } else {
      
      vassert(0);
   }

   putMMXReg( eregOfRM(rm), mkexpr(e1) );
   return delta;
}



static
UInt dis_MMX ( Bool* decode_ok, UChar sorb, Int sz, Int delta )
{
   Int   len;
   UChar modrm;
   HChar dis_buf[50];
   UChar opc = getIByte(delta);
   delta++;

   
   do_MMX_preamble();

   switch (opc) {

      case 0x6E: 
         
         if (sz != 4) 
            goto mmx_decode_failure;
         modrm = getIByte(delta);
         if (epartIsReg(modrm)) {
            delta++;
            putMMXReg(
               gregOfRM(modrm),
               binop( Iop_32HLto64,
                      mkU32(0),
                      getIReg(4, eregOfRM(modrm)) ) );
            DIP("movd %s, %s\n", 
                nameIReg(4,eregOfRM(modrm)), nameMMXReg(gregOfRM(modrm)));
         } else {
            IRTemp addr = disAMode( &len, sorb, delta, dis_buf );
            delta += len;
            putMMXReg(
               gregOfRM(modrm),
               binop( Iop_32HLto64,
                      mkU32(0),
                      loadLE(Ity_I32, mkexpr(addr)) ) );
            DIP("movd %s, %s\n", dis_buf, nameMMXReg(gregOfRM(modrm)));
         }
         break;

      case 0x7E: 
         if (sz != 4) 
            goto mmx_decode_failure;
         modrm = getIByte(delta);
         if (epartIsReg(modrm)) {
            delta++;
            putIReg( 4, eregOfRM(modrm),
                     unop(Iop_64to32, getMMXReg(gregOfRM(modrm)) ) );
            DIP("movd %s, %s\n", 
                nameMMXReg(gregOfRM(modrm)), nameIReg(4,eregOfRM(modrm)));
         } else {
            IRTemp addr = disAMode( &len, sorb, delta, dis_buf );
            delta += len;
            storeLE( mkexpr(addr),
                     unop(Iop_64to32, getMMXReg(gregOfRM(modrm)) ) );
            DIP("movd %s, %s\n", nameMMXReg(gregOfRM(modrm)), dis_buf);
         }
         break;

      case 0x6F:
         
         if (sz != 4) 
            goto mmx_decode_failure;
         modrm = getIByte(delta);
         if (epartIsReg(modrm)) {
            delta++;
            putMMXReg( gregOfRM(modrm), getMMXReg(eregOfRM(modrm)) );
            DIP("movq %s, %s\n", 
                nameMMXReg(eregOfRM(modrm)), nameMMXReg(gregOfRM(modrm)));
         } else {
            IRTemp addr = disAMode( &len, sorb, delta, dis_buf );
            delta += len;
            putMMXReg( gregOfRM(modrm), loadLE(Ity_I64, mkexpr(addr)) );
            DIP("movq %s, %s\n", 
                dis_buf, nameMMXReg(gregOfRM(modrm)));
         }
         break;

      case 0x7F:
         
         if (sz != 4) 
            goto mmx_decode_failure;
         modrm = getIByte(delta);
         if (epartIsReg(modrm)) {
            delta++;
            putMMXReg( eregOfRM(modrm), getMMXReg(gregOfRM(modrm)) );
            DIP("movq %s, %s\n", 
                nameMMXReg(gregOfRM(modrm)), nameMMXReg(eregOfRM(modrm)));
         } else {
            IRTemp addr = disAMode( &len, sorb, delta, dis_buf );
            delta += len;
            storeLE( mkexpr(addr), getMMXReg(gregOfRM(modrm)) );
            DIP("mov(nt)q %s, %s\n", 
                nameMMXReg(gregOfRM(modrm)), dis_buf);
         }
         break;

      case 0xFC: 
      case 0xFD: 
      case 0xFE: 
         if (sz != 4) 
            goto mmx_decode_failure;
         delta = dis_MMXop_regmem_to_reg ( sorb, delta, opc, "padd", True );
         break;

      case 0xEC: 
      case 0xED: 
         if (sz != 4) 
            goto mmx_decode_failure;
         delta = dis_MMXop_regmem_to_reg ( sorb, delta, opc, "padds", True );
         break;

      case 0xDC: 
      case 0xDD: 
         if (sz != 4) 
            goto mmx_decode_failure;
         delta = dis_MMXop_regmem_to_reg ( sorb, delta, opc, "paddus", True );
         break;

      case 0xF8: 
      case 0xF9: 
      case 0xFA: 
         if (sz != 4) 
            goto mmx_decode_failure;
         delta = dis_MMXop_regmem_to_reg ( sorb, delta, opc, "psub", True );
         break;

      case 0xE8: 
      case 0xE9: 
         if (sz != 4) 
            goto mmx_decode_failure;
         delta = dis_MMXop_regmem_to_reg ( sorb, delta, opc, "psubs", True );
         break;

      case 0xD8: 
      case 0xD9: 
         if (sz != 4) 
            goto mmx_decode_failure;
         delta = dis_MMXop_regmem_to_reg ( sorb, delta, opc, "psubus", True );
         break;

      case 0xE5: 
         if (sz != 4) 
            goto mmx_decode_failure;
         delta = dis_MMXop_regmem_to_reg ( sorb, delta, opc, "pmulhw", False );
         break;

      case 0xD5: 
         if (sz != 4) 
            goto mmx_decode_failure;
         delta = dis_MMXop_regmem_to_reg ( sorb, delta, opc, "pmullw", False );
         break;

      case 0xF5: 
         vassert(sz == 4);
         delta = dis_MMXop_regmem_to_reg ( sorb, delta, opc, "pmaddwd", False );
         break;

      case 0x74: 
      case 0x75: 
      case 0x76: 
         if (sz != 4) 
            goto mmx_decode_failure;
         delta = dis_MMXop_regmem_to_reg ( sorb, delta, opc, "pcmpeq", True );
         break;

      case 0x64: 
      case 0x65: 
      case 0x66: 
         if (sz != 4) 
            goto mmx_decode_failure;
         delta = dis_MMXop_regmem_to_reg ( sorb, delta, opc, "pcmpgt", True );
         break;

      case 0x6B: 
         if (sz != 4) 
            goto mmx_decode_failure;
         delta = dis_MMXop_regmem_to_reg ( sorb, delta, opc, "packssdw", False );
         break;

      case 0x63: 
         if (sz != 4) 
            goto mmx_decode_failure;
         delta = dis_MMXop_regmem_to_reg ( sorb, delta, opc, "packsswb", False );
         break;

      case 0x67: 
         if (sz != 4) 
            goto mmx_decode_failure;
         delta = dis_MMXop_regmem_to_reg ( sorb, delta, opc, "packuswb", False );
         break;

      case 0x68: 
      case 0x69: 
      case 0x6A: 
         if (sz != 4) 
            goto mmx_decode_failure;
         delta = dis_MMXop_regmem_to_reg ( sorb, delta, opc, "punpckh", True );
         break;

      case 0x60: 
      case 0x61: 
      case 0x62: 
         if (sz != 4) 
            goto mmx_decode_failure;
         delta = dis_MMXop_regmem_to_reg ( sorb, delta, opc, "punpckl", True );
         break;

      case 0xDB: 
         if (sz != 4) 
            goto mmx_decode_failure;
         delta = dis_MMXop_regmem_to_reg ( sorb, delta, opc, "pand", False );
         break;

      case 0xDF: 
         if (sz != 4) 
            goto mmx_decode_failure;
         delta = dis_MMXop_regmem_to_reg ( sorb, delta, opc, "pandn", False );
         break;

      case 0xEB: 
         if (sz != 4) 
            goto mmx_decode_failure;
         delta = dis_MMXop_regmem_to_reg ( sorb, delta, opc, "por", False );
         break;

      case 0xEF: 
         if (sz != 4) 
            goto mmx_decode_failure;
         delta = dis_MMXop_regmem_to_reg ( sorb, delta, opc, "pxor", False );
         break; 

#     define SHIFT_BY_REG(_name,_op)                                 \
                delta = dis_MMX_shiftG_byE(sorb, delta, _name, _op); \
                break;

      
      case 0xF1: SHIFT_BY_REG("psllw", Iop_ShlN16x4);
      case 0xF2: SHIFT_BY_REG("pslld", Iop_ShlN32x2);
      case 0xF3: SHIFT_BY_REG("psllq", Iop_Shl64);

      
      case 0xD1: SHIFT_BY_REG("psrlw", Iop_ShrN16x4);
      case 0xD2: SHIFT_BY_REG("psrld", Iop_ShrN32x2);
      case 0xD3: SHIFT_BY_REG("psrlq", Iop_Shr64);

      
      case 0xE1: SHIFT_BY_REG("psraw", Iop_SarN16x4);
      case 0xE2: SHIFT_BY_REG("psrad", Iop_SarN32x2);

#     undef SHIFT_BY_REG

      case 0x71: 
      case 0x72: 
      case 0x73: {
         
         UChar byte2, subopc;
         if (sz != 4) 
            goto mmx_decode_failure;
         byte2  = getIByte(delta);           
         subopc = toUChar( (byte2 >> 3) & 7 );

#        define SHIFT_BY_IMM(_name,_op)                         \
             do { delta = dis_MMX_shiftE_imm(delta,_name,_op);  \
             } while (0)

              if (subopc == 2  && opc == 0x71) 
                 SHIFT_BY_IMM("psrlw", Iop_ShrN16x4);
         else if (subopc == 2  && opc == 0x72) 
                 SHIFT_BY_IMM("psrld", Iop_ShrN32x2);
         else if (subopc == 2  && opc == 0x73) 
                 SHIFT_BY_IMM("psrlq", Iop_Shr64);

         else if (subopc == 4  && opc == 0x71) 
                 SHIFT_BY_IMM("psraw", Iop_SarN16x4);
         else if (subopc == 4  && opc == 0x72) 
                 SHIFT_BY_IMM("psrad", Iop_SarN32x2);

         else if (subopc == 6  && opc == 0x71) 
                 SHIFT_BY_IMM("psllw", Iop_ShlN16x4);
         else if (subopc == 6  && opc == 0x72) 
                 SHIFT_BY_IMM("pslld", Iop_ShlN32x2);
         else if (subopc == 6  && opc == 0x73) 
                 SHIFT_BY_IMM("psllq", Iop_Shl64);

         else goto mmx_decode_failure;

#        undef SHIFT_BY_IMM
         break;
      }

      case 0xF7: {
         IRTemp addr    = newTemp(Ity_I32);
         IRTemp regD    = newTemp(Ity_I64);
         IRTemp regM    = newTemp(Ity_I64);
         IRTemp mask    = newTemp(Ity_I64);
         IRTemp olddata = newTemp(Ity_I64);
         IRTemp newdata = newTemp(Ity_I64);

         modrm = getIByte(delta);
         if (sz != 4 || (!epartIsReg(modrm)))
            goto mmx_decode_failure;
         delta++;

         assign( addr, handleSegOverride( sorb, getIReg(4, R_EDI) ));
         assign( regM, getMMXReg( eregOfRM(modrm) ));
         assign( regD, getMMXReg( gregOfRM(modrm) ));
         assign( mask, binop(Iop_SarN8x8, mkexpr(regM), mkU8(7)) );
         assign( olddata, loadLE( Ity_I64, mkexpr(addr) ));
         assign( newdata, 
                 binop(Iop_Or64, 
                       binop(Iop_And64, 
                             mkexpr(regD), 
                             mkexpr(mask) ),
                       binop(Iop_And64, 
                             mkexpr(olddata),
                             unop(Iop_Not64, mkexpr(mask)))) );
         storeLE( mkexpr(addr), mkexpr(newdata) );
         DIP("maskmovq %s,%s\n", nameMMXReg( eregOfRM(modrm) ),
                                 nameMMXReg( gregOfRM(modrm) ) );
         break;
      }

      
      default:
      mmx_decode_failure:
         *decode_ok = False;
         return delta; 

   }

   *decode_ok = True;
   return delta;
}



static
UInt dis_SHLRD_Gv_Ev ( UChar sorb,
                       Int delta, UChar modrm,
                       Int sz,
                       IRExpr* shift_amt,
                       Bool amt_is_literal,
                       const HChar* shift_amt_txt,
                       Bool left_shift )
{
   Int len;
   HChar dis_buf[50];

   IRType ty       = szToITy(sz);
   IRTemp gsrc     = newTemp(ty);
   IRTemp esrc     = newTemp(ty);
   IRTemp addr     = IRTemp_INVALID;
   IRTemp tmpSH    = newTemp(Ity_I8);
   IRTemp tmpL     = IRTemp_INVALID;
   IRTemp tmpRes   = IRTemp_INVALID;
   IRTemp tmpSubSh = IRTemp_INVALID;
   IROp   mkpair;
   IROp   getres;
   IROp   shift;
   IRExpr* mask = NULL;

   vassert(sz == 2 || sz == 4);


   

   assign( gsrc, getIReg(sz, gregOfRM(modrm)) );

   if (epartIsReg(modrm)) {
      delta++;
      assign( esrc, getIReg(sz, eregOfRM(modrm)) );
      DIP("sh%cd%c %s, %s, %s\n",
          ( left_shift ? 'l' : 'r' ), nameISize(sz), 
          shift_amt_txt,
          nameIReg(sz, gregOfRM(modrm)), nameIReg(sz, eregOfRM(modrm)));
   } else {
      addr = disAMode ( &len, sorb, delta, dis_buf );
      delta += len;
      assign( esrc, loadLE(ty, mkexpr(addr)) );
      DIP("sh%cd%c %s, %s, %s\n", 
          ( left_shift ? 'l' : 'r' ), nameISize(sz), 
          shift_amt_txt,
          nameIReg(sz, gregOfRM(modrm)), dis_buf);
   }

   

   if (sz == 4) {
      tmpL     = newTemp(Ity_I64);
      tmpRes   = newTemp(Ity_I32);
      tmpSubSh = newTemp(Ity_I32);
      mkpair   = Iop_32HLto64;
      getres   = left_shift ? Iop_64HIto32 : Iop_64to32;
      shift    = left_shift ? Iop_Shl64 : Iop_Shr64;
      mask     = mkU8(31);
   } else {
      
      tmpL     = newTemp(Ity_I32);
      tmpRes   = newTemp(Ity_I16);
      tmpSubSh = newTemp(Ity_I16);
      mkpair   = Iop_16HLto32;
      getres   = left_shift ? Iop_32HIto16 : Iop_32to16;
      shift    = left_shift ? Iop_Shl32 : Iop_Shr32;
      mask     = mkU8(15);
   }


   assign( tmpSH, binop(Iop_And8, shift_amt, mask) );

   if (left_shift)
      assign( tmpL, binop(mkpair, mkexpr(esrc), mkexpr(gsrc)) );
   else
      assign( tmpL, binop(mkpair, mkexpr(gsrc), mkexpr(esrc)) );

   assign( tmpRes, unop(getres, binop(shift, mkexpr(tmpL), mkexpr(tmpSH)) ) );
   assign( tmpSubSh, 
           unop(getres, 
                binop(shift, 
                      mkexpr(tmpL), 
                      binop(Iop_And8, 
                            binop(Iop_Sub8, mkexpr(tmpSH), mkU8(1) ),
                            mask))) );

   setFlags_DEP1_DEP2_shift ( left_shift ? Iop_Shl32 : Iop_Sar32,
                              tmpRes, tmpSubSh, ty, tmpSH );

   

   if (epartIsReg(modrm)) {
      putIReg(sz, eregOfRM(modrm), mkexpr(tmpRes));
   } else {
      storeLE( mkexpr(addr), mkexpr(tmpRes) );
   }

   if (amt_is_literal) delta++;
   return delta;
}



typedef enum { BtOpNone, BtOpSet, BtOpReset, BtOpComp } BtOp;

static const HChar* nameBtOp ( BtOp op )
{
   switch (op) {
      case BtOpNone:  return "";
      case BtOpSet:   return "s";
      case BtOpReset: return "r";
      case BtOpComp:  return "c";
      default: vpanic("nameBtOp(x86)");
   }
}


static
UInt dis_bt_G_E ( const VexAbiInfo* vbi,
                  UChar sorb, Bool locked, Int sz, Int delta, BtOp op )
{
   HChar  dis_buf[50];
   UChar  modrm;
   Int    len;
   IRTemp t_fetched, t_bitno0, t_bitno1, t_bitno2, t_addr0, 
          t_addr1, t_esp, t_mask, t_new;

   vassert(sz == 2 || sz == 4);

   t_fetched = t_bitno0 = t_bitno1 = t_bitno2 
             = t_addr0 = t_addr1 = t_esp 
             = t_mask = t_new = IRTemp_INVALID;

   t_fetched = newTemp(Ity_I8);
   t_new     = newTemp(Ity_I8);
   t_bitno0  = newTemp(Ity_I32);
   t_bitno1  = newTemp(Ity_I32);
   t_bitno2  = newTemp(Ity_I8);
   t_addr1   = newTemp(Ity_I32);
   modrm     = getIByte(delta);

   assign( t_bitno0, widenSto32(getIReg(sz, gregOfRM(modrm))) );
   
   if (epartIsReg(modrm)) {
      delta++;
      
      t_esp = newTemp(Ity_I32);
      t_addr0 = newTemp(Ity_I32);

      vassert(vbi->guest_stack_redzone_size == 0);
      assign( t_esp, binop(Iop_Sub32, getIReg(4, R_ESP), mkU32(128)) );
      putIReg(4, R_ESP, mkexpr(t_esp));

      storeLE( mkexpr(t_esp), getIReg(sz, eregOfRM(modrm)) );

      
      assign( t_addr0, mkexpr(t_esp) );

      assign( t_bitno1, binop(Iop_And32, 
                              mkexpr(t_bitno0), 
                              mkU32(sz == 4 ? 31 : 15)) );

   } else {
      t_addr0 = disAMode ( &len, sorb, delta, dis_buf );
      delta += len;
      assign( t_bitno1, mkexpr(t_bitno0) );
   }
  
  
   
   assign( t_addr1, 
           binop(Iop_Add32, 
                 mkexpr(t_addr0), 
                 binop(Iop_Sar32, mkexpr(t_bitno1), mkU8(3))) );

   

   assign( t_bitno2, 
           unop(Iop_32to8, 
                binop(Iop_And32, mkexpr(t_bitno1), mkU32(7))) );

   

   if (op != BtOpNone) {
      t_mask = newTemp(Ity_I8);
      assign( t_mask, binop(Iop_Shl8, mkU8(1), mkexpr(t_bitno2)) );
   }

   

   assign( t_fetched, loadLE(Ity_I8, mkexpr(t_addr1)) );

   if (op != BtOpNone) {
      switch (op) {
         case BtOpSet:
            assign( t_new,
                    binop(Iop_Or8, mkexpr(t_fetched), mkexpr(t_mask)) );
            break;
         case BtOpComp:
            assign( t_new,
                    binop(Iop_Xor8, mkexpr(t_fetched), mkexpr(t_mask)) );
            break;
         case BtOpReset:
            assign( t_new,
                    binop(Iop_And8, mkexpr(t_fetched), 
                                    unop(Iop_Not8, mkexpr(t_mask))) );
            break;
         default: 
            vpanic("dis_bt_G_E(x86)");
      }
      if (locked && !epartIsReg(modrm)) {
         casLE( mkexpr(t_addr1), mkexpr(t_fetched),
                                 mkexpr(t_new),
                                 guest_EIP_curr_instr );
      } else {
         storeLE( mkexpr(t_addr1), mkexpr(t_new) );
      }
   }
 
   
   
   stmt( IRStmt_Put( OFFB_CC_OP,   mkU32(X86G_CC_OP_COPY) ));
   stmt( IRStmt_Put( OFFB_CC_DEP2, mkU32(0) ));
   stmt( IRStmt_Put( 
            OFFB_CC_DEP1,
            binop(Iop_And32,
                  binop(Iop_Shr32, 
                        unop(Iop_8Uto32, mkexpr(t_fetched)),
                        mkexpr(t_bitno2)),
                  mkU32(1)))
       );
   stmt( IRStmt_Put( OFFB_CC_NDEP, mkU32(0) ));

   
   if (epartIsReg(modrm)) {
      
      putIReg(sz, eregOfRM(modrm), loadLE(szToITy(sz), mkexpr(t_esp)) );
      putIReg(4, R_ESP, binop(Iop_Add32, mkexpr(t_esp), mkU32(128)) );
   }

   DIP("bt%s%c %s, %s\n",
       nameBtOp(op), nameISize(sz), nameIReg(sz, gregOfRM(modrm)), 
       ( epartIsReg(modrm) ? nameIReg(sz, eregOfRM(modrm)) : dis_buf ) );
 
   return delta;
}



static
UInt dis_bs_E_G ( UChar sorb, Int sz, Int delta, Bool fwds )
{
   Bool   isReg;
   UChar  modrm;
   HChar  dis_buf[50];
   
   IRType ty  = szToITy(sz);
   IRTemp src = newTemp(ty);
   IRTemp dst = newTemp(ty);

   IRTemp src32 = newTemp(Ity_I32);
   IRTemp dst32 = newTemp(Ity_I32);
   IRTemp srcB  = newTemp(Ity_I1);

   vassert(sz == 4 || sz == 2);

   modrm = getIByte(delta);

   isReg = epartIsReg(modrm);
   if (isReg) {
      delta++;
      assign( src, getIReg(sz, eregOfRM(modrm)) );
   } else {
      Int    len;
      IRTemp addr = disAMode( &len, sorb, delta, dis_buf );
      delta += len;
      assign( src, loadLE(ty, mkexpr(addr)) );
   }

   DIP("bs%c%c %s, %s\n",
       fwds ? 'f' : 'r', nameISize(sz), 
       ( isReg ? nameIReg(sz, eregOfRM(modrm)) : dis_buf ), 
       nameIReg(sz, gregOfRM(modrm)));

   assign( srcB, binop(mkSizedOp(ty,Iop_ExpCmpNE8),
                       mkexpr(src), mkU(ty,0)) );

   stmt( IRStmt_Put( OFFB_CC_OP,   mkU32(X86G_CC_OP_COPY) ));
   stmt( IRStmt_Put( OFFB_CC_DEP2, mkU32(0) ));
   stmt( IRStmt_Put( 
            OFFB_CC_DEP1,
            IRExpr_ITE( mkexpr(srcB),
                        
                        mkU32(0),
                        
                        mkU32(X86G_CC_MASK_Z)
                        )
       ));
   stmt( IRStmt_Put( OFFB_CC_NDEP, mkU32(0) ));


   if (sz == 2)
      assign( src32, unop(Iop_16Uto32, mkexpr(src)) );
   else
      assign( src32, mkexpr(src) );

   
   assign( dst32,   
           IRExpr_ITE( 
              mkexpr(srcB),
              
              fwds ? unop(Iop_Ctz32, mkexpr(src32))
                   : binop(Iop_Sub32, 
                           mkU32(31), 
                           unop(Iop_Clz32, mkexpr(src32))),
              
              widenUto32( getIReg( sz, gregOfRM(modrm) ) )
           )
         );

   if (sz == 2)
      assign( dst, unop(Iop_32to16, mkexpr(dst32)) );
   else
      assign( dst, mkexpr(dst32) );

   
   putIReg( sz, gregOfRM(modrm), mkexpr(dst) );

   return delta;
}


static 
void codegen_xchg_eAX_Reg ( Int sz, Int reg )
{
   IRType ty = szToITy(sz);
   IRTemp t1 = newTemp(ty);
   IRTemp t2 = newTemp(ty);
   vassert(sz == 2 || sz == 4);
   assign( t1, getIReg(sz, R_EAX) );
   assign( t2, getIReg(sz, reg) );
   putIReg( sz, R_EAX, mkexpr(t2) );
   putIReg( sz, reg, mkexpr(t1) );
   DIP("xchg%c %s, %s\n", 
       nameISize(sz), nameIReg(sz, R_EAX), nameIReg(sz, reg));
}


static 
void codegen_SAHF ( void )
{
   UInt   mask_SZACP = X86G_CC_MASK_S|X86G_CC_MASK_Z|X86G_CC_MASK_A
                       |X86G_CC_MASK_C|X86G_CC_MASK_P;
   IRTemp oldflags   = newTemp(Ity_I32);
   assign( oldflags, mk_x86g_calculate_eflags_all() );
   stmt( IRStmt_Put( OFFB_CC_OP,   mkU32(X86G_CC_OP_COPY) ));
   stmt( IRStmt_Put( OFFB_CC_NDEP, mkU32(0) ));
   stmt( IRStmt_Put( OFFB_CC_DEP2, mkU32(0) ));
   stmt( IRStmt_Put( OFFB_CC_DEP1,
         binop(Iop_Or32,
               binop(Iop_And32, mkexpr(oldflags), mkU32(X86G_CC_MASK_O)),
               binop(Iop_And32, 
                     binop(Iop_Shr32, getIReg(4, R_EAX), mkU8(8)),
                     mkU32(mask_SZACP))
              )
   ));
   stmt( IRStmt_Put( OFFB_CC_NDEP, mkU32(0) ));
}


static 
void codegen_LAHF ( void  )
{
   
   IRExpr* eax_with_hole;
   IRExpr* new_byte;
   IRExpr* new_eax;
   UInt    mask_SZACP = X86G_CC_MASK_S|X86G_CC_MASK_Z|X86G_CC_MASK_A
                        |X86G_CC_MASK_C|X86G_CC_MASK_P;

   IRTemp  flags = newTemp(Ity_I32);
   assign( flags, mk_x86g_calculate_eflags_all() );

   eax_with_hole 
      = binop(Iop_And32, getIReg(4, R_EAX), mkU32(0xFFFF00FF));
   new_byte 
      = binop(Iop_Or32, binop(Iop_And32, mkexpr(flags), mkU32(mask_SZACP)),
                        mkU32(1<<1));
   new_eax 
      = binop(Iop_Or32, eax_with_hole,
                        binop(Iop_Shl32, new_byte, mkU8(8)));
   putIReg(4, R_EAX, new_eax);
}


static
UInt dis_cmpxchg_G_E ( UChar       sorb,
                       Bool        locked,
                       Int         size, 
                       Int         delta0 )
{
   HChar dis_buf[50];
   Int   len;

   IRType ty    = szToITy(size);
   IRTemp acc   = newTemp(ty);
   IRTemp src   = newTemp(ty);
   IRTemp dest  = newTemp(ty);
   IRTemp dest2 = newTemp(ty);
   IRTemp acc2  = newTemp(ty);
   IRTemp cond  = newTemp(Ity_I1);
   IRTemp addr  = IRTemp_INVALID;
   UChar  rm    = getUChar(delta0);

   if (epartIsReg(rm)) {
      
      assign( dest, getIReg(size, eregOfRM(rm)) );
      delta0++;
      assign( src, getIReg(size, gregOfRM(rm)) );
      assign( acc, getIReg(size, R_EAX) );
      setFlags_DEP1_DEP2(Iop_Sub8, acc, dest, ty);
      assign( cond, mk_x86g_calculate_condition(X86CondZ) );
      assign( dest2, IRExpr_ITE(mkexpr(cond), mkexpr(src), mkexpr(dest)) );
      assign( acc2,  IRExpr_ITE(mkexpr(cond), mkexpr(acc), mkexpr(dest)) );
      putIReg(size, R_EAX, mkexpr(acc2));
      putIReg(size, eregOfRM(rm), mkexpr(dest2));
      DIP("cmpxchg%c %s,%s\n", nameISize(size),
                               nameIReg(size,gregOfRM(rm)),
                               nameIReg(size,eregOfRM(rm)) );
   } 
   else if (!epartIsReg(rm) && !locked) {
      
      addr = disAMode ( &len, sorb, delta0, dis_buf );
      assign( dest, loadLE(ty, mkexpr(addr)) );
      delta0 += len;
      assign( src, getIReg(size, gregOfRM(rm)) );
      assign( acc, getIReg(size, R_EAX) );
      setFlags_DEP1_DEP2(Iop_Sub8, acc, dest, ty);
      assign( cond, mk_x86g_calculate_condition(X86CondZ) );
      assign( dest2, IRExpr_ITE(mkexpr(cond), mkexpr(src), mkexpr(dest)) );
      assign( acc2,  IRExpr_ITE(mkexpr(cond), mkexpr(acc), mkexpr(dest)) );
      putIReg(size, R_EAX, mkexpr(acc2));
      storeLE( mkexpr(addr), mkexpr(dest2) );
      DIP("cmpxchg%c %s,%s\n", nameISize(size), 
                               nameIReg(size,gregOfRM(rm)), dis_buf);
   }
   else if (!epartIsReg(rm) && locked) {
      
      addr = disAMode ( &len, sorb, delta0, dis_buf );
      delta0 += len;
      assign( src, getIReg(size, gregOfRM(rm)) );
      assign( acc, getIReg(size, R_EAX) );
      stmt( IRStmt_CAS( 
         mkIRCAS( IRTemp_INVALID, dest, Iend_LE, mkexpr(addr), 
                  NULL, mkexpr(acc), NULL, mkexpr(src) )
      ));
      setFlags_DEP1_DEP2(Iop_Sub8, acc, dest, ty);
      assign( cond, mk_x86g_calculate_condition(X86CondZ) );
      assign( acc2,  IRExpr_ITE(mkexpr(cond), mkexpr(acc), mkexpr(dest)) );
      putIReg(size, R_EAX, mkexpr(acc2));
      DIP("cmpxchg%c %s,%s\n", nameISize(size), 
                               nameIReg(size,gregOfRM(rm)), dis_buf);
   }
   else vassert(0);

   return delta0;
}


static
UInt dis_cmov_E_G ( UChar       sorb,
                    Int         sz, 
                    X86Condcode cond,
                    Int         delta0 )
{
   UChar rm  = getIByte(delta0);
   HChar dis_buf[50];
   Int   len;

   IRType ty   = szToITy(sz);
   IRTemp tmps = newTemp(ty);
   IRTemp tmpd = newTemp(ty);

   if (epartIsReg(rm)) {
      assign( tmps, getIReg(sz, eregOfRM(rm)) );
      assign( tmpd, getIReg(sz, gregOfRM(rm)) );

      putIReg(sz, gregOfRM(rm),
                  IRExpr_ITE( mk_x86g_calculate_condition(cond),
                              mkexpr(tmps),
                              mkexpr(tmpd) )
             );
      DIP("cmov%c%s %s,%s\n", nameISize(sz), 
                              name_X86Condcode(cond),
                              nameIReg(sz,eregOfRM(rm)),
                              nameIReg(sz,gregOfRM(rm)));
      return 1+delta0;
   }

       
   {
      IRTemp addr = disAMode ( &len, sorb, delta0, dis_buf );
      assign( tmps, loadLE(ty, mkexpr(addr)) );
      assign( tmpd, getIReg(sz, gregOfRM(rm)) );

      putIReg(sz, gregOfRM(rm),
                  IRExpr_ITE( mk_x86g_calculate_condition(cond),
                              mkexpr(tmps),
                              mkexpr(tmpd) )
             );

      DIP("cmov%c%s %s,%s\n", nameISize(sz), 
                              name_X86Condcode(cond),
                              dis_buf,
                              nameIReg(sz,gregOfRM(rm)));
      return len+delta0;
   }
}


static
UInt dis_xadd_G_E ( UChar sorb, Bool locked, Int sz, Int delta0,
                    Bool* decodeOK )
{
   Int   len;
   UChar rm = getIByte(delta0);
   HChar dis_buf[50];

   IRType ty    = szToITy(sz);
   IRTemp tmpd  = newTemp(ty);
   IRTemp tmpt0 = newTemp(ty);
   IRTemp tmpt1 = newTemp(ty);


   if (epartIsReg(rm)) {
      
      assign( tmpd,  getIReg(sz, eregOfRM(rm)));
      assign( tmpt0, getIReg(sz, gregOfRM(rm)) );
      assign( tmpt1, binop(mkSizedOp(ty,Iop_Add8),
                           mkexpr(tmpd), mkexpr(tmpt0)) );
      setFlags_DEP1_DEP2( Iop_Add8, tmpd, tmpt0, ty );
      putIReg(sz, eregOfRM(rm), mkexpr(tmpt1));
      putIReg(sz, gregOfRM(rm), mkexpr(tmpd));
      DIP("xadd%c %s, %s\n",
          nameISize(sz), nameIReg(sz,gregOfRM(rm)), 
          				 nameIReg(sz,eregOfRM(rm)));
      *decodeOK = True;
      return 1+delta0;
   }
   else if (!epartIsReg(rm) && !locked) {
      
      IRTemp addr = disAMode ( &len, sorb, delta0, dis_buf );
      assign( tmpd,  loadLE(ty, mkexpr(addr)) );
      assign( tmpt0, getIReg(sz, gregOfRM(rm)) );
      assign( tmpt1, binop(mkSizedOp(ty,Iop_Add8),
                           mkexpr(tmpd), mkexpr(tmpt0)) );
      storeLE( mkexpr(addr), mkexpr(tmpt1) );
      setFlags_DEP1_DEP2( Iop_Add8, tmpd, tmpt0, ty );
      putIReg(sz, gregOfRM(rm), mkexpr(tmpd));
      DIP("xadd%c %s, %s\n",
          nameISize(sz), nameIReg(sz,gregOfRM(rm)), dis_buf);
      *decodeOK = True;
      return len+delta0;
   }
   else if (!epartIsReg(rm) && locked) {
      
      IRTemp addr = disAMode ( &len, sorb, delta0, dis_buf );
      assign( tmpd,  loadLE(ty, mkexpr(addr)) );
      assign( tmpt0, getIReg(sz, gregOfRM(rm)) );
      assign( tmpt1, binop(mkSizedOp(ty,Iop_Add8), 
                           mkexpr(tmpd), mkexpr(tmpt0)) );
      casLE( mkexpr(addr), mkexpr(tmpd),
                           mkexpr(tmpt1), guest_EIP_curr_instr );
      setFlags_DEP1_DEP2( Iop_Add8, tmpd, tmpt0, ty );
      putIReg(sz, gregOfRM(rm), mkexpr(tmpd));
      DIP("xadd%c %s, %s\n",
          nameISize(sz), nameIReg(sz,gregOfRM(rm)), dis_buf);
      *decodeOK = True;
      return len+delta0;
   }
   
   vassert(0);
}


static
UInt dis_mov_Ew_Sw ( UChar sorb, Int delta0 )
{
   Int    len;
   IRTemp addr;
   UChar  rm  = getIByte(delta0);
   HChar  dis_buf[50];

   if (epartIsReg(rm)) {
      putSReg( gregOfRM(rm), getIReg(2, eregOfRM(rm)) );
      DIP("movw %s,%s\n", nameIReg(2,eregOfRM(rm)), nameSReg(gregOfRM(rm)));
      return 1+delta0;
   } else {
      addr = disAMode ( &len, sorb, delta0, dis_buf );
      putSReg( gregOfRM(rm), loadLE(Ity_I16, mkexpr(addr)) );
      DIP("movw %s,%s\n", dis_buf, nameSReg(gregOfRM(rm)));
      return len+delta0;
   }
}


static
UInt dis_mov_Sw_Ew ( UChar sorb,
                     Int   sz,
                     Int   delta0 )
{
   Int    len;
   IRTemp addr;
   UChar  rm  = getIByte(delta0);
   HChar  dis_buf[50];

   vassert(sz == 2 || sz == 4);

   if (epartIsReg(rm)) {
      if (sz == 4)
         putIReg(4, eregOfRM(rm), unop(Iop_16Uto32, getSReg(gregOfRM(rm))));
      else
         putIReg(2, eregOfRM(rm), getSReg(gregOfRM(rm)));

      DIP("mov %s,%s\n", nameSReg(gregOfRM(rm)), nameIReg(sz,eregOfRM(rm)));
      return 1+delta0;
   } else {
      addr = disAMode ( &len, sorb, delta0, dis_buf );
      storeLE( mkexpr(addr), getSReg(gregOfRM(rm)) );
      DIP("mov %s,%s\n", nameSReg(gregOfRM(rm)), dis_buf);
      return len+delta0;
   }
}


static 
void dis_push_segreg ( UInt sreg, Int sz )
{
    IRTemp t1 = newTemp(Ity_I16);
    IRTemp ta = newTemp(Ity_I32);
    vassert(sz == 2 || sz == 4);

    assign( t1, getSReg(sreg) );
    assign( ta, binop(Iop_Sub32, getIReg(4, R_ESP), mkU32(sz)) );
    putIReg(4, R_ESP, mkexpr(ta));
    storeLE( mkexpr(ta), mkexpr(t1) );

    DIP("push%c %s\n", sz==2 ? 'w' : 'l', nameSReg(sreg));
}

static
void dis_pop_segreg ( UInt sreg, Int sz )
{
    IRTemp t1 = newTemp(Ity_I16);
    IRTemp ta = newTemp(Ity_I32);
    vassert(sz == 2 || sz == 4);

    assign( ta, getIReg(4, R_ESP) );
    assign( t1, loadLE(Ity_I16, mkexpr(ta)) );

    putIReg(4, R_ESP, binop(Iop_Add32, mkexpr(ta), mkU32(sz)) );
    putSReg( sreg, mkexpr(t1) );
    DIP("pop%c %s\n", sz==2 ? 'w' : 'l', nameSReg(sreg));
}

static
void dis_ret ( DisResult* dres, UInt d32 )
{
   IRTemp t1 = newTemp(Ity_I32);
   IRTemp t2 = newTemp(Ity_I32);
   assign(t1, getIReg(4,R_ESP));
   assign(t2, loadLE(Ity_I32,mkexpr(t1)));
   putIReg(4, R_ESP,binop(Iop_Add32, mkexpr(t1), mkU32(4+d32)));
   jmp_treg(dres, Ijk_Ret, t2);
   vassert(dres->whatNext == Dis_StopHere);
}


static Bool requiresRMode ( IROp op )
{
   switch (op) {
      
      case Iop_Add32Fx4: case Iop_Sub32Fx4:
      case Iop_Mul32Fx4: case Iop_Div32Fx4:
      case Iop_Add64Fx2: case Iop_Sub64Fx2:
      case Iop_Mul64Fx2: case Iop_Div64Fx2:
         return True;
      default:
         break;
   }
   return False;
}



static UInt dis_SSE_E_to_G_all_wrk ( 
               UChar sorb, Int delta, 
               const HChar* opname, IROp op,
               Bool   invertG
            )
{
   HChar   dis_buf[50];
   Int     alen;
   IRTemp  addr;
   UChar   rm = getIByte(delta);
   IRExpr* gpart
      = invertG ? unop(Iop_NotV128, getXMMReg(gregOfRM(rm)))
                : getXMMReg(gregOfRM(rm));
   if (epartIsReg(rm)) {
      putXMMReg(
         gregOfRM(rm),
         requiresRMode(op)
            ? triop(op, get_FAKE_roundingmode(), 
                        gpart,
                        getXMMReg(eregOfRM(rm)))
            : binop(op, gpart,
                        getXMMReg(eregOfRM(rm)))
      );
      DIP("%s %s,%s\n", opname,
                        nameXMMReg(eregOfRM(rm)),
                        nameXMMReg(gregOfRM(rm)) );
      return delta+1;
   } else {
      addr = disAMode ( &alen, sorb, delta, dis_buf );
      putXMMReg(
         gregOfRM(rm), 
         requiresRMode(op)
            ? triop(op, get_FAKE_roundingmode(), 
                        gpart,
                        loadLE(Ity_V128, mkexpr(addr)))
            : binop(op, gpart,
                        loadLE(Ity_V128, mkexpr(addr)))
      );
      DIP("%s %s,%s\n", opname,
                        dis_buf,
                        nameXMMReg(gregOfRM(rm)) );
      return delta+alen;
   }
}



static
UInt dis_SSE_E_to_G_all ( UChar sorb, Int delta, const HChar* opname, IROp op )
{
   return dis_SSE_E_to_G_all_wrk( sorb, delta, opname, op, False );
}


static
UInt dis_SSE_E_to_G_all_invG ( UChar sorb, Int delta, 
                               const HChar* opname, IROp op )
{
   return dis_SSE_E_to_G_all_wrk( sorb, delta, opname, op, True );
}



static UInt dis_SSE_E_to_G_lo32 ( UChar sorb, Int delta, 
                                  const HChar* opname, IROp op )
{
   HChar   dis_buf[50];
   Int     alen;
   IRTemp  addr;
   UChar   rm = getIByte(delta);
   IRExpr* gpart = getXMMReg(gregOfRM(rm));
   if (epartIsReg(rm)) {
      putXMMReg( gregOfRM(rm), 
                 binop(op, gpart,
                           getXMMReg(eregOfRM(rm))) );
      DIP("%s %s,%s\n", opname,
                        nameXMMReg(eregOfRM(rm)),
                        nameXMMReg(gregOfRM(rm)) );
      return delta+1;
   } else {
      IRTemp epart = newTemp(Ity_V128);
      addr = disAMode ( &alen, sorb, delta, dis_buf );
      assign( epart, unop( Iop_32UtoV128,
                           loadLE(Ity_I32, mkexpr(addr))) );
      putXMMReg( gregOfRM(rm), 
                 binop(op, gpart, mkexpr(epart)) );
      DIP("%s %s,%s\n", opname,
                        dis_buf,
                        nameXMMReg(gregOfRM(rm)) );
      return delta+alen;
   }
}



static UInt dis_SSE_E_to_G_lo64 ( UChar sorb, Int delta, 
                                  const HChar* opname, IROp op )
{
   HChar   dis_buf[50];
   Int     alen;
   IRTemp  addr;
   UChar   rm = getIByte(delta);
   IRExpr* gpart = getXMMReg(gregOfRM(rm));
   if (epartIsReg(rm)) {
      putXMMReg( gregOfRM(rm), 
                 binop(op, gpart,
                           getXMMReg(eregOfRM(rm))) );
      DIP("%s %s,%s\n", opname,
                        nameXMMReg(eregOfRM(rm)),
                        nameXMMReg(gregOfRM(rm)) );
      return delta+1;
   } else {
      IRTemp epart = newTemp(Ity_V128);
      addr = disAMode ( &alen, sorb, delta, dis_buf );
      assign( epart, unop( Iop_64UtoV128,
                           loadLE(Ity_I64, mkexpr(addr))) );
      putXMMReg( gregOfRM(rm), 
                 binop(op, gpart, mkexpr(epart)) );
      DIP("%s %s,%s\n", opname,
                        dis_buf,
                        nameXMMReg(gregOfRM(rm)) );
      return delta+alen;
   }
}



static UInt dis_SSE_E_to_G_unary_all ( 
               UChar sorb, Int delta, 
               const HChar* opname, IROp op
            )
{
   HChar   dis_buf[50];
   Int     alen;
   IRTemp  addr;
   UChar   rm = getIByte(delta);
   
   
   Bool needsIRRM = op == Iop_Sqrt32Fx4 || op == Iop_Sqrt64Fx2;
   if (epartIsReg(rm)) {
      IRExpr* src = getXMMReg(eregOfRM(rm));
      
      IRExpr* res = needsIRRM ? binop(op, get_FAKE_roundingmode(), src)
                              : unop(op, src);
      putXMMReg( gregOfRM(rm), res );
      DIP("%s %s,%s\n", opname,
                        nameXMMReg(eregOfRM(rm)),
                        nameXMMReg(gregOfRM(rm)) );
      return delta+1;
   } else {
      addr = disAMode ( &alen, sorb, delta, dis_buf );
      IRExpr* src = loadLE(Ity_V128, mkexpr(addr));
      
      IRExpr* res = needsIRRM ? binop(op, get_FAKE_roundingmode(), src)
                              : unop(op, src);
      putXMMReg( gregOfRM(rm), res );
      DIP("%s %s,%s\n", opname,
                        dis_buf,
                        nameXMMReg(gregOfRM(rm)) );
      return delta+alen;
   }
}



static UInt dis_SSE_E_to_G_unary_lo32 ( 
               UChar sorb, Int delta, 
               const HChar* opname, IROp op
            )
{
   HChar   dis_buf[50];
   Int     alen;
   IRTemp  addr;
   UChar   rm = getIByte(delta);
   IRTemp  oldG0 = newTemp(Ity_V128);
   IRTemp  oldG1 = newTemp(Ity_V128);

   assign( oldG0, getXMMReg(gregOfRM(rm)) );

   if (epartIsReg(rm)) {
      assign( oldG1, 
              binop( Iop_SetV128lo32,
                     mkexpr(oldG0),
                     getXMMRegLane32(eregOfRM(rm), 0)) );
      putXMMReg( gregOfRM(rm), unop(op, mkexpr(oldG1)) );
      DIP("%s %s,%s\n", opname,
                        nameXMMReg(eregOfRM(rm)),
                        nameXMMReg(gregOfRM(rm)) );
      return delta+1;
   } else {
      addr = disAMode ( &alen, sorb, delta, dis_buf );
      assign( oldG1, 
              binop( Iop_SetV128lo32,
                     mkexpr(oldG0),
                     loadLE(Ity_I32, mkexpr(addr)) ));
      putXMMReg( gregOfRM(rm), unop(op, mkexpr(oldG1)) );
      DIP("%s %s,%s\n", opname,
                        dis_buf,
                        nameXMMReg(gregOfRM(rm)) );
      return delta+alen;
   }
}



static UInt dis_SSE_E_to_G_unary_lo64 ( 
               UChar sorb, Int delta, 
               const HChar* opname, IROp op
            )
{
   HChar   dis_buf[50];
   Int     alen;
   IRTemp  addr;
   UChar   rm = getIByte(delta);
   IRTemp  oldG0 = newTemp(Ity_V128);
   IRTemp  oldG1 = newTemp(Ity_V128);

   assign( oldG0, getXMMReg(gregOfRM(rm)) );

   if (epartIsReg(rm)) {
      assign( oldG1, 
              binop( Iop_SetV128lo64,
                     mkexpr(oldG0),
                     getXMMRegLane64(eregOfRM(rm), 0)) );
      putXMMReg( gregOfRM(rm), unop(op, mkexpr(oldG1)) );
      DIP("%s %s,%s\n", opname,
                        nameXMMReg(eregOfRM(rm)),
                        nameXMMReg(gregOfRM(rm)) );
      return delta+1;
   } else {
      addr = disAMode ( &alen, sorb, delta, dis_buf );
      assign( oldG1, 
              binop( Iop_SetV128lo64,
                     mkexpr(oldG0),
                     loadLE(Ity_I64, mkexpr(addr)) ));
      putXMMReg( gregOfRM(rm), unop(op, mkexpr(oldG1)) );
      DIP("%s %s,%s\n", opname,
                        dis_buf,
                        nameXMMReg(gregOfRM(rm)) );
      return delta+alen;
   }
}


static UInt dis_SSEint_E_to_G( 
               UChar sorb, Int delta, 
               const HChar* opname, IROp op,
               Bool   eLeft
            )
{
   HChar   dis_buf[50];
   Int     alen;
   IRTemp  addr;
   UChar   rm = getIByte(delta);
   IRExpr* gpart = getXMMReg(gregOfRM(rm));
   IRExpr* epart = NULL;
   if (epartIsReg(rm)) {
      epart = getXMMReg(eregOfRM(rm));
      DIP("%s %s,%s\n", opname,
                        nameXMMReg(eregOfRM(rm)),
                        nameXMMReg(gregOfRM(rm)) );
      delta += 1;
   } else {
      addr  = disAMode ( &alen, sorb, delta, dis_buf );
      epart = loadLE(Ity_V128, mkexpr(addr));
      DIP("%s %s,%s\n", opname,
                        dis_buf,
                        nameXMMReg(gregOfRM(rm)) );
      delta += alen;
   }
   putXMMReg( gregOfRM(rm), 
              eLeft ? binop(op, epart, gpart)
	            : binop(op, gpart, epart) );
   return delta;
}



static void findSSECmpOp ( Bool* needNot, IROp* op, 
                           Int imm8, Bool all_lanes, Int sz )
{
   imm8 &= 7;
   *needNot = False;
   *op      = Iop_INVALID;
   if (imm8 >= 4) {
      *needNot = True;
      imm8 -= 4;
   }

   if (sz == 4 && all_lanes) {
      switch (imm8) {
         case 0: *op = Iop_CmpEQ32Fx4; return;
         case 1: *op = Iop_CmpLT32Fx4; return;
         case 2: *op = Iop_CmpLE32Fx4; return;
         case 3: *op = Iop_CmpUN32Fx4; return;
         default: break;
      }
   }
   if (sz == 4 && !all_lanes) {
      switch (imm8) {
         case 0: *op = Iop_CmpEQ32F0x4; return;
         case 1: *op = Iop_CmpLT32F0x4; return;
         case 2: *op = Iop_CmpLE32F0x4; return;
         case 3: *op = Iop_CmpUN32F0x4; return;
         default: break;
      }
   }
   if (sz == 8 && all_lanes) {
      switch (imm8) {
         case 0: *op = Iop_CmpEQ64Fx2; return;
         case 1: *op = Iop_CmpLT64Fx2; return;
         case 2: *op = Iop_CmpLE64Fx2; return;
         case 3: *op = Iop_CmpUN64Fx2; return;
         default: break;
      }
   }
   if (sz == 8 && !all_lanes) {
      switch (imm8) {
         case 0: *op = Iop_CmpEQ64F0x2; return;
         case 1: *op = Iop_CmpLT64F0x2; return;
         case 2: *op = Iop_CmpLE64F0x2; return;
         case 3: *op = Iop_CmpUN64F0x2; return;
         default: break;
      }
   }
   vpanic("findSSECmpOp(x86,guest)");
}


static UInt dis_SSEcmp_E_to_G ( UChar sorb, Int delta, 
				const HChar* opname, Bool all_lanes, Int sz )
{
   HChar   dis_buf[50];
   Int     alen, imm8;
   IRTemp  addr;
   Bool    needNot = False;
   IROp    op      = Iop_INVALID;
   IRTemp  plain   = newTemp(Ity_V128);
   UChar   rm      = getIByte(delta);
   UShort  mask    = 0;
   vassert(sz == 4 || sz == 8);
   if (epartIsReg(rm)) {
      imm8 = getIByte(delta+1);
      findSSECmpOp(&needNot, &op, imm8, all_lanes, sz);
      assign( plain, binop(op, getXMMReg(gregOfRM(rm)), 
                               getXMMReg(eregOfRM(rm))) );
      delta += 2;
      DIP("%s $%d,%s,%s\n", opname,
                            (Int)imm8,
                            nameXMMReg(eregOfRM(rm)),
                            nameXMMReg(gregOfRM(rm)) );
   } else {
      addr = disAMode ( &alen, sorb, delta, dis_buf );
      imm8 = getIByte(delta+alen);
      findSSECmpOp(&needNot, &op, imm8, all_lanes, sz);
      assign( plain, 
              binop(
                 op,
                 getXMMReg(gregOfRM(rm)), 
                   all_lanes  ? loadLE(Ity_V128, mkexpr(addr))
                 : sz == 8    ? unop( Iop_64UtoV128, loadLE(Ity_I64, mkexpr(addr)))
                 :     unop( Iop_32UtoV128, loadLE(Ity_I32, mkexpr(addr)))
             ) 
      );
      delta += alen+1;
      DIP("%s $%d,%s,%s\n", opname,
                            (Int)imm8,
                            dis_buf,
                            nameXMMReg(gregOfRM(rm)) );
   }

   if (needNot && all_lanes) {
      putXMMReg( gregOfRM(rm), 
                 unop(Iop_NotV128, mkexpr(plain)) );
   }
   else
   if (needNot && !all_lanes) {
      mask = toUShort( sz==4 ? 0x000F : 0x00FF );
      putXMMReg( gregOfRM(rm), 
                 binop(Iop_XorV128, mkexpr(plain), mkV128(mask)) );
   }
   else {
      putXMMReg( gregOfRM(rm), mkexpr(plain) );
   }

   return delta;
}



static UInt dis_SSE_shiftG_byE ( UChar sorb, Int delta, 
                                 const HChar* opname, IROp op )
{
   HChar   dis_buf[50];
   Int     alen, size;
   IRTemp  addr;
   Bool    shl, shr, sar;
   UChar   rm   = getIByte(delta);
   IRTemp  g0   = newTemp(Ity_V128);
   IRTemp  g1   = newTemp(Ity_V128);
   IRTemp  amt  = newTemp(Ity_I32);
   IRTemp  amt8 = newTemp(Ity_I8);
   if (epartIsReg(rm)) {
      assign( amt, getXMMRegLane32(eregOfRM(rm), 0) );
      DIP("%s %s,%s\n", opname,
                        nameXMMReg(eregOfRM(rm)),
                        nameXMMReg(gregOfRM(rm)) );
      delta++;
   } else {
      addr = disAMode ( &alen, sorb, delta, dis_buf );
      assign( amt, loadLE(Ity_I32, mkexpr(addr)) );
      DIP("%s %s,%s\n", opname,
                        dis_buf,
                        nameXMMReg(gregOfRM(rm)) );
      delta += alen;
   }
   assign( g0,   getXMMReg(gregOfRM(rm)) );
   assign( amt8, unop(Iop_32to8, mkexpr(amt)) );

   shl = shr = sar = False;
   size = 0;
   switch (op) {
      case Iop_ShlN16x8: shl = True; size = 32; break;
      case Iop_ShlN32x4: shl = True; size = 32; break;
      case Iop_ShlN64x2: shl = True; size = 64; break;
      case Iop_SarN16x8: sar = True; size = 16; break;
      case Iop_SarN32x4: sar = True; size = 32; break;
      case Iop_ShrN16x8: shr = True; size = 16; break;
      case Iop_ShrN32x4: shr = True; size = 32; break;
      case Iop_ShrN64x2: shr = True; size = 64; break;
      default: vassert(0);
   }

   if (shl || shr) {
     assign( 
        g1,
        IRExpr_ITE(
           binop(Iop_CmpLT32U,mkexpr(amt),mkU32(size)),
           binop(op, mkexpr(g0), mkexpr(amt8)),
           mkV128(0x0000)
        )
     );
   } else 
   if (sar) {
     assign( 
        g1,
        IRExpr_ITE(
           binop(Iop_CmpLT32U,mkexpr(amt),mkU32(size)),
           binop(op, mkexpr(g0), mkexpr(amt8)),
           binop(op, mkexpr(g0), mkU8(size-1))
        )
     );
   } else {
      
      vassert(0);
   }

   putXMMReg( gregOfRM(rm), mkexpr(g1) );
   return delta;
}



static 
UInt dis_SSE_shiftE_imm ( Int delta, const HChar* opname, IROp op )
{
   Bool    shl, shr, sar;
   UChar   rm   = getIByte(delta);
   IRTemp  e0   = newTemp(Ity_V128);
   IRTemp  e1   = newTemp(Ity_V128);
   UChar   amt, size;
   vassert(epartIsReg(rm));
   vassert(gregOfRM(rm) == 2 
           || gregOfRM(rm) == 4 || gregOfRM(rm) == 6);
   amt = getIByte(delta+1);
   delta += 2;
   DIP("%s $%d,%s\n", opname,
                      (Int)amt,
                      nameXMMReg(eregOfRM(rm)) );
   assign( e0, getXMMReg(eregOfRM(rm)) );

   shl = shr = sar = False;
   size = 0;
   switch (op) {
      case Iop_ShlN16x8: shl = True; size = 16; break;
      case Iop_ShlN32x4: shl = True; size = 32; break;
      case Iop_ShlN64x2: shl = True; size = 64; break;
      case Iop_SarN16x8: sar = True; size = 16; break;
      case Iop_SarN32x4: sar = True; size = 32; break;
      case Iop_ShrN16x8: shr = True; size = 16; break;
      case Iop_ShrN32x4: shr = True; size = 32; break;
      case Iop_ShrN64x2: shr = True; size = 64; break;
      default: vassert(0);
   }

   if (shl || shr) {
      assign( e1, amt >= size 
                     ? mkV128(0x0000)
                     : binop(op, mkexpr(e0), mkU8(amt))
      );
   } else 
   if (sar) {
      assign( e1, amt >= size 
                     ? binop(op, mkexpr(e0), mkU8(size-1))
                     : binop(op, mkexpr(e0), mkU8(amt))
      );
   } else {
      
      vassert(0);
   }

   putXMMReg( eregOfRM(rm), mkexpr(e1) );
   return delta;
}



static IRExpr*  get_sse_roundingmode ( void )
{
   return binop( Iop_And32, 
                 IRExpr_Get( OFFB_SSEROUND, Ity_I32 ), 
                 mkU32(3) );
}

static void put_sse_roundingmode ( IRExpr* sseround )
{
   vassert(typeOfIRExpr(irsb->tyenv, sseround) == Ity_I32);
   stmt( IRStmt_Put( OFFB_SSEROUND, sseround ) );
}


static void breakup128to32s ( IRTemp t128,
			      
                              IRTemp* t3, IRTemp* t2,
                              IRTemp* t1, IRTemp* t0 )
{
   IRTemp hi64 = newTemp(Ity_I64);
   IRTemp lo64 = newTemp(Ity_I64);
   assign( hi64, unop(Iop_V128HIto64, mkexpr(t128)) );
   assign( lo64, unop(Iop_V128to64,   mkexpr(t128)) );

   vassert(t0 && *t0 == IRTemp_INVALID);
   vassert(t1 && *t1 == IRTemp_INVALID);
   vassert(t2 && *t2 == IRTemp_INVALID);
   vassert(t3 && *t3 == IRTemp_INVALID);

   *t0 = newTemp(Ity_I32);
   *t1 = newTemp(Ity_I32);
   *t2 = newTemp(Ity_I32);
   *t3 = newTemp(Ity_I32);
   assign( *t0, unop(Iop_64to32,   mkexpr(lo64)) );
   assign( *t1, unop(Iop_64HIto32, mkexpr(lo64)) );
   assign( *t2, unop(Iop_64to32,   mkexpr(hi64)) );
   assign( *t3, unop(Iop_64HIto32, mkexpr(hi64)) );
}


static IRExpr* mk128from32s ( IRTemp t3, IRTemp t2,
                              IRTemp t1, IRTemp t0 )
{
   return
      binop( Iop_64HLtoV128,
             binop(Iop_32HLto64, mkexpr(t3), mkexpr(t2)),
             binop(Iop_32HLto64, mkexpr(t1), mkexpr(t0))
   );
}


static void breakup64to16s ( IRTemp t64,
                             
                             IRTemp* t3, IRTemp* t2,
                             IRTemp* t1, IRTemp* t0 )
{
   IRTemp hi32 = newTemp(Ity_I32);
   IRTemp lo32 = newTemp(Ity_I32);
   assign( hi32, unop(Iop_64HIto32, mkexpr(t64)) );
   assign( lo32, unop(Iop_64to32,   mkexpr(t64)) );

   vassert(t0 && *t0 == IRTemp_INVALID);
   vassert(t1 && *t1 == IRTemp_INVALID);
   vassert(t2 && *t2 == IRTemp_INVALID);
   vassert(t3 && *t3 == IRTemp_INVALID);

   *t0 = newTemp(Ity_I16);
   *t1 = newTemp(Ity_I16);
   *t2 = newTemp(Ity_I16);
   *t3 = newTemp(Ity_I16);
   assign( *t0, unop(Iop_32to16,   mkexpr(lo32)) );
   assign( *t1, unop(Iop_32HIto16, mkexpr(lo32)) );
   assign( *t2, unop(Iop_32to16,   mkexpr(hi32)) );
   assign( *t3, unop(Iop_32HIto16, mkexpr(hi32)) );
}


static IRExpr* mk64from16s ( IRTemp t3, IRTemp t2,
                             IRTemp t1, IRTemp t0 )
{
   return
      binop( Iop_32HLto64,
             binop(Iop_16HLto32, mkexpr(t3), mkexpr(t2)),
             binop(Iop_16HLto32, mkexpr(t1), mkexpr(t0))
   );
}


static 
void set_EFLAGS_from_value ( IRTemp t1, 
                             Bool   emit_AC_emwarn,
                             Addr32 next_insn_EIP )
{
   vassert(typeOfIRTemp(irsb->tyenv,t1) == Ity_I32);

   stmt( IRStmt_Put( OFFB_CC_OP,   mkU32(X86G_CC_OP_COPY) ));
   stmt( IRStmt_Put( OFFB_CC_DEP2, mkU32(0) ));
   stmt( IRStmt_Put( OFFB_CC_DEP1, 
                     binop(Iop_And32,
                           mkexpr(t1), 
                           mkU32( X86G_CC_MASK_C | X86G_CC_MASK_P 
                                  | X86G_CC_MASK_A | X86G_CC_MASK_Z 
                                  | X86G_CC_MASK_S| X86G_CC_MASK_O )
                          )
                    )
       );
   stmt( IRStmt_Put( OFFB_CC_NDEP, mkU32(0) ));

   stmt( IRStmt_Put( 
            OFFB_DFLAG,
            IRExpr_ITE( 
               unop(Iop_32to1,
                    binop(Iop_And32, 
                          binop(Iop_Shr32, mkexpr(t1), mkU8(10)), 
                          mkU32(1))),
               mkU32(0xFFFFFFFF),
               mkU32(1)))
       );

   
   stmt( IRStmt_Put( 
            OFFB_IDFLAG,
            IRExpr_ITE( 
               unop(Iop_32to1,
                    binop(Iop_And32, 
                          binop(Iop_Shr32, mkexpr(t1), mkU8(21)), 
                          mkU32(1))),
               mkU32(1),
               mkU32(0)))
       );

   stmt( IRStmt_Put( 
            OFFB_ACFLAG,
            IRExpr_ITE( 
               unop(Iop_32to1,
                    binop(Iop_And32, 
                          binop(Iop_Shr32, mkexpr(t1), mkU8(18)), 
                          mkU32(1))),
               mkU32(1),
               mkU32(0)))
       );

   if (emit_AC_emwarn) {
      put_emwarn( mkU32(EmWarn_X86_acFlag) );
      stmt( 
         IRStmt_Exit(
            binop( Iop_CmpNE32, 
                   binop(Iop_And32, mkexpr(t1), mkU32(1<<18)), 
                   mkU32(0) ),
            Ijk_EmWarn,
            IRConst_U32( next_insn_EIP ),
            OFFB_EIP
         )
      );
   }
}


static IRExpr* dis_PMULHRSW_helper ( IRExpr* aax, IRExpr* bbx )
{
   IRTemp aa      = newTemp(Ity_I64);
   IRTemp bb      = newTemp(Ity_I64);
   IRTemp aahi32s = newTemp(Ity_I64);
   IRTemp aalo32s = newTemp(Ity_I64);
   IRTemp bbhi32s = newTemp(Ity_I64);
   IRTemp bblo32s = newTemp(Ity_I64);
   IRTemp rHi     = newTemp(Ity_I64);
   IRTemp rLo     = newTemp(Ity_I64);
   IRTemp one32x2 = newTemp(Ity_I64);
   assign(aa, aax);
   assign(bb, bbx);
   assign( aahi32s,
           binop(Iop_SarN32x2,
                 binop(Iop_InterleaveHI16x4, mkexpr(aa), mkexpr(aa)),
                 mkU8(16) ));
   assign( aalo32s,
           binop(Iop_SarN32x2,
                 binop(Iop_InterleaveLO16x4, mkexpr(aa), mkexpr(aa)),
                 mkU8(16) ));
   assign( bbhi32s,
           binop(Iop_SarN32x2,
                 binop(Iop_InterleaveHI16x4, mkexpr(bb), mkexpr(bb)),
                 mkU8(16) ));
   assign( bblo32s,
           binop(Iop_SarN32x2,
                 binop(Iop_InterleaveLO16x4, mkexpr(bb), mkexpr(bb)),
                 mkU8(16) ));
   assign(one32x2, mkU64( (1ULL << 32) + 1 ));
   assign(
      rHi,
      binop(
         Iop_ShrN32x2,
         binop(
            Iop_Add32x2, 
            binop(
               Iop_ShrN32x2,
               binop(Iop_Mul32x2, mkexpr(aahi32s), mkexpr(bbhi32s)),
               mkU8(14)
            ),
            mkexpr(one32x2)
         ),
         mkU8(1)
      )
   );
   assign(
      rLo,
      binop(
         Iop_ShrN32x2,
         binop(
            Iop_Add32x2, 
            binop(
               Iop_ShrN32x2,
               binop(Iop_Mul32x2, mkexpr(aalo32s), mkexpr(bblo32s)),
               mkU8(14)
            ),
            mkexpr(one32x2)
         ),
         mkU8(1)
      )
   );
   return
      binop(Iop_CatEvenLanes16x4, mkexpr(rHi), mkexpr(rLo));
}

static IRExpr* dis_PSIGN_helper ( IRExpr* aax, IRExpr* bbx, Int laneszB )
{
   IRTemp aa       = newTemp(Ity_I64);
   IRTemp bb       = newTemp(Ity_I64);
   IRTemp zero     = newTemp(Ity_I64);
   IRTemp bbNeg    = newTemp(Ity_I64);
   IRTemp negMask  = newTemp(Ity_I64);
   IRTemp posMask  = newTemp(Ity_I64);
   IROp   opSub    = Iop_INVALID;
   IROp   opCmpGTS = Iop_INVALID;

   switch (laneszB) {
      case 1: opSub = Iop_Sub8x8;  opCmpGTS = Iop_CmpGT8Sx8;  break;
      case 2: opSub = Iop_Sub16x4; opCmpGTS = Iop_CmpGT16Sx4; break;
      case 4: opSub = Iop_Sub32x2; opCmpGTS = Iop_CmpGT32Sx2; break;
      default: vassert(0);
   }

   assign( aa,      aax );
   assign( bb,      bbx );
   assign( zero,    mkU64(0) );
   assign( bbNeg,   binop(opSub,    mkexpr(zero), mkexpr(bb)) );
   assign( negMask, binop(opCmpGTS, mkexpr(zero), mkexpr(aa)) );
   assign( posMask, binop(opCmpGTS, mkexpr(aa),   mkexpr(zero)) );

   return
      binop(Iop_Or64,
            binop(Iop_And64, mkexpr(bb),    mkexpr(posMask)),
            binop(Iop_And64, mkexpr(bbNeg), mkexpr(negMask)) );

}

static IRExpr* dis_PABS_helper ( IRExpr* aax, Int laneszB )
{
   IRTemp aa      = newTemp(Ity_I64);
   IRTemp zero    = newTemp(Ity_I64);
   IRTemp aaNeg   = newTemp(Ity_I64);
   IRTemp negMask = newTemp(Ity_I64);
   IRTemp posMask = newTemp(Ity_I64);
   IROp   opSub   = Iop_INVALID;
   IROp   opSarN  = Iop_INVALID;

   switch (laneszB) {
      case 1: opSub = Iop_Sub8x8;  opSarN = Iop_SarN8x8;  break;
      case 2: opSub = Iop_Sub16x4; opSarN = Iop_SarN16x4; break;
      case 4: opSub = Iop_Sub32x2; opSarN = Iop_SarN32x2; break;
      default: vassert(0);
   }

   assign( aa,      aax );
   assign( negMask, binop(opSarN, mkexpr(aa), mkU8(8*laneszB-1)) );
   assign( posMask, unop(Iop_Not64, mkexpr(negMask)) );
   assign( zero,    mkU64(0) );
   assign( aaNeg,   binop(opSub, mkexpr(zero), mkexpr(aa)) );
   return
      binop(Iop_Or64,
            binop(Iop_And64, mkexpr(aa),    mkexpr(posMask)),
            binop(Iop_And64, mkexpr(aaNeg), mkexpr(negMask)) );
}

static IRExpr* dis_PALIGNR_XMM_helper ( IRTemp hi64,
                                        IRTemp lo64, Int byteShift )
{
   vassert(byteShift >= 1 && byteShift <= 7);
   return
      binop(Iop_Or64,
            binop(Iop_Shl64, mkexpr(hi64), mkU8(8*(8-byteShift))),
            binop(Iop_Shr64, mkexpr(lo64), mkU8(8*byteShift))
      );
}

static void gen_SEGV_if_not_16_aligned ( IRTemp effective_addr )
{
   stmt(
      IRStmt_Exit(
         binop(Iop_CmpNE32,
               binop(Iop_And32,mkexpr(effective_addr),mkU32(0xF)),
               mkU32(0)),
         Ijk_SigSEGV,
         IRConst_U32(guest_EIP_curr_instr),
         OFFB_EIP
      )
   );
}


static Bool can_be_used_with_LOCK_prefix ( const UChar* opc )
{
   switch (opc[0]) {
      case 0x00: case 0x01: case 0x08: case 0x09:
      case 0x10: case 0x11: case 0x18: case 0x19:
      case 0x20: case 0x21: case 0x28: case 0x29:
      case 0x30: case 0x31:
         if (!epartIsReg(opc[1]))
            return True;
         break;

      case 0x80: case 0x81: case 0x82: case 0x83:
         if (gregOfRM(opc[1]) >= 0 && gregOfRM(opc[1]) <= 6
             && !epartIsReg(opc[1]))
            return True;
         break;

      case 0xFE: case 0xFF:
         if (gregOfRM(opc[1]) >= 0 && gregOfRM(opc[1]) <= 1
             && !epartIsReg(opc[1]))
            return True;
         break;

      case 0xF6: case 0xF7:
         if (gregOfRM(opc[1]) >= 2 && gregOfRM(opc[1]) <= 3
             && !epartIsReg(opc[1]))
            return True;
         break;

      case 0x86: case 0x87:
         if (!epartIsReg(opc[1]))
            return True;
         break;

      case 0x0F: {
         switch (opc[1]) {
            case 0xBB: case 0xB3: case 0xAB:
               if (!epartIsReg(opc[2]))
                  return True;
               break;
            case 0xBA: 
               if (gregOfRM(opc[2]) >= 5 && gregOfRM(opc[2]) <= 7
                   && !epartIsReg(opc[2]))
                  return True;
               break;
            case 0xB0: case 0xB1:
               if (!epartIsReg(opc[2]))
                  return True;
               break;
            case 0xC7: 
               if (gregOfRM(opc[2]) == 1 && !epartIsReg(opc[2]) )
                  return True;
               break;
            case 0xC0: case 0xC1:
               if (!epartIsReg(opc[2]))
                  return True;
               break;
            default:
               break;
         } 
         break;
      }

      default:
         break;
   } 

   return False;
}

static IRTemp math_BSWAP ( IRTemp t1, IRType ty )
{
   IRTemp t2 = newTemp(ty);
   if (ty == Ity_I32) {
      assign( t2,
         binop(
            Iop_Or32,
            binop(Iop_Shl32, mkexpr(t1), mkU8(24)),
            binop(
               Iop_Or32,
               binop(Iop_And32, binop(Iop_Shl32, mkexpr(t1), mkU8(8)),
                                mkU32(0x00FF0000)),
               binop(Iop_Or32,
                     binop(Iop_And32, binop(Iop_Shr32, mkexpr(t1), mkU8(8)),
                                      mkU32(0x0000FF00)),
                     binop(Iop_And32, binop(Iop_Shr32, mkexpr(t1), mkU8(24)),
                                      mkU32(0x000000FF) )
            )))
      );
      return t2;
   }
   if (ty == Ity_I16) {
      assign(t2, 
             binop(Iop_Or16,
                   binop(Iop_Shl16, mkexpr(t1), mkU8(8)),
                   binop(Iop_Shr16, mkexpr(t1), mkU8(8)) ));
      return t2;
   }
   vassert(0);
   
   return IRTemp_INVALID;
}


static
DisResult disInstr_X86_WRK (
             Bool* expect_CAS,
             Bool         (*resteerOkFn) ( void*, Addr ),
             Bool         resteerCisOk,
             void*        callback_opaque,
             Long         delta64,
             const VexArchInfo* archinfo,
             const VexAbiInfo*  vbi,
             Bool         sigill_diag
          )
{
   IRType    ty;
   IRTemp    addr, t0, t1, t2, t3, t4, t5, t6;
   Int       alen;
   UChar     opc, modrm, abyte, pre;
   UInt      d32;
   HChar     dis_buf[50];
   Int       am_sz, d_sz, n_prefixes;
   DisResult dres;
   const UChar* insn; 

   
   Int delta = (Int)delta64;

   Int delta_start = delta;

   Int sz = 4;

   UChar sorb = 0;

   
   Bool pfx_lock = False;

   
   dres.whatNext    = Dis_Continue;
   dres.len         = 0;
   dres.continueAt  = 0;
   dres.jk_StopHere = Ijk_INVALID;

   *expect_CAS = False;

   addr = t0 = t1 = t2 = t3 = t4 = t5 = t6 = IRTemp_INVALID; 

   vassert(guest_EIP_bbstart + delta == guest_EIP_curr_instr);
   DIP("\t0x%x:  ", guest_EIP_bbstart+delta);

   
   {
      const UChar* code = guest_code + delta;
      if (code[ 0] == 0xC1 && code[ 1] == 0xC7 && code[ 2] == 0x03 &&
          code[ 3] == 0xC1 && code[ 4] == 0xC7 && code[ 5] == 0x0D &&
          code[ 6] == 0xC1 && code[ 7] == 0xC7 && code[ 8] == 0x1D &&
          code[ 9] == 0xC1 && code[10] == 0xC7 && code[11] == 0x13) {
         
         if (code[12] == 0x87 && code[13] == 0xDB ) {
            
            DIP("%%edx = client_request ( %%eax )\n");
            delta += 14;
            jmp_lit(&dres, Ijk_ClientReq, guest_EIP_bbstart+delta);
            vassert(dres.whatNext == Dis_StopHere);
            goto decode_success;
         }
         else
         if (code[12] == 0x87 && code[13] == 0xC9 ) {
            
            DIP("%%eax = guest_NRADDR\n");
            delta += 14;
            putIReg(4, R_EAX, IRExpr_Get( OFFB_NRADDR, Ity_I32 ));
            goto decode_success;
         }
         else
         if (code[12] == 0x87 && code[13] == 0xD2 ) {
            
            DIP("call-noredir *%%eax\n");
            delta += 14;
            t1 = newTemp(Ity_I32);
            assign(t1, getIReg(4,R_EAX));
            t2 = newTemp(Ity_I32);
            assign(t2, binop(Iop_Sub32, getIReg(4,R_ESP), mkU32(4)));
            putIReg(4, R_ESP, mkexpr(t2));
            storeLE( mkexpr(t2), mkU32(guest_EIP_bbstart+delta));
            jmp_treg(&dres, Ijk_NoRedir, t1);
            vassert(dres.whatNext == Dis_StopHere);
            goto decode_success;
         }
         else
         if (code[12] == 0x87 && code[13] == 0xFF ) {
            
            DIP("IR injection\n");
            vex_inject_ir(irsb, Iend_LE);

            
            
            
            
            stmt(IRStmt_Put(OFFB_CMSTART, mkU32(guest_EIP_curr_instr)));
            stmt(IRStmt_Put(OFFB_CMLEN,   mkU32(14)));
   
            delta += 14;

            stmt( IRStmt_Put( OFFB_EIP, mkU32(guest_EIP_bbstart + delta) ) );
            dres.whatNext    = Dis_StopHere;
            dres.jk_StopHere = Ijk_InvalICache;
            goto decode_success;
         }
         
         goto decode_failure;
         
      }
   }

   {
      const UChar* code = guest_code + delta;
      if (code[0] == 0x26 && code[1] == 0x2E && code[2] == 0x64 
          && code[3] == 0x65 && code[4] == 0x90) {
         DIP("%%es:%%cs:%%fs:%%gs:nop\n");
         delta += 5;
         goto decode_success;
      }
      if (code[0] == 0x66) {
         Int data16_cnt;
         for (data16_cnt = 1; data16_cnt < 6; data16_cnt++)
            if (code[data16_cnt] != 0x66)
               break;
         if (code[data16_cnt] == 0x2E && code[data16_cnt + 1] == 0x0F
             && code[data16_cnt + 2] == 0x1F && code[data16_cnt + 3] == 0x84
             && code[data16_cnt + 4] == 0x00 && code[data16_cnt + 5] == 0x00
             && code[data16_cnt + 6] == 0x00 && code[data16_cnt + 7] == 0x00
             && code[data16_cnt + 8] == 0x00 ) {
            DIP("nopw %%cs:0x0(%%eax,%%eax,1)\n");
            delta += 9 + data16_cnt;
            goto decode_success;
         }
      }
   }       

   

   n_prefixes = 0;
   while (True) {
      if (n_prefixes > 7) goto decode_failure;
      pre = getUChar(delta);
      switch (pre) {
         case 0x66: 
            sz = 2;
            break;
         case 0xF0: 
            pfx_lock = True; 
            *expect_CAS = True;
            break;
         case 0x3E: 
         case 0x26: 
         case 0x64: 
         case 0x65: 
            if (sorb != 0) 
               goto decode_failure; 
            sorb = pre;
            break;
         case 0x2E: { 
            UChar op1 = getIByte(delta+1);
            UChar op2 = getIByte(delta+2);
            if ((op1 >= 0x70 && op1 <= 0x7F)
                || (op1 == 0xE3)
                || (op1 == 0x0F && op2 >= 0x80 && op2 <= 0x8F)) {
               if (0) vex_printf("vex x86->IR: ignoring branch hint\n");
            } else {
               
               goto decode_failure;
            }
            break;
         }
         case 0x36: 
            
            goto decode_failure;
         default: 
            goto not_a_prefix;
      }
      n_prefixes++;
      delta++;
   }

   not_a_prefix:


   if (pfx_lock) {
     if (can_be_used_with_LOCK_prefix( &guest_code[delta] )) {
         DIP("lock ");
      } else {
         *expect_CAS = False;
         goto decode_failure;
      }
   }


   
   
   



   insn = &guest_code[delta];


   
   if (sz == 4 && insn[0] == 0x0F && insn[1] == 0xAE
       && !epartIsReg(insn[2]) && gregOfRM(insn[2]) == 0) {
      IRDirty* d;
      modrm = getIByte(delta+2);
      vassert(sz == 4);
      vassert(!epartIsReg(modrm));

      addr = disAMode ( &alen, sorb, delta+2, dis_buf );
      delta += 2+alen;
      gen_SEGV_if_not_16_aligned(addr);

      DIP("fxsave %s\n", dis_buf);

      d = unsafeIRDirty_0_N ( 
             0, 
             "x86g_dirtyhelper_FXSAVE", 
             &x86g_dirtyhelper_FXSAVE,
             mkIRExprVec_2( IRExpr_BBPTR(), mkexpr(addr) )
          );

      
      d->mFx   = Ifx_Write;
      d->mAddr = mkexpr(addr);
      d->mSize = 464; 

      
      d->nFxState = 7;
      vex_bzero(&d->fxState, sizeof(d->fxState));

      d->fxState[0].fx     = Ifx_Read;
      d->fxState[0].offset = OFFB_FTOP;
      d->fxState[0].size   = sizeof(UInt);

      d->fxState[1].fx     = Ifx_Read;
      d->fxState[1].offset = OFFB_FPREGS;
      d->fxState[1].size   = 8 * sizeof(ULong);

      d->fxState[2].fx     = Ifx_Read;
      d->fxState[2].offset = OFFB_FPTAGS;
      d->fxState[2].size   = 8 * sizeof(UChar);

      d->fxState[3].fx     = Ifx_Read;
      d->fxState[3].offset = OFFB_FPROUND;
      d->fxState[3].size   = sizeof(UInt);

      d->fxState[4].fx     = Ifx_Read;
      d->fxState[4].offset = OFFB_FC3210;
      d->fxState[4].size   = sizeof(UInt);

      d->fxState[5].fx     = Ifx_Read;
      d->fxState[5].offset = OFFB_XMM0;
      d->fxState[5].size   = 8 * sizeof(U128);

      d->fxState[6].fx     = Ifx_Read;
      d->fxState[6].offset = OFFB_SSEROUND;
      d->fxState[6].size   = sizeof(UInt);

      vassert(16 == sizeof(U128));
      vassert(OFFB_XMM7 == (OFFB_XMM0 + 7 * 16));

      stmt( IRStmt_Dirty(d) );

      goto decode_success;
   }

   
   if (sz == 4 && insn[0] == 0x0F && insn[1] == 0xAE
       && !epartIsReg(insn[2]) && gregOfRM(insn[2]) == 1) {
      IRDirty* d;
      modrm = getIByte(delta+2);
      vassert(sz == 4);
      vassert(!epartIsReg(modrm));

      addr = disAMode ( &alen, sorb, delta+2, dis_buf );
      delta += 2+alen;
      gen_SEGV_if_not_16_aligned(addr);

      DIP("fxrstor %s\n", dis_buf);

      d = unsafeIRDirty_0_N ( 
             0, 
             "x86g_dirtyhelper_FXRSTOR", 
             &x86g_dirtyhelper_FXRSTOR,
             mkIRExprVec_2( IRExpr_BBPTR(), mkexpr(addr) )
          );

      
      d->mFx   = Ifx_Read;
      d->mAddr = mkexpr(addr);
      d->mSize = 464; 

      
      d->nFxState = 7;
      vex_bzero(&d->fxState, sizeof(d->fxState));

      d->fxState[0].fx     = Ifx_Write;
      d->fxState[0].offset = OFFB_FTOP;
      d->fxState[0].size   = sizeof(UInt);

      d->fxState[1].fx     = Ifx_Write;
      d->fxState[1].offset = OFFB_FPREGS;
      d->fxState[1].size   = 8 * sizeof(ULong);

      d->fxState[2].fx     = Ifx_Write;
      d->fxState[2].offset = OFFB_FPTAGS;
      d->fxState[2].size   = 8 * sizeof(UChar);

      d->fxState[3].fx     = Ifx_Write;
      d->fxState[3].offset = OFFB_FPROUND;
      d->fxState[3].size   = sizeof(UInt);

      d->fxState[4].fx     = Ifx_Write;
      d->fxState[4].offset = OFFB_FC3210;
      d->fxState[4].size   = sizeof(UInt);

      d->fxState[5].fx     = Ifx_Write;
      d->fxState[5].offset = OFFB_XMM0;
      d->fxState[5].size   = 8 * sizeof(U128);

      d->fxState[6].fx     = Ifx_Write;
      d->fxState[6].offset = OFFB_SSEROUND;
      d->fxState[6].size   = sizeof(UInt);

      vassert(16 == sizeof(U128));
      vassert(OFFB_XMM7 == (OFFB_XMM0 + 7 * 16));

      stmt( IRStmt_Dirty(d) );

      goto decode_success;
   }

   

   if (archinfo->hwcaps == 0)
      goto after_sse_decoders;


   if (archinfo->hwcaps == VEX_HWCAPS_X86_MMXEXT)
      goto mmxext;


   
   if (sz == 4 && insn[0] == 0x0F && insn[1] == 0x58) {
      delta = dis_SSE_E_to_G_all( sorb, delta+2, "addps", Iop_Add32Fx4 );
      goto decode_success;
   }

   
   if (insn[0] == 0xF3 && insn[1] == 0x0F && insn[2] == 0x58) {
      vassert(sz == 4);
      delta = dis_SSE_E_to_G_lo32( sorb, delta+3, "addss", Iop_Add32F0x4 );
      goto decode_success;
   }

   
   if (sz == 4 && insn[0] == 0x0F && insn[1] == 0x55) {
      delta = dis_SSE_E_to_G_all_invG( sorb, delta+2, "andnps", Iop_AndV128 );
      goto decode_success;
   }

   
   if (sz == 4 && insn[0] == 0x0F && insn[1] == 0x54) {
      delta = dis_SSE_E_to_G_all( sorb, delta+2, "andps", Iop_AndV128 );
      goto decode_success;
   }

   
   if (sz == 4 && insn[0] == 0x0F && insn[1] == 0xC2) {
      delta = dis_SSEcmp_E_to_G( sorb, delta+2, "cmpps", True, 4 );
      goto decode_success;
   }

   
   if (insn[0] == 0xF3 && insn[1] == 0x0F && insn[2] == 0xC2) {
      vassert(sz == 4);
      delta = dis_SSEcmp_E_to_G( sorb, delta+3, "cmpss", False, 4 );
      goto decode_success;
   }

   
   
   if (sz == 4 && insn[0] == 0x0F && (insn[1] == 0x2F || insn[1] == 0x2E)) {
      IRTemp argL = newTemp(Ity_F32);
      IRTemp argR = newTemp(Ity_F32);
      modrm = getIByte(delta+2);
      if (epartIsReg(modrm)) {
         assign( argR, getXMMRegLane32F( eregOfRM(modrm), 0 ) );
         delta += 2+1;
         DIP("[u]comiss %s,%s\n", nameXMMReg(eregOfRM(modrm)),
                                  nameXMMReg(gregOfRM(modrm)) );
      } else {
         addr = disAMode ( &alen, sorb, delta+2, dis_buf );
	 assign( argR, loadLE(Ity_F32, mkexpr(addr)) );
         delta += 2+alen;
         DIP("[u]comiss %s,%s\n", dis_buf,
                                  nameXMMReg(gregOfRM(modrm)) );
      }
      assign( argL, getXMMRegLane32F( gregOfRM(modrm), 0 ) );

      stmt( IRStmt_Put( OFFB_CC_OP,   mkU32(X86G_CC_OP_COPY) ));
      stmt( IRStmt_Put( OFFB_CC_DEP2, mkU32(0) ));
      stmt( IRStmt_Put( 
               OFFB_CC_DEP1,
               binop( Iop_And32,
                      binop(Iop_CmpF64, 
                            unop(Iop_F32toF64,mkexpr(argL)),
                            unop(Iop_F32toF64,mkexpr(argR))),
                      mkU32(0x45)
          )));
      stmt( IRStmt_Put( OFFB_CC_NDEP, mkU32(0) ));
      goto decode_success;
   }

   if (sz == 4 && insn[0] == 0x0F && insn[1] == 0x2A) {
      IRTemp arg64 = newTemp(Ity_I64);
      IRTemp rmode = newTemp(Ity_I32);
      vassert(sz == 4);

      modrm = getIByte(delta+2);
      do_MMX_preamble();
      if (epartIsReg(modrm)) {
         assign( arg64, getMMXReg(eregOfRM(modrm)) );
         delta += 2+1;
         DIP("cvtpi2ps %s,%s\n", nameMMXReg(eregOfRM(modrm)),
                                 nameXMMReg(gregOfRM(modrm)));
      } else {
         addr = disAMode ( &alen, sorb, delta+2, dis_buf );
	 assign( arg64, loadLE(Ity_I64, mkexpr(addr)) );
         delta += 2+alen;
         DIP("cvtpi2ps %s,%s\n", dis_buf,
                                 nameXMMReg(gregOfRM(modrm)) );
      }

      assign( rmode, get_sse_roundingmode() );

      putXMMRegLane32F( 
         gregOfRM(modrm), 0,
         binop(Iop_F64toF32, 
               mkexpr(rmode),
               unop(Iop_I32StoF64, 
                    unop(Iop_64to32, mkexpr(arg64)) )) );

      putXMMRegLane32F(
         gregOfRM(modrm), 1, 
         binop(Iop_F64toF32, 
               mkexpr(rmode),
               unop(Iop_I32StoF64,
                    unop(Iop_64HIto32, mkexpr(arg64)) )) );

      goto decode_success;
   }

   if (insn[0] == 0xF3 && insn[1] == 0x0F && insn[2] == 0x2A) {
      IRTemp arg32 = newTemp(Ity_I32);
      IRTemp rmode = newTemp(Ity_I32);
      vassert(sz == 4);

      modrm = getIByte(delta+3);
      if (epartIsReg(modrm)) {
         assign( arg32, getIReg(4, eregOfRM(modrm)) );
         delta += 3+1;
         DIP("cvtsi2ss %s,%s\n", nameIReg(4, eregOfRM(modrm)),
                                 nameXMMReg(gregOfRM(modrm)));
      } else {
         addr = disAMode ( &alen, sorb, delta+3, dis_buf );
	 assign( arg32, loadLE(Ity_I32, mkexpr(addr)) );
         delta += 3+alen;
         DIP("cvtsi2ss %s,%s\n", dis_buf,
                                 nameXMMReg(gregOfRM(modrm)) );
      }

      assign( rmode, get_sse_roundingmode() );

      putXMMRegLane32F( 
         gregOfRM(modrm), 0,
         binop(Iop_F64toF32,
               mkexpr(rmode),
               unop(Iop_I32StoF64, mkexpr(arg32)) ) );

      goto decode_success;
   }

   if (sz == 4 && insn[0] == 0x0F && (insn[1] == 0x2D || insn[1] == 0x2C)) {
      IRTemp dst64  = newTemp(Ity_I64);
      IRTemp rmode  = newTemp(Ity_I32);
      IRTemp f32lo  = newTemp(Ity_F32);
      IRTemp f32hi  = newTemp(Ity_F32);
      Bool   r2zero = toBool(insn[1] == 0x2C);

      do_MMX_preamble();
      modrm = getIByte(delta+2);

      if (epartIsReg(modrm)) {
         delta += 2+1;
	 assign(f32lo, getXMMRegLane32F(eregOfRM(modrm), 0));
	 assign(f32hi, getXMMRegLane32F(eregOfRM(modrm), 1));
         DIP("cvt%sps2pi %s,%s\n", r2zero ? "t" : "",
                                   nameXMMReg(eregOfRM(modrm)),
                                   nameMMXReg(gregOfRM(modrm)));
      } else {
         addr = disAMode ( &alen, sorb, delta+2, dis_buf );
	 assign(f32lo, loadLE(Ity_F32, mkexpr(addr)));
	 assign(f32hi, loadLE(Ity_F32, binop( Iop_Add32, 
                                              mkexpr(addr), 
                                              mkU32(4) )));
         delta += 2+alen;
         DIP("cvt%sps2pi %s,%s\n", r2zero ? "t" : "",
                                   dis_buf,
                                   nameMMXReg(gregOfRM(modrm)));
      }

      if (r2zero) {
         assign(rmode, mkU32((UInt)Irrm_ZERO) );
      } else {
         assign( rmode, get_sse_roundingmode() );
      }

      assign( 
         dst64,
         binop( Iop_32HLto64,
                binop( Iop_F64toI32S, 
                       mkexpr(rmode), 
                       unop( Iop_F32toF64, mkexpr(f32hi) ) ),
                binop( Iop_F64toI32S, 
                       mkexpr(rmode), 
                       unop( Iop_F32toF64, mkexpr(f32lo) ) )
              )
      );

      putMMXReg(gregOfRM(modrm), mkexpr(dst64));
      goto decode_success;
   }

   if (insn[0] == 0xF3 && insn[1] == 0x0F 
       && (insn[2] == 0x2D || insn[2] == 0x2C)) {
      IRTemp rmode = newTemp(Ity_I32);
      IRTemp f32lo = newTemp(Ity_F32);
      Bool   r2zero = toBool(insn[2] == 0x2C);
      vassert(sz == 4);

      modrm = getIByte(delta+3);
      if (epartIsReg(modrm)) {
         delta += 3+1;
	 assign(f32lo, getXMMRegLane32F(eregOfRM(modrm), 0));
         DIP("cvt%sss2si %s,%s\n", r2zero ? "t" : "",
                                   nameXMMReg(eregOfRM(modrm)),
                                   nameIReg(4, gregOfRM(modrm)));
      } else {
         addr = disAMode ( &alen, sorb, delta+3, dis_buf );
	 assign(f32lo, loadLE(Ity_F32, mkexpr(addr)));
         delta += 3+alen;
         DIP("cvt%sss2si %s,%s\n", r2zero ? "t" : "",
                                   dis_buf,
                                   nameIReg(4, gregOfRM(modrm)));
      }

      if (r2zero) {
         assign( rmode, mkU32((UInt)Irrm_ZERO) );
      } else {
         assign( rmode, get_sse_roundingmode() );
      }

      putIReg(4, gregOfRM(modrm),
                 binop( Iop_F64toI32S, 
                        mkexpr(rmode), 
                        unop( Iop_F32toF64, mkexpr(f32lo) ) )
      );

      goto decode_success;
   }

   
   if (sz == 4 && insn[0] == 0x0F && insn[1] == 0x5E) {
      delta = dis_SSE_E_to_G_all( sorb, delta+2, "divps", Iop_Div32Fx4 );
      goto decode_success;
   }

   
   if (insn[0] == 0xF3 && insn[1] == 0x0F && insn[2] == 0x5E) {
      vassert(sz == 4);
      delta = dis_SSE_E_to_G_lo32( sorb, delta+3, "divss", Iop_Div32F0x4 );
      goto decode_success;
   }

   
   if (insn[0] == 0x0F && insn[1] == 0xAE
       && !epartIsReg(insn[2]) && gregOfRM(insn[2]) == 2) {

      IRTemp t64 = newTemp(Ity_I64);
      IRTemp ew = newTemp(Ity_I32);

      modrm = getIByte(delta+2);
      vassert(!epartIsReg(modrm));
      vassert(sz == 4);

      addr = disAMode ( &alen, sorb, delta+2, dis_buf );
      delta += 2+alen;
      DIP("ldmxcsr %s\n", dis_buf);

      
      assign( t64, mkIRExprCCall(
                      Ity_I64, 0, 
                      "x86g_check_ldmxcsr",
                      &x86g_check_ldmxcsr, 
                      mkIRExprVec_1( loadLE(Ity_I32, mkexpr(addr)) )
                   )
            );

      put_sse_roundingmode( unop(Iop_64to32, mkexpr(t64)) );
      assign( ew, unop(Iop_64HIto32, mkexpr(t64) ) );
      put_emwarn( mkexpr(ew) );
      stmt( 
         IRStmt_Exit(
            binop(Iop_CmpNE32, mkexpr(ew), mkU32(0)),
            Ijk_EmWarn,
            IRConst_U32( ((Addr32)guest_EIP_bbstart)+delta),
            OFFB_EIP
         )
      );
      goto decode_success;
   }


  mmxext:

   
   
   if (sz == 4 && insn[0] == 0x0F && insn[1] == 0xF7) {
      Bool ok = False;
      delta = dis_MMX( &ok, sorb, sz, delta+1 );
      if (!ok)
         goto decode_failure;
      goto decode_success;
   }

   
   if (insn[0] == 0x0F && insn[1] == 0xE7) {
      modrm = getIByte(delta+2);
      if (sz == 4 && !epartIsReg(modrm)) {
         
         addr = disAMode ( &alen, sorb, delta+2, dis_buf );
         storeLE( mkexpr(addr), getMMXReg(gregOfRM(modrm)) );
         DIP("movntq %s,%s\n", dis_buf,
                               nameMMXReg(gregOfRM(modrm)));
         delta += 2+alen;
         goto decode_success;
      }
      
   }

   
   
   if (sz == 4 && insn[0] == 0x0F && insn[1] == 0xE0) {
      do_MMX_preamble();
      delta = dis_MMXop_regmem_to_reg ( 
                sorb, delta+2, insn[1], "pavgb", False );
      goto decode_success;
   }

   
   
   if (sz == 4 && insn[0] == 0x0F && insn[1] == 0xE3) {
      do_MMX_preamble();
      delta = dis_MMXop_regmem_to_reg ( 
                sorb, delta+2, insn[1], "pavgw", False );
      goto decode_success;
   }

   
   if (insn[0] == 0x0F && insn[1] == 0xC5) {
      modrm = insn[2];
      if (sz == 4 && epartIsReg(modrm)) {
         IRTemp sV = newTemp(Ity_I64);
         t5 = newTemp(Ity_I16);
         do_MMX_preamble();
         assign(sV, getMMXReg(eregOfRM(modrm)));
         breakup64to16s( sV, &t3, &t2, &t1, &t0 );
         switch (insn[3] & 3) {
            case 0:  assign(t5, mkexpr(t0)); break;
            case 1:  assign(t5, mkexpr(t1)); break;
            case 2:  assign(t5, mkexpr(t2)); break;
            case 3:  assign(t5, mkexpr(t3)); break;
            default: vassert(0); 
         }
         putIReg(4, gregOfRM(modrm), unop(Iop_16Uto32, mkexpr(t5)));
         DIP("pextrw $%d,%s,%s\n",
             (Int)insn[3], nameMMXReg(eregOfRM(modrm)),
                           nameIReg(4,gregOfRM(modrm)));
         delta += 4;
         goto decode_success;
      } 
      
   }

   
   if (sz == 4 && insn[0] == 0x0F && insn[1] == 0xC4) {
      Int lane;
      t4 = newTemp(Ity_I16);
      t5 = newTemp(Ity_I64);
      t6 = newTemp(Ity_I64);
      modrm = insn[2];
      do_MMX_preamble();

      assign(t5, getMMXReg(gregOfRM(modrm)));
      breakup64to16s( t5, &t3, &t2, &t1, &t0 );

      if (epartIsReg(modrm)) {
         assign(t4, getIReg(2, eregOfRM(modrm)));
         delta += 3+1;
         lane = insn[3+1-1];
         DIP("pinsrw $%d,%s,%s\n", (Int)lane, 
                                   nameIReg(2,eregOfRM(modrm)),
                                   nameMMXReg(gregOfRM(modrm)));
      } else {
         addr = disAMode ( &alen, sorb, delta+2, dis_buf );
         delta += 3+alen;
         lane = insn[3+alen-1];
         assign(t4, loadLE(Ity_I16, mkexpr(addr)));
         DIP("pinsrw $%d,%s,%s\n", (Int)lane, 
                                   dis_buf,
                                   nameMMXReg(gregOfRM(modrm)));
      }

      switch (lane & 3) {
         case 0:  assign(t6, mk64from16s(t3,t2,t1,t4)); break;
         case 1:  assign(t6, mk64from16s(t3,t2,t4,t0)); break;
         case 2:  assign(t6, mk64from16s(t3,t4,t1,t0)); break;
         case 3:  assign(t6, mk64from16s(t4,t2,t1,t0)); break;
         default: vassert(0); 
      }
      putMMXReg(gregOfRM(modrm), mkexpr(t6));
      goto decode_success;
   }

   
   
   if (sz == 4 && insn[0] == 0x0F && insn[1] == 0xEE) {
      do_MMX_preamble();
      delta = dis_MMXop_regmem_to_reg ( 
                sorb, delta+2, insn[1], "pmaxsw", False );
      goto decode_success;
   }

   
   
   if (sz == 4 && insn[0] == 0x0F && insn[1] == 0xDE) {
      do_MMX_preamble();
      delta = dis_MMXop_regmem_to_reg ( 
                sorb, delta+2, insn[1], "pmaxub", False );
      goto decode_success;
   }

   
   
   if (sz == 4 && insn[0] == 0x0F && insn[1] == 0xEA) {
      do_MMX_preamble();
      delta = dis_MMXop_regmem_to_reg ( 
                sorb, delta+2, insn[1], "pminsw", False );
      goto decode_success;
   }

   
   
   if (sz == 4 && insn[0] == 0x0F && insn[1] == 0xDA) {
      do_MMX_preamble();
      delta = dis_MMXop_regmem_to_reg ( 
                sorb, delta+2, insn[1], "pminub", False );
      goto decode_success;
   }

   
   if (sz == 4 && insn[0] == 0x0F && insn[1] == 0xD7) {
      modrm = insn[2];
      if (epartIsReg(modrm)) {
         do_MMX_preamble();
         t0 = newTemp(Ity_I64);
         t1 = newTemp(Ity_I32);
         assign(t0, getMMXReg(eregOfRM(modrm)));
         assign(t1, unop(Iop_8Uto32, unop(Iop_GetMSBs8x8, mkexpr(t0))));
         putIReg(4, gregOfRM(modrm), mkexpr(t1));
         DIP("pmovmskb %s,%s\n", nameMMXReg(eregOfRM(modrm)),
                                 nameIReg(4,gregOfRM(modrm)));
         delta += 3;
         goto decode_success;
      } 
      
   }

   
   
   if (sz == 4 && insn[0] == 0x0F && insn[1] == 0xE4) {
      do_MMX_preamble();
      delta = dis_MMXop_regmem_to_reg ( 
                sorb, delta+2, insn[1], "pmuluh", False );
      goto decode_success;
   }

   
   
   
   
   if (insn[0] == 0x0F && insn[1] == 0x18
       && !epartIsReg(insn[2]) 
       && gregOfRM(insn[2]) >= 0 && gregOfRM(insn[2]) <= 3) {
      const HChar* hintstr = "??";

      modrm = getIByte(delta+2);
      vassert(!epartIsReg(modrm));

      addr = disAMode ( &alen, sorb, delta+2, dis_buf );
      delta += 2+alen;

      switch (gregOfRM(modrm)) {
         case 0: hintstr = "nta"; break;
         case 1: hintstr = "t0"; break;
         case 2: hintstr = "t1"; break;
         case 3: hintstr = "t2"; break;
         default: vassert(0); 
      }

      DIP("prefetch%s %s\n", hintstr, dis_buf);
      goto decode_success;
   }

   
   
   if (insn[0] == 0x0F && insn[1] == 0x0D
       && !epartIsReg(insn[2]) 
       && gregOfRM(insn[2]) >= 0 && gregOfRM(insn[2]) <= 1) {
      const HChar* hintstr = "??";

      modrm = getIByte(delta+2);
      vassert(!epartIsReg(modrm));

      addr = disAMode ( &alen, sorb, delta+2, dis_buf );
      delta += 2+alen;

      switch (gregOfRM(modrm)) {
         case 0: hintstr = ""; break;
         case 1: hintstr = "w"; break;
         default: vassert(0); 
      }

      DIP("prefetch%s %s\n", hintstr, dis_buf);
      goto decode_success;
   }

   
   
   if (sz == 4 && insn[0] == 0x0F && insn[1] == 0xF6) {
      do_MMX_preamble();
      delta = dis_MMXop_regmem_to_reg ( 
                 sorb, delta+2, insn[1], "psadbw", False );
      goto decode_success;
   }

   
   
   if (sz == 4 && insn[0] == 0x0F && insn[1] == 0x70) {
      Int order;
      IRTemp sV, dV, s3, s2, s1, s0;
      s3 = s2 = s1 = s0 = IRTemp_INVALID;
      sV = newTemp(Ity_I64);
      dV = newTemp(Ity_I64);
      do_MMX_preamble();
      modrm = insn[2];
      if (epartIsReg(modrm)) {
         assign( sV, getMMXReg(eregOfRM(modrm)) );
         order = (Int)insn[3];
         delta += 2+2;
         DIP("pshufw $%d,%s,%s\n", order, 
                                   nameMMXReg(eregOfRM(modrm)),
                                   nameMMXReg(gregOfRM(modrm)));
      } else {
         addr = disAMode ( &alen, sorb, delta+2, dis_buf );
         assign( sV, loadLE(Ity_I64, mkexpr(addr)) );
	 order = (Int)insn[2+alen];
         delta += 3+alen;
         DIP("pshufw $%d,%s,%s\n", order, 
                                   dis_buf,
                                   nameMMXReg(gregOfRM(modrm)));
      }
      breakup64to16s( sV, &s3, &s2, &s1, &s0 );

#     define SEL(n) \
                ((n)==0 ? s0 : ((n)==1 ? s1 : ((n)==2 ? s2 : s3)))
      assign(dV,
	     mk64from16s( SEL((order>>6)&3), SEL((order>>4)&3),
                          SEL((order>>2)&3), SEL((order>>0)&3) )
      );
      putMMXReg(gregOfRM(modrm), mkexpr(dV));
#     undef SEL
      goto decode_success;
   }

   
   if (insn[0] == 0x0F && insn[1] == 0xAE
       && epartIsReg(insn[2]) && gregOfRM(insn[2]) == 7) {
      vassert(sz == 4);
      delta += 3;
      stmt( IRStmt_MBE(Imbe_Fence) );
      DIP("sfence\n");
      goto decode_success;
   }

   
   if (archinfo->hwcaps == VEX_HWCAPS_X86_MMXEXT)
      goto after_sse_decoders;


   
   if (sz == 4 && insn[0] == 0x0F && insn[1] == 0x5F) {
      delta = dis_SSE_E_to_G_all( sorb, delta+2, "maxps", Iop_Max32Fx4 );
      goto decode_success;
   }

   
   if (insn[0] == 0xF3 && insn[1] == 0x0F && insn[2] == 0x5F) {
      vassert(sz == 4);
      delta = dis_SSE_E_to_G_lo32( sorb, delta+3, "maxss", Iop_Max32F0x4 );
      goto decode_success;
   }

   
   if (sz == 4 && insn[0] == 0x0F && insn[1] == 0x5D) {
      delta = dis_SSE_E_to_G_all( sorb, delta+2, "minps", Iop_Min32Fx4 );
      goto decode_success;
   }

   
   if (insn[0] == 0xF3 && insn[1] == 0x0F && insn[2] == 0x5D) {
      vassert(sz == 4);
      delta = dis_SSE_E_to_G_lo32( sorb, delta+3, "minss", Iop_Min32F0x4 );
      goto decode_success;
   }

   
   
   if (sz == 4 && insn[0] == 0x0F && (insn[1] == 0x28 || insn[1] == 0x10)) {
      modrm = getIByte(delta+2);
      if (epartIsReg(modrm)) {
         putXMMReg( gregOfRM(modrm), 
                    getXMMReg( eregOfRM(modrm) ));
         DIP("mov[ua]ps %s,%s\n", nameXMMReg(eregOfRM(modrm)),
                                  nameXMMReg(gregOfRM(modrm)));
         delta += 2+1;
      } else {
         addr = disAMode ( &alen, sorb, delta+2, dis_buf );
         if (insn[1] == 0x28)
            gen_SEGV_if_not_16_aligned( addr );
         putXMMReg( gregOfRM(modrm), 
                    loadLE(Ity_V128, mkexpr(addr)) );
         DIP("mov[ua]ps %s,%s\n", dis_buf,
                                  nameXMMReg(gregOfRM(modrm)));
         delta += 2+alen;
      }
      goto decode_success;
   }

   
   
   if (sz == 4 && insn[0] == 0x0F 
       && (insn[1] == 0x29 || insn[1] == 0x11)) {
      modrm = getIByte(delta+2);
      if (epartIsReg(modrm)) {
         
      } else {
         addr = disAMode ( &alen, sorb, delta+2, dis_buf );
         if (insn[1] == 0x29)
            gen_SEGV_if_not_16_aligned( addr );
         storeLE( mkexpr(addr), getXMMReg(gregOfRM(modrm)) );
         DIP("mov[ua]ps %s,%s\n", nameXMMReg(gregOfRM(modrm)),
                                  dis_buf );
         delta += 2+alen;
         goto decode_success;
      }
   }

   
   
   if (sz == 4 && insn[0] == 0x0F && insn[1] == 0x16) {
      modrm = getIByte(delta+2);
      if (epartIsReg(modrm)) {
         delta += 2+1;
         putXMMRegLane64( gregOfRM(modrm), 1,
                          getXMMRegLane64( eregOfRM(modrm), 0 ) );
         DIP("movhps %s,%s\n", nameXMMReg(eregOfRM(modrm)), 
                               nameXMMReg(gregOfRM(modrm)));
      } else {
         addr = disAMode ( &alen, sorb, delta+2, dis_buf );
         delta += 2+alen;
         putXMMRegLane64( gregOfRM(modrm), 1,
                          loadLE(Ity_I64, mkexpr(addr)) );
         DIP("movhps %s,%s\n", dis_buf, 
                               nameXMMReg( gregOfRM(modrm) ));
      }
      goto decode_success;
   }

   
   if (sz == 4 && insn[0] == 0x0F && insn[1] == 0x17) {
      if (!epartIsReg(insn[2])) {
         delta += 2;
         addr = disAMode ( &alen, sorb, delta, dis_buf );
         delta += alen;
         storeLE( mkexpr(addr), 
                  getXMMRegLane64( gregOfRM(insn[2]),
                                   1 ) );
         DIP("movhps %s,%s\n", nameXMMReg( gregOfRM(insn[2]) ),
                               dis_buf);
         goto decode_success;
      }
      
   }

   
   
   if (sz == 4 && insn[0] == 0x0F && insn[1] == 0x12) {
      modrm = getIByte(delta+2);
      if (epartIsReg(modrm)) {
         delta += 2+1;
         putXMMRegLane64( gregOfRM(modrm),  
                          0,
                          getXMMRegLane64( eregOfRM(modrm), 1 ));
         DIP("movhlps %s, %s\n", nameXMMReg(eregOfRM(modrm)), 
                                 nameXMMReg(gregOfRM(modrm)));
      } else {
         addr = disAMode ( &alen, sorb, delta+2, dis_buf );
         delta += 2+alen;
         putXMMRegLane64( gregOfRM(modrm),  0,
                          loadLE(Ity_I64, mkexpr(addr)) );
         DIP("movlps %s, %s\n", 
             dis_buf, nameXMMReg( gregOfRM(modrm) ));
      }
      goto decode_success;
   }

   
   if (sz == 4 && insn[0] == 0x0F && insn[1] == 0x13) {
      if (!epartIsReg(insn[2])) {
         delta += 2;
         addr = disAMode ( &alen, sorb, delta, dis_buf );
         delta += alen;
         storeLE( mkexpr(addr), 
                  getXMMRegLane64( gregOfRM(insn[2]), 
                                   0 ) );
         DIP("movlps %s, %s\n", nameXMMReg( gregOfRM(insn[2]) ),
                                dis_buf);
         goto decode_success;
      }
      
   }

   if (insn[0] == 0x0F && insn[1] == 0x50) {
      modrm = getIByte(delta+2);
      if (sz == 4 && epartIsReg(modrm)) {
         Int src;
         t0 = newTemp(Ity_I32);
         t1 = newTemp(Ity_I32);
         t2 = newTemp(Ity_I32);
         t3 = newTemp(Ity_I32);
         delta += 2+1;
         src = eregOfRM(modrm);
         assign( t0, binop( Iop_And32,
                            binop(Iop_Shr32, getXMMRegLane32(src,0), mkU8(31)),
                            mkU32(1) ));
         assign( t1, binop( Iop_And32,
                            binop(Iop_Shr32, getXMMRegLane32(src,1), mkU8(30)),
                            mkU32(2) ));
         assign( t2, binop( Iop_And32,
                            binop(Iop_Shr32, getXMMRegLane32(src,2), mkU8(29)),
                            mkU32(4) ));
         assign( t3, binop( Iop_And32,
                            binop(Iop_Shr32, getXMMRegLane32(src,3), mkU8(28)),
                            mkU32(8) ));
         putIReg(4, gregOfRM(modrm),
                    binop(Iop_Or32,
                          binop(Iop_Or32, mkexpr(t0), mkexpr(t1)),
                          binop(Iop_Or32, mkexpr(t2), mkexpr(t3))
                         )
                 );
         DIP("movmskps %s,%s\n", nameXMMReg(src), 
                                 nameIReg(4, gregOfRM(modrm)));
         goto decode_success;
      }
      
   }

   
   
   if (insn[0] == 0x0F && insn[1] == 0x2B) {
      modrm = getIByte(delta+2);
      if (!epartIsReg(modrm)) {
         addr = disAMode ( &alen, sorb, delta+2, dis_buf );
         gen_SEGV_if_not_16_aligned( addr );
         storeLE( mkexpr(addr), getXMMReg(gregOfRM(modrm)) );
         DIP("movntp%s %s,%s\n", sz==2 ? "d" : "s",
                                 dis_buf,
                                 nameXMMReg(gregOfRM(modrm)));
         delta += 2+alen;
         goto decode_success;
      }
      
   }

   if (insn[0] == 0xF3 && insn[1] == 0x0F && insn[2] == 0x10) {
      vassert(sz == 4);
      modrm = getIByte(delta+3);
      if (epartIsReg(modrm)) {
         putXMMRegLane32( gregOfRM(modrm), 0,
                          getXMMRegLane32( eregOfRM(modrm), 0 ));
         DIP("movss %s,%s\n", nameXMMReg(eregOfRM(modrm)),
                              nameXMMReg(gregOfRM(modrm)));
         delta += 3+1;
      } else {
         addr = disAMode ( &alen, sorb, delta+3, dis_buf );
         
         putXMMRegLane64( gregOfRM(modrm), 1, mkU64(0) ); 
         
         putXMMRegLane32( gregOfRM(modrm), 1, mkU32(0) ); 
         
         putXMMRegLane32( gregOfRM(modrm), 0,
                          loadLE(Ity_I32, mkexpr(addr)) );
         DIP("movss %s,%s\n", dis_buf,
                              nameXMMReg(gregOfRM(modrm)));
         delta += 3+alen;
      }
      goto decode_success;
   }

   if (insn[0] == 0xF3 && insn[1] == 0x0F && insn[2] == 0x11) {
      vassert(sz == 4);
      modrm = getIByte(delta+3);
      if (epartIsReg(modrm)) {
         
      } else {
         addr = disAMode ( &alen, sorb, delta+3, dis_buf );
         storeLE( mkexpr(addr),
                  getXMMRegLane32(gregOfRM(modrm), 0) );
         DIP("movss %s,%s\n", nameXMMReg(gregOfRM(modrm)),
                              dis_buf);
         delta += 3+alen;
         goto decode_success;
      }
   }

   
   if (sz == 4 && insn[0] == 0x0F && insn[1] == 0x59) {
      delta = dis_SSE_E_to_G_all( sorb, delta+2, "mulps", Iop_Mul32Fx4 );
      goto decode_success;
   }

   
   if (insn[0] == 0xF3 && insn[1] == 0x0F && insn[2] == 0x59) {
      vassert(sz == 4);
      delta = dis_SSE_E_to_G_lo32( sorb, delta+3, "mulss", Iop_Mul32F0x4 );
      goto decode_success;
   }

   
   if (sz == 4 && insn[0] == 0x0F && insn[1] == 0x56) {
      delta = dis_SSE_E_to_G_all( sorb, delta+2, "orps", Iop_OrV128 );
      goto decode_success;
   }

   
   if (insn[0] == 0x0F && insn[1] == 0x53) {
      vassert(sz == 4);
      delta = dis_SSE_E_to_G_unary_all( sorb, delta+2, 
                                        "rcpps", Iop_RecipEst32Fx4 );
      goto decode_success;
   }

   
   if (insn[0] == 0xF3 && insn[1] == 0x0F && insn[2] == 0x53) {
      vassert(sz == 4);
      delta = dis_SSE_E_to_G_unary_lo32( sorb, delta+3, 
                                         "rcpss", Iop_RecipEst32F0x4 );
      goto decode_success;
   }

   
   if (insn[0] == 0x0F && insn[1] == 0x52) {
      vassert(sz == 4);
      delta = dis_SSE_E_to_G_unary_all( sorb, delta+2, 
                                        "rsqrtps", Iop_RSqrtEst32Fx4 );
      goto decode_success;
   }

   
   if (insn[0] == 0xF3 && insn[1] == 0x0F && insn[2] == 0x52) {
      vassert(sz == 4);
      delta = dis_SSE_E_to_G_unary_lo32( sorb, delta+3, 
                                         "rsqrtss", Iop_RSqrtEst32F0x4 );
      goto decode_success;
   }

   
   if (sz == 4 && insn[0] == 0x0F && insn[1] == 0xC6) {
      Int    select;
      IRTemp sV, dV;
      IRTemp s3, s2, s1, s0, d3, d2, d1, d0;
      sV = newTemp(Ity_V128);
      dV = newTemp(Ity_V128);
      s3 = s2 = s1 = s0 = d3 = d2 = d1 = d0 = IRTemp_INVALID;
      modrm = insn[2];
      assign( dV, getXMMReg(gregOfRM(modrm)) );

      if (epartIsReg(modrm)) {
         assign( sV, getXMMReg(eregOfRM(modrm)) );
         select = (Int)insn[3];
         delta += 2+2;
         DIP("shufps $%d,%s,%s\n", select, 
                                   nameXMMReg(eregOfRM(modrm)),
                                   nameXMMReg(gregOfRM(modrm)));
      } else {
         addr = disAMode ( &alen, sorb, delta+2, dis_buf );
         assign( sV, loadLE(Ity_V128, mkexpr(addr)) );
         select = (Int)insn[2+alen];
         delta += 3+alen;
         DIP("shufps $%d,%s,%s\n", select, 
                                   dis_buf,
                                   nameXMMReg(gregOfRM(modrm)));
      }

      breakup128to32s( dV, &d3, &d2, &d1, &d0 );
      breakup128to32s( sV, &s3, &s2, &s1, &s0 );

#     define SELD(n) ((n)==0 ? d0 : ((n)==1 ? d1 : ((n)==2 ? d2 : d3)))
#     define SELS(n) ((n)==0 ? s0 : ((n)==1 ? s1 : ((n)==2 ? s2 : s3)))

      putXMMReg(
         gregOfRM(modrm), 
         mk128from32s( SELS((select>>6)&3), SELS((select>>4)&3), 
                       SELD((select>>2)&3), SELD((select>>0)&3) )
      );

#     undef SELD
#     undef SELS

      goto decode_success;
   }

   
   if (sz == 4 && insn[0] == 0x0F && insn[1] == 0x51) {
      delta = dis_SSE_E_to_G_unary_all( sorb, delta+2, 
                                        "sqrtps", Iop_Sqrt32Fx4 );
      goto decode_success;
   }

   
   if (insn[0] == 0xF3 && insn[1] == 0x0F && insn[2] == 0x51) {
      vassert(sz == 4);
      delta = dis_SSE_E_to_G_unary_lo32( sorb, delta+3, 
                                         "sqrtss", Iop_Sqrt32F0x4 );
      goto decode_success;
   }

   
   if (insn[0] == 0x0F && insn[1] == 0xAE
       && !epartIsReg(insn[2]) && gregOfRM(insn[2]) == 3) {
      modrm = getIByte(delta+2);
      vassert(sz == 4);
      vassert(!epartIsReg(modrm));

      addr = disAMode ( &alen, sorb, delta+2, dis_buf );
      delta += 2+alen;

      
      DIP("stmxcsr %s\n", dis_buf);
      storeLE( mkexpr(addr), 
               mkIRExprCCall(
                  Ity_I32, 0,
                  "x86g_create_mxcsr", &x86g_create_mxcsr, 
                  mkIRExprVec_1( get_sse_roundingmode() ) 
               ) 
             );
      goto decode_success;
   }

   
   if (sz == 4 && insn[0] == 0x0F && insn[1] == 0x5C) {
      delta = dis_SSE_E_to_G_all( sorb, delta+2, "subps", Iop_Sub32Fx4 );
      goto decode_success;
   }

   
   if (insn[0] == 0xF3 && insn[1] == 0x0F && insn[2] == 0x5C) {
      vassert(sz == 4);
      delta = dis_SSE_E_to_G_lo32( sorb, delta+3, "subss", Iop_Sub32F0x4 );
      goto decode_success;
   }

   
   
   
   if (sz == 4 && insn[0] == 0x0F && (insn[1] == 0x15 || insn[1] == 0x14)) {
      IRTemp sV, dV;
      IRTemp s3, s2, s1, s0, d3, d2, d1, d0;
      Bool hi = toBool(insn[1] == 0x15);
      sV = newTemp(Ity_V128);
      dV = newTemp(Ity_V128);
      s3 = s2 = s1 = s0 = d3 = d2 = d1 = d0 = IRTemp_INVALID;
      modrm = insn[2];
      assign( dV, getXMMReg(gregOfRM(modrm)) );

      if (epartIsReg(modrm)) {
         assign( sV, getXMMReg(eregOfRM(modrm)) );
         delta += 2+1;
         DIP("unpck%sps %s,%s\n", hi ? "h" : "l",
                                  nameXMMReg(eregOfRM(modrm)),
                                  nameXMMReg(gregOfRM(modrm)));
      } else {
         addr = disAMode ( &alen, sorb, delta+2, dis_buf );
         assign( sV, loadLE(Ity_V128, mkexpr(addr)) );
         delta += 2+alen;
         DIP("unpck%sps %s,%s\n", hi ? "h" : "l",
                                  dis_buf,
                                  nameXMMReg(gregOfRM(modrm)));
      }

      breakup128to32s( dV, &d3, &d2, &d1, &d0 );
      breakup128to32s( sV, &s3, &s2, &s1, &s0 );

      if (hi) {
         putXMMReg( gregOfRM(modrm), mk128from32s( s3, d3, s2, d2 ) );
      } else {
         putXMMReg( gregOfRM(modrm), mk128from32s( s1, d1, s0, d0 ) );
      }

      goto decode_success;
   }

   
   if (sz == 4 && insn[0] == 0x0F && insn[1] == 0x57) {
      delta = dis_SSE_E_to_G_all( sorb, delta+2, "xorps", Iop_XorV128 );
      goto decode_success;
   }

   
   
   

   
   
   

   if (0 == (archinfo->hwcaps & VEX_HWCAPS_X86_SSE2))
      goto after_sse_decoders; 

   insn = &guest_code[delta];

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0x58) {
      delta = dis_SSE_E_to_G_all( sorb, delta+2, "addpd", Iop_Add64Fx2 );
      goto decode_success;
   }
 
   
   if (insn[0] == 0xF2 && insn[1] == 0x0F && insn[2] == 0x58) {
      vassert(sz == 4);
      delta = dis_SSE_E_to_G_lo64( sorb, delta+3, "addsd", Iop_Add64F0x2 );
      goto decode_success;
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0x55) {
      delta = dis_SSE_E_to_G_all_invG( sorb, delta+2, "andnpd", Iop_AndV128 );
      goto decode_success;
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0x54) {
      delta = dis_SSE_E_to_G_all( sorb, delta+2, "andpd", Iop_AndV128 );
      goto decode_success;
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0xC2) {
      delta = dis_SSEcmp_E_to_G( sorb, delta+2, "cmppd", True, 8 );
      goto decode_success;
   }

   
   if (insn[0] == 0xF2 && insn[1] == 0x0F && insn[2] == 0xC2) {
      vassert(sz == 4);
      delta = dis_SSEcmp_E_to_G( sorb, delta+3, "cmpsd", False, 8 );
      goto decode_success;
   }

   
   
   if (sz == 2 && insn[0] == 0x0F && (insn[1] == 0x2F || insn[1] == 0x2E)) {
      IRTemp argL = newTemp(Ity_F64);
      IRTemp argR = newTemp(Ity_F64);
      modrm = getIByte(delta+2);
      if (epartIsReg(modrm)) {
         assign( argR, getXMMRegLane64F( eregOfRM(modrm), 0 ) );
         delta += 2+1;
         DIP("[u]comisd %s,%s\n", nameXMMReg(eregOfRM(modrm)),
                                  nameXMMReg(gregOfRM(modrm)) );
      } else {
         addr = disAMode ( &alen, sorb, delta+2, dis_buf );
	 assign( argR, loadLE(Ity_F64, mkexpr(addr)) );
         delta += 2+alen;
         DIP("[u]comisd %s,%s\n", dis_buf,
                                  nameXMMReg(gregOfRM(modrm)) );
      }
      assign( argL, getXMMRegLane64F( gregOfRM(modrm), 0 ) );

      stmt( IRStmt_Put( OFFB_CC_OP,   mkU32(X86G_CC_OP_COPY) ));
      stmt( IRStmt_Put( OFFB_CC_DEP2, mkU32(0) ));
      stmt( IRStmt_Put( 
               OFFB_CC_DEP1,
               binop( Iop_And32,
                      binop(Iop_CmpF64, mkexpr(argL), mkexpr(argR)),
                      mkU32(0x45)
          )));
      stmt( IRStmt_Put( OFFB_CC_NDEP, mkU32(0) ));
      goto decode_success;
   }

   if (insn[0] == 0xF3 && insn[1] == 0x0F && insn[2] == 0xE6) {
      IRTemp arg64 = newTemp(Ity_I64);
      vassert(sz == 4);

      modrm = getIByte(delta+3);
      if (epartIsReg(modrm)) {
         assign( arg64, getXMMRegLane64(eregOfRM(modrm), 0) );
         delta += 3+1;
         DIP("cvtdq2pd %s,%s\n", nameXMMReg(eregOfRM(modrm)),
                                 nameXMMReg(gregOfRM(modrm)));
      } else {
         addr = disAMode ( &alen, sorb, delta+3, dis_buf );
	 assign( arg64, loadLE(Ity_I64, mkexpr(addr)) );
         delta += 3+alen;
         DIP("cvtdq2pd %s,%s\n", dis_buf,
                                 nameXMMReg(gregOfRM(modrm)) );
      }

      putXMMRegLane64F( 
         gregOfRM(modrm), 0,
         unop(Iop_I32StoF64, unop(Iop_64to32, mkexpr(arg64)))
      );

      putXMMRegLane64F(
         gregOfRM(modrm), 1, 
         unop(Iop_I32StoF64, unop(Iop_64HIto32, mkexpr(arg64)))
      );

      goto decode_success;
   }

   if (sz == 4 && insn[0] == 0x0F && insn[1] == 0x5B) {
      IRTemp argV  = newTemp(Ity_V128);
      IRTemp rmode = newTemp(Ity_I32);

      modrm = getIByte(delta+2);
      if (epartIsReg(modrm)) {
         assign( argV, getXMMReg(eregOfRM(modrm)) );
         delta += 2+1;
         DIP("cvtdq2ps %s,%s\n", nameXMMReg(eregOfRM(modrm)),
                                 nameXMMReg(gregOfRM(modrm)));
      } else {
         addr = disAMode ( &alen, sorb, delta+2, dis_buf );
	 assign( argV, loadLE(Ity_V128, mkexpr(addr)) );
         delta += 2+alen;
         DIP("cvtdq2ps %s,%s\n", dis_buf,
                                 nameXMMReg(gregOfRM(modrm)) );
      }
         
      assign( rmode, get_sse_roundingmode() );
      breakup128to32s( argV, &t3, &t2, &t1, &t0 );

#     define CVT(_t)  binop( Iop_F64toF32,                    \
                             mkexpr(rmode),                   \
                             unop(Iop_I32StoF64,mkexpr(_t)))
      
      putXMMRegLane32F( gregOfRM(modrm), 3, CVT(t3) );
      putXMMRegLane32F( gregOfRM(modrm), 2, CVT(t2) );
      putXMMRegLane32F( gregOfRM(modrm), 1, CVT(t1) );
      putXMMRegLane32F( gregOfRM(modrm), 0, CVT(t0) );

#     undef CVT

      goto decode_success;
   }

   if (insn[0] == 0xF2 && insn[1] == 0x0F && insn[2] == 0xE6) {
      IRTemp argV  = newTemp(Ity_V128);
      IRTemp rmode = newTemp(Ity_I32);
      vassert(sz == 4);

      modrm = getIByte(delta+3);
      if (epartIsReg(modrm)) {
         assign( argV, getXMMReg(eregOfRM(modrm)) );
         delta += 3+1;
         DIP("cvtpd2dq %s,%s\n", nameXMMReg(eregOfRM(modrm)),
                                 nameXMMReg(gregOfRM(modrm)));
      } else {
         addr = disAMode ( &alen, sorb, delta+3, dis_buf );
	 assign( argV, loadLE(Ity_V128, mkexpr(addr)) );
         delta += 3+alen;
         DIP("cvtpd2dq %s,%s\n", dis_buf,
                                 nameXMMReg(gregOfRM(modrm)) );
      }
         
      assign( rmode, get_sse_roundingmode() );
      t0 = newTemp(Ity_F64);
      t1 = newTemp(Ity_F64);
      assign( t0, unop(Iop_ReinterpI64asF64, 
                       unop(Iop_V128to64, mkexpr(argV))) );
      assign( t1, unop(Iop_ReinterpI64asF64, 
                       unop(Iop_V128HIto64, mkexpr(argV))) );
      
#     define CVT(_t)  binop( Iop_F64toI32S,                   \
                             mkexpr(rmode),                   \
                             mkexpr(_t) )
      
      putXMMRegLane32( gregOfRM(modrm), 3, mkU32(0) );
      putXMMRegLane32( gregOfRM(modrm), 2, mkU32(0) );
      putXMMRegLane32( gregOfRM(modrm), 1, CVT(t1) );
      putXMMRegLane32( gregOfRM(modrm), 0, CVT(t0) );

#     undef CVT

      goto decode_success;
   }

   if (sz == 2 && insn[0] == 0x0F && (insn[1] == 0x2D || insn[1] == 0x2C)) {
      IRTemp dst64  = newTemp(Ity_I64);
      IRTemp rmode  = newTemp(Ity_I32);
      IRTemp f64lo  = newTemp(Ity_F64);
      IRTemp f64hi  = newTemp(Ity_F64);
      Bool   r2zero = toBool(insn[1] == 0x2C);

      do_MMX_preamble();
      modrm = getIByte(delta+2);

      if (epartIsReg(modrm)) {
         delta += 2+1;
	 assign(f64lo, getXMMRegLane64F(eregOfRM(modrm), 0));
	 assign(f64hi, getXMMRegLane64F(eregOfRM(modrm), 1));
         DIP("cvt%spd2pi %s,%s\n", r2zero ? "t" : "",
                                   nameXMMReg(eregOfRM(modrm)),
                                   nameMMXReg(gregOfRM(modrm)));
      } else {
         addr = disAMode ( &alen, sorb, delta+2, dis_buf );
	 assign(f64lo, loadLE(Ity_F64, mkexpr(addr)));
	 assign(f64hi, loadLE(Ity_F64, binop( Iop_Add32, 
                                              mkexpr(addr), 
                                              mkU32(8) )));
         delta += 2+alen;
         DIP("cvt%spf2pi %s,%s\n", r2zero ? "t" : "",
                                   dis_buf,
                                   nameMMXReg(gregOfRM(modrm)));
      }

      if (r2zero) {
         assign(rmode, mkU32((UInt)Irrm_ZERO) );
      } else {
         assign( rmode, get_sse_roundingmode() );
      }

      assign( 
         dst64,
         binop( Iop_32HLto64,
                binop( Iop_F64toI32S, mkexpr(rmode), mkexpr(f64hi) ),
                binop( Iop_F64toI32S, mkexpr(rmode), mkexpr(f64lo) )
              )
      );

      putMMXReg(gregOfRM(modrm), mkexpr(dst64));
      goto decode_success;
   }

   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0x5A) {
      IRTemp argV  = newTemp(Ity_V128);
      IRTemp rmode = newTemp(Ity_I32);

      modrm = getIByte(delta+2);
      if (epartIsReg(modrm)) {
         assign( argV, getXMMReg(eregOfRM(modrm)) );
         delta += 2+1;
         DIP("cvtpd2ps %s,%s\n", nameXMMReg(eregOfRM(modrm)),
                                 nameXMMReg(gregOfRM(modrm)));
      } else {
         addr = disAMode ( &alen, sorb, delta+2, dis_buf );
	 assign( argV, loadLE(Ity_V128, mkexpr(addr)) );
         delta += 2+alen;
         DIP("cvtpd2ps %s,%s\n", dis_buf,
                                 nameXMMReg(gregOfRM(modrm)) );
      }
         
      assign( rmode, get_sse_roundingmode() );
      t0 = newTemp(Ity_F64);
      t1 = newTemp(Ity_F64);
      assign( t0, unop(Iop_ReinterpI64asF64, 
                       unop(Iop_V128to64, mkexpr(argV))) );
      assign( t1, unop(Iop_ReinterpI64asF64, 
                       unop(Iop_V128HIto64, mkexpr(argV))) );
      
#     define CVT(_t)  binop( Iop_F64toF32,                    \
                             mkexpr(rmode),                   \
                             mkexpr(_t) )
      
      putXMMRegLane32(  gregOfRM(modrm), 3, mkU32(0) );
      putXMMRegLane32(  gregOfRM(modrm), 2, mkU32(0) );
      putXMMRegLane32F( gregOfRM(modrm), 1, CVT(t1) );
      putXMMRegLane32F( gregOfRM(modrm), 0, CVT(t0) );

#     undef CVT

      goto decode_success;
   }

   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0x2A) {
      IRTemp arg64 = newTemp(Ity_I64);

      modrm = getIByte(delta+2);
      if (epartIsReg(modrm)) {
         do_MMX_preamble();
         assign( arg64, getMMXReg(eregOfRM(modrm)) );
         delta += 2+1;
         DIP("cvtpi2pd %s,%s\n", nameMMXReg(eregOfRM(modrm)),
                                 nameXMMReg(gregOfRM(modrm)));
      } else {
         addr = disAMode ( &alen, sorb, delta+2, dis_buf );
	 assign( arg64, loadLE(Ity_I64, mkexpr(addr)) );
         delta += 2+alen;
         DIP("cvtpi2pd %s,%s\n", dis_buf,
                                 nameXMMReg(gregOfRM(modrm)) );
      }

      putXMMRegLane64F( 
         gregOfRM(modrm), 0,
         unop(Iop_I32StoF64, unop(Iop_64to32, mkexpr(arg64)) )
      );

      putXMMRegLane64F( 
         gregOfRM(modrm), 1,
         unop(Iop_I32StoF64, unop(Iop_64HIto32, mkexpr(arg64)) )
      );

      goto decode_success;
   }

   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0x5B) {
      IRTemp argV  = newTemp(Ity_V128);
      IRTemp rmode = newTemp(Ity_I32);

      modrm = getIByte(delta+2);
      if (epartIsReg(modrm)) {
         assign( argV, getXMMReg(eregOfRM(modrm)) );
         delta += 2+1;
         DIP("cvtps2dq %s,%s\n", nameXMMReg(eregOfRM(modrm)),
                                 nameXMMReg(gregOfRM(modrm)));
      } else {
         addr = disAMode ( &alen, sorb, delta+2, dis_buf );
	 assign( argV, loadLE(Ity_V128, mkexpr(addr)) );
         delta += 2+alen;
         DIP("cvtps2dq %s,%s\n", dis_buf,
                                 nameXMMReg(gregOfRM(modrm)) );
      }
         
      assign( rmode, get_sse_roundingmode() );
      breakup128to32s( argV, &t3, &t2, &t1, &t0 );

#     define CVT(_t)                            \
        binop( Iop_F64toI32S,                   \
               mkexpr(rmode),                   \
               unop( Iop_F32toF64,              \
                     unop( Iop_ReinterpI32asF32, mkexpr(_t))) )
      
      putXMMRegLane32( gregOfRM(modrm), 3, CVT(t3) );
      putXMMRegLane32( gregOfRM(modrm), 2, CVT(t2) );
      putXMMRegLane32( gregOfRM(modrm), 1, CVT(t1) );
      putXMMRegLane32( gregOfRM(modrm), 0, CVT(t0) );

#     undef CVT

      goto decode_success;
   }

   if (sz == 4 && insn[0] == 0x0F && insn[1] == 0x5A) {
      IRTemp f32lo = newTemp(Ity_F32);
      IRTemp f32hi = newTemp(Ity_F32);

      modrm = getIByte(delta+2);
      if (epartIsReg(modrm)) {
         assign( f32lo, getXMMRegLane32F(eregOfRM(modrm), 0) );
         assign( f32hi, getXMMRegLane32F(eregOfRM(modrm), 1) );
         delta += 2+1;
         DIP("cvtps2pd %s,%s\n", nameXMMReg(eregOfRM(modrm)),
                                 nameXMMReg(gregOfRM(modrm)));
      } else {
         addr = disAMode ( &alen, sorb, delta+2, dis_buf );
	 assign( f32lo, loadLE(Ity_F32, mkexpr(addr)) );
	 assign( f32hi, loadLE(Ity_F32, 
                               binop(Iop_Add32,mkexpr(addr),mkU32(4))) );
         delta += 2+alen;
         DIP("cvtps2pd %s,%s\n", dis_buf,
                                 nameXMMReg(gregOfRM(modrm)) );
      }

      putXMMRegLane64F( gregOfRM(modrm), 1,
                        unop(Iop_F32toF64, mkexpr(f32hi)) );
      putXMMRegLane64F( gregOfRM(modrm), 0,
                        unop(Iop_F32toF64, mkexpr(f32lo)) );

      goto decode_success;
   }

   if (insn[0] == 0xF2 && insn[1] == 0x0F 
       && (insn[2] == 0x2D || insn[2] == 0x2C)) {
      IRTemp rmode = newTemp(Ity_I32);
      IRTemp f64lo = newTemp(Ity_F64);
      Bool   r2zero = toBool(insn[2] == 0x2C);
      vassert(sz == 4);

      modrm = getIByte(delta+3);
      if (epartIsReg(modrm)) {
         delta += 3+1;
	 assign(f64lo, getXMMRegLane64F(eregOfRM(modrm), 0));
         DIP("cvt%ssd2si %s,%s\n", r2zero ? "t" : "",
                                   nameXMMReg(eregOfRM(modrm)),
                                   nameIReg(4, gregOfRM(modrm)));
      } else {
         addr = disAMode ( &alen, sorb, delta+3, dis_buf );
	 assign(f64lo, loadLE(Ity_F64, mkexpr(addr)));
         delta += 3+alen;
         DIP("cvt%ssd2si %s,%s\n", r2zero ? "t" : "",
                                   dis_buf,
                                   nameIReg(4, gregOfRM(modrm)));
      }

      if (r2zero) {
         assign( rmode, mkU32((UInt)Irrm_ZERO) );
      } else {
         assign( rmode, get_sse_roundingmode() );
      }

      putIReg(4, gregOfRM(modrm),
                 binop( Iop_F64toI32S, mkexpr(rmode), mkexpr(f64lo)) );

      goto decode_success;
   }

   if (insn[0] == 0xF2 && insn[1] == 0x0F && insn[2] == 0x5A) {
      IRTemp rmode = newTemp(Ity_I32);
      IRTemp f64lo = newTemp(Ity_F64);
      vassert(sz == 4);

      modrm = getIByte(delta+3);
      if (epartIsReg(modrm)) {
         delta += 3+1;
	 assign(f64lo, getXMMRegLane64F(eregOfRM(modrm), 0));
         DIP("cvtsd2ss %s,%s\n", nameXMMReg(eregOfRM(modrm)),
                                 nameXMMReg(gregOfRM(modrm)));
      } else {
         addr = disAMode ( &alen, sorb, delta+3, dis_buf );
	 assign(f64lo, loadLE(Ity_F64, mkexpr(addr)));
         delta += 3+alen;
         DIP("cvtsd2ss %s,%s\n", dis_buf,
                                 nameXMMReg(gregOfRM(modrm)));
      }

      assign( rmode, get_sse_roundingmode() );
      putXMMRegLane32F( 
         gregOfRM(modrm), 0, 
         binop( Iop_F64toF32, mkexpr(rmode), mkexpr(f64lo) )
      );

      goto decode_success;
   }

   if (insn[0] == 0xF2 && insn[1] == 0x0F && insn[2] == 0x2A) {
      IRTemp arg32 = newTemp(Ity_I32);
      vassert(sz == 4);

      modrm = getIByte(delta+3);
      if (epartIsReg(modrm)) {
         assign( arg32, getIReg(4, eregOfRM(modrm)) );
         delta += 3+1;
         DIP("cvtsi2sd %s,%s\n", nameIReg(4, eregOfRM(modrm)),
                                 nameXMMReg(gregOfRM(modrm)));
      } else {
         addr = disAMode ( &alen, sorb, delta+3, dis_buf );
	 assign( arg32, loadLE(Ity_I32, mkexpr(addr)) );
         delta += 3+alen;
         DIP("cvtsi2sd %s,%s\n", dis_buf,
                                 nameXMMReg(gregOfRM(modrm)) );
      }

      putXMMRegLane64F( 
         gregOfRM(modrm), 0,
         unop(Iop_I32StoF64, mkexpr(arg32)) );

      goto decode_success;
   }

   if (insn[0] == 0xF3 && insn[1] == 0x0F && insn[2] == 0x5A) {
      IRTemp f32lo = newTemp(Ity_F32);
      vassert(sz == 4);

      modrm = getIByte(delta+3);
      if (epartIsReg(modrm)) {
         delta += 3+1;
	 assign(f32lo, getXMMRegLane32F(eregOfRM(modrm), 0));
         DIP("cvtss2sd %s,%s\n", nameXMMReg(eregOfRM(modrm)),
                                 nameXMMReg(gregOfRM(modrm)));
      } else {
         addr = disAMode ( &alen, sorb, delta+3, dis_buf );
	 assign(f32lo, loadLE(Ity_F32, mkexpr(addr)));
         delta += 3+alen;
         DIP("cvtss2sd %s,%s\n", dis_buf,
                                 nameXMMReg(gregOfRM(modrm)));
      }

      putXMMRegLane64F( gregOfRM(modrm), 0, 
                        unop( Iop_F32toF64, mkexpr(f32lo) ) );

      goto decode_success;
   }

   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0xE6) {
      IRTemp argV  = newTemp(Ity_V128);
      IRTemp rmode = newTemp(Ity_I32);

      modrm = getIByte(delta+2);
      if (epartIsReg(modrm)) {
         assign( argV, getXMMReg(eregOfRM(modrm)) );
         delta += 2+1;
         DIP("cvttpd2dq %s,%s\n", nameXMMReg(eregOfRM(modrm)),
                                  nameXMMReg(gregOfRM(modrm)));
      } else {
         addr = disAMode ( &alen, sorb, delta+2, dis_buf );
	 assign( argV, loadLE(Ity_V128, mkexpr(addr)) );
         delta += 2+alen;
         DIP("cvttpd2dq %s,%s\n", dis_buf,
                                  nameXMMReg(gregOfRM(modrm)) );
      }

      assign( rmode, mkU32((UInt)Irrm_ZERO) );

      t0 = newTemp(Ity_F64);
      t1 = newTemp(Ity_F64);
      assign( t0, unop(Iop_ReinterpI64asF64, 
                       unop(Iop_V128to64, mkexpr(argV))) );
      assign( t1, unop(Iop_ReinterpI64asF64, 
                       unop(Iop_V128HIto64, mkexpr(argV))) );
      
#     define CVT(_t)  binop( Iop_F64toI32S,                   \
                             mkexpr(rmode),                   \
                             mkexpr(_t) )
      
      putXMMRegLane32( gregOfRM(modrm), 3, mkU32(0) );
      putXMMRegLane32( gregOfRM(modrm), 2, mkU32(0) );
      putXMMRegLane32( gregOfRM(modrm), 1, CVT(t1) );
      putXMMRegLane32( gregOfRM(modrm), 0, CVT(t0) );

#     undef CVT

      goto decode_success;
   }

   if (insn[0] == 0xF3 && insn[1] == 0x0F && insn[2] == 0x5B) {
      IRTemp argV  = newTemp(Ity_V128);
      IRTemp rmode = newTemp(Ity_I32);
      vassert(sz == 4);

      modrm = getIByte(delta+3);
      if (epartIsReg(modrm)) {
         assign( argV, getXMMReg(eregOfRM(modrm)) );
         delta += 3+1;
         DIP("cvttps2dq %s,%s\n", nameXMMReg(eregOfRM(modrm)),
                                  nameXMMReg(gregOfRM(modrm)));
      } else {
         addr = disAMode ( &alen, sorb, delta+3, dis_buf );
	 assign( argV, loadLE(Ity_V128, mkexpr(addr)) );
         delta += 3+alen;
         DIP("cvttps2dq %s,%s\n", dis_buf,
                                  nameXMMReg(gregOfRM(modrm)) );
      }
         
      assign( rmode, mkU32((UInt)Irrm_ZERO) );
      breakup128to32s( argV, &t3, &t2, &t1, &t0 );

#     define CVT(_t)                            \
        binop( Iop_F64toI32S,                   \
               mkexpr(rmode),                   \
               unop( Iop_F32toF64,              \
                     unop( Iop_ReinterpI32asF32, mkexpr(_t))) )
      
      putXMMRegLane32( gregOfRM(modrm), 3, CVT(t3) );
      putXMMRegLane32( gregOfRM(modrm), 2, CVT(t2) );
      putXMMRegLane32( gregOfRM(modrm), 1, CVT(t1) );
      putXMMRegLane32( gregOfRM(modrm), 0, CVT(t0) );

#     undef CVT

      goto decode_success;
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0x5E) {
      delta = dis_SSE_E_to_G_all( sorb, delta+2, "divpd", Iop_Div64Fx2 );
      goto decode_success;
   }

   
   if (insn[0] == 0xF2 && insn[1] == 0x0F && insn[2] == 0x5E) {
      vassert(sz == 4);
      delta = dis_SSE_E_to_G_lo64( sorb, delta+3, "divsd", Iop_Div64F0x2 );
      goto decode_success;
   }

   
   
   if (insn[0] == 0x0F && insn[1] == 0xAE
       && epartIsReg(insn[2]) 
       && (gregOfRM(insn[2]) == 5 || gregOfRM(insn[2]) == 6)) {
      vassert(sz == 4);
      delta += 3;
      stmt( IRStmt_MBE(Imbe_Fence) );
      DIP("%sfence\n", gregOfRM(insn[2])==5 ? "l" : "m");
      goto decode_success;
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0x5F) {
      delta = dis_SSE_E_to_G_all( sorb, delta+2, "maxpd", Iop_Max64Fx2 );
      goto decode_success;
   }

   
   if (insn[0] == 0xF2 && insn[1] == 0x0F && insn[2] == 0x5F) {
      vassert(sz == 4);
      delta = dis_SSE_E_to_G_lo64( sorb, delta+3, "maxsd", Iop_Max64F0x2 );
      goto decode_success;
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0x5D) {
      delta = dis_SSE_E_to_G_all( sorb, delta+2, "minpd", Iop_Min64Fx2 );
      goto decode_success;
   }

   
   if (insn[0] == 0xF2 && insn[1] == 0x0F && insn[2] == 0x5D) {
      vassert(sz == 4);
      delta = dis_SSE_E_to_G_lo64( sorb, delta+3, "minsd", Iop_Min64F0x2 );
      goto decode_success;
   }

   
   
   
   if (sz == 2 && insn[0] == 0x0F 
       && (insn[1] == 0x28 || insn[1] == 0x10 || insn[1] == 0x6F)) {
      const HChar* wot = insn[1]==0x28 ? "apd" :
                         insn[1]==0x10 ? "upd" : "dqa";
      modrm = getIByte(delta+2);
      if (epartIsReg(modrm)) {
         putXMMReg( gregOfRM(modrm), 
                    getXMMReg( eregOfRM(modrm) ));
         DIP("mov%s %s,%s\n", wot, nameXMMReg(eregOfRM(modrm)),
                                   nameXMMReg(gregOfRM(modrm)));
         delta += 2+1;
      } else {
         addr = disAMode ( &alen, sorb, delta+2, dis_buf );
         if (insn[1] == 0x28 || insn[1] == 0x6F)
            gen_SEGV_if_not_16_aligned( addr );
         putXMMReg( gregOfRM(modrm), 
                    loadLE(Ity_V128, mkexpr(addr)) );
         DIP("mov%s %s,%s\n", wot, dis_buf,
                                   nameXMMReg(gregOfRM(modrm)));
         delta += 2+alen;
      }
      goto decode_success;
   }

   
   
   if (sz == 2 && insn[0] == 0x0F 
       && (insn[1] == 0x29 || insn[1] == 0x11)) {
      const HChar* wot = insn[1]==0x29 ? "apd" : "upd";
      modrm = getIByte(delta+2);
      if (epartIsReg(modrm)) {
         
      } else {
         addr = disAMode ( &alen, sorb, delta+2, dis_buf );
         if (insn[1] == 0x29)
            gen_SEGV_if_not_16_aligned( addr );
         storeLE( mkexpr(addr), getXMMReg(gregOfRM(modrm)) );
         DIP("mov%s %s,%s\n", wot, nameXMMReg(gregOfRM(modrm)),
                                   dis_buf );
         delta += 2+alen;
         goto decode_success;
      }
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0x6E) {
      modrm = getIByte(delta+2);
      if (epartIsReg(modrm)) {
         delta += 2+1;
         putXMMReg(
            gregOfRM(modrm),
            unop( Iop_32UtoV128, getIReg(4, eregOfRM(modrm)) ) 
         );
         DIP("movd %s, %s\n", 
             nameIReg(4,eregOfRM(modrm)), nameXMMReg(gregOfRM(modrm)));
      } else {
         addr = disAMode( &alen, sorb, delta+2, dis_buf );
         delta += 2+alen;
         putXMMReg(
            gregOfRM(modrm),
            unop( Iop_32UtoV128,loadLE(Ity_I32, mkexpr(addr)) ) 
         );
         DIP("movd %s, %s\n", dis_buf, nameXMMReg(gregOfRM(modrm)));
      }
      goto decode_success;
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0x7E) {
      modrm = getIByte(delta+2);
      if (epartIsReg(modrm)) {
         delta += 2+1;
         putIReg( 4, eregOfRM(modrm),
                  getXMMRegLane32(gregOfRM(modrm), 0) );
         DIP("movd %s, %s\n", 
             nameXMMReg(gregOfRM(modrm)), nameIReg(4,eregOfRM(modrm)));
      } else {
         addr = disAMode( &alen, sorb, delta+2, dis_buf );
         delta += 2+alen;
         storeLE( mkexpr(addr),
                  getXMMRegLane32(gregOfRM(modrm), 0) );
         DIP("movd %s, %s\n", nameXMMReg(gregOfRM(modrm)), dis_buf);
      }
      goto decode_success;
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0x7F) {
      modrm = getIByte(delta+2);
      if (epartIsReg(modrm)) {
         delta += 2+1;
         putXMMReg( eregOfRM(modrm),
                    getXMMReg(gregOfRM(modrm)) );
         DIP("movdqa %s, %s\n", nameXMMReg(gregOfRM(modrm)), 
                                nameXMMReg(eregOfRM(modrm)));
      } else {
         addr = disAMode( &alen, sorb, delta+2, dis_buf );
         delta += 2+alen;
         gen_SEGV_if_not_16_aligned( addr );
         storeLE( mkexpr(addr), getXMMReg(gregOfRM(modrm)) );
         DIP("movdqa %s, %s\n", nameXMMReg(gregOfRM(modrm)), dis_buf);
      }
      goto decode_success;
   }

   
   if (insn[0] == 0xF3 && insn[1] == 0x0F && insn[2] == 0x6F) {
      vassert(sz == 4);
      modrm = getIByte(delta+3);
      if (epartIsReg(modrm)) {
         putXMMReg( gregOfRM(modrm), 
                    getXMMReg( eregOfRM(modrm) ));
         DIP("movdqu %s,%s\n", nameXMMReg(eregOfRM(modrm)),
                               nameXMMReg(gregOfRM(modrm)));
         delta += 3+1;
      } else {
         addr = disAMode ( &alen, sorb, delta+3, dis_buf );
         putXMMReg( gregOfRM(modrm), 
                    loadLE(Ity_V128, mkexpr(addr)) );
         DIP("movdqu %s,%s\n", dis_buf,
                               nameXMMReg(gregOfRM(modrm)));
         delta += 3+alen;
      }
      goto decode_success;
   }

   
   if (insn[0] == 0xF3 && insn[1] == 0x0F && insn[2] == 0x7F) {
      vassert(sz == 4);
      modrm = getIByte(delta+3);
      if (epartIsReg(modrm)) {
         delta += 3+1;
         putXMMReg( eregOfRM(modrm),
                    getXMMReg(gregOfRM(modrm)) );
         DIP("movdqu %s, %s\n", nameXMMReg(gregOfRM(modrm)), 
                                nameXMMReg(eregOfRM(modrm)));
      } else {
         addr = disAMode( &alen, sorb, delta+3, dis_buf );
         delta += 3+alen;
         storeLE( mkexpr(addr), getXMMReg(gregOfRM(modrm)) );
         DIP("movdqu %s, %s\n", nameXMMReg(gregOfRM(modrm)), dis_buf);
      }
      goto decode_success;
   }

   
   if (insn[0] == 0xF2 && insn[1] == 0x0F && insn[2] == 0xD6) {
      vassert(sz == 4);
      modrm = getIByte(delta+3);
      if (epartIsReg(modrm)) {
         do_MMX_preamble();
         putMMXReg( gregOfRM(modrm), 
                    getXMMRegLane64( eregOfRM(modrm), 0 ));
         DIP("movdq2q %s,%s\n", nameXMMReg(eregOfRM(modrm)),
                                nameMMXReg(gregOfRM(modrm)));
         delta += 3+1;
         goto decode_success;
      } else {
         
      }
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0x16) {
      modrm = getIByte(delta+2);
      if (epartIsReg(modrm)) {
         
      } else {
         addr = disAMode ( &alen, sorb, delta+2, dis_buf );
         delta += 2+alen;
         putXMMRegLane64( gregOfRM(modrm), 1,
                          loadLE(Ity_I64, mkexpr(addr)) );
         DIP("movhpd %s,%s\n", dis_buf, 
                               nameXMMReg( gregOfRM(modrm) ));
         goto decode_success;
      }
   }

   
   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0x17) {
      if (!epartIsReg(insn[2])) {
         delta += 2;
         addr = disAMode ( &alen, sorb, delta, dis_buf );
         delta += alen;
         storeLE( mkexpr(addr), 
                  getXMMRegLane64( gregOfRM(insn[2]),
                                   1 ) );
         DIP("movhpd %s,%s\n", nameXMMReg( gregOfRM(insn[2]) ),
                               dis_buf);
         goto decode_success;
      }
      
   }

   
   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0x12) {
      modrm = getIByte(delta+2);
      if (epartIsReg(modrm)) {
         
      } else {
         addr = disAMode ( &alen, sorb, delta+2, dis_buf );
         delta += 2+alen;
         putXMMRegLane64( gregOfRM(modrm),  0,
                          loadLE(Ity_I64, mkexpr(addr)) );
         DIP("movlpd %s, %s\n", 
             dis_buf, nameXMMReg( gregOfRM(modrm) ));
         goto decode_success;
      }
   }

   
   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0x13) {
      if (!epartIsReg(insn[2])) {
         delta += 2;
         addr = disAMode ( &alen, sorb, delta, dis_buf );
         delta += alen;
         storeLE( mkexpr(addr), 
                  getXMMRegLane64( gregOfRM(insn[2]), 
                                   0 ) );
         DIP("movlpd %s, %s\n", nameXMMReg( gregOfRM(insn[2]) ),
                                dis_buf);
         goto decode_success;
      }
      
   }

   if (insn[0] == 0x0F && insn[1] == 0x50) {
      modrm = getIByte(delta+2);
      if (sz == 2 && epartIsReg(modrm)) {
         Int src;
         t0 = newTemp(Ity_I32);
         t1 = newTemp(Ity_I32);
         delta += 2+1;
         src = eregOfRM(modrm);
         assign( t0, binop( Iop_And32,
                            binop(Iop_Shr32, getXMMRegLane32(src,1), mkU8(31)),
                            mkU32(1) ));
         assign( t1, binop( Iop_And32,
                            binop(Iop_Shr32, getXMMRegLane32(src,3), mkU8(30)),
                            mkU32(2) ));
         putIReg(4, gregOfRM(modrm),
                    binop(Iop_Or32, mkexpr(t0), mkexpr(t1))
                 );
         DIP("movmskpd %s,%s\n", nameXMMReg(src), 
                                 nameIReg(4, gregOfRM(modrm)));
         goto decode_success;
      }
      
   }

   
   if (insn[0] == 0x0F && insn[1] == 0xF7) {
      modrm = getIByte(delta+2);
      if (sz == 2 && epartIsReg(modrm)) {
         IRTemp regD    = newTemp(Ity_V128);
         IRTemp mask    = newTemp(Ity_V128);
         IRTemp olddata = newTemp(Ity_V128);
         IRTemp newdata = newTemp(Ity_V128);
                addr    = newTemp(Ity_I32);

         assign( addr, handleSegOverride( sorb, getIReg(4, R_EDI) ));
         assign( regD, getXMMReg( gregOfRM(modrm) ));

	 assign( 
            mask, 
            binop(Iop_64HLtoV128,
                  binop(Iop_SarN8x8, 
                        getXMMRegLane64( eregOfRM(modrm), 1 ), 
                        mkU8(7) ),
                  binop(Iop_SarN8x8, 
                        getXMMRegLane64( eregOfRM(modrm), 0 ), 
                        mkU8(7) ) ));
         assign( olddata, loadLE( Ity_V128, mkexpr(addr) ));
         assign( newdata, 
                 binop(Iop_OrV128, 
                       binop(Iop_AndV128, 
                             mkexpr(regD), 
                             mkexpr(mask) ),
                       binop(Iop_AndV128, 
                             mkexpr(olddata),
                             unop(Iop_NotV128, mkexpr(mask)))) );
         storeLE( mkexpr(addr), mkexpr(newdata) );

         delta += 2+1;
         DIP("maskmovdqu %s,%s\n", nameXMMReg( eregOfRM(modrm) ),
                                   nameXMMReg( gregOfRM(modrm) ) );
         goto decode_success;
      }
      
   }

   
   if (insn[0] == 0x0F && insn[1] == 0xE7) {
      modrm = getIByte(delta+2);
      if (sz == 2 && !epartIsReg(modrm)) {
         addr = disAMode ( &alen, sorb, delta+2, dis_buf );
         gen_SEGV_if_not_16_aligned( addr );
         storeLE( mkexpr(addr), getXMMReg(gregOfRM(modrm)) );
         DIP("movntdq %s,%s\n", dis_buf,
                                nameXMMReg(gregOfRM(modrm)));
         delta += 2+alen;
         goto decode_success;
      }
      
   }

   
   if (insn[0] == 0x0F && insn[1] == 0xC3) {
      vassert(sz == 4);
      modrm = getIByte(delta+2);
      if (!epartIsReg(modrm)) {
         addr = disAMode ( &alen, sorb, delta+2, dis_buf );
         storeLE( mkexpr(addr), getIReg(4, gregOfRM(modrm)) );
         DIP("movnti %s,%s\n", dis_buf,
                               nameIReg(4, gregOfRM(modrm)));
         delta += 2+alen;
         goto decode_success;
      }
      
   }

   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0xD6) {
      modrm = getIByte(delta+2);
      if (epartIsReg(modrm)) {
         
         
      } else {
         addr = disAMode ( &alen, sorb, delta+2, dis_buf );
         storeLE( mkexpr(addr), 
                  getXMMRegLane64( gregOfRM(modrm), 0 ));
         DIP("movq %s,%s\n", nameXMMReg(gregOfRM(modrm)), dis_buf );
         delta += 2+alen;
         goto decode_success;
      }
   }

   if (insn[0] == 0xF3 && insn[1] == 0x0F && insn[2] == 0xD6) {
      vassert(sz == 4);
      modrm = getIByte(delta+3);
      if (epartIsReg(modrm)) {
         do_MMX_preamble();
         putXMMReg( gregOfRM(modrm), 
                    unop(Iop_64UtoV128, getMMXReg( eregOfRM(modrm) )) );
         DIP("movq2dq %s,%s\n", nameMMXReg(eregOfRM(modrm)),
                                nameXMMReg(gregOfRM(modrm)));
         delta += 3+1;
         goto decode_success;
      } else {
         
      }
   }

   if ((insn[0] == 0xF2 && insn[1] == 0x0F && insn[2] == 0x10)
       || (insn[0] == 0xF3 && insn[1] == 0x0F && insn[2] == 0x7E)) {
      vassert(sz == 4);
      modrm = getIByte(delta+3);
      if (epartIsReg(modrm)) {
         putXMMRegLane64( gregOfRM(modrm), 0,
                          getXMMRegLane64( eregOfRM(modrm), 0 ));
         if (insn[0] == 0xF3) {
            
            putXMMRegLane64( gregOfRM(modrm), 1, mkU64(0) );
         }
         DIP("movsd %s,%s\n", nameXMMReg(eregOfRM(modrm)),
                              nameXMMReg(gregOfRM(modrm)));
         delta += 3+1;
      } else {
         addr = disAMode ( &alen, sorb, delta+3, dis_buf );
         
         putXMMRegLane64( gregOfRM(modrm), 1, mkU64(0) );
         
         putXMMRegLane64( gregOfRM(modrm), 0,
                          loadLE(Ity_I64, mkexpr(addr)) );
         DIP("movsd %s,%s\n", dis_buf,
                              nameXMMReg(gregOfRM(modrm)));
         delta += 3+alen;
      }
      goto decode_success;
   }

   if (insn[0] == 0xF2 && insn[1] == 0x0F && insn[2] == 0x11) {
      vassert(sz == 4);
      modrm = getIByte(delta+3);
      if (epartIsReg(modrm)) {
         putXMMRegLane64( eregOfRM(modrm), 0,
                          getXMMRegLane64( gregOfRM(modrm), 0 ));
         DIP("movsd %s,%s\n", nameXMMReg(gregOfRM(modrm)),
                              nameXMMReg(eregOfRM(modrm)));
         delta += 3+1;
      } else {
         addr = disAMode ( &alen, sorb, delta+3, dis_buf );
         storeLE( mkexpr(addr),
                  getXMMRegLane64(gregOfRM(modrm), 0) );
         DIP("movsd %s,%s\n", nameXMMReg(gregOfRM(modrm)),
                              dis_buf);
         delta += 3+alen;
      }
      goto decode_success;
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0x59) {
      delta = dis_SSE_E_to_G_all( sorb, delta+2, "mulpd", Iop_Mul64Fx2 );
      goto decode_success;
   }

   
   if (insn[0] == 0xF2 && insn[1] == 0x0F && insn[2] == 0x59) {
      vassert(sz == 4);
      delta = dis_SSE_E_to_G_lo64( sorb, delta+3, "mulsd", Iop_Mul64F0x2 );
      goto decode_success;
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0x56) {
      delta = dis_SSE_E_to_G_all( sorb, delta+2, "orpd", Iop_OrV128 );
      goto decode_success;
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0xC6) {
      Int    select;
      IRTemp sV = newTemp(Ity_V128);
      IRTemp dV = newTemp(Ity_V128);
      IRTemp s1 = newTemp(Ity_I64);
      IRTemp s0 = newTemp(Ity_I64);
      IRTemp d1 = newTemp(Ity_I64);
      IRTemp d0 = newTemp(Ity_I64);

      modrm = insn[2];
      assign( dV, getXMMReg(gregOfRM(modrm)) );

      if (epartIsReg(modrm)) {
         assign( sV, getXMMReg(eregOfRM(modrm)) );
         select = (Int)insn[3];
         delta += 2+2;
         DIP("shufpd $%d,%s,%s\n", select, 
                                   nameXMMReg(eregOfRM(modrm)),
                                   nameXMMReg(gregOfRM(modrm)));
      } else {
         addr = disAMode ( &alen, sorb, delta+2, dis_buf );
         assign( sV, loadLE(Ity_V128, mkexpr(addr)) );
         select = (Int)insn[2+alen];
         delta += 3+alen;
         DIP("shufpd $%d,%s,%s\n", select, 
                                   dis_buf,
                                   nameXMMReg(gregOfRM(modrm)));
      }

      assign( d1, unop(Iop_V128HIto64, mkexpr(dV)) );
      assign( d0, unop(Iop_V128to64,   mkexpr(dV)) );
      assign( s1, unop(Iop_V128HIto64, mkexpr(sV)) );
      assign( s0, unop(Iop_V128to64,   mkexpr(sV)) );

#     define SELD(n) mkexpr((n)==0 ? d0 : d1)
#     define SELS(n) mkexpr((n)==0 ? s0 : s1)

      putXMMReg(
         gregOfRM(modrm), 
         binop(Iop_64HLtoV128, SELS((select>>1)&1), SELD((select>>0)&1) )
      );

#     undef SELD
#     undef SELS

      goto decode_success;
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0x51) {
      delta = dis_SSE_E_to_G_unary_all( sorb, delta+2, 
                                        "sqrtpd", Iop_Sqrt64Fx2 );
      goto decode_success;
   }

   
   if (insn[0] == 0xF2 && insn[1] == 0x0F && insn[2] == 0x51) {
      vassert(sz == 4);
      delta = dis_SSE_E_to_G_unary_lo64( sorb, delta+3, 
                                         "sqrtsd", Iop_Sqrt64F0x2 );
      goto decode_success;
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0x5C) {
      delta = dis_SSE_E_to_G_all( sorb, delta+2, "subpd", Iop_Sub64Fx2 );
      goto decode_success;
   }

   
   if (insn[0] == 0xF2 && insn[1] == 0x0F && insn[2] == 0x5C) {
      vassert(sz == 4);
      delta = dis_SSE_E_to_G_lo64( sorb, delta+3, "subsd", Iop_Sub64F0x2 );
      goto decode_success;
   }

   
   
   
   if (sz == 2 && insn[0] == 0x0F && (insn[1] == 0x15 || insn[1] == 0x14)) {
      IRTemp s1 = newTemp(Ity_I64);
      IRTemp s0 = newTemp(Ity_I64);
      IRTemp d1 = newTemp(Ity_I64);
      IRTemp d0 = newTemp(Ity_I64);
      IRTemp sV = newTemp(Ity_V128);
      IRTemp dV = newTemp(Ity_V128);
      Bool   hi = toBool(insn[1] == 0x15);

      modrm = insn[2];
      assign( dV, getXMMReg(gregOfRM(modrm)) );

      if (epartIsReg(modrm)) {
         assign( sV, getXMMReg(eregOfRM(modrm)) );
         delta += 2+1;
         DIP("unpck%sps %s,%s\n", hi ? "h" : "l",
                                  nameXMMReg(eregOfRM(modrm)),
                                  nameXMMReg(gregOfRM(modrm)));
      } else {
         addr = disAMode ( &alen, sorb, delta+2, dis_buf );
         assign( sV, loadLE(Ity_V128, mkexpr(addr)) );
         delta += 2+alen;
         DIP("unpck%sps %s,%s\n", hi ? "h" : "l",
                                  dis_buf,
                                  nameXMMReg(gregOfRM(modrm)));
      }

      assign( d1, unop(Iop_V128HIto64, mkexpr(dV)) );
      assign( d0, unop(Iop_V128to64,   mkexpr(dV)) );
      assign( s1, unop(Iop_V128HIto64, mkexpr(sV)) );
      assign( s0, unop(Iop_V128to64,   mkexpr(sV)) );

      if (hi) {
         putXMMReg( gregOfRM(modrm), 
                    binop(Iop_64HLtoV128, mkexpr(s1), mkexpr(d1)) );
      } else {
         putXMMReg( gregOfRM(modrm), 
                    binop(Iop_64HLtoV128, mkexpr(s0), mkexpr(d0)) );
      }

      goto decode_success;
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0x57) {
      delta = dis_SSE_E_to_G_all( sorb, delta+2, "xorpd", Iop_XorV128 );
      goto decode_success;
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0x6B) {
      delta = dis_SSEint_E_to_G( sorb, delta+2, 
                                 "packssdw",
                                 Iop_QNarrowBin32Sto16Sx8, True );
      goto decode_success;
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0x63) {
      delta = dis_SSEint_E_to_G( sorb, delta+2, 
                                 "packsswb",
                                 Iop_QNarrowBin16Sto8Sx16, True );
      goto decode_success;
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0x67) {
      delta = dis_SSEint_E_to_G( sorb, delta+2, 
                                 "packuswb",
                                 Iop_QNarrowBin16Sto8Ux16, True );
      goto decode_success;
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0xFC) {
      delta = dis_SSEint_E_to_G( sorb, delta+2, 
                                 "paddb", Iop_Add8x16, False );
      goto decode_success;
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0xFE) {
      delta = dis_SSEint_E_to_G( sorb, delta+2, 
                                 "paddd", Iop_Add32x4, False );
      goto decode_success;
   }

   
   
   if (sz == 4 && insn[0] == 0x0F && insn[1] == 0xD4) {
      do_MMX_preamble();
      delta = dis_MMXop_regmem_to_reg ( 
                sorb, delta+2, insn[1], "paddq", False );
      goto decode_success;
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0xD4) {
      delta = dis_SSEint_E_to_G( sorb, delta+2, 
                                 "paddq", Iop_Add64x2, False );
      goto decode_success;
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0xFD) {
      delta = dis_SSEint_E_to_G( sorb, delta+2, 
                                 "paddw", Iop_Add16x8, False );
      goto decode_success;
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0xEC) {
      delta = dis_SSEint_E_to_G( sorb, delta+2, 
                                 "paddsb", Iop_QAdd8Sx16, False );
      goto decode_success;
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0xED) {
      delta = dis_SSEint_E_to_G( sorb, delta+2, 
                                 "paddsw", Iop_QAdd16Sx8, False );
      goto decode_success;
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0xDC) {
      delta = dis_SSEint_E_to_G( sorb, delta+2, 
                                 "paddusb", Iop_QAdd8Ux16, False );
      goto decode_success;
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0xDD) {
      delta = dis_SSEint_E_to_G( sorb, delta+2, 
                                 "paddusw", Iop_QAdd16Ux8, False );
      goto decode_success;
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0xDB) {
      delta = dis_SSE_E_to_G_all( sorb, delta+2, "pand", Iop_AndV128 );
      goto decode_success;
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0xDF) {
      delta = dis_SSE_E_to_G_all_invG( sorb, delta+2, "pandn", Iop_AndV128 );
      goto decode_success;
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0xE0) {
      delta = dis_SSEint_E_to_G( sorb, delta+2, 
                                 "pavgb", Iop_Avg8Ux16, False );
      goto decode_success;
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0xE3) {
      delta = dis_SSEint_E_to_G( sorb, delta+2, 
                                 "pavgw", Iop_Avg16Ux8, False );
      goto decode_success;
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0x74) {
      delta = dis_SSEint_E_to_G( sorb, delta+2, 
                                 "pcmpeqb", Iop_CmpEQ8x16, False );
      goto decode_success;
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0x76) {
      delta = dis_SSEint_E_to_G( sorb, delta+2, 
                                 "pcmpeqd", Iop_CmpEQ32x4, False );
      goto decode_success;
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0x75) {
      delta = dis_SSEint_E_to_G( sorb, delta+2, 
                                 "pcmpeqw", Iop_CmpEQ16x8, False );
      goto decode_success;
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0x64) {
      delta = dis_SSEint_E_to_G( sorb, delta+2, 
                                 "pcmpgtb", Iop_CmpGT8Sx16, False );
      goto decode_success;
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0x66) {
      delta = dis_SSEint_E_to_G( sorb, delta+2, 
                                 "pcmpgtd", Iop_CmpGT32Sx4, False );
      goto decode_success;
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0x65) {
      delta = dis_SSEint_E_to_G( sorb, delta+2, 
                                 "pcmpgtw", Iop_CmpGT16Sx8, False );
      goto decode_success;
   }

   if (insn[0] == 0x0F && insn[1] == 0xC5) {
      modrm = insn[2];
      if (sz == 2 && epartIsReg(modrm)) {
         t5 = newTemp(Ity_V128);
         t4 = newTemp(Ity_I16);
         assign(t5, getXMMReg(eregOfRM(modrm)));
         breakup128to32s( t5, &t3, &t2, &t1, &t0 );
         switch (insn[3] & 7) {
            case 0:  assign(t4, unop(Iop_32to16,   mkexpr(t0))); break;
            case 1:  assign(t4, unop(Iop_32HIto16, mkexpr(t0))); break;
            case 2:  assign(t4, unop(Iop_32to16,   mkexpr(t1))); break;
            case 3:  assign(t4, unop(Iop_32HIto16, mkexpr(t1))); break;
            case 4:  assign(t4, unop(Iop_32to16,   mkexpr(t2))); break;
            case 5:  assign(t4, unop(Iop_32HIto16, mkexpr(t2))); break;
            case 6:  assign(t4, unop(Iop_32to16,   mkexpr(t3))); break;
            case 7:  assign(t4, unop(Iop_32HIto16, mkexpr(t3))); break;
            default: vassert(0); 
         }
         putIReg(4, gregOfRM(modrm), unop(Iop_16Uto32, mkexpr(t4)));
         DIP("pextrw $%d,%s,%s\n",
             (Int)insn[3], nameXMMReg(eregOfRM(modrm)),
                           nameIReg(4,gregOfRM(modrm)));
         delta += 4;
         goto decode_success;
      } 
      
   }

   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0xC4) {
      Int lane;
      t4 = newTemp(Ity_I16);
      modrm = insn[2];

      if (epartIsReg(modrm)) {
         assign(t4, getIReg(2, eregOfRM(modrm)));
         delta += 3+1;
         lane = insn[3+1-1];
         DIP("pinsrw $%d,%s,%s\n", (Int)lane, 
                                   nameIReg(2,eregOfRM(modrm)),
                                   nameXMMReg(gregOfRM(modrm)));
      } else {
         addr = disAMode ( &alen, sorb, delta+2, dis_buf );
         delta += 3+alen;
         lane = insn[3+alen-1];
         assign(t4, loadLE(Ity_I16, mkexpr(addr)));
         DIP("pinsrw $%d,%s,%s\n", (Int)lane, 
                                   dis_buf,
                                   nameXMMReg(gregOfRM(modrm)));
      }

      putXMMRegLane16( gregOfRM(modrm), lane & 7, mkexpr(t4) );
      goto decode_success;
   }

   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0xF5) {
      IRTemp s1V  = newTemp(Ity_V128);
      IRTemp s2V  = newTemp(Ity_V128);
      IRTemp dV   = newTemp(Ity_V128);
      IRTemp s1Hi = newTemp(Ity_I64);
      IRTemp s1Lo = newTemp(Ity_I64);
      IRTemp s2Hi = newTemp(Ity_I64);
      IRTemp s2Lo = newTemp(Ity_I64);
      IRTemp dHi  = newTemp(Ity_I64);
      IRTemp dLo  = newTemp(Ity_I64);
      modrm = insn[2];
      if (epartIsReg(modrm)) {
         assign( s1V, getXMMReg(eregOfRM(modrm)) );
         delta += 2+1;
         DIP("pmaddwd %s,%s\n", nameXMMReg(eregOfRM(modrm)),
                                nameXMMReg(gregOfRM(modrm)));
      } else {
         addr = disAMode ( &alen, sorb, delta+2, dis_buf );
         assign( s1V, loadLE(Ity_V128, mkexpr(addr)) );
         delta += 2+alen;
         DIP("pmaddwd %s,%s\n", dis_buf,
                                nameXMMReg(gregOfRM(modrm)));
      }
      assign( s2V, getXMMReg(gregOfRM(modrm)) );
      assign( s1Hi, unop(Iop_V128HIto64, mkexpr(s1V)) );
      assign( s1Lo, unop(Iop_V128to64,   mkexpr(s1V)) );
      assign( s2Hi, unop(Iop_V128HIto64, mkexpr(s2V)) );
      assign( s2Lo, unop(Iop_V128to64,   mkexpr(s2V)) );
      assign( dHi, mkIRExprCCall(
                      Ity_I64, 0,
                      "x86g_calculate_mmx_pmaddwd", 
                      &x86g_calculate_mmx_pmaddwd,
                      mkIRExprVec_2( mkexpr(s1Hi), mkexpr(s2Hi))
                   ));
      assign( dLo, mkIRExprCCall(
                      Ity_I64, 0,
                      "x86g_calculate_mmx_pmaddwd", 
                      &x86g_calculate_mmx_pmaddwd,
                      mkIRExprVec_2( mkexpr(s1Lo), mkexpr(s2Lo))
                   ));
      assign( dV, binop(Iop_64HLtoV128, mkexpr(dHi), mkexpr(dLo))) ;
      putXMMReg(gregOfRM(modrm), mkexpr(dV));
      goto decode_success;
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0xEE) {
      delta = dis_SSEint_E_to_G( sorb, delta+2, 
                                 "pmaxsw", Iop_Max16Sx8, False );
      goto decode_success;
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0xDE) {
      delta = dis_SSEint_E_to_G( sorb, delta+2, 
                                 "pmaxub", Iop_Max8Ux16, False );
      goto decode_success;
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0xEA) {
      delta = dis_SSEint_E_to_G( sorb, delta+2, 
                                 "pminsw", Iop_Min16Sx8, False );
      goto decode_success;
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0xDA) {
      delta = dis_SSEint_E_to_G( sorb, delta+2, 
                                 "pminub", Iop_Min8Ux16, False );
      goto decode_success;
   }

   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0xD7) {
      modrm = insn[2];
      if (epartIsReg(modrm)) {
         t0 = newTemp(Ity_I64);
         t1 = newTemp(Ity_I64);
         assign(t0, getXMMRegLane64(eregOfRM(modrm), 0));
         assign(t1, getXMMRegLane64(eregOfRM(modrm), 1));
         t5 = newTemp(Ity_I32);
         assign(t5,
                unop(Iop_16Uto32,
                     binop(Iop_8HLto16,
                           unop(Iop_GetMSBs8x8, mkexpr(t1)),
                           unop(Iop_GetMSBs8x8, mkexpr(t0)))));
         putIReg(4, gregOfRM(modrm), mkexpr(t5));
         DIP("pmovmskb %s,%s\n", nameXMMReg(eregOfRM(modrm)),
                                 nameIReg(4,gregOfRM(modrm)));
         delta += 3;
         goto decode_success;
      } 
      
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0xE4) {
      delta = dis_SSEint_E_to_G( sorb, delta+2, 
                                 "pmulhuw", Iop_MulHi16Ux8, False );
      goto decode_success;
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0xE5) {
      delta = dis_SSEint_E_to_G( sorb, delta+2, 
                                 "pmulhw", Iop_MulHi16Sx8, False );
      goto decode_success;
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0xD5) {
      delta = dis_SSEint_E_to_G( sorb, delta+2, 
                                 "pmullw", Iop_Mul16x8, False );
      goto decode_success;
   }

   
   if (sz == 4 && insn[0] == 0x0F && insn[1] == 0xF4) {
      IRTemp sV = newTemp(Ity_I64);
      IRTemp dV = newTemp(Ity_I64);
      t1 = newTemp(Ity_I32);
      t0 = newTemp(Ity_I32);
      modrm = insn[2];

      do_MMX_preamble();
      assign( dV, getMMXReg(gregOfRM(modrm)) );

      if (epartIsReg(modrm)) {
         assign( sV, getMMXReg(eregOfRM(modrm)) );
         delta += 2+1;
         DIP("pmuludq %s,%s\n", nameMMXReg(eregOfRM(modrm)),
                                nameMMXReg(gregOfRM(modrm)));
      } else {
         addr = disAMode ( &alen, sorb, delta+2, dis_buf );
         assign( sV, loadLE(Ity_I64, mkexpr(addr)) );
         delta += 2+alen;
         DIP("pmuludq %s,%s\n", dis_buf,
                                nameMMXReg(gregOfRM(modrm)));
      }

      assign( t0, unop(Iop_64to32, mkexpr(dV)) );
      assign( t1, unop(Iop_64to32, mkexpr(sV)) );
      putMMXReg( gregOfRM(modrm),
                 binop( Iop_MullU32, mkexpr(t0), mkexpr(t1) ) );
      goto decode_success;
   }

   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0xF4) {
      IRTemp sV, dV;
      IRTemp s3, s2, s1, s0, d3, d2, d1, d0;
      sV = newTemp(Ity_V128);
      dV = newTemp(Ity_V128);
      s3 = s2 = s1 = s0 = d3 = d2 = d1 = d0 = IRTemp_INVALID;
      t1 = newTemp(Ity_I64);
      t0 = newTemp(Ity_I64);
      modrm = insn[2];
      assign( dV, getXMMReg(gregOfRM(modrm)) );

      if (epartIsReg(modrm)) {
         assign( sV, getXMMReg(eregOfRM(modrm)) );
         delta += 2+1;
         DIP("pmuludq %s,%s\n", nameXMMReg(eregOfRM(modrm)),
                                nameXMMReg(gregOfRM(modrm)));
      } else {
         addr = disAMode ( &alen, sorb, delta+2, dis_buf );
         assign( sV, loadLE(Ity_V128, mkexpr(addr)) );
         delta += 2+alen;
         DIP("pmuludq %s,%s\n", dis_buf,
                                nameXMMReg(gregOfRM(modrm)));
      }

      breakup128to32s( dV, &d3, &d2, &d1, &d0 );
      breakup128to32s( sV, &s3, &s2, &s1, &s0 );

      assign( t0, binop( Iop_MullU32, mkexpr(d0), mkexpr(s0)) );
      putXMMRegLane64( gregOfRM(modrm), 0, mkexpr(t0) );
      assign( t1, binop( Iop_MullU32, mkexpr(d2), mkexpr(s2)) );
      putXMMRegLane64( gregOfRM(modrm), 1, mkexpr(t1) );
      goto decode_success;
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0xEB) {
      delta = dis_SSE_E_to_G_all( sorb, delta+2, "por", Iop_OrV128 );
      goto decode_success;
   }

   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0xF6) {
      IRTemp s1V  = newTemp(Ity_V128);
      IRTemp s2V  = newTemp(Ity_V128);
      IRTemp dV   = newTemp(Ity_V128);
      IRTemp s1Hi = newTemp(Ity_I64);
      IRTemp s1Lo = newTemp(Ity_I64);
      IRTemp s2Hi = newTemp(Ity_I64);
      IRTemp s2Lo = newTemp(Ity_I64);
      IRTemp dHi  = newTemp(Ity_I64);
      IRTemp dLo  = newTemp(Ity_I64);
      modrm = insn[2];
      if (epartIsReg(modrm)) {
         assign( s1V, getXMMReg(eregOfRM(modrm)) );
         delta += 2+1;
         DIP("psadbw %s,%s\n", nameXMMReg(eregOfRM(modrm)),
                               nameXMMReg(gregOfRM(modrm)));
      } else {
         addr = disAMode ( &alen, sorb, delta+2, dis_buf );
         assign( s1V, loadLE(Ity_V128, mkexpr(addr)) );
         delta += 2+alen;
         DIP("psadbw %s,%s\n", dis_buf,
                               nameXMMReg(gregOfRM(modrm)));
      }
      assign( s2V, getXMMReg(gregOfRM(modrm)) );
      assign( s1Hi, unop(Iop_V128HIto64, mkexpr(s1V)) );
      assign( s1Lo, unop(Iop_V128to64,   mkexpr(s1V)) );
      assign( s2Hi, unop(Iop_V128HIto64, mkexpr(s2V)) );
      assign( s2Lo, unop(Iop_V128to64,   mkexpr(s2V)) );
      assign( dHi, mkIRExprCCall(
                      Ity_I64, 0,
                      "x86g_calculate_mmx_psadbw", 
                      &x86g_calculate_mmx_psadbw,
                      mkIRExprVec_2( mkexpr(s1Hi), mkexpr(s2Hi))
                   ));
      assign( dLo, mkIRExprCCall(
                      Ity_I64, 0,
                      "x86g_calculate_mmx_psadbw", 
                      &x86g_calculate_mmx_psadbw,
                      mkIRExprVec_2( mkexpr(s1Lo), mkexpr(s2Lo))
                   ));
      assign( dV, binop(Iop_64HLtoV128, mkexpr(dHi), mkexpr(dLo))) ;
      putXMMReg(gregOfRM(modrm), mkexpr(dV));
      goto decode_success;
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0x70) {
      Int order;
      IRTemp sV, dV, s3, s2, s1, s0;
      s3 = s2 = s1 = s0 = IRTemp_INVALID;
      sV = newTemp(Ity_V128);
      dV = newTemp(Ity_V128);
      modrm = insn[2];
      if (epartIsReg(modrm)) {
         assign( sV, getXMMReg(eregOfRM(modrm)) );
         order = (Int)insn[3];
         delta += 2+2;
         DIP("pshufd $%d,%s,%s\n", order, 
                                   nameXMMReg(eregOfRM(modrm)),
                                   nameXMMReg(gregOfRM(modrm)));
      } else {
         addr = disAMode ( &alen, sorb, delta+2, dis_buf );
         assign( sV, loadLE(Ity_V128, mkexpr(addr)) );
	 order = (Int)insn[2+alen];
         delta += 3+alen;
         DIP("pshufd $%d,%s,%s\n", order, 
                                   dis_buf,
                                   nameXMMReg(gregOfRM(modrm)));
      }
      breakup128to32s( sV, &s3, &s2, &s1, &s0 );

#     define SEL(n) \
                ((n)==0 ? s0 : ((n)==1 ? s1 : ((n)==2 ? s2 : s3)))
      assign(dV,
	     mk128from32s( SEL((order>>6)&3), SEL((order>>4)&3),
                           SEL((order>>2)&3), SEL((order>>0)&3) )
      );
      putXMMReg(gregOfRM(modrm), mkexpr(dV));
#     undef SEL
      goto decode_success;
   }

   if (insn[0] == 0xF3 && insn[1] == 0x0F && insn[2] == 0x70) {
      Int order;
      IRTemp sVhi, dVhi, sV, dV, s3, s2, s1, s0;
      s3 = s2 = s1 = s0 = IRTemp_INVALID;
      sV   = newTemp(Ity_V128);
      dV   = newTemp(Ity_V128);
      sVhi = newTemp(Ity_I64);
      dVhi = newTemp(Ity_I64);
      modrm = insn[3];
      if (epartIsReg(modrm)) {
         assign( sV, getXMMReg(eregOfRM(modrm)) );
         order = (Int)insn[4];
         delta += 4+1;
         DIP("pshufhw $%d,%s,%s\n", order, 
                                    nameXMMReg(eregOfRM(modrm)),
                                    nameXMMReg(gregOfRM(modrm)));
      } else {
         addr = disAMode ( &alen, sorb, delta+3, dis_buf );
         assign( sV, loadLE(Ity_V128, mkexpr(addr)) );
	 order = (Int)insn[3+alen];
         delta += 4+alen;
         DIP("pshufhw $%d,%s,%s\n", order, 
                                    dis_buf,
                                    nameXMMReg(gregOfRM(modrm)));
      }
      assign( sVhi, unop(Iop_V128HIto64, mkexpr(sV)) );
      breakup64to16s( sVhi, &s3, &s2, &s1, &s0 );

#     define SEL(n) \
                ((n)==0 ? s0 : ((n)==1 ? s1 : ((n)==2 ? s2 : s3)))
      assign(dVhi,
	     mk64from16s( SEL((order>>6)&3), SEL((order>>4)&3),
                          SEL((order>>2)&3), SEL((order>>0)&3) )
      );
      assign(dV, binop( Iop_64HLtoV128, 
                        mkexpr(dVhi),
                        unop(Iop_V128to64, mkexpr(sV))) );
      putXMMReg(gregOfRM(modrm), mkexpr(dV));
#     undef SEL
      goto decode_success;
   }

   if (insn[0] == 0xF2 && insn[1] == 0x0F && insn[2] == 0x70) {
      Int order;
      IRTemp sVlo, dVlo, sV, dV, s3, s2, s1, s0;
      s3 = s2 = s1 = s0 = IRTemp_INVALID;
      sV   = newTemp(Ity_V128);
      dV   = newTemp(Ity_V128);
      sVlo = newTemp(Ity_I64);
      dVlo = newTemp(Ity_I64);
      modrm = insn[3];
      if (epartIsReg(modrm)) {
         assign( sV, getXMMReg(eregOfRM(modrm)) );
         order = (Int)insn[4];
         delta += 4+1;
         DIP("pshuflw $%d,%s,%s\n", order, 
                                    nameXMMReg(eregOfRM(modrm)),
                                    nameXMMReg(gregOfRM(modrm)));
      } else {
         addr = disAMode ( &alen, sorb, delta+3, dis_buf );
         assign( sV, loadLE(Ity_V128, mkexpr(addr)) );
	 order = (Int)insn[3+alen];
         delta += 4+alen;
         DIP("pshuflw $%d,%s,%s\n", order, 
                                    dis_buf,
                                    nameXMMReg(gregOfRM(modrm)));
      }
      assign( sVlo, unop(Iop_V128to64, mkexpr(sV)) );
      breakup64to16s( sVlo, &s3, &s2, &s1, &s0 );

#     define SEL(n) \
                ((n)==0 ? s0 : ((n)==1 ? s1 : ((n)==2 ? s2 : s3)))
      assign(dVlo,
	     mk64from16s( SEL((order>>6)&3), SEL((order>>4)&3),
                          SEL((order>>2)&3), SEL((order>>0)&3) )
      );
      assign(dV, binop( Iop_64HLtoV128,
                        unop(Iop_V128HIto64, mkexpr(sV)),
                        mkexpr(dVlo) ) );
      putXMMReg(gregOfRM(modrm), mkexpr(dV));
#     undef SEL
      goto decode_success;
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0x72
       && epartIsReg(insn[2])
       && gregOfRM(insn[2]) == 6) {
      delta = dis_SSE_shiftE_imm( delta+2, "pslld", Iop_ShlN32x4 );
      goto decode_success;
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0xF2) {
      delta = dis_SSE_shiftG_byE( sorb, delta+2, "pslld", Iop_ShlN32x4 );
      goto decode_success;
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0x73
       && epartIsReg(insn[2])
       && gregOfRM(insn[2]) == 7) {
      IRTemp sV, dV, hi64, lo64, hi64r, lo64r;
      Int    imm = (Int)insn[3];
      Int    reg = eregOfRM(insn[2]);
      DIP("pslldq $%d,%s\n", imm, nameXMMReg(reg));
      vassert(imm >= 0 && imm <= 255);
      delta += 4;

      sV    = newTemp(Ity_V128);
      dV    = newTemp(Ity_V128);
      hi64  = newTemp(Ity_I64);
      lo64  = newTemp(Ity_I64);
      hi64r = newTemp(Ity_I64);
      lo64r = newTemp(Ity_I64);

      if (imm >= 16) {
         putXMMReg(reg, mkV128(0x0000));
         goto decode_success;
      }

      assign( sV, getXMMReg(reg) );
      assign( hi64, unop(Iop_V128HIto64, mkexpr(sV)) );
      assign( lo64, unop(Iop_V128to64, mkexpr(sV)) );

      if (imm == 0) {
         assign( lo64r, mkexpr(lo64) );
         assign( hi64r, mkexpr(hi64) );
      }
      else
      if (imm == 8) {
         assign( lo64r, mkU64(0) );
         assign( hi64r, mkexpr(lo64) );
      }
      else
      if (imm > 8) {
         assign( lo64r, mkU64(0) );
         assign( hi64r, binop( Iop_Shl64, 
                               mkexpr(lo64),
                               mkU8( 8*(imm-8) ) ));
      } else {
         assign( lo64r, binop( Iop_Shl64, 
                               mkexpr(lo64),
                               mkU8(8 * imm) ));
         assign( hi64r, 
                 binop( Iop_Or64,
                        binop(Iop_Shl64, mkexpr(hi64), 
                                         mkU8(8 * imm)),
                        binop(Iop_Shr64, mkexpr(lo64),
                                         mkU8(8 * (8 - imm)) )
                      )
               );
      }
      assign( dV, binop(Iop_64HLtoV128, mkexpr(hi64r), mkexpr(lo64r)) );
      putXMMReg(reg, mkexpr(dV));
      goto decode_success;
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0x73
       && epartIsReg(insn[2])
       && gregOfRM(insn[2]) == 6) {
      delta = dis_SSE_shiftE_imm( delta+2, "psllq", Iop_ShlN64x2 );
      goto decode_success;
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0xF3) {
      delta = dis_SSE_shiftG_byE( sorb, delta+2, "psllq", Iop_ShlN64x2 );
      goto decode_success;
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0x71
       && epartIsReg(insn[2])
       && gregOfRM(insn[2]) == 6) {
      delta = dis_SSE_shiftE_imm( delta+2, "psllw", Iop_ShlN16x8 );
      goto decode_success;
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0xF1) {
      delta = dis_SSE_shiftG_byE( sorb, delta+2, "psllw", Iop_ShlN16x8 );
      goto decode_success;
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0x72
       && epartIsReg(insn[2])
       && gregOfRM(insn[2]) == 4) {
      delta = dis_SSE_shiftE_imm( delta+2, "psrad", Iop_SarN32x4 );
      goto decode_success;
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0xE2) {
      delta = dis_SSE_shiftG_byE( sorb, delta+2, "psrad", Iop_SarN32x4 );
      goto decode_success;
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0x71
       && epartIsReg(insn[2])
       && gregOfRM(insn[2]) == 4) {
      delta = dis_SSE_shiftE_imm( delta+2, "psraw", Iop_SarN16x8 );
      goto decode_success;
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0xE1) {
      delta = dis_SSE_shiftG_byE( sorb, delta+2, "psraw", Iop_SarN16x8 );
      goto decode_success;
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0x72
       && epartIsReg(insn[2])
       && gregOfRM(insn[2]) == 2) {
      delta = dis_SSE_shiftE_imm( delta+2, "psrld", Iop_ShrN32x4 );
      goto decode_success;
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0xD2) {
      delta = dis_SSE_shiftG_byE( sorb, delta+2, "psrld", Iop_ShrN32x4 );
      goto decode_success;
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0x73
       && epartIsReg(insn[2])
       && gregOfRM(insn[2]) == 3) {
      IRTemp sV, dV, hi64, lo64, hi64r, lo64r;
      Int    imm = (Int)insn[3];
      Int    reg = eregOfRM(insn[2]);
      DIP("psrldq $%d,%s\n", imm, nameXMMReg(reg));
      vassert(imm >= 0 && imm <= 255);
      delta += 4;

      sV    = newTemp(Ity_V128);
      dV    = newTemp(Ity_V128);
      hi64  = newTemp(Ity_I64);
      lo64  = newTemp(Ity_I64);
      hi64r = newTemp(Ity_I64);
      lo64r = newTemp(Ity_I64);

      if (imm >= 16) {
         putXMMReg(reg, mkV128(0x0000));
         goto decode_success;
      }

      assign( sV, getXMMReg(reg) );
      assign( hi64, unop(Iop_V128HIto64, mkexpr(sV)) );
      assign( lo64, unop(Iop_V128to64, mkexpr(sV)) );

      if (imm == 0) {
         assign( lo64r, mkexpr(lo64) );
         assign( hi64r, mkexpr(hi64) );
      }
      else
      if (imm == 8) {
         assign( hi64r, mkU64(0) );
         assign( lo64r, mkexpr(hi64) );
      }
      else 
      if (imm > 8) {
         assign( hi64r, mkU64(0) );
         assign( lo64r, binop( Iop_Shr64, 
                               mkexpr(hi64),
                               mkU8( 8*(imm-8) ) ));
      } else {
         assign( hi64r, binop( Iop_Shr64, 
                               mkexpr(hi64),
                               mkU8(8 * imm) ));
         assign( lo64r, 
                 binop( Iop_Or64,
                        binop(Iop_Shr64, mkexpr(lo64), 
                                         mkU8(8 * imm)),
                        binop(Iop_Shl64, mkexpr(hi64),
                                         mkU8(8 * (8 - imm)) )
                      )
               );
      }

      assign( dV, binop(Iop_64HLtoV128, mkexpr(hi64r), mkexpr(lo64r)) );
      putXMMReg(reg, mkexpr(dV));
      goto decode_success;
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0x73
       && epartIsReg(insn[2])
       && gregOfRM(insn[2]) == 2) {
      delta = dis_SSE_shiftE_imm( delta+2, "psrlq", Iop_ShrN64x2 );
      goto decode_success;
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0xD3) {
      delta = dis_SSE_shiftG_byE( sorb, delta+2, "psrlq", Iop_ShrN64x2 );
      goto decode_success;
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0x71
       && epartIsReg(insn[2])
       && gregOfRM(insn[2]) == 2) {
      delta = dis_SSE_shiftE_imm( delta+2, "psrlw", Iop_ShrN16x8 );
      goto decode_success;
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0xD1) {
      delta = dis_SSE_shiftG_byE( sorb, delta+2, "psrlw", Iop_ShrN16x8 );
      goto decode_success;
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0xF8) {
      delta = dis_SSEint_E_to_G( sorb, delta+2, 
                                 "psubb", Iop_Sub8x16, False );
      goto decode_success;
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0xFA) {
      delta = dis_SSEint_E_to_G( sorb, delta+2, 
                                 "psubd", Iop_Sub32x4, False );
      goto decode_success;
   }

   
   
   if (sz == 4 && insn[0] == 0x0F && insn[1] == 0xFB) {
      do_MMX_preamble();
      delta = dis_MMXop_regmem_to_reg ( 
                sorb, delta+2, insn[1], "psubq", False );
      goto decode_success;
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0xFB) {
      delta = dis_SSEint_E_to_G( sorb, delta+2, 
                                 "psubq", Iop_Sub64x2, False );
      goto decode_success;
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0xF9) {
      delta = dis_SSEint_E_to_G( sorb, delta+2, 
                                 "psubw", Iop_Sub16x8, False );
      goto decode_success;
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0xE8) {
      delta = dis_SSEint_E_to_G( sorb, delta+2, 
                                 "psubsb", Iop_QSub8Sx16, False );
      goto decode_success;
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0xE9) {
      delta = dis_SSEint_E_to_G( sorb, delta+2, 
                                 "psubsw", Iop_QSub16Sx8, False );
      goto decode_success;
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0xD8) {
      delta = dis_SSEint_E_to_G( sorb, delta+2, 
                                 "psubusb", Iop_QSub8Ux16, False );
      goto decode_success;
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0xD9) {
      delta = dis_SSEint_E_to_G( sorb, delta+2, 
                                 "psubusw", Iop_QSub16Ux8, False );
      goto decode_success;
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0x68) {
      delta = dis_SSEint_E_to_G( sorb, delta+2, 
                                 "punpckhbw",
                                 Iop_InterleaveHI8x16, True );
      goto decode_success;
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0x6A) {
      delta = dis_SSEint_E_to_G( sorb, delta+2, 
                                 "punpckhdq",
                                 Iop_InterleaveHI32x4, True );
      goto decode_success;
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0x6D) {
      delta = dis_SSEint_E_to_G( sorb, delta+2, 
                                 "punpckhqdq",
                                 Iop_InterleaveHI64x2, True );
      goto decode_success;
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0x69) {
      delta = dis_SSEint_E_to_G( sorb, delta+2, 
                                 "punpckhwd",
                                 Iop_InterleaveHI16x8, True );
      goto decode_success;
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0x60) {
      delta = dis_SSEint_E_to_G( sorb, delta+2, 
                                 "punpcklbw",
                                 Iop_InterleaveLO8x16, True );
      goto decode_success;
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0x62) {
      delta = dis_SSEint_E_to_G( sorb, delta+2, 
                                 "punpckldq",
                                 Iop_InterleaveLO32x4, True );
      goto decode_success;
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0x6C) {
      delta = dis_SSEint_E_to_G( sorb, delta+2, 
                                 "punpcklqdq",
                                 Iop_InterleaveLO64x2, True );
      goto decode_success;
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0x61) {
      delta = dis_SSEint_E_to_G( sorb, delta+2, 
                                 "punpcklwd",
                                 Iop_InterleaveLO16x8, True );
      goto decode_success;
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0xEF) {
      delta = dis_SSE_E_to_G_all( sorb, delta+2, "pxor", Iop_XorV128 );
      goto decode_success;
   }


   
   if (sz == 4 && insn[0] == 0x0F && insn[1] == 0xAE
       && !epartIsReg(insn[2]) && gregOfRM(insn[2]) == 7) {

      UInt lineszB = 256;

      addr = disAMode ( &alen, sorb, delta+2, dis_buf );
      delta += 2+alen;

      
      stmt( IRStmt_Put(
               OFFB_CMSTART,
               binop( Iop_And32, 
                      mkexpr(addr), 
                      mkU32( ~(lineszB-1) ))) );

      stmt( IRStmt_Put(OFFB_CMLEN, mkU32(lineszB) ) );

      jmp_lit(&dres, Ijk_InvalICache, (Addr32)(guest_EIP_bbstart+delta));

      DIP("clflush %s\n", dis_buf);
      goto decode_success;
   }

   
   
   

   
   
   

   if (0 == (archinfo->hwcaps & VEX_HWCAPS_X86_SSE3))
      goto after_sse_decoders; 

   insn = &guest_code[delta];

   if (sz == 4 && insn[0] == 0xF3 && insn[1] == 0x0F 
       && (insn[2] == 0x12 || insn[2] == 0x16)) {
      IRTemp s3, s2, s1, s0;
      IRTemp sV  = newTemp(Ity_V128);
      Bool   isH = insn[2] == 0x16;
      s3 = s2 = s1 = s0 = IRTemp_INVALID;

      modrm = insn[3];
      if (epartIsReg(modrm)) {
         assign( sV, getXMMReg( eregOfRM(modrm)) );
         DIP("movs%cdup %s,%s\n", isH ? 'h' : 'l',
                                  nameXMMReg(eregOfRM(modrm)),
                                  nameXMMReg(gregOfRM(modrm)));
         delta += 3+1;
      } else {
         addr = disAMode ( &alen, sorb, delta+3, dis_buf );
         gen_SEGV_if_not_16_aligned( addr );
         assign( sV, loadLE(Ity_V128, mkexpr(addr)) );
         DIP("movs%cdup %s,%s\n", isH ? 'h' : 'l',
	     dis_buf,
             nameXMMReg(gregOfRM(modrm)));
         delta += 3+alen;
      }

      breakup128to32s( sV, &s3, &s2, &s1, &s0 );
      putXMMReg( gregOfRM(modrm), 
                 isH ? mk128from32s( s3, s3, s1, s1 )
                     : mk128from32s( s2, s2, s0, s0 ) );
      goto decode_success;
   }

   if (sz == 4 && insn[0] == 0xF2 && insn[1] == 0x0F && insn[2] == 0x12) {
      IRTemp sV = newTemp(Ity_V128);
      IRTemp d0 = newTemp(Ity_I64);

      modrm = insn[3];
      if (epartIsReg(modrm)) {
         assign( sV, getXMMReg( eregOfRM(modrm)) );
         DIP("movddup %s,%s\n", nameXMMReg(eregOfRM(modrm)),
                                nameXMMReg(gregOfRM(modrm)));
         delta += 3+1;
         assign ( d0, unop(Iop_V128to64, mkexpr(sV)) );
      } else {
         addr = disAMode ( &alen, sorb, delta+3, dis_buf );
         assign( d0, loadLE(Ity_I64, mkexpr(addr)) );
         DIP("movddup %s,%s\n", dis_buf,
                                nameXMMReg(gregOfRM(modrm)));
         delta += 3+alen;
      }

      putXMMReg( gregOfRM(modrm), binop(Iop_64HLtoV128,mkexpr(d0),mkexpr(d0)) );
      goto decode_success;
   }

   
   if (sz == 4 && insn[0] == 0xF2 && insn[1] == 0x0F && insn[2] == 0xD0) {
      IRTemp a3, a2, a1, a0, s3, s2, s1, s0;
      IRTemp eV   = newTemp(Ity_V128);
      IRTemp gV   = newTemp(Ity_V128);
      IRTemp addV = newTemp(Ity_V128);
      IRTemp subV = newTemp(Ity_V128);
      IRTemp rm     = newTemp(Ity_I32);
      a3 = a2 = a1 = a0 = s3 = s2 = s1 = s0 = IRTemp_INVALID;

      modrm = insn[3];
      if (epartIsReg(modrm)) {
         assign( eV, getXMMReg( eregOfRM(modrm)) );
         DIP("addsubps %s,%s\n", nameXMMReg(eregOfRM(modrm)),
                                 nameXMMReg(gregOfRM(modrm)));
         delta += 3+1;
      } else {
         addr = disAMode ( &alen, sorb, delta+3, dis_buf );
         assign( eV, loadLE(Ity_V128, mkexpr(addr)) );
         DIP("addsubps %s,%s\n", dis_buf,
                                 nameXMMReg(gregOfRM(modrm)));
         delta += 3+alen;
      }

      assign( gV, getXMMReg(gregOfRM(modrm)) );

      assign( rm, get_FAKE_roundingmode() ); 
      assign( addV, triop(Iop_Add32Fx4, mkexpr(rm), mkexpr(gV), mkexpr(eV)) );
      assign( subV, triop(Iop_Sub32Fx4, mkexpr(rm), mkexpr(gV), mkexpr(eV)) );

      breakup128to32s( addV, &a3, &a2, &a1, &a0 );
      breakup128to32s( subV, &s3, &s2, &s1, &s0 );

      putXMMReg( gregOfRM(modrm), mk128from32s( a3, s2, a1, s0 ));
      goto decode_success;
   }

   
   if (sz == 2 && insn[0] == 0x0F && insn[1] == 0xD0) {
      IRTemp eV   = newTemp(Ity_V128);
      IRTemp gV   = newTemp(Ity_V128);
      IRTemp addV = newTemp(Ity_V128);
      IRTemp subV = newTemp(Ity_V128);
      IRTemp a1     = newTemp(Ity_I64);
      IRTemp s0     = newTemp(Ity_I64);
      IRTemp rm     = newTemp(Ity_I32);

      modrm = insn[2];
      if (epartIsReg(modrm)) {
         assign( eV, getXMMReg( eregOfRM(modrm)) );
         DIP("addsubpd %s,%s\n", nameXMMReg(eregOfRM(modrm)),
                                 nameXMMReg(gregOfRM(modrm)));
         delta += 2+1;
      } else {
         addr = disAMode ( &alen, sorb, delta+2, dis_buf );
         assign( eV, loadLE(Ity_V128, mkexpr(addr)) );
         DIP("addsubpd %s,%s\n", dis_buf,
                                 nameXMMReg(gregOfRM(modrm)));
         delta += 2+alen;
      }

      assign( gV, getXMMReg(gregOfRM(modrm)) );

      assign( rm, get_FAKE_roundingmode() ); 
      assign( addV, triop(Iop_Add64Fx2, mkexpr(rm), mkexpr(gV), mkexpr(eV)) );
      assign( subV, triop(Iop_Sub64Fx2, mkexpr(rm), mkexpr(gV), mkexpr(eV)) );

      assign( a1, unop(Iop_V128HIto64, mkexpr(addV) ));
      assign( s0, unop(Iop_V128to64,   mkexpr(subV) ));

      putXMMReg( gregOfRM(modrm), 
                 binop(Iop_64HLtoV128, mkexpr(a1), mkexpr(s0)) );
      goto decode_success;
   }

   
   
   if (sz == 4 && insn[0] == 0xF2 && insn[1] == 0x0F 
       && (insn[2] == 0x7C || insn[2] == 0x7D)) {
      IRTemp e3, e2, e1, e0, g3, g2, g1, g0;
      IRTemp eV     = newTemp(Ity_V128);
      IRTemp gV     = newTemp(Ity_V128);
      IRTemp leftV  = newTemp(Ity_V128);
      IRTemp rightV = newTemp(Ity_V128);
      IRTemp rm     = newTemp(Ity_I32);
      Bool   isAdd  = insn[2] == 0x7C;
      const HChar* str = isAdd ? "add" : "sub";
      e3 = e2 = e1 = e0 = g3 = g2 = g1 = g0 = IRTemp_INVALID;

      modrm = insn[3];
      if (epartIsReg(modrm)) {
         assign( eV, getXMMReg( eregOfRM(modrm)) );
         DIP("h%sps %s,%s\n", str, nameXMMReg(eregOfRM(modrm)),
                                   nameXMMReg(gregOfRM(modrm)));
         delta += 3+1;
      } else {
         addr = disAMode ( &alen, sorb, delta+3, dis_buf );
         assign( eV, loadLE(Ity_V128, mkexpr(addr)) );
         DIP("h%sps %s,%s\n", str, dis_buf,
                                   nameXMMReg(gregOfRM(modrm)));
         delta += 3+alen;
      }

      assign( gV, getXMMReg(gregOfRM(modrm)) );

      breakup128to32s( eV, &e3, &e2, &e1, &e0 );
      breakup128to32s( gV, &g3, &g2, &g1, &g0 );

      assign( leftV,  mk128from32s( e2, e0, g2, g0 ) );
      assign( rightV, mk128from32s( e3, e1, g3, g1 ) );

      assign( rm, get_FAKE_roundingmode() ); 
      putXMMReg( gregOfRM(modrm), 
                 triop(isAdd ? Iop_Add32Fx4 : Iop_Sub32Fx4, 
                       mkexpr(rm), mkexpr(leftV), mkexpr(rightV) ) );
      goto decode_success;
   }

   
   
   if (sz == 2 && insn[0] == 0x0F && (insn[1] == 0x7C || insn[1] == 0x7D)) {
      IRTemp e1     = newTemp(Ity_I64);
      IRTemp e0     = newTemp(Ity_I64);
      IRTemp g1     = newTemp(Ity_I64);
      IRTemp g0     = newTemp(Ity_I64);
      IRTemp eV     = newTemp(Ity_V128);
      IRTemp gV     = newTemp(Ity_V128);
      IRTemp leftV  = newTemp(Ity_V128);
      IRTemp rightV = newTemp(Ity_V128);
      IRTemp rm     = newTemp(Ity_I32);
      Bool   isAdd  = insn[1] == 0x7C;
      const HChar* str = isAdd ? "add" : "sub";

      modrm = insn[2];
      if (epartIsReg(modrm)) {
         assign( eV, getXMMReg( eregOfRM(modrm)) );
         DIP("h%spd %s,%s\n", str, nameXMMReg(eregOfRM(modrm)),
                                   nameXMMReg(gregOfRM(modrm)));
         delta += 2+1;
      } else {
         addr = disAMode ( &alen, sorb, delta+2, dis_buf );
         assign( eV, loadLE(Ity_V128, mkexpr(addr)) );
         DIP("h%spd %s,%s\n", str, dis_buf,
                              nameXMMReg(gregOfRM(modrm)));
         delta += 2+alen;
      }

      assign( gV, getXMMReg(gregOfRM(modrm)) );

      assign( e1, unop(Iop_V128HIto64, mkexpr(eV) ));
      assign( e0, unop(Iop_V128to64, mkexpr(eV) ));
      assign( g1, unop(Iop_V128HIto64, mkexpr(gV) ));
      assign( g0, unop(Iop_V128to64, mkexpr(gV) ));

      assign( leftV,  binop(Iop_64HLtoV128, mkexpr(e0),mkexpr(g0)) );
      assign( rightV, binop(Iop_64HLtoV128, mkexpr(e1),mkexpr(g1)) );

      assign( rm, get_FAKE_roundingmode() ); 
      putXMMReg( gregOfRM(modrm), 
                 triop(isAdd ? Iop_Add64Fx2 : Iop_Sub64Fx2, 
                       mkexpr(rm), mkexpr(leftV), mkexpr(rightV) ) );
      goto decode_success;
   }

   
   if (sz == 4 && insn[0] == 0xF2 && insn[1] == 0x0F && insn[2] == 0xF0) {
      modrm = getIByte(delta+3);
      if (epartIsReg(modrm)) {
         goto decode_failure;
      } else {
         addr = disAMode ( &alen, sorb, delta+3, dis_buf );
         putXMMReg( gregOfRM(modrm), 
                    loadLE(Ity_V128, mkexpr(addr)) );
         DIP("lddqu %s,%s\n", dis_buf,
                              nameXMMReg(gregOfRM(modrm)));
         delta += 3+alen;
      }
      goto decode_success;
   }

   
   
   

   
   
   

   if (sz == 4
       && insn[0] == 0x0F && insn[1] == 0x38 && insn[2] == 0x04) {
      IRTemp sV        = newTemp(Ity_I64);
      IRTemp dV        = newTemp(Ity_I64);
      IRTemp sVoddsSX  = newTemp(Ity_I64);
      IRTemp sVevensSX = newTemp(Ity_I64);
      IRTemp dVoddsZX  = newTemp(Ity_I64);
      IRTemp dVevensZX = newTemp(Ity_I64);

      modrm = insn[3];
      do_MMX_preamble();
      assign( dV, getMMXReg(gregOfRM(modrm)) );

      if (epartIsReg(modrm)) {
         assign( sV, getMMXReg(eregOfRM(modrm)) );
         delta += 3+1;
         DIP("pmaddubsw %s,%s\n", nameMMXReg(eregOfRM(modrm)),
                                  nameMMXReg(gregOfRM(modrm)));
      } else {
         addr = disAMode ( &alen, sorb, delta+3, dis_buf );
         assign( sV, loadLE(Ity_I64, mkexpr(addr)) );
         delta += 3+alen;
         DIP("pmaddubsw %s,%s\n", dis_buf,
                                  nameMMXReg(gregOfRM(modrm)));
      }

      
      assign( sVoddsSX,
              binop(Iop_SarN16x4, mkexpr(sV), mkU8(8)) );
      assign( sVevensSX,
              binop(Iop_SarN16x4, 
                    binop(Iop_ShlN16x4, mkexpr(sV), mkU8(8)), 
                    mkU8(8)) );
      assign( dVoddsZX,
              binop(Iop_ShrN16x4, mkexpr(dV), mkU8(8)) );
      assign( dVevensZX,
              binop(Iop_ShrN16x4,
                    binop(Iop_ShlN16x4, mkexpr(dV), mkU8(8)),
                    mkU8(8)) );

      putMMXReg(
         gregOfRM(modrm),
         binop(Iop_QAdd16Sx4,
               binop(Iop_Mul16x4, mkexpr(sVoddsSX), mkexpr(dVoddsZX)),
               binop(Iop_Mul16x4, mkexpr(sVevensSX), mkexpr(dVevensZX))
         )
      );
      goto decode_success;
   }

   if (sz == 2
       && insn[0] == 0x0F && insn[1] == 0x38 && insn[2] == 0x04) {
      IRTemp sV        = newTemp(Ity_V128);
      IRTemp dV        = newTemp(Ity_V128);
      IRTemp sVoddsSX  = newTemp(Ity_V128);
      IRTemp sVevensSX = newTemp(Ity_V128);
      IRTemp dVoddsZX  = newTemp(Ity_V128);
      IRTemp dVevensZX = newTemp(Ity_V128);

      modrm = insn[3];
      assign( dV, getXMMReg(gregOfRM(modrm)) );

      if (epartIsReg(modrm)) {
         assign( sV, getXMMReg(eregOfRM(modrm)) );
         delta += 3+1;
         DIP("pmaddubsw %s,%s\n", nameXMMReg(eregOfRM(modrm)),
                                  nameXMMReg(gregOfRM(modrm)));
      } else {
         addr = disAMode ( &alen, sorb, delta+3, dis_buf );
         gen_SEGV_if_not_16_aligned( addr );
         assign( sV, loadLE(Ity_V128, mkexpr(addr)) );
         delta += 3+alen;
         DIP("pmaddubsw %s,%s\n", dis_buf,
                                  nameXMMReg(gregOfRM(modrm)));
      }

      
      assign( sVoddsSX,
              binop(Iop_SarN16x8, mkexpr(sV), mkU8(8)) );
      assign( sVevensSX,
              binop(Iop_SarN16x8, 
                    binop(Iop_ShlN16x8, mkexpr(sV), mkU8(8)), 
                    mkU8(8)) );
      assign( dVoddsZX,
              binop(Iop_ShrN16x8, mkexpr(dV), mkU8(8)) );
      assign( dVevensZX,
              binop(Iop_ShrN16x8,
                    binop(Iop_ShlN16x8, mkexpr(dV), mkU8(8)),
                    mkU8(8)) );

      putXMMReg(
         gregOfRM(modrm),
         binop(Iop_QAdd16Sx8,
               binop(Iop_Mul16x8, mkexpr(sVoddsSX), mkexpr(dVoddsZX)),
               binop(Iop_Mul16x8, mkexpr(sVevensSX), mkexpr(dVevensZX))
         )
      );
      goto decode_success;
   }

   

   if (sz == 4 
       && insn[0] == 0x0F && insn[1] == 0x38 
       && (insn[2] == 0x03 || insn[2] == 0x07 || insn[2] == 0x01
           || insn[2] == 0x05 || insn[2] == 0x02 || insn[2] == 0x06)) {
      const HChar* str = "???";
      IROp   opV64  = Iop_INVALID;
      IROp   opCatO = Iop_CatOddLanes16x4;
      IROp   opCatE = Iop_CatEvenLanes16x4;
      IRTemp sV     = newTemp(Ity_I64);
      IRTemp dV     = newTemp(Ity_I64);

      modrm = insn[3];

      switch (insn[2]) {
         case 0x03: opV64 = Iop_QAdd16Sx4; str = "addsw"; break;
         case 0x07: opV64 = Iop_QSub16Sx4; str = "subsw"; break;
         case 0x01: opV64 = Iop_Add16x4;   str = "addw";  break;
         case 0x05: opV64 = Iop_Sub16x4;   str = "subw";  break;
         case 0x02: opV64 = Iop_Add32x2;   str = "addd";  break;
         case 0x06: opV64 = Iop_Sub32x2;   str = "subd";  break;
         default: vassert(0);
      }
      if (insn[2] == 0x02 || insn[2] == 0x06) {
         opCatO = Iop_InterleaveHI32x2;
         opCatE = Iop_InterleaveLO32x2;
      }

      do_MMX_preamble();
      assign( dV, getMMXReg(gregOfRM(modrm)) );

      if (epartIsReg(modrm)) {
         assign( sV, getMMXReg(eregOfRM(modrm)) );
         delta += 3+1;
         DIP("ph%s %s,%s\n", str, nameMMXReg(eregOfRM(modrm)),
                                  nameMMXReg(gregOfRM(modrm)));
      } else {
         addr = disAMode ( &alen, sorb, delta+3, dis_buf );
         assign( sV, loadLE(Ity_I64, mkexpr(addr)) );
         delta += 3+alen;
         DIP("ph%s %s,%s\n", str, dis_buf,
                                  nameMMXReg(gregOfRM(modrm)));
      }

      putMMXReg(
         gregOfRM(modrm),
         binop(opV64,
               binop(opCatE,mkexpr(sV),mkexpr(dV)),
               binop(opCatO,mkexpr(sV),mkexpr(dV))
         )
      );
      goto decode_success;
   }


   if (sz == 2
       && insn[0] == 0x0F && insn[1] == 0x38 
       && (insn[2] == 0x03 || insn[2] == 0x07 || insn[2] == 0x01
           || insn[2] == 0x05 || insn[2] == 0x02 || insn[2] == 0x06)) {
      const HChar* str = "???";
      IROp   opV64  = Iop_INVALID;
      IROp   opCatO = Iop_CatOddLanes16x4;
      IROp   opCatE = Iop_CatEvenLanes16x4;
      IRTemp sV     = newTemp(Ity_V128);
      IRTemp dV     = newTemp(Ity_V128);
      IRTemp sHi    = newTemp(Ity_I64);
      IRTemp sLo    = newTemp(Ity_I64);
      IRTemp dHi    = newTemp(Ity_I64);
      IRTemp dLo    = newTemp(Ity_I64);

      modrm = insn[3];

      switch (insn[2]) {
         case 0x03: opV64 = Iop_QAdd16Sx4; str = "addsw"; break;
         case 0x07: opV64 = Iop_QSub16Sx4; str = "subsw"; break;
         case 0x01: opV64 = Iop_Add16x4;   str = "addw";  break;
         case 0x05: opV64 = Iop_Sub16x4;   str = "subw";  break;
         case 0x02: opV64 = Iop_Add32x2;   str = "addd";  break;
         case 0x06: opV64 = Iop_Sub32x2;   str = "subd";  break;
         default: vassert(0);
      }
      if (insn[2] == 0x02 || insn[2] == 0x06) {
         opCatO = Iop_InterleaveHI32x2;
         opCatE = Iop_InterleaveLO32x2;
      }

      assign( dV, getXMMReg(gregOfRM(modrm)) );

      if (epartIsReg(modrm)) {
         assign( sV, getXMMReg( eregOfRM(modrm)) );
         DIP("ph%s %s,%s\n", str, nameXMMReg(eregOfRM(modrm)),
                                  nameXMMReg(gregOfRM(modrm)));
         delta += 3+1;
      } else {
         addr = disAMode ( &alen, sorb, delta+3, dis_buf );
         gen_SEGV_if_not_16_aligned( addr );
         assign( sV, loadLE(Ity_V128, mkexpr(addr)) );
         DIP("ph%s %s,%s\n", str, dis_buf,
                             nameXMMReg(gregOfRM(modrm)));
         delta += 3+alen;
      }

      assign( dHi, unop(Iop_V128HIto64, mkexpr(dV)) );
      assign( dLo, unop(Iop_V128to64,   mkexpr(dV)) );
      assign( sHi, unop(Iop_V128HIto64, mkexpr(sV)) );
      assign( sLo, unop(Iop_V128to64,   mkexpr(sV)) );

      putXMMReg(
         gregOfRM(modrm), 
         binop(Iop_64HLtoV128,
               binop(opV64,
                     binop(opCatE,mkexpr(sHi),mkexpr(sLo)),
                     binop(opCatO,mkexpr(sHi),mkexpr(sLo))
               ),
               binop(opV64,
                     binop(opCatE,mkexpr(dHi),mkexpr(dLo)),
                     binop(opCatO,mkexpr(dHi),mkexpr(dLo))
               )
         )
      );
      goto decode_success;
   }

   if (sz == 4 
       && insn[0] == 0x0F && insn[1] == 0x38 && insn[2] == 0x0B) {
      IRTemp sV = newTemp(Ity_I64);
      IRTemp dV = newTemp(Ity_I64);

      modrm = insn[3];
      do_MMX_preamble();
      assign( dV, getMMXReg(gregOfRM(modrm)) );

      if (epartIsReg(modrm)) {
         assign( sV, getMMXReg(eregOfRM(modrm)) );
         delta += 3+1;
         DIP("pmulhrsw %s,%s\n", nameMMXReg(eregOfRM(modrm)),
                                 nameMMXReg(gregOfRM(modrm)));
      } else {
         addr = disAMode ( &alen, sorb, delta+3, dis_buf );
         assign( sV, loadLE(Ity_I64, mkexpr(addr)) );
         delta += 3+alen;
         DIP("pmulhrsw %s,%s\n", dis_buf,
                                 nameMMXReg(gregOfRM(modrm)));
      }

      putMMXReg(
         gregOfRM(modrm),
         dis_PMULHRSW_helper( mkexpr(sV), mkexpr(dV) )
      );
      goto decode_success;
   }

   if (sz == 2
       && insn[0] == 0x0F && insn[1] == 0x38 && insn[2] == 0x0B) {
      IRTemp sV  = newTemp(Ity_V128);
      IRTemp dV  = newTemp(Ity_V128);
      IRTemp sHi = newTemp(Ity_I64);
      IRTemp sLo = newTemp(Ity_I64);
      IRTemp dHi = newTemp(Ity_I64);
      IRTemp dLo = newTemp(Ity_I64);

      modrm = insn[3];
      assign( dV, getXMMReg(gregOfRM(modrm)) );

      if (epartIsReg(modrm)) {
         assign( sV, getXMMReg(eregOfRM(modrm)) );
         delta += 3+1;
         DIP("pmulhrsw %s,%s\n", nameXMMReg(eregOfRM(modrm)),
                                 nameXMMReg(gregOfRM(modrm)));
      } else {
         addr = disAMode ( &alen, sorb, delta+3, dis_buf );
         gen_SEGV_if_not_16_aligned( addr );
         assign( sV, loadLE(Ity_V128, mkexpr(addr)) );
         delta += 3+alen;
         DIP("pmulhrsw %s,%s\n", dis_buf,
                                 nameXMMReg(gregOfRM(modrm)));
      }

      assign( dHi, unop(Iop_V128HIto64, mkexpr(dV)) );
      assign( dLo, unop(Iop_V128to64,   mkexpr(dV)) );
      assign( sHi, unop(Iop_V128HIto64, mkexpr(sV)) );
      assign( sLo, unop(Iop_V128to64,   mkexpr(sV)) );

      putXMMReg(
         gregOfRM(modrm),
         binop(Iop_64HLtoV128,
               dis_PMULHRSW_helper( mkexpr(sHi), mkexpr(dHi) ),
               dis_PMULHRSW_helper( mkexpr(sLo), mkexpr(dLo) )
         )
      );
      goto decode_success;
   }

   
   
   
   if (sz == 4 
       && insn[0] == 0x0F && insn[1] == 0x38 
       && (insn[2] == 0x08 || insn[2] == 0x09 || insn[2] == 0x0A)) {
      IRTemp sV      = newTemp(Ity_I64);
      IRTemp dV      = newTemp(Ity_I64);
      const HChar* str = "???";
      Int    laneszB = 0;

      switch (insn[2]) {
         case 0x08: laneszB = 1; str = "b"; break;
         case 0x09: laneszB = 2; str = "w"; break;
         case 0x0A: laneszB = 4; str = "d"; break;
         default: vassert(0);
      }

      modrm = insn[3];
      do_MMX_preamble();
      assign( dV, getMMXReg(gregOfRM(modrm)) );

      if (epartIsReg(modrm)) {
         assign( sV, getMMXReg(eregOfRM(modrm)) );
         delta += 3+1;
         DIP("psign%s %s,%s\n", str, nameMMXReg(eregOfRM(modrm)),
                                     nameMMXReg(gregOfRM(modrm)));
      } else {
         addr = disAMode ( &alen, sorb, delta+3, dis_buf );
         assign( sV, loadLE(Ity_I64, mkexpr(addr)) );
         delta += 3+alen;
         DIP("psign%s %s,%s\n", str, dis_buf,
                                     nameMMXReg(gregOfRM(modrm)));
      }

      putMMXReg(
         gregOfRM(modrm),
         dis_PSIGN_helper( mkexpr(sV), mkexpr(dV), laneszB )
      );
      goto decode_success;
   }

   
   
   
   if (sz == 2
       && insn[0] == 0x0F && insn[1] == 0x38 
       && (insn[2] == 0x08 || insn[2] == 0x09 || insn[2] == 0x0A)) {
      IRTemp sV      = newTemp(Ity_V128);
      IRTemp dV      = newTemp(Ity_V128);
      IRTemp sHi     = newTemp(Ity_I64);
      IRTemp sLo     = newTemp(Ity_I64);
      IRTemp dHi     = newTemp(Ity_I64);
      IRTemp dLo     = newTemp(Ity_I64);
      const HChar* str = "???";
      Int    laneszB = 0;

      switch (insn[2]) {
         case 0x08: laneszB = 1; str = "b"; break;
         case 0x09: laneszB = 2; str = "w"; break;
         case 0x0A: laneszB = 4; str = "d"; break;
         default: vassert(0);
      }

      modrm = insn[3];
      assign( dV, getXMMReg(gregOfRM(modrm)) );

      if (epartIsReg(modrm)) {
         assign( sV, getXMMReg(eregOfRM(modrm)) );
         delta += 3+1;
         DIP("psign%s %s,%s\n", str, nameXMMReg(eregOfRM(modrm)),
                                     nameXMMReg(gregOfRM(modrm)));
      } else {
         addr = disAMode ( &alen, sorb, delta+3, dis_buf );
         gen_SEGV_if_not_16_aligned( addr );
         assign( sV, loadLE(Ity_V128, mkexpr(addr)) );
         delta += 3+alen;
         DIP("psign%s %s,%s\n", str, dis_buf,
                                     nameXMMReg(gregOfRM(modrm)));
      }

      assign( dHi, unop(Iop_V128HIto64, mkexpr(dV)) );
      assign( dLo, unop(Iop_V128to64,   mkexpr(dV)) );
      assign( sHi, unop(Iop_V128HIto64, mkexpr(sV)) );
      assign( sLo, unop(Iop_V128to64,   mkexpr(sV)) );

      putXMMReg(
         gregOfRM(modrm),
         binop(Iop_64HLtoV128,
               dis_PSIGN_helper( mkexpr(sHi), mkexpr(dHi), laneszB ),
               dis_PSIGN_helper( mkexpr(sLo), mkexpr(dLo), laneszB )
         )
      );
      goto decode_success;
   }

   
   
   
   if (sz == 4 
       && insn[0] == 0x0F && insn[1] == 0x38 
       && (insn[2] == 0x1C || insn[2] == 0x1D || insn[2] == 0x1E)) {
      IRTemp sV      = newTemp(Ity_I64);
      const HChar* str = "???";
      Int    laneszB = 0;

      switch (insn[2]) {
         case 0x1C: laneszB = 1; str = "b"; break;
         case 0x1D: laneszB = 2; str = "w"; break;
         case 0x1E: laneszB = 4; str = "d"; break;
         default: vassert(0);
      }

      modrm = insn[3];
      do_MMX_preamble();

      if (epartIsReg(modrm)) {
         assign( sV, getMMXReg(eregOfRM(modrm)) );
         delta += 3+1;
         DIP("pabs%s %s,%s\n", str, nameMMXReg(eregOfRM(modrm)),
                                    nameMMXReg(gregOfRM(modrm)));
      } else {
         addr = disAMode ( &alen, sorb, delta+3, dis_buf );
         assign( sV, loadLE(Ity_I64, mkexpr(addr)) );
         delta += 3+alen;
         DIP("pabs%s %s,%s\n", str, dis_buf,
                                    nameMMXReg(gregOfRM(modrm)));
      }

      putMMXReg(
         gregOfRM(modrm),
         dis_PABS_helper( mkexpr(sV), laneszB )
      );
      goto decode_success;
   }

   
   
   
   if (sz == 2
       && insn[0] == 0x0F && insn[1] == 0x38 
       && (insn[2] == 0x1C || insn[2] == 0x1D || insn[2] == 0x1E)) {
      IRTemp sV      = newTemp(Ity_V128);
      IRTemp sHi     = newTemp(Ity_I64);
      IRTemp sLo     = newTemp(Ity_I64);
      const HChar* str = "???";
      Int    laneszB = 0;

      switch (insn[2]) {
         case 0x1C: laneszB = 1; str = "b"; break;
         case 0x1D: laneszB = 2; str = "w"; break;
         case 0x1E: laneszB = 4; str = "d"; break;
         default: vassert(0);
      }

      modrm = insn[3];

      if (epartIsReg(modrm)) {
         assign( sV, getXMMReg(eregOfRM(modrm)) );
         delta += 3+1;
         DIP("pabs%s %s,%s\n", str, nameXMMReg(eregOfRM(modrm)),
                                    nameXMMReg(gregOfRM(modrm)));
      } else {
         addr = disAMode ( &alen, sorb, delta+3, dis_buf );
         gen_SEGV_if_not_16_aligned( addr );
         assign( sV, loadLE(Ity_V128, mkexpr(addr)) );
         delta += 3+alen;
         DIP("pabs%s %s,%s\n", str, dis_buf,
                                    nameXMMReg(gregOfRM(modrm)));
      }

      assign( sHi, unop(Iop_V128HIto64, mkexpr(sV)) );
      assign( sLo, unop(Iop_V128to64,   mkexpr(sV)) );

      putXMMReg(
         gregOfRM(modrm),
         binop(Iop_64HLtoV128,
               dis_PABS_helper( mkexpr(sHi), laneszB ),
               dis_PABS_helper( mkexpr(sLo), laneszB )
         )
      );
      goto decode_success;
   }

   
   if (sz == 4 
       && insn[0] == 0x0F && insn[1] == 0x3A && insn[2] == 0x0F) {
      IRTemp sV  = newTemp(Ity_I64);
      IRTemp dV  = newTemp(Ity_I64);
      IRTemp res = newTemp(Ity_I64);

      modrm = insn[3];
      do_MMX_preamble();
      assign( dV, getMMXReg(gregOfRM(modrm)) );

      if (epartIsReg(modrm)) {
         assign( sV, getMMXReg(eregOfRM(modrm)) );
         d32 = (UInt)insn[3+1];
         delta += 3+1+1;
         DIP("palignr $%d,%s,%s\n",  (Int)d32, 
                                     nameMMXReg(eregOfRM(modrm)),
                                     nameMMXReg(gregOfRM(modrm)));
      } else {
         addr = disAMode ( &alen, sorb, delta+3, dis_buf );
         assign( sV, loadLE(Ity_I64, mkexpr(addr)) );
         d32 = (UInt)insn[3+alen];
         delta += 3+alen+1;
         DIP("palignr $%d%s,%s\n", (Int)d32,
                                   dis_buf,
                                   nameMMXReg(gregOfRM(modrm)));
      }

      if (d32 == 0) {
         assign( res, mkexpr(sV) );
      }
      else if (d32 >= 1 && d32 <= 7) {
         assign(res, 
                binop(Iop_Or64,
                      binop(Iop_Shr64, mkexpr(sV), mkU8(8*d32)),
                      binop(Iop_Shl64, mkexpr(dV), mkU8(8*(8-d32))
                     )));
      }
      else if (d32 == 8) {
        assign( res, mkexpr(dV) );
      }
      else if (d32 >= 9 && d32 <= 15) {
         assign( res, binop(Iop_Shr64, mkexpr(dV), mkU8(8*(d32-8))) );
      }
      else if (d32 >= 16 && d32 <= 255) {
         assign( res, mkU64(0) );
      }
      else
         vassert(0);

      putMMXReg( gregOfRM(modrm), mkexpr(res) );
      goto decode_success;
   }

   
   if (sz == 2
       && insn[0] == 0x0F && insn[1] == 0x3A && insn[2] == 0x0F) {
      IRTemp sV  = newTemp(Ity_V128);
      IRTemp dV  = newTemp(Ity_V128);
      IRTemp sHi = newTemp(Ity_I64);
      IRTemp sLo = newTemp(Ity_I64);
      IRTemp dHi = newTemp(Ity_I64);
      IRTemp dLo = newTemp(Ity_I64);
      IRTemp rHi = newTemp(Ity_I64);
      IRTemp rLo = newTemp(Ity_I64);

      modrm = insn[3];
      assign( dV, getXMMReg(gregOfRM(modrm)) );

      if (epartIsReg(modrm)) {
         assign( sV, getXMMReg(eregOfRM(modrm)) );
         d32 = (UInt)insn[3+1];
         delta += 3+1+1;
         DIP("palignr $%d,%s,%s\n", (Int)d32,
                                    nameXMMReg(eregOfRM(modrm)),
                                    nameXMMReg(gregOfRM(modrm)));
      } else {
         addr = disAMode ( &alen, sorb, delta+3, dis_buf );
         gen_SEGV_if_not_16_aligned( addr );
         assign( sV, loadLE(Ity_V128, mkexpr(addr)) );
         d32 = (UInt)insn[3+alen];
         delta += 3+alen+1;
         DIP("palignr $%d,%s,%s\n", (Int)d32,
                                    dis_buf,
                                    nameXMMReg(gregOfRM(modrm)));
      }

      assign( dHi, unop(Iop_V128HIto64, mkexpr(dV)) );
      assign( dLo, unop(Iop_V128to64,   mkexpr(dV)) );
      assign( sHi, unop(Iop_V128HIto64, mkexpr(sV)) );
      assign( sLo, unop(Iop_V128to64,   mkexpr(sV)) );

      if (d32 == 0) {
         assign( rHi, mkexpr(sHi) );
         assign( rLo, mkexpr(sLo) );
      }
      else if (d32 >= 1 && d32 <= 7) {
         assign( rHi, dis_PALIGNR_XMM_helper(dLo, sHi, d32) );
         assign( rLo, dis_PALIGNR_XMM_helper(sHi, sLo, d32) );
      }
      else if (d32 == 8) {
         assign( rHi, mkexpr(dLo) );
         assign( rLo, mkexpr(sHi) );
      }
      else if (d32 >= 9 && d32 <= 15) {
         assign( rHi, dis_PALIGNR_XMM_helper(dHi, dLo, d32-8) );
         assign( rLo, dis_PALIGNR_XMM_helper(dLo, sHi, d32-8) );
      }
      else if (d32 == 16) {
         assign( rHi, mkexpr(dHi) );
         assign( rLo, mkexpr(dLo) );
      }
      else if (d32 >= 17 && d32 <= 23) {
         assign( rHi, binop(Iop_Shr64, mkexpr(dHi), mkU8(8*(d32-16))) );
         assign( rLo, dis_PALIGNR_XMM_helper(dHi, dLo, d32-16) );
      }
      else if (d32 == 24) {
         assign( rHi, mkU64(0) );
         assign( rLo, mkexpr(dHi) );
      }
      else if (d32 >= 25 && d32 <= 31) {
         assign( rHi, mkU64(0) );
         assign( rLo, binop(Iop_Shr64, mkexpr(dHi), mkU8(8*(d32-24))) );
      }
      else if (d32 >= 32 && d32 <= 255) {
         assign( rHi, mkU64(0) );
         assign( rLo, mkU64(0) );
      }
      else
         vassert(0);

      putXMMReg(
         gregOfRM(modrm),
         binop(Iop_64HLtoV128, mkexpr(rHi), mkexpr(rLo))
      );
      goto decode_success;
   }

   
   if (sz == 4 
       && insn[0] == 0x0F && insn[1] == 0x38 && insn[2] == 0x00) {
      IRTemp sV      = newTemp(Ity_I64);
      IRTemp dV      = newTemp(Ity_I64);

      modrm = insn[3];
      do_MMX_preamble();
      assign( dV, getMMXReg(gregOfRM(modrm)) );

      if (epartIsReg(modrm)) {
         assign( sV, getMMXReg(eregOfRM(modrm)) );
         delta += 3+1;
         DIP("pshufb %s,%s\n", nameMMXReg(eregOfRM(modrm)),
                               nameMMXReg(gregOfRM(modrm)));
      } else {
         addr = disAMode ( &alen, sorb, delta+3, dis_buf );
         assign( sV, loadLE(Ity_I64, mkexpr(addr)) );
         delta += 3+alen;
         DIP("pshufb %s,%s\n", dis_buf,
                               nameMMXReg(gregOfRM(modrm)));
      }

      putMMXReg(
         gregOfRM(modrm),
         binop(
            Iop_And64,
            
            binop(
               Iop_Perm8x8,
               mkexpr(dV),
               binop(Iop_And64, mkexpr(sV), mkU64(0x0707070707070707ULL))
            ),
            
            unop(Iop_Not64, binop(Iop_SarN8x8, mkexpr(sV), mkU8(7)))
         )
      );
      goto decode_success;
   }

   
   if (sz == 2
       && insn[0] == 0x0F && insn[1] == 0x38 && insn[2] == 0x00) {
      IRTemp sV         = newTemp(Ity_V128);
      IRTemp dV         = newTemp(Ity_V128);
      IRTemp sHi        = newTemp(Ity_I64);
      IRTemp sLo        = newTemp(Ity_I64);
      IRTemp dHi        = newTemp(Ity_I64);
      IRTemp dLo        = newTemp(Ity_I64);
      IRTemp rHi        = newTemp(Ity_I64);
      IRTemp rLo        = newTemp(Ity_I64);
      IRTemp sevens     = newTemp(Ity_I64);
      IRTemp mask0x80hi = newTemp(Ity_I64);
      IRTemp mask0x80lo = newTemp(Ity_I64);
      IRTemp maskBit3hi = newTemp(Ity_I64);
      IRTemp maskBit3lo = newTemp(Ity_I64);
      IRTemp sAnd7hi    = newTemp(Ity_I64);
      IRTemp sAnd7lo    = newTemp(Ity_I64);
      IRTemp permdHi    = newTemp(Ity_I64);
      IRTemp permdLo    = newTemp(Ity_I64);

      modrm = insn[3];
      assign( dV, getXMMReg(gregOfRM(modrm)) );

      if (epartIsReg(modrm)) {
         assign( sV, getXMMReg(eregOfRM(modrm)) );
         delta += 3+1;
         DIP("pshufb %s,%s\n", nameXMMReg(eregOfRM(modrm)),
                               nameXMMReg(gregOfRM(modrm)));
      } else {
         addr = disAMode ( &alen, sorb, delta+3, dis_buf );
         gen_SEGV_if_not_16_aligned( addr );
         assign( sV, loadLE(Ity_V128, mkexpr(addr)) );
         delta += 3+alen;
         DIP("pshufb %s,%s\n", dis_buf,
                               nameXMMReg(gregOfRM(modrm)));
      }

      assign( dHi, unop(Iop_V128HIto64, mkexpr(dV)) );
      assign( dLo, unop(Iop_V128to64,   mkexpr(dV)) );
      assign( sHi, unop(Iop_V128HIto64, mkexpr(sV)) );
      assign( sLo, unop(Iop_V128to64,   mkexpr(sV)) );

      assign( sevens, mkU64(0x0707070707070707ULL) );

      assign(
         mask0x80hi,
         unop(Iop_Not64, binop(Iop_SarN8x8,mkexpr(sHi),mkU8(7))));

      assign(
         maskBit3hi,
         binop(Iop_SarN8x8,
               binop(Iop_ShlN8x8,mkexpr(sHi),mkU8(4)),
               mkU8(7)));

      assign(sAnd7hi, binop(Iop_And64,mkexpr(sHi),mkexpr(sevens)));

      assign(
         permdHi,
         binop(
            Iop_Or64,
            binop(Iop_And64,
                  binop(Iop_Perm8x8,mkexpr(dHi),mkexpr(sAnd7hi)),
                  mkexpr(maskBit3hi)),
            binop(Iop_And64,
                  binop(Iop_Perm8x8,mkexpr(dLo),mkexpr(sAnd7hi)),
                  unop(Iop_Not64,mkexpr(maskBit3hi))) ));

      assign(rHi, binop(Iop_And64,mkexpr(permdHi),mkexpr(mask0x80hi)) );

      

      assign(
         mask0x80lo,
         unop(Iop_Not64, binop(Iop_SarN8x8,mkexpr(sLo),mkU8(7))));

      assign(
         maskBit3lo,
         binop(Iop_SarN8x8,
               binop(Iop_ShlN8x8,mkexpr(sLo),mkU8(4)),
               mkU8(7)));

      assign(sAnd7lo, binop(Iop_And64,mkexpr(sLo),mkexpr(sevens)));

      assign(
         permdLo,
         binop(
            Iop_Or64,
            binop(Iop_And64,
                  binop(Iop_Perm8x8,mkexpr(dHi),mkexpr(sAnd7lo)),
                  mkexpr(maskBit3lo)),
            binop(Iop_And64,
                  binop(Iop_Perm8x8,mkexpr(dLo),mkexpr(sAnd7lo)),
                  unop(Iop_Not64,mkexpr(maskBit3lo))) ));

      assign(rLo, binop(Iop_And64,mkexpr(permdLo),mkexpr(mask0x80lo)) );

      putXMMReg(
         gregOfRM(modrm),
         binop(Iop_64HLtoV128, mkexpr(rHi), mkexpr(rLo))
      );
      goto decode_success;
   }
   
   
   
   if ((sz == 2 || sz == 4)
       && insn[0] == 0x0F && insn[1] == 0x38
       && (insn[2] == 0xF0 || insn[2] == 0xF1)
       && !epartIsReg(insn[3])) {

      modrm = insn[3];
      addr = disAMode(&alen, sorb, delta + 3, dis_buf);
      delta += 3 + alen;
      ty = szToITy(sz);
      IRTemp src = newTemp(ty);

      if (insn[2] == 0xF0) { 
         assign(src, loadLE(ty, mkexpr(addr)));
         IRTemp dst = math_BSWAP(src, ty);
         putIReg(sz, gregOfRM(modrm), mkexpr(dst));
         DIP("movbe %s,%s\n", dis_buf, nameIReg(sz, gregOfRM(modrm)));
      } else { 
         assign(src, getIReg(sz, gregOfRM(modrm)));
         IRTemp dst = math_BSWAP(src, ty);
         storeLE(mkexpr(addr), mkexpr(dst));
         DIP("movbe %s,%s\n", nameIReg(sz, gregOfRM(modrm)), dis_buf);
      }
      goto decode_success;
   }

   
   
   

   
   
   

   if (sz == 2 
       && insn[0] == 0x0F && insn[1] == 0x3A
       && (insn[2] == 0x0A)) {

      Bool   isD = insn[2] == 0x0B;
      IRTemp src = newTemp(isD ? Ity_F64 : Ity_F32);
      IRTemp res = newTemp(isD ? Ity_F64 : Ity_F32);
      Int    imm = 0;

      modrm = insn[3];

      if (epartIsReg(modrm)) {
         assign( src, 
                 isD ? getXMMRegLane64F( eregOfRM(modrm), 0 )
                     : getXMMRegLane32F( eregOfRM(modrm), 0 ) );
         imm = insn[3+1];
         if (imm & ~3) goto decode_failure;
         delta += 3+1+1;
         DIP( "rounds%c $%d,%s,%s\n",
              isD ? 'd' : 's',
              imm, nameXMMReg( eregOfRM(modrm) ),
                   nameXMMReg( gregOfRM(modrm) ) );
      } else {
         addr = disAMode( &alen, sorb, delta+3, dis_buf );
         assign( src, loadLE( isD ? Ity_F64 : Ity_F32, mkexpr(addr) ));
         imm = insn[3+alen];
         if (imm & ~3) goto decode_failure;
         delta += 3+alen+1;
         DIP( "roundsd $%d,%s,%s\n",
              imm, dis_buf, nameXMMReg( gregOfRM(modrm) ) );
      }

      assign(res, binop(isD ? Iop_RoundF64toInt : Iop_RoundF32toInt,
                  mkU32(imm & 3), mkexpr(src)) );

      if (isD)
         putXMMRegLane64F( gregOfRM(modrm), 0, mkexpr(res) );
      else
         putXMMRegLane32F( gregOfRM(modrm), 0, mkexpr(res) );

      goto decode_success;
   }

   if (insn[0] == 0xF3 && insn[1] == 0x0F && insn[2] == 0xBD
       && 0 != (archinfo->hwcaps & VEX_HWCAPS_X86_LZCNT)) {
      vassert(sz == 2 || sz == 4);
       ty  = szToITy(sz);
      IRTemp     src = newTemp(ty);
      modrm = insn[3];
      if (epartIsReg(modrm)) {
         assign(src, getIReg(sz, eregOfRM(modrm)));
         delta += 3+1;
         DIP("lzcnt%c %s, %s\n", nameISize(sz),
             nameIReg(sz, eregOfRM(modrm)),
             nameIReg(sz, gregOfRM(modrm)));
      } else {
         addr = disAMode( &alen, sorb, delta+3, dis_buf );
         assign(src, loadLE(ty, mkexpr(addr)));
         delta += 3+alen;
         DIP("lzcnt%c %s, %s\n", nameISize(sz), dis_buf,
             nameIReg(sz, gregOfRM(modrm)));
      }

      IRTemp res = gen_LZCNT(ty, src);
      putIReg(sz, gregOfRM(modrm), mkexpr(res));

      
      
      
      
      IRTemp src32 = newTemp(Ity_I32);
      IRTemp res32 = newTemp(Ity_I32);
      assign(src32, widenUto32(mkexpr(src)));
      assign(res32, widenUto32(mkexpr(res)));

      IRTemp oszacp = newTemp(Ity_I32);
      assign(
         oszacp,
         binop(Iop_Or32,
               binop(Iop_Shl32,
                     unop(Iop_1Uto32,
                          binop(Iop_CmpEQ32, mkexpr(res32), mkU32(0))),
                     mkU8(X86G_CC_SHIFT_Z)),
               binop(Iop_Shl32,
                     unop(Iop_1Uto32,
                          binop(Iop_CmpEQ32, mkexpr(src32), mkU32(0))),
                     mkU8(X86G_CC_SHIFT_C))
         )
      );

      stmt( IRStmt_Put( OFFB_CC_OP,   mkU32(X86G_CC_OP_COPY) ));
      stmt( IRStmt_Put( OFFB_CC_DEP2, mkU32(0) ));
      stmt( IRStmt_Put( OFFB_CC_NDEP, mkU32(0) ));
      stmt( IRStmt_Put( OFFB_CC_DEP1, mkexpr(oszacp) ));

      goto decode_success;
   }

   
   
   

   after_sse_decoders:

   
   
   

   
   if (insn[0] == 0x67 && insn[1] == 0xE3 && sz == 4) {
      delta += 2;
      d32 = (((Addr32)guest_EIP_bbstart)+delta+1) + getSDisp8(delta);
      delta ++;
      stmt( IRStmt_Exit(
               binop(Iop_CmpEQ16, getIReg(2,R_ECX), mkU16(0)),
               Ijk_Boring,
               IRConst_U32(d32),
               OFFB_EIP
            ));
       DIP("jcxz 0x%x\n", d32);
       goto decode_success;
   }

   
   
   

   
   opc = getIByte(delta); delta++;


   switch (opc) {

   

   case 0xC2: 
      d32 = getUDisp16(delta); 
      delta += 2;
      dis_ret(&dres, d32);
      DIP("ret %d\n", (Int)d32);
      break;
   case 0xC3: 
      dis_ret(&dres, 0);
      DIP("ret\n");
      break;

   case 0xCF: 
      t1 = newTemp(Ity_I32); 
      t2 = newTemp(Ity_I32); 
      t3 = newTemp(Ity_I32); 
      t4 = newTemp(Ity_I32); 
      assign(t1, getIReg(4,R_ESP));
      assign(t2, loadLE(Ity_I32, binop(Iop_Add32,mkexpr(t1),mkU32(0) )));
      assign(t3, loadLE(Ity_I32, binop(Iop_Add32,mkexpr(t1),mkU32(4) )));
      assign(t4, loadLE(Ity_I32, binop(Iop_Add32,mkexpr(t1),mkU32(8) )));
      
      putIReg(4, R_ESP,binop(Iop_Add32, mkexpr(t1), mkU32(12)));
      
      putSReg( R_CS, unop(Iop_32to16, mkexpr(t3)) );
      
      set_EFLAGS_from_value( t4, False, 0 );
      
      jmp_treg(&dres, Ijk_Ret, t2);
      vassert(dres.whatNext == Dis_StopHere);
      DIP("iret (very kludgey)\n");
      break;

   case 0xE8: 
      d32 = getUDisp32(delta); delta += 4;
      d32 += (guest_EIP_bbstart+delta); 
      
      if (d32 == guest_EIP_bbstart+delta && getIByte(delta) >= 0x58 
                                         && getIByte(delta) <= 0x5F) {
         Int archReg = getIByte(delta) - 0x58;
         
         putIReg(4, archReg, mkU32(guest_EIP_bbstart+delta));
         delta++; 
         DIP("call 0x%x ; popl %s\n",d32,nameIReg(4,archReg));
      } else {
         
         t1 = newTemp(Ity_I32); 
         assign(t1, binop(Iop_Sub32, getIReg(4,R_ESP), mkU32(4)));
         putIReg(4, R_ESP, mkexpr(t1));
         storeLE( mkexpr(t1), mkU32(guest_EIP_bbstart+delta));
         if (resteerOkFn( callback_opaque, (Addr32)d32 )) {
            
            dres.whatNext   = Dis_ResteerU;
            dres.continueAt = (Addr32)d32;
         } else {
            jmp_lit(&dres, Ijk_Call, d32);
            vassert(dres.whatNext == Dis_StopHere);
         }
         DIP("call 0x%x\n",d32);
      }
      break;


   case 0xC9: 
      vassert(sz == 4);
      t1 = newTemp(Ity_I32); t2 = newTemp(Ity_I32);
      assign(t1, getIReg(4,R_EBP));
      putIReg(4, R_ESP, mkexpr(t1));
      assign(t2, loadLE(Ity_I32,mkexpr(t1)));
      putIReg(4, R_EBP, mkexpr(t2));
      putIReg(4, R_ESP, binop(Iop_Add32, mkexpr(t1), mkU32(4)) );
      DIP("leave\n");
      break;

   

   case 0x27: 
   case 0x2F: 
   case 0x37: 
   case 0x3F: 
      if (sz != 4) goto decode_failure;
      t1 = newTemp(Ity_I32);
      t2 = newTemp(Ity_I32);
      assign(t1, 
             binop(Iop_16HLto32, 
                   unop(Iop_32to16,
                        mk_x86g_calculate_eflags_all()),
                   getIReg(2, R_EAX)
            ));
      vassert(opc == 0x27 || opc == 0x2F || opc == 0x37 || opc == 0x3F);
      assign(t2,
              mkIRExprCCall(
                 Ity_I32, 0, "x86g_calculate_daa_das_aaa_aas",
                 &x86g_calculate_daa_das_aaa_aas,
                 mkIRExprVec_2( mkexpr(t1), mkU32( opc & 0xFF) )
            ));
     putIReg(2, R_EAX, unop(Iop_32to16, mkexpr(t2) ));

     stmt( IRStmt_Put( OFFB_CC_OP,   mkU32(X86G_CC_OP_COPY) ));
     stmt( IRStmt_Put( OFFB_CC_DEP2, mkU32(0) ));
     stmt( IRStmt_Put( OFFB_CC_DEP1, 
                       binop(Iop_And32,
                             binop(Iop_Shr32, mkexpr(t2), mkU8(16)),
                             mkU32( X86G_CC_MASK_C | X86G_CC_MASK_P 
                                    | X86G_CC_MASK_A | X86G_CC_MASK_Z 
                                    | X86G_CC_MASK_S| X86G_CC_MASK_O )
                            )
                      )
         );
     stmt( IRStmt_Put( OFFB_CC_NDEP, mkU32(0) ));
     switch (opc) {
        case 0x27: DIP("daa\n"); break;
        case 0x2F: DIP("das\n"); break;
        case 0x37: DIP("aaa\n"); break;
        case 0x3F: DIP("aas\n"); break;
        default: vassert(0);
     }
     break;

   case 0xD4: 
   case 0xD5: 
      d32 = getIByte(delta); delta++;
      if (sz != 4 || d32 != 10) goto decode_failure;
      t1 = newTemp(Ity_I32);
      t2 = newTemp(Ity_I32);
      assign(t1, 
             binop(Iop_16HLto32, 
                   unop(Iop_32to16,
                        mk_x86g_calculate_eflags_all()),
                   getIReg(2, R_EAX)
            ));
      assign(t2,
              mkIRExprCCall(
                 Ity_I32, 0, "x86g_calculate_aad_aam",
                 &x86g_calculate_aad_aam,
                 mkIRExprVec_2( mkexpr(t1), mkU32( opc & 0xFF) )
            ));
      putIReg(2, R_EAX, unop(Iop_32to16, mkexpr(t2) ));

      stmt( IRStmt_Put( OFFB_CC_OP,   mkU32(X86G_CC_OP_COPY) ));
      stmt( IRStmt_Put( OFFB_CC_DEP2, mkU32(0) ));
      stmt( IRStmt_Put( OFFB_CC_DEP1, 
                        binop(Iop_And32,
                              binop(Iop_Shr32, mkexpr(t2), mkU8(16)),
                              mkU32( X86G_CC_MASK_C | X86G_CC_MASK_P 
                                     | X86G_CC_MASK_A | X86G_CC_MASK_Z 
                                     | X86G_CC_MASK_S| X86G_CC_MASK_O )
                             )
                       )
          );
      stmt( IRStmt_Put( OFFB_CC_NDEP, mkU32(0) ));

      DIP(opc == 0xD4 ? "aam\n" : "aad\n");
      break;

   

   case 0x98: 
      if (sz == 4) {
         putIReg(4, R_EAX, unop(Iop_16Sto32, getIReg(2, R_EAX)));
         DIP("cwde\n");
      } else {
         vassert(sz == 2);
         putIReg(2, R_EAX, unop(Iop_8Sto16, getIReg(1, R_EAX)));
         DIP("cbw\n");
      }
      break;

   case 0x99: 
      ty = szToITy(sz);
      putIReg(sz, R_EDX,
                  binop(mkSizedOp(ty,Iop_Sar8), 
                        getIReg(sz, R_EAX),
                        mkU8(sz == 2 ? 15 : 31)) );
      DIP(sz == 2 ? "cwdq\n" : "cdqq\n");
      break;

   

   case 0x9E: 
      codegen_SAHF();
      DIP("sahf\n");
      break;

   case 0x9F: 
      codegen_LAHF();
      DIP("lahf\n");
      break;

   case 0x9B: 
      
      DIP("fwait\n");
      break;

   case 0xD8:
   case 0xD9:
   case 0xDA:
   case 0xDB:
   case 0xDC:
   case 0xDD:
   case 0xDE:
   case 0xDF: {
      Int  delta0    = delta;
      Bool decode_OK = False;
      delta = dis_FPU ( &decode_OK, sorb, delta );
      if (!decode_OK) {
         delta = delta0;
         goto decode_failure;
      }
      break;
   }

   

   case 0x40: 
   case 0x41: 
   case 0x42: 
   case 0x43: 
   case 0x44: 
   case 0x45: 
   case 0x46: 
   case 0x47: 
      vassert(sz == 2 || sz == 4);
      ty = szToITy(sz);
      t1 = newTemp(ty);
      assign( t1, binop(mkSizedOp(ty,Iop_Add8),
                        getIReg(sz, (UInt)(opc - 0x40)),
                        mkU(ty,1)) );
      setFlags_INC_DEC( True, t1, ty );
      putIReg(sz, (UInt)(opc - 0x40), mkexpr(t1));
      DIP("inc%c %s\n", nameISize(sz), nameIReg(sz,opc-0x40));
      break;

   case 0x48: 
   case 0x49: 
   case 0x4A: 
   case 0x4B: 
   case 0x4C: 
   case 0x4D: 
   case 0x4E: 
   case 0x4F: 
      vassert(sz == 2 || sz == 4);
      ty = szToITy(sz);
      t1 = newTemp(ty);
      assign( t1, binop(mkSizedOp(ty,Iop_Sub8),
                        getIReg(sz, (UInt)(opc - 0x48)),
                        mkU(ty,1)) );
      setFlags_INC_DEC( False, t1, ty );
      putIReg(sz, (UInt)(opc - 0x48), mkexpr(t1));
      DIP("dec%c %s\n", nameISize(sz), nameIReg(sz,opc-0x48));
      break;

   

   case 0xCC: 
      jmp_lit(&dres, Ijk_SigTRAP, ((Addr32)guest_EIP_bbstart)+delta);
      vassert(dres.whatNext == Dis_StopHere);
      DIP("int $0x3\n");
      break;

   case 0xCD: 
      d32 = getIByte(delta); delta++;


      if (d32 >= 0x3F && d32 <= 0x4F) {
         jmp_lit(&dres, Ijk_SigSEGV, ((Addr32)guest_EIP_bbstart)+delta-2);
         vassert(dres.whatNext == Dis_StopHere);
         DIP("int $0x%x\n", (Int)d32);
         break;
      }

      if (d32 == 0x80) {
         stmt( IRStmt_Put( OFFB_IP_AT_SYSCALL,
                           mkU32(guest_EIP_curr_instr) ) );
         jmp_lit(&dres, Ijk_Sys_int128, ((Addr32)guest_EIP_bbstart)+delta);
         vassert(dres.whatNext == Dis_StopHere);
         DIP("int $0x80\n");
         break;
      }
      if (d32 == 0x81) {
         stmt( IRStmt_Put( OFFB_IP_AT_SYSCALL,
                           mkU32(guest_EIP_curr_instr) ) );
         jmp_lit(&dres, Ijk_Sys_int129, ((Addr32)guest_EIP_bbstart)+delta);
         vassert(dres.whatNext == Dis_StopHere);
         DIP("int $0x81\n");
         break;
      }
      if (d32 == 0x82) {
         stmt( IRStmt_Put( OFFB_IP_AT_SYSCALL,
                           mkU32(guest_EIP_curr_instr) ) );
         jmp_lit(&dres, Ijk_Sys_int130, ((Addr32)guest_EIP_bbstart)+delta);
         vassert(dres.whatNext == Dis_StopHere);
         DIP("int $0x82\n");
         break;
      }

      
      goto decode_failure;

   

   case 0xEB: 
      d32 = (((Addr32)guest_EIP_bbstart)+delta+1) + getSDisp8(delta); 
      delta++;
      if (resteerOkFn( callback_opaque, (Addr32)d32) ) {
         dres.whatNext   = Dis_ResteerU;
         dres.continueAt = (Addr32)d32;
      } else {
         jmp_lit(&dres, Ijk_Boring, d32);
         vassert(dres.whatNext == Dis_StopHere);
      }
      DIP("jmp-8 0x%x\n", d32);
      break;

   case 0xE9: 
      vassert(sz == 4); 
      d32 = (((Addr32)guest_EIP_bbstart)+delta+sz) + getSDisp(sz,delta); 
      delta += sz;
      if (resteerOkFn( callback_opaque, (Addr32)d32) ) {
         dres.whatNext   = Dis_ResteerU;
         dres.continueAt = (Addr32)d32;
      } else {
         jmp_lit(&dres, Ijk_Boring, d32);
         vassert(dres.whatNext == Dis_StopHere);
      }
      DIP("jmp 0x%x\n", d32);
      break;

   case 0x70:
   case 0x71:
   case 0x72: 
   case 0x73: 
   case 0x74: 
   case 0x75: 
   case 0x76: 
   case 0x77: 
   case 0x78: 
   case 0x79: 
   case 0x7A: 
   case 0x7B: 
   case 0x7C: 
   case 0x7D: 
   case 0x7E: 
   case 0x7F: 
    { Int    jmpDelta;
      const HChar* comment  = "";
      jmpDelta = (Int)getSDisp8(delta);
      vassert(-128 <= jmpDelta && jmpDelta < 128);
      d32 = (((Addr32)guest_EIP_bbstart)+delta+1) + jmpDelta; 
      delta++;
      if (resteerCisOk
          && vex_control.guest_chase_cond
          && (Addr32)d32 != (Addr32)guest_EIP_bbstart
          && jmpDelta < 0
          && resteerOkFn( callback_opaque, (Addr32)d32) ) {
         stmt( IRStmt_Exit( 
                  mk_x86g_calculate_condition((X86Condcode)(1 ^ (opc - 0x70))),
                  Ijk_Boring,
                  IRConst_U32(guest_EIP_bbstart+delta),
                  OFFB_EIP ) );
         dres.whatNext   = Dis_ResteerC;
         dres.continueAt = (Addr32)d32;
         comment = "(assumed taken)";
      }
      else
      if (resteerCisOk
          && vex_control.guest_chase_cond
          && (Addr32)d32 != (Addr32)guest_EIP_bbstart
          && jmpDelta >= 0
          && resteerOkFn( callback_opaque, 
                          (Addr32)(guest_EIP_bbstart+delta)) ) {
         stmt( IRStmt_Exit( 
                  mk_x86g_calculate_condition((X86Condcode)(opc - 0x70)),
                  Ijk_Boring,
                  IRConst_U32(d32),
                  OFFB_EIP ) );
         dres.whatNext   = Dis_ResteerC;
         dres.continueAt = guest_EIP_bbstart + delta;
         comment = "(assumed not taken)";
      }
      else {
         jcc_01( &dres, (X86Condcode)(opc - 0x70), 
                 (Addr32)(guest_EIP_bbstart+delta), d32);
         vassert(dres.whatNext == Dis_StopHere);
      }
      DIP("j%s-8 0x%x %s\n", name_X86Condcode(opc - 0x70), d32, comment);
      break;
    }

   case 0xE3: 
      if (sz != 4) goto decode_failure;
      d32 = (((Addr32)guest_EIP_bbstart)+delta+1) + getSDisp8(delta);
      delta ++;
      stmt( IRStmt_Exit(
               binop(Iop_CmpEQ32, getIReg(4,R_ECX), mkU32(0)),
            Ijk_Boring,
            IRConst_U32(d32),
            OFFB_EIP
          ));
      DIP("jecxz 0x%x\n", d32);
      break;

   case 0xE0: 
   case 0xE1: 
   case 0xE2: 
    { 
      IRExpr* zbit  = NULL;
      IRExpr* count = NULL;
      IRExpr* cond  = NULL;
      const HChar* xtra = NULL;

      if (sz != 4) goto decode_failure;
      d32 = (((Addr32)guest_EIP_bbstart)+delta+1) + getSDisp8(delta);
      delta++;
      putIReg(4, R_ECX, binop(Iop_Sub32, getIReg(4,R_ECX), mkU32(1)));

      count = getIReg(4,R_ECX);
      cond = binop(Iop_CmpNE32, count, mkU32(0));
      switch (opc) {
         case 0xE2: 
            xtra = ""; 
            break;
         case 0xE1: 
            xtra = "e"; 
            zbit = mk_x86g_calculate_condition( X86CondZ );
	    cond = mkAnd1(cond, zbit);
            break;
         case 0xE0: 
            xtra = "ne";
            zbit = mk_x86g_calculate_condition( X86CondNZ );
	    cond = mkAnd1(cond, zbit);
            break;
         default:
	    vassert(0);
      }
      stmt( IRStmt_Exit(cond, Ijk_Boring, IRConst_U32(d32), OFFB_EIP) );

      DIP("loop%s 0x%x\n", xtra, d32);
      break;
    }

   

   case 0x69: 
      delta = dis_imul_I_E_G ( sorb, sz, delta, sz );
      break;
   case 0x6B: 
      delta = dis_imul_I_E_G ( sorb, sz, delta, 1 );
      break;

   

   case 0x88: 
      delta = dis_mov_G_E(sorb, 1, delta);
      break;

   case 0x89: 
      delta = dis_mov_G_E(sorb, sz, delta);
      break;

   case 0x8A: 
      delta = dis_mov_E_G(sorb, 1, delta);
      break;
 
   case 0x8B: 
      delta = dis_mov_E_G(sorb, sz, delta);
      break;
 
   case 0x8D: 
      if (sz != 4)
         goto decode_failure;
      modrm = getIByte(delta);
      if (epartIsReg(modrm)) 
         goto decode_failure;
      addr = disAMode ( &alen,  0, delta, dis_buf );
      delta += alen;
      putIReg(sz, gregOfRM(modrm), mkexpr(addr));
      DIP("lea%c %s, %s\n", nameISize(sz), dis_buf, 
                            nameIReg(sz,gregOfRM(modrm)));
      break;

   case 0x8C: 
      delta = dis_mov_Sw_Ew(sorb, sz, delta);
      break;

   case 0x8E: 
      delta = dis_mov_Ew_Sw(sorb, delta);
      break;
 
   case 0xA0: 
      sz = 1;
      
   case 0xA1: 
      d32 = getUDisp32(delta); delta += 4;
      ty = szToITy(sz);
      addr = newTemp(Ity_I32);
      assign( addr, handleSegOverride(sorb, mkU32(d32)) );
      putIReg(sz, R_EAX, loadLE(ty, mkexpr(addr)));
      DIP("mov%c %s0x%x, %s\n", nameISize(sz), sorbTxt(sorb),
                                d32, nameIReg(sz,R_EAX));
      break;

   case 0xA2: 
      sz = 1;
      
   case 0xA3: 
      d32 = getUDisp32(delta); delta += 4;
      ty = szToITy(sz);
      addr = newTemp(Ity_I32);
      assign( addr, handleSegOverride(sorb, mkU32(d32)) );
      storeLE( mkexpr(addr), getIReg(sz,R_EAX) );
      DIP("mov%c %s, %s0x%x\n", nameISize(sz), nameIReg(sz,R_EAX),
                                sorbTxt(sorb), d32);
      break;

   case 0xB0: 
   case 0xB1: 
   case 0xB2: 
   case 0xB3: 
   case 0xB4: 
   case 0xB5: 
   case 0xB6: 
   case 0xB7: 
      d32 = getIByte(delta); delta += 1;
      putIReg(1, opc-0xB0, mkU8(d32));
      DIP("movb $0x%x,%s\n", d32, nameIReg(1,opc-0xB0));
      break;

   case 0xB8: 
   case 0xB9: 
   case 0xBA: 
   case 0xBB: 
   case 0xBC: 
   case 0xBD: 
   case 0xBE: 
   case 0xBF: 
      d32 = getUDisp(sz,delta); delta += sz;
      putIReg(sz, opc-0xB8, mkU(szToITy(sz), d32));
      DIP("mov%c $0x%x,%s\n", nameISize(sz), d32, nameIReg(sz,opc-0xB8));
      break;

   case 0xC6: 
      sz = 1;
      goto maybe_do_Mov_I_E;
   case 0xC7: 
      goto maybe_do_Mov_I_E;

   maybe_do_Mov_I_E:
      modrm = getIByte(delta);
      if (gregOfRM(modrm) == 0) {
         if (epartIsReg(modrm)) {
            delta++; 
            d32 = getUDisp(sz,delta); delta += sz;
            putIReg(sz, eregOfRM(modrm), mkU(szToITy(sz), d32));
            DIP("mov%c $0x%x, %s\n", nameISize(sz), d32, 
                                     nameIReg(sz,eregOfRM(modrm)));
         } else {
            addr = disAMode ( &alen, sorb, delta, dis_buf );
            delta += alen;
            d32 = getUDisp(sz,delta); delta += sz;
            storeLE(mkexpr(addr), mkU(szToITy(sz), d32));
            DIP("mov%c $0x%x, %s\n", nameISize(sz), d32, dis_buf);
         }
         break;
      }
      goto decode_failure;

   

   case 0x04: 
      delta = dis_op_imm_A(  1, False, Iop_Add8, True, delta, "add" );
      break;
   case 0x05: 
      delta = dis_op_imm_A( sz, False, Iop_Add8, True, delta, "add" );
      break;

   case 0x0C: 
      delta = dis_op_imm_A(  1, False, Iop_Or8, True, delta, "or" );
      break;
   case 0x0D: 
      delta = dis_op_imm_A( sz, False, Iop_Or8, True, delta, "or" );
      break;

   case 0x14: 
      delta = dis_op_imm_A(  1, True, Iop_Add8, True, delta, "adc" );
      break;
   case 0x15: 
      delta = dis_op_imm_A( sz, True, Iop_Add8, True, delta, "adc" );
      break;

   case 0x1C: 
      delta = dis_op_imm_A( 1, True, Iop_Sub8, True, delta, "sbb" );
      break;
   case 0x1D: 
      delta = dis_op_imm_A( sz, True, Iop_Sub8, True, delta, "sbb" );
      break;

   case 0x24: 
      delta = dis_op_imm_A(  1, False, Iop_And8, True, delta, "and" );
      break;
   case 0x25: 
      delta = dis_op_imm_A( sz, False, Iop_And8, True, delta, "and" );
      break;

   case 0x2C: 
      delta = dis_op_imm_A(  1, False, Iop_Sub8, True, delta, "sub" );
      break;
   case 0x2D: 
      delta = dis_op_imm_A( sz, False, Iop_Sub8, True, delta, "sub" );
      break;

   case 0x34: 
      delta = dis_op_imm_A(  1, False, Iop_Xor8, True, delta, "xor" );
      break;
   case 0x35: 
      delta = dis_op_imm_A( sz, False, Iop_Xor8, True, delta, "xor" );
      break;

   case 0x3C: 
      delta = dis_op_imm_A(  1, False, Iop_Sub8, False, delta, "cmp" );
      break;
   case 0x3D: 
      delta = dis_op_imm_A( sz, False, Iop_Sub8, False, delta, "cmp" );
      break;

   case 0xA8: 
      delta = dis_op_imm_A(  1, False, Iop_And8, False, delta, "test" );
      break;
   case 0xA9: 
      delta = dis_op_imm_A( sz, False, Iop_And8, False, delta, "test" );
      break;

   
 
   case 0x02: 
      delta = dis_op2_E_G ( sorb, False, Iop_Add8, True, 1, delta, "add" );
      break;
   case 0x03: 
      delta = dis_op2_E_G ( sorb, False, Iop_Add8, True, sz, delta, "add" );
      break;

   case 0x0A: 
      delta = dis_op2_E_G ( sorb, False, Iop_Or8, True, 1, delta, "or" );
      break;
   case 0x0B: 
      delta = dis_op2_E_G ( sorb, False, Iop_Or8, True, sz, delta, "or" );
      break;

   case 0x12: 
      delta = dis_op2_E_G ( sorb, True, Iop_Add8, True, 1, delta, "adc" );
      break;
   case 0x13: 
      delta = dis_op2_E_G ( sorb, True, Iop_Add8, True, sz, delta, "adc" );
      break;

   case 0x1A: 
      delta = dis_op2_E_G ( sorb, True, Iop_Sub8, True, 1, delta, "sbb" );
      break;
   case 0x1B: 
      delta = dis_op2_E_G ( sorb, True, Iop_Sub8, True, sz, delta, "sbb" );
      break;

   case 0x22: 
      delta = dis_op2_E_G ( sorb, False, Iop_And8, True, 1, delta, "and" );
      break;
   case 0x23: 
      delta = dis_op2_E_G ( sorb, False, Iop_And8, True, sz, delta, "and" );
      break;

   case 0x2A: 
      delta = dis_op2_E_G ( sorb, False, Iop_Sub8, True, 1, delta, "sub" );
      break;
   case 0x2B: 
      delta = dis_op2_E_G ( sorb, False, Iop_Sub8, True, sz, delta, "sub" );
      break;

   case 0x32: 
      delta = dis_op2_E_G ( sorb, False, Iop_Xor8, True, 1, delta, "xor" );
      break;
   case 0x33: 
      delta = dis_op2_E_G ( sorb, False, Iop_Xor8, True, sz, delta, "xor" );
      break;

   case 0x3A: 
      delta = dis_op2_E_G ( sorb, False, Iop_Sub8, False, 1, delta, "cmp" );
      break;
   case 0x3B: 
      delta = dis_op2_E_G ( sorb, False, Iop_Sub8, False, sz, delta, "cmp" );
      break;

   case 0x84: 
      delta = dis_op2_E_G ( sorb, False, Iop_And8, False, 1, delta, "test" );
      break;
   case 0x85: 
      delta = dis_op2_E_G ( sorb, False, Iop_And8, False, sz, delta, "test" );
      break;

   

   case 0x00: 
      delta = dis_op2_G_E ( sorb, pfx_lock, False,
                            Iop_Add8, True, 1, delta, "add" );
      break;
   case 0x01: 
      delta = dis_op2_G_E ( sorb, pfx_lock, False,
                            Iop_Add8, True, sz, delta, "add" );
      break;

   case 0x08: 
      delta = dis_op2_G_E ( sorb, pfx_lock, False,
                            Iop_Or8, True, 1, delta, "or" );
      break;
   case 0x09: 
      delta = dis_op2_G_E ( sorb, pfx_lock, False,
                            Iop_Or8, True, sz, delta, "or" );
      break;

   case 0x10: 
      delta = dis_op2_G_E ( sorb, pfx_lock, True,
                            Iop_Add8, True, 1, delta, "adc" );
      break;
   case 0x11: 
      delta = dis_op2_G_E ( sorb, pfx_lock, True,
                            Iop_Add8, True, sz, delta, "adc" );
      break;

   case 0x18: 
      delta = dis_op2_G_E ( sorb, pfx_lock, True,
                            Iop_Sub8, True, 1, delta, "sbb" );
      break;
   case 0x19: 
      delta = dis_op2_G_E ( sorb, pfx_lock, True,
                            Iop_Sub8, True, sz, delta, "sbb" );
      break;

   case 0x20: 
      delta = dis_op2_G_E ( sorb, pfx_lock, False,
                            Iop_And8, True, 1, delta, "and" );
      break;
   case 0x21: 
      delta = dis_op2_G_E ( sorb, pfx_lock, False,
                            Iop_And8, True, sz, delta, "and" );
      break;

   case 0x28: 
      delta = dis_op2_G_E ( sorb, pfx_lock, False,
                            Iop_Sub8, True, 1, delta, "sub" );
      break;
   case 0x29: 
      delta = dis_op2_G_E ( sorb, pfx_lock, False,
                            Iop_Sub8, True, sz, delta, "sub" );
      break;

   case 0x30: 
      delta = dis_op2_G_E ( sorb, pfx_lock, False,
                            Iop_Xor8, True, 1, delta, "xor" );
      break;
   case 0x31: 
      delta = dis_op2_G_E ( sorb, pfx_lock, False,
                            Iop_Xor8, True, sz, delta, "xor" );
      break;

   case 0x38: 
      delta = dis_op2_G_E ( sorb, pfx_lock, False,
                            Iop_Sub8, False, 1, delta, "cmp" );
      break;
   case 0x39: 
      delta = dis_op2_G_E ( sorb, pfx_lock, False,
                            Iop_Sub8, False, sz, delta, "cmp" );
      break;

   

   case 0x58: 
   case 0x59: 
   case 0x5A: 
   case 0x5B: 
   case 0x5D: 
   case 0x5E: 
   case 0x5F: 
   case 0x5C: 
      vassert(sz == 2 || sz == 4);
      t1 = newTemp(szToITy(sz)); t2 = newTemp(Ity_I32);
      assign(t2, getIReg(4, R_ESP));
      assign(t1, loadLE(szToITy(sz),mkexpr(t2)));
      putIReg(4, R_ESP, binop(Iop_Add32, mkexpr(t2), mkU32(sz)));
      putIReg(sz, opc-0x58, mkexpr(t1));
      DIP("pop%c %s\n", nameISize(sz), nameIReg(sz,opc-0x58));
      break;

   case 0x9D: 
      vassert(sz == 2 || sz == 4);
      t1 = newTemp(Ity_I32); t2 = newTemp(Ity_I32);
      assign(t2, getIReg(4, R_ESP));
      assign(t1, widenUto32(loadLE(szToITy(sz),mkexpr(t2))));
      putIReg(4, R_ESP, binop(Iop_Add32, mkexpr(t2), mkU32(sz)));

      set_EFLAGS_from_value( t1, True,
                                 ((Addr32)guest_EIP_bbstart)+delta );

      DIP("popf%c\n", nameISize(sz));
      break;

   case 0x61: 
      
      if (sz != 4) goto decode_failure;

      
      t5 = newTemp(Ity_I32);
      assign( t5, getIReg(4, R_ESP) );

      
      putIReg(4,R_EAX, loadLE(Ity_I32, binop(Iop_Add32,mkexpr(t5),mkU32(28)) ));
      putIReg(4,R_ECX, loadLE(Ity_I32, binop(Iop_Add32,mkexpr(t5),mkU32(24)) ));
      putIReg(4,R_EDX, loadLE(Ity_I32, binop(Iop_Add32,mkexpr(t5),mkU32(20)) ));
      putIReg(4,R_EBX, loadLE(Ity_I32, binop(Iop_Add32,mkexpr(t5),mkU32(16)) ));
      
      putIReg(4,R_EBP, loadLE(Ity_I32, binop(Iop_Add32,mkexpr(t5),mkU32( 8)) ));
      putIReg(4,R_ESI, loadLE(Ity_I32, binop(Iop_Add32,mkexpr(t5),mkU32( 4)) ));
      putIReg(4,R_EDI, loadLE(Ity_I32, binop(Iop_Add32,mkexpr(t5),mkU32( 0)) ));

      
      putIReg( 4, R_ESP, binop(Iop_Add32, mkexpr(t5), mkU32(8*4)) );

      DIP("popa%c\n", nameISize(sz));
      break;

   case 0x8F: 
     { Int    len;
       UChar  rm = getIByte(delta);

       
       if (epartIsReg(rm) || gregOfRM(rm) != 0)
          goto decode_failure;
       
       if (sz != 4 && sz != 2)
          goto decode_failure;
       ty = szToITy(sz);

       t1 = newTemp(Ity_I32); 
       t3 = newTemp(ty); 
       
       assign( t1, getIReg(4, R_ESP) );
       
       assign( t3, loadLE(ty, mkexpr(t1)) );
       
       putIReg(4, R_ESP, binop(Iop_Add32, mkexpr(t1), mkU32(sz)) );

       
       addr = disAMode ( &len, sorb, delta, dis_buf);
       storeLE( mkexpr(addr), mkexpr(t3) );

       DIP("pop%c %s\n", sz==2 ? 'w' : 'l', dis_buf);

       delta += len;
       break;
     }

   case 0x1F: 
      dis_pop_segreg( R_DS, sz ); break;
   case 0x07: 
      dis_pop_segreg( R_ES, sz ); break;
   case 0x17: 
      dis_pop_segreg( R_SS, sz ); break;

   

   case 0x50: 
   case 0x51: 
   case 0x52: 
   case 0x53: 
   case 0x55: 
   case 0x56: 
   case 0x57: 
   case 0x54: 
      vassert(sz == 2 || sz == 4);
      ty = sz==2 ? Ity_I16 : Ity_I32;
      t1 = newTemp(ty); t2 = newTemp(Ity_I32);
      assign(t1, getIReg(sz, opc-0x50));
      assign(t2, binop(Iop_Sub32, getIReg(4, R_ESP), mkU32(sz)));
      putIReg(4, R_ESP, mkexpr(t2) );
      storeLE(mkexpr(t2),mkexpr(t1));
      DIP("push%c %s\n", nameISize(sz), nameIReg(sz,opc-0x50));
      break;


   case 0x68: 
      d32 = getUDisp(sz,delta); delta += sz;
      goto do_push_I;
   case 0x6A: 
      d32 = getSDisp8(delta); delta += 1;
      goto do_push_I;
   do_push_I:
      ty = szToITy(sz);
      t1 = newTemp(Ity_I32); t2 = newTemp(ty);
      assign( t1, binop(Iop_Sub32,getIReg(4,R_ESP),mkU32(sz)) );
      putIReg(4, R_ESP, mkexpr(t1) );
      if (ty == Ity_I16)
         d32 &= 0xFFFF;
      storeLE( mkexpr(t1), mkU(ty,d32) );
      DIP("push%c $0x%x\n", nameISize(sz), d32);
      break;

   case 0x9C:  {
      vassert(sz == 2 || sz == 4);

      t1 = newTemp(Ity_I32);
      assign( t1, binop(Iop_Sub32,getIReg(4,R_ESP),mkU32(sz)) );
      putIReg(4, R_ESP, mkexpr(t1) );

      t2 = newTemp(Ity_I32);
      assign( t2, binop(Iop_Or32, 
                        mk_x86g_calculate_eflags_all(), 
                        mkU32( (1<<1)|(1<<9) ) ));

      t3 = newTemp(Ity_I32);
      assign( t3, binop(Iop_Or32,
                        mkexpr(t2),
                        binop(Iop_And32,
                              IRExpr_Get(OFFB_DFLAG,Ity_I32),
                              mkU32(1<<10))) 
            );

      
      t4 = newTemp(Ity_I32);
      assign( t4, binop(Iop_Or32,
                        mkexpr(t3),
                        binop(Iop_And32,
                              binop(Iop_Shl32, IRExpr_Get(OFFB_IDFLAG,Ity_I32), 
                                               mkU8(21)),
                              mkU32(1<<21)))
            );

      
      t5 = newTemp(Ity_I32);
      assign( t5, binop(Iop_Or32,
                        mkexpr(t4),
                        binop(Iop_And32,
                              binop(Iop_Shl32, IRExpr_Get(OFFB_ACFLAG,Ity_I32), 
                                               mkU8(18)),
                              mkU32(1<<18)))
            );

      
      if (sz == 2)
        storeLE( mkexpr(t1), unop(Iop_32to16,mkexpr(t5)) );
      else 
        storeLE( mkexpr(t1), mkexpr(t5) );

      DIP("pushf%c\n", nameISize(sz));
      break;
   }

   case 0x60: 
      
      if (sz != 4) goto decode_failure;

      
      t0 = newTemp(Ity_I32);
      assign( t0, getIReg(4, R_ESP) );

      
      t5 = newTemp(Ity_I32);
      assign( t5, binop(Iop_Sub32, mkexpr(t0), mkU32(8*4)) );

      
      putIReg(4, R_ESP, mkexpr(t5));

      
      storeLE( binop(Iop_Add32,mkexpr(t5),mkU32(28)), getIReg(4,R_EAX) );
      storeLE( binop(Iop_Add32,mkexpr(t5),mkU32(24)), getIReg(4,R_ECX) );
      storeLE( binop(Iop_Add32,mkexpr(t5),mkU32(20)), getIReg(4,R_EDX) );
      storeLE( binop(Iop_Add32,mkexpr(t5),mkU32(16)), getIReg(4,R_EBX) );
      storeLE( binop(Iop_Add32,mkexpr(t5),mkU32(12)), mkexpr(t0) );
      storeLE( binop(Iop_Add32,mkexpr(t5),mkU32( 8)), getIReg(4,R_EBP) );
      storeLE( binop(Iop_Add32,mkexpr(t5),mkU32( 4)), getIReg(4,R_ESI) );
      storeLE( binop(Iop_Add32,mkexpr(t5),mkU32( 0)), getIReg(4,R_EDI) );

      DIP("pusha%c\n", nameISize(sz));
      break;

   case 0x0E: 
      dis_push_segreg( R_CS, sz ); break;
   case 0x1E: 
      dis_push_segreg( R_DS, sz ); break;
   case 0x06: 
      dis_push_segreg( R_ES, sz ); break;
   case 0x16: 
      dis_push_segreg( R_SS, sz ); break;

   

   case 0xA4: 
   case 0xA5: 
      if (sorb != 0)
         goto decode_failure; 
      dis_string_op( dis_MOVS, ( opc == 0xA4 ? 1 : sz ), "movs", sorb );
      break;

  case 0xA6: 
  case 0xA7:
      if (sorb != 0)
         goto decode_failure; 
      dis_string_op( dis_CMPS, ( opc == 0xA6 ? 1 : sz ), "cmps", sorb );
      break;

   case 0xAA: 
   case 0xAB:
      if (sorb != 0)
         goto decode_failure; 
      dis_string_op( dis_STOS, ( opc == 0xAA ? 1 : sz ), "stos", sorb );
      break;

   case 0xAC: 
   case 0xAD:
      if (sorb != 0)
         goto decode_failure; 
      dis_string_op( dis_LODS, ( opc == 0xAC ? 1 : sz ), "lods", sorb );
      break;

   case 0xAE: 
   case 0xAF:
      if (sorb != 0) 
         goto decode_failure; 
      dis_string_op( dis_SCAS, ( opc == 0xAE ? 1 : sz ), "scas", sorb );
      break;


   case 0xFC: 
      stmt( IRStmt_Put( OFFB_DFLAG, mkU32(1)) );
      DIP("cld\n");
      break;

   case 0xFD: 
      stmt( IRStmt_Put( OFFB_DFLAG, mkU32(0xFFFFFFFF)) );
      DIP("std\n");
      break;

   case 0xF8: 
   case 0xF9: 
   case 0xF5: 
      t0 = newTemp(Ity_I32);
      t1 = newTemp(Ity_I32);
      assign( t0, mk_x86g_calculate_eflags_all() );
      switch (opc) {
         case 0xF8: 
            assign( t1, binop(Iop_And32, mkexpr(t0), 
                                         mkU32(~X86G_CC_MASK_C)));
            DIP("clc\n");
            break;
         case 0xF9: 
            assign( t1, binop(Iop_Or32, mkexpr(t0), 
                                        mkU32(X86G_CC_MASK_C)));
            DIP("stc\n");
            break;
         case 0xF5: 
            assign( t1, binop(Iop_Xor32, mkexpr(t0), 
                                         mkU32(X86G_CC_MASK_C)));
            DIP("cmc\n");
            break;
         default: 
            vpanic("disInstr(x86)(clc/stc/cmc)");
      }
      stmt( IRStmt_Put( OFFB_CC_OP,   mkU32(X86G_CC_OP_COPY) ));
      stmt( IRStmt_Put( OFFB_CC_DEP2, mkU32(0) ));
      stmt( IRStmt_Put( OFFB_CC_DEP1, mkexpr(t1) ));
      stmt( IRStmt_Put( OFFB_CC_NDEP, mkU32(0) ));
      break;

   case 0xD6: 
      t0 = newTemp(Ity_I32);
      t1 = newTemp(Ity_I32);
      assign( t0,  binop(Iop_And32,
                         mk_x86g_calculate_eflags_c(),
                         mkU32(1)) );
      assign( t1, binop(Iop_Sar32, 
                        binop(Iop_Shl32, mkexpr(t0), mkU8(31)), 
                        mkU8(31)) );
      putIReg(1, R_EAX, unop(Iop_32to8, mkexpr(t1)) );
      DIP("salc\n");
      break;

   
   case 0xF2: { 
      Addr32 eip_orig = guest_EIP_bbstart + delta_start;
      if (sorb != 0) goto decode_failure;
      abyte = getIByte(delta); delta++;

      if (abyte == 0x66) { sz = 2; abyte = getIByte(delta); delta++; }

      switch (abyte) {
      case 0xA4: sz = 1;   
      case 0xA5: 
         dis_REP_op ( &dres, X86CondNZ, dis_MOVS, sz, eip_orig,
                             guest_EIP_bbstart+delta, "repne movs" );
         break;

      case 0xA6: sz = 1;   
      case 0xA7:
         dis_REP_op ( &dres, X86CondNZ, dis_CMPS, sz, eip_orig, 
                             guest_EIP_bbstart+delta, "repne cmps" );
         break;

      case 0xAA: sz = 1;   
      case 0xAB:
         dis_REP_op ( &dres, X86CondNZ, dis_STOS, sz, eip_orig, 
                             guest_EIP_bbstart+delta, "repne stos" );
         break;

      case 0xAE: sz = 1;   
      case 0xAF:
         dis_REP_op ( &dres, X86CondNZ, dis_SCAS, sz, eip_orig,
                             guest_EIP_bbstart+delta, "repne scas" );
         break;

      default:
         goto decode_failure;
      }
      break;
   }

   case 0xF3: { 
      Addr32 eip_orig = guest_EIP_bbstart + delta_start;
      abyte = getIByte(delta); delta++;

      if (abyte == 0x66) { sz = 2; abyte = getIByte(delta); delta++; }

      if (sorb != 0 && abyte != 0x0F) goto decode_failure;

      switch (abyte) {
      case 0x0F:
         switch (getIByte(delta)) {
         
         case 0xBC: 
            delta = dis_bs_E_G ( sorb, sz, delta + 1, True );
            break;
         
         case 0xBD: 
            delta = dis_bs_E_G ( sorb, sz, delta + 1, False );
            break;
         default:
            goto decode_failure;
         }
         break;

      case 0xA4: sz = 1;   
      case 0xA5:
         dis_REP_op ( &dres, X86CondAlways, dis_MOVS, sz, eip_orig, 
                             guest_EIP_bbstart+delta, "rep movs" );
         break;

      case 0xA6: sz = 1;   
      case 0xA7:
         dis_REP_op ( &dres, X86CondZ, dis_CMPS, sz, eip_orig, 
                             guest_EIP_bbstart+delta, "repe cmps" );
         break;

      case 0xAA: sz = 1;   
      case 0xAB:
         dis_REP_op ( &dres, X86CondAlways, dis_STOS, sz, eip_orig, 
                             guest_EIP_bbstart+delta, "rep stos" );
         break;

      case 0xAC: sz = 1;   
      case 0xAD:
         dis_REP_op ( &dres, X86CondAlways, dis_LODS, sz, eip_orig, 
                             guest_EIP_bbstart+delta, "rep lods" );
         break;

      case 0xAE: sz = 1;   
      case 0xAF: 
         dis_REP_op ( &dres, X86CondZ, dis_SCAS, sz, eip_orig, 
                             guest_EIP_bbstart+delta, "repe scas" );
         break;
      
      case 0x90:           
         
         DIP("rep nop (P4 pause)\n");
         jmp_lit(&dres, Ijk_Yield, ((Addr32)guest_EIP_bbstart)+delta);
         vassert(dres.whatNext == Dis_StopHere);
         break;

      case 0xC3:           
         dis_ret(&dres, 0);
         DIP("rep ret\n");
         break;

      default:
         goto decode_failure;
      }
      break;
   }

   

   case 0x86: 
      sz = 1;
      
   case 0x87: 
      modrm = getIByte(delta);
      ty = szToITy(sz);
      t1 = newTemp(ty); t2 = newTemp(ty);
      if (epartIsReg(modrm)) {
         assign(t1, getIReg(sz, eregOfRM(modrm)));
         assign(t2, getIReg(sz, gregOfRM(modrm)));
         putIReg(sz, gregOfRM(modrm), mkexpr(t1));
         putIReg(sz, eregOfRM(modrm), mkexpr(t2));
         delta++;
         DIP("xchg%c %s, %s\n", 
             nameISize(sz), nameIReg(sz,gregOfRM(modrm)), 
                            nameIReg(sz,eregOfRM(modrm)));
      } else {
         *expect_CAS = True;
         addr = disAMode ( &alen, sorb, delta, dis_buf );
         assign( t1, loadLE(ty,mkexpr(addr)) );
         assign( t2, getIReg(sz,gregOfRM(modrm)) );
         casLE( mkexpr(addr),
                mkexpr(t1), mkexpr(t2), guest_EIP_curr_instr );
         putIReg( sz, gregOfRM(modrm), mkexpr(t1) );
         delta += alen;
         DIP("xchg%c %s, %s\n", nameISize(sz), 
                                nameIReg(sz,gregOfRM(modrm)), dis_buf);
      }
      break;

   case 0x90: 
      DIP("nop\n");
      break;
   case 0x91: 
   case 0x92: 
   case 0x93: 
   case 0x94: 
   case 0x95: 
   case 0x96: 
   case 0x97: 
      codegen_xchg_eAX_Reg ( sz, opc - 0x90 );
      break;

   

   case 0xD7: 
      if (sz != 4) goto decode_failure; 
      putIReg( 
         1, 
         R_EAX,
         loadLE(Ity_I8, 
                handleSegOverride( 
                   sorb, 
                   binop(Iop_Add32, 
                         getIReg(4, R_EBX), 
                         unop(Iop_8Uto32, getIReg(1, R_EAX))))));

      DIP("xlat%c [ebx]\n", nameISize(sz));
      break;

   

   case 0xE4: 
      sz = 1; 
      t1 = newTemp(Ity_I32);
      abyte = getIByte(delta); delta++;
      assign(t1, mkU32( abyte & 0xFF ));
      DIP("in%c $%d,%s\n", nameISize(sz), (Int)abyte, nameIReg(sz,R_EAX));
      goto do_IN;
   case 0xE5: 
      vassert(sz == 2 || sz == 4);
      t1 = newTemp(Ity_I32);
      abyte = getIByte(delta); delta++;
      assign(t1, mkU32( abyte & 0xFF ));
      DIP("in%c $%d,%s\n", nameISize(sz), (Int)abyte, nameIReg(sz,R_EAX));
      goto do_IN;
   case 0xEC: 
      sz = 1; 
      t1 = newTemp(Ity_I32);
      assign(t1, unop(Iop_16Uto32, getIReg(2, R_EDX)));
      DIP("in%c %s,%s\n", nameISize(sz), nameIReg(2,R_EDX), 
                                         nameIReg(sz,R_EAX));
      goto do_IN;
   case 0xED: 
      vassert(sz == 2 || sz == 4);
      t1 = newTemp(Ity_I32);
      assign(t1, unop(Iop_16Uto32, getIReg(2, R_EDX)));
      DIP("in%c %s,%s\n", nameISize(sz), nameIReg(2,R_EDX), 
                                         nameIReg(sz,R_EAX));
      goto do_IN;
   do_IN: {
      IRDirty* d;
      vassert(sz == 1 || sz == 2 || sz == 4);
      ty = szToITy(sz);
      t2 = newTemp(Ity_I32);
      d = unsafeIRDirty_1_N( 
             t2,
             0, 
             "x86g_dirtyhelper_IN", 
             &x86g_dirtyhelper_IN,
             mkIRExprVec_2( mkexpr(t1), mkU32(sz) )
          );
      
      stmt( IRStmt_Dirty(d) );
      putIReg(sz, R_EAX, narrowTo( ty, mkexpr(t2) ) );
      break;
   }

   case 0xE6: 
      sz = 1;
      t1 = newTemp(Ity_I32);
      abyte = getIByte(delta); delta++;
      assign( t1, mkU32( abyte & 0xFF ) );
      DIP("out%c %s,$%d\n", nameISize(sz), nameIReg(sz,R_EAX), (Int)abyte);
      goto do_OUT;
   case 0xE7: 
      vassert(sz == 2 || sz == 4);
      t1 = newTemp(Ity_I32);
      abyte = getIByte(delta); delta++;
      assign( t1, mkU32( abyte & 0xFF ) );
      DIP("out%c %s,$%d\n", nameISize(sz), nameIReg(sz,R_EAX), (Int)abyte);
      goto do_OUT;
   case 0xEE: 
      sz = 1;
      t1 = newTemp(Ity_I32);
      assign( t1, unop(Iop_16Uto32, getIReg(2, R_EDX)) );
      DIP("out%c %s,%s\n", nameISize(sz), nameIReg(sz,R_EAX),
                                          nameIReg(2,R_EDX));
      goto do_OUT;
   case 0xEF: 
      vassert(sz == 2 || sz == 4);
      t1 = newTemp(Ity_I32);
      assign( t1, unop(Iop_16Uto32, getIReg(2, R_EDX)) );
      DIP("out%c %s,%s\n", nameISize(sz), nameIReg(sz,R_EAX),
                                          nameIReg(2,R_EDX));
      goto do_OUT;
   do_OUT: {
      IRDirty* d;
      vassert(sz == 1 || sz == 2 || sz == 4);
      ty = szToITy(sz);
      d = unsafeIRDirty_0_N( 
             0, 
             "x86g_dirtyhelper_OUT", 
             &x86g_dirtyhelper_OUT,
             mkIRExprVec_3( mkexpr(t1),
                            widenUto32( getIReg(sz, R_EAX) ), 
                            mkU32(sz) )
          );
      stmt( IRStmt_Dirty(d) );
      break;
   }

   

   case 0x82: 
      
   case 0x80: 
      modrm = getIByte(delta);
      am_sz = lengthAMode(delta);
      sz    = 1;
      d_sz  = 1;
      d32   = getUChar(delta + am_sz);
      delta = dis_Grp1 ( sorb, pfx_lock, delta, modrm, am_sz, d_sz, sz, d32 );
      break;

   case 0x81: 
      modrm = getIByte(delta);
      am_sz = lengthAMode(delta);
      d_sz  = sz;
      d32   = getUDisp(d_sz, delta + am_sz);
      delta = dis_Grp1 ( sorb, pfx_lock, delta, modrm, am_sz, d_sz, sz, d32 );
      break;

   case 0x83: 
      modrm = getIByte(delta);
      am_sz = lengthAMode(delta);
      d_sz  = 1;
      d32   = getSDisp8(delta + am_sz);
      delta = dis_Grp1 ( sorb, pfx_lock, delta, modrm, am_sz, d_sz, sz, d32 );
      break;

   

   case 0xC0: { 
      Bool decode_OK = True;
      modrm = getIByte(delta);
      am_sz = lengthAMode(delta);
      d_sz  = 1;
      d32   = getUChar(delta + am_sz);
      sz    = 1;
      delta = dis_Grp2 ( sorb, delta, modrm, am_sz, d_sz, sz, 
                         mkU8(d32 & 0xFF), NULL, &decode_OK );
      if (!decode_OK)
         goto decode_failure;
      break;
   }
   case 0xC1: { 
      Bool decode_OK = True;
      modrm = getIByte(delta);
      am_sz = lengthAMode(delta);
      d_sz  = 1;
      d32   = getUChar(delta + am_sz);
      delta = dis_Grp2 ( sorb, delta, modrm, am_sz, d_sz, sz, 
                         mkU8(d32 & 0xFF), NULL, &decode_OK );
      if (!decode_OK)
         goto decode_failure;
      break;
   }
   case 0xD0: { 
      Bool decode_OK = True;
      modrm = getIByte(delta);
      am_sz = lengthAMode(delta);
      d_sz  = 0;
      d32   = 1;
      sz    = 1;
      delta = dis_Grp2 ( sorb, delta, modrm, am_sz, d_sz, sz, 
                         mkU8(d32), NULL, &decode_OK );
      if (!decode_OK)
         goto decode_failure;
      break;
   }
   case 0xD1: { 
      Bool decode_OK = True;
      modrm = getUChar(delta);
      am_sz = lengthAMode(delta);
      d_sz  = 0;
      d32   = 1;
      delta = dis_Grp2 ( sorb, delta, modrm, am_sz, d_sz, sz, 
                         mkU8(d32), NULL, &decode_OK );
      if (!decode_OK)
         goto decode_failure;
      break;
   }
   case 0xD2: { 
      Bool decode_OK = True;
      modrm = getUChar(delta);
      am_sz = lengthAMode(delta);
      d_sz  = 0;
      sz    = 1;
      delta = dis_Grp2 ( sorb, delta, modrm, am_sz, d_sz, sz, 
                         getIReg(1,R_ECX), "%cl", &decode_OK );
      if (!decode_OK)
         goto decode_failure;
      break;
   }
   case 0xD3: { 
      Bool decode_OK = True;
      modrm = getIByte(delta);
      am_sz = lengthAMode(delta);
      d_sz  = 0;
      delta = dis_Grp2 ( sorb, delta, modrm, am_sz, d_sz, sz, 
                         getIReg(1,R_ECX), "%cl", &decode_OK );
      if (!decode_OK)
         goto decode_failure;
      break;
   }

   

   case 0xF6: { 
      Bool decode_OK = True;
      delta = dis_Grp3 ( sorb, pfx_lock, 1, delta, &decode_OK );
      if (!decode_OK)
         goto decode_failure;
      break;
   }
   case 0xF7: { 
      Bool decode_OK = True;
      delta = dis_Grp3 ( sorb, pfx_lock, sz, delta, &decode_OK );
      if (!decode_OK)
         goto decode_failure;
      break;
   }

   

   case 0xFE: { 
      Bool decode_OK = True;
      delta = dis_Grp4 ( sorb, pfx_lock, delta, &decode_OK );
      if (!decode_OK)
         goto decode_failure;
      break;
   }

   

   case 0xFF: { 
      Bool decode_OK = True;
      delta = dis_Grp5 ( sorb, pfx_lock, sz, delta, &dres, &decode_OK );
      if (!decode_OK)
         goto decode_failure;
      break;
   }

   

   case 0x0F: {
      opc = getIByte(delta); delta++;
      switch (opc) {

      

      case 0xBA: { 
         Bool decode_OK = False;
         modrm = getUChar(delta);
         am_sz = lengthAMode(delta);
         d32   = getSDisp8(delta + am_sz);
         delta = dis_Grp8_Imm ( sorb, pfx_lock, delta, modrm, 
                                am_sz, sz, d32, &decode_OK );
         if (!decode_OK)
            goto decode_failure;
         break;
      }

      

      case 0xBC: 
         delta = dis_bs_E_G ( sorb, sz, delta, True );
         break;
      case 0xBD: 
         delta = dis_bs_E_G ( sorb, sz, delta, False );
         break;

      

      case 0xC8: 
      case 0xC9:
      case 0xCA:
      case 0xCB:
      case 0xCC:
      case 0xCD:
      case 0xCE:
      case 0xCF: 
         
         if (sz != 4) goto decode_failure;
         
         t1 = newTemp(Ity_I32);
         assign( t1, getIReg(4, opc-0xC8) );
         t2 = math_BSWAP(t1, Ity_I32);

         putIReg(4, opc-0xC8, mkexpr(t2));
         DIP("bswapl %s\n", nameIReg(4, opc-0xC8));
         break;

      

      case 0xA3: 
         delta = dis_bt_G_E ( vbi, sorb, pfx_lock, sz, delta, BtOpNone );
         break;
      case 0xB3: 
         delta = dis_bt_G_E ( vbi, sorb, pfx_lock, sz, delta, BtOpReset );
         break;
      case 0xAB: 
         delta = dis_bt_G_E ( vbi, sorb, pfx_lock, sz, delta, BtOpSet );
         break;
      case 0xBB: 
         delta = dis_bt_G_E ( vbi, sorb, pfx_lock, sz, delta, BtOpComp );
         break;

      
 
      case 0x40:
      case 0x41:
      case 0x42: 
      case 0x43: 
      case 0x44: 
      case 0x45: 
      case 0x46: 
      case 0x47: 
      case 0x48: 
      case 0x49: 
      case 0x4A: 
      case 0x4B: 
      case 0x4C: 
      case 0x4D: 
      case 0x4E: 
      case 0x4F: 
         delta = dis_cmov_E_G(sorb, sz, (X86Condcode)(opc - 0x40), delta);
         break;

      

      case 0xB0: 
         delta = dis_cmpxchg_G_E ( sorb, pfx_lock, 1, delta );
         break;
      case 0xB1: 
         delta = dis_cmpxchg_G_E ( sorb, pfx_lock, sz, delta );
         break;

      case 0xC7: { 
         IRTemp expdHi    = newTemp(Ity_I32);
         IRTemp expdLo    = newTemp(Ity_I32);
         IRTemp dataHi    = newTemp(Ity_I32);
         IRTemp dataLo    = newTemp(Ity_I32);
         IRTemp oldHi     = newTemp(Ity_I32);
         IRTemp oldLo     = newTemp(Ity_I32);
         IRTemp flags_old = newTemp(Ity_I32);
         IRTemp flags_new = newTemp(Ity_I32);
         IRTemp success   = newTemp(Ity_I1);

         *expect_CAS = True;

	 
         if (sz != 4) goto decode_failure;
         modrm = getIByte(delta);
         if (epartIsReg(modrm)) goto decode_failure;
         if (gregOfRM(modrm) != 1) goto decode_failure;
         addr = disAMode ( &alen, sorb, delta, dis_buf );
         delta += alen;

         
         assign( expdHi, getIReg(4,R_EDX) );
         assign( expdLo, getIReg(4,R_EAX) );
         assign( dataHi, getIReg(4,R_ECX) );
         assign( dataLo, getIReg(4,R_EBX) );

         
         stmt( IRStmt_CAS(
                  mkIRCAS( oldHi, oldLo, 
                           Iend_LE, mkexpr(addr), 
                           mkexpr(expdHi), mkexpr(expdLo),
                           mkexpr(dataHi), mkexpr(dataLo)
               )));

         
         assign( success,
                 binop(Iop_CasCmpEQ32,
                       binop(Iop_Or32,
                             binop(Iop_Xor32, mkexpr(oldHi), mkexpr(expdHi)),
                             binop(Iop_Xor32, mkexpr(oldLo), mkexpr(expdLo))
                       ),
                       mkU32(0)
                 ));

         putIReg(4, R_EDX,
                    IRExpr_ITE( mkexpr(success),
                                mkexpr(expdHi), mkexpr(oldHi)
                ));
         putIReg(4, R_EAX,
                    IRExpr_ITE( mkexpr(success),
                                mkexpr(expdLo), mkexpr(oldLo)
                ));

         assign( flags_old, widenUto32(mk_x86g_calculate_eflags_all()));
         assign( 
            flags_new,
            binop(Iop_Or32,
                  binop(Iop_And32, mkexpr(flags_old), 
                                   mkU32(~X86G_CC_MASK_Z)),
                  binop(Iop_Shl32, 
                        binop(Iop_And32, 
                              unop(Iop_1Uto32, mkexpr(success)), mkU32(1)), 
                        mkU8(X86G_CC_SHIFT_Z)) ));

         stmt( IRStmt_Put( OFFB_CC_OP,   mkU32(X86G_CC_OP_COPY) ));
         stmt( IRStmt_Put( OFFB_CC_DEP1, mkexpr(flags_new) ));
         stmt( IRStmt_Put( OFFB_CC_DEP2, mkU32(0) ));
         stmt( IRStmt_Put( OFFB_CC_NDEP, mkU32(0) ));


	 DIP("cmpxchg8b %s\n", dis_buf);
	 break;
      }

      

      case 0xA2: { 
         IRDirty* d     = NULL;
         void*    fAddr = NULL;
         const HChar* fName = NULL;
         if (archinfo->hwcaps & VEX_HWCAPS_X86_SSE2) {
            fName = "x86g_dirtyhelper_CPUID_sse2";
            fAddr = &x86g_dirtyhelper_CPUID_sse2; 
         } 
         else
         if (archinfo->hwcaps & VEX_HWCAPS_X86_SSE1) {
            fName = "x86g_dirtyhelper_CPUID_sse1";
            fAddr = &x86g_dirtyhelper_CPUID_sse1; 
         } 
         else
         if (archinfo->hwcaps & VEX_HWCAPS_X86_MMXEXT) {
            fName = "x86g_dirtyhelper_CPUID_mmxext";
            fAddr = &x86g_dirtyhelper_CPUID_mmxext;
         }
         else
         if (archinfo->hwcaps == 0) {
            fName = "x86g_dirtyhelper_CPUID_sse0";
            fAddr = &x86g_dirtyhelper_CPUID_sse0; 
         } else
            vpanic("disInstr(x86)(cpuid)");

         vassert(fName); vassert(fAddr);
         d = unsafeIRDirty_0_N ( 0, 
                                 fName, fAddr, mkIRExprVec_1(IRExpr_BBPTR()) );
         
         d->nFxState = 4;
         vex_bzero(&d->fxState, sizeof(d->fxState));
         d->fxState[0].fx     = Ifx_Modify;
         d->fxState[0].offset = OFFB_EAX;
         d->fxState[0].size   = 4;
         d->fxState[1].fx     = Ifx_Write;
         d->fxState[1].offset = OFFB_EBX;
         d->fxState[1].size   = 4;
         d->fxState[2].fx     = Ifx_Modify;
         d->fxState[2].offset = OFFB_ECX;
         d->fxState[2].size   = 4;
         d->fxState[3].fx     = Ifx_Write;
         d->fxState[3].offset = OFFB_EDX;
         d->fxState[3].size   = 4;
         
         stmt( IRStmt_Dirty(d) );
         stmt( IRStmt_MBE(Imbe_Fence) );
         DIP("cpuid\n");
         break;
      }

      

      case 0xB6: 
         if (sz != 2 && sz != 4)
            goto decode_failure;
         delta = dis_movx_E_G ( sorb, delta, 1, sz, False );
         break;

      case 0xB7: 
         if (sz != 4)
            goto decode_failure;
         delta = dis_movx_E_G ( sorb, delta, 2, 4, False );
         break;

      case 0xBE: 
         if (sz != 2 && sz != 4)
            goto decode_failure;
         delta = dis_movx_E_G ( sorb, delta, 1, sz, True );
         break;

      case 0xBF: 
         if (sz != 4 && sz != 2)
            goto decode_failure;
         delta = dis_movx_E_G ( sorb, delta, 2, sz, True );
         break;


      

      case 0xAF: 
         delta = dis_mul_E_G ( sorb, sz, delta );
         break;

      

      case 0x1F:
         modrm = getUChar(delta);
         if (epartIsReg(modrm)) goto decode_failure;
         addr = disAMode ( &alen, sorb, delta, dis_buf );
         delta += alen;
         DIP("nop%c %s\n", nameISize(sz), dis_buf);
         break;

      
      case 0x80:
      case 0x81:
      case 0x82: 
      case 0x83: 
      case 0x84: 
      case 0x85: 
      case 0x86: 
      case 0x87: 
      case 0x88: 
      case 0x89: 
      case 0x8A: 
      case 0x8B: 
      case 0x8C: 
      case 0x8D: 
      case 0x8E: 
      case 0x8F: 
       { Int    jmpDelta;
         const HChar* comment  = "";
         jmpDelta = (Int)getUDisp32(delta);
         d32 = (((Addr32)guest_EIP_bbstart)+delta+4) + jmpDelta;
         delta += 4;
         if (resteerCisOk
             && vex_control.guest_chase_cond
             && (Addr32)d32 != (Addr32)guest_EIP_bbstart
             && jmpDelta < 0
             && resteerOkFn( callback_opaque, (Addr32)d32) ) {
            stmt( IRStmt_Exit( 
                     mk_x86g_calculate_condition((X86Condcode)
                                                 (1 ^ (opc - 0x80))),
                     Ijk_Boring,
                     IRConst_U32(guest_EIP_bbstart+delta),
                     OFFB_EIP ) );
            dres.whatNext   = Dis_ResteerC;
            dres.continueAt = (Addr32)d32;
            comment = "(assumed taken)";
         }
         else
         if (resteerCisOk
             && vex_control.guest_chase_cond
             && (Addr32)d32 != (Addr32)guest_EIP_bbstart
             && jmpDelta >= 0
             && resteerOkFn( callback_opaque, 
                             (Addr32)(guest_EIP_bbstart+delta)) ) {
            stmt( IRStmt_Exit( 
                     mk_x86g_calculate_condition((X86Condcode)(opc - 0x80)),
                     Ijk_Boring,
                     IRConst_U32(d32),
                     OFFB_EIP ) );
            dres.whatNext   = Dis_ResteerC;
            dres.continueAt = guest_EIP_bbstart + delta;
            comment = "(assumed not taken)";
         }
         else {
            jcc_01( &dres, (X86Condcode)(opc - 0x80), 
                    (Addr32)(guest_EIP_bbstart+delta), d32);
            vassert(dres.whatNext == Dis_StopHere);
         }
         DIP("j%s-32 0x%x %s\n", name_X86Condcode(opc - 0x80), d32, comment);
         break;
       }

      
      case 0x31: { 
         IRTemp   val  = newTemp(Ity_I64);
         IRExpr** args = mkIRExprVec_0();
         IRDirty* d    = unsafeIRDirty_1_N ( 
                            val, 
                            0, 
                            "x86g_dirtyhelper_RDTSC", 
                            &x86g_dirtyhelper_RDTSC, 
                            args 
                         );
         
         stmt( IRStmt_Dirty(d) );
         putIReg(4, R_EDX, unop(Iop_64HIto32, mkexpr(val)));
         putIReg(4, R_EAX, unop(Iop_64to32, mkexpr(val)));
         DIP("rdtsc\n");
         break;
      }

      

      case 0xA1: 
         dis_pop_segreg( R_FS, sz ); break;
      case 0xA9: 
         dis_pop_segreg( R_GS, sz ); break;

      case 0xA0: 
         dis_push_segreg( R_FS, sz ); break;
      case 0xA8: 
         dis_push_segreg( R_GS, sz ); break;

      
      case 0x90:
      case 0x91:
      case 0x92: 
      case 0x93: 
      case 0x94: 
      case 0x95: 
      case 0x96: 
      case 0x97: 
      case 0x98: 
      case 0x99: 
      case 0x9A: 
      case 0x9B: 
      case 0x9C: 
      case 0x9D: 
      case 0x9E: 
      case 0x9F: 
         t1 = newTemp(Ity_I8);
         assign( t1, unop(Iop_1Uto8,mk_x86g_calculate_condition(opc-0x90)) );
         modrm = getIByte(delta);
         if (epartIsReg(modrm)) {
            delta++;
            putIReg(1, eregOfRM(modrm), mkexpr(t1));
            DIP("set%s %s\n", name_X86Condcode(opc-0x90), 
                              nameIReg(1,eregOfRM(modrm)));
         } else {
           addr = disAMode ( &alen, sorb, delta, dis_buf );
           delta += alen;
           storeLE( mkexpr(addr), mkexpr(t1) );
           DIP("set%s %s\n", name_X86Condcode(opc-0x90), dis_buf);
         }
         break;

      

      case 0xA4: 
         modrm = getIByte(delta);
         d32   = delta + lengthAMode(delta);
         vex_sprintf(dis_buf, "$%d", getIByte(d32));
         delta = dis_SHLRD_Gv_Ev ( 
                  sorb, delta, modrm, sz, 
                  mkU8(getIByte(d32)), True, 
                  dis_buf, True );
         break;
      case 0xA5: 
         modrm = getIByte(delta);
         delta = dis_SHLRD_Gv_Ev ( 
                    sorb, delta, modrm, sz,
                    getIReg(1,R_ECX), False, 
                    "%cl", True );
         break;

      case 0xAC: 
         modrm = getIByte(delta);
         d32   = delta + lengthAMode(delta);
         vex_sprintf(dis_buf, "$%d", getIByte(d32));
         delta = dis_SHLRD_Gv_Ev ( 
                    sorb, delta, modrm, sz, 
                    mkU8(getIByte(d32)), True, 
                    dis_buf, False );
         break;
      case 0xAD: 
         modrm = getIByte(delta);
         delta = dis_SHLRD_Gv_Ev ( 
                    sorb, delta, modrm, sz, 
                    getIReg(1,R_ECX), False, 
                    "%cl", False );
         break;

      

      case 0x34:

         stmt( IRStmt_Put( OFFB_IP_AT_SYSCALL,
                           mkU32(guest_EIP_curr_instr) ) );
         jmp_lit(&dres, Ijk_Sys_sysenter, 0);
         vassert(dres.whatNext == Dis_StopHere);
         DIP("sysenter");
         break;

      

      case 0xC0: { 
         Bool decodeOK;
         delta = dis_xadd_G_E ( sorb, pfx_lock, 1, delta, &decodeOK );
         if (!decodeOK) goto decode_failure;
         break;
      }
      case 0xC1: { 
         Bool decodeOK;
         delta = dis_xadd_G_E ( sorb, pfx_lock, sz, delta, &decodeOK );
         if (!decodeOK) goto decode_failure;
         break;
      }

      

      case 0x71: 
      case 0x72: 
      case 0x73: 

      case 0x6E: 
      case 0x7E: 
      case 0x7F: 
      case 0x6F: 

      case 0xFC: 
      case 0xFD: 
      case 0xFE: 

      case 0xEC: 
      case 0xED: 

      case 0xDC:
      case 0xDD: 

      case 0xF8: 
      case 0xF9: 
      case 0xFA: 

      case 0xE8: 
      case 0xE9: 

      case 0xD8: 
      case 0xD9: 

      case 0xE5: 
      case 0xD5: 

      case 0xF5: 

      case 0x74: 
      case 0x75: 
      case 0x76: 

      case 0x64: 
      case 0x65: 
      case 0x66: 

      case 0x6B: 
      case 0x63: 
      case 0x67: 

      case 0x68: 
      case 0x69: 
      case 0x6A: 

      case 0x60: 
      case 0x61: 
      case 0x62: 

      case 0xDB: 
      case 0xDF: 
      case 0xEB: 
      case 0xEF: 

      case 0xF1: 
      case 0xF2: 
      case 0xF3: 

      case 0xD1: 
      case 0xD2: 
      case 0xD3: 

      case 0xE1: 
      case 0xE2: 
      {
         Int  delta0    = delta-1;
         Bool decode_OK = False;

         if (sz != 4)
            goto decode_failure;

         delta = dis_MMX ( &decode_OK, sorb, sz, delta-1 );
         if (!decode_OK) {
            delta = delta0;
            goto decode_failure;
         }
         break;
      }

      case 0x0E: 
      case 0x77: 
         if (sz != 4)
            goto decode_failure;
         do_EMMS_preamble();
         DIP("{f}emms\n");
         break;

      
      case 0x01: 
                 
      {
         modrm = getUChar(delta);
         addr = disAMode ( &alen, sorb, delta, dis_buf );
         delta += alen;
         if (epartIsReg(modrm)) goto decode_failure;
         if (gregOfRM(modrm) != 0 && gregOfRM(modrm) != 1)
            goto decode_failure;
         switch (gregOfRM(modrm)) {
            case 0: DIP("sgdt %s\n", dis_buf); break;
            case 1: DIP("sidt %s\n", dis_buf); break;
            default: vassert(0); 
         }

         IRDirty* d = unsafeIRDirty_0_N (
                          0,
                          "x86g_dirtyhelper_SxDT",
                          &x86g_dirtyhelper_SxDT,
                          mkIRExprVec_2( mkexpr(addr),
                                         mkU32(gregOfRM(modrm)) )
                      );
         
         d->mFx   = Ifx_Write;
         d->mAddr = mkexpr(addr);
         d->mSize = 6;
         stmt( IRStmt_Dirty(d) );
         break;
      }

      case 0x05: 
         stmt( IRStmt_Put( OFFB_IP_AT_SYSCALL,
              mkU32(guest_EIP_curr_instr) ) );
         jmp_lit(&dres, Ijk_Sys_syscall, ((Addr32)guest_EIP_bbstart)+delta);
         vassert(dres.whatNext == Dis_StopHere);
         DIP("syscall\n");
         break;

      

      default:
         goto decode_failure;
   } 
   goto decode_success;
   } 

   
  
  default:
  decode_failure:
   
   if (sigill_diag) {
      vex_printf("vex x86->IR: unhandled instruction bytes: "
                 "0x%x 0x%x 0x%x 0x%x\n",
                 (Int)getIByte(delta_start+0),
                 (Int)getIByte(delta_start+1),
                 (Int)getIByte(delta_start+2),
                 (Int)getIByte(delta_start+3) );
   }

   stmt( IRStmt_Put( OFFB_EIP, mkU32(guest_EIP_curr_instr) ) );
   jmp_lit(&dres, Ijk_NoDecode, guest_EIP_curr_instr);
   vassert(dres.whatNext == Dis_StopHere);
   dres.len = 0;
   *expect_CAS = False;
   return dres;

   } 

  decode_success:
   
   switch (dres.whatNext) {
      case Dis_Continue:
         stmt( IRStmt_Put( OFFB_EIP, mkU32(guest_EIP_bbstart + delta) ) );
         break;
      case Dis_ResteerU:
      case Dis_ResteerC:
         stmt( IRStmt_Put( OFFB_EIP, mkU32(dres.continueAt) ) );
         break;
      case Dis_StopHere:
         break;
      default:
         vassert(0);
   }

   DIP("\n");
   dres.len = delta - delta_start;
   return dres;
}

#undef DIP
#undef DIS




DisResult disInstr_X86 ( IRSB*        irsb_IN,
                         Bool         (*resteerOkFn) ( void*, Addr ),
                         Bool         resteerCisOk,
                         void*        callback_opaque,
                         const UChar* guest_code_IN,
                         Long         delta,
                         Addr         guest_IP,
                         VexArch      guest_arch,
                         const VexArchInfo* archinfo,
                         const VexAbiInfo*  abiinfo,
                         VexEndness   host_endness_IN,
                         Bool         sigill_diag_IN )
{
   Int       i, x1, x2;
   Bool      expect_CAS, has_CAS;
   DisResult dres;

   
   vassert(guest_arch == VexArchX86);
   guest_code           = guest_code_IN;
   irsb                 = irsb_IN;
   host_endness         = host_endness_IN;
   guest_EIP_curr_instr = (Addr32)guest_IP;
   guest_EIP_bbstart    = (Addr32)toUInt(guest_IP - delta);

   x1 = irsb_IN->stmts_used;
   expect_CAS = False;
   dres = disInstr_X86_WRK ( &expect_CAS, resteerOkFn,
                             resteerCisOk,
                             callback_opaque,
                             delta, archinfo, abiinfo, sigill_diag_IN );
   x2 = irsb_IN->stmts_used;
   vassert(x2 >= x1);

   has_CAS = False;
   for (i = x1; i < x2; i++) {
      if (irsb_IN->stmts[i]->tag == Ist_CAS)
         has_CAS = True;
   }

   if (expect_CAS != has_CAS) {
      vex_traceflags |= VEX_TRACE_FE;
      dres = disInstr_X86_WRK ( &expect_CAS, resteerOkFn,
                                resteerCisOk,
                                callback_opaque,
                                delta, archinfo, abiinfo, sigill_diag_IN );
      for (i = x1; i < x2; i++) {
         vex_printf("\t\t");
         ppIRStmt(irsb_IN->stmts[i]);
         vex_printf("\n");
      }
      vpanic("disInstr_X86: inconsistency in LOCK prefix handling");
   }

   return dres;
}


