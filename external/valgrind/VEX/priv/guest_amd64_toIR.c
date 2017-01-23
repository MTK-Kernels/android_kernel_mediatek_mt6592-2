

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
   to ensure a 64-bit value is being written.

   x87 FP Limitations:
 
   * all arithmetic done at 64 bits
 
   * no FP exceptions, except for handling stack over/underflow
 
   * FP rounding mode observed only for float->int conversions and
     int->float conversions which could lose accuracy, and for
     float-to-float rounding.  For all other operations,
     round-to-nearest is used, regardless.
 
   * some of the FCOM cases could do with testing -- not convinced
     that the args are the right way round.
 
   * FSAVE does not re-initialise the FPU; it should do
 
   * FINIT not only initialises the FPU environment, it also zeroes
     all the FP registers.  It should leave the registers unchanged.
 
    SAHF should cause eflags[1] == 1, and in fact it produces 0.  As
    per Intel docs this bit has no meaning anyway.  Since PUSHF is the
    only way to observe eflags[1], a proper fix would be to make that
    bit be set by PUSHF.
 
    This module uses global variables and so is not MT-safe (if that
    should ever become relevant).
*/






#include "libvex_basictypes.h"
#include "libvex_ir.h"
#include "libvex.h"
#include "libvex_guest_amd64.h"

#include "main_util.h"
#include "main_globals.h"
#include "guest_generic_bb_to_IR.h"
#include "guest_generic_x87.h"
#include "guest_amd64_defs.h"





static VexEndness host_endness;

static const UChar* guest_code;

static Addr64 guest_RIP_bbstart;

static Addr64 guest_RIP_curr_instr;

static IRSB* irsb;



static Addr64 guest_RIP_next_assumed;
static Bool   guest_RIP_next_mustcheck;


 
static IRTemp newTemp ( IRType ty )
{
   vassert(isPlausibleIRType(ty));
   return newIRTemp( irsb->tyenv, ty );
}

static void stmt ( IRStmt* st )
{
   addStmtToIRSB( irsb, st );
}

 
static void assign ( IRTemp dst, IRExpr* e )
{
   stmt( IRStmt_WrTmp(dst, e) );
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

static IRExpr* mkU8 ( ULong i )
{
   vassert(i < 256);
   return IRExpr_Const(IRConst_U8( (UChar)i ));
}

static IRExpr* mkU16 ( ULong i )
{
   vassert(i < 0x10000ULL);
   return IRExpr_Const(IRConst_U16( (UShort)i ));
}

static IRExpr* mkU32 ( ULong i )
{
   vassert(i < 0x100000000ULL);
   return IRExpr_Const(IRConst_U32( (UInt)i ));
}

static IRExpr* mkU64 ( ULong i )
{
   return IRExpr_Const(IRConst_U64(i));
}

static IRExpr* mkU ( IRType ty, ULong i )
{
   switch (ty) {
      case Ity_I8:  return mkU8(i);
      case Ity_I16: return mkU16(i);
      case Ity_I32: return mkU32(i);
      case Ity_I64: return mkU64(i);
      default: vpanic("mkU(amd64)");
   }
}

static void storeLE ( IRExpr* addr, IRExpr* data )
{
   stmt( IRStmt_Store(Iend_LE, addr, data) );
}

static IRExpr* loadLE ( IRType ty, IRExpr* addr )
{
   return IRExpr_Load(Iend_LE, ty, addr);
}

static IROp mkSizedOp ( IRType ty, IROp op8 )
{
   vassert(op8 == Iop_Add8 || op8 == Iop_Sub8 
           || op8 == Iop_Mul8 
           || op8 == Iop_Or8 || op8 == Iop_And8 || op8 == Iop_Xor8
           || op8 == Iop_Shl8 || op8 == Iop_Shr8 || op8 == Iop_Sar8
           || op8 == Iop_CmpEQ8 || op8 == Iop_CmpNE8
           || op8 == Iop_CasCmpNE8
           || op8 == Iop_Not8 );
   switch (ty) {
      case Ity_I8:  return 0 +op8;
      case Ity_I16: return 1 +op8;
      case Ity_I32: return 2 +op8;
      case Ity_I64: return 3 +op8;
      default: vpanic("mkSizedOp(amd64)");
   }
}

static 
IRExpr* doScalarWidening ( Int szSmall, Int szBig, Bool signd, IRExpr* src )
{
   if (szSmall == 1 && szBig == 4) {
      return unop(signd ? Iop_8Sto32 : Iop_8Uto32, src);
   }
   if (szSmall == 1 && szBig == 2) {
      return unop(signd ? Iop_8Sto16 : Iop_8Uto16, src);
   }
   if (szSmall == 2 && szBig == 4) {
      return unop(signd ? Iop_16Sto32 : Iop_16Uto32, src);
   }
   if (szSmall == 1 && szBig == 8 && !signd) {
      return unop(Iop_8Uto64, src);
   }
   if (szSmall == 1 && szBig == 8 && signd) {
      return unop(Iop_8Sto64, src);
   }
   if (szSmall == 2 && szBig == 8 && !signd) {
      return unop(Iop_16Uto64, src);
   }
   if (szSmall == 2 && szBig == 8 && signd) {
      return unop(Iop_16Sto64, src);
   }
   vpanic("doScalarWidening(amd64)");
}




__attribute__ ((noreturn))
static void unimplemented ( const HChar* str )
{
   vex_printf("amd64toIR: unimplemented feature\n");
   vpanic(str);
}

#define DIP(format, args...)           \
   if (vex_traceflags & VEX_TRACE_FE)  \
      vex_printf(format, ## args)

#define DIS(buf, format, args...)      \
   if (vex_traceflags & VEX_TRACE_FE)  \
      vex_sprintf(buf, format, ## args)



#define OFFB_RAX       offsetof(VexGuestAMD64State,guest_RAX)
#define OFFB_RBX       offsetof(VexGuestAMD64State,guest_RBX)
#define OFFB_RCX       offsetof(VexGuestAMD64State,guest_RCX)
#define OFFB_RDX       offsetof(VexGuestAMD64State,guest_RDX)
#define OFFB_RSP       offsetof(VexGuestAMD64State,guest_RSP)
#define OFFB_RBP       offsetof(VexGuestAMD64State,guest_RBP)
#define OFFB_RSI       offsetof(VexGuestAMD64State,guest_RSI)
#define OFFB_RDI       offsetof(VexGuestAMD64State,guest_RDI)
#define OFFB_R8        offsetof(VexGuestAMD64State,guest_R8)
#define OFFB_R9        offsetof(VexGuestAMD64State,guest_R9)
#define OFFB_R10       offsetof(VexGuestAMD64State,guest_R10)
#define OFFB_R11       offsetof(VexGuestAMD64State,guest_R11)
#define OFFB_R12       offsetof(VexGuestAMD64State,guest_R12)
#define OFFB_R13       offsetof(VexGuestAMD64State,guest_R13)
#define OFFB_R14       offsetof(VexGuestAMD64State,guest_R14)
#define OFFB_R15       offsetof(VexGuestAMD64State,guest_R15)

#define OFFB_RIP       offsetof(VexGuestAMD64State,guest_RIP)

#define OFFB_FS_CONST  offsetof(VexGuestAMD64State,guest_FS_CONST)
#define OFFB_GS_CONST  offsetof(VexGuestAMD64State,guest_GS_CONST)

#define OFFB_CC_OP     offsetof(VexGuestAMD64State,guest_CC_OP)
#define OFFB_CC_DEP1   offsetof(VexGuestAMD64State,guest_CC_DEP1)
#define OFFB_CC_DEP2   offsetof(VexGuestAMD64State,guest_CC_DEP2)
#define OFFB_CC_NDEP   offsetof(VexGuestAMD64State,guest_CC_NDEP)

#define OFFB_FPREGS    offsetof(VexGuestAMD64State,guest_FPREG[0])
#define OFFB_FPTAGS    offsetof(VexGuestAMD64State,guest_FPTAG[0])
#define OFFB_DFLAG     offsetof(VexGuestAMD64State,guest_DFLAG)
#define OFFB_ACFLAG    offsetof(VexGuestAMD64State,guest_ACFLAG)
#define OFFB_IDFLAG    offsetof(VexGuestAMD64State,guest_IDFLAG)
#define OFFB_FTOP      offsetof(VexGuestAMD64State,guest_FTOP)
#define OFFB_FC3210    offsetof(VexGuestAMD64State,guest_FC3210)
#define OFFB_FPROUND   offsetof(VexGuestAMD64State,guest_FPROUND)

#define OFFB_SSEROUND  offsetof(VexGuestAMD64State,guest_SSEROUND)
#define OFFB_YMM0      offsetof(VexGuestAMD64State,guest_YMM0)
#define OFFB_YMM1      offsetof(VexGuestAMD64State,guest_YMM1)
#define OFFB_YMM2      offsetof(VexGuestAMD64State,guest_YMM2)
#define OFFB_YMM3      offsetof(VexGuestAMD64State,guest_YMM3)
#define OFFB_YMM4      offsetof(VexGuestAMD64State,guest_YMM4)
#define OFFB_YMM5      offsetof(VexGuestAMD64State,guest_YMM5)
#define OFFB_YMM6      offsetof(VexGuestAMD64State,guest_YMM6)
#define OFFB_YMM7      offsetof(VexGuestAMD64State,guest_YMM7)
#define OFFB_YMM8      offsetof(VexGuestAMD64State,guest_YMM8)
#define OFFB_YMM9      offsetof(VexGuestAMD64State,guest_YMM9)
#define OFFB_YMM10     offsetof(VexGuestAMD64State,guest_YMM10)
#define OFFB_YMM11     offsetof(VexGuestAMD64State,guest_YMM11)
#define OFFB_YMM12     offsetof(VexGuestAMD64State,guest_YMM12)
#define OFFB_YMM13     offsetof(VexGuestAMD64State,guest_YMM13)
#define OFFB_YMM14     offsetof(VexGuestAMD64State,guest_YMM14)
#define OFFB_YMM15     offsetof(VexGuestAMD64State,guest_YMM15)
#define OFFB_YMM16     offsetof(VexGuestAMD64State,guest_YMM16)

#define OFFB_EMNOTE    offsetof(VexGuestAMD64State,guest_EMNOTE)
#define OFFB_CMSTART   offsetof(VexGuestAMD64State,guest_CMSTART)
#define OFFB_CMLEN     offsetof(VexGuestAMD64State,guest_CMLEN)

#define OFFB_NRADDR    offsetof(VexGuestAMD64State,guest_NRADDR)



#define R_RAX 0
#define R_RCX 1
#define R_RDX 2
#define R_RBX 3
#define R_RSP 4
#define R_RBP 5
#define R_RSI 6
#define R_RDI 7
#define R_R8  8
#define R_R9  9
#define R_R10 10
#define R_R11 11
#define R_R12 12
#define R_R13 13
#define R_R14 14
#define R_R15 15

#define R_ES 0
#define R_CS 1
#define R_SS 2
#define R_DS 3
#define R_FS 4
#define R_GS 5



static ULong extend_s_8to64 ( UChar x )
{
   return (ULong)((Long)(((ULong)x) << 56) >> 56);
}

static ULong extend_s_16to64 ( UShort x )
{
   return (ULong)((Long)(((ULong)x) << 48) >> 48);
}

static ULong extend_s_32to64 ( UInt x )
{
   return (ULong)((Long)(((ULong)x) << 32) >> 32);
}

inline
static Bool epartIsReg ( UChar mod_reg_rm )
{
   return toBool(0xC0 == (mod_reg_rm & 0xC0));
}

inline
static Int gregLO3ofRM ( UChar mod_reg_rm )
{
   return (Int)( (mod_reg_rm >> 3) & 7 );
}

inline
static Int eregLO3ofRM ( UChar mod_reg_rm )
{
   return (Int)(mod_reg_rm & 0x7);
}


static inline UChar getUChar ( Long delta )
{
   UChar v = guest_code[delta+0];
   return v;
}

static UInt getUDisp16 ( Long delta )
{
   UInt v = guest_code[delta+1]; v <<= 8;
   v |= guest_code[delta+0];
   return v & 0xFFFF;
}



static Long getSDisp8 ( Long delta )
{
   return extend_s_8to64( guest_code[delta] );
}

static Long getSDisp16 ( Long delta )
{
   UInt v = guest_code[delta+1]; v <<= 8;
   v |= guest_code[delta+0];
   return extend_s_16to64( (UShort)v );
}

static Long getSDisp32 ( Long delta )
{
   UInt v = guest_code[delta+3]; v <<= 8;
   v |= guest_code[delta+2]; v <<= 8;
   v |= guest_code[delta+1]; v <<= 8;
   v |= guest_code[delta+0];
   return extend_s_32to64( v );
}

static Long getDisp64 ( Long delta )
{
   ULong v = 0;
   v |= guest_code[delta+7]; v <<= 8;
   v |= guest_code[delta+6]; v <<= 8;
   v |= guest_code[delta+5]; v <<= 8;
   v |= guest_code[delta+4]; v <<= 8;
   v |= guest_code[delta+3]; v <<= 8;
   v |= guest_code[delta+2]; v <<= 8;
   v |= guest_code[delta+1]; v <<= 8;
   v |= guest_code[delta+0];
   return v;
}

static Long getSDisp ( Int size, Long delta )
{
   switch (size) {
      case 4: return getSDisp32(delta);
      case 2: return getSDisp16(delta);
      case 1: return getSDisp8(delta);
      default: vpanic("getSDisp(amd64)");
  }
}

static ULong mkSizeMask ( Int sz )
{
   switch (sz) {
      case 1: return 0x00000000000000FFULL;
      case 2: return 0x000000000000FFFFULL;
      case 4: return 0x00000000FFFFFFFFULL;
      case 8: return 0xFFFFFFFFFFFFFFFFULL;
      default: vpanic("mkSzMask(amd64)");
   }
}

static Int imin ( Int a, Int b )
{
   return (a < b) ? a : b;
}

static IRType szToITy ( Int n )
{
   switch (n) {
      case 1: return Ity_I8;
      case 2: return Ity_I16;
      case 4: return Ity_I32;
      case 8: return Ity_I64;
      default: vex_printf("\nszToITy(%d)\n", n);
               vpanic("szToITy(amd64)");
   }
}




typedef UInt  Prefix;

#define PFX_ASO    (1<<0)    
#define PFX_66     (1<<1)    
#define PFX_REX    (1<<2)    
#define PFX_REXW   (1<<3)    
#define PFX_REXR   (1<<4)    
#define PFX_REXX   (1<<5)    
#define PFX_REXB   (1<<6)    
#define PFX_LOCK   (1<<7)    
#define PFX_F2     (1<<8)    
#define PFX_F3     (1<<9)    
#define PFX_CS     (1<<10)   
#define PFX_DS     (1<<11)   
#define PFX_ES     (1<<12)   
#define PFX_FS     (1<<13)   
#define PFX_GS     (1<<14)   
#define PFX_SS     (1<<15)   
#define PFX_VEX    (1<<16)   
#define PFX_VEXL   (1<<17)   
#define PFX_VEXnV0 (1<<18)   
#define PFX_VEXnV1 (1<<19)   
#define PFX_VEXnV2 (1<<20)   
#define PFX_VEXnV3 (1<<21)   


#define PFX_EMPTY 0x55000000

static Bool IS_VALID_PFX ( Prefix pfx ) {
   return toBool((pfx & 0xFF000000) == PFX_EMPTY);
}

static Bool haveREX ( Prefix pfx ) {
   return toBool(pfx & PFX_REX);
}

static Int getRexW ( Prefix pfx ) {
   return (pfx & PFX_REXW) ? 1 : 0;
}
static Int getRexR ( Prefix pfx ) {
   return (pfx & PFX_REXR) ? 1 : 0;
}
static Int getRexX ( Prefix pfx ) {
   return (pfx & PFX_REXX) ? 1 : 0;
}
static Int getRexB ( Prefix pfx ) {
   return (pfx & PFX_REXB) ? 1 : 0;
}

static Bool haveF2orF3 ( Prefix pfx ) {
   return toBool((pfx & (PFX_F2|PFX_F3)) > 0);
}
static Bool haveF2andF3 ( Prefix pfx ) {
   return toBool((pfx & (PFX_F2|PFX_F3)) == (PFX_F2|PFX_F3));
}
static Bool haveF2 ( Prefix pfx ) {
   return toBool((pfx & PFX_F2) > 0);
}
static Bool haveF3 ( Prefix pfx ) {
   return toBool((pfx & PFX_F3) > 0);
}

static Bool have66 ( Prefix pfx ) {
   return toBool((pfx & PFX_66) > 0);
}
static Bool haveASO ( Prefix pfx ) {
   return toBool((pfx & PFX_ASO) > 0);
}
static Bool haveLOCK ( Prefix pfx ) {
   return toBool((pfx & PFX_LOCK) > 0);
}

static Bool have66noF2noF3 ( Prefix pfx )
{
  return 
     toBool((pfx & (PFX_66|PFX_F2|PFX_F3)) == PFX_66);
}

static Bool haveF2no66noF3 ( Prefix pfx )
{
  return 
     toBool((pfx & (PFX_66|PFX_F2|PFX_F3)) == PFX_F2);
}

static Bool haveF3no66noF2 ( Prefix pfx )
{
  return 
     toBool((pfx & (PFX_66|PFX_F2|PFX_F3)) == PFX_F3);
}

static Bool haveF3noF2 ( Prefix pfx )
{
  return 
     toBool((pfx & (PFX_F2|PFX_F3)) == PFX_F3);
}

static Bool haveF2noF3 ( Prefix pfx )
{
  return 
     toBool((pfx & (PFX_F2|PFX_F3)) == PFX_F2);
}

static Bool haveNo66noF2noF3 ( Prefix pfx )
{
  return 
     toBool((pfx & (PFX_66|PFX_F2|PFX_F3)) == 0);
}

static Bool have66orF2orF3 ( Prefix pfx )
{
  return toBool( ! haveNo66noF2noF3(pfx) );
}

static Bool have66orF3 ( Prefix pfx )
{
   return toBool((pfx & (PFX_66|PFX_F3)) > 0);
}

static Prefix clearSegBits ( Prefix p )
{
   return 
      p & ~(PFX_CS | PFX_DS | PFX_ES | PFX_FS | PFX_GS | PFX_SS);
}

static UInt getVexNvvvv ( Prefix pfx ) {
   UInt r = (UInt)pfx;
   r /= (UInt)PFX_VEXnV0; 
   return r & 0xF;
}

static Bool haveVEX ( Prefix pfx ) {
   return toBool(pfx & PFX_VEX);
}

static Int getVexL ( Prefix pfx ) {
   return (pfx & PFX_VEXL) ? 1 : 0;
}





typedef
   enum { 
      ESC_NONE=0xF0000000, 
      ESC_0F,              
      ESC_0F38,            
      ESC_0F3A             
   }
   Escape;






static Int integerGuestReg64Offset ( UInt reg )
{
   switch (reg) {
      case R_RAX: return OFFB_RAX;
      case R_RCX: return OFFB_RCX;
      case R_RDX: return OFFB_RDX;
      case R_RBX: return OFFB_RBX;
      case R_RSP: return OFFB_RSP;
      case R_RBP: return OFFB_RBP;
      case R_RSI: return OFFB_RSI;
      case R_RDI: return OFFB_RDI;
      case R_R8:  return OFFB_R8;
      case R_R9:  return OFFB_R9;
      case R_R10: return OFFB_R10;
      case R_R11: return OFFB_R11;
      case R_R12: return OFFB_R12;
      case R_R13: return OFFB_R13;
      case R_R14: return OFFB_R14;
      case R_R15: return OFFB_R15;
      default: vpanic("integerGuestReg64Offset(amd64)");
   }
}



static 
const HChar* nameIReg ( Int sz, UInt reg, Bool irregular )
{
   static const HChar* ireg64_names[16]
     = { "%rax", "%rcx", "%rdx", "%rbx", "%rsp", "%rbp", "%rsi", "%rdi",
         "%r8",  "%r9",  "%r10", "%r11", "%r12", "%r13", "%r14", "%r15" };
   static const HChar* ireg32_names[16]
     = { "%eax", "%ecx", "%edx", "%ebx", "%esp", "%ebp", "%esi", "%edi",
         "%r8d", "%r9d", "%r10d","%r11d","%r12d","%r13d","%r14d","%r15d" };
   static const HChar* ireg16_names[16]
     = { "%ax",  "%cx",  "%dx",  "%bx",  "%sp",  "%bp",  "%si",  "%di",
         "%r8w", "%r9w", "%r10w","%r11w","%r12w","%r13w","%r14w","%r15w" };
   static const HChar* ireg8_names[16]
     = { "%al",  "%cl",  "%dl",  "%bl",  "%spl", "%bpl", "%sil", "%dil",
         "%r8b", "%r9b", "%r10b","%r11b","%r12b","%r13b","%r14b","%r15b" };
   static const HChar* ireg8_irregular[8] 
     = { "%al", "%cl", "%dl", "%bl", "%ah", "%ch", "%dh", "%bh" };

   vassert(reg < 16);
   if (sz == 1) {
      if (irregular)
         vassert(reg < 8);
   } else {
      vassert(irregular == False);
   }

   switch (sz) {
      case 8: return ireg64_names[reg];
      case 4: return ireg32_names[reg];
      case 2: return ireg16_names[reg];
      case 1: if (irregular) {
                 return ireg8_irregular[reg];
              } else {
                 return ireg8_names[reg];
              }
      default: vpanic("nameIReg(amd64)");
   }
}


static 
Int offsetIReg ( Int sz, UInt reg, Bool irregular )
{
   vassert(reg < 16);
   if (sz == 1) {
      if (irregular)
         vassert(reg < 8);
   } else {
      vassert(irregular == False);
   }

   
   if (sz == 1 && irregular) {
      switch (reg) {
         case R_RSP: return 1+ OFFB_RAX;
         case R_RBP: return 1+ OFFB_RCX;
         case R_RSI: return 1+ OFFB_RDX;
         case R_RDI: return 1+ OFFB_RBX;
         default:    break; 
      }
   }

   
   return integerGuestReg64Offset(reg);
}



static IRExpr* getIRegCL ( void )
{
   vassert(host_endness == VexEndnessLE);
   return IRExpr_Get( OFFB_RCX, Ity_I8 );
}



static void putIRegAH ( IRExpr* e )
{
   vassert(host_endness == VexEndnessLE);
   vassert(typeOfIRExpr(irsb->tyenv, e) == Ity_I8);
   stmt( IRStmt_Put( OFFB_RAX+1, e ) );
}



static const HChar* nameIRegRAX ( Int sz )
{
   switch (sz) {
      case 1: return "%al";
      case 2: return "%ax";
      case 4: return "%eax";
      case 8: return "%rax";
      default: vpanic("nameIRegRAX(amd64)");
   }
}

static IRExpr* getIRegRAX ( Int sz )
{
   vassert(host_endness == VexEndnessLE);
   switch (sz) {
      case 1: return IRExpr_Get( OFFB_RAX, Ity_I8 );
      case 2: return IRExpr_Get( OFFB_RAX, Ity_I16 );
      case 4: return unop(Iop_64to32, IRExpr_Get( OFFB_RAX, Ity_I64 ));
      case 8: return IRExpr_Get( OFFB_RAX, Ity_I64 );
      default: vpanic("getIRegRAX(amd64)");
   }
}

static void putIRegRAX ( Int sz, IRExpr* e )
{
   IRType ty = typeOfIRExpr(irsb->tyenv, e);
   vassert(host_endness == VexEndnessLE);
   switch (sz) {
      case 8: vassert(ty == Ity_I64);
              stmt( IRStmt_Put( OFFB_RAX, e ));
              break;
      case 4: vassert(ty == Ity_I32);
              stmt( IRStmt_Put( OFFB_RAX, unop(Iop_32Uto64,e) ));
              break;
      case 2: vassert(ty == Ity_I16);
              stmt( IRStmt_Put( OFFB_RAX, e ));
              break;
      case 1: vassert(ty == Ity_I8);
              stmt( IRStmt_Put( OFFB_RAX, e ));
              break;
      default: vpanic("putIRegRAX(amd64)");
   }
}



static const HChar* nameIRegRDX ( Int sz )
{
   switch (sz) {
      case 1: return "%dl";
      case 2: return "%dx";
      case 4: return "%edx";
      case 8: return "%rdx";
      default: vpanic("nameIRegRDX(amd64)");
   }
}

static IRExpr* getIRegRDX ( Int sz )
{
   vassert(host_endness == VexEndnessLE);
   switch (sz) {
      case 1: return IRExpr_Get( OFFB_RDX, Ity_I8 );
      case 2: return IRExpr_Get( OFFB_RDX, Ity_I16 );
      case 4: return unop(Iop_64to32, IRExpr_Get( OFFB_RDX, Ity_I64 ));
      case 8: return IRExpr_Get( OFFB_RDX, Ity_I64 );
      default: vpanic("getIRegRDX(amd64)");
   }
}

static void putIRegRDX ( Int sz, IRExpr* e )
{
   vassert(host_endness == VexEndnessLE);
   vassert(typeOfIRExpr(irsb->tyenv, e) == szToITy(sz));
   switch (sz) {
      case 8: stmt( IRStmt_Put( OFFB_RDX, e ));
              break;
      case 4: stmt( IRStmt_Put( OFFB_RDX, unop(Iop_32Uto64,e) ));
              break;
      case 2: stmt( IRStmt_Put( OFFB_RDX, e ));
              break;
      case 1: stmt( IRStmt_Put( OFFB_RDX, e ));
              break;
      default: vpanic("putIRegRDX(amd64)");
   }
}



static IRExpr* getIReg64 ( UInt regno )
{
   return IRExpr_Get( integerGuestReg64Offset(regno),
                      Ity_I64 );
}

static void putIReg64 ( UInt regno, IRExpr* e )
{
   vassert(typeOfIRExpr(irsb->tyenv,e) == Ity_I64);
   stmt( IRStmt_Put( integerGuestReg64Offset(regno), e ) );
}

static const HChar* nameIReg64 ( UInt regno )
{
   return nameIReg( 8, regno, False );
}



static IRExpr* getIReg32 ( UInt regno )
{
   vassert(host_endness == VexEndnessLE);
   return unop(Iop_64to32,
               IRExpr_Get( integerGuestReg64Offset(regno),
                           Ity_I64 ));
}

static void putIReg32 ( UInt regno, IRExpr* e )
{
   vassert(typeOfIRExpr(irsb->tyenv,e) == Ity_I32);
   stmt( IRStmt_Put( integerGuestReg64Offset(regno), 
                     unop(Iop_32Uto64,e) ) );
}

static const HChar* nameIReg32 ( UInt regno )
{
   return nameIReg( 4, regno, False );
}



static IRExpr* getIReg16 ( UInt regno )
{
   vassert(host_endness == VexEndnessLE);
   return IRExpr_Get( integerGuestReg64Offset(regno),
                      Ity_I16 );
}

static void putIReg16 ( UInt regno, IRExpr* e )
{
   vassert(typeOfIRExpr(irsb->tyenv,e) == Ity_I16);
   stmt( IRStmt_Put( integerGuestReg64Offset(regno), 
                     unop(Iop_16Uto64,e) ) );
}

static const HChar* nameIReg16 ( UInt regno )
{
   return nameIReg( 2, regno, False );
}


static IRExpr* getIReg64rexX ( Prefix pfx, UInt lo3bits )
{
   vassert(lo3bits < 8);
   vassert(IS_VALID_PFX(pfx));
   return getIReg64( lo3bits | (getRexX(pfx) << 3) );
}

static const HChar* nameIReg64rexX ( Prefix pfx, UInt lo3bits )
{
   vassert(lo3bits < 8);
   vassert(IS_VALID_PFX(pfx));
   return nameIReg( 8, lo3bits | (getRexX(pfx) << 3), False );
}

static const HChar* nameIRegRexB ( Int sz, Prefix pfx, UInt lo3bits )
{
   vassert(lo3bits < 8);
   vassert(IS_VALID_PFX(pfx));
   vassert(sz == 8 || sz == 4 || sz == 2 || sz == 1);
   return nameIReg( sz, lo3bits | (getRexB(pfx) << 3), 
                        toBool(sz==1 && !haveREX(pfx)) );
}

static IRExpr* getIRegRexB ( Int sz, Prefix pfx, UInt lo3bits )
{
   vassert(lo3bits < 8);
   vassert(IS_VALID_PFX(pfx));
   vassert(sz == 8 || sz == 4 || sz == 2 || sz == 1);
   if (sz == 4) {
      sz = 8;
      return unop(Iop_64to32,
                  IRExpr_Get(
                     offsetIReg( sz, lo3bits | (getRexB(pfx) << 3), 
                                     False ),
                     szToITy(sz)
                 )
             );
   } else {
      return IRExpr_Get(
                offsetIReg( sz, lo3bits | (getRexB(pfx) << 3), 
                                toBool(sz==1 && !haveREX(pfx)) ),
                szToITy(sz)
             );
   }
}

static void putIRegRexB ( Int sz, Prefix pfx, UInt lo3bits, IRExpr* e )
{
   vassert(lo3bits < 8);
   vassert(IS_VALID_PFX(pfx));
   vassert(sz == 8 || sz == 4 || sz == 2 || sz == 1);
   vassert(typeOfIRExpr(irsb->tyenv, e) == szToITy(sz));
   stmt( IRStmt_Put( 
            offsetIReg( sz, lo3bits | (getRexB(pfx) << 3), 
                            toBool(sz==1 && !haveREX(pfx)) ),
            sz==4 ? unop(Iop_32Uto64,e) : e
   ));
}


static UInt gregOfRexRM ( Prefix pfx, UChar mod_reg_rm )
{
   Int reg = (Int)( (mod_reg_rm >> 3) & 7 );
   reg += (pfx & PFX_REXR) ? 8 : 0;
   return reg;
}

static UInt eregOfRexRM ( Prefix pfx, UChar mod_reg_rm )
{
   Int rm;
   vassert(epartIsReg(mod_reg_rm));
   rm = (Int)(mod_reg_rm & 0x7);
   rm += (pfx & PFX_REXB) ? 8 : 0;
   return rm;
}



static UInt offsetIRegG ( Int sz, Prefix pfx, UChar mod_reg_rm )
{
   UInt reg;
   vassert(host_endness == VexEndnessLE);
   vassert(IS_VALID_PFX(pfx));
   vassert(sz == 8 || sz == 4 || sz == 2 || sz == 1);
   reg = gregOfRexRM( pfx, mod_reg_rm );
   return offsetIReg( sz, reg, toBool(sz == 1 && !haveREX(pfx)) );
}

static 
IRExpr* getIRegG ( Int sz, Prefix pfx, UChar mod_reg_rm )
{
   if (sz == 4) {
      sz = 8;
      return unop(Iop_64to32,
                  IRExpr_Get( offsetIRegG( sz, pfx, mod_reg_rm ),
                              szToITy(sz) ));
   } else {
      return IRExpr_Get( offsetIRegG( sz, pfx, mod_reg_rm ),
                         szToITy(sz) );
   }
}

static 
void putIRegG ( Int sz, Prefix pfx, UChar mod_reg_rm, IRExpr* e )
{
   vassert(typeOfIRExpr(irsb->tyenv,e) == szToITy(sz));
   if (sz == 4) {
      e = unop(Iop_32Uto64,e);
   }
   stmt( IRStmt_Put( offsetIRegG( sz, pfx, mod_reg_rm ), e ) );
}

static
const HChar* nameIRegG ( Int sz, Prefix pfx, UChar mod_reg_rm )
{
   return nameIReg( sz, gregOfRexRM(pfx,mod_reg_rm),
                        toBool(sz==1 && !haveREX(pfx)) );
}


static
IRExpr* getIRegV ( Int sz, Prefix pfx )
{
   if (sz == 4) {
      sz = 8;
      return unop(Iop_64to32,
                  IRExpr_Get( offsetIReg( sz, getVexNvvvv(pfx), False ),
                              szToITy(sz) ));
   } else {
      return IRExpr_Get( offsetIReg( sz, getVexNvvvv(pfx), False ),
                         szToITy(sz) );
   }
}

static
void putIRegV ( Int sz, Prefix pfx, IRExpr* e )
{
   vassert(typeOfIRExpr(irsb->tyenv,e) == szToITy(sz));
   if (sz == 4) {
      e = unop(Iop_32Uto64,e);
   }
   stmt( IRStmt_Put( offsetIReg( sz, getVexNvvvv(pfx), False ), e ) );
}

static
const HChar* nameIRegV ( Int sz, Prefix pfx )
{
   return nameIReg( sz, getVexNvvvv(pfx), False );
}



static UInt offsetIRegE ( Int sz, Prefix pfx, UChar mod_reg_rm )
{
   UInt reg;
   vassert(host_endness == VexEndnessLE);
   vassert(IS_VALID_PFX(pfx));
   vassert(sz == 8 || sz == 4 || sz == 2 || sz == 1);
   reg = eregOfRexRM( pfx, mod_reg_rm );
   return offsetIReg( sz, reg, toBool(sz == 1 && !haveREX(pfx)) );
}

static 
IRExpr* getIRegE ( Int sz, Prefix pfx, UChar mod_reg_rm )
{
   if (sz == 4) {
      sz = 8;
      return unop(Iop_64to32,
                  IRExpr_Get( offsetIRegE( sz, pfx, mod_reg_rm ),
                              szToITy(sz) ));
   } else {
      return IRExpr_Get( offsetIRegE( sz, pfx, mod_reg_rm ),
                         szToITy(sz) );
   }
}

static 
void putIRegE ( Int sz, Prefix pfx, UChar mod_reg_rm, IRExpr* e )
{
   vassert(typeOfIRExpr(irsb->tyenv,e) == szToITy(sz));
   if (sz == 4) {
      e = unop(Iop_32Uto64,e);
   }
   stmt( IRStmt_Put( offsetIRegE( sz, pfx, mod_reg_rm ), e ) );
}

static
const HChar* nameIRegE ( Int sz, Prefix pfx, UChar mod_reg_rm )
{
   return nameIReg( sz, eregOfRexRM(pfx,mod_reg_rm),
                        toBool(sz==1 && !haveREX(pfx)) );
}



static Int ymmGuestRegOffset ( UInt ymmreg )
{
   switch (ymmreg) {
      case 0:  return OFFB_YMM0;
      case 1:  return OFFB_YMM1;
      case 2:  return OFFB_YMM2;
      case 3:  return OFFB_YMM3;
      case 4:  return OFFB_YMM4;
      case 5:  return OFFB_YMM5;
      case 6:  return OFFB_YMM6;
      case 7:  return OFFB_YMM7;
      case 8:  return OFFB_YMM8;
      case 9:  return OFFB_YMM9;
      case 10: return OFFB_YMM10;
      case 11: return OFFB_YMM11;
      case 12: return OFFB_YMM12;
      case 13: return OFFB_YMM13;
      case 14: return OFFB_YMM14;
      case 15: return OFFB_YMM15;
      default: vpanic("ymmGuestRegOffset(amd64)");
   }
}

static Int xmmGuestRegOffset ( UInt xmmreg )
{
   
   vassert(host_endness == VexEndnessLE);
   return ymmGuestRegOffset( xmmreg );
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

static Int ymmGuestRegLane128offset ( UInt ymmreg, Int laneno )
{
   
   vassert(host_endness == VexEndnessLE);
   vassert(laneno >= 0 && laneno < 2);
   return ymmGuestRegOffset( ymmreg ) + 16 * laneno;
}

static Int ymmGuestRegLane64offset ( UInt ymmreg, Int laneno )
{
   
   vassert(host_endness == VexEndnessLE);
   vassert(laneno >= 0 && laneno < 4);
   return ymmGuestRegOffset( ymmreg ) + 8 * laneno;
}

static Int ymmGuestRegLane32offset ( UInt ymmreg, Int laneno )
{
   
   vassert(host_endness == VexEndnessLE);
   vassert(laneno >= 0 && laneno < 8);
   return ymmGuestRegOffset( ymmreg ) + 4 * laneno;
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

static IRExpr* getXMMRegLane16 ( UInt xmmreg, Int laneno )
{
  return IRExpr_Get( xmmGuestRegLane16offset(xmmreg,laneno), Ity_I16 );
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

static IRExpr* getYMMReg ( UInt xmmreg )
{
   return IRExpr_Get( ymmGuestRegOffset(xmmreg), Ity_V256 );
}

static IRExpr* getYMMRegLane128 ( UInt ymmreg, Int laneno )
{
   return IRExpr_Get( ymmGuestRegLane128offset(ymmreg,laneno), Ity_V128 );
}

static IRExpr* getYMMRegLane64 ( UInt ymmreg, Int laneno )
{
   return IRExpr_Get( ymmGuestRegLane64offset(ymmreg,laneno), Ity_I64 );
}

static IRExpr* getYMMRegLane32 ( UInt ymmreg, Int laneno )
{
   return IRExpr_Get( ymmGuestRegLane32offset(ymmreg,laneno), Ity_I32 );
}

static void putYMMReg ( UInt ymmreg, IRExpr* e )
{
   vassert(typeOfIRExpr(irsb->tyenv,e) == Ity_V256);
   stmt( IRStmt_Put( ymmGuestRegOffset(ymmreg), e ) );
}

static void putYMMRegLane128 ( UInt ymmreg, Int laneno, IRExpr* e )
{
   vassert(typeOfIRExpr(irsb->tyenv,e) == Ity_V128);
   stmt( IRStmt_Put( ymmGuestRegLane128offset(ymmreg,laneno), e ) );
}

static void putYMMRegLane64F ( UInt ymmreg, Int laneno, IRExpr* e )
{
   vassert(typeOfIRExpr(irsb->tyenv,e) == Ity_F64);
   stmt( IRStmt_Put( ymmGuestRegLane64offset(ymmreg,laneno), e ) );
}

static void putYMMRegLane64 ( UInt ymmreg, Int laneno, IRExpr* e )
{
   vassert(typeOfIRExpr(irsb->tyenv,e) == Ity_I64);
   stmt( IRStmt_Put( ymmGuestRegLane64offset(ymmreg,laneno), e ) );
}

static void putYMMRegLane32F ( UInt ymmreg, Int laneno, IRExpr* e )
{
   vassert(typeOfIRExpr(irsb->tyenv,e) == Ity_F32);
   stmt( IRStmt_Put( ymmGuestRegLane32offset(ymmreg,laneno), e ) );
}

static void putYMMRegLane32 ( UInt ymmreg, Int laneno, IRExpr* e )
{
   vassert(typeOfIRExpr(irsb->tyenv,e) == Ity_I32);
   stmt( IRStmt_Put( ymmGuestRegLane32offset(ymmreg,laneno), e ) );
}

static IRExpr* mkV128 ( UShort mask )
{
   return IRExpr_Const(IRConst_V128(mask));
}

static void putYMMRegLoAndZU ( UInt ymmreg, IRExpr* e )
{
   putYMMRegLane128( ymmreg, 0, e );
   putYMMRegLane128( ymmreg, 1, mkV128(0) );
}

static IRExpr* mkAnd1 ( IRExpr* x, IRExpr* y )
{
   vassert(typeOfIRExpr(irsb->tyenv,x) == Ity_I1);
   vassert(typeOfIRExpr(irsb->tyenv,y) == Ity_I1);
   return unop(Iop_64to1, 
               binop(Iop_And64, 
                     unop(Iop_1Uto64,x), 
                     unop(Iop_1Uto64,y)));
}

static void casLE ( IRExpr* addr, IRExpr* expVal, IRExpr* newVal,
                    Addr64 restart_point )
{
   IRCAS* cas;
   IRType tyE    = typeOfIRExpr(irsb->tyenv, expVal);
   IRType tyN    = typeOfIRExpr(irsb->tyenv, newVal);
   IRTemp oldTmp = newTemp(tyE);
   IRTemp expTmp = newTemp(tyE);
   vassert(tyE == tyN);
   vassert(tyE == Ity_I64 || tyE == Ity_I32
           || tyE == Ity_I16 || tyE == Ity_I8);
   assign(expTmp, expVal);
   cas = mkIRCAS( IRTemp_INVALID, oldTmp, Iend_LE, addr, 
                  NULL, mkexpr(expTmp), NULL, newVal );
   stmt( IRStmt_CAS(cas) );
   stmt( IRStmt_Exit(
            binop( mkSizedOp(tyE,Iop_CasCmpNE8),
                   mkexpr(oldTmp), mkexpr(expTmp) ),
            Ijk_Boring, 
            IRConst_U64( restart_point ),
            OFFB_RIP
         ));
}




static IRExpr* mk_amd64g_calculate_rflags_all ( void )
{
   IRExpr** args
      = mkIRExprVec_4( IRExpr_Get(OFFB_CC_OP,   Ity_I64),
                       IRExpr_Get(OFFB_CC_DEP1, Ity_I64),
                       IRExpr_Get(OFFB_CC_DEP2, Ity_I64),
                       IRExpr_Get(OFFB_CC_NDEP, Ity_I64) );
   IRExpr* call
      = mkIRExprCCall(
           Ity_I64,
           0, 
           "amd64g_calculate_rflags_all", &amd64g_calculate_rflags_all,
           args
        );
   call->Iex.CCall.cee->mcx_mask = (1<<0) | (1<<3);
   return call;
}

static IRExpr* mk_amd64g_calculate_condition ( AMD64Condcode cond )
{
   IRExpr** args
      = mkIRExprVec_5( mkU64(cond),
                       IRExpr_Get(OFFB_CC_OP,   Ity_I64),
                       IRExpr_Get(OFFB_CC_DEP1, Ity_I64),
                       IRExpr_Get(OFFB_CC_DEP2, Ity_I64),
                       IRExpr_Get(OFFB_CC_NDEP, Ity_I64) );
   IRExpr* call
      = mkIRExprCCall(
           Ity_I64,
           0, 
           "amd64g_calculate_condition", &amd64g_calculate_condition,
           args
        );
   call->Iex.CCall.cee->mcx_mask = (1<<0) | (1<<1) | (1<<4);
   return unop(Iop_64to1, call);
}

static IRExpr* mk_amd64g_calculate_rflags_c ( void )
{
   IRExpr** args
      = mkIRExprVec_4( IRExpr_Get(OFFB_CC_OP,   Ity_I64),
                       IRExpr_Get(OFFB_CC_DEP1, Ity_I64),
                       IRExpr_Get(OFFB_CC_DEP2, Ity_I64),
                       IRExpr_Get(OFFB_CC_NDEP, Ity_I64) );
   IRExpr* call
      = mkIRExprCCall(
           Ity_I64,
           0, 
           "amd64g_calculate_rflags_c", &amd64g_calculate_rflags_c,
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

static IRExpr* widenUto64 ( IRExpr* e )
{
   switch (typeOfIRExpr(irsb->tyenv,e)) {
      case Ity_I64: return e;
      case Ity_I32: return unop(Iop_32Uto64, e);
      case Ity_I16: return unop(Iop_16Uto64, e);
      case Ity_I8:  return unop(Iop_8Uto64, e);
      case Ity_I1:  return unop(Iop_1Uto64, e);
      default: vpanic("widenUto64");
   }
}

static IRExpr* widenSto64 ( IRExpr* e )
{
   switch (typeOfIRExpr(irsb->tyenv,e)) {
      case Ity_I64: return e;
      case Ity_I32: return unop(Iop_32Sto64, e);
      case Ity_I16: return unop(Iop_16Sto64, e);
      case Ity_I8:  return unop(Iop_8Sto64, e);
      default: vpanic("widenSto64");
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
   if (src_ty == Ity_I64 && dst_ty == Ity_I32)
      return unop(Iop_64to32, e);
   if (src_ty == Ity_I64 && dst_ty == Ity_I16)
      return unop(Iop_64to16, e);
   if (src_ty == Ity_I64 && dst_ty == Ity_I8)
      return unop(Iop_64to8, e);

   vex_printf("\nsrc, dst tys are: ");
   ppIRType(src_ty);
   vex_printf(", ");
   ppIRType(dst_ty);
   vex_printf("\n");
   vpanic("narrowTo(amd64)");
}



static 
void setFlags_DEP1_DEP2 ( IROp op8, IRTemp dep1, IRTemp dep2, IRType ty )
{
   Int ccOp = 0;
   switch (ty) {
      case Ity_I8:  ccOp = 0; break;
      case Ity_I16: ccOp = 1; break;
      case Ity_I32: ccOp = 2; break;
      case Ity_I64: ccOp = 3; break;
      default: vassert(0);
   }
   switch (op8) {
      case Iop_Add8: ccOp += AMD64G_CC_OP_ADDB;   break;
      case Iop_Sub8: ccOp += AMD64G_CC_OP_SUBB;   break;
      default:       ppIROp(op8);
                     vpanic("setFlags_DEP1_DEP2(amd64)");
   }
   stmt( IRStmt_Put( OFFB_CC_OP,   mkU64(ccOp)) );
   stmt( IRStmt_Put( OFFB_CC_DEP1, widenUto64(mkexpr(dep1))) );
   stmt( IRStmt_Put( OFFB_CC_DEP2, widenUto64(mkexpr(dep2))) );
}



static 
void setFlags_DEP1 ( IROp op8, IRTemp dep1, IRType ty )
{
   Int ccOp = 0;
   switch (ty) {
      case Ity_I8:  ccOp = 0; break;
      case Ity_I16: ccOp = 1; break;
      case Ity_I32: ccOp = 2; break;
      case Ity_I64: ccOp = 3; break;
      default: vassert(0);
   }
   switch (op8) {
      case Iop_Or8:
      case Iop_And8:
      case Iop_Xor8: ccOp += AMD64G_CC_OP_LOGICB; break;
      default:       ppIROp(op8);
                     vpanic("setFlags_DEP1(amd64)");
   }
   stmt( IRStmt_Put( OFFB_CC_OP,   mkU64(ccOp)) );
   stmt( IRStmt_Put( OFFB_CC_DEP1, widenUto64(mkexpr(dep1))) );
   stmt( IRStmt_Put( OFFB_CC_DEP2, mkU64(0)) );
}



static void setFlags_DEP1_DEP2_shift ( IROp    op64,
                                       IRTemp  res,
                                       IRTemp  resUS,
                                       IRType  ty,
                                       IRTemp  guard )
{
   Int ccOp = 0;
   switch (ty) {
      case Ity_I8:  ccOp = 0; break;
      case Ity_I16: ccOp = 1; break;
      case Ity_I32: ccOp = 2; break;
      case Ity_I64: ccOp = 3; break;
      default: vassert(0);
   }

   vassert(guard);

   switch (op64) {
      case Iop_Shr64:
      case Iop_Sar64: ccOp += AMD64G_CC_OP_SHRB; break;
      case Iop_Shl64: ccOp += AMD64G_CC_OP_SHLB; break;
      default:        ppIROp(op64);
                      vpanic("setFlags_DEP1_DEP2_shift(amd64)");
   }

   
   IRTemp guardB = newTemp(Ity_I1);
   assign( guardB, binop(Iop_CmpNE8, mkexpr(guard), mkU8(0)) );

   
   stmt( IRStmt_Put( OFFB_CC_OP,
                     IRExpr_ITE( mkexpr(guardB),
                                 mkU64(ccOp),
                                 IRExpr_Get(OFFB_CC_OP,Ity_I64) ) ));
   stmt( IRStmt_Put( OFFB_CC_DEP1,
                     IRExpr_ITE( mkexpr(guardB),
                                 widenUto64(mkexpr(res)),
                                 IRExpr_Get(OFFB_CC_DEP1,Ity_I64) ) ));
   stmt( IRStmt_Put( OFFB_CC_DEP2, 
                     IRExpr_ITE( mkexpr(guardB),
                                 widenUto64(mkexpr(resUS)),
                                 IRExpr_Get(OFFB_CC_DEP2,Ity_I64) ) ));
}



static void setFlags_INC_DEC ( Bool inc, IRTemp res, IRType ty )
{
   Int ccOp = inc ? AMD64G_CC_OP_INCB : AMD64G_CC_OP_DECB;

   switch (ty) {
      case Ity_I8:  ccOp += 0; break;
      case Ity_I16: ccOp += 1; break;
      case Ity_I32: ccOp += 2; break;
      case Ity_I64: ccOp += 3; break;
      default: vassert(0);
   }
   
   stmt( IRStmt_Put( OFFB_CC_NDEP, mk_amd64g_calculate_rflags_c()) );
   stmt( IRStmt_Put( OFFB_CC_OP,   mkU64(ccOp)) );
   stmt( IRStmt_Put( OFFB_CC_DEP1, widenUto64(mkexpr(res))) );
   stmt( IRStmt_Put( OFFB_CC_DEP2, mkU64(0)) );
}



static
void setFlags_MUL ( IRType ty, IRTemp arg1, IRTemp arg2, ULong base_op )
{
   switch (ty) {
      case Ity_I8:
         stmt( IRStmt_Put( OFFB_CC_OP, mkU64(base_op+0) ) );
         break;
      case Ity_I16:
         stmt( IRStmt_Put( OFFB_CC_OP, mkU64(base_op+1) ) );
         break;
      case Ity_I32:
         stmt( IRStmt_Put( OFFB_CC_OP, mkU64(base_op+2) ) );
         break;
      case Ity_I64:
         stmt( IRStmt_Put( OFFB_CC_OP, mkU64(base_op+3) ) );
         break;
      default:
         vpanic("setFlags_MUL(amd64)");
   }
   stmt( IRStmt_Put( OFFB_CC_DEP1, widenUto64(mkexpr(arg1)) ));
   stmt( IRStmt_Put( OFFB_CC_DEP2, widenUto64(mkexpr(arg2)) ));
}




static const HChar* name_AMD64Condcode ( AMD64Condcode cond )
{
   switch (cond) {
      case AMD64CondO:      return "o";
      case AMD64CondNO:     return "no";
      case AMD64CondB:      return "b";
      case AMD64CondNB:     return "ae"; 
      case AMD64CondZ:      return "e"; 
      case AMD64CondNZ:     return "ne"; 
      case AMD64CondBE:     return "be";
      case AMD64CondNBE:    return "a"; 
      case AMD64CondS:      return "s";
      case AMD64CondNS:     return "ns";
      case AMD64CondP:      return "p";
      case AMD64CondNP:     return "np";
      case AMD64CondL:      return "l";
      case AMD64CondNL:     return "ge"; 
      case AMD64CondLE:     return "le";
      case AMD64CondNLE:    return "g"; 
      case AMD64CondAlways: return "ALWAYS";
      default: vpanic("name_AMD64Condcode");
   }
}

static 
AMD64Condcode positiveIse_AMD64Condcode ( AMD64Condcode  cond,
                                          Bool*   needInvert )
{
   vassert(cond >= AMD64CondO && cond <= AMD64CondNLE);
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
                         
                         IRTemp taddr, IRTemp texpVal, Addr64 restart_point )
{
   UInt    thunkOp;
   IRType  ty    = szToITy(sz);
   IRTemp  oldc  = newTemp(Ity_I64);
   IRTemp  oldcn = newTemp(ty);
   IROp    plus  = mkSizedOp(ty, Iop_Add8);
   IROp    xor   = mkSizedOp(ty, Iop_Xor8);

   vassert(typeOfIRTemp(irsb->tyenv, tres) == ty);

   switch (sz) {
      case 8:  thunkOp = AMD64G_CC_OP_ADCQ; break;
      case 4:  thunkOp = AMD64G_CC_OP_ADCL; break;
      case 2:  thunkOp = AMD64G_CC_OP_ADCW; break;
      case 1:  thunkOp = AMD64G_CC_OP_ADCB; break;
      default: vassert(0);
   }

   
   assign( oldc,  binop(Iop_And64,
                        mk_amd64g_calculate_rflags_c(),
                        mkU64(1)) );

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

   stmt( IRStmt_Put( OFFB_CC_OP,   mkU64(thunkOp) ) );
   stmt( IRStmt_Put( OFFB_CC_DEP1, widenUto64(mkexpr(ta1))  ));
   stmt( IRStmt_Put( OFFB_CC_DEP2, widenUto64(binop(xor, mkexpr(ta2), 
                                                         mkexpr(oldcn)) )) );
   stmt( IRStmt_Put( OFFB_CC_NDEP, mkexpr(oldc) ) );
}


static void helper_SBB ( Int sz,
                         IRTemp tres, IRTemp ta1, IRTemp ta2,
                         
                         IRTemp taddr, IRTemp texpVal, Addr64 restart_point )
{
   UInt    thunkOp;
   IRType  ty    = szToITy(sz);
   IRTemp  oldc  = newTemp(Ity_I64);
   IRTemp  oldcn = newTemp(ty);
   IROp    minus = mkSizedOp(ty, Iop_Sub8);
   IROp    xor   = mkSizedOp(ty, Iop_Xor8);

   vassert(typeOfIRTemp(irsb->tyenv, tres) == ty);

   switch (sz) {
      case 8:  thunkOp = AMD64G_CC_OP_SBBQ; break;
      case 4:  thunkOp = AMD64G_CC_OP_SBBL; break;
      case 2:  thunkOp = AMD64G_CC_OP_SBBW; break;
      case 1:  thunkOp = AMD64G_CC_OP_SBBB; break;
      default: vassert(0);
   }

   
   assign( oldc, binop(Iop_And64,
                       mk_amd64g_calculate_rflags_c(),
                       mkU64(1)) );

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

   stmt( IRStmt_Put( OFFB_CC_OP,   mkU64(thunkOp) ) );
   stmt( IRStmt_Put( OFFB_CC_DEP1, widenUto64(mkexpr(ta1) )) );
   stmt( IRStmt_Put( OFFB_CC_DEP2, widenUto64(binop(xor, mkexpr(ta2), 
                                                         mkexpr(oldcn)) )) );
   stmt( IRStmt_Put( OFFB_CC_NDEP, mkexpr(oldc) ) );
}



static const HChar* nameGrp1 ( Int opc_aux )
{
   static const HChar* grp1_names[8] 
     = { "add", "or", "adc", "sbb", "and", "sub", "xor", "cmp" };
   if (opc_aux < 0 || opc_aux > 7) vpanic("nameGrp1(amd64)");
   return grp1_names[opc_aux];
}

static const HChar* nameGrp2 ( Int opc_aux )
{
   static const HChar* grp2_names[8] 
     = { "rol", "ror", "rcl", "rcr", "shl", "shr", "shl", "sar" };
   if (opc_aux < 0 || opc_aux > 7) vpanic("nameGrp2(amd64)");
   return grp2_names[opc_aux];
}

static const HChar* nameGrp4 ( Int opc_aux )
{
   static const HChar* grp4_names[8] 
     = { "inc", "dec", "???", "???", "???", "???", "???", "???" };
   if (opc_aux < 0 || opc_aux > 1) vpanic("nameGrp4(amd64)");
   return grp4_names[opc_aux];
}

static const HChar* nameGrp5 ( Int opc_aux )
{
   static const HChar* grp5_names[8] 
     = { "inc", "dec", "call*", "call*", "jmp*", "jmp*", "push", "???" };
   if (opc_aux < 0 || opc_aux > 6) vpanic("nameGrp5(amd64)");
   return grp5_names[opc_aux];
}

static const HChar* nameGrp8 ( Int opc_aux )
{
   static const HChar* grp8_names[8] 
      = { "???", "???", "???", "???", "bt", "bts", "btr", "btc" };
   if (opc_aux < 4 || opc_aux > 7) vpanic("nameGrp8(amd64)");
   return grp8_names[opc_aux];
}


static const HChar* nameMMXReg ( Int mmxreg )
{
   static const HChar* mmx_names[8] 
     = { "%mm0", "%mm1", "%mm2", "%mm3", "%mm4", "%mm5", "%mm6", "%mm7" };
   if (mmxreg < 0 || mmxreg > 7) vpanic("nameMMXReg(amd64,guest)");
   return mmx_names[mmxreg];
}

static const HChar* nameXMMReg ( Int xmmreg )
{
   static const HChar* xmm_names[16] 
     = { "%xmm0",  "%xmm1",  "%xmm2",  "%xmm3", 
         "%xmm4",  "%xmm5",  "%xmm6",  "%xmm7", 
         "%xmm8",  "%xmm9",  "%xmm10", "%xmm11", 
         "%xmm12", "%xmm13", "%xmm14", "%xmm15" };
   if (xmmreg < 0 || xmmreg > 15) vpanic("nameXMMReg(amd64)");
   return xmm_names[xmmreg];
}
 
static const HChar* nameMMXGran ( Int gran )
{
   switch (gran) {
      case 0: return "b";
      case 1: return "w";
      case 2: return "d";
      case 3: return "q";
      default: vpanic("nameMMXGran(amd64,guest)");
   }
}

static HChar nameISize ( Int size )
{
   switch (size) {
      case 8: return 'q';
      case 4: return 'l';
      case 2: return 'w';
      case 1: return 'b';
      default: vpanic("nameISize(amd64)");
   }
}

static const HChar* nameYMMReg ( Int ymmreg )
{
   static const HChar* ymm_names[16] 
     = { "%ymm0",  "%ymm1",  "%ymm2",  "%ymm3", 
         "%ymm4",  "%ymm5",  "%ymm6",  "%ymm7", 
         "%ymm8",  "%ymm9",  "%ymm10", "%ymm11", 
         "%ymm12", "%ymm13", "%ymm14", "%ymm15" };
   if (ymmreg < 0 || ymmreg > 15) vpanic("nameYMMReg(amd64)");
   return ymm_names[ymmreg];
}



static void jmp_lit( DisResult* dres,
                     IRJumpKind kind, Addr64 d64 )
{
   vassert(dres->whatNext    == Dis_Continue);
   vassert(dres->len         == 0);
   vassert(dres->continueAt  == 0);
   vassert(dres->jk_StopHere == Ijk_INVALID);
   dres->whatNext    = Dis_StopHere;
   dres->jk_StopHere = kind;
   stmt( IRStmt_Put( OFFB_RIP, mkU64(d64) ) );
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
   stmt( IRStmt_Put( OFFB_RIP, mkexpr(t) ) );
}

static 
void jcc_01 ( DisResult* dres,
              AMD64Condcode cond, Addr64 d64_false, Addr64 d64_true )
{
   Bool          invert;
   AMD64Condcode condPos;
   vassert(dres->whatNext    == Dis_Continue);
   vassert(dres->len         == 0);
   vassert(dres->continueAt  == 0);
   vassert(dres->jk_StopHere == Ijk_INVALID);
   dres->whatNext    = Dis_StopHere;
   dres->jk_StopHere = Ijk_Boring;
   condPos = positiveIse_AMD64Condcode ( cond, &invert );
   if (invert) {
      stmt( IRStmt_Exit( mk_amd64g_calculate_condition(condPos),
                         Ijk_Boring,
                         IRConst_U64(d64_false),
                         OFFB_RIP ) );
      stmt( IRStmt_Put( OFFB_RIP, mkU64(d64_true) ) );
   } else {
      stmt( IRStmt_Exit( mk_amd64g_calculate_condition(condPos),
                         Ijk_Boring,
                         IRConst_U64(d64_true),
                         OFFB_RIP ) );
      stmt( IRStmt_Put( OFFB_RIP, mkU64(d64_false) ) );
   }
}

static 
void make_redzone_AbiHint ( const VexAbiInfo* vbi,
                            IRTemp new_rsp, IRTemp nia, const HChar* who )
{
   Int szB = vbi->guest_stack_redzone_size;
   vassert(szB >= 0);

   vassert(szB == 128);

   if (0) vex_printf("AbiHint: %s\n", who);
   vassert(typeOfIRTemp(irsb->tyenv, new_rsp) == Ity_I64);
   vassert(typeOfIRTemp(irsb->tyenv, nia) == Ity_I64);
   if (szB > 0)
      stmt( IRStmt_AbiHint( 
               binop(Iop_Sub64, mkexpr(new_rsp), mkU64(szB)), 
               szB,
               mkexpr(nia)
            ));
}



static 
const HChar* segRegTxt ( Prefix pfx )
{
   if (pfx & PFX_CS) return "%cs:";
   if (pfx & PFX_DS) return "%ds:";
   if (pfx & PFX_ES) return "%es:";
   if (pfx & PFX_FS) return "%fs:";
   if (pfx & PFX_GS) return "%gs:";
   if (pfx & PFX_SS) return "%ss:";
   return ""; 
}


static
IRExpr* handleAddrOverrides ( const VexAbiInfo* vbi, 
                              Prefix pfx, IRExpr* virtual )
{
   
   if (pfx & PFX_FS) {
      if (vbi->guest_amd64_assume_fs_is_const) {
         
         virtual = binop(Iop_Add64, virtual,
                                    IRExpr_Get(OFFB_FS_CONST, Ity_I64));
      } else {
         unimplemented("amd64 %fs segment override");
      }
   }

   if (pfx & PFX_GS) {
      if (vbi->guest_amd64_assume_gs_is_const) {
         
         virtual = binop(Iop_Add64, virtual,
                                    IRExpr_Get(OFFB_GS_CONST, Ity_I64));
      } else {
         unimplemented("amd64 %gs segment override");
      }
   }

   

   
   if (haveASO(pfx))
      virtual = unop(Iop_32Uto64, unop(Iop_64to32, virtual));

   return virtual;
}




static IRTemp disAMode_copy2tmp ( IRExpr* addr64 )
{
   IRTemp tmp = newTemp(Ity_I64);
   assign( tmp, addr64 );
   return tmp;
}

static 
IRTemp disAMode ( Int* len,
                  const VexAbiInfo* vbi, Prefix pfx, Long delta, 
                  HChar* buf, Int extra_bytes )
{
   UChar mod_reg_rm = getUChar(delta);
   delta++;

   buf[0] = (UChar)0;
   vassert(extra_bytes >= 0 && extra_bytes < 10);

   mod_reg_rm &= 0xC7;                         
   mod_reg_rm  = toUChar(mod_reg_rm | (mod_reg_rm >> 3));
                                               
   mod_reg_rm &= 0x1F;                         
   switch (mod_reg_rm) {

      case 0x00: case 0x01: case 0x02: case 0x03: 
        case 0x06: case 0x07:
         { UChar rm = toUChar(mod_reg_rm & 7);
           DIS(buf, "%s(%s)", segRegTxt(pfx), nameIRegRexB(8,pfx,rm));
           *len = 1;
           return disAMode_copy2tmp(
                  handleAddrOverrides(vbi, pfx, getIRegRexB(8,pfx,rm)));
         }

      case 0x08: case 0x09: case 0x0A: case 0x0B: 
       case 0x0D: case 0x0E: case 0x0F:
         { UChar rm = toUChar(mod_reg_rm & 7);
           Long d   = getSDisp8(delta);
           if (d == 0) {
              DIS(buf, "%s(%s)", segRegTxt(pfx), nameIRegRexB(8,pfx,rm));
           } else {
              DIS(buf, "%s%lld(%s)", segRegTxt(pfx), d, nameIRegRexB(8,pfx,rm));
           }
           *len = 2;
           return disAMode_copy2tmp(
                  handleAddrOverrides(vbi, pfx,
                     binop(Iop_Add64,getIRegRexB(8,pfx,rm),mkU64(d))));
         }

      case 0x10: case 0x11: case 0x12: case 0x13: 
       case 0x15: case 0x16: case 0x17:
         { UChar rm = toUChar(mod_reg_rm & 7);
           Long  d  = getSDisp32(delta);
           DIS(buf, "%s%lld(%s)", segRegTxt(pfx), d, nameIRegRexB(8,pfx,rm));
           *len = 5;
           return disAMode_copy2tmp(
                  handleAddrOverrides(vbi, pfx,
                     binop(Iop_Add64,getIRegRexB(8,pfx,rm),mkU64(d))));
         }

      
      
      case 0x18: case 0x19: case 0x1A: case 0x1B:
      case 0x1C: case 0x1D: case 0x1E: case 0x1F:
         vpanic("disAMode(amd64): not an addr!");

      case 0x05: 
         { Long d = getSDisp32(delta);
           *len = 5;
           DIS(buf, "%s%lld(%%rip)", segRegTxt(pfx), d);
           guest_RIP_next_mustcheck = True;
           guest_RIP_next_assumed = guest_RIP_bbstart 
                                    + delta+4 + extra_bytes;
           return disAMode_copy2tmp( 
                     handleAddrOverrides(vbi, pfx, 
                        binop(Iop_Add64, mkU64(guest_RIP_next_assumed), 
                                         mkU64(d))));
         }

      case 0x04: {
         UChar sib     = getUChar(delta);
         UChar scale   = toUChar((sib >> 6) & 3);
         UChar index_r = toUChar((sib >> 3) & 7);
         UChar base_r  = toUChar(sib & 7);
         
         Bool  base_is_BPor13 = toBool(base_r == R_RBP);
         Bool  index_is_SP    = toBool(index_r == R_RSP && 0==getRexX(pfx));
         delta++;

         if ((!index_is_SP) && (!base_is_BPor13)) {
            if (scale == 0) {
               DIS(buf, "%s(%s,%s)", segRegTxt(pfx), 
                         nameIRegRexB(8,pfx,base_r), 
                         nameIReg64rexX(pfx,index_r));
            } else {
               DIS(buf, "%s(%s,%s,%d)", segRegTxt(pfx), 
                         nameIRegRexB(8,pfx,base_r), 
                         nameIReg64rexX(pfx,index_r), 1<<scale);
            }
            *len = 2;
            return
               disAMode_copy2tmp( 
               handleAddrOverrides(vbi, pfx,
                  binop(Iop_Add64, 
                        getIRegRexB(8,pfx,base_r),
                        binop(Iop_Shl64, getIReg64rexX(pfx,index_r),
                              mkU8(scale)))));
         }

         if ((!index_is_SP) && base_is_BPor13) {
            Long d = getSDisp32(delta);
            DIS(buf, "%s%lld(,%s,%d)", segRegTxt(pfx), d, 
                      nameIReg64rexX(pfx,index_r), 1<<scale);
            *len = 6;
            return
               disAMode_copy2tmp(
               handleAddrOverrides(vbi, pfx, 
                  binop(Iop_Add64,
                        binop(Iop_Shl64, getIReg64rexX(pfx,index_r), 
                                         mkU8(scale)),
                        mkU64(d))));
         }

         if (index_is_SP && (!base_is_BPor13)) {
            DIS(buf, "%s(%s)", segRegTxt(pfx), nameIRegRexB(8,pfx,base_r));
            *len = 2;
            return disAMode_copy2tmp(
                   handleAddrOverrides(vbi, pfx, getIRegRexB(8,pfx,base_r)));
         }

         if (index_is_SP && base_is_BPor13) {
            Long d = getSDisp32(delta);
            DIS(buf, "%s%lld", segRegTxt(pfx), d);
            *len = 6;
            return disAMode_copy2tmp(
                   handleAddrOverrides(vbi, pfx, mkU64(d)));
         }

         vassert(0);
      }

      case 0x0C: {
         UChar sib     = getUChar(delta);
         UChar scale   = toUChar((sib >> 6) & 3);
         UChar index_r = toUChar((sib >> 3) & 7);
         UChar base_r  = toUChar(sib & 7);
         Long d        = getSDisp8(delta+1);

         if (index_r == R_RSP && 0==getRexX(pfx)) {
            DIS(buf, "%s%lld(%s)", segRegTxt(pfx), 
                                   d, nameIRegRexB(8,pfx,base_r));
            *len = 3;
            return disAMode_copy2tmp(
                   handleAddrOverrides(vbi, pfx, 
                      binop(Iop_Add64, getIRegRexB(8,pfx,base_r), mkU64(d)) ));
         } else {
            if (scale == 0) {
               DIS(buf, "%s%lld(%s,%s)", segRegTxt(pfx), d, 
                         nameIRegRexB(8,pfx,base_r), 
                         nameIReg64rexX(pfx,index_r));
            } else {
               DIS(buf, "%s%lld(%s,%s,%d)", segRegTxt(pfx), d, 
                         nameIRegRexB(8,pfx,base_r), 
                         nameIReg64rexX(pfx,index_r), 1<<scale);
            }
            *len = 3;
            return 
                disAMode_copy2tmp(
                handleAddrOverrides(vbi, pfx,
                  binop(Iop_Add64,
                        binop(Iop_Add64, 
                              getIRegRexB(8,pfx,base_r), 
                              binop(Iop_Shl64, 
                                    getIReg64rexX(pfx,index_r), mkU8(scale))),
                        mkU64(d))));
         }
         vassert(0); 
      }

      case 0x14: {
         UChar sib     = getUChar(delta);
         UChar scale   = toUChar((sib >> 6) & 3);
         UChar index_r = toUChar((sib >> 3) & 7);
         UChar base_r  = toUChar(sib & 7);
         Long d        = getSDisp32(delta+1);

         if (index_r == R_RSP && 0==getRexX(pfx)) {
            DIS(buf, "%s%lld(%s)", segRegTxt(pfx), 
                                   d, nameIRegRexB(8,pfx,base_r));
            *len = 6;
            return disAMode_copy2tmp(
                   handleAddrOverrides(vbi, pfx, 
                      binop(Iop_Add64, getIRegRexB(8,pfx,base_r), mkU64(d)) ));
         } else {
            if (scale == 0) {
               DIS(buf, "%s%lld(%s,%s)", segRegTxt(pfx), d, 
                         nameIRegRexB(8,pfx,base_r), 
                         nameIReg64rexX(pfx,index_r));
            } else {
               DIS(buf, "%s%lld(%s,%s,%d)", segRegTxt(pfx), d, 
                         nameIRegRexB(8,pfx,base_r), 
                         nameIReg64rexX(pfx,index_r), 1<<scale);
            }
            *len = 6;
            return 
                disAMode_copy2tmp(
                handleAddrOverrides(vbi, pfx,
                  binop(Iop_Add64,
                        binop(Iop_Add64, 
                              getIRegRexB(8,pfx,base_r), 
                              binop(Iop_Shl64, 
                                    getIReg64rexX(pfx,index_r), mkU8(scale))),
                        mkU64(d))));
         }
         vassert(0); 
      }

      default:
         vpanic("disAMode(amd64)");
         return 0; 
   }
}


static
IRTemp disAVSIBMode ( Int* len,
                      const VexAbiInfo* vbi, Prefix pfx, Long delta,
                      HChar* buf, UInt* rI,
                      IRType ty, Int* vscale )
{
   UChar mod_reg_rm = getUChar(delta);
   const HChar *vindex;

   *len = 0;
   *rI = 0;
   *vscale = 0;
   buf[0] = (UChar)0;
   if ((mod_reg_rm & 7) != 4 || epartIsReg(mod_reg_rm))
      return IRTemp_INVALID;

   UChar sib     = getUChar(delta+1);
   UChar scale   = toUChar((sib >> 6) & 3);
   UChar index_r = toUChar((sib >> 3) & 7);
   UChar base_r  = toUChar(sib & 7);
   Long  d       = 0;
   
   Bool  base_is_BPor13 = toBool(base_r == R_RBP);
   delta += 2;
   *len = 2;

   *rI = index_r | (getRexX(pfx) << 3);
   if (ty == Ity_V128)
      vindex = nameXMMReg(*rI);
   else
      vindex = nameYMMReg(*rI);
   *vscale = 1<<scale;

   switch (mod_reg_rm >> 6) {
   case 0:
      if (base_is_BPor13) {
         d = getSDisp32(delta);
         *len += 4;
         if (scale == 0) {
            DIS(buf, "%s%lld(,%s)", segRegTxt(pfx), d, vindex);
         } else {
            DIS(buf, "%s%lld(,%s,%d)", segRegTxt(pfx), d, vindex, 1<<scale);
         }
         return disAMode_copy2tmp( mkU64(d) );
      } else {
         if (scale == 0) {
            DIS(buf, "%s(%s,%s)", segRegTxt(pfx),
                     nameIRegRexB(8,pfx,base_r), vindex);
         } else {
            DIS(buf, "%s(%s,%s,%d)", segRegTxt(pfx),
                     nameIRegRexB(8,pfx,base_r), vindex, 1<<scale);
         }
      }
      break;
   case 1:
      d = getSDisp8(delta);
      *len += 1;
      goto have_disp;
   case 2:
      d = getSDisp32(delta);
      *len += 4;
   have_disp:
      if (scale == 0) {
         DIS(buf, "%s%lld(%s,%s)", segRegTxt(pfx), d,
                  nameIRegRexB(8,pfx,base_r), vindex);
      } else {
         DIS(buf, "%s%lld(%s,%s,%d)", segRegTxt(pfx), d,
                  nameIRegRexB(8,pfx,base_r), vindex, 1<<scale);
      }
      break;
   }

   if (!d)
      return disAMode_copy2tmp( getIRegRexB(8,pfx,base_r) );
   return disAMode_copy2tmp( binop(Iop_Add64, getIRegRexB(8,pfx,base_r),
                                   mkU64(d)) );
}



static UInt lengthAMode ( Prefix pfx, Long delta )
{
   UChar mod_reg_rm = getUChar(delta);
   delta++;

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

      
      case 0x05: 
         return 5;

      case 0x04: {
         
         UChar sib     = getUChar(delta);
         UChar base_r  = toUChar(sib & 7);
         
         Bool  base_is_BPor13 = toBool(base_r == R_RBP);

         if (base_is_BPor13) {
            return 6;
         } else {
            return 2;
         }
      }

      
      case 0x0C:
         return 3;

      
      case 0x14:
         return 6;

      default:
         vpanic("lengthAMode(amd64)");
         return 0; 
   }
}



static
ULong dis_op2_E_G ( const VexAbiInfo* vbi,
                    Prefix      pfx,
                    Bool        addSubCarry,
                    IROp        op8, 
                    Bool        keep,
                    Int         size, 
                    Long        delta0,
                    const HChar* t_amd64opc )
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
          && offsetIRegG(size,pfx,rm) == offsetIRegE(size,pfx,rm)) {
         if (False && op8 == Iop_Sub8)
            vex_printf("vex amd64->IR: sbb %%r,%%r optimisation(1)\n");
         putIRegG(size,pfx,rm, mkU(ty,0));
      }

      assign( dst0, getIRegG(size,pfx,rm) );
      assign( src,  getIRegE(size,pfx,rm) );

      if (addSubCarry && op8 == Iop_Add8) {
         helper_ADC( size, dst1, dst0, src,
                     IRTemp_INVALID, IRTemp_INVALID, 0 );
         putIRegG(size, pfx, rm, mkexpr(dst1));
      } else
      if (addSubCarry && op8 == Iop_Sub8) {
         helper_SBB( size, dst1, dst0, src,
                     IRTemp_INVALID, IRTemp_INVALID, 0 );
         putIRegG(size, pfx, rm, mkexpr(dst1));
      } else {
         assign( dst1, binop(mkSizedOp(ty,op8), mkexpr(dst0), mkexpr(src)) );
         if (isAddSub(op8))
            setFlags_DEP1_DEP2(op8, dst0, src, ty);
         else
            setFlags_DEP1(op8, dst1, ty);
         if (keep)
            putIRegG(size, pfx, rm, mkexpr(dst1));
      }

      DIP("%s%c %s,%s\n", t_amd64opc, nameISize(size), 
                          nameIRegE(size,pfx,rm),
                          nameIRegG(size,pfx,rm));
      return 1+delta0;
   } else {
      
      addr = disAMode ( &len, vbi, pfx, delta0, dis_buf, 0 );
      assign( dst0, getIRegG(size,pfx,rm) );
      assign( src,  loadLE(szToITy(size), mkexpr(addr)) );

      if (addSubCarry && op8 == Iop_Add8) {
         helper_ADC( size, dst1, dst0, src,
                     IRTemp_INVALID, IRTemp_INVALID, 0 );
         putIRegG(size, pfx, rm, mkexpr(dst1));
      } else
      if (addSubCarry && op8 == Iop_Sub8) {
         helper_SBB( size, dst1, dst0, src,
                     IRTemp_INVALID, IRTemp_INVALID, 0 );
         putIRegG(size, pfx, rm, mkexpr(dst1));
      } else {
         assign( dst1, binop(mkSizedOp(ty,op8), mkexpr(dst0), mkexpr(src)) );
         if (isAddSub(op8))
            setFlags_DEP1_DEP2(op8, dst0, src, ty);
         else
            setFlags_DEP1(op8, dst1, ty);
         if (keep)
            putIRegG(size, pfx, rm, mkexpr(dst1));
      }

      DIP("%s%c %s,%s\n", t_amd64opc, nameISize(size), 
                          dis_buf, nameIRegG(size, pfx, rm));
      return len+delta0;
   }
}



static
ULong dis_op2_G_E ( const VexAbiInfo* vbi,
                    Prefix      pfx,
                    Bool        addSubCarry,
                    IROp        op8, 
                    Bool        keep,
                    Int         size, 
                    Long        delta0,
                    const HChar* t_amd64opc )
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
          && offsetIRegG(size,pfx,rm) == offsetIRegE(size,pfx,rm)) {
         putIRegE(size,pfx,rm, mkU(ty,0));
      }

      assign(dst0, getIRegE(size,pfx,rm));
      assign(src,  getIRegG(size,pfx,rm));

      if (addSubCarry && op8 == Iop_Add8) {
         helper_ADC( size, dst1, dst0, src,
                     IRTemp_INVALID, IRTemp_INVALID, 0 );
         putIRegE(size, pfx, rm, mkexpr(dst1));
      } else
      if (addSubCarry && op8 == Iop_Sub8) {
         helper_SBB( size, dst1, dst0, src,
                     IRTemp_INVALID, IRTemp_INVALID, 0 );
         putIRegE(size, pfx, rm, mkexpr(dst1));
      } else {
         assign(dst1, binop(mkSizedOp(ty,op8), mkexpr(dst0), mkexpr(src)));
         if (isAddSub(op8))
            setFlags_DEP1_DEP2(op8, dst0, src, ty);
         else
            setFlags_DEP1(op8, dst1, ty);
         if (keep)
            putIRegE(size, pfx, rm, mkexpr(dst1));
      }

      DIP("%s%c %s,%s\n", t_amd64opc, nameISize(size), 
                          nameIRegG(size,pfx,rm),
                          nameIRegE(size,pfx,rm));
      return 1+delta0;
   }

       
   {
      addr = disAMode ( &len, vbi, pfx, delta0, dis_buf, 0 );
      assign(dst0, loadLE(ty,mkexpr(addr)));
      assign(src,  getIRegG(size,pfx,rm));

      if (addSubCarry && op8 == Iop_Add8) {
         if (haveLOCK(pfx)) {
            
            helper_ADC( size, dst1, dst0, src,
                        addr, dst0, guest_RIP_curr_instr );
         } else {
            
            helper_ADC( size, dst1, dst0, src,
                        addr, IRTemp_INVALID, 0 );
         }
      } else
      if (addSubCarry && op8 == Iop_Sub8) {
         if (haveLOCK(pfx)) {
            
            helper_SBB( size, dst1, dst0, src,
                        addr, dst0, guest_RIP_curr_instr );
         } else {
            
            helper_SBB( size, dst1, dst0, src,
                        addr, IRTemp_INVALID, 0 );
         }
      } else {
         assign(dst1, binop(mkSizedOp(ty,op8), mkexpr(dst0), mkexpr(src)));
         if (keep) {
            if (haveLOCK(pfx)) {
               if (0) vex_printf("locked case\n" );
               casLE( mkexpr(addr),
                      mkexpr(dst0), 
                      mkexpr(dst1), guest_RIP_curr_instr );
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

      DIP("%s%c %s,%s\n", t_amd64opc, nameISize(size), 
                          nameIRegG(size,pfx,rm), dis_buf);
      return len+delta0;
   }
}


static
ULong dis_mov_E_G ( const VexAbiInfo* vbi,
                    Prefix      pfx,
                    Int         size, 
                    Long        delta0 )
{
   Int len;
   UChar rm = getUChar(delta0);
   HChar dis_buf[50];

   if (epartIsReg(rm)) {
      putIRegG(size, pfx, rm, getIRegE(size, pfx, rm));
      DIP("mov%c %s,%s\n", nameISize(size),
                           nameIRegE(size,pfx,rm),
                           nameIRegG(size,pfx,rm));
      return 1+delta0;
   }

       
   {
      IRTemp addr = disAMode ( &len, vbi, pfx, delta0, dis_buf, 0 );
      putIRegG(size, pfx, rm, loadLE(szToITy(size), mkexpr(addr)));
      DIP("mov%c %s,%s\n", nameISize(size), 
                           dis_buf,
                           nameIRegG(size,pfx,rm));
      return delta0+len;
   }
}


static
ULong dis_mov_G_E ( const VexAbiInfo*  vbi,
                    Prefix       pfx,
                    Int          size, 
                    Long         delta0,
                    Bool* ok )
{
   Int   len;
   UChar rm = getUChar(delta0);
   HChar dis_buf[50];

   *ok = True;

   if (epartIsReg(rm)) {
      if (haveF2orF3(pfx)) { *ok = False; return delta0; }
      putIRegE(size, pfx, rm, getIRegG(size, pfx, rm));
      DIP("mov%c %s,%s\n", nameISize(size),
                           nameIRegG(size,pfx,rm),
                           nameIRegE(size,pfx,rm));
      return 1+delta0;
   }

       
   {
      if (haveF2(pfx)) { *ok = False; return delta0; }
      
      IRTemp addr = disAMode ( &len, vbi, pfx, delta0, dis_buf, 0 );
      storeLE( mkexpr(addr), getIRegG(size, pfx, rm) );
      DIP("mov%c %s,%s\n", nameISize(size), 
                           nameIRegG(size,pfx,rm), 
                           dis_buf);
      return len+delta0;
   }
}


static
ULong dis_op_imm_A ( Int    size,
                     Bool   carrying,
                     IROp   op8,
                     Bool   keep,
                     Long   delta,
                     const HChar* t_amd64opc )
{
   Int    size4 = imin(size,4);
   IRType ty    = szToITy(size);
   IRTemp dst0  = newTemp(ty);
   IRTemp src   = newTemp(ty);
   IRTemp dst1  = newTemp(ty);
   Long  lit    = getSDisp(size4,delta);
   assign(dst0, getIRegRAX(size));
   assign(src,  mkU(ty,lit & mkSizeMask(size)));

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
      vpanic("dis_op_imm_A(amd64,guest)");

   if (keep)
      putIRegRAX(size, mkexpr(dst1));

   DIP("%s%c $%lld, %s\n", t_amd64opc, nameISize(size), 
                           lit, nameIRegRAX(size));
   return delta+size4;
}


static
ULong dis_movx_E_G ( const VexAbiInfo* vbi,
                     Prefix pfx,
                     Long delta, Int szs, Int szd, Bool sign_extend )
{
   UChar rm = getUChar(delta);
   if (epartIsReg(rm)) {
      putIRegG(szd, pfx, rm,
                    doScalarWidening(
                       szs,szd,sign_extend,
                       getIRegE(szs,pfx,rm)));
      DIP("mov%c%c%c %s,%s\n", sign_extend ? 's' : 'z',
                               nameISize(szs), 
                               nameISize(szd),
                               nameIRegE(szs,pfx,rm),
                               nameIRegG(szd,pfx,rm));
      return 1+delta;
   }

       
   {
      Int    len;
      HChar  dis_buf[50];
      IRTemp addr = disAMode ( &len, vbi, pfx, delta, dis_buf, 0 );
      putIRegG(szd, pfx, rm,
                    doScalarWidening(
                       szs,szd,sign_extend, 
                       loadLE(szToITy(szs),mkexpr(addr))));
      DIP("mov%c%c%c %s,%s\n", sign_extend ? 's' : 'z',
                               nameISize(szs), 
                               nameISize(szd),
                               dis_buf, 
                               nameIRegG(szd,pfx,rm));
      return len+delta;
   }
}


static
void codegen_div ( Int sz, IRTemp t, Bool signed_divide )
{
   
   if (sz == 8) {
      IROp   op     = signed_divide ? Iop_DivModS128to64 
                                    : Iop_DivModU128to64;
      IRTemp src128 = newTemp(Ity_I128);
      IRTemp dst128 = newTemp(Ity_I128);
      assign( src128, binop(Iop_64HLto128, 
                            getIReg64(R_RDX), 
                            getIReg64(R_RAX)) );
      assign( dst128, binop(op, mkexpr(src128), mkexpr(t)) );
      putIReg64( R_RAX, unop(Iop_128to64,mkexpr(dst128)) );
      putIReg64( R_RDX, unop(Iop_128HIto64,mkexpr(dst128)) );
   } else {
      IROp   op    = signed_divide ? Iop_DivModS64to32 
                                   : Iop_DivModU64to32;
      IRTemp src64 = newTemp(Ity_I64);
      IRTemp dst64 = newTemp(Ity_I64);
      switch (sz) {
      case 4:
         assign( src64, 
                 binop(Iop_32HLto64, getIRegRDX(4), getIRegRAX(4)) );
         assign( dst64, 
                 binop(op, mkexpr(src64), mkexpr(t)) );
         putIRegRAX( 4, unop(Iop_64to32,mkexpr(dst64)) );
         putIRegRDX( 4, unop(Iop_64HIto32,mkexpr(dst64)) );
         break;
      case 2: {
         IROp widen3264 = signed_divide ? Iop_32Sto64 : Iop_32Uto64;
         IROp widen1632 = signed_divide ? Iop_16Sto32 : Iop_16Uto32;
         assign( src64, unop(widen3264,
                             binop(Iop_16HLto32, 
                                   getIRegRDX(2), 
                                   getIRegRAX(2))) );
         assign( dst64, binop(op, mkexpr(src64), unop(widen1632,mkexpr(t))) );
         putIRegRAX( 2, unop(Iop_32to16,unop(Iop_64to32,mkexpr(dst64))) );
         putIRegRDX( 2, unop(Iop_32to16,unop(Iop_64HIto32,mkexpr(dst64))) );
         break;
      }
      case 1: {
         IROp widen3264 = signed_divide ? Iop_32Sto64 : Iop_32Uto64;
         IROp widen1632 = signed_divide ? Iop_16Sto32 : Iop_16Uto32;
         IROp widen816  = signed_divide ? Iop_8Sto16  : Iop_8Uto16;
         assign( src64, unop(widen3264, 
                        unop(widen1632, getIRegRAX(2))) );
         assign( dst64, 
                 binop(op, mkexpr(src64), 
                           unop(widen1632, unop(widen816, mkexpr(t)))) );
         putIRegRAX( 1, unop(Iop_16to8, 
                        unop(Iop_32to16,
                        unop(Iop_64to32,mkexpr(dst64)))) );
         putIRegAH( unop(Iop_16to8, 
                    unop(Iop_32to16,
                    unop(Iop_64HIto32,mkexpr(dst64)))) );
         break;
      }
      default: 
         vpanic("codegen_div(amd64)");
      }
   }
}

static 
ULong dis_Grp1 ( const VexAbiInfo* vbi,
                 Prefix pfx,
                 Long delta, UChar modrm, 
                 Int am_sz, Int d_sz, Int sz, Long d64 )
{
   Int     len;
   HChar   dis_buf[50];
   IRType  ty   = szToITy(sz);
   IRTemp  dst1 = newTemp(ty);
   IRTemp  src  = newTemp(ty);
   IRTemp  dst0 = newTemp(ty);
   IRTemp  addr = IRTemp_INVALID;
   IROp    op8  = Iop_INVALID;
   ULong   mask = mkSizeMask(sz);

   switch (gregLO3ofRM(modrm)) {
      case 0: op8 = Iop_Add8; break;  case 1: op8 = Iop_Or8;  break;
      case 2: break;  
      case 3: break;  
      case 4: op8 = Iop_And8; break;  case 5: op8 = Iop_Sub8; break;
      case 6: op8 = Iop_Xor8; break;  case 7: op8 = Iop_Sub8; break;
      
      default: vpanic("dis_Grp1(amd64): unhandled case");
   }

   if (epartIsReg(modrm)) {
      vassert(am_sz == 1);

      assign(dst0, getIRegE(sz,pfx,modrm));
      assign(src,  mkU(ty,d64 & mask));

      if (gregLO3ofRM(modrm) == 2 ) {
         helper_ADC( sz, dst1, dst0, src,
                     IRTemp_INVALID, IRTemp_INVALID, 0 );
      } else 
      if (gregLO3ofRM(modrm) == 3 ) {
         helper_SBB( sz, dst1, dst0, src,
                     IRTemp_INVALID, IRTemp_INVALID, 0 );
      } else {
         assign(dst1, binop(mkSizedOp(ty,op8), mkexpr(dst0), mkexpr(src)));
         if (isAddSub(op8))
            setFlags_DEP1_DEP2(op8, dst0, src, ty);
         else
            setFlags_DEP1(op8, dst1, ty);
      }

      if (gregLO3ofRM(modrm) < 7)
         putIRegE(sz, pfx, modrm, mkexpr(dst1));

      delta += (am_sz + d_sz);
      DIP("%s%c $%lld, %s\n", 
          nameGrp1(gregLO3ofRM(modrm)), nameISize(sz), d64, 
          nameIRegE(sz,pfx,modrm));
   } else {
      addr = disAMode ( &len, vbi, pfx, delta, dis_buf, d_sz );

      assign(dst0, loadLE(ty,mkexpr(addr)));
      assign(src, mkU(ty,d64 & mask));

      if (gregLO3ofRM(modrm) == 2 ) {
         if (haveLOCK(pfx)) {
            
            helper_ADC( sz, dst1, dst0, src,
                       addr, dst0, guest_RIP_curr_instr );
         } else {
            
            helper_ADC( sz, dst1, dst0, src,
                        addr, IRTemp_INVALID, 0 );
         }
      } else 
      if (gregLO3ofRM(modrm) == 3 ) {
         if (haveLOCK(pfx)) {
            
            helper_SBB( sz, dst1, dst0, src,
                       addr, dst0, guest_RIP_curr_instr );
         } else {
            
            helper_SBB( sz, dst1, dst0, src,
                        addr, IRTemp_INVALID, 0 );
         }
      } else {
         assign(dst1, binop(mkSizedOp(ty,op8), mkexpr(dst0), mkexpr(src)));
         if (gregLO3ofRM(modrm) < 7) {
            if (haveLOCK(pfx)) {
               casLE( mkexpr(addr), mkexpr(dst0), 
                                    mkexpr(dst1),
                                    guest_RIP_curr_instr );
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
      DIP("%s%c $%lld, %s\n", 
          nameGrp1(gregLO3ofRM(modrm)), nameISize(sz),
          d64, dis_buf);
   }
   return delta;
}



static
ULong dis_Grp2 ( const VexAbiInfo* vbi,
                 Prefix pfx,
                 Long delta, UChar modrm,
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

   vassert(sz == 1 || sz == 2 || sz == 4 || sz == 8);

   
   if (epartIsReg(modrm)) {
      assign(dst0, getIRegE(sz, pfx, modrm));
      delta += (am_sz + d_sz);
   } else {
      addr = disAMode ( &len, vbi, pfx, delta, dis_buf, d_sz );
      assign(dst0, loadLE(ty,mkexpr(addr)));
      delta += len + d_sz;
   }

   isShift = False;
   switch (gregLO3ofRM(modrm)) { case 4: case 5: case 6: case 7: isShift = True; }

   isRotate = False;
   switch (gregLO3ofRM(modrm)) { case 0: case 1: isRotate = True; }

   isRotateC = False;
   switch (gregLO3ofRM(modrm)) { case 2: case 3: isRotateC = True; }

   if (!isShift && !isRotate && !isRotateC) {
      
      vpanic("dis_Grp2(Reg): unhandled case(amd64)");
   }

   if (isRotateC) {
      Bool     left = toBool(gregLO3ofRM(modrm) == 2);
      IRExpr** argsVALUE;
      IRExpr** argsRFLAGS;

      IRTemp new_value  = newTemp(Ity_I64);
      IRTemp new_rflags = newTemp(Ity_I64);
      IRTemp old_rflags = newTemp(Ity_I64);

      assign( old_rflags, widenUto64(mk_amd64g_calculate_rflags_all()) );

      argsVALUE
         = mkIRExprVec_4( widenUto64(mkexpr(dst0)), 
                          widenUto64(shift_expr),   
                          mkexpr(old_rflags),
                          mkU64(sz) );
      assign( new_value, 
                 mkIRExprCCall(
                    Ity_I64, 
                    0, 
                    left ? "amd64g_calculate_RCL" : "amd64g_calculate_RCR",
                    left ? &amd64g_calculate_RCL  : &amd64g_calculate_RCR,
                    argsVALUE
                 )
            );
      
      argsRFLAGS
         = mkIRExprVec_4( widenUto64(mkexpr(dst0)), 
                          widenUto64(shift_expr),   
                          mkexpr(old_rflags),
                          mkU64(-sz) );
      assign( new_rflags, 
                 mkIRExprCCall(
                    Ity_I64, 
                    0, 
                    left ? "amd64g_calculate_RCL" : "amd64g_calculate_RCR",
                    left ? &amd64g_calculate_RCL  : &amd64g_calculate_RCR,
                    argsRFLAGS
                 )
            );

      assign( dst1, narrowTo(ty, mkexpr(new_value)) );
      stmt( IRStmt_Put( OFFB_CC_OP,   mkU64(AMD64G_CC_OP_COPY) ));
      stmt( IRStmt_Put( OFFB_CC_DEP1, mkexpr(new_rflags) ));
      stmt( IRStmt_Put( OFFB_CC_DEP2, mkU64(0) ));
      stmt( IRStmt_Put( OFFB_CC_NDEP, mkU64(0) ));
   }

   else
   if (isShift) {

      IRTemp pre64     = newTemp(Ity_I64);
      IRTemp res64     = newTemp(Ity_I64);
      IRTemp res64ss   = newTemp(Ity_I64);
      IRTemp shift_amt = newTemp(Ity_I8);
      UChar  mask      = toUChar(sz==8 ? 63 : 31);
      IROp   op64;

      switch (gregLO3ofRM(modrm)) { 
         case 4: op64 = Iop_Shl64; break;
         case 5: op64 = Iop_Shr64; break;
         case 6: op64 = Iop_Shl64; break;
         case 7: op64 = Iop_Sar64; break;
         
         default: vpanic("dis_Grp2:shift"); break;
      }

      
      assign( shift_amt, binop(Iop_And8, shift_expr, mkU8(mask)) );

      
      assign( pre64, op64==Iop_Sar64 ? widenSto64(mkexpr(dst0))
                                     : widenUto64(mkexpr(dst0)) );

      
      assign( res64, binop(op64, mkexpr(pre64), mkexpr(shift_amt)) );

      
      assign( res64ss,
              binop(op64,
                    mkexpr(pre64), 
                    binop(Iop_And8,
                          binop(Iop_Sub8,
                                mkexpr(shift_amt), mkU8(1)),
                          mkU8(mask))) );

      
      setFlags_DEP1_DEP2_shift(op64, res64, res64ss, ty, shift_amt);

      
      assign( dst1, narrowTo(ty, mkexpr(res64)) );

   } 

   else 
   if (isRotate) {
      Int    ccOp      = ty==Ity_I8 ? 0 : (ty==Ity_I16 ? 1 
                                        : (ty==Ity_I32 ? 2 : 3));
      Bool   left      = toBool(gregLO3ofRM(modrm) == 0);
      IRTemp rot_amt   = newTemp(Ity_I8);
      IRTemp rot_amt64 = newTemp(Ity_I8);
      IRTemp oldFlags  = newTemp(Ity_I64);
      UChar  mask      = toUChar(sz==8 ? 63 : 31);

      
      assign(rot_amt64, binop(Iop_And8, shift_expr, mkU8(mask)));

      if (ty == Ity_I64)
         assign(rot_amt, mkexpr(rot_amt64));
      else
         assign(rot_amt, binop(Iop_And8, mkexpr(rot_amt64), mkU8(8*sz-1)));

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
         ccOp += AMD64G_CC_OP_ROLB;

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
         ccOp += AMD64G_CC_OP_RORB;

      }


      assign(oldFlags, mk_amd64g_calculate_rflags_all());

      
      IRTemp rot_amt64b = newTemp(Ity_I1);
      assign(rot_amt64b, binop(Iop_CmpNE8, mkexpr(rot_amt64), mkU8(0)) );

      
      stmt( IRStmt_Put( OFFB_CC_OP,
                        IRExpr_ITE( mkexpr(rot_amt64b),
                                    mkU64(ccOp),
                                    IRExpr_Get(OFFB_CC_OP,Ity_I64) ) ));
      stmt( IRStmt_Put( OFFB_CC_DEP1, 
                        IRExpr_ITE( mkexpr(rot_amt64b),
                                    widenUto64(mkexpr(dst1)),
                                    IRExpr_Get(OFFB_CC_DEP1,Ity_I64) ) ));
      stmt( IRStmt_Put( OFFB_CC_DEP2, 
                        IRExpr_ITE( mkexpr(rot_amt64b),
                                    mkU64(0),
                                    IRExpr_Get(OFFB_CC_DEP2,Ity_I64) ) ));
      stmt( IRStmt_Put( OFFB_CC_NDEP, 
                        IRExpr_ITE( mkexpr(rot_amt64b),
                                    mkexpr(oldFlags),
                                    IRExpr_Get(OFFB_CC_NDEP,Ity_I64) ) ));
   } 

   
   if (epartIsReg(modrm)) {
      putIRegE(sz, pfx, modrm, mkexpr(dst1));
      if (vex_traceflags & VEX_TRACE_FE) {
         vex_printf("%s%c ",
                    nameGrp2(gregLO3ofRM(modrm)), nameISize(sz) );
         if (shift_expr_txt)
            vex_printf("%s", shift_expr_txt);
         else
            ppIRExpr(shift_expr);
         vex_printf(", %s\n", nameIRegE(sz,pfx,modrm));
      }
   } else {
      storeLE(mkexpr(addr), mkexpr(dst1));
      if (vex_traceflags & VEX_TRACE_FE) {
         vex_printf("%s%c ",
                    nameGrp2(gregLO3ofRM(modrm)), nameISize(sz) );
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
ULong dis_Grp8_Imm ( const VexAbiInfo* vbi,
                     Prefix pfx,
                     Long delta, UChar modrm,
                     Int am_sz, Int sz, ULong src_val,
                     Bool* decode_OK )
{

   IRType ty     = szToITy(sz);
   IRTemp t2     = newTemp(Ity_I64);
   IRTemp t2m    = newTemp(Ity_I64);
   IRTemp t_addr = IRTemp_INVALID;
   HChar  dis_buf[50];
   ULong  mask;

   
   *decode_OK = True;

   
   if (epartIsReg(modrm)) {
      
      if (haveF2orF3(pfx)) {
         *decode_OK = False;
         return delta;
     }
   } else {
      if (haveF2orF3(pfx)) {
         if (haveF2andF3(pfx) || !haveLOCK(pfx)) {
            *decode_OK = False;
            return delta;
         }
      }
   }

   switch (sz) {
      case 2:  src_val &= 15; break;
      case 4:  src_val &= 31; break;
      case 8:  src_val &= 63; break;
      default: *decode_OK = False; return delta;
   }

   
   switch (gregLO3ofRM(modrm)) {
      case 4:   mask = 0;                  break;
      case 5:  mask = 1ULL << src_val;    break;
      case 6:  mask = ~(1ULL << src_val); break;
      case 7:  mask = 1ULL << src_val;    break;
      default: *decode_OK = False; return delta;
   }

   if (epartIsReg(modrm)) {
      vassert(am_sz == 1);
      assign( t2, widenUto64(getIRegE(sz, pfx, modrm)) );
      delta += (am_sz + 1);
      DIP("%s%c $0x%llx, %s\n", nameGrp8(gregLO3ofRM(modrm)), 
                                nameISize(sz),
                                src_val, nameIRegE(sz,pfx,modrm));
   } else {
      Int len;
      t_addr = disAMode ( &len, vbi, pfx, delta, dis_buf, 1 );
      delta  += (len+1);
      assign( t2, widenUto64(loadLE(ty, mkexpr(t_addr))) );
      DIP("%s%c $0x%llx, %s\n", nameGrp8(gregLO3ofRM(modrm)), 
                                nameISize(sz),
                                src_val, dis_buf);
   }

   
   switch (gregLO3ofRM(modrm)) {
      case 4: 
         break;
      case 5: 
         assign( t2m, binop(Iop_Or64, mkU64(mask), mkexpr(t2)) );
         break;
      case 6: 
         assign( t2m, binop(Iop_And64, mkU64(mask), mkexpr(t2)) );
         break;
      case 7: 
         assign( t2m, binop(Iop_Xor64, mkU64(mask), mkexpr(t2)) );
         break;
     default: 
          
         vassert(0);
   }

   
   if (gregLO3ofRM(modrm) != 4 ) {
      if (epartIsReg(modrm)) {
        putIRegE(sz, pfx, modrm, narrowTo(ty, mkexpr(t2m)));
      } else {
         if (haveLOCK(pfx)) {
            casLE( mkexpr(t_addr),
                   narrowTo(ty, mkexpr(t2)),
                   narrowTo(ty, mkexpr(t2m)),
                   guest_RIP_curr_instr );
         } else {
            storeLE(mkexpr(t_addr), narrowTo(ty, mkexpr(t2m)));
         }
      }
   }

   
   
   stmt( IRStmt_Put( OFFB_CC_OP,   mkU64(AMD64G_CC_OP_COPY) ));
   stmt( IRStmt_Put( OFFB_CC_DEP2, mkU64(0) ));
   stmt( IRStmt_Put( 
            OFFB_CC_DEP1,
            binop(Iop_And64,
                  binop(Iop_Shr64, mkexpr(t2), mkU8(src_val)),
                  mkU64(1))
       ));
   stmt( IRStmt_Put( OFFB_CC_NDEP, mkU64(0) ));

   return delta;
}


static void codegen_mulL_A_D ( Int sz, Bool syned, 
                               IRTemp tmp, const HChar* tmp_txt )
{
   IRType ty = szToITy(sz);
   IRTemp t1 = newTemp(ty);

   assign( t1, getIRegRAX(sz) );

   switch (ty) {
      case Ity_I64: {
         IRTemp res128  = newTemp(Ity_I128);
         IRTemp resHi   = newTemp(Ity_I64);
         IRTemp resLo   = newTemp(Ity_I64);
         IROp   mulOp   = syned ? Iop_MullS64 : Iop_MullU64;
         UInt   tBaseOp = syned ? AMD64G_CC_OP_SMULB : AMD64G_CC_OP_UMULB;
         setFlags_MUL ( Ity_I64, t1, tmp, tBaseOp );
         assign( res128, binop(mulOp, mkexpr(t1), mkexpr(tmp)) );
         assign( resHi, unop(Iop_128HIto64,mkexpr(res128)));
         assign( resLo, unop(Iop_128to64,mkexpr(res128)));
         putIReg64(R_RDX, mkexpr(resHi));
         putIReg64(R_RAX, mkexpr(resLo));
         break;
      }
      case Ity_I32: {
         IRTemp res64   = newTemp(Ity_I64);
         IRTemp resHi   = newTemp(Ity_I32);
         IRTemp resLo   = newTemp(Ity_I32);
         IROp   mulOp   = syned ? Iop_MullS32 : Iop_MullU32;
         UInt   tBaseOp = syned ? AMD64G_CC_OP_SMULB : AMD64G_CC_OP_UMULB;
         setFlags_MUL ( Ity_I32, t1, tmp, tBaseOp );
         assign( res64, binop(mulOp, mkexpr(t1), mkexpr(tmp)) );
         assign( resHi, unop(Iop_64HIto32,mkexpr(res64)));
         assign( resLo, unop(Iop_64to32,mkexpr(res64)));
         putIRegRDX(4, mkexpr(resHi));
         putIRegRAX(4, mkexpr(resLo));
         break;
      }
      case Ity_I16: {
         IRTemp res32   = newTemp(Ity_I32);
         IRTemp resHi   = newTemp(Ity_I16);
         IRTemp resLo   = newTemp(Ity_I16);
         IROp   mulOp   = syned ? Iop_MullS16 : Iop_MullU16;
         UInt   tBaseOp = syned ? AMD64G_CC_OP_SMULB : AMD64G_CC_OP_UMULB;
         setFlags_MUL ( Ity_I16, t1, tmp, tBaseOp );
         assign( res32, binop(mulOp, mkexpr(t1), mkexpr(tmp)) );
         assign( resHi, unop(Iop_32HIto16,mkexpr(res32)));
         assign( resLo, unop(Iop_32to16,mkexpr(res32)));
         putIRegRDX(2, mkexpr(resHi));
         putIRegRAX(2, mkexpr(resLo));
         break;
      }
      case Ity_I8: {
         IRTemp res16   = newTemp(Ity_I16);
         IRTemp resHi   = newTemp(Ity_I8);
         IRTemp resLo   = newTemp(Ity_I8);
         IROp   mulOp   = syned ? Iop_MullS8 : Iop_MullU8;
         UInt   tBaseOp = syned ? AMD64G_CC_OP_SMULB : AMD64G_CC_OP_UMULB;
         setFlags_MUL ( Ity_I8, t1, tmp, tBaseOp );
         assign( res16, binop(mulOp, mkexpr(t1), mkexpr(tmp)) );
         assign( resHi, unop(Iop_16HIto8,mkexpr(res16)));
         assign( resLo, unop(Iop_16to8,mkexpr(res16)));
         putIRegRAX(2, mkexpr(res16));
         break;
      }
      default:
         ppIRType(ty);
         vpanic("codegen_mulL_A_D(amd64)");
   }
   DIP("%s%c %s\n", syned ? "imul" : "mul", nameISize(sz), tmp_txt);
}


static 
ULong dis_Grp3 ( const VexAbiInfo* vbi, 
                 Prefix pfx, Int sz, Long delta, Bool* decode_OK )
{
   Long    d64;
   UChar   modrm;
   HChar   dis_buf[50];
   Int     len;
   IRTemp  addr;
   IRType  ty = szToITy(sz);
   IRTemp  t1 = newTemp(ty);
   IRTemp dst1, src, dst0;
   *decode_OK = True;
   modrm = getUChar(delta);
   if (epartIsReg(modrm)) {
      
      if (haveF2orF3(pfx)) goto unhandled;
      switch (gregLO3ofRM(modrm)) {
         case 0: { 
            delta++; 
            d64 = getSDisp(imin(4,sz), delta); 
            delta += imin(4,sz);
            dst1 = newTemp(ty);
            assign(dst1, binop(mkSizedOp(ty,Iop_And8),
                               getIRegE(sz,pfx,modrm),
                               mkU(ty, d64 & mkSizeMask(sz))));
            setFlags_DEP1( Iop_And8, dst1, ty );
            DIP("test%c $%lld, %s\n", 
                nameISize(sz), d64, 
                nameIRegE(sz, pfx, modrm));
            break;
         }
         case 1:
            *decode_OK = False;
            return delta;
         case 2: 
            delta++;
            putIRegE(sz, pfx, modrm,
                              unop(mkSizedOp(ty,Iop_Not8),
                                   getIRegE(sz, pfx, modrm)));
            DIP("not%c %s\n", nameISize(sz), 
                              nameIRegE(sz, pfx, modrm));
            break;
         case 3: 
            delta++;
            dst0 = newTemp(ty);
            src  = newTemp(ty);
            dst1 = newTemp(ty);
            assign(dst0, mkU(ty,0));
            assign(src,  getIRegE(sz, pfx, modrm));
            assign(dst1, binop(mkSizedOp(ty,Iop_Sub8), mkexpr(dst0),
                                                       mkexpr(src)));
            setFlags_DEP1_DEP2(Iop_Sub8, dst0, src, ty);
            putIRegE(sz, pfx, modrm, mkexpr(dst1));
            DIP("neg%c %s\n", nameISize(sz), nameIRegE(sz, pfx, modrm));
            break;
         case 4: 
            delta++;
            src = newTemp(ty);
            assign(src, getIRegE(sz,pfx,modrm));
            codegen_mulL_A_D ( sz, False, src,
                               nameIRegE(sz,pfx,modrm) );
            break;
         case 5: 
            delta++;
            src = newTemp(ty);
            assign(src, getIRegE(sz,pfx,modrm));
            codegen_mulL_A_D ( sz, True, src,
                               nameIRegE(sz,pfx,modrm) );
            break;
         case 6: 
            delta++;
            assign( t1, getIRegE(sz, pfx, modrm) );
            codegen_div ( sz, t1, False );
            DIP("div%c %s\n", nameISize(sz), 
                              nameIRegE(sz, pfx, modrm));
            break;
         case 7: 
            delta++;
            assign( t1, getIRegE(sz, pfx, modrm) );
            codegen_div ( sz, t1, True );
            DIP("idiv%c %s\n", nameISize(sz), 
                               nameIRegE(sz, pfx, modrm));
            break;
         default: 
            
            vpanic("Grp3(amd64,R)");
      }
   } else {
      
      Bool validF2orF3 = haveF2orF3(pfx) ? False : True;
      if ((gregLO3ofRM(modrm) == 3 || gregLO3ofRM(modrm) == 2)
          && haveF2orF3(pfx) && !haveF2andF3(pfx) && haveLOCK(pfx)) {
         validF2orF3 = True;
      }
      if (!validF2orF3) goto unhandled;
      
      addr = disAMode ( &len, vbi, pfx, delta, dis_buf,
                        gregLO3ofRM(modrm)==0
                           ? imin(4,sz)
                           : 0
                      );
      t1   = newTemp(ty);
      delta += len;
      assign(t1, loadLE(ty,mkexpr(addr)));
      switch (gregLO3ofRM(modrm)) {
         case 0: { 
            d64 = getSDisp(imin(4,sz), delta); 
            delta += imin(4,sz);
            dst1 = newTemp(ty);
            assign(dst1, binop(mkSizedOp(ty,Iop_And8),
                               mkexpr(t1), 
                               mkU(ty, d64 & mkSizeMask(sz))));
            setFlags_DEP1( Iop_And8, dst1, ty );
            DIP("test%c $%lld, %s\n", nameISize(sz), d64, dis_buf);
            break;
         }
         case 1:
            *decode_OK = False;
            return delta;
         case 2: 
            dst1 = newTemp(ty);
            assign(dst1, unop(mkSizedOp(ty,Iop_Not8), mkexpr(t1)));
            if (haveLOCK(pfx)) {
               casLE( mkexpr(addr), mkexpr(t1), mkexpr(dst1),
                                    guest_RIP_curr_instr );
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
            assign(dst1, binop(mkSizedOp(ty,Iop_Sub8), mkexpr(dst0),
                                                       mkexpr(src)));
            if (haveLOCK(pfx)) {
               casLE( mkexpr(addr), mkexpr(t1), mkexpr(dst1),
                                    guest_RIP_curr_instr );
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
            
            vpanic("Grp3(amd64,M)");
      }
   }
   return delta;
  unhandled:
   *decode_OK = False;
   return delta;
}


static
ULong dis_Grp4 ( const VexAbiInfo* vbi,
                 Prefix pfx, Long delta, Bool* decode_OK )
{
   Int   alen;
   UChar modrm;
   HChar dis_buf[50];
   IRType ty = Ity_I8;
   IRTemp t1 = newTemp(ty);
   IRTemp t2 = newTemp(ty);

   *decode_OK = True;

   modrm = getUChar(delta);
   if (epartIsReg(modrm)) {
      
      if (haveF2orF3(pfx)) goto unhandled;
      assign(t1, getIRegE(1, pfx, modrm));
      switch (gregLO3ofRM(modrm)) {
         case 0: 
            assign(t2, binop(Iop_Add8, mkexpr(t1), mkU8(1)));
            putIRegE(1, pfx, modrm, mkexpr(t2));
            setFlags_INC_DEC( True, t2, ty );
            break;
         case 1: 
            assign(t2, binop(Iop_Sub8, mkexpr(t1), mkU8(1)));
            putIRegE(1, pfx, modrm, mkexpr(t2));
            setFlags_INC_DEC( False, t2, ty );
            break;
         default: 
            *decode_OK = False;
            return delta;
      }
      delta++;
      DIP("%sb %s\n", nameGrp4(gregLO3ofRM(modrm)),
                      nameIRegE(1, pfx, modrm));
   } else {
      
      Bool validF2orF3 = haveF2orF3(pfx) ? False : True;
      if ((gregLO3ofRM(modrm) == 0 || gregLO3ofRM(modrm) == 1)
          && haveF2orF3(pfx) && !haveF2andF3(pfx) && haveLOCK(pfx)) {
         validF2orF3 = True;
      }
      if (!validF2orF3) goto unhandled;
      
      IRTemp addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
      assign( t1, loadLE(ty, mkexpr(addr)) );
      switch (gregLO3ofRM(modrm)) {
         case 0: 
            assign(t2, binop(Iop_Add8, mkexpr(t1), mkU8(1)));
            if (haveLOCK(pfx)) {
               casLE( mkexpr(addr), mkexpr(t1), mkexpr(t2), 
                      guest_RIP_curr_instr );
            } else {
               storeLE( mkexpr(addr), mkexpr(t2) );
            }
            setFlags_INC_DEC( True, t2, ty );
            break;
         case 1: 
            assign(t2, binop(Iop_Sub8, mkexpr(t1), mkU8(1)));
            if (haveLOCK(pfx)) {
               casLE( mkexpr(addr), mkexpr(t1), mkexpr(t2), 
                      guest_RIP_curr_instr );
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
      DIP("%sb %s\n", nameGrp4(gregLO3ofRM(modrm)), dis_buf);
   }
   return delta;
  unhandled:
   *decode_OK = False;
   return delta;
}


static
ULong dis_Grp5 ( const VexAbiInfo* vbi,
                 Prefix pfx, Int sz, Long delta,
                 DisResult* dres, Bool* decode_OK )
{
   Int     len;
   UChar   modrm;
   HChar   dis_buf[50];
   IRTemp  addr = IRTemp_INVALID;
   IRType  ty = szToITy(sz);
   IRTemp  t1 = newTemp(ty);
   IRTemp  t2 = IRTemp_INVALID;
   IRTemp  t3 = IRTemp_INVALID;
   Bool    showSz = True;

   *decode_OK = True;

   modrm = getUChar(delta);
   if (epartIsReg(modrm)) {
     if (haveF2orF3(pfx)
         && ! (haveF2(pfx)
               && (gregLO3ofRM(modrm) == 2 || gregLO3ofRM(modrm) == 4)))
        goto unhandledR;
      assign(t1, getIRegE(sz,pfx,modrm));
      switch (gregLO3ofRM(modrm)) {
         case 0: 
            t2 = newTemp(ty);
            assign(t2, binop(mkSizedOp(ty,Iop_Add8),
                             mkexpr(t1), mkU(ty,1)));
            setFlags_INC_DEC( True, t2, ty );
            putIRegE(sz,pfx,modrm, mkexpr(t2));
            break;
         case 1: 
            t2 = newTemp(ty);
            assign(t2, binop(mkSizedOp(ty,Iop_Sub8),
                             mkexpr(t1), mkU(ty,1)));
            setFlags_INC_DEC( False, t2, ty );
            putIRegE(sz,pfx,modrm, mkexpr(t2));
            break;
         case 2: 
            
            if (!(sz == 4 || sz == 8)) goto unhandledR;
            if (haveF2(pfx)) DIP("bnd ; "); 
            sz = 8;
            t3 = newTemp(Ity_I64);
            assign(t3, getIRegE(sz,pfx,modrm));
            t2 = newTemp(Ity_I64);
            assign(t2, binop(Iop_Sub64, getIReg64(R_RSP), mkU64(8)));
            putIReg64(R_RSP, mkexpr(t2));
            storeLE( mkexpr(t2), mkU64(guest_RIP_bbstart+delta+1));
            make_redzone_AbiHint(vbi, t2, t3, "call-Ev(reg)");
            jmp_treg(dres, Ijk_Call, t3);
            vassert(dres->whatNext == Dis_StopHere);
            showSz = False;
            break;
         case 4: 
            
            if (!(sz == 4 || sz == 8)) goto unhandledR;
            if (haveF2(pfx)) DIP("bnd ; "); 
            sz = 8;
            t3 = newTemp(Ity_I64);
            assign(t3, getIRegE(sz,pfx,modrm));
            jmp_treg(dres, Ijk_Boring, t3);
            vassert(dres->whatNext == Dis_StopHere);
            showSz = False;
            break;
         case 6: 
            
            if (sz == 4) sz = 8;
            if (sz == 8 || sz == 2) {
               ty = szToITy(sz); 
               t3 = newTemp(ty);
               assign(t3, getIRegE(sz,pfx,modrm));
               t2 = newTemp(Ity_I64);
               assign( t2, binop(Iop_Sub64,getIReg64(R_RSP),mkU64(sz)) );
               putIReg64(R_RSP, mkexpr(t2) );
               storeLE( mkexpr(t2), mkexpr(t3) );
               break;
            } else {
               goto unhandledR; 
            }
         default:
         unhandledR:
            *decode_OK = False;
            return delta;
      }
      delta++;
      DIP("%s%c %s\n", nameGrp5(gregLO3ofRM(modrm)),
                       showSz ? nameISize(sz) : ' ', 
                       nameIRegE(sz, pfx, modrm));
   } else {
      
      Bool validF2orF3 = haveF2orF3(pfx) ? False : True;
      if ((gregLO3ofRM(modrm) == 0 || gregLO3ofRM(modrm) == 1)
          && haveF2orF3(pfx) && !haveF2andF3(pfx) && haveLOCK(pfx)) {
         validF2orF3 = True;
      } else if ((gregLO3ofRM(modrm) == 2 || gregLO3ofRM(modrm) == 4)
                 && (haveF2(pfx) && !haveF3(pfx))) {
         validF2orF3 = True;
      }
      if (!validF2orF3) goto unhandledM;
      
      addr = disAMode ( &len, vbi, pfx, delta, dis_buf, 0 );
      if (gregLO3ofRM(modrm) != 2 && gregLO3ofRM(modrm) != 4
                                  && gregLO3ofRM(modrm) != 6) {
         assign(t1, loadLE(ty,mkexpr(addr)));
      }
      switch (gregLO3ofRM(modrm)) {
         case 0:  
            t2 = newTemp(ty);
            assign(t2, binop(mkSizedOp(ty,Iop_Add8),
                             mkexpr(t1), mkU(ty,1)));
            if (haveLOCK(pfx)) {
               casLE( mkexpr(addr),
                      mkexpr(t1), mkexpr(t2), guest_RIP_curr_instr );
            } else {
               storeLE(mkexpr(addr),mkexpr(t2));
            }
            setFlags_INC_DEC( True, t2, ty );
            break;
         case 1:  
            t2 = newTemp(ty);
            assign(t2, binop(mkSizedOp(ty,Iop_Sub8),
                             mkexpr(t1), mkU(ty,1)));
            if (haveLOCK(pfx)) {
               casLE( mkexpr(addr),
                      mkexpr(t1), mkexpr(t2), guest_RIP_curr_instr );
            } else {
               storeLE(mkexpr(addr),mkexpr(t2));
            }
            setFlags_INC_DEC( False, t2, ty );
            break;
         case 2: 
            
            if (!(sz == 4 || sz == 8)) goto unhandledM;
            if (haveF2(pfx)) DIP("bnd ; "); 
            sz = 8;
            t3 = newTemp(Ity_I64);
            assign(t3, loadLE(Ity_I64,mkexpr(addr)));
            t2 = newTemp(Ity_I64);
            assign(t2, binop(Iop_Sub64, getIReg64(R_RSP), mkU64(8)));
            putIReg64(R_RSP, mkexpr(t2));
            storeLE( mkexpr(t2), mkU64(guest_RIP_bbstart+delta+len));
            make_redzone_AbiHint(vbi, t2, t3, "call-Ev(mem)");
            jmp_treg(dres, Ijk_Call, t3);
            vassert(dres->whatNext == Dis_StopHere);
            showSz = False;
            break;
         case 4: 
            
            if (!(sz == 4 || sz == 8)) goto unhandledM;
            if (haveF2(pfx)) DIP("bnd ; "); 
            sz = 8;
            t3 = newTemp(Ity_I64);
            assign(t3, loadLE(Ity_I64,mkexpr(addr)));
            jmp_treg(dres, Ijk_Boring, t3);
            vassert(dres->whatNext == Dis_StopHere);
            showSz = False;
            break;
         case 6: 
            
            if (sz == 4) sz = 8;
            if (sz == 8 || sz == 2) {
               ty = szToITy(sz); 
               t3 = newTemp(ty);
               assign(t3, loadLE(ty,mkexpr(addr)));
               t2 = newTemp(Ity_I64);
               assign( t2, binop(Iop_Sub64,getIReg64(R_RSP),mkU64(sz)) );
               putIReg64(R_RSP, mkexpr(t2) );
               storeLE( mkexpr(t2), mkexpr(t3) );
               break;
            } else {
               goto unhandledM; 
            }
         default: 
         unhandledM:
            *decode_OK = False;
            return delta;
      }
      delta += len;
      DIP("%s%c %s\n", nameGrp5(gregLO3ofRM(modrm)),
                       showSz ? nameISize(sz) : ' ', 
                       dis_buf);
   }
   return delta;
}



static
void dis_string_op_increment ( Int sz, IRTemp t_inc )
{
   UChar logSz;
   if (sz == 8 || sz == 4 || sz == 2) {
      logSz = 1;
      if (sz == 4) logSz = 2;
      if (sz == 8) logSz = 3;
      assign( t_inc, 
              binop(Iop_Shl64, IRExpr_Get( OFFB_DFLAG, Ity_I64 ),
                               mkU8(logSz) ) );
   } else {
      assign( t_inc, 
              IRExpr_Get( OFFB_DFLAG, Ity_I64 ) );
   }
}

static
void dis_string_op( void (*dis_OP)( Int, IRTemp, Prefix pfx ),
                    Int sz, const HChar* name, Prefix pfx )
{
   IRTemp t_inc = newTemp(Ity_I64);
   vassert(pfx == clearSegBits(pfx));
   dis_string_op_increment(sz, t_inc);
   dis_OP( sz, t_inc, pfx );
   DIP("%s%c\n", name, nameISize(sz));
}

static 
void dis_MOVS ( Int sz, IRTemp t_inc, Prefix pfx )
{
   IRType ty = szToITy(sz);
   IRTemp td = newTemp(Ity_I64);   
   IRTemp ts = newTemp(Ity_I64);   
   IRExpr *incd, *incs;

   if (haveASO(pfx)) {
      assign( td, unop(Iop_32Uto64, getIReg32(R_RDI)) );
      assign( ts, unop(Iop_32Uto64, getIReg32(R_RSI)) );
   } else {
      assign( td, getIReg64(R_RDI) );
      assign( ts, getIReg64(R_RSI) );
   }

   storeLE( mkexpr(td), loadLE(ty,mkexpr(ts)) );

   incd = binop(Iop_Add64, mkexpr(td), mkexpr(t_inc));
   incs = binop(Iop_Add64, mkexpr(ts), mkexpr(t_inc));
   if (haveASO(pfx)) {
      incd = unop(Iop_32Uto64, unop(Iop_64to32, incd));
      incs = unop(Iop_32Uto64, unop(Iop_64to32, incs));
   }
   putIReg64( R_RDI, incd );
   putIReg64( R_RSI, incs );
}

static 
void dis_LODS ( Int sz, IRTemp t_inc, Prefix pfx )
{
   IRType ty = szToITy(sz);
   IRTemp ts = newTemp(Ity_I64);   
   IRExpr *incs;

   if (haveASO(pfx))
      assign( ts, unop(Iop_32Uto64, getIReg32(R_RSI)) );
   else
      assign( ts, getIReg64(R_RSI) );

   putIRegRAX ( sz, loadLE(ty, mkexpr(ts)) );

   incs = binop(Iop_Add64, mkexpr(ts), mkexpr(t_inc));
   if (haveASO(pfx))
      incs = unop(Iop_32Uto64, unop(Iop_64to32, incs));
   putIReg64( R_RSI, incs );
}

static 
void dis_STOS ( Int sz, IRTemp t_inc, Prefix pfx )
{
   IRType ty = szToITy(sz);
   IRTemp ta = newTemp(ty);        
   IRTemp td = newTemp(Ity_I64);   
   IRExpr *incd;

   assign( ta, getIRegRAX(sz) );

   if (haveASO(pfx))
      assign( td, unop(Iop_32Uto64, getIReg32(R_RDI)) );
   else
      assign( td, getIReg64(R_RDI) );

   storeLE( mkexpr(td), mkexpr(ta) );

   incd = binop(Iop_Add64, mkexpr(td), mkexpr(t_inc));
   if (haveASO(pfx))
      incd = unop(Iop_32Uto64, unop(Iop_64to32, incd));
   putIReg64( R_RDI, incd );
}

static 
void dis_CMPS ( Int sz, IRTemp t_inc, Prefix pfx )
{
   IRType ty  = szToITy(sz);
   IRTemp tdv = newTemp(ty);      
   IRTemp tsv = newTemp(ty);      
   IRTemp td  = newTemp(Ity_I64); 
   IRTemp ts  = newTemp(Ity_I64); 
   IRExpr *incd, *incs;

   if (haveASO(pfx)) {
      assign( td, unop(Iop_32Uto64, getIReg32(R_RDI)) );
      assign( ts, unop(Iop_32Uto64, getIReg32(R_RSI)) );
   } else {
      assign( td, getIReg64(R_RDI) );
      assign( ts, getIReg64(R_RSI) );
   }

   assign( tdv, loadLE(ty,mkexpr(td)) );

   assign( tsv, loadLE(ty,mkexpr(ts)) );

   setFlags_DEP1_DEP2 ( Iop_Sub8, tsv, tdv, ty );

   incd = binop(Iop_Add64, mkexpr(td), mkexpr(t_inc));
   incs = binop(Iop_Add64, mkexpr(ts), mkexpr(t_inc));
   if (haveASO(pfx)) {
      incd = unop(Iop_32Uto64, unop(Iop_64to32, incd));
      incs = unop(Iop_32Uto64, unop(Iop_64to32, incs));
   }
   putIReg64( R_RDI, incd );
   putIReg64( R_RSI, incs );
}

static 
void dis_SCAS ( Int sz, IRTemp t_inc, Prefix pfx )
{
   IRType ty  = szToITy(sz);
   IRTemp ta  = newTemp(ty);       
   IRTemp td  = newTemp(Ity_I64);  
   IRTemp tdv = newTemp(ty);       
   IRExpr *incd;

   assign( ta, getIRegRAX(sz) );

   if (haveASO(pfx))
      assign( td, unop(Iop_32Uto64, getIReg32(R_RDI)) );
   else
      assign( td, getIReg64(R_RDI) );

   assign( tdv, loadLE(ty,mkexpr(td)) );

   setFlags_DEP1_DEP2 ( Iop_Sub8, ta, tdv, ty );

   incd = binop(Iop_Add64, mkexpr(td), mkexpr(t_inc));
   if (haveASO(pfx))
      incd = unop(Iop_32Uto64, unop(Iop_64to32, incd));
   putIReg64( R_RDI, incd );
}


static 
void dis_REP_op ( DisResult* dres,
                  AMD64Condcode cond,
                  void (*dis_OP)(Int, IRTemp, Prefix),
                  Int sz, Addr64 rip, Addr64 rip_next, const HChar* name,
                  Prefix pfx )
{
   IRTemp t_inc = newTemp(Ity_I64);
   IRTemp tc;
   IRExpr* cmp;

   vassert(pfx == clearSegBits(pfx));

   if (haveASO(pfx)) {
      tc = newTemp(Ity_I32);  
      assign( tc, getIReg32(R_RCX) );
      cmp = binop(Iop_CmpEQ32, mkexpr(tc), mkU32(0));
   } else {
      tc = newTemp(Ity_I64);  
      assign( tc, getIReg64(R_RCX) );
      cmp = binop(Iop_CmpEQ64, mkexpr(tc), mkU64(0));
   }

   stmt( IRStmt_Exit( cmp, Ijk_Boring,
                      IRConst_U64(rip_next), OFFB_RIP ) );

   if (haveASO(pfx))
      putIReg32(R_RCX, binop(Iop_Sub32, mkexpr(tc), mkU32(1)) );
  else
      putIReg64(R_RCX, binop(Iop_Sub64, mkexpr(tc), mkU64(1)) );

   dis_string_op_increment(sz, t_inc);
   dis_OP (sz, t_inc, pfx);

   if (cond == AMD64CondAlways) {
      jmp_lit(dres, Ijk_Boring, rip);
      vassert(dres->whatNext == Dis_StopHere);
   } else {
      stmt( IRStmt_Exit( mk_amd64g_calculate_condition(cond),
                         Ijk_Boring,
                         IRConst_U64(rip),
                         OFFB_RIP ) );
      jmp_lit(dres, Ijk_Boring, rip_next);
      vassert(dres->whatNext == Dis_StopHere);
   }
   DIP("%s%c\n", name, nameISize(sz));
}



static
ULong dis_mul_E_G ( const VexAbiInfo* vbi,
                    Prefix      pfx,
                    Int         size, 
                    Long        delta0 )
{
   Int    alen;
   HChar  dis_buf[50];
   UChar  rm = getUChar(delta0);
   IRType ty = szToITy(size);
   IRTemp te = newTemp(ty);
   IRTemp tg = newTemp(ty);
   IRTemp resLo = newTemp(ty);

   assign( tg, getIRegG(size, pfx, rm) );
   if (epartIsReg(rm)) {
      assign( te, getIRegE(size, pfx, rm) );
   } else {
      IRTemp addr = disAMode( &alen, vbi, pfx, delta0, dis_buf, 0 );
      assign( te, loadLE(ty,mkexpr(addr)) );
   }

   setFlags_MUL ( ty, te, tg, AMD64G_CC_OP_SMULB );

   assign( resLo, binop( mkSizedOp(ty, Iop_Mul8), mkexpr(te), mkexpr(tg) ) );

   putIRegG(size, pfx, rm, mkexpr(resLo) );

   if (epartIsReg(rm)) {
      DIP("imul%c %s, %s\n", nameISize(size), 
                             nameIRegE(size,pfx,rm),
                             nameIRegG(size,pfx,rm));
      return 1+delta0;
   } else {
      DIP("imul%c %s, %s\n", nameISize(size), 
                             dis_buf, 
                             nameIRegG(size,pfx,rm));
      return alen+delta0;
   }
}


static
ULong dis_imul_I_E_G ( const VexAbiInfo* vbi,
                       Prefix      pfx,
                       Int         size, 
                       Long        delta,
                       Int         litsize )
{
   Long   d64;
   Int    alen;
   HChar  dis_buf[50];
   UChar  rm = getUChar(delta);
   IRType ty = szToITy(size);
   IRTemp te = newTemp(ty);
   IRTemp tl = newTemp(ty);
   IRTemp resLo = newTemp(ty);

   vassert( size == 2 || size == 4 || size == 8);

   if (epartIsReg(rm)) {
      assign(te, getIRegE(size, pfx, rm));
      delta++;
   } else {
      IRTemp addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 
                                     imin(4,litsize) );
      assign(te, loadLE(ty, mkexpr(addr)));
      delta += alen;
   }
   d64 = getSDisp(imin(4,litsize),delta);
   delta += imin(4,litsize);

   d64 &= mkSizeMask(size);
   assign(tl, mkU(ty,d64));

   assign( resLo, binop( mkSizedOp(ty, Iop_Mul8), mkexpr(te), mkexpr(tl) ));

   setFlags_MUL ( ty, te, tl, AMD64G_CC_OP_SMULB );

   putIRegG(size, pfx, rm, mkexpr(resLo));

   DIP("imul%c $%lld, %s, %s\n", 
       nameISize(size), d64, 
       ( epartIsReg(rm) ? nameIRegE(size,pfx,rm) : dis_buf ),
       nameIRegG(size,pfx,rm) );
   return delta;
}


static IRTemp gen_POPCOUNT ( IRType ty, IRTemp src )
{
   Int i;
   if (ty == Ity_I16) {
      IRTemp old = IRTemp_INVALID;
      IRTemp nyu = IRTemp_INVALID;
      IRTemp mask[4], shift[4];
      for (i = 0; i < 4; i++) {
         mask[i]  = newTemp(ty);
         shift[i] = 1 << i;
      }
      assign(mask[0], mkU16(0x5555));
      assign(mask[1], mkU16(0x3333));
      assign(mask[2], mkU16(0x0F0F));
      assign(mask[3], mkU16(0x00FF));
      old = src;
      for (i = 0; i < 4; i++) {
         nyu = newTemp(ty);
         assign(nyu,
                binop(Iop_Add16, 
                      binop(Iop_And16,
                            mkexpr(old),
                            mkexpr(mask[i])),
                      binop(Iop_And16,
                            binop(Iop_Shr16, mkexpr(old), mkU8(shift[i])),
                            mkexpr(mask[i]))));
         old = nyu;
      }
      return nyu;
   }
   if (ty == Ity_I32) {
      IRTemp old = IRTemp_INVALID;
      IRTemp nyu = IRTemp_INVALID;
      IRTemp mask[5], shift[5];
      for (i = 0; i < 5; i++) {
         mask[i]  = newTemp(ty);
         shift[i] = 1 << i;
      }
      assign(mask[0], mkU32(0x55555555));
      assign(mask[1], mkU32(0x33333333));
      assign(mask[2], mkU32(0x0F0F0F0F));
      assign(mask[3], mkU32(0x00FF00FF));
      assign(mask[4], mkU32(0x0000FFFF));
      old = src;
      for (i = 0; i < 5; i++) {
         nyu = newTemp(ty);
         assign(nyu,
                binop(Iop_Add32, 
                      binop(Iop_And32,
                            mkexpr(old),
                            mkexpr(mask[i])),
                      binop(Iop_And32,
                            binop(Iop_Shr32, mkexpr(old), mkU8(shift[i])),
                            mkexpr(mask[i]))));
         old = nyu;
      }
      return nyu;
   }
   if (ty == Ity_I64) {
      IRTemp old = IRTemp_INVALID;
      IRTemp nyu = IRTemp_INVALID;
      IRTemp mask[6], shift[6];
      for (i = 0; i < 6; i++) {
         mask[i]  = newTemp(ty);
         shift[i] = 1 << i;
      }
      assign(mask[0], mkU64(0x5555555555555555ULL));
      assign(mask[1], mkU64(0x3333333333333333ULL));
      assign(mask[2], mkU64(0x0F0F0F0F0F0F0F0FULL));
      assign(mask[3], mkU64(0x00FF00FF00FF00FFULL));
      assign(mask[4], mkU64(0x0000FFFF0000FFFFULL));
      assign(mask[5], mkU64(0x00000000FFFFFFFFULL));
      old = src;
      for (i = 0; i < 6; i++) {
         nyu = newTemp(ty);
         assign(nyu,
                binop(Iop_Add64, 
                      binop(Iop_And64,
                            mkexpr(old),
                            mkexpr(mask[i])),
                      binop(Iop_And64,
                            binop(Iop_Shr64, mkexpr(old), mkU8(shift[i])),
                            mkexpr(mask[i]))));
         old = nyu;
      }
      return nyu;
   }
   
   vassert(0);
}


static IRTemp gen_LZCNT ( IRType ty, IRTemp src )
{
   vassert(ty == Ity_I64 || ty == Ity_I32 || ty == Ity_I16);

   IRTemp src64 = newTemp(Ity_I64);
   assign(src64, widenUto64( mkexpr(src) ));

   IRTemp src64x = newTemp(Ity_I64);
   assign(src64x, 
          binop(Iop_Shl64, mkexpr(src64),
                           mkU8(64 - 8 * sizeofIRType(ty))));

   
   
   IRTemp res64 = newTemp(Ity_I64);
   assign(res64,
          IRExpr_ITE(
             binop(Iop_CmpEQ64, mkexpr(src64x), mkU64(0)),
             mkU64(8 * sizeofIRType(ty)),
             unop(Iop_Clz64, mkexpr(src64x))
   ));

   IRTemp res = newTemp(ty);
   assign(res, narrowTo(ty, mkexpr(res64)));
   return res;
}


static IRTemp gen_TZCNT ( IRType ty, IRTemp src )
{
   vassert(ty == Ity_I64 || ty == Ity_I32 || ty == Ity_I16);

   IRTemp src64 = newTemp(Ity_I64);
   assign(src64, widenUto64( mkexpr(src) ));

   
   
   IRTemp res64 = newTemp(Ity_I64);
   assign(res64,
          IRExpr_ITE(
             binop(Iop_CmpEQ64, mkexpr(src64), mkU64(0)),
             mkU64(8 * sizeofIRType(ty)),
             unop(Iop_Ctz64, mkexpr(src64))
   ));

   IRTemp res = newTemp(ty);
   assign(res, narrowTo(ty, mkexpr(res64)));
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


static IRExpr*   get_C3210 ( void )
{
   return IRExpr_Get( OFFB_FC3210, Ity_I64 );
}

static void put_C3210 ( IRExpr* e   )
{
   vassert(typeOfIRExpr(irsb->tyenv, e) == Ity_I64);
   stmt( IRStmt_Put( OFFB_FC3210, e ) );
}

static IRExpr*  get_fpround ( void )
{
   return unop(Iop_64to32, IRExpr_Get( OFFB_FPROUND, Ity_I64 ));
}

static void put_fpround ( IRExpr*  e )
{
   vassert(typeOfIRExpr(irsb->tyenv, e) == Ity_I32);
   stmt( IRStmt_Put( OFFB_FPROUND, unop(Iop_32Uto64,e) ) );
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
   IRExpr* cleared = binop(Iop_And64, get_C3210(), mkU64(~AMD64G_FC_MASK_C2));
   put_C3210( binop(Iop_Or64,
                    cleared,
                    binop(Iop_Shl64, e, mkU8(AMD64G_FC_SHIFT_C2))) );
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
                       binop(Iop_And32, unop(Iop_64to32, get_C3210()), 
                                        mkU32(0x4700))
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
   DIP("f%s%s st(%u), st(%u)\n", op_txt, pop_after?"p":"", st_src, st_dst );
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
void fp_do_oprev_ST_ST ( const HChar* op_txt, IROp op, UInt st_src, UInt st_dst,
                         Bool pop_after )
{
   DIP("f%s%s st(%u), st(%u)\n", op_txt, pop_after?"p":"", st_src, st_dst );
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
   DIP("fucomi%s %%st(0),%%st(%u)\n", pop_after ? "p" : "", i);
   stmt( IRStmt_Put( OFFB_CC_OP,   mkU64(AMD64G_CC_OP_COPY) ));
   stmt( IRStmt_Put( OFFB_CC_DEP2, mkU64(0) ));
   stmt( IRStmt_Put( 
            OFFB_CC_DEP1,
            binop( Iop_And64,
                   unop( Iop_32Uto64,
                         binop(Iop_CmpF64, get_ST(0), get_ST(i))),
                   mkU64(0x45)
        )));
   if (pop_after)
      fp_pop();
}


static IRExpr* x87ishly_qnarrow_32_to_16 ( IRExpr* e32 )
{
   IRTemp t32 = newTemp(Ity_I32);
   assign( t32, e32 );
   return
      IRExpr_ITE( 
         binop(Iop_CmpLT64U, 
               unop(Iop_32Uto64, 
                    binop(Iop_Add32, mkexpr(t32), mkU32(32768))), 
               mkU64(65536)),
         unop(Iop_32to16, mkexpr(t32)),
         mkU16( 0x8000 ) );
}


static
ULong dis_FPU ( Bool* decode_ok, 
                const VexAbiInfo* vbi, Prefix pfx, Long delta )
{
   Int    len;
   UInt   r_src, r_dst;
   HChar  dis_buf[50];
   IRTemp t1, t2;

   UChar first_opcode = getUChar(delta-1);
   UChar modrm        = getUChar(delta+0);

   

   if (first_opcode == 0xD8) {
      if (modrm < 0xC0) {

         IRTemp addr = disAMode( &len, vbi, pfx, delta, dis_buf, 0 );
         delta += len;

         switch (gregLO3ofRM(modrm)) {

            case 0: 
               fp_do_op_mem_ST_0 ( addr, "add", dis_buf, Iop_AddF64, False );
               break;

            case 1: 
               fp_do_op_mem_ST_0 ( addr, "mul", dis_buf, Iop_MulF64, False );
               break;

            case 2: 
               DIP("fcoms %s\n", dis_buf);
               
               put_C3210( 
                   unop( Iop_32Uto64,
                       binop( Iop_And32,
                              binop(Iop_Shl32, 
                                    binop(Iop_CmpF64, 
                                          get_ST(0),
                                          unop(Iop_F32toF64, 
                                               loadLE(Ity_F32,mkexpr(addr)))),
                                    mkU8(8)),
                              mkU32(0x4500)
                   )));
               break;  

            case 3: 
               DIP("fcomps %s\n", dis_buf);
               
               put_C3210( 
                   unop( Iop_32Uto64,
                       binop( Iop_And32,
                              binop(Iop_Shl32, 
                                    binop(Iop_CmpF64, 
                                          get_ST(0),
                                          unop(Iop_F32toF64, 
                                               loadLE(Ity_F32,mkexpr(addr)))),
                                    mkU8(8)),
                              mkU32(0x4500)
                   )));
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
               vex_printf("unhandled opc_aux = 0x%2x\n", gregLO3ofRM(modrm));
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
               DIP("fcom %%st(0),%%st(%d)\n", r_dst);
               
               put_C3210( 
                   unop(Iop_32Uto64,
                   binop( Iop_And32,
                          binop(Iop_Shl32, 
                                binop(Iop_CmpF64, get_ST(0), get_ST(r_dst)),
                                mkU8(8)),
                          mkU32(0x4500)
                   )));
               break;

            
            case 0xD8 ... 0xDF: 
               r_dst = (UInt)modrm - 0xD8;
               DIP("fcomp %%st(0),%%st(%d)\n", r_dst);
               
               put_C3210( 
                   unop(Iop_32Uto64,
                   binop( Iop_And32,
                          binop(Iop_Shl32, 
                                binop(Iop_CmpF64, get_ST(0), get_ST(r_dst)),
                                mkU8(8)),
                          mkU32(0x4500)
                   )));
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

         IRTemp addr = disAMode( &len, vbi, pfx, delta, dis_buf, 0 );
         delta += len;

         switch (gregLO3ofRM(modrm)) {

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
               IRTemp    ew = newTemp(Ity_I32);
               IRTemp   w64 = newTemp(Ity_I64);
               IRDirty*   d = unsafeIRDirty_0_N ( 
                                 0, 
                                 "amd64g_dirtyhelper_FLDENV", 
                                 &amd64g_dirtyhelper_FLDENV,
                                 mkIRExprVec_2( IRExpr_BBPTR(), mkexpr(addr) )
                              );
               d->tmp       = w64;
               
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
               d->fxState[2].size   = sizeof(ULong);

               d->fxState[3].fx     = Ifx_Write;
               d->fxState[3].offset = OFFB_FC3210;
               d->fxState[3].size   = sizeof(ULong);

               stmt( IRStmt_Dirty(d) );

               assign(ew, unop(Iop_64to32,mkexpr(w64)) );
               put_emwarn( mkexpr(ew) );
               stmt( 
                  IRStmt_Exit(
                     binop(Iop_CmpNE32, mkexpr(ew), mkU32(0)),
                     Ijk_EmWarn,
                     IRConst_U64( guest_RIP_bbstart+delta ),
                     OFFB_RIP
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
                               "amd64g_check_fldcw",
                               &amd64g_check_fldcw, 
                               mkIRExprVec_1( 
                                  unop( Iop_16Uto64, 
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
                     IRConst_U64( guest_RIP_bbstart+delta ),
                     OFFB_RIP
                  )
               );
               break;
            }

            case 6: { 
               IRDirty* d = unsafeIRDirty_0_N ( 
                               0, 
                               "amd64g_dirtyhelper_FSTENV", 
                               &amd64g_dirtyhelper_FSTENV,
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
               d->fxState[2].size   = sizeof(ULong);

               d->fxState[3].fx     = Ifx_Read;
               d->fxState[3].offset = OFFB_FC3210;
               d->fxState[3].size   = sizeof(ULong);

               stmt( IRStmt_Dirty(d) );

               DIP("fnstenv %s\n", dis_buf);
               break;
            }

            case 7: 
               
               DIP("fnstcw %s\n", dis_buf);
               storeLE(
                  mkexpr(addr), 
                  unop( Iop_64to16, 
                        mkIRExprCCall(
                           Ity_I64, 0,
                           "amd64g_create_fpucw", &amd64g_create_fpucw, 
                           mkIRExprVec_1( unop(Iop_32Uto64, get_fpround()) ) 
                        ) 
                  ) 
               );
               break;

            default:
               vex_printf("unhandled opc_aux = 0x%2x\n", gregLO3ofRM(modrm));
               vex_printf("first_opcode == 0xD9\n");
               goto decode_fail;
         }

      } else {
         delta++;
         switch (modrm) {

            case 0xC0 ... 0xC7: 
               r_src = (UInt)modrm - 0xC0;
               DIP("fld %%st(%u)\n", r_src);
               t1 = newTemp(Ity_F64);
               assign(t1, get_ST(r_src));
               fp_push();
               put_ST(0, mkexpr(t1));
               break;

            case 0xC8 ... 0xCF: 
               r_src = (UInt)modrm - 0xC8;
               DIP("fxch %%st(%u)\n", r_src);
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

            case 0xE5: { 
               IRExpr** args
                  = mkIRExprVec_2( unop(Iop_8Uto64, get_ST_TAG(0)),
                                   unop(Iop_ReinterpF64asI64, 
                                        get_ST_UNCHECKED(0)) );
               put_C3210(mkIRExprCCall(
                            Ity_I64, 
                            0, 
                            "amd64g_calculate_FXAM", &amd64g_calculate_FXAM,
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
               set_C2( binop(Iop_Xor64,
                             unop(Iop_1Uto64, mkexpr(argOK)), 
                             mkU64(1)) );
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
                  unop(Iop_32Uto64,
                  triop(Iop_PRem1C3210F64,
                        get_FAKE_roundingmode(), 
                        mkexpr(a1),
                        mkexpr(a2)) ));
               break;
            }

            case 0xF7: 
               DIP("fincstp\n");
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
                  unop(Iop_32Uto64,
                  triop(Iop_PRemC3210F64,
                        get_FAKE_roundingmode(), 
                        mkexpr(a1),
                        mkexpr(a2)) ));
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
               set_C2( binop(Iop_Xor64,
                             unop(Iop_1Uto64, mkexpr(argOK)), 
                             mkU64(1)) );
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
               set_C2( binop(Iop_Xor64,
                             unop(Iop_1Uto64, mkexpr(argOK)), 
                             mkU64(1)) );
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
         IRTemp addr = disAMode( &len, vbi, pfx, delta, dis_buf, 0 );
         delta += len;
         switch (gregLO3ofRM(modrm)) {

            case 0:  
               DIP("fiaddl %s\n", dis_buf);
               fop = Iop_AddF64;
               goto do_fop_m32;

            case 1:  
               DIP("fimull %s\n", dis_buf);
               fop = Iop_MulF64;
               goto do_fop_m32;

            case 4:  
               DIP("fisubl %s\n", dis_buf);
               fop = Iop_SubF64;
               goto do_fop_m32;

            case 5:  
               DIP("fisubrl %s\n", dis_buf);
               fop = Iop_SubF64;
               goto do_foprev_m32;

            case 6:  
               DIP("fisubl %s\n", dis_buf);
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
               vex_printf("unhandled opc_aux = 0x%2x\n", gregLO3ofRM(modrm));
               vex_printf("first_opcode == 0xDA\n");
               goto decode_fail;
         }

      } else {

         delta++;
         switch (modrm) {

            case 0xC0 ... 0xC7: 
               r_src = (UInt)modrm - 0xC0;
               DIP("fcmovb %%st(%u), %%st(0)\n", r_src);
               put_ST_UNCHECKED(0, 
                                IRExpr_ITE( 
                                    mk_amd64g_calculate_condition(AMD64CondB),
                                    get_ST(r_src), get_ST(0)) );
               break;

            case 0xC8 ... 0xCF: 
               r_src = (UInt)modrm - 0xC8;
               DIP("fcmovz %%st(%u), %%st(0)\n", r_src);
               put_ST_UNCHECKED(0, 
                                IRExpr_ITE( 
                                    mk_amd64g_calculate_condition(AMD64CondZ),
                                    get_ST(r_src), get_ST(0)) );
               break;

            case 0xD0 ... 0xD7: 
               r_src = (UInt)modrm - 0xD0;
               DIP("fcmovbe %%st(%u), %%st(0)\n", r_src);
               put_ST_UNCHECKED(0, 
                                IRExpr_ITE( 
                                    mk_amd64g_calculate_condition(AMD64CondBE),
                                    get_ST(r_src), get_ST(0)) );
               break;

            case 0xD8 ... 0xDF: 
               r_src = (UInt)modrm - 0xD8;
               DIP("fcmovu %%st(%u), %%st(0)\n", r_src);
               put_ST_UNCHECKED(0, 
                                IRExpr_ITE( 
                                    mk_amd64g_calculate_condition(AMD64CondP),
                                    get_ST(r_src), get_ST(0)) );
               break;

            case 0xE9: 
               DIP("fucompp %%st(0),%%st(1)\n");
               
               put_C3210( 
                   unop(Iop_32Uto64,
                   binop( Iop_And32,
                          binop(Iop_Shl32, 
                                binop(Iop_CmpF64, get_ST(0), get_ST(1)),
                                mkU8(8)),
                          mkU32(0x4500)
                   )));
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

         IRTemp addr = disAMode( &len, vbi, pfx, delta, dis_buf, 0 );
         delta += len;

         switch (gregLO3ofRM(modrm)) {

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
                               "amd64g_dirtyhelper_loadF80le", 
                               &amd64g_dirtyhelper_loadF80le, 
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
                               "amd64g_dirtyhelper_storeF80le", 
                               &amd64g_dirtyhelper_storeF80le,
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
               vex_printf("unhandled opc_aux = 0x%2x\n", gregLO3ofRM(modrm));
               vex_printf("first_opcode == 0xDB\n");
               goto decode_fail;
         }

      } else {

         delta++;
         switch (modrm) {

            case 0xC0 ... 0xC7: 
               r_src = (UInt)modrm - 0xC0;
               DIP("fcmovnb %%st(%u), %%st(0)\n", r_src);
               put_ST_UNCHECKED(0, 
                                IRExpr_ITE( 
                                    mk_amd64g_calculate_condition(AMD64CondNB),
                                    get_ST(r_src), get_ST(0)) );
               break;

            case 0xC8 ... 0xCF: 
               r_src = (UInt)modrm - 0xC8;
               DIP("fcmovnz %%st(%u), %%st(0)\n", r_src);
               put_ST_UNCHECKED(
                  0, 
                  IRExpr_ITE( 
                     mk_amd64g_calculate_condition(AMD64CondNZ),
                     get_ST(r_src),
                     get_ST(0)
                  )
               );
               break;

            case 0xD0 ... 0xD7: 
               r_src = (UInt)modrm - 0xD0;
               DIP("fcmovnbe %%st(%u), %%st(0)\n", r_src);
               put_ST_UNCHECKED(
                  0, 
                  IRExpr_ITE( 
                     mk_amd64g_calculate_condition(AMD64CondNBE),
                     get_ST(r_src),
                     get_ST(0)
                  ) 
               );
               break;

            case 0xD8 ... 0xDF: 
               r_src = (UInt)modrm - 0xD8;
               DIP("fcmovnu %%st(%u), %%st(0)\n", r_src);
               put_ST_UNCHECKED(
                  0, 
                  IRExpr_ITE( 
                     mk_amd64g_calculate_condition(AMD64CondNP),
                     get_ST(r_src),
                     get_ST(0)
                  )
               );
               break;

            case 0xE2:
               DIP("fnclex\n");
               break;

            case 0xE3: {
               IRDirty* d  = unsafeIRDirty_0_N ( 
                                0, 
                                "amd64g_dirtyhelper_FINIT", 
                                &amd64g_dirtyhelper_FINIT,
                                mkIRExprVec_1( IRExpr_BBPTR() )
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
               d->fxState[3].size   = sizeof(ULong);

               d->fxState[4].fx     = Ifx_Write;
               d->fxState[4].offset = OFFB_FC3210;
               d->fxState[4].size   = sizeof(ULong);

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

         IRTemp addr = disAMode( &len, vbi, pfx, delta, dis_buf, 0 );
         delta += len;

         switch (gregLO3ofRM(modrm)) {

            case 0: 
               fp_do_op_mem_ST_0 ( addr, "add", dis_buf, Iop_AddF64, True );
               break;

            case 1: 
               fp_do_op_mem_ST_0 ( addr, "mul", dis_buf, Iop_MulF64, True );
               break;


            case 3: 
               DIP("fcompl %s\n", dis_buf);
               
               put_C3210( 
                   unop(Iop_32Uto64,
                   binop( Iop_And32,
                          binop(Iop_Shl32, 
                                binop(Iop_CmpF64, 
                                      get_ST(0),
                                      loadLE(Ity_F64,mkexpr(addr))),
                                mkU8(8)),
                          mkU32(0x4500)
                   )));
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
               vex_printf("unhandled opc_aux = 0x%2x\n", gregLO3ofRM(modrm));
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

         IRTemp addr = disAMode( &len, vbi, pfx, delta, dis_buf, 0 );
         delta += len;

         switch (gregLO3ofRM(modrm)) {

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
               IRTemp  w64 = newTemp(Ity_I64);
               IRDirty*  d;
               if ( have66(pfx) ) {
                  d = unsafeIRDirty_0_N ( 
                         0, 
                         "amd64g_dirtyhelper_FRSTORS",
                         &amd64g_dirtyhelper_FRSTORS,
                         mkIRExprVec_1( mkexpr(addr) )
                      );
                  d->mSize = 94;
               } else {
                  d = unsafeIRDirty_0_N ( 
                         0, 
                         "amd64g_dirtyhelper_FRSTOR",
                         &amd64g_dirtyhelper_FRSTOR,
                         mkIRExprVec_2( IRExpr_BBPTR(), mkexpr(addr) )
                      );
                  d->mSize = 108;
               }

               d->tmp    = w64;
               
               d->mFx   = Ifx_Read;
               d->mAddr = mkexpr(addr);
               

               
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
               d->fxState[3].size   = sizeof(ULong);

               d->fxState[4].fx     = Ifx_Write;
               d->fxState[4].offset = OFFB_FC3210;
               d->fxState[4].size   = sizeof(ULong);

               stmt( IRStmt_Dirty(d) );

               assign(ew, unop(Iop_64to32,mkexpr(w64)) );
               put_emwarn( mkexpr(ew) );
               stmt( 
                  IRStmt_Exit(
                     binop(Iop_CmpNE32, mkexpr(ew), mkU32(0)),
                     Ijk_EmWarn,
                     IRConst_U64( guest_RIP_bbstart+delta ),
                     OFFB_RIP
                  )
               );

               if ( have66(pfx) ) {
                  DIP("frstors %s\n", dis_buf);
               } else {
                  DIP("frstor %s\n", dis_buf);
               }
               break;
            }

            case 6: { 
               IRDirty *d;
               if ( have66(pfx) ) {
                  d = unsafeIRDirty_0_N ( 
                         0, 
                         "amd64g_dirtyhelper_FNSAVES", 
                         &amd64g_dirtyhelper_FNSAVES,
                         mkIRExprVec_1( mkexpr(addr) )
                         );
                  d->mSize = 94;
               } else {
                  d = unsafeIRDirty_0_N ( 
                         0, 
                         "amd64g_dirtyhelper_FNSAVE",
                         &amd64g_dirtyhelper_FNSAVE,
                         mkIRExprVec_2( IRExpr_BBPTR(), mkexpr(addr) )
                      );
                  d->mSize = 108;
               }

               
               d->mFx   = Ifx_Write;
               d->mAddr = mkexpr(addr);
               

               
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
               d->fxState[3].size   = sizeof(ULong);

               d->fxState[4].fx     = Ifx_Read;
               d->fxState[4].offset = OFFB_FC3210;
               d->fxState[4].size   = sizeof(ULong);

               stmt( IRStmt_Dirty(d) );

               if ( have66(pfx) ) {
                 DIP("fnsaves %s\n", dis_buf);
               } else {
                 DIP("fnsave %s\n", dis_buf);
               }
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
               vex_printf("unhandled opc_aux = 0x%2x\n", gregLO3ofRM(modrm));
               vex_printf("first_opcode == 0xDD\n");
               goto decode_fail;
         }
      } else {
         delta++;
         switch (modrm) {

            case 0xC0 ... 0xC7: 
               r_dst = (UInt)modrm - 0xC0;
               DIP("ffree %%st(%u)\n", r_dst);
               put_ST_TAG ( r_dst, mkU8(0) );
               break;

            case 0xD0 ... 0xD7: 
               r_dst = (UInt)modrm - 0xD0;
               DIP("fst %%st(0),%%st(%u)\n", r_dst);
               put_ST_UNCHECKED(r_dst, get_ST(0));
               break;

            case 0xD8 ... 0xDF: 
               r_dst = (UInt)modrm - 0xD8;
               DIP("fstp %%st(0),%%st(%u)\n", r_dst);
               put_ST_UNCHECKED(r_dst, get_ST(0));
               fp_pop();
               break;

            case 0xE0 ... 0xE7: 
               r_dst = (UInt)modrm - 0xE0;
               DIP("fucom %%st(0),%%st(%u)\n", r_dst);
               
               put_C3210(
                   unop(Iop_32Uto64, 
                   binop( Iop_And32,
                          binop(Iop_Shl32, 
                                binop(Iop_CmpF64, get_ST(0), get_ST(r_dst)),
                                mkU8(8)),
                          mkU32(0x4500)
                   )));
               break;

            case 0xE8 ... 0xEF: 
               r_dst = (UInt)modrm - 0xE8;
               DIP("fucomp %%st(0),%%st(%u)\n", r_dst);
               
               put_C3210( 
                   unop(Iop_32Uto64, 
                   binop( Iop_And32,
                          binop(Iop_Shl32, 
                                binop(Iop_CmpF64, get_ST(0), get_ST(r_dst)),
                                mkU8(8)),
                          mkU32(0x4500)
                   )));
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
         IRTemp addr = disAMode( &len, vbi, pfx, delta, dis_buf, 0 );
         delta += len;

         switch (gregLO3ofRM(modrm)) {

            case 0:  
               DIP("fiaddw %s\n", dis_buf);
               fop = Iop_AddF64;
               goto do_fop_m16;

            case 1:  
               DIP("fimulw %s\n", dis_buf);
               fop = Iop_MulF64;
               goto do_fop_m16;

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
               vex_printf("unhandled opc_aux = 0x%2x\n", gregLO3ofRM(modrm));
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
               DIP("fcompp %%st(0),%%st(1)\n");
               
               put_C3210( 
                   unop(Iop_32Uto64,
                   binop( Iop_And32,
                          binop(Iop_Shl32, 
                                binop(Iop_CmpF64, get_ST(0), get_ST(1)),
                                mkU8(8)),
                          mkU32(0x4500)
                   )));
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

         IRTemp addr = disAMode( &len, vbi, pfx, delta, dis_buf, 0 );
         delta += len;

         switch (gregLO3ofRM(modrm)) {

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
                        x87ishly_qnarrow_32_to_16( 
                        binop(Iop_F64toI32S, mkU32(Irrm_ZERO), get_ST(0)) ));
               fp_pop();
               break;

            case 2: 
               DIP("fists %s\n", dis_buf);
               storeLE( mkexpr(addr), 
                        x87ishly_qnarrow_32_to_16(
                        binop(Iop_F64toI32S, get_roundingmode(), get_ST(0)) ));
               break;

            case 3: 
               DIP("fistps %s\n", dis_buf);
               storeLE( mkexpr(addr),
                        x87ishly_qnarrow_32_to_16( 
                        binop(Iop_F64toI32S, get_roundingmode(), get_ST(0)) ));
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
               vex_printf("unhandled opc_aux = 0x%2x\n", gregLO3ofRM(modrm));
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
               putIRegRAX(
                  2,
                  unop(Iop_32to16,
                       binop(Iop_Or32,
                             binop(Iop_Shl32, 
                                   binop(Iop_And32, get_ftop(), mkU32(7)), 
                                   mkU8(11)),
                             binop(Iop_And32, 
                                   unop(Iop_64to32, get_C3210()), 
                                   mkU32(0x4700))
               )));
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
      goto decode_fail;

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
ULong dis_MMXop_regmem_to_reg ( const VexAbiInfo* vbi,
                                Prefix      pfx,
                                Long        delta,
                                UChar       opc,
                                const HChar* name,
                                Bool        show_granularity )
{
   HChar   dis_buf[50];
   UChar   modrm = getUChar(delta);
   Bool    isReg = epartIsReg(modrm);
   IRExpr* argL  = NULL;
   IRExpr* argR  = NULL;
   IRExpr* argG  = NULL;
   IRExpr* argE  = NULL;
   IRTemp  res   = newTemp(Ity_I64);

   Bool    invG  = False;
   IROp    op    = Iop_INVALID;
   void*   hAddr = NULL;
   const HChar*  hName = NULL;
   Bool    eLeft = False;

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
      case 0xF5: XXX(amd64g_calculate_mmx_pmaddwd); break;

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
      case 0xF6: XXX(amd64g_calculate_mmx_psadbw); break;

      
      case 0xD4: op = Iop_Add64; break;
      case 0xFB: op = Iop_Sub64; break;

      default: 
         vex_printf("\n0x%x\n", (Int)opc);
         vpanic("dis_MMXop_regmem_to_reg");
   }

#  undef XXX

   argG = getMMXReg(gregLO3ofRM(modrm));
   if (invG)
      argG = unop(Iop_Not64, argG);

   if (isReg) {
      delta++;
      argE = getMMXReg(eregLO3ofRM(modrm));
   } else {
      Int    len;
      IRTemp addr = disAMode( &len, vbi, pfx, delta, dis_buf, 0 );
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

   putMMXReg( gregLO3ofRM(modrm), mkexpr(res) );

   DIP("%s%s %s, %s\n", 
       name, show_granularity ? nameMMXGran(opc & 3) : "",
       ( isReg ? nameMMXReg(eregLO3ofRM(modrm)) : dis_buf ),
       nameMMXReg(gregLO3ofRM(modrm)) );

   return delta;
}



static ULong dis_MMX_shiftG_byE ( const VexAbiInfo* vbi,
                                  Prefix pfx, Long delta, 
                                  const HChar* opname, IROp op )
{
   HChar   dis_buf[50];
   Int     alen, size;
   IRTemp  addr;
   Bool    shl, shr, sar;
   UChar   rm   = getUChar(delta);
   IRTemp  g0   = newTemp(Ity_I64);
   IRTemp  g1   = newTemp(Ity_I64);
   IRTemp  amt  = newTemp(Ity_I64);
   IRTemp  amt8 = newTemp(Ity_I8);

   if (epartIsReg(rm)) {
      assign( amt, getMMXReg(eregLO3ofRM(rm)) );
      DIP("%s %s,%s\n", opname,
                        nameMMXReg(eregLO3ofRM(rm)),
                        nameMMXReg(gregLO3ofRM(rm)) );
      delta++;
   } else {
      addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
      assign( amt, loadLE(Ity_I64, mkexpr(addr)) );
      DIP("%s %s,%s\n", opname,
                        dis_buf,
                        nameMMXReg(gregLO3ofRM(rm)) );
      delta += alen;
   }
   assign( g0,   getMMXReg(gregLO3ofRM(rm)) );
   assign( amt8, unop(Iop_64to8, mkexpr(amt)) );

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
           binop(Iop_CmpLT64U,mkexpr(amt),mkU64(size)),
           binop(op, mkexpr(g0), mkexpr(amt8)),
           mkU64(0)
        )
     );
   } else 
   if (sar) {
     assign( 
        g1,
        IRExpr_ITE(
           binop(Iop_CmpLT64U,mkexpr(amt),mkU64(size)),
           binop(op, mkexpr(g0), mkexpr(amt8)),
           binop(op, mkexpr(g0), mkU8(size-1))
        )
     );
   } else {
      vassert(0);
   }

   putMMXReg( gregLO3ofRM(rm), mkexpr(g1) );
   return delta;
}



static 
ULong dis_MMX_shiftE_imm ( Long delta, const HChar* opname, IROp op )
{
   Bool    shl, shr, sar;
   UChar   rm   = getUChar(delta);
   IRTemp  e0   = newTemp(Ity_I64);
   IRTemp  e1   = newTemp(Ity_I64);
   UChar   amt, size;
   vassert(epartIsReg(rm));
   vassert(gregLO3ofRM(rm) == 2 
           || gregLO3ofRM(rm) == 4 || gregLO3ofRM(rm) == 6);
   amt = getUChar(delta+1);
   delta += 2;
   DIP("%s $%d,%s\n", opname,
                      (Int)amt,
                      nameMMXReg(eregLO3ofRM(rm)) );

   assign( e0, getMMXReg(eregLO3ofRM(rm)) );

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

   putMMXReg( eregLO3ofRM(rm), mkexpr(e1) );
   return delta;
}



static
ULong dis_MMX ( Bool* decode_ok,
                const VexAbiInfo* vbi, Prefix pfx, Int sz, Long delta )
{
   Int   len;
   UChar modrm;
   HChar dis_buf[50];
   UChar opc = getUChar(delta);
   delta++;

   
   do_MMX_preamble();

   switch (opc) {

      case 0x6E: 
         if (sz == 4) {
            
            modrm = getUChar(delta);
            if (epartIsReg(modrm)) {
               delta++;
               putMMXReg(
                  gregLO3ofRM(modrm),
                  binop( Iop_32HLto64,
                         mkU32(0),
                         getIReg32(eregOfRexRM(pfx,modrm)) ) );
               DIP("movd %s, %s\n", 
                   nameIReg32(eregOfRexRM(pfx,modrm)), 
                   nameMMXReg(gregLO3ofRM(modrm)));
            } else {
               IRTemp addr = disAMode( &len, vbi, pfx, delta, dis_buf, 0 );
               delta += len;
               putMMXReg(
                  gregLO3ofRM(modrm),
                  binop( Iop_32HLto64,
                         mkU32(0),
                         loadLE(Ity_I32, mkexpr(addr)) ) );
               DIP("movd %s, %s\n", dis_buf, nameMMXReg(gregLO3ofRM(modrm)));
            }
         } 
         else
         if (sz == 8) {
            
            modrm = getUChar(delta);
            if (epartIsReg(modrm)) {
               delta++;
               putMMXReg( gregLO3ofRM(modrm),
                          getIReg64(eregOfRexRM(pfx,modrm)) );
               DIP("movd %s, %s\n", 
                   nameIReg64(eregOfRexRM(pfx,modrm)), 
                   nameMMXReg(gregLO3ofRM(modrm)));
            } else {
               IRTemp addr = disAMode( &len, vbi, pfx, delta, dis_buf, 0 );
               delta += len;
               putMMXReg( gregLO3ofRM(modrm),
                          loadLE(Ity_I64, mkexpr(addr)) );
               DIP("movd{64} %s, %s\n", dis_buf, nameMMXReg(gregLO3ofRM(modrm)));
            }
         }
         else {
            goto mmx_decode_failure;
         }
         break;

      case 0x7E:
         if (sz == 4) {
            
            modrm = getUChar(delta);
            if (epartIsReg(modrm)) {
               delta++;
               putIReg32( eregOfRexRM(pfx,modrm),
                          unop(Iop_64to32, getMMXReg(gregLO3ofRM(modrm)) ) );
               DIP("movd %s, %s\n", 
                   nameMMXReg(gregLO3ofRM(modrm)), 
                   nameIReg32(eregOfRexRM(pfx,modrm)));
            } else {
               IRTemp addr = disAMode( &len, vbi, pfx, delta, dis_buf, 0 );
               delta += len;
               storeLE( mkexpr(addr),
                        unop(Iop_64to32, getMMXReg(gregLO3ofRM(modrm)) ) );
               DIP("movd %s, %s\n", nameMMXReg(gregLO3ofRM(modrm)), dis_buf);
            }
         }
         else
         if (sz == 8) {
            
            modrm = getUChar(delta);
            if (epartIsReg(modrm)) {
               delta++;
               putIReg64( eregOfRexRM(pfx,modrm),
                          getMMXReg(gregLO3ofRM(modrm)) );
               DIP("movd %s, %s\n", 
                   nameMMXReg(gregLO3ofRM(modrm)), 
                   nameIReg64(eregOfRexRM(pfx,modrm)));
            } else {
               IRTemp addr = disAMode( &len, vbi, pfx, delta, dis_buf, 0 );
               delta += len;
               storeLE( mkexpr(addr),
                       getMMXReg(gregLO3ofRM(modrm)) );
               DIP("movd{64} %s, %s\n", nameMMXReg(gregLO3ofRM(modrm)), dis_buf);
            }
         } else {
            goto mmx_decode_failure;
         }
         break;

      case 0x6F:
         
         if (sz != 4
             && !(sz==8 && haveNo66noF2noF3(pfx))) 
            goto mmx_decode_failure;
         modrm = getUChar(delta);
         if (epartIsReg(modrm)) {
            delta++;
            putMMXReg( gregLO3ofRM(modrm), getMMXReg(eregLO3ofRM(modrm)) );
            DIP("movq %s, %s\n", 
                nameMMXReg(eregLO3ofRM(modrm)), 
                nameMMXReg(gregLO3ofRM(modrm)));
         } else {
            IRTemp addr = disAMode( &len, vbi, pfx, delta, dis_buf, 0 );
            delta += len;
            putMMXReg( gregLO3ofRM(modrm), loadLE(Ity_I64, mkexpr(addr)) );
            DIP("movq %s, %s\n", 
                dis_buf, nameMMXReg(gregLO3ofRM(modrm)));
         }
         break;

      case 0x7F:
         
         if (sz != 4
             && !(sz==8 && haveNo66noF2noF3(pfx)))
            goto mmx_decode_failure;
         modrm = getUChar(delta);
         if (epartIsReg(modrm)) {
            delta++;
            putMMXReg( eregLO3ofRM(modrm), getMMXReg(gregLO3ofRM(modrm)) );
            DIP("movq %s, %s\n",
                nameMMXReg(gregLO3ofRM(modrm)),
                nameMMXReg(eregLO3ofRM(modrm)));
         } else {
            IRTemp addr = disAMode( &len, vbi, pfx, delta, dis_buf, 0 );
            delta += len;
            storeLE( mkexpr(addr), getMMXReg(gregLO3ofRM(modrm)) );
            DIP("mov(nt)q %s, %s\n", 
                nameMMXReg(gregLO3ofRM(modrm)), dis_buf);
         }
         break;

      case 0xFC: 
      case 0xFD: 
      case 0xFE: 
         if (sz != 4) 
            goto mmx_decode_failure;
         delta = dis_MMXop_regmem_to_reg ( vbi, pfx, delta, opc, "padd", True );
         break;

      case 0xEC: 
      case 0xED: 
         if (sz != 4
             && !(sz==8 && haveNo66noF2noF3(pfx)))
            goto mmx_decode_failure;
         delta = dis_MMXop_regmem_to_reg ( vbi, pfx, delta, opc, "padds", True );
         break;

      case 0xDC: 
      case 0xDD: 
         if (sz != 4) 
            goto mmx_decode_failure;
         delta = dis_MMXop_regmem_to_reg ( vbi, pfx, delta, opc, "paddus", True );
         break;

      case 0xF8: 
      case 0xF9: 
      case 0xFA: 
         if (sz != 4) 
            goto mmx_decode_failure;
         delta = dis_MMXop_regmem_to_reg ( vbi, pfx, delta, opc, "psub", True );
         break;

      case 0xE8: 
      case 0xE9: 
         if (sz != 4) 
            goto mmx_decode_failure;
         delta = dis_MMXop_regmem_to_reg ( vbi, pfx, delta, opc, "psubs", True );
         break;

      case 0xD8: 
      case 0xD9: 
         if (sz != 4) 
            goto mmx_decode_failure;
         delta = dis_MMXop_regmem_to_reg ( vbi, pfx, delta, opc, "psubus", True );
         break;

      case 0xE5: 
         if (sz != 4) 
            goto mmx_decode_failure;
         delta = dis_MMXop_regmem_to_reg ( vbi, pfx, delta, opc, "pmulhw", False );
         break;

      case 0xD5: 
         if (sz != 4) 
            goto mmx_decode_failure;
         delta = dis_MMXop_regmem_to_reg ( vbi, pfx, delta, opc, "pmullw", False );
         break;

      case 0xF5: 
         vassert(sz == 4);
         delta = dis_MMXop_regmem_to_reg ( vbi, pfx, delta, opc, "pmaddwd", False );
         break;

      case 0x74: 
      case 0x75: 
      case 0x76: 
         if (sz != 4) 
            goto mmx_decode_failure;
         delta = dis_MMXop_regmem_to_reg ( vbi, pfx, delta, opc, "pcmpeq", True );
         break;

      case 0x64: 
      case 0x65: 
      case 0x66: 
         if (sz != 4) 
            goto mmx_decode_failure;
         delta = dis_MMXop_regmem_to_reg ( vbi, pfx, delta, opc, "pcmpgt", True );
         break;

      case 0x6B: 
         if (sz != 4) 
            goto mmx_decode_failure;
         delta = dis_MMXop_regmem_to_reg ( vbi, pfx, delta, opc, "packssdw", False );
         break;

      case 0x63: 
         if (sz != 4) 
            goto mmx_decode_failure;
         delta = dis_MMXop_regmem_to_reg ( vbi, pfx, delta, opc, "packsswb", False );
         break;

      case 0x67: 
         if (sz != 4) 
            goto mmx_decode_failure;
         delta = dis_MMXop_regmem_to_reg ( vbi, pfx, delta, opc, "packuswb", False );
         break;

      case 0x68: 
      case 0x69: 
      case 0x6A: 
         if (sz != 4
             && !(sz==8 && haveNo66noF2noF3(pfx))) 
            goto mmx_decode_failure;
         delta = dis_MMXop_regmem_to_reg ( vbi, pfx, delta, opc, "punpckh", True );
         break;

      case 0x60: 
      case 0x61: 
      case 0x62: 
         if (sz != 4
             && !(sz==8 && haveNo66noF2noF3(pfx))) 
            goto mmx_decode_failure;
         delta = dis_MMXop_regmem_to_reg ( vbi, pfx, delta, opc, "punpckl", True );
         break;

      case 0xDB: 
         if (sz != 4) 
            goto mmx_decode_failure;
         delta = dis_MMXop_regmem_to_reg ( vbi, pfx, delta, opc, "pand", False );
         break;

      case 0xDF: 
         if (sz != 4) 
            goto mmx_decode_failure;
         delta = dis_MMXop_regmem_to_reg ( vbi, pfx, delta, opc, "pandn", False );
         break;

      case 0xEB: 
         if (sz != 4) 
            goto mmx_decode_failure;
         delta = dis_MMXop_regmem_to_reg ( vbi, pfx, delta, opc, "por", False );
         break;

      case 0xEF: 
         if (sz != 4) 
            goto mmx_decode_failure;
         delta = dis_MMXop_regmem_to_reg ( vbi, pfx, delta, opc, "pxor", False );
         break; 

#     define SHIFT_BY_REG(_name,_op)                                     \
                delta = dis_MMX_shiftG_byE(vbi, pfx, delta, _name, _op); \
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
         byte2  = getUChar(delta);      
         subopc = toUChar( (byte2 >> 3) & 7 );

#        define SHIFT_BY_IMM(_name,_op)                        \
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
         IRTemp addr    = newTemp(Ity_I64);
         IRTemp regD    = newTemp(Ity_I64);
         IRTemp regM    = newTemp(Ity_I64);
         IRTemp mask    = newTemp(Ity_I64);
         IRTemp olddata = newTemp(Ity_I64);
         IRTemp newdata = newTemp(Ity_I64);

         modrm = getUChar(delta);
         if (sz != 4 || (!epartIsReg(modrm)))
            goto mmx_decode_failure;
         delta++;

         assign( addr, handleAddrOverrides( vbi, pfx, getIReg64(R_RDI) ));
         assign( regM, getMMXReg( eregLO3ofRM(modrm) ));
         assign( regD, getMMXReg( gregLO3ofRM(modrm) ));
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
         DIP("maskmovq %s,%s\n", nameMMXReg( eregLO3ofRM(modrm) ),
                                 nameMMXReg( gregLO3ofRM(modrm) ) );
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
IRExpr* shiftL64_with_extras ( IRTemp base, IRTemp xtra, IRTemp amt )
{
   return
      IRExpr_ITE( 
         binop(Iop_CmpNE8, mkexpr(amt), mkU8(0)),
         binop(Iop_Or64, 
               binop(Iop_Shl64, mkexpr(base), mkexpr(amt)),
               binop(Iop_Shr64, mkexpr(xtra), 
                                binop(Iop_Sub8, mkU8(64), mkexpr(amt)))
               ),
         mkexpr(base)
      );
}

static 
IRExpr* shiftR64_with_extras ( IRTemp xtra, IRTemp base, IRTemp amt )
{
   return
      IRExpr_ITE( 
         binop(Iop_CmpNE8, mkexpr(amt), mkU8(0)),
         binop(Iop_Or64, 
               binop(Iop_Shr64, mkexpr(base), mkexpr(amt)),
               binop(Iop_Shl64, mkexpr(xtra), 
                                binop(Iop_Sub8, mkU8(64), mkexpr(amt)))
               ),
         mkexpr(base)
      );
}

static
ULong dis_SHLRD_Gv_Ev ( const VexAbiInfo* vbi,
                        Prefix pfx,
                        Long delta, UChar modrm,
                        Int sz,
                        IRExpr* shift_amt,
                        Bool amt_is_literal,
                        const HChar* shift_amt_txt,
                        Bool left_shift )
{
   Int len;
   HChar dis_buf[50];

   IRType ty     = szToITy(sz);
   IRTemp gsrc   = newTemp(ty);
   IRTemp esrc   = newTemp(ty);
   IRTemp addr   = IRTemp_INVALID;
   IRTemp tmpSH  = newTemp(Ity_I8);
   IRTemp tmpSS  = newTemp(Ity_I8);
   IRTemp tmp64  = IRTemp_INVALID;
   IRTemp res64  = IRTemp_INVALID;
   IRTemp rss64  = IRTemp_INVALID;
   IRTemp resTy  = IRTemp_INVALID;
   IRTemp rssTy  = IRTemp_INVALID;
   Int    mask   = sz==8 ? 63 : 31;

   vassert(sz == 2 || sz == 4 || sz == 8);


   

   assign( gsrc, getIRegG(sz, pfx, modrm) );

   if (epartIsReg(modrm)) {
      delta++;
      assign( esrc, getIRegE(sz, pfx, modrm) );
      DIP("sh%cd%c %s, %s, %s\n",
          ( left_shift ? 'l' : 'r' ), nameISize(sz), 
          shift_amt_txt,
          nameIRegG(sz, pfx, modrm), nameIRegE(sz, pfx, modrm));
   } else {
      addr = disAMode ( &len, vbi, pfx, delta, dis_buf, 
                        
                        amt_is_literal ? 1 : 0 );
      delta += len;
      assign( esrc, loadLE(ty, mkexpr(addr)) );
      DIP("sh%cd%c %s, %s, %s\n", 
          ( left_shift ? 'l' : 'r' ), nameISize(sz), 
          shift_amt_txt,
          nameIRegG(sz, pfx, modrm), dis_buf);
   }


   assign( tmpSH, binop(Iop_And8, shift_amt, mkU8(mask)) );
   assign( tmpSS, binop(Iop_And8, 
                        binop(Iop_Sub8, mkexpr(tmpSH), mkU8(1) ),
                        mkU8(mask)));

   tmp64 = newTemp(Ity_I64);
   res64 = newTemp(Ity_I64);
   rss64 = newTemp(Ity_I64);

   if (sz == 2 || sz == 4) {

      
      
      if (sz == 4 && left_shift) {
         assign( tmp64, binop(Iop_32HLto64, mkexpr(esrc), mkexpr(gsrc)) );
         assign( res64, 
                 binop(Iop_Shr64, 
                       binop(Iop_Shl64, mkexpr(tmp64), mkexpr(tmpSH)),
                       mkU8(32)) );
         assign( rss64, 
                 binop(Iop_Shr64, 
                       binop(Iop_Shl64, mkexpr(tmp64), mkexpr(tmpSS)),
                       mkU8(32)) );
      }
      else
      if (sz == 4 && !left_shift) {
         assign( tmp64, binop(Iop_32HLto64, mkexpr(gsrc), mkexpr(esrc)) );
         assign( res64, binop(Iop_Shr64, mkexpr(tmp64), mkexpr(tmpSH)) );
         assign( rss64, binop(Iop_Shr64, mkexpr(tmp64), mkexpr(tmpSS)) );
      }
      else
      if (sz == 2 && left_shift) {
         assign( tmp64,
                 binop(Iop_32HLto64,
                       binop(Iop_16HLto32, mkexpr(esrc), mkexpr(gsrc)),
                       binop(Iop_16HLto32, mkexpr(gsrc), mkexpr(gsrc))
         ));
         
         assign( res64, 
                 binop(Iop_Shr64, 
                       binop(Iop_Shl64, mkexpr(tmp64), mkexpr(tmpSH)),
                       mkU8(48)) );
         
         assign( rss64, 
                 binop(Iop_Shr64, 
                       binop(Iop_Shl64, 
                             binop(Iop_Shl64, unop(Iop_16Uto64, mkexpr(esrc)),
                                              mkU8(48)),
                             mkexpr(tmpSS)),
                       mkU8(48)) );
      }
      else
      if (sz == 2 && !left_shift) {
         assign( tmp64,
                 binop(Iop_32HLto64,
                       binop(Iop_16HLto32, mkexpr(gsrc), mkexpr(gsrc)),
                       binop(Iop_16HLto32, mkexpr(gsrc), mkexpr(esrc))
         ));
         
         assign( res64, binop(Iop_Shr64, mkexpr(tmp64), mkexpr(tmpSH)) );
         
         assign( rss64, binop(Iop_Shr64, 
                              unop(Iop_16Uto64, mkexpr(esrc)), 
                              mkexpr(tmpSS)) );
      }

   } else {

      vassert(sz == 8);
      if (left_shift) {
         assign( res64, shiftL64_with_extras( esrc, gsrc, tmpSH ));
         assign( rss64, shiftL64_with_extras( esrc, gsrc, tmpSS ));
      } else {
         assign( res64, shiftR64_with_extras( gsrc, esrc, tmpSH ));
         assign( rss64, shiftR64_with_extras( gsrc, esrc, tmpSS ));
      }

   }

   resTy = newTemp(ty);
   rssTy = newTemp(ty);
   assign( resTy, narrowTo(ty, mkexpr(res64)) );
   assign( rssTy, narrowTo(ty, mkexpr(rss64)) );

   
   setFlags_DEP1_DEP2_shift ( left_shift ? Iop_Shl64 : Iop_Sar64,
                              resTy, rssTy, ty, tmpSH );

   if (epartIsReg(modrm)) {
      putIRegE(sz, pfx, modrm, mkexpr(resTy));
   } else {
      storeLE( mkexpr(addr), mkexpr(resTy) );
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
      default: vpanic("nameBtOp(amd64)");
   }
}


static
ULong dis_bt_G_E ( const VexAbiInfo* vbi,
                   Prefix pfx, Int sz, Long delta, BtOp op,
                   Bool* decode_OK )
{
   HChar  dis_buf[50];
   UChar  modrm;
   Int    len;
   IRTemp t_fetched, t_bitno0, t_bitno1, t_bitno2, t_addr0, 
          t_addr1, t_rsp, t_mask, t_new;

   vassert(sz == 2 || sz == 4 || sz == 8);

   t_fetched = t_bitno0 = t_bitno1 = t_bitno2 
             = t_addr0 = t_addr1 = t_rsp
             = t_mask = t_new = IRTemp_INVALID;

   t_fetched = newTemp(Ity_I8);
   t_new     = newTemp(Ity_I8);
   t_bitno0  = newTemp(Ity_I64);
   t_bitno1  = newTemp(Ity_I64);
   t_bitno2  = newTemp(Ity_I8);
   t_addr1   = newTemp(Ity_I64);
   modrm     = getUChar(delta);

   *decode_OK = True;
   if (epartIsReg(modrm)) {
      
      if (haveF2orF3(pfx)) {
         *decode_OK = False;
         return delta;
      }
   } else {
      if (haveF2orF3(pfx)) {
         if (haveF2andF3(pfx) || !haveLOCK(pfx) || op == BtOpNone) {
            *decode_OK = False;
            return delta;
         }
      }
   }

   assign( t_bitno0, widenSto64(getIRegG(sz, pfx, modrm)) );
   
   if (epartIsReg(modrm)) {
      delta++;
      t_rsp = newTemp(Ity_I64);
      t_addr0 = newTemp(Ity_I64);

      vassert(vbi->guest_stack_redzone_size == 128);
      assign( t_rsp, binop(Iop_Sub64, getIReg64(R_RSP), mkU64(288)) );
      putIReg64(R_RSP, mkexpr(t_rsp));

      storeLE( mkexpr(t_rsp), getIRegE(sz, pfx, modrm) );

      
      assign( t_addr0, mkexpr(t_rsp) );

      assign( t_bitno1, binop(Iop_And64, 
                              mkexpr(t_bitno0), 
                              mkU64(sz == 8 ? 63 : sz == 4 ? 31 : 15)) );

   } else {
      t_addr0 = disAMode ( &len, vbi, pfx, delta, dis_buf, 0 );
      delta += len;
      assign( t_bitno1, mkexpr(t_bitno0) );
   }
  
  
   
   assign( t_addr1, 
           binop(Iop_Add64, 
                 mkexpr(t_addr0), 
                 binop(Iop_Sar64, mkexpr(t_bitno1), mkU8(3))) );

   

   assign( t_bitno2, 
           unop(Iop_64to8, 
                binop(Iop_And64, mkexpr(t_bitno1), mkU64(7))) );

   

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
            vpanic("dis_bt_G_E(amd64)");
      }
      if ((haveLOCK(pfx)) && !epartIsReg(modrm)) {
         casLE( mkexpr(t_addr1), mkexpr(t_fetched),
                                 mkexpr(t_new),
                                 guest_RIP_curr_instr );
      } else {
         storeLE( mkexpr(t_addr1), mkexpr(t_new) );
      }
   }
  
   
   
   stmt( IRStmt_Put( OFFB_CC_OP,   mkU64(AMD64G_CC_OP_COPY) ));
   stmt( IRStmt_Put( OFFB_CC_DEP2, mkU64(0) ));
   stmt( IRStmt_Put( 
            OFFB_CC_DEP1,
            binop(Iop_And64,
                  binop(Iop_Shr64, 
                        unop(Iop_8Uto64, mkexpr(t_fetched)),
                        mkexpr(t_bitno2)),
                  mkU64(1)))
       );
   stmt( IRStmt_Put( OFFB_CC_NDEP, mkU64(0) ));

   
   if (epartIsReg(modrm)) {
      
      if (op != BtOpNone)
         putIRegE(sz, pfx, modrm, loadLE(szToITy(sz), mkexpr(t_rsp)) );
      putIReg64(R_RSP, binop(Iop_Add64, mkexpr(t_rsp), mkU64(288)) );
   }

   DIP("bt%s%c %s, %s\n",
       nameBtOp(op), nameISize(sz), nameIRegG(sz, pfx, modrm), 
       ( epartIsReg(modrm) ? nameIRegE(sz, pfx, modrm) : dis_buf ) );
 
   return delta;
}



static
ULong dis_bs_E_G ( const VexAbiInfo* vbi,
                   Prefix pfx, Int sz, Long delta, Bool fwds )
{
   Bool   isReg;
   UChar  modrm;
   HChar  dis_buf[50];

   IRType ty    = szToITy(sz);
   IRTemp src   = newTemp(ty);
   IRTemp dst   = newTemp(ty);
   IRTemp src64 = newTemp(Ity_I64);
   IRTemp dst64 = newTemp(Ity_I64);
   IRTemp srcB  = newTemp(Ity_I1);

   vassert(sz == 8 || sz == 4 || sz == 2);

   modrm = getUChar(delta);
   isReg = epartIsReg(modrm);
   if (isReg) {
      delta++;
      assign( src, getIRegE(sz, pfx, modrm) );
   } else {
      Int    len;
      IRTemp addr = disAMode( &len, vbi, pfx, delta, dis_buf, 0 );
      delta += len;
      assign( src, loadLE(ty, mkexpr(addr)) );
   }

   DIP("bs%c%c %s, %s\n",
       fwds ? 'f' : 'r', nameISize(sz), 
       ( isReg ? nameIRegE(sz, pfx, modrm) : dis_buf ), 
       nameIRegG(sz, pfx, modrm));

   
   assign( src64, widenUto64(mkexpr(src)) );

   assign( srcB, binop(Iop_ExpCmpNE64, mkexpr(src64), mkU64(0)) );

   stmt( IRStmt_Put( OFFB_CC_OP,   mkU64(AMD64G_CC_OP_COPY) ));
   stmt( IRStmt_Put( OFFB_CC_DEP2, mkU64(0) ));
   stmt( IRStmt_Put( 
            OFFB_CC_DEP1,
            IRExpr_ITE( mkexpr(srcB),
                        
                        mkU64(0),
                        
                        mkU64(AMD64G_CC_MASK_Z)
                        )
       ));
   stmt( IRStmt_Put( OFFB_CC_NDEP, mkU64(0) ));



   
   assign( dst64,
           IRExpr_ITE( 
              mkexpr(srcB),
              
              fwds ? unop(Iop_Ctz64, mkexpr(src64))
                   : binop(Iop_Sub64, 
                           mkU64(63), 
                           unop(Iop_Clz64, mkexpr(src64))),
              
              widenUto64( getIRegG( sz, pfx, modrm ) )
           )
         );

   if (sz == 2)
      assign( dst, unop(Iop_64to16, mkexpr(dst64)) );
   else
   if (sz == 4)
      assign( dst, unop(Iop_64to32, mkexpr(dst64)) );
   else
      assign( dst, mkexpr(dst64) );

   
   putIRegG( sz, pfx, modrm, mkexpr(dst) );

   return delta;
}


static 
void codegen_xchg_rAX_Reg ( Prefix pfx, Int sz, UInt regLo3 )
{
   IRType ty = szToITy(sz);
   IRTemp t1 = newTemp(ty);
   IRTemp t2 = newTemp(ty);
   vassert(sz == 2 || sz == 4 || sz == 8);
   vassert(regLo3 < 8);
   if (sz == 8) {
      assign( t1, getIReg64(R_RAX) );
      assign( t2, getIRegRexB(8, pfx, regLo3) );
      putIReg64( R_RAX, mkexpr(t2) );
      putIRegRexB(8, pfx, regLo3, mkexpr(t1) );
   } else if (sz == 4) {
      assign( t1, getIReg32(R_RAX) );
      assign( t2, getIRegRexB(4, pfx, regLo3) );
      putIReg32( R_RAX, mkexpr(t2) );
      putIRegRexB(4, pfx, regLo3, mkexpr(t1) );
   } else {
      assign( t1, getIReg16(R_RAX) );
      assign( t2, getIRegRexB(2, pfx, regLo3) );
      putIReg16( R_RAX, mkexpr(t2) );
      putIRegRexB(2, pfx, regLo3, mkexpr(t1) );
   }
   DIP("xchg%c %s, %s\n", 
       nameISize(sz), nameIRegRAX(sz), 
                      nameIRegRexB(sz,pfx, regLo3));
}


static 
void codegen_SAHF ( void )
{
   ULong  mask_SZACP = AMD64G_CC_MASK_S|AMD64G_CC_MASK_Z|AMD64G_CC_MASK_A
                       |AMD64G_CC_MASK_C|AMD64G_CC_MASK_P;
   IRTemp oldflags   = newTemp(Ity_I64);
   assign( oldflags, mk_amd64g_calculate_rflags_all() );
   stmt( IRStmt_Put( OFFB_CC_OP,   mkU64(AMD64G_CC_OP_COPY) ));
   stmt( IRStmt_Put( OFFB_CC_NDEP, mkU64(0) ));
   stmt( IRStmt_Put( OFFB_CC_DEP2, mkU64(0) ));
   stmt( IRStmt_Put( OFFB_CC_DEP1,
         binop(Iop_Or64,
               binop(Iop_And64, mkexpr(oldflags), mkU64(AMD64G_CC_MASK_O)),
               binop(Iop_And64, 
                     binop(Iop_Shr64, getIReg64(R_RAX), mkU8(8)),
                     mkU64(mask_SZACP))
              )
   ));
}


static 
void codegen_LAHF ( void  )
{
   
   IRExpr* rax_with_hole;
   IRExpr* new_byte;
   IRExpr* new_rax;
   ULong   mask_SZACP = AMD64G_CC_MASK_S|AMD64G_CC_MASK_Z|AMD64G_CC_MASK_A
                        |AMD64G_CC_MASK_C|AMD64G_CC_MASK_P;

   IRTemp  flags = newTemp(Ity_I64);
   assign( flags, mk_amd64g_calculate_rflags_all() );

   rax_with_hole 
      = binop(Iop_And64, getIReg64(R_RAX), mkU64(~0xFF00ULL));
   new_byte 
      = binop(Iop_Or64, binop(Iop_And64, mkexpr(flags), mkU64(mask_SZACP)),
                        mkU64(1<<1));
   new_rax 
      = binop(Iop_Or64, rax_with_hole,
                        binop(Iop_Shl64, new_byte, mkU8(8)));
   putIReg64(R_RAX, new_rax);
}


static
ULong dis_cmpxchg_G_E ( Bool* ok,
                        const VexAbiInfo*  vbi,
                        Prefix       pfx,
                        Int          size, 
                        Long         delta0 )
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
      if (haveF2orF3(pfx)) {
         *ok = False;
         return delta0;
      }
   } else {
      if (haveF2orF3(pfx)) {
         if (haveF2andF3(pfx) || !haveLOCK(pfx)) {
            *ok = False;
            return delta0;
         }
      }
   }

   if (epartIsReg(rm)) {
      
      assign( dest, getIRegE(size, pfx, rm) );
      delta0++;
      assign( src, getIRegG(size, pfx, rm) );
      assign( acc, getIRegRAX(size) );
      setFlags_DEP1_DEP2(Iop_Sub8, acc, dest, ty);
      assign( cond, mk_amd64g_calculate_condition(AMD64CondZ) );
      assign( dest2, IRExpr_ITE(mkexpr(cond), mkexpr(src), mkexpr(dest)) );
      assign( acc2,  IRExpr_ITE(mkexpr(cond), mkexpr(acc), mkexpr(dest)) );
      putIRegRAX(size, mkexpr(acc2));
      putIRegE(size, pfx, rm, mkexpr(dest2));
      DIP("cmpxchg%c %s,%s\n", nameISize(size),
                               nameIRegG(size,pfx,rm),
                               nameIRegE(size,pfx,rm) );
   } 
   else if (!epartIsReg(rm) && !haveLOCK(pfx)) {
      
      addr = disAMode ( &len, vbi, pfx, delta0, dis_buf, 0 );
      assign( dest, loadLE(ty, mkexpr(addr)) );
      delta0 += len;
      assign( src, getIRegG(size, pfx, rm) );
      assign( acc, getIRegRAX(size) );
      setFlags_DEP1_DEP2(Iop_Sub8, acc, dest, ty);
      assign( cond, mk_amd64g_calculate_condition(AMD64CondZ) );
      assign( dest2, IRExpr_ITE(mkexpr(cond), mkexpr(src), mkexpr(dest)) );
      assign( acc2,  IRExpr_ITE(mkexpr(cond), mkexpr(acc), mkexpr(dest)) );
      putIRegRAX(size, mkexpr(acc2));
      storeLE( mkexpr(addr), mkexpr(dest2) );
      DIP("cmpxchg%c %s,%s\n", nameISize(size), 
                               nameIRegG(size,pfx,rm), dis_buf);
   }
   else if (!epartIsReg(rm) && haveLOCK(pfx)) {
      
      addr = disAMode ( &len, vbi, pfx, delta0, dis_buf, 0 );
      delta0 += len;
      assign( src, getIRegG(size, pfx, rm) );
      assign( acc, getIRegRAX(size) );
      stmt( IRStmt_CAS( 
         mkIRCAS( IRTemp_INVALID, dest, Iend_LE, mkexpr(addr), 
                  NULL, mkexpr(acc), NULL, mkexpr(src) )
      ));
      setFlags_DEP1_DEP2(Iop_Sub8, acc, dest, ty);
      assign( cond, mk_amd64g_calculate_condition(AMD64CondZ) );
      assign( acc2,  IRExpr_ITE(mkexpr(cond), mkexpr(acc), mkexpr(dest)) );
      putIRegRAX(size, mkexpr(acc2));
      DIP("cmpxchg%c %s,%s\n", nameISize(size), 
                               nameIRegG(size,pfx,rm), dis_buf);
   }
   else vassert(0);

   *ok = True;
   return delta0;
}


static
ULong dis_cmov_E_G ( const VexAbiInfo* vbi,
                     Prefix        pfx,
                     Int           sz, 
                     AMD64Condcode cond,
                     Long          delta0 )
{
   UChar rm  = getUChar(delta0);
   HChar dis_buf[50];
   Int   len;

   IRType ty   = szToITy(sz);
   IRTemp tmps = newTemp(ty);
   IRTemp tmpd = newTemp(ty);

   if (epartIsReg(rm)) {
      assign( tmps, getIRegE(sz, pfx, rm) );
      assign( tmpd, getIRegG(sz, pfx, rm) );

      putIRegG( sz, pfx, rm,
                IRExpr_ITE( mk_amd64g_calculate_condition(cond),
                            mkexpr(tmps),
                            mkexpr(tmpd) )
              );
      DIP("cmov%s %s,%s\n", name_AMD64Condcode(cond),
                            nameIRegE(sz,pfx,rm),
                            nameIRegG(sz,pfx,rm));
      return 1+delta0;
   }

       
   {
      IRTemp addr = disAMode ( &len, vbi, pfx, delta0, dis_buf, 0 );
      assign( tmps, loadLE(ty, mkexpr(addr)) );
      assign( tmpd, getIRegG(sz, pfx, rm) );

      putIRegG( sz, pfx, rm,
                IRExpr_ITE( mk_amd64g_calculate_condition(cond),
                            mkexpr(tmps),
                            mkexpr(tmpd) )
              );

      DIP("cmov%s %s,%s\n", name_AMD64Condcode(cond),
                            dis_buf,
                            nameIRegG(sz,pfx,rm));
      return len+delta0;
   }
}


static
ULong dis_xadd_G_E ( Bool* decode_ok,
                     const VexAbiInfo* vbi,
                     Prefix pfx, Int sz, Long delta0 )
{
   Int   len;
   UChar rm = getUChar(delta0);
   HChar dis_buf[50];

   IRType ty    = szToITy(sz);
   IRTemp tmpd  = newTemp(ty);
   IRTemp tmpt0 = newTemp(ty);
   IRTemp tmpt1 = newTemp(ty);


   if (epartIsReg(rm)) {
      
      assign( tmpd, getIRegE(sz, pfx, rm) );
      assign( tmpt0, getIRegG(sz, pfx, rm) );
      assign( tmpt1, binop(mkSizedOp(ty,Iop_Add8),
                           mkexpr(tmpd), mkexpr(tmpt0)) );
      setFlags_DEP1_DEP2( Iop_Add8, tmpd, tmpt0, ty );
      putIRegG(sz, pfx, rm, mkexpr(tmpd));
      putIRegE(sz, pfx, rm, mkexpr(tmpt1));
      DIP("xadd%c %s, %s\n",
          nameISize(sz), nameIRegG(sz,pfx,rm), nameIRegE(sz,pfx,rm));
      *decode_ok = True;
      return 1+delta0;
   }
   else if (!epartIsReg(rm) && !haveLOCK(pfx)) {
      
      IRTemp addr = disAMode ( &len, vbi, pfx, delta0, dis_buf, 0 );
      assign( tmpd,  loadLE(ty, mkexpr(addr)) );
      assign( tmpt0, getIRegG(sz, pfx, rm) );
      assign( tmpt1, binop(mkSizedOp(ty,Iop_Add8),
                           mkexpr(tmpd), mkexpr(tmpt0)) );
      setFlags_DEP1_DEP2( Iop_Add8, tmpd, tmpt0, ty );
      storeLE( mkexpr(addr), mkexpr(tmpt1) );
      putIRegG(sz, pfx, rm, mkexpr(tmpd));
      DIP("xadd%c %s, %s\n",
          nameISize(sz), nameIRegG(sz,pfx,rm), dis_buf);
      *decode_ok = True;
      return len+delta0;
   }
   else if (!epartIsReg(rm) && haveLOCK(pfx)) {
      
      IRTemp addr = disAMode ( &len, vbi, pfx, delta0, dis_buf, 0 );
      assign( tmpd,  loadLE(ty, mkexpr(addr)) );
      assign( tmpt0, getIRegG(sz, pfx, rm) );
      assign( tmpt1, binop(mkSizedOp(ty,Iop_Add8), 
                           mkexpr(tmpd), mkexpr(tmpt0)) );
      casLE( mkexpr(addr), mkexpr(tmpd),
                           mkexpr(tmpt1), guest_RIP_curr_instr );
      setFlags_DEP1_DEP2( Iop_Add8, tmpd, tmpt0, ty );
      putIRegG(sz, pfx, rm, mkexpr(tmpd));
      DIP("xadd%c %s, %s\n",
          nameISize(sz), nameIRegG(sz,pfx,rm), dis_buf);
      *decode_ok = True;
      return len+delta0;
   }
   
   vassert(0);
}


static
void dis_ret ( DisResult* dres, const VexAbiInfo* vbi, ULong d64 )
{
   IRTemp t1 = newTemp(Ity_I64); 
   IRTemp t2 = newTemp(Ity_I64);
   IRTemp t3 = newTemp(Ity_I64);
   assign(t1, getIReg64(R_RSP));
   assign(t2, loadLE(Ity_I64,mkexpr(t1)));
   assign(t3, binop(Iop_Add64, mkexpr(t1), mkU64(8+d64)));
   putIReg64(R_RSP, mkexpr(t3));
   make_redzone_AbiHint(vbi, t3, t2, "ret");
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
      
      case Iop_Add32Fx8: case Iop_Sub32Fx8:
      case Iop_Mul32Fx8: case Iop_Div32Fx8:
      case Iop_Add64Fx4: case Iop_Sub64Fx4:
      case Iop_Mul64Fx4: case Iop_Div64Fx4:
         return True;
      default:
         break;
   }
   return False;
}



static ULong dis_SSE_E_to_G_all_wrk ( 
                const VexAbiInfo* vbi,
                Prefix pfx, Long delta, 
                const HChar* opname, IROp op,
                Bool   invertG
             )
{
   HChar   dis_buf[50];
   Int     alen;
   IRTemp  addr;
   UChar   rm = getUChar(delta);
   Bool    needsRMode = requiresRMode(op);
   IRExpr* gpart
      = invertG ? unop(Iop_NotV128, getXMMReg(gregOfRexRM(pfx,rm)))
                : getXMMReg(gregOfRexRM(pfx,rm));
   if (epartIsReg(rm)) {
      putXMMReg(
         gregOfRexRM(pfx,rm),
         needsRMode
            ? triop(op, get_FAKE_roundingmode(), 
                        gpart,
                        getXMMReg(eregOfRexRM(pfx,rm)))
            : binop(op, gpart,
                        getXMMReg(eregOfRexRM(pfx,rm)))
      );
      DIP("%s %s,%s\n", opname,
                        nameXMMReg(eregOfRexRM(pfx,rm)),
                        nameXMMReg(gregOfRexRM(pfx,rm)) );
      return delta+1;
   } else {
      addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
      putXMMReg(
         gregOfRexRM(pfx,rm), 
         needsRMode
            ? triop(op, get_FAKE_roundingmode(), 
                        gpart,
                        loadLE(Ity_V128, mkexpr(addr)))
            : binop(op, gpart,
                        loadLE(Ity_V128, mkexpr(addr)))
      );
      DIP("%s %s,%s\n", opname,
                        dis_buf,
                        nameXMMReg(gregOfRexRM(pfx,rm)) );
      return delta+alen;
   }
}



static
ULong dis_SSE_E_to_G_all ( const VexAbiInfo* vbi,
                           Prefix pfx, Long delta, 
                           const HChar* opname, IROp op )
{
   return dis_SSE_E_to_G_all_wrk( vbi, pfx, delta, opname, op, False );
}


static
ULong dis_SSE_E_to_G_all_invG ( const VexAbiInfo* vbi,
                                Prefix pfx, Long delta, 
                                const HChar* opname, IROp op )
{
   return dis_SSE_E_to_G_all_wrk( vbi, pfx, delta, opname, op, True );
}



static ULong dis_SSE_E_to_G_lo32 ( const VexAbiInfo* vbi,
                                   Prefix pfx, Long delta, 
                                   const HChar* opname, IROp op )
{
   HChar   dis_buf[50];
   Int     alen;
   IRTemp  addr;
   UChar   rm = getUChar(delta);
   IRExpr* gpart = getXMMReg(gregOfRexRM(pfx,rm));
   if (epartIsReg(rm)) {
      putXMMReg( gregOfRexRM(pfx,rm), 
                 binop(op, gpart,
                           getXMMReg(eregOfRexRM(pfx,rm))) );
      DIP("%s %s,%s\n", opname,
                        nameXMMReg(eregOfRexRM(pfx,rm)),
                        nameXMMReg(gregOfRexRM(pfx,rm)) );
      return delta+1;
   } else {
      IRTemp epart = newTemp(Ity_V128);
      addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
      assign( epart, unop( Iop_32UtoV128,
                           loadLE(Ity_I32, mkexpr(addr))) );
      putXMMReg( gregOfRexRM(pfx,rm), 
                 binop(op, gpart, mkexpr(epart)) );
      DIP("%s %s,%s\n", opname,
                        dis_buf,
                        nameXMMReg(gregOfRexRM(pfx,rm)) );
      return delta+alen;
   }
}



static ULong dis_SSE_E_to_G_lo64 ( const VexAbiInfo* vbi,
                                   Prefix pfx, Long delta, 
                                   const HChar* opname, IROp op )
{
   HChar   dis_buf[50];
   Int     alen;
   IRTemp  addr;
   UChar   rm = getUChar(delta);
   IRExpr* gpart = getXMMReg(gregOfRexRM(pfx,rm));
   if (epartIsReg(rm)) {
      putXMMReg( gregOfRexRM(pfx,rm), 
                 binop(op, gpart,
                           getXMMReg(eregOfRexRM(pfx,rm))) );
      DIP("%s %s,%s\n", opname,
                        nameXMMReg(eregOfRexRM(pfx,rm)),
                        nameXMMReg(gregOfRexRM(pfx,rm)) );
      return delta+1;
   } else {
      IRTemp epart = newTemp(Ity_V128);
      addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
      assign( epart, unop( Iop_64UtoV128,
                           loadLE(Ity_I64, mkexpr(addr))) );
      putXMMReg( gregOfRexRM(pfx,rm), 
                 binop(op, gpart, mkexpr(epart)) );
      DIP("%s %s,%s\n", opname,
                        dis_buf,
                        nameXMMReg(gregOfRexRM(pfx,rm)) );
      return delta+alen;
   }
}



static ULong dis_SSE_E_to_G_unary_all ( 
                const VexAbiInfo* vbi,
                Prefix pfx, Long delta, 
                const HChar* opname, IROp op
             )
{
   HChar   dis_buf[50];
   Int     alen;
   IRTemp  addr;
   UChar   rm = getUChar(delta);
   
   
   Bool needsIRRM = op == Iop_Sqrt32Fx4 || op == Iop_Sqrt64Fx2;
   if (epartIsReg(rm)) {
      IRExpr* src = getXMMReg(eregOfRexRM(pfx,rm));
      
      IRExpr* res = needsIRRM ? binop(op, get_FAKE_roundingmode(), src)
                              : unop(op, src);
      putXMMReg( gregOfRexRM(pfx,rm), res );
      DIP("%s %s,%s\n", opname,
                        nameXMMReg(eregOfRexRM(pfx,rm)),
                        nameXMMReg(gregOfRexRM(pfx,rm)) );
      return delta+1;
   } else {
      addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
      IRExpr* src = loadLE(Ity_V128, mkexpr(addr));
      
      IRExpr* res = needsIRRM ? binop(op, get_FAKE_roundingmode(), src)
                              : unop(op, src);
      putXMMReg( gregOfRexRM(pfx,rm), res );
      DIP("%s %s,%s\n", opname,
                        dis_buf,
                        nameXMMReg(gregOfRexRM(pfx,rm)) );
      return delta+alen;
   }
}



static ULong dis_SSE_E_to_G_unary_lo32 ( 
                const VexAbiInfo* vbi,
                Prefix pfx, Long delta, 
                const HChar* opname, IROp op
             )
{
   HChar   dis_buf[50];
   Int     alen;
   IRTemp  addr;
   UChar   rm = getUChar(delta);
   IRTemp  oldG0 = newTemp(Ity_V128);
   IRTemp  oldG1 = newTemp(Ity_V128);

   assign( oldG0, getXMMReg(gregOfRexRM(pfx,rm)) );

   if (epartIsReg(rm)) {
      assign( oldG1, 
              binop( Iop_SetV128lo32,
                     mkexpr(oldG0),
                     getXMMRegLane32(eregOfRexRM(pfx,rm), 0)) );
      putXMMReg( gregOfRexRM(pfx,rm), unop(op, mkexpr(oldG1)) );
      DIP("%s %s,%s\n", opname,
                        nameXMMReg(eregOfRexRM(pfx,rm)),
                        nameXMMReg(gregOfRexRM(pfx,rm)) );
      return delta+1;
   } else {
      addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
      assign( oldG1, 
              binop( Iop_SetV128lo32,
                     mkexpr(oldG0),
                     loadLE(Ity_I32, mkexpr(addr)) ));
      putXMMReg( gregOfRexRM(pfx,rm), unop(op, mkexpr(oldG1)) );
      DIP("%s %s,%s\n", opname,
                        dis_buf,
                        nameXMMReg(gregOfRexRM(pfx,rm)) );
      return delta+alen;
   }
}



static ULong dis_SSE_E_to_G_unary_lo64 ( 
                const VexAbiInfo* vbi,
                Prefix pfx, Long delta, 
                const HChar* opname, IROp op
             )
{
   HChar   dis_buf[50];
   Int     alen;
   IRTemp  addr;
   UChar   rm = getUChar(delta);
   IRTemp  oldG0 = newTemp(Ity_V128);
   IRTemp  oldG1 = newTemp(Ity_V128);

   assign( oldG0, getXMMReg(gregOfRexRM(pfx,rm)) );

   if (epartIsReg(rm)) {
      assign( oldG1, 
              binop( Iop_SetV128lo64,
                     mkexpr(oldG0),
                     getXMMRegLane64(eregOfRexRM(pfx,rm), 0)) );
      putXMMReg( gregOfRexRM(pfx,rm), unop(op, mkexpr(oldG1)) );
      DIP("%s %s,%s\n", opname,
                        nameXMMReg(eregOfRexRM(pfx,rm)),
                        nameXMMReg(gregOfRexRM(pfx,rm)) );
      return delta+1;
   } else {
      addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
      assign( oldG1, 
              binop( Iop_SetV128lo64,
                     mkexpr(oldG0),
                     loadLE(Ity_I64, mkexpr(addr)) ));
      putXMMReg( gregOfRexRM(pfx,rm), unop(op, mkexpr(oldG1)) );
      DIP("%s %s,%s\n", opname,
                        dis_buf,
                        nameXMMReg(gregOfRexRM(pfx,rm)) );
      return delta+alen;
   }
}


static ULong dis_SSEint_E_to_G( 
                const VexAbiInfo* vbi,
                Prefix pfx, Long delta, 
                const HChar* opname, IROp op,
                Bool   eLeft
             )
{
   HChar   dis_buf[50];
   Int     alen;
   IRTemp  addr;
   UChar   rm = getUChar(delta);
   IRExpr* gpart = getXMMReg(gregOfRexRM(pfx,rm));
   IRExpr* epart = NULL;
   if (epartIsReg(rm)) {
      epart = getXMMReg(eregOfRexRM(pfx,rm));
      DIP("%s %s,%s\n", opname,
                        nameXMMReg(eregOfRexRM(pfx,rm)),
                        nameXMMReg(gregOfRexRM(pfx,rm)) );
      delta += 1;
   } else {
      addr  = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
      epart = loadLE(Ity_V128, mkexpr(addr));
      DIP("%s %s,%s\n", opname,
                        dis_buf,
                        nameXMMReg(gregOfRexRM(pfx,rm)) );
      delta += alen;
   }
   putXMMReg( gregOfRexRM(pfx,rm), 
              eLeft ? binop(op, epart, gpart)
                    : binop(op, gpart, epart) );
   return delta;
}


static Bool findSSECmpOp ( Bool* preSwapP,
                           IROp* opP,
                           Bool* postNotP,
                           UInt imm8, Bool all_lanes, Int sz )
{
   if (imm8 >= 32) return False;

   Bool pre = False;
   IROp op  = Iop_INVALID;
   Bool not = False;

#  define XXX(_pre, _op, _not) { pre = _pre; op = _op; not = _not; }
   
   
   switch (imm8) {
      
      
      
      
      
      
      
      
      case 0x0:  XXX(False, Iop_CmpEQ32Fx4, False); break; 
      case 0x1:  XXX(False, Iop_CmpLT32Fx4, False); break; 
      case 0x2:  XXX(False, Iop_CmpLE32Fx4, False); break; 
      case 0x3:  XXX(False, Iop_CmpUN32Fx4, False); break; 
      case 0x4:  XXX(False, Iop_CmpEQ32Fx4, True);  break; 
      case 0x5:  XXX(False, Iop_CmpLT32Fx4, True);  break; 
      case 0x6:  XXX(False, Iop_CmpLE32Fx4, True);  break; 
      case 0x7:  XXX(False, Iop_CmpUN32Fx4, True);  break; 
      case 0x8:  XXX(False, Iop_CmpEQ32Fx4, False); break; 
      case 0x9:  XXX(True,  Iop_CmpLE32Fx4, True);  break; 
      
      case 0xA:  XXX(True,  Iop_CmpLT32Fx4, True);  break; 
      
      
      
      case 0xC:  XXX(False, Iop_CmpEQ32Fx4, True);  break; 
      case 0xD:  XXX(True,  Iop_CmpLE32Fx4, False); break; 
      case 0xE:  XXX(True,  Iop_CmpLT32Fx4, False); break; 
      
      
      case 0x11: XXX(False, Iop_CmpLT32Fx4, False); break; 
      case 0x12: XXX(False, Iop_CmpLE32Fx4, False); break; 
      
      
      
      case 0x16: XXX(False, Iop_CmpLE32Fx4, True);  break; 
      
      
      
      
      
      
      
      case 0x1E: XXX(True,  Iop_CmpLT32Fx4, False); break; 
      
      default: break;
   }
#  undef XXX
   if (op == Iop_INVALID) return False;


    if (sz == 4 && all_lanes) {
      switch (op) {
         case Iop_CmpEQ32Fx4: op = Iop_CmpEQ32Fx4; break;
         case Iop_CmpLT32Fx4: op = Iop_CmpLT32Fx4; break;
         case Iop_CmpLE32Fx4: op = Iop_CmpLE32Fx4; break;
         case Iop_CmpUN32Fx4: op = Iop_CmpUN32Fx4; break;
         default: vassert(0);
      }
   }
   else if (sz == 4 && !all_lanes) {
      switch (op) {
         case Iop_CmpEQ32Fx4: op = Iop_CmpEQ32F0x4; break;
         case Iop_CmpLT32Fx4: op = Iop_CmpLT32F0x4; break;
         case Iop_CmpLE32Fx4: op = Iop_CmpLE32F0x4; break;
         case Iop_CmpUN32Fx4: op = Iop_CmpUN32F0x4; break;
         default: vassert(0);
      }
   }
   else if (sz == 8 && all_lanes) {
      switch (op) {
         case Iop_CmpEQ32Fx4: op = Iop_CmpEQ64Fx2; break;
         case Iop_CmpLT32Fx4: op = Iop_CmpLT64Fx2; break;
         case Iop_CmpLE32Fx4: op = Iop_CmpLE64Fx2; break;
         case Iop_CmpUN32Fx4: op = Iop_CmpUN64Fx2; break;
         default: vassert(0);
      }
   }
   else if (sz == 8 && !all_lanes) {
      switch (op) {
         case Iop_CmpEQ32Fx4: op = Iop_CmpEQ64F0x2; break;
         case Iop_CmpLT32Fx4: op = Iop_CmpLT64F0x2; break;
         case Iop_CmpLE32Fx4: op = Iop_CmpLE64F0x2; break;
         case Iop_CmpUN32Fx4: op = Iop_CmpUN64F0x2; break;
         default: vassert(0);
      }
   }
   else {
      vpanic("findSSECmpOp(amd64,guest)");
   }

   *preSwapP = pre; *opP = op; *postNotP = not;
   return True;
}



static Long dis_SSE_cmp_E_to_G ( const VexAbiInfo* vbi,
                                 Prefix pfx, Long delta, 
                                 const HChar* opname, Bool all_lanes, Int sz )
{
   Long    delta0 = delta;
   HChar   dis_buf[50];
   Int     alen;
   UInt    imm8;
   IRTemp  addr;
   Bool    preSwap = False;
   IROp    op      = Iop_INVALID;
   Bool    postNot = False;
   IRTemp  plain   = newTemp(Ity_V128);
   UChar   rm      = getUChar(delta);
   UShort  mask    = 0;
   vassert(sz == 4 || sz == 8);
   if (epartIsReg(rm)) {
      imm8 = getUChar(delta+1);
      if (imm8 >= 8) return delta0; 
      Bool ok = findSSECmpOp(&preSwap, &op, &postNot, imm8, all_lanes, sz);
      if (!ok) return delta0; 
      vassert(!preSwap); 
      assign( plain, binop(op, getXMMReg(gregOfRexRM(pfx,rm)), 
                               getXMMReg(eregOfRexRM(pfx,rm))) );
      delta += 2;
      DIP("%s $%d,%s,%s\n", opname,
                            (Int)imm8,
                            nameXMMReg(eregOfRexRM(pfx,rm)),
                            nameXMMReg(gregOfRexRM(pfx,rm)) );
   } else {
      addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 1 );
      imm8 = getUChar(delta+alen);
      if (imm8 >= 8) return delta0; 
      Bool ok = findSSECmpOp(&preSwap, &op, &postNot, imm8, all_lanes, sz);
      if (!ok) return delta0; 
      vassert(!preSwap); 
      assign( plain, 
              binop(
                 op,
                 getXMMReg(gregOfRexRM(pfx,rm)), 
                   all_lanes 
                      ? loadLE(Ity_V128, mkexpr(addr))
                   : sz == 8
                      ? unop( Iop_64UtoV128, loadLE(Ity_I64, mkexpr(addr)))
                   : 
                      unop( Iop_32UtoV128, loadLE(Ity_I32, mkexpr(addr)))
              ) 
      );
      delta += alen+1;
      DIP("%s $%d,%s,%s\n", opname,
                            (Int)imm8,
                            dis_buf,
                            nameXMMReg(gregOfRexRM(pfx,rm)) );
   }

   if (postNot && all_lanes) {
      putXMMReg( gregOfRexRM(pfx,rm), 
                 unop(Iop_NotV128, mkexpr(plain)) );
   }
   else
   if (postNot && !all_lanes) {
      mask = toUShort(sz==4 ? 0x000F : 0x00FF);
      putXMMReg( gregOfRexRM(pfx,rm), 
                 binop(Iop_XorV128, mkexpr(plain), mkV128(mask)) );
   }
   else {
      putXMMReg( gregOfRexRM(pfx,rm), mkexpr(plain) );
   }

   return delta;
}



static ULong dis_SSE_shiftG_byE ( const VexAbiInfo* vbi,
                                  Prefix pfx, Long delta, 
                                  const HChar* opname, IROp op )
{
   HChar   dis_buf[50];
   Int     alen, size;
   IRTemp  addr;
   Bool    shl, shr, sar;
   UChar   rm   = getUChar(delta);
   IRTemp  g0   = newTemp(Ity_V128);
   IRTemp  g1   = newTemp(Ity_V128);
   IRTemp  amt  = newTemp(Ity_I64);
   IRTemp  amt8 = newTemp(Ity_I8);
   if (epartIsReg(rm)) {
      assign( amt, getXMMRegLane64(eregOfRexRM(pfx,rm), 0) );
      DIP("%s %s,%s\n", opname,
                        nameXMMReg(eregOfRexRM(pfx,rm)),
                        nameXMMReg(gregOfRexRM(pfx,rm)) );
      delta++;
   } else {
      addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
      assign( amt, loadLE(Ity_I64, mkexpr(addr)) );
      DIP("%s %s,%s\n", opname,
                        dis_buf,
                        nameXMMReg(gregOfRexRM(pfx,rm)) );
      delta += alen;
   }
   assign( g0,   getXMMReg(gregOfRexRM(pfx,rm)) );
   assign( amt8, unop(Iop_64to8, mkexpr(amt)) );

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
           binop(Iop_CmpLT64U, mkexpr(amt), mkU64(size)),
           binop(op, mkexpr(g0), mkexpr(amt8)),
           mkV128(0x0000)
        )
     );
   } else 
   if (sar) {
     assign( 
        g1,
        IRExpr_ITE(
           binop(Iop_CmpLT64U, mkexpr(amt), mkU64(size)),
           binop(op, mkexpr(g0), mkexpr(amt8)),
           binop(op, mkexpr(g0), mkU8(size-1))
        )
     );
   } else {
      vassert(0);
   }

   putXMMReg( gregOfRexRM(pfx,rm), mkexpr(g1) );
   return delta;
}



static 
ULong dis_SSE_shiftE_imm ( Prefix pfx, 
                           Long delta, const HChar* opname, IROp op )
{
   Bool    shl, shr, sar;
   UChar   rm   = getUChar(delta);
   IRTemp  e0   = newTemp(Ity_V128);
   IRTemp  e1   = newTemp(Ity_V128);
   UChar   amt, size;
   vassert(epartIsReg(rm));
   vassert(gregLO3ofRM(rm) == 2 
           || gregLO3ofRM(rm) == 4 || gregLO3ofRM(rm) == 6);
   amt = getUChar(delta+1);
   delta += 2;
   DIP("%s $%d,%s\n", opname,
                      (Int)amt,
                      nameXMMReg(eregOfRexRM(pfx,rm)) );
   assign( e0, getXMMReg(eregOfRexRM(pfx,rm)) );

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

   putXMMReg( eregOfRexRM(pfx,rm), mkexpr(e1) );
   return delta;
}



static IRExpr*  get_sse_roundingmode ( void )
{
   return 
      unop( Iop_64to32, 
            binop( Iop_And64, 
                   IRExpr_Get( OFFB_SSEROUND, Ity_I64 ), 
                   mkU64(3) ));
}

static void put_sse_roundingmode ( IRExpr* sseround )
{
   vassert(typeOfIRExpr(irsb->tyenv, sseround) == Ity_I32);
   stmt( IRStmt_Put( OFFB_SSEROUND, 
                     unop(Iop_32Uto64,sseround) ) );
}


static void breakupV128to32s ( IRTemp t128,
                               
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


static IRExpr* mkV128from32s ( IRTemp t3, IRTemp t2,
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


static void breakupV256to64s ( IRTemp t256,
                               
                               IRTemp* t3, IRTemp* t2,
                               IRTemp* t1, IRTemp* t0 )
{ 
   vassert(t0 && *t0 == IRTemp_INVALID);
   vassert(t1 && *t1 == IRTemp_INVALID);
   vassert(t2 && *t2 == IRTemp_INVALID);
   vassert(t3 && *t3 == IRTemp_INVALID);
   *t0 = newTemp(Ity_I64);
   *t1 = newTemp(Ity_I64);
   *t2 = newTemp(Ity_I64);
   *t3 = newTemp(Ity_I64);
   assign( *t0, unop(Iop_V256to64_0, mkexpr(t256)) );
   assign( *t1, unop(Iop_V256to64_1, mkexpr(t256)) );
   assign( *t2, unop(Iop_V256to64_2, mkexpr(t256)) );
   assign( *t3, unop(Iop_V256to64_3, mkexpr(t256)) );
}


static void breakupV256toV128s ( IRTemp t256,
                                 
                                 IRTemp* t1, IRTemp* t0 )
{ 
   vassert(t0 && *t0 == IRTemp_INVALID);
   vassert(t1 && *t1 == IRTemp_INVALID);
   *t0 = newTemp(Ity_V128);
   *t1 = newTemp(Ity_V128);
   assign(*t1, unop(Iop_V256toV128_1, mkexpr(t256)));
   assign(*t0, unop(Iop_V256toV128_0, mkexpr(t256)));
}


static void breakupV256to32s ( IRTemp t256,
                               
                               IRTemp* t7, IRTemp* t6,
                               IRTemp* t5, IRTemp* t4,
                               IRTemp* t3, IRTemp* t2,
                               IRTemp* t1, IRTemp* t0 )
{
   IRTemp t128_1 = IRTemp_INVALID;
   IRTemp t128_0 = IRTemp_INVALID;
   breakupV256toV128s( t256, &t128_1, &t128_0 );
   breakupV128to32s( t128_1, t7, t6, t5, t4 );
   breakupV128to32s( t128_0, t3, t2, t1, t0 );
}


static void breakupV128to64s ( IRTemp t128,
                               
                               IRTemp* t1, IRTemp* t0 )
{
   vassert(t0 && *t0 == IRTemp_INVALID);
   vassert(t1 && *t1 == IRTemp_INVALID);
   *t0 = newTemp(Ity_I64);
   *t1 = newTemp(Ity_I64);
   assign( *t0, unop(Iop_V128to64,   mkexpr(t128)) );
   assign( *t1, unop(Iop_V128HIto64, mkexpr(t128)) );
}


static IRExpr* mkV256from32s ( IRTemp t7, IRTemp t6,
                               IRTemp t5, IRTemp t4,
                               IRTemp t3, IRTemp t2,
                               IRTemp t1, IRTemp t0 )
{
   return
      binop( Iop_V128HLtoV256,
             binop( Iop_64HLtoV128,
                    binop(Iop_32HLto64, mkexpr(t7), mkexpr(t6)),
                    binop(Iop_32HLto64, mkexpr(t5), mkexpr(t4)) ),
             binop( Iop_64HLtoV128,
                    binop(Iop_32HLto64, mkexpr(t3), mkexpr(t2)),
                    binop(Iop_32HLto64, mkexpr(t1), mkexpr(t0)) )
   );
}


static IRExpr* mkV256from64s ( IRTemp t3, IRTemp t2,
                               IRTemp t1, IRTemp t0 )
{
   return
      binop( Iop_V128HLtoV256,
             binop(Iop_64HLtoV128, mkexpr(t3), mkexpr(t2)),
             binop(Iop_64HLtoV128, mkexpr(t1), mkexpr(t0))
   );
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


static IRTemp math_PABS_MMX ( IRTemp aa, Int laneszB )
{
   IRTemp res     = newTemp(Ity_I64);
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

   assign( negMask, binop(opSarN, mkexpr(aa), mkU8(8*laneszB-1)) );
   assign( posMask, unop(Iop_Not64, mkexpr(negMask)) );
   assign( zero,    mkU64(0) );
   assign( aaNeg,   binop(opSub, mkexpr(zero), mkexpr(aa)) );
   assign( res,
           binop(Iop_Or64,
                 binop(Iop_And64, mkexpr(aa),    mkexpr(posMask)),
                 binop(Iop_And64, mkexpr(aaNeg), mkexpr(negMask)) ));
   return res;
}

static IRTemp math_PABS_XMM ( IRTemp aa, Int laneszB )
{
   IRTemp res  = newTemp(Ity_V128);
   IRTemp aaHi = newTemp(Ity_I64);
   IRTemp aaLo = newTemp(Ity_I64);
   assign(aaHi, unop(Iop_V128HIto64, mkexpr(aa)));
   assign(aaLo, unop(Iop_V128to64, mkexpr(aa)));
   assign(res, binop(Iop_64HLtoV128,
                     mkexpr(math_PABS_MMX(aaHi, laneszB)),
                     mkexpr(math_PABS_MMX(aaLo, laneszB))));
   return res;
}

static IRTemp math_PABS_XMM_pap4 ( IRTemp aa ) {
   return math_PABS_XMM(aa, 4);
}

static IRTemp math_PABS_XMM_pap2 ( IRTemp aa ) {
   return math_PABS_XMM(aa, 2);
}

static IRTemp math_PABS_XMM_pap1 ( IRTemp aa ) {
   return math_PABS_XMM(aa, 1);
}

static IRTemp math_PABS_YMM ( IRTemp aa, Int laneszB )
{
   IRTemp res  = newTemp(Ity_V256);
   IRTemp aaHi = IRTemp_INVALID;
   IRTemp aaLo = IRTemp_INVALID;
   breakupV256toV128s(aa, &aaHi, &aaLo);
   assign(res, binop(Iop_V128HLtoV256,
                     mkexpr(math_PABS_XMM(aaHi, laneszB)),
                     mkexpr(math_PABS_XMM(aaLo, laneszB))));
   return res;
}

static IRTemp math_PABS_YMM_pap4 ( IRTemp aa ) {
   return math_PABS_YMM(aa, 4);
}

static IRTemp math_PABS_YMM_pap2 ( IRTemp aa ) {
   return math_PABS_YMM(aa, 2);
}

static IRTemp math_PABS_YMM_pap1 ( IRTemp aa ) {
   return math_PABS_YMM(aa, 1);
}

static IRExpr* dis_PALIGNR_XMM_helper ( IRTemp hi64,
                                        IRTemp lo64, Long byteShift )
{
   vassert(byteShift >= 1 && byteShift <= 7);
   return
      binop(Iop_Or64,
            binop(Iop_Shl64, mkexpr(hi64), mkU8(8*(8-byteShift))),
            binop(Iop_Shr64, mkexpr(lo64), mkU8(8*byteShift))
      );
}

static IRTemp math_PALIGNR_XMM ( IRTemp sV, IRTemp dV, UInt imm8 ) 
{
   IRTemp res = newTemp(Ity_V128);
   IRTemp sHi = newTemp(Ity_I64);
   IRTemp sLo = newTemp(Ity_I64);
   IRTemp dHi = newTemp(Ity_I64);
   IRTemp dLo = newTemp(Ity_I64);
   IRTemp rHi = newTemp(Ity_I64);
   IRTemp rLo = newTemp(Ity_I64);

   assign( dHi, unop(Iop_V128HIto64, mkexpr(dV)) );
   assign( dLo, unop(Iop_V128to64,   mkexpr(dV)) );
   assign( sHi, unop(Iop_V128HIto64, mkexpr(sV)) );
   assign( sLo, unop(Iop_V128to64,   mkexpr(sV)) );

   if (imm8 == 0) {
      assign( rHi, mkexpr(sHi) );
      assign( rLo, mkexpr(sLo) );
   }
   else if (imm8 >= 1 && imm8 <= 7) {
      assign( rHi, dis_PALIGNR_XMM_helper(dLo, sHi, imm8) );
      assign( rLo, dis_PALIGNR_XMM_helper(sHi, sLo, imm8) );
   }
   else if (imm8 == 8) {
      assign( rHi, mkexpr(dLo) );
      assign( rLo, mkexpr(sHi) );
   }
   else if (imm8 >= 9 && imm8 <= 15) {
      assign( rHi, dis_PALIGNR_XMM_helper(dHi, dLo, imm8-8) );
      assign( rLo, dis_PALIGNR_XMM_helper(dLo, sHi, imm8-8) );
   }
   else if (imm8 == 16) {
      assign( rHi, mkexpr(dHi) );
      assign( rLo, mkexpr(dLo) );
   }
   else if (imm8 >= 17 && imm8 <= 23) {
      assign( rHi, binop(Iop_Shr64, mkexpr(dHi), mkU8(8*(imm8-16))) );
      assign( rLo, dis_PALIGNR_XMM_helper(dHi, dLo, imm8-16) );
   }
   else if (imm8 == 24) {
      assign( rHi, mkU64(0) );
      assign( rLo, mkexpr(dHi) );
   }
   else if (imm8 >= 25 && imm8 <= 31) {
      assign( rHi, mkU64(0) );
      assign( rLo, binop(Iop_Shr64, mkexpr(dHi), mkU8(8*(imm8-24))) );
   }
   else if (imm8 >= 32 && imm8 <= 255) {
      assign( rHi, mkU64(0) );
      assign( rLo, mkU64(0) );
   }
   else
      vassert(0);

   assign( res, binop(Iop_64HLtoV128, mkexpr(rHi), mkexpr(rLo)));
   return res;
}


static
void gen_SEGV_if_not_XX_aligned ( IRTemp effective_addr, ULong mask )
{
   stmt(
      IRStmt_Exit(
         binop(Iop_CmpNE64,
               binop(Iop_And64,mkexpr(effective_addr),mkU64(mask)),
               mkU64(0)),
         Ijk_SigSEGV,
         IRConst_U64(guest_RIP_curr_instr),
         OFFB_RIP
      )
   );
}

static void gen_SEGV_if_not_16_aligned ( IRTemp effective_addr ) {
   gen_SEGV_if_not_XX_aligned(effective_addr, 16-1);
}

static void gen_SEGV_if_not_32_aligned ( IRTemp effective_addr ) {
   gen_SEGV_if_not_XX_aligned(effective_addr, 32-1);
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
         if (gregLO3ofRM(opc[1]) >= 0 && gregLO3ofRM(opc[1]) <= 6
             && !epartIsReg(opc[1]))
            return True;
         break;

      case 0xFE: case 0xFF:
         if (gregLO3ofRM(opc[1]) >= 0 && gregLO3ofRM(opc[1]) <= 1
             && !epartIsReg(opc[1]))
            return True;
         break;

      case 0xF6: case 0xF7:
         if (gregLO3ofRM(opc[1]) >= 2 && gregLO3ofRM(opc[1]) <= 3
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
               if (gregLO3ofRM(opc[2]) >= 5 && gregLO3ofRM(opc[2]) <= 7
                   && !epartIsReg(opc[2]))
                  return True;
               break;
            case 0xB0: case 0xB1:
               if (!epartIsReg(opc[2]))
                  return True;
               break;
            case 0xC7: 
               if (gregLO3ofRM(opc[2]) == 1 && !epartIsReg(opc[2]) )
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



static Long dis_COMISD ( const VexAbiInfo* vbi, Prefix pfx,
                         Long delta, Bool isAvx, UChar opc )
{
   vassert(opc == 0x2F || opc == 0x2E);
   Int    alen  = 0;
   HChar  dis_buf[50];
   IRTemp argL  = newTemp(Ity_F64);
   IRTemp argR  = newTemp(Ity_F64);
   UChar  modrm = getUChar(delta);
   IRTemp addr  = IRTemp_INVALID;
   if (epartIsReg(modrm)) {
      assign( argR, getXMMRegLane64F( eregOfRexRM(pfx,modrm), 
                                      0 ) );
      delta += 1;
      DIP("%s%scomisd %s,%s\n", isAvx ? "v" : "",
                                opc==0x2E ? "u" : "",
                                nameXMMReg(eregOfRexRM(pfx,modrm)),
                                nameXMMReg(gregOfRexRM(pfx,modrm)) );
   } else {
      addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
      assign( argR, loadLE(Ity_F64, mkexpr(addr)) );
      delta += alen;
      DIP("%s%scomisd %s,%s\n", isAvx ? "v" : "",
                                opc==0x2E ? "u" : "",
                                dis_buf,
                                nameXMMReg(gregOfRexRM(pfx,modrm)) );
   }
   assign( argL, getXMMRegLane64F( gregOfRexRM(pfx,modrm), 
                                   0 ) );

   stmt( IRStmt_Put( OFFB_CC_OP,   mkU64(AMD64G_CC_OP_COPY) ));
   stmt( IRStmt_Put( OFFB_CC_DEP2, mkU64(0) ));
   stmt( IRStmt_Put( 
            OFFB_CC_DEP1,
            binop( Iop_And64,
                   unop( Iop_32Uto64, 
                         binop(Iop_CmpF64, mkexpr(argL), mkexpr(argR)) ),
                   mkU64(0x45)
       )));
   return delta;
}


static Long dis_COMISS ( const VexAbiInfo* vbi, Prefix pfx,
                         Long delta, Bool isAvx, UChar opc )
{
   vassert(opc == 0x2F || opc == 0x2E);
   Int    alen  = 0;
   HChar  dis_buf[50];
   IRTemp argL  = newTemp(Ity_F32);
   IRTemp argR  = newTemp(Ity_F32);
   UChar  modrm = getUChar(delta);
   IRTemp addr  = IRTemp_INVALID;
   if (epartIsReg(modrm)) {
      assign( argR, getXMMRegLane32F( eregOfRexRM(pfx,modrm), 
                                      0 ) );
      delta += 1;
      DIP("%s%scomiss %s,%s\n", isAvx ? "v" : "",
                                opc==0x2E ? "u" : "",
                                nameXMMReg(eregOfRexRM(pfx,modrm)),
                                nameXMMReg(gregOfRexRM(pfx,modrm)) );
   } else {
      addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
      assign( argR, loadLE(Ity_F32, mkexpr(addr)) );
      delta += alen;
      DIP("%s%scomiss %s,%s\n", isAvx ? "v" : "",
                                opc==0x2E ? "u" : "",
                                dis_buf,
                                nameXMMReg(gregOfRexRM(pfx,modrm)) );
   }
   assign( argL, getXMMRegLane32F( gregOfRexRM(pfx,modrm), 
                                   0 ) );

   stmt( IRStmt_Put( OFFB_CC_OP,   mkU64(AMD64G_CC_OP_COPY) ));
   stmt( IRStmt_Put( OFFB_CC_DEP2, mkU64(0) ));
   stmt( IRStmt_Put( 
            OFFB_CC_DEP1,
            binop( Iop_And64,
                   unop( Iop_32Uto64, 
                         binop(Iop_CmpF64, 
                               unop(Iop_F32toF64,mkexpr(argL)),
                               unop(Iop_F32toF64,mkexpr(argR)))),
                   mkU64(0x45)
       )));
   return delta;
}


static Long dis_PSHUFD_32x4 ( const VexAbiInfo* vbi, Prefix pfx,
                              Long delta, Bool writesYmm )
{
   Int    order;
   Int    alen  = 0;
   HChar  dis_buf[50];
   IRTemp sV    = newTemp(Ity_V128);
   UChar  modrm = getUChar(delta);
   const HChar* strV  = writesYmm ? "v" : "";
   IRTemp addr  = IRTemp_INVALID;
   if (epartIsReg(modrm)) {
      assign( sV, getXMMReg(eregOfRexRM(pfx,modrm)) );
      order = (Int)getUChar(delta+1);
      delta += 1+1;
      DIP("%spshufd $%d,%s,%s\n", strV, order, 
                                  nameXMMReg(eregOfRexRM(pfx,modrm)),
                                  nameXMMReg(gregOfRexRM(pfx,modrm)));
   } else {
      addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 
                        1 );
      assign( sV, loadLE(Ity_V128, mkexpr(addr)) );
      order = (Int)getUChar(delta+alen);
      delta += alen+1;
      DIP("%spshufd $%d,%s,%s\n", strV, order, 
                                 dis_buf,
                                 nameXMMReg(gregOfRexRM(pfx,modrm)));
   }

   IRTemp s3, s2, s1, s0;
   s3 = s2 = s1 = s0 = IRTemp_INVALID;
   breakupV128to32s( sV, &s3, &s2, &s1, &s0 );

#  define SEL(n)  ((n)==0 ? s0 : ((n)==1 ? s1 : ((n)==2 ? s2 : s3)))
   IRTemp dV = newTemp(Ity_V128);
   assign(dV,
          mkV128from32s( SEL((order>>6)&3), SEL((order>>4)&3),
                         SEL((order>>2)&3), SEL((order>>0)&3) )
   );
#  undef SEL

   (writesYmm ? putYMMRegLoAndZU : putXMMReg)
      (gregOfRexRM(pfx,modrm), mkexpr(dV));
   return delta;
}


static Long dis_PSHUFD_32x8 ( const VexAbiInfo* vbi, Prefix pfx, Long delta )
{
   Int    order;
   Int    alen  = 0;
   HChar  dis_buf[50];
   IRTemp sV    = newTemp(Ity_V256);
   UChar  modrm = getUChar(delta);
   IRTemp addr  = IRTemp_INVALID;
   UInt   rG    = gregOfRexRM(pfx,modrm);
   if (epartIsReg(modrm)) {
      UInt rE = eregOfRexRM(pfx,modrm);
      assign( sV, getYMMReg(rE) );
      order = (Int)getUChar(delta+1);
      delta += 1+1;
      DIP("vpshufd $%d,%s,%s\n", order, nameYMMReg(rE), nameYMMReg(rG));
   } else {
      addr = disAMode ( &alen, vbi, pfx, delta, dis_buf,
                        1 );
      assign( sV, loadLE(Ity_V256, mkexpr(addr)) );
      order = (Int)getUChar(delta+alen);
      delta += alen+1;
      DIP("vpshufd $%d,%s,%s\n", order,  dis_buf, nameYMMReg(rG));
   }

   IRTemp s[8];
   s[7] = s[6] = s[5] = s[4] = s[3] = s[2] = s[1] = s[0] = IRTemp_INVALID;
   breakupV256to32s( sV, &s[7], &s[6], &s[5], &s[4],
                         &s[3], &s[2], &s[1], &s[0] );

   putYMMReg( rG, mkV256from32s( s[4 + ((order>>6)&3)],
                                 s[4 + ((order>>4)&3)],
                                 s[4 + ((order>>2)&3)],
                                 s[4 + ((order>>0)&3)],
                                 s[0 + ((order>>6)&3)],
                                 s[0 + ((order>>4)&3)],
                                 s[0 + ((order>>2)&3)],
                                 s[0 + ((order>>0)&3)] ) );
   return delta;
}


static IRTemp math_PSRLDQ ( IRTemp sV, Int imm )
{
   IRTemp dV    = newTemp(Ity_V128);
   IRTemp hi64  = newTemp(Ity_I64);
   IRTemp lo64  = newTemp(Ity_I64);
   IRTemp hi64r = newTemp(Ity_I64);
   IRTemp lo64r = newTemp(Ity_I64);

   vassert(imm >= 0 && imm <= 255);
   if (imm >= 16) {
      assign(dV, mkV128(0x0000));
      return dV;
   }

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
      assign( lo64r, binop( Iop_Shr64, mkexpr(hi64), mkU8( 8*(imm-8) ) ));
   } else {
      assign( hi64r, binop( Iop_Shr64, mkexpr(hi64), mkU8(8 * imm) ));
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
   return dV;
}


static IRTemp math_PSLLDQ ( IRTemp sV, Int imm )
{
   IRTemp       dV    = newTemp(Ity_V128);
   IRTemp       hi64  = newTemp(Ity_I64);
   IRTemp       lo64  = newTemp(Ity_I64);
   IRTemp       hi64r = newTemp(Ity_I64);
   IRTemp       lo64r = newTemp(Ity_I64);

   vassert(imm >= 0 && imm <= 255);
   if (imm >= 16) {
      assign(dV, mkV128(0x0000));
      return dV;
   }

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
      assign( hi64r, binop( Iop_Shl64, mkexpr(lo64), mkU8( 8*(imm-8) ) ));
   } else {
      assign( lo64r, binop( Iop_Shl64, mkexpr(lo64), mkU8(8 * imm) ));
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
   return dV;
}


static Long dis_CVTxSD2SI ( const VexAbiInfo* vbi, Prefix pfx,
                            Long delta, Bool isAvx, UChar opc, Int sz )
{
   vassert(opc == 0x2D || opc == 0x2C);
   HChar  dis_buf[50];
   Int    alen   = 0;
   UChar  modrm  = getUChar(delta);
   IRTemp addr   = IRTemp_INVALID;
   IRTemp rmode  = newTemp(Ity_I32);
   IRTemp f64lo  = newTemp(Ity_F64);
   Bool   r2zero = toBool(opc == 0x2C);

   if (epartIsReg(modrm)) {
      delta += 1;
      assign(f64lo, getXMMRegLane64F(eregOfRexRM(pfx,modrm), 0));
      DIP("%scvt%ssd2si %s,%s\n", isAvx ? "v" : "", r2zero ? "t" : "",
                                  nameXMMReg(eregOfRexRM(pfx,modrm)),
                                  nameIReg(sz, gregOfRexRM(pfx,modrm),
                                           False));
   } else {
      addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
      assign(f64lo, loadLE(Ity_F64, mkexpr(addr)));
      delta += alen;
      DIP("%scvt%ssd2si %s,%s\n", isAvx ? "v" : "", r2zero ? "t" : "",
                                  dis_buf,
                                  nameIReg(sz, gregOfRexRM(pfx,modrm),
                                           False));
   }

   if (r2zero) {
      assign( rmode, mkU32((UInt)Irrm_ZERO) );
   } else {
      assign( rmode, get_sse_roundingmode() );
   }

   if (sz == 4) {
      putIReg32( gregOfRexRM(pfx,modrm),
                 binop( Iop_F64toI32S, mkexpr(rmode), mkexpr(f64lo)) );
   } else {
      vassert(sz == 8);
      putIReg64( gregOfRexRM(pfx,modrm),
                 binop( Iop_F64toI64S, mkexpr(rmode), mkexpr(f64lo)) );
   }

   return delta;
}


static Long dis_CVTxSS2SI ( const VexAbiInfo* vbi, Prefix pfx,
                            Long delta, Bool isAvx, UChar opc, Int sz )
{
   vassert(opc == 0x2D || opc == 0x2C);
   HChar  dis_buf[50];
   Int    alen   = 0;
   UChar  modrm  = getUChar(delta);
   IRTemp addr   = IRTemp_INVALID;
   IRTemp rmode  = newTemp(Ity_I32);
   IRTemp f32lo  = newTemp(Ity_F32);
   Bool   r2zero = toBool(opc == 0x2C);

   if (epartIsReg(modrm)) {
      delta += 1;
      assign(f32lo, getXMMRegLane32F(eregOfRexRM(pfx,modrm), 0));
      DIP("%scvt%sss2si %s,%s\n", isAvx ? "v" : "", r2zero ? "t" : "",
                                  nameXMMReg(eregOfRexRM(pfx,modrm)),
                                  nameIReg(sz, gregOfRexRM(pfx,modrm), 
                                           False));
   } else {
      addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
      assign(f32lo, loadLE(Ity_F32, mkexpr(addr)));
      delta += alen;
      DIP("%scvt%sss2si %s,%s\n", isAvx ? "v" : "", r2zero ? "t" : "",
                                  dis_buf,
                                  nameIReg(sz, gregOfRexRM(pfx,modrm),
                                           False));
   }

   if (r2zero) {
      assign( rmode, mkU32((UInt)Irrm_ZERO) );
   } else {
      assign( rmode, get_sse_roundingmode() );
   }

   if (sz == 4) {
      putIReg32( gregOfRexRM(pfx,modrm),
                 binop( Iop_F64toI32S, 
                        mkexpr(rmode), 
                        unop(Iop_F32toF64, mkexpr(f32lo))) );
   } else {
      vassert(sz == 8);
      putIReg64( gregOfRexRM(pfx,modrm),
                 binop( Iop_F64toI64S, 
                        mkexpr(rmode), 
                        unop(Iop_F32toF64, mkexpr(f32lo))) );
   }
   
   return delta;
}


static Long dis_CVTPS2PD_128 ( const VexAbiInfo* vbi, Prefix pfx,
                               Long delta, Bool isAvx )
{
   IRTemp addr  = IRTemp_INVALID;
   Int    alen  = 0;
   HChar  dis_buf[50];
   IRTemp f32lo = newTemp(Ity_F32);
   IRTemp f32hi = newTemp(Ity_F32);
   UChar  modrm = getUChar(delta);
   UInt   rG    = gregOfRexRM(pfx,modrm);
   if (epartIsReg(modrm)) {
      UInt rE = eregOfRexRM(pfx,modrm);
      assign( f32lo, getXMMRegLane32F(rE, 0) );
      assign( f32hi, getXMMRegLane32F(rE, 1) );
      delta += 1;
      DIP("%scvtps2pd %s,%s\n",
          isAvx ? "v" : "", nameXMMReg(rE), nameXMMReg(rG));
   } else {
      addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
      assign( f32lo, loadLE(Ity_F32, mkexpr(addr)) );
      assign( f32hi, loadLE(Ity_F32, 
                            binop(Iop_Add64,mkexpr(addr),mkU64(4))) );
      delta += alen;
      DIP("%scvtps2pd %s,%s\n",
          isAvx ? "v" : "", dis_buf, nameXMMReg(rG));
   }

   putXMMRegLane64F( rG, 1, unop(Iop_F32toF64, mkexpr(f32hi)) );
   putXMMRegLane64F( rG, 0, unop(Iop_F32toF64, mkexpr(f32lo)) );
   if (isAvx)
      putYMMRegLane128( rG, 1, mkV128(0));
   return delta;
}


static Long dis_CVTPS2PD_256 ( const VexAbiInfo* vbi, Prefix pfx,
                               Long delta )
{
   IRTemp addr  = IRTemp_INVALID;
   Int    alen  = 0;
   HChar  dis_buf[50];
   IRTemp f32_0 = newTemp(Ity_F32);
   IRTemp f32_1 = newTemp(Ity_F32);
   IRTemp f32_2 = newTemp(Ity_F32);
   IRTemp f32_3 = newTemp(Ity_F32);
   UChar  modrm = getUChar(delta);
   UInt   rG    = gregOfRexRM(pfx,modrm);
   if (epartIsReg(modrm)) {
      UInt rE = eregOfRexRM(pfx,modrm);
      assign( f32_0, getXMMRegLane32F(rE, 0) );
      assign( f32_1, getXMMRegLane32F(rE, 1) );
      assign( f32_2, getXMMRegLane32F(rE, 2) );
      assign( f32_3, getXMMRegLane32F(rE, 3) );
      delta += 1;
      DIP("vcvtps2pd %s,%s\n", nameXMMReg(rE), nameYMMReg(rG));
   } else {
      addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
      assign( f32_0, loadLE(Ity_F32, mkexpr(addr)) );
      assign( f32_1, loadLE(Ity_F32, 
                            binop(Iop_Add64,mkexpr(addr),mkU64(4))) );
      assign( f32_2, loadLE(Ity_F32, 
                            binop(Iop_Add64,mkexpr(addr),mkU64(8))) );
      assign( f32_3, loadLE(Ity_F32, 
                            binop(Iop_Add64,mkexpr(addr),mkU64(12))) );
      delta += alen;
      DIP("vcvtps2pd %s,%s\n", dis_buf, nameYMMReg(rG));
   }

   putYMMRegLane64F( rG, 3, unop(Iop_F32toF64, mkexpr(f32_3)) );
   putYMMRegLane64F( rG, 2, unop(Iop_F32toF64, mkexpr(f32_2)) );
   putYMMRegLane64F( rG, 1, unop(Iop_F32toF64, mkexpr(f32_1)) );
   putYMMRegLane64F( rG, 0, unop(Iop_F32toF64, mkexpr(f32_0)) );
   return delta;
}


static Long dis_CVTPD2PS_128 ( const VexAbiInfo* vbi, Prefix pfx,
                               Long delta, Bool isAvx )
{
   IRTemp addr  = IRTemp_INVALID;
   Int    alen  = 0;
   HChar  dis_buf[50];
   UChar  modrm = getUChar(delta);
   UInt   rG    = gregOfRexRM(pfx,modrm);
   IRTemp argV  = newTemp(Ity_V128);
   IRTemp rmode = newTemp(Ity_I32);
   if (epartIsReg(modrm)) {
      UInt rE = eregOfRexRM(pfx,modrm);
      assign( argV, getXMMReg(rE) );
      delta += 1;
      DIP("%scvtpd2ps %s,%s\n", isAvx ? "v" : "",
          nameXMMReg(rE), nameXMMReg(rG));
   } else {
      addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
      assign( argV, loadLE(Ity_V128, mkexpr(addr)) );
      delta += alen;
      DIP("%scvtpd2ps %s,%s\n", isAvx ? "v" : "",
          dis_buf, nameXMMReg(rG) );
   }
         
   assign( rmode, get_sse_roundingmode() );
   IRTemp t0 = newTemp(Ity_F64);
   IRTemp t1 = newTemp(Ity_F64);
   assign( t0, unop(Iop_ReinterpI64asF64, 
                    unop(Iop_V128to64, mkexpr(argV))) );
   assign( t1, unop(Iop_ReinterpI64asF64, 
                    unop(Iop_V128HIto64, mkexpr(argV))) );
      
#  define CVT(_t)  binop( Iop_F64toF32, mkexpr(rmode), mkexpr(_t) )
   putXMMRegLane32(  rG, 3, mkU32(0) );
   putXMMRegLane32(  rG, 2, mkU32(0) );
   putXMMRegLane32F( rG, 1, CVT(t1) );
   putXMMRegLane32F( rG, 0, CVT(t0) );
#  undef CVT
   if (isAvx)
      putYMMRegLane128( rG, 1, mkV128(0) );

   return delta;
}


static Long dis_CVTxPS2DQ_128 ( const VexAbiInfo* vbi, Prefix pfx,
                                Long delta, Bool isAvx, Bool r2zero )
{
   IRTemp addr  = IRTemp_INVALID;
   Int    alen  = 0;
   HChar  dis_buf[50];
   UChar  modrm = getUChar(delta);
   IRTemp argV  = newTemp(Ity_V128);
   IRTemp rmode = newTemp(Ity_I32);
   UInt   rG    = gregOfRexRM(pfx,modrm);
   IRTemp t0, t1, t2, t3;

   if (epartIsReg(modrm)) {
      UInt rE = eregOfRexRM(pfx,modrm);
      assign( argV, getXMMReg(rE) );
      delta += 1;
      DIP("%scvt%sps2dq %s,%s\n",
          isAvx ? "v" : "", r2zero ? "t" : "", nameXMMReg(rE), nameXMMReg(rG));
   } else {
      addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
      assign( argV, loadLE(Ity_V128, mkexpr(addr)) );
      delta += alen;
      DIP("%scvt%sps2dq %s,%s\n",
          isAvx ? "v" : "", r2zero ? "t" : "", dis_buf, nameXMMReg(rG) );
   }

   assign( rmode, r2zero ? mkU32((UInt)Irrm_ZERO)
                         : get_sse_roundingmode() );
   t0 = t1 = t2 = t3 = IRTemp_INVALID;
   breakupV128to32s( argV, &t3, &t2, &t1, &t0 );
#  define CVT(_t)                             \
      binop( Iop_F64toI32S,                   \
             mkexpr(rmode),                   \
             unop( Iop_F32toF64,              \
                   unop( Iop_ReinterpI32asF32, mkexpr(_t))) )
      
   putXMMRegLane32( rG, 3, CVT(t3) );
   putXMMRegLane32( rG, 2, CVT(t2) );
   putXMMRegLane32( rG, 1, CVT(t1) );
   putXMMRegLane32( rG, 0, CVT(t0) );
#  undef CVT
   if (isAvx)
      putYMMRegLane128( rG, 1, mkV128(0) );

   return delta;
}


static Long dis_CVTxPS2DQ_256 ( const VexAbiInfo* vbi, Prefix pfx,
                                Long delta, Bool r2zero )
{
   IRTemp addr  = IRTemp_INVALID;
   Int    alen  = 0;
   HChar  dis_buf[50];
   UChar  modrm = getUChar(delta);
   IRTemp argV  = newTemp(Ity_V256);
   IRTemp rmode = newTemp(Ity_I32);
   UInt   rG    = gregOfRexRM(pfx,modrm);
   IRTemp t0, t1, t2, t3, t4, t5, t6, t7;

   if (epartIsReg(modrm)) {
      UInt rE = eregOfRexRM(pfx,modrm);
      assign( argV, getYMMReg(rE) );
      delta += 1;
      DIP("vcvt%sps2dq %s,%s\n",
          r2zero ? "t" : "", nameYMMReg(rE), nameYMMReg(rG));
   } else {
      addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
      assign( argV, loadLE(Ity_V256, mkexpr(addr)) );
      delta += alen;
      DIP("vcvt%sps2dq %s,%s\n",
          r2zero ? "t" : "", dis_buf, nameYMMReg(rG) );
   }

   assign( rmode, r2zero ? mkU32((UInt)Irrm_ZERO)
                         : get_sse_roundingmode() );
   t0 = t1 = t2 = t3 = t4 = t5 = t6 = t7 = IRTemp_INVALID;
   breakupV256to32s( argV, &t7, &t6, &t5, &t4, &t3, &t2, &t1, &t0 );
#  define CVT(_t)                             \
      binop( Iop_F64toI32S,                   \
             mkexpr(rmode),                   \
             unop( Iop_F32toF64,              \
                   unop( Iop_ReinterpI32asF32, mkexpr(_t))) )
      
   putYMMRegLane32( rG, 7, CVT(t7) );
   putYMMRegLane32( rG, 6, CVT(t6) );
   putYMMRegLane32( rG, 5, CVT(t5) );
   putYMMRegLane32( rG, 4, CVT(t4) );
   putYMMRegLane32( rG, 3, CVT(t3) );
   putYMMRegLane32( rG, 2, CVT(t2) );
   putYMMRegLane32( rG, 1, CVT(t1) );
   putYMMRegLane32( rG, 0, CVT(t0) );
#  undef CVT

   return delta;
}


static Long dis_CVTxPD2DQ_128 ( const VexAbiInfo* vbi, Prefix pfx,
                                Long delta, Bool isAvx, Bool r2zero )
{
   IRTemp addr  = IRTemp_INVALID;
   Int    alen  = 0;
   HChar  dis_buf[50];
   UChar  modrm = getUChar(delta);
   IRTemp argV  = newTemp(Ity_V128);
   IRTemp rmode = newTemp(Ity_I32);
   UInt   rG    = gregOfRexRM(pfx,modrm);
   IRTemp t0, t1;

   if (epartIsReg(modrm)) {
      UInt rE = eregOfRexRM(pfx,modrm);
      assign( argV, getXMMReg(rE) );
      delta += 1;
      DIP("%scvt%spd2dq %s,%s\n",
          isAvx ? "v" : "", r2zero ? "t" : "", nameXMMReg(rE), nameXMMReg(rG));
   } else {
      addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
      assign( argV, loadLE(Ity_V128, mkexpr(addr)) );
      delta += alen;
      DIP("%scvt%spd2dqx %s,%s\n",
          isAvx ? "v" : "", r2zero ? "t" : "", dis_buf, nameXMMReg(rG) );
   }

   if (r2zero) {
      assign(rmode, mkU32((UInt)Irrm_ZERO) );
   } else {
      assign( rmode, get_sse_roundingmode() );
   }

   t0 = newTemp(Ity_F64);
   t1 = newTemp(Ity_F64);
   assign( t0, unop(Iop_ReinterpI64asF64, 
                    unop(Iop_V128to64, mkexpr(argV))) );
   assign( t1, unop(Iop_ReinterpI64asF64, 
                    unop(Iop_V128HIto64, mkexpr(argV))) );

#  define CVT(_t)  binop( Iop_F64toI32S,                   \
                          mkexpr(rmode),                   \
                          mkexpr(_t) )

   putXMMRegLane32( rG, 3, mkU32(0) );
   putXMMRegLane32( rG, 2, mkU32(0) );
   putXMMRegLane32( rG, 1, CVT(t1) );
   putXMMRegLane32( rG, 0, CVT(t0) );
#  undef CVT
   if (isAvx)
      putYMMRegLane128( rG, 1, mkV128(0) );

   return delta;
}


static Long dis_CVTxPD2DQ_256 ( const VexAbiInfo* vbi, Prefix pfx,
                                Long delta, Bool r2zero )
{
   IRTemp addr  = IRTemp_INVALID;
   Int    alen  = 0;
   HChar  dis_buf[50];
   UChar  modrm = getUChar(delta);
   IRTemp argV  = newTemp(Ity_V256);
   IRTemp rmode = newTemp(Ity_I32);
   UInt   rG    = gregOfRexRM(pfx,modrm);
   IRTemp t0, t1, t2, t3;

   if (epartIsReg(modrm)) {
      UInt rE = eregOfRexRM(pfx,modrm);
      assign( argV, getYMMReg(rE) );
      delta += 1;
      DIP("vcvt%spd2dq %s,%s\n",
          r2zero ? "t" : "", nameYMMReg(rE), nameXMMReg(rG));
   } else {
      addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
      assign( argV, loadLE(Ity_V256, mkexpr(addr)) );
      delta += alen;
      DIP("vcvt%spd2dqy %s,%s\n",
          r2zero ? "t" : "", dis_buf, nameXMMReg(rG) );
   }

   if (r2zero) {
      assign(rmode, mkU32((UInt)Irrm_ZERO) );
   } else {
      assign( rmode, get_sse_roundingmode() );
   }

   t0 = IRTemp_INVALID;
   t1 = IRTemp_INVALID;
   t2 = IRTemp_INVALID;
   t3 = IRTemp_INVALID;
   breakupV256to64s( argV, &t3, &t2, &t1, &t0 );

#  define CVT(_t)  binop( Iop_F64toI32S,                   \
                          mkexpr(rmode),                   \
                          unop( Iop_ReinterpI64asF64,      \
                                mkexpr(_t) ) )

   putXMMRegLane32( rG, 3, CVT(t3) );
   putXMMRegLane32( rG, 2, CVT(t2) );
   putXMMRegLane32( rG, 1, CVT(t1) );
   putXMMRegLane32( rG, 0, CVT(t0) );
#  undef CVT
   putYMMRegLane128( rG, 1, mkV128(0) );

   return delta;
}


static Long dis_CVTDQ2PS_128 ( const VexAbiInfo* vbi, Prefix pfx,
                               Long delta, Bool isAvx )
{
   IRTemp addr  = IRTemp_INVALID;
   Int    alen  = 0;
   HChar  dis_buf[50];
   UChar  modrm = getUChar(delta);
   IRTemp argV  = newTemp(Ity_V128);
   IRTemp rmode = newTemp(Ity_I32);
   UInt   rG    = gregOfRexRM(pfx,modrm);
   IRTemp t0, t1, t2, t3;

   if (epartIsReg(modrm)) {
      UInt rE = eregOfRexRM(pfx,modrm);
      assign( argV, getXMMReg(rE) );
      delta += 1;
      DIP("%scvtdq2ps %s,%s\n",
          isAvx ? "v" : "", nameXMMReg(rE), nameXMMReg(rG));
   } else {
      addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
      assign( argV, loadLE(Ity_V128, mkexpr(addr)) );
      delta += alen;
      DIP("%scvtdq2ps %s,%s\n",
          isAvx ? "v" : "", dis_buf, nameXMMReg(rG) );
   }

   assign( rmode, get_sse_roundingmode() );
   t0 = IRTemp_INVALID;
   t1 = IRTemp_INVALID;
   t2 = IRTemp_INVALID;
   t3 = IRTemp_INVALID;
   breakupV128to32s( argV, &t3, &t2, &t1, &t0 );

#  define CVT(_t)  binop( Iop_F64toF32,                    \
                          mkexpr(rmode),                   \
                          unop(Iop_I32StoF64,mkexpr(_t)))
      
   putXMMRegLane32F( rG, 3, CVT(t3) );
   putXMMRegLane32F( rG, 2, CVT(t2) );
   putXMMRegLane32F( rG, 1, CVT(t1) );
   putXMMRegLane32F( rG, 0, CVT(t0) );
#  undef CVT
   if (isAvx)
      putYMMRegLane128( rG, 1, mkV128(0) );

   return delta;
}

static Long dis_CVTDQ2PS_256 ( const VexAbiInfo* vbi, Prefix pfx,
                               Long delta )
{
   IRTemp addr   = IRTemp_INVALID;
   Int    alen   = 0;
   HChar  dis_buf[50];
   UChar  modrm  = getUChar(delta);
   IRTemp argV   = newTemp(Ity_V256);
   IRTemp rmode  = newTemp(Ity_I32);
   UInt   rG     = gregOfRexRM(pfx,modrm);
   IRTemp t0, t1, t2, t3, t4, t5, t6, t7;

   if (epartIsReg(modrm)) {
      UInt rE = eregOfRexRM(pfx,modrm);
      assign( argV, getYMMReg(rE) );
      delta += 1;
      DIP("vcvtdq2ps %s,%s\n", nameYMMReg(rE), nameYMMReg(rG));
   } else {
      addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
      assign( argV, loadLE(Ity_V256, mkexpr(addr)) );
      delta += alen;
      DIP("vcvtdq2ps %s,%s\n", dis_buf, nameYMMReg(rG) );
   }

   assign( rmode, get_sse_roundingmode() );
   t0 = IRTemp_INVALID;
   t1 = IRTemp_INVALID;
   t2 = IRTemp_INVALID;
   t3 = IRTemp_INVALID;
   t4 = IRTemp_INVALID;
   t5 = IRTemp_INVALID;
   t6 = IRTemp_INVALID;
   t7 = IRTemp_INVALID;
   breakupV256to32s( argV, &t7, &t6, &t5, &t4, &t3, &t2, &t1, &t0 );

#  define CVT(_t)  binop( Iop_F64toF32,                    \
                          mkexpr(rmode),                   \
                          unop(Iop_I32StoF64,mkexpr(_t)))
      
   putYMMRegLane32F( rG, 7, CVT(t7) );
   putYMMRegLane32F( rG, 6, CVT(t6) );
   putYMMRegLane32F( rG, 5, CVT(t5) );
   putYMMRegLane32F( rG, 4, CVT(t4) );
   putYMMRegLane32F( rG, 3, CVT(t3) );
   putYMMRegLane32F( rG, 2, CVT(t2) );
   putYMMRegLane32F( rG, 1, CVT(t1) );
   putYMMRegLane32F( rG, 0, CVT(t0) );
#  undef CVT

   return delta;
}


static Long dis_PMOVMSKB_128 ( const VexAbiInfo* vbi, Prefix pfx,
                               Long delta, Bool isAvx )
{
   UChar modrm = getUChar(delta);
   vassert(epartIsReg(modrm)); 
   UInt   rE = eregOfRexRM(pfx,modrm);
   UInt   rG = gregOfRexRM(pfx,modrm);
   IRTemp t0 = newTemp(Ity_V128);
   IRTemp t1 = newTemp(Ity_I32);
   assign(t0, getXMMReg(rE));
   assign(t1, unop(Iop_16Uto32, unop(Iop_GetMSBs8x16, mkexpr(t0))));
   putIReg32(rG, mkexpr(t1));
   DIP("%spmovmskb %s,%s\n", isAvx ? "v" : "", nameXMMReg(rE),
       nameIReg32(rG));
   delta += 1;
   return delta;
}


static Long dis_PMOVMSKB_256 ( const VexAbiInfo* vbi, Prefix pfx,
                               Long delta  )
{
   UChar modrm = getUChar(delta);
   vassert(epartIsReg(modrm)); 
   UInt   rE = eregOfRexRM(pfx,modrm);
   UInt   rG = gregOfRexRM(pfx,modrm);
   IRTemp t0 = newTemp(Ity_V128);
   IRTemp t1 = newTemp(Ity_V128);
   IRTemp t2 = newTemp(Ity_I16);
   IRTemp t3 = newTemp(Ity_I16);
   assign(t0, getYMMRegLane128(rE, 0));
   assign(t1, getYMMRegLane128(rE, 1));
   assign(t2, unop(Iop_GetMSBs8x16, mkexpr(t0)));
   assign(t3, unop(Iop_GetMSBs8x16, mkexpr(t1)));
   putIReg32(rG, binop(Iop_16HLto32, mkexpr(t3), mkexpr(t2)));
   DIP("vpmovmskb %s,%s\n", nameYMMReg(rE), nameIReg32(rG));
   delta += 1;
   return delta;
}


static IRTemp math_UNPCKxPS_128 ( IRTemp sV, IRTemp dV, Bool xIsH )
{
   IRTemp s3, s2, s1, s0, d3, d2, d1, d0;
   s3 = s2 = s1 = s0 = d3 = d2 = d1 = d0 = IRTemp_INVALID;
   breakupV128to32s( dV, &d3, &d2, &d1, &d0 );
   breakupV128to32s( sV, &s3, &s2, &s1, &s0 );
   IRTemp res = newTemp(Ity_V128);
   assign(res,  xIsH ? mkV128from32s( s3, d3, s2, d2 )
                     : mkV128from32s( s1, d1, s0, d0 ));
   return res;
}


static IRTemp math_UNPCKxPD_128 ( IRTemp sV, IRTemp dV, Bool xIsH )
{
   IRTemp s1 = newTemp(Ity_I64);
   IRTemp s0 = newTemp(Ity_I64);
   IRTemp d1 = newTemp(Ity_I64);
   IRTemp d0 = newTemp(Ity_I64);
   assign( d1, unop(Iop_V128HIto64, mkexpr(dV)) );
   assign( d0, unop(Iop_V128to64,   mkexpr(dV)) );
   assign( s1, unop(Iop_V128HIto64, mkexpr(sV)) );
   assign( s0, unop(Iop_V128to64,   mkexpr(sV)) );
   IRTemp res = newTemp(Ity_V128);
   assign(res, xIsH ? binop(Iop_64HLtoV128, mkexpr(s1), mkexpr(d1))
                    : binop(Iop_64HLtoV128, mkexpr(s0), mkexpr(d0)));
   return res;
}


static IRTemp math_UNPCKxPD_256 ( IRTemp sV, IRTemp dV, Bool xIsH )
{
   IRTemp s3, s2, s1, s0, d3, d2, d1, d0;
   s3 = s2 = s1 = s0 = d3 = d2 = d1 = d0 = IRTemp_INVALID;
   breakupV256to64s( dV, &d3, &d2, &d1, &d0 );
   breakupV256to64s( sV, &s3, &s2, &s1, &s0 );
   IRTemp res = newTemp(Ity_V256);
   assign(res, xIsH
               ? IRExpr_Qop(Iop_64x4toV256, mkexpr(s3), mkexpr(d3),
                                            mkexpr(s1), mkexpr(d1))
               : IRExpr_Qop(Iop_64x4toV256, mkexpr(s2), mkexpr(d2),
                                            mkexpr(s0), mkexpr(d0)));
   return res;
}


static IRTemp math_UNPCKxPS_256 ( IRTemp sV, IRTemp dV, Bool xIsH )
{
   IRTemp sVhi = IRTemp_INVALID, sVlo = IRTemp_INVALID;
   IRTemp dVhi = IRTemp_INVALID, dVlo = IRTemp_INVALID;
   breakupV256toV128s( sV, &sVhi, &sVlo );
   breakupV256toV128s( dV, &dVhi, &dVlo );
   IRTemp rVhi = math_UNPCKxPS_128(sVhi, dVhi, xIsH);
   IRTemp rVlo = math_UNPCKxPS_128(sVlo, dVlo, xIsH);
   IRTemp rV   = newTemp(Ity_V256);
   assign(rV, binop(Iop_V128HLtoV256, mkexpr(rVhi), mkexpr(rVlo)));
   return rV;
}


static IRTemp math_SHUFPS_128 ( IRTemp sV, IRTemp dV, UInt imm8 )
{
   IRTemp s3, s2, s1, s0, d3, d2, d1, d0;
   s3 = s2 = s1 = s0 = d3 = d2 = d1 = d0 = IRTemp_INVALID;
   vassert(imm8 < 256);

   breakupV128to32s( dV, &d3, &d2, &d1, &d0 );
   breakupV128to32s( sV, &s3, &s2, &s1, &s0 );

#  define SELD(n) ((n)==0 ? d0 : ((n)==1 ? d1 : ((n)==2 ? d2 : d3)))
#  define SELS(n) ((n)==0 ? s0 : ((n)==1 ? s1 : ((n)==2 ? s2 : s3)))
   IRTemp res = newTemp(Ity_V128);
   assign(res, 
          mkV128from32s( SELS((imm8>>6)&3), SELS((imm8>>4)&3), 
                         SELD((imm8>>2)&3), SELD((imm8>>0)&3) ) );
#  undef SELD
#  undef SELS
   return res;
}


static IRTemp math_SHUFPS_256 ( IRTemp sV, IRTemp dV, UInt imm8 )
{
   IRTemp sVhi = IRTemp_INVALID, sVlo = IRTemp_INVALID;
   IRTemp dVhi = IRTemp_INVALID, dVlo = IRTemp_INVALID;
   breakupV256toV128s( sV, &sVhi, &sVlo );
   breakupV256toV128s( dV, &dVhi, &dVlo );
   IRTemp rVhi = math_SHUFPS_128(sVhi, dVhi, imm8);
   IRTemp rVlo = math_SHUFPS_128(sVlo, dVlo, imm8);
   IRTemp rV   = newTemp(Ity_V256);
   assign(rV, binop(Iop_V128HLtoV256, mkexpr(rVhi), mkexpr(rVlo)));
   return rV;
}


static IRTemp math_SHUFPD_128 ( IRTemp sV, IRTemp dV, UInt imm8 )
{
   IRTemp s1 = newTemp(Ity_I64);
   IRTemp s0 = newTemp(Ity_I64);
   IRTemp d1 = newTemp(Ity_I64);
   IRTemp d0 = newTemp(Ity_I64);

   assign( d1, unop(Iop_V128HIto64, mkexpr(dV)) );
   assign( d0, unop(Iop_V128to64,   mkexpr(dV)) );
   assign( s1, unop(Iop_V128HIto64, mkexpr(sV)) );
   assign( s0, unop(Iop_V128to64,   mkexpr(sV)) );

#  define SELD(n) mkexpr((n)==0 ? d0 : d1)
#  define SELS(n) mkexpr((n)==0 ? s0 : s1)

   IRTemp res = newTemp(Ity_V128);
   assign(res, binop( Iop_64HLtoV128,
                      SELS((imm8>>1)&1), SELD((imm8>>0)&1) ) );

#  undef SELD
#  undef SELS
   return res;
}


static IRTemp math_SHUFPD_256 ( IRTemp sV, IRTemp dV, UInt imm8 )
{
   IRTemp sVhi = IRTemp_INVALID, sVlo = IRTemp_INVALID;
   IRTemp dVhi = IRTemp_INVALID, dVlo = IRTemp_INVALID;
   breakupV256toV128s( sV, &sVhi, &sVlo );
   breakupV256toV128s( dV, &dVhi, &dVlo );
   IRTemp rVhi = math_SHUFPD_128(sVhi, dVhi, (imm8 >> 2) & 3);
   IRTemp rVlo = math_SHUFPD_128(sVlo, dVlo, imm8 & 3);
   IRTemp rV   = newTemp(Ity_V256);
   assign(rV, binop(Iop_V128HLtoV256, mkexpr(rVhi), mkexpr(rVlo)));
   return rV;
}


static IRTemp math_BLENDPD_128 ( IRTemp sV, IRTemp dV, UInt imm8 )
{
   UShort imm8_mask_16;
   IRTemp imm8_mask = newTemp(Ity_V128);

   switch( imm8 & 3 ) {
      case 0:  imm8_mask_16 = 0x0000; break;
      case 1:  imm8_mask_16 = 0x00FF; break;
      case 2:  imm8_mask_16 = 0xFF00; break;
      case 3:  imm8_mask_16 = 0xFFFF; break;
      default: vassert(0);            break;
   }
   assign( imm8_mask, mkV128( imm8_mask_16 ) );

   IRTemp res = newTemp(Ity_V128);
   assign ( res, binop( Iop_OrV128, 
                        binop( Iop_AndV128, mkexpr(sV),
                                            mkexpr(imm8_mask) ), 
                        binop( Iop_AndV128, mkexpr(dV), 
                               unop( Iop_NotV128, mkexpr(imm8_mask) ) ) ) );
   return res;
}


static IRTemp math_BLENDPD_256 ( IRTemp sV, IRTemp dV, UInt imm8 )
{
   IRTemp sVhi = IRTemp_INVALID, sVlo = IRTemp_INVALID;
   IRTemp dVhi = IRTemp_INVALID, dVlo = IRTemp_INVALID;
   breakupV256toV128s( sV, &sVhi, &sVlo );
   breakupV256toV128s( dV, &dVhi, &dVlo );
   IRTemp rVhi = math_BLENDPD_128(sVhi, dVhi, (imm8 >> 2) & 3);
   IRTemp rVlo = math_BLENDPD_128(sVlo, dVlo, imm8 & 3);
   IRTemp rV   = newTemp(Ity_V256);
   assign(rV, binop(Iop_V128HLtoV256, mkexpr(rVhi), mkexpr(rVlo)));
   return rV;
}


static IRTemp math_BLENDPS_128 ( IRTemp sV, IRTemp dV, UInt imm8 )
{
   UShort imm8_perms[16] = { 0x0000, 0x000F, 0x00F0, 0x00FF, 0x0F00,
                             0x0F0F, 0x0FF0, 0x0FFF, 0xF000, 0xF00F,
                             0xF0F0, 0xF0FF, 0xFF00, 0xFF0F, 0xFFF0,
                             0xFFFF };
   IRTemp imm8_mask = newTemp(Ity_V128);
   assign( imm8_mask, mkV128( imm8_perms[ (imm8 & 15) ] ) );

   IRTemp res = newTemp(Ity_V128);
   assign ( res, binop( Iop_OrV128,
                        binop( Iop_AndV128, mkexpr(sV), 
                                            mkexpr(imm8_mask) ),
                        binop( Iop_AndV128, mkexpr(dV),
                               unop( Iop_NotV128, mkexpr(imm8_mask) ) ) ) );
   return res;
}


static IRTemp math_BLENDPS_256 ( IRTemp sV, IRTemp dV, UInt imm8 )
{
   IRTemp sVhi = IRTemp_INVALID, sVlo = IRTemp_INVALID;
   IRTemp dVhi = IRTemp_INVALID, dVlo = IRTemp_INVALID;
   breakupV256toV128s( sV, &sVhi, &sVlo );
   breakupV256toV128s( dV, &dVhi, &dVlo );
   IRTemp rVhi = math_BLENDPS_128(sVhi, dVhi, (imm8 >> 4) & 15);
   IRTemp rVlo = math_BLENDPS_128(sVlo, dVlo, imm8 & 15);
   IRTemp rV   = newTemp(Ity_V256);
   assign(rV, binop(Iop_V128HLtoV256, mkexpr(rVhi), mkexpr(rVlo)));
   return rV;
}


static IRTemp math_PBLENDW_128 ( IRTemp sV, IRTemp dV, UInt imm8 )
{
   Int i;
   UShort imm16 = 0;
   for (i = 0; i < 8; i++) {
      if (imm8 & (1 << i))
         imm16 |= (3 << (2*i));
   }
   IRTemp imm16_mask = newTemp(Ity_V128);
   assign( imm16_mask, mkV128( imm16 ));

   IRTemp res = newTemp(Ity_V128);
   assign ( res, binop( Iop_OrV128,
                        binop( Iop_AndV128, mkexpr(sV), 
                                            mkexpr(imm16_mask) ),
                        binop( Iop_AndV128, mkexpr(dV),
                               unop( Iop_NotV128, mkexpr(imm16_mask) ) ) ) );
   return res;
}


static IRTemp math_PMULUDQ_128 ( IRTemp sV, IRTemp dV )
{
   IRTemp s3, s2, s1, s0, d3, d2, d1, d0;
   s3 = s2 = s1 = s0 = d3 = d2 = d1 = d0 = IRTemp_INVALID;
   breakupV128to32s( dV, &d3, &d2, &d1, &d0 );
   breakupV128to32s( sV, &s3, &s2, &s1, &s0 );
   IRTemp res = newTemp(Ity_V128);
   assign(res, binop(Iop_64HLtoV128,
                     binop( Iop_MullU32, mkexpr(d2), mkexpr(s2)),
                     binop( Iop_MullU32, mkexpr(d0), mkexpr(s0)) ));
   return res;
}


static IRTemp math_PMULUDQ_256 ( IRTemp sV, IRTemp dV )
{
   IRTemp sHi, sLo, dHi, dLo;
   sHi = sLo = dHi = dLo = IRTemp_INVALID;
   breakupV256toV128s( dV, &dHi, &dLo);
   breakupV256toV128s( sV, &sHi, &sLo);
   IRTemp res = newTemp(Ity_V256);
   assign(res, binop(Iop_V128HLtoV256,
                     mkexpr(math_PMULUDQ_128(sHi, dHi)),
                     mkexpr(math_PMULUDQ_128(sLo, dLo))));
   return res;
}


static IRTemp math_PMULDQ_128 ( IRTemp dV, IRTemp sV )
{
   IRTemp s3, s2, s1, s0, d3, d2, d1, d0;
   s3 = s2 = s1 = s0 = d3 = d2 = d1 = d0 = IRTemp_INVALID;
   breakupV128to32s( dV, &d3, &d2, &d1, &d0 );
   breakupV128to32s( sV, &s3, &s2, &s1, &s0 );
   IRTemp res = newTemp(Ity_V128);
   assign(res, binop(Iop_64HLtoV128,
                     binop( Iop_MullS32, mkexpr(d2), mkexpr(s2)),
                     binop( Iop_MullS32, mkexpr(d0), mkexpr(s0)) ));
   return res;
}


static IRTemp math_PMULDQ_256 ( IRTemp sV, IRTemp dV )
{
   IRTemp sHi, sLo, dHi, dLo;
   sHi = sLo = dHi = dLo = IRTemp_INVALID;
   breakupV256toV128s( dV, &dHi, &dLo);
   breakupV256toV128s( sV, &sHi, &sLo);
   IRTemp res = newTemp(Ity_V256);
   assign(res, binop(Iop_V128HLtoV256,
                     mkexpr(math_PMULDQ_128(sHi, dHi)),
                     mkexpr(math_PMULDQ_128(sLo, dLo))));
   return res;
}


static IRTemp math_PMADDWD_128 ( IRTemp dV, IRTemp sV )
{
   IRTemp sVhi, sVlo, dVhi, dVlo;
   IRTemp resHi = newTemp(Ity_I64);
   IRTemp resLo = newTemp(Ity_I64);
   sVhi = sVlo = dVhi = dVlo = IRTemp_INVALID;
   breakupV128to64s( sV, &sVhi, &sVlo );
   breakupV128to64s( dV, &dVhi, &dVlo );
   assign( resHi, mkIRExprCCall(Ity_I64, 0,
                                "amd64g_calculate_mmx_pmaddwd", 
                                &amd64g_calculate_mmx_pmaddwd,
                                mkIRExprVec_2( mkexpr(sVhi), mkexpr(dVhi))));
   assign( resLo, mkIRExprCCall(Ity_I64, 0,
                                "amd64g_calculate_mmx_pmaddwd", 
                                &amd64g_calculate_mmx_pmaddwd,
                                mkIRExprVec_2( mkexpr(sVlo), mkexpr(dVlo))));
   IRTemp res = newTemp(Ity_V128);
   assign( res, binop(Iop_64HLtoV128, mkexpr(resHi), mkexpr(resLo))) ;
   return res;
}


static IRTemp math_PMADDWD_256 ( IRTemp dV, IRTemp sV )
{
   IRTemp sHi, sLo, dHi, dLo;
   sHi = sLo = dHi = dLo = IRTemp_INVALID;
   breakupV256toV128s( dV, &dHi, &dLo);
   breakupV256toV128s( sV, &sHi, &sLo);
   IRTemp res = newTemp(Ity_V256);
   assign(res, binop(Iop_V128HLtoV256,
                     mkexpr(math_PMADDWD_128(dHi, sHi)),
                     mkexpr(math_PMADDWD_128(dLo, sLo))));
   return res;
}


static IRTemp math_ADDSUBPD_128 ( IRTemp dV, IRTemp sV )
{
   IRTemp addV = newTemp(Ity_V128);
   IRTemp subV = newTemp(Ity_V128);
   IRTemp a1   = newTemp(Ity_I64);
   IRTemp s0   = newTemp(Ity_I64);
   IRTemp rm   = newTemp(Ity_I32);

   assign( rm, get_FAKE_roundingmode() ); 
   assign( addV, triop(Iop_Add64Fx2, mkexpr(rm), mkexpr(dV), mkexpr(sV)) );
   assign( subV, triop(Iop_Sub64Fx2, mkexpr(rm), mkexpr(dV), mkexpr(sV)) );

   assign( a1, unop(Iop_V128HIto64, mkexpr(addV) ));
   assign( s0, unop(Iop_V128to64,   mkexpr(subV) ));

   IRTemp res = newTemp(Ity_V128);
   assign( res, binop(Iop_64HLtoV128, mkexpr(a1), mkexpr(s0)) );
   return res;
}


static IRTemp math_ADDSUBPD_256 ( IRTemp dV, IRTemp sV )
{
   IRTemp a3, a2, a1, a0, s3, s2, s1, s0;
   IRTemp addV = newTemp(Ity_V256);
   IRTemp subV = newTemp(Ity_V256);
   IRTemp rm   = newTemp(Ity_I32);
   a3 = a2 = a1 = a0 = s3 = s2 = s1 = s0 = IRTemp_INVALID;

   assign( rm, get_FAKE_roundingmode() ); 
   assign( addV, triop(Iop_Add64Fx4, mkexpr(rm), mkexpr(dV), mkexpr(sV)) );
   assign( subV, triop(Iop_Sub64Fx4, mkexpr(rm), mkexpr(dV), mkexpr(sV)) );

   breakupV256to64s( addV, &a3, &a2, &a1, &a0 );
   breakupV256to64s( subV, &s3, &s2, &s1, &s0 );

   IRTemp res = newTemp(Ity_V256);
   assign( res, mkV256from64s( a3, s2, a1, s0 ) );
   return res;
}


static IRTemp math_ADDSUBPS_128 ( IRTemp dV, IRTemp sV )
{
   IRTemp a3, a2, a1, a0, s3, s2, s1, s0;
   IRTemp addV = newTemp(Ity_V128);
   IRTemp subV = newTemp(Ity_V128);
   IRTemp rm   = newTemp(Ity_I32);
   a3 = a2 = a1 = a0 = s3 = s2 = s1 = s0 = IRTemp_INVALID;

   assign( rm, get_FAKE_roundingmode() ); 
   assign( addV, triop(Iop_Add32Fx4, mkexpr(rm), mkexpr(dV), mkexpr(sV)) );
   assign( subV, triop(Iop_Sub32Fx4, mkexpr(rm), mkexpr(dV), mkexpr(sV)) );

   breakupV128to32s( addV, &a3, &a2, &a1, &a0 );
   breakupV128to32s( subV, &s3, &s2, &s1, &s0 );

   IRTemp res = newTemp(Ity_V128);
   assign( res, mkV128from32s( a3, s2, a1, s0 ) );
   return res;
}


static IRTemp math_ADDSUBPS_256 ( IRTemp dV, IRTemp sV )
{
   IRTemp a7, a6, a5, a4, a3, a2, a1, a0;
   IRTemp s7, s6, s5, s4, s3, s2, s1, s0;
   IRTemp addV = newTemp(Ity_V256);
   IRTemp subV = newTemp(Ity_V256);
   IRTemp rm   = newTemp(Ity_I32);
   a7 = a6 = a5 = a4 = a3 = a2 = a1 = a0 = IRTemp_INVALID;
   s7 = s6 = s5 = s4 = s3 = s2 = s1 = s0 = IRTemp_INVALID;

   assign( rm, get_FAKE_roundingmode() ); 
   assign( addV, triop(Iop_Add32Fx8, mkexpr(rm), mkexpr(dV), mkexpr(sV)) );
   assign( subV, triop(Iop_Sub32Fx8, mkexpr(rm), mkexpr(dV), mkexpr(sV)) );

   breakupV256to32s( addV, &a7, &a6, &a5, &a4, &a3, &a2, &a1, &a0 );
   breakupV256to32s( subV, &s7, &s6, &s5, &s4, &s3, &s2, &s1, &s0 );

   IRTemp res = newTemp(Ity_V256);
   assign( res, mkV256from32s( a7, s6, a5, s4, a3, s2, a1, s0 ) );
   return res;
}


static Long dis_PSHUFxW_128 ( const VexAbiInfo* vbi, Prefix pfx,
                              Long delta, Bool isAvx, Bool xIsH )
{
   IRTemp addr  = IRTemp_INVALID;
   Int    alen  = 0;
   HChar  dis_buf[50];
   UChar  modrm = getUChar(delta);
   UInt   rG = gregOfRexRM(pfx,modrm);
   UInt   imm8;
   IRTemp sVmut, dVmut, sVcon, sV, dV, s3, s2, s1, s0;
   s3 = s2 = s1 = s0 = IRTemp_INVALID;
   sV    = newTemp(Ity_V128);
   dV    = newTemp(Ity_V128);
   sVmut = newTemp(Ity_I64);
   dVmut = newTemp(Ity_I64);
   sVcon = newTemp(Ity_I64);
   if (epartIsReg(modrm)) {
      UInt rE = eregOfRexRM(pfx,modrm);
      assign( sV, getXMMReg(rE) );
      imm8 = (UInt)getUChar(delta+1);
      delta += 1+1;
      DIP("%spshuf%cw $%u,%s,%s\n",
          isAvx ? "v" : "", xIsH ? 'h' : 'l',
          imm8, nameXMMReg(rE), nameXMMReg(rG));
   } else {
      addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 1 );
      assign( sV, loadLE(Ity_V128, mkexpr(addr)) );
      imm8 = (UInt)getUChar(delta+alen);
      delta += alen+1;
      DIP("%spshuf%cw $%u,%s,%s\n",
          isAvx ? "v" : "", xIsH ? 'h' : 'l',
          imm8, dis_buf, nameXMMReg(rG));
   }

   assign( sVmut, unop(xIsH ? Iop_V128HIto64 : Iop_V128to64,   mkexpr(sV)) );
   assign( sVcon, unop(xIsH ? Iop_V128to64   : Iop_V128HIto64, mkexpr(sV)) );

   breakup64to16s( sVmut, &s3, &s2, &s1, &s0 );
#  define SEL(n) \
             ((n)==0 ? s0 : ((n)==1 ? s1 : ((n)==2 ? s2 : s3)))
   assign(dVmut, mk64from16s( SEL((imm8>>6)&3), SEL((imm8>>4)&3),
                              SEL((imm8>>2)&3), SEL((imm8>>0)&3) ));
#  undef SEL

   assign(dV, xIsH ? binop(Iop_64HLtoV128, mkexpr(dVmut), mkexpr(sVcon))
                   : binop(Iop_64HLtoV128, mkexpr(sVcon), mkexpr(dVmut)) );

   (isAvx ? putYMMRegLoAndZU : putXMMReg)(rG, mkexpr(dV));
   return delta;
}


static Long dis_PSHUFxW_256 ( const VexAbiInfo* vbi, Prefix pfx,
                              Long delta, Bool xIsH )
{
   IRTemp addr  = IRTemp_INVALID;
   Int    alen  = 0;
   HChar  dis_buf[50];
   UChar  modrm = getUChar(delta);
   UInt   rG = gregOfRexRM(pfx,modrm);
   UInt   imm8;
   IRTemp sV, s[8], sV64[4], dVhi, dVlo;
   sV64[3] = sV64[2] = sV64[1] = sV64[0] = IRTemp_INVALID;
   s[7] = s[6] = s[5] = s[4] = s[3] = s[2] = s[1] = s[0] = IRTemp_INVALID;
   sV    = newTemp(Ity_V256);
   dVhi  = newTemp(Ity_I64);
   dVlo  = newTemp(Ity_I64);
   if (epartIsReg(modrm)) {
      UInt rE = eregOfRexRM(pfx,modrm);
      assign( sV, getYMMReg(rE) );
      imm8 = (UInt)getUChar(delta+1);
      delta += 1+1;
      DIP("vpshuf%cw $%u,%s,%s\n", xIsH ? 'h' : 'l',
          imm8, nameYMMReg(rE), nameYMMReg(rG));
   } else {
      addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 1 );
      assign( sV, loadLE(Ity_V256, mkexpr(addr)) );
      imm8 = (UInt)getUChar(delta+alen);
      delta += alen+1;
      DIP("vpshuf%cw $%u,%s,%s\n", xIsH ? 'h' : 'l',
          imm8, dis_buf, nameYMMReg(rG));
   }

   breakupV256to64s( sV, &sV64[3], &sV64[2], &sV64[1], &sV64[0] );
   breakup64to16s( sV64[xIsH ? 3 : 2], &s[7], &s[6], &s[5], &s[4] );
   breakup64to16s( sV64[xIsH ? 1 : 0], &s[3], &s[2], &s[1], &s[0] );

   assign( dVhi, mk64from16s( s[4 + ((imm8>>6)&3)], s[4 + ((imm8>>4)&3)],
                              s[4 + ((imm8>>2)&3)], s[4 + ((imm8>>0)&3)] ) );
   assign( dVlo, mk64from16s( s[0 + ((imm8>>6)&3)], s[0 + ((imm8>>4)&3)],
                              s[0 + ((imm8>>2)&3)], s[0 + ((imm8>>0)&3)] ) );
   putYMMReg( rG, mkV256from64s( xIsH ? dVhi : sV64[3],
                                 xIsH ? sV64[2] : dVhi,
                                 xIsH ? dVlo : sV64[1],
                                 xIsH ? sV64[0] : dVlo ) );
   return delta;
}


static Long dis_PEXTRW_128_EregOnly_toG ( const VexAbiInfo* vbi, Prefix pfx,
                                          Long delta, Bool isAvx )
{
   Long   deltaIN = delta;
   UChar  modrm   = getUChar(delta);
   UInt   rG      = gregOfRexRM(pfx,modrm);
   IRTemp sV      = newTemp(Ity_V128);
   IRTemp d16     = newTemp(Ity_I16);
   UInt   imm8;
   IRTemp s0, s1, s2, s3;
   if (epartIsReg(modrm)) {
      UInt rE = eregOfRexRM(pfx,modrm);
      assign(sV, getXMMReg(rE));
      imm8 = getUChar(delta+1) & 7;
      delta += 1+1;
      DIP("%spextrw $%d,%s,%s\n", isAvx ? "v" : "",
          (Int)imm8, nameXMMReg(rE), nameIReg32(rG));
   } else {
      
      return deltaIN; 
   }
   s3 = s2 = s1 = s0 = IRTemp_INVALID;
   breakupV128to32s( sV, &s3, &s2, &s1, &s0 );
   switch (imm8) {
      case 0:  assign(d16, unop(Iop_32to16,   mkexpr(s0))); break;
      case 1:  assign(d16, unop(Iop_32HIto16, mkexpr(s0))); break;
      case 2:  assign(d16, unop(Iop_32to16,   mkexpr(s1))); break;
      case 3:  assign(d16, unop(Iop_32HIto16, mkexpr(s1))); break;
      case 4:  assign(d16, unop(Iop_32to16,   mkexpr(s2))); break;
      case 5:  assign(d16, unop(Iop_32HIto16, mkexpr(s2))); break;
      case 6:  assign(d16, unop(Iop_32to16,   mkexpr(s3))); break;
      case 7:  assign(d16, unop(Iop_32HIto16, mkexpr(s3))); break;
      default: vassert(0);
   }
   putIReg32(rG, unop(Iop_16Uto32, mkexpr(d16)));
   return delta;
}
 

static Long dis_CVTDQ2PD_128 ( const VexAbiInfo* vbi, Prefix pfx,
                               Long delta, Bool isAvx )
{
   IRTemp addr  = IRTemp_INVALID;
   Int    alen  = 0;
   HChar  dis_buf[50];
   UChar  modrm = getUChar(delta);
   IRTemp arg64 = newTemp(Ity_I64);
   UInt   rG    = gregOfRexRM(pfx,modrm);
   const HChar* mbV   = isAvx ? "v" : "";
   if (epartIsReg(modrm)) {
      UInt rE = eregOfRexRM(pfx,modrm);
      assign( arg64, getXMMRegLane64(rE, 0) );
      delta += 1;
      DIP("%scvtdq2pd %s,%s\n", mbV, nameXMMReg(rE), nameXMMReg(rG));
   } else {
      addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
      assign( arg64, loadLE(Ity_I64, mkexpr(addr)) );
      delta += alen;
      DIP("%scvtdq2pd %s,%s\n", mbV, dis_buf, nameXMMReg(rG) );
   }
   putXMMRegLane64F( 
      rG, 0,
      unop(Iop_I32StoF64, unop(Iop_64to32, mkexpr(arg64)))
   );
   putXMMRegLane64F(
      rG, 1, 
      unop(Iop_I32StoF64, unop(Iop_64HIto32, mkexpr(arg64)))
   );
   if (isAvx)
      putYMMRegLane128(rG, 1, mkV128(0));
   return delta;
}


static Long dis_STMXCSR ( const VexAbiInfo* vbi, Prefix pfx,
                          Long delta, Bool isAvx )
{
   IRTemp addr  = IRTemp_INVALID;
   Int    alen  = 0;
   HChar  dis_buf[50];
   UChar  modrm = getUChar(delta);
   vassert(!epartIsReg(modrm)); 
   vassert(gregOfRexRM(pfx,modrm) == 3); 

   addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
   delta += alen;

   
   DIP("%sstmxcsr %s\n",  isAvx ? "v" : "", dis_buf);
   storeLE( 
      mkexpr(addr), 
      unop(Iop_64to32,      
           mkIRExprCCall(
              Ity_I64, 0,
              "amd64g_create_mxcsr", &amd64g_create_mxcsr, 
              mkIRExprVec_1( unop(Iop_32Uto64,get_sse_roundingmode()) ) 
           ) 
      )
   );
   return delta;
}


static Long dis_LDMXCSR ( const VexAbiInfo* vbi, Prefix pfx,
                          Long delta, Bool isAvx )
{
   IRTemp addr  = IRTemp_INVALID;
   Int    alen  = 0;
   HChar  dis_buf[50];
   UChar  modrm = getUChar(delta);
   vassert(!epartIsReg(modrm)); 
   vassert(gregOfRexRM(pfx,modrm) == 2); 

   IRTemp t64 = newTemp(Ity_I64);
   IRTemp ew  = newTemp(Ity_I32);

   addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
   delta += alen;
   DIP("%sldmxcsr %s\n",  isAvx ? "v" : "", dis_buf);

   
   assign( t64, mkIRExprCCall(
                   Ity_I64, 0, 
                   "amd64g_check_ldmxcsr",
                   &amd64g_check_ldmxcsr, 
                   mkIRExprVec_1( 
                      unop(Iop_32Uto64,
                           loadLE(Ity_I32, mkexpr(addr))
                      )
                   )
                )
         );

   put_sse_roundingmode( unop(Iop_64to32, mkexpr(t64)) );
   assign( ew, unop(Iop_64HIto32, mkexpr(t64) ) );
   put_emwarn( mkexpr(ew) );
   stmt( 
      IRStmt_Exit(
         binop(Iop_CmpNE64, unop(Iop_32Uto64,mkexpr(ew)), mkU64(0)),
         Ijk_EmWarn,
         IRConst_U64(guest_RIP_bbstart+delta),
         OFFB_RIP
      )
   );
   return delta;
}


static IRTemp math_PINSRW_128 ( IRTemp v128, IRTemp u16, UInt imm8 )
{
   vassert(imm8 >= 0 && imm8 <= 7);

   
   
   IRTemp tmp128    = newTemp(Ity_V128);
   IRTemp halfshift = newTemp(Ity_I64);
   assign(halfshift, binop(Iop_Shl64,
                           unop(Iop_16Uto64, mkexpr(u16)),
                           mkU8(16 * (imm8 & 3))));
   if (imm8 < 4) {
      assign(tmp128, binop(Iop_64HLtoV128, mkU64(0), mkexpr(halfshift)));
   } else {
      assign(tmp128, binop(Iop_64HLtoV128, mkexpr(halfshift), mkU64(0)));
   }

   UShort mask = ~(3 << (imm8 * 2));
   IRTemp res  = newTemp(Ity_V128);
   assign( res, binop(Iop_OrV128,
                      mkexpr(tmp128),
                      binop(Iop_AndV128, mkexpr(v128), mkV128(mask))) );
   return res;
}


static IRTemp math_PSADBW_128 ( IRTemp dV, IRTemp sV )
{
   IRTemp s1, s0, d1, d0;
   s1 = s0 = d1 = d0 = IRTemp_INVALID;

   breakupV128to64s( sV, &s1, &s0 );
   breakupV128to64s( dV, &d1, &d0 );
   
   IRTemp res = newTemp(Ity_V128);
   assign( res,
           binop(Iop_64HLtoV128,
                 mkIRExprCCall(Ity_I64, 0,
                               "amd64g_calculate_mmx_psadbw", 
                               &amd64g_calculate_mmx_psadbw,
                               mkIRExprVec_2( mkexpr(s1), mkexpr(d1))),
                 mkIRExprCCall(Ity_I64, 0,
                               "amd64g_calculate_mmx_psadbw", 
                               &amd64g_calculate_mmx_psadbw,
                               mkIRExprVec_2( mkexpr(s0), mkexpr(d0)))) );
   return res;
}


static IRTemp math_PSADBW_256 ( IRTemp dV, IRTemp sV )
{
   IRTemp sHi, sLo, dHi, dLo;
   sHi = sLo = dHi = dLo = IRTemp_INVALID;
   breakupV256toV128s( dV, &dHi, &dLo);
   breakupV256toV128s( sV, &sHi, &sLo);
   IRTemp res = newTemp(Ity_V256);
   assign(res, binop(Iop_V128HLtoV256,
                     mkexpr(math_PSADBW_128(dHi, sHi)),
                     mkexpr(math_PSADBW_128(dLo, sLo))));
   return res;
}


static Long dis_MASKMOVDQU ( const VexAbiInfo* vbi, Prefix pfx,
                             Long delta, Bool isAvx )
{
   IRTemp regD    = newTemp(Ity_V128);
   IRTemp mask    = newTemp(Ity_V128);
   IRTemp olddata = newTemp(Ity_V128);
   IRTemp newdata = newTemp(Ity_V128);
   IRTemp addr    = newTemp(Ity_I64);
   UChar  modrm   = getUChar(delta);
   UInt   rG      = gregOfRexRM(pfx,modrm);
   UInt   rE      = eregOfRexRM(pfx,modrm);

   assign( addr, handleAddrOverrides( vbi, pfx, getIReg64(R_RDI) ));
   assign( regD, getXMMReg( rG ));

   assign( mask, 
           binop(Iop_64HLtoV128,
                 binop(Iop_SarN8x8, 
                       getXMMRegLane64( eregOfRexRM(pfx,modrm), 1 ), 
                       mkU8(7) ),
                 binop(Iop_SarN8x8, 
                       getXMMRegLane64( eregOfRexRM(pfx,modrm), 0 ), 
                       mkU8(7) ) ));
   assign( olddata, loadLE( Ity_V128, mkexpr(addr) ));
   assign( newdata, binop(Iop_OrV128, 
                          binop(Iop_AndV128, 
                                mkexpr(regD), 
                                mkexpr(mask) ),
                          binop(Iop_AndV128, 
                                mkexpr(olddata),
                                unop(Iop_NotV128, mkexpr(mask)))) );
   storeLE( mkexpr(addr), mkexpr(newdata) );

   delta += 1;
   DIP("%smaskmovdqu %s,%s\n", isAvx ? "v" : "",
       nameXMMReg(rE), nameXMMReg(rG) );
   return delta;
}


static Long dis_MOVMSKPS_128 ( const VexAbiInfo* vbi, Prefix pfx,
                               Long delta, Bool isAvx )
{
   UChar modrm = getUChar(delta);
   UInt   rG   = gregOfRexRM(pfx,modrm);
   UInt   rE   = eregOfRexRM(pfx,modrm);
   IRTemp t0   = newTemp(Ity_I32);
   IRTemp t1   = newTemp(Ity_I32);
   IRTemp t2   = newTemp(Ity_I32);
   IRTemp t3   = newTemp(Ity_I32);
   delta += 1;
   assign( t0, binop( Iop_And32,
                      binop(Iop_Shr32, getXMMRegLane32(rE,0), mkU8(31)),
                      mkU32(1) ));
   assign( t1, binop( Iop_And32,
                      binop(Iop_Shr32, getXMMRegLane32(rE,1), mkU8(30)),
                      mkU32(2) ));
   assign( t2, binop( Iop_And32,
                      binop(Iop_Shr32, getXMMRegLane32(rE,2), mkU8(29)),
                      mkU32(4) ));
   assign( t3, binop( Iop_And32,
                      binop(Iop_Shr32, getXMMRegLane32(rE,3), mkU8(28)),
                      mkU32(8) ));
   putIReg32( rG, binop(Iop_Or32,
                        binop(Iop_Or32, mkexpr(t0), mkexpr(t1)),
                        binop(Iop_Or32, mkexpr(t2), mkexpr(t3)) ) );
   DIP("%smovmskps %s,%s\n", isAvx ? "v" : "",
       nameXMMReg(rE), nameIReg32(rG));
   return delta;
}


static Long dis_MOVMSKPS_256 ( const VexAbiInfo* vbi, Prefix pfx, Long delta )
{
   UChar modrm = getUChar(delta);
   UInt   rG   = gregOfRexRM(pfx,modrm);
   UInt   rE   = eregOfRexRM(pfx,modrm);
   IRTemp t0   = newTemp(Ity_I32);
   IRTemp t1   = newTemp(Ity_I32);
   IRTemp t2   = newTemp(Ity_I32);
   IRTemp t3   = newTemp(Ity_I32);
   IRTemp t4   = newTemp(Ity_I32);
   IRTemp t5   = newTemp(Ity_I32);
   IRTemp t6   = newTemp(Ity_I32);
   IRTemp t7   = newTemp(Ity_I32);
   delta += 1;
   assign( t0, binop( Iop_And32,
                      binop(Iop_Shr32, getYMMRegLane32(rE,0), mkU8(31)),
                      mkU32(1) ));
   assign( t1, binop( Iop_And32,
                      binop(Iop_Shr32, getYMMRegLane32(rE,1), mkU8(30)),
                      mkU32(2) ));
   assign( t2, binop( Iop_And32,
                      binop(Iop_Shr32, getYMMRegLane32(rE,2), mkU8(29)),
                      mkU32(4) ));
   assign( t3, binop( Iop_And32,
                      binop(Iop_Shr32, getYMMRegLane32(rE,3), mkU8(28)),
                      mkU32(8) ));
   assign( t4, binop( Iop_And32,
                      binop(Iop_Shr32, getYMMRegLane32(rE,4), mkU8(27)),
                      mkU32(16) ));
   assign( t5, binop( Iop_And32,
                      binop(Iop_Shr32, getYMMRegLane32(rE,5), mkU8(26)),
                      mkU32(32) ));
   assign( t6, binop( Iop_And32,
                      binop(Iop_Shr32, getYMMRegLane32(rE,6), mkU8(25)),
                      mkU32(64) ));
   assign( t7, binop( Iop_And32,
                      binop(Iop_Shr32, getYMMRegLane32(rE,7), mkU8(24)),
                      mkU32(128) ));
   putIReg32( rG, binop(Iop_Or32,
                        binop(Iop_Or32,
                              binop(Iop_Or32, mkexpr(t0), mkexpr(t1)),
                              binop(Iop_Or32, mkexpr(t2), mkexpr(t3)) ),
                        binop(Iop_Or32,
                              binop(Iop_Or32, mkexpr(t4), mkexpr(t5)),
                              binop(Iop_Or32, mkexpr(t6), mkexpr(t7)) ) ) );
   DIP("vmovmskps %s,%s\n", nameYMMReg(rE), nameIReg32(rG));
   return delta;
}


static Long dis_MOVMSKPD_128 ( const VexAbiInfo* vbi, Prefix pfx,
                               Long delta, Bool isAvx )
{
   UChar modrm = getUChar(delta);
   UInt   rG   = gregOfRexRM(pfx,modrm);
   UInt   rE   = eregOfRexRM(pfx,modrm);
   IRTemp t0   = newTemp(Ity_I32);
   IRTemp t1   = newTemp(Ity_I32);
   delta += 1;
   assign( t0, binop( Iop_And32,
                      binop(Iop_Shr32, getXMMRegLane32(rE,1), mkU8(31)),
                      mkU32(1) ));
   assign( t1, binop( Iop_And32,
                      binop(Iop_Shr32, getXMMRegLane32(rE,3), mkU8(30)),
                      mkU32(2) ));
   putIReg32( rG, binop(Iop_Or32, mkexpr(t0), mkexpr(t1) ) );
   DIP("%smovmskpd %s,%s\n", isAvx ? "v" : "",
       nameXMMReg(rE), nameIReg32(rG));
   return delta;
}


static Long dis_MOVMSKPD_256 ( const VexAbiInfo* vbi, Prefix pfx, Long delta )
{
   UChar modrm = getUChar(delta);
   UInt   rG   = gregOfRexRM(pfx,modrm);
   UInt   rE   = eregOfRexRM(pfx,modrm);
   IRTemp t0   = newTemp(Ity_I32);
   IRTemp t1   = newTemp(Ity_I32);
   IRTemp t2   = newTemp(Ity_I32);
   IRTemp t3   = newTemp(Ity_I32);
   delta += 1;
   assign( t0, binop( Iop_And32,
                      binop(Iop_Shr32, getYMMRegLane32(rE,1), mkU8(31)),
                      mkU32(1) ));
   assign( t1, binop( Iop_And32,
                      binop(Iop_Shr32, getYMMRegLane32(rE,3), mkU8(30)),
                      mkU32(2) ));
   assign( t2, binop( Iop_And32,
                      binop(Iop_Shr32, getYMMRegLane32(rE,5), mkU8(29)),
                      mkU32(4) ));
   assign( t3, binop( Iop_And32,
                      binop(Iop_Shr32, getYMMRegLane32(rE,7), mkU8(28)),
                      mkU32(8) ));
   putIReg32( rG, binop(Iop_Or32,
                        binop(Iop_Or32, mkexpr(t0), mkexpr(t1)),
                        binop(Iop_Or32, mkexpr(t2), mkexpr(t3)) ) );
   DIP("vmovmskps %s,%s\n", nameYMMReg(rE), nameIReg32(rG));
   return delta;
}


__attribute__((noinline))
static
Long dis_ESC_0F__SSE2 ( Bool* decode_OK,
                        const VexAbiInfo* vbi,
                        Prefix pfx, Int sz, Long deltaIN,
                        DisResult* dres )
{
   IRTemp addr  = IRTemp_INVALID;
   IRTemp t0    = IRTemp_INVALID;
   IRTemp t1    = IRTemp_INVALID;
   IRTemp t2    = IRTemp_INVALID;
   IRTemp t3    = IRTemp_INVALID;
   IRTemp t4    = IRTemp_INVALID;
   IRTemp t5    = IRTemp_INVALID;
   IRTemp t6    = IRTemp_INVALID;
   UChar  modrm = 0;
   Int    alen  = 0;
   HChar  dis_buf[50];

   *decode_OK = False;

   Long   delta = deltaIN;
   UChar  opc   = getUChar(delta);
   delta++;
   switch (opc) {

   case 0x10:
      if (have66noF2noF3(pfx) 
          && (sz == 2 ||  sz == 8)) {
         
         modrm = getUChar(delta);
         if (epartIsReg(modrm)) {
            putXMMReg( gregOfRexRM(pfx,modrm), 
                       getXMMReg( eregOfRexRM(pfx,modrm) ));
            DIP("movupd %s,%s\n", nameXMMReg(eregOfRexRM(pfx,modrm)),
                                  nameXMMReg(gregOfRexRM(pfx,modrm)));
            delta += 1;
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            putXMMReg( gregOfRexRM(pfx,modrm), 
                       loadLE(Ity_V128, mkexpr(addr)) );
            DIP("movupd %s,%s\n", dis_buf,
                                  nameXMMReg(gregOfRexRM(pfx,modrm)));
            delta += alen;
         }
         goto decode_success;
      }
      if (haveF2no66noF3(pfx) 
          && (sz == 4 ||  sz == 8) ) {
         modrm = getUChar(delta);
         if (epartIsReg(modrm)) {
            putXMMRegLane64( gregOfRexRM(pfx,modrm), 0,
                             getXMMRegLane64( eregOfRexRM(pfx,modrm), 0 ));
            DIP("movsd %s,%s\n", nameXMMReg(eregOfRexRM(pfx,modrm)),
                                 nameXMMReg(gregOfRexRM(pfx,modrm)));
            delta += 1;
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            putXMMReg( gregOfRexRM(pfx,modrm), mkV128(0) );
            putXMMRegLane64( gregOfRexRM(pfx,modrm), 0,
                             loadLE(Ity_I64, mkexpr(addr)) );
            DIP("movsd %s,%s\n", dis_buf,
                                 nameXMMReg(gregOfRexRM(pfx,modrm)));
            delta += alen;
         }
         goto decode_success;
      }
      if (haveF3no66noF2(pfx) 
          && (sz == 4 ||  sz == 8)) {
         modrm = getUChar(delta);
         if (epartIsReg(modrm)) {
            putXMMRegLane32( gregOfRexRM(pfx,modrm), 0,
                             getXMMRegLane32( eregOfRexRM(pfx,modrm), 0 ));
            DIP("movss %s,%s\n", nameXMMReg(eregOfRexRM(pfx,modrm)),
                                 nameXMMReg(gregOfRexRM(pfx,modrm)));
            delta += 1;
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            putXMMReg( gregOfRexRM(pfx,modrm), mkV128(0) );
            putXMMRegLane32( gregOfRexRM(pfx,modrm), 0,
                             loadLE(Ity_I32, mkexpr(addr)) );
            DIP("movss %s,%s\n", dis_buf,
                                 nameXMMReg(gregOfRexRM(pfx,modrm)));
            delta += alen;
         }
         goto decode_success;
      }
      
      if (haveNo66noF2noF3(pfx) 
          && (sz == 4 ||  sz == 8)) {
         modrm = getUChar(delta);
         if (epartIsReg(modrm)) {
            putXMMReg( gregOfRexRM(pfx,modrm), 
                       getXMMReg( eregOfRexRM(pfx,modrm) ));
            DIP("movups %s,%s\n", nameXMMReg(eregOfRexRM(pfx,modrm)),
                                  nameXMMReg(gregOfRexRM(pfx,modrm)));
            delta += 1;
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            putXMMReg( gregOfRexRM(pfx,modrm), 
                       loadLE(Ity_V128, mkexpr(addr)) );
            DIP("movups %s,%s\n", dis_buf,
                                     nameXMMReg(gregOfRexRM(pfx,modrm)));
            delta += alen;
         }
         goto decode_success;
      }
      break;

   case 0x11:
      if (haveF2no66noF3(pfx) 
          && (sz == 4 ||  sz == 8)) {
         modrm = getUChar(delta);
         if (epartIsReg(modrm)) {
            putXMMRegLane64( eregOfRexRM(pfx,modrm), 0,
                             getXMMRegLane64( gregOfRexRM(pfx,modrm), 0 ));
            DIP("movsd %s,%s\n", nameXMMReg(gregOfRexRM(pfx,modrm)),
                                 nameXMMReg(eregOfRexRM(pfx,modrm)));
            delta += 1;
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            storeLE( mkexpr(addr),
                     getXMMRegLane64(gregOfRexRM(pfx,modrm), 0) );
            DIP("movsd %s,%s\n", nameXMMReg(gregOfRexRM(pfx,modrm)),
                                 dis_buf);
            delta += alen;
         }
         goto decode_success;
      }
      if (haveF3no66noF2(pfx) && sz == 4) {
         modrm = getUChar(delta);
         if (epartIsReg(modrm)) {
            
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            storeLE( mkexpr(addr),
                     getXMMRegLane32(gregOfRexRM(pfx,modrm), 0) );
            DIP("movss %s,%s\n", nameXMMReg(gregOfRexRM(pfx,modrm)),
                                 dis_buf);
            delta += alen;
            goto decode_success;
         }
      }
      
      if (have66noF2noF3(pfx)
          && (sz == 2 ||  sz == 8)) {
         modrm = getUChar(delta);
         if (epartIsReg(modrm)) {
            putXMMReg( eregOfRexRM(pfx,modrm),
                       getXMMReg( gregOfRexRM(pfx,modrm) ) );
            DIP("movupd %s,%s\n", nameXMMReg(gregOfRexRM(pfx,modrm)),
                                  nameXMMReg(eregOfRexRM(pfx,modrm)));
            delta += 1;
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            storeLE( mkexpr(addr), getXMMReg(gregOfRexRM(pfx,modrm)) );
            DIP("movupd %s,%s\n", nameXMMReg(gregOfRexRM(pfx,modrm)),
                                  dis_buf );
            delta += alen;
         }
         goto decode_success;
      }
      
      if (haveNo66noF2noF3(pfx)
          && (sz == 4 ||  sz == 8)) {
         modrm = getUChar(delta);
         if (epartIsReg(modrm)) {
            
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            storeLE( mkexpr(addr), getXMMReg(gregOfRexRM(pfx,modrm)) );
            DIP("movups %s,%s\n", nameXMMReg(gregOfRexRM(pfx,modrm)),
                                  dis_buf );
            delta += alen;
            goto decode_success;
         }
      }
      break;

   case 0x12:
      
      
      if (have66noF2noF3(pfx)
          && (sz == 2 ||  sz == 8)) {
         modrm = getUChar(delta);
         if (epartIsReg(modrm)) {
            
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            delta += alen;
            putXMMRegLane64( gregOfRexRM(pfx,modrm),
                             0,
                             loadLE(Ity_I64, mkexpr(addr)) );
            DIP("movlpd %s, %s\n", 
                dis_buf, nameXMMReg( gregOfRexRM(pfx,modrm) ));
            goto decode_success;
         }
      }
      
      
      if (haveNo66noF2noF3(pfx)
          && (sz == 4 ||  sz == 8)) {
         modrm = getUChar(delta);
         if (epartIsReg(modrm)) {
            delta += 1;
            putXMMRegLane64( gregOfRexRM(pfx,modrm),  
                             0,
                             getXMMRegLane64( eregOfRexRM(pfx,modrm), 1 ));
            DIP("movhlps %s, %s\n", nameXMMReg(eregOfRexRM(pfx,modrm)), 
                                    nameXMMReg(gregOfRexRM(pfx,modrm)));
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            delta += alen;
            putXMMRegLane64( gregOfRexRM(pfx,modrm),  0,
                             loadLE(Ity_I64, mkexpr(addr)) );
            DIP("movlps %s, %s\n", 
                dis_buf, nameXMMReg( gregOfRexRM(pfx,modrm) ));
         }
         goto decode_success;
      }
      break;

   case 0x13:
      
      if (haveNo66noF2noF3(pfx)
          && (sz == 4 ||  sz == 8)) {
         modrm = getUChar(delta);
         if (!epartIsReg(modrm)) {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            delta += alen;
            storeLE( mkexpr(addr), 
                     getXMMRegLane64( gregOfRexRM(pfx,modrm), 
                                      0 ) );
            DIP("movlps %s, %s\n", nameXMMReg( gregOfRexRM(pfx,modrm) ),
                                   dis_buf);
            goto decode_success;
         }
         
      }
      
      
      if (have66noF2noF3(pfx)
          && (sz == 2 ||  sz == 8)) {
         modrm = getUChar(delta);
         if (!epartIsReg(modrm)) {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            delta += alen;
            storeLE( mkexpr(addr), 
                     getXMMRegLane64( gregOfRexRM(pfx,modrm), 
                                      0 ) );
            DIP("movlpd %s, %s\n", nameXMMReg( gregOfRexRM(pfx,modrm) ),
                                   dis_buf);
            goto decode_success;
         }
         
      }
      break;

   case 0x14:
   case 0x15:
      
      
      
      if (haveNo66noF2noF3(pfx) && sz == 4) {
         Bool   hi = toBool(opc == 0x15);
         IRTemp sV = newTemp(Ity_V128);
         IRTemp dV = newTemp(Ity_V128);
         modrm = getUChar(delta);
         UInt   rG = gregOfRexRM(pfx,modrm);
         assign( dV, getXMMReg(rG) );
         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx,modrm);
            assign( sV, getXMMReg(rE) );
            delta += 1;
            DIP("unpck%sps %s,%s\n", hi ? "h" : "l",
                nameXMMReg(rE), nameXMMReg(rG));
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            assign( sV, loadLE(Ity_V128, mkexpr(addr)) );
            delta += alen;
            DIP("unpck%sps %s,%s\n", hi ? "h" : "l",
                dis_buf, nameXMMReg(rG));
         }
         IRTemp res = math_UNPCKxPS_128( sV, dV, hi );
         putXMMReg( rG, mkexpr(res) );
         goto decode_success;
      }
      
      
      
      if (have66noF2noF3(pfx) 
          && sz == 2 ) {
         Bool   hi = toBool(opc == 0x15);
         IRTemp sV = newTemp(Ity_V128);
         IRTemp dV = newTemp(Ity_V128);
         modrm = getUChar(delta);
         UInt   rG = gregOfRexRM(pfx,modrm);
         assign( dV, getXMMReg(rG) );
         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx,modrm);
            assign( sV, getXMMReg(rE) );
            delta += 1;
            DIP("unpck%sps %s,%s\n", hi ? "h" : "l",
                nameXMMReg(rE), nameXMMReg(rG));
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            assign( sV, loadLE(Ity_V128, mkexpr(addr)) );
            delta += alen;
            DIP("unpck%sps %s,%s\n", hi ? "h" : "l",
                dis_buf, nameXMMReg(rG));
         }
         IRTemp res = math_UNPCKxPD_128( sV, dV, hi );
         putXMMReg( rG, mkexpr(res) );
         goto decode_success;
      }
      break;

   case 0x16:
      
      if (have66noF2noF3(pfx)
          && (sz == 2 ||  sz == 8)) {
         modrm = getUChar(delta);
         if (epartIsReg(modrm)) {
            
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            delta += alen;
            putXMMRegLane64( gregOfRexRM(pfx,modrm), 1,
                             loadLE(Ity_I64, mkexpr(addr)) );
            DIP("movhpd %s,%s\n", dis_buf, 
                                  nameXMMReg( gregOfRexRM(pfx,modrm) ));
            goto decode_success;
         }
      }
      
      
      if (haveNo66noF2noF3(pfx)
          && (sz == 4 ||  sz == 8)) {
         modrm = getUChar(delta);
         if (epartIsReg(modrm)) {
            delta += 1;
            putXMMRegLane64( gregOfRexRM(pfx,modrm), 1,
                             getXMMRegLane64( eregOfRexRM(pfx,modrm), 0 ) );
            DIP("movhps %s,%s\n", nameXMMReg(eregOfRexRM(pfx,modrm)), 
                                  nameXMMReg(gregOfRexRM(pfx,modrm)));
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            delta += alen;
            putXMMRegLane64( gregOfRexRM(pfx,modrm), 1,
                             loadLE(Ity_I64, mkexpr(addr)) );
            DIP("movhps %s,%s\n", dis_buf, 
                                  nameXMMReg( gregOfRexRM(pfx,modrm) ));
         }
         goto decode_success;
      }
      break;

   case 0x17:
      
      if (haveNo66noF2noF3(pfx)
          && (sz == 4 ||  sz == 8)) {
         modrm = getUChar(delta);
         if (!epartIsReg(modrm)) {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            delta += alen;
            storeLE( mkexpr(addr), 
                     getXMMRegLane64( gregOfRexRM(pfx,modrm),
                                      1 ) );
            DIP("movhps %s,%s\n", nameXMMReg( gregOfRexRM(pfx,modrm) ),
                                  dis_buf);
            goto decode_success;
         }
         
      }
      
      
      if (have66noF2noF3(pfx)
          && (sz == 2 ||  sz == 8)) {
         modrm = getUChar(delta);
         if (!epartIsReg(modrm)) {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            delta += alen;
            storeLE( mkexpr(addr), 
                     getXMMRegLane64( gregOfRexRM(pfx,modrm),
                                      1 ) );
            DIP("movhpd %s,%s\n", nameXMMReg( gregOfRexRM(pfx,modrm) ),
                                  dis_buf);
            goto decode_success;
         }
         
      }
      break;

   case 0x18:
      
      
      
      
      if (haveNo66noF2noF3(pfx)
          && !epartIsReg(getUChar(delta)) 
          && gregLO3ofRM(getUChar(delta)) >= 0
          && gregLO3ofRM(getUChar(delta)) <= 3) {
         const HChar* hintstr = "??";

         modrm = getUChar(delta);
         vassert(!epartIsReg(modrm));

         addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
         delta += alen;

         switch (gregLO3ofRM(modrm)) {
            case 0: hintstr = "nta"; break;
            case 1: hintstr = "t0"; break;
            case 2: hintstr = "t1"; break;
            case 3: hintstr = "t2"; break;
            default: vassert(0);
         }

         DIP("prefetch%s %s\n", hintstr, dis_buf);
         goto decode_success;
      }
      break;

   case 0x28:
      
      if (have66noF2noF3(pfx) 
          && (sz == 2 ||  sz == 8)) {
         modrm = getUChar(delta);
         if (epartIsReg(modrm)) {
            putXMMReg( gregOfRexRM(pfx,modrm), 
                       getXMMReg( eregOfRexRM(pfx,modrm) ));
            DIP("movapd %s,%s\n", nameXMMReg(eregOfRexRM(pfx,modrm)),
                                  nameXMMReg(gregOfRexRM(pfx,modrm)));
            delta += 1;
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            gen_SEGV_if_not_16_aligned( addr );
            putXMMReg( gregOfRexRM(pfx,modrm), 
                       loadLE(Ity_V128, mkexpr(addr)) );
            DIP("movapd %s,%s\n", dis_buf,
                                  nameXMMReg(gregOfRexRM(pfx,modrm)));
            delta += alen;
         }
         goto decode_success;
      }
      
      if (haveNo66noF2noF3(pfx) 
          && (sz == 4 ||  sz == 8)) {
         modrm = getUChar(delta);
         if (epartIsReg(modrm)) {
            putXMMReg( gregOfRexRM(pfx,modrm), 
                       getXMMReg( eregOfRexRM(pfx,modrm) ));
            DIP("movaps %s,%s\n", nameXMMReg(eregOfRexRM(pfx,modrm)),
                                  nameXMMReg(gregOfRexRM(pfx,modrm)));
            delta += 1;
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            gen_SEGV_if_not_16_aligned( addr );
            putXMMReg( gregOfRexRM(pfx,modrm), 
                       loadLE(Ity_V128, mkexpr(addr)) );
            DIP("movaps %s,%s\n", dis_buf,
                                  nameXMMReg(gregOfRexRM(pfx,modrm)));
            delta += alen;
         }
         goto decode_success;
      }
      break;

   case 0x29:
      
      if (haveNo66noF2noF3(pfx)
          && (sz == 4 ||  sz == 8)) {
         modrm = getUChar(delta);
         if (epartIsReg(modrm)) {
            putXMMReg( eregOfRexRM(pfx,modrm),
                       getXMMReg( gregOfRexRM(pfx,modrm) ));
            DIP("movaps %s,%s\n", nameXMMReg(gregOfRexRM(pfx,modrm)),
                                  nameXMMReg(eregOfRexRM(pfx,modrm)));
            delta += 1;
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            gen_SEGV_if_not_16_aligned( addr );
            storeLE( mkexpr(addr), getXMMReg(gregOfRexRM(pfx,modrm)) );
            DIP("movaps %s,%s\n", nameXMMReg(gregOfRexRM(pfx,modrm)),
                                  dis_buf );
            delta += alen;
         }
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx)
          && (sz == 2 ||  sz == 8)) {
         modrm = getUChar(delta);
         if (epartIsReg(modrm)) {
            putXMMReg( eregOfRexRM(pfx,modrm),
                       getXMMReg( gregOfRexRM(pfx,modrm) ) );
            DIP("movapd %s,%s\n", nameXMMReg(gregOfRexRM(pfx,modrm)),
                                  nameXMMReg(eregOfRexRM(pfx,modrm)));
            delta += 1;
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            gen_SEGV_if_not_16_aligned( addr );
            storeLE( mkexpr(addr), getXMMReg(gregOfRexRM(pfx,modrm)) );
            DIP("movapd %s,%s\n", nameXMMReg(gregOfRexRM(pfx,modrm)),
                                  dis_buf );
            delta += alen;
         }
         goto decode_success;
      }
      break;

   case 0x2A:
      if (haveNo66noF2noF3(pfx) && sz == 4) {
         IRTemp arg64 = newTemp(Ity_I64);
         IRTemp rmode = newTemp(Ity_I32);

         modrm = getUChar(delta);
         do_MMX_preamble();
         if (epartIsReg(modrm)) {
            assign( arg64, getMMXReg(eregLO3ofRM(modrm)) );
            delta += 1;
            DIP("cvtpi2ps %s,%s\n", nameMMXReg(eregLO3ofRM(modrm)),
                                    nameXMMReg(gregOfRexRM(pfx,modrm)));
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            assign( arg64, loadLE(Ity_I64, mkexpr(addr)) );
            delta += alen;
            DIP("cvtpi2ps %s,%s\n", dis_buf,
                                    nameXMMReg(gregOfRexRM(pfx,modrm)) );
         }

         assign( rmode, get_sse_roundingmode() );

         putXMMRegLane32F( 
            gregOfRexRM(pfx,modrm), 0,
            binop(Iop_F64toF32, 
                  mkexpr(rmode),
                  unop(Iop_I32StoF64, 
                       unop(Iop_64to32, mkexpr(arg64)) )) );

         putXMMRegLane32F(
            gregOfRexRM(pfx,modrm), 1, 
            binop(Iop_F64toF32, 
                  mkexpr(rmode),
                  unop(Iop_I32StoF64,
                       unop(Iop_64HIto32, mkexpr(arg64)) )) );

         goto decode_success;
      }
      if (haveF3no66noF2(pfx) && (sz == 4 || sz == 8)) {
         IRTemp rmode = newTemp(Ity_I32);
         assign( rmode, get_sse_roundingmode() );
         modrm = getUChar(delta);
         if (sz == 4) {
            IRTemp arg32 = newTemp(Ity_I32);
            if (epartIsReg(modrm)) {
               assign( arg32, getIReg32(eregOfRexRM(pfx,modrm)) );
               delta += 1;
               DIP("cvtsi2ss %s,%s\n", nameIReg32(eregOfRexRM(pfx,modrm)),
                                       nameXMMReg(gregOfRexRM(pfx,modrm)));
            } else {
               addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
               assign( arg32, loadLE(Ity_I32, mkexpr(addr)) );
               delta += alen;
               DIP("cvtsi2ss %s,%s\n", dis_buf,
                                       nameXMMReg(gregOfRexRM(pfx,modrm)) );
            }
            putXMMRegLane32F( 
               gregOfRexRM(pfx,modrm), 0,
               binop(Iop_F64toF32,
                     mkexpr(rmode),
                     unop(Iop_I32StoF64, mkexpr(arg32)) ) );
         } else {
            
            IRTemp arg64 = newTemp(Ity_I64);
            if (epartIsReg(modrm)) {
               assign( arg64, getIReg64(eregOfRexRM(pfx,modrm)) );
               delta += 1;
               DIP("cvtsi2ssq %s,%s\n", nameIReg64(eregOfRexRM(pfx,modrm)),
                                        nameXMMReg(gregOfRexRM(pfx,modrm)));
            } else {
               addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
               assign( arg64, loadLE(Ity_I64, mkexpr(addr)) );
               delta += alen;
               DIP("cvtsi2ssq %s,%s\n", dis_buf,
                                        nameXMMReg(gregOfRexRM(pfx,modrm)) );
            }
            putXMMRegLane32F( 
               gregOfRexRM(pfx,modrm), 0,
               binop(Iop_F64toF32,
                     mkexpr(rmode),
                     binop(Iop_I64StoF64, mkexpr(rmode), mkexpr(arg64)) ) );
         }
         goto decode_success;
      }
      if (haveF2no66noF3(pfx) && (sz == 4 || sz == 8)) {
         modrm = getUChar(delta);
         if (sz == 4) {
            IRTemp arg32 = newTemp(Ity_I32);
            if (epartIsReg(modrm)) {
               assign( arg32, getIReg32(eregOfRexRM(pfx,modrm)) );
               delta += 1;
               DIP("cvtsi2sdl %s,%s\n", nameIReg32(eregOfRexRM(pfx,modrm)),
                                        nameXMMReg(gregOfRexRM(pfx,modrm)));
            } else {
               addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
               assign( arg32, loadLE(Ity_I32, mkexpr(addr)) );
               delta += alen;
               DIP("cvtsi2sdl %s,%s\n", dis_buf,
                                        nameXMMReg(gregOfRexRM(pfx,modrm)) );
            }
            putXMMRegLane64F( gregOfRexRM(pfx,modrm), 0,
                              unop(Iop_I32StoF64, mkexpr(arg32)) 
            );
         } else {
            
            IRTemp arg64 = newTemp(Ity_I64);
            if (epartIsReg(modrm)) {
               assign( arg64, getIReg64(eregOfRexRM(pfx,modrm)) );
               delta += 1;
               DIP("cvtsi2sdq %s,%s\n", nameIReg64(eregOfRexRM(pfx,modrm)),
                                        nameXMMReg(gregOfRexRM(pfx,modrm)));
            } else {
               addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
               assign( arg64, loadLE(Ity_I64, mkexpr(addr)) );
               delta += alen;
               DIP("cvtsi2sdq %s,%s\n", dis_buf,
                                        nameXMMReg(gregOfRexRM(pfx,modrm)) );
            }
            putXMMRegLane64F( 
               gregOfRexRM(pfx,modrm), 
               0,
               binop( Iop_I64StoF64,
                      get_sse_roundingmode(),
                      mkexpr(arg64)
               ) 
            );
         }
         goto decode_success;
      }
      if (have66noF2noF3(pfx) && sz == 2) {
         IRTemp arg64 = newTemp(Ity_I64);

         modrm = getUChar(delta);
         if (epartIsReg(modrm)) {
            do_MMX_preamble();
            assign( arg64, getMMXReg(eregLO3ofRM(modrm)) );
            delta += 1;
            DIP("cvtpi2pd %s,%s\n", nameMMXReg(eregLO3ofRM(modrm)),
                                    nameXMMReg(gregOfRexRM(pfx,modrm)));
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            assign( arg64, loadLE(Ity_I64, mkexpr(addr)) );
            delta += alen;
            DIP("cvtpi2pd %s,%s\n", dis_buf,
                                    nameXMMReg(gregOfRexRM(pfx,modrm)) );
         }

         putXMMRegLane64F( 
            gregOfRexRM(pfx,modrm), 0,
            unop(Iop_I32StoF64, unop(Iop_64to32, mkexpr(arg64)) )
         );

         putXMMRegLane64F( 
            gregOfRexRM(pfx,modrm), 1,
            unop(Iop_I32StoF64, unop(Iop_64HIto32, mkexpr(arg64)) )
         );

         goto decode_success;
      }
      break;

   case 0x2B:
      
      
      if ( (haveNo66noF2noF3(pfx) && sz == 4)
           || (have66noF2noF3(pfx) && sz == 2) ) {
         modrm = getUChar(delta);
         if (!epartIsReg(modrm)) {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            gen_SEGV_if_not_16_aligned( addr );
            storeLE( mkexpr(addr), getXMMReg(gregOfRexRM(pfx,modrm)) );
            DIP("movntp%s %s,%s\n", sz==2 ? "d" : "s",
                                    dis_buf,
                                    nameXMMReg(gregOfRexRM(pfx,modrm)));
            delta += alen;
            goto decode_success;
         }
         
      }
      break;

   case 0x2C:
   case 0x2D:
      if (haveNo66noF2noF3(pfx) && sz == 4) {
         IRTemp dst64  = newTemp(Ity_I64);
         IRTemp rmode  = newTemp(Ity_I32);
         IRTemp f32lo  = newTemp(Ity_F32);
         IRTemp f32hi  = newTemp(Ity_F32);
         Bool   r2zero = toBool(opc == 0x2C);

         do_MMX_preamble();
         modrm = getUChar(delta);

         if (epartIsReg(modrm)) {
            delta += 1;
            assign(f32lo, getXMMRegLane32F(eregOfRexRM(pfx,modrm), 0));
            assign(f32hi, getXMMRegLane32F(eregOfRexRM(pfx,modrm), 1));
            DIP("cvt%sps2pi %s,%s\n", r2zero ? "t" : "",
                                      nameXMMReg(eregOfRexRM(pfx,modrm)),
                                      nameMMXReg(gregLO3ofRM(modrm)));
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            assign(f32lo, loadLE(Ity_F32, mkexpr(addr)));
            assign(f32hi, loadLE(Ity_F32, binop( Iop_Add64, 
                                                 mkexpr(addr), 
                                                 mkU64(4) )));
            delta += alen;
            DIP("cvt%sps2pi %s,%s\n", r2zero ? "t" : "",
                                      dis_buf,
                                      nameMMXReg(gregLO3ofRM(modrm)));
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

         putMMXReg(gregLO3ofRM(modrm), mkexpr(dst64));
         goto decode_success;
      }
      if (haveF3no66noF2(pfx) && (sz == 4 || sz == 8)) {
         delta = dis_CVTxSS2SI( vbi, pfx, delta, False, opc, sz);
         goto decode_success;
      }
      if (haveF2no66noF3(pfx) && (sz == 4 || sz == 8)) {
         delta = dis_CVTxSD2SI( vbi, pfx, delta, False, opc, sz);
         goto decode_success;
      }
      if (have66noF2noF3(pfx) && sz == 2) {
         IRTemp dst64  = newTemp(Ity_I64);
         IRTemp rmode  = newTemp(Ity_I32);
         IRTemp f64lo  = newTemp(Ity_F64);
         IRTemp f64hi  = newTemp(Ity_F64);
         Bool   r2zero = toBool(opc == 0x2C);

         do_MMX_preamble();
         modrm = getUChar(delta);

         if (epartIsReg(modrm)) {
            delta += 1;
            assign(f64lo, getXMMRegLane64F(eregOfRexRM(pfx,modrm), 0));
            assign(f64hi, getXMMRegLane64F(eregOfRexRM(pfx,modrm), 1));
            DIP("cvt%spd2pi %s,%s\n", r2zero ? "t" : "",
                                      nameXMMReg(eregOfRexRM(pfx,modrm)),
                                      nameMMXReg(gregLO3ofRM(modrm)));
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            assign(f64lo, loadLE(Ity_F64, mkexpr(addr)));
            assign(f64hi, loadLE(Ity_F64, binop( Iop_Add64, 
                                                 mkexpr(addr), 
                                                 mkU64(8) )));
            delta += alen;
            DIP("cvt%spf2pi %s,%s\n", r2zero ? "t" : "",
                                      dis_buf,
                                      nameMMXReg(gregLO3ofRM(modrm)));
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

         putMMXReg(gregLO3ofRM(modrm), mkexpr(dst64));
         goto decode_success;
      }
      break;

   case 0x2E:
   case 0x2F:
      
      
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_COMISD( vbi, pfx, delta, False, opc );
         goto decode_success;
      }
      
      
      if (haveNo66noF2noF3(pfx) && sz == 4) {
         delta = dis_COMISS( vbi, pfx, delta, False, opc );
         goto decode_success;
      }
      break;

   case 0x50:
      if (haveNo66noF2noF3(pfx) && (sz == 4 || sz == 8)
          && epartIsReg(getUChar(delta))) {
         /* sz == 8 is a kludge to handle insns with REX.W redundantly
            set to 1, which has been known to happen:

            4c 0f 50 d9             rex64X movmskps %xmm1,%r11d

            20071106: Intel docs say that REX.W isn't redundant: when
            present, a 64-bit register is written; when not present, only
            the 32-bit half is written.  However, testing on a Core2
            machine suggests the entire 64 bit register is written
            irrespective of the status of REX.W.  That could be because
            of the default rule that says "if the lower half of a 32-bit
            register is written, the upper half is zeroed".  By using
            putIReg32 here we inadvertantly produce the same behaviour as
            the Core2, for the same reason -- putIReg32 implements said
            rule.

            AMD docs give no indication that REX.W is even valid for this
            insn. */
         delta = dis_MOVMSKPS_128( vbi, pfx, delta, False );
         goto decode_success;
      }
      if (have66noF2noF3(pfx) && (sz == 2 || sz == 8)) {
         delta = dis_MOVMSKPD_128( vbi, pfx, delta, False );
         goto decode_success;
      }
      break;

   case 0x51:
      
      if (haveF3no66noF2(pfx) && sz == 4) {
         delta = dis_SSE_E_to_G_unary_lo32( vbi, pfx, delta, 
                                            "sqrtss", Iop_Sqrt32F0x4 );
         goto decode_success;
      }
      
      if (haveNo66noF2noF3(pfx) && sz == 4) {
         delta = dis_SSE_E_to_G_unary_all( vbi, pfx, delta, 
                                           "sqrtps", Iop_Sqrt32Fx4 );
         goto decode_success;
      }
      
      if (haveF2no66noF3(pfx) && sz == 4) {
         delta = dis_SSE_E_to_G_unary_lo64( vbi, pfx, delta, 
                                            "sqrtsd", Iop_Sqrt64F0x2 );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_SSE_E_to_G_unary_all( vbi, pfx, delta, 
                                           "sqrtpd", Iop_Sqrt64Fx2 );
         goto decode_success;
      }
      break;

   case 0x52:
      
      if (haveF3no66noF2(pfx) && sz == 4) {
         delta = dis_SSE_E_to_G_unary_lo32( vbi, pfx, delta, 
                                            "rsqrtss", Iop_RSqrtEst32F0x4 );
         goto decode_success;
      }
      
      if (haveNo66noF2noF3(pfx) && sz == 4) {
         delta = dis_SSE_E_to_G_unary_all( vbi, pfx, delta, 
                                           "rsqrtps", Iop_RSqrtEst32Fx4 );
         goto decode_success;
      }
      break;

   case 0x53:
      
      if (haveF3no66noF2(pfx) && sz == 4) {
         delta = dis_SSE_E_to_G_unary_lo32( vbi, pfx, delta,
                                            "rcpss", Iop_RecipEst32F0x4 );
         goto decode_success;
      }
      
      if (haveNo66noF2noF3(pfx) && sz == 4) {
         delta = dis_SSE_E_to_G_unary_all( vbi, pfx, delta,
                                           "rcpps", Iop_RecipEst32Fx4 );
         goto decode_success;
      }
      break;

   case 0x54:
      
      if (haveNo66noF2noF3(pfx) && sz == 4) {
         delta = dis_SSE_E_to_G_all( vbi, pfx, delta, "andps", Iop_AndV128 );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_SSE_E_to_G_all( vbi, pfx, delta, "andpd", Iop_AndV128 );
         goto decode_success;
      }
      break;

   case 0x55:
      
      if (haveNo66noF2noF3(pfx) && sz == 4) {
         delta = dis_SSE_E_to_G_all_invG( vbi, pfx, delta, "andnps",
                                                           Iop_AndV128 );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_SSE_E_to_G_all_invG( vbi, pfx, delta, "andnpd",
                                                           Iop_AndV128 );
         goto decode_success;
      }
      break;

   case 0x56:
      
      if (haveNo66noF2noF3(pfx) && sz == 4) {
         delta = dis_SSE_E_to_G_all( vbi, pfx, delta, "orps", Iop_OrV128 );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_SSE_E_to_G_all( vbi, pfx, delta, "orpd", Iop_OrV128 );
         goto decode_success;
      }
      break;

   case 0x57:
      
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_SSE_E_to_G_all( vbi, pfx, delta, "xorpd", Iop_XorV128 );
         goto decode_success;
      }
      
      if (haveNo66noF2noF3(pfx) && sz == 4) {
         delta = dis_SSE_E_to_G_all( vbi, pfx, delta, "xorps", Iop_XorV128 );
         goto decode_success;
      }
      break;

   case 0x58:
      
      if (haveNo66noF2noF3(pfx) && sz == 4) {
         delta = dis_SSE_E_to_G_all( vbi, pfx, delta, "addps", Iop_Add32Fx4 );
         goto decode_success;
      }
      
      if (haveF3no66noF2(pfx) && sz == 4) {
         delta = dis_SSE_E_to_G_lo32( vbi, pfx, delta, "addss", Iop_Add32F0x4 );
         goto decode_success;
      }
      
      if (haveF2no66noF3(pfx) 
          && (sz == 4 ||  sz == 8)) {
         delta = dis_SSE_E_to_G_lo64( vbi, pfx, delta, "addsd", Iop_Add64F0x2 );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) 
          && (sz == 2 ||  sz == 8)) {
         delta = dis_SSE_E_to_G_all( vbi, pfx, delta, "addpd", Iop_Add64Fx2 );
         goto decode_success;
      }
      break;

   case 0x59:
      
      if (haveF2no66noF3(pfx) 
          && (sz == 4 ||  sz == 8)) {
         delta = dis_SSE_E_to_G_lo64( vbi, pfx, delta, "mulsd", Iop_Mul64F0x2 );
         goto decode_success;
      }
      
      if (haveF3no66noF2(pfx) && sz == 4) {
         delta = dis_SSE_E_to_G_lo32( vbi, pfx, delta, "mulss", Iop_Mul32F0x4 );
         goto decode_success;
      }
      
      if (haveNo66noF2noF3(pfx) && sz == 4) {
         delta = dis_SSE_E_to_G_all( vbi, pfx, delta, "mulps", Iop_Mul32Fx4 );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) 
          && (sz == 2 ||  sz == 8)) {
         delta = dis_SSE_E_to_G_all( vbi, pfx, delta, "mulpd", Iop_Mul64Fx2 );
         goto decode_success;
      }
      break;

   case 0x5A:
      if (haveNo66noF2noF3(pfx) && sz == 4) {
         delta = dis_CVTPS2PD_128( vbi, pfx, delta, False );
         goto decode_success;
      }
      if (haveF3no66noF2(pfx) && sz == 4) {
         IRTemp f32lo = newTemp(Ity_F32);

         modrm = getUChar(delta);
         if (epartIsReg(modrm)) {
            delta += 1;
            assign(f32lo, getXMMRegLane32F(eregOfRexRM(pfx,modrm), 0));
            DIP("cvtss2sd %s,%s\n", nameXMMReg(eregOfRexRM(pfx,modrm)),
                                    nameXMMReg(gregOfRexRM(pfx,modrm)));
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            assign(f32lo, loadLE(Ity_F32, mkexpr(addr)));
            delta += alen;
            DIP("cvtss2sd %s,%s\n", dis_buf,
                                    nameXMMReg(gregOfRexRM(pfx,modrm)));
         }

         putXMMRegLane64F( gregOfRexRM(pfx,modrm), 0, 
                           unop( Iop_F32toF64, mkexpr(f32lo) ) );

         goto decode_success;
      }
      if (haveF2no66noF3(pfx) && sz == 4) {
         IRTemp rmode = newTemp(Ity_I32);
         IRTemp f64lo = newTemp(Ity_F64);

         modrm = getUChar(delta);
         if (epartIsReg(modrm)) {
            delta += 1;
            assign(f64lo, getXMMRegLane64F(eregOfRexRM(pfx,modrm), 0));
            DIP("cvtsd2ss %s,%s\n", nameXMMReg(eregOfRexRM(pfx,modrm)),
                                    nameXMMReg(gregOfRexRM(pfx,modrm)));
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            assign(f64lo, loadLE(Ity_F64, mkexpr(addr)));
            delta += alen;
            DIP("cvtsd2ss %s,%s\n", dis_buf,
                                    nameXMMReg(gregOfRexRM(pfx,modrm)));
         }

         assign( rmode, get_sse_roundingmode() );
         putXMMRegLane32F( 
            gregOfRexRM(pfx,modrm), 0, 
            binop( Iop_F64toF32, mkexpr(rmode), mkexpr(f64lo) )
         );

         goto decode_success;
      }
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_CVTPD2PS_128( vbi, pfx, delta, False );
         goto decode_success;
      }
      break;

   case 0x5B:
      if ( (have66noF2noF3(pfx) && sz == 2)
           || (haveF3no66noF2(pfx) && sz == 4) ) {
         Bool r2zero = toBool(sz == 4); 
         delta = dis_CVTxPS2DQ_128( vbi, pfx, delta, False, r2zero );
         goto decode_success;
      }
      if (haveNo66noF2noF3(pfx) && sz == 4) {
         delta = dis_CVTDQ2PS_128( vbi, pfx, delta, False );
         goto decode_success;
      }
      break;

   case 0x5C:
      
      if (haveF3no66noF2(pfx) && sz == 4) {
         delta = dis_SSE_E_to_G_lo32( vbi, pfx, delta, "subss", Iop_Sub32F0x4 );
         goto decode_success;
      }
      
      if (haveF2no66noF3(pfx) 
          && (sz == 4 ||  sz == 8)) {
         delta = dis_SSE_E_to_G_lo64( vbi, pfx, delta, "subsd", Iop_Sub64F0x2 );
         goto decode_success;
      }
      
      if (haveNo66noF2noF3(pfx) && sz == 4) {
         delta = dis_SSE_E_to_G_all( vbi, pfx, delta, "subps", Iop_Sub32Fx4 );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_SSE_E_to_G_all( vbi, pfx, delta, "subpd", Iop_Sub64Fx2 );
         goto decode_success;
      }
      break;

   case 0x5D:
      
      if (haveNo66noF2noF3(pfx) && sz == 4) {
         delta = dis_SSE_E_to_G_all( vbi, pfx, delta, "minps", Iop_Min32Fx4 );
         goto decode_success;
      }
      
      if (haveF3no66noF2(pfx) && sz == 4) {
         delta = dis_SSE_E_to_G_lo32( vbi, pfx, delta, "minss", Iop_Min32F0x4 );
         goto decode_success;
      }
      
      if (haveF2no66noF3(pfx) && sz == 4) {
         delta = dis_SSE_E_to_G_lo64( vbi, pfx, delta, "minsd", Iop_Min64F0x2 );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_SSE_E_to_G_all( vbi, pfx, delta, "minpd", Iop_Min64Fx2 );
         goto decode_success;
      }
      break;

   case 0x5E:
      
      if (haveF2no66noF3(pfx) && sz == 4) {
         delta = dis_SSE_E_to_G_lo64( vbi, pfx, delta, "divsd", Iop_Div64F0x2 );
         goto decode_success;
      }
      
      if (haveNo66noF2noF3(pfx) && sz == 4) {
         delta = dis_SSE_E_to_G_all( vbi, pfx, delta, "divps", Iop_Div32Fx4 );
         goto decode_success;
      }
      
      if (haveF3no66noF2(pfx) && sz == 4) {
         delta = dis_SSE_E_to_G_lo32( vbi, pfx, delta, "divss", Iop_Div32F0x4 );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_SSE_E_to_G_all( vbi, pfx, delta, "divpd", Iop_Div64Fx2 );
         goto decode_success;
      }
      break;

   case 0x5F:
      
      if (haveNo66noF2noF3(pfx) && sz == 4) {
         delta = dis_SSE_E_to_G_all( vbi, pfx, delta, "maxps", Iop_Max32Fx4 );
         goto decode_success;
      }
      
      if (haveF3no66noF2(pfx) && sz == 4) {
         delta = dis_SSE_E_to_G_lo32( vbi, pfx, delta, "maxss", Iop_Max32F0x4 );
         goto decode_success;
      }
      
      if (haveF2no66noF3(pfx) && sz == 4) {
         delta = dis_SSE_E_to_G_lo64( vbi, pfx, delta, "maxsd", Iop_Max64F0x2 );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_SSE_E_to_G_all( vbi, pfx, delta, "maxpd", Iop_Max64Fx2 );
         goto decode_success;
      }
      break;

   case 0x60:
      
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_SSEint_E_to_G( vbi, pfx, delta,
                                    "punpcklbw",
                                    Iop_InterleaveLO8x16, True );
         goto decode_success;
      }
      break;

   case 0x61:
      
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_SSEint_E_to_G( vbi, pfx, delta,
                                    "punpcklwd",
                                    Iop_InterleaveLO16x8, True );
         goto decode_success;
      }
      break;

   case 0x62:
      
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_SSEint_E_to_G( vbi, pfx, delta,
                                    "punpckldq",
                                    Iop_InterleaveLO32x4, True );
         goto decode_success;
      }
      break;

   case 0x63:
      
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_SSEint_E_to_G( vbi, pfx, delta,
                                    "packsswb",
                                    Iop_QNarrowBin16Sto8Sx16, True );
         goto decode_success;
      }
      break;

   case 0x64:
      
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_SSEint_E_to_G( vbi, pfx, delta, 
                                    "pcmpgtb", Iop_CmpGT8Sx16, False );
         goto decode_success;
      }
      break;

   case 0x65:
      
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_SSEint_E_to_G( vbi, pfx, delta,
                                    "pcmpgtw", Iop_CmpGT16Sx8, False );
         goto decode_success;
      }
      break;

   case 0x66:
      
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_SSEint_E_to_G( vbi, pfx, delta,
                                    "pcmpgtd", Iop_CmpGT32Sx4, False );
         goto decode_success;
      }
      break;

   case 0x67:
      
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_SSEint_E_to_G( vbi, pfx, delta,
                                    "packuswb",
                                    Iop_QNarrowBin16Sto8Ux16, True );
         goto decode_success;
      }
      break;

   case 0x68:
      
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_SSEint_E_to_G( vbi, pfx, delta,
                                    "punpckhbw",
                                    Iop_InterleaveHI8x16, True );
         goto decode_success;
      }
      break;

   case 0x69:
      
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_SSEint_E_to_G( vbi, pfx, delta,
                                    "punpckhwd",
                                    Iop_InterleaveHI16x8, True );
         goto decode_success;
      }
      break;

   case 0x6A:
      
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_SSEint_E_to_G( vbi, pfx, delta, 
                                    "punpckhdq",
                                    Iop_InterleaveHI32x4, True );
         goto decode_success;
      }
      break;

   case 0x6B:
      
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_SSEint_E_to_G( vbi, pfx, delta,
                                    "packssdw",
                                    Iop_QNarrowBin32Sto16Sx8, True );
         goto decode_success;
      }
      break;

   case 0x6C:
      
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_SSEint_E_to_G( vbi, pfx, delta,
                                    "punpcklqdq",
                                    Iop_InterleaveLO64x2, True );
         goto decode_success;
      }
      break;

   case 0x6D:
      
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_SSEint_E_to_G( vbi, pfx, delta,
                                    "punpckhqdq",
                                    Iop_InterleaveHI64x2, True );
         goto decode_success;
      }
      break;

   case 0x6E:
      if (have66noF2noF3(pfx)) {
         vassert(sz == 2 || sz == 8);
         if (sz == 2) sz = 4;
         modrm = getUChar(delta);
         if (epartIsReg(modrm)) {
            delta += 1;
            if (sz == 4) {
               putXMMReg(
                  gregOfRexRM(pfx,modrm),
                  unop( Iop_32UtoV128, getIReg32(eregOfRexRM(pfx,modrm)) ) 
               );
               DIP("movd %s, %s\n", nameIReg32(eregOfRexRM(pfx,modrm)), 
                                    nameXMMReg(gregOfRexRM(pfx,modrm)));
            } else {
               putXMMReg(
                  gregOfRexRM(pfx,modrm),
                  unop( Iop_64UtoV128, getIReg64(eregOfRexRM(pfx,modrm)) ) 
               );
               DIP("movq %s, %s\n", nameIReg64(eregOfRexRM(pfx,modrm)), 
                                    nameXMMReg(gregOfRexRM(pfx,modrm)));
            }
         } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 0 );
            delta += alen;
            putXMMReg(
               gregOfRexRM(pfx,modrm),
               sz == 4 
                  ?  unop( Iop_32UtoV128,loadLE(Ity_I32, mkexpr(addr)) ) 
                  :  unop( Iop_64UtoV128,loadLE(Ity_I64, mkexpr(addr)) )
            );
            DIP("mov%c %s, %s\n", sz == 4 ? 'd' : 'q', dis_buf, 
                                  nameXMMReg(gregOfRexRM(pfx,modrm)));
         }
         goto decode_success;
      }
      break;

   case 0x6F:
      if (have66noF2noF3(pfx) 
          && (sz == 2 ||  sz == 8)) {
         
         modrm = getUChar(delta);
         if (epartIsReg(modrm)) {
            putXMMReg( gregOfRexRM(pfx,modrm), 
                       getXMMReg( eregOfRexRM(pfx,modrm) ));
            DIP("movdqa %s,%s\n", nameXMMReg(eregOfRexRM(pfx,modrm)),
                                  nameXMMReg(gregOfRexRM(pfx,modrm)));
            delta += 1;
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            gen_SEGV_if_not_16_aligned( addr );
            putXMMReg( gregOfRexRM(pfx,modrm), 
                       loadLE(Ity_V128, mkexpr(addr)) );
            DIP("movdqa %s,%s\n", dis_buf,
                                  nameXMMReg(gregOfRexRM(pfx,modrm)));
            delta += alen;
         }
         goto decode_success;
      }
      if (haveF3no66noF2(pfx) && sz == 4) {
         
         modrm = getUChar(delta);
         if (epartIsReg(modrm)) {
            putXMMReg( gregOfRexRM(pfx,modrm), 
                       getXMMReg( eregOfRexRM(pfx,modrm) ));
            DIP("movdqu %s,%s\n", nameXMMReg(eregOfRexRM(pfx,modrm)),
                                  nameXMMReg(gregOfRexRM(pfx,modrm)));
            delta += 1;
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            putXMMReg( gregOfRexRM(pfx,modrm), 
                       loadLE(Ity_V128, mkexpr(addr)) );
            DIP("movdqu %s,%s\n", dis_buf,
                                  nameXMMReg(gregOfRexRM(pfx,modrm)));
            delta += alen;
         }
         goto decode_success;
      }
      break;

   case 0x70:
      
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_PSHUFD_32x4( vbi, pfx, delta, False);
         goto decode_success;
      }
      
      
      if (haveNo66noF2noF3(pfx) && sz == 4) {
         Int order;
         IRTemp sV, dV, s3, s2, s1, s0;
         s3 = s2 = s1 = s0 = IRTemp_INVALID;
         sV = newTemp(Ity_I64);
         dV = newTemp(Ity_I64);
         do_MMX_preamble();
         modrm = getUChar(delta);
         if (epartIsReg(modrm)) {
            assign( sV, getMMXReg(eregLO3ofRM(modrm)) );
            order = (Int)getUChar(delta+1);
            delta += 1+1;
            DIP("pshufw $%d,%s,%s\n", order, 
                                      nameMMXReg(eregLO3ofRM(modrm)),
                                      nameMMXReg(gregLO3ofRM(modrm)));
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf,
                              1 );
            assign( sV, loadLE(Ity_I64, mkexpr(addr)) );
            order = (Int)getUChar(delta+alen);
            delta += 1+alen;
            DIP("pshufw $%d,%s,%s\n", order, 
                                      dis_buf,
                                      nameMMXReg(gregLO3ofRM(modrm)));
         }
         breakup64to16s( sV, &s3, &s2, &s1, &s0 );
#        define SEL(n) \
                   ((n)==0 ? s0 : ((n)==1 ? s1 : ((n)==2 ? s2 : s3)))
         assign(dV,
                mk64from16s( SEL((order>>6)&3), SEL((order>>4)&3),
                             SEL((order>>2)&3), SEL((order>>0)&3) )
         );
         putMMXReg(gregLO3ofRM(modrm), mkexpr(dV));
#        undef SEL
         goto decode_success;
      }
      if (haveF2no66noF3(pfx) && sz == 4) {
         delta = dis_PSHUFxW_128( vbi, pfx, delta,
                                  False, False );
         goto decode_success;
      }
      if (haveF3no66noF2(pfx) && sz == 4) {
         delta = dis_PSHUFxW_128( vbi, pfx, delta,
                                  False, True );
         goto decode_success;
      }
      break;

   case 0x71:
      
      if (have66noF2noF3(pfx) && sz == 2
          && epartIsReg(getUChar(delta))
          && gregLO3ofRM(getUChar(delta)) == 2) {
         delta = dis_SSE_shiftE_imm( pfx, delta, "psrlw", Iop_ShrN16x8 );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && sz == 2 
          && epartIsReg(getUChar(delta))
          && gregLO3ofRM(getUChar(delta)) == 4) {
         delta = dis_SSE_shiftE_imm( pfx, delta, "psraw", Iop_SarN16x8 );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && sz == 2 
          && epartIsReg(getUChar(delta))
          && gregLO3ofRM(getUChar(delta)) == 6) {
         delta = dis_SSE_shiftE_imm( pfx, delta, "psllw", Iop_ShlN16x8 );
         goto decode_success;
      }
      break;

   case 0x72:
      
      if (have66noF2noF3(pfx) && sz == 2 
          && epartIsReg(getUChar(delta))
          && gregLO3ofRM(getUChar(delta)) == 2) {
         delta = dis_SSE_shiftE_imm( pfx, delta, "psrld", Iop_ShrN32x4 );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && sz == 2 
          && epartIsReg(getUChar(delta))
          && gregLO3ofRM(getUChar(delta)) == 4) {
         delta = dis_SSE_shiftE_imm( pfx, delta, "psrad", Iop_SarN32x4 );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && sz == 2 
          && epartIsReg(getUChar(delta))
          && gregLO3ofRM(getUChar(delta)) == 6) {
         delta = dis_SSE_shiftE_imm( pfx, delta, "pslld", Iop_ShlN32x4 );
         goto decode_success;
      }
      break;

   case 0x73:
      
      
      if (have66noF2noF3(pfx) && sz == 2 
          && epartIsReg(getUChar(delta))
          && gregLO3ofRM(getUChar(delta)) == 3) {
         Int imm = (Int)getUChar(delta+1);
         Int reg = eregOfRexRM(pfx,getUChar(delta));
         DIP("psrldq $%d,%s\n", imm, nameXMMReg(reg));
         delta += 2;
         IRTemp sV = newTemp(Ity_V128);
         assign( sV, getXMMReg(reg) );
         putXMMReg(reg, mkexpr(math_PSRLDQ( sV, imm )));
         goto decode_success;
      }
      
      
      if (have66noF2noF3(pfx) && sz == 2 
          && epartIsReg(getUChar(delta))
          && gregLO3ofRM(getUChar(delta)) == 7) {
         Int imm = (Int)getUChar(delta+1);
         Int reg = eregOfRexRM(pfx,getUChar(delta));
         DIP("pslldq $%d,%s\n", imm, nameXMMReg(reg));
         vassert(imm >= 0 && imm <= 255);
         delta += 2;
         IRTemp sV = newTemp(Ity_V128);
         assign( sV, getXMMReg(reg) );
         putXMMReg(reg, mkexpr(math_PSLLDQ( sV, imm )));
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && sz == 2
          && epartIsReg(getUChar(delta))
          && gregLO3ofRM(getUChar(delta)) == 2) {
         delta = dis_SSE_shiftE_imm( pfx, delta, "psrlq", Iop_ShrN64x2 );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && sz == 2 
          && epartIsReg(getUChar(delta))
          && gregLO3ofRM(getUChar(delta)) == 6) {
         delta = dis_SSE_shiftE_imm( pfx, delta, "psllq", Iop_ShlN64x2 );
         goto decode_success;
      }
      break;

   case 0x74:
      
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_SSEint_E_to_G( vbi, pfx, delta,
                                    "pcmpeqb", Iop_CmpEQ8x16, False );
         goto decode_success;
      }
      break;

   case 0x75:
      
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_SSEint_E_to_G( vbi, pfx, delta,
                                    "pcmpeqw", Iop_CmpEQ16x8, False );
         goto decode_success;
      }
      break;

   case 0x76:
      
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_SSEint_E_to_G( vbi, pfx, delta,
                                    "pcmpeqd", Iop_CmpEQ32x4, False );
         goto decode_success;
      }
      break;

   case 0x7E:
      if (haveF3no66noF2(pfx) 
          && (sz == 4 ||  sz == 8)) {
         modrm = getUChar(delta);
         if (epartIsReg(modrm)) {
            putXMMRegLane64( gregOfRexRM(pfx,modrm), 0,
                             getXMMRegLane64( eregOfRexRM(pfx,modrm), 0 ));
               
               putXMMRegLane64( gregOfRexRM(pfx,modrm), 1, mkU64(0) );
            DIP("movsd %s,%s\n", nameXMMReg(eregOfRexRM(pfx,modrm)),
                                 nameXMMReg(gregOfRexRM(pfx,modrm)));
            delta += 1;
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            putXMMReg( gregOfRexRM(pfx,modrm), mkV128(0) );
            putXMMRegLane64( gregOfRexRM(pfx,modrm), 0,
                             loadLE(Ity_I64, mkexpr(addr)) );
            DIP("movsd %s,%s\n", dis_buf,
                                 nameXMMReg(gregOfRexRM(pfx,modrm)));
            delta += alen;
         }
         goto decode_success;
      }
      
      
         if (have66noF2noF3(pfx) && (sz == 2 || sz == 8)) {
         if (sz == 2) sz = 4;
         modrm = getUChar(delta);
         if (epartIsReg(modrm)) {
            delta += 1;
            if (sz == 4) {
               putIReg32( eregOfRexRM(pfx,modrm),
                          getXMMRegLane32(gregOfRexRM(pfx,modrm), 0) );
               DIP("movd %s, %s\n", nameXMMReg(gregOfRexRM(pfx,modrm)), 
                                    nameIReg32(eregOfRexRM(pfx,modrm)));
            } else {
               putIReg64( eregOfRexRM(pfx,modrm),
                          getXMMRegLane64(gregOfRexRM(pfx,modrm), 0) );
               DIP("movq %s, %s\n", nameXMMReg(gregOfRexRM(pfx,modrm)), 
                                    nameIReg64(eregOfRexRM(pfx,modrm)));
            }
         } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 0 );
            delta += alen;
            storeLE( mkexpr(addr),
                     sz == 4
                        ? getXMMRegLane32(gregOfRexRM(pfx,modrm),0)
                        : getXMMRegLane64(gregOfRexRM(pfx,modrm),0) );
            DIP("mov%c %s, %s\n", sz == 4 ? 'd' : 'q',
                                  nameXMMReg(gregOfRexRM(pfx,modrm)), dis_buf);
         }
         goto decode_success;
      }
      break;

   case 0x7F:
      
      if (haveF3no66noF2(pfx) && sz == 4) {
         modrm = getUChar(delta);
         if (epartIsReg(modrm)) {
            goto decode_failure; 
            delta += 1;
            putXMMReg( eregOfRexRM(pfx,modrm),
                       getXMMReg(gregOfRexRM(pfx,modrm)) );
            DIP("movdqu %s, %s\n", nameXMMReg(gregOfRexRM(pfx,modrm)), 
                                   nameXMMReg(eregOfRexRM(pfx,modrm)));
         } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 0 );
            delta += alen;
            storeLE( mkexpr(addr), getXMMReg(gregOfRexRM(pfx,modrm)) );
            DIP("movdqu %s, %s\n", nameXMMReg(gregOfRexRM(pfx,modrm)), dis_buf);
         }
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && sz == 2) {
         modrm = getUChar(delta);
         if (epartIsReg(modrm)) {
            delta += 1;
            putXMMReg( eregOfRexRM(pfx,modrm),
                       getXMMReg(gregOfRexRM(pfx,modrm)) );
            DIP("movdqa %s, %s\n", nameXMMReg(gregOfRexRM(pfx,modrm)), 
                                   nameXMMReg(eregOfRexRM(pfx,modrm)));
         } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 0 );
            gen_SEGV_if_not_16_aligned( addr );
            delta += alen;
            storeLE( mkexpr(addr), getXMMReg(gregOfRexRM(pfx,modrm)) );
            DIP("movdqa %s, %s\n", nameXMMReg(gregOfRexRM(pfx,modrm)), dis_buf);
         }
         goto decode_success;
      }
      break;

   case 0xAE:
      
      if (haveNo66noF2noF3(pfx) 
          && epartIsReg(getUChar(delta)) && gregLO3ofRM(getUChar(delta)) == 7
          && sz == 4) {
         delta += 1;
         stmt( IRStmt_MBE(Imbe_Fence) );
         DIP("sfence\n");
         goto decode_success;
      }
      
      
      
      if (haveNo66noF2noF3(pfx)
          && epartIsReg(getUChar(delta))
          && (gregLO3ofRM(getUChar(delta)) == 5
              || gregLO3ofRM(getUChar(delta)) == 6)
          && sz == 4) {
         delta += 1;
         stmt( IRStmt_MBE(Imbe_Fence) );
         DIP("%sfence\n", gregLO3ofRM(getUChar(delta-1))==5 ? "l" : "m");
         goto decode_success;
      }

      
      if (haveNo66noF2noF3(pfx)
          && !epartIsReg(getUChar(delta)) && gregLO3ofRM(getUChar(delta)) == 7
          && sz == 4) {

         ULong lineszB = 256ULL;

         addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
         delta += alen;

         
         stmt( IRStmt_Put(
                  OFFB_CMSTART,
                  binop( Iop_And64, 
                         mkexpr(addr), 
                         mkU64( ~(lineszB-1) ))) );

         stmt( IRStmt_Put(OFFB_CMLEN, mkU64(lineszB) ) );

         jmp_lit(dres, Ijk_InvalICache, (Addr64)(guest_RIP_bbstart+delta));

         DIP("clflush %s\n", dis_buf);
         goto decode_success;
      }

      
      if (haveNo66noF2noF3(pfx)
          && !epartIsReg(getUChar(delta)) && gregLO3ofRM(getUChar(delta)) == 3
          && sz == 4) {
         delta = dis_STMXCSR(vbi, pfx, delta, False);
         goto decode_success;
      }
      
      if (haveNo66noF2noF3(pfx)
          && !epartIsReg(getUChar(delta)) && gregLO3ofRM(getUChar(delta)) == 2
          && sz == 4) {
         delta = dis_LDMXCSR(vbi, pfx, delta, False);
         goto decode_success;
      }
      /* 0F AE /0 = FXSAVE m512 -- write x87 and SSE state to memory.
         Note that the presence or absence of REX.W slightly affects the
         written format: whether the saved FPU IP and DP pointers are 64
         or 32 bits.  But the helper function we call simply writes zero
         bits in the relevant fields (which are 64 bits regardless of
         what REX.W is) and so it's good enough (iow, equally broken) in
         both cases. */
      if (haveNo66noF2noF3(pfx) && (sz == 4 || sz == 8)
          && !epartIsReg(getUChar(delta))
          && gregOfRexRM(pfx,getUChar(delta)) == 0) {
          IRDirty* d;
         modrm = getUChar(delta);
         vassert(!epartIsReg(modrm));

         addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
         delta += alen;
         gen_SEGV_if_not_16_aligned(addr);

         DIP("%sfxsave %s\n", sz==8 ? "rex64/" : "", dis_buf);

         d = unsafeIRDirty_0_N ( 
                0, 
                "amd64g_dirtyhelper_FXSAVE_ALL_EXCEPT_XMM",
                &amd64g_dirtyhelper_FXSAVE_ALL_EXCEPT_XMM,
                mkIRExprVec_2( IRExpr_BBPTR(), mkexpr(addr) )
             );

         
         d->mFx   = Ifx_Write;
         d->mAddr = mkexpr(addr);
         d->mSize = 464; 

         
         d->nFxState = 6;
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
         d->fxState[3].size   = sizeof(ULong);

         d->fxState[4].fx     = Ifx_Read;
         d->fxState[4].offset = OFFB_FC3210;
         d->fxState[4].size   = sizeof(ULong);

         d->fxState[5].fx     = Ifx_Read;
         d->fxState[5].offset = OFFB_SSEROUND;
         d->fxState[5].size   = sizeof(ULong);

         stmt( IRStmt_Dirty(d) );

         
         UInt xmm;
         for (xmm = 0; xmm < 16; xmm++) {
            storeLE( binop(Iop_Add64, mkexpr(addr), mkU64(160 + xmm * 16)),
                     getXMMReg(xmm) );
         }

         goto decode_success;
      }
      if (haveNo66noF2noF3(pfx) && (sz == 4 || sz == 8)
          && !epartIsReg(getUChar(delta))
          && gregOfRexRM(pfx,getUChar(delta)) == 1) {
         IRDirty* d;
         modrm = getUChar(delta);
         vassert(!epartIsReg(modrm));

         addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
         delta += alen;
         gen_SEGV_if_not_16_aligned(addr);

         DIP("%sfxrstor %s\n", sz==8 ? "rex64/" : "", dis_buf);

         d = unsafeIRDirty_0_N ( 
                0, 
                "amd64g_dirtyhelper_FXRSTOR_ALL_EXCEPT_XMM", 
                &amd64g_dirtyhelper_FXRSTOR_ALL_EXCEPT_XMM,
                mkIRExprVec_2( IRExpr_BBPTR(), mkexpr(addr) )
             );

         
         d->mFx   = Ifx_Read;
         d->mAddr = mkexpr(addr);
         d->mSize = 464; 

         
         d->nFxState = 6;
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
         d->fxState[3].size   = sizeof(ULong);

         d->fxState[4].fx     = Ifx_Write;
         d->fxState[4].offset = OFFB_FC3210;
         d->fxState[4].size   = sizeof(ULong);

         d->fxState[5].fx     = Ifx_Write;
         d->fxState[5].offset = OFFB_SSEROUND;
         d->fxState[5].size   = sizeof(ULong);

         stmt( IRStmt_Dirty(d) );

         
         UInt xmm;
         for (xmm = 0; xmm < 16; xmm++) {
            putXMMReg(xmm, loadLE(Ity_V128,
                                  binop(Iop_Add64, mkexpr(addr),
                                                   mkU64(160 + xmm * 16))));
         }

         goto decode_success;
      }
      break;

   case 0xC2:
      
      if (haveNo66noF2noF3(pfx) && sz == 4) {
         Long delta0 = delta;
         delta = dis_SSE_cmp_E_to_G( vbi, pfx, delta, "cmpps", True, 4 );
         if (delta > delta0) goto decode_success;
      }
      
      if (haveF3no66noF2(pfx) && sz == 4) {
         Long delta0 = delta;
         delta = dis_SSE_cmp_E_to_G( vbi, pfx, delta, "cmpss", False, 4 );
         if (delta > delta0) goto decode_success;
      }
      
      if (haveF2no66noF3(pfx) && sz == 4) {
         Long delta0 = delta;
         delta = dis_SSE_cmp_E_to_G( vbi, pfx, delta, "cmpsd", False, 8 );
         if (delta > delta0) goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && sz == 2) {
         Long delta0 = delta;
         delta = dis_SSE_cmp_E_to_G( vbi, pfx, delta, "cmppd", True, 8 );
         if (delta > delta0) goto decode_success;
      }
      break;

   case 0xC3:
      
      if (haveNo66noF2noF3(pfx) && (sz == 4 || sz == 8)) {
         modrm = getUChar(delta);
         if (!epartIsReg(modrm)) {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            storeLE( mkexpr(addr), getIRegG(sz, pfx, modrm) );
            DIP("movnti %s,%s\n", dis_buf,
                                  nameIRegG(sz, pfx, modrm));
            delta += alen;
            goto decode_success;
         }
         
      }
      break;

   case 0xC4:
      
      if (haveNo66noF2noF3(pfx)
          && (sz == 4 ||  sz == 8)) {
         Int lane;
         t4 = newTemp(Ity_I16);
         t5 = newTemp(Ity_I64);
         t6 = newTemp(Ity_I64);
         modrm = getUChar(delta);
         do_MMX_preamble();

         assign(t5, getMMXReg(gregLO3ofRM(modrm)));
         breakup64to16s( t5, &t3, &t2, &t1, &t0 );

         if (epartIsReg(modrm)) {
            assign(t4, getIReg16(eregOfRexRM(pfx,modrm)));
            delta += 1+1;
            lane = getUChar(delta-1);
            DIP("pinsrw $%d,%s,%s\n", (Int)lane, 
                                      nameIReg16(eregOfRexRM(pfx,modrm)),
                                      nameMMXReg(gregLO3ofRM(modrm)));
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 1 );
            delta += 1+alen;
            lane = getUChar(delta-1);
            assign(t4, loadLE(Ity_I16, mkexpr(addr)));
            DIP("pinsrw $%d,%s,%s\n", (Int)lane,
                                      dis_buf,
                                      nameMMXReg(gregLO3ofRM(modrm)));
         }

         switch (lane & 3) {
            case 0:  assign(t6, mk64from16s(t3,t2,t1,t4)); break;
            case 1:  assign(t6, mk64from16s(t3,t2,t4,t0)); break;
            case 2:  assign(t6, mk64from16s(t3,t4,t1,t0)); break;
            case 3:  assign(t6, mk64from16s(t4,t2,t1,t0)); break;
            default: vassert(0);
         }
         putMMXReg(gregLO3ofRM(modrm), mkexpr(t6));
         goto decode_success;
      }
      if (have66noF2noF3(pfx) 
          && (sz == 2 ||  sz == 8)) {
         Int lane;
         t4 = newTemp(Ity_I16);
         modrm = getUChar(delta);
         UInt rG = gregOfRexRM(pfx,modrm);
         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx,modrm);
            assign(t4, getIReg16(rE));
            delta += 1+1;
            lane = getUChar(delta-1);
            DIP("pinsrw $%d,%s,%s\n",
                (Int)lane, nameIReg16(rE), nameXMMReg(rG));
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 
                              1 );
            delta += 1+alen;
            lane = getUChar(delta-1);
            assign(t4, loadLE(Ity_I16, mkexpr(addr)));
            DIP("pinsrw $%d,%s,%s\n",
                (Int)lane, dis_buf, nameXMMReg(rG));
         }
         IRTemp src_vec = newTemp(Ity_V128);
         assign(src_vec, getXMMReg(rG));
         IRTemp res_vec = math_PINSRW_128( src_vec, t4, lane & 7);
         putXMMReg(rG, mkexpr(res_vec));
         goto decode_success;
      }
      break;

   case 0xC5:
      
      if (haveNo66noF2noF3(pfx) && (sz == 4 || sz == 8)) {
         modrm = getUChar(delta);
         if (epartIsReg(modrm)) {
            IRTemp sV = newTemp(Ity_I64);
            t5 = newTemp(Ity_I16);
            do_MMX_preamble();
            assign(sV, getMMXReg(eregLO3ofRM(modrm)));
            breakup64to16s( sV, &t3, &t2, &t1, &t0 );
            switch (getUChar(delta+1) & 3) {
               case 0:  assign(t5, mkexpr(t0)); break;
               case 1:  assign(t5, mkexpr(t1)); break;
               case 2:  assign(t5, mkexpr(t2)); break;
               case 3:  assign(t5, mkexpr(t3)); break;
               default: vassert(0);
            }
            if (sz == 8)
               putIReg64(gregOfRexRM(pfx,modrm), unop(Iop_16Uto64, mkexpr(t5)));
            else
               putIReg32(gregOfRexRM(pfx,modrm), unop(Iop_16Uto32, mkexpr(t5)));
            DIP("pextrw $%d,%s,%s\n",
                (Int)getUChar(delta+1),
                nameMMXReg(eregLO3ofRM(modrm)),
                sz==8 ? nameIReg64(gregOfRexRM(pfx,modrm))
                      : nameIReg32(gregOfRexRM(pfx,modrm))
            );
            delta += 2;
            goto decode_success;
         } 
         
      }
      if (have66noF2noF3(pfx) 
          && (sz == 2 ||  sz == 8)) {
         Long delta0 = delta;
         delta = dis_PEXTRW_128_EregOnly_toG( vbi, pfx, delta,
                                              False );
         if (delta > delta0) goto decode_success;
         
      }
      break;

   case 0xC6:
      
      if (haveNo66noF2noF3(pfx) && sz == 4) {
         Int    imm8 = 0;
         IRTemp sV   = newTemp(Ity_V128);
         IRTemp dV   = newTemp(Ity_V128);
         modrm = getUChar(delta);
         UInt rG = gregOfRexRM(pfx,modrm);
         assign( dV, getXMMReg(rG) );
         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx,modrm);
            assign( sV, getXMMReg(rE) );
            imm8 = (Int)getUChar(delta+1);
            delta += 1+1;
            DIP("shufps $%d,%s,%s\n", imm8, nameXMMReg(rE), nameXMMReg(rG));
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 1 );
            assign( sV, loadLE(Ity_V128, mkexpr(addr)) );
            imm8 = (Int)getUChar(delta+alen);
            delta += 1+alen;
            DIP("shufps $%d,%s,%s\n", imm8, dis_buf, nameXMMReg(rG));
         }
         IRTemp res = math_SHUFPS_128( sV, dV, imm8 );
         putXMMReg( gregOfRexRM(pfx,modrm), mkexpr(res) );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && sz == 2) {
         Int    select;
         IRTemp sV = newTemp(Ity_V128);
         IRTemp dV = newTemp(Ity_V128);

         modrm = getUChar(delta);
         assign( dV, getXMMReg(gregOfRexRM(pfx,modrm)) );

         if (epartIsReg(modrm)) {
            assign( sV, getXMMReg(eregOfRexRM(pfx,modrm)) );
            select = (Int)getUChar(delta+1);
            delta += 1+1;
            DIP("shufpd $%d,%s,%s\n", select, 
                                      nameXMMReg(eregOfRexRM(pfx,modrm)),
                                      nameXMMReg(gregOfRexRM(pfx,modrm)));
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 1 );
            assign( sV, loadLE(Ity_V128, mkexpr(addr)) );
            select = getUChar(delta+alen);
            delta += 1+alen;
            DIP("shufpd $%d,%s,%s\n", select, 
                                      dis_buf,
                                      nameXMMReg(gregOfRexRM(pfx,modrm)));
         }

         IRTemp res = math_SHUFPD_128( sV, dV, select );
         putXMMReg( gregOfRexRM(pfx,modrm), mkexpr(res) );
         goto decode_success;
      }
      break;

   case 0xD1:
      
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_SSE_shiftG_byE( vbi, pfx, delta, "psrlw", Iop_ShrN16x8 );
         goto decode_success;
      }
      break;

   case 0xD2:
      
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_SSE_shiftG_byE( vbi, pfx, delta, "psrld", Iop_ShrN32x4 );
         goto decode_success;
      }
      break;

   case 0xD3:
      
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_SSE_shiftG_byE( vbi, pfx, delta, "psrlq", Iop_ShrN64x2 );
         goto decode_success;
      }
      break;

   case 0xD4:
      
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_SSEint_E_to_G( vbi, pfx, delta,
                                    "paddq", Iop_Add64x2, False );
         goto decode_success;
      }
      
      
      if (haveNo66noF2noF3(pfx) && sz == 4) {
         do_MMX_preamble();
         delta = dis_MMXop_regmem_to_reg ( 
                   vbi, pfx, delta, opc, "paddq", False );
         goto decode_success;
      }
      break;

   case 0xD5:
      
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_SSEint_E_to_G( vbi, pfx, delta, 
                                    "pmullw", Iop_Mul16x8, False );
         goto decode_success;
      }
      break;

   case 0xD6:
      if (haveF3no66noF2(pfx) && sz == 4) {
         modrm = getUChar(delta);
         if (epartIsReg(modrm)) {
            do_MMX_preamble();
            putXMMReg( gregOfRexRM(pfx,modrm), 
                       unop(Iop_64UtoV128, getMMXReg( eregLO3ofRM(modrm) )) );
            DIP("movq2dq %s,%s\n", nameMMXReg(eregLO3ofRM(modrm)),
                                   nameXMMReg(gregOfRexRM(pfx,modrm)));
            delta += 1;
            goto decode_success;
         }
         
      }
      if (have66noF2noF3(pfx) 
          && (sz == 2 ||  sz == 8)) {
         modrm = getUChar(delta);
         if (epartIsReg(modrm)) {
            
            
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            storeLE( mkexpr(addr), 
                     getXMMRegLane64( gregOfRexRM(pfx,modrm), 0 ));
            DIP("movq %s,%s\n", nameXMMReg(gregOfRexRM(pfx,modrm)), dis_buf );
            delta += alen;
            goto decode_success;
         }
      }
      
      if (haveF2no66noF3(pfx) && sz == 4) {
         modrm = getUChar(delta);
         if (epartIsReg(modrm)) {
            do_MMX_preamble();
            putMMXReg( gregLO3ofRM(modrm), 
                       getXMMRegLane64( eregOfRexRM(pfx,modrm), 0 ));
            DIP("movdq2q %s,%s\n", nameXMMReg(eregOfRexRM(pfx,modrm)),
                                   nameMMXReg(gregLO3ofRM(modrm)));
            delta += 1;
            goto decode_success;
         }
         
      }
      break;

   case 0xD7:
      if (have66noF2noF3(pfx) 
          && (sz == 2 ||  sz == 8)
          && epartIsReg(getUChar(delta))) { 
         delta = dis_PMOVMSKB_128( vbi, pfx, delta, False );
         goto decode_success;
      }
      
      if (haveNo66noF2noF3(pfx)
          && (sz == 4 ||  sz == 8)) {
         modrm = getUChar(delta);
         if (epartIsReg(modrm)) {
            do_MMX_preamble();
            t0 = newTemp(Ity_I64);
            t1 = newTemp(Ity_I32);
            assign(t0, getMMXReg(eregLO3ofRM(modrm)));
            assign(t1, unop(Iop_8Uto32, unop(Iop_GetMSBs8x8, mkexpr(t0))));
            putIReg32(gregOfRexRM(pfx,modrm), mkexpr(t1));
            DIP("pmovmskb %s,%s\n", nameMMXReg(eregLO3ofRM(modrm)),
                                    nameIReg32(gregOfRexRM(pfx,modrm)));
            delta += 1;
            goto decode_success;
         } 
         
      }
      break;

   case 0xD8:
      
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_SSEint_E_to_G( vbi, pfx, delta,
                                    "psubusb", Iop_QSub8Ux16, False );
         goto decode_success;
      }
      break;

   case 0xD9:
      
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_SSEint_E_to_G( vbi, pfx, delta,
                                    "psubusw", Iop_QSub16Ux8, False );
         goto decode_success;
      }
      break;

   case 0xDA:
      
      
      if (haveNo66noF2noF3(pfx) && sz == 4) {
         do_MMX_preamble();
         delta = dis_MMXop_regmem_to_reg ( 
                    vbi, pfx, delta, opc, "pminub", False );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_SSEint_E_to_G( vbi, pfx, delta,
                                    "pminub", Iop_Min8Ux16, False );
         goto decode_success;
      }
      break;

   case 0xDB:
      
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_SSE_E_to_G_all( vbi, pfx, delta, "pand", Iop_AndV128 );
         goto decode_success;
      }
      break;

   case 0xDC:
      
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_SSEint_E_to_G( vbi, pfx, delta,
                                    "paddusb", Iop_QAdd8Ux16, False );
         goto decode_success;
      }
      break;

   case 0xDD:
      
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_SSEint_E_to_G( vbi, pfx, delta,
                                    "paddusw", Iop_QAdd16Ux8, False );
         goto decode_success;
      }
      break;

   case 0xDE:
      
      
      if (haveNo66noF2noF3(pfx) && sz == 4) {
         do_MMX_preamble();
         delta = dis_MMXop_regmem_to_reg ( 
                    vbi, pfx, delta, opc, "pmaxub", False );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_SSEint_E_to_G( vbi, pfx, delta,
                                    "pmaxub", Iop_Max8Ux16, False );
         goto decode_success;
      }
      break;

   case 0xDF:
      
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_SSE_E_to_G_all_invG( vbi, pfx, delta, "pandn", Iop_AndV128 );
         goto decode_success;
      }
      break;

   case 0xE0:
      
      
      if (haveNo66noF2noF3(pfx) && sz == 4) {
         do_MMX_preamble();
         delta = dis_MMXop_regmem_to_reg ( 
                    vbi, pfx, delta, opc, "pavgb", False );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_SSEint_E_to_G( vbi, pfx, delta,
                                    "pavgb", Iop_Avg8Ux16, False );
         goto decode_success;
      }
      break;

   case 0xE1:
      
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_SSE_shiftG_byE( vbi, pfx, delta, "psraw", Iop_SarN16x8 );
         goto decode_success;
      }
      break;

   case 0xE2:
      
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_SSE_shiftG_byE( vbi, pfx, delta, "psrad", Iop_SarN32x4 );
         goto decode_success;
      }
      break;

   case 0xE3:
      
      
      if (haveNo66noF2noF3(pfx) && sz == 4) {
         do_MMX_preamble();
         delta = dis_MMXop_regmem_to_reg ( 
                    vbi, pfx, delta, opc, "pavgw", False );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_SSEint_E_to_G( vbi, pfx, delta,
                                    "pavgw", Iop_Avg16Ux8, False );
         goto decode_success;
      }
      break;

   case 0xE4:
      
      
      if (haveNo66noF2noF3(pfx) && sz == 4) {
         do_MMX_preamble();
         delta = dis_MMXop_regmem_to_reg ( 
                    vbi, pfx, delta, opc, "pmuluh", False );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_SSEint_E_to_G( vbi, pfx, delta,
                                    "pmulhuw", Iop_MulHi16Ux8, False );
         goto decode_success;
      }
      break;

   case 0xE5:
      
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_SSEint_E_to_G( vbi, pfx, delta,
                                    "pmulhw", Iop_MulHi16Sx8, False );
         goto decode_success;
      }
      break;

   case 0xE6:
      if ( (haveF2no66noF3(pfx) && sz == 4)
           || (have66noF2noF3(pfx) && sz == 2) ) {
         delta = dis_CVTxPD2DQ_128( vbi, pfx, delta, False,
                                    toBool(sz == 2));
         goto decode_success;
      }
      if (haveF3no66noF2(pfx) && sz == 4) {
         delta = dis_CVTDQ2PD_128(vbi, pfx, delta, False);
         goto decode_success;
      }
      break;

   case 0xE7:
      
      if (haveNo66noF2noF3(pfx) && sz == 4) {
         modrm = getUChar(delta);
         if (!epartIsReg(modrm)) {
            
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            storeLE( mkexpr(addr), getMMXReg(gregLO3ofRM(modrm)) );
            DIP("movntq %s,%s\n", dis_buf,
                                  nameMMXReg(gregLO3ofRM(modrm)));
            delta += alen;
            goto decode_success;
         }
         
      }
      
      if (have66noF2noF3(pfx) && sz == 2) {
         modrm = getUChar(delta);
         if (!epartIsReg(modrm)) {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            gen_SEGV_if_not_16_aligned( addr );
            storeLE( mkexpr(addr), getXMMReg(gregOfRexRM(pfx,modrm)) );
            DIP("movntdq %s,%s\n", dis_buf,
                                   nameXMMReg(gregOfRexRM(pfx,modrm)));
            delta += alen;
            goto decode_success;
         }
         
      }
      break;

   case 0xE8:
      
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_SSEint_E_to_G( vbi, pfx, delta,
                                    "psubsb", Iop_QSub8Sx16, False );
         goto decode_success;
      }
      break;

   case 0xE9:
      
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_SSEint_E_to_G( vbi, pfx, delta,
                                    "psubsw", Iop_QSub16Sx8, False );
         goto decode_success;
      }
      break;

   case 0xEA:
      
      
      if (haveNo66noF2noF3(pfx) && sz == 4) {
         do_MMX_preamble();
         delta = dis_MMXop_regmem_to_reg ( 
                    vbi, pfx, delta, opc, "pminsw", False );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_SSEint_E_to_G( vbi, pfx, delta,
                                    "pminsw", Iop_Min16Sx8, False );
         goto decode_success;
      }
      break;

   case 0xEB:
      
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_SSE_E_to_G_all( vbi, pfx, delta, "por", Iop_OrV128 );
         goto decode_success;
      }
      break;

   case 0xEC:
      
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_SSEint_E_to_G( vbi, pfx, delta,
                                    "paddsb", Iop_QAdd8Sx16, False );
         goto decode_success;
      }
      break;

   case 0xED:
      
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_SSEint_E_to_G( vbi, pfx, delta,
                                    "paddsw", Iop_QAdd16Sx8, False );
         goto decode_success;
      }
      break;

   case 0xEE:
      
      
      if (haveNo66noF2noF3(pfx) && sz == 4) {
         do_MMX_preamble();
         delta = dis_MMXop_regmem_to_reg ( 
                    vbi, pfx, delta, opc, "pmaxsw", False );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_SSEint_E_to_G( vbi, pfx, delta,
                                    "pmaxsw", Iop_Max16Sx8, False );
         goto decode_success;
      }
      break;

   case 0xEF:
      
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_SSE_E_to_G_all( vbi, pfx, delta, "pxor", Iop_XorV128 );
         goto decode_success;
      }
      break;

   case 0xF1:
      
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_SSE_shiftG_byE( vbi, pfx, delta, "psllw", Iop_ShlN16x8 );
         goto decode_success;
      }
      break;

   case 0xF2:
      
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_SSE_shiftG_byE( vbi, pfx, delta, "pslld", Iop_ShlN32x4 );
         goto decode_success;
      }
      break;

   case 0xF3:
      
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_SSE_shiftG_byE( vbi, pfx, delta, "psllq", Iop_ShlN64x2 );
         goto decode_success;
      }
      break;

   case 0xF4:
      if (have66noF2noF3(pfx) && sz == 2) {
         IRTemp sV = newTemp(Ity_V128);
         IRTemp dV = newTemp(Ity_V128);
         modrm = getUChar(delta);
         UInt rG = gregOfRexRM(pfx,modrm);
         assign( dV, getXMMReg(rG) );
         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx,modrm);
            assign( sV, getXMMReg(rE) );
            delta += 1;
            DIP("pmuludq %s,%s\n", nameXMMReg(rE), nameXMMReg(rG));
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            assign( sV, loadLE(Ity_V128, mkexpr(addr)) );
            delta += alen;
            DIP("pmuludq %s,%s\n", dis_buf, nameXMMReg(rG));
         }
         putXMMReg( rG, mkexpr(math_PMULUDQ_128( sV, dV )) );
         goto decode_success;
      }
      
      if (haveNo66noF2noF3(pfx) && sz == 4) {
         IRTemp sV = newTemp(Ity_I64);
         IRTemp dV = newTemp(Ity_I64);
         t1 = newTemp(Ity_I32);
         t0 = newTemp(Ity_I32);
         modrm = getUChar(delta);

         do_MMX_preamble();
         assign( dV, getMMXReg(gregLO3ofRM(modrm)) );

         if (epartIsReg(modrm)) {
            assign( sV, getMMXReg(eregLO3ofRM(modrm)) );
            delta += 1;
            DIP("pmuludq %s,%s\n", nameMMXReg(eregLO3ofRM(modrm)),
                                   nameMMXReg(gregLO3ofRM(modrm)));
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            assign( sV, loadLE(Ity_I64, mkexpr(addr)) );
            delta += alen;
            DIP("pmuludq %s,%s\n", dis_buf,
                                   nameMMXReg(gregLO3ofRM(modrm)));
         }

         assign( t0, unop(Iop_64to32, mkexpr(dV)) );
         assign( t1, unop(Iop_64to32, mkexpr(sV)) );
         putMMXReg( gregLO3ofRM(modrm),
                    binop( Iop_MullU32, mkexpr(t0), mkexpr(t1) ) );
         goto decode_success;
      }
      break;

   case 0xF5:
      if (have66noF2noF3(pfx) && sz == 2) {
         IRTemp sV = newTemp(Ity_V128);
         IRTemp dV = newTemp(Ity_V128);
         modrm     = getUChar(delta);
         UInt   rG = gregOfRexRM(pfx,modrm);
         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx,modrm);
            assign( sV, getXMMReg(rE) );
            delta += 1;
            DIP("pmaddwd %s,%s\n", nameXMMReg(rE), nameXMMReg(rG));
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            assign( sV, loadLE(Ity_V128, mkexpr(addr)) );
            delta += alen;
            DIP("pmaddwd %s,%s\n", dis_buf, nameXMMReg(rG));
         }
         assign( dV, getXMMReg(rG) );
         putXMMReg( rG, mkexpr(math_PMADDWD_128(dV, sV)) );
         goto decode_success;
      }
      break;

   case 0xF6:
      
      
      if (haveNo66noF2noF3(pfx) && sz == 4) {
         do_MMX_preamble();
         delta = dis_MMXop_regmem_to_reg ( 
                    vbi, pfx, delta, opc, "psadbw", False );
         goto decode_success;
      }
      if (have66noF2noF3(pfx) && sz == 2) {
         IRTemp sV  = newTemp(Ity_V128);
         IRTemp dV  = newTemp(Ity_V128);
         modrm = getUChar(delta);
         UInt   rG   = gregOfRexRM(pfx,modrm);
         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx,modrm);
            assign( sV, getXMMReg(rE) );
            delta += 1;
            DIP("psadbw %s,%s\n", nameXMMReg(rE), nameXMMReg(rG));
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            assign( sV, loadLE(Ity_V128, mkexpr(addr)) );
            delta += alen;
            DIP("psadbw %s,%s\n", dis_buf, nameXMMReg(rG));
         }
         assign( dV, getXMMReg(rG) );
         putXMMReg( rG, mkexpr( math_PSADBW_128 ( dV, sV ) ) );

         goto decode_success;
      }
      break;

   case 0xF7:
      
      
      if (haveNo66noF2noF3(pfx) && sz == 4) {
         Bool ok = False;
         delta = dis_MMX( &ok, vbi, pfx, sz, delta-1 );
         if (ok) goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && sz == 2 && epartIsReg(getUChar(delta))) {
         delta = dis_MASKMOVDQU( vbi, pfx, delta, False );
         goto decode_success;
      }
      break;

   case 0xF8:
      
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_SSEint_E_to_G( vbi, pfx, delta, 
                                    "psubb", Iop_Sub8x16, False );
         goto decode_success;
      }
      break;

   case 0xF9:
      
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_SSEint_E_to_G( vbi, pfx, delta,
                                    "psubw", Iop_Sub16x8, False );
         goto decode_success;
      }
      break;

   case 0xFA:
      
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_SSEint_E_to_G( vbi, pfx, delta,
                                    "psubd", Iop_Sub32x4, False );
         goto decode_success;
      }
      break;

   case 0xFB:
      
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_SSEint_E_to_G( vbi, pfx, delta,
                                    "psubq", Iop_Sub64x2, False );
         goto decode_success;
      }
      
      
      if (haveNo66noF2noF3(pfx) && sz == 4) {
         do_MMX_preamble();
         delta = dis_MMXop_regmem_to_reg ( 
                   vbi, pfx, delta, opc, "psubq", False );
         goto decode_success;
      }
      break;

   case 0xFC:
      
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_SSEint_E_to_G( vbi, pfx, delta,
                                    "paddb", Iop_Add8x16, False );
         goto decode_success;
      }
      break;

   case 0xFD:
      
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_SSEint_E_to_G( vbi, pfx, delta,
                                    "paddw", Iop_Add16x8, False );
         goto decode_success;
      }
      break;

   case 0xFE:
      
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_SSEint_E_to_G( vbi, pfx, delta,
                                    "paddd", Iop_Add32x4, False );
         goto decode_success;
      }
      break;

   default:
      goto decode_failure;

   }

  decode_failure:
   *decode_OK = False;
   return deltaIN;

  decode_success:
   *decode_OK = True;
   return delta;
}



static Long dis_MOVDDUP_128 ( const VexAbiInfo* vbi, Prefix pfx,
                              Long delta, Bool isAvx )
{
   IRTemp addr   = IRTemp_INVALID;
   Int    alen   = 0;
   HChar  dis_buf[50];
   IRTemp sV    = newTemp(Ity_V128);
   IRTemp d0    = newTemp(Ity_I64);
   UChar  modrm = getUChar(delta);
   UInt   rG    = gregOfRexRM(pfx,modrm);
   if (epartIsReg(modrm)) {
      UInt rE = eregOfRexRM(pfx,modrm);
      assign( sV, getXMMReg(rE) );
      DIP("%smovddup %s,%s\n",
          isAvx ? "v" : "", nameXMMReg(rE), nameXMMReg(rG));
      delta += 1;
      assign ( d0, unop(Iop_V128to64, mkexpr(sV)) );
   } else {
      addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
      assign( d0, loadLE(Ity_I64, mkexpr(addr)) );
      DIP("%smovddup %s,%s\n",
          isAvx ? "v" : "", dis_buf, nameXMMReg(rG));
      delta += alen;
   }
   (isAvx ? putYMMRegLoAndZU : putXMMReg)
      ( rG, binop(Iop_64HLtoV128,mkexpr(d0),mkexpr(d0)) );
   return delta;
}


static Long dis_MOVDDUP_256 ( const VexAbiInfo* vbi, Prefix pfx,
                              Long delta )
{
   IRTemp addr   = IRTemp_INVALID;
   Int    alen   = 0;
   HChar  dis_buf[50];
   IRTemp d0    = newTemp(Ity_I64);
   IRTemp d1    = newTemp(Ity_I64);
   UChar  modrm = getUChar(delta);
   UInt   rG    = gregOfRexRM(pfx,modrm);
   if (epartIsReg(modrm)) {
      UInt rE = eregOfRexRM(pfx,modrm);
      DIP("vmovddup %s,%s\n", nameYMMReg(rE), nameYMMReg(rG));
      delta += 1;
      assign ( d0, getYMMRegLane64(rE, 0) );
      assign ( d1, getYMMRegLane64(rE, 2) );
   } else {
      addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
      assign( d0, loadLE(Ity_I64, mkexpr(addr)) );
      assign( d1, loadLE(Ity_I64, binop(Iop_Add64,
                                        mkexpr(addr), mkU64(16))) );
      DIP("vmovddup %s,%s\n", dis_buf, nameYMMReg(rG));
      delta += alen;
   }
   putYMMRegLane64( rG, 0, mkexpr(d0) );
   putYMMRegLane64( rG, 1, mkexpr(d0) );
   putYMMRegLane64( rG, 2, mkexpr(d1) );
   putYMMRegLane64( rG, 3, mkexpr(d1) );
   return delta;
}


static Long dis_MOVSxDUP_128 ( const VexAbiInfo* vbi, Prefix pfx,
                               Long delta, Bool isAvx, Bool isL )
{
   IRTemp addr  = IRTemp_INVALID;
   Int    alen  = 0;
   HChar  dis_buf[50];
   IRTemp sV    = newTemp(Ity_V128);
   UChar  modrm = getUChar(delta);
   UInt   rG    = gregOfRexRM(pfx,modrm);
   IRTemp s3, s2, s1, s0;
   s3 = s2 = s1 = s0 = IRTemp_INVALID;
   if (epartIsReg(modrm)) {
      UInt rE = eregOfRexRM(pfx,modrm);
      assign( sV, getXMMReg(rE) );
      DIP("%smovs%cdup %s,%s\n",
          isAvx ? "v" : "", isL ? 'l' : 'h', nameXMMReg(rE), nameXMMReg(rG));
      delta += 1;
   } else {
      addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
      if (!isAvx)
         gen_SEGV_if_not_16_aligned( addr );
      assign( sV, loadLE(Ity_V128, mkexpr(addr)) );
      DIP("%smovs%cdup %s,%s\n",
          isAvx ? "v" : "", isL ? 'l' : 'h', dis_buf, nameXMMReg(rG));
      delta += alen;
   }
   breakupV128to32s( sV, &s3, &s2, &s1, &s0 );
   (isAvx ? putYMMRegLoAndZU : putXMMReg)
      ( rG, isL ? mkV128from32s( s2, s2, s0, s0 )
                : mkV128from32s( s3, s3, s1, s1 ) );
   return delta;
}


static Long dis_MOVSxDUP_256 ( const VexAbiInfo* vbi, Prefix pfx,
                               Long delta, Bool isL )
{
   IRTemp addr  = IRTemp_INVALID;
   Int    alen  = 0;
   HChar  dis_buf[50];
   IRTemp sV    = newTemp(Ity_V256);
   UChar  modrm = getUChar(delta);
   UInt   rG    = gregOfRexRM(pfx,modrm);
   IRTemp s7, s6, s5, s4, s3, s2, s1, s0;
   s7 = s6 = s5 = s4 = s3 = s2 = s1 = s0 = IRTemp_INVALID;
   if (epartIsReg(modrm)) {
      UInt rE = eregOfRexRM(pfx,modrm);
      assign( sV, getYMMReg(rE) );
      DIP("vmovs%cdup %s,%s\n",
          isL ? 'l' : 'h', nameYMMReg(rE), nameYMMReg(rG));
      delta += 1;
   } else {
      addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
      assign( sV, loadLE(Ity_V256, mkexpr(addr)) );
      DIP("vmovs%cdup %s,%s\n",
          isL ? 'l' : 'h', dis_buf, nameYMMReg(rG));
      delta += alen;
   }
   breakupV256to32s( sV, &s7, &s6, &s5, &s4, &s3, &s2, &s1, &s0 );
   putYMMRegLane128( rG, 1, isL ? mkV128from32s( s6, s6, s4, s4 )
                                : mkV128from32s( s7, s7, s5, s5 ) );
   putYMMRegLane128( rG, 0, isL ? mkV128from32s( s2, s2, s0, s0 )
                                : mkV128from32s( s3, s3, s1, s1 ) );
   return delta;
}


static IRTemp math_HADDPS_128 ( IRTemp dV, IRTemp sV, Bool isAdd )
{
   IRTemp s3, s2, s1, s0, d3, d2, d1, d0;
   IRTemp leftV  = newTemp(Ity_V128);
   IRTemp rightV = newTemp(Ity_V128);
   IRTemp rm     = newTemp(Ity_I32);
   s3 = s2 = s1 = s0 = d3 = d2 = d1 = d0 = IRTemp_INVALID;

   breakupV128to32s( sV, &s3, &s2, &s1, &s0 );
   breakupV128to32s( dV, &d3, &d2, &d1, &d0 );

   assign( leftV,  mkV128from32s( s2, s0, d2, d0 ) );
   assign( rightV, mkV128from32s( s3, s1, d3, d1 ) );

   IRTemp res = newTemp(Ity_V128);
   assign( rm, get_FAKE_roundingmode() ); 
   assign( res, triop(isAdd ? Iop_Add32Fx4 : Iop_Sub32Fx4,
                      mkexpr(rm), mkexpr(leftV), mkexpr(rightV) ) );
   return res;
}


static IRTemp math_HADDPD_128 ( IRTemp dV, IRTemp sV, Bool isAdd )
{
   IRTemp s1, s0, d1, d0;
   IRTemp leftV  = newTemp(Ity_V128);
   IRTemp rightV = newTemp(Ity_V128);
   IRTemp rm     = newTemp(Ity_I32);
   s1 = s0 = d1 = d0 = IRTemp_INVALID;

   breakupV128to64s( sV, &s1, &s0 );
   breakupV128to64s( dV, &d1, &d0 );
   
   assign( leftV,  binop(Iop_64HLtoV128, mkexpr(s0), mkexpr(d0)) );
   assign( rightV, binop(Iop_64HLtoV128, mkexpr(s1), mkexpr(d1)) );

   IRTemp res = newTemp(Ity_V128);
   assign( rm, get_FAKE_roundingmode() ); 
   assign( res, triop(isAdd ? Iop_Add64Fx2 : Iop_Sub64Fx2,
                      mkexpr(rm), mkexpr(leftV), mkexpr(rightV) ) );
   return res;
}


__attribute__((noinline))
static
Long dis_ESC_0F__SSE3 ( Bool* decode_OK,
                        const VexAbiInfo* vbi,
                        Prefix pfx, Int sz, Long deltaIN )
{
   IRTemp addr  = IRTemp_INVALID;
   UChar  modrm = 0;
   Int    alen  = 0;
   HChar  dis_buf[50];

   *decode_OK = False;

   Long   delta = deltaIN;
   UChar  opc   = getUChar(delta);
   delta++;
   switch (opc) {

   case 0x12:
      if (haveF3no66noF2(pfx) && sz == 4) {
         delta = dis_MOVSxDUP_128( vbi, pfx, delta, False,
                                   True );
         goto decode_success;
      }
      if (haveF2no66noF3(pfx) 
          && (sz == 4 ||  sz == 8)) {
         delta = dis_MOVDDUP_128( vbi, pfx, delta, False );
         goto decode_success;
      }
      break;

   case 0x16:
      if (haveF3no66noF2(pfx) && sz == 4) {
         delta = dis_MOVSxDUP_128( vbi, pfx, delta, False,
                                   False );
         goto decode_success;
      }
      break;

   case 0x7C:
   case 0x7D:
      
      
      if (haveF2no66noF3(pfx) && sz == 4) {
         IRTemp eV     = newTemp(Ity_V128);
         IRTemp gV     = newTemp(Ity_V128);
         Bool   isAdd  = opc == 0x7C;
         const HChar* str = isAdd ? "add" : "sub";
         modrm         = getUChar(delta);
         UInt   rG     = gregOfRexRM(pfx,modrm);
         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx,modrm);
            assign( eV, getXMMReg(rE) );
            DIP("h%sps %s,%s\n", str, nameXMMReg(rE), nameXMMReg(rG));
            delta += 1;
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            assign( eV, loadLE(Ity_V128, mkexpr(addr)) );
            DIP("h%sps %s,%s\n", str, dis_buf, nameXMMReg(rG));
            delta += alen;
         }

         assign( gV, getXMMReg(rG) );
         putXMMReg( rG, mkexpr( math_HADDPS_128 ( gV, eV, isAdd ) ) );
         goto decode_success;
      }
      
      
      if (have66noF2noF3(pfx) && sz == 2) {
         IRTemp eV     = newTemp(Ity_V128);
         IRTemp gV     = newTemp(Ity_V128);
         Bool   isAdd  = opc == 0x7C;
         const HChar* str = isAdd ? "add" : "sub";
         modrm         = getUChar(delta);
         UInt   rG     = gregOfRexRM(pfx,modrm);
         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx,modrm);
            assign( eV, getXMMReg(rE) );
            DIP("h%spd %s,%s\n", str, nameXMMReg(rE), nameXMMReg(rG));
            delta += 1;
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            assign( eV, loadLE(Ity_V128, mkexpr(addr)) );
            DIP("h%spd %s,%s\n", str, dis_buf, nameXMMReg(rG));
            delta += alen;
         }

         assign( gV, getXMMReg(rG) );
         putXMMReg( rG, mkexpr( math_HADDPD_128 ( gV, eV, isAdd ) ) );
         goto decode_success;
      }
      break;

   case 0xD0:
      
      if (have66noF2noF3(pfx) && sz == 2) {
         IRTemp eV   = newTemp(Ity_V128);
         IRTemp gV   = newTemp(Ity_V128);
         modrm       = getUChar(delta);
         UInt   rG   = gregOfRexRM(pfx,modrm);
         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx,modrm);
            assign( eV, getXMMReg(rE) );
            DIP("addsubpd %s,%s\n", nameXMMReg(rE), nameXMMReg(rG));
            delta += 1;
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            assign( eV, loadLE(Ity_V128, mkexpr(addr)) );
            DIP("addsubpd %s,%s\n", dis_buf, nameXMMReg(rG));
            delta += alen;
         }

         assign( gV, getXMMReg(rG) );
         putXMMReg( rG, mkexpr( math_ADDSUBPD_128 ( gV, eV ) ) );
         goto decode_success;
      }
      
      if (haveF2no66noF3(pfx) && sz == 4) {
         IRTemp eV   = newTemp(Ity_V128);
         IRTemp gV   = newTemp(Ity_V128);
         modrm       = getUChar(delta);
         UInt   rG   = gregOfRexRM(pfx,modrm);

         modrm = getUChar(delta);
         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx,modrm);
            assign( eV, getXMMReg(rE) );
            DIP("addsubps %s,%s\n", nameXMMReg(rE), nameXMMReg(rG));
            delta += 1;
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            assign( eV, loadLE(Ity_V128, mkexpr(addr)) );
            DIP("addsubps %s,%s\n", dis_buf, nameXMMReg(rG));
            delta += alen;
         }

         assign( gV, getXMMReg(rG) );
         putXMMReg( rG, mkexpr( math_ADDSUBPS_128 ( gV, eV ) ) );
         goto decode_success;
      }
      break;

   case 0xF0:
      
      if (haveF2no66noF3(pfx) && sz == 4) {
         modrm = getUChar(delta);
         if (epartIsReg(modrm)) {
            goto decode_failure;
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            putXMMReg( gregOfRexRM(pfx,modrm), 
                       loadLE(Ity_V128, mkexpr(addr)) );
            DIP("lddqu %s,%s\n", dis_buf,
                                 nameXMMReg(gregOfRexRM(pfx,modrm)));
            delta += alen;
         }
         goto decode_success;
      }
      break;

   default:
      goto decode_failure;

   }

  decode_failure:
   *decode_OK = False;
   return deltaIN;

  decode_success:
   *decode_OK = True;
   return delta;
}



static
IRTemp math_PSHUFB_XMM ( IRTemp dV, IRTemp sV )
{
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
   IRTemp res        = newTemp(Ity_V128);

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

   assign(res, binop(Iop_64HLtoV128, mkexpr(rHi), mkexpr(rLo)));
   return res;
}


static
IRTemp math_PSHUFB_YMM ( IRTemp dV, IRTemp sV )
{
   IRTemp sHi, sLo, dHi, dLo;
   sHi = sLo = dHi = dLo = IRTemp_INVALID;
   breakupV256toV128s( dV, &dHi, &dLo);
   breakupV256toV128s( sV, &sHi, &sLo);
   IRTemp res = newTemp(Ity_V256);
   assign(res, binop(Iop_V128HLtoV256,
                     mkexpr(math_PSHUFB_XMM(dHi, sHi)),
                     mkexpr(math_PSHUFB_XMM(dLo, sLo))));
   return res;
}


static Long dis_PHADD_128 ( const VexAbiInfo* vbi, Prefix pfx, Long delta,
                            Bool isAvx, UChar opc )
{
   IRTemp addr   = IRTemp_INVALID;
   Int    alen   = 0;
   HChar  dis_buf[50];
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
   UChar  modrm  = getUChar(delta);
   UInt   rG     = gregOfRexRM(pfx,modrm);
   UInt   rV     = isAvx ? getVexNvvvv(pfx) : rG;

   switch (opc) {
      case 0x01: opV64 = Iop_Add16x4;   str = "addw";  break;
      case 0x02: opV64 = Iop_Add32x2;   str = "addd";  break;
      case 0x03: opV64 = Iop_QAdd16Sx4; str = "addsw"; break;
      case 0x05: opV64 = Iop_Sub16x4;   str = "subw";  break;
      case 0x06: opV64 = Iop_Sub32x2;   str = "subd";  break;
      case 0x07: opV64 = Iop_QSub16Sx4; str = "subsw"; break;
      default: vassert(0);
   }
   if (opc == 0x02 || opc == 0x06) {
      opCatO = Iop_InterleaveHI32x2;
      opCatE = Iop_InterleaveLO32x2;
   }

   assign( dV, getXMMReg(rV) );

   if (epartIsReg(modrm)) {
      UInt rE = eregOfRexRM(pfx,modrm);
      assign( sV, getXMMReg(rE) );
      DIP("%sph%s %s,%s\n", isAvx ? "v" : "", str,
          nameXMMReg(rE), nameXMMReg(rG));
      delta += 1;
   } else {
      addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
      if (!isAvx)
         gen_SEGV_if_not_16_aligned( addr );
      assign( sV, loadLE(Ity_V128, mkexpr(addr)) );
      DIP("%sph%s %s,%s\n", isAvx ? "v" : "", str,
          dis_buf, nameXMMReg(rG));
      delta += alen;
   }

   assign( dHi, unop(Iop_V128HIto64, mkexpr(dV)) );
   assign( dLo, unop(Iop_V128to64,   mkexpr(dV)) );
   assign( sHi, unop(Iop_V128HIto64, mkexpr(sV)) );
   assign( sLo, unop(Iop_V128to64,   mkexpr(sV)) );

   
   (isAvx ? putYMMRegLoAndZU : putXMMReg)
      ( rG, 
        binop(Iop_64HLtoV128,
              binop(opV64,
                    binop(opCatE,mkexpr(sHi),mkexpr(sLo)),
                    binop(opCatO,mkexpr(sHi),mkexpr(sLo)) ),
              binop(opV64,
                    binop(opCatE,mkexpr(dHi),mkexpr(dLo)),
                    binop(opCatO,mkexpr(dHi),mkexpr(dLo)) ) ) );
   return delta;
}


static Long dis_PHADD_256 ( const VexAbiInfo* vbi, Prefix pfx, Long delta,
                            UChar opc )
{
   IRTemp addr   = IRTemp_INVALID;
   Int    alen   = 0;
   HChar  dis_buf[50];
   const HChar* str = "???";
   IROp   opV64  = Iop_INVALID;
   IROp   opCatO = Iop_CatOddLanes16x4;
   IROp   opCatE = Iop_CatEvenLanes16x4;
   IRTemp sV     = newTemp(Ity_V256);
   IRTemp dV     = newTemp(Ity_V256);
   IRTemp s3, s2, s1, s0, d3, d2, d1, d0;
   s3 = s2 = s1 = s0 = d3 = d2 = d1 = d0 = IRTemp_INVALID;
   UChar  modrm  = getUChar(delta);
   UInt   rG     = gregOfRexRM(pfx,modrm);
   UInt   rV     = getVexNvvvv(pfx);

   switch (opc) {
      case 0x01: opV64 = Iop_Add16x4;   str = "addw";  break;
      case 0x02: opV64 = Iop_Add32x2;   str = "addd";  break;
      case 0x03: opV64 = Iop_QAdd16Sx4; str = "addsw"; break;
      case 0x05: opV64 = Iop_Sub16x4;   str = "subw";  break;
      case 0x06: opV64 = Iop_Sub32x2;   str = "subd";  break;
      case 0x07: opV64 = Iop_QSub16Sx4; str = "subsw"; break;
      default: vassert(0);
   }
   if (opc == 0x02 || opc == 0x06) {
      opCatO = Iop_InterleaveHI32x2;
      opCatE = Iop_InterleaveLO32x2;
   }

   assign( dV, getYMMReg(rV) );

   if (epartIsReg(modrm)) {
      UInt rE = eregOfRexRM(pfx,modrm);
      assign( sV, getYMMReg(rE) );
      DIP("vph%s %s,%s\n", str, nameYMMReg(rE), nameYMMReg(rG));
      delta += 1;
   } else {
      addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
      assign( sV, loadLE(Ity_V256, mkexpr(addr)) );
      DIP("vph%s %s,%s\n", str, dis_buf, nameYMMReg(rG));
      delta += alen;
   }

   breakupV256to64s( dV, &d3, &d2, &d1, &d0 );
   breakupV256to64s( sV, &s3, &s2, &s1, &s0 );


   putYMMReg( rG,
              binop(Iop_V128HLtoV256,
                    binop(Iop_64HLtoV128,
                          binop(opV64,
                                binop(opCatE,mkexpr(s3),mkexpr(s2)),
                                binop(opCatO,mkexpr(s3),mkexpr(s2)) ),
                          binop(opV64,
                                binop(opCatE,mkexpr(d3),mkexpr(d2)),
                                binop(opCatO,mkexpr(d3),mkexpr(d2)) ) ),
                    binop(Iop_64HLtoV128,
                          binop(opV64,
                                binop(opCatE,mkexpr(s1),mkexpr(s0)),
                                binop(opCatO,mkexpr(s1),mkexpr(s0)) ),
                          binop(opV64,
                                binop(opCatE,mkexpr(d1),mkexpr(d0)),
                                binop(opCatO,mkexpr(d1),mkexpr(d0)) ) ) ) );
   return delta;
}


static IRTemp math_PMADDUBSW_128 ( IRTemp dV, IRTemp sV )
{
   IRTemp sVoddsSX  = newTemp(Ity_V128);
   IRTemp sVevensSX = newTemp(Ity_V128);
   IRTemp dVoddsZX  = newTemp(Ity_V128);
   IRTemp dVevensZX = newTemp(Ity_V128);
   
   assign( sVoddsSX, binop(Iop_SarN16x8, mkexpr(sV), mkU8(8)) );
   assign( sVevensSX, binop(Iop_SarN16x8, 
                            binop(Iop_ShlN16x8, mkexpr(sV), mkU8(8)),
                            mkU8(8)) );
   assign( dVoddsZX, binop(Iop_ShrN16x8, mkexpr(dV), mkU8(8)) );
   assign( dVevensZX, binop(Iop_ShrN16x8,
                            binop(Iop_ShlN16x8, mkexpr(dV), mkU8(8)),
                            mkU8(8)) );

   IRTemp res = newTemp(Ity_V128);
   assign( res, binop(Iop_QAdd16Sx8,
                      binop(Iop_Mul16x8, mkexpr(sVoddsSX), mkexpr(dVoddsZX)),
                      binop(Iop_Mul16x8, mkexpr(sVevensSX), mkexpr(dVevensZX))
                     )
         );
   return res;
}


static
IRTemp math_PMADDUBSW_256 ( IRTemp dV, IRTemp sV )
{
   IRTemp sHi, sLo, dHi, dLo;
   sHi = sLo = dHi = dLo = IRTemp_INVALID;
   breakupV256toV128s( dV, &dHi, &dLo);
   breakupV256toV128s( sV, &sHi, &sLo);
   IRTemp res = newTemp(Ity_V256);
   assign(res, binop(Iop_V128HLtoV256,
                     mkexpr(math_PMADDUBSW_128(dHi, sHi)),
                     mkexpr(math_PMADDUBSW_128(dLo, sLo))));
   return res;
}


__attribute__((noinline))
static
Long dis_ESC_0F38__SupSSE3 ( Bool* decode_OK,
                             const VexAbiInfo* vbi,
                             Prefix pfx, Int sz, Long deltaIN )
{
   IRTemp addr  = IRTemp_INVALID;
   UChar  modrm = 0;
   Int    alen  = 0;
   HChar  dis_buf[50];

   *decode_OK = False;

   Long   delta = deltaIN;
   UChar  opc   = getUChar(delta);
   delta++;
   switch (opc) {

   case 0x00:
      
      if (have66noF2noF3(pfx) 
          && (sz == 2 ||  sz == 8)) {
         IRTemp sV = newTemp(Ity_V128);
         IRTemp dV = newTemp(Ity_V128);

         modrm = getUChar(delta);
         assign( dV, getXMMReg(gregOfRexRM(pfx,modrm)) );

         if (epartIsReg(modrm)) {
            assign( sV, getXMMReg(eregOfRexRM(pfx,modrm)) );
            delta += 1;
            DIP("pshufb %s,%s\n", nameXMMReg(eregOfRexRM(pfx,modrm)),
                                  nameXMMReg(gregOfRexRM(pfx,modrm)));
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            gen_SEGV_if_not_16_aligned( addr );
            assign( sV, loadLE(Ity_V128, mkexpr(addr)) );
            delta += alen;
            DIP("pshufb %s,%s\n", dis_buf,
                                  nameXMMReg(gregOfRexRM(pfx,modrm)));
         }

         IRTemp res = math_PSHUFB_XMM( dV, sV );
         putXMMReg(gregOfRexRM(pfx,modrm), mkexpr(res));
         goto decode_success;
      }
      
      if (haveNo66noF2noF3(pfx) && sz == 4) {
         IRTemp sV      = newTemp(Ity_I64);
         IRTemp dV      = newTemp(Ity_I64);

         modrm = getUChar(delta);
         do_MMX_preamble();
         assign( dV, getMMXReg(gregLO3ofRM(modrm)) );

         if (epartIsReg(modrm)) {
            assign( sV, getMMXReg(eregLO3ofRM(modrm)) );
            delta += 1;
            DIP("pshufb %s,%s\n", nameMMXReg(eregLO3ofRM(modrm)),
                                  nameMMXReg(gregLO3ofRM(modrm)));
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            assign( sV, loadLE(Ity_I64, mkexpr(addr)) );
            delta += alen;
            DIP("pshufb %s,%s\n", dis_buf,
                                  nameMMXReg(gregLO3ofRM(modrm)));
         }

         putMMXReg(
            gregLO3ofRM(modrm),
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
      break;

   case 0x01:
   case 0x02:
   case 0x03:
   case 0x05:
   case 0x06:
   case 0x07:
      if (have66noF2noF3(pfx) 
          && (sz == 2 ||  sz == 8)) {
         delta = dis_PHADD_128( vbi, pfx, delta, False, opc );
         goto decode_success;
      }
      
      if (haveNo66noF2noF3(pfx) && sz == 4) {
         const HChar* str = "???";
         IROp   opV64  = Iop_INVALID;
         IROp   opCatO = Iop_CatOddLanes16x4;
         IROp   opCatE = Iop_CatEvenLanes16x4;
         IRTemp sV     = newTemp(Ity_I64);
         IRTemp dV     = newTemp(Ity_I64);

         modrm = getUChar(delta);

         switch (opc) {
            case 0x01: opV64 = Iop_Add16x4;   str = "addw";  break;
            case 0x02: opV64 = Iop_Add32x2;   str = "addd";  break;
            case 0x03: opV64 = Iop_QAdd16Sx4; str = "addsw"; break;
            case 0x05: opV64 = Iop_Sub16x4;   str = "subw";  break;
            case 0x06: opV64 = Iop_Sub32x2;   str = "subd";  break;
            case 0x07: opV64 = Iop_QSub16Sx4; str = "subsw"; break;
            default: vassert(0);
         }
         if (opc == 0x02 || opc == 0x06) {
            opCatO = Iop_InterleaveHI32x2;
            opCatE = Iop_InterleaveLO32x2;
         }

         do_MMX_preamble();
         assign( dV, getMMXReg(gregLO3ofRM(modrm)) );

         if (epartIsReg(modrm)) {
            assign( sV, getMMXReg(eregLO3ofRM(modrm)) );
            delta += 1;
            DIP("ph%s %s,%s\n", str, nameMMXReg(eregLO3ofRM(modrm)),
                                     nameMMXReg(gregLO3ofRM(modrm)));
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            assign( sV, loadLE(Ity_I64, mkexpr(addr)) );
            delta += alen;
            DIP("ph%s %s,%s\n", str, dis_buf,
                                     nameMMXReg(gregLO3ofRM(modrm)));
         }

         putMMXReg(
            gregLO3ofRM(modrm),
            binop(opV64,
                  binop(opCatE,mkexpr(sV),mkexpr(dV)),
                  binop(opCatO,mkexpr(sV),mkexpr(dV))
            )
         );
         goto decode_success;
      }
      break;

   case 0x04:
      if (have66noF2noF3(pfx) 
          && (sz == 2 ||  sz == 8)) {
         IRTemp sV = newTemp(Ity_V128);
         IRTemp dV = newTemp(Ity_V128);
         modrm     = getUChar(delta);
         UInt   rG = gregOfRexRM(pfx,modrm);

         assign( dV, getXMMReg(rG) );

         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx,modrm);
            assign( sV, getXMMReg(rE) );
            delta += 1;
            DIP("pmaddubsw %s,%s\n", nameXMMReg(rE), nameXMMReg(rG));
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            gen_SEGV_if_not_16_aligned( addr );
            assign( sV, loadLE(Ity_V128, mkexpr(addr)) );
            delta += alen;
            DIP("pmaddubsw %s,%s\n", dis_buf, nameXMMReg(rG));
         }

         putXMMReg( rG, mkexpr( math_PMADDUBSW_128( dV, sV ) ) );
         goto decode_success;
      }
      if (haveNo66noF2noF3(pfx) && sz == 4) {
         IRTemp sV        = newTemp(Ity_I64);
         IRTemp dV        = newTemp(Ity_I64);
         IRTemp sVoddsSX  = newTemp(Ity_I64);
         IRTemp sVevensSX = newTemp(Ity_I64);
         IRTemp dVoddsZX  = newTemp(Ity_I64);
         IRTemp dVevensZX = newTemp(Ity_I64);

         modrm = getUChar(delta);
         do_MMX_preamble();
         assign( dV, getMMXReg(gregLO3ofRM(modrm)) );

         if (epartIsReg(modrm)) {
            assign( sV, getMMXReg(eregLO3ofRM(modrm)) );
            delta += 1;
            DIP("pmaddubsw %s,%s\n", nameMMXReg(eregLO3ofRM(modrm)),
                                     nameMMXReg(gregLO3ofRM(modrm)));
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            assign( sV, loadLE(Ity_I64, mkexpr(addr)) );
            delta += alen;
            DIP("pmaddubsw %s,%s\n", dis_buf,
                                     nameMMXReg(gregLO3ofRM(modrm)));
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
            gregLO3ofRM(modrm),
            binop(Iop_QAdd16Sx4,
                  binop(Iop_Mul16x4, mkexpr(sVoddsSX), mkexpr(dVoddsZX)),
                  binop(Iop_Mul16x4, mkexpr(sVevensSX), mkexpr(dVevensZX))
            )
         );
         goto decode_success;
      }
      break;

   case 0x08:
   case 0x09:
   case 0x0A:
      
      
      
      if (have66noF2noF3(pfx) 
          && (sz == 2 ||  sz == 8)) {
         IRTemp sV      = newTemp(Ity_V128);
         IRTemp dV      = newTemp(Ity_V128);
         IRTemp sHi     = newTemp(Ity_I64);
         IRTemp sLo     = newTemp(Ity_I64);
         IRTemp dHi     = newTemp(Ity_I64);
         IRTemp dLo     = newTemp(Ity_I64);
         const HChar* str = "???";
         Int    laneszB = 0;

         switch (opc) {
            case 0x08: laneszB = 1; str = "b"; break;
            case 0x09: laneszB = 2; str = "w"; break;
            case 0x0A: laneszB = 4; str = "d"; break;
            default: vassert(0);
         }

         modrm = getUChar(delta);
         assign( dV, getXMMReg(gregOfRexRM(pfx,modrm)) );

         if (epartIsReg(modrm)) {
            assign( sV, getXMMReg(eregOfRexRM(pfx,modrm)) );
            delta += 1;
            DIP("psign%s %s,%s\n", str, nameXMMReg(eregOfRexRM(pfx,modrm)),
                                        nameXMMReg(gregOfRexRM(pfx,modrm)));
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            gen_SEGV_if_not_16_aligned( addr );
            assign( sV, loadLE(Ity_V128, mkexpr(addr)) );
            delta += alen;
            DIP("psign%s %s,%s\n", str, dis_buf,
                                        nameXMMReg(gregOfRexRM(pfx,modrm)));
         }

         assign( dHi, unop(Iop_V128HIto64, mkexpr(dV)) );
         assign( dLo, unop(Iop_V128to64,   mkexpr(dV)) );
         assign( sHi, unop(Iop_V128HIto64, mkexpr(sV)) );
         assign( sLo, unop(Iop_V128to64,   mkexpr(sV)) );

         putXMMReg(
            gregOfRexRM(pfx,modrm),
            binop(Iop_64HLtoV128,
                  dis_PSIGN_helper( mkexpr(sHi), mkexpr(dHi), laneszB ),
                  dis_PSIGN_helper( mkexpr(sLo), mkexpr(dLo), laneszB )
            )
         );
         goto decode_success;
      }
      
      
      
      if (haveNo66noF2noF3(pfx) && sz == 4) {
         IRTemp sV      = newTemp(Ity_I64);
         IRTemp dV      = newTemp(Ity_I64);
         const HChar* str = "???";
         Int    laneszB = 0;

         switch (opc) {
            case 0x08: laneszB = 1; str = "b"; break;
            case 0x09: laneszB = 2; str = "w"; break;
            case 0x0A: laneszB = 4; str = "d"; break;
            default: vassert(0);
         }

         modrm = getUChar(delta);
         do_MMX_preamble();
         assign( dV, getMMXReg(gregLO3ofRM(modrm)) );

         if (epartIsReg(modrm)) {
            assign( sV, getMMXReg(eregLO3ofRM(modrm)) );
            delta += 1;
            DIP("psign%s %s,%s\n", str, nameMMXReg(eregLO3ofRM(modrm)),
                                        nameMMXReg(gregLO3ofRM(modrm)));
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            assign( sV, loadLE(Ity_I64, mkexpr(addr)) );
            delta += alen;
            DIP("psign%s %s,%s\n", str, dis_buf,
                                        nameMMXReg(gregLO3ofRM(modrm)));
         }

         putMMXReg(
            gregLO3ofRM(modrm),
            dis_PSIGN_helper( mkexpr(sV), mkexpr(dV), laneszB )
         );
         goto decode_success;
      }
      break;

   case 0x0B:
      if (have66noF2noF3(pfx)
          && (sz == 2 ||  sz == 8)) {
         IRTemp sV  = newTemp(Ity_V128);
         IRTemp dV  = newTemp(Ity_V128);
         IRTemp sHi = newTemp(Ity_I64);
         IRTemp sLo = newTemp(Ity_I64);
         IRTemp dHi = newTemp(Ity_I64);
         IRTemp dLo = newTemp(Ity_I64);

         modrm = getUChar(delta);
         assign( dV, getXMMReg(gregOfRexRM(pfx,modrm)) );

         if (epartIsReg(modrm)) {
            assign( sV, getXMMReg(eregOfRexRM(pfx,modrm)) );
            delta += 1;
            DIP("pmulhrsw %s,%s\n", nameXMMReg(eregOfRexRM(pfx,modrm)),
                                    nameXMMReg(gregOfRexRM(pfx,modrm)));
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            gen_SEGV_if_not_16_aligned( addr );
            assign( sV, loadLE(Ity_V128, mkexpr(addr)) );
            delta += alen;
            DIP("pmulhrsw %s,%s\n", dis_buf,
                                    nameXMMReg(gregOfRexRM(pfx,modrm)));
         }

         assign( dHi, unop(Iop_V128HIto64, mkexpr(dV)) );
         assign( dLo, unop(Iop_V128to64,   mkexpr(dV)) );
         assign( sHi, unop(Iop_V128HIto64, mkexpr(sV)) );
         assign( sLo, unop(Iop_V128to64,   mkexpr(sV)) );

         putXMMReg(
            gregOfRexRM(pfx,modrm),
            binop(Iop_64HLtoV128,
                  dis_PMULHRSW_helper( mkexpr(sHi), mkexpr(dHi) ),
                  dis_PMULHRSW_helper( mkexpr(sLo), mkexpr(dLo) )
            )
         );
         goto decode_success;
      }
      if (haveNo66noF2noF3(pfx) && sz == 4) {
         IRTemp sV = newTemp(Ity_I64);
         IRTemp dV = newTemp(Ity_I64);

         modrm = getUChar(delta);
         do_MMX_preamble();
         assign( dV, getMMXReg(gregLO3ofRM(modrm)) );

         if (epartIsReg(modrm)) {
            assign( sV, getMMXReg(eregLO3ofRM(modrm)) );
            delta += 1;
            DIP("pmulhrsw %s,%s\n", nameMMXReg(eregLO3ofRM(modrm)),
                                    nameMMXReg(gregLO3ofRM(modrm)));
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            assign( sV, loadLE(Ity_I64, mkexpr(addr)) );
            delta += alen;
            DIP("pmulhrsw %s,%s\n", dis_buf,
                                    nameMMXReg(gregLO3ofRM(modrm)));
         }

         putMMXReg(
            gregLO3ofRM(modrm),
            dis_PMULHRSW_helper( mkexpr(sV), mkexpr(dV) )
         );
         goto decode_success;
      }
      break;

   case 0x1C:
   case 0x1D:
   case 0x1E:
      
      
      
      if (have66noF2noF3(pfx) 
          && (sz == 2 ||  sz == 8)) {
         IRTemp sV  = newTemp(Ity_V128);
         const HChar* str = "???";
         Int    laneszB = 0;

         switch (opc) {
            case 0x1C: laneszB = 1; str = "b"; break;
            case 0x1D: laneszB = 2; str = "w"; break;
            case 0x1E: laneszB = 4; str = "d"; break;
            default: vassert(0);
         }

         modrm = getUChar(delta);
         if (epartIsReg(modrm)) {
            assign( sV, getXMMReg(eregOfRexRM(pfx,modrm)) );
            delta += 1;
            DIP("pabs%s %s,%s\n", str, nameXMMReg(eregOfRexRM(pfx,modrm)),
                                       nameXMMReg(gregOfRexRM(pfx,modrm)));
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            gen_SEGV_if_not_16_aligned( addr );
            assign( sV, loadLE(Ity_V128, mkexpr(addr)) );
            delta += alen;
            DIP("pabs%s %s,%s\n", str, dis_buf,
                                       nameXMMReg(gregOfRexRM(pfx,modrm)));
         }

         putXMMReg( gregOfRexRM(pfx,modrm),
                    mkexpr(math_PABS_XMM(sV, laneszB)) );
         goto decode_success;
      }
      
      
      
      if (haveNo66noF2noF3(pfx) && sz == 4) {
         IRTemp sV      = newTemp(Ity_I64);
         const HChar* str = "???";
         Int    laneszB = 0;

         switch (opc) {
            case 0x1C: laneszB = 1; str = "b"; break;
            case 0x1D: laneszB = 2; str = "w"; break;
            case 0x1E: laneszB = 4; str = "d"; break;
            default: vassert(0);
         }

         modrm = getUChar(delta);
         do_MMX_preamble();

         if (epartIsReg(modrm)) {
            assign( sV, getMMXReg(eregLO3ofRM(modrm)) );
            delta += 1;
            DIP("pabs%s %s,%s\n", str, nameMMXReg(eregLO3ofRM(modrm)),
                                       nameMMXReg(gregLO3ofRM(modrm)));
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            assign( sV, loadLE(Ity_I64, mkexpr(addr)) );
            delta += alen;
            DIP("pabs%s %s,%s\n", str, dis_buf,
                                       nameMMXReg(gregLO3ofRM(modrm)));
         }

         putMMXReg( gregLO3ofRM(modrm),
                    mkexpr(math_PABS_MMX( sV, laneszB )) );
         goto decode_success;
      }
      break;

   default:
      break;

   }

  
   *decode_OK = False;
   return deltaIN;

  decode_success:
   *decode_OK = True;
   return delta;
}



__attribute__((noinline))
static
Long dis_ESC_0F3A__SupSSE3 ( Bool* decode_OK,
                             const VexAbiInfo* vbi,
                             Prefix pfx, Int sz, Long deltaIN )
{
   Long   d64   = 0;
   IRTemp addr  = IRTemp_INVALID;
   UChar  modrm = 0;
   Int    alen  = 0;
   HChar  dis_buf[50];

   *decode_OK = False;

   Long   delta = deltaIN;
   UChar  opc   = getUChar(delta);
   delta++;
   switch (opc) {

   case 0x0F:
      
      if (have66noF2noF3(pfx) 
          && (sz == 2 ||  sz == 8)) {
         IRTemp sV  = newTemp(Ity_V128);
         IRTemp dV  = newTemp(Ity_V128);

         modrm = getUChar(delta);
         assign( dV, getXMMReg(gregOfRexRM(pfx,modrm)) );

         if (epartIsReg(modrm)) {
            assign( sV, getXMMReg(eregOfRexRM(pfx,modrm)) );
            d64 = (Long)getUChar(delta+1);
            delta += 1+1;
            DIP("palignr $%d,%s,%s\n", (Int)d64,
                                       nameXMMReg(eregOfRexRM(pfx,modrm)),
                                       nameXMMReg(gregOfRexRM(pfx,modrm)));
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 1 );
            gen_SEGV_if_not_16_aligned( addr );
            assign( sV, loadLE(Ity_V128, mkexpr(addr)) );
            d64 = (Long)getUChar(delta+alen);
            delta += alen+1;
            DIP("palignr $%d,%s,%s\n", (Int)d64,
                                       dis_buf,
                                       nameXMMReg(gregOfRexRM(pfx,modrm)));
         }

         IRTemp res = math_PALIGNR_XMM( sV, dV, d64 );
         putXMMReg( gregOfRexRM(pfx,modrm), mkexpr(res) );
         goto decode_success;
      }
      
      if (haveNo66noF2noF3(pfx) && sz == 4) {
         IRTemp sV  = newTemp(Ity_I64);
         IRTemp dV  = newTemp(Ity_I64);
         IRTemp res = newTemp(Ity_I64);

         modrm = getUChar(delta);
         do_MMX_preamble();
         assign( dV, getMMXReg(gregLO3ofRM(modrm)) );

         if (epartIsReg(modrm)) {
            assign( sV, getMMXReg(eregLO3ofRM(modrm)) );
            d64 = (Long)getUChar(delta+1);
            delta += 1+1;
            DIP("palignr $%d,%s,%s\n",  (Int)d64, 
                                        nameMMXReg(eregLO3ofRM(modrm)),
                                        nameMMXReg(gregLO3ofRM(modrm)));
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 1 );
            assign( sV, loadLE(Ity_I64, mkexpr(addr)) );
            d64 = (Long)getUChar(delta+alen);
            delta += alen+1;
            DIP("palignr $%d%s,%s\n", (Int)d64,
                                      dis_buf,
                                      nameMMXReg(gregLO3ofRM(modrm)));
         }

         if (d64 == 0) {
            assign( res, mkexpr(sV) );
         }
         else if (d64 >= 1 && d64 <= 7) {
            assign(res, 
                   binop(Iop_Or64,
                         binop(Iop_Shr64, mkexpr(sV), mkU8(8*d64)),
                         binop(Iop_Shl64, mkexpr(dV), mkU8(8*(8-d64))
                        )));
         }
         else if (d64 == 8) {
           assign( res, mkexpr(dV) );
         }
         else if (d64 >= 9 && d64 <= 15) {
            assign( res, binop(Iop_Shr64, mkexpr(dV), mkU8(8*(d64-8))) );
         }
         else if (d64 >= 16 && d64 <= 255) {
            assign( res, mkU64(0) );
         }
         else
            vassert(0);

         putMMXReg( gregLO3ofRM(modrm), mkexpr(res) );
         goto decode_success;
      }
      break;

   default:
      break;

   }

  
   *decode_OK = False;
   return deltaIN;

  decode_success:
   *decode_OK = True;
   return delta;
}



__attribute__((noinline))
static
Long dis_ESC_0F__SSE4 ( Bool* decode_OK,
                        const VexArchInfo* archinfo,
                        const VexAbiInfo* vbi,
                        Prefix pfx, Int sz, Long deltaIN )
{
   IRTemp addr  = IRTemp_INVALID;
   IRType ty    = Ity_INVALID;
   UChar  modrm = 0;
   Int    alen  = 0;
   HChar  dis_buf[50];

   *decode_OK = False;

   Long   delta = deltaIN;
   UChar  opc   = getUChar(delta);
   delta++;
   switch (opc) {

   case 0xB8:
      if (haveF3noF2(pfx) 
          && (sz == 2 || sz == 4 || sz == 8)) {
          ty  = szToITy(sz);
         IRTemp     src = newTemp(ty);
         modrm = getUChar(delta);
         if (epartIsReg(modrm)) {
            assign(src, getIRegE(sz, pfx, modrm));
            delta += 1;
            DIP("popcnt%c %s, %s\n", nameISize(sz), nameIRegE(sz, pfx, modrm),
                nameIRegG(sz, pfx, modrm));
         } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 0);
            assign(src, loadLE(ty, mkexpr(addr)));
            delta += alen;
            DIP("popcnt%c %s, %s\n", nameISize(sz), dis_buf,
                nameIRegG(sz, pfx, modrm));
         }

         IRTemp result = gen_POPCOUNT(ty, src);
         putIRegG(sz, pfx, modrm, mkexpr(result));

         
         
         
         stmt( IRStmt_Put( OFFB_CC_OP,   mkU64(AMD64G_CC_OP_COPY) ));
         stmt( IRStmt_Put( OFFB_CC_DEP2, mkU64(0) ));
         stmt( IRStmt_Put( OFFB_CC_NDEP, mkU64(0) ));
         stmt( IRStmt_Put( OFFB_CC_DEP1,
               binop(Iop_Shl64,
                     unop(Iop_1Uto64,
                          binop(Iop_CmpEQ64,
                                widenUto64(mkexpr(src)),
                                mkU64(0))),
                     mkU8(AMD64G_CC_SHIFT_Z))));

         goto decode_success;
      }
      break;

   case 0xBC:
      if (haveF3noF2(pfx) 
          && (sz == 2 || sz == 4 || sz == 8)
          && 0 != (archinfo->hwcaps & VEX_HWCAPS_AMD64_BMI)) {
          ty  = szToITy(sz);
         IRTemp     src = newTemp(ty);
         modrm = getUChar(delta);
         if (epartIsReg(modrm)) {
            assign(src, getIRegE(sz, pfx, modrm));
            delta += 1;
            DIP("tzcnt%c %s, %s\n", nameISize(sz), nameIRegE(sz, pfx, modrm),
                nameIRegG(sz, pfx, modrm));
         } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 0);
            assign(src, loadLE(ty, mkexpr(addr)));
            delta += alen;
            DIP("tzcnt%c %s, %s\n", nameISize(sz), dis_buf,
                nameIRegG(sz, pfx, modrm));
         }

         IRTemp res = gen_TZCNT(ty, src);
         putIRegG(sz, pfx, modrm, mkexpr(res));

         
         
         
         
         IRTemp src64 = newTemp(Ity_I64);
         IRTemp res64 = newTemp(Ity_I64);
         assign(src64, widenUto64(mkexpr(src)));
         assign(res64, widenUto64(mkexpr(res)));

         IRTemp oszacp = newTemp(Ity_I64);
         assign(
            oszacp,
            binop(Iop_Or64,
                  binop(Iop_Shl64,
                        unop(Iop_1Uto64,
                             binop(Iop_CmpEQ64, mkexpr(res64), mkU64(0))),
                        mkU8(AMD64G_CC_SHIFT_Z)),
                  binop(Iop_Shl64,
                        unop(Iop_1Uto64,
                             binop(Iop_CmpEQ64, mkexpr(src64), mkU64(0))),
                        mkU8(AMD64G_CC_SHIFT_C))
            )
         );

         stmt( IRStmt_Put( OFFB_CC_OP,   mkU64(AMD64G_CC_OP_COPY) ));
         stmt( IRStmt_Put( OFFB_CC_DEP2, mkU64(0) ));
         stmt( IRStmt_Put( OFFB_CC_NDEP, mkU64(0) ));
         stmt( IRStmt_Put( OFFB_CC_DEP1, mkexpr(oszacp) ));

         goto decode_success;
      }
      break;

   case 0xBD:
      if (haveF3noF2(pfx) 
          && (sz == 2 || sz == 4 || sz == 8) 
          && 0 != (archinfo->hwcaps & VEX_HWCAPS_AMD64_LZCNT)) {
          ty  = szToITy(sz);
         IRTemp     src = newTemp(ty);
         modrm = getUChar(delta);
         if (epartIsReg(modrm)) {
            assign(src, getIRegE(sz, pfx, modrm));
            delta += 1;
            DIP("lzcnt%c %s, %s\n", nameISize(sz), nameIRegE(sz, pfx, modrm),
                nameIRegG(sz, pfx, modrm));
         } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 0);
            assign(src, loadLE(ty, mkexpr(addr)));
            delta += alen;
            DIP("lzcnt%c %s, %s\n", nameISize(sz), dis_buf,
                nameIRegG(sz, pfx, modrm));
         }

         IRTemp res = gen_LZCNT(ty, src);
         putIRegG(sz, pfx, modrm, mkexpr(res));

         
         
         
         
         IRTemp src64 = newTemp(Ity_I64);
         IRTemp res64 = newTemp(Ity_I64);
         assign(src64, widenUto64(mkexpr(src)));
         assign(res64, widenUto64(mkexpr(res)));

         IRTemp oszacp = newTemp(Ity_I64);
         assign(
            oszacp,
            binop(Iop_Or64,
                  binop(Iop_Shl64,
                        unop(Iop_1Uto64,
                             binop(Iop_CmpEQ64, mkexpr(res64), mkU64(0))),
                        mkU8(AMD64G_CC_SHIFT_Z)),
                  binop(Iop_Shl64,
                        unop(Iop_1Uto64,
                             binop(Iop_CmpEQ64, mkexpr(src64), mkU64(0))),
                        mkU8(AMD64G_CC_SHIFT_C))
            )
         );

         stmt( IRStmt_Put( OFFB_CC_OP,   mkU64(AMD64G_CC_OP_COPY) ));
         stmt( IRStmt_Put( OFFB_CC_DEP2, mkU64(0) ));
         stmt( IRStmt_Put( OFFB_CC_NDEP, mkU64(0) ));
         stmt( IRStmt_Put( OFFB_CC_DEP1, mkexpr(oszacp) ));

         goto decode_success;
      }
      break;

   default:
      break;

   }

  
   *decode_OK = False;
   return deltaIN;

  decode_success:
   *decode_OK = True;
   return delta;
}



static IRTemp math_PBLENDVB_128 ( IRTemp vecE, IRTemp vecG,
                                  IRTemp vec0,
                                  UInt gran, IROp opSAR )
{
   IRTemp sh = newTemp(Ity_I8);
   assign(sh, mkU8(8 * gran - 1));

   IRTemp mask = newTemp(Ity_V128);
   assign(mask, binop(opSAR, mkexpr(vec0), mkexpr(sh)));

   IRTemp notmask = newTemp(Ity_V128);
   assign(notmask, unop(Iop_NotV128, mkexpr(mask)));

   IRTemp res = newTemp(Ity_V128);
   assign(res,  binop(Iop_OrV128,
                      binop(Iop_AndV128, mkexpr(vecE), mkexpr(mask)),
                      binop(Iop_AndV128, mkexpr(vecG), mkexpr(notmask))));
   return res;
}

static IRTemp math_PBLENDVB_256 ( IRTemp vecE, IRTemp vecG,
                                  IRTemp vec0,
                                  UInt gran, IROp opSAR128 )
{
   IRTemp sh = newTemp(Ity_I8);
   assign(sh, mkU8(8 * gran - 1));

   IRTemp vec0Hi = IRTemp_INVALID;
   IRTemp vec0Lo = IRTemp_INVALID;
   breakupV256toV128s( vec0, &vec0Hi, &vec0Lo );

   IRTemp mask = newTemp(Ity_V256);
   assign(mask, binop(Iop_V128HLtoV256,
                      binop(opSAR128, mkexpr(vec0Hi), mkexpr(sh)),
                      binop(opSAR128, mkexpr(vec0Lo), mkexpr(sh))));

   IRTemp notmask = newTemp(Ity_V256);
   assign(notmask, unop(Iop_NotV256, mkexpr(mask)));

   IRTemp res = newTemp(Ity_V256);
   assign(res,  binop(Iop_OrV256,
                      binop(Iop_AndV256, mkexpr(vecE), mkexpr(mask)),
                      binop(Iop_AndV256, mkexpr(vecG), mkexpr(notmask))));
   return res;
}

static Long dis_VBLENDV_128 ( const VexAbiInfo* vbi, Prefix pfx, Long delta,
                              const HChar *name, UInt gran, IROp opSAR )
{
   IRTemp addr   = IRTemp_INVALID;
   Int    alen   = 0;
   HChar  dis_buf[50];
   UChar  modrm  = getUChar(delta);
   UInt   rG     = gregOfRexRM(pfx, modrm);
   UInt   rV     = getVexNvvvv(pfx);
   UInt   rIS4   = 0xFF; 
   IRTemp vecE   = newTemp(Ity_V128);
   IRTemp vecV   = newTemp(Ity_V128);
   IRTemp vecIS4 = newTemp(Ity_V128);
   if (epartIsReg(modrm)) {
      delta++;
      UInt rE = eregOfRexRM(pfx, modrm);
      assign(vecE, getXMMReg(rE));
      UChar ib = getUChar(delta);
      rIS4 = (ib >> 4) & 0xF;
      DIP("%s %s,%s,%s,%s\n",
          name, nameXMMReg(rIS4), nameXMMReg(rE),
          nameXMMReg(rV), nameXMMReg(rG));
   } else {
      addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 1 );
      delta += alen;
      assign(vecE, loadLE(Ity_V128, mkexpr(addr)));
      UChar ib = getUChar(delta);
      rIS4 = (ib >> 4) & 0xF;
      DIP("%s %s,%s,%s,%s\n",
          name, nameXMMReg(rIS4), dis_buf, nameXMMReg(rV), nameXMMReg(rG));
   }
   delta++;
   assign(vecV,   getXMMReg(rV));
   assign(vecIS4, getXMMReg(rIS4));
   IRTemp res = math_PBLENDVB_128( vecE, vecV, vecIS4, gran, opSAR );
   putYMMRegLoAndZU( rG, mkexpr(res) );
   return delta;
}

static Long dis_VBLENDV_256 ( const VexAbiInfo* vbi, Prefix pfx, Long delta,
                              const HChar *name, UInt gran, IROp opSAR128 )
{
   IRTemp addr   = IRTemp_INVALID;
   Int    alen   = 0;
   HChar  dis_buf[50];
   UChar  modrm  = getUChar(delta);
   UInt   rG     = gregOfRexRM(pfx, modrm);
   UInt   rV     = getVexNvvvv(pfx);
   UInt   rIS4   = 0xFF; 
   IRTemp vecE   = newTemp(Ity_V256);
   IRTemp vecV   = newTemp(Ity_V256);
   IRTemp vecIS4 = newTemp(Ity_V256);
   if (epartIsReg(modrm)) {
      delta++;
      UInt rE = eregOfRexRM(pfx, modrm);
      assign(vecE, getYMMReg(rE));
      UChar ib = getUChar(delta);
      rIS4 = (ib >> 4) & 0xF;
      DIP("%s %s,%s,%s,%s\n",
          name, nameYMMReg(rIS4), nameYMMReg(rE),
          nameYMMReg(rV), nameYMMReg(rG));
   } else {
      addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 1 );
      delta += alen;
      assign(vecE, loadLE(Ity_V256, mkexpr(addr)));
      UChar ib = getUChar(delta);
      rIS4 = (ib >> 4) & 0xF;
      DIP("%s %s,%s,%s,%s\n",
          name, nameYMMReg(rIS4), dis_buf, nameYMMReg(rV), nameYMMReg(rG));
   }
   delta++;
   assign(vecV,   getYMMReg(rV));
   assign(vecIS4, getYMMReg(rIS4));
   IRTemp res = math_PBLENDVB_256( vecE, vecV, vecIS4, gran, opSAR128 );
   putYMMReg( rG, mkexpr(res) );
   return delta;
}

static void finish_xTESTy ( IRTemp andV, IRTemp andnV, Int sign )
{

   

   IRTemp and64  = newTemp(Ity_I64);
   IRTemp andn64 = newTemp(Ity_I64);

   assign(and64,
          unop(Iop_V128to64,
               binop(Iop_OrV128,
                     binop(Iop_InterleaveLO64x2,
                           mkexpr(andV), mkexpr(andV)),
                     binop(Iop_InterleaveHI64x2,
                           mkexpr(andV), mkexpr(andV)))));

   assign(andn64,
          unop(Iop_V128to64,
               binop(Iop_OrV128,
                     binop(Iop_InterleaveLO64x2,
                           mkexpr(andnV), mkexpr(andnV)),
                     binop(Iop_InterleaveHI64x2,
                           mkexpr(andnV), mkexpr(andnV)))));

   IRTemp z64 = newTemp(Ity_I64);
   IRTemp c64 = newTemp(Ity_I64);
   if (sign == 64) {
      assign(z64,
             unop(Iop_Not64,
                  binop(Iop_Sar64, mkexpr(and64), mkU8(63))));

      assign(c64,
             unop(Iop_Not64,
                  binop(Iop_Sar64, mkexpr(andn64), mkU8(63))));
   } else {
      if (sign == 32) {
         IRTemp t0 = newTemp(Ity_I64);
         IRTemp t1 = newTemp(Ity_I64);
         IRTemp t2 = newTemp(Ity_I64);
         assign(t0, mkU64(0x8000000080000000ULL));
         assign(t1, binop(Iop_And64, mkexpr(and64), mkexpr(t0)));
         assign(t2, binop(Iop_And64, mkexpr(andn64), mkexpr(t0)));
         and64 = t1;
         andn64 = t2;
      }
      assign(z64,
             unop(Iop_Not64,
                  binop(Iop_Sar64,
                        binop(Iop_Or64,
                              binop(Iop_Sub64, mkU64(0), mkexpr(and64)),
                                    mkexpr(and64)), mkU8(63))));

      assign(c64,
             unop(Iop_Not64,
                  binop(Iop_Sar64,
                        binop(Iop_Or64,
                              binop(Iop_Sub64, mkU64(0), mkexpr(andn64)),
                                    mkexpr(andn64)), mkU8(63))));
   }

   IRTemp newOSZACP = newTemp(Ity_I64);
   assign(newOSZACP, 
          binop(Iop_Or64,
                binop(Iop_And64, mkexpr(z64), mkU64(AMD64G_CC_MASK_Z)),
                binop(Iop_And64, mkexpr(c64), mkU64(AMD64G_CC_MASK_C))));

   stmt( IRStmt_Put( OFFB_CC_DEP1, mkexpr(newOSZACP)));
   stmt( IRStmt_Put( OFFB_CC_OP,   mkU64(AMD64G_CC_OP_COPY) ));
   stmt( IRStmt_Put( OFFB_CC_DEP2, mkU64(0) ));
   stmt( IRStmt_Put( OFFB_CC_NDEP, mkU64(0) ));
}


static Long dis_xTESTy_128 ( const VexAbiInfo* vbi, Prefix pfx,
                             Long delta, Bool isAvx, Int sign )
{
   IRTemp addr   = IRTemp_INVALID;
   Int    alen   = 0;
   HChar  dis_buf[50];
   UChar  modrm  = getUChar(delta);
   UInt   rG     = gregOfRexRM(pfx, modrm);
   IRTemp vecE = newTemp(Ity_V128);
   IRTemp vecG = newTemp(Ity_V128);

   if ( epartIsReg(modrm) ) {
      UInt rE = eregOfRexRM(pfx, modrm);
      assign(vecE, getXMMReg(rE));
      delta += 1;
      DIP( "%s%stest%s %s,%s\n",
           isAvx ? "v" : "", sign == 0 ? "p" : "",
           sign == 0 ? "" : sign == 32 ? "ps" : "pd",
           nameXMMReg(rE), nameXMMReg(rG) );
   } else {
      addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 0 );
      if (!isAvx)
         gen_SEGV_if_not_16_aligned( addr );
      assign(vecE, loadLE( Ity_V128, mkexpr(addr) ));
      delta += alen;
      DIP( "%s%stest%s %s,%s\n",
           isAvx ? "v" : "", sign == 0 ? "p" : "",
           sign == 0 ? "" : sign == 32 ? "ps" : "pd",
           dis_buf, nameXMMReg(rG) );
   }

   assign(vecG, getXMMReg(rG));


   
   IRTemp andV  = newTemp(Ity_V128);
   IRTemp andnV = newTemp(Ity_V128);
   assign(andV,  binop(Iop_AndV128, mkexpr(vecE), mkexpr(vecG)));
   assign(andnV, binop(Iop_AndV128,
                       mkexpr(vecE),
                       binop(Iop_XorV128, mkexpr(vecG),
                                          mkV128(0xFFFF))));

   finish_xTESTy ( andV, andnV, sign );
   return delta;
}


static Long dis_xTESTy_256 ( const VexAbiInfo* vbi, Prefix pfx,
                             Long delta, Int sign )
{
   IRTemp addr   = IRTemp_INVALID;
   Int    alen   = 0;
   HChar  dis_buf[50];
   UChar  modrm  = getUChar(delta);
   UInt   rG     = gregOfRexRM(pfx, modrm);
   IRTemp vecE   = newTemp(Ity_V256);
   IRTemp vecG   = newTemp(Ity_V256);

   if ( epartIsReg(modrm) ) {
      UInt rE = eregOfRexRM(pfx, modrm);
      assign(vecE, getYMMReg(rE));
      delta += 1;
      DIP( "v%stest%s %s,%s\n", sign == 0 ? "p" : "",
           sign == 0 ? "" : sign == 32 ? "ps" : "pd",
           nameYMMReg(rE), nameYMMReg(rG) );
   } else {
      addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 0 );
      assign(vecE, loadLE( Ity_V256, mkexpr(addr) ));
      delta += alen;
      DIP( "v%stest%s %s,%s\n", sign == 0 ? "p" : "",
           sign == 0 ? "" : sign == 32 ? "ps" : "pd",
           dis_buf, nameYMMReg(rG) );
   }

   assign(vecG, getYMMReg(rG));


   
   IRTemp andV  = newTemp(Ity_V256);
   IRTemp andnV = newTemp(Ity_V256);
   assign(andV,  binop(Iop_AndV256, mkexpr(vecE), mkexpr(vecG)));
   assign(andnV, binop(Iop_AndV256,
                       mkexpr(vecE), unop(Iop_NotV256, mkexpr(vecG))));

   IRTemp andVhi  = IRTemp_INVALID;
   IRTemp andVlo  = IRTemp_INVALID;
   IRTemp andnVhi = IRTemp_INVALID;
   IRTemp andnVlo = IRTemp_INVALID;
   breakupV256toV128s( andV, &andVhi, &andVlo );
   breakupV256toV128s( andnV, &andnVhi, &andnVlo );

   IRTemp andV128  = newTemp(Ity_V128);
   IRTemp andnV128 = newTemp(Ity_V128);
   assign( andV128, binop( Iop_OrV128, mkexpr(andVhi), mkexpr(andVlo) ) );
   assign( andnV128, binop( Iop_OrV128, mkexpr(andnVhi), mkexpr(andnVlo) ) );

   finish_xTESTy ( andV128, andnV128, sign );
   return delta;
}


static Long dis_PMOVxXBW_128 ( const VexAbiInfo* vbi, Prefix pfx,
                               Long delta, Bool isAvx, Bool xIsZ )
{
   IRTemp addr   = IRTemp_INVALID;
   Int    alen   = 0;
   HChar  dis_buf[50];
   IRTemp srcVec = newTemp(Ity_V128);
   UChar  modrm  = getUChar(delta);
   const HChar* mbV    = isAvx ? "v" : "";
   const HChar  how    = xIsZ ? 'z' : 's';
   UInt   rG     = gregOfRexRM(pfx, modrm);
   if ( epartIsReg(modrm) ) {
      UInt rE = eregOfRexRM(pfx, modrm);
      assign( srcVec, getXMMReg(rE) );
      delta += 1;
      DIP( "%spmov%cxbw %s,%s\n", mbV, how, nameXMMReg(rE), nameXMMReg(rG) );
   } else {
      addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 0 );
      assign( srcVec, 
              unop( Iop_64UtoV128, loadLE( Ity_I64, mkexpr(addr) ) ) );
      delta += alen;
      DIP( "%spmov%cxbw %s,%s\n", mbV, how, dis_buf, nameXMMReg(rG) );
   }

   IRExpr* res 
      = xIsZ 
        ? binop( Iop_InterleaveLO8x16, 
                 IRExpr_Const( IRConst_V128(0) ), mkexpr(srcVec) )
        : binop( Iop_SarN16x8, 
                 binop( Iop_ShlN16x8, 
                        binop( Iop_InterleaveLO8x16,
                               IRExpr_Const( IRConst_V128(0) ),
                               mkexpr(srcVec) ),
                        mkU8(8) ),
                 mkU8(8) );

   (isAvx ? putYMMRegLoAndZU : putXMMReg) ( rG, res );

   return delta;
}


static Long dis_PMOVxXBW_256 ( const VexAbiInfo* vbi, Prefix pfx,
                               Long delta, Bool xIsZ )
{
   IRTemp addr   = IRTemp_INVALID;
   Int    alen   = 0;
   HChar  dis_buf[50];
   IRTemp srcVec = newTemp(Ity_V128);
   UChar  modrm  = getUChar(delta);
   UChar  how    = xIsZ ? 'z' : 's';
   UInt   rG     = gregOfRexRM(pfx, modrm);
   if ( epartIsReg(modrm) ) {
      UInt rE = eregOfRexRM(pfx, modrm);
      assign( srcVec, getXMMReg(rE) );
      delta += 1;
      DIP( "vpmov%cxbw %s,%s\n", how, nameXMMReg(rE), nameYMMReg(rG) );
   } else {
      addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 0 );
      assign( srcVec, loadLE( Ity_V128, mkexpr(addr) ) );
      delta += alen;
      DIP( "vpmov%cxbw %s,%s\n", how, dis_buf, nameYMMReg(rG) );
   }

   
   IRExpr* res
      = binop( Iop_V128HLtoV256,
               binop( Iop_InterleaveHI8x16,
                      IRExpr_Const( IRConst_V128(0) ), mkexpr(srcVec) ),
               binop( Iop_InterleaveLO8x16,
                      IRExpr_Const( IRConst_V128(0) ), mkexpr(srcVec) ) );
   
   if (!xIsZ)
      res = binop( Iop_SarN16x16,
                   binop( Iop_ShlN16x16, res, mkU8(8) ), mkU8(8) );

   putYMMReg ( rG, res );

   return delta;
}


static Long dis_PMOVxXWD_128 ( const VexAbiInfo* vbi, Prefix pfx,
                               Long delta, Bool isAvx, Bool xIsZ )
{
   IRTemp addr   = IRTemp_INVALID;
   Int    alen   = 0;
   HChar  dis_buf[50];
   IRTemp srcVec = newTemp(Ity_V128);
   UChar  modrm  = getUChar(delta);
   const HChar* mbV    = isAvx ? "v" : "";
   const HChar  how    = xIsZ ? 'z' : 's';
   UInt   rG     = gregOfRexRM(pfx, modrm);

   if ( epartIsReg(modrm) ) {
      UInt rE = eregOfRexRM(pfx, modrm);
      assign( srcVec, getXMMReg(rE) );
      delta += 1;
      DIP( "%spmov%cxwd %s,%s\n", mbV, how, nameXMMReg(rE), nameXMMReg(rG) );
   } else {
      addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 0 );
      assign( srcVec, 
              unop( Iop_64UtoV128, loadLE( Ity_I64, mkexpr(addr) ) ) );
      delta += alen;
      DIP( "%spmov%cxwd %s,%s\n", mbV, how, dis_buf, nameXMMReg(rG) );
   }

   IRExpr* res
      = binop( Iop_InterleaveLO16x8,  
               IRExpr_Const( IRConst_V128(0) ), mkexpr(srcVec) );
   if (!xIsZ)
      res = binop(Iop_SarN32x4, 
                  binop(Iop_ShlN32x4, res, mkU8(16)), mkU8(16));

   (isAvx ? putYMMRegLoAndZU : putXMMReg)
      ( gregOfRexRM(pfx, modrm), res );

   return delta;
}


static Long dis_PMOVxXWD_256 ( const VexAbiInfo* vbi, Prefix pfx,
                               Long delta, Bool xIsZ )
{
   IRTemp addr   = IRTemp_INVALID;
   Int    alen   = 0;
   HChar  dis_buf[50];
   IRTemp srcVec = newTemp(Ity_V128);
   UChar  modrm  = getUChar(delta);
   UChar  how    = xIsZ ? 'z' : 's';
   UInt   rG     = gregOfRexRM(pfx, modrm);

   if ( epartIsReg(modrm) ) {
      UInt rE = eregOfRexRM(pfx, modrm);
      assign( srcVec, getXMMReg(rE) );
      delta += 1;
      DIP( "vpmov%cxwd %s,%s\n", how, nameXMMReg(rE), nameYMMReg(rG) );
   } else {
      addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 0 );
      assign( srcVec, loadLE( Ity_V128, mkexpr(addr) ) );
      delta += alen;
      DIP( "vpmov%cxwd %s,%s\n", how, dis_buf, nameYMMReg(rG) );
   }

   IRExpr* res
      = binop( Iop_V128HLtoV256,
               binop( Iop_InterleaveHI16x8,
                      IRExpr_Const( IRConst_V128(0) ), mkexpr(srcVec) ),
               binop( Iop_InterleaveLO16x8,
                      IRExpr_Const( IRConst_V128(0) ), mkexpr(srcVec) ) );
   if (!xIsZ)
      res = binop(Iop_SarN32x8,
                  binop(Iop_ShlN32x8, res, mkU8(16)), mkU8(16));

   putYMMReg ( rG, res );

   return delta;
}


static Long dis_PMOVSXWQ_128 ( const VexAbiInfo* vbi, Prefix pfx,
                               Long delta, Bool isAvx )
{
   IRTemp addr     = IRTemp_INVALID;
   Int    alen     = 0;
   HChar  dis_buf[50];
   IRTemp srcBytes = newTemp(Ity_I32);
   UChar  modrm    = getUChar(delta);
   const HChar* mbV = isAvx ? "v" : "";
   UInt   rG       = gregOfRexRM(pfx, modrm);

   if ( epartIsReg( modrm ) ) {
      UInt rE = eregOfRexRM(pfx, modrm);
      assign( srcBytes, getXMMRegLane32( rE, 0 ) );
      delta += 1;
      DIP( "%spmovsxwq %s,%s\n", mbV, nameXMMReg(rE), nameXMMReg(rG) );
   } else {
      addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 0 );
      assign( srcBytes, loadLE( Ity_I32, mkexpr(addr) ) );
      delta += alen;
      DIP( "%spmovsxwq %s,%s\n", mbV, dis_buf, nameXMMReg(rG) );
   }

   (isAvx ? putYMMRegLoAndZU : putXMMReg)
      ( rG, binop( Iop_64HLtoV128, 
                   unop( Iop_16Sto64,
                         unop( Iop_32HIto16, mkexpr(srcBytes) ) ),
                   unop( Iop_16Sto64, 
                         unop( Iop_32to16, mkexpr(srcBytes) ) ) ) );
   return delta;
}


static Long dis_PMOVSXWQ_256 ( const VexAbiInfo* vbi, Prefix pfx, Long delta )
{
   IRTemp addr     = IRTemp_INVALID;
   Int    alen     = 0;
   HChar  dis_buf[50];
   IRTemp srcBytes = newTemp(Ity_I64);
   UChar  modrm    = getUChar(delta);
   UInt   rG       = gregOfRexRM(pfx, modrm);
   IRTemp s3, s2, s1, s0;
   s3 = s2 = s1 = s0 = IRTemp_INVALID;

   if ( epartIsReg( modrm ) ) {
      UInt rE = eregOfRexRM(pfx, modrm);
      assign( srcBytes, getXMMRegLane64( rE, 0 ) );
      delta += 1;
      DIP( "vpmovsxwq %s,%s\n", nameXMMReg(rE), nameYMMReg(rG) );
   } else {
      addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 0 );
      assign( srcBytes, loadLE( Ity_I64, mkexpr(addr) ) );
      delta += alen;
      DIP( "vpmovsxwq %s,%s\n", dis_buf, nameYMMReg(rG) );
   }

   breakup64to16s( srcBytes, &s3, &s2, &s1, &s0 );
   putYMMReg( rG, binop( Iop_V128HLtoV256,
                         binop( Iop_64HLtoV128,
                                unop( Iop_16Sto64, mkexpr(s3) ),
                                unop( Iop_16Sto64, mkexpr(s2) ) ),
                         binop( Iop_64HLtoV128,
                                unop( Iop_16Sto64, mkexpr(s1) ),
                                unop( Iop_16Sto64, mkexpr(s0) ) ) ) );
   return delta;
}


static Long dis_PMOVZXWQ_128 ( const VexAbiInfo* vbi, Prefix pfx,
                               Long delta, Bool isAvx )
{
   IRTemp addr     = IRTemp_INVALID;
   Int    alen     = 0;
   HChar  dis_buf[50];
   IRTemp srcVec = newTemp(Ity_V128);
   UChar  modrm    = getUChar(delta);
   const HChar* mbV = isAvx ? "v" : "";
   UInt   rG       = gregOfRexRM(pfx, modrm);

   if ( epartIsReg( modrm ) ) {
      UInt rE = eregOfRexRM(pfx, modrm);
      assign( srcVec, getXMMReg(rE) );
      delta += 1;
      DIP( "%spmovzxwq %s,%s\n", mbV, nameXMMReg(rE), nameXMMReg(rG) );
   } else {
      addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 0 );
      assign( srcVec, 
              unop( Iop_32UtoV128, loadLE( Ity_I32, mkexpr(addr) ) ) );
      delta += alen;
      DIP( "%spmovzxwq %s,%s\n", mbV, dis_buf, nameXMMReg(rG) );
   }

   IRTemp zeroVec = newTemp( Ity_V128 );
   assign( zeroVec, IRExpr_Const( IRConst_V128(0) ) );

   (isAvx ? putYMMRegLoAndZU : putXMMReg)
      ( rG, binop( Iop_InterleaveLO16x8, 
                   mkexpr(zeroVec), 
                   binop( Iop_InterleaveLO16x8, 
                          mkexpr(zeroVec), mkexpr(srcVec) ) ) );
   return delta;
}


static Long dis_PMOVZXWQ_256 ( const VexAbiInfo* vbi, Prefix pfx,
                               Long delta )
{
   IRTemp addr     = IRTemp_INVALID;
   Int    alen     = 0;
   HChar  dis_buf[50];
   IRTemp srcVec = newTemp(Ity_V128);
   UChar  modrm    = getUChar(delta);
   UInt   rG       = gregOfRexRM(pfx, modrm);

   if ( epartIsReg( modrm ) ) {
      UInt rE = eregOfRexRM(pfx, modrm);
      assign( srcVec, getXMMReg(rE) );
      delta += 1;
      DIP( "vpmovzxwq %s,%s\n", nameXMMReg(rE), nameYMMReg(rG) );
   } else {
      addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 0 );
      assign( srcVec,
              unop( Iop_64UtoV128, loadLE( Ity_I64, mkexpr(addr) ) ) );
      delta += alen;
      DIP( "vpmovzxwq %s,%s\n", dis_buf, nameYMMReg(rG) );
   }

   IRTemp zeroVec = newTemp( Ity_V128 );
   assign( zeroVec, IRExpr_Const( IRConst_V128(0) ) );

   putYMMReg( rG, binop( Iop_V128HLtoV256,
                         binop( Iop_InterleaveHI16x8,
                                mkexpr(zeroVec),
                                binop( Iop_InterleaveLO16x8,
                                       mkexpr(zeroVec), mkexpr(srcVec) ) ),
                         binop( Iop_InterleaveLO16x8,
                                mkexpr(zeroVec),
                                binop( Iop_InterleaveLO16x8,
                                       mkexpr(zeroVec), mkexpr(srcVec) ) ) ) );
   return delta;
}


static Long dis_PMOVxXDQ_128 ( const VexAbiInfo* vbi, Prefix pfx,
                               Long delta, Bool isAvx, Bool xIsZ )
{
   IRTemp addr   = IRTemp_INVALID;
   Int    alen   = 0;
   HChar  dis_buf[50];
   IRTemp srcI64 = newTemp(Ity_I64);
   IRTemp srcVec = newTemp(Ity_V128);
   UChar  modrm  = getUChar(delta);
   const HChar* mbV = isAvx ? "v" : "";
   const HChar  how = xIsZ ? 'z' : 's';
   UInt   rG     = gregOfRexRM(pfx, modrm);
   if ( epartIsReg(modrm) ) {
      UInt rE = eregOfRexRM(pfx, modrm);
      assign( srcVec, getXMMReg(rE) );
      assign( srcI64, unop(Iop_V128to64, mkexpr(srcVec)) );
      delta += 1;
      DIP( "%spmov%cxdq %s,%s\n", mbV, how, nameXMMReg(rE), nameXMMReg(rG) );
   } else {
      addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 0 );
      assign( srcI64, loadLE(Ity_I64, mkexpr(addr)) );
      assign( srcVec, unop( Iop_64UtoV128, mkexpr(srcI64)) );
      delta += alen;
      DIP( "%spmov%cxdq %s,%s\n", mbV, how, dis_buf, nameXMMReg(rG) );
   }

   IRExpr* res 
      = xIsZ 
        ? binop( Iop_InterleaveLO32x4, 
                 IRExpr_Const( IRConst_V128(0) ), mkexpr(srcVec) )
        : binop( Iop_64HLtoV128, 
                 unop( Iop_32Sto64, 
                       unop( Iop_64HIto32, mkexpr(srcI64) ) ), 
                 unop( Iop_32Sto64, 
                       unop( Iop_64to32, mkexpr(srcI64) ) ) );

   (isAvx ? putYMMRegLoAndZU : putXMMReg) ( rG, res );

   return delta;
}


static Long dis_PMOVxXDQ_256 ( const VexAbiInfo* vbi, Prefix pfx,
                               Long delta, Bool xIsZ )
{
   IRTemp addr   = IRTemp_INVALID;
   Int    alen   = 0;
   HChar  dis_buf[50];
   IRTemp srcVec = newTemp(Ity_V128);
   UChar  modrm  = getUChar(delta);
   UChar  how    = xIsZ ? 'z' : 's';
   UInt   rG     = gregOfRexRM(pfx, modrm);
   if ( epartIsReg(modrm) ) {
      UInt rE = eregOfRexRM(pfx, modrm);
      assign( srcVec, getXMMReg(rE) );
      delta += 1;
      DIP( "vpmov%cxdq %s,%s\n", how, nameXMMReg(rE), nameYMMReg(rG) );
   } else {
      addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 0 );
      assign( srcVec, loadLE(Ity_V128, mkexpr(addr)) );
      delta += alen;
      DIP( "vpmov%cxdq %s,%s\n", how, dis_buf, nameYMMReg(rG) );
   }

   IRExpr* res;
   if (xIsZ)
      res = binop( Iop_V128HLtoV256,
                   binop( Iop_InterleaveHI32x4,
                          IRExpr_Const( IRConst_V128(0) ), mkexpr(srcVec) ),
                   binop( Iop_InterleaveLO32x4,
                          IRExpr_Const( IRConst_V128(0) ), mkexpr(srcVec) ) );
   else {
      IRTemp s3, s2, s1, s0;
      s3 = s2 = s1 = s0 = IRTemp_INVALID;
      breakupV128to32s( srcVec, &s3, &s2, &s1, &s0 );
      res = binop( Iop_V128HLtoV256,
                   binop( Iop_64HLtoV128,
                          unop( Iop_32Sto64, mkexpr(s3) ),
                          unop( Iop_32Sto64, mkexpr(s2) ) ),
                   binop( Iop_64HLtoV128,
                          unop( Iop_32Sto64, mkexpr(s1) ),
                          unop( Iop_32Sto64, mkexpr(s0) ) ) );
   }

   putYMMReg ( rG, res );

   return delta;
}


static Long dis_PMOVxXBD_128 ( const VexAbiInfo* vbi, Prefix pfx,
                               Long delta, Bool isAvx, Bool xIsZ )
{
   IRTemp addr   = IRTemp_INVALID;
   Int    alen   = 0;
   HChar  dis_buf[50];
   IRTemp srcVec = newTemp(Ity_V128);
   UChar  modrm  = getUChar(delta);
   const HChar* mbV = isAvx ? "v" : "";
   const HChar  how = xIsZ ? 'z' : 's';
   UInt   rG     = gregOfRexRM(pfx, modrm);
   if ( epartIsReg(modrm) ) {
      UInt rE = eregOfRexRM(pfx, modrm);
      assign( srcVec, getXMMReg(rE) );
      delta += 1;
      DIP( "%spmov%cxbd %s,%s\n", mbV, how, nameXMMReg(rE), nameXMMReg(rG) );
   } else {
      addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 0 );
      assign( srcVec, 
              unop( Iop_32UtoV128, loadLE( Ity_I32, mkexpr(addr) ) ) );
      delta += alen;
      DIP( "%spmov%cxbd %s,%s\n", mbV, how, dis_buf, nameXMMReg(rG) );
   }

   IRTemp zeroVec = newTemp(Ity_V128);
   assign( zeroVec, IRExpr_Const( IRConst_V128(0) ) );

   IRExpr* res
      = binop(Iop_InterleaveLO8x16,
              mkexpr(zeroVec),
              binop(Iop_InterleaveLO8x16, 
                    mkexpr(zeroVec), mkexpr(srcVec)));
   if (!xIsZ)
      res = binop(Iop_SarN32x4, 
                  binop(Iop_ShlN32x4, res, mkU8(24)), mkU8(24));

   (isAvx ? putYMMRegLoAndZU : putXMMReg) ( rG, res );

   return delta;
}


static Long dis_PMOVxXBD_256 ( const VexAbiInfo* vbi, Prefix pfx,
                               Long delta, Bool xIsZ )
{
   IRTemp addr   = IRTemp_INVALID;
   Int    alen   = 0;
   HChar  dis_buf[50];
   IRTemp srcVec = newTemp(Ity_V128);
   UChar  modrm  = getUChar(delta);
   UChar  how    = xIsZ ? 'z' : 's';
   UInt   rG     = gregOfRexRM(pfx, modrm);
   if ( epartIsReg(modrm) ) {
      UInt rE = eregOfRexRM(pfx, modrm);
      assign( srcVec, getXMMReg(rE) );
      delta += 1;
      DIP( "vpmov%cxbd %s,%s\n", how, nameXMMReg(rE), nameYMMReg(rG) );
   } else {
      addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 0 );
      assign( srcVec,
              unop( Iop_64UtoV128, loadLE( Ity_I64, mkexpr(addr) ) ) );
      delta += alen;
      DIP( "vpmov%cxbd %s,%s\n", how, dis_buf, nameYMMReg(rG) );
   }

   IRTemp zeroVec = newTemp(Ity_V128);
   assign( zeroVec, IRExpr_Const( IRConst_V128(0) ) );

   IRExpr* res
      = binop( Iop_V128HLtoV256,
               binop(Iop_InterleaveHI8x16,
                     mkexpr(zeroVec),
                     binop(Iop_InterleaveLO8x16,
                           mkexpr(zeroVec), mkexpr(srcVec)) ),
               binop(Iop_InterleaveLO8x16,
                     mkexpr(zeroVec),
                     binop(Iop_InterleaveLO8x16,
                           mkexpr(zeroVec), mkexpr(srcVec)) ) );
   if (!xIsZ)
      res = binop(Iop_SarN32x8,
                  binop(Iop_ShlN32x8, res, mkU8(24)), mkU8(24));

   putYMMReg ( rG, res );

   return delta;
}


static Long dis_PMOVSXBQ_128 ( const VexAbiInfo* vbi, Prefix pfx,
                               Long delta, Bool isAvx )
{
   IRTemp addr     = IRTemp_INVALID;
   Int    alen     = 0;
   HChar  dis_buf[50];
   IRTemp srcBytes = newTemp(Ity_I16);
   UChar  modrm    = getUChar(delta);
   const HChar* mbV = isAvx ? "v" : "";
   UInt   rG       = gregOfRexRM(pfx, modrm);
   if ( epartIsReg(modrm) ) {
      UInt rE = eregOfRexRM(pfx, modrm);
      assign( srcBytes, getXMMRegLane16( rE, 0 ) );
      delta += 1;
      DIP( "%spmovsxbq %s,%s\n", mbV, nameXMMReg(rE), nameXMMReg(rG) );
   } else {
      addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 0 );
      assign( srcBytes, loadLE( Ity_I16, mkexpr(addr) ) );
      delta += alen;
      DIP( "%spmovsxbq %s,%s\n", mbV, dis_buf, nameXMMReg(rG) );
   }

   (isAvx ? putYMMRegLoAndZU : putXMMReg)
      ( rG, binop( Iop_64HLtoV128,
                   unop( Iop_8Sto64,
                         unop( Iop_16HIto8, mkexpr(srcBytes) ) ),
                   unop( Iop_8Sto64,
                         unop( Iop_16to8, mkexpr(srcBytes) ) ) ) );
   return delta;
}


static Long dis_PMOVSXBQ_256 ( const VexAbiInfo* vbi, Prefix pfx,
                               Long delta )
{
   IRTemp addr     = IRTemp_INVALID;
   Int    alen     = 0;
   HChar  dis_buf[50];
   IRTemp srcBytes = newTemp(Ity_I32);
   UChar  modrm    = getUChar(delta);
   UInt   rG       = gregOfRexRM(pfx, modrm);
   if ( epartIsReg(modrm) ) {
      UInt rE = eregOfRexRM(pfx, modrm);
      assign( srcBytes, getXMMRegLane32( rE, 0 ) );
      delta += 1;
      DIP( "vpmovsxbq %s,%s\n", nameXMMReg(rE), nameYMMReg(rG) );
   } else {
      addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 0 );
      assign( srcBytes, loadLE( Ity_I32, mkexpr(addr) ) );
      delta += alen;
      DIP( "vpmovsxbq %s,%s\n", dis_buf, nameYMMReg(rG) );
   }

   putYMMReg
      ( rG, binop( Iop_V128HLtoV256,
                   binop( Iop_64HLtoV128,
                          unop( Iop_8Sto64,
                                unop( Iop_16HIto8,
                                      unop( Iop_32HIto16,
                                            mkexpr(srcBytes) ) ) ),
                          unop( Iop_8Sto64,
                                unop( Iop_16to8,
                                      unop( Iop_32HIto16,
                                            mkexpr(srcBytes) ) ) ) ),
                   binop( Iop_64HLtoV128,
                          unop( Iop_8Sto64,
                                unop( Iop_16HIto8,
                                      unop( Iop_32to16,
                                            mkexpr(srcBytes) ) ) ),
                          unop( Iop_8Sto64,
                                unop( Iop_16to8,
                                      unop( Iop_32to16,
                                            mkexpr(srcBytes) ) ) ) ) ) );
   return delta;
}


static Long dis_PMOVZXBQ_128 ( const VexAbiInfo* vbi, Prefix pfx,
                               Long delta, Bool isAvx )
{
   IRTemp addr     = IRTemp_INVALID;
   Int    alen     = 0;
   HChar  dis_buf[50];
   IRTemp srcVec   = newTemp(Ity_V128);
   UChar  modrm    = getUChar(delta);
   const HChar* mbV = isAvx ? "v" : "";
   UInt   rG       = gregOfRexRM(pfx, modrm);
   if ( epartIsReg(modrm) ) {
      UInt rE = eregOfRexRM(pfx, modrm);
      assign( srcVec, getXMMReg(rE) );
      delta += 1;
      DIP( "%spmovzxbq %s,%s\n", mbV, nameXMMReg(rE), nameXMMReg(rG) );
   } else {
      addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 0 );
      assign( srcVec, 
              unop( Iop_32UtoV128, 
                    unop( Iop_16Uto32, loadLE( Ity_I16, mkexpr(addr) ))));
      delta += alen;
      DIP( "%spmovzxbq %s,%s\n", mbV, dis_buf, nameXMMReg(rG) );
   }

   IRTemp zeroVec = newTemp(Ity_V128);
   assign( zeroVec, IRExpr_Const( IRConst_V128(0) ) );

   (isAvx ? putYMMRegLoAndZU : putXMMReg)
      ( rG, binop( Iop_InterleaveLO8x16, 
                   mkexpr(zeroVec), 
                   binop( Iop_InterleaveLO8x16, 
                          mkexpr(zeroVec), 
                          binop( Iop_InterleaveLO8x16, 
                                 mkexpr(zeroVec), mkexpr(srcVec) ) ) ) );
   return delta;
}


static Long dis_PMOVZXBQ_256 ( const VexAbiInfo* vbi, Prefix pfx,
                               Long delta )
{
   IRTemp addr     = IRTemp_INVALID;
   Int    alen     = 0;
   HChar  dis_buf[50];
   IRTemp srcVec   = newTemp(Ity_V128);
   UChar  modrm    = getUChar(delta);
   UInt   rG       = gregOfRexRM(pfx, modrm);
   if ( epartIsReg(modrm) ) {
      UInt rE = eregOfRexRM(pfx, modrm);
      assign( srcVec, getXMMReg(rE) );
      delta += 1;
      DIP( "vpmovzxbq %s,%s\n", nameXMMReg(rE), nameYMMReg(rG) );
   } else {
      addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 0 );
      assign( srcVec,
              unop( Iop_32UtoV128, loadLE( Ity_I32, mkexpr(addr) )));
      delta += alen;
      DIP( "vpmovzxbq %s,%s\n", dis_buf, nameYMMReg(rG) );
   }

   IRTemp zeroVec = newTemp(Ity_V128);
   assign( zeroVec, IRExpr_Const( IRConst_V128(0) ) );

   putYMMReg
      ( rG, binop( Iop_V128HLtoV256,
                   binop( Iop_InterleaveHI8x16,
                          mkexpr(zeroVec),
                          binop( Iop_InterleaveLO8x16,
                                 mkexpr(zeroVec),
                                 binop( Iop_InterleaveLO8x16,
                                        mkexpr(zeroVec), mkexpr(srcVec) ) ) ),
                   binop( Iop_InterleaveLO8x16,
                          mkexpr(zeroVec),
                          binop( Iop_InterleaveLO8x16,
                                 mkexpr(zeroVec),
                                 binop( Iop_InterleaveLO8x16,
                                        mkexpr(zeroVec), mkexpr(srcVec) ) ) )
                 ) );
   return delta;
}


static Long dis_PHMINPOSUW_128 ( const VexAbiInfo* vbi, Prefix pfx,
                                 Long delta, Bool isAvx )
{
   IRTemp addr   = IRTemp_INVALID;
   Int    alen   = 0;
   HChar  dis_buf[50];
   UChar  modrm  = getUChar(delta);
   const HChar* mbV = isAvx ? "v" : "";
   IRTemp sV     = newTemp(Ity_V128);
   IRTemp sHi    = newTemp(Ity_I64);
   IRTemp sLo    = newTemp(Ity_I64);
   IRTemp dLo    = newTemp(Ity_I64);
   UInt   rG     = gregOfRexRM(pfx,modrm);
   if (epartIsReg(modrm)) {
      UInt rE = eregOfRexRM(pfx,modrm);
      assign( sV, getXMMReg(rE) );
      delta += 1;
      DIP("%sphminposuw %s,%s\n", mbV, nameXMMReg(rE), nameXMMReg(rG));
   } else {
      addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
      if (!isAvx)
         gen_SEGV_if_not_16_aligned(addr);
      assign( sV, loadLE(Ity_V128, mkexpr(addr)) );
      delta += alen;
      DIP("%sphminposuw %s,%s\n", mbV, dis_buf, nameXMMReg(rG));
   }
   assign( sHi, unop(Iop_V128HIto64, mkexpr(sV)) );
   assign( sLo, unop(Iop_V128to64,   mkexpr(sV)) );
   assign( dLo, mkIRExprCCall(
                   Ity_I64, 0,
                   "amd64g_calculate_sse_phminposuw", 
                   &amd64g_calculate_sse_phminposuw,
                   mkIRExprVec_2( mkexpr(sLo), mkexpr(sHi) )
         ));
   (isAvx ? putYMMRegLoAndZU : putXMMReg)
      (rG, unop(Iop_64UtoV128, mkexpr(dLo)));
   return delta;
}


static Long dis_AESx ( const VexAbiInfo* vbi, Prefix pfx,
                       Long delta, Bool isAvx, UChar opc )
{
   IRTemp addr   = IRTemp_INVALID;
   Int    alen   = 0;
   HChar  dis_buf[50];
   UChar  modrm  = getUChar(delta);
   UInt   rG     = gregOfRexRM(pfx, modrm);
   UInt   regNoL = 0;
   UInt   regNoR = (isAvx && opc != 0xDB) ? getVexNvvvv(pfx) : rG;

   if (epartIsReg(modrm)) {
      regNoL = eregOfRexRM(pfx, modrm);
      delta += 1;
   } else {
      regNoL = 16; 
      addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 0 );
      
      stmt( IRStmt_Put( OFFB_YMM16, loadLE(Ity_V128, mkexpr(addr)) ));
      delta += alen;
   }

   void*  fn = &amd64g_dirtyhelper_AES;
   const HChar* nm = "amd64g_dirtyhelper_AES";

   UInt gstOffD = ymmGuestRegOffset(rG);
   UInt gstOffL = regNoL == 16 ? OFFB_YMM16 : ymmGuestRegOffset(regNoL);
   UInt gstOffR = ymmGuestRegOffset(regNoR);
   IRExpr*  opc4         = mkU64(opc);
   IRExpr*  gstOffDe     = mkU64(gstOffD);
   IRExpr*  gstOffLe     = mkU64(gstOffL);
   IRExpr*  gstOffRe     = mkU64(gstOffR);
   IRExpr** args
      = mkIRExprVec_5( IRExpr_BBPTR(), opc4, gstOffDe, gstOffLe, gstOffRe );

   IRDirty* d    = unsafeIRDirty_0_N( 0, nm, fn, args );
   d->nFxState = 2;
   vex_bzero(&d->fxState, sizeof(d->fxState));
   d->fxState[0].fx     = Ifx_Read;
   d->fxState[0].offset = gstOffL;
   d->fxState[0].size   = sizeof(U128);
   d->fxState[1].offset = gstOffR;
   d->fxState[1].size   = sizeof(U128);
   if (opc == 0xDB)
      d->fxState[1].fx   = Ifx_Write;
   else if (!isAvx || rG == regNoR)
      d->fxState[1].fx   = Ifx_Modify;
   else {
      d->fxState[1].fx     = Ifx_Read;
      d->nFxState++;
      d->fxState[2].fx     = Ifx_Write;
      d->fxState[2].offset = gstOffD; 
      d->fxState[2].size   = sizeof(U128);
   }

   stmt( IRStmt_Dirty(d) );
   {
      const HChar* opsuf;
      switch (opc) {
         case 0xDC: opsuf = "enc"; break;
         case 0XDD: opsuf = "enclast"; break;
         case 0xDE: opsuf = "dec"; break;
         case 0xDF: opsuf = "declast"; break;
         case 0xDB: opsuf = "imc"; break;
         default: vassert(0);
      }
      DIP("%saes%s %s,%s%s%s\n", isAvx ? "v" : "", opsuf, 
          (regNoL == 16 ? dis_buf : nameXMMReg(regNoL)),
          nameXMMReg(regNoR),
          (isAvx && opc != 0xDB) ? "," : "",
          (isAvx && opc != 0xDB) ? nameXMMReg(rG) : "");
   }
   if (isAvx)
      putYMMRegLane128( rG, 1, mkV128(0) );
   return delta;
}

static Long dis_AESKEYGENASSIST ( const VexAbiInfo* vbi, Prefix pfx,
                                  Long delta, Bool isAvx )
{
   IRTemp addr   = IRTemp_INVALID;
   Int    alen   = 0;
   HChar  dis_buf[50];
   UChar  modrm  = getUChar(delta);
   UInt   regNoL = 0;
   UInt   regNoR = gregOfRexRM(pfx, modrm);
   UChar  imm    = 0;

   
   modrm = getUChar(delta);
   if (epartIsReg(modrm)) {
      regNoL = eregOfRexRM(pfx, modrm);
      imm = getUChar(delta+1);
      delta += 1+1;
   } else {
      regNoL = 16; 
      addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 1 );
      
      stmt( IRStmt_Put( OFFB_YMM16, loadLE(Ity_V128, mkexpr(addr)) ));
      imm = getUChar(delta+alen);
      delta += alen+1;
   }

   
   void*  fn = &amd64g_dirtyhelper_AESKEYGENASSIST;
   const HChar* nm = "amd64g_dirtyhelper_AESKEYGENASSIST";

   UInt gstOffL = regNoL == 16 ? OFFB_YMM16 : ymmGuestRegOffset(regNoL);
   UInt gstOffR = ymmGuestRegOffset(regNoR);

   IRExpr*  imme          = mkU64(imm & 0xFF);
   IRExpr*  gstOffLe     = mkU64(gstOffL);
   IRExpr*  gstOffRe     = mkU64(gstOffR);
   IRExpr** args
      = mkIRExprVec_4( IRExpr_BBPTR(), imme, gstOffLe, gstOffRe );

   IRDirty* d    = unsafeIRDirty_0_N( 0, nm, fn, args );
   d->nFxState = 2;
   vex_bzero(&d->fxState, sizeof(d->fxState));
   d->fxState[0].fx     = Ifx_Read;
   d->fxState[0].offset = gstOffL;
   d->fxState[0].size   = sizeof(U128);
   d->fxState[1].fx     = Ifx_Write;
   d->fxState[1].offset = gstOffR;
   d->fxState[1].size   = sizeof(U128);
   stmt( IRStmt_Dirty(d) );

   DIP("%saeskeygenassist $%x,%s,%s\n", isAvx ? "v" : "", (UInt)imm,
       (regNoL == 16 ? dis_buf : nameXMMReg(regNoL)),
       nameXMMReg(regNoR));
   if (isAvx)
      putYMMRegLane128( regNoR, 1, mkV128(0) );
   return delta;
}


__attribute__((noinline))
static
Long dis_ESC_0F38__SSE4 ( Bool* decode_OK,
                          const VexAbiInfo* vbi,
                          Prefix pfx, Int sz, Long deltaIN )
{
   IRTemp addr  = IRTemp_INVALID;
   UChar  modrm = 0;
   Int    alen  = 0;
   HChar  dis_buf[50];

   *decode_OK = False;

   Long   delta = deltaIN;
   UChar  opc   = getUChar(delta);
   delta++;
   switch (opc) {

   case 0x10:
   case 0x14:
   case 0x15:
      if (have66noF2noF3(pfx) && sz == 2) {
         modrm = getUChar(delta);

         const HChar* nm    = NULL;
         UInt   gran  = 0;
         IROp   opSAR = Iop_INVALID;
         switch (opc) {
            case 0x10:
               nm = "pblendvb"; gran = 1; opSAR = Iop_SarN8x16;
               break;
            case 0x14:
               nm = "blendvps"; gran = 4; opSAR = Iop_SarN32x4;
               break;
            case 0x15:
               nm = "blendvpd"; gran = 8; opSAR = Iop_SarN64x2;
               break;
         }
         vassert(nm);

         IRTemp vecE = newTemp(Ity_V128);
         IRTemp vecG = newTemp(Ity_V128);
         IRTemp vec0 = newTemp(Ity_V128);

         if ( epartIsReg(modrm) ) {
            assign(vecE, getXMMReg(eregOfRexRM(pfx, modrm)));
            delta += 1;
            DIP( "%s %s,%s\n", nm,
                 nameXMMReg( eregOfRexRM(pfx, modrm) ),
                 nameXMMReg( gregOfRexRM(pfx, modrm) ) );
         } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 0 );
            gen_SEGV_if_not_16_aligned( addr );
            assign(vecE, loadLE( Ity_V128, mkexpr(addr) ));
            delta += alen;
            DIP( "%s %s,%s\n", nm,
                 dis_buf, nameXMMReg( gregOfRexRM(pfx, modrm) ) );
         }

         assign(vecG, getXMMReg(gregOfRexRM(pfx, modrm)));
         assign(vec0, getXMMReg(0));

         IRTemp res = math_PBLENDVB_128( vecE, vecG, vec0, gran, opSAR );
         putXMMReg(gregOfRexRM(pfx, modrm), mkexpr(res));

         goto decode_success;
      }
      break;

   case 0x17:
      if (have66noF2noF3(pfx)
          && (sz == 2 ||  sz == 8)) {
         delta = dis_xTESTy_128( vbi, pfx, delta, False, 0 );
         goto decode_success;
      }
      break;

   case 0x20:
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_PMOVxXBW_128( vbi, pfx, delta,
                                   False, False );
         goto decode_success;
      }
      break;

   case 0x21:
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_PMOVxXBD_128( vbi, pfx, delta,
                                   False, False );
         goto decode_success;
      }
      break;

   case 0x22:
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_PMOVSXBQ_128( vbi, pfx, delta, False );
         goto decode_success;
      }
      break;

   case 0x23:
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_PMOVxXWD_128(vbi, pfx, delta,
                                  False, False);
         goto decode_success;
      }
      break;

   case 0x24:
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_PMOVSXWQ_128( vbi, pfx, delta, False );
         goto decode_success;
      }
      break;

   case 0x25:
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_PMOVxXDQ_128( vbi, pfx, delta,
                                   False, False );
         goto decode_success;
      }
      break;

   case 0x28:
      if (have66noF2noF3(pfx) && sz == 2) {
         IRTemp sV = newTemp(Ity_V128);
         IRTemp dV = newTemp(Ity_V128);
         modrm = getUChar(delta);
         UInt rG = gregOfRexRM(pfx,modrm);
         assign( dV, getXMMReg(rG) );
         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx,modrm);
            assign( sV, getXMMReg(rE) );
            delta += 1;
            DIP("pmuldq %s,%s\n", nameXMMReg(rE), nameXMMReg(rG));
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            assign( sV, loadLE(Ity_V128, mkexpr(addr)) );
            delta += alen;
            DIP("pmuldq %s,%s\n", dis_buf, nameXMMReg(rG));
         }

         putXMMReg( rG, mkexpr(math_PMULDQ_128( dV, sV )) );
         goto decode_success;
      }
      break;

   case 0x29:
      if (have66noF2noF3(pfx) && sz == 2) { 
         
         delta = dis_SSEint_E_to_G( vbi, pfx, delta, 
                                    "pcmpeqq", Iop_CmpEQ64x2, False );
         goto decode_success;
      }
      break;

   case 0x2A:
      if (have66noF2noF3(pfx) && sz == 2) {
         modrm = getUChar(delta);
         if (!epartIsReg(modrm)) {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            gen_SEGV_if_not_16_aligned( addr );
            putXMMReg( gregOfRexRM(pfx,modrm),
                       loadLE(Ity_V128, mkexpr(addr)) );
            DIP("movntdqa %s,%s\n", dis_buf,
                                    nameXMMReg(gregOfRexRM(pfx,modrm)));
            delta += alen;
            goto decode_success;
         }
      }
      break;

   case 0x2B:
      if (have66noF2noF3(pfx) && sz == 2) {
  
         modrm = getUChar(delta);

         IRTemp argL = newTemp(Ity_V128);
         IRTemp argR = newTemp(Ity_V128);

         if ( epartIsReg(modrm) ) {
            assign( argL, getXMMReg( eregOfRexRM(pfx, modrm) ) );
            delta += 1;
            DIP( "packusdw %s,%s\n",
                 nameXMMReg( eregOfRexRM(pfx, modrm) ),
                 nameXMMReg( gregOfRexRM(pfx, modrm) ) );
         } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 0 );
            gen_SEGV_if_not_16_aligned( addr );
            assign( argL, loadLE( Ity_V128, mkexpr(addr) ));
            delta += alen;
            DIP( "packusdw %s,%s\n",
                 dis_buf, nameXMMReg( gregOfRexRM(pfx, modrm) ) );
         }

         assign(argR, getXMMReg( gregOfRexRM(pfx, modrm) ));

         putXMMReg( gregOfRexRM(pfx, modrm), 
                    binop( Iop_QNarrowBin32Sto16Ux8,
                           mkexpr(argL), mkexpr(argR)) );

         goto decode_success;
      }
      break;

   case 0x30:
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_PMOVxXBW_128( vbi, pfx, delta,
                                   False, True );
         goto decode_success;
      }
      break;

   case 0x31:
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_PMOVxXBD_128( vbi, pfx, delta,
                                   False, True );
         goto decode_success;
      }
      break;

   case 0x32:
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_PMOVZXBQ_128( vbi, pfx, delta, False );
         goto decode_success;
      }
      break;

   case 0x33:
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_PMOVxXWD_128( vbi, pfx, delta,
                                   False, True );
         goto decode_success;
      }
      break;

   case 0x34:
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_PMOVZXWQ_128( vbi, pfx, delta, False );
         goto decode_success;
      }
      break;

   case 0x35:
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_PMOVxXDQ_128( vbi, pfx, delta,
                                   False, True );
         goto decode_success;
      }
      break;

   case 0x37:
      if (have66noF2noF3(pfx) && sz == 2) {
         
         delta = dis_SSEint_E_to_G( vbi, pfx, delta,
                                    "pcmpgtq", Iop_CmpGT64Sx2, False );
         goto decode_success;
      }
      break;

   case 0x38:
   case 0x3C:
      if (have66noF2noF3(pfx) && sz == 2) {
         
         Bool isMAX = opc == 0x3C;
         delta = dis_SSEint_E_to_G(
                    vbi, pfx, delta, 
                    isMAX ? "pmaxsb" : "pminsb",
                    isMAX ? Iop_Max8Sx16 : Iop_Min8Sx16,
                    False
                 );
         goto decode_success;
      }
      break;

   case 0x39:
   case 0x3D:
      if (have66noF2noF3(pfx) && sz == 2) {
         
         Bool isMAX = opc == 0x3D;
         delta = dis_SSEint_E_to_G(
                    vbi, pfx, delta, 
                    isMAX ? "pmaxsd" : "pminsd",
                    isMAX ? Iop_Max32Sx4 : Iop_Min32Sx4,
                    False
                 );
         goto decode_success;
      }
      break;

   case 0x3A:
   case 0x3E:
      if (have66noF2noF3(pfx) && sz == 2) {
         
         Bool isMAX = opc == 0x3E;
         delta = dis_SSEint_E_to_G(
                    vbi, pfx, delta, 
                    isMAX ? "pmaxuw" : "pminuw",
                    isMAX ? Iop_Max16Ux8 : Iop_Min16Ux8,
                    False
                 );
         goto decode_success;
      }
      break;

   case 0x3B:
   case 0x3F:
      if (have66noF2noF3(pfx) && sz == 2) {
         
         Bool isMAX = opc == 0x3F;
         delta = dis_SSEint_E_to_G(
                    vbi, pfx, delta, 
                    isMAX ? "pmaxud" : "pminud",
                    isMAX ? Iop_Max32Ux4 : Iop_Min32Ux4,
                    False
                 );
         goto decode_success;
      }
      break;

   case 0x40:
      if (have66noF2noF3(pfx) && sz == 2) {
  
         modrm = getUChar(delta);

         IRTemp argL = newTemp(Ity_V128);
         IRTemp argR = newTemp(Ity_V128);

         if ( epartIsReg(modrm) ) {
            assign( argL, getXMMReg( eregOfRexRM(pfx, modrm) ) );
            delta += 1;
            DIP( "pmulld %s,%s\n",
                 nameXMMReg( eregOfRexRM(pfx, modrm) ),
                 nameXMMReg( gregOfRexRM(pfx, modrm) ) );
         } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 0 );
            gen_SEGV_if_not_16_aligned( addr );
            assign( argL, loadLE( Ity_V128, mkexpr(addr) ));
            delta += alen;
            DIP( "pmulld %s,%s\n",
                 dis_buf, nameXMMReg( gregOfRexRM(pfx, modrm) ) );
         }

         assign(argR, getXMMReg( gregOfRexRM(pfx, modrm) ));

         putXMMReg( gregOfRexRM(pfx, modrm), 
                    binop( Iop_Mul32x4, mkexpr(argL), mkexpr(argR)) );

         goto decode_success;
      }
      break;

   case 0x41:
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_PHMINPOSUW_128( vbi, pfx, delta, False );
         goto decode_success;
      } 
      break;

   case 0xDC:
   case 0xDD:
   case 0xDE:
   case 0xDF:
   case 0xDB:
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_AESx( vbi, pfx, delta, False, opc );
         goto decode_success;
      }
      break;

   case 0xF0:
   case 0xF1:
      if (haveF2noF3(pfx)
          && (opc == 0xF1 || (opc == 0xF0 && !have66(pfx)))) {
         modrm = getUChar(delta);

         if (opc == 0xF0) 
            sz = 1;
         else
            vassert(sz == 2 || sz == 4 || sz == 8);

         IRType tyE = szToITy(sz);
         IRTemp valE = newTemp(tyE);

         if (epartIsReg(modrm)) {
            assign(valE, getIRegE(sz, pfx, modrm));
            delta += 1;
            DIP("crc32b %s,%s\n", nameIRegE(sz, pfx, modrm),
                nameIRegG(1==getRexW(pfx) ? 8 : 4, pfx, modrm));
         } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 0 );
            assign(valE, loadLE(tyE, mkexpr(addr)));
            delta += alen;
            DIP("crc32b %s,%s\n", dis_buf,
                nameIRegG(1==getRexW(pfx) ? 8 : 4, pfx, modrm));
         }

         IRTemp valG0 = newTemp(Ity_I64);
         assign(valG0, binop(Iop_And64, getIRegG(8, pfx, modrm),
                             mkU64(0xFFFFFFFF)));

         const HChar* nm = NULL;
         void*  fn = NULL;
         switch (sz) {
            case 1: nm = "amd64g_calc_crc32b";
                    fn = &amd64g_calc_crc32b; break;
            case 2: nm = "amd64g_calc_crc32w";
                    fn = &amd64g_calc_crc32w; break;
            case 4: nm = "amd64g_calc_crc32l";
                    fn = &amd64g_calc_crc32l; break;
            case 8: nm = "amd64g_calc_crc32q";
                    fn = &amd64g_calc_crc32q; break;
         }
         vassert(nm && fn);
         IRTemp valG1 = newTemp(Ity_I64);
         assign(valG1,
                mkIRExprCCall(Ity_I64, 0, nm, fn, 
                              mkIRExprVec_2(mkexpr(valG0),
                                            widenUto64(mkexpr(valE)))));

         putIRegG(4, pfx, modrm, unop(Iop_64to32, mkexpr(valG1)));
         goto decode_success;
      }
      break;

   default:
      break;

   }

  
   *decode_OK = False;
   return deltaIN;

  decode_success:
   *decode_OK = True;
   return delta;
}



static Long dis_PEXTRW ( const VexAbiInfo* vbi, Prefix pfx,
                         Long delta, Bool isAvx )
{
   IRTemp addr  = IRTemp_INVALID;
   IRTemp t0    = IRTemp_INVALID;
   IRTemp t1    = IRTemp_INVALID;
   IRTemp t2    = IRTemp_INVALID;
   IRTemp t3    = IRTemp_INVALID;
   UChar  modrm = getUChar(delta);
   Int    alen  = 0;
   HChar  dis_buf[50];
   UInt   rG    = gregOfRexRM(pfx,modrm);
   Int    imm8_20;
   IRTemp xmm_vec = newTemp(Ity_V128);
   IRTemp d16   = newTemp(Ity_I16);
   const HChar* mbV = isAvx ? "v" : "";

   vassert(0==getRexW(pfx)); 
   assign( xmm_vec, getXMMReg(rG) );
   breakupV128to32s( xmm_vec, &t3, &t2, &t1, &t0 );

   if ( epartIsReg( modrm ) ) {
      imm8_20 = (Int)(getUChar(delta+1) & 7);
   } else { 
      addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 1 );
      imm8_20 = (Int)(getUChar(delta+alen) & 7);
   }

   switch (imm8_20) {
      case 0:  assign(d16, unop(Iop_32to16,   mkexpr(t0))); break;
      case 1:  assign(d16, unop(Iop_32HIto16, mkexpr(t0))); break;
      case 2:  assign(d16, unop(Iop_32to16,   mkexpr(t1))); break;
      case 3:  assign(d16, unop(Iop_32HIto16, mkexpr(t1))); break;
      case 4:  assign(d16, unop(Iop_32to16,   mkexpr(t2))); break;
      case 5:  assign(d16, unop(Iop_32HIto16, mkexpr(t2))); break;
      case 6:  assign(d16, unop(Iop_32to16,   mkexpr(t3))); break;
      case 7:  assign(d16, unop(Iop_32HIto16, mkexpr(t3))); break;
      default: vassert(0);
   }

   if ( epartIsReg( modrm ) ) {
      UInt rE = eregOfRexRM(pfx,modrm);
      putIReg32( rE, unop(Iop_16Uto32, mkexpr(d16)) );
      delta += 1+1;
      DIP( "%spextrw $%d, %s,%s\n", mbV, imm8_20,
           nameXMMReg( rG ), nameIReg32( rE ) );
   } else {
      storeLE( mkexpr(addr), mkexpr(d16) );
      delta += alen+1;
      DIP( "%spextrw $%d, %s,%s\n", mbV, imm8_20, nameXMMReg( rG ), dis_buf );
   }
   return delta;
}


static Long dis_PEXTRD ( const VexAbiInfo* vbi, Prefix pfx,
                         Long delta, Bool isAvx )
{
   IRTemp addr  = IRTemp_INVALID;
   IRTemp t0    = IRTemp_INVALID;
   IRTemp t1    = IRTemp_INVALID;
   IRTemp t2    = IRTemp_INVALID;
   IRTemp t3    = IRTemp_INVALID;
   UChar  modrm = 0;
   Int    alen  = 0;
   HChar  dis_buf[50];

   Int    imm8_10;
   IRTemp xmm_vec   = newTemp(Ity_V128);
   IRTemp src_dword = newTemp(Ity_I32);
   const HChar* mbV = isAvx ? "v" : "";

   vassert(0==getRexW(pfx)); 
   modrm = getUChar(delta);
   assign( xmm_vec, getXMMReg( gregOfRexRM(pfx,modrm) ) );
   breakupV128to32s( xmm_vec, &t3, &t2, &t1, &t0 );

   if ( epartIsReg( modrm ) ) {
      imm8_10 = (Int)(getUChar(delta+1) & 3);
   } else { 
      addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 1 );
      imm8_10 = (Int)(getUChar(delta+alen) & 3);
   }

   switch ( imm8_10 ) {
      case 0:  assign( src_dword, mkexpr(t0) ); break;
      case 1:  assign( src_dword, mkexpr(t1) ); break;
      case 2:  assign( src_dword, mkexpr(t2) ); break;
      case 3:  assign( src_dword, mkexpr(t3) ); break;
      default: vassert(0);
   }

   if ( epartIsReg( modrm ) ) {
      putIReg32( eregOfRexRM(pfx,modrm), mkexpr(src_dword) );
      delta += 1+1;
      DIP( "%spextrd $%d, %s,%s\n", mbV, imm8_10,
           nameXMMReg( gregOfRexRM(pfx, modrm) ),
           nameIReg32( eregOfRexRM(pfx, modrm) ) );
   } else {
      storeLE( mkexpr(addr), mkexpr(src_dword) );
      delta += alen+1;
      DIP( "%spextrd $%d, %s,%s\n", mbV,
           imm8_10, nameXMMReg( gregOfRexRM(pfx, modrm) ), dis_buf );
   }
   return delta;
}


static Long dis_PEXTRQ ( const VexAbiInfo* vbi, Prefix pfx,
                         Long delta, Bool isAvx )
{
   IRTemp addr  = IRTemp_INVALID;
   UChar  modrm = 0;
   Int    alen  = 0;
   HChar  dis_buf[50];

   Int imm8_0;
   IRTemp xmm_vec   = newTemp(Ity_V128);
   IRTemp src_qword = newTemp(Ity_I64);
   const HChar* mbV = isAvx ? "v" : "";

   vassert(1==getRexW(pfx)); 
   modrm = getUChar(delta);
   assign( xmm_vec, getXMMReg( gregOfRexRM(pfx,modrm) ) );

   if ( epartIsReg( modrm ) ) {
      imm8_0 = (Int)(getUChar(delta+1) & 1);
   } else {
      addr   = disAMode( &alen, vbi, pfx, delta, dis_buf, 1 );
      imm8_0 = (Int)(getUChar(delta+alen) & 1);
   }

   switch ( imm8_0 ) {
      case 0:  assign( src_qword, unop(Iop_V128to64,   mkexpr(xmm_vec)) );
               break;
      case 1:  assign( src_qword, unop(Iop_V128HIto64, mkexpr(xmm_vec)) );
               break;
      default: vassert(0);
   }

   if ( epartIsReg( modrm ) ) {
      putIReg64( eregOfRexRM(pfx,modrm), mkexpr(src_qword) );
      delta += 1+1;
      DIP( "%spextrq $%d, %s,%s\n", mbV, imm8_0,
           nameXMMReg( gregOfRexRM(pfx, modrm) ),
           nameIReg64( eregOfRexRM(pfx, modrm) ) );
   } else {
      storeLE( mkexpr(addr), mkexpr(src_qword) );
      delta += alen+1;
      DIP( "%spextrq $%d, %s,%s\n", mbV,
           imm8_0, nameXMMReg( gregOfRexRM(pfx, modrm) ), dis_buf );
   }
   return delta;
}

static IRExpr* math_CTZ32(IRExpr *exp)
{
   
   return unop(Iop_64to32, unop(Iop_Ctz64, unop(Iop_32Uto64, exp)));
}

static Long dis_PCMPISTRI_3A ( UChar modrm, UInt regNoL, UInt regNoR,
                               Long delta, UChar opc, UChar imm,
                               HChar dis_buf[])
{
   
   vassert((opc & 0x03) == 0x03);
   
   vassert((imm & ~0x02) == 0x38);

   
   IRTemp argL = newTemp(Ity_V128);
   assign(argL, getXMMReg(regNoL));
   IRTemp argR = newTemp(Ity_V128);
   assign(argR, getXMMReg(regNoR));

   IRTemp zmaskL = newTemp(Ity_I32);
   assign(zmaskL, unop(Iop_16Uto32,
                       unop(Iop_GetMSBs8x16,
                            binop(Iop_CmpEQ8x16, mkexpr(argL), mkV128(0)))));
   IRTemp zmaskR = newTemp(Ity_I32);
   assign(zmaskR, unop(Iop_16Uto32,
                       unop(Iop_GetMSBs8x16,
                            binop(Iop_CmpEQ8x16, mkexpr(argR), mkV128(0)))));


   IRExpr *ctzL = unop(Iop_32to8, math_CTZ32(mkexpr(zmaskL)));

   IRTemp zmaskL_zero = newTemp(Ity_I1);
   assign(zmaskL_zero, binop(Iop_ExpCmpNE32, mkexpr(zmaskL), mkU32(0)));

   IRTemp validL = newTemp(Ity_I32);
   assign(validL, binop(Iop_Sub32,
                        IRExpr_ITE(mkexpr(zmaskL_zero),
                                   binop(Iop_Shl32, mkU32(1), ctzL),
                                   mkU32(0)),
                        mkU32(1)));

   
   IRExpr *ctzR = unop(Iop_32to8, math_CTZ32(mkexpr(zmaskR)));
   IRTemp zmaskR_zero = newTemp(Ity_I1);
   assign(zmaskR_zero, binop(Iop_ExpCmpNE32, mkexpr(zmaskR), mkU32(0)));
   IRTemp validR = newTemp(Ity_I32);
   assign(validR, binop(Iop_Sub32,
                        IRExpr_ITE(mkexpr(zmaskR_zero),
                                   binop(Iop_Shl32, mkU32(1), ctzR),
                                   mkU32(0)),
                        mkU32(1)));

   
   IRExpr *boolResII = unop(Iop_16Uto32,
                            unop(Iop_GetMSBs8x16,
                                 binop(Iop_CmpEQ8x16, mkexpr(argL),
                                                      mkexpr(argR))));

   IRExpr *intRes1_a = binop(Iop_And32, boolResII,
                             binop(Iop_And32,
                                   mkexpr(validL), mkexpr(validR)));

   
   IRExpr *intRes1_b = unop(Iop_Not32, binop(Iop_Or32,
                                             mkexpr(validL), mkexpr(validR)));
   
   IRExpr *intRes1 = binop(Iop_And32, mkU32(0xFFFF),
                           binop(Iop_Or32, intRes1_a, intRes1_b));

   IRTemp intRes2 = newTemp(Ity_I32);
   assign(intRes2, binop(Iop_And32, mkU32(0xFFFF),
                         binop(Iop_Xor32, intRes1, mkexpr(validL))));

   IRExpr *newECX = math_CTZ32(binop(Iop_Or32,
                                     mkexpr(intRes2), mkU32(0x10000)));

   
   putIReg32(R_RCX, newECX);

   

   
   IRExpr *c_bit = IRExpr_ITE( binop(Iop_ExpCmpNE32, mkexpr(intRes2),
                                     mkU32(0)),
                               mkU32(1 << AMD64G_CC_SHIFT_C),
                               mkU32(0));
   
   IRExpr *z_bit = IRExpr_ITE( mkexpr(zmaskL_zero),
                               mkU32(1 << AMD64G_CC_SHIFT_Z),
                               mkU32(0));
   
   IRExpr *s_bit = IRExpr_ITE( mkexpr(zmaskR_zero),
                               mkU32(1 << AMD64G_CC_SHIFT_S),
                               mkU32(0));
   
   IRExpr *o_bit = binop(Iop_Shl32, binop(Iop_And32, mkexpr(intRes2),
                                          mkU32(0x01)),
                         mkU8(AMD64G_CC_SHIFT_O));

   
   IRTemp cc = newTemp(Ity_I64);
   assign(cc, widenUto64(binop(Iop_Or32,
                               binop(Iop_Or32, c_bit, z_bit),
                               binop(Iop_Or32, s_bit, o_bit))));
   stmt(IRStmt_Put(OFFB_CC_OP, mkU64(AMD64G_CC_OP_COPY)));
   stmt(IRStmt_Put(OFFB_CC_DEP1, mkexpr(cc)));
   stmt(IRStmt_Put(OFFB_CC_DEP2, mkU64(0)));
   stmt(IRStmt_Put(OFFB_CC_NDEP, mkU64(0)));

   return delta;
}

static Long dis_PCMPxSTRx ( const VexAbiInfo* vbi, Prefix pfx,
                            Long delta, Bool isAvx, UChar opc )
{
   Long   delta0  = delta;
   UInt   isISTRx = opc & 2;
   UInt   isxSTRM = (opc & 1) ^ 1;
   UInt   regNoL  = 0;
   UInt   regNoR  = 0;
   UChar  imm     = 0;
   IRTemp addr    = IRTemp_INVALID;
   Int    alen    = 0;
   HChar  dis_buf[50];

   UChar modrm = getUChar(delta);
   if (epartIsReg(modrm)) {
      regNoL = eregOfRexRM(pfx, modrm);
      regNoR = gregOfRexRM(pfx, modrm);
      imm = getUChar(delta+1);
      delta += 1+1;
   } else {
      regNoL = 16; 
      regNoR = gregOfRexRM(pfx, modrm);
      addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 1 );
      stmt( IRStmt_Put( OFFB_YMM16, loadLE(Ity_V128, mkexpr(addr)) ));
      imm = getUChar(delta+alen);
      delta += alen+1;
   }

   if (regNoL == 16) {
      DIP("%spcmp%cstr%c $%x,%s,%s\n",
          isAvx ? "v" : "", isISTRx ? 'i' : 'e', isxSTRM ? 'm' : 'i',
          (UInt)imm, dis_buf, nameXMMReg(regNoR));
   } else {
      DIP("%spcmp%cstr%c $%x,%s,%s\n",
          isAvx ? "v" : "", isISTRx ? 'i' : 'e', isxSTRM ? 'm' : 'i',
          (UInt)imm, nameXMMReg(regNoL), nameXMMReg(regNoR));
   }

   
   if (imm == 0x3A && isISTRx && !isxSTRM) {
      return dis_PCMPISTRI_3A ( modrm, regNoL, regNoR, delta,
                                opc, imm, dis_buf);
   }

   switch (imm) {
      case 0x00: case 0x02: case 0x08: case 0x0A: case 0x0C: case 0x0E:
      case 0x12: case 0x14: case 0x1A:
      case 0x30: case 0x34: case 0x38: case 0x3A:
      case 0x40: case 0x44: case 0x46: case 0x4A:
         break;
      
      case 0x01: case 0x03: case 0x09: case 0x0B: case 0x0D:
      case 0x13:            case 0x1B:
                            case 0x39: case 0x3B:
                 case 0x45:            case 0x4B:
         break;
      default:
         return delta0; 
   }

   
   void*  fn = &amd64g_dirtyhelper_PCMPxSTRx;
   const HChar* nm = "amd64g_dirtyhelper_PCMPxSTRx";

   UInt gstOffL = regNoL == 16 ? OFFB_YMM16 : ymmGuestRegOffset(regNoL);
   UInt gstOffR = ymmGuestRegOffset(regNoR);

   IRExpr*  opc4_and_imm = mkU64((opc << 8) | (imm & 0xFF));
   IRExpr*  gstOffLe     = mkU64(gstOffL);
   IRExpr*  gstOffRe     = mkU64(gstOffR);
   IRExpr*  edxIN        = isISTRx ? mkU64(0) : getIRegRDX(8);
   IRExpr*  eaxIN        = isISTRx ? mkU64(0) : getIRegRAX(8);
   IRExpr** args
      = mkIRExprVec_6( IRExpr_BBPTR(),
                       opc4_and_imm, gstOffLe, gstOffRe, edxIN, eaxIN );

   IRTemp   resT = newTemp(Ity_I64);
   IRDirty* d    = unsafeIRDirty_1_N( resT, 0, nm, fn, args );
   d->nFxState = 2;
   vex_bzero(&d->fxState, sizeof(d->fxState));
   d->fxState[0].fx     = Ifx_Read;
   d->fxState[0].offset = gstOffL;
   d->fxState[0].size   = sizeof(U128);
   d->fxState[1].fx     = Ifx_Read;
   d->fxState[1].offset = gstOffR;
   d->fxState[1].size   = sizeof(U128);
   if (isxSTRM) {
      
      d->nFxState = 3;
      d->fxState[2].fx     = Ifx_Write;
      d->fxState[2].offset = ymmGuestRegOffset(0);
      d->fxState[2].size   = sizeof(U128);
   }

   stmt( IRStmt_Dirty(d) );

   if (!isxSTRM) {
      putIReg64(R_RCX, binop(Iop_And64,
                             binop(Iop_Shr64, mkexpr(resT), mkU8(16)),
                             mkU64(0xFFFF)));
   }

   
   if (isxSTRM && isAvx)
      putYMMRegLane128(0, 1, mkV128(0));

   stmt( IRStmt_Put(
            OFFB_CC_DEP1,
            binop(Iop_And64, mkexpr(resT), mkU64(0xFFFF))
   ));
   stmt( IRStmt_Put( OFFB_CC_OP,   mkU64(AMD64G_CC_OP_COPY) ));
   stmt( IRStmt_Put( OFFB_CC_DEP2, mkU64(0) ));
   stmt( IRStmt_Put( OFFB_CC_NDEP, mkU64(0) ));

   return delta;
}


static IRTemp math_PINSRB_128 ( IRTemp v128, IRTemp u8, UInt imm8 )
{
   vassert(imm8 >= 0 && imm8 <= 15);

   
   
   IRTemp tmp128    = newTemp(Ity_V128);
   IRTemp halfshift = newTemp(Ity_I64);
   assign(halfshift, binop(Iop_Shl64,
                           unop(Iop_8Uto64, mkexpr(u8)),
                           mkU8(8 * (imm8 & 7))));
   if (imm8 < 8) {
      assign(tmp128, binop(Iop_64HLtoV128, mkU64(0), mkexpr(halfshift)));
   } else {
      assign(tmp128, binop(Iop_64HLtoV128, mkexpr(halfshift), mkU64(0)));
   }

   UShort mask = ~(1 << imm8);
   IRTemp res  = newTemp(Ity_V128);
   assign( res, binop(Iop_OrV128,
                      mkexpr(tmp128),
                      binop(Iop_AndV128, mkexpr(v128), mkV128(mask))) );
   return res;
}


static IRTemp math_PINSRD_128 ( IRTemp v128, IRTemp u32, UInt imm8 )
{
   IRTemp z32 = newTemp(Ity_I32);
   assign(z32, mkU32(0));

   IRTemp withZs = newTemp(Ity_V128);
   UShort mask = 0;
   switch (imm8) {
      case 3:  mask = 0x0FFF;
               assign(withZs, mkV128from32s(u32, z32, z32, z32));
               break;
      case 2:  mask = 0xF0FF;
               assign(withZs, mkV128from32s(z32, u32, z32, z32));
               break;
      case 1:  mask = 0xFF0F;
               assign(withZs, mkV128from32s(z32, z32, u32, z32));
               break;
      case 0:  mask = 0xFFF0;
               assign(withZs, mkV128from32s(z32, z32, z32, u32));
               break;
      default: vassert(0);
   }

   IRTemp res = newTemp(Ity_V128);
   assign(res, binop( Iop_OrV128,
                      mkexpr(withZs),
                      binop( Iop_AndV128, mkexpr(v128), mkV128(mask) ) ) );
   return res;
}


static IRTemp math_PINSRQ_128 ( IRTemp v128, IRTemp u64, UInt imm8 )
{
   IRTemp withZs = newTemp(Ity_V128);
   UShort mask = 0;
   if (imm8 == 0) { 
      mask = 0xFF00; 
      assign(withZs, binop(Iop_64HLtoV128, mkU64(0), mkexpr(u64)));
   } else {
      vassert(imm8 == 1);
      mask = 0x00FF;
      assign( withZs, binop(Iop_64HLtoV128, mkexpr(u64), mkU64(0)));
   }

   IRTemp res = newTemp(Ity_V128);
   assign( res, binop( Iop_OrV128,
                       mkexpr(withZs),
                       binop( Iop_AndV128, mkexpr(v128), mkV128(mask) ) ) );
   return res;
}


static IRTemp math_INSERTPS ( IRTemp dstV, IRTemp toInsertD, UInt imm8 )
{
   const IRTemp inval = IRTemp_INVALID;
   IRTemp dstDs[4] = { inval, inval, inval, inval };
   breakupV128to32s( dstV, &dstDs[3], &dstDs[2], &dstDs[1], &dstDs[0] );

   vassert(imm8 <= 255);
   dstDs[(imm8 >> 4) & 3] = toInsertD; 

   UInt imm8_zmask = (imm8 & 15);
   IRTemp zero_32 = newTemp(Ity_I32);
   assign( zero_32, mkU32(0) );
   IRTemp resV = newTemp(Ity_V128);
   assign( resV, mkV128from32s( 
                    ((imm8_zmask & 8) == 8) ? zero_32 : dstDs[3], 
                    ((imm8_zmask & 4) == 4) ? zero_32 : dstDs[2], 
                    ((imm8_zmask & 2) == 2) ? zero_32 : dstDs[1], 
                    ((imm8_zmask & 1) == 1) ? zero_32 : dstDs[0]) );
   return resV;
}


static Long dis_PEXTRB_128_GtoE ( const VexAbiInfo* vbi, Prefix pfx,
                                  Long delta, Bool isAvx )
{
   IRTemp addr     = IRTemp_INVALID;
   Int    alen     = 0;
   HChar  dis_buf[50];
   IRTemp xmm_vec  = newTemp(Ity_V128);
   IRTemp sel_lane = newTemp(Ity_I32);
   IRTemp shr_lane = newTemp(Ity_I32);
   const HChar* mbV = isAvx ? "v" : "";
   UChar  modrm    = getUChar(delta);
   IRTemp t3, t2, t1, t0;
   Int    imm8;
   assign( xmm_vec, getXMMReg( gregOfRexRM(pfx,modrm) ) );
   t3 = t2 = t1 = t0 = IRTemp_INVALID;
   breakupV128to32s( xmm_vec, &t3, &t2, &t1, &t0 );

   if ( epartIsReg( modrm ) ) {
      imm8 = (Int)getUChar(delta+1);
   } else {
      addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 1 );
      imm8 = (Int)getUChar(delta+alen);
   }
   switch ( (imm8 >> 2) & 3 ) {
      case 0:  assign( sel_lane, mkexpr(t0) ); break;
      case 1:  assign( sel_lane, mkexpr(t1) ); break;
      case 2:  assign( sel_lane, mkexpr(t2) ); break;
      case 3:  assign( sel_lane, mkexpr(t3) ); break;
      default: vassert(0);
   }
   assign( shr_lane, 
           binop( Iop_Shr32, mkexpr(sel_lane), mkU8(((imm8 & 3)*8)) ) );

   if ( epartIsReg( modrm ) ) {
      putIReg64( eregOfRexRM(pfx,modrm), 
                 unop( Iop_32Uto64, 
                       binop(Iop_And32, mkexpr(shr_lane), mkU32(255)) ) );
      delta += 1+1;
      DIP( "%spextrb $%d, %s,%s\n", mbV, imm8, 
           nameXMMReg( gregOfRexRM(pfx, modrm) ), 
           nameIReg64( eregOfRexRM(pfx, modrm) ) );
   } else {
      storeLE( mkexpr(addr), unop(Iop_32to8, mkexpr(shr_lane) ) );
      delta += alen+1;
      DIP( "%spextrb $%d,%s,%s\n", mbV,
           imm8, nameXMMReg( gregOfRexRM(pfx, modrm) ), dis_buf );
   }
   
   return delta;
}


static IRTemp math_DPPD_128 ( IRTemp src_vec, IRTemp dst_vec, UInt imm8 )
{
   vassert(imm8 < 256);
   UShort imm8_perms[4] = { 0x0000, 0x00FF, 0xFF00, 0xFFFF };
   IRTemp and_vec = newTemp(Ity_V128);
   IRTemp sum_vec = newTemp(Ity_V128);
   IRTemp rm      = newTemp(Ity_I32);
   assign( rm, get_FAKE_roundingmode() ); 
   assign( and_vec, binop( Iop_AndV128,
                           triop( Iop_Mul64Fx2,
                                  mkexpr(rm),
                                  mkexpr(dst_vec), mkexpr(src_vec) ),
                           mkV128( imm8_perms[ ((imm8 >> 4) & 3) ] ) ) );

   assign( sum_vec, binop( Iop_Add64F0x2,
                           binop( Iop_InterleaveHI64x2,
                                  mkexpr(and_vec), mkexpr(and_vec) ),
                           binop( Iop_InterleaveLO64x2,
                                  mkexpr(and_vec), mkexpr(and_vec) ) ) );
   IRTemp res = newTemp(Ity_V128);
   assign(res, binop( Iop_AndV128,
                      binop( Iop_InterleaveLO64x2,
                             mkexpr(sum_vec), mkexpr(sum_vec) ),
                      mkV128( imm8_perms[ (imm8 & 3) ] ) ) );
   return res;
}


static IRTemp math_DPPS_128 ( IRTemp src_vec, IRTemp dst_vec, UInt imm8 )
{
   vassert(imm8 < 256);
   IRTemp tmp_prod_vec = newTemp(Ity_V128);
   IRTemp prod_vec     = newTemp(Ity_V128);
   IRTemp sum_vec      = newTemp(Ity_V128);
   IRTemp rm           = newTemp(Ity_I32);
   IRTemp v3, v2, v1, v0;
   v3 = v2 = v1 = v0   = IRTemp_INVALID;
   UShort imm8_perms[16] = { 0x0000, 0x000F, 0x00F0, 0x00FF, 0x0F00, 
                             0x0F0F, 0x0FF0, 0x0FFF, 0xF000, 0xF00F,
                             0xF0F0, 0xF0FF, 0xFF00, 0xFF0F, 0xFFF0,
                             0xFFFF };

   assign( rm, get_FAKE_roundingmode() ); 
   assign( tmp_prod_vec, 
           binop( Iop_AndV128, 
                  triop( Iop_Mul32Fx4,
                         mkexpr(rm), mkexpr(dst_vec), mkexpr(src_vec) ), 
                  mkV128( imm8_perms[((imm8 >> 4)& 15)] ) ) );
   breakupV128to32s( tmp_prod_vec, &v3, &v2, &v1, &v0 );
   assign( prod_vec, mkV128from32s( v3, v1, v2, v0 ) );

   assign( sum_vec, triop( Iop_Add32Fx4,
                           mkexpr(rm),
                           binop( Iop_InterleaveHI32x4, 
                                  mkexpr(prod_vec), mkexpr(prod_vec) ), 
                           binop( Iop_InterleaveLO32x4, 
                                  mkexpr(prod_vec), mkexpr(prod_vec) ) ) );

   IRTemp res = newTemp(Ity_V128);
   assign( res, binop( Iop_AndV128, 
                       triop( Iop_Add32Fx4,
                              mkexpr(rm),
                              binop( Iop_InterleaveHI32x4,
                                     mkexpr(sum_vec), mkexpr(sum_vec) ), 
                              binop( Iop_InterleaveLO32x4,
                                     mkexpr(sum_vec), mkexpr(sum_vec) ) ), 
                       mkV128( imm8_perms[ (imm8 & 15) ] ) ) );
   return res;
}


static IRTemp math_MPSADBW_128 ( IRTemp dst_vec, IRTemp src_vec, UInt imm8 )
{
   UShort src_mask[4] = { 0x000F, 0x00F0, 0x0F00, 0xF000 };
   UShort dst_mask[2] = { 0x07FF, 0x7FF0 };

   IRTemp src_maskV = newTemp(Ity_V128);
   IRTemp dst_maskV = newTemp(Ity_V128);
   assign(src_maskV, mkV128( src_mask[ imm8 & 3 ] ));
   assign(dst_maskV, mkV128( dst_mask[ (imm8 >> 2) & 1 ] ));

   IRTemp src_masked = newTemp(Ity_V128);
   IRTemp dst_masked = newTemp(Ity_V128);
   assign(src_masked, binop(Iop_AndV128, mkexpr(src_vec), mkexpr(src_maskV)));
   assign(dst_masked, binop(Iop_AndV128, mkexpr(dst_vec), mkexpr(dst_maskV)));

   
   IRTemp sHi = newTemp(Ity_I64);
   IRTemp sLo = newTemp(Ity_I64);
   assign( sHi, unop(Iop_V128HIto64, mkexpr(src_masked)) );
   assign( sLo, unop(Iop_V128to64,   mkexpr(src_masked)) );

   IRTemp dHi = newTemp(Ity_I64);
   IRTemp dLo = newTemp(Ity_I64);
   assign( dHi, unop(Iop_V128HIto64, mkexpr(dst_masked)) );
   assign( dLo, unop(Iop_V128to64,   mkexpr(dst_masked)) );

   
   IRTemp resHi = newTemp(Ity_I64);
   IRTemp resLo = newTemp(Ity_I64);

   IRExpr** argsHi
      = mkIRExprVec_5( mkexpr(sHi), mkexpr(sLo), mkexpr(dHi), mkexpr(dLo),
                       mkU64( 0x80 | (imm8 & 7) ));
   IRExpr** argsLo
      = mkIRExprVec_5( mkexpr(sHi), mkexpr(sLo), mkexpr(dHi), mkexpr(dLo),
                       mkU64( 0x00 | (imm8 & 7) ));

   assign(resHi, mkIRExprCCall( Ity_I64, 0,
                                "amd64g_calc_mpsadbw",
                                &amd64g_calc_mpsadbw, argsHi ));
   assign(resLo, mkIRExprCCall( Ity_I64, 0,
                                "amd64g_calc_mpsadbw",
                                &amd64g_calc_mpsadbw, argsLo ));

   IRTemp res = newTemp(Ity_V128);
   assign(res, binop(Iop_64HLtoV128, mkexpr(resHi), mkexpr(resLo)));
   return res;
}

static Long dis_EXTRACTPS ( const VexAbiInfo* vbi, Prefix pfx,
                            Long delta, Bool isAvx )
{
   IRTemp addr       = IRTemp_INVALID;
   Int    alen       = 0;
   HChar  dis_buf[50];
   UChar  modrm      = getUChar(delta);
   Int imm8_10;
   IRTemp xmm_vec    = newTemp(Ity_V128);
   IRTemp src_dword  = newTemp(Ity_I32);
   UInt   rG         = gregOfRexRM(pfx,modrm);
   IRTemp t3, t2, t1, t0;
   t3 = t2 = t1 = t0 = IRTemp_INVALID;

   assign( xmm_vec, getXMMReg( rG ) );
   breakupV128to32s( xmm_vec, &t3, &t2, &t1, &t0 );

   if ( epartIsReg( modrm ) ) {
      imm8_10 = (Int)(getUChar(delta+1) & 3);
   } else { 
      addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 1 );
      imm8_10 = (Int)(getUChar(delta+alen) & 3);
   }

   switch ( imm8_10 ) {
      case 0:  assign( src_dword, mkexpr(t0) ); break;
      case 1:  assign( src_dword, mkexpr(t1) ); break;
      case 2:  assign( src_dword, mkexpr(t2) ); break;
      case 3:  assign( src_dword, mkexpr(t3) ); break;
      default: vassert(0);
   }

   if ( epartIsReg( modrm ) ) {
      UInt rE = eregOfRexRM(pfx,modrm);
      putIReg32( rE, mkexpr(src_dword) );
      delta += 1+1;
      DIP( "%sextractps $%d, %s,%s\n", isAvx ? "v" : "", imm8_10,
           nameXMMReg( rG ), nameIReg32( rE ) );
   } else {
      storeLE( mkexpr(addr), mkexpr(src_dword) );
      delta += alen+1;
      DIP( "%sextractps $%d, %s,%s\n", isAvx ? "v" : "", imm8_10,
           nameXMMReg( rG ), dis_buf );
   }

   return delta;
}


static IRTemp math_PCLMULQDQ( IRTemp dV, IRTemp sV, UInt imm8 )
{
   IRTemp t0 = newTemp(Ity_I64);
   IRTemp t1 = newTemp(Ity_I64);
   assign(t0, unop((imm8&1)? Iop_V128HIto64 : Iop_V128to64, 
              mkexpr(dV)));
   assign(t1, unop((imm8&16) ? Iop_V128HIto64 : Iop_V128to64,
              mkexpr(sV)));

   IRTemp t2 = newTemp(Ity_I64);
   IRTemp t3 = newTemp(Ity_I64);

   IRExpr** args;

   args = mkIRExprVec_3(mkexpr(t0), mkexpr(t1), mkU64(0));
   assign(t2, mkIRExprCCall(Ity_I64,0, "amd64g_calculate_pclmul",
                            &amd64g_calculate_pclmul, args));
   args = mkIRExprVec_3(mkexpr(t0), mkexpr(t1), mkU64(1));
   assign(t3, mkIRExprCCall(Ity_I64,0, "amd64g_calculate_pclmul",
                            &amd64g_calculate_pclmul, args));

   IRTemp res     = newTemp(Ity_V128);
   assign(res, binop(Iop_64HLtoV128, mkexpr(t3), mkexpr(t2)));
   return res;
}


__attribute__((noinline))
static
Long dis_ESC_0F3A__SSE4 ( Bool* decode_OK,
                          const VexAbiInfo* vbi,
                          Prefix pfx, Int sz, Long deltaIN )
{
   IRTemp addr  = IRTemp_INVALID;
   UChar  modrm = 0;
   Int    alen  = 0;
   HChar  dis_buf[50];

   *decode_OK = False;

   Long   delta = deltaIN;
   UChar  opc   = getUChar(delta);
   delta++;
   switch (opc) {

   case 0x08:
      
      if (have66noF2noF3(pfx) && sz == 2) {

         IRTemp src0 = newTemp(Ity_F32);
         IRTemp src1 = newTemp(Ity_F32);
         IRTemp src2 = newTemp(Ity_F32);
         IRTemp src3 = newTemp(Ity_F32);
         IRTemp res0 = newTemp(Ity_F32);
         IRTemp res1 = newTemp(Ity_F32);
         IRTemp res2 = newTemp(Ity_F32);
         IRTemp res3 = newTemp(Ity_F32);
         IRTemp rm   = newTemp(Ity_I32);
         Int    imm  = 0;

         modrm = getUChar(delta);

         if (epartIsReg(modrm)) {
            assign( src0, 
                    getXMMRegLane32F( eregOfRexRM(pfx, modrm), 0 ) );
            assign( src1, 
                    getXMMRegLane32F( eregOfRexRM(pfx, modrm), 1 ) );
            assign( src2, 
                    getXMMRegLane32F( eregOfRexRM(pfx, modrm), 2 ) );
            assign( src3, 
                    getXMMRegLane32F( eregOfRexRM(pfx, modrm), 3 ) );
            imm = getUChar(delta+1);
            if (imm & ~15) goto decode_failure;
            delta += 1+1;
            DIP( "roundps $%d,%s,%s\n",
                 imm, nameXMMReg( eregOfRexRM(pfx, modrm) ),
                      nameXMMReg( gregOfRexRM(pfx, modrm) ) );
         } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 1 );
            gen_SEGV_if_not_16_aligned(addr);
            assign( src0, loadLE(Ity_F32,
                                 binop(Iop_Add64, mkexpr(addr), mkU64(0) )));
            assign( src1, loadLE(Ity_F32,
                                 binop(Iop_Add64, mkexpr(addr), mkU64(4) )));
            assign( src2, loadLE(Ity_F32,
                                 binop(Iop_Add64, mkexpr(addr), mkU64(8) )));
            assign( src3, loadLE(Ity_F32,
                                 binop(Iop_Add64, mkexpr(addr), mkU64(12) )));
            imm = getUChar(delta+alen);
            if (imm & ~15) goto decode_failure;
            delta += alen+1;
            DIP( "roundps $%d,%s,%s\n",
                 imm, dis_buf, nameXMMReg( gregOfRexRM(pfx, modrm) ) );
         }

         assign(rm, (imm & 4) ? get_sse_roundingmode() : mkU32(imm & 3));

         assign(res0, binop(Iop_RoundF32toInt, mkexpr(rm), mkexpr(src0)) );
         assign(res1, binop(Iop_RoundF32toInt, mkexpr(rm), mkexpr(src1)) );
         assign(res2, binop(Iop_RoundF32toInt, mkexpr(rm), mkexpr(src2)) );
         assign(res3, binop(Iop_RoundF32toInt, mkexpr(rm), mkexpr(src3)) );

         putXMMRegLane32F( gregOfRexRM(pfx, modrm), 0, mkexpr(res0) );
         putXMMRegLane32F( gregOfRexRM(pfx, modrm), 1, mkexpr(res1) );
         putXMMRegLane32F( gregOfRexRM(pfx, modrm), 2, mkexpr(res2) );
         putXMMRegLane32F( gregOfRexRM(pfx, modrm), 3, mkexpr(res3) );

         goto decode_success;
      }
      break;

   case 0x09:
      
      if (have66noF2noF3(pfx) && sz == 2) {

         IRTemp src0 = newTemp(Ity_F64);
         IRTemp src1 = newTemp(Ity_F64);
         IRTemp res0 = newTemp(Ity_F64);
         IRTemp res1 = newTemp(Ity_F64);
         IRTemp rm   = newTemp(Ity_I32);
         Int    imm  = 0;

         modrm = getUChar(delta);

         if (epartIsReg(modrm)) {
            assign( src0, 
                    getXMMRegLane64F( eregOfRexRM(pfx, modrm), 0 ) );
            assign( src1, 
                    getXMMRegLane64F( eregOfRexRM(pfx, modrm), 1 ) );
            imm = getUChar(delta+1);
            if (imm & ~15) goto decode_failure;
            delta += 1+1;
            DIP( "roundpd $%d,%s,%s\n",
                 imm, nameXMMReg( eregOfRexRM(pfx, modrm) ),
                      nameXMMReg( gregOfRexRM(pfx, modrm) ) );
         } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 1 );
            gen_SEGV_if_not_16_aligned(addr);
            assign( src0, loadLE(Ity_F64,
                                 binop(Iop_Add64, mkexpr(addr), mkU64(0) )));
            assign( src1, loadLE(Ity_F64,
                                 binop(Iop_Add64, mkexpr(addr), mkU64(8) )));
            imm = getUChar(delta+alen);
            if (imm & ~15) goto decode_failure;
            delta += alen+1;
            DIP( "roundpd $%d,%s,%s\n",
                 imm, dis_buf, nameXMMReg( gregOfRexRM(pfx, modrm) ) );
         }

         assign(rm, (imm & 4) ? get_sse_roundingmode() : mkU32(imm & 3));

         assign(res0, binop(Iop_RoundF64toInt, mkexpr(rm), mkexpr(src0)) );
         assign(res1, binop(Iop_RoundF64toInt, mkexpr(rm), mkexpr(src1)) );

         putXMMRegLane64F( gregOfRexRM(pfx, modrm), 0, mkexpr(res0) );
         putXMMRegLane64F( gregOfRexRM(pfx, modrm), 1, mkexpr(res1) );

         goto decode_success;
      }
      break;

   case 0x0A:
   case 0x0B:
      if (have66noF2noF3(pfx) && sz == 2) {

         Bool   isD = opc == 0x0B;
         IRTemp src = newTemp(isD ? Ity_F64 : Ity_F32);
         IRTemp res = newTemp(isD ? Ity_F64 : Ity_F32);
         Int    imm = 0;

         modrm = getUChar(delta);

         if (epartIsReg(modrm)) {
            assign( src, 
                    isD ? getXMMRegLane64F( eregOfRexRM(pfx, modrm), 0 )
                        : getXMMRegLane32F( eregOfRexRM(pfx, modrm), 0 ) );
            imm = getUChar(delta+1);
            if (imm & ~15) goto decode_failure;
            delta += 1+1;
            DIP( "rounds%c $%d,%s,%s\n",
                 isD ? 'd' : 's',
                 imm, nameXMMReg( eregOfRexRM(pfx, modrm) ),
                      nameXMMReg( gregOfRexRM(pfx, modrm) ) );
         } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 1 );
            assign( src, loadLE( isD ? Ity_F64 : Ity_F32, mkexpr(addr) ));
            imm = getUChar(delta+alen);
            if (imm & ~15) goto decode_failure;
            delta += alen+1;
            DIP( "rounds%c $%d,%s,%s\n",
                 isD ? 'd' : 's',
                 imm, dis_buf, nameXMMReg( gregOfRexRM(pfx, modrm) ) );
         }

         assign(res, binop(isD ? Iop_RoundF64toInt : Iop_RoundF32toInt,
                           (imm & 4) ? get_sse_roundingmode() 
                                     : mkU32(imm & 3),
                           mkexpr(src)) );

         if (isD)
            putXMMRegLane64F( gregOfRexRM(pfx, modrm), 0, mkexpr(res) );
         else
            putXMMRegLane32F( gregOfRexRM(pfx, modrm), 0, mkexpr(res) );

         goto decode_success;
      }
      break;

   case 0x0C:
      if (have66noF2noF3(pfx) && sz == 2) {

         Int imm8;
         IRTemp dst_vec = newTemp(Ity_V128);
         IRTemp src_vec = newTemp(Ity_V128);

         modrm = getUChar(delta);

         assign( dst_vec, getXMMReg( gregOfRexRM(pfx, modrm) ) );

         if ( epartIsReg( modrm ) ) {
            imm8 = (Int)getUChar(delta+1);
            assign( src_vec, getXMMReg( eregOfRexRM(pfx, modrm) ) );
            delta += 1+1;
            DIP( "blendps $%d, %s,%s\n", imm8,
                 nameXMMReg( eregOfRexRM(pfx, modrm) ),
                 nameXMMReg( gregOfRexRM(pfx, modrm) ) );    
         } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 
                             1 );
            gen_SEGV_if_not_16_aligned( addr );
            assign( src_vec, loadLE( Ity_V128, mkexpr(addr) ) );
            imm8 = (Int)getUChar(delta+alen);
            delta += alen+1;
            DIP( "blendpd $%d, %s,%s\n", 
                 imm8, dis_buf, nameXMMReg( gregOfRexRM(pfx, modrm) ) );
         }

         putXMMReg( gregOfRexRM(pfx, modrm), 
                    mkexpr( math_BLENDPS_128( src_vec, dst_vec, imm8) ) );
         goto decode_success;
      }
      break;

   case 0x0D:
      if (have66noF2noF3(pfx) && sz == 2) {

         Int imm8;
         IRTemp dst_vec = newTemp(Ity_V128);
         IRTemp src_vec = newTemp(Ity_V128);

         modrm = getUChar(delta);
         assign( dst_vec, getXMMReg( gregOfRexRM(pfx, modrm) ) );

         if ( epartIsReg( modrm ) ) {
            imm8 = (Int)getUChar(delta+1);
            assign( src_vec, getXMMReg( eregOfRexRM(pfx, modrm) ) );
            delta += 1+1;
            DIP( "blendpd $%d, %s,%s\n", imm8,
                 nameXMMReg( eregOfRexRM(pfx, modrm) ),
                 nameXMMReg( gregOfRexRM(pfx, modrm) ) );
         } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 
                             1 );
            gen_SEGV_if_not_16_aligned( addr );
            assign( src_vec, loadLE( Ity_V128, mkexpr(addr) ) );
            imm8 = (Int)getUChar(delta+alen);
            delta += alen+1;
            DIP( "blendpd $%d, %s,%s\n", 
                 imm8, dis_buf, nameXMMReg( gregOfRexRM(pfx, modrm) ) );
         }

         putXMMReg( gregOfRexRM(pfx, modrm), 
                    mkexpr( math_BLENDPD_128( src_vec, dst_vec, imm8) ) );
         goto decode_success;
      }
      break;

   case 0x0E:
      if (have66noF2noF3(pfx) && sz == 2) {

         Int imm8;
         IRTemp dst_vec = newTemp(Ity_V128);
         IRTemp src_vec = newTemp(Ity_V128);

         modrm = getUChar(delta);

         assign( dst_vec, getXMMReg( gregOfRexRM(pfx, modrm) ) );

         if ( epartIsReg( modrm ) ) {
            imm8 = (Int)getUChar(delta+1);
            assign( src_vec, getXMMReg( eregOfRexRM(pfx, modrm) ) );
            delta += 1+1;
            DIP( "pblendw $%d, %s,%s\n", imm8,
                 nameXMMReg( eregOfRexRM(pfx, modrm) ),
                 nameXMMReg( gregOfRexRM(pfx, modrm) ) );    
         } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 
                             1 );
            gen_SEGV_if_not_16_aligned( addr );
            assign( src_vec, loadLE( Ity_V128, mkexpr(addr) ) );
            imm8 = (Int)getUChar(delta+alen);
            delta += alen+1;
            DIP( "pblendw $%d, %s,%s\n", 
                 imm8, dis_buf, nameXMMReg( gregOfRexRM(pfx, modrm) ) );
         }

         putXMMReg( gregOfRexRM(pfx, modrm), 
                    mkexpr( math_PBLENDW_128( src_vec, dst_vec, imm8) ) );
         goto decode_success;
      }
      break;

   case 0x14:
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_PEXTRB_128_GtoE( vbi, pfx, delta, False );
         goto decode_success;
      }
      break;

   case 0x15:
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_PEXTRW( vbi, pfx, delta, False );
         goto decode_success;
      }
      break;

   case 0x16:
      if (have66noF2noF3(pfx) 
          && sz == 2 ) {
         delta = dis_PEXTRD( vbi, pfx, delta, False );
         goto decode_success;
      }
      if (have66noF2noF3(pfx) 
          && sz == 8 ) {
         delta = dis_PEXTRQ( vbi, pfx, delta, False);
         goto decode_success;
      }
      break;

   case 0x17:
      if (have66noF2noF3(pfx) 
          && (sz == 2 ||  sz == 8)) {
         delta = dis_EXTRACTPS( vbi, pfx, delta, False );
         goto decode_success;
      }
      break;

   case 0x20:
      if (have66noF2noF3(pfx) && sz == 2) {
         Int    imm8;
         IRTemp new8 = newTemp(Ity_I8);
         modrm = getUChar(delta);
         UInt rG = gregOfRexRM(pfx, modrm);
         if ( epartIsReg( modrm ) ) {
            UInt rE = eregOfRexRM(pfx,modrm);
            imm8 = (Int)(getUChar(delta+1) & 0xF);
            assign( new8, unop(Iop_32to8, getIReg32(rE)) );
            delta += 1+1;
            DIP( "pinsrb $%d,%s,%s\n", imm8,
                 nameIReg32(rE), nameXMMReg(rG) );
         } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 1 );
            imm8 = (Int)(getUChar(delta+alen) & 0xF);
            assign( new8, loadLE( Ity_I8, mkexpr(addr) ) );
            delta += alen+1;
            DIP( "pinsrb $%d,%s,%s\n", 
                 imm8, dis_buf, nameXMMReg(rG) );
         }
         IRTemp src_vec = newTemp(Ity_V128);
         assign(src_vec, getXMMReg( gregOfRexRM(pfx, modrm) ));
         IRTemp res = math_PINSRB_128( src_vec, new8, imm8 );
         putXMMReg( rG, mkexpr(res) );
         goto decode_success;
      }
      break;

   case 0x21:
      if (have66noF2noF3(pfx) && sz == 2) {
         UInt   imm8;
         IRTemp d2ins = newTemp(Ity_I32); 
         const IRTemp inval = IRTemp_INVALID;

         modrm = getUChar(delta);
         UInt rG = gregOfRexRM(pfx, modrm);

         if ( epartIsReg( modrm ) ) {
            UInt   rE = eregOfRexRM(pfx, modrm);
            IRTemp vE = newTemp(Ity_V128);
            assign( vE, getXMMReg(rE) );
            IRTemp dsE[4] = { inval, inval, inval, inval };
            breakupV128to32s( vE, &dsE[3], &dsE[2], &dsE[1], &dsE[0] );
            imm8 = getUChar(delta+1);
            d2ins = dsE[(imm8 >> 6) & 3]; 
            delta += 1+1;
            DIP( "insertps $%u, %s,%s\n",
                 imm8, nameXMMReg(rE), nameXMMReg(rG) );
         } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 1 );
            assign( d2ins, loadLE( Ity_I32, mkexpr(addr) ) );
            imm8 = getUChar(delta+alen);
            delta += alen+1;
            DIP( "insertps $%u, %s,%s\n", 
                 imm8, dis_buf, nameXMMReg(rG) );
         }

         IRTemp vG = newTemp(Ity_V128);
         assign( vG, getXMMReg(rG) );

         putXMMReg( rG, mkexpr(math_INSERTPS( vG, d2ins, imm8 )) );
         goto decode_success;
      }
      break;

   case 0x22:
      if (have66noF2noF3(pfx) 
          && sz == 2 ) {
         Int    imm8_10;
         IRTemp src_u32 = newTemp(Ity_I32);
         modrm = getUChar(delta);
         UInt rG = gregOfRexRM(pfx, modrm);

         if ( epartIsReg( modrm ) ) {
            UInt rE = eregOfRexRM(pfx,modrm);
            imm8_10 = (Int)(getUChar(delta+1) & 3);
            assign( src_u32, getIReg32( rE ) );
            delta += 1+1;
            DIP( "pinsrd $%d, %s,%s\n",
                 imm8_10, nameIReg32(rE), nameXMMReg(rG) );
         } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 1 );
            imm8_10 = (Int)(getUChar(delta+alen) & 3);
            assign( src_u32, loadLE( Ity_I32, mkexpr(addr) ) );
            delta += alen+1;
            DIP( "pinsrd $%d, %s,%s\n", 
                 imm8_10, dis_buf, nameXMMReg(rG) );
         }

         IRTemp src_vec = newTemp(Ity_V128);
         assign(src_vec, getXMMReg( rG ));
         IRTemp res_vec = math_PINSRD_128( src_vec, src_u32, imm8_10 );
         putXMMReg( rG, mkexpr(res_vec) );
         goto decode_success;
      }
      if (have66noF2noF3(pfx) 
          && sz == 8 ) {
         Int imm8_0;
         IRTemp src_u64 = newTemp(Ity_I64);
         modrm = getUChar(delta);
         UInt rG = gregOfRexRM(pfx, modrm);

         if ( epartIsReg( modrm ) ) {
            UInt rE = eregOfRexRM(pfx,modrm);
            imm8_0 = (Int)(getUChar(delta+1) & 1);
            assign( src_u64, getIReg64( rE ) );
            delta += 1+1;
            DIP( "pinsrq $%d, %s,%s\n",
                 imm8_0, nameIReg64(rE), nameXMMReg(rG) );
         } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 1 );
            imm8_0 = (Int)(getUChar(delta+alen) & 1);
            assign( src_u64, loadLE( Ity_I64, mkexpr(addr) ) );
            delta += alen+1;
            DIP( "pinsrq $%d, %s,%s\n", 
                 imm8_0, dis_buf, nameXMMReg(rG) );
         }

         IRTemp src_vec = newTemp(Ity_V128);
         assign(src_vec, getXMMReg( rG ));
         IRTemp res_vec = math_PINSRQ_128( src_vec, src_u64, imm8_0 );
         putXMMReg( rG, mkexpr(res_vec) );
         goto decode_success;
      }
      break;

   case 0x40:
      if (have66noF2noF3(pfx) && sz == 2) {
         modrm = getUChar(delta);
         Int    imm8;
         IRTemp src_vec = newTemp(Ity_V128);
         IRTemp dst_vec = newTemp(Ity_V128);
         UInt   rG      = gregOfRexRM(pfx, modrm);
         assign( dst_vec, getXMMReg( rG ) );
         if ( epartIsReg( modrm ) ) {
            UInt rE = eregOfRexRM(pfx, modrm);
            imm8 = (Int)getUChar(delta+1);
            assign( src_vec, getXMMReg(rE) );
            delta += 1+1;
            DIP( "dpps $%d, %s,%s\n",
                 imm8, nameXMMReg(rE), nameXMMReg(rG) );    
         } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 
                             1 );
            gen_SEGV_if_not_16_aligned( addr );
            assign( src_vec, loadLE( Ity_V128, mkexpr(addr) ) );
            imm8 = (Int)getUChar(delta+alen);
            delta += alen+1;
            DIP( "dpps $%d, %s,%s\n", 
                 imm8, dis_buf, nameXMMReg(rG) );
         }
         IRTemp res = math_DPPS_128( src_vec, dst_vec, imm8 );
         putXMMReg( rG, mkexpr(res) );
         goto decode_success;
      }
      break;

   case 0x41:
      if (have66noF2noF3(pfx) && sz == 2) {
         modrm = getUChar(delta);
         Int    imm8;
         IRTemp src_vec = newTemp(Ity_V128);
         IRTemp dst_vec = newTemp(Ity_V128);
         UInt   rG      = gregOfRexRM(pfx, modrm);
         assign( dst_vec, getXMMReg( rG ) );
         if ( epartIsReg( modrm ) ) {
            UInt rE = eregOfRexRM(pfx, modrm);
            imm8 = (Int)getUChar(delta+1);
            assign( src_vec, getXMMReg(rE) );
            delta += 1+1;
            DIP( "dppd $%d, %s,%s\n",
                 imm8, nameXMMReg(rE), nameXMMReg(rG) );    
         } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 
                             1 );
            gen_SEGV_if_not_16_aligned( addr );
            assign( src_vec, loadLE( Ity_V128, mkexpr(addr) ) );
            imm8 = (Int)getUChar(delta+alen);
            delta += alen+1;
            DIP( "dppd $%d, %s,%s\n", 
                 imm8, dis_buf, nameXMMReg(rG) );
         }
         IRTemp res = math_DPPD_128( src_vec, dst_vec, imm8 );
         putXMMReg( rG, mkexpr(res) );
         goto decode_success;
      }
      break;

   case 0x42:
      if (have66noF2noF3(pfx) && sz == 2) {
         Int    imm8;
         IRTemp src_vec = newTemp(Ity_V128);
         IRTemp dst_vec = newTemp(Ity_V128);
         modrm          = getUChar(delta);
         UInt   rG      = gregOfRexRM(pfx, modrm);

         assign( dst_vec, getXMMReg(rG) );
  
         if ( epartIsReg( modrm ) ) {
            UInt rE = eregOfRexRM(pfx, modrm);

            imm8 = (Int)getUChar(delta+1);
            assign( src_vec, getXMMReg(rE) );
            delta += 1+1;
            DIP( "mpsadbw $%d, %s,%s\n", imm8,
                 nameXMMReg(rE), nameXMMReg(rG) );
         } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 
                             1 );
            gen_SEGV_if_not_16_aligned( addr );
            assign( src_vec, loadLE( Ity_V128, mkexpr(addr) ) );
            imm8 = (Int)getUChar(delta+alen);
            delta += alen+1;
            DIP( "mpsadbw $%d, %s,%s\n", imm8, dis_buf, nameXMMReg(rG) );
         }

         putXMMReg( rG, mkexpr( math_MPSADBW_128(dst_vec, src_vec, imm8) ) );
         goto decode_success;
      }
      break;

   case 0x44:
      if (have66noF2noF3(pfx) && sz == 2) {
  
         Int imm8;
         IRTemp svec = newTemp(Ity_V128);
         IRTemp dvec = newTemp(Ity_V128);
         modrm       = getUChar(delta);
         UInt   rG   = gregOfRexRM(pfx, modrm);

         assign( dvec, getXMMReg(rG) );
  
         if ( epartIsReg( modrm ) ) {
            UInt rE = eregOfRexRM(pfx, modrm);
            imm8 = (Int)getUChar(delta+1);
            assign( svec, getXMMReg(rE) );
            delta += 1+1;
            DIP( "pclmulqdq $%d, %s,%s\n", imm8,
                 nameXMMReg(rE), nameXMMReg(rG) );    
         } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 
                             1 );
            gen_SEGV_if_not_16_aligned( addr );
            assign( svec, loadLE( Ity_V128, mkexpr(addr) ) );
            imm8 = (Int)getUChar(delta+alen);
            delta += alen+1;
            DIP( "pclmulqdq $%d, %s,%s\n", 
                 imm8, dis_buf, nameXMMReg(rG) );
         }

         putXMMReg( rG, mkexpr( math_PCLMULQDQ(dvec, svec, imm8) ) );
         goto decode_success;
      }
      break;

   case 0x60:
   case 0x61:
   case 0x62:
   case 0x63:
      if (have66noF2noF3(pfx) && sz == 2) {
         Long delta0 = delta;
         delta = dis_PCMPxSTRx( vbi, pfx, delta, False, opc );
         if (delta > delta0) goto decode_success;
         
      }
      break;

   case 0xDF:
      
      if (have66noF2noF3(pfx) && sz == 2) {
         delta = dis_AESKEYGENASSIST( vbi, pfx, delta, False );
         goto decode_success;
      }
      break;

   default:
      break;

   }

  decode_failure:
   *decode_OK = False;
   return deltaIN;

  decode_success:
   *decode_OK = True;
   return delta;
}



__attribute__((noinline))
static
Long dis_ESC_NONE (
        DisResult* dres,
        Bool*      expect_CAS,
        Bool         (*resteerOkFn) ( void*, Addr ),
        Bool         resteerCisOk,
        void*        callback_opaque,
        const VexArchInfo* archinfo,
        const VexAbiInfo*  vbi,
        Prefix pfx, Int sz, Long deltaIN 
     )
{
   Long   d64   = 0;
   UChar  abyte = 0;
   IRTemp addr  = IRTemp_INVALID;
   IRTemp t1    = IRTemp_INVALID;
   IRTemp t2    = IRTemp_INVALID;
   IRTemp t3    = IRTemp_INVALID;
   IRTemp t4    = IRTemp_INVALID;
   IRTemp t5    = IRTemp_INVALID;
   IRType ty    = Ity_INVALID;
   UChar  modrm = 0;
   Int    am_sz = 0;
   Int    d_sz  = 0;
   Int    alen  = 0;
   HChar  dis_buf[50];

   Long   delta = deltaIN;
   UChar  opc   = getUChar(delta); delta++;

   Bool validF2orF3 = haveF2orF3(pfx) ? False : True;
   { UChar tmp_modrm = getUChar(delta);
     switch (opc) {
        case 0x00:   case 0x01: 
        case 0x08:   case 0x09: 
        case 0x10:   case 0x11: 
        case 0x18:   case 0x19: 
        case 0x20:   case 0x21: 
        case 0x28:   case 0x29: 
        case 0x30:   case 0x31: 
           if (!epartIsReg(tmp_modrm)
               && haveF2orF3(pfx) && !haveF2andF3(pfx) && haveLOCK(pfx)) {
              
              validF2orF3 = True;
           }
           break;
        default:
           break;
     }
   }

   switch (opc) {

   case 0x00: 
      if (!validF2orF3) goto decode_failure;
      delta = dis_op2_G_E ( vbi, pfx, False, Iop_Add8, True, 1, delta, "add" );
      return delta;
   case 0x01: 
      if (!validF2orF3) goto decode_failure;
      delta = dis_op2_G_E ( vbi, pfx, False, Iop_Add8, True, sz, delta, "add" );
      return delta;

   case 0x02: 
      if (haveF2orF3(pfx)) goto decode_failure;
      delta = dis_op2_E_G ( vbi, pfx, False, Iop_Add8, True, 1, delta, "add" );
      return delta;
   case 0x03: 
      if (haveF2orF3(pfx)) goto decode_failure;
      delta = dis_op2_E_G ( vbi, pfx, False, Iop_Add8, True, sz, delta, "add" );
      return delta;

   case 0x04: 
      if (haveF2orF3(pfx)) goto decode_failure;
      delta = dis_op_imm_A( 1, False, Iop_Add8, True, delta, "add" );
      return delta;
   case 0x05: 
      if (haveF2orF3(pfx)) goto decode_failure;
      delta = dis_op_imm_A(sz, False, Iop_Add8, True, delta, "add" );
      return delta;

   case 0x08: 
      if (!validF2orF3) goto decode_failure;
      delta = dis_op2_G_E ( vbi, pfx, False, Iop_Or8, True, 1, delta, "or" );
      return delta;
   case 0x09: 
      if (!validF2orF3) goto decode_failure;
      delta = dis_op2_G_E ( vbi, pfx, False, Iop_Or8, True, sz, delta, "or" );
      return delta;

   case 0x0A: 
      if (haveF2orF3(pfx)) goto decode_failure;
      delta = dis_op2_E_G ( vbi, pfx, False, Iop_Or8, True, 1, delta, "or" );
      return delta;
   case 0x0B: 
      if (haveF2orF3(pfx)) goto decode_failure;
      delta = dis_op2_E_G ( vbi, pfx, False, Iop_Or8, True, sz, delta, "or" );
      return delta;

   case 0x0C: 
      if (haveF2orF3(pfx)) goto decode_failure;
      delta = dis_op_imm_A( 1, False, Iop_Or8, True, delta, "or" );
      return delta;
   case 0x0D: 
      if (haveF2orF3(pfx)) goto decode_failure;
      delta = dis_op_imm_A( sz, False, Iop_Or8, True, delta, "or" );
      return delta;

   case 0x10: 
      if (!validF2orF3) goto decode_failure;
      delta = dis_op2_G_E ( vbi, pfx, True, Iop_Add8, True, 1, delta, "adc" );
      return delta;
   case 0x11: 
      if (!validF2orF3) goto decode_failure;
      delta = dis_op2_G_E ( vbi, pfx, True, Iop_Add8, True, sz, delta, "adc" );
      return delta;

   case 0x12: 
      if (haveF2orF3(pfx)) goto decode_failure;
      delta = dis_op2_E_G ( vbi, pfx, True, Iop_Add8, True, 1, delta, "adc" );
      return delta;
   case 0x13: 
      if (haveF2orF3(pfx)) goto decode_failure;
      delta = dis_op2_E_G ( vbi, pfx, True, Iop_Add8, True, sz, delta, "adc" );
      return delta;

   case 0x14: 
      if (haveF2orF3(pfx)) goto decode_failure;
      delta = dis_op_imm_A( 1, True, Iop_Add8, True, delta, "adc" );
      return delta;
   case 0x15: 
      if (haveF2orF3(pfx)) goto decode_failure;
      delta = dis_op_imm_A( sz, True, Iop_Add8, True, delta, "adc" );
      return delta;

   case 0x18: 
      if (!validF2orF3) goto decode_failure;
      delta = dis_op2_G_E ( vbi, pfx, True, Iop_Sub8, True, 1, delta, "sbb" );
      return delta;
   case 0x19: 
      if (!validF2orF3) goto decode_failure;
      delta = dis_op2_G_E ( vbi, pfx, True, Iop_Sub8, True, sz, delta, "sbb" );
      return delta;

   case 0x1A: 
      if (haveF2orF3(pfx)) goto decode_failure;
      delta = dis_op2_E_G ( vbi, pfx, True, Iop_Sub8, True, 1, delta, "sbb" );
      return delta;
   case 0x1B: 
      if (haveF2orF3(pfx)) goto decode_failure;
      delta = dis_op2_E_G ( vbi, pfx, True, Iop_Sub8, True, sz, delta, "sbb" );
      return delta;

   case 0x1C: 
      if (haveF2orF3(pfx)) goto decode_failure;
      delta = dis_op_imm_A( 1, True, Iop_Sub8, True, delta, "sbb" );
      return delta;
   case 0x1D: 
      if (haveF2orF3(pfx)) goto decode_failure;
      delta = dis_op_imm_A( sz, True, Iop_Sub8, True, delta, "sbb" );
      return delta;

   case 0x20: 
      if (!validF2orF3) goto decode_failure;
      delta = dis_op2_G_E ( vbi, pfx, False, Iop_And8, True, 1, delta, "and" );
      return delta;
   case 0x21: 
      if (!validF2orF3) goto decode_failure;
      delta = dis_op2_G_E ( vbi, pfx, False, Iop_And8, True, sz, delta, "and" );
      return delta;

   case 0x22: 
      if (haveF2orF3(pfx)) goto decode_failure;
      delta = dis_op2_E_G ( vbi, pfx, False, Iop_And8, True, 1, delta, "and" );
      return delta;
   case 0x23: 
      if (haveF2orF3(pfx)) goto decode_failure;
      delta = dis_op2_E_G ( vbi, pfx, False, Iop_And8, True, sz, delta, "and" );
      return delta;

   case 0x24: 
      if (haveF2orF3(pfx)) goto decode_failure;
      delta = dis_op_imm_A( 1, False, Iop_And8, True, delta, "and" );
      return delta;
   case 0x25: 
      if (haveF2orF3(pfx)) goto decode_failure;
      delta = dis_op_imm_A( sz, False, Iop_And8, True, delta, "and" );
      return delta;

   case 0x28: 
      if (!validF2orF3) goto decode_failure;
      delta = dis_op2_G_E ( vbi, pfx, False, Iop_Sub8, True, 1, delta, "sub" );
      return delta;
   case 0x29: 
      if (!validF2orF3) goto decode_failure;
      delta = dis_op2_G_E ( vbi, pfx, False, Iop_Sub8, True, sz, delta, "sub" );
      return delta;

   case 0x2A: 
      if (haveF2orF3(pfx)) goto decode_failure;
      delta = dis_op2_E_G ( vbi, pfx, False, Iop_Sub8, True, 1, delta, "sub" );
      return delta;
   case 0x2B: 
      if (haveF2orF3(pfx)) goto decode_failure;
      delta = dis_op2_E_G ( vbi, pfx, False, Iop_Sub8, True, sz, delta, "sub" );
      return delta;

   case 0x2C: 
      if (haveF2orF3(pfx)) goto decode_failure;
      delta = dis_op_imm_A(1, False, Iop_Sub8, True, delta, "sub" );
      return delta;
   case 0x2D: 
      if (haveF2orF3(pfx)) goto decode_failure;
      delta = dis_op_imm_A( sz, False, Iop_Sub8, True, delta, "sub" );
      return delta;

   case 0x30: 
      if (!validF2orF3) goto decode_failure;
      delta = dis_op2_G_E ( vbi, pfx, False, Iop_Xor8, True, 1, delta, "xor" );
      return delta;
   case 0x31: 
      if (!validF2orF3) goto decode_failure;
      delta = dis_op2_G_E ( vbi, pfx, False, Iop_Xor8, True, sz, delta, "xor" );
      return delta;

   case 0x32: 
      if (haveF2orF3(pfx)) goto decode_failure;
      delta = dis_op2_E_G ( vbi, pfx, False, Iop_Xor8, True, 1, delta, "xor" );
      return delta;
   case 0x33: 
      if (haveF2orF3(pfx)) goto decode_failure;
      delta = dis_op2_E_G ( vbi, pfx, False, Iop_Xor8, True, sz, delta, "xor" );
      return delta;

   case 0x34: 
      if (haveF2orF3(pfx)) goto decode_failure;
      delta = dis_op_imm_A( 1, False, Iop_Xor8, True, delta, "xor" );
      return delta;
   case 0x35: 
      if (haveF2orF3(pfx)) goto decode_failure;
      delta = dis_op_imm_A( sz, False, Iop_Xor8, True, delta, "xor" );
      return delta;

   case 0x38: 
      if (haveF2orF3(pfx)) goto decode_failure;
      delta = dis_op2_G_E ( vbi, pfx, False, Iop_Sub8, False, 1, delta, "cmp" );
      return delta;
   case 0x39: 
      if (haveF2orF3(pfx)) goto decode_failure;
      delta = dis_op2_G_E ( vbi, pfx, False, Iop_Sub8, False, sz, delta, "cmp" );
      return delta;

   case 0x3A: 
      if (haveF2orF3(pfx)) goto decode_failure;
      delta = dis_op2_E_G ( vbi, pfx, False, Iop_Sub8, False, 1, delta, "cmp" );
      return delta;
   case 0x3B: 
      if (haveF2orF3(pfx)) goto decode_failure;
      delta = dis_op2_E_G ( vbi, pfx, False, Iop_Sub8, False, sz, delta, "cmp" );
      return delta;

   case 0x3C: 
      if (haveF2orF3(pfx)) goto decode_failure;
      delta = dis_op_imm_A( 1, False, Iop_Sub8, False, delta, "cmp" );
      return delta;
   case 0x3D: 
      if (haveF2orF3(pfx)) goto decode_failure;
      delta = dis_op_imm_A( sz, False, Iop_Sub8, False, delta, "cmp" );
      return delta;

   case 0x50: 
   case 0x51: 
   case 0x52: 
   case 0x53: 
   case 0x55: 
   case 0x56: 
   case 0x57: 
   case 0x54: 
      if (haveF2orF3(pfx)) goto decode_failure;
      vassert(sz == 2 || sz == 4 || sz == 8);
      if (sz == 4)
         sz = 8; 
      ty = sz==2 ? Ity_I16 : Ity_I64;
      t1 = newTemp(ty); 
      t2 = newTemp(Ity_I64);
      assign(t1, getIRegRexB(sz, pfx, opc-0x50));
      assign(t2, binop(Iop_Sub64, getIReg64(R_RSP), mkU64(sz)));
      putIReg64(R_RSP, mkexpr(t2) );
      storeLE(mkexpr(t2),mkexpr(t1));
      DIP("push%c %s\n", nameISize(sz), nameIRegRexB(sz,pfx,opc-0x50));
      return delta;

   case 0x58: 
   case 0x59: 
   case 0x5A: 
   case 0x5B: 
   case 0x5D: 
   case 0x5E: 
   case 0x5F: 
   case 0x5C: 
      if (haveF2orF3(pfx)) goto decode_failure;
      vassert(sz == 2 || sz == 4 || sz == 8);
      if (sz == 4)
         sz = 8; 
      t1 = newTemp(szToITy(sz)); 
      t2 = newTemp(Ity_I64);
      assign(t2, getIReg64(R_RSP));
      assign(t1, loadLE(szToITy(sz),mkexpr(t2)));
      putIReg64(R_RSP, binop(Iop_Add64, mkexpr(t2), mkU64(sz)));
      putIRegRexB(sz, pfx, opc-0x58, mkexpr(t1));
      DIP("pop%c %s\n", nameISize(sz), nameIRegRexB(sz,pfx,opc-0x58));
      return delta;

   case 0x63: 
      if (haveF2orF3(pfx)) goto decode_failure;
      if (haveREX(pfx) && 1==getRexW(pfx)) {
         vassert(sz == 8);
         
         modrm = getUChar(delta);
         if (epartIsReg(modrm)) {
            delta++;
            putIRegG(8, pfx, modrm, 
                             unop(Iop_32Sto64, 
                                  getIRegE(4, pfx, modrm)));
            DIP("movslq %s,%s\n",
                nameIRegE(4, pfx, modrm),
                nameIRegG(8, pfx, modrm));
            return delta;
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            delta += alen;
            putIRegG(8, pfx, modrm, 
                             unop(Iop_32Sto64, 
                                  loadLE(Ity_I32, mkexpr(addr))));
            DIP("movslq %s,%s\n", dis_buf, 
                nameIRegG(8, pfx, modrm));
            return delta;
         }
      } else {
         goto decode_failure;
      }

   case 0x68: 
      if (haveF2orF3(pfx)) goto decode_failure;
      
      if (sz == 4) sz = 8;
      d64 = getSDisp(imin(4,sz),delta); 
      delta += imin(4,sz);
      goto do_push_I;

   case 0x69: 
      if (haveF2orF3(pfx)) goto decode_failure;
      delta = dis_imul_I_E_G ( vbi, pfx, sz, delta, sz );
      return delta;

   case 0x6A: 
      if (haveF2orF3(pfx)) goto decode_failure;
      
      if (sz == 4) sz = 8;
      d64 = getSDisp8(delta); delta += 1;
      goto do_push_I;
   do_push_I:
      ty = szToITy(sz);
      t1 = newTemp(Ity_I64);
      t2 = newTemp(ty);
      assign( t1, binop(Iop_Sub64,getIReg64(R_RSP),mkU64(sz)) );
      putIReg64(R_RSP, mkexpr(t1) );
      if (ty == Ity_I16)
         d64 &= 0xFFFF;
      storeLE( mkexpr(t1), mkU(ty,d64) );
      DIP("push%c $%lld\n", nameISize(sz), (Long)d64);
      return delta;

   case 0x6B: 
      delta = dis_imul_I_E_G ( vbi, pfx, sz, delta, 1 );
      return delta;

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
   case 0x7F: { 
      Long   jmpDelta;
      const HChar* comment  = "";
      if (haveF3(pfx)) goto decode_failure;
      if (haveF2(pfx)) DIP("bnd ; "); 
      jmpDelta = getSDisp8(delta);
      vassert(-128 <= jmpDelta && jmpDelta < 128);
      d64 = (guest_RIP_bbstart+delta+1) + jmpDelta;
      delta++;
      if (resteerCisOk
          && vex_control.guest_chase_cond
          && (Addr64)d64 != (Addr64)guest_RIP_bbstart
          && jmpDelta < 0
          && resteerOkFn( callback_opaque, (Addr64)d64) ) {
         stmt( IRStmt_Exit( 
                  mk_amd64g_calculate_condition(
                     (AMD64Condcode)(1 ^ (opc - 0x70))),
                  Ijk_Boring,
                  IRConst_U64(guest_RIP_bbstart+delta),
                  OFFB_RIP ) );
         dres->whatNext   = Dis_ResteerC;
         dres->continueAt = d64;
         comment = "(assumed taken)";
      }
      else
      if (resteerCisOk
          && vex_control.guest_chase_cond
          && (Addr64)d64 != (Addr64)guest_RIP_bbstart
          && jmpDelta >= 0
          && resteerOkFn( callback_opaque, guest_RIP_bbstart+delta ) ) {
         stmt( IRStmt_Exit( 
                  mk_amd64g_calculate_condition((AMD64Condcode)(opc - 0x70)),
                  Ijk_Boring,
                  IRConst_U64(d64),
                  OFFB_RIP ) );
         dres->whatNext   = Dis_ResteerC;
         dres->continueAt = guest_RIP_bbstart+delta;
         comment = "(assumed not taken)";
      }
      else {
         jcc_01( dres, (AMD64Condcode)(opc - 0x70),
                 guest_RIP_bbstart+delta, d64 );
         vassert(dres->whatNext == Dis_StopHere);
      }
      DIP("j%s-8 0x%llx %s\n", name_AMD64Condcode(opc - 0x70), d64, comment);
      return delta;
   }

   case 0x80: 
      modrm = getUChar(delta);
      if (epartIsReg(modrm) && haveF2orF3(pfx))
         goto decode_failure;
      if (!epartIsReg(modrm) && haveF2andF3(pfx))
         goto decode_failure;
      if (!epartIsReg(modrm) && haveF2orF3(pfx) && !haveLOCK(pfx))
         goto decode_failure;
      am_sz = lengthAMode(pfx,delta);
      sz    = 1;
      d_sz  = 1;
      d64   = getSDisp8(delta + am_sz);
      delta = dis_Grp1 ( vbi, pfx, delta, modrm, am_sz, d_sz, sz, d64 );
      return delta;

   case 0x81: 
      modrm = getUChar(delta);
      
      if (epartIsReg(modrm) && haveF2orF3(pfx))
         goto decode_failure;
      if (!epartIsReg(modrm) && haveF2andF3(pfx))
         goto decode_failure;
      if (!epartIsReg(modrm) && haveF2orF3(pfx) && !haveLOCK(pfx))
         goto decode_failure;
      am_sz = lengthAMode(pfx,delta);
      d_sz  = imin(sz,4);
      d64   = getSDisp(d_sz, delta + am_sz);
      delta = dis_Grp1 ( vbi, pfx, delta, modrm, am_sz, d_sz, sz, d64 );
      return delta;

   case 0x83: 
      if (haveF2orF3(pfx)) goto decode_failure;
      modrm = getUChar(delta);
      am_sz = lengthAMode(pfx,delta);
      d_sz  = 1;
      d64   = getSDisp8(delta + am_sz);
      delta = dis_Grp1 ( vbi, pfx, delta, modrm, am_sz, d_sz, sz, d64 );
      return delta;

   case 0x84: 
      if (haveF2orF3(pfx)) goto decode_failure;
      delta = dis_op2_E_G ( vbi, pfx, False, Iop_And8, False, 1, delta, "test" );
      return delta;

   case 0x85: 
      if (haveF2orF3(pfx)) goto decode_failure;
      delta = dis_op2_E_G ( vbi, pfx, False, Iop_And8, False, sz, delta, "test" );
      return delta;

   case 0x86: 
      sz = 1;
      
   case 0x87: 
      modrm = getUChar(delta);
      if (haveF2orF3(pfx)) {
         if (epartIsReg(modrm)) { 
            goto decode_failure;
         } else {
            if (haveF2andF3(pfx))
               goto decode_failure;
         }
      }
      ty = szToITy(sz);
      t1 = newTemp(ty); t2 = newTemp(ty);
      if (epartIsReg(modrm)) {
         assign(t1, getIRegE(sz, pfx, modrm));
         assign(t2, getIRegG(sz, pfx, modrm));
         putIRegG(sz, pfx, modrm, mkexpr(t1));
         putIRegE(sz, pfx, modrm, mkexpr(t2));
         delta++;
         DIP("xchg%c %s, %s\n", 
             nameISize(sz), nameIRegG(sz, pfx, modrm), 
                            nameIRegE(sz, pfx, modrm));
      } else {
         *expect_CAS = True;
         addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
         assign( t1, loadLE(ty, mkexpr(addr)) );
         assign( t2, getIRegG(sz, pfx, modrm) );
         casLE( mkexpr(addr),
                mkexpr(t1), mkexpr(t2), guest_RIP_curr_instr );
         putIRegG( sz, pfx, modrm, mkexpr(t1) );
         delta += alen;
         DIP("xchg%c %s, %s\n", nameISize(sz), 
                                nameIRegG(sz, pfx, modrm), dis_buf);
      }
      return delta;

   case 0x88: { 
      
      Bool ok = True;
      delta = dis_mov_G_E(vbi, pfx, 1, delta, &ok);
      if (!ok) goto decode_failure;
      return delta;
   }

   case 0x89: { 
      
      Bool ok = True;
      delta = dis_mov_G_E(vbi, pfx, sz, delta, &ok);
      if (!ok) goto decode_failure;
      return delta;
   }

   case 0x8A: 
      if (haveF2orF3(pfx)) goto decode_failure;
      delta = dis_mov_E_G(vbi, pfx, 1, delta);
      return delta;

   case 0x8B: 
      if (haveF2orF3(pfx)) goto decode_failure;
      delta = dis_mov_E_G(vbi, pfx, sz, delta);
      return delta;

   case 0x8D: 
      if (haveF2orF3(pfx)) goto decode_failure;
      if (sz != 4 && sz != 8)
         goto decode_failure;
      modrm = getUChar(delta);
      if (epartIsReg(modrm)) 
         goto decode_failure;
      addr = disAMode ( &alen, vbi, clearSegBits(pfx), delta, dis_buf, 0 );
      delta += alen;
      putIRegG( sz, pfx, modrm, 
                         sz == 4
                            ? unop(Iop_64to32, mkexpr(addr))
                            : mkexpr(addr)
              );
      DIP("lea%c %s, %s\n", nameISize(sz), dis_buf, 
                            nameIRegG(sz,pfx,modrm));
      return delta;

   case 0x8F: { 
      Int   len;
      UChar rm;
      if (haveF2orF3(pfx)) goto decode_failure;
      vassert(sz == 2 || sz == 4
              ||  sz == 8);
      if (sz == 4) sz = 8;
      if (sz != 8) goto decode_failure; 

      rm = getUChar(delta);

      
      if (epartIsReg(rm) || gregLO3ofRM(rm) != 0)
         goto decode_failure;
      
      vassert(sz == 8);      
       
      t1 = newTemp(Ity_I64);
      t3 = newTemp(Ity_I64);
      assign( t1, getIReg64(R_RSP) );
      assign( t3, loadLE(Ity_I64, mkexpr(t1)) );
       
      putIReg64(R_RSP, binop(Iop_Add64, mkexpr(t1), mkU64(sz)) );

      addr = disAMode ( &len, vbi, pfx, delta, dis_buf, 0 );
      storeLE( mkexpr(addr), mkexpr(t3) );

      DIP("popl %s\n", dis_buf);

      delta += len;
      return delta;
   }

   case 0x90: 
      
      if (!have66(pfx) && !haveF2(pfx) && haveF3(pfx)) {
         DIP("rep nop (P4 pause)\n");
         jmp_lit(dres, Ijk_Yield, guest_RIP_bbstart+delta);
         vassert(dres->whatNext == Dis_StopHere);
         return delta;
      }
      
      if (
          !haveF2orF3(pfx)
          
          && getRexB(pfx)==0 ) {
         DIP("nop\n");
         return delta;
      }
      
   case 0x91: 
   case 0x92: 
   case 0x93: 
   case 0x94: 
   case 0x95: 
   case 0x96: 
   case 0x97: 
      
      if (haveF2orF3(pfx)) goto decode_failure;
      codegen_xchg_rAX_Reg ( pfx, sz, opc - 0x90 );
      return delta;

   case 0x98: 
      if (haveF2orF3(pfx)) goto decode_failure;
      if (sz == 8) {
         putIRegRAX( 8, unop(Iop_32Sto64, getIRegRAX(4)) );
         DIP("cltq");
         return delta;
      }
      if (sz == 4) {
         putIRegRAX( 4, unop(Iop_16Sto32, getIRegRAX(2)) );
         DIP("cwtl\n");
         return delta;
      }
      if (sz == 2) {
         putIRegRAX( 2, unop(Iop_8Sto16, getIRegRAX(1)) );
         DIP("cbw\n");
         return delta;
      }
      goto decode_failure;

   case 0x99: 
      if (haveF2orF3(pfx)) goto decode_failure;
      vassert(sz == 2 || sz == 4 || sz == 8);
      ty = szToITy(sz);
      putIRegRDX( sz, 
                  binop(mkSizedOp(ty,Iop_Sar8), 
                        getIRegRAX(sz),
                        mkU8(sz == 2 ? 15 : (sz == 4 ? 31 : 63))) );
      DIP(sz == 2 ? "cwd\n" 
                  : (sz == 4 ?  "cltd\n" 
                             : "cqo\n"));
      return delta;

   case 0x9B: 
      
      DIP("fwait\n");
      return delta;

   case 0x9C:  {
      if (haveF2orF3(pfx)) goto decode_failure;
      vassert(sz == 2 || sz == 4 || sz == 8);
      if (sz == 4) sz = 8;
      if (sz != 8) goto decode_failure; 

      t1 = newTemp(Ity_I64);
      assign( t1, binop(Iop_Sub64,getIReg64(R_RSP),mkU64(sz)) );
      putIReg64(R_RSP, mkexpr(t1) );

      t2 = newTemp(Ity_I64);
      assign( t2, mk_amd64g_calculate_rflags_all() );

      t3 = newTemp(Ity_I64);
      assign( t3, binop(Iop_Or64,
                        mkexpr(t2),
                        binop(Iop_And64,
                              IRExpr_Get(OFFB_DFLAG,Ity_I64),
                              mkU64(1<<10))) 
            );

      
      t4 = newTemp(Ity_I64);
      assign( t4, binop(Iop_Or64,
                        mkexpr(t3),
                        binop(Iop_And64,
                              binop(Iop_Shl64, IRExpr_Get(OFFB_IDFLAG,Ity_I64), 
                                               mkU8(21)),
                              mkU64(1<<21)))
            );

      
      t5 = newTemp(Ity_I64);
      assign( t5, binop(Iop_Or64,
                        mkexpr(t4),
                        binop(Iop_And64,
                              binop(Iop_Shl64, IRExpr_Get(OFFB_ACFLAG,Ity_I64), 
                                               mkU8(18)),
                              mkU64(1<<18)))
            );

      
      if (sz == 2)
        storeLE( mkexpr(t1), unop(Iop_32to16,
                             unop(Iop_64to32,mkexpr(t5))) );
      else 
        storeLE( mkexpr(t1), mkexpr(t5) );

      DIP("pushf%c\n", nameISize(sz));
      return delta;
   }

   case 0x9D: 
      if (haveF2orF3(pfx)) goto decode_failure;
      vassert(sz == 2 || sz == 4);
      if (sz == 4) sz = 8;
      if (sz != 8) goto decode_failure; 
      t1 = newTemp(Ity_I64); t2 = newTemp(Ity_I64);
      assign(t2, getIReg64(R_RSP));
      assign(t1, widenUto64(loadLE(szToITy(sz),mkexpr(t2))));
      putIReg64(R_RSP, binop(Iop_Add64, mkexpr(t2), mkU64(sz)));
      stmt( IRStmt_Put( OFFB_CC_OP,   mkU64(AMD64G_CC_OP_COPY) ));
      stmt( IRStmt_Put( OFFB_CC_DEP2, mkU64(0) ));
      stmt( IRStmt_Put( OFFB_CC_DEP1, 
                        binop(Iop_And64,
                              mkexpr(t1), 
                              mkU64( AMD64G_CC_MASK_C | AMD64G_CC_MASK_P 
                                     | AMD64G_CC_MASK_A | AMD64G_CC_MASK_Z 
                                     | AMD64G_CC_MASK_S| AMD64G_CC_MASK_O )
                             )
                       )
          );

      stmt( IRStmt_Put( 
               OFFB_DFLAG,
               IRExpr_ITE( 
                  unop(Iop_64to1,
                       binop(Iop_And64, 
                             binop(Iop_Shr64, mkexpr(t1), mkU8(10)), 
                             mkU64(1))),
                  mkU64(0xFFFFFFFFFFFFFFFFULL),
                  mkU64(1)))
          );

      
      stmt( IRStmt_Put( 
               OFFB_IDFLAG,
               IRExpr_ITE( 
                  unop(Iop_64to1,
                       binop(Iop_And64, 
                             binop(Iop_Shr64, mkexpr(t1), mkU8(21)), 
                             mkU64(1))),
                  mkU64(1),
                  mkU64(0))) 
          );

      
      stmt( IRStmt_Put( 
               OFFB_ACFLAG,
               IRExpr_ITE( 
                  unop(Iop_64to1,
                       binop(Iop_And64, 
                             binop(Iop_Shr64, mkexpr(t1), mkU8(18)), 
                             mkU64(1))),
                  mkU64(1),
                  mkU64(0))) 
          );

      DIP("popf%c\n", nameISize(sz));
      return delta;

   case 0x9E: 
      codegen_SAHF();
      DIP("sahf\n");
      return delta;

   case 0x9F: 
      codegen_LAHF();
      DIP("lahf\n");
      return delta;

   case 0xA0: 
      if (have66orF2orF3(pfx)) goto decode_failure;
      sz = 1;
      
   case 0xA1: 
      if (sz != 8 && sz != 4 && sz != 2 && sz != 1) 
         goto decode_failure;
      d64 = getDisp64(delta); 
      delta += 8;
      ty = szToITy(sz);
      addr = newTemp(Ity_I64);
      assign( addr, handleAddrOverrides(vbi, pfx, mkU64(d64)) );
      putIRegRAX(sz, loadLE( ty, mkexpr(addr) ));
      DIP("mov%c %s0x%llx, %s\n", nameISize(sz), 
                                  segRegTxt(pfx), d64,
                                  nameIRegRAX(sz));
      return delta;

   case 0xA2: 
      if (have66orF2orF3(pfx)) goto decode_failure;
      sz = 1;
      
   case 0xA3: 
      if (sz != 8 && sz != 4 && sz != 2 && sz != 1) 
         goto decode_failure;
      d64 = getDisp64(delta); 
      delta += 8;
      ty = szToITy(sz);
      addr = newTemp(Ity_I64);
      assign( addr, handleAddrOverrides(vbi, pfx, mkU64(d64)) );
      storeLE( mkexpr(addr), getIRegRAX(sz) );
      DIP("mov%c %s, %s0x%llx\n", nameISize(sz), nameIRegRAX(sz),
                                  segRegTxt(pfx), d64);
      return delta;

   case 0xA4:
   case 0xA5:
      
      if (haveF3(pfx) && !haveF2(pfx)) {
         if (opc == 0xA4)
            sz = 1;
         dis_REP_op ( dres, AMD64CondAlways, dis_MOVS, sz,
                      guest_RIP_curr_instr,
                      guest_RIP_bbstart+delta, "rep movs", pfx );
        dres->whatNext = Dis_StopHere;
        return delta;
      }
      
      if (!haveF3(pfx) && !haveF2(pfx)) {
         if (opc == 0xA4)
            sz = 1;
         dis_string_op( dis_MOVS, sz, "movs", pfx );
         return delta;
      }
      goto decode_failure;

   case 0xA6:
   case 0xA7:
      
      if (haveF3(pfx) && !haveF2(pfx)) {
         if (opc == 0xA6)
            sz = 1;
         dis_REP_op ( dres, AMD64CondZ, dis_CMPS, sz, 
                      guest_RIP_curr_instr,
                      guest_RIP_bbstart+delta, "repe cmps", pfx );
         dres->whatNext = Dis_StopHere;
         return delta;
      }
      goto decode_failure;

   case 0xAA:
   case 0xAB:
      
      if (haveF3(pfx) && !haveF2(pfx)) {
         if (opc == 0xAA)
            sz = 1;
         dis_REP_op ( dres, AMD64CondAlways, dis_STOS, sz,
                      guest_RIP_curr_instr,
                      guest_RIP_bbstart+delta, "rep stos", pfx );
         vassert(dres->whatNext == Dis_StopHere);
         return delta;
      }
      
      if (!haveF3(pfx) && !haveF2(pfx)) {
         if (opc == 0xAA)
            sz = 1;
         dis_string_op( dis_STOS, sz, "stos", pfx );
         return delta;
      }
      goto decode_failure;

   case 0xA8: 
      if (haveF2orF3(pfx)) goto decode_failure;
      delta = dis_op_imm_A( 1, False, Iop_And8, False, delta, "test" );
      return delta;
   case 0xA9: 
      if (haveF2orF3(pfx)) goto decode_failure;
      delta = dis_op_imm_A( sz, False, Iop_And8, False, delta, "test" );
      return delta;

   case 0xAC: 
   case 0xAD:
      dis_string_op( dis_LODS, ( opc == 0xAC ? 1 : sz ), "lods", pfx );
      return delta;

   case 0xAE:
   case 0xAF:
      
      if (haveF2(pfx) && !haveF3(pfx)) {
         if (opc == 0xAE)
            sz = 1;
         dis_REP_op ( dres, AMD64CondNZ, dis_SCAS, sz, 
                      guest_RIP_curr_instr,
                      guest_RIP_bbstart+delta, "repne scas", pfx );
         vassert(dres->whatNext == Dis_StopHere);
         return delta;
      }
      
      if (!haveF2(pfx) && haveF3(pfx)) {
         if (opc == 0xAE)
            sz = 1;
         dis_REP_op ( dres, AMD64CondZ, dis_SCAS, sz, 
                      guest_RIP_curr_instr,
                      guest_RIP_bbstart+delta, "repe scas", pfx );
         vassert(dres->whatNext == Dis_StopHere);
         return delta;
      }
      
      if (!haveF2(pfx) && !haveF3(pfx)) {
         if (opc == 0xAE)
            sz = 1;
         dis_string_op( dis_SCAS, sz, "scas", pfx );
         return delta;
      }
      goto decode_failure;

   
   case 0xB0: 
   case 0xB1: 
   case 0xB2: 
   case 0xB3: 
   case 0xB4: 
   case 0xB5: 
   case 0xB6: 
   case 0xB7: 
      if (haveF2orF3(pfx)) goto decode_failure;
      d64 = getUChar(delta); 
      delta += 1;
      putIRegRexB(1, pfx, opc-0xB0, mkU8(d64));
      DIP("movb $%lld,%s\n", d64, nameIRegRexB(1,pfx,opc-0xB0));
      return delta;

   case 0xB8: 
   case 0xB9: 
   case 0xBA: 
   case 0xBB: 
   case 0xBC: 
   case 0xBD: 
   case 0xBE: 
   case 0xBF: 
      if (haveF2orF3(pfx)) goto decode_failure;
      if (sz == 8) {
         d64 = getDisp64(delta);
         delta += 8;
         putIRegRexB(8, pfx, opc-0xB8, mkU64(d64));
         DIP("movabsq $%lld,%s\n", (Long)d64, 
                                   nameIRegRexB(8,pfx,opc-0xB8));
      } else {
         d64 = getSDisp(imin(4,sz),delta);
         delta += imin(4,sz);
         putIRegRexB(sz, pfx, opc-0xB8, 
                         mkU(szToITy(sz), d64 & mkSizeMask(sz)));
         DIP("mov%c $%lld,%s\n", nameISize(sz), 
                                 (Long)d64, 
                                 nameIRegRexB(sz,pfx,opc-0xB8));
      }
      return delta;

   case 0xC0: { 
      Bool decode_OK = True;
      if (haveF2orF3(pfx)) goto decode_failure;
      modrm = getUChar(delta);
      am_sz = lengthAMode(pfx,delta);
      d_sz  = 1;
      d64   = getUChar(delta + am_sz);
      sz    = 1;
      delta = dis_Grp2 ( vbi, pfx, delta, modrm, am_sz, d_sz, sz, 
                         mkU8(d64 & 0xFF), NULL, &decode_OK );
      if (!decode_OK) goto decode_failure;
      return delta;
   }

   case 0xC1: { 
      Bool decode_OK = True;
      if (haveF2orF3(pfx)) goto decode_failure;
      modrm = getUChar(delta);
      am_sz = lengthAMode(pfx,delta);
      d_sz  = 1;
      d64   = getUChar(delta + am_sz);
      delta = dis_Grp2 ( vbi, pfx, delta, modrm, am_sz, d_sz, sz, 
                         mkU8(d64 & 0xFF), NULL, &decode_OK );
      if (!decode_OK) goto decode_failure;
      return delta;
   }

   case 0xC2: 
      if (have66orF3(pfx)) goto decode_failure;
      if (haveF2(pfx)) DIP("bnd ; "); 
      d64 = getUDisp16(delta); 
      delta += 2;
      dis_ret(dres, vbi, d64);
      DIP("ret $%lld\n", d64);
      return delta;

   case 0xC3: 
      if (have66(pfx)) goto decode_failure;
      
      if (haveF2(pfx)) DIP("bnd ; "); 
      dis_ret(dres, vbi, 0);
      DIP(haveF3(pfx) ? "rep ; ret\n" : "ret\n");
      return delta;

   case 0xC6: 
      sz = 1;
      goto maybe_do_Mov_I_E;
   case 0xC7: 
      goto maybe_do_Mov_I_E;
   maybe_do_Mov_I_E:
      modrm = getUChar(delta);
      if (gregLO3ofRM(modrm) == 0) {
         if (epartIsReg(modrm)) {
            
            if (haveF2orF3(pfx)) goto decode_failure;
            delta++; 
            d64 = getSDisp(imin(4,sz),delta); 
            delta += imin(4,sz);
            putIRegE(sz, pfx, modrm, 
                         mkU(szToITy(sz), d64 & mkSizeMask(sz)));
            DIP("mov%c $%lld, %s\n", nameISize(sz), 
                                     (Long)d64, 
                                     nameIRegE(sz,pfx,modrm));
         } else {
            if (haveF2(pfx)) goto decode_failure;
            
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 
                              imin(4,sz) );
            delta += alen;
            d64 = getSDisp(imin(4,sz),delta);
            delta += imin(4,sz);
            storeLE(mkexpr(addr), 
                    mkU(szToITy(sz), d64 & mkSizeMask(sz)));
            DIP("mov%c $%lld, %s\n", nameISize(sz), (Long)d64, dis_buf);
         }
         return delta;
      }
      
      if (opc == 0xC7 && modrm == 0xF8 && !have66orF2orF3(pfx) && sz == 4
          && (archinfo->hwcaps & VEX_HWCAPS_AMD64_AVX)) {
         delta++; 
         d64 = getSDisp(4,delta); 
         delta += 4;
         guest_RIP_next_mustcheck = True;
         guest_RIP_next_assumed   = guest_RIP_bbstart + delta;
         Addr64 failAddr = guest_RIP_bbstart + delta + d64;
         putIRegRAX(4, mkU32(1<<3));
         
         jmp_lit(dres, Ijk_Boring, failAddr);
         vassert(dres->whatNext == Dis_StopHere);
         DIP("xbeginq 0x%llx\n", failAddr);
         return delta;
      }
      
      
      if (opc == 0xC6 && modrm == 0xF8 && !have66orF2orF3(pfx) && sz == 1
          && (archinfo->hwcaps & VEX_HWCAPS_AMD64_AVX)) {
         delta++; 
         abyte = getUChar(delta); delta++;
         
         DIP("xabort $%d", (Int)abyte);
         return delta;
      }
      
      goto decode_failure;

   case 0xC8: 
      if (sz != 4)
         goto decode_failure;
      d64 = getUDisp16(delta);
      delta += 2;
      vassert(d64 >= 0 && d64 <= 0xFFFF);
      if (getUChar(delta) != 0)
         goto decode_failure;
      delta++;
      t1 = newTemp(Ity_I64);
      assign(t1, getIReg64(R_RBP));
      t2 = newTemp(Ity_I64);
      assign(t2, binop(Iop_Sub64, getIReg64(R_RSP), mkU64(8)));
      putIReg64(R_RSP, mkexpr(t2));
      storeLE(mkexpr(t2), mkexpr(t1));
      putIReg64(R_RBP, mkexpr(t2));
      if (d64 > 0) {
         putIReg64(R_RSP, binop(Iop_Sub64, mkexpr(t2), mkU64(d64)));
      }
      DIP("enter $%u, $0\n", (UInt)d64);
      return delta;

   case 0xC9: 
      if (sz != 4) 
         goto decode_failure;
      t1 = newTemp(Ity_I64); 
      t2 = newTemp(Ity_I64);
      assign(t1, getIReg64(R_RBP));
      putIReg64(R_RSP, mkexpr(t1));
      assign(t2, loadLE(Ity_I64,mkexpr(t1)));
      putIReg64(R_RBP, mkexpr(t2));
      putIReg64(R_RSP, binop(Iop_Add64, mkexpr(t1), mkU64(8)) );
      DIP("leave\n");
      return delta;

   case 0xCC: 
      jmp_lit(dres, Ijk_SigTRAP, guest_RIP_bbstart + delta);
      vassert(dres->whatNext == Dis_StopHere);
      DIP("int $0x3\n");
      return delta;

   case 0xD0: { 
      Bool decode_OK = True;
      if (haveF2orF3(pfx)) goto decode_failure;
      modrm = getUChar(delta);
      am_sz = lengthAMode(pfx,delta);
      d_sz  = 0;
      d64   = 1;
      sz    = 1;
      delta = dis_Grp2 ( vbi, pfx, delta, modrm, am_sz, d_sz, sz, 
                         mkU8(d64), NULL, &decode_OK );
      if (!decode_OK) goto decode_failure;
      return delta;
   }

   case 0xD1: { 
      Bool decode_OK = True;
      if (haveF2orF3(pfx)) goto decode_failure;
      modrm = getUChar(delta);
      am_sz = lengthAMode(pfx,delta);
      d_sz  = 0;
      d64   = 1;
      delta = dis_Grp2 ( vbi, pfx, delta, modrm, am_sz, d_sz, sz, 
                         mkU8(d64), NULL, &decode_OK );
      if (!decode_OK) goto decode_failure;
      return delta;
   }

   case 0xD2: { 
      Bool decode_OK = True;
      if (haveF2orF3(pfx)) goto decode_failure;
      modrm = getUChar(delta);
      am_sz = lengthAMode(pfx,delta);
      d_sz  = 0;
      sz    = 1;
      delta = dis_Grp2 ( vbi, pfx, delta, modrm, am_sz, d_sz, sz, 
                         getIRegCL(), "%cl", &decode_OK );
      if (!decode_OK) goto decode_failure;
      return delta;
   }

   case 0xD3: { 
      Bool decode_OK = True;
      if (haveF2orF3(pfx)) goto decode_failure;
      modrm = getUChar(delta);
      am_sz = lengthAMode(pfx,delta);
      d_sz  = 0;
      delta = dis_Grp2 ( vbi, pfx, delta, modrm, am_sz, d_sz, sz, 
                         getIRegCL(), "%cl", &decode_OK );
      if (!decode_OK) goto decode_failure;
      return delta;
   }

   case 0xD8: 
   case 0xD9:
   case 0xDA:
   case 0xDB:
   case 0xDC:
   case 0xDD:
   case 0xDE:
   case 0xDF: {
      Bool redundantREXWok = False;

      if (haveF2orF3(pfx)) 
         goto decode_failure;

      
      if ( (opc == 0xD9 && getUChar(delta+0) == 0xFA) )
         redundantREXWok = True;

      Bool size_OK = False;
      if ( sz == 4 )
         size_OK = True;
      else if ( sz == 8 )
         size_OK = redundantREXWok;
      else if ( sz == 2 ) {
         int mod_rm = getUChar(delta+0);
         int reg = gregLO3ofRM(mod_rm);
         
         if ( (opc == 0xDD) && (reg == 0  ||
                                reg == 4  ||
                                reg == 6  ) )
            size_OK = True;
      }
      if (!size_OK)
         goto decode_failure;

      Bool decode_OK = False;
      delta = dis_FPU ( &decode_OK, vbi, pfx, delta );
      if (!decode_OK)
         goto decode_failure;

      return delta;
   }

   case 0xE0: 
   case 0xE1: 
   case 0xE2: 
    { 
      IRExpr* zbit  = NULL;
      IRExpr* count = NULL;
      IRExpr* cond  = NULL;
      const HChar* xtra = NULL;

      if (have66orF2orF3(pfx) || 1==getRexW(pfx)) goto decode_failure;
      d64 = guest_RIP_bbstart+delta+1 + getSDisp8(delta);
      delta++;
      if (haveASO(pfx)) {
         putIReg32(R_RCX, binop(Iop_Sub32,
                                unop(Iop_64to32, getIReg64(R_RCX)), 
                                mkU32(1)));
      } else {
         putIReg64(R_RCX, binop(Iop_Sub64, getIReg64(R_RCX), mkU64(1)));
      }

      count = getIReg64(R_RCX);
      cond = binop(Iop_CmpNE64, count, mkU64(0));
      switch (opc) {
         case 0xE2: 
            xtra = ""; 
            break;
         case 0xE1: 
            xtra = "e"; 
            zbit = mk_amd64g_calculate_condition( AMD64CondZ );
            cond = mkAnd1(cond, zbit);
            break;
         case 0xE0: 
            xtra = "ne";
            zbit = mk_amd64g_calculate_condition( AMD64CondNZ );
            cond = mkAnd1(cond, zbit);
            break;
         default:
            vassert(0);
      }
      stmt( IRStmt_Exit(cond, Ijk_Boring, IRConst_U64(d64), OFFB_RIP) );

      DIP("loop%s%s 0x%llx\n", xtra, haveASO(pfx) ? "l" : "", d64);
      return delta;
    }

   case 0xE3: 
      
      if (have66orF2orF3(pfx)) goto decode_failure;
      d64 = (guest_RIP_bbstart+delta+1) + getSDisp8(delta); 
      delta++;
      if (haveASO(pfx)) {
         
         stmt( IRStmt_Exit( binop(Iop_CmpEQ64, 
                                  unop(Iop_32Uto64, getIReg32(R_RCX)), 
                                  mkU64(0)),
                            Ijk_Boring,
                            IRConst_U64(d64),
                            OFFB_RIP
             ));
         DIP("jecxz 0x%llx\n", d64);
      } else {
         
         stmt( IRStmt_Exit( binop(Iop_CmpEQ64, 
                                  getIReg64(R_RCX), 
                                  mkU64(0)),
                            Ijk_Boring,
                            IRConst_U64(d64),
                            OFFB_RIP
               ));
         DIP("jrcxz 0x%llx\n", d64);
      }
      return delta;

   case 0xE4: 
      sz = 1; 
      t1 = newTemp(Ity_I64);
      abyte = getUChar(delta); delta++;
      assign(t1, mkU64( abyte & 0xFF ));
      DIP("in%c $%d,%s\n", nameISize(sz), (Int)abyte, nameIRegRAX(sz));
      goto do_IN;
   case 0xE5: 
      if (!(sz == 2 || sz == 4)) goto decode_failure;
      t1 = newTemp(Ity_I64);
      abyte = getUChar(delta); delta++;
      assign(t1, mkU64( abyte & 0xFF ));
      DIP("in%c $%d,%s\n", nameISize(sz), (Int)abyte, nameIRegRAX(sz));
      goto do_IN;
   case 0xEC: 
      sz = 1; 
      t1 = newTemp(Ity_I64);
      assign(t1, unop(Iop_16Uto64, getIRegRDX(2)));
      DIP("in%c %s,%s\n", nameISize(sz), nameIRegRDX(2), 
                                         nameIRegRAX(sz));
      goto do_IN;
   case 0xED: 
      if (!(sz == 2 || sz == 4)) goto decode_failure;
      t1 = newTemp(Ity_I64);
      assign(t1, unop(Iop_16Uto64, getIRegRDX(2)));
      DIP("in%c %s,%s\n", nameISize(sz), nameIRegRDX(2), 
                                         nameIRegRAX(sz));
      goto do_IN;
   do_IN: {
      IRDirty* d;
      if (haveF2orF3(pfx)) goto decode_failure;
      vassert(sz == 1 || sz == 2 || sz == 4);
      ty = szToITy(sz);
      t2 = newTemp(Ity_I64);
      d = unsafeIRDirty_1_N( 
             t2,
             0, 
             "amd64g_dirtyhelper_IN", 
             &amd64g_dirtyhelper_IN,
             mkIRExprVec_2( mkexpr(t1), mkU64(sz) )
          );
      
      stmt( IRStmt_Dirty(d) );
      putIRegRAX(sz, narrowTo( ty, mkexpr(t2) ) );
      return delta;
   }

   case 0xE6: 
      sz = 1;
      t1 = newTemp(Ity_I64);
      abyte = getUChar(delta); delta++;
      assign( t1, mkU64( abyte & 0xFF ) );
      DIP("out%c %s,$%d\n", nameISize(sz), nameIRegRAX(sz), (Int)abyte);
      goto do_OUT;
   case 0xE7: 
      if (!(sz == 2 || sz == 4)) goto decode_failure;
      t1 = newTemp(Ity_I64);
      abyte = getUChar(delta); delta++;
      assign( t1, mkU64( abyte & 0xFF ) );
      DIP("out%c %s,$%d\n", nameISize(sz), nameIRegRAX(sz), (Int)abyte);
      goto do_OUT;
   case 0xEE: 
      sz = 1;
      t1 = newTemp(Ity_I64);
      assign( t1, unop(Iop_16Uto64, getIRegRDX(2)) );
      DIP("out%c %s,%s\n", nameISize(sz), nameIRegRAX(sz),
                                          nameIRegRDX(2));
      goto do_OUT;
   case 0xEF: 
      if (!(sz == 2 || sz == 4)) goto decode_failure;
      t1 = newTemp(Ity_I64);
      assign( t1, unop(Iop_16Uto64, getIRegRDX(2)) );
      DIP("out%c %s,%s\n", nameISize(sz), nameIRegRAX(sz),
                                          nameIRegRDX(2));
      goto do_OUT;
   do_OUT: {
      IRDirty* d;
      if (haveF2orF3(pfx)) goto decode_failure;
      vassert(sz == 1 || sz == 2 || sz == 4);
      ty = szToITy(sz);
      d = unsafeIRDirty_0_N( 
             0, 
             "amd64g_dirtyhelper_OUT", 
             &amd64g_dirtyhelper_OUT,
             mkIRExprVec_3( mkexpr(t1),
                            widenUto64( getIRegRAX(sz) ), 
                            mkU64(sz) )
          );
      stmt( IRStmt_Dirty(d) );
      return delta;
   }

   case 0xE8: 
      if (haveF3(pfx)) goto decode_failure;
      if (haveF2(pfx)) DIP("bnd ; "); 
      d64 = getSDisp32(delta); delta += 4;
      d64 += (guest_RIP_bbstart+delta); 
      
      t1 = newTemp(Ity_I64); 
      assign(t1, binop(Iop_Sub64, getIReg64(R_RSP), mkU64(8)));
      putIReg64(R_RSP, mkexpr(t1));
      storeLE( mkexpr(t1), mkU64(guest_RIP_bbstart+delta));
      t2 = newTemp(Ity_I64);
      assign(t2, mkU64((Addr64)d64));
      make_redzone_AbiHint(vbi, t1, t2, "call-d32");
      if (resteerOkFn( callback_opaque, (Addr64)d64) ) {
         
         dres->whatNext   = Dis_ResteerU;
         dres->continueAt = d64;
      } else {
         jmp_lit(dres, Ijk_Call, d64);
         vassert(dres->whatNext == Dis_StopHere);
      }
      DIP("call 0x%llx\n",d64);
      return delta;

   case 0xE9: 
      if (haveF3(pfx)) goto decode_failure;
      if (sz != 4) 
         goto decode_failure; 
      if (haveF2(pfx)) DIP("bnd ; "); 
      d64 = (guest_RIP_bbstart+delta+sz) + getSDisp(sz,delta); 
      delta += sz;
      if (resteerOkFn(callback_opaque, (Addr64)d64)) {
         dres->whatNext   = Dis_ResteerU;
         dres->continueAt = d64;
      } else {
         jmp_lit(dres, Ijk_Boring, d64);
         vassert(dres->whatNext == Dis_StopHere);
      }
      DIP("jmp 0x%llx\n", d64);
      return delta;

   case 0xEB: 
      if (haveF3(pfx)) goto decode_failure;
      if (sz != 4) 
         goto decode_failure; 
      if (haveF2(pfx)) DIP("bnd ; "); 
      d64 = (guest_RIP_bbstart+delta+1) + getSDisp8(delta); 
      delta++;
      if (resteerOkFn(callback_opaque, (Addr64)d64)) {
         dres->whatNext   = Dis_ResteerU;
         dres->continueAt = d64;
      } else {
         jmp_lit(dres, Ijk_Boring, d64);
         vassert(dres->whatNext == Dis_StopHere);
      }
      DIP("jmp-8 0x%llx\n", d64);
      return delta;

   case 0xF5: 
   case 0xF8: 
   case 0xF9: 
      t1 = newTemp(Ity_I64);
      t2 = newTemp(Ity_I64);
      assign( t1, mk_amd64g_calculate_rflags_all() );
      switch (opc) {
         case 0xF5: 
            assign( t2, binop(Iop_Xor64, mkexpr(t1), 
                                         mkU64(AMD64G_CC_MASK_C)));
            DIP("cmc\n");
            break;
         case 0xF8: 
            assign( t2, binop(Iop_And64, mkexpr(t1), 
                                         mkU64(~AMD64G_CC_MASK_C)));
            DIP("clc\n");
            break;
         case 0xF9: 
            assign( t2, binop(Iop_Or64, mkexpr(t1), 
                                        mkU64(AMD64G_CC_MASK_C)));
            DIP("stc\n");
            break;
         default: 
            vpanic("disInstr(x64)(cmc/clc/stc)");
      }
      stmt( IRStmt_Put( OFFB_CC_OP,   mkU64(AMD64G_CC_OP_COPY) ));
      stmt( IRStmt_Put( OFFB_CC_DEP2, mkU64(0) ));
      stmt( IRStmt_Put( OFFB_CC_DEP1, mkexpr(t2) ));
      stmt( IRStmt_Put( OFFB_CC_NDEP, mkU64(0) ));
      return delta;

   case 0xF6: { 
      Bool decode_OK = True;
      
      
      delta = dis_Grp3 ( vbi, pfx, 1, delta, &decode_OK );
      if (!decode_OK) goto decode_failure;
      return delta;
   }

   case 0xF7: { 
      Bool decode_OK = True;
      
      
      delta = dis_Grp3 ( vbi, pfx, sz, delta, &decode_OK );
      if (!decode_OK) goto decode_failure;
      return delta;
   }

   case 0xFC: 
      if (haveF2orF3(pfx)) goto decode_failure;
      stmt( IRStmt_Put( OFFB_DFLAG, mkU64(1)) );
      DIP("cld\n");
      return delta;

   case 0xFD: 
      if (haveF2orF3(pfx)) goto decode_failure;
      stmt( IRStmt_Put( OFFB_DFLAG, mkU64(-1ULL)) );
      DIP("std\n");
      return delta;

   case 0xFE: { 
      Bool decode_OK = True;
      
      
      delta = dis_Grp4 ( vbi, pfx, delta, &decode_OK );
      if (!decode_OK) goto decode_failure;
      return delta;
   }

   case 0xFF: { 
      Bool decode_OK = True;
      
      
      delta = dis_Grp5 ( vbi, pfx, sz, delta, dres, &decode_OK );
      if (!decode_OK) goto decode_failure;
      return delta;
   }

   default:
      break;

   }

  decode_failure:
   return deltaIN; 
}



static IRTemp math_BSWAP ( IRTemp t1, IRType ty )
{
   IRTemp t2 = newTemp(ty);
   if (ty == Ity_I64) {
      IRTemp m8  = newTemp(Ity_I64);
      IRTemp s8  = newTemp(Ity_I64);
      IRTemp m16 = newTemp(Ity_I64);
      IRTemp s16 = newTemp(Ity_I64);
      IRTemp m32 = newTemp(Ity_I64);
      assign( m8, mkU64(0xFF00FF00FF00FF00ULL) );
      assign( s8,
              binop(Iop_Or64,
                    binop(Iop_Shr64,
                          binop(Iop_And64,mkexpr(t1),mkexpr(m8)),
                          mkU8(8)),
                    binop(Iop_And64,
                          binop(Iop_Shl64,mkexpr(t1),mkU8(8)),
                          mkexpr(m8))
                   ) 
            );

      assign( m16, mkU64(0xFFFF0000FFFF0000ULL) );
      assign( s16,
              binop(Iop_Or64,
                    binop(Iop_Shr64,
                          binop(Iop_And64,mkexpr(s8),mkexpr(m16)),
                          mkU8(16)),
                    binop(Iop_And64,
                          binop(Iop_Shl64,mkexpr(s8),mkU8(16)),
                          mkexpr(m16))
                   ) 
            );

      assign( m32, mkU64(0xFFFFFFFF00000000ULL) );
      assign( t2,
              binop(Iop_Or64,
                    binop(Iop_Shr64,
                          binop(Iop_And64,mkexpr(s16),mkexpr(m32)),
                          mkU8(32)),
                    binop(Iop_And64,
                          binop(Iop_Shl64,mkexpr(s16),mkU8(32)),
                          mkexpr(m32))
                   ) 
            );
      return t2;
   }
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


__attribute__((noinline))
static
Long dis_ESC_0F (
        DisResult* dres,
        Bool*      expect_CAS,
        Bool         (*resteerOkFn) ( void*, Addr ),
        Bool         resteerCisOk,
        void*        callback_opaque,
        const VexArchInfo* archinfo,
        const VexAbiInfo*  vbi,
        Prefix pfx, Int sz, Long deltaIN 
     )
{
   Long   d64   = 0;
   IRTemp addr  = IRTemp_INVALID;
   IRTemp t1    = IRTemp_INVALID;
   IRTemp t2    = IRTemp_INVALID;
   UChar  modrm = 0;
   Int    am_sz = 0;
   Int    alen  = 0;
   HChar  dis_buf[50];

   
   Long   delta = deltaIN;
   UChar  opc   = getUChar(delta);
   delta++;
   switch (opc) { 

   case 0x01:
   {
      modrm = getUChar(delta);
      
      
      if (!epartIsReg(modrm)
          && (gregLO3ofRM(modrm) == 0 || gregLO3ofRM(modrm) == 1)) {
         addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
         delta += alen;
         switch (gregLO3ofRM(modrm)) {
            case 0: DIP("sgdt %s\n", dis_buf); break;
            case 1: DIP("sidt %s\n", dis_buf); break;
            default: vassert(0); 
         }
         IRDirty* d = unsafeIRDirty_0_N (
                          0,
                          "amd64g_dirtyhelper_SxDT",
                          &amd64g_dirtyhelper_SxDT,
                          mkIRExprVec_2( mkexpr(addr),
                                         mkU64(gregLO3ofRM(modrm)) )
                      );
         
         d->mFx   = Ifx_Write;
         d->mAddr = mkexpr(addr);
         d->mSize = 6;
         stmt( IRStmt_Dirty(d) );
         return delta;
      }
      
      if (modrm == 0xD0 && (archinfo->hwcaps & VEX_HWCAPS_AMD64_AVX)) {
         delta += 1;
         DIP("xgetbv\n");
         t1 = newTemp(Ity_I32);
         assign( t1, getIReg32(R_RCX) );
         stmt( IRStmt_Exit(binop(Iop_CmpNE32, mkexpr(t1), mkU32(0)),
                           Ijk_SigSEGV,
                           IRConst_U64(guest_RIP_curr_instr),
                           OFFB_RIP
         ));
         putIRegRAX(4, mkU32(7));
         putIRegRDX(4, mkU32(0));
         return delta;
      }
      
      
      if (modrm == 0xD5 && (archinfo->hwcaps & VEX_HWCAPS_AMD64_AVX)) {
         delta += 1;
         jmp_lit(dres, Ijk_SigSEGV, guest_RIP_bbstart + delta);
         vassert(dres->whatNext == Dis_StopHere);
         DIP("xend\n");
         return delta;
      }
      
      
      
      if (modrm == 0xD6 && (archinfo->hwcaps & VEX_HWCAPS_AMD64_AVX)) {
         delta += 1;
         DIP("xtest\n");
         stmt( IRStmt_Put( OFFB_CC_OP,   mkU64(AMD64G_CC_OP_COPY) ));
         stmt( IRStmt_Put( OFFB_CC_DEP2, mkU64(0) ));
         stmt( IRStmt_Put( OFFB_CC_DEP1, mkU64(AMD64G_CC_MASK_Z) ));
         stmt( IRStmt_Put( OFFB_CC_NDEP, mkU64(0) ));
         return delta;
      }
      
      
      if (modrm == 0xF9 && (archinfo->hwcaps & VEX_HWCAPS_AMD64_RDTSCP)) {
         delta += 1;
         const HChar* fName = "amd64g_dirtyhelper_RDTSCP";
         void*        fAddr = &amd64g_dirtyhelper_RDTSCP;
         IRDirty* d
            = unsafeIRDirty_0_N ( 0, 
                                  fName, fAddr, mkIRExprVec_1(IRExpr_BBPTR()) );
         
         d->nFxState = 3;
         vex_bzero(&d->fxState, sizeof(d->fxState));
         d->fxState[0].fx     = Ifx_Write;
         d->fxState[0].offset = OFFB_RAX;
         d->fxState[0].size   = 8;
         d->fxState[1].fx     = Ifx_Write;
         d->fxState[1].offset = OFFB_RCX;
         d->fxState[1].size   = 8;
         d->fxState[2].fx     = Ifx_Write;
         d->fxState[2].offset = OFFB_RDX;
         d->fxState[2].size   = 8;
         
         stmt( IRStmt_Dirty(d) );
         stmt( IRStmt_MBE(Imbe_Fence) );
         DIP("rdtscp\n");
         return delta;
      }
      
      break;
   }

   case 0x05: 
      guest_RIP_next_mustcheck = True;
      guest_RIP_next_assumed = guest_RIP_bbstart + delta;
      putIReg64( R_RCX, mkU64(guest_RIP_next_assumed) );
      jmp_lit(dres, Ijk_Sys_syscall, guest_RIP_next_assumed);
      vassert(dres->whatNext == Dis_StopHere);
      DIP("syscall\n");
      return delta;

   case 0x0B: 
      stmt( IRStmt_Put( OFFB_RIP, mkU64(guest_RIP_curr_instr) ) );
      jmp_lit(dres, Ijk_NoDecode, guest_RIP_curr_instr);
      vassert(dres->whatNext == Dis_StopHere);
      DIP("ud2\n");
      return delta;

   case 0x0D: 
              
      if (have66orF2orF3(pfx)) goto decode_failure;
      modrm = getUChar(delta);
      if (epartIsReg(modrm)) goto decode_failure;
      if (gregLO3ofRM(modrm) != 0 && gregLO3ofRM(modrm) != 1)
         goto decode_failure;
      addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
      delta += alen;
      switch (gregLO3ofRM(modrm)) {
         case 0: DIP("prefetch %s\n", dis_buf); break;
         case 1: DIP("prefetchw %s\n", dis_buf); break;
         default: vassert(0); 
      }
      return delta;

   case 0x1F:
      if (haveF2orF3(pfx)) goto decode_failure;
      modrm = getUChar(delta);
      if (epartIsReg(modrm)) goto decode_failure;
      addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
      delta += alen;
      DIP("nop%c %s\n", nameISize(sz), dis_buf);
      return delta;

   case 0x31: { 
      IRTemp   val  = newTemp(Ity_I64);
      IRExpr** args = mkIRExprVec_0();
      IRDirty* d    = unsafeIRDirty_1_N ( 
                         val, 
                         0, 
                         "amd64g_dirtyhelper_RDTSC", 
                         &amd64g_dirtyhelper_RDTSC, 
                         args 
                      );
      if (have66orF2orF3(pfx)) goto decode_failure;
      
      stmt( IRStmt_Dirty(d) );
      putIRegRDX(4, unop(Iop_64HIto32, mkexpr(val)));
      putIRegRAX(4, unop(Iop_64to32, mkexpr(val)));
      DIP("rdtsc\n");
      return delta;
   }

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
      if (haveF2orF3(pfx)) goto decode_failure;
      delta = dis_cmov_E_G(vbi, pfx, sz, (AMD64Condcode)(opc - 0x40), delta);
      return delta;

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
   case 0x8F: { 
      Long   jmpDelta;
      const HChar* comment  = "";
      if (haveF3(pfx)) goto decode_failure;
      if (haveF2(pfx)) DIP("bnd ; "); 
      jmpDelta = getSDisp32(delta);
      d64 = (guest_RIP_bbstart+delta+4) + jmpDelta;
      delta += 4;
      if (resteerCisOk
          && vex_control.guest_chase_cond
          && (Addr64)d64 != (Addr64)guest_RIP_bbstart
          && jmpDelta < 0
          && resteerOkFn( callback_opaque, (Addr64)d64) ) {
         stmt( IRStmt_Exit( 
                  mk_amd64g_calculate_condition(
                     (AMD64Condcode)(1 ^ (opc - 0x80))),
                  Ijk_Boring,
                  IRConst_U64(guest_RIP_bbstart+delta),
                  OFFB_RIP
             ));
         dres->whatNext   = Dis_ResteerC;
         dres->continueAt = d64;
         comment = "(assumed taken)";
      }
      else
      if (resteerCisOk
          && vex_control.guest_chase_cond
          && (Addr64)d64 != (Addr64)guest_RIP_bbstart
          && jmpDelta >= 0
          && resteerOkFn( callback_opaque, guest_RIP_bbstart+delta ) ) {
         stmt( IRStmt_Exit( 
                  mk_amd64g_calculate_condition((AMD64Condcode)
                                                (opc - 0x80)),
                  Ijk_Boring,
                  IRConst_U64(d64),
                  OFFB_RIP
             ));
         dres->whatNext   = Dis_ResteerC;
         dres->continueAt = guest_RIP_bbstart+delta;
         comment = "(assumed not taken)";
      }
      else {
         jcc_01( dres, (AMD64Condcode)(opc - 0x80),
                 guest_RIP_bbstart+delta, d64 );
         vassert(dres->whatNext == Dis_StopHere);
      }
      DIP("j%s-32 0x%llx %s\n", name_AMD64Condcode(opc - 0x80), d64, comment);
      return delta;
   }

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
      if (haveF2orF3(pfx)) goto decode_failure;
      t1 = newTemp(Ity_I8);
      assign( t1, unop(Iop_1Uto8,mk_amd64g_calculate_condition(opc-0x90)) );
      modrm = getUChar(delta);
      if (epartIsReg(modrm)) {
         delta++;
         putIRegE(1, pfx, modrm, mkexpr(t1));
         DIP("set%s %s\n", name_AMD64Condcode(opc-0x90), 
                           nameIRegE(1,pfx,modrm));
      } else {
         addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
         delta += alen;
         storeLE( mkexpr(addr), mkexpr(t1) );
         DIP("set%s %s\n", name_AMD64Condcode(opc-0x90), dis_buf);
      }
      return delta;

   case 0x1A:
   case 0x1B: { 


      modrm = getUChar(delta);
      int bnd = gregOfRexRM(pfx,modrm);
      const HChar *oper;
      if (epartIsReg(modrm)) {
         oper = nameIReg64 (eregOfRexRM(pfx,modrm));
         delta += 1;
      } else {
         addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
         delta += alen;
         oper = dis_buf;
      }

      if (haveF3no66noF2 (pfx)) {
         if (opc == 0x1B) {
            DIP ("bndmk %s, %%bnd%d\n", oper, bnd);
         } else  {
            DIP ("bndcl %s, %%bnd%d\n", oper, bnd);
         }
      } else if (haveF2no66noF3 (pfx)) {
         if (opc == 0x1A) {
            DIP ("bndcu %s, %%bnd%d\n", oper, bnd);
         } else  {
            DIP ("bndcn %s, %%bnd%d\n", oper, bnd);
         }
      } else if (have66noF2noF3 (pfx)) {
         if (opc == 0x1A) {
            DIP ("bndmov %s, %%bnd%d\n", oper, bnd);
         } else  {
            DIP ("bndmov %%bnd%d, %s\n", bnd, oper);
         }
      } else if (haveNo66noF2noF3 (pfx)) {
         if (opc == 0x1A) {
            DIP ("bndldx %s, %%bnd%d\n", oper, bnd);
         } else  {
            DIP ("bndstx %%bnd%d, %s\n", bnd, oper);
         }
      } else goto decode_failure;

      return delta;
   }

   case 0xA2: { 
      IRDirty*     d     = NULL;
      const HChar* fName = NULL;
      void*        fAddr = NULL;

      Bool this_is_yosemite = False;
#     if defined(VGP_amd64_darwin) && DARWIN_VERS == DARWIN_10_10
      this_is_yosemite = True;
#     endif

      if (haveF2orF3(pfx)) goto decode_failure;
      if (!this_is_yosemite &&
          (archinfo->hwcaps & VEX_HWCAPS_AMD64_SSE3) &&
          (archinfo->hwcaps & VEX_HWCAPS_AMD64_CX16) &&
          (archinfo->hwcaps & VEX_HWCAPS_AMD64_AVX)) {
         fName = "amd64g_dirtyhelper_CPUID_avx_and_cx16";
         fAddr = &amd64g_dirtyhelper_CPUID_avx_and_cx16;
         
      }
      else if ((archinfo->hwcaps & VEX_HWCAPS_AMD64_SSE3) &&
               (archinfo->hwcaps & VEX_HWCAPS_AMD64_CX16)) {
         fName = "amd64g_dirtyhelper_CPUID_sse42_and_cx16";
         fAddr = &amd64g_dirtyhelper_CPUID_sse42_and_cx16;
         
      }
      else {
         fName = "amd64g_dirtyhelper_CPUID_baseline";
         fAddr = &amd64g_dirtyhelper_CPUID_baseline;
      }

      vassert(fName); vassert(fAddr);
      d = unsafeIRDirty_0_N ( 0, 
                              fName, fAddr, mkIRExprVec_1(IRExpr_BBPTR()) );
      
      d->nFxState = 4;
      vex_bzero(&d->fxState, sizeof(d->fxState));
      d->fxState[0].fx     = Ifx_Modify;
      d->fxState[0].offset = OFFB_RAX;
      d->fxState[0].size   = 8;
      d->fxState[1].fx     = Ifx_Write;
      d->fxState[1].offset = OFFB_RBX;
      d->fxState[1].size   = 8;
      d->fxState[2].fx     = Ifx_Modify;
      d->fxState[2].offset = OFFB_RCX;
      d->fxState[2].size   = 8;
      d->fxState[3].fx     = Ifx_Write;
      d->fxState[3].offset = OFFB_RDX;
      d->fxState[3].size   = 8;
      
      stmt( IRStmt_Dirty(d) );
      stmt( IRStmt_MBE(Imbe_Fence) );
      DIP("cpuid\n");
      return delta;
   }

   case 0xA3: { 
      
      Bool ok = True;
      if (sz != 8 && sz != 4 && sz != 2) goto decode_failure;
      delta = dis_bt_G_E ( vbi, pfx, sz, delta, BtOpNone, &ok );
      if (!ok) goto decode_failure;
      return delta;
   }

   case 0xA4: 
      modrm = getUChar(delta);
      d64   = delta + lengthAMode(pfx, delta);
      vex_sprintf(dis_buf, "$%d", (Int)getUChar(d64));
      delta = dis_SHLRD_Gv_Ev ( 
                 vbi, pfx, delta, modrm, sz, 
                 mkU8(getUChar(d64)), True, 
                 dis_buf, True  );
      return delta;

   case 0xA5: 
      modrm = getUChar(delta);
      delta = dis_SHLRD_Gv_Ev ( 
                 vbi, pfx, delta, modrm, sz,
                 getIRegCL(), False, 
                 "%cl", True  );
      return delta;

   case 0xAB: { 
      
      Bool ok = True;
      if (sz != 8 && sz != 4 && sz != 2) goto decode_failure;
      delta = dis_bt_G_E ( vbi, pfx, sz, delta, BtOpSet, &ok );
      if (!ok) goto decode_failure;
      return delta;
   }

   case 0xAC: 
      modrm = getUChar(delta);
      d64   = delta + lengthAMode(pfx, delta);
      vex_sprintf(dis_buf, "$%d", (Int)getUChar(d64));
      delta = dis_SHLRD_Gv_Ev ( 
                 vbi, pfx, delta, modrm, sz, 
                 mkU8(getUChar(d64)), True, 
                 dis_buf, False  );
      return delta;

   case 0xAD: 
      modrm = getUChar(delta);
      delta = dis_SHLRD_Gv_Ev ( 
                 vbi, pfx, delta, modrm, sz, 
                 getIRegCL(), False, 
                 "%cl", False );
      return delta;

   case 0xAF: 
      if (haveF2orF3(pfx)) goto decode_failure;
      delta = dis_mul_E_G ( vbi, pfx, sz, delta );
      return delta;

   case 0xB0: { 
      Bool ok = True;
      
      delta = dis_cmpxchg_G_E ( &ok, vbi, pfx, 1, delta );
      if (!ok) goto decode_failure;
      return delta;
   }

   case 0xB1: { 
      Bool ok = True;
      
      if (sz != 2 && sz != 4 && sz != 8) goto decode_failure;
      delta = dis_cmpxchg_G_E ( &ok, vbi, pfx, sz, delta );
      if (!ok) goto decode_failure;
      return delta;
   }

   case 0xB3: { 
      
      Bool ok = True;
      if (sz != 8 && sz != 4 && sz != 2) goto decode_failure;
      delta = dis_bt_G_E ( vbi, pfx, sz, delta, BtOpReset, &ok );
      if (!ok) goto decode_failure;
      return delta;
   }

   case 0xB6: 
      if (haveF2orF3(pfx)) goto decode_failure;
      if (sz != 2 && sz != 4 && sz != 8)
         goto decode_failure;
      delta = dis_movx_E_G ( vbi, pfx, delta, 1, sz, False );
      return delta;

   case 0xB7: 
      if (haveF2orF3(pfx)) goto decode_failure;
      if (sz != 4 && sz != 8)
         goto decode_failure;
      delta = dis_movx_E_G ( vbi, pfx, delta, 2, sz, False );
      return delta;

   case 0xBA: { 
      
      Bool decode_OK = False;
      modrm = getUChar(delta);
      am_sz = lengthAMode(pfx,delta);
      d64   = getSDisp8(delta + am_sz);
      delta = dis_Grp8_Imm ( vbi, pfx, delta, modrm, am_sz, sz, d64,
                             &decode_OK );
      if (!decode_OK)
         goto decode_failure;
      return delta;
   }

   case 0xBB: { 
      
      Bool ok = False;
      if (sz != 8 && sz != 4 && sz != 2) goto decode_failure;
      delta = dis_bt_G_E ( vbi, pfx, sz, delta, BtOpComp, &ok );
      if (!ok) goto decode_failure;
      return delta;
   }

   case 0xBC: 
      if (!haveF2orF3(pfx)
          || (haveF3noF2(pfx)
              && 0 == (archinfo->hwcaps & VEX_HWCAPS_AMD64_BMI))) {
         delta = dis_bs_E_G ( vbi, pfx, sz, delta, True );
         return delta;
      }
      break;

   case 0xBD: 
      if (!haveF2orF3(pfx)
          || (haveF3noF2(pfx)
              && 0 == (archinfo->hwcaps & VEX_HWCAPS_AMD64_LZCNT))) {
         delta = dis_bs_E_G ( vbi, pfx, sz, delta, False );
         return delta;
      }
      break;

   case 0xBE: 
      if (haveF2orF3(pfx)) goto decode_failure;
      if (sz != 2 && sz != 4 && sz != 8)
         goto decode_failure;
      delta = dis_movx_E_G ( vbi, pfx, delta, 1, sz, True );
      return delta;

   case 0xBF: 
      if (haveF2orF3(pfx)) goto decode_failure;
      if (sz != 4 && sz != 8)
         goto decode_failure;
      delta = dis_movx_E_G ( vbi, pfx, delta, 2, sz, True );
      return delta;

   case 0xC0: {  
      Bool decode_OK = False;
      delta = dis_xadd_G_E ( &decode_OK, vbi, pfx, 1, delta );
      if (!decode_OK)
         goto decode_failure;
      return delta;
   }

   case 0xC1: {  
      Bool decode_OK = False;
      delta = dis_xadd_G_E ( &decode_OK, vbi, pfx, sz, delta );
      if (!decode_OK)
         goto decode_failure;
      return delta;
   }

   case 0xC7: { 
      IRType  elemTy     = sz==4 ? Ity_I32 : Ity_I64;
      IRTemp  expdHi     = newTemp(elemTy);
      IRTemp  expdLo     = newTemp(elemTy);
      IRTemp  dataHi     = newTemp(elemTy);
      IRTemp  dataLo     = newTemp(elemTy);
      IRTemp  oldHi      = newTemp(elemTy);
      IRTemp  oldLo      = newTemp(elemTy);
      IRTemp  flags_old  = newTemp(Ity_I64);
      IRTemp  flags_new  = newTemp(Ity_I64);
      IRTemp  success    = newTemp(Ity_I1);
      IROp    opOR       = sz==4 ? Iop_Or32    : Iop_Or64;
      IROp    opXOR      = sz==4 ? Iop_Xor32   : Iop_Xor64;
      IROp    opCasCmpEQ = sz==4 ? Iop_CasCmpEQ32 : Iop_CasCmpEQ64;
      IRExpr* zero       = sz==4 ? mkU32(0)    : mkU64(0);
      IRTemp expdHi64    = newTemp(Ity_I64);
      IRTemp expdLo64    = newTemp(Ity_I64);

      *expect_CAS = True;

      
      if (have66(pfx)) goto decode_failure;
      if (sz != 4 && sz != 8) goto decode_failure;
      if (sz == 8 && !(archinfo->hwcaps & VEX_HWCAPS_AMD64_CX16))
         goto decode_failure;
      modrm = getUChar(delta);
      if (epartIsReg(modrm)) goto decode_failure;
      if (gregLO3ofRM(modrm) != 1) goto decode_failure;
      if (haveF2orF3(pfx)) {
         if (sz == 8) goto decode_failure;
         if (haveF2andF3(pfx) || !haveLOCK(pfx)) goto decode_failure;
      }

      addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
      delta += alen;

      
      if (sz == 8)
         gen_SEGV_if_not_16_aligned( addr );

      
      assign( expdHi64, getIReg64(R_RDX) );
      assign( expdLo64, getIReg64(R_RAX) );

      assign( expdHi, sz==4 ? unop(Iop_64to32, mkexpr(expdHi64))
                            : mkexpr(expdHi64) );
      assign( expdLo, sz==4 ? unop(Iop_64to32, mkexpr(expdLo64))
                            : mkexpr(expdLo64) );
      assign( dataHi, sz==4 ? getIReg32(R_RCX) : getIReg64(R_RCX) );
      assign( dataLo, sz==4 ? getIReg32(R_RBX) : getIReg64(R_RBX) );

      
      stmt( IRStmt_CAS(
               mkIRCAS( oldHi, oldLo, 
                        Iend_LE, mkexpr(addr), 
                        mkexpr(expdHi), mkexpr(expdLo),
                        mkexpr(dataHi), mkexpr(dataLo)
            )));

      
      assign( success,
              binop(opCasCmpEQ,
                    binop(opOR,
                          binop(opXOR, mkexpr(oldHi), mkexpr(expdHi)),
                          binop(opXOR, mkexpr(oldLo), mkexpr(expdLo))
                    ),
                    zero
              ));

      
      putIRegRDX( 8,
                  IRExpr_ITE( mkexpr(success),
                              mkexpr(expdHi64),
                              sz == 4 ? unop(Iop_32Uto64, mkexpr(oldHi))
                                      : mkexpr(oldHi)
                ));
      putIRegRAX( 8,
                  IRExpr_ITE( mkexpr(success),
                              mkexpr(expdLo64),
                              sz == 4 ? unop(Iop_32Uto64, mkexpr(oldLo))
                                      : mkexpr(oldLo)
                ));

      assign( flags_old, widenUto64(mk_amd64g_calculate_rflags_all()));
      assign( 
         flags_new,
         binop(Iop_Or64,
               binop(Iop_And64, mkexpr(flags_old), 
                                mkU64(~AMD64G_CC_MASK_Z)),
               binop(Iop_Shl64,
                     binop(Iop_And64,
                           unop(Iop_1Uto64, mkexpr(success)), mkU64(1)), 
                     mkU8(AMD64G_CC_SHIFT_Z)) ));

      stmt( IRStmt_Put( OFFB_CC_OP,   mkU64(AMD64G_CC_OP_COPY) ));
      stmt( IRStmt_Put( OFFB_CC_DEP1, mkexpr(flags_new) ));
      stmt( IRStmt_Put( OFFB_CC_DEP2, mkU64(0) ));
      stmt( IRStmt_Put( OFFB_CC_NDEP, mkU64(0) ));


      DIP("cmpxchg8b %s\n", dis_buf);
      return delta;
   }

   case 0xC8: 
   case 0xC9:
   case 0xCA:
   case 0xCB:
   case 0xCC:
   case 0xCD:
   case 0xCE:
   case 0xCF: 
      if (haveF2orF3(pfx)) goto decode_failure;
      if (sz == 4) {
         t1 = newTemp(Ity_I32);
         assign( t1, getIRegRexB(4, pfx, opc-0xC8) );
         t2 = math_BSWAP( t1, Ity_I32 );
         putIRegRexB(4, pfx, opc-0xC8, mkexpr(t2));
         DIP("bswapl %s\n", nameIRegRexB(4, pfx, opc-0xC8));
         return delta;
      }
      if (sz == 8) {
         t1 = newTemp(Ity_I64);
         t2 = newTemp(Ity_I64);
         assign( t1, getIRegRexB(8, pfx, opc-0xC8) );
         t2 = math_BSWAP( t1, Ity_I64 );
         putIRegRexB(8, pfx, opc-0xC8, mkexpr(t2));
         DIP("bswapq %s\n", nameIRegRexB(8, pfx, opc-0xC8));
         return delta;
      }
      goto decode_failure;

   default:
      break;

   } 


   
   

   if (!have66orF2orF3(pfx)) {
      

      vassert(sz == 4 || sz == 8);

      switch (opc) { 

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
      case 0xE2: { 
         Bool decode_OK = False;
         delta = dis_MMX ( &decode_OK, vbi, pfx, sz, deltaIN );
         if (decode_OK)
            return delta;
         goto decode_failure;
      }

      default:
         break;
      } 

   }

   
   if (opc == 0x0E || opc == 0x77) {
      if (sz != 4)
         goto decode_failure;
      do_EMMS_preamble();
      DIP("{f}emms\n");
      return delta;
   }

   
   {
      Bool decode_OK = False;
      delta = dis_ESC_0F__SSE2 ( &decode_OK, vbi, pfx, sz, deltaIN, dres );
      if (decode_OK)
         return delta;
   }

   
   {
      Bool decode_OK = False;
      delta = dis_ESC_0F__SSE3 ( &decode_OK, vbi, pfx, sz, deltaIN );
      if (decode_OK)
         return delta;
   }

   
   {
      Bool decode_OK = False;
      delta = dis_ESC_0F__SSE4 ( &decode_OK,
                                 archinfo, vbi, pfx, sz, deltaIN );
      if (decode_OK)
         return delta;
   }

  decode_failure:
   return deltaIN; 
}



__attribute__((noinline))
static
Long dis_ESC_0F38 (
        DisResult* dres,
        Bool         (*resteerOkFn) ( void*, Addr ),
        Bool         resteerCisOk,
        void*        callback_opaque,
        const VexArchInfo* archinfo,
        const VexAbiInfo*  vbi,
        Prefix pfx, Int sz, Long deltaIN 
     )
{
   Long   delta = deltaIN;
   UChar  opc   = getUChar(delta);
   delta++;
   switch (opc) {

   case 0xF0:   
   case 0xF1: { 
      if (!haveF2orF3(pfx) && !haveVEX(pfx)
          && (sz == 2 || sz == 4 || sz == 8)) {
         IRTemp addr  = IRTemp_INVALID;
         UChar  modrm = 0;
         Int    alen  = 0;
         HChar  dis_buf[50];
         modrm = getUChar(delta);
         if (epartIsReg(modrm)) break;
         addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
         delta += alen;
         IRType ty = szToITy(sz);
         IRTemp src = newTemp(ty);
         if (opc == 0xF0) { 
            assign(src, loadLE(ty, mkexpr(addr)));
            IRTemp dst = math_BSWAP(src, ty);
            putIRegG(sz, pfx, modrm, mkexpr(dst));
            DIP("movbe %s,%s\n", dis_buf, nameIRegG(sz, pfx, modrm));
         } else { 
            assign(src, getIRegG(sz, pfx, modrm));
            IRTemp dst = math_BSWAP(src, ty);
            storeLE(mkexpr(addr), mkexpr(dst));
            DIP("movbe %s,%s\n", nameIRegG(sz, pfx, modrm), dis_buf);
         }
         return delta;
      }
      break;
   }

   default:
      break;

   }

   
   {
      Bool decode_OK = False;
      delta = dis_ESC_0F38__SupSSE3 ( &decode_OK, vbi, pfx, sz, deltaIN );
      if (decode_OK)
         return delta;
   }

   
   {
      Bool decode_OK = False;
      delta = dis_ESC_0F38__SSE4 ( &decode_OK, vbi, pfx, sz, deltaIN );
      if (decode_OK)
         return delta;
   }

  
   return deltaIN; 
}



__attribute__((noinline))
static
Long dis_ESC_0F3A (
        DisResult* dres,
        Bool         (*resteerOkFn) ( void*, Addr ),
        Bool         resteerCisOk,
        void*        callback_opaque,
        const VexArchInfo* archinfo,
        const VexAbiInfo*  vbi,
        Prefix pfx, Int sz, Long deltaIN 
     )
{
   Long   delta = deltaIN;
   UChar  opc   = getUChar(delta);
   delta++;
   switch (opc) {

   default:
      break;

   }

   
   {
      Bool decode_OK = False;
      delta = dis_ESC_0F3A__SupSSE3 ( &decode_OK, vbi, pfx, sz, deltaIN );
      if (decode_OK)
         return delta;
   }

   
   {
      Bool decode_OK = False;
      delta = dis_ESC_0F3A__SSE4 ( &decode_OK, vbi, pfx, sz, deltaIN );
      if (decode_OK)
         return delta;
   }

   return deltaIN; 
}



static
Long dis_VEX_NDS_128_AnySimdPfx_0F_WIG (
        Bool* uses_vvvv, const VexAbiInfo* vbi,
        Prefix pfx, Long delta, const HChar* name,
        IROp op, IRTemp(*opFn)(IRTemp,IRTemp),
        Bool invertLeftArg,
        Bool swapArgs
     )
{
   UChar  modrm = getUChar(delta);
   UInt   rD    = gregOfRexRM(pfx, modrm);
   UInt   rSL   = getVexNvvvv(pfx);
   IRTemp tSL   = newTemp(Ity_V128);
   IRTemp tSR   = newTemp(Ity_V128);
   IRTemp addr  = IRTemp_INVALID;
   HChar  dis_buf[50];
   Int    alen  = 0;
   vassert(0==getVexL(pfx) && 0==getRexW(pfx));

   assign(tSL, invertLeftArg ? unop(Iop_NotV128, getXMMReg(rSL))
                             : getXMMReg(rSL));

   if (epartIsReg(modrm)) {
      UInt rSR = eregOfRexRM(pfx, modrm);
      delta += 1;
      assign(tSR, getXMMReg(rSR));
      DIP("%s %s,%s,%s\n",
          name, nameXMMReg(rSR), nameXMMReg(rSL), nameXMMReg(rD));
   } else {
      addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 0 );
      delta += alen;
      assign(tSR, loadLE(Ity_V128, mkexpr(addr)));
      DIP("%s %s,%s,%s\n",
          name, dis_buf, nameXMMReg(rSL), nameXMMReg(rD));
   }

   IRTemp res = IRTemp_INVALID;
   if (op != Iop_INVALID) {
      vassert(opFn == NULL);
      res = newTemp(Ity_V128);
      if (requiresRMode(op)) {
         IRTemp rm = newTemp(Ity_I32);
         assign(rm, get_FAKE_roundingmode()); 
         assign(res, swapArgs
                        ? triop(op, mkexpr(rm), mkexpr(tSR), mkexpr(tSL))
                        : triop(op, mkexpr(rm), mkexpr(tSL), mkexpr(tSR)));
      } else {
         assign(res, swapArgs
                        ? binop(op, mkexpr(tSR), mkexpr(tSL))
                        : binop(op, mkexpr(tSL), mkexpr(tSR)));
      }
   } else {
      vassert(opFn != NULL);
      res = swapArgs ? opFn(tSR, tSL) : opFn(tSL, tSR);
   }

   putYMMRegLoAndZU(rD, mkexpr(res));

   *uses_vvvv = True;
   return delta;
}


static
Long dis_VEX_NDS_128_AnySimdPfx_0F_WIG_simple (
        Bool* uses_vvvv, const VexAbiInfo* vbi,
        Prefix pfx, Long delta, const HChar* name,
        IROp op
     )
{
   return dis_VEX_NDS_128_AnySimdPfx_0F_WIG(
             uses_vvvv, vbi, pfx, delta, name, op, NULL, False, False);
}


static
Long dis_VEX_NDS_128_AnySimdPfx_0F_WIG_complex (
        Bool* uses_vvvv, const VexAbiInfo* vbi,
        Prefix pfx, Long delta, const HChar* name,
        IRTemp(*opFn)(IRTemp,IRTemp)
     )
{
   return dis_VEX_NDS_128_AnySimdPfx_0F_WIG(
             uses_vvvv, vbi, pfx, delta, name,
             Iop_INVALID, opFn, False, False );
}


static ULong dis_AVX128_shiftV_byE ( const VexAbiInfo* vbi,
                                     Prefix pfx, Long delta, 
                                     const HChar* opname, IROp op )
{
   HChar   dis_buf[50];
   Int     alen, size;
   IRTemp  addr;
   Bool    shl, shr, sar;
   UChar   modrm = getUChar(delta);
   UInt    rG    = gregOfRexRM(pfx,modrm);
   UInt    rV    = getVexNvvvv(pfx);;
   IRTemp  g0    = newTemp(Ity_V128);
   IRTemp  g1    = newTemp(Ity_V128);
   IRTemp  amt   = newTemp(Ity_I64);
   IRTemp  amt8  = newTemp(Ity_I8);
   if (epartIsReg(modrm)) {
      UInt rE = eregOfRexRM(pfx,modrm);
      assign( amt, getXMMRegLane64(rE, 0) );
      DIP("%s %s,%s,%s\n", opname, nameXMMReg(rE),
          nameXMMReg(rV), nameXMMReg(rG) );
      delta++;
   } else {
      addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
      assign( amt, loadLE(Ity_I64, mkexpr(addr)) );
      DIP("%s %s,%s,%s\n", opname, dis_buf, nameXMMReg(rV), nameXMMReg(rG) );
      delta += alen;
   }
   assign( g0, getXMMReg(rV) );
   assign( amt8, unop(Iop_64to8, mkexpr(amt)) );

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
           binop(Iop_CmpLT64U, mkexpr(amt), mkU64(size)),
           binop(op, mkexpr(g0), mkexpr(amt8)),
           mkV128(0x0000)
        )
     );
   } else 
   if (sar) {
     assign( 
        g1,
        IRExpr_ITE(
           binop(Iop_CmpLT64U, mkexpr(amt), mkU64(size)),
           binop(op, mkexpr(g0), mkexpr(amt8)),
           binop(op, mkexpr(g0), mkU8(size-1))
        )
     );
   } else {
      vassert(0);
   }

   putYMMRegLoAndZU( rG, mkexpr(g1) );
   return delta;
}


static ULong dis_AVX256_shiftV_byE ( const VexAbiInfo* vbi,
                                     Prefix pfx, Long delta, 
                                     const HChar* opname, IROp op )
{
   HChar   dis_buf[50];
   Int     alen, size;
   IRTemp  addr;
   Bool    shl, shr, sar;
   UChar   modrm = getUChar(delta);
   UInt    rG    = gregOfRexRM(pfx,modrm);
   UInt    rV    = getVexNvvvv(pfx);;
   IRTemp  g0    = newTemp(Ity_V256);
   IRTemp  g1    = newTemp(Ity_V256);
   IRTemp  amt   = newTemp(Ity_I64);
   IRTemp  amt8  = newTemp(Ity_I8);
   if (epartIsReg(modrm)) {
      UInt rE = eregOfRexRM(pfx,modrm);
      assign( amt, getXMMRegLane64(rE, 0) );
      DIP("%s %s,%s,%s\n", opname, nameXMMReg(rE),
          nameYMMReg(rV), nameYMMReg(rG) );
      delta++;
   } else {
      addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
      assign( amt, loadLE(Ity_I64, mkexpr(addr)) );
      DIP("%s %s,%s,%s\n", opname, dis_buf, nameYMMReg(rV), nameYMMReg(rG) );
      delta += alen;
   }
   assign( g0, getYMMReg(rV) );
   assign( amt8, unop(Iop_64to8, mkexpr(amt)) );

   shl = shr = sar = False;
   size = 0;
   switch (op) {
      case Iop_ShlN16x16: shl = True; size = 32; break;
      case Iop_ShlN32x8:  shl = True; size = 32; break;
      case Iop_ShlN64x4:  shl = True; size = 64; break;
      case Iop_SarN16x16: sar = True; size = 16; break;
      case Iop_SarN32x8:  sar = True; size = 32; break;
      case Iop_ShrN16x16: shr = True; size = 16; break;
      case Iop_ShrN32x8:  shr = True; size = 32; break;
      case Iop_ShrN64x4:  shr = True; size = 64; break;
      default: vassert(0);
   }

   if (shl || shr) {
     assign( 
        g1,
        IRExpr_ITE(
           binop(Iop_CmpLT64U, mkexpr(amt), mkU64(size)),
           binop(op, mkexpr(g0), mkexpr(amt8)),
           binop(Iop_V128HLtoV256, mkV128(0), mkV128(0))
        )
     );
   } else 
   if (sar) {
     assign( 
        g1,
        IRExpr_ITE(
           binop(Iop_CmpLT64U, mkexpr(amt), mkU64(size)),
           binop(op, mkexpr(g0), mkexpr(amt8)),
           binop(op, mkexpr(g0), mkU8(size-1))
        )
     );
   } else {
      vassert(0);
   }

   putYMMReg( rG, mkexpr(g1) );
   return delta;
}


static ULong dis_AVX_var_shiftV_byE ( const VexAbiInfo* vbi,
                                      Prefix pfx, Long delta,
                                      const HChar* opname, IROp op, Bool isYMM )
{
   HChar   dis_buf[50];
   Int     alen, size, i;
   IRTemp  addr;
   UChar   modrm = getUChar(delta);
   UInt    rG    = gregOfRexRM(pfx,modrm);
   UInt    rV    = getVexNvvvv(pfx);;
   IRTemp  sV    = isYMM ? newTemp(Ity_V256) : newTemp(Ity_V128);
   IRTemp  amt   = isYMM ? newTemp(Ity_V256) : newTemp(Ity_V128);
   IRTemp  amts[8], sVs[8], res[8];
   if (epartIsReg(modrm)) {
      UInt rE = eregOfRexRM(pfx,modrm);
      assign( amt, isYMM ? getYMMReg(rE) : getXMMReg(rE) );
      if (isYMM) {
         DIP("%s %s,%s,%s\n", opname, nameYMMReg(rE),
             nameYMMReg(rV), nameYMMReg(rG) );
      } else {
         DIP("%s %s,%s,%s\n", opname, nameXMMReg(rE),
             nameXMMReg(rV), nameXMMReg(rG) );
      }
      delta++;
   } else {
      addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
      assign( amt, loadLE(isYMM ? Ity_V256 : Ity_V128, mkexpr(addr)) );
      if (isYMM) {
         DIP("%s %s,%s,%s\n", opname, dis_buf, nameYMMReg(rV),
             nameYMMReg(rG) );
      } else {
         DIP("%s %s,%s,%s\n", opname, dis_buf, nameXMMReg(rV),
             nameXMMReg(rG) );
      }
      delta += alen;
   }
   assign( sV, isYMM ? getYMMReg(rV) : getXMMReg(rV) );

   size = 0;
   switch (op) {
      case Iop_Shl32: size = 32; break;
      case Iop_Shl64: size = 64; break;
      case Iop_Sar32: size = 32; break;
      case Iop_Shr32: size = 32; break;
      case Iop_Shr64: size = 64; break;
      default: vassert(0);
   }

   for (i = 0; i < 8; i++) {
      sVs[i] = IRTemp_INVALID;
      amts[i] = IRTemp_INVALID;
   }
   switch (size) {
      case 32:
         if (isYMM) {
            breakupV256to32s( sV, &sVs[7], &sVs[6], &sVs[5], &sVs[4],
                                  &sVs[3], &sVs[2], &sVs[1], &sVs[0] );
            breakupV256to32s( amt, &amts[7], &amts[6], &amts[5], &amts[4],
                                   &amts[3], &amts[2], &amts[1], &amts[0] );
         } else {
            breakupV128to32s( sV, &sVs[3], &sVs[2], &sVs[1], &sVs[0] );
            breakupV128to32s( amt, &amts[3], &amts[2], &amts[1], &amts[0] );
        }
         break;
      case 64:
         if (isYMM) {
            breakupV256to64s( sV, &sVs[3], &sVs[2], &sVs[1], &sVs[0] );
            breakupV256to64s( amt, &amts[3], &amts[2], &amts[1], &amts[0] );
         } else {
            breakupV128to64s( sV, &sVs[1], &sVs[0] );
            breakupV128to64s( amt, &amts[1], &amts[0] );
         }
         break;
      default: vassert(0);
   }
   for (i = 0; i < 8; i++)
      if (sVs[i] != IRTemp_INVALID) {
         res[i] = size == 32 ? newTemp(Ity_I32) : newTemp(Ity_I64);
         assign( res[i],
                 IRExpr_ITE(
                    binop(size == 32 ? Iop_CmpLT32U : Iop_CmpLT64U,
                          mkexpr(amts[i]),
                          size == 32 ? mkU32(size) : mkU64(size)),
                    binop(op, mkexpr(sVs[i]),
                               unop(size == 32 ? Iop_32to8 : Iop_64to8,
                                    mkexpr(amts[i]))),
                    op == Iop_Sar32 ? binop(op, mkexpr(sVs[i]), mkU8(size-1))
                                    : size == 32 ? mkU32(0) : mkU64(0)
         ));
      }
   switch (size) {
      case 32:
         for (i = 0; i < 8; i++)
            putYMMRegLane32( rG, i, (i < 4 || isYMM)
                                    ? mkexpr(res[i]) : mkU32(0) );
         break;
      case 64:
         for (i = 0; i < 4; i++)
            putYMMRegLane64( rG, i, (i < 2 || isYMM)
                                    ? mkexpr(res[i]) : mkU64(0) );
         break;
      default: vassert(0);
   }

   return delta;
}


static
Long dis_AVX128_shiftE_to_V_imm( Prefix pfx, 
                                 Long delta, const HChar* opname, IROp op )
{
   Bool    shl, shr, sar;
   UChar   rm   = getUChar(delta);
   IRTemp  e0   = newTemp(Ity_V128);
   IRTemp  e1   = newTemp(Ity_V128);
   UInt    rD   = getVexNvvvv(pfx);
   UChar   amt, size;
   vassert(epartIsReg(rm));
   vassert(gregLO3ofRM(rm) == 2 
           || gregLO3ofRM(rm) == 4 || gregLO3ofRM(rm) == 6);
   amt = getUChar(delta+1);
   delta += 2;
   DIP("%s $%d,%s,%s\n", opname,
                         (Int)amt,
                         nameXMMReg(eregOfRexRM(pfx,rm)),
                         nameXMMReg(rD));
   assign( e0, getXMMReg(eregOfRexRM(pfx,rm)) );

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

   putYMMRegLoAndZU( rD, mkexpr(e1) );
   return delta;
}


static
Long dis_AVX256_shiftE_to_V_imm( Prefix pfx, 
                                 Long delta, const HChar* opname, IROp op )
{
   Bool    shl, shr, sar;
   UChar   rm   = getUChar(delta);
   IRTemp  e0   = newTemp(Ity_V256);
   IRTemp  e1   = newTemp(Ity_V256);
   UInt    rD   = getVexNvvvv(pfx);
   UChar   amt, size;
   vassert(epartIsReg(rm));
   vassert(gregLO3ofRM(rm) == 2 
           || gregLO3ofRM(rm) == 4 || gregLO3ofRM(rm) == 6);
   amt = getUChar(delta+1);
   delta += 2;
   DIP("%s $%d,%s,%s\n", opname,
                         (Int)amt,
                         nameYMMReg(eregOfRexRM(pfx,rm)),
                         nameYMMReg(rD));
   assign( e0, getYMMReg(eregOfRexRM(pfx,rm)) );

   shl = shr = sar = False;
   size = 0;
   switch (op) {
      case Iop_ShlN16x16: shl = True; size = 16; break;
      case Iop_ShlN32x8:  shl = True; size = 32; break;
      case Iop_ShlN64x4:  shl = True; size = 64; break;
      case Iop_SarN16x16: sar = True; size = 16; break;
      case Iop_SarN32x8:  sar = True; size = 32; break;
      case Iop_ShrN16x16: shr = True; size = 16; break;
      case Iop_ShrN32x8:  shr = True; size = 32; break;
      case Iop_ShrN64x4:  shr = True; size = 64; break;
      default: vassert(0);
   }


   if (shl || shr) {
     assign( e1, amt >= size 
                    ? binop(Iop_V128HLtoV256, mkV128(0), mkV128(0))
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

   putYMMReg( rD, mkexpr(e1) );
   return delta;
}


static Long dis_AVX128_E_V_to_G_lo64 ( Bool* uses_vvvv,
                                       const VexAbiInfo* vbi,
                                       Prefix pfx, Long delta, 
                                       const HChar* opname, IROp op )
{
   HChar   dis_buf[50];
   Int     alen;
   IRTemp  addr;
   UChar   rm    = getUChar(delta);
   UInt    rG    = gregOfRexRM(pfx,rm);
   UInt    rV    = getVexNvvvv(pfx);
   IRExpr* vpart = getXMMReg(rV);
   if (epartIsReg(rm)) {
      UInt rE = eregOfRexRM(pfx,rm);
      putXMMReg( rG, binop(op, vpart, getXMMReg(rE)) );
      DIP("%s %s,%s,%s\n", opname,
          nameXMMReg(rE), nameXMMReg(rV), nameXMMReg(rG));
      delta = delta+1;
   } else {
      IRTemp epart = newTemp(Ity_V128);
      addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
      assign( epart, unop( Iop_64UtoV128,
                           loadLE(Ity_I64, mkexpr(addr))) );
      putXMMReg( rG, binop(op, vpart, mkexpr(epart)) );
      DIP("%s %s,%s,%s\n", opname,
          dis_buf, nameXMMReg(rV), nameXMMReg(rG));
      delta = delta+alen;
   }
   putYMMRegLane128( rG, 1, mkV128(0) );
   *uses_vvvv = True;
   return delta;
}


static Long dis_AVX128_E_V_to_G_lo64_unary ( Bool* uses_vvvv,
                                             const VexAbiInfo* vbi,
                                             Prefix pfx, Long delta, 
                                             const HChar* opname, IROp op )
{
   HChar   dis_buf[50];
   Int     alen;
   IRTemp  addr;
   UChar   rm  = getUChar(delta);
   UInt    rG  = gregOfRexRM(pfx,rm);
   UInt    rV  = getVexNvvvv(pfx);
   IRTemp  e64 = newTemp(Ity_I64);

   
   if (epartIsReg(rm)) {
      UInt rE = eregOfRexRM(pfx,rm);
      assign(e64, getXMMRegLane64(rE, 0));
      DIP("%s %s,%s,%s\n", opname,
          nameXMMReg(rE), nameXMMReg(rV), nameXMMReg(rG));
      delta += 1;
   } else {
      addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
      assign(e64, loadLE(Ity_I64, mkexpr(addr)));
      DIP("%s %s,%s,%s\n", opname,
          dis_buf, nameXMMReg(rV), nameXMMReg(rG));
      delta += alen;
   }

   
   IRTemp arg = newTemp(Ity_V128);
   assign(arg,
          binop(Iop_SetV128lo64,
                getXMMReg(rV), mkexpr(e64)));
   
   putYMMRegLoAndZU( rG, unop(op, mkexpr(arg)) );
   *uses_vvvv = True;
   return delta;
}


static Long dis_AVX128_E_V_to_G_lo32_unary ( Bool* uses_vvvv,
                                             const VexAbiInfo* vbi,
                                             Prefix pfx, Long delta, 
                                             const HChar* opname, IROp op )
{
   HChar   dis_buf[50];
   Int     alen;
   IRTemp  addr;
   UChar   rm  = getUChar(delta);
   UInt    rG  = gregOfRexRM(pfx,rm);
   UInt    rV  = getVexNvvvv(pfx);
   IRTemp  e32 = newTemp(Ity_I32);

   
   if (epartIsReg(rm)) {
      UInt rE = eregOfRexRM(pfx,rm);
      assign(e32, getXMMRegLane32(rE, 0));
      DIP("%s %s,%s,%s\n", opname,
          nameXMMReg(rE), nameXMMReg(rV), nameXMMReg(rG));
      delta += 1;
   } else {
      addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
      assign(e32, loadLE(Ity_I32, mkexpr(addr)));
      DIP("%s %s,%s,%s\n", opname,
          dis_buf, nameXMMReg(rV), nameXMMReg(rG));
      delta += alen;
   }

   
   IRTemp arg = newTemp(Ity_V128);
   assign(arg,
          binop(Iop_SetV128lo32,
                getXMMReg(rV), mkexpr(e32)));
   
   putYMMRegLoAndZU( rG, unop(op, mkexpr(arg)) );
   *uses_vvvv = True;
   return delta;
}


static Long dis_AVX128_E_V_to_G_lo32 ( Bool* uses_vvvv,
                                       const VexAbiInfo* vbi,
                                       Prefix pfx, Long delta, 
                                       const HChar* opname, IROp op )
{
   HChar   dis_buf[50];
   Int     alen;
   IRTemp  addr;
   UChar   rm    = getUChar(delta);
   UInt    rG    = gregOfRexRM(pfx,rm);
   UInt    rV    = getVexNvvvv(pfx);
   IRExpr* vpart = getXMMReg(rV);
   if (epartIsReg(rm)) {
      UInt rE = eregOfRexRM(pfx,rm);
      putXMMReg( rG, binop(op, vpart, getXMMReg(rE)) );
      DIP("%s %s,%s,%s\n", opname,
          nameXMMReg(rE), nameXMMReg(rV), nameXMMReg(rG));
      delta = delta+1;
   } else {
      IRTemp epart = newTemp(Ity_V128);
      addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
      assign( epart, unop( Iop_32UtoV128,
                           loadLE(Ity_I32, mkexpr(addr))) );
      putXMMReg( rG, binop(op, vpart, mkexpr(epart)) );
      DIP("%s %s,%s,%s\n", opname,
          dis_buf, nameXMMReg(rV), nameXMMReg(rG));
      delta = delta+alen;
   }
   putYMMRegLane128( rG, 1, mkV128(0) );
   *uses_vvvv = True;
   return delta;
}


static Long dis_AVX128_E_V_to_G ( Bool* uses_vvvv,
                                  const VexAbiInfo* vbi,
                                  Prefix pfx, Long delta, 
                                  const HChar* opname, IROp op )
{
   return dis_VEX_NDS_128_AnySimdPfx_0F_WIG(
             uses_vvvv, vbi, pfx, delta, opname, op,
             NULL, False, False
   );
}


static
Long dis_AVX128_cmp_V_E_to_G ( Bool* uses_vvvv,
                               const VexAbiInfo* vbi,
                               Prefix pfx, Long delta, 
                               const HChar* opname, Bool all_lanes, Int sz )
{
   vassert(sz == 4 || sz == 8);
   Long    deltaIN = delta;
   HChar   dis_buf[50];
   Int     alen;
   UInt    imm8;
   IRTemp  addr;
   Bool    preSwap = False;
   IROp    op      = Iop_INVALID;
   Bool    postNot = False;
   IRTemp  plain   = newTemp(Ity_V128);
   UChar   rm      = getUChar(delta);
   UInt    rG      = gregOfRexRM(pfx, rm);
   UInt    rV      = getVexNvvvv(pfx);
   IRTemp argL     = newTemp(Ity_V128);
   IRTemp argR     = newTemp(Ity_V128);

   assign(argL, getXMMReg(rV));
   if (epartIsReg(rm)) {
      imm8 = getUChar(delta+1);
      Bool ok = findSSECmpOp(&preSwap, &op, &postNot, imm8, all_lanes, sz);
      if (!ok) return deltaIN; 
      UInt rE = eregOfRexRM(pfx,rm);
      assign(argR, getXMMReg(rE));
      delta += 1+1;
      DIP("%s $%d,%s,%s,%s\n",
          opname, (Int)imm8,
          nameXMMReg(rE), nameXMMReg(rV), nameXMMReg(rG));
   } else {
      addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 1 );
      imm8 = getUChar(delta+alen);
      Bool ok = findSSECmpOp(&preSwap, &op, &postNot, imm8, all_lanes, sz);
      if (!ok) return deltaIN; 
      assign(argR, 
             all_lanes   ? loadLE(Ity_V128, mkexpr(addr))
             : sz == 8   ? unop( Iop_64UtoV128, loadLE(Ity_I64, mkexpr(addr)))
             :    unop( Iop_32UtoV128, loadLE(Ity_I32, mkexpr(addr))));
      delta += alen+1;
      DIP("%s $%d,%s,%s,%s\n",
          opname, (Int)imm8, dis_buf, nameXMMReg(rV), nameXMMReg(rG));
   }

   assign(plain, preSwap ? binop(op, mkexpr(argR), mkexpr(argL))
                         : binop(op, mkexpr(argL), mkexpr(argR)));

   if (all_lanes) {
      if (postNot) {
         putYMMRegLoAndZU( rG, unop(Iop_NotV128, mkexpr(plain)) );
      } else {
         putYMMRegLoAndZU( rG, mkexpr(plain) );
      }
   }
   else
   if (!preSwap) {
      if (postNot) {
         IRExpr* mask = mkV128(sz==4 ? 0x000F : 0x00FF);
         putYMMRegLoAndZU( rG, binop(Iop_XorV128, mkexpr(plain),
                                                  mask) );
      } else {
         putYMMRegLoAndZU( rG, mkexpr(plain) );
      }
   }
   else {
      IRTemp res     = newTemp(Ity_V128);
      IRTemp mask    = newTemp(Ity_V128);
      IRTemp notMask = newTemp(Ity_V128);
      assign(mask,    mkV128(sz==4 ? 0x000F : 0x00FF));
      assign(notMask, mkV128(sz==4 ? 0xFFF0 : 0xFF00));
      if (postNot) {
         assign(res,
                binop(Iop_OrV128,
                      binop(Iop_AndV128,
                            unop(Iop_NotV128, mkexpr(plain)),
                            mkexpr(mask)),
                      binop(Iop_AndV128, mkexpr(argL), mkexpr(notMask))));
      } else {
         assign(res,
                binop(Iop_OrV128,
                      binop(Iop_AndV128,
                            mkexpr(plain),
                            mkexpr(mask)),
                      binop(Iop_AndV128, mkexpr(argL), mkexpr(notMask))));
      }
      putYMMRegLoAndZU( rG, mkexpr(res) );
   }

   *uses_vvvv = True;
   return delta;
}


static
Long dis_AVX256_cmp_V_E_to_G ( Bool* uses_vvvv,
                               const VexAbiInfo* vbi,
                               Prefix pfx, Long delta, 
                               const HChar* opname, Int sz )
{
   vassert(sz == 4 || sz == 8);
   Long    deltaIN = delta;
   HChar   dis_buf[50];
   Int     alen;
   UInt    imm8;
   IRTemp  addr;
   Bool    preSwap = False;
   IROp    op      = Iop_INVALID;
   Bool    postNot = False;
   IRTemp  plain   = newTemp(Ity_V256);
   UChar   rm      = getUChar(delta);
   UInt    rG      = gregOfRexRM(pfx, rm);
   UInt    rV      = getVexNvvvv(pfx);
   IRTemp argL     = newTemp(Ity_V256);
   IRTemp argR     = newTemp(Ity_V256);
   IRTemp argLhi   = IRTemp_INVALID;
   IRTemp argLlo   = IRTemp_INVALID;
   IRTemp argRhi   = IRTemp_INVALID;
   IRTemp argRlo   = IRTemp_INVALID;

   assign(argL, getYMMReg(rV));
   if (epartIsReg(rm)) {
      imm8 = getUChar(delta+1);
      Bool ok = findSSECmpOp(&preSwap, &op, &postNot, imm8,
                             True, sz);
      if (!ok) return deltaIN; 
      UInt rE = eregOfRexRM(pfx,rm);
      assign(argR, getYMMReg(rE));
      delta += 1+1;
      DIP("%s $%d,%s,%s,%s\n",
          opname, (Int)imm8,
          nameYMMReg(rE), nameYMMReg(rV), nameYMMReg(rG));
   } else {
      addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 1 );
      imm8 = getUChar(delta+alen);
      Bool ok = findSSECmpOp(&preSwap, &op, &postNot, imm8,
                             True, sz);
      if (!ok) return deltaIN; 
      assign(argR, loadLE(Ity_V256, mkexpr(addr)) );
      delta += alen+1;
      DIP("%s $%d,%s,%s,%s\n",
          opname, (Int)imm8, dis_buf, nameYMMReg(rV), nameYMMReg(rG));
   }

   breakupV256toV128s( preSwap ? argR : argL, &argLhi, &argLlo );
   breakupV256toV128s( preSwap ? argL : argR, &argRhi, &argRlo );
   assign(plain, binop( Iop_V128HLtoV256,
                        binop(op, mkexpr(argLhi), mkexpr(argRhi)),
                        binop(op, mkexpr(argLlo), mkexpr(argRlo)) ) );

   if (postNot) {
      putYMMReg( rG, unop(Iop_NotV256, mkexpr(plain)) );
   } else {
      putYMMReg( rG, mkexpr(plain) );
   }

   *uses_vvvv = True;
   return delta;
}


static
Long dis_AVX128_E_to_G_unary ( Bool* uses_vvvv,
                               const VexAbiInfo* vbi,
                               Prefix pfx, Long delta, 
                               const HChar* opname,
                               IRTemp (*opFn)(IRTemp) )
{
   HChar  dis_buf[50];
   Int    alen;
   IRTemp addr;
   IRTemp res  = newTemp(Ity_V128);
   IRTemp arg  = newTemp(Ity_V128);
   UChar  rm   = getUChar(delta);
   UInt   rG   = gregOfRexRM(pfx, rm);
   if (epartIsReg(rm)) {
      UInt rE = eregOfRexRM(pfx,rm);
      assign(arg, getXMMReg(rE));
      delta += 1;
      DIP("%s %s,%s\n", opname, nameXMMReg(rE), nameXMMReg(rG));
   } else {
      addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
      assign(arg, loadLE(Ity_V128, mkexpr(addr)));
      delta += alen;
      DIP("%s %s,%s\n", opname, dis_buf, nameXMMReg(rG));
   }
   res = opFn(arg);
   putYMMRegLoAndZU( rG, mkexpr(res) );
   *uses_vvvv = False;
   return delta;
}


static
Long dis_AVX128_E_to_G_unary_all ( Bool* uses_vvvv,
                                   const VexAbiInfo* vbi,
                                   Prefix pfx, Long delta, 
                                   const HChar* opname, IROp op )
{
   HChar  dis_buf[50];
   Int    alen;
   IRTemp addr;
   IRTemp arg  = newTemp(Ity_V128);
   UChar  rm   = getUChar(delta);
   UInt   rG   = gregOfRexRM(pfx, rm);
   if (epartIsReg(rm)) {
      UInt rE = eregOfRexRM(pfx,rm);
      assign(arg, getXMMReg(rE));
      delta += 1;
      DIP("%s %s,%s\n", opname, nameXMMReg(rE), nameXMMReg(rG));
   } else {
      addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
      assign(arg, loadLE(Ity_V128, mkexpr(addr)));
      delta += alen;
      DIP("%s %s,%s\n", opname, dis_buf, nameXMMReg(rG));
   }
   
   
   Bool needsIRRM = op == Iop_Sqrt32Fx4 || op == Iop_Sqrt64Fx2;
   
   IRExpr* res = needsIRRM ? binop(op, get_FAKE_roundingmode(), mkexpr(arg))
                           : unop(op, mkexpr(arg));
   putYMMRegLoAndZU( rG, res );
   *uses_vvvv = False;
   return delta;
}


static
Long dis_VEX_NDS_256_AnySimdPfx_0F_WIG (
        Bool* uses_vvvv, const VexAbiInfo* vbi,
        Prefix pfx, Long delta, const HChar* name,
        IROp op, IRTemp(*opFn)(IRTemp,IRTemp),
        Bool invertLeftArg,
        Bool swapArgs
     )
{
   UChar  modrm = getUChar(delta);
   UInt   rD    = gregOfRexRM(pfx, modrm);
   UInt   rSL   = getVexNvvvv(pfx);
   IRTemp tSL   = newTemp(Ity_V256);
   IRTemp tSR   = newTemp(Ity_V256);
   IRTemp addr  = IRTemp_INVALID;
   HChar  dis_buf[50];
   Int    alen  = 0;
   vassert(1==getVexL(pfx) && 0==getRexW(pfx));

   assign(tSL, invertLeftArg ? unop(Iop_NotV256, getYMMReg(rSL))
                             : getYMMReg(rSL));

   if (epartIsReg(modrm)) {
      UInt rSR = eregOfRexRM(pfx, modrm);
      delta += 1;
      assign(tSR, getYMMReg(rSR));
      DIP("%s %s,%s,%s\n",
          name, nameYMMReg(rSR), nameYMMReg(rSL), nameYMMReg(rD));
   } else {
      addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 0 );
      delta += alen;
      assign(tSR, loadLE(Ity_V256, mkexpr(addr)));
      DIP("%s %s,%s,%s\n",
          name, dis_buf, nameYMMReg(rSL), nameYMMReg(rD));
   }

   IRTemp res = IRTemp_INVALID;
   if (op != Iop_INVALID) {
      vassert(opFn == NULL);
      res = newTemp(Ity_V256);
      if (requiresRMode(op)) {
         IRTemp rm = newTemp(Ity_I32);
         assign(rm, get_FAKE_roundingmode()); 
         assign(res, swapArgs
                        ? triop(op, mkexpr(rm), mkexpr(tSR), mkexpr(tSL))
                        : triop(op, mkexpr(rm), mkexpr(tSL), mkexpr(tSR)));
      } else {
         assign(res, swapArgs
                        ? binop(op, mkexpr(tSR), mkexpr(tSL))
                        : binop(op, mkexpr(tSL), mkexpr(tSR)));
      }
   } else {
      vassert(opFn != NULL);
      res = swapArgs ? opFn(tSR, tSL) : opFn(tSL, tSR);
   }

   putYMMReg(rD, mkexpr(res));

   *uses_vvvv = True;
   return delta;
}


static Long dis_AVX256_E_V_to_G ( Bool* uses_vvvv,
                                  const VexAbiInfo* vbi,
                                  Prefix pfx, Long delta, 
                                  const HChar* opname, IROp op )
{
   return dis_VEX_NDS_256_AnySimdPfx_0F_WIG(
             uses_vvvv, vbi, pfx, delta, opname, op,
             NULL, False, False
   );
}


static
Long dis_VEX_NDS_256_AnySimdPfx_0F_WIG_simple (
        Bool* uses_vvvv, const VexAbiInfo* vbi,
        Prefix pfx, Long delta, const HChar* name,
        IROp op
     )
{
   return dis_VEX_NDS_256_AnySimdPfx_0F_WIG(
             uses_vvvv, vbi, pfx, delta, name, op, NULL, False, False);
}


static
Long dis_VEX_NDS_256_AnySimdPfx_0F_WIG_complex (
        Bool* uses_vvvv, const VexAbiInfo* vbi,
        Prefix pfx, Long delta, const HChar* name,
        IRTemp(*opFn)(IRTemp,IRTemp)
     )
{
   return dis_VEX_NDS_256_AnySimdPfx_0F_WIG(
             uses_vvvv, vbi, pfx, delta, name,
             Iop_INVALID, opFn, False, False );
}


static
Long dis_AVX256_E_to_G_unary ( Bool* uses_vvvv,
                               const VexAbiInfo* vbi,
                               Prefix pfx, Long delta,
                               const HChar* opname,
                               IRTemp (*opFn)(IRTemp) )
{
   HChar  dis_buf[50];
   Int    alen;
   IRTemp addr;
   IRTemp res  = newTemp(Ity_V256);
   IRTemp arg  = newTemp(Ity_V256);
   UChar  rm   = getUChar(delta);
   UInt   rG   = gregOfRexRM(pfx, rm);
   if (epartIsReg(rm)) {
      UInt rE = eregOfRexRM(pfx,rm);
      assign(arg, getYMMReg(rE));
      delta += 1;
      DIP("%s %s,%s\n", opname, nameYMMReg(rE), nameYMMReg(rG));
   } else {
      addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
      assign(arg, loadLE(Ity_V256, mkexpr(addr)));
      delta += alen;
      DIP("%s %s,%s\n", opname, dis_buf, nameYMMReg(rG));
   }
   res = opFn(arg);
   putYMMReg( rG, mkexpr(res) );
   *uses_vvvv = False;
   return delta;
}


static
Long dis_AVX256_E_to_G_unary_all ( Bool* uses_vvvv,
                                   const VexAbiInfo* vbi,
                                   Prefix pfx, Long delta, 
                                   const HChar* opname, IROp op )
{
   HChar  dis_buf[50];
   Int    alen;
   IRTemp addr;
   IRTemp arg  = newTemp(Ity_V256);
   UChar  rm   = getUChar(delta);
   UInt   rG   = gregOfRexRM(pfx, rm);
   if (epartIsReg(rm)) {
      UInt rE = eregOfRexRM(pfx,rm);
      assign(arg, getYMMReg(rE));
      delta += 1;
      DIP("%s %s,%s\n", opname, nameYMMReg(rE), nameYMMReg(rG));
   } else {
      addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
      assign(arg, loadLE(Ity_V256, mkexpr(addr)));
      delta += alen;
      DIP("%s %s,%s\n", opname, dis_buf, nameYMMReg(rG));
   }
   putYMMReg( rG, unop(op, mkexpr(arg)) );
   *uses_vvvv = False;
   return delta;
}


static Long dis_CVTDQ2PD_256 ( const VexAbiInfo* vbi, Prefix pfx,
                               Long delta )
{
   IRTemp addr  = IRTemp_INVALID;
   Int    alen  = 0;
   HChar  dis_buf[50];
   UChar  modrm = getUChar(delta);
   IRTemp sV    = newTemp(Ity_V128);
   UInt   rG    = gregOfRexRM(pfx,modrm);
   if (epartIsReg(modrm)) {
      UInt rE = eregOfRexRM(pfx,modrm);
      assign( sV, getXMMReg(rE) );
      delta += 1;
      DIP("vcvtdq2pd %s,%s\n", nameXMMReg(rE), nameYMMReg(rG));
   } else {
      addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
      assign( sV, loadLE(Ity_V128, mkexpr(addr)) );
      delta += alen;
      DIP("vcvtdq2pd %s,%s\n", dis_buf, nameYMMReg(rG) );
   }
   IRTemp s3, s2, s1, s0;
   s3 = s2 = s1 = s0 = IRTemp_INVALID;
   breakupV128to32s( sV, &s3, &s2, &s1, &s0 );
   IRExpr* res 
      = IRExpr_Qop(
           Iop_64x4toV256,
           unop(Iop_ReinterpF64asI64, unop(Iop_I32StoF64, mkexpr(s3))),
           unop(Iop_ReinterpF64asI64, unop(Iop_I32StoF64, mkexpr(s2))),
           unop(Iop_ReinterpF64asI64, unop(Iop_I32StoF64, mkexpr(s1))),
           unop(Iop_ReinterpF64asI64, unop(Iop_I32StoF64, mkexpr(s0)))
        );
   putYMMReg(rG, res);
   return delta;
}


static Long dis_CVTPD2PS_256 ( const VexAbiInfo* vbi, Prefix pfx,
                               Long delta )
{
   IRTemp addr  = IRTemp_INVALID;
   Int    alen  = 0;
   HChar  dis_buf[50];
   UChar  modrm = getUChar(delta);
   UInt   rG    = gregOfRexRM(pfx,modrm);
   IRTemp argV  = newTemp(Ity_V256);
   IRTemp rmode = newTemp(Ity_I32);
   if (epartIsReg(modrm)) {
      UInt rE = eregOfRexRM(pfx,modrm);
      assign( argV, getYMMReg(rE) );
      delta += 1;
      DIP("vcvtpd2psy %s,%s\n", nameYMMReg(rE), nameXMMReg(rG));
   } else {
      addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
      assign( argV, loadLE(Ity_V256, mkexpr(addr)) );
      delta += alen;
      DIP("vcvtpd2psy %s,%s\n", dis_buf, nameXMMReg(rG) );
   }
         
   assign( rmode, get_sse_roundingmode() );
   IRTemp t3, t2, t1, t0;
   t3 = t2 = t1 = t0 = IRTemp_INVALID;
   breakupV256to64s( argV, &t3, &t2, &t1, &t0 );
#  define CVT(_t)  binop( Iop_F64toF32, mkexpr(rmode), \
                          unop(Iop_ReinterpI64asF64, mkexpr(_t)) )
   putXMMRegLane32F( rG, 3, CVT(t3) );
   putXMMRegLane32F( rG, 2, CVT(t2) );
   putXMMRegLane32F( rG, 1, CVT(t1) );
   putXMMRegLane32F( rG, 0, CVT(t0) );
#  undef CVT
   putYMMRegLane128( rG, 1, mkV128(0) );
   return delta;
}


static IRTemp math_VPUNPCK_YMM ( IRTemp tL, IRType tR, IROp op )
{
   IRTemp tLhi, tLlo, tRhi, tRlo;
   tLhi = tLlo = tRhi = tRlo = IRTemp_INVALID;
   IRTemp res = newTemp(Ity_V256);
   breakupV256toV128s( tL, &tLhi, &tLlo );
   breakupV256toV128s( tR, &tRhi, &tRlo );
   assign( res, binop( Iop_V128HLtoV256,
                       binop( op, mkexpr(tRhi), mkexpr(tLhi) ),
                       binop( op, mkexpr(tRlo), mkexpr(tLlo) ) ) );
   return res;
}


static IRTemp math_VPUNPCKLBW_YMM ( IRTemp tL, IRTemp tR )
{
   return math_VPUNPCK_YMM( tL, tR, Iop_InterleaveLO8x16 );
}


static IRTemp math_VPUNPCKLWD_YMM ( IRTemp tL, IRTemp tR )
{
   return math_VPUNPCK_YMM( tL, tR, Iop_InterleaveLO16x8 );
}


static IRTemp math_VPUNPCKLDQ_YMM ( IRTemp tL, IRTemp tR )
{
   return math_VPUNPCK_YMM( tL, tR, Iop_InterleaveLO32x4 );
}


static IRTemp math_VPUNPCKLQDQ_YMM ( IRTemp tL, IRTemp tR )
{
   return math_VPUNPCK_YMM( tL, tR, Iop_InterleaveLO64x2 );
}


static IRTemp math_VPUNPCKHBW_YMM ( IRTemp tL, IRTemp tR )
{
   return math_VPUNPCK_YMM( tL, tR, Iop_InterleaveHI8x16 );
}


static IRTemp math_VPUNPCKHWD_YMM ( IRTemp tL, IRTemp tR )
{
   return math_VPUNPCK_YMM( tL, tR, Iop_InterleaveHI16x8 );
}


static IRTemp math_VPUNPCKHDQ_YMM ( IRTemp tL, IRTemp tR )
{
   return math_VPUNPCK_YMM( tL, tR, Iop_InterleaveHI32x4 );
}


static IRTemp math_VPUNPCKHQDQ_YMM ( IRTemp tL, IRTemp tR )
{
   return math_VPUNPCK_YMM( tL, tR, Iop_InterleaveHI64x2 );
}


static IRTemp math_VPACKSSWB_YMM ( IRTemp tL, IRTemp tR )
{
   return math_VPUNPCK_YMM( tL, tR, Iop_QNarrowBin16Sto8Sx16 );
}


static IRTemp math_VPACKUSWB_YMM ( IRTemp tL, IRTemp tR )
{
   return math_VPUNPCK_YMM( tL, tR, Iop_QNarrowBin16Sto8Ux16 );
}


static IRTemp math_VPACKSSDW_YMM ( IRTemp tL, IRTemp tR )
{
   return math_VPUNPCK_YMM( tL, tR, Iop_QNarrowBin32Sto16Sx8 );
}


static IRTemp math_VPACKUSDW_YMM ( IRTemp tL, IRTemp tR )
{
   return math_VPUNPCK_YMM( tL, tR, Iop_QNarrowBin32Sto16Ux8 );
}


__attribute__((noinline))
static
Long dis_ESC_0F__VEX (
        DisResult* dres,
           Bool*      uses_vvvv,
        Bool         (*resteerOkFn) ( void*, Addr ),
        Bool         resteerCisOk,
        void*        callback_opaque,
        const VexArchInfo* archinfo,
        const VexAbiInfo*  vbi,
        Prefix pfx, Int sz, Long deltaIN 
     )
{
   IRTemp addr  = IRTemp_INVALID;
   Int    alen  = 0;
   HChar  dis_buf[50];
   Long   delta = deltaIN;
   UChar  opc   = getUChar(delta);
   delta++;
   *uses_vvvv = False;

   switch (opc) {

   case 0x10:
      
      if (haveF2no66noF3(pfx) && !epartIsReg(getUChar(delta))) {
         UChar modrm = getUChar(delta);
         addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
         UInt   rG   = gregOfRexRM(pfx,modrm);
         IRTemp z128 = newTemp(Ity_V128);
         assign(z128, mkV128(0));
         putXMMReg( rG, mkexpr(z128) );
         
         putXMMRegLane64( rG, 0, loadLE(Ity_I64, mkexpr(addr)) );
         putYMMRegLane128( rG, 1, mkexpr(z128) );
         DIP("vmovsd %s,%s\n", dis_buf, nameXMMReg(rG));
         delta += alen;
         goto decode_success;
      }
      
      
      if (haveF2no66noF3(pfx) && epartIsReg(getUChar(delta))) {
         UChar modrm = getUChar(delta);
         UInt  rG    = gregOfRexRM(pfx, modrm);
         UInt  rE    = eregOfRexRM(pfx, modrm);
         UInt  rV    = getVexNvvvv(pfx);
         delta++;
         DIP("vmovsd %s,%s,%s\n",
             nameXMMReg(rE), nameXMMReg(rV), nameXMMReg(rG));
         IRTemp res = newTemp(Ity_V128);
         assign(res, binop(Iop_64HLtoV128,
                           getXMMRegLane64(rV, 1),
                           getXMMRegLane64(rE, 0)));
         putYMMRegLoAndZU(rG, mkexpr(res));
         *uses_vvvv = True;
         goto decode_success;
      }
      
      if (haveF3no66noF2(pfx) && !epartIsReg(getUChar(delta))) {
         UChar modrm = getUChar(delta);
         addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
         UInt   rG   = gregOfRexRM(pfx,modrm);
         IRTemp z128 = newTemp(Ity_V128);
         assign(z128, mkV128(0));
         putXMMReg( rG, mkexpr(z128) );
         
         putXMMRegLane32( rG, 0, loadLE(Ity_I32, mkexpr(addr)) );
         putYMMRegLane128( rG, 1, mkexpr(z128) );
         DIP("vmovss %s,%s\n", dis_buf, nameXMMReg(rG));
         delta += alen;
         goto decode_success;
      }
      
      
      if (haveF3no66noF2(pfx) && epartIsReg(getUChar(delta))) {
         UChar modrm = getUChar(delta);
         UInt  rG    = gregOfRexRM(pfx, modrm);
         UInt  rE    = eregOfRexRM(pfx, modrm);
         UInt  rV    = getVexNvvvv(pfx);
         delta++;
         DIP("vmovss %s,%s,%s\n",
             nameXMMReg(rE), nameXMMReg(rV), nameXMMReg(rG));
         IRTemp res = newTemp(Ity_V128);
         assign( res, binop( Iop_64HLtoV128,
                             getXMMRegLane64(rV, 1),
                             binop(Iop_32HLto64,
                                   getXMMRegLane32(rV, 1),
                                   getXMMRegLane32(rE, 0)) ) );
         putYMMRegLoAndZU(rG, mkexpr(res));
         *uses_vvvv = True;
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         UChar modrm = getUChar(delta);
         UInt  rG    = gregOfRexRM(pfx, modrm);
         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx,modrm);
            putYMMRegLoAndZU( rG, getXMMReg( rE ));
            DIP("vmovupd %s,%s\n", nameXMMReg(rE), nameXMMReg(rG));
            delta += 1;
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            putYMMRegLoAndZU( rG, loadLE(Ity_V128, mkexpr(addr)) );
            DIP("vmovupd %s,%s\n", dis_buf, nameXMMReg(rG));
            delta += alen;
         }
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         UChar modrm = getUChar(delta);
         UInt  rG    = gregOfRexRM(pfx, modrm);
         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx,modrm);
            putYMMReg( rG, getYMMReg( rE ));
            DIP("vmovupd %s,%s\n", nameYMMReg(rE), nameYMMReg(rG));
            delta += 1;
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            putYMMReg( rG, loadLE(Ity_V256, mkexpr(addr)) );
            DIP("vmovupd %s,%s\n", dis_buf, nameYMMReg(rG));
            delta += alen;
         }
         goto decode_success;
      }
      
      if (haveNo66noF2noF3(pfx) && 0==getVexL(pfx)) {
         UChar modrm = getUChar(delta);
         UInt  rG    = gregOfRexRM(pfx, modrm);
         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx,modrm);
            putYMMRegLoAndZU( rG, getXMMReg( rE ));
            DIP("vmovups %s,%s\n", nameXMMReg(rE), nameXMMReg(rG));
            delta += 1;
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            putYMMRegLoAndZU( rG, loadLE(Ity_V128, mkexpr(addr)) );
            DIP("vmovups %s,%s\n", dis_buf, nameXMMReg(rG));
            delta += alen;
         }
         goto decode_success;
      }
      
      if (haveNo66noF2noF3(pfx) && 1==getVexL(pfx)) {
         UChar modrm = getUChar(delta);
         UInt  rG    = gregOfRexRM(pfx, modrm);
         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx,modrm);
            putYMMReg( rG, getYMMReg( rE ));
            DIP("vmovups %s,%s\n", nameYMMReg(rE), nameYMMReg(rG));
            delta += 1;
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            putYMMReg( rG, loadLE(Ity_V256, mkexpr(addr)) );
            DIP("vmovups %s,%s\n", dis_buf, nameYMMReg(rG));
            delta += alen;
         }
         goto decode_success;
      }
      break;

   case 0x11:
      
      
      if (haveF2no66noF3(pfx) && !epartIsReg(getUChar(delta))) {
         UChar modrm = getUChar(delta);
         addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
         UInt   rG   = gregOfRexRM(pfx,modrm);
         
         storeLE( mkexpr(addr), getXMMRegLane64(rG, 0));
         DIP("vmovsd %s,%s\n", nameXMMReg(rG), dis_buf);
         delta += alen;
         goto decode_success;
      }
      
      
      if (haveF2no66noF3(pfx) && epartIsReg(getUChar(delta))) {
         UChar modrm = getUChar(delta);
         UInt  rG    = gregOfRexRM(pfx, modrm);
         UInt  rE    = eregOfRexRM(pfx, modrm);
         UInt  rV    = getVexNvvvv(pfx);
         delta++;
         DIP("vmovsd %s,%s,%s\n",
             nameXMMReg(rE), nameXMMReg(rV), nameXMMReg(rG));
         IRTemp res = newTemp(Ity_V128);
         assign(res, binop(Iop_64HLtoV128,
                           getXMMRegLane64(rV, 1),
                           getXMMRegLane64(rE, 0)));
         putYMMRegLoAndZU(rG, mkexpr(res));
         *uses_vvvv = True;
         goto decode_success;
      }
      
      
      if (haveF3no66noF2(pfx) && !epartIsReg(getUChar(delta))) {
         UChar modrm = getUChar(delta);
         addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
         UInt   rG   = gregOfRexRM(pfx,modrm);
         
         storeLE( mkexpr(addr), getXMMRegLane32(rG, 0));
         DIP("vmovss %s,%s\n", nameXMMReg(rG), dis_buf);
         delta += alen;
         goto decode_success;
      }
      
      
      if (haveF3no66noF2(pfx) && epartIsReg(getUChar(delta))) {
         UChar modrm = getUChar(delta);
         UInt  rG    = gregOfRexRM(pfx, modrm);
         UInt  rE    = eregOfRexRM(pfx, modrm);
         UInt  rV    = getVexNvvvv(pfx);
         delta++;
         DIP("vmovss %s,%s,%s\n",
             nameXMMReg(rE), nameXMMReg(rV), nameXMMReg(rG));
         IRTemp res = newTemp(Ity_V128);
         assign( res, binop( Iop_64HLtoV128,
                             getXMMRegLane64(rV, 1),
                             binop(Iop_32HLto64,
                                   getXMMRegLane32(rV, 1),
                                   getXMMRegLane32(rE, 0)) ) );
         putYMMRegLoAndZU(rG, mkexpr(res));
         *uses_vvvv = True;
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         UChar modrm = getUChar(delta);
         UInt  rG    = gregOfRexRM(pfx,modrm);
         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx,modrm);
            putYMMRegLoAndZU( rE, getXMMReg(rG) );
            DIP("vmovupd %s,%s\n", nameXMMReg(rG), nameXMMReg(rE));
            delta += 1;
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            storeLE( mkexpr(addr), getXMMReg(rG) );
            DIP("vmovupd %s,%s\n", nameXMMReg(rG), dis_buf);
            delta += alen;
         }
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         UChar modrm = getUChar(delta);
         UInt  rG    = gregOfRexRM(pfx,modrm);
         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx,modrm);
            putYMMReg( rE, getYMMReg(rG) );
            DIP("vmovupd %s,%s\n", nameYMMReg(rG), nameYMMReg(rE));
            delta += 1;
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            storeLE( mkexpr(addr), getYMMReg(rG) );
            DIP("vmovupd %s,%s\n", nameYMMReg(rG), dis_buf);
            delta += alen;
         }
         goto decode_success;
      }
      
      if (haveNo66noF2noF3(pfx) && 0==getVexL(pfx)) {
         UChar modrm = getUChar(delta);
         UInt  rG    = gregOfRexRM(pfx,modrm);
         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx,modrm);
            putYMMRegLoAndZU( rE, getXMMReg(rG) );
            DIP("vmovups %s,%s\n", nameXMMReg(rG), nameXMMReg(rE));
            delta += 1;
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            storeLE( mkexpr(addr), getXMMReg(rG) );
            DIP("vmovups %s,%s\n", nameXMMReg(rG), dis_buf);
            delta += alen;
         }
         goto decode_success;
      }
      
      if (haveNo66noF2noF3(pfx) && 1==getVexL(pfx)) {
         UChar modrm = getUChar(delta);
         UInt  rG    = gregOfRexRM(pfx,modrm);
         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx,modrm);
            putYMMReg( rE, getYMMReg(rG) );
            DIP("vmovups %s,%s\n", nameYMMReg(rG), nameYMMReg(rE));
            delta += 1;
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            storeLE( mkexpr(addr), getYMMReg(rG) );
            DIP("vmovups %s,%s\n", nameYMMReg(rG), dis_buf);
            delta += alen;
         }
         goto decode_success;
      }
      break;

   case 0x12:
      
      if (haveF2no66noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_MOVDDUP_128( vbi, pfx, delta, True );
         goto decode_success;
      }
      
      if (haveF2no66noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_MOVDDUP_256( vbi, pfx, delta );
         goto decode_success;
      }
      
      
      if (haveNo66noF2noF3(pfx) && 0==getVexL(pfx)
          && epartIsReg(getUChar(delta))) {
         UChar modrm = getUChar(delta);
         UInt  rG    = gregOfRexRM(pfx, modrm);
         UInt  rE    = eregOfRexRM(pfx, modrm);
         UInt  rV    = getVexNvvvv(pfx);
         delta++;
         DIP("vmovhlps %s,%s,%s\n",
             nameXMMReg(rE), nameXMMReg(rV), nameXMMReg(rG));
         IRTemp res = newTemp(Ity_V128);
         assign(res, binop(Iop_64HLtoV128,
                           getXMMRegLane64(rV, 1),
                           getXMMRegLane64(rE, 1)));
         putYMMRegLoAndZU(rG, mkexpr(res));
         *uses_vvvv = True;
         goto decode_success;
      }
      
      
      
      
      if ((have66noF2noF3(pfx) || haveNo66noF2noF3(pfx))
          && 0==getVexL(pfx) && !epartIsReg(getUChar(delta))) {
         UChar modrm = getUChar(delta);
         UInt  rG    = gregOfRexRM(pfx, modrm);
         UInt  rV    = getVexNvvvv(pfx);
         addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
         delta += alen;
         DIP("vmovlpd %s,%s,%s\n",
             dis_buf, nameXMMReg(rV), nameXMMReg(rG));
         IRTemp res = newTemp(Ity_V128);
         assign(res, binop(Iop_64HLtoV128,
                           getXMMRegLane64(rV, 1),
                           loadLE(Ity_I64, mkexpr(addr))));
         putYMMRegLoAndZU(rG, mkexpr(res));
         *uses_vvvv = True;
         goto decode_success;
      }
      
      if (haveF3no66noF2(pfx) && 0==getVexL(pfx)) {
         delta = dis_MOVSxDUP_128( vbi, pfx, delta, True,
                                   True );
         goto decode_success;
      }
      
      if (haveF3no66noF2(pfx) && 1==getVexL(pfx)) {
         delta = dis_MOVSxDUP_256( vbi, pfx, delta, True );
         goto decode_success;
      }
      break;

   case 0x13:
      
      
      
      
      if ((have66noF2noF3(pfx) || haveNo66noF2noF3(pfx))
          && 0==getVexL(pfx) && !epartIsReg(getUChar(delta))) {
         UChar modrm = getUChar(delta);
         UInt  rG    = gregOfRexRM(pfx, modrm);
         addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
         delta += alen;
         storeLE( mkexpr(addr), getXMMRegLane64( rG, 0));
         DIP("vmovlpd %s,%s\n", nameXMMReg(rG), dis_buf);
         goto decode_success;
      }
      break;

   case 0x14:
   case 0x15:
      
      
      if (haveNo66noF2noF3(pfx) && 0==getVexL(pfx)) {
         Bool   hi    = opc == 0x15;
         UChar  modrm = getUChar(delta);
         UInt   rG    = gregOfRexRM(pfx,modrm);
         UInt   rV    = getVexNvvvv(pfx);
         IRTemp eV    = newTemp(Ity_V128);
         IRTemp vV    = newTemp(Ity_V128);
         assign( vV, getXMMReg(rV) );
         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx,modrm);
            assign( eV, getXMMReg(rE) );
            delta += 1;
            DIP("vunpck%sps %s,%s\n", hi ? "h" : "l",
                nameXMMReg(rE), nameXMMReg(rG));
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            assign( eV, loadLE(Ity_V128, mkexpr(addr)) );
            delta += alen;
            DIP("vunpck%sps %s,%s\n", hi ? "h" : "l",
                dis_buf, nameXMMReg(rG));
         }
         IRTemp res = math_UNPCKxPS_128( eV, vV, hi );
         putYMMRegLoAndZU( rG, mkexpr(res) );
         *uses_vvvv = True;
         goto decode_success;
      }
      
      
      if (haveNo66noF2noF3(pfx) && 1==getVexL(pfx)) {
         Bool   hi    = opc == 0x15;
         UChar  modrm = getUChar(delta);
         UInt   rG    = gregOfRexRM(pfx,modrm);
         UInt   rV    = getVexNvvvv(pfx);
         IRTemp eV    = newTemp(Ity_V256);
         IRTemp vV    = newTemp(Ity_V256);
         assign( vV, getYMMReg(rV) );
         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx,modrm);
            assign( eV, getYMMReg(rE) );
            delta += 1;
            DIP("vunpck%sps %s,%s\n", hi ? "h" : "l",
                nameYMMReg(rE), nameYMMReg(rG));
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            assign( eV, loadLE(Ity_V256, mkexpr(addr)) );
            delta += alen;
            DIP("vunpck%sps %s,%s\n", hi ? "h" : "l",
                dis_buf, nameYMMReg(rG));
         }
         IRTemp res = math_UNPCKxPS_256( eV, vV, hi );
         putYMMReg( rG, mkexpr(res) );
         *uses_vvvv = True;
         goto decode_success;
      }
      
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         Bool   hi    = opc == 0x15;
         UChar  modrm = getUChar(delta);
         UInt   rG    = gregOfRexRM(pfx,modrm);
         UInt   rV    = getVexNvvvv(pfx);
         IRTemp eV    = newTemp(Ity_V128);
         IRTemp vV    = newTemp(Ity_V128);
         assign( vV, getXMMReg(rV) );
         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx,modrm);
            assign( eV, getXMMReg(rE) );
            delta += 1;
            DIP("vunpck%spd %s,%s\n", hi ? "h" : "l",
                nameXMMReg(rE), nameXMMReg(rG));
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            assign( eV, loadLE(Ity_V128, mkexpr(addr)) );
            delta += alen;
            DIP("vunpck%spd %s,%s\n", hi ? "h" : "l",
                dis_buf, nameXMMReg(rG));
         }
         IRTemp res = math_UNPCKxPD_128( eV, vV, hi );
         putYMMRegLoAndZU( rG, mkexpr(res) );
         *uses_vvvv = True;
         goto decode_success;
      }
      
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         Bool   hi    = opc == 0x15;
         UChar  modrm = getUChar(delta);
         UInt   rG    = gregOfRexRM(pfx,modrm);
         UInt   rV    = getVexNvvvv(pfx);
         IRTemp eV    = newTemp(Ity_V256);
         IRTemp vV    = newTemp(Ity_V256);
         assign( vV, getYMMReg(rV) );
         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx,modrm);
            assign( eV, getYMMReg(rE) );
            delta += 1;
            DIP("vunpck%spd %s,%s\n", hi ? "h" : "l",
                nameYMMReg(rE), nameYMMReg(rG));
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            assign( eV, loadLE(Ity_V256, mkexpr(addr)) );
            delta += alen;
            DIP("vunpck%spd %s,%s\n", hi ? "h" : "l",
                dis_buf, nameYMMReg(rG));
         }
         IRTemp res = math_UNPCKxPD_256( eV, vV, hi );
         putYMMReg( rG, mkexpr(res) );
         *uses_vvvv = True;
         goto decode_success;
      }
      break;

   case 0x16:
      
      
      if (haveNo66noF2noF3(pfx) && 0==getVexL(pfx)
          && epartIsReg(getUChar(delta))) {
         UChar modrm = getUChar(delta);
         UInt  rG    = gregOfRexRM(pfx, modrm);
         UInt  rE    = eregOfRexRM(pfx, modrm);
         UInt  rV    = getVexNvvvv(pfx);
         delta++;
         DIP("vmovlhps %s,%s,%s\n",
             nameXMMReg(rE), nameXMMReg(rV), nameXMMReg(rG));
         IRTemp res = newTemp(Ity_V128);
         assign(res, binop(Iop_64HLtoV128,
                           getXMMRegLane64(rE, 0),
                           getXMMRegLane64(rV, 0)));
         putYMMRegLoAndZU(rG, mkexpr(res));
         *uses_vvvv = True;
         goto decode_success;
      }
      
      
      
      
      if ((have66noF2noF3(pfx) || haveNo66noF2noF3(pfx))
          && 0==getVexL(pfx) && !epartIsReg(getUChar(delta))) {
         UChar modrm = getUChar(delta);
         UInt  rG    = gregOfRexRM(pfx, modrm);
         UInt  rV    = getVexNvvvv(pfx);
         addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
         delta += alen;
         DIP("vmovhp%c %s,%s,%s\n", have66(pfx) ? 'd' : 's',
             dis_buf, nameXMMReg(rV), nameXMMReg(rG));
         IRTemp res = newTemp(Ity_V128);
         assign(res, binop(Iop_64HLtoV128,
                           loadLE(Ity_I64, mkexpr(addr)),
                           getXMMRegLane64(rV, 0)));
         putYMMRegLoAndZU(rG, mkexpr(res));
         *uses_vvvv = True;
         goto decode_success;
      }
      
      if (haveF3no66noF2(pfx) && 0==getVexL(pfx)) {
         delta = dis_MOVSxDUP_128( vbi, pfx, delta, True,
                                   False );
         goto decode_success;
      }
      
      if (haveF3no66noF2(pfx) && 1==getVexL(pfx)) {
         delta = dis_MOVSxDUP_256( vbi, pfx, delta, False );
         goto decode_success;
      }
      break;

   case 0x17:
      
      
      
      
      if ((have66noF2noF3(pfx) || haveNo66noF2noF3(pfx))
          && 0==getVexL(pfx) && !epartIsReg(getUChar(delta))) {
         UChar modrm = getUChar(delta);
         UInt  rG    = gregOfRexRM(pfx, modrm);
         addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
         delta += alen;
         storeLE( mkexpr(addr), getXMMRegLane64( rG, 1));
         DIP("vmovhp%c %s,%s\n", have66(pfx) ? 'd' : 's',
             nameXMMReg(rG), dis_buf);
         goto decode_success;
      }
      break;

   case 0x28:
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         UChar modrm = getUChar(delta);
         UInt  rG    = gregOfRexRM(pfx, modrm);
         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx,modrm);
            putYMMRegLoAndZU( rG, getXMMReg( rE ));
            DIP("vmovapd %s,%s\n", nameXMMReg(rE), nameXMMReg(rG));
            delta += 1;
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            gen_SEGV_if_not_16_aligned( addr );
            putYMMRegLoAndZU( rG, loadLE(Ity_V128, mkexpr(addr)) );
            DIP("vmovapd %s,%s\n", dis_buf, nameXMMReg(rG));
            delta += alen;
         }
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         UChar modrm = getUChar(delta);
         UInt  rG    = gregOfRexRM(pfx, modrm);
         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx,modrm);
            putYMMReg( rG, getYMMReg( rE ));
            DIP("vmovapd %s,%s\n", nameYMMReg(rE), nameYMMReg(rG));
            delta += 1;
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            gen_SEGV_if_not_32_aligned( addr );
            putYMMReg( rG, loadLE(Ity_V256, mkexpr(addr)) );
            DIP("vmovapd %s,%s\n", dis_buf, nameYMMReg(rG));
            delta += alen;
         }
         goto decode_success;
      }
      
      if (haveNo66noF2noF3(pfx) && 0==getVexL(pfx)) {
         UChar modrm = getUChar(delta);
         UInt  rG    = gregOfRexRM(pfx, modrm);
         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx,modrm);
            putYMMRegLoAndZU( rG, getXMMReg( rE ));
            DIP("vmovaps %s,%s\n", nameXMMReg(rE), nameXMMReg(rG));
            delta += 1;
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            gen_SEGV_if_not_16_aligned( addr );
            putYMMRegLoAndZU( rG, loadLE(Ity_V128, mkexpr(addr)) );
            DIP("vmovaps %s,%s\n", dis_buf, nameXMMReg(rG));
            delta += alen;
         }
         goto decode_success;
      }
      
      if (haveNo66noF2noF3(pfx) && 1==getVexL(pfx)) {
         UChar modrm = getUChar(delta);
         UInt  rG    = gregOfRexRM(pfx, modrm);
         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx,modrm);
            putYMMReg( rG, getYMMReg( rE ));
            DIP("vmovaps %s,%s\n", nameYMMReg(rE), nameYMMReg(rG));
            delta += 1;
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            gen_SEGV_if_not_32_aligned( addr );
            putYMMReg( rG, loadLE(Ity_V256, mkexpr(addr)) );
            DIP("vmovaps %s,%s\n", dis_buf, nameYMMReg(rG));
            delta += alen;
         }
         goto decode_success;
      }
      break;

   case 0x29:
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         UChar modrm = getUChar(delta);
         UInt  rG    = gregOfRexRM(pfx,modrm);
         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx,modrm);
            putYMMRegLoAndZU( rE, getXMMReg(rG) );
            DIP("vmovapd %s,%s\n", nameXMMReg(rG), nameXMMReg(rE));
            delta += 1;
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            gen_SEGV_if_not_16_aligned( addr );
            storeLE( mkexpr(addr), getXMMReg(rG) );
            DIP("vmovapd %s,%s\n", nameXMMReg(rG), dis_buf );
            delta += alen;
         }
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         UChar modrm = getUChar(delta);
         UInt  rG    = gregOfRexRM(pfx,modrm);
         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx,modrm);
            putYMMReg( rE, getYMMReg(rG) );
            DIP("vmovapd %s,%s\n", nameYMMReg(rG), nameYMMReg(rE));
            delta += 1;
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            gen_SEGV_if_not_32_aligned( addr );
            storeLE( mkexpr(addr), getYMMReg(rG) );
            DIP("vmovapd %s,%s\n", nameYMMReg(rG), dis_buf );
            delta += alen;
         }
         goto decode_success;
      }
      
      if (haveNo66noF2noF3(pfx) && 0==getVexL(pfx)) {
         UChar modrm = getUChar(delta);
         UInt  rG    = gregOfRexRM(pfx,modrm);
         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx,modrm);
            putYMMRegLoAndZU( rE, getXMMReg(rG) );
            DIP("vmovaps %s,%s\n", nameXMMReg(rG), nameXMMReg(rE));
            delta += 1;
            goto decode_success;
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            gen_SEGV_if_not_16_aligned( addr );
            storeLE( mkexpr(addr), getXMMReg(rG) );
            DIP("vmovaps %s,%s\n", nameXMMReg(rG), dis_buf );
            delta += alen;
            goto decode_success;
         }
      }
      
      if (haveNo66noF2noF3(pfx) && 1==getVexL(pfx)) {
         UChar modrm = getUChar(delta);
         UInt  rG    = gregOfRexRM(pfx,modrm);
         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx,modrm);
            putYMMReg( rE, getYMMReg(rG) );
            DIP("vmovaps %s,%s\n", nameYMMReg(rG), nameYMMReg(rE));
            delta += 1;
            goto decode_success;
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            gen_SEGV_if_not_32_aligned( addr );
            storeLE( mkexpr(addr), getYMMReg(rG) );
            DIP("vmovaps %s,%s\n", nameYMMReg(rG), dis_buf );
            delta += alen;
            goto decode_success;
         }
      }
      break;

   case 0x2A: {
      IRTemp rmode = newTemp(Ity_I32);
      assign( rmode, get_sse_roundingmode() );
      
      if (haveF2no66noF3(pfx) && 0==getRexW(pfx)) {
         UChar  modrm = getUChar(delta);
         UInt   rV    = getVexNvvvv(pfx);
         UInt   rD    = gregOfRexRM(pfx, modrm);
         IRTemp arg32 = newTemp(Ity_I32);
         if (epartIsReg(modrm)) {
            UInt rS = eregOfRexRM(pfx,modrm);
            assign( arg32, getIReg32(rS) );
            delta += 1;
            DIP("vcvtsi2sdl %s,%s,%s\n",
                nameIReg32(rS), nameXMMReg(rV), nameXMMReg(rD));
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            assign( arg32, loadLE(Ity_I32, mkexpr(addr)) );
            delta += alen;
            DIP("vcvtsi2sdl %s,%s,%s\n",
                dis_buf, nameXMMReg(rV), nameXMMReg(rD));
         }
         putXMMRegLane64F( rD, 0,
                           unop(Iop_I32StoF64, mkexpr(arg32)));
         putXMMRegLane64( rD, 1, getXMMRegLane64( rV, 1 ));
         putYMMRegLane128( rD, 1, mkV128(0) );
         *uses_vvvv = True;
         goto decode_success;
      }
      
      if (haveF2no66noF3(pfx) && 1==getRexW(pfx)) {
         UChar  modrm = getUChar(delta);
         UInt   rV    = getVexNvvvv(pfx);
         UInt   rD    = gregOfRexRM(pfx, modrm);
         IRTemp arg64 = newTemp(Ity_I64);
         if (epartIsReg(modrm)) {
            UInt rS = eregOfRexRM(pfx,modrm);
            assign( arg64, getIReg64(rS) );
            delta += 1;
            DIP("vcvtsi2sdq %s,%s,%s\n",
                nameIReg64(rS), nameXMMReg(rV), nameXMMReg(rD));
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            assign( arg64, loadLE(Ity_I64, mkexpr(addr)) );
            delta += alen;
            DIP("vcvtsi2sdq %s,%s,%s\n",
                dis_buf, nameXMMReg(rV), nameXMMReg(rD));
         }
         putXMMRegLane64F( rD, 0,
                           binop( Iop_I64StoF64,
                                  get_sse_roundingmode(),
                                  mkexpr(arg64)) );
         putXMMRegLane64( rD, 1, getXMMRegLane64( rV, 1 ));
         putYMMRegLane128( rD, 1, mkV128(0) );
         *uses_vvvv = True;
         goto decode_success;
      }
      
      if (haveF3no66noF2(pfx) && 1==getRexW(pfx)) {
         UChar  modrm = getUChar(delta);
         UInt   rV    = getVexNvvvv(pfx);
         UInt   rD    = gregOfRexRM(pfx, modrm);
         IRTemp arg64 = newTemp(Ity_I64);
         if (epartIsReg(modrm)) {
            UInt rS = eregOfRexRM(pfx,modrm);
            assign( arg64, getIReg64(rS) );
            delta += 1;
            DIP("vcvtsi2ssq %s,%s,%s\n",
                nameIReg64(rS), nameXMMReg(rV), nameXMMReg(rD));
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            assign( arg64, loadLE(Ity_I64, mkexpr(addr)) );
            delta += alen;
            DIP("vcvtsi2ssq %s,%s,%s\n",
                dis_buf, nameXMMReg(rV), nameXMMReg(rD));
         }
         putXMMRegLane32F( rD, 0,
                           binop(Iop_F64toF32,
                                 mkexpr(rmode),
                                 binop(Iop_I64StoF64, mkexpr(rmode),
                                                      mkexpr(arg64)) ) );
         putXMMRegLane32( rD, 1, getXMMRegLane32( rV, 1 ));
         putXMMRegLane64( rD, 1, getXMMRegLane64( rV, 1 ));
         putYMMRegLane128( rD, 1, mkV128(0) );
         *uses_vvvv = True;
         goto decode_success;
      }
      
      if (haveF3no66noF2(pfx) && 0==getRexW(pfx)) {
         UChar  modrm = getUChar(delta);
         UInt   rV    = getVexNvvvv(pfx);
         UInt   rD    = gregOfRexRM(pfx, modrm);
         IRTemp arg32 = newTemp(Ity_I32);
         if (epartIsReg(modrm)) {
            UInt rS = eregOfRexRM(pfx,modrm);
            assign( arg32, getIReg32(rS) );
            delta += 1;
            DIP("vcvtsi2ssl %s,%s,%s\n",
                nameIReg32(rS), nameXMMReg(rV), nameXMMReg(rD));
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            assign( arg32, loadLE(Ity_I32, mkexpr(addr)) );
            delta += alen;
            DIP("vcvtsi2ssl %s,%s,%s\n",
                dis_buf, nameXMMReg(rV), nameXMMReg(rD));
         }
         putXMMRegLane32F( rD, 0,
                           binop(Iop_F64toF32,
                                 mkexpr(rmode),
                                 unop(Iop_I32StoF64, mkexpr(arg32)) ) );
         putXMMRegLane32( rD, 1, getXMMRegLane32( rV, 1 ));
         putXMMRegLane64( rD, 1, getXMMRegLane64( rV, 1 ));
         putYMMRegLane128( rD, 1, mkV128(0) );
         *uses_vvvv = True;
         goto decode_success;
      }
      break;
   }

   case 0x2B:
      
      
      if ((have66noF2noF3(pfx) || haveNo66noF2noF3(pfx))
          && 0==getVexL(pfx) && !epartIsReg(getUChar(delta))) {
         UChar  modrm = getUChar(delta);
         UInt   rS    = gregOfRexRM(pfx, modrm);
         IRTemp tS    = newTemp(Ity_V128);
         assign(tS, getXMMReg(rS));
         addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 0 );
         delta += alen;
         gen_SEGV_if_not_16_aligned(addr);
         storeLE(mkexpr(addr), mkexpr(tS));
         DIP("vmovntp%c %s,%s\n", have66(pfx) ? 'd' : 's',
             nameXMMReg(rS), dis_buf);
         goto decode_success;
      }
      
      
      if ((have66noF2noF3(pfx) || haveNo66noF2noF3(pfx))
          && 1==getVexL(pfx) && !epartIsReg(getUChar(delta))) {
         UChar  modrm = getUChar(delta);
         UInt   rS    = gregOfRexRM(pfx, modrm);
         IRTemp tS    = newTemp(Ity_V256);
         assign(tS, getYMMReg(rS));
         addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 0 );
         delta += alen;
         gen_SEGV_if_not_32_aligned(addr);
         storeLE(mkexpr(addr), mkexpr(tS));
         DIP("vmovntp%c %s,%s\n", have66(pfx) ? 'd' : 's',
             nameYMMReg(rS), dis_buf);
         goto decode_success;
      }
      break;

   case 0x2C:
      
      if (haveF2no66noF3(pfx) && 0==getRexW(pfx)) {
         delta = dis_CVTxSD2SI( vbi, pfx, delta, True, opc, 4);
         goto decode_success;
      }
      
      if (haveF2no66noF3(pfx) && 1==getRexW(pfx)) {
         delta = dis_CVTxSD2SI( vbi, pfx, delta, True, opc, 8);
         goto decode_success;
      }
      
      if (haveF3no66noF2(pfx) && 0==getRexW(pfx)) {
         delta = dis_CVTxSS2SI( vbi, pfx, delta, True, opc, 4);
         goto decode_success;
      }
      
      if (haveF3no66noF2(pfx) && 1==getRexW(pfx)) {
         delta = dis_CVTxSS2SI( vbi, pfx, delta, True, opc, 8);
         goto decode_success;
      }
      break;

   case 0x2D:
      
      if (haveF2no66noF3(pfx) && 0==getRexW(pfx)) {
         delta = dis_CVTxSD2SI( vbi, pfx, delta, True, opc, 4);
         goto decode_success;
      }
      
      if (haveF2no66noF3(pfx) && 1==getRexW(pfx)) {
         delta = dis_CVTxSD2SI( vbi, pfx, delta, True, opc, 8);
         goto decode_success;
      }
      
      if (haveF3no66noF2(pfx) && 0==getRexW(pfx)) {
         delta = dis_CVTxSS2SI( vbi, pfx, delta, True, opc, 4);
         goto decode_success;
      }
      
      if (haveF3no66noF2(pfx) && 1==getRexW(pfx)) {
         delta = dis_CVTxSS2SI( vbi, pfx, delta, True, opc, 8);
         goto decode_success;
      }
      break;

   case 0x2E:
   case 0x2F:
      
      
      if (have66noF2noF3(pfx)) {
         delta = dis_COMISD( vbi, pfx, delta, True, opc );
         goto decode_success;
      }
      
      
      if (haveNo66noF2noF3(pfx)) {
         delta = dis_COMISS( vbi, pfx, delta, True, opc );
         goto decode_success;
      }
      break;

   case 0x50:
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_MOVMSKPD_128( vbi, pfx, delta, True );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_MOVMSKPD_256( vbi, pfx, delta );
         goto decode_success;
      }
      
      if (haveNo66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_MOVMSKPS_128( vbi, pfx, delta, True );
         goto decode_success;
      }
      
      if (haveNo66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_MOVMSKPS_256( vbi, pfx, delta );
         goto decode_success;
      }
      break;

   case 0x51:
      
      if (haveF3no66noF2(pfx)) {
         delta = dis_AVX128_E_V_to_G_lo32_unary(
                    uses_vvvv, vbi, pfx, delta, "vsqrtss", Iop_Sqrt32F0x4 );
         goto decode_success;
      }
      
      if (haveNo66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_AVX128_E_to_G_unary_all(
                    uses_vvvv, vbi, pfx, delta, "vsqrtps", Iop_Sqrt32Fx4 );
         goto decode_success;
      }
      
      if (haveNo66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_AVX256_E_to_G_unary_all(
                    uses_vvvv, vbi, pfx, delta, "vsqrtps", Iop_Sqrt32Fx8 );
         goto decode_success;
      }
      
      if (haveF2no66noF3(pfx)) {
         delta = dis_AVX128_E_V_to_G_lo64_unary(
                    uses_vvvv, vbi, pfx, delta, "vsqrtsd", Iop_Sqrt64F0x2 );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_AVX128_E_to_G_unary_all(
                    uses_vvvv, vbi, pfx, delta, "vsqrtpd", Iop_Sqrt64Fx2 );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_AVX256_E_to_G_unary_all(
                    uses_vvvv, vbi, pfx, delta, "vsqrtpd", Iop_Sqrt64Fx4 );
         goto decode_success;
      }
      break;

   case 0x52:
      
      if (haveF3no66noF2(pfx)) {
         delta = dis_AVX128_E_V_to_G_lo32_unary(
                    uses_vvvv, vbi, pfx, delta, "vrsqrtss",
                    Iop_RSqrtEst32F0x4 );
         goto decode_success;
      }
      
      if (haveNo66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_AVX128_E_to_G_unary_all(
                    uses_vvvv, vbi, pfx, delta, "vrsqrtps", Iop_RSqrtEst32Fx4 );
         goto decode_success;
      }
      
      if (haveNo66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_AVX256_E_to_G_unary_all(
                    uses_vvvv, vbi, pfx, delta, "vrsqrtps", Iop_RSqrtEst32Fx8 );
         goto decode_success;
      }
      break;

   case 0x53:
      
      if (haveF3no66noF2(pfx)) {
         delta = dis_AVX128_E_V_to_G_lo32_unary(
                    uses_vvvv, vbi, pfx, delta, "vrcpss", Iop_RecipEst32F0x4 );
         goto decode_success;
      }
      
      if (haveNo66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_AVX128_E_to_G_unary_all(
                    uses_vvvv, vbi, pfx, delta, "vrcpps", Iop_RecipEst32Fx4 );
         goto decode_success;
      }
      
      if (haveNo66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_AVX256_E_to_G_unary_all(
                    uses_vvvv, vbi, pfx, delta, "vrcpps", Iop_RecipEst32Fx8 );
         goto decode_success;
      }
      break;

   case 0x54:
      
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_VEX_NDS_128_AnySimdPfx_0F_WIG_simple(
                    uses_vvvv, vbi, pfx, delta, "vandpd", Iop_AndV128 );
         goto decode_success;
      }
      
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_AVX256_E_V_to_G(
                    uses_vvvv, vbi, pfx, delta, "vandpd", Iop_AndV256 );
         goto decode_success;
      }
      
      if (haveNo66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_VEX_NDS_128_AnySimdPfx_0F_WIG_simple(
                    uses_vvvv, vbi, pfx, delta, "vandps", Iop_AndV128 );
         goto decode_success;
      }
      
      if (haveNo66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_AVX256_E_V_to_G(
                    uses_vvvv, vbi, pfx, delta, "vandps", Iop_AndV256 );
         goto decode_success;
      }
      break;

   case 0x55:
      
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_VEX_NDS_128_AnySimdPfx_0F_WIG(
                    uses_vvvv, vbi, pfx, delta, "vandpd", Iop_AndV128,
                    NULL, True, False );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_VEX_NDS_256_AnySimdPfx_0F_WIG(
                    uses_vvvv, vbi, pfx, delta, "vandpd", Iop_AndV256,
                    NULL, True, False );
         goto decode_success;
      }
      
      if (haveNo66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_VEX_NDS_128_AnySimdPfx_0F_WIG(
                    uses_vvvv, vbi, pfx, delta, "vandps", Iop_AndV128,
                    NULL, True, False );
         goto decode_success;
      }
      
      if (haveNo66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_VEX_NDS_256_AnySimdPfx_0F_WIG(
                    uses_vvvv, vbi, pfx, delta, "vandps", Iop_AndV256,
                    NULL, True, False );
         goto decode_success;
      }
      break;

   case 0x56:
      
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_VEX_NDS_128_AnySimdPfx_0F_WIG_simple(
                    uses_vvvv, vbi, pfx, delta, "vorpd", Iop_OrV128 );
         goto decode_success;
      }
      
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_AVX256_E_V_to_G(
                    uses_vvvv, vbi, pfx, delta, "vorpd", Iop_OrV256 );
         goto decode_success;
      }
      
      
      if (haveNo66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_VEX_NDS_128_AnySimdPfx_0F_WIG_simple(
                    uses_vvvv, vbi, pfx, delta, "vorps", Iop_OrV128 );
         goto decode_success;
      }
      
      
      if (haveNo66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_AVX256_E_V_to_G(
                    uses_vvvv, vbi, pfx, delta, "vorps", Iop_OrV256 );
         goto decode_success;
      }
      break;

   case 0x57:
      
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_VEX_NDS_128_AnySimdPfx_0F_WIG_simple(
                    uses_vvvv, vbi, pfx, delta, "vxorpd", Iop_XorV128 );
         goto decode_success;
      }
      
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_AVX256_E_V_to_G(
                    uses_vvvv, vbi, pfx, delta, "vxorpd", Iop_XorV256 );
         goto decode_success;
      }
      
      
      if (haveNo66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_VEX_NDS_128_AnySimdPfx_0F_WIG_simple(
                    uses_vvvv, vbi, pfx, delta, "vxorps", Iop_XorV128 );
         goto decode_success;
      }
      
      
      if (haveNo66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_AVX256_E_V_to_G(
                    uses_vvvv, vbi, pfx, delta, "vxorps", Iop_XorV256 );
         goto decode_success;
      }
      break;

   case 0x58:
      
      if (haveF2no66noF3(pfx)) {
         delta = dis_AVX128_E_V_to_G_lo64(
                    uses_vvvv, vbi, pfx, delta, "vaddsd", Iop_Add64F0x2 );
         goto decode_success;
      }
      
      if (haveF3no66noF2(pfx)) {
         delta = dis_AVX128_E_V_to_G_lo32(
                    uses_vvvv, vbi, pfx, delta, "vaddss", Iop_Add32F0x4 );
         goto decode_success;
      }
      
      if (haveNo66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_AVX128_E_V_to_G(
                    uses_vvvv, vbi, pfx, delta, "vaddps", Iop_Add32Fx4 );
         goto decode_success;
      }
      
      if (haveNo66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_AVX256_E_V_to_G(
                    uses_vvvv, vbi, pfx, delta, "vaddps", Iop_Add32Fx8 );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_AVX128_E_V_to_G(
                    uses_vvvv, vbi, pfx, delta, "vaddpd", Iop_Add64Fx2 );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_AVX256_E_V_to_G(
                    uses_vvvv, vbi, pfx, delta, "vaddpd", Iop_Add64Fx4 );
         goto decode_success;
      }
      break;

   case 0x59:
      
      if (haveF2no66noF3(pfx)) {
         delta = dis_AVX128_E_V_to_G_lo64(
                    uses_vvvv, vbi, pfx, delta, "vmulsd", Iop_Mul64F0x2 );
         goto decode_success;
      }
      
      if (haveF3no66noF2(pfx)) {
         delta = dis_AVX128_E_V_to_G_lo32(
                    uses_vvvv, vbi, pfx, delta, "vmulss", Iop_Mul32F0x4 );
         goto decode_success;
      }
      
      if (haveNo66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_AVX128_E_V_to_G(
                    uses_vvvv, vbi, pfx, delta, "vmulps", Iop_Mul32Fx4 );
         goto decode_success;
      }
      
      if (haveNo66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_AVX256_E_V_to_G(
                    uses_vvvv, vbi, pfx, delta, "vmulps", Iop_Mul32Fx8 );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_AVX128_E_V_to_G(
                    uses_vvvv, vbi, pfx, delta, "vmulpd", Iop_Mul64Fx2 );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_AVX256_E_V_to_G(
                    uses_vvvv, vbi, pfx, delta, "vmulpd", Iop_Mul64Fx4 );
         goto decode_success;
      }
      break;

   case 0x5A:
      
      if (haveNo66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_CVTPS2PD_128( vbi, pfx, delta, True );
         goto decode_success;
      }
      
      if (haveNo66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_CVTPS2PD_256( vbi, pfx, delta );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_CVTPD2PS_128( vbi, pfx, delta, True );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_CVTPD2PS_256( vbi, pfx, delta );
         goto decode_success;
      }
      
      if (haveF2no66noF3(pfx)) {
         UChar  modrm = getUChar(delta);
         UInt   rV    = getVexNvvvv(pfx);
         UInt   rD    = gregOfRexRM(pfx, modrm);
         IRTemp f64lo = newTemp(Ity_F64);
         IRTemp rmode = newTemp(Ity_I32);
         assign( rmode, get_sse_roundingmode() );
         if (epartIsReg(modrm)) {
            UInt rS = eregOfRexRM(pfx,modrm);
            assign(f64lo, getXMMRegLane64F(rS, 0));
            delta += 1;
            DIP("vcvtsd2ss %s,%s,%s\n",
                nameXMMReg(rS), nameXMMReg(rV), nameXMMReg(rD));
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            assign(f64lo, loadLE(Ity_F64, mkexpr(addr)) );
            delta += alen;
            DIP("vcvtsd2ss %s,%s,%s\n",
                dis_buf, nameXMMReg(rV), nameXMMReg(rD));
         }
         putXMMRegLane32F( rD, 0,
                           binop( Iop_F64toF32, mkexpr(rmode),
                                                mkexpr(f64lo)) );
         putXMMRegLane32( rD, 1, getXMMRegLane32( rV, 1 ));
         putXMMRegLane64( rD, 1, getXMMRegLane64( rV, 1 ));
         putYMMRegLane128( rD, 1, mkV128(0) );
         *uses_vvvv = True;
         goto decode_success;
      }
      
      if (haveF3no66noF2(pfx)) {
         UChar  modrm = getUChar(delta);
         UInt   rV    = getVexNvvvv(pfx);
         UInt   rD    = gregOfRexRM(pfx, modrm);
         IRTemp f32lo = newTemp(Ity_F32);
         if (epartIsReg(modrm)) {
            UInt rS = eregOfRexRM(pfx,modrm);
            assign(f32lo, getXMMRegLane32F(rS, 0));
            delta += 1;
            DIP("vcvtss2sd %s,%s,%s\n",
                nameXMMReg(rS), nameXMMReg(rV), nameXMMReg(rD));
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            assign(f32lo, loadLE(Ity_F32, mkexpr(addr)) );
            delta += alen;
            DIP("vcvtss2sd %s,%s,%s\n",
                dis_buf, nameXMMReg(rV), nameXMMReg(rD));
         }
         putXMMRegLane64F( rD, 0,
                           unop( Iop_F32toF64, mkexpr(f32lo)) );
         putXMMRegLane64( rD, 1, getXMMRegLane64( rV, 1 ));
         putYMMRegLane128( rD, 1, mkV128(0) );
         *uses_vvvv = True;
         goto decode_success;
      }
      break;

   case 0x5B:
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_CVTxPS2DQ_128( vbi, pfx, delta,
                                    True, False );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_CVTxPS2DQ_256( vbi, pfx, delta,
                                    False );
         goto decode_success;
      }
      
      if (haveF3no66noF2(pfx) && 0==getVexL(pfx)) {
         delta = dis_CVTxPS2DQ_128( vbi, pfx, delta,
                                    True, True );
         goto decode_success;
      }
      
      if (haveF3no66noF2(pfx) && 1==getVexL(pfx)) {
         delta = dis_CVTxPS2DQ_256( vbi, pfx, delta,
                                    True );
         goto decode_success;
      }
      
      if (haveNo66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_CVTDQ2PS_128 ( vbi, pfx, delta, True );
         goto decode_success;
      }
      
      if (haveNo66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_CVTDQ2PS_256 ( vbi, pfx, delta );
         goto decode_success;
      }
      break;

   case 0x5C:
      
      if (haveF2no66noF3(pfx)) {
         delta = dis_AVX128_E_V_to_G_lo64(
                    uses_vvvv, vbi, pfx, delta, "vsubsd", Iop_Sub64F0x2 );
         goto decode_success;
      }
      
      if (haveF3no66noF2(pfx)) {
         delta = dis_AVX128_E_V_to_G_lo32(
                    uses_vvvv, vbi, pfx, delta, "vsubss", Iop_Sub32F0x4 );
         goto decode_success;
      }
      
      if (haveNo66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_AVX128_E_V_to_G(
                    uses_vvvv, vbi, pfx, delta, "vsubps", Iop_Sub32Fx4 );
         goto decode_success;
      }
      
      if (haveNo66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_AVX256_E_V_to_G(
                    uses_vvvv, vbi, pfx, delta, "vsubps", Iop_Sub32Fx8 );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_AVX128_E_V_to_G(
                    uses_vvvv, vbi, pfx, delta, "vsubpd", Iop_Sub64Fx2 );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_AVX256_E_V_to_G(
                    uses_vvvv, vbi, pfx, delta, "vsubpd", Iop_Sub64Fx4 );
         goto decode_success;
      }
      break;

   case 0x5D:
      
      if (haveF2no66noF3(pfx)) {
         delta = dis_AVX128_E_V_to_G_lo64(
                    uses_vvvv, vbi, pfx, delta, "vminsd", Iop_Min64F0x2 );
         goto decode_success;
      }
      
      if (haveF3no66noF2(pfx)) {
         delta = dis_AVX128_E_V_to_G_lo32(
                    uses_vvvv, vbi, pfx, delta, "vminss", Iop_Min32F0x4 );
         goto decode_success;
      }
      
      if (haveNo66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_AVX128_E_V_to_G(
                    uses_vvvv, vbi, pfx, delta, "vminps", Iop_Min32Fx4 );
         goto decode_success;
      }
      
      if (haveNo66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_AVX256_E_V_to_G(
                    uses_vvvv, vbi, pfx, delta, "vminps", Iop_Min32Fx8 );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_AVX128_E_V_to_G(
                    uses_vvvv, vbi, pfx, delta, "vminpd", Iop_Min64Fx2 );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_AVX256_E_V_to_G(
                    uses_vvvv, vbi, pfx, delta, "vminpd", Iop_Min64Fx4 );
         goto decode_success;
      }
      break;

   case 0x5E:
      
      if (haveF2no66noF3(pfx)) {
         delta = dis_AVX128_E_V_to_G_lo64(
                    uses_vvvv, vbi, pfx, delta, "vdivsd", Iop_Div64F0x2 );
         goto decode_success;
      }
      
      if (haveF3no66noF2(pfx)) {
         delta = dis_AVX128_E_V_to_G_lo32(
                    uses_vvvv, vbi, pfx, delta, "vdivss", Iop_Div32F0x4 );
         goto decode_success;
      }
      
      if (haveNo66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_AVX128_E_V_to_G(
                    uses_vvvv, vbi, pfx, delta, "vdivps", Iop_Div32Fx4 );
         goto decode_success;
      }
      
      if (haveNo66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_AVX256_E_V_to_G(
                    uses_vvvv, vbi, pfx, delta, "vdivps", Iop_Div32Fx8 );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_AVX128_E_V_to_G(
                    uses_vvvv, vbi, pfx, delta, "vdivpd", Iop_Div64Fx2 );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_AVX256_E_V_to_G(
                    uses_vvvv, vbi, pfx, delta, "vdivpd", Iop_Div64Fx4 );
         goto decode_success;
      }
      break;

   case 0x5F:
      
      if (haveF2no66noF3(pfx)) {
         delta = dis_AVX128_E_V_to_G_lo64(
                    uses_vvvv, vbi, pfx, delta, "vmaxsd", Iop_Max64F0x2 );
         goto decode_success;
      }
      
      if (haveF3no66noF2(pfx)) {
         delta = dis_AVX128_E_V_to_G_lo32(
                    uses_vvvv, vbi, pfx, delta, "vmaxss", Iop_Max32F0x4 );
         goto decode_success;
      }
      
      if (haveNo66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_AVX128_E_V_to_G(
                    uses_vvvv, vbi, pfx, delta, "vmaxps", Iop_Max32Fx4 );
         goto decode_success;
      }
      
      if (haveNo66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_AVX256_E_V_to_G(
                    uses_vvvv, vbi, pfx, delta, "vmaxps", Iop_Max32Fx8 );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_AVX128_E_V_to_G(
                    uses_vvvv, vbi, pfx, delta, "vmaxpd", Iop_Max64Fx2 );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_AVX256_E_V_to_G(
                    uses_vvvv, vbi, pfx, delta, "vmaxpd", Iop_Max64Fx4 );
         goto decode_success;
      }
      break;

   case 0x60:
      
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_VEX_NDS_128_AnySimdPfx_0F_WIG(
                    uses_vvvv, vbi, pfx, delta, "vpunpcklbw",
                    Iop_InterleaveLO8x16, NULL,
                    False, True );
         goto decode_success;
      }
      
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_VEX_NDS_256_AnySimdPfx_0F_WIG_complex(
                    uses_vvvv, vbi, pfx, delta, "vpunpcklbw",
                    math_VPUNPCKLBW_YMM );
         goto decode_success;
      }
      break;

   case 0x61:
      
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_VEX_NDS_128_AnySimdPfx_0F_WIG(
                    uses_vvvv, vbi, pfx, delta, "vpunpcklwd",
                    Iop_InterleaveLO16x8, NULL,
                    False, True );
         goto decode_success;
      }
      
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_VEX_NDS_256_AnySimdPfx_0F_WIG_complex(
                    uses_vvvv, vbi, pfx, delta, "vpunpcklwd",
                    math_VPUNPCKLWD_YMM );
         goto decode_success;
      }
      break;

   case 0x62:
      
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_VEX_NDS_128_AnySimdPfx_0F_WIG(
                    uses_vvvv, vbi, pfx, delta, "vpunpckldq",
                    Iop_InterleaveLO32x4, NULL,
                    False, True );
         goto decode_success;
      }
      
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_VEX_NDS_256_AnySimdPfx_0F_WIG_complex(
                    uses_vvvv, vbi, pfx, delta, "vpunpckldq",
                    math_VPUNPCKLDQ_YMM );
         goto decode_success;
      }
      break;

   case 0x63:
      
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_VEX_NDS_128_AnySimdPfx_0F_WIG(
                    uses_vvvv, vbi, pfx, delta, "vpacksswb",
                    Iop_QNarrowBin16Sto8Sx16, NULL,
                    False, True );
         goto decode_success;
      }
      
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_VEX_NDS_256_AnySimdPfx_0F_WIG_complex(
                    uses_vvvv, vbi, pfx, delta, "vpacksswb",
                    math_VPACKSSWB_YMM );
         goto decode_success;
      }
      break;

   case 0x64:
      
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_VEX_NDS_128_AnySimdPfx_0F_WIG_simple(
                    uses_vvvv, vbi, pfx, delta, "vpcmpgtb", Iop_CmpGT8Sx16 );
         goto decode_success;
      }
      
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_VEX_NDS_256_AnySimdPfx_0F_WIG_simple(
                    uses_vvvv, vbi, pfx, delta, "vpcmpgtb", Iop_CmpGT8Sx32 );
         goto decode_success;
      }
      break;

   case 0x65:
      
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_VEX_NDS_128_AnySimdPfx_0F_WIG_simple(
                    uses_vvvv, vbi, pfx, delta, "vpcmpgtw", Iop_CmpGT16Sx8 );
         goto decode_success;
      }
      
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_VEX_NDS_256_AnySimdPfx_0F_WIG_simple(
                    uses_vvvv, vbi, pfx, delta, "vpcmpgtw", Iop_CmpGT16Sx16 );
         goto decode_success;
      }
      break;

   case 0x66:
      
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_VEX_NDS_128_AnySimdPfx_0F_WIG_simple(
                    uses_vvvv, vbi, pfx, delta, "vpcmpgtd", Iop_CmpGT32Sx4 );
         goto decode_success;
      }
      
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_VEX_NDS_256_AnySimdPfx_0F_WIG_simple(
                    uses_vvvv, vbi, pfx, delta, "vpcmpgtd", Iop_CmpGT32Sx8 );
         goto decode_success;
      }
      break;

   case 0x67:
      
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_VEX_NDS_128_AnySimdPfx_0F_WIG(
                    uses_vvvv, vbi, pfx, delta, "vpackuswb",
                    Iop_QNarrowBin16Sto8Ux16, NULL,
                    False, True );
         goto decode_success;
      }
      
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_VEX_NDS_256_AnySimdPfx_0F_WIG_complex(
                    uses_vvvv, vbi, pfx, delta, "vpackuswb",
                    math_VPACKUSWB_YMM );
         goto decode_success;
      }
      break;

   case 0x68:
      
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_VEX_NDS_128_AnySimdPfx_0F_WIG(
                    uses_vvvv, vbi, pfx, delta, "vpunpckhbw",
                    Iop_InterleaveHI8x16, NULL,
                    False, True );
         goto decode_success;
      }
      
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_VEX_NDS_256_AnySimdPfx_0F_WIG_complex(
                    uses_vvvv, vbi, pfx, delta, "vpunpckhbw",
                    math_VPUNPCKHBW_YMM );
         goto decode_success;
      }
      break;

   case 0x69:
      
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_VEX_NDS_128_AnySimdPfx_0F_WIG(
                    uses_vvvv, vbi, pfx, delta, "vpunpckhwd",
                    Iop_InterleaveHI16x8, NULL,
                    False, True );
         goto decode_success;
      }
      
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_VEX_NDS_256_AnySimdPfx_0F_WIG_complex(
                    uses_vvvv, vbi, pfx, delta, "vpunpckhwd",
                    math_VPUNPCKHWD_YMM );
         goto decode_success;
      }
      break;

   case 0x6A:
      
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_VEX_NDS_128_AnySimdPfx_0F_WIG(
                    uses_vvvv, vbi, pfx, delta, "vpunpckhdq",
                    Iop_InterleaveHI32x4, NULL,
                    False, True );
         goto decode_success;
      }
      
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_VEX_NDS_256_AnySimdPfx_0F_WIG_complex(
                    uses_vvvv, vbi, pfx, delta, "vpunpckhdq",
                    math_VPUNPCKHDQ_YMM );
         goto decode_success;
      }
      break;

   case 0x6B:
      
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_VEX_NDS_128_AnySimdPfx_0F_WIG(
                    uses_vvvv, vbi, pfx, delta, "vpackssdw",
                    Iop_QNarrowBin32Sto16Sx8, NULL,
                    False, True );
         goto decode_success;
      }
      
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_VEX_NDS_256_AnySimdPfx_0F_WIG_complex(
                    uses_vvvv, vbi, pfx, delta, "vpackssdw",
                    math_VPACKSSDW_YMM );
         goto decode_success;
      }
      break;

   case 0x6C:
      
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_VEX_NDS_128_AnySimdPfx_0F_WIG(
                    uses_vvvv, vbi, pfx, delta, "vpunpcklqdq",
                    Iop_InterleaveLO64x2, NULL,
                    False, True );
         goto decode_success;
      }
      
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_VEX_NDS_256_AnySimdPfx_0F_WIG_complex(
                    uses_vvvv, vbi, pfx, delta, "vpunpcklqdq",
                    math_VPUNPCKLQDQ_YMM );
         goto decode_success;
      }
      break;

   case 0x6D:
      
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_VEX_NDS_128_AnySimdPfx_0F_WIG(
                    uses_vvvv, vbi, pfx, delta, "vpunpckhqdq",
                    Iop_InterleaveHI64x2, NULL,
                    False, True );
         goto decode_success;
      }
      
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_VEX_NDS_256_AnySimdPfx_0F_WIG_complex(
                    uses_vvvv, vbi, pfx, delta, "vpunpckhqdq",
                    math_VPUNPCKHQDQ_YMM );
         goto decode_success;
      }
      break;

   case 0x6E:
      
      if (have66noF2noF3(pfx)
          && 0==getVexL(pfx) && 0==getRexW(pfx)) {
         vassert(sz == 2); 
         UChar modrm = getUChar(delta);
         if (epartIsReg(modrm)) {
            delta += 1;
            putYMMRegLoAndZU(
               gregOfRexRM(pfx,modrm),
               unop( Iop_32UtoV128, getIReg32(eregOfRexRM(pfx,modrm)) ) 
            );
            DIP("vmovd %s, %s\n", nameIReg32(eregOfRexRM(pfx,modrm)), 
                                  nameXMMReg(gregOfRexRM(pfx,modrm)));
        } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 0 );
            delta += alen;
            putYMMRegLoAndZU(
               gregOfRexRM(pfx,modrm),
               unop( Iop_32UtoV128,loadLE(Ity_I32, mkexpr(addr)))
                             );
            DIP("vmovd %s, %s\n", dis_buf, 
                                  nameXMMReg(gregOfRexRM(pfx,modrm)));
         }
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx)
          && 0==getVexL(pfx) && 1==getRexW(pfx)) {
         vassert(sz == 2); 
         UChar modrm = getUChar(delta);
         if (epartIsReg(modrm)) {
            delta += 1;
            putYMMRegLoAndZU(
               gregOfRexRM(pfx,modrm),
               unop( Iop_64UtoV128, getIReg64(eregOfRexRM(pfx,modrm)) ) 
            );
            DIP("vmovq %s, %s\n", nameIReg64(eregOfRexRM(pfx,modrm)), 
                                  nameXMMReg(gregOfRexRM(pfx,modrm)));
        } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 0 );
            delta += alen;
            putYMMRegLoAndZU(
               gregOfRexRM(pfx,modrm),
               unop( Iop_64UtoV128,loadLE(Ity_I64, mkexpr(addr)))
                             );
            DIP("vmovq %s, %s\n", dis_buf, 
                                  nameXMMReg(gregOfRexRM(pfx,modrm)));
         }
         goto decode_success;
      }
      break;

   case 0x6F:
      
      
      if ((have66noF2noF3(pfx) || haveF3no66noF2(pfx))
          && 1==getVexL(pfx)) {
         UChar  modrm = getUChar(delta);
         UInt   rD    = gregOfRexRM(pfx, modrm);
         IRTemp tD    = newTemp(Ity_V256);
         Bool   isA   = have66noF2noF3(pfx);
         HChar  ch    = isA ? 'a' : 'u';
         if (epartIsReg(modrm)) {
            UInt rS = eregOfRexRM(pfx, modrm);
            delta += 1;
            assign(tD, getYMMReg(rS));
            DIP("vmovdq%c %s,%s\n", ch, nameYMMReg(rS), nameYMMReg(rD));
         } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 0 );
            delta += alen;
            if (isA)
               gen_SEGV_if_not_32_aligned(addr);
            assign(tD, loadLE(Ity_V256, mkexpr(addr)));
            DIP("vmovdq%c %s,%s\n", ch, dis_buf, nameYMMReg(rD));
         }
         putYMMReg(rD, mkexpr(tD));
         goto decode_success;
      }
      
      
      if ((have66noF2noF3(pfx) || haveF3no66noF2(pfx))
          && 0==getVexL(pfx)) {
         UChar  modrm = getUChar(delta);
         UInt   rD    = gregOfRexRM(pfx, modrm);
         IRTemp tD    = newTemp(Ity_V128);
         Bool   isA   = have66noF2noF3(pfx);
         HChar  ch    = isA ? 'a' : 'u';
         if (epartIsReg(modrm)) {
            UInt rS = eregOfRexRM(pfx, modrm);
            delta += 1;
            assign(tD, getXMMReg(rS));
            DIP("vmovdq%c %s,%s\n", ch, nameXMMReg(rS), nameXMMReg(rD));
         } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 0 );
            delta += alen;
            if (isA)
               gen_SEGV_if_not_16_aligned(addr);
            assign(tD, loadLE(Ity_V128, mkexpr(addr)));
            DIP("vmovdq%c %s,%s\n", ch, dis_buf, nameXMMReg(rD));
         }
         putYMMRegLoAndZU(rD, mkexpr(tD));
         goto decode_success;
      }
      break;

   case 0x70:
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_PSHUFD_32x4( vbi, pfx, delta, True);
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_PSHUFD_32x8( vbi, pfx, delta);
         goto decode_success;
      }
      
      if (haveF2no66noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_PSHUFxW_128( vbi, pfx, delta,
                                  True, False );
         goto decode_success;
      }
      
      if (haveF2no66noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_PSHUFxW_256( vbi, pfx, delta, False );
         goto decode_success;
      }
      
      if (haveF3no66noF2(pfx) && 0==getVexL(pfx)) {
         delta = dis_PSHUFxW_128( vbi, pfx, delta,
                                  True, True );
         goto decode_success;
      }
      
      if (haveF3no66noF2(pfx) && 1==getVexL(pfx)) {
         delta = dis_PSHUFxW_256( vbi, pfx, delta, True );
         goto decode_success;
      }
      break;

   case 0x71:
      
      
      
      if (have66noF2noF3(pfx)
          && 0==getVexL(pfx)
          && epartIsReg(getUChar(delta))) {
         if (gregLO3ofRM(getUChar(delta)) == 2) {
            delta = dis_AVX128_shiftE_to_V_imm( pfx, delta,
                                                "vpsrlw", Iop_ShrN16x8 );
            *uses_vvvv = True;
            goto decode_success;
         }
         if (gregLO3ofRM(getUChar(delta)) == 4) {
            delta = dis_AVX128_shiftE_to_V_imm( pfx, delta,
                                                "vpsraw", Iop_SarN16x8 );
            *uses_vvvv = True;
            goto decode_success;
         }
         if (gregLO3ofRM(getUChar(delta)) == 6) {
            delta = dis_AVX128_shiftE_to_V_imm( pfx, delta,
                                                "vpsllw", Iop_ShlN16x8 );
            *uses_vvvv = True;
            goto decode_success;
         }
         
      }
      
      
      
      if (have66noF2noF3(pfx)
          && 1==getVexL(pfx)
          && epartIsReg(getUChar(delta))) {
         if (gregLO3ofRM(getUChar(delta)) == 2) {
            delta = dis_AVX256_shiftE_to_V_imm( pfx, delta,
                                                "vpsrlw", Iop_ShrN16x16 );
            *uses_vvvv = True;
            goto decode_success;
         }
         if (gregLO3ofRM(getUChar(delta)) == 4) {
            delta = dis_AVX256_shiftE_to_V_imm( pfx, delta,
                                                "vpsraw", Iop_SarN16x16 );
            *uses_vvvv = True;
            goto decode_success;
         }
         if (gregLO3ofRM(getUChar(delta)) == 6) {
            delta = dis_AVX256_shiftE_to_V_imm( pfx, delta,
                                                "vpsllw", Iop_ShlN16x16 );
            *uses_vvvv = True;
            goto decode_success;
         }
         
      }
      break;

   case 0x72:
      
      
      
      if (have66noF2noF3(pfx)
          && 0==getVexL(pfx)
          && epartIsReg(getUChar(delta))) {
         if (gregLO3ofRM(getUChar(delta)) == 2) {
            delta = dis_AVX128_shiftE_to_V_imm( pfx, delta,
                                                "vpsrld", Iop_ShrN32x4 );
            *uses_vvvv = True;
            goto decode_success;
         }
         if (gregLO3ofRM(getUChar(delta)) == 4) {
            delta = dis_AVX128_shiftE_to_V_imm( pfx, delta,
                                                "vpsrad", Iop_SarN32x4 );
            *uses_vvvv = True;
            goto decode_success;
         }
         if (gregLO3ofRM(getUChar(delta)) == 6) {
            delta = dis_AVX128_shiftE_to_V_imm( pfx, delta,
                                                "vpslld", Iop_ShlN32x4 );
            *uses_vvvv = True;
            goto decode_success;
         }
         
      }
      
      
      
      if (have66noF2noF3(pfx)
          && 1==getVexL(pfx)
          && epartIsReg(getUChar(delta))) {
         if (gregLO3ofRM(getUChar(delta)) == 2) {
            delta = dis_AVX256_shiftE_to_V_imm( pfx, delta,
                                                "vpsrld", Iop_ShrN32x8 );
            *uses_vvvv = True;
            goto decode_success;
         }
         if (gregLO3ofRM(getUChar(delta)) == 4) {
            delta = dis_AVX256_shiftE_to_V_imm( pfx, delta,
                                                "vpsrad", Iop_SarN32x8 );
            *uses_vvvv = True;
            goto decode_success;
         }
         if (gregLO3ofRM(getUChar(delta)) == 6) {
            delta = dis_AVX256_shiftE_to_V_imm( pfx, delta,
                                                "vpslld", Iop_ShlN32x8 );
            *uses_vvvv = True;
            goto decode_success;
         }
         
      }
      break;

   case 0x73:
      
      
      
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)
          && epartIsReg(getUChar(delta))) {
         Int    rS   = eregOfRexRM(pfx,getUChar(delta));
         Int    rD   = getVexNvvvv(pfx);
         IRTemp vecS = newTemp(Ity_V128);
         if (gregLO3ofRM(getUChar(delta)) == 3) {
            Int imm = (Int)getUChar(delta+1);
            DIP("vpsrldq $%d,%s,%s\n", imm, nameXMMReg(rS), nameXMMReg(rD));
            delta += 2;
            assign( vecS, getXMMReg(rS) );
            putYMMRegLoAndZU(rD, mkexpr(math_PSRLDQ( vecS, imm )));
            *uses_vvvv = True;
            goto decode_success;
         }
         if (gregLO3ofRM(getUChar(delta)) == 7) {
            Int imm = (Int)getUChar(delta+1);
            DIP("vpslldq $%d,%s,%s\n", imm, nameXMMReg(rS), nameXMMReg(rD));
            delta += 2;
            assign( vecS, getXMMReg(rS) );
            putYMMRegLoAndZU(rD, mkexpr(math_PSLLDQ( vecS, imm )));
            *uses_vvvv = True;
            goto decode_success;
         }
         if (gregLO3ofRM(getUChar(delta)) == 2) {
            delta = dis_AVX128_shiftE_to_V_imm( pfx, delta,
                                                "vpsrlq", Iop_ShrN64x2 );
            *uses_vvvv = True;
            goto decode_success;
         }
         if (gregLO3ofRM(getUChar(delta)) == 6) {
            delta = dis_AVX128_shiftE_to_V_imm( pfx, delta,
                                                "vpsllq", Iop_ShlN64x2 );
            *uses_vvvv = True;
            goto decode_success;
         }
         
      }
      
      
      
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)
          && epartIsReg(getUChar(delta))) {
         Int    rS   = eregOfRexRM(pfx,getUChar(delta));
         Int    rD   = getVexNvvvv(pfx);
         if (gregLO3ofRM(getUChar(delta)) == 3) {
            IRTemp vecS0 = newTemp(Ity_V128);
            IRTemp vecS1 = newTemp(Ity_V128);
            Int imm = (Int)getUChar(delta+1);
            DIP("vpsrldq $%d,%s,%s\n", imm, nameYMMReg(rS), nameYMMReg(rD));
            delta += 2;
            assign( vecS0, getYMMRegLane128(rS, 0));
            assign( vecS1, getYMMRegLane128(rS, 1));
            putYMMRegLane128(rD, 0, mkexpr(math_PSRLDQ( vecS0, imm )));
            putYMMRegLane128(rD, 1, mkexpr(math_PSRLDQ( vecS1, imm )));
            *uses_vvvv = True;
            goto decode_success;
         }
         if (gregLO3ofRM(getUChar(delta)) == 7) {
            IRTemp vecS0 = newTemp(Ity_V128);
            IRTemp vecS1 = newTemp(Ity_V128);
            Int imm = (Int)getUChar(delta+1);
            DIP("vpslldq $%d,%s,%s\n", imm, nameYMMReg(rS), nameYMMReg(rD));
            delta += 2;
            assign( vecS0, getYMMRegLane128(rS, 0));
            assign( vecS1, getYMMRegLane128(rS, 1));
            putYMMRegLane128(rD, 0, mkexpr(math_PSLLDQ( vecS0, imm )));
            putYMMRegLane128(rD, 1, mkexpr(math_PSLLDQ( vecS1, imm )));
            *uses_vvvv = True;
            goto decode_success;
         }
         if (gregLO3ofRM(getUChar(delta)) == 2) {
            delta = dis_AVX256_shiftE_to_V_imm( pfx, delta,
                                                "vpsrlq", Iop_ShrN64x4 );
            *uses_vvvv = True;
            goto decode_success;
         }
         if (gregLO3ofRM(getUChar(delta)) == 6) {
            delta = dis_AVX256_shiftE_to_V_imm( pfx, delta,
                                                "vpsllq", Iop_ShlN64x4 );
            *uses_vvvv = True;
            goto decode_success;
         }
         
      }
      break;

   case 0x74:
      
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_VEX_NDS_128_AnySimdPfx_0F_WIG_simple(
                    uses_vvvv, vbi, pfx, delta, "vpcmpeqb", Iop_CmpEQ8x16 );
         goto decode_success;
      }
      
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_VEX_NDS_256_AnySimdPfx_0F_WIG_simple(
                    uses_vvvv, vbi, pfx, delta, "vpcmpeqb", Iop_CmpEQ8x32 );
         goto decode_success;
      }
      break;

   case 0x75:
      
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_VEX_NDS_128_AnySimdPfx_0F_WIG_simple(
                    uses_vvvv, vbi, pfx, delta, "vpcmpeqw", Iop_CmpEQ16x8 );
         goto decode_success;
      }
      
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_VEX_NDS_256_AnySimdPfx_0F_WIG_simple(
                    uses_vvvv, vbi, pfx, delta, "vpcmpeqw", Iop_CmpEQ16x16 );
         goto decode_success;
      }
      break;

   case 0x76:
      
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_VEX_NDS_128_AnySimdPfx_0F_WIG_simple(
                    uses_vvvv, vbi, pfx, delta, "vpcmpeqd", Iop_CmpEQ32x4 );
         goto decode_success;
      }
      
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_VEX_NDS_256_AnySimdPfx_0F_WIG_simple(
                    uses_vvvv, vbi, pfx, delta, "vpcmpeqd", Iop_CmpEQ32x8 );
         goto decode_success;
      }
      break;

   case 0x77:
      
      if (haveNo66noF2noF3(pfx) && 0==getVexL(pfx)) {
         Int i;
         IRTemp zero128 = newTemp(Ity_V128);
         assign(zero128, mkV128(0));
         for (i = 0; i < 16; i++) {
            putYMMRegLane128(i, 1, mkexpr(zero128));
         }
         DIP("vzeroupper\n");
         goto decode_success;
      }
      
      if (haveNo66noF2noF3(pfx) && 1==getVexL(pfx)) {
         Int i;
         IRTemp zero128 = newTemp(Ity_V128);
         assign(zero128, mkV128(0));
         for (i = 0; i < 16; i++) {
            putYMMRegLoAndZU(i, mkexpr(zero128));
         }
         DIP("vzeroall\n");
         goto decode_success;
      }
      break;

   case 0x7C:
   case 0x7D:
      
      
      if (haveF2no66noF3(pfx) && 0==getVexL(pfx)) {
         IRTemp sV     = newTemp(Ity_V128);
         IRTemp dV     = newTemp(Ity_V128);
         Bool   isAdd  = opc == 0x7C;
         const HChar* str = isAdd ? "add" : "sub";
         UChar modrm   = getUChar(delta);
         UInt   rG     = gregOfRexRM(pfx,modrm);
         UInt   rV     = getVexNvvvv(pfx);
         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx,modrm);
            assign( sV, getXMMReg(rE) );
            DIP("vh%spd %s,%s,%s\n", str, nameXMMReg(rE),
                nameXMMReg(rV), nameXMMReg(rG));
            delta += 1;
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            assign( sV, loadLE(Ity_V128, mkexpr(addr)) );
            DIP("vh%spd %s,%s,%s\n", str, dis_buf,
                nameXMMReg(rV), nameXMMReg(rG));
            delta += alen;
         }
         assign( dV, getXMMReg(rV) );
         putYMMRegLoAndZU( rG, mkexpr( math_HADDPS_128 ( dV, sV, isAdd ) ) );
         *uses_vvvv = True;
         goto decode_success;
      }
      
      
      if (haveF2no66noF3(pfx) && 1==getVexL(pfx)) {
         IRTemp sV     = newTemp(Ity_V256);
         IRTemp dV     = newTemp(Ity_V256);
         IRTemp s1, s0, d1, d0;
         Bool   isAdd  = opc == 0x7C;
         const HChar* str = isAdd ? "add" : "sub";
         UChar modrm   = getUChar(delta);
         UInt   rG     = gregOfRexRM(pfx,modrm);
         UInt   rV     = getVexNvvvv(pfx);
         s1 = s0 = d1 = d0 = IRTemp_INVALID;
         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx,modrm);
            assign( sV, getYMMReg(rE) );
            DIP("vh%spd %s,%s,%s\n", str, nameYMMReg(rE),
                nameYMMReg(rV), nameYMMReg(rG));
            delta += 1;
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            assign( sV, loadLE(Ity_V256, mkexpr(addr)) );
            DIP("vh%spd %s,%s,%s\n", str, dis_buf,
                nameYMMReg(rV), nameYMMReg(rG));
            delta += alen;
         }
         assign( dV, getYMMReg(rV) );
         breakupV256toV128s( dV, &d1, &d0 );
         breakupV256toV128s( sV, &s1, &s0 );
         putYMMReg( rG, binop(Iop_V128HLtoV256,
                              mkexpr( math_HADDPS_128 ( d1, s1, isAdd ) ),
                              mkexpr( math_HADDPS_128 ( d0, s0, isAdd ) ) ) );
         *uses_vvvv = True;
         goto decode_success;
      }
      
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         IRTemp sV     = newTemp(Ity_V128);
         IRTemp dV     = newTemp(Ity_V128);
         Bool   isAdd  = opc == 0x7C;
         const HChar* str = isAdd ? "add" : "sub";
         UChar modrm   = getUChar(delta);
         UInt   rG     = gregOfRexRM(pfx,modrm);
         UInt   rV     = getVexNvvvv(pfx);
         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx,modrm);
            assign( sV, getXMMReg(rE) );
            DIP("vh%spd %s,%s,%s\n", str, nameXMMReg(rE),
                nameXMMReg(rV), nameXMMReg(rG));
            delta += 1;
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            assign( sV, loadLE(Ity_V128, mkexpr(addr)) );
            DIP("vh%spd %s,%s,%s\n", str, dis_buf,
                nameXMMReg(rV), nameXMMReg(rG));
            delta += alen;
         }
         assign( dV, getXMMReg(rV) );
         putYMMRegLoAndZU( rG, mkexpr( math_HADDPD_128 ( dV, sV, isAdd ) ) );
         *uses_vvvv = True;
         goto decode_success;
      }
      
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         IRTemp sV     = newTemp(Ity_V256);
         IRTemp dV     = newTemp(Ity_V256);
         IRTemp s1, s0, d1, d0;
         Bool   isAdd  = opc == 0x7C;
         const HChar* str = isAdd ? "add" : "sub";
         UChar modrm   = getUChar(delta);
         UInt   rG     = gregOfRexRM(pfx,modrm);
         UInt   rV     = getVexNvvvv(pfx);
         s1 = s0 = d1 = d0 = IRTemp_INVALID;
         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx,modrm);
            assign( sV, getYMMReg(rE) );
            DIP("vh%spd %s,%s,%s\n", str, nameYMMReg(rE),
                nameYMMReg(rV), nameYMMReg(rG));
            delta += 1;
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            assign( sV, loadLE(Ity_V256, mkexpr(addr)) );
            DIP("vh%spd %s,%s,%s\n", str, dis_buf,
                nameYMMReg(rV), nameYMMReg(rG));
            delta += alen;
         }
         assign( dV, getYMMReg(rV) );
         breakupV256toV128s( dV, &d1, &d0 );
         breakupV256toV128s( sV, &s1, &s0 );
         putYMMReg( rG, binop(Iop_V128HLtoV256,
                              mkexpr( math_HADDPD_128 ( d1, s1, isAdd ) ),
                              mkexpr( math_HADDPD_128 ( d0, s0, isAdd ) ) ) );
         *uses_vvvv = True;
         goto decode_success;
      }
      break;

   case 0x7E:
      
      if (haveF3no66noF2(pfx) 
          && 0==getVexL(pfx) && 0==getRexW(pfx)) {
         vassert(sz == 4); 
         UChar modrm = getUChar(delta);
         UInt  rG    = gregOfRexRM(pfx,modrm);
         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx,modrm);
            putXMMRegLane64( rG, 0, getXMMRegLane64( rE, 0 ));
            DIP("vmovq %s,%s\n", nameXMMReg(rE), nameXMMReg(rG));
            delta += 1;
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            putXMMRegLane64( rG, 0, loadLE(Ity_I64, mkexpr(addr)) );
            DIP("vmovq %s,%s\n", dis_buf, nameXMMReg(rG));
            delta += alen;
         }
         
         putXMMRegLane64( rG, 1, mkU64(0) );
         putYMMRegLane128( rG, 1, mkV128(0) );
         goto decode_success;
      }
      
      
      
      if (have66noF2noF3(pfx)
          && 0==getVexL(pfx) && 1==getRexW(pfx)) {
         UChar modrm = getUChar(delta);
         UInt  rG    = gregOfRexRM(pfx,modrm);
         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx,modrm);
            DIP("vmovq %s,%s\n", nameXMMReg(rG), nameIReg64(rE));
            putIReg64(rE, getXMMRegLane64(rG, 0));
            delta += 1;
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            storeLE( mkexpr(addr), getXMMRegLane64(rG, 0) );
            DIP("vmovq %s,%s\n", dis_buf, nameXMMReg(rG));
            delta += alen;
         }
         goto decode_success;
      }
      
      
      if (have66noF2noF3(pfx)
          && 0==getVexL(pfx) && 0==getRexW(pfx)) {
         UChar modrm = getUChar(delta);
         UInt  rG    = gregOfRexRM(pfx,modrm);
         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx,modrm);
            DIP("vmovd %s,%s\n", nameXMMReg(rG), nameIReg32(rE));
            putIReg32(rE, getXMMRegLane32(rG, 0));
            delta += 1;
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            storeLE( mkexpr(addr), getXMMRegLane32(rG, 0) );
            DIP("vmovd %s,%s\n", dis_buf, nameXMMReg(rG));
            delta += alen;
         }
         goto decode_success;
      }
      break;

   case 0x7F:
      
      
      if ((have66noF2noF3(pfx) || haveF3no66noF2(pfx))
          && 1==getVexL(pfx)) {
         UChar  modrm = getUChar(delta);
         UInt   rS    = gregOfRexRM(pfx, modrm);
         IRTemp tS    = newTemp(Ity_V256);
         Bool   isA   = have66noF2noF3(pfx);
         HChar  ch    = isA ? 'a' : 'u';
         assign(tS, getYMMReg(rS));
         if (epartIsReg(modrm)) {
            UInt rD = eregOfRexRM(pfx, modrm);
            delta += 1;
            putYMMReg(rD, mkexpr(tS));
            DIP("vmovdq%c %s,%s\n", ch, nameYMMReg(rS), nameYMMReg(rD));
         } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 0 );
            delta += alen;
            if (isA)
               gen_SEGV_if_not_32_aligned(addr);
            storeLE(mkexpr(addr), mkexpr(tS));
            DIP("vmovdq%c %s,%s\n", ch, nameYMMReg(rS), dis_buf);
         }
         goto decode_success;
      }
      
      
      if ((have66noF2noF3(pfx) || haveF3no66noF2(pfx))
          && 0==getVexL(pfx)) {
         UChar  modrm = getUChar(delta);
         UInt   rS    = gregOfRexRM(pfx, modrm);
         IRTemp tS    = newTemp(Ity_V128);
         Bool   isA   = have66noF2noF3(pfx);
         HChar  ch    = isA ? 'a' : 'u';
         assign(tS, getXMMReg(rS));
         if (epartIsReg(modrm)) {
            UInt rD = eregOfRexRM(pfx, modrm);
            delta += 1;
            putYMMRegLoAndZU(rD, mkexpr(tS));
            DIP("vmovdq%c %s,%s\n", ch, nameXMMReg(rS), nameXMMReg(rD));
         } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 0 );
            delta += alen;
            if (isA)
               gen_SEGV_if_not_16_aligned(addr);
            storeLE(mkexpr(addr), mkexpr(tS));
            DIP("vmovdq%c %s,%s\n", ch, nameXMMReg(rS), dis_buf);
         }
         goto decode_success;
      }
      break;

   case 0xAE:
      
      if (haveNo66noF2noF3(pfx)
          && 0==getVexL(pfx)
          && 0==getRexW(pfx) 
          && !epartIsReg(getUChar(delta)) && gregLO3ofRM(getUChar(delta)) == 3
          && sz == 4) {
         delta = dis_STMXCSR(vbi, pfx, delta, True);
         goto decode_success;
      }
      
      if (haveNo66noF2noF3(pfx)
          && 0==getVexL(pfx)
          && 0==getRexW(pfx) 
          && !epartIsReg(getUChar(delta)) && gregLO3ofRM(getUChar(delta)) == 2
          && sz == 4) {
         delta = dis_LDMXCSR(vbi, pfx, delta, True);
         goto decode_success;
      }
      break;

   case 0xC2:
      
      
      if (haveF2no66noF3(pfx)) {
         Long delta0 = delta;
         delta = dis_AVX128_cmp_V_E_to_G( uses_vvvv, vbi, pfx, delta,
                                          "vcmpsd", False,
                                          8);
         if (delta > delta0) goto decode_success;
         
      }
      
      
      if (haveF3no66noF2(pfx)) {
         Long delta0 = delta;
         delta = dis_AVX128_cmp_V_E_to_G( uses_vvvv, vbi, pfx, delta,
                                          "vcmpss", False,
                                          4);
         if (delta > delta0) goto decode_success;
         
      }
      
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         Long delta0 = delta;
         delta = dis_AVX128_cmp_V_E_to_G( uses_vvvv, vbi, pfx, delta,
                                          "vcmppd", True,
                                          8);
         if (delta > delta0) goto decode_success;
         
      }
      
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         Long delta0 = delta;
         delta = dis_AVX256_cmp_V_E_to_G( uses_vvvv, vbi, pfx, delta,
                                          "vcmppd", 8);
         if (delta > delta0) goto decode_success;
         
      }
      
      
      if (haveNo66noF2noF3(pfx) && 0==getVexL(pfx)) {
         Long delta0 = delta;
         delta = dis_AVX128_cmp_V_E_to_G( uses_vvvv, vbi, pfx, delta,
                                          "vcmpps", True,
                                          4);
         if (delta > delta0) goto decode_success;
         
      }
      
      
      if (haveNo66noF2noF3(pfx) && 1==getVexL(pfx)) {
         Long delta0 = delta;
         delta = dis_AVX256_cmp_V_E_to_G( uses_vvvv, vbi, pfx, delta,
                                          "vcmpps", 4);
         if (delta > delta0) goto decode_success;
         
      }
      break;

   case 0xC4:
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         UChar  modrm = getUChar(delta);
         UInt   rG    = gregOfRexRM(pfx, modrm);
         UInt   rV    = getVexNvvvv(pfx);
         Int    imm8;
         IRTemp new16 = newTemp(Ity_I16);

         if ( epartIsReg( modrm ) ) {
            imm8 = (Int)(getUChar(delta+1) & 7);
            assign( new16, unop(Iop_32to16,
                                getIReg32(eregOfRexRM(pfx,modrm))) );
            delta += 1+1;
            DIP( "vpinsrw $%d,%s,%s\n", imm8,
                 nameIReg32( eregOfRexRM(pfx, modrm) ), nameXMMReg(rG) );
         } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 1 );
            imm8 = (Int)(getUChar(delta+alen) & 7);
            assign( new16, loadLE( Ity_I16, mkexpr(addr) ));
            delta += alen+1;
            DIP( "vpinsrw $%d,%s,%s\n", 
                 imm8, dis_buf, nameXMMReg(rG) );
         }

         IRTemp src_vec = newTemp(Ity_V128);
         assign(src_vec, getXMMReg( rV ));
         IRTemp res_vec = math_PINSRW_128( src_vec, new16, imm8 );
         putYMMRegLoAndZU( rG, mkexpr(res_vec) );
         *uses_vvvv = True;
         goto decode_success;
      }
      break;

   case 0xC5:
      
      if (have66noF2noF3(pfx)
         && 0==getVexL(pfx) && 0==getRexW(pfx)) {
         Long delta0 = delta;
         delta = dis_PEXTRW_128_EregOnly_toG( vbi, pfx, delta,
                                              True );
         if (delta > delta0) goto decode_success;
         
      }
      break; 

   case 0xC6:
      
      
      if (haveNo66noF2noF3(pfx) && 0==getVexL(pfx)) {
         Int    imm8 = 0;
         IRTemp eV   = newTemp(Ity_V128);
         IRTemp vV   = newTemp(Ity_V128);
         UInt  modrm = getUChar(delta);
         UInt  rG    = gregOfRexRM(pfx,modrm);
         UInt  rV    = getVexNvvvv(pfx);
         assign( vV, getXMMReg(rV) );
         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx,modrm);
            assign( eV, getXMMReg(rE) );
            imm8 = (Int)getUChar(delta+1);
            delta += 1+1;
            DIP("vshufps $%d,%s,%s,%s\n",
                imm8, nameXMMReg(rE), nameXMMReg(rV), nameXMMReg(rG));
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 1 );
            assign( eV, loadLE(Ity_V128, mkexpr(addr)) );
            imm8 = (Int)getUChar(delta+alen);
            delta += 1+alen;
            DIP("vshufps $%d,%s,%s,%s\n",
                imm8, dis_buf, nameXMMReg(rV), nameXMMReg(rG));
         }
         IRTemp res = math_SHUFPS_128( eV, vV, imm8 );
         putYMMRegLoAndZU( rG, mkexpr(res) );
         *uses_vvvv = True;
         goto decode_success;
      }
      
      
      if (haveNo66noF2noF3(pfx) && 1==getVexL(pfx)) {
         Int    imm8 = 0;
         IRTemp eV   = newTemp(Ity_V256);
         IRTemp vV   = newTemp(Ity_V256);
         UInt  modrm = getUChar(delta);
         UInt  rG    = gregOfRexRM(pfx,modrm);
         UInt  rV    = getVexNvvvv(pfx);
         assign( vV, getYMMReg(rV) );
         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx,modrm);
            assign( eV, getYMMReg(rE) );
            imm8 = (Int)getUChar(delta+1);
            delta += 1+1;
            DIP("vshufps $%d,%s,%s,%s\n",
                imm8, nameYMMReg(rE), nameYMMReg(rV), nameYMMReg(rG));
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 1 );
            assign( eV, loadLE(Ity_V256, mkexpr(addr)) );
            imm8 = (Int)getUChar(delta+alen);
            delta += 1+alen;
            DIP("vshufps $%d,%s,%s,%s\n",
                imm8, dis_buf, nameYMMReg(rV), nameYMMReg(rG));
         }
         IRTemp res = math_SHUFPS_256( eV, vV, imm8 );
         putYMMReg( rG, mkexpr(res) );
         *uses_vvvv = True;
         goto decode_success;
      }
      
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         Int    imm8 = 0;
         IRTemp eV   = newTemp(Ity_V128);
         IRTemp vV   = newTemp(Ity_V128);
         UInt  modrm = getUChar(delta);
         UInt  rG    = gregOfRexRM(pfx,modrm);
         UInt  rV    = getVexNvvvv(pfx);
         assign( vV, getXMMReg(rV) );
         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx,modrm);
            assign( eV, getXMMReg(rE) );
            imm8 = (Int)getUChar(delta+1);
            delta += 1+1;
            DIP("vshufpd $%d,%s,%s,%s\n",
                imm8, nameXMMReg(rE), nameXMMReg(rV), nameXMMReg(rG));
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 1 );
            assign( eV, loadLE(Ity_V128, mkexpr(addr)) );
            imm8 = (Int)getUChar(delta+alen);
            delta += 1+alen;
            DIP("vshufpd $%d,%s,%s,%s\n",
                imm8, dis_buf, nameXMMReg(rV), nameXMMReg(rG));
         }
         IRTemp res = math_SHUFPD_128( eV, vV, imm8 );
         putYMMRegLoAndZU( rG, mkexpr(res) );
         *uses_vvvv = True;
         goto decode_success;
      }
      
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         Int    imm8 = 0;
         IRTemp eV   = newTemp(Ity_V256);
         IRTemp vV   = newTemp(Ity_V256);
         UInt  modrm = getUChar(delta);
         UInt  rG    = gregOfRexRM(pfx,modrm);
         UInt  rV    = getVexNvvvv(pfx);
         assign( vV, getYMMReg(rV) );
         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx,modrm);
            assign( eV, getYMMReg(rE) );
            imm8 = (Int)getUChar(delta+1);
            delta += 1+1;
            DIP("vshufpd $%d,%s,%s,%s\n",
                imm8, nameYMMReg(rE), nameYMMReg(rV), nameYMMReg(rG));
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 1 );
            assign( eV, loadLE(Ity_V256, mkexpr(addr)) );
            imm8 = (Int)getUChar(delta+alen);
            delta += 1+alen;
            DIP("vshufpd $%d,%s,%s,%s\n",
                imm8, dis_buf, nameYMMReg(rV), nameYMMReg(rG));
         }
         IRTemp res = math_SHUFPD_256( eV, vV, imm8 );
         putYMMReg( rG, mkexpr(res) );
         *uses_vvvv = True;
         goto decode_success;
      }
      break;

   case 0xD0:
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_VEX_NDS_128_AnySimdPfx_0F_WIG_complex(
                    uses_vvvv, vbi, pfx, delta,
                    "vaddsubpd", math_ADDSUBPD_128 );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_VEX_NDS_256_AnySimdPfx_0F_WIG_complex(
                    uses_vvvv, vbi, pfx, delta,
                    "vaddsubpd", math_ADDSUBPD_256 );
         goto decode_success;
      }
      
      if (haveF2no66noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_VEX_NDS_128_AnySimdPfx_0F_WIG_complex(
                    uses_vvvv, vbi, pfx, delta,
                    "vaddsubps", math_ADDSUBPS_128 );
         goto decode_success;
      }
      
      if (haveF2no66noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_VEX_NDS_256_AnySimdPfx_0F_WIG_complex(
                    uses_vvvv, vbi, pfx, delta,
                    "vaddsubps", math_ADDSUBPS_256 );
         goto decode_success;
      }
      break;

   case 0xD1:
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_AVX128_shiftV_byE( vbi, pfx, delta,
                                        "vpsrlw", Iop_ShrN16x8 );
         *uses_vvvv = True;
         goto decode_success;
                        
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_AVX256_shiftV_byE( vbi, pfx, delta,
                                        "vpsrlw", Iop_ShrN16x16 );
         *uses_vvvv = True;
         goto decode_success;
                        
      }
      break;

   case 0xD2:
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_AVX128_shiftV_byE( vbi, pfx, delta,
                                        "vpsrld", Iop_ShrN32x4 );
         *uses_vvvv = True;
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_AVX256_shiftV_byE( vbi, pfx, delta,
                                        "vpsrld", Iop_ShrN32x8 );
         *uses_vvvv = True;
         goto decode_success;
      }
      break;

   case 0xD3:
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_AVX128_shiftV_byE( vbi, pfx, delta,
                                        "vpsrlq", Iop_ShrN64x2 );
         *uses_vvvv = True;
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_AVX256_shiftV_byE( vbi, pfx, delta,
                                        "vpsrlq", Iop_ShrN64x4 );
         *uses_vvvv = True;
         goto decode_success;
      }
      break;

   case 0xD4:
      
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_VEX_NDS_128_AnySimdPfx_0F_WIG_simple(
                    uses_vvvv, vbi, pfx, delta, "vpaddq", Iop_Add64x2 );
         goto decode_success;
      }
      
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_VEX_NDS_256_AnySimdPfx_0F_WIG_simple(
                    uses_vvvv, vbi, pfx, delta, "vpaddq", Iop_Add64x4 );
         goto decode_success;
      }
      break;

   case 0xD5:
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_AVX128_E_V_to_G(
                    uses_vvvv, vbi, pfx, delta, "vpmullw", Iop_Mul16x8 );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_AVX256_E_V_to_G(
                    uses_vvvv, vbi, pfx, delta, "vpmullw", Iop_Mul16x16 );
         goto decode_success;
      }
      break;

   case 0xD6:
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)
          && 0==getRexW(pfx)) {
         UChar modrm = getUChar(delta);
         UInt  rG    = gregOfRexRM(pfx,modrm);
         if (epartIsReg(modrm)) {
            
            
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            storeLE( mkexpr(addr), getXMMRegLane64( rG, 0 ));
            DIP("vmovq %s,%s\n", nameXMMReg(rG), dis_buf );
            delta += alen;
            goto decode_success;
         }
      }
      break;

   case 0xD7:
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_PMOVMSKB_128( vbi, pfx, delta, True );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_PMOVMSKB_256( vbi, pfx, delta );
         goto decode_success;
      }
      break;

   case 0xD8:
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_AVX128_E_V_to_G(
                    uses_vvvv, vbi, pfx, delta, "vpsubusb", Iop_QSub8Ux16 );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_AVX256_E_V_to_G(
                    uses_vvvv, vbi, pfx, delta, "vpsubusb", Iop_QSub8Ux32 );
         goto decode_success;
      }
      break;

   case 0xD9:
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_AVX128_E_V_to_G(
                    uses_vvvv, vbi, pfx, delta, "vpsubusw", Iop_QSub16Ux8 );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_AVX256_E_V_to_G(
                    uses_vvvv, vbi, pfx, delta, "vpsubusw", Iop_QSub16Ux16 );
         goto decode_success;
      }
      break;

   case 0xDA:
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_AVX128_E_V_to_G(
                    uses_vvvv, vbi, pfx, delta, "vpminub", Iop_Min8Ux16 );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_AVX256_E_V_to_G(
                    uses_vvvv, vbi, pfx, delta, "vpminub", Iop_Min8Ux32 );
         goto decode_success;
      }
      break;

   case 0xDB:
      
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_VEX_NDS_128_AnySimdPfx_0F_WIG_simple(
                    uses_vvvv, vbi, pfx, delta, "vpand", Iop_AndV128 );
         goto decode_success;
      }
      
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_VEX_NDS_256_AnySimdPfx_0F_WIG_simple(
                    uses_vvvv, vbi, pfx, delta, "vpand", Iop_AndV256 );
         goto decode_success;
      }
      break;

   case 0xDC:
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_AVX128_E_V_to_G(
                    uses_vvvv, vbi, pfx, delta, "vpaddusb", Iop_QAdd8Ux16 );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_AVX256_E_V_to_G(
                    uses_vvvv, vbi, pfx, delta, "vpaddusb", Iop_QAdd8Ux32 );
         goto decode_success;
      }
      break;

   case 0xDD:
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_AVX128_E_V_to_G(
                    uses_vvvv, vbi, pfx, delta, "vpaddusw", Iop_QAdd16Ux8 );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_AVX256_E_V_to_G(
                    uses_vvvv, vbi, pfx, delta, "vpaddusw", Iop_QAdd16Ux16 );
         goto decode_success;
      }
      break;

   case 0xDE:
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_AVX128_E_V_to_G(
                    uses_vvvv, vbi, pfx, delta, "vpmaxub", Iop_Max8Ux16 );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_AVX256_E_V_to_G(
                    uses_vvvv, vbi, pfx, delta, "vpmaxub", Iop_Max8Ux32 );
         goto decode_success;
      }
      break;

   case 0xDF:
      
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_VEX_NDS_128_AnySimdPfx_0F_WIG(
                    uses_vvvv, vbi, pfx, delta, "vpandn", Iop_AndV128,
                    NULL, True, False );
         goto decode_success;
      }
      
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_VEX_NDS_256_AnySimdPfx_0F_WIG(
                    uses_vvvv, vbi, pfx, delta, "vpandn", Iop_AndV256,
                    NULL, True, False );
         goto decode_success;
      }
      break;

   case 0xE0:
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_AVX128_E_V_to_G(
                    uses_vvvv, vbi, pfx, delta, "vpavgb", Iop_Avg8Ux16 );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_AVX256_E_V_to_G(
                    uses_vvvv, vbi, pfx, delta, "vpavgb", Iop_Avg8Ux32 );
         goto decode_success;
      }
      break;

   case 0xE1:
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_AVX128_shiftV_byE( vbi, pfx, delta,
                                        "vpsraw", Iop_SarN16x8 );
         *uses_vvvv = True;
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_AVX256_shiftV_byE( vbi, pfx, delta,
                                        "vpsraw", Iop_SarN16x16 );
         *uses_vvvv = True;
         goto decode_success;
      }
      break;

   case 0xE2:
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_AVX128_shiftV_byE( vbi, pfx, delta,
                                        "vpsrad", Iop_SarN32x4 );
         *uses_vvvv = True;
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_AVX256_shiftV_byE( vbi, pfx, delta,
                                        "vpsrad", Iop_SarN32x8 );
         *uses_vvvv = True;
         goto decode_success;
      }
      break;

   case 0xE3:
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_AVX128_E_V_to_G(
                    uses_vvvv, vbi, pfx, delta, "vpavgw", Iop_Avg16Ux8 );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_AVX256_E_V_to_G(
                    uses_vvvv, vbi, pfx, delta, "vpavgw", Iop_Avg16Ux16 );
         goto decode_success;
      }
      break;

   case 0xE4:
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_AVX128_E_V_to_G(
                    uses_vvvv, vbi, pfx, delta, "vpmulhuw", Iop_MulHi16Ux8 );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_AVX256_E_V_to_G(
                    uses_vvvv, vbi, pfx, delta, "vpmulhuw", Iop_MulHi16Ux16 );
         goto decode_success;
      }
      break;

   case 0xE5:
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_AVX128_E_V_to_G(
                    uses_vvvv, vbi, pfx, delta, "vpmulhw", Iop_MulHi16Sx8 );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_AVX256_E_V_to_G(
                    uses_vvvv, vbi, pfx, delta, "vpmulhw", Iop_MulHi16Sx16 );
         goto decode_success;
      }
      break;

   case 0xE6:
      
      if (haveF3no66noF2(pfx) && 0==getVexL(pfx)) {
         delta = dis_CVTDQ2PD_128(vbi, pfx, delta, True);
         goto decode_success;
      }
      
      if (haveF3no66noF2(pfx) && 1==getVexL(pfx)) {
         delta = dis_CVTDQ2PD_256(vbi, pfx, delta);
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_CVTxPD2DQ_128(vbi, pfx, delta, True,
                                   True);
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_CVTxPD2DQ_256(vbi, pfx, delta, True);
         goto decode_success;
      }
      
      if (haveF2no66noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_CVTxPD2DQ_128(vbi, pfx, delta, True,
                                   False);
         goto decode_success;
      }
      
      if (haveF2no66noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_CVTxPD2DQ_256(vbi, pfx, delta, False);
         goto decode_success;
      }
      break;

   case 0xE7:
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         UChar modrm = getUChar(delta);
         UInt rG     = gregOfRexRM(pfx,modrm);
         if (!epartIsReg(modrm)) {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            gen_SEGV_if_not_16_aligned( addr );
            storeLE( mkexpr(addr), getXMMReg(rG) );
            DIP("vmovntdq %s,%s\n", dis_buf, nameXMMReg(rG));
            delta += alen;
            goto decode_success;
         }
         
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         UChar modrm = getUChar(delta);
         UInt rG     = gregOfRexRM(pfx,modrm);
         if (!epartIsReg(modrm)) {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            gen_SEGV_if_not_32_aligned( addr );
            storeLE( mkexpr(addr), getYMMReg(rG) );
            DIP("vmovntdq %s,%s\n", dis_buf, nameYMMReg(rG));
            delta += alen;
            goto decode_success;
         }
         
      }
      break;

   case 0xE8:
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_AVX128_E_V_to_G(
                    uses_vvvv, vbi, pfx, delta, "vpsubsb", Iop_QSub8Sx16 );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_AVX256_E_V_to_G(
                    uses_vvvv, vbi, pfx, delta, "vpsubsb", Iop_QSub8Sx32 );
         goto decode_success;
      }
      break;

   case 0xE9:
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_AVX128_E_V_to_G(
                    uses_vvvv, vbi, pfx, delta, "vpsubsw", Iop_QSub16Sx8 );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_AVX256_E_V_to_G(
                    uses_vvvv, vbi, pfx, delta, "vpsubsw", Iop_QSub16Sx16 );
         goto decode_success;
      }
      break;

   case 0xEA:
      
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_VEX_NDS_128_AnySimdPfx_0F_WIG_simple(
                    uses_vvvv, vbi, pfx, delta, "vpminsw", Iop_Min16Sx8 );
         goto decode_success;
      }
      
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_VEX_NDS_256_AnySimdPfx_0F_WIG_simple(
                    uses_vvvv, vbi, pfx, delta, "vpminsw", Iop_Min16Sx16 );
         goto decode_success;
      }
      break;

   case 0xEB:
      
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_VEX_NDS_128_AnySimdPfx_0F_WIG_simple(
                    uses_vvvv, vbi, pfx, delta, "vpor", Iop_OrV128 );
         goto decode_success;
      }
      
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_VEX_NDS_256_AnySimdPfx_0F_WIG_simple(
                    uses_vvvv, vbi, pfx, delta, "vpor", Iop_OrV256 );
         goto decode_success;
      }
      break;

   case 0xEC:
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_AVX128_E_V_to_G(
                    uses_vvvv, vbi, pfx, delta, "vpaddsb", Iop_QAdd8Sx16 );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_AVX256_E_V_to_G(
                    uses_vvvv, vbi, pfx, delta, "vpaddsb", Iop_QAdd8Sx32 );
         goto decode_success;
      }
      break;

   case 0xED:
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_AVX128_E_V_to_G(
                    uses_vvvv, vbi, pfx, delta, "vpaddsw", Iop_QAdd16Sx8 );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_AVX256_E_V_to_G(
                    uses_vvvv, vbi, pfx, delta, "vpaddsw", Iop_QAdd16Sx16 );
         goto decode_success;
      }
      break;

   case 0xEE:
      
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_VEX_NDS_128_AnySimdPfx_0F_WIG_simple(
                    uses_vvvv, vbi, pfx, delta, "vpmaxsw", Iop_Max16Sx8 );
         goto decode_success;
      }
      
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_VEX_NDS_256_AnySimdPfx_0F_WIG_simple(
                    uses_vvvv, vbi, pfx, delta, "vpmaxsw", Iop_Max16Sx16 );
         goto decode_success;
      }
      break;

   case 0xEF:
      
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_VEX_NDS_128_AnySimdPfx_0F_WIG_simple(
                    uses_vvvv, vbi, pfx, delta, "vpxor", Iop_XorV128 );
         goto decode_success;
      }
      
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_VEX_NDS_256_AnySimdPfx_0F_WIG_simple(
                    uses_vvvv, vbi, pfx, delta, "vpxor", Iop_XorV256 );
         goto decode_success;
      }
      break;

   case 0xF0:
      
      if (haveF2no66noF3(pfx) && 1==getVexL(pfx)) {
         UChar  modrm = getUChar(delta);
         UInt   rD    = gregOfRexRM(pfx, modrm);
         IRTemp tD    = newTemp(Ity_V256);
         if (epartIsReg(modrm)) break;
         addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 0 );
         delta += alen;
         assign(tD, loadLE(Ity_V256, mkexpr(addr)));
         DIP("vlddqu %s,%s\n", dis_buf, nameYMMReg(rD));
         putYMMReg(rD, mkexpr(tD));
         goto decode_success;
      }
      
      if (haveF2no66noF3(pfx) && 0==getVexL(pfx)) {
         UChar  modrm = getUChar(delta);
         UInt   rD    = gregOfRexRM(pfx, modrm);
         IRTemp tD    = newTemp(Ity_V128);
         if (epartIsReg(modrm)) break;
         addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 0 );
         delta += alen;
         assign(tD, loadLE(Ity_V128, mkexpr(addr)));
         DIP("vlddqu %s,%s\n", dis_buf, nameXMMReg(rD));
         putYMMRegLoAndZU(rD, mkexpr(tD));
         goto decode_success;
      }
      break;

   case 0xF1:
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_AVX128_shiftV_byE( vbi, pfx, delta,
                                        "vpsllw", Iop_ShlN16x8 );
         *uses_vvvv = True;
         goto decode_success;
                        
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_AVX256_shiftV_byE( vbi, pfx, delta,
                                        "vpsllw", Iop_ShlN16x16 );
         *uses_vvvv = True;
         goto decode_success;
                        
      }
      break;

   case 0xF2:
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_AVX128_shiftV_byE( vbi, pfx, delta,
                                        "vpslld", Iop_ShlN32x4 );
         *uses_vvvv = True;
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_AVX256_shiftV_byE( vbi, pfx, delta,
                                        "vpslld", Iop_ShlN32x8 );
         *uses_vvvv = True;
         goto decode_success;
      }
      break;

   case 0xF3:
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_AVX128_shiftV_byE( vbi, pfx, delta,
                                        "vpsllq", Iop_ShlN64x2 );
         *uses_vvvv = True;
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_AVX256_shiftV_byE( vbi, pfx, delta,
                                        "vpsllq", Iop_ShlN64x4 );
         *uses_vvvv = True;
         goto decode_success;
      }
      break;

   case 0xF4:
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_VEX_NDS_128_AnySimdPfx_0F_WIG_complex(
                    uses_vvvv, vbi, pfx, delta,
                    "vpmuludq", math_PMULUDQ_128 );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_VEX_NDS_256_AnySimdPfx_0F_WIG_complex(
                    uses_vvvv, vbi, pfx, delta,
                    "vpmuludq", math_PMULUDQ_256 );
         goto decode_success;
      }
      break;

   case 0xF5:
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_VEX_NDS_128_AnySimdPfx_0F_WIG_complex(
                    uses_vvvv, vbi, pfx, delta,
                    "vpmaddwd", math_PMADDWD_128 );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_VEX_NDS_256_AnySimdPfx_0F_WIG_complex(
                    uses_vvvv, vbi, pfx, delta,
                    "vpmaddwd", math_PMADDWD_256 );
         goto decode_success;
      }
      break;

   case 0xF6:
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_VEX_NDS_128_AnySimdPfx_0F_WIG_complex(
                    uses_vvvv, vbi, pfx, delta,
                    "vpsadbw", math_PSADBW_128 );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_VEX_NDS_256_AnySimdPfx_0F_WIG_complex(
                    uses_vvvv, vbi, pfx, delta,
                    "vpsadbw", math_PSADBW_256 );
         goto decode_success;
      }
      break;

   case 0xF7:
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)
          && epartIsReg(getUChar(delta))) {
         delta = dis_MASKMOVDQU( vbi, pfx, delta, True );
         goto decode_success;
      }
      break;

   case 0xF8:
      
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_VEX_NDS_128_AnySimdPfx_0F_WIG_simple(
                    uses_vvvv, vbi, pfx, delta, "vpsubb", Iop_Sub8x16 );
         goto decode_success;
      }
      
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_VEX_NDS_256_AnySimdPfx_0F_WIG_simple(
                    uses_vvvv, vbi, pfx, delta, "vpsubb", Iop_Sub8x32 );
         goto decode_success;
      }
      break;

   case 0xF9:
      
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_VEX_NDS_128_AnySimdPfx_0F_WIG_simple(
                    uses_vvvv, vbi, pfx, delta, "vpsubw", Iop_Sub16x8 );
         goto decode_success;
      }
      
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_VEX_NDS_256_AnySimdPfx_0F_WIG_simple(
                    uses_vvvv, vbi, pfx, delta, "vpsubw", Iop_Sub16x16 );
         goto decode_success;
      }
      break;

   case 0xFA:
      
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_VEX_NDS_128_AnySimdPfx_0F_WIG_simple(
                    uses_vvvv, vbi, pfx, delta, "vpsubd", Iop_Sub32x4 );
         goto decode_success;
      }
      
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_VEX_NDS_256_AnySimdPfx_0F_WIG_simple(
                    uses_vvvv, vbi, pfx, delta, "vpsubd", Iop_Sub32x8 );
         goto decode_success;
      }
      break;

   case 0xFB:
      
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_VEX_NDS_128_AnySimdPfx_0F_WIG_simple(
                    uses_vvvv, vbi, pfx, delta, "vpsubq", Iop_Sub64x2 );
         goto decode_success;
      }
      
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_VEX_NDS_256_AnySimdPfx_0F_WIG_simple(
                    uses_vvvv, vbi, pfx, delta, "vpsubq", Iop_Sub64x4 );
         goto decode_success;
      }
      break;

   case 0xFC:
      
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_VEX_NDS_128_AnySimdPfx_0F_WIG_simple(
                    uses_vvvv, vbi, pfx, delta, "vpaddb", Iop_Add8x16 );
         goto decode_success;
      }
      
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_VEX_NDS_256_AnySimdPfx_0F_WIG_simple(
                    uses_vvvv, vbi, pfx, delta, "vpaddb", Iop_Add8x32 );
         goto decode_success;
      }
      break;

   case 0xFD:
      
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_VEX_NDS_128_AnySimdPfx_0F_WIG_simple(
                    uses_vvvv, vbi, pfx, delta, "vpaddw", Iop_Add16x8 );
         goto decode_success;
      }
      
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_VEX_NDS_256_AnySimdPfx_0F_WIG_simple(
                    uses_vvvv, vbi, pfx, delta, "vpaddw", Iop_Add16x16 );
         goto decode_success;
      }
      break;

   case 0xFE:
      
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_VEX_NDS_128_AnySimdPfx_0F_WIG_simple(
                    uses_vvvv, vbi, pfx, delta, "vpaddd", Iop_Add32x4 );
         goto decode_success;
      }
      
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_VEX_NDS_256_AnySimdPfx_0F_WIG_simple(
                    uses_vvvv, vbi, pfx, delta, "vpaddd", Iop_Add32x8 );
         goto decode_success;
      }
      break;

   default:
      break;

   }

  
   return deltaIN;

  decode_success:
   return delta;
}



static IRTemp math_PERMILPS_VAR_128 ( IRTemp dataV, IRTemp ctrlV )
{
   IRExpr* cv1 = binop(Iop_ShrN32x4,
                       binop(Iop_ShlN32x4, mkexpr(ctrlV), mkU8(30)),
                       mkU8(30));
   IRTemp res = newTemp(Ity_V128);
   assign(res, binop(Iop_Perm32x4, mkexpr(dataV), cv1));
   return res;
}

static IRTemp math_PERMILPS_VAR_256 ( IRTemp dataV, IRTemp ctrlV )
{
   IRTemp dHi, dLo, cHi, cLo;
   dHi = dLo = cHi = cLo = IRTemp_INVALID;
   breakupV256toV128s( dataV, &dHi, &dLo );
   breakupV256toV128s( ctrlV, &cHi, &cLo );
   IRTemp rHi = math_PERMILPS_VAR_128( dHi, cHi );
   IRTemp rLo = math_PERMILPS_VAR_128( dLo, cLo );
   IRTemp res = newTemp(Ity_V256);
   assign(res, binop(Iop_V128HLtoV256, mkexpr(rHi), mkexpr(rLo)));
   return res;
}

static IRTemp math_PERMILPD_VAR_128 ( IRTemp dataV, IRTemp ctrlV )
{
   
   IRTemp dHi, dLo, cHi, cLo;
   dHi = dLo = cHi = cLo = IRTemp_INVALID;
   breakupV128to64s( dataV, &dHi, &dLo );
   breakupV128to64s( ctrlV, &cHi, &cLo );
   IRExpr* rHi
      = IRExpr_ITE( unop(Iop_64to1,
                         binop(Iop_Shr64, mkexpr(cHi), mkU8(1))),
                    mkexpr(dHi), mkexpr(dLo) );
   IRExpr* rLo
      = IRExpr_ITE( unop(Iop_64to1,
                         binop(Iop_Shr64, mkexpr(cLo), mkU8(1))),
                    mkexpr(dHi), mkexpr(dLo) );
   IRTemp res = newTemp(Ity_V128);
   assign(res, binop(Iop_64HLtoV128, rHi, rLo));
   return res;
}

static IRTemp math_PERMILPD_VAR_256 ( IRTemp dataV, IRTemp ctrlV )
{
   IRTemp dHi, dLo, cHi, cLo;
   dHi = dLo = cHi = cLo = IRTemp_INVALID;
   breakupV256toV128s( dataV, &dHi, &dLo );
   breakupV256toV128s( ctrlV, &cHi, &cLo );
   IRTemp rHi = math_PERMILPD_VAR_128( dHi, cHi );
   IRTemp rLo = math_PERMILPD_VAR_128( dLo, cLo );
   IRTemp res = newTemp(Ity_V256);
   assign(res, binop(Iop_V128HLtoV256, mkexpr(rHi), mkexpr(rLo)));
   return res;
}

static IRTemp math_VPERMD ( IRTemp ctrlV, IRTemp dataV )
{
   IRExpr* cv1 = binop(Iop_ShrN32x8,
                       binop(Iop_ShlN32x8, mkexpr(ctrlV), mkU8(29)),
                       mkU8(29));
   IRTemp res = newTemp(Ity_V256);
   assign(res, binop(Iop_Perm32x8, mkexpr(dataV), cv1));
   return res;
}

static Long dis_SHIFTX ( Bool* uses_vvvv,
                         const VexAbiInfo* vbi, Prefix pfx, Long delta,
                         const HChar* opname, IROp op8 )
{
   HChar   dis_buf[50];
   Int     alen;
   Int     size = getRexW(pfx) ? 8 : 4;
   IRType  ty   = szToITy(size);
   IRTemp  src  = newTemp(ty);
   IRTemp  amt  = newTemp(ty);
   UChar   rm   = getUChar(delta);

   assign( amt, getIRegV(size,pfx) );
   if (epartIsReg(rm)) {
      assign( src, getIRegE(size,pfx,rm) );
      DIP("%s %s,%s,%s\n", opname, nameIRegV(size,pfx),
                           nameIRegE(size,pfx,rm), nameIRegG(size,pfx,rm));
      delta++;
   } else {
      IRTemp addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
      assign( src, loadLE(ty, mkexpr(addr)) );
      DIP("%s %s,%s,%s\n", opname, nameIRegV(size,pfx), dis_buf,
                           nameIRegG(size,pfx,rm));
      delta += alen;
   }

   putIRegG( size, pfx, rm,
             binop(mkSizedOp(ty,op8), mkexpr(src),
                   narrowTo(Ity_I8, binop(mkSizedOp(ty,Iop_And8), mkexpr(amt),
                                          mkU(ty,8*size-1)))) );
   
   *uses_vvvv = True;
   return delta;
}


static Long dis_FMA ( const VexAbiInfo* vbi, Prefix pfx, Long delta, UChar opc )
{
   UChar  modrm   = getUChar(delta);
   UInt   rG      = gregOfRexRM(pfx, modrm);
   UInt   rV      = getVexNvvvv(pfx);
   Bool   scalar  = (opc & 0xF) > 7 && (opc & 1);
   IRType ty      = getRexW(pfx) ? Ity_F64 : Ity_F32;
   IRType vty     = scalar ? ty : getVexL(pfx) ? Ity_V256 : Ity_V128;
   IRTemp vX      = newTemp(vty);
   IRTemp vY      = newTemp(vty);
   IRTemp vZ      = newTemp(vty);
   IRExpr *x[8], *y[8], *z[8];
   IRTemp addr    = IRTemp_INVALID;
   HChar  dis_buf[50];
   Int    alen    = 0;
   const HChar *name;
   const HChar *suffix;
   const HChar *order;
   Bool   negateRes   = False;
   Bool   negateZeven = False;
   Bool   negateZodd  = False;
   Int    i, j;
   Int    count;
   static IROp ops[] = { Iop_V256to64_0, Iop_V256to64_1,
                         Iop_V256to64_2, Iop_V256to64_3,
                         Iop_V128to64, Iop_V128HIto64 };

   switch (opc & 0xF) {
   case 0x6:
      name = "addsub";
      negateZeven = True;
      break;
   case 0x7:
      name = "subadd";
      negateZodd = True;
      break;
   case 0x8:
   case 0x9:
      name = "add";
      break;
   case 0xA:
   case 0xB:
      name = "sub";
      negateZeven = True;
      negateZodd = True;
      break;
   case 0xC:
   case 0xD:
      name = "add";
      negateRes = True;
      negateZeven = True;
      negateZodd = True;
      break;
   case 0xE:
   case 0xF:
      name = "sub";
      negateRes = True;
      break;
   default:
      vpanic("dis_FMA(amd64)");
      break;
   }
   switch (opc & 0xF0) {
   case 0x90: order = "132"; break;
   case 0xA0: order = "213"; break;
   case 0xB0: order = "231"; break;
   default: vpanic("dis_FMA(amd64)"); break;
   }
   if (scalar)
      suffix = ty == Ity_F64 ? "sd" : "ss";
   else
      suffix = ty == Ity_F64 ? "pd" : "ps";

   if (scalar) {
      assign( vX, ty == Ity_F64
                  ? getXMMRegLane64F(rG, 0) : getXMMRegLane32F(rG, 0) );
      assign( vZ, ty == Ity_F64
                  ? getXMMRegLane64F(rV, 0) : getXMMRegLane32F(rV, 0) );
   } else {
      assign( vX, vty == Ity_V256 ? getYMMReg(rG) : getXMMReg(rG) );
      assign( vZ, vty == Ity_V256 ? getYMMReg(rV) : getXMMReg(rV) );
   }

   if (epartIsReg(modrm)) {
      UInt rE = eregOfRexRM(pfx, modrm);
      delta += 1;
      if (scalar)
         assign( vY, ty == Ity_F64
                     ? getXMMRegLane64F(rE, 0) : getXMMRegLane32F(rE, 0) );
      else
         assign( vY, vty == Ity_V256 ? getYMMReg(rE) : getXMMReg(rE) );
      if (vty == Ity_V256) {
         DIP("vf%sm%s%s%s %s,%s,%s\n", negateRes ? "n" : "",
             name, order, suffix, nameYMMReg(rE), nameYMMReg(rV),
             nameYMMReg(rG));
      } else {
         DIP("vf%sm%s%s%s %s,%s,%s\n", negateRes ? "n" : "",
             name, order, suffix, nameXMMReg(rE), nameXMMReg(rV),
             nameXMMReg(rG));
      }
   } else {
      addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 0 );
      delta += alen;
      assign(vY, loadLE(vty, mkexpr(addr)));
      if (vty == Ity_V256) {
         DIP("vf%sm%s%s%s %s,%s,%s\n", negateRes ? "n" : "",
             name, order, suffix, dis_buf, nameYMMReg(rV),
             nameYMMReg(rG));
      } else {
         DIP("vf%sm%s%s%s %s,%s,%s\n", negateRes ? "n" : "",
             name, order, suffix, dis_buf, nameXMMReg(rV),
             nameXMMReg(rG));
      }
   }

   if ((opc & 0xF0) != 0x90) {
      IRTemp tem = vX;
      if ((opc & 0xF0) == 0xA0) {
         vX = vZ;
         vZ = vY;
         vY = tem;
      } else {
         vX = vZ;
         vZ = tem;
      }
   }

   if (scalar) {
      count = 1;
      x[0] = mkexpr(vX);
      y[0] = mkexpr(vY);
      z[0] = mkexpr(vZ);
   } else if (ty == Ity_F32) {
      count = vty == Ity_V256 ? 8 : 4;
      j = vty == Ity_V256 ? 0 : 4;
      for (i = 0; i < count; i += 2) {
         IRTemp tem = newTemp(Ity_I64);
         assign(tem, unop(ops[i / 2 + j], mkexpr(vX)));
         x[i] = unop(Iop_64to32, mkexpr(tem));
         x[i + 1] = unop(Iop_64HIto32, mkexpr(tem));
         tem = newTemp(Ity_I64);
         assign(tem, unop(ops[i / 2 + j], mkexpr(vY)));
         y[i] = unop(Iop_64to32, mkexpr(tem));
         y[i + 1] = unop(Iop_64HIto32, mkexpr(tem));
         tem = newTemp(Ity_I64);
         assign(tem, unop(ops[i / 2 + j], mkexpr(vZ)));
         z[i] = unop(Iop_64to32, mkexpr(tem));
         z[i + 1] = unop(Iop_64HIto32, mkexpr(tem));
      }
   } else {
      count = vty == Ity_V256 ? 4 : 2;
      j = vty == Ity_V256 ? 0 : 4;
      for (i = 0; i < count; i++) {
         x[i] = unop(ops[i + j], mkexpr(vX));
         y[i] = unop(ops[i + j], mkexpr(vY));
         z[i] = unop(ops[i + j], mkexpr(vZ));
      }
   }
   if (!scalar)
      for (i = 0; i < count; i++) {
         IROp op = ty == Ity_F64
                   ? Iop_ReinterpI64asF64 : Iop_ReinterpI32asF32;
         x[i] = unop(op, x[i]);
         y[i] = unop(op, y[i]);
         z[i] = unop(op, z[i]);
      }
   for (i = 0; i < count; i++) {
      if ((i & 1) ? negateZodd : negateZeven)
         z[i] = unop(ty == Ity_F64 ? Iop_NegF64 : Iop_NegF32, z[i]);
      x[i] = IRExpr_Qop(ty == Ity_F64 ? Iop_MAddF64 : Iop_MAddF32,
                        get_FAKE_roundingmode(), x[i], y[i], z[i]);
      if (negateRes)
         x[i] = unop(ty == Ity_F64 ? Iop_NegF64 : Iop_NegF32, x[i]);
      if (ty == Ity_F64)
         putYMMRegLane64F( rG, i, x[i] );
      else
         putYMMRegLane32F( rG, i, x[i] );
   }
   if (vty != Ity_V256)
      putYMMRegLane128( rG, 1, mkV128(0) );

   return delta;
}


static ULong dis_VMASKMOV ( Bool *uses_vvvv, const VexAbiInfo* vbi,
                            Prefix pfx, Long delta,
                            const HChar* opname, Bool isYMM, IRType ty,
                            Bool isLoad )
{
   HChar   dis_buf[50];
   Int     alen, i;
   IRTemp  addr;
   UChar   modrm = getUChar(delta);
   UInt    rG    = gregOfRexRM(pfx,modrm);
   UInt    rV    = getVexNvvvv(pfx);

   addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
   delta += alen;

    if (isLoad && isYMM) {
      DIP("%s %s,%s,%s\n", opname, dis_buf, nameYMMReg(rV), nameYMMReg(rG) );
   }
   else if (isLoad && !isYMM) {
      DIP("%s %s,%s,%s\n", opname, dis_buf, nameXMMReg(rV), nameXMMReg(rG) );
   }

   else if (!isLoad && isYMM) {
      DIP("%s %s,%s,%s\n", opname, nameYMMReg(rG), nameYMMReg(rV), dis_buf );
   }
   else {
      vassert(!isLoad && !isYMM);
      DIP("%s %s,%s,%s\n", opname, nameXMMReg(rG), nameXMMReg(rV), dis_buf );
   }

   vassert(ty == Ity_I32 || ty == Ity_I64);
   Bool laneIs32 = ty == Ity_I32;

   Int nLanes = (isYMM ? 2 : 1) * (laneIs32 ? 4 : 2);

   for (i = 0; i < nLanes; i++) {
      IRExpr* shAmt = laneIs32 ? mkU8(31)    : mkU8(63);
      IRExpr* one   = laneIs32 ? mkU32(1)    : mkU64(1);
      IROp    opSHR = laneIs32 ? Iop_Shr32   : Iop_Shr64;
      IROp    opEQ  = laneIs32 ? Iop_CmpEQ32 : Iop_CmpEQ64;
      IRExpr* lane  = (laneIs32 ? getYMMRegLane32 : getYMMRegLane64)( rV, i );

      IRTemp  cond = newTemp(Ity_I1);
      assign(cond, binop(opEQ, binop(opSHR, lane, shAmt), one));

      IRTemp  data = newTemp(ty);
      IRExpr* ea   = binop(Iop_Add64, mkexpr(addr),
                                      mkU64(i * (laneIs32 ? 4 : 8)));
      if (isLoad) {
         stmt(
            IRStmt_LoadG(
               Iend_LE, laneIs32 ? ILGop_Ident32 : ILGop_Ident64,
               data, ea, laneIs32 ? mkU32(0) : mkU64(0), mkexpr(cond)
         ));
         (laneIs32 ? putYMMRegLane32 : putYMMRegLane64)( rG, i, mkexpr(data) );
      } else {
         assign(data, (laneIs32 ? getYMMRegLane32 : getYMMRegLane64)( rG, i ));
         stmt( IRStmt_StoreG(Iend_LE, ea, mkexpr(data), mkexpr(cond)) );
      }
   }

   if (isLoad && !isYMM)
      putYMMRegLane128( rG, 1, mkV128(0) );

   *uses_vvvv = True;
   return delta;
}


static ULong dis_VGATHER ( Bool *uses_vvvv, const VexAbiInfo* vbi,
                           Prefix pfx, Long delta,
                           const HChar* opname, Bool isYMM,
                           Bool isVM64x, IRType ty )
{
   HChar  dis_buf[50];
   Int    alen, i, vscale, count1, count2;
   IRTemp addr;
   UChar  modrm = getUChar(delta);
   UInt   rG    = gregOfRexRM(pfx,modrm);
   UInt   rV    = getVexNvvvv(pfx);
   UInt   rI;
   IRType dstTy = (isYMM && (ty == Ity_I64 || !isVM64x)) ? Ity_V256 : Ity_V128;
   IRType idxTy = (isYMM && (ty == Ity_I32 || isVM64x)) ? Ity_V256 : Ity_V128;
   IRTemp cond;
   addr = disAVSIBMode ( &alen, vbi, pfx, delta, dis_buf, &rI,
                         idxTy, &vscale );
   if (addr == IRTemp_INVALID || rI == rG || rI == rV || rG == rV)
      return delta;
   if (dstTy == Ity_V256) {
      DIP("%s %s,%s,%s\n", opname, nameYMMReg(rV), dis_buf, nameYMMReg(rG) );
   } else {
      DIP("%s %s,%s,%s\n", opname, nameXMMReg(rV), dis_buf, nameXMMReg(rG) );
   }
   delta += alen;

   if (ty == Ity_I32) {
      count1 = isYMM ? 8 : 4;
      count2 = isVM64x ? count1 / 2 : count1;
   } else {
      count1 = count2 = isYMM ? 4 : 2;
   }

   
   if (ty == Ity_I32) {
      if (isYMM)
         putYMMReg( rV, binop(Iop_SarN32x8, getYMMReg( rV ), mkU8(31)) );
      else
         putYMMRegLoAndZU( rV, binop(Iop_SarN32x4, getXMMReg( rV ), mkU8(31)) );
   } else {
      for (i = 0; i < count1; i++) {
         putYMMRegLane64( rV, i, binop(Iop_Sar64, getYMMRegLane64( rV, i ),
                                       mkU8(63)) );
      }
   }

   for (i = 0; i < count2; i++) {
      IRExpr *expr, *addr_expr;
      cond = newTemp(Ity_I1);
      assign( cond, 
              binop(ty == Ity_I32 ? Iop_CmpLT32S : Iop_CmpLT64S,
                    ty == Ity_I32 ? getYMMRegLane32( rV, i )
                                  : getYMMRegLane64( rV, i ),
                    mkU(ty, 0)) );
      expr = ty == Ity_I32 ? getYMMRegLane32( rG, i )
                           : getYMMRegLane64( rG, i );
      addr_expr = isVM64x ? getYMMRegLane64( rI, i )
                          : unop(Iop_32Sto64, getYMMRegLane32( rI, i ));
      switch (vscale) {
         case 2: addr_expr = binop(Iop_Shl64, addr_expr, mkU8(1)); break;
         case 4: addr_expr = binop(Iop_Shl64, addr_expr, mkU8(2)); break;
         case 8: addr_expr = binop(Iop_Shl64, addr_expr, mkU8(3)); break;
         default: break;
      }
      addr_expr = binop(Iop_Add64, mkexpr(addr), addr_expr);
      addr_expr = handleAddrOverrides(vbi, pfx, addr_expr);
      addr_expr = IRExpr_ITE(mkexpr(cond), addr_expr, getIReg64(R_RSP));
      expr = IRExpr_ITE(mkexpr(cond), loadLE(ty, addr_expr), expr);
      if (ty == Ity_I32) {
         putYMMRegLane32( rG, i, expr );
         putYMMRegLane32( rV, i, mkU32(0) );
      } else {
         putYMMRegLane64( rG, i, expr);
         putYMMRegLane64( rV, i, mkU64(0) );
      }
   }

   if (!isYMM || (ty == Ity_I32 && isVM64x)) {
      if (ty == Ity_I64 || isYMM)
         putYMMRegLane128( rV, 1, mkV128(0) );
      else if (ty == Ity_I32 && count2 == 2) {
         putYMMRegLane64( rV, 1, mkU64(0) );
         putYMMRegLane64( rG, 1, mkU64(0) );
      }
      putYMMRegLane128( rG, 1, mkV128(0) );
   }

   *uses_vvvv = True;
   return delta;
}


__attribute__((noinline))
static
Long dis_ESC_0F38__VEX (
        DisResult* dres,
           Bool*      uses_vvvv,
        Bool         (*resteerOkFn) ( void*, Addr ),
        Bool         resteerCisOk,
        void*        callback_opaque,
        const VexArchInfo* archinfo,
        const VexAbiInfo*  vbi,
        Prefix pfx, Int sz, Long deltaIN 
     )
{
   IRTemp addr  = IRTemp_INVALID;
   Int    alen  = 0;
   HChar  dis_buf[50];
   Long   delta = deltaIN;
   UChar  opc   = getUChar(delta);
   delta++;
   *uses_vvvv = False;

   switch (opc) {

   case 0x00:
      
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_VEX_NDS_128_AnySimdPfx_0F_WIG_complex(
                    uses_vvvv, vbi, pfx, delta, "vpshufb", math_PSHUFB_XMM );
         goto decode_success;
      }
      
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_VEX_NDS_256_AnySimdPfx_0F_WIG_complex(
                    uses_vvvv, vbi, pfx, delta, "vpshufb", math_PSHUFB_YMM );
         goto decode_success;
      }
      break;

   case 0x01:
   case 0x02:
   case 0x03:
      
      
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_PHADD_128( vbi, pfx, delta, True, opc );
         *uses_vvvv = True;
         goto decode_success;
      }
      
      
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_PHADD_256( vbi, pfx, delta, opc );
         *uses_vvvv = True;
         goto decode_success;
      }
      break;

   case 0x04:
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_VEX_NDS_128_AnySimdPfx_0F_WIG_complex(
                    uses_vvvv, vbi, pfx, delta, "vpmaddubsw",
                    math_PMADDUBSW_128 );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_VEX_NDS_256_AnySimdPfx_0F_WIG_complex(
                    uses_vvvv, vbi, pfx, delta, "vpmaddubsw",
                    math_PMADDUBSW_256 );
         goto decode_success;
      }
      break;
      
   case 0x05:
   case 0x06:
   case 0x07:
      
      
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_PHADD_128( vbi, pfx, delta, True, opc );
         *uses_vvvv = True;
         goto decode_success;
      }
      
      
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_PHADD_256( vbi, pfx, delta, opc );
         *uses_vvvv = True;
         goto decode_success;
      }
      break;

   case 0x08:
   case 0x09:
   case 0x0A:
      
      
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         IRTemp sV      = newTemp(Ity_V128);
         IRTemp dV      = newTemp(Ity_V128);
         IRTemp sHi, sLo, dHi, dLo;
         sHi = sLo = dHi = dLo = IRTemp_INVALID;
         HChar  ch      = '?';
         Int    laneszB = 0;
         UChar  modrm   = getUChar(delta);
         UInt   rG      = gregOfRexRM(pfx,modrm);
         UInt   rV      = getVexNvvvv(pfx);

         switch (opc) {
            case 0x08: laneszB = 1; ch = 'b'; break;
            case 0x09: laneszB = 2; ch = 'w'; break;
            case 0x0A: laneszB = 4; ch = 'd'; break;
            default: vassert(0);
         }

         assign( dV, getXMMReg(rV) );

         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx,modrm);
            assign( sV, getXMMReg(rE) );
            delta += 1;
            DIP("vpsign%c %s,%s,%s\n", ch, nameXMMReg(rE),
                nameXMMReg(rV), nameXMMReg(rG));
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            assign( sV, loadLE(Ity_V128, mkexpr(addr)) );
            delta += alen;
            DIP("vpsign%c %s,%s,%s\n", ch, dis_buf,
                nameXMMReg(rV), nameXMMReg(rG));
         }

         breakupV128to64s( dV, &dHi, &dLo );
         breakupV128to64s( sV, &sHi, &sLo );

         putYMMRegLoAndZU(
            rG,
            binop(Iop_64HLtoV128,
                  dis_PSIGN_helper( mkexpr(sHi), mkexpr(dHi), laneszB ),
                  dis_PSIGN_helper( mkexpr(sLo), mkexpr(dLo), laneszB )
            )
         );
         *uses_vvvv = True;
         goto decode_success;
      }
      
      
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         IRTemp sV      = newTemp(Ity_V256);
         IRTemp dV      = newTemp(Ity_V256);
         IRTemp s3, s2, s1, s0, d3, d2, d1, d0;
         s3 = s2 = s1 = s0 = IRTemp_INVALID;
         d3 = d2 = d1 = d0 = IRTemp_INVALID;
         UChar  ch      = '?';
         Int    laneszB = 0;
         UChar  modrm   = getUChar(delta);
         UInt   rG      = gregOfRexRM(pfx,modrm);
         UInt   rV      = getVexNvvvv(pfx);

         switch (opc) {
            case 0x08: laneszB = 1; ch = 'b'; break;
            case 0x09: laneszB = 2; ch = 'w'; break;
            case 0x0A: laneszB = 4; ch = 'd'; break;
            default: vassert(0);
         }

         assign( dV, getYMMReg(rV) );

         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx,modrm);
            assign( sV, getYMMReg(rE) );
            delta += 1;
            DIP("vpsign%c %s,%s,%s\n", ch, nameYMMReg(rE),
                nameYMMReg(rV), nameYMMReg(rG));
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            assign( sV, loadLE(Ity_V256, mkexpr(addr)) );
            delta += alen;
            DIP("vpsign%c %s,%s,%s\n", ch, dis_buf,
                nameYMMReg(rV), nameYMMReg(rG));
         }

         breakupV256to64s( dV, &d3, &d2, &d1, &d0 );
         breakupV256to64s( sV, &s3, &s2, &s1, &s0 );

         putYMMReg(
            rG,
            binop( Iop_V128HLtoV256,
                   binop(Iop_64HLtoV128,
                         dis_PSIGN_helper( mkexpr(s3), mkexpr(d3), laneszB ),
                         dis_PSIGN_helper( mkexpr(s2), mkexpr(d2), laneszB )
                   ),
                   binop(Iop_64HLtoV128,
                         dis_PSIGN_helper( mkexpr(s1), mkexpr(d1), laneszB ),
                         dis_PSIGN_helper( mkexpr(s0), mkexpr(d0), laneszB )
                   )
            )
         );
         *uses_vvvv = True;
         goto decode_success;
      }
      break;

   case 0x0B:
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         IRTemp sV      = newTemp(Ity_V128);
         IRTemp dV      = newTemp(Ity_V128);
         IRTemp sHi, sLo, dHi, dLo;
         sHi = sLo = dHi = dLo = IRTemp_INVALID;
         UChar  modrm   = getUChar(delta);
         UInt   rG      = gregOfRexRM(pfx,modrm);
         UInt   rV      = getVexNvvvv(pfx);

         assign( dV, getXMMReg(rV) );

         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx,modrm);
            assign( sV, getXMMReg(rE) );
            delta += 1;
            DIP("vpmulhrsw %s,%s,%s\n", nameXMMReg(rE),
                nameXMMReg(rV), nameXMMReg(rG));
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            assign( sV, loadLE(Ity_V128, mkexpr(addr)) );
            delta += alen;
            DIP("vpmulhrsw %s,%s,%s\n", dis_buf,
                nameXMMReg(rV), nameXMMReg(rG));
         }

         breakupV128to64s( dV, &dHi, &dLo );
         breakupV128to64s( sV, &sHi, &sLo );

         putYMMRegLoAndZU(
            rG,
            binop(Iop_64HLtoV128,
                  dis_PMULHRSW_helper( mkexpr(sHi), mkexpr(dHi) ),
                  dis_PMULHRSW_helper( mkexpr(sLo), mkexpr(dLo) )
            )
         );
         *uses_vvvv = True;
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         IRTemp sV      = newTemp(Ity_V256);
         IRTemp dV      = newTemp(Ity_V256);
         IRTemp s3, s2, s1, s0, d3, d2, d1, d0;
         s3 = s2 = s1 = s0 = d3 = d2 = d1 = d0 = IRTemp_INVALID;
         UChar  modrm   = getUChar(delta);
         UInt   rG      = gregOfRexRM(pfx,modrm);
         UInt   rV      = getVexNvvvv(pfx);

         assign( dV, getYMMReg(rV) );

         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx,modrm);
            assign( sV, getYMMReg(rE) );
            delta += 1;
            DIP("vpmulhrsw %s,%s,%s\n", nameYMMReg(rE),
                nameYMMReg(rV), nameYMMReg(rG));
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            assign( sV, loadLE(Ity_V256, mkexpr(addr)) );
            delta += alen;
            DIP("vpmulhrsw %s,%s,%s\n", dis_buf,
                nameYMMReg(rV), nameYMMReg(rG));
         }

         breakupV256to64s( dV, &d3, &d2, &d1, &d0 );
         breakupV256to64s( sV, &s3, &s2, &s1, &s0 );

         putYMMReg(
            rG,
            binop(Iop_V128HLtoV256,
                  binop(Iop_64HLtoV128,
                        dis_PMULHRSW_helper( mkexpr(s3), mkexpr(d3) ),
                        dis_PMULHRSW_helper( mkexpr(s2), mkexpr(d2) ) ),
                  binop(Iop_64HLtoV128,
                        dis_PMULHRSW_helper( mkexpr(s1), mkexpr(d1) ),
                        dis_PMULHRSW_helper( mkexpr(s0), mkexpr(d0) ) )
            )
         );
         *uses_vvvv = True;
         goto decode_success;
      }
      break;

   case 0x0C:
      
      if (have66noF2noF3(pfx)
          && 0==getVexL(pfx) && 0==getRexW(pfx)) {
         UChar  modrm = getUChar(delta);
         UInt   rG    = gregOfRexRM(pfx, modrm);
         UInt   rV    = getVexNvvvv(pfx);
         IRTemp ctrlV = newTemp(Ity_V128);
         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx, modrm);
            delta += 1;
            DIP("vpermilps %s,%s,%s\n",
                nameXMMReg(rE), nameXMMReg(rV), nameXMMReg(rG));
            assign(ctrlV, getXMMReg(rE));
         } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 0 );
            delta += alen;
            DIP("vpermilps %s,%s,%s\n",
                dis_buf, nameXMMReg(rV), nameXMMReg(rG));
            assign(ctrlV, loadLE(Ity_V128, mkexpr(addr)));
         }
         IRTemp dataV = newTemp(Ity_V128);
         assign(dataV, getXMMReg(rV));
         IRTemp resV = math_PERMILPS_VAR_128(dataV, ctrlV);
         putYMMRegLoAndZU(rG, mkexpr(resV));
         *uses_vvvv = True;
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx)
          && 1==getVexL(pfx) && 0==getRexW(pfx)) {
         UChar  modrm = getUChar(delta);
         UInt   rG    = gregOfRexRM(pfx, modrm);
         UInt   rV    = getVexNvvvv(pfx);
         IRTemp ctrlV = newTemp(Ity_V256);
         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx, modrm);
            delta += 1;
            DIP("vpermilps %s,%s,%s\n",
                nameYMMReg(rE), nameYMMReg(rV), nameYMMReg(rG));
            assign(ctrlV, getYMMReg(rE));
         } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 0 );
            delta += alen;
            DIP("vpermilps %s,%s,%s\n",
                dis_buf, nameYMMReg(rV), nameYMMReg(rG));
            assign(ctrlV, loadLE(Ity_V256, mkexpr(addr)));
         }
         IRTemp dataV = newTemp(Ity_V256);
         assign(dataV, getYMMReg(rV));
         IRTemp resV = math_PERMILPS_VAR_256(dataV, ctrlV);
         putYMMReg(rG, mkexpr(resV));
         *uses_vvvv = True;
         goto decode_success;
      }
      break;

   case 0x0D:
      
      if (have66noF2noF3(pfx)
          && 0==getVexL(pfx) && 0==getRexW(pfx)) {
         UChar  modrm = getUChar(delta);
         UInt   rG    = gregOfRexRM(pfx, modrm);
         UInt   rV    = getVexNvvvv(pfx);
         IRTemp ctrlV = newTemp(Ity_V128);
         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx, modrm);
            delta += 1;
            DIP("vpermilpd %s,%s,%s\n",
                nameXMMReg(rE), nameXMMReg(rV), nameXMMReg(rG));
            assign(ctrlV, getXMMReg(rE));
         } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 0 );
            delta += alen;
            DIP("vpermilpd %s,%s,%s\n",
                dis_buf, nameXMMReg(rV), nameXMMReg(rG));
            assign(ctrlV, loadLE(Ity_V128, mkexpr(addr)));
         }
         IRTemp dataV = newTemp(Ity_V128);
         assign(dataV, getXMMReg(rV));
         IRTemp resV = math_PERMILPD_VAR_128(dataV, ctrlV);
         putYMMRegLoAndZU(rG, mkexpr(resV));
         *uses_vvvv = True;
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx)
          && 1==getVexL(pfx) && 0==getRexW(pfx)) {
         UChar  modrm = getUChar(delta);
         UInt   rG    = gregOfRexRM(pfx, modrm);
         UInt   rV    = getVexNvvvv(pfx);
         IRTemp ctrlV = newTemp(Ity_V256);
         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx, modrm);
            delta += 1;
            DIP("vpermilpd %s,%s,%s\n",
                nameYMMReg(rE), nameYMMReg(rV), nameYMMReg(rG));
            assign(ctrlV, getYMMReg(rE));
         } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 0 );
            delta += alen;
            DIP("vpermilpd %s,%s,%s\n",
                dis_buf, nameYMMReg(rV), nameYMMReg(rG));
            assign(ctrlV, loadLE(Ity_V256, mkexpr(addr)));
         }
         IRTemp dataV = newTemp(Ity_V256);
         assign(dataV, getYMMReg(rV));
         IRTemp resV = math_PERMILPD_VAR_256(dataV, ctrlV);
         putYMMReg(rG, mkexpr(resV));
         *uses_vvvv = True;
         goto decode_success;
      }
      break;

   case 0x0E:
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_xTESTy_128( vbi, pfx, delta, True, 32 );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_xTESTy_256( vbi, pfx, delta, 32 );
         goto decode_success;
      }
      break;

   case 0x0F:
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_xTESTy_128( vbi, pfx, delta, True, 64 );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_xTESTy_256( vbi, pfx, delta, 64 );
         goto decode_success;
      }
      break;

   case 0x16:
      
      if (have66noF2noF3(pfx)
          && 1==getVexL(pfx) && 0==getRexW(pfx)) {
         delta = dis_VEX_NDS_256_AnySimdPfx_0F_WIG_complex(
                    uses_vvvv, vbi, pfx, delta, "vpermps", math_VPERMD );
         goto decode_success;
      }
      break;

   case 0x17:
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_xTESTy_128( vbi, pfx, delta, True, 0 );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_xTESTy_256( vbi, pfx, delta, 0 );
         goto decode_success;
      }
      break;

   case 0x18:
      
      if (have66noF2noF3(pfx)
          && 0==getVexL(pfx)
          && !epartIsReg(getUChar(delta))) {
         UChar modrm = getUChar(delta);
         UInt  rG    = gregOfRexRM(pfx, modrm);
         addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 0 );
         delta += alen;
         DIP("vbroadcastss %s,%s\n", dis_buf, nameXMMReg(rG));
         IRTemp t32 = newTemp(Ity_I32);
         assign(t32, loadLE(Ity_I32, mkexpr(addr)));
         IRTemp t64 = newTemp(Ity_I64);
         assign(t64, binop(Iop_32HLto64, mkexpr(t32), mkexpr(t32)));
         IRExpr* res = binop(Iop_64HLtoV128, mkexpr(t64), mkexpr(t64));
         putYMMRegLoAndZU(rG, res);
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx)
          && 1==getVexL(pfx)
          && !epartIsReg(getUChar(delta))) {
         UChar modrm = getUChar(delta);
         UInt  rG    = gregOfRexRM(pfx, modrm);
         addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 0 );
         delta += alen;
         DIP("vbroadcastss %s,%s\n", dis_buf, nameYMMReg(rG));
         IRTemp t32 = newTemp(Ity_I32);
         assign(t32, loadLE(Ity_I32, mkexpr(addr)));
         IRTemp t64 = newTemp(Ity_I64);
         assign(t64, binop(Iop_32HLto64, mkexpr(t32), mkexpr(t32)));
         IRExpr* res = IRExpr_Qop(Iop_64x4toV256, mkexpr(t64), mkexpr(t64),
                                                  mkexpr(t64), mkexpr(t64));
         putYMMReg(rG, res);
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx)
          && 0==getVexL(pfx)
          && epartIsReg(getUChar(delta))) {
         UChar modrm = getUChar(delta);
         UInt  rG    = gregOfRexRM(pfx, modrm);
         UInt  rE    = eregOfRexRM(pfx, modrm);
         DIP("vbroadcastss %s,%s\n", nameXMMReg(rE), nameXMMReg(rG));
         IRTemp t32 = newTemp(Ity_I32);
         assign(t32, getXMMRegLane32(rE, 0));
         IRTemp t64 = newTemp(Ity_I64);
         assign(t64, binop(Iop_32HLto64, mkexpr(t32), mkexpr(t32)));
         IRExpr* res = binop(Iop_64HLtoV128, mkexpr(t64), mkexpr(t64));
         putYMMRegLoAndZU(rG, res);
         delta++;
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx)
          && 1==getVexL(pfx)
          && epartIsReg(getUChar(delta))) {
         UChar modrm = getUChar(delta);
         UInt  rG    = gregOfRexRM(pfx, modrm);
         UInt  rE    = eregOfRexRM(pfx, modrm);
         DIP("vbroadcastss %s,%s\n", nameXMMReg(rE), nameYMMReg(rG));
         IRTemp t32 = newTemp(Ity_I32);
         assign(t32, getXMMRegLane32(rE, 0));
         IRTemp t64 = newTemp(Ity_I64);
         assign(t64, binop(Iop_32HLto64, mkexpr(t32), mkexpr(t32)));
         IRExpr* res = IRExpr_Qop(Iop_64x4toV256, mkexpr(t64), mkexpr(t64),
                                                  mkexpr(t64), mkexpr(t64));
         putYMMReg(rG, res);
         delta++;
         goto decode_success;
      }
      break;

   case 0x19:
      
      if (have66noF2noF3(pfx)
          && 1==getVexL(pfx)
          && !epartIsReg(getUChar(delta))) {
         UChar modrm = getUChar(delta);
         UInt  rG    = gregOfRexRM(pfx, modrm);
         addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 0 );
         delta += alen;
         DIP("vbroadcastsd %s,%s\n", dis_buf, nameYMMReg(rG));
         IRTemp t64 = newTemp(Ity_I64);
         assign(t64, loadLE(Ity_I64, mkexpr(addr)));
         IRExpr* res = IRExpr_Qop(Iop_64x4toV256, mkexpr(t64), mkexpr(t64),
                                                  mkexpr(t64), mkexpr(t64));
         putYMMReg(rG, res);
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx)
          && 1==getVexL(pfx)
          && epartIsReg(getUChar(delta))) {
         UChar modrm = getUChar(delta);
         UInt  rG    = gregOfRexRM(pfx, modrm);
         UInt  rE    = eregOfRexRM(pfx, modrm);
         DIP("vbroadcastsd %s,%s\n", nameXMMReg(rE), nameYMMReg(rG));
         IRTemp t64 = newTemp(Ity_I64);
         assign(t64, getXMMRegLane64(rE, 0));
         IRExpr* res = IRExpr_Qop(Iop_64x4toV256, mkexpr(t64), mkexpr(t64),
                                                  mkexpr(t64), mkexpr(t64));
         putYMMReg(rG, res);
         delta++;
         goto decode_success;
      }
      break;

   case 0x1A:
      
      if (have66noF2noF3(pfx)
          && 1==getVexL(pfx)
          && !epartIsReg(getUChar(delta))) {
         UChar modrm = getUChar(delta);
         UInt  rG    = gregOfRexRM(pfx, modrm);
         addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 0 );
         delta += alen;
         DIP("vbroadcastf128 %s,%s\n", dis_buf, nameYMMReg(rG));
         IRTemp t128 = newTemp(Ity_V128);
         assign(t128, loadLE(Ity_V128, mkexpr(addr)));
         putYMMReg( rG, binop(Iop_V128HLtoV256, mkexpr(t128), mkexpr(t128)) );
         goto decode_success;
      }
      break;

   case 0x1C:
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_AVX128_E_to_G_unary(
                    uses_vvvv, vbi, pfx, delta,
                    "vpabsb", math_PABS_XMM_pap1 );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_AVX256_E_to_G_unary(
                    uses_vvvv, vbi, pfx, delta,
                    "vpabsb", math_PABS_YMM_pap1 );
         goto decode_success;
      }
      break;

   case 0x1D:
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_AVX128_E_to_G_unary(
                    uses_vvvv, vbi, pfx, delta,
                    "vpabsw", math_PABS_XMM_pap2 );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_AVX256_E_to_G_unary(
                    uses_vvvv, vbi, pfx, delta,
                    "vpabsw", math_PABS_YMM_pap2 );
         goto decode_success;
      }
      break;

   case 0x1E:
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_AVX128_E_to_G_unary(
                    uses_vvvv, vbi, pfx, delta,
                    "vpabsd", math_PABS_XMM_pap4 );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_AVX256_E_to_G_unary(
                    uses_vvvv, vbi, pfx, delta,
                    "vpabsd", math_PABS_YMM_pap4 );
         goto decode_success;
      }
      break;

   case 0x20:
      
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_PMOVxXBW_128( vbi, pfx, delta,
                                   True, False );
         goto decode_success;
      }
      
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_PMOVxXBW_256( vbi, pfx, delta, False );
         goto decode_success;
      }
      break;

   case 0x21:
      
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_PMOVxXBD_128( vbi, pfx, delta,
                                   True, False );
         goto decode_success;
      }
      
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_PMOVxXBD_256( vbi, pfx, delta, False );
         goto decode_success;
      }
      break;

   case 0x22:
      
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_PMOVSXBQ_128( vbi, pfx, delta, True );
         goto decode_success;
      }
      
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_PMOVSXBQ_256( vbi, pfx, delta );
         goto decode_success;
      }
      break;

   case 0x23:
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_PMOVxXWD_128( vbi, pfx, delta,
                                   True, False );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_PMOVxXWD_256( vbi, pfx, delta, False );
         goto decode_success;
      }
      break;

   case 0x24:
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_PMOVSXWQ_128( vbi, pfx, delta, True );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_PMOVSXWQ_256( vbi, pfx, delta );
         goto decode_success;
      }
      break;

   case 0x25:
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_PMOVxXDQ_128( vbi, pfx, delta,
                                   True, False );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_PMOVxXDQ_256( vbi, pfx, delta, False );
         goto decode_success;
      }
      break;

   case 0x28:
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_VEX_NDS_128_AnySimdPfx_0F_WIG_complex(
                    uses_vvvv, vbi, pfx, delta,
                    "vpmuldq", math_PMULDQ_128 );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_VEX_NDS_256_AnySimdPfx_0F_WIG_complex(
                    uses_vvvv, vbi, pfx, delta,
                    "vpmuldq", math_PMULDQ_256 );
         goto decode_success;
      }
      break;

   case 0x29:
      
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_VEX_NDS_128_AnySimdPfx_0F_WIG_simple(
                    uses_vvvv, vbi, pfx, delta, "vpcmpeqq", Iop_CmpEQ64x2 );
         goto decode_success;
      }
      
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_VEX_NDS_256_AnySimdPfx_0F_WIG_simple(
                    uses_vvvv, vbi, pfx, delta, "vpcmpeqq", Iop_CmpEQ64x4 );
         goto decode_success;
      }
      break;

   case 0x2A:
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)
          && !epartIsReg(getUChar(delta))) {
         UChar  modrm = getUChar(delta);
         UInt   rD    = gregOfRexRM(pfx, modrm);
         IRTemp tD    = newTemp(Ity_V128);
         addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 0 );
         delta += alen;
         gen_SEGV_if_not_16_aligned(addr);
         assign(tD, loadLE(Ity_V128, mkexpr(addr)));
         DIP("vmovntdqa %s,%s\n", dis_buf, nameXMMReg(rD));
         putYMMRegLoAndZU(rD, mkexpr(tD));
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)
          && !epartIsReg(getUChar(delta))) {
         UChar  modrm = getUChar(delta);
         UInt   rD    = gregOfRexRM(pfx, modrm);
         IRTemp tD    = newTemp(Ity_V256);
         addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 0 );
         delta += alen;
         gen_SEGV_if_not_32_aligned(addr);
         assign(tD, loadLE(Ity_V256, mkexpr(addr)));
         DIP("vmovntdqa %s,%s\n", dis_buf, nameYMMReg(rD));
         putYMMReg(rD, mkexpr(tD));
         goto decode_success;
      }
      break;

   case 0x2B:
      
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_VEX_NDS_128_AnySimdPfx_0F_WIG(
                    uses_vvvv, vbi, pfx, delta, "vpackusdw",
                    Iop_QNarrowBin32Sto16Ux8, NULL,
                    False, True );
         goto decode_success;
      }
      
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_VEX_NDS_256_AnySimdPfx_0F_WIG_complex(
                    uses_vvvv, vbi, pfx, delta, "vpackusdw",
                    math_VPACKUSDW_YMM );
         goto decode_success;
      }
      break;

   case 0x2C:
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)
          && 0==getRexW(pfx)
          && !epartIsReg(getUChar(delta))) {
         delta = dis_VMASKMOV( uses_vvvv, vbi, pfx, delta, "vmaskmovps",
                               False, Ity_I32, True );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)
          && 0==getRexW(pfx)
          && !epartIsReg(getUChar(delta))) {
         delta = dis_VMASKMOV( uses_vvvv, vbi, pfx, delta, "vmaskmovps",
                               True, Ity_I32, True );
         goto decode_success;
      }
      break;

   case 0x2D:
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)
          && 0==getRexW(pfx)
          && !epartIsReg(getUChar(delta))) {
         delta = dis_VMASKMOV( uses_vvvv, vbi, pfx, delta, "vmaskmovpd",
                               False, Ity_I64, True );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)
          && 0==getRexW(pfx)
          && !epartIsReg(getUChar(delta))) {
         delta = dis_VMASKMOV( uses_vvvv, vbi, pfx, delta, "vmaskmovpd",
                               True, Ity_I64, True );
         goto decode_success;
      }
      break;

   case 0x2E:
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)
          && 0==getRexW(pfx)
          && !epartIsReg(getUChar(delta))) {
         delta = dis_VMASKMOV( uses_vvvv, vbi, pfx, delta, "vmaskmovps",
                               False, Ity_I32, False );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)
          && 0==getRexW(pfx)
          && !epartIsReg(getUChar(delta))) {
         delta = dis_VMASKMOV( uses_vvvv, vbi, pfx, delta, "vmaskmovps",
                               True, Ity_I32, False );
         goto decode_success;
      }
      break;

   case 0x2F:
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)
          && 0==getRexW(pfx)
          && !epartIsReg(getUChar(delta))) {
         delta = dis_VMASKMOV( uses_vvvv, vbi, pfx, delta, "vmaskmovpd",
                               False, Ity_I64, False );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)
          && 0==getRexW(pfx)
          && !epartIsReg(getUChar(delta))) {
         delta = dis_VMASKMOV( uses_vvvv, vbi, pfx, delta, "vmaskmovpd",
                               True, Ity_I64, False );
         goto decode_success;
      }
      break;

   case 0x30:
      
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_PMOVxXBW_128( vbi, pfx, delta,
                                   True, True );
         goto decode_success;
      }
      
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_PMOVxXBW_256( vbi, pfx, delta, True );
         goto decode_success;
      }
      break;

   case 0x31:
      
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_PMOVxXBD_128( vbi, pfx, delta,
                                   True, True );
         goto decode_success;
      }
      
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_PMOVxXBD_256( vbi, pfx, delta, True );
         goto decode_success;
      }
      break;

   case 0x32:
      
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_PMOVZXBQ_128( vbi, pfx, delta, True );
         goto decode_success;
      }
      
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_PMOVZXBQ_256( vbi, pfx, delta );
         goto decode_success;
      }
      break;

   case 0x33:
      
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_PMOVxXWD_128( vbi, pfx, delta,
                                   True, True );
         goto decode_success;
      }
      
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_PMOVxXWD_256( vbi, pfx, delta, True );
         goto decode_success;
      }
      break;

   case 0x34:
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_PMOVZXWQ_128( vbi, pfx, delta, True );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_PMOVZXWQ_256( vbi, pfx, delta );
         goto decode_success;
      }
      break;

   case 0x35:
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_PMOVxXDQ_128( vbi, pfx, delta,
                                   True, True );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_PMOVxXDQ_256( vbi, pfx, delta, True );
         goto decode_success;
      }
      break;

   case 0x36:
      
      if (have66noF2noF3(pfx)
          && 1==getVexL(pfx) && 0==getRexW(pfx)) {
         delta = dis_VEX_NDS_256_AnySimdPfx_0F_WIG_complex(
                    uses_vvvv, vbi, pfx, delta, "vpermd", math_VPERMD );
         goto decode_success;
      }
      break;

   case 0x37:
      
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_VEX_NDS_128_AnySimdPfx_0F_WIG_simple(
                    uses_vvvv, vbi, pfx, delta, "vpcmpgtq", Iop_CmpGT64Sx2 );
         goto decode_success;
      }
      
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_VEX_NDS_256_AnySimdPfx_0F_WIG_simple(
                    uses_vvvv, vbi, pfx, delta, "vpcmpgtq", Iop_CmpGT64Sx4 );
         goto decode_success;
      }
      break;

   case 0x38:
      
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_VEX_NDS_128_AnySimdPfx_0F_WIG_simple(
                    uses_vvvv, vbi, pfx, delta, "vpminsb", Iop_Min8Sx16 );
         goto decode_success;
      }
      
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_VEX_NDS_256_AnySimdPfx_0F_WIG_simple(
                    uses_vvvv, vbi, pfx, delta, "vpminsb", Iop_Min8Sx32 );
         goto decode_success;
      }
      break;

   case 0x39:
      
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_VEX_NDS_128_AnySimdPfx_0F_WIG_simple(
                    uses_vvvv, vbi, pfx, delta, "vpminsd", Iop_Min32Sx4 );
         goto decode_success;
      }
      
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_VEX_NDS_256_AnySimdPfx_0F_WIG_simple(
                    uses_vvvv, vbi, pfx, delta, "vpminsd", Iop_Min32Sx8 );
         goto decode_success;
      }
      break;

   case 0x3A:
      
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_VEX_NDS_128_AnySimdPfx_0F_WIG_simple(
                    uses_vvvv, vbi, pfx, delta, "vpminuw", Iop_Min16Ux8 );
         goto decode_success;
      }
      
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_VEX_NDS_256_AnySimdPfx_0F_WIG_simple(
                    uses_vvvv, vbi, pfx, delta, "vpminuw", Iop_Min16Ux16 );
         goto decode_success;
      }
      break;

   case 0x3B:
      
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_VEX_NDS_128_AnySimdPfx_0F_WIG_simple(
                    uses_vvvv, vbi, pfx, delta, "vpminud", Iop_Min32Ux4 );
         goto decode_success;
      }
      
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_VEX_NDS_256_AnySimdPfx_0F_WIG_simple(
                    uses_vvvv, vbi, pfx, delta, "vpminud", Iop_Min32Ux8 );
         goto decode_success;
      }
      break;

   case 0x3C:
      
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_VEX_NDS_128_AnySimdPfx_0F_WIG_simple(
                    uses_vvvv, vbi, pfx, delta, "vpmaxsb", Iop_Max8Sx16 );
         goto decode_success;
      }
      
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_VEX_NDS_256_AnySimdPfx_0F_WIG_simple(
                    uses_vvvv, vbi, pfx, delta, "vpmaxsb", Iop_Max8Sx32 );
         goto decode_success;
      }
      break;

   case 0x3D:
      
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_VEX_NDS_128_AnySimdPfx_0F_WIG_simple(
                    uses_vvvv, vbi, pfx, delta, "vpmaxsd", Iop_Max32Sx4 );
         goto decode_success;
      }
      
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_VEX_NDS_256_AnySimdPfx_0F_WIG_simple(
                    uses_vvvv, vbi, pfx, delta, "vpmaxsd", Iop_Max32Sx8 );
         goto decode_success;
      }
      break;

   case 0x3E:
      
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_VEX_NDS_128_AnySimdPfx_0F_WIG_simple(
                    uses_vvvv, vbi, pfx, delta, "vpmaxuw", Iop_Max16Ux8 );
         goto decode_success;
      }
      
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_VEX_NDS_256_AnySimdPfx_0F_WIG_simple(
                    uses_vvvv, vbi, pfx, delta, "vpmaxuw", Iop_Max16Ux16 );
         goto decode_success;
      }
      break;

   case 0x3F:
      
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_VEX_NDS_128_AnySimdPfx_0F_WIG_simple(
                    uses_vvvv, vbi, pfx, delta, "vpmaxud", Iop_Max32Ux4 );
         goto decode_success;
      }
      
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_VEX_NDS_256_AnySimdPfx_0F_WIG_simple(
                    uses_vvvv, vbi, pfx, delta, "vpmaxud", Iop_Max32Ux8 );
         goto decode_success;
      }
      break;

   case 0x40:
      
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_VEX_NDS_128_AnySimdPfx_0F_WIG_simple(
                    uses_vvvv, vbi, pfx, delta, "vpmulld", Iop_Mul32x4 );
         goto decode_success;
      }
      
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_VEX_NDS_256_AnySimdPfx_0F_WIG_simple(
                    uses_vvvv, vbi, pfx, delta, "vpmulld", Iop_Mul32x8 );
         goto decode_success;
      }
      break;

   case 0x41:
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_PHMINPOSUW_128( vbi, pfx, delta, True );
         goto decode_success;
      } 
      break;

   case 0x45:
      
      
      if (have66noF2noF3(pfx) && 0==getRexW(pfx)) {
         delta = dis_AVX_var_shiftV_byE( vbi, pfx, delta, "vpsrlvd",
                                         Iop_Shr32, 1==getVexL(pfx) );
         *uses_vvvv = True;
         goto decode_success;
      }
      
      
      if (have66noF2noF3(pfx) && 1==getRexW(pfx)) {
         delta = dis_AVX_var_shiftV_byE( vbi, pfx, delta, "vpsrlvq",
                                         Iop_Shr64, 1==getVexL(pfx) );
         *uses_vvvv = True;
         goto decode_success;
      }
      break;

   case 0x46:
      
      
      if (have66noF2noF3(pfx) && 0==getRexW(pfx)) {
         delta = dis_AVX_var_shiftV_byE( vbi, pfx, delta, "vpsravd",
                                         Iop_Sar32, 1==getVexL(pfx) );
         *uses_vvvv = True;
         goto decode_success;
      }
      break;

   case 0x47:
      
      
      if (have66noF2noF3(pfx) && 0==getRexW(pfx)) {
         delta = dis_AVX_var_shiftV_byE( vbi, pfx, delta, "vpsllvd",
                                         Iop_Shl32, 1==getVexL(pfx) );
         *uses_vvvv = True;
         goto decode_success;
      }
      
      
      if (have66noF2noF3(pfx) && 1==getRexW(pfx)) {
         delta = dis_AVX_var_shiftV_byE( vbi, pfx, delta, "vpsllvq",
                                         Iop_Shl64, 1==getVexL(pfx) );
         *uses_vvvv = True;
         goto decode_success;
      }
      break;

   case 0x58:
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)
          && 0==getRexW(pfx)) {
         UChar modrm = getUChar(delta);
         UInt  rG    = gregOfRexRM(pfx, modrm);
         IRTemp t32 = newTemp(Ity_I32);
         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx, modrm);
            delta++;
            DIP("vpbroadcastd %s,%s\n", nameXMMReg(rE), nameXMMReg(rG));
            assign(t32, getXMMRegLane32(rE, 0));
         } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 0 );
            delta += alen;
            DIP("vpbroadcastd %s,%s\n", dis_buf, nameXMMReg(rG));
            assign(t32, loadLE(Ity_I32, mkexpr(addr)));
         }
         IRTemp t64 = newTemp(Ity_I64);
         assign(t64, binop(Iop_32HLto64, mkexpr(t32), mkexpr(t32)));
         IRExpr* res = binop(Iop_64HLtoV128, mkexpr(t64), mkexpr(t64));
         putYMMRegLoAndZU(rG, res);
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)
          && 0==getRexW(pfx)) {
         UChar modrm = getUChar(delta);
         UInt  rG    = gregOfRexRM(pfx, modrm);
         IRTemp t32 = newTemp(Ity_I32);
         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx, modrm);
            delta++;
            DIP("vpbroadcastd %s,%s\n", nameXMMReg(rE), nameYMMReg(rG));
            assign(t32, getXMMRegLane32(rE, 0));
         } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 0 );
            delta += alen;
            DIP("vpbroadcastd %s,%s\n", dis_buf, nameYMMReg(rG));
            assign(t32, loadLE(Ity_I32, mkexpr(addr)));
         }
         IRTemp t64 = newTemp(Ity_I64);
         assign(t64, binop(Iop_32HLto64, mkexpr(t32), mkexpr(t32)));
         IRExpr* res = IRExpr_Qop(Iop_64x4toV256, mkexpr(t64), mkexpr(t64),
                                                  mkexpr(t64), mkexpr(t64));
         putYMMReg(rG, res);
         goto decode_success;
      }
      break;

   case 0x59:
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)
          && 0==getRexW(pfx)) {
         UChar modrm = getUChar(delta);
         UInt  rG    = gregOfRexRM(pfx, modrm);
         IRTemp t64 = newTemp(Ity_I64);
         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx, modrm);
            delta++;
            DIP("vpbroadcastq %s,%s\n", nameXMMReg(rE), nameXMMReg(rG));
            assign(t64, getXMMRegLane64(rE, 0));
         } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 0 );
            delta += alen;
            DIP("vpbroadcastq %s,%s\n", dis_buf, nameXMMReg(rG));
            assign(t64, loadLE(Ity_I64, mkexpr(addr)));
         }
         IRExpr* res = binop(Iop_64HLtoV128, mkexpr(t64), mkexpr(t64));
         putYMMRegLoAndZU(rG, res);
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)
          && 0==getRexW(pfx)) {
         UChar modrm = getUChar(delta);
         UInt  rG    = gregOfRexRM(pfx, modrm);
         IRTemp t64 = newTemp(Ity_I64);
         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx, modrm);
            delta++;
            DIP("vpbroadcastq %s,%s\n", nameXMMReg(rE), nameYMMReg(rG));
            assign(t64, getXMMRegLane64(rE, 0));
         } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 0 );
            delta += alen;
            DIP("vpbroadcastq %s,%s\n", dis_buf, nameYMMReg(rG));
            assign(t64, loadLE(Ity_I64, mkexpr(addr)));
         }
         IRExpr* res = IRExpr_Qop(Iop_64x4toV256, mkexpr(t64), mkexpr(t64),
                                                  mkexpr(t64), mkexpr(t64));
         putYMMReg(rG, res);
         goto decode_success;
      }
      break;

   case 0x5A:
      
      if (have66noF2noF3(pfx)
          && 1==getVexL(pfx)
          && !epartIsReg(getUChar(delta))) {
         UChar modrm = getUChar(delta);
         UInt  rG    = gregOfRexRM(pfx, modrm);
         addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 0 );
         delta += alen;
         DIP("vbroadcasti128 %s,%s\n", dis_buf, nameYMMReg(rG));
         IRTemp t128 = newTemp(Ity_V128);
         assign(t128, loadLE(Ity_V128, mkexpr(addr)));
         putYMMReg( rG, binop(Iop_V128HLtoV256, mkexpr(t128), mkexpr(t128)) );
         goto decode_success;
      }
      break;

   case 0x78:
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)
          && 0==getRexW(pfx)) {
         UChar modrm = getUChar(delta);
         UInt  rG    = gregOfRexRM(pfx, modrm);
         IRTemp t8   = newTemp(Ity_I8);
         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx, modrm);
            delta++;
            DIP("vpbroadcastb %s,%s\n", nameXMMReg(rE), nameXMMReg(rG));
            assign(t8, unop(Iop_32to8, getXMMRegLane32(rE, 0)));
         } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 0 );
            delta += alen;
            DIP("vpbroadcastb %s,%s\n", dis_buf, nameXMMReg(rG));
            assign(t8, loadLE(Ity_I8, mkexpr(addr)));
         }
         IRTemp t16 = newTemp(Ity_I16);
         assign(t16, binop(Iop_8HLto16, mkexpr(t8), mkexpr(t8)));
         IRTemp t32 = newTemp(Ity_I32);
         assign(t32, binop(Iop_16HLto32, mkexpr(t16), mkexpr(t16)));
         IRTemp t64 = newTemp(Ity_I64);
         assign(t64, binop(Iop_32HLto64, mkexpr(t32), mkexpr(t32)));
         IRExpr* res = binop(Iop_64HLtoV128, mkexpr(t64), mkexpr(t64));
         putYMMRegLoAndZU(rG, res);
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)
          && 0==getRexW(pfx)) {
         UChar modrm = getUChar(delta);
         UInt  rG    = gregOfRexRM(pfx, modrm);
         IRTemp t8   = newTemp(Ity_I8);
         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx, modrm);
            delta++;
            DIP("vpbroadcastb %s,%s\n", nameXMMReg(rE), nameYMMReg(rG));
            assign(t8, unop(Iop_32to8, getXMMRegLane32(rE, 0)));
         } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 0 );
            delta += alen;
            DIP("vpbroadcastb %s,%s\n", dis_buf, nameYMMReg(rG));
            assign(t8, loadLE(Ity_I8, mkexpr(addr)));
         }
         IRTemp t16 = newTemp(Ity_I16);
         assign(t16, binop(Iop_8HLto16, mkexpr(t8), mkexpr(t8)));
         IRTemp t32 = newTemp(Ity_I32);
         assign(t32, binop(Iop_16HLto32, mkexpr(t16), mkexpr(t16)));
         IRTemp t64 = newTemp(Ity_I64);
         assign(t64, binop(Iop_32HLto64, mkexpr(t32), mkexpr(t32)));
         IRExpr* res = IRExpr_Qop(Iop_64x4toV256, mkexpr(t64), mkexpr(t64),
                                                  mkexpr(t64), mkexpr(t64));
         putYMMReg(rG, res);
         goto decode_success;
      }
      break;

   case 0x79:
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)
          && 0==getRexW(pfx)) {
         UChar modrm = getUChar(delta);
         UInt  rG    = gregOfRexRM(pfx, modrm);
         IRTemp t16  = newTemp(Ity_I16);
         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx, modrm);
            delta++;
            DIP("vpbroadcastw %s,%s\n", nameXMMReg(rE), nameXMMReg(rG));
            assign(t16, unop(Iop_32to16, getXMMRegLane32(rE, 0)));
         } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 0 );
            delta += alen;
            DIP("vpbroadcastw %s,%s\n", dis_buf, nameXMMReg(rG));
            assign(t16, loadLE(Ity_I16, mkexpr(addr)));
         }
         IRTemp t32 = newTemp(Ity_I32);
         assign(t32, binop(Iop_16HLto32, mkexpr(t16), mkexpr(t16)));
         IRTemp t64 = newTemp(Ity_I64);
         assign(t64, binop(Iop_32HLto64, mkexpr(t32), mkexpr(t32)));
         IRExpr* res = binop(Iop_64HLtoV128, mkexpr(t64), mkexpr(t64));
         putYMMRegLoAndZU(rG, res);
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)
          && 0==getRexW(pfx)) {
         UChar modrm = getUChar(delta);
         UInt  rG    = gregOfRexRM(pfx, modrm);
         IRTemp t16  = newTemp(Ity_I16);
         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx, modrm);
            delta++;
            DIP("vpbroadcastw %s,%s\n", nameXMMReg(rE), nameYMMReg(rG));
            assign(t16, unop(Iop_32to16, getXMMRegLane32(rE, 0)));
         } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 0 );
            delta += alen;
            DIP("vpbroadcastw %s,%s\n", dis_buf, nameYMMReg(rG));
            assign(t16, loadLE(Ity_I16, mkexpr(addr)));
         }
         IRTemp t32 = newTemp(Ity_I32);
         assign(t32, binop(Iop_16HLto32, mkexpr(t16), mkexpr(t16)));
         IRTemp t64 = newTemp(Ity_I64);
         assign(t64, binop(Iop_32HLto64, mkexpr(t32), mkexpr(t32)));
         IRExpr* res = IRExpr_Qop(Iop_64x4toV256, mkexpr(t64), mkexpr(t64),
                                                  mkexpr(t64), mkexpr(t64));
         putYMMReg(rG, res);
         goto decode_success;
      }
      break;

   case 0x8C:
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)
          && 0==getRexW(pfx) && !epartIsReg(getUChar(delta))) {
         delta = dis_VMASKMOV( uses_vvvv, vbi, pfx, delta, "vpmaskmovd",
                               False, Ity_I32, True );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)
          && 0==getRexW(pfx) && !epartIsReg(getUChar(delta))) {
         delta = dis_VMASKMOV( uses_vvvv, vbi, pfx, delta, "vpmaskmovd",
                               True, Ity_I32, True );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)
          && 1==getRexW(pfx) && !epartIsReg(getUChar(delta))) {
         delta = dis_VMASKMOV( uses_vvvv, vbi, pfx, delta, "vpmaskmovq",
                               False, Ity_I64, True );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)
          && 1==getRexW(pfx) && !epartIsReg(getUChar(delta))) {
         delta = dis_VMASKMOV( uses_vvvv, vbi, pfx, delta, "vpmaskmovq",
                               True, Ity_I64, True );
         goto decode_success;
      }
      break;

   case 0x8E:
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)
          && 0==getRexW(pfx) && !epartIsReg(getUChar(delta))) {
         delta = dis_VMASKMOV( uses_vvvv, vbi, pfx, delta, "vpmaskmovd",
                               False, Ity_I32, False );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)
          && 0==getRexW(pfx) && !epartIsReg(getUChar(delta))) {
         delta = dis_VMASKMOV( uses_vvvv, vbi, pfx, delta, "vpmaskmovd",
                               True, Ity_I32, False );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)
          && 1==getRexW(pfx) && !epartIsReg(getUChar(delta))) {
         delta = dis_VMASKMOV( uses_vvvv, vbi, pfx, delta, "vpmaskmovq",
                               False, Ity_I64, False );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)
          && 1==getRexW(pfx) && !epartIsReg(getUChar(delta))) {
         delta = dis_VMASKMOV( uses_vvvv, vbi, pfx, delta, "vpmaskmovq",
                               True, Ity_I64, False );
         goto decode_success;
      }
      break;

   case 0x90:
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)
          && 0 == getRexW(pfx) && !epartIsReg(getUChar(delta))) {
         Long delta0 = delta;
         delta = dis_VGATHER( uses_vvvv, vbi, pfx, delta, "vpgatherdd",
                              False, False, Ity_I32 );
         if (delta != delta0)
            goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)
          && 0 == getRexW(pfx) && !epartIsReg(getUChar(delta))) {
         Long delta0 = delta;
         delta = dis_VGATHER( uses_vvvv, vbi, pfx, delta, "vpgatherdd",
                              True, False, Ity_I32 );
         if (delta != delta0)
            goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)
          && 1 == getRexW(pfx) && !epartIsReg(getUChar(delta))) {
         Long delta0 = delta;
         delta = dis_VGATHER( uses_vvvv, vbi, pfx, delta, "vpgatherdq",
                              False, False, Ity_I64 );
         if (delta != delta0)
            goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)
          && 1 == getRexW(pfx) && !epartIsReg(getUChar(delta))) {
         Long delta0 = delta;
         delta = dis_VGATHER( uses_vvvv, vbi, pfx, delta, "vpgatherdq",
                              True, False, Ity_I64 );
         if (delta != delta0)
            goto decode_success;
      }
      break;

   case 0x91:
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)
          && 0 == getRexW(pfx) && !epartIsReg(getUChar(delta))) {
         Long delta0 = delta;
         delta = dis_VGATHER( uses_vvvv, vbi, pfx, delta, "vpgatherqd",
                              False, True, Ity_I32 );
         if (delta != delta0)
            goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)
          && 0 == getRexW(pfx) && !epartIsReg(getUChar(delta))) {
         Long delta0 = delta;
         delta = dis_VGATHER( uses_vvvv, vbi, pfx, delta, "vpgatherqd",
                              True, True, Ity_I32 );
         if (delta != delta0)
            goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)
          && 1 == getRexW(pfx) && !epartIsReg(getUChar(delta))) {
         Long delta0 = delta;
         delta = dis_VGATHER( uses_vvvv, vbi, pfx, delta, "vpgatherqq",
                              False, True, Ity_I64 );
         if (delta != delta0)
            goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)
          && 1 == getRexW(pfx) && !epartIsReg(getUChar(delta))) {
         Long delta0 = delta;
         delta = dis_VGATHER( uses_vvvv, vbi, pfx, delta, "vpgatherqq",
                              True, True, Ity_I64 );
         if (delta != delta0)
            goto decode_success;
      }
      break;

   case 0x92:
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)
          && 0 == getRexW(pfx) && !epartIsReg(getUChar(delta))) {
         Long delta0 = delta;
         delta = dis_VGATHER( uses_vvvv, vbi, pfx, delta, "vgatherdps",
                              False, False, Ity_I32 );
         if (delta != delta0)
            goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)
          && 0 == getRexW(pfx) && !epartIsReg(getUChar(delta))) {
         Long delta0 = delta;
         delta = dis_VGATHER( uses_vvvv, vbi, pfx, delta, "vgatherdps",
                              True, False, Ity_I32 );
         if (delta != delta0)
            goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)
          && 1 == getRexW(pfx) && !epartIsReg(getUChar(delta))) {
         Long delta0 = delta;
         delta = dis_VGATHER( uses_vvvv, vbi, pfx, delta, "vgatherdpd",
                              False, False, Ity_I64 );
         if (delta != delta0)
            goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)
          && 1 == getRexW(pfx) && !epartIsReg(getUChar(delta))) {
         Long delta0 = delta;
         delta = dis_VGATHER( uses_vvvv, vbi, pfx, delta, "vgatherdpd",
                              True, False, Ity_I64 );
         if (delta != delta0)
            goto decode_success;
      }
      break;

   case 0x93:
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)
          && 0 == getRexW(pfx) && !epartIsReg(getUChar(delta))) {
         Long delta0 = delta;
         delta = dis_VGATHER( uses_vvvv, vbi, pfx, delta, "vgatherqps",
                              False, True, Ity_I32 );
         if (delta != delta0)
            goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)
          && 0 == getRexW(pfx) && !epartIsReg(getUChar(delta))) {
         Long delta0 = delta;
         delta = dis_VGATHER( uses_vvvv, vbi, pfx, delta, "vgatherqps",
                              True, True, Ity_I32 );
         if (delta != delta0)
            goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)
          && 1 == getRexW(pfx) && !epartIsReg(getUChar(delta))) {
         Long delta0 = delta;
         delta = dis_VGATHER( uses_vvvv, vbi, pfx, delta, "vgatherqpd",
                              False, True, Ity_I64 );
         if (delta != delta0)
            goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)
          && 1 == getRexW(pfx) && !epartIsReg(getUChar(delta))) {
         Long delta0 = delta;
         delta = dis_VGATHER( uses_vvvv, vbi, pfx, delta, "vgatherqpd",
                              True, True, Ity_I64 );
         if (delta != delta0)
            goto decode_success;
      }
      break;

   case 0x96 ... 0x9F:
   case 0xA6 ... 0xAF:
   case 0xB6 ... 0xBF:
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      if (have66noF2noF3(pfx)) {
         delta = dis_FMA( vbi, pfx, delta, opc );
         *uses_vvvv = True;
         goto decode_success;
      }
      break;

   case 0xDB:
   case 0xDC:
   case 0xDD:
   case 0xDE:
   case 0xDF:
      
      
      
      
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_AESx( vbi, pfx, delta, True, opc );
         if (opc != 0xDB) *uses_vvvv = True;
         goto decode_success;
      }
      break;

   case 0xF2:
      
      
      if (haveNo66noF2noF3(pfx) && 0==getVexL(pfx) && !haveREX(pfx)) {
         Int     size = getRexW(pfx) ? 8 : 4;
         IRType  ty   = szToITy(size);
         IRTemp  dst  = newTemp(ty);
         IRTemp  src1 = newTemp(ty);
         IRTemp  src2 = newTemp(ty);
         UChar   rm   = getUChar(delta);

         assign( src1, getIRegV(size,pfx) );
         if (epartIsReg(rm)) {
            assign( src2, getIRegE(size,pfx,rm) );
            DIP("andn %s,%s,%s\n", nameIRegE(size,pfx,rm),
                nameIRegV(size,pfx), nameIRegG(size,pfx,rm));
            delta++;
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            assign( src2, loadLE(ty, mkexpr(addr)) );
            DIP("andn %s,%s,%s\n", dis_buf, nameIRegV(size,pfx),
                nameIRegG(size,pfx,rm));
            delta += alen;
         }

         assign( dst, binop( mkSizedOp(ty,Iop_And8),
                             unop( mkSizedOp(ty,Iop_Not8), mkexpr(src1) ),
                             mkexpr(src2) ) );
         putIRegG( size, pfx, rm, mkexpr(dst) );
         stmt( IRStmt_Put( OFFB_CC_OP,   mkU64(size == 8
                                               ? AMD64G_CC_OP_ANDN64
                                               : AMD64G_CC_OP_ANDN32)) );
         stmt( IRStmt_Put( OFFB_CC_DEP1, widenUto64(mkexpr(dst))) );
         stmt( IRStmt_Put( OFFB_CC_DEP2, mkU64(0)) );
         *uses_vvvv = True;
         goto decode_success;
      }
      break;

   case 0xF3:
      
      
      if (haveNo66noF2noF3(pfx) && 0==getVexL(pfx)
          && !haveREX(pfx) && gregLO3ofRM(getUChar(delta)) == 3) {
         Int     size = getRexW(pfx) ? 8 : 4;
         IRType  ty   = szToITy(size);
         IRTemp  src  = newTemp(ty);
         IRTemp  dst  = newTemp(ty);
         UChar   rm   = getUChar(delta);

         if (epartIsReg(rm)) {
            assign( src, getIRegE(size,pfx,rm) );
            DIP("blsi %s,%s\n", nameIRegE(size,pfx,rm),
                nameIRegV(size,pfx));
            delta++;
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            assign( src, loadLE(ty, mkexpr(addr)) );
            DIP("blsi %s,%s\n", dis_buf, nameIRegV(size,pfx));
            delta += alen;
         }

         assign( dst, binop(mkSizedOp(ty,Iop_And8),
                            binop(mkSizedOp(ty,Iop_Sub8), mkU(ty, 0),
                                  mkexpr(src)), mkexpr(src)) );
         putIRegV( size, pfx, mkexpr(dst) );
         stmt( IRStmt_Put( OFFB_CC_OP,   mkU64(size == 8
                                               ? AMD64G_CC_OP_BLSI64
                                               : AMD64G_CC_OP_BLSI32)) );
         stmt( IRStmt_Put( OFFB_CC_DEP1, widenUto64(mkexpr(dst))) );
         stmt( IRStmt_Put( OFFB_CC_DEP2, widenUto64(mkexpr(src))) );
         *uses_vvvv = True;
         goto decode_success;
      }
      
      
      if (haveNo66noF2noF3(pfx) && 0==getVexL(pfx)
          && !haveREX(pfx) && gregLO3ofRM(getUChar(delta)) == 2) {
         Int     size = getRexW(pfx) ? 8 : 4;
         IRType  ty   = szToITy(size);
         IRTemp  src  = newTemp(ty);
         IRTemp  dst  = newTemp(ty);
         UChar   rm   = getUChar(delta);

         if (epartIsReg(rm)) {
            assign( src, getIRegE(size,pfx,rm) );
            DIP("blsmsk %s,%s\n", nameIRegE(size,pfx,rm),
                nameIRegV(size,pfx));
            delta++;
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            assign( src, loadLE(ty, mkexpr(addr)) );
            DIP("blsmsk %s,%s\n", dis_buf, nameIRegV(size,pfx));
            delta += alen;
         }

         assign( dst, binop(mkSizedOp(ty,Iop_Xor8),
                            binop(mkSizedOp(ty,Iop_Sub8), mkexpr(src),
                                  mkU(ty, 1)), mkexpr(src)) );
         putIRegV( size, pfx, mkexpr(dst) );
         stmt( IRStmt_Put( OFFB_CC_OP,   mkU64(size == 8
                                               ? AMD64G_CC_OP_BLSMSK64
                                               : AMD64G_CC_OP_BLSMSK32)) );
         stmt( IRStmt_Put( OFFB_CC_DEP1, widenUto64(mkexpr(dst))) );
         stmt( IRStmt_Put( OFFB_CC_DEP2, widenUto64(mkexpr(src))) );
         *uses_vvvv = True;
         goto decode_success;
      }
      
      
      if (haveNo66noF2noF3(pfx) && 0==getVexL(pfx)
          && !haveREX(pfx) && gregLO3ofRM(getUChar(delta)) == 1) {
         Int     size = getRexW(pfx) ? 8 : 4;
         IRType  ty   = szToITy(size);
         IRTemp  src  = newTemp(ty);
         IRTemp  dst  = newTemp(ty);
         UChar   rm   = getUChar(delta);

         if (epartIsReg(rm)) {
            assign( src, getIRegE(size,pfx,rm) );
            DIP("blsr %s,%s\n", nameIRegE(size,pfx,rm),
                nameIRegV(size,pfx));
            delta++;
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            assign( src, loadLE(ty, mkexpr(addr)) );
            DIP("blsr %s,%s\n", dis_buf, nameIRegV(size,pfx));
            delta += alen;
         }

         assign( dst, binop(mkSizedOp(ty,Iop_And8),
                            binop(mkSizedOp(ty,Iop_Sub8), mkexpr(src),
                                  mkU(ty, 1)), mkexpr(src)) );
         putIRegV( size, pfx, mkexpr(dst) );
         stmt( IRStmt_Put( OFFB_CC_OP,   mkU64(size == 8
                                               ? AMD64G_CC_OP_BLSR64
                                               : AMD64G_CC_OP_BLSR32)) );
         stmt( IRStmt_Put( OFFB_CC_DEP1, widenUto64(mkexpr(dst))) );
         stmt( IRStmt_Put( OFFB_CC_DEP2, widenUto64(mkexpr(src))) );
         *uses_vvvv = True;
         goto decode_success;
      }
      break;

   case 0xF5:
      
      
      if (haveNo66noF2noF3(pfx) && 0==getVexL(pfx) && !haveREX(pfx)) {
         Int     size  = getRexW(pfx) ? 8 : 4;
         IRType  ty    = szToITy(size);
         IRTemp  dst   = newTemp(ty);
         IRTemp  src1  = newTemp(ty);
         IRTemp  src2  = newTemp(ty);
         IRTemp  start = newTemp(Ity_I8);
         IRTemp  cond  = newTemp(Ity_I1);
         UChar   rm    = getUChar(delta);

         assign( src2, getIRegV(size,pfx) );
         if (epartIsReg(rm)) {
            assign( src1, getIRegE(size,pfx,rm) );
            DIP("bzhi %s,%s,%s\n", nameIRegV(size,pfx),
                nameIRegE(size,pfx,rm), nameIRegG(size,pfx,rm));
            delta++;
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            assign( src1, loadLE(ty, mkexpr(addr)) );
            DIP("bzhi %s,%s,%s\n", nameIRegV(size,pfx), dis_buf,
                nameIRegG(size,pfx,rm));
            delta += alen;
         }

         assign( start, narrowTo( Ity_I8, mkexpr(src2) ) );
         assign( cond, binop(Iop_CmpLT32U,
                             unop(Iop_8Uto32, mkexpr(start)),
                             mkU32(8*size)) );
         assign( dst,
                 IRExpr_ITE(
                    mkexpr(cond),
                    IRExpr_ITE(
                       binop(Iop_CmpEQ8, mkexpr(start), mkU8(0)),
                       mkU(ty, 0),
                       binop(
                          mkSizedOp(ty,Iop_Shr8),
                          binop(
                             mkSizedOp(ty,Iop_Shl8),
                             mkexpr(src1),
                             binop(Iop_Sub8, mkU8(8*size), mkexpr(start))
                          ),
                          binop(Iop_Sub8, mkU8(8*size), mkexpr(start))
                       )
                    ),
                    mkexpr(src1)
                 )
               );
         putIRegG( size, pfx, rm, mkexpr(dst) );
         stmt( IRStmt_Put( OFFB_CC_OP,   mkU64(size == 8
                                               ? AMD64G_CC_OP_BLSR64
                                               : AMD64G_CC_OP_BLSR32)) );
         stmt( IRStmt_Put( OFFB_CC_DEP1, widenUto64(mkexpr(dst))) );
         stmt( IRStmt_Put( OFFB_CC_DEP2, widenUto64(mkexpr(cond))) );
         *uses_vvvv = True;
         goto decode_success;
      }
      
      
      if (haveF2no66noF3(pfx) && 0==getVexL(pfx) && !haveREX(pfx)) {
         Int     size = getRexW(pfx) ? 8 : 4;
         IRType  ty   = szToITy(size);
         IRTemp  src  = newTemp(ty);
         IRTemp  mask = newTemp(ty);
         UChar   rm   = getUChar(delta);

         assign( src, getIRegV(size,pfx) );
         if (epartIsReg(rm)) {
            assign( mask, getIRegE(size,pfx,rm) );
            DIP("pdep %s,%s,%s\n", nameIRegE(size,pfx,rm),
                nameIRegV(size,pfx), nameIRegG(size,pfx,rm));
            delta++;
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            assign( mask, loadLE(ty, mkexpr(addr)) );
            DIP("pdep %s,%s,%s\n", dis_buf, nameIRegV(size,pfx),
                nameIRegG(size,pfx,rm));
            delta += alen;
         }

         IRExpr** args = mkIRExprVec_2( widenUto64(mkexpr(src)),
                                        widenUto64(mkexpr(mask)) );
         putIRegG( size, pfx, rm,
                   narrowTo(ty, mkIRExprCCall(Ity_I64, 0,
                                              "amd64g_calculate_pdep",
                                              &amd64g_calculate_pdep, args)) );
         *uses_vvvv = True;
         
         goto decode_success;
      }
      
      
      if (haveF3no66noF2(pfx) && 0==getVexL(pfx) && !haveREX(pfx)) {
         Int     size = getRexW(pfx) ? 8 : 4;
         IRType  ty   = szToITy(size);
         IRTemp  src  = newTemp(ty);
         IRTemp  mask = newTemp(ty);
         UChar   rm   = getUChar(delta);

         assign( src, getIRegV(size,pfx) );
         if (epartIsReg(rm)) {
            assign( mask, getIRegE(size,pfx,rm) );
            DIP("pext %s,%s,%s\n", nameIRegE(size,pfx,rm),
                nameIRegV(size,pfx), nameIRegG(size,pfx,rm));
            delta++;
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            assign( mask, loadLE(ty, mkexpr(addr)) );
            DIP("pext %s,%s,%s\n", dis_buf, nameIRegV(size,pfx),
                nameIRegG(size,pfx,rm));
            delta += alen;
         }

         IRExpr* masked = binop(mkSizedOp(ty,Iop_And8),
                                mkexpr(src), mkexpr(mask));
         IRExpr** args = mkIRExprVec_2( widenUto64(masked),
                                        widenUto64(mkexpr(mask)) );
         putIRegG( size, pfx, rm,
                   narrowTo(ty, mkIRExprCCall(Ity_I64, 0,
                                              "amd64g_calculate_pext",
                                              &amd64g_calculate_pext, args)) );
         *uses_vvvv = True;
         
         goto decode_success;
      }
      break;

   case 0xF6:
      
      
      if (haveF2no66noF3(pfx) && 0==getVexL(pfx) && !haveREX(pfx)) {
         Int     size = getRexW(pfx) ? 8 : 4;
         IRType  ty   = szToITy(size);
         IRTemp  src1 = newTemp(ty);
         IRTemp  src2 = newTemp(ty);
         IRTemp  res  = newTemp(size == 8 ? Ity_I128 : Ity_I64);
         UChar   rm   = getUChar(delta);

         assign( src1, getIRegRDX(size) );
         if (epartIsReg(rm)) {
            assign( src2, getIRegE(size,pfx,rm) );
            DIP("mulx %s,%s,%s\n", nameIRegE(size,pfx,rm),
                nameIRegV(size,pfx), nameIRegG(size,pfx,rm));
            delta++;
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            assign( src2, loadLE(ty, mkexpr(addr)) );
            DIP("mulx %s,%s,%s\n", dis_buf, nameIRegV(size,pfx),
                nameIRegG(size,pfx,rm));
            delta += alen;
         }

         assign( res, binop(size == 8 ? Iop_MullU64 : Iop_MullU32,
                            mkexpr(src1), mkexpr(src2)) );
         putIRegV( size, pfx,
                   unop(size == 8 ? Iop_128to64 : Iop_64to32, mkexpr(res)) );
         putIRegG( size, pfx, rm,
                   unop(size == 8 ? Iop_128HIto64 : Iop_64HIto32,
                        mkexpr(res)) );
         *uses_vvvv = True;
         
         goto decode_success;
      }
      break;

   case 0xF7:
      
      
      if (haveF3no66noF2(pfx) && 0==getVexL(pfx) && !haveREX(pfx)) {
         delta = dis_SHIFTX( uses_vvvv, vbi, pfx, delta, "sarx", Iop_Sar8 );
         goto decode_success;
      }
      
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx) && !haveREX(pfx)) {
         delta = dis_SHIFTX( uses_vvvv, vbi, pfx, delta, "shlx", Iop_Shl8 );
         goto decode_success;
      }
      
      
      if (haveF2no66noF3(pfx) && 0==getVexL(pfx) && !haveREX(pfx)) {
         delta = dis_SHIFTX( uses_vvvv, vbi, pfx, delta, "shrx", Iop_Shr8 );
         goto decode_success;
      }
      
      
      if (haveNo66noF2noF3(pfx) && 0==getVexL(pfx) && !haveREX(pfx)) {
         Int     size  = getRexW(pfx) ? 8 : 4;
         IRType  ty    = szToITy(size);
         IRTemp  dst   = newTemp(ty);
         IRTemp  src1  = newTemp(ty);
         IRTemp  src2  = newTemp(ty);
         IRTemp  stle  = newTemp(Ity_I16);
         IRTemp  start = newTemp(Ity_I8);
         IRTemp  len   = newTemp(Ity_I8);
         UChar   rm    = getUChar(delta);

         assign( src2, getIRegV(size,pfx) );
         if (epartIsReg(rm)) {
            assign( src1, getIRegE(size,pfx,rm) );
            DIP("bextr %s,%s,%s\n", nameIRegV(size,pfx),
                nameIRegE(size,pfx,rm), nameIRegG(size,pfx,rm));
            delta++;
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            assign( src1, loadLE(ty, mkexpr(addr)) );
            DIP("bextr %s,%s,%s\n", nameIRegV(size,pfx), dis_buf,
                nameIRegG(size,pfx,rm));
            delta += alen;
         }

         assign( stle, narrowTo( Ity_I16, mkexpr(src2) ) );
         assign( start, unop( Iop_16to8, mkexpr(stle) ) );
         assign( len, unop( Iop_16HIto8, mkexpr(stle) ) );
         assign( dst,
                 IRExpr_ITE(
                    binop(Iop_CmpLT32U,
                          binop(Iop_Add32,
                                unop(Iop_8Uto32, mkexpr(start)),
                                unop(Iop_8Uto32, mkexpr(len))),
                          mkU32(8*size)),
                    IRExpr_ITE(
                       binop(Iop_CmpEQ8, mkexpr(len), mkU8(0)),
                       mkU(ty, 0),
                       binop(mkSizedOp(ty,Iop_Shr8),
                             binop(mkSizedOp(ty,Iop_Shl8), mkexpr(src1),
                                   binop(Iop_Sub8,
                                         binop(Iop_Sub8, mkU8(8*size),
                                               mkexpr(start)),
                                         mkexpr(len))),
                             binop(Iop_Sub8, mkU8(8*size),
                                   mkexpr(len)))
                    ),
                    IRExpr_ITE(
                       binop(Iop_CmpLT32U,
                             unop(Iop_8Uto32, mkexpr(start)),
                             mkU32(8*size)),
                       binop(mkSizedOp(ty,Iop_Shr8), mkexpr(src1),
                             mkexpr(start)),
                       mkU(ty, 0)
                    )
                 )
               );
         putIRegG( size, pfx, rm, mkexpr(dst) );
         stmt( IRStmt_Put( OFFB_CC_OP,   mkU64(size == 8
                                               ? AMD64G_CC_OP_ANDN64
                                               : AMD64G_CC_OP_ANDN32)) );
         stmt( IRStmt_Put( OFFB_CC_DEP1, widenUto64(mkexpr(dst))) );
         stmt( IRStmt_Put( OFFB_CC_DEP2, mkU64(0)) );
         *uses_vvvv = True;
         goto decode_success;
      }
      break;

   default:
      break;

   }

  
   return deltaIN;

  decode_success:
   return delta;
}



static IRTemp math_VPERMILPS_128 ( IRTemp sV, UInt imm8 )
{
   vassert(imm8 < 256);
   IRTemp s3, s2, s1, s0;
   s3 = s2 = s1 = s0 = IRTemp_INVALID;
   breakupV128to32s( sV, &s3, &s2, &s1, &s0 );
#  define SEL(_nn) (((_nn)==0) ? s0 : ((_nn)==1) ? s1 \
                                    : ((_nn)==2) ? s2 : s3)
   IRTemp res = newTemp(Ity_V128);
   assign(res, mkV128from32s( SEL((imm8 >> 6) & 3),
                              SEL((imm8 >> 4) & 3),
                              SEL((imm8 >> 2) & 3),
                              SEL((imm8 >> 0) & 3) ));
#  undef SEL
   return res;
}

__attribute__((noinline))
static
Long dis_ESC_0F3A__VEX (
        DisResult* dres,
           Bool*      uses_vvvv,
        Bool         (*resteerOkFn) ( void*, Addr ),
        Bool         resteerCisOk,
        void*        callback_opaque,
        const VexArchInfo* archinfo,
        const VexAbiInfo*  vbi,
        Prefix pfx, Int sz, Long deltaIN 
     )
{
   IRTemp addr  = IRTemp_INVALID;
   Int    alen  = 0;
   HChar  dis_buf[50];
   Long   delta = deltaIN;
   UChar  opc   = getUChar(delta);
   delta++;
   *uses_vvvv = False;

   switch (opc) {

   case 0x00:
   case 0x01:
      
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)
          && 1==getRexW(pfx)) {
         UChar  modrm = getUChar(delta);
         UInt   imm8  = 0;
         UInt   rG    = gregOfRexRM(pfx, modrm);
         IRTemp sV    = newTemp(Ity_V256);
         const HChar *name  = opc == 0 ? "vpermq" : "vpermpd";
         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx, modrm);
            delta += 1;
            imm8 = getUChar(delta);
            DIP("%s $%u,%s,%s\n",
                name, imm8, nameYMMReg(rE), nameYMMReg(rG));
            assign(sV, getYMMReg(rE));
         } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 1 );
            delta += alen;
            imm8 = getUChar(delta);
            DIP("%s $%u,%s,%s\n",
                name, imm8, dis_buf, nameYMMReg(rG));
            assign(sV, loadLE(Ity_V256, mkexpr(addr)));
         }
         delta++;
         IRTemp s[4];
         s[3] = s[2] = s[1] = s[0] = IRTemp_INVALID;
         breakupV256to64s(sV, &s[3], &s[2], &s[1], &s[0]);
         IRTemp dV = newTemp(Ity_V256);
         assign(dV, IRExpr_Qop(Iop_64x4toV256,
                               mkexpr(s[(imm8 >> 6) & 3]),
                               mkexpr(s[(imm8 >> 4) & 3]),
                               mkexpr(s[(imm8 >> 2) & 3]),
                               mkexpr(s[(imm8 >> 0) & 3])));
         putYMMReg(rG, mkexpr(dV));
         goto decode_success;
      }
      break;

   case 0x02:
      
      if (have66noF2noF3(pfx)
          && 0==getVexL(pfx) && 0==getRexW(pfx)) {
         UChar  modrm = getUChar(delta);
         UInt   imm8  = 0;
         UInt   rG    = gregOfRexRM(pfx, modrm);
         UInt   rV    = getVexNvvvv(pfx);
         IRTemp sV    = newTemp(Ity_V128);
         IRTemp dV    = newTemp(Ity_V128);
         UInt   i;
         IRTemp s[4], d[4];
         assign(sV, getXMMReg(rV));
         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx, modrm);
            delta += 1;
            imm8 = getUChar(delta);
            DIP("vpblendd $%u,%s,%s,%s\n",
                imm8, nameXMMReg(rE), nameXMMReg(rV), nameXMMReg(rG));
            assign(dV, getXMMReg(rE));
         } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 1 );
            delta += alen;
            imm8 = getUChar(delta);
            DIP("vpblendd $%u,%s,%s,%s\n",
                imm8, dis_buf, nameXMMReg(rV), nameXMMReg(rG));
            assign(dV, loadLE(Ity_V128, mkexpr(addr)));
         }
         delta++;
         for (i = 0; i < 4; i++) {
            s[i] = IRTemp_INVALID;
            d[i] = IRTemp_INVALID;
         }
         breakupV128to32s( sV, &s[3], &s[2], &s[1], &s[0] );
         breakupV128to32s( dV, &d[3], &d[2], &d[1], &d[0] );
         for (i = 0; i < 4; i++)
            putYMMRegLane32(rG, i, mkexpr((imm8 & (1<<i)) ? d[i] : s[i]));
         putYMMRegLane128(rG, 1, mkV128(0));
         *uses_vvvv = True;
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx)
          && 1==getVexL(pfx) && 0==getRexW(pfx)) {
         UChar  modrm = getUChar(delta);
         UInt   imm8  = 0;
         UInt   rG    = gregOfRexRM(pfx, modrm);
         UInt   rV    = getVexNvvvv(pfx);
         IRTemp sV    = newTemp(Ity_V256);
         IRTemp dV    = newTemp(Ity_V256);
         UInt   i;
         IRTemp s[8], d[8];
         assign(sV, getYMMReg(rV));
         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx, modrm);
            delta += 1;
            imm8 = getUChar(delta);
            DIP("vpblendd $%u,%s,%s,%s\n",
                imm8, nameYMMReg(rE), nameYMMReg(rV), nameYMMReg(rG));
            assign(dV, getYMMReg(rE));
         } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 1 );
            delta += alen;
            imm8 = getUChar(delta);
            DIP("vpblendd $%u,%s,%s,%s\n",
                imm8, dis_buf, nameYMMReg(rV), nameYMMReg(rG));
            assign(dV, loadLE(Ity_V256, mkexpr(addr)));
         }
         delta++;
         for (i = 0; i < 8; i++) {
            s[i] = IRTemp_INVALID;
            d[i] = IRTemp_INVALID;
         }
         breakupV256to32s( sV, &s[7], &s[6], &s[5], &s[4],
                               &s[3], &s[2], &s[1], &s[0] );
         breakupV256to32s( dV, &d[7], &d[6], &d[5], &d[4],
                               &d[3], &d[2], &d[1], &d[0] );
         for (i = 0; i < 8; i++)
            putYMMRegLane32(rG, i, mkexpr((imm8 & (1<<i)) ? d[i] : s[i]));
         *uses_vvvv = True;
         goto decode_success;
      }
      break;

   case 0x04:
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         UChar  modrm = getUChar(delta);
         UInt   imm8  = 0;
         UInt   rG    = gregOfRexRM(pfx, modrm);
         IRTemp sV    = newTemp(Ity_V256);
         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx, modrm);
            delta += 1;
            imm8 = getUChar(delta);
            DIP("vpermilps $%u,%s,%s\n",
                imm8, nameYMMReg(rE), nameYMMReg(rG));
            assign(sV, getYMMReg(rE));
         } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 1 );
            delta += alen;
            imm8 = getUChar(delta);
            DIP("vpermilps $%u,%s,%s\n",
                imm8, dis_buf, nameYMMReg(rG));
            assign(sV, loadLE(Ity_V256, mkexpr(addr)));
         }
         delta++;
         IRTemp  sVhi = IRTemp_INVALID, sVlo = IRTemp_INVALID;
         breakupV256toV128s( sV, &sVhi, &sVlo );
         IRTemp  dVhi = math_VPERMILPS_128( sVhi, imm8 );
         IRTemp  dVlo = math_VPERMILPS_128( sVlo, imm8 );
         IRExpr* res  = binop(Iop_V128HLtoV256, mkexpr(dVhi), mkexpr(dVlo));
         putYMMReg(rG, res);
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         UChar  modrm = getUChar(delta);
         UInt   imm8  = 0;
         UInt   rG    = gregOfRexRM(pfx, modrm);
         IRTemp sV    = newTemp(Ity_V128);
         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx, modrm);
            delta += 1;
            imm8 = getUChar(delta);
            DIP("vpermilps $%u,%s,%s\n",
                imm8, nameXMMReg(rE), nameXMMReg(rG));
            assign(sV, getXMMReg(rE));
         } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 1 );
            delta += alen;
            imm8 = getUChar(delta);
            DIP("vpermilps $%u,%s,%s\n",
                imm8, dis_buf, nameXMMReg(rG));
            assign(sV, loadLE(Ity_V128, mkexpr(addr)));
         }
         delta++;
         putYMMRegLoAndZU(rG, mkexpr ( math_VPERMILPS_128 ( sV, imm8 ) ) );
         goto decode_success;
      }
      break;

   case 0x05:
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         UChar  modrm = getUChar(delta);
         UInt   imm8  = 0;
         UInt   rG    = gregOfRexRM(pfx, modrm);
         IRTemp sV    = newTemp(Ity_V128);
         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx, modrm);
            delta += 1;
            imm8 = getUChar(delta);
            DIP("vpermilpd $%u,%s,%s\n",
                imm8, nameXMMReg(rE), nameXMMReg(rG));
            assign(sV, getXMMReg(rE));
         } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 1 );
            delta += alen;
            imm8 = getUChar(delta);
            DIP("vpermilpd $%u,%s,%s\n",
                imm8, dis_buf, nameXMMReg(rG));
            assign(sV, loadLE(Ity_V128, mkexpr(addr)));
         }
         delta++;
         IRTemp s1 = newTemp(Ity_I64);
         IRTemp s0 = newTemp(Ity_I64);
         assign(s1, unop(Iop_V128HIto64, mkexpr(sV)));
         assign(s0, unop(Iop_V128to64,   mkexpr(sV)));
         IRTemp dV = newTemp(Ity_V128);
         assign(dV, binop(Iop_64HLtoV128,
                               mkexpr((imm8 & (1<<1)) ? s1 : s0),
                               mkexpr((imm8 & (1<<0)) ? s1 : s0)));
         putYMMRegLoAndZU(rG, mkexpr(dV));
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         UChar  modrm = getUChar(delta);
         UInt   imm8  = 0;
         UInt   rG    = gregOfRexRM(pfx, modrm);
         IRTemp sV    = newTemp(Ity_V256);
         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx, modrm);
            delta += 1;
            imm8 = getUChar(delta);
            DIP("vpermilpd $%u,%s,%s\n",
                imm8, nameYMMReg(rE), nameYMMReg(rG));
            assign(sV, getYMMReg(rE));
         } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 1 );
            delta += alen;
            imm8 = getUChar(delta);
            DIP("vpermilpd $%u,%s,%s\n",
                imm8, dis_buf, nameYMMReg(rG));
            assign(sV, loadLE(Ity_V256, mkexpr(addr)));
         }
         delta++;
         IRTemp s3, s2, s1, s0;
         s3 = s2 = s1 = s0 = IRTemp_INVALID;
         breakupV256to64s(sV, &s3, &s2, &s1, &s0);
         IRTemp dV = newTemp(Ity_V256);
         assign(dV, IRExpr_Qop(Iop_64x4toV256,
                               mkexpr((imm8 & (1<<3)) ? s3 : s2),
                               mkexpr((imm8 & (1<<2)) ? s3 : s2),
                               mkexpr((imm8 & (1<<1)) ? s1 : s0),
                               mkexpr((imm8 & (1<<0)) ? s1 : s0)));
         putYMMReg(rG, mkexpr(dV));
         goto decode_success;
      }
      break;

   case 0x06:
      
      if (have66noF2noF3(pfx)
          && 1==getVexL(pfx) && 0==getRexW(pfx)) {
         UChar  modrm = getUChar(delta);
         UInt   imm8  = 0;
         UInt   rG    = gregOfRexRM(pfx, modrm);
         UInt   rV    = getVexNvvvv(pfx);
         IRTemp s00   = newTemp(Ity_V128);
         IRTemp s01   = newTemp(Ity_V128);
         IRTemp s10   = newTemp(Ity_V128);
         IRTemp s11   = newTemp(Ity_V128);
         assign(s00, getYMMRegLane128(rV, 0));
         assign(s01, getYMMRegLane128(rV, 1));
         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx, modrm);
            delta += 1;
            imm8 = getUChar(delta);
            DIP("vperm2f128 $%u,%s,%s,%s\n",
                imm8, nameYMMReg(rE), nameYMMReg(rV), nameYMMReg(rG));
            assign(s10, getYMMRegLane128(rE, 0));
            assign(s11, getYMMRegLane128(rE, 1));
         } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 1 );
            delta += alen;
            imm8 = getUChar(delta);
            DIP("vperm2f128 $%u,%s,%s,%s\n",
                imm8, dis_buf, nameYMMReg(rV), nameYMMReg(rG));
            assign(s10, loadLE(Ity_V128, binop(Iop_Add64,
                                               mkexpr(addr), mkU64(0))));
            assign(s11, loadLE(Ity_V128, binop(Iop_Add64,
                                               mkexpr(addr), mkU64(16))));
         }
         delta++;
#        define SEL(_nn) (((_nn)==0) ? s00 : ((_nn)==1) ? s01 \
                                           : ((_nn)==2) ? s10 : s11)
         putYMMRegLane128(rG, 0, mkexpr(SEL((imm8 >> 0) & 3)));
         putYMMRegLane128(rG, 1, mkexpr(SEL((imm8 >> 4) & 3)));
#        undef SEL
         if (imm8 & (1<<3)) putYMMRegLane128(rG, 0, mkV128(0));
         if (imm8 & (1<<7)) putYMMRegLane128(rG, 1, mkV128(0));
         *uses_vvvv = True;
         goto decode_success;
      }
      break;

   case 0x08:
      
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         UChar  modrm = getUChar(delta);
         UInt   rG    = gregOfRexRM(pfx, modrm);
         IRTemp src   = newTemp(Ity_V128);
         IRTemp s0    = IRTemp_INVALID;
         IRTemp s1    = IRTemp_INVALID;
         IRTemp s2    = IRTemp_INVALID;
         IRTemp s3    = IRTemp_INVALID;
         IRTemp rm    = newTemp(Ity_I32);
         Int    imm   = 0;

         modrm = getUChar(delta);

         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx, modrm);
            assign( src, getXMMReg( rE ) );
            imm = getUChar(delta+1);
            if (imm & ~15) break;
            delta += 1+1;
            DIP( "vroundps $%d,%s,%s\n", imm, nameXMMReg(rE), nameXMMReg(rG) );
         } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 1 );
            assign( src, loadLE(Ity_V128, mkexpr(addr) ) );
            imm = getUChar(delta+alen);
            if (imm & ~15) break;
            delta += alen+1;
            DIP( "vroundps $%d,%s,%s\n", imm, dis_buf, nameXMMReg(rG) );
         }

         assign(rm, (imm & 4) ? get_sse_roundingmode() : mkU32(imm & 3));

         breakupV128to32s( src, &s3, &s2, &s1, &s0 );
         putYMMRegLane128( rG, 1, mkV128(0) );
#        define CVT(s) binop(Iop_RoundF32toInt, mkexpr(rm), \
                             unop(Iop_ReinterpI32asF32, mkexpr(s)))
         putYMMRegLane32F( rG, 3, CVT(s3) );
         putYMMRegLane32F( rG, 2, CVT(s2) );
         putYMMRegLane32F( rG, 1, CVT(s1) );
         putYMMRegLane32F( rG, 0, CVT(s0) );
#        undef CVT
         goto decode_success;
      }
      
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         UChar  modrm = getUChar(delta);
         UInt   rG    = gregOfRexRM(pfx, modrm);
         IRTemp src   = newTemp(Ity_V256);
         IRTemp s0    = IRTemp_INVALID;
         IRTemp s1    = IRTemp_INVALID;
         IRTemp s2    = IRTemp_INVALID;
         IRTemp s3    = IRTemp_INVALID;
         IRTemp s4    = IRTemp_INVALID;
         IRTemp s5    = IRTemp_INVALID;
         IRTemp s6    = IRTemp_INVALID;
         IRTemp s7    = IRTemp_INVALID;
         IRTemp rm    = newTemp(Ity_I32);
         Int    imm   = 0;

         modrm = getUChar(delta);

         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx, modrm);
            assign( src, getYMMReg( rE ) );
            imm = getUChar(delta+1);
            if (imm & ~15) break;
            delta += 1+1;
            DIP( "vroundps $%d,%s,%s\n", imm, nameYMMReg(rE), nameYMMReg(rG) );
         } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 1 );
            assign( src, loadLE(Ity_V256, mkexpr(addr) ) );
            imm = getUChar(delta+alen);
            if (imm & ~15) break;
            delta += alen+1;
            DIP( "vroundps $%d,%s,%s\n", imm, dis_buf, nameYMMReg(rG) );
         }

         assign(rm, (imm & 4) ? get_sse_roundingmode() : mkU32(imm & 3));

         breakupV256to32s( src, &s7, &s6, &s5, &s4, &s3, &s2, &s1, &s0 );
#        define CVT(s) binop(Iop_RoundF32toInt, mkexpr(rm), \
                             unop(Iop_ReinterpI32asF32, mkexpr(s)))
         putYMMRegLane32F( rG, 7, CVT(s7) );
         putYMMRegLane32F( rG, 6, CVT(s6) );
         putYMMRegLane32F( rG, 5, CVT(s5) );
         putYMMRegLane32F( rG, 4, CVT(s4) );
         putYMMRegLane32F( rG, 3, CVT(s3) );
         putYMMRegLane32F( rG, 2, CVT(s2) );
         putYMMRegLane32F( rG, 1, CVT(s1) );
         putYMMRegLane32F( rG, 0, CVT(s0) );
#        undef CVT
         goto decode_success;
      }

   case 0x09:
      
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         UChar  modrm = getUChar(delta);
         UInt   rG    = gregOfRexRM(pfx, modrm);
         IRTemp src   = newTemp(Ity_V128);
         IRTemp s0    = IRTemp_INVALID;
         IRTemp s1    = IRTemp_INVALID;
         IRTemp rm    = newTemp(Ity_I32);
         Int    imm   = 0;

         modrm = getUChar(delta);

         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx, modrm);
            assign( src, getXMMReg( rE ) );
            imm = getUChar(delta+1);
            if (imm & ~15) break;
            delta += 1+1;
            DIP( "vroundpd $%d,%s,%s\n", imm, nameXMMReg(rE), nameXMMReg(rG) );
         } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 1 );
            assign( src, loadLE(Ity_V128, mkexpr(addr) ) );
            imm = getUChar(delta+alen);
            if (imm & ~15) break;
            delta += alen+1;
            DIP( "vroundpd $%d,%s,%s\n", imm, dis_buf, nameXMMReg(rG) );
         }

         assign(rm, (imm & 4) ? get_sse_roundingmode() : mkU32(imm & 3));

         breakupV128to64s( src, &s1, &s0 );
         putYMMRegLane128( rG, 1, mkV128(0) );
#        define CVT(s) binop(Iop_RoundF64toInt, mkexpr(rm), \
                             unop(Iop_ReinterpI64asF64, mkexpr(s)))
         putYMMRegLane64F( rG, 1, CVT(s1) );
         putYMMRegLane64F( rG, 0, CVT(s0) );
#        undef CVT
         goto decode_success;
      }
      
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         UChar  modrm = getUChar(delta);
         UInt   rG    = gregOfRexRM(pfx, modrm);
         IRTemp src   = newTemp(Ity_V256);
         IRTemp s0    = IRTemp_INVALID;
         IRTemp s1    = IRTemp_INVALID;
         IRTemp s2    = IRTemp_INVALID;
         IRTemp s3    = IRTemp_INVALID;
         IRTemp rm    = newTemp(Ity_I32);
         Int    imm   = 0;

         modrm = getUChar(delta);

         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx, modrm);
            assign( src, getYMMReg( rE ) );
            imm = getUChar(delta+1);
            if (imm & ~15) break;
            delta += 1+1;
            DIP( "vroundpd $%d,%s,%s\n", imm, nameYMMReg(rE), nameYMMReg(rG) );
         } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 1 );
            assign( src, loadLE(Ity_V256, mkexpr(addr) ) );
            imm = getUChar(delta+alen);
            if (imm & ~15) break;
            delta += alen+1;
            DIP( "vroundps $%d,%s,%s\n", imm, dis_buf, nameYMMReg(rG) );
         }

         assign(rm, (imm & 4) ? get_sse_roundingmode() : mkU32(imm & 3));

         breakupV256to64s( src, &s3, &s2, &s1, &s0 );
#        define CVT(s) binop(Iop_RoundF64toInt, mkexpr(rm), \
                             unop(Iop_ReinterpI64asF64, mkexpr(s)))
         putYMMRegLane64F( rG, 3, CVT(s3) );
         putYMMRegLane64F( rG, 2, CVT(s2) );
         putYMMRegLane64F( rG, 1, CVT(s1) );
         putYMMRegLane64F( rG, 0, CVT(s0) );
#        undef CVT
         goto decode_success;
      }

   case 0x0A:
   case 0x0B:
      
      
      
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         UChar  modrm = getUChar(delta);
         UInt   rG    = gregOfRexRM(pfx, modrm);
         UInt   rV    = getVexNvvvv(pfx);
         Bool   isD   = opc == 0x0B;
         IRTemp src   = newTemp(isD ? Ity_F64 : Ity_F32);
         IRTemp res   = newTemp(isD ? Ity_F64 : Ity_F32);
         Int    imm   = 0;

         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx, modrm);
            assign( src, 
                    isD ? getXMMRegLane64F(rE, 0) : getXMMRegLane32F(rE, 0) );
            imm = getUChar(delta+1);
            if (imm & ~15) break;
            delta += 1+1;
            DIP( "vrounds%c $%d,%s,%s,%s\n",
                 isD ? 'd' : 's',
                 imm, nameXMMReg( rE ), nameXMMReg( rV ), nameXMMReg( rG ) );
         } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 1 );
            assign( src, loadLE( isD ? Ity_F64 : Ity_F32, mkexpr(addr) ));
            imm = getUChar(delta+alen);
            if (imm & ~15) break;
            delta += alen+1;
            DIP( "vrounds%c $%d,%s,%s,%s\n",
                 isD ? 'd' : 's',
                 imm, dis_buf, nameXMMReg( rV ), nameXMMReg( rG ) );
         }

         assign(res, binop(isD ? Iop_RoundF64toInt : Iop_RoundF32toInt,
                           (imm & 4) ? get_sse_roundingmode() 
                                     : mkU32(imm & 3),
                           mkexpr(src)) );

         if (isD)
            putXMMRegLane64F( rG, 0, mkexpr(res) );
         else {
            putXMMRegLane32F( rG, 0, mkexpr(res) );
            putXMMRegLane32F( rG, 1, getXMMRegLane32F( rV, 1 ) );
         }
         putXMMRegLane64F( rG, 1, getXMMRegLane64F( rV, 1 ) );
         putYMMRegLane128( rG, 1, mkV128(0) );
         *uses_vvvv = True;
         goto decode_success;
      }
      break;

   case 0x0C:
      
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         UChar  modrm = getUChar(delta);
         UInt   imm8;
         UInt   rG    = gregOfRexRM(pfx, modrm);
         UInt   rV    = getVexNvvvv(pfx);
         IRTemp sV    = newTemp(Ity_V256);
         IRTemp sE    = newTemp(Ity_V256);
         assign ( sV, getYMMReg(rV) );
         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx, modrm);
            delta += 1;
            imm8 = getUChar(delta);
            DIP("vblendps $%u,%s,%s,%s\n",
                imm8, nameYMMReg(rE), nameYMMReg(rV), nameYMMReg(rG));
            assign(sE, getYMMReg(rE));
         } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 1 );
            delta += alen;
            imm8 = getUChar(delta);
            DIP("vblendps $%u,%s,%s,%s\n",
                imm8, dis_buf, nameYMMReg(rV), nameYMMReg(rG));
            assign(sE, loadLE(Ity_V256, mkexpr(addr)));
         }
         delta++;
         putYMMReg( rG, 
                    mkexpr( math_BLENDPS_256( sE, sV, imm8) ) );
         *uses_vvvv = True;
         goto decode_success;
      }
      
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         UChar  modrm = getUChar(delta);
         UInt   imm8;
         UInt   rG    = gregOfRexRM(pfx, modrm);
         UInt   rV    = getVexNvvvv(pfx);
         IRTemp sV    = newTemp(Ity_V128);
         IRTemp sE    = newTemp(Ity_V128);
         assign ( sV, getXMMReg(rV) );
         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx, modrm);
            delta += 1;
            imm8 = getUChar(delta);
            DIP("vblendps $%u,%s,%s,%s\n",
                imm8, nameXMMReg(rE), nameXMMReg(rV), nameXMMReg(rG));
            assign(sE, getXMMReg(rE));
         } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 1 );
            delta += alen;
            imm8 = getUChar(delta);
            DIP("vblendps $%u,%s,%s,%s\n",
                imm8, dis_buf, nameXMMReg(rV), nameXMMReg(rG));
            assign(sE, loadLE(Ity_V128, mkexpr(addr)));
         }
         delta++;
         putYMMRegLoAndZU( rG, 
                           mkexpr( math_BLENDPS_128( sE, sV, imm8) ) );
         *uses_vvvv = True;
         goto decode_success;
      }
      break;

   case 0x0D:
      
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         UChar  modrm = getUChar(delta);
         UInt   imm8;
         UInt   rG    = gregOfRexRM(pfx, modrm);
         UInt   rV    = getVexNvvvv(pfx);
         IRTemp sV    = newTemp(Ity_V256);
         IRTemp sE    = newTemp(Ity_V256);
         assign ( sV, getYMMReg(rV) );
         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx, modrm);
            delta += 1;
            imm8 = getUChar(delta);
            DIP("vblendpd $%u,%s,%s,%s\n",
                imm8, nameYMMReg(rE), nameYMMReg(rV), nameYMMReg(rG));
            assign(sE, getYMMReg(rE));
         } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 1 );
            delta += alen;
            imm8 = getUChar(delta);
            DIP("vblendpd $%u,%s,%s,%s\n",
                imm8, dis_buf, nameYMMReg(rV), nameYMMReg(rG));
            assign(sE, loadLE(Ity_V256, mkexpr(addr)));
         }
         delta++;
         putYMMReg( rG, 
                    mkexpr( math_BLENDPD_256( sE, sV, imm8) ) );
         *uses_vvvv = True;
         goto decode_success;
      }
      
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         UChar  modrm = getUChar(delta);
         UInt   imm8;
         UInt   rG    = gregOfRexRM(pfx, modrm);
         UInt   rV    = getVexNvvvv(pfx);
         IRTemp sV    = newTemp(Ity_V128);
         IRTemp sE    = newTemp(Ity_V128);
         assign ( sV, getXMMReg(rV) );
         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx, modrm);
            delta += 1;
            imm8 = getUChar(delta);
            DIP("vblendpd $%u,%s,%s,%s\n",
                imm8, nameXMMReg(rE), nameXMMReg(rV), nameXMMReg(rG));
            assign(sE, getXMMReg(rE));
         } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 1 );
            delta += alen;
            imm8 = getUChar(delta);
            DIP("vblendpd $%u,%s,%s,%s\n",
                imm8, dis_buf, nameXMMReg(rV), nameXMMReg(rG));
            assign(sE, loadLE(Ity_V128, mkexpr(addr)));
         }
         delta++;
         putYMMRegLoAndZU( rG, 
                           mkexpr( math_BLENDPD_128( sE, sV, imm8) ) );
         *uses_vvvv = True;
         goto decode_success;
      }
      break;

   case 0x0E:
      
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         UChar  modrm = getUChar(delta);
         UInt   imm8;
         UInt   rG    = gregOfRexRM(pfx, modrm);
         UInt   rV    = getVexNvvvv(pfx);
         IRTemp sV    = newTemp(Ity_V128);
         IRTemp sE    = newTemp(Ity_V128);
         assign ( sV, getXMMReg(rV) );
         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx, modrm);
            delta += 1;
            imm8 = getUChar(delta);
            DIP("vpblendw $%u,%s,%s,%s\n",
                imm8, nameXMMReg(rE), nameXMMReg(rV), nameXMMReg(rG));
            assign(sE, getXMMReg(rE));
         } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 1 );
            delta += alen;
            imm8 = getUChar(delta);
            DIP("vpblendw $%u,%s,%s,%s\n",
                imm8, dis_buf, nameXMMReg(rV), nameXMMReg(rG));
            assign(sE, loadLE(Ity_V128, mkexpr(addr)));
         }
         delta++;
         putYMMRegLoAndZU( rG, 
                           mkexpr( math_PBLENDW_128( sE, sV, imm8) ) );
         *uses_vvvv = True;
         goto decode_success;
      }
      
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         UChar  modrm = getUChar(delta);
         UInt   imm8;
         UInt   rG    = gregOfRexRM(pfx, modrm);
         UInt   rV    = getVexNvvvv(pfx);
         IRTemp sV    = newTemp(Ity_V256);
         IRTemp sE    = newTemp(Ity_V256);
         IRTemp sVhi, sVlo, sEhi, sElo;
         sVhi = sVlo = sEhi = sElo = IRTemp_INVALID;
         assign ( sV, getYMMReg(rV) );
         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx, modrm);
            delta += 1;
            imm8 = getUChar(delta);
            DIP("vpblendw $%u,%s,%s,%s\n",
                imm8, nameYMMReg(rE), nameYMMReg(rV), nameYMMReg(rG));
            assign(sE, getYMMReg(rE));
         } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 1 );
            delta += alen;
            imm8 = getUChar(delta);
            DIP("vpblendw $%u,%s,%s,%s\n",
                imm8, dis_buf, nameYMMReg(rV), nameYMMReg(rG));
            assign(sE, loadLE(Ity_V256, mkexpr(addr)));
         }
         delta++;
         breakupV256toV128s( sV, &sVhi, &sVlo );
         breakupV256toV128s( sE, &sEhi, &sElo );
         putYMMReg( rG, binop( Iop_V128HLtoV256,
                               mkexpr( math_PBLENDW_128( sEhi, sVhi, imm8) ),
                               mkexpr( math_PBLENDW_128( sElo, sVlo, imm8) ) ) );
         *uses_vvvv = True;
         goto decode_success;
      }
      break;

   case 0x0F:
      
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         UChar  modrm = getUChar(delta);
         UInt   rG    = gregOfRexRM(pfx, modrm);
         UInt   rV    = getVexNvvvv(pfx);
         IRTemp sV    = newTemp(Ity_V128);
         IRTemp dV    = newTemp(Ity_V128);
         UInt   imm8;

         assign( dV, getXMMReg(rV) );

         if ( epartIsReg( modrm ) ) {
            UInt   rE = eregOfRexRM(pfx, modrm);
            assign( sV, getXMMReg(rE) );
            imm8 = getUChar(delta+1);
            delta += 1+1;
            DIP("vpalignr $%d,%s,%s,%s\n", imm8, nameXMMReg(rE),
                                           nameXMMReg(rV), nameXMMReg(rG));
         } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 1 );
            assign( sV, loadLE(Ity_V128, mkexpr(addr)) );
            imm8 = getUChar(delta+alen);
            delta += alen+1;
            DIP("vpalignr $%d,%s,%s,%s\n", imm8, dis_buf,
                                           nameXMMReg(rV), nameXMMReg(rG));
         }

         IRTemp res = math_PALIGNR_XMM( sV, dV, imm8 );
         putYMMRegLoAndZU( rG, mkexpr(res) );
         *uses_vvvv = True;
         goto decode_success;
      }
      
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         UChar  modrm = getUChar(delta);
         UInt   rG    = gregOfRexRM(pfx, modrm);
         UInt   rV    = getVexNvvvv(pfx);
         IRTemp sV    = newTemp(Ity_V256);
         IRTemp dV    = newTemp(Ity_V256);
         IRTemp sHi, sLo, dHi, dLo;
         sHi = sLo = dHi = dLo = IRTemp_INVALID;
         UInt   imm8;

         assign( dV, getYMMReg(rV) );

         if ( epartIsReg( modrm ) ) {
            UInt   rE = eregOfRexRM(pfx, modrm);
            assign( sV, getYMMReg(rE) );
            imm8 = getUChar(delta+1);
            delta += 1+1;
            DIP("vpalignr $%d,%s,%s,%s\n", imm8, nameYMMReg(rE),
                                           nameYMMReg(rV), nameYMMReg(rG));
         } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 1 );
            assign( sV, loadLE(Ity_V256, mkexpr(addr)) );
            imm8 = getUChar(delta+alen);
            delta += alen+1;
            DIP("vpalignr $%d,%s,%s,%s\n", imm8, dis_buf,
                                           nameYMMReg(rV), nameYMMReg(rG));
         }

         breakupV256toV128s( dV, &dHi, &dLo );
         breakupV256toV128s( sV, &sHi, &sLo );
         putYMMReg( rG, binop( Iop_V128HLtoV256,
                               mkexpr( math_PALIGNR_XMM( sHi, dHi, imm8 ) ),
                               mkexpr( math_PALIGNR_XMM( sLo, dLo, imm8 ) ) )
                    );
         *uses_vvvv = True;
         goto decode_success;
      }
      break;

   case 0x14:
      
      if (have66noF2noF3(pfx)
          && 0==getVexL(pfx) && 0==getRexW(pfx)) {
         delta = dis_PEXTRB_128_GtoE( vbi, pfx, delta, False );
         goto decode_success;
      }
      break;

   case 0x15:
      
      
      if (have66noF2noF3(pfx)
          && 0==getVexL(pfx) && 0==getRexW(pfx)) {
         delta = dis_PEXTRW( vbi, pfx, delta, True );
         goto decode_success;
      }
      break;

   case 0x16:
      
      
      if (have66noF2noF3(pfx)
          && 0==getVexL(pfx) && 0==getRexW(pfx)) {
         delta = dis_PEXTRD( vbi, pfx, delta, True );
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx)
          && 0==getVexL(pfx) && 1==getRexW(pfx)) {
         delta = dis_PEXTRQ( vbi, pfx, delta, True );
         goto decode_success;
      }
      break;

   case 0x17:
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_EXTRACTPS( vbi, pfx, delta, True );
         goto decode_success;
      }
      break;

   case 0x18:
      
      if (have66noF2noF3(pfx)
          && 1==getVexL(pfx) && 0==getRexW(pfx)) {
         UChar  modrm = getUChar(delta);
         UInt   ib    = 0;
         UInt   rG    = gregOfRexRM(pfx, modrm);
         UInt   rV    = getVexNvvvv(pfx);
         IRTemp t128  = newTemp(Ity_V128);
         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx, modrm);
            delta += 1;
            assign(t128, getXMMReg(rE));
            ib = getUChar(delta);
            DIP("vinsertf128 $%u,%s,%s,%s\n",
                ib, nameXMMReg(rE), nameYMMReg(rV), nameYMMReg(rG));
         } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 1 );
            assign(t128, loadLE(Ity_V128, mkexpr(addr)));
            delta += alen;
            ib = getUChar(delta);
            DIP("vinsertf128 $%u,%s,%s,%s\n",
                ib, dis_buf, nameYMMReg(rV), nameYMMReg(rG));
         }
         delta++;
         putYMMRegLane128(rG, 0,   getYMMRegLane128(rV, 0));
         putYMMRegLane128(rG, 1,   getYMMRegLane128(rV, 1));
         putYMMRegLane128(rG, ib & 1, mkexpr(t128));
         *uses_vvvv = True;
         goto decode_success;
      }
      break;

   case 0x19:
     
      if (have66noF2noF3(pfx)
          && 1==getVexL(pfx) && 0==getRexW(pfx)) {
         UChar  modrm = getUChar(delta);
         UInt   ib    = 0;
         UInt   rS    = gregOfRexRM(pfx, modrm);
         IRTemp t128  = newTemp(Ity_V128);
         if (epartIsReg(modrm)) {
            UInt rD = eregOfRexRM(pfx, modrm);
            delta += 1;
            ib = getUChar(delta);
            assign(t128, getYMMRegLane128(rS, ib & 1));
            putYMMRegLoAndZU(rD, mkexpr(t128));
            DIP("vextractf128 $%u,%s,%s\n",
                ib, nameXMMReg(rS), nameYMMReg(rD));
         } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 1 );
            delta += alen;
            ib = getUChar(delta);
            assign(t128, getYMMRegLane128(rS, ib & 1));
            storeLE(mkexpr(addr), mkexpr(t128));
            DIP("vextractf128 $%u,%s,%s\n",
                ib, nameYMMReg(rS), dis_buf);
         }
         delta++;
         
         goto decode_success;
      }
      break;

   case 0x20:
      
      if (have66noF2noF3(pfx)
          && 0==getVexL(pfx) && 0==getRexW(pfx)) {
         UChar  modrm  = getUChar(delta);
         UInt   rG     = gregOfRexRM(pfx, modrm);
         UInt   rV     = getVexNvvvv(pfx);
         Int    imm8;
         IRTemp src_u8 = newTemp(Ity_I8);

         if ( epartIsReg( modrm ) ) {
            UInt rE = eregOfRexRM(pfx,modrm);
            imm8 = (Int)(getUChar(delta+1) & 15);
            assign( src_u8, unop(Iop_32to8, getIReg32( rE )) );
            delta += 1+1;
            DIP( "vpinsrb $%d,%s,%s,%s\n",
                 imm8, nameIReg32(rE), nameXMMReg(rV), nameXMMReg(rG) );
         } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 1 );
            imm8 = (Int)(getUChar(delta+alen) & 15);
            assign( src_u8, loadLE( Ity_I8, mkexpr(addr) ) );
            delta += alen+1;
            DIP( "vpinsrb $%d,%s,%s,%s\n", 
                 imm8, dis_buf, nameXMMReg(rV), nameXMMReg(rG) );
         }

         IRTemp src_vec = newTemp(Ity_V128);
         assign(src_vec, getXMMReg( rV ));
         IRTemp res_vec = math_PINSRB_128( src_vec, src_u8, imm8 );
         putYMMRegLoAndZU( rG, mkexpr(res_vec) );
         *uses_vvvv = True;
         goto decode_success;
      }
      break;

   case 0x21:
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         UChar  modrm = getUChar(delta);
         UInt   rG    = gregOfRexRM(pfx, modrm);
         UInt   rV    = getVexNvvvv(pfx);
         UInt   imm8;
         IRTemp d2ins = newTemp(Ity_I32); 
         const IRTemp inval = IRTemp_INVALID;

         if ( epartIsReg( modrm ) ) {
            UInt   rE = eregOfRexRM(pfx, modrm);
            IRTemp vE = newTemp(Ity_V128);
            assign( vE, getXMMReg(rE) );
            IRTemp dsE[4] = { inval, inval, inval, inval };
            breakupV128to32s( vE, &dsE[3], &dsE[2], &dsE[1], &dsE[0] );
            imm8 = getUChar(delta+1);
            d2ins = dsE[(imm8 >> 6) & 3]; 
            delta += 1+1;
            DIP( "insertps $%u, %s,%s\n",
                 imm8, nameXMMReg(rE), nameXMMReg(rG) );
         } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 1 );
            assign( d2ins, loadLE( Ity_I32, mkexpr(addr) ) );
            imm8 = getUChar(delta+alen);
            delta += alen+1;
            DIP( "insertps $%u, %s,%s\n", 
                 imm8, dis_buf, nameXMMReg(rG) );
         }

         IRTemp vV = newTemp(Ity_V128);
         assign( vV, getXMMReg(rV) );

         putYMMRegLoAndZU( rG, mkexpr(math_INSERTPS( vV, d2ins, imm8 )) );
         *uses_vvvv = True;
         goto decode_success;
      }
      break;

   case 0x22:
      
      if (have66noF2noF3(pfx)
          && 0==getVexL(pfx) && 0==getRexW(pfx)) {
         UChar  modrm = getUChar(delta);
         UInt   rG    = gregOfRexRM(pfx, modrm);
         UInt   rV    = getVexNvvvv(pfx);
         Int    imm8_10;
         IRTemp src_u32 = newTemp(Ity_I32);

         if ( epartIsReg( modrm ) ) {
            UInt rE = eregOfRexRM(pfx,modrm);
            imm8_10 = (Int)(getUChar(delta+1) & 3);
            assign( src_u32, getIReg32( rE ) );
            delta += 1+1;
            DIP( "vpinsrd $%d,%s,%s,%s\n",
                 imm8_10, nameIReg32(rE), nameXMMReg(rV), nameXMMReg(rG) );
         } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 1 );
            imm8_10 = (Int)(getUChar(delta+alen) & 3);
            assign( src_u32, loadLE( Ity_I32, mkexpr(addr) ) );
            delta += alen+1;
            DIP( "vpinsrd $%d,%s,%s,%s\n", 
                 imm8_10, dis_buf, nameXMMReg(rV), nameXMMReg(rG) );
         }

         IRTemp src_vec = newTemp(Ity_V128);
         assign(src_vec, getXMMReg( rV ));
         IRTemp res_vec = math_PINSRD_128( src_vec, src_u32, imm8_10 );
         putYMMRegLoAndZU( rG, mkexpr(res_vec) );
         *uses_vvvv = True;
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx)
          && 0==getVexL(pfx) && 1==getRexW(pfx)) {
         UChar  modrm = getUChar(delta);
         UInt   rG    = gregOfRexRM(pfx, modrm);
         UInt   rV    = getVexNvvvv(pfx);
         Int    imm8_0;
         IRTemp src_u64 = newTemp(Ity_I64);

         if ( epartIsReg( modrm ) ) {
            UInt rE = eregOfRexRM(pfx,modrm);
            imm8_0 = (Int)(getUChar(delta+1) & 1);
            assign( src_u64, getIReg64( rE ) );
            delta += 1+1;
            DIP( "vpinsrq $%d,%s,%s,%s\n",
                 imm8_0, nameIReg64(rE), nameXMMReg(rV), nameXMMReg(rG) );
         } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 1 );
            imm8_0 = (Int)(getUChar(delta+alen) & 1);
            assign( src_u64, loadLE( Ity_I64, mkexpr(addr) ) );
            delta += alen+1;
            DIP( "vpinsrd $%d,%s,%s,%s\n", 
                 imm8_0, dis_buf, nameXMMReg(rV), nameXMMReg(rG) );
         }

         IRTemp src_vec = newTemp(Ity_V128);
         assign(src_vec, getXMMReg( rV ));
         IRTemp res_vec = math_PINSRQ_128( src_vec, src_u64, imm8_0 );
         putYMMRegLoAndZU( rG, mkexpr(res_vec) );
         *uses_vvvv = True;
         goto decode_success;
      }
      break;

   case 0x38:
      
      if (have66noF2noF3(pfx)
          && 1==getVexL(pfx) && 0==getRexW(pfx)) {
         UChar  modrm = getUChar(delta);
         UInt   ib    = 0;
         UInt   rG    = gregOfRexRM(pfx, modrm);
         UInt   rV    = getVexNvvvv(pfx);
         IRTemp t128  = newTemp(Ity_V128);
         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx, modrm);
            delta += 1;
            assign(t128, getXMMReg(rE));
            ib = getUChar(delta);
            DIP("vinserti128 $%u,%s,%s,%s\n",
                ib, nameXMMReg(rE), nameYMMReg(rV), nameYMMReg(rG));
         } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 1 );
            assign(t128, loadLE(Ity_V128, mkexpr(addr)));
            delta += alen;
            ib = getUChar(delta);
            DIP("vinserti128 $%u,%s,%s,%s\n",
                ib, dis_buf, nameYMMReg(rV), nameYMMReg(rG));
         }
         delta++;
         putYMMRegLane128(rG, 0,   getYMMRegLane128(rV, 0));
         putYMMRegLane128(rG, 1,   getYMMRegLane128(rV, 1));
         putYMMRegLane128(rG, ib & 1, mkexpr(t128));
         *uses_vvvv = True;
         goto decode_success;
      }
      break;

   case 0x39:
      
      if (have66noF2noF3(pfx)
          && 1==getVexL(pfx) && 0==getRexW(pfx)) {
         UChar  modrm = getUChar(delta);
         UInt   ib    = 0;
         UInt   rS    = gregOfRexRM(pfx, modrm);
         IRTemp t128  = newTemp(Ity_V128);
         if (epartIsReg(modrm)) {
            UInt rD = eregOfRexRM(pfx, modrm);
            delta += 1;
            ib = getUChar(delta);
            assign(t128, getYMMRegLane128(rS, ib & 1));
            putYMMRegLoAndZU(rD, mkexpr(t128));
            DIP("vextracti128 $%u,%s,%s\n",
                ib, nameXMMReg(rS), nameYMMReg(rD));
         } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 1 );
            delta += alen;
            ib = getUChar(delta);
            assign(t128, getYMMRegLane128(rS, ib & 1));
            storeLE(mkexpr(addr), mkexpr(t128));
            DIP("vextracti128 $%u,%s,%s\n",
                ib, nameYMMReg(rS), dis_buf);
         }
         delta++;
         
         goto decode_success;
      }
      break;

   case 0x40:
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         UChar  modrm   = getUChar(delta);
         UInt   rG      = gregOfRexRM(pfx, modrm);
         UInt   rV      = getVexNvvvv(pfx);
         IRTemp dst_vec = newTemp(Ity_V128);
         Int    imm8;
         if (epartIsReg( modrm )) {
            UInt rE = eregOfRexRM(pfx,modrm);
            imm8 = (Int)getUChar(delta+1);
            assign( dst_vec, getXMMReg( rE ) );
            delta += 1+1;
            DIP( "vdpps $%d,%s,%s,%s\n",
                 imm8, nameXMMReg(rE), nameXMMReg(rV), nameXMMReg(rG) );
         } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 1 );
            imm8 = (Int)getUChar(delta+alen);
            assign( dst_vec, loadLE( Ity_V128, mkexpr(addr) ) );
            delta += alen+1;
            DIP( "vdpps $%d,%s,%s,%s\n", 
                 imm8, dis_buf, nameXMMReg(rV), nameXMMReg(rG) );
         }

         IRTemp src_vec = newTemp(Ity_V128);
         assign(src_vec, getXMMReg( rV ));
         IRTemp res_vec = math_DPPS_128( src_vec, dst_vec, imm8 );
         putYMMRegLoAndZU( rG, mkexpr(res_vec) );
         *uses_vvvv = True;
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         UChar  modrm   = getUChar(delta);
         UInt   rG      = gregOfRexRM(pfx, modrm);
         UInt   rV      = getVexNvvvv(pfx);
         IRTemp dst_vec = newTemp(Ity_V256);
         Int    imm8;
         if (epartIsReg( modrm )) {
            UInt rE = eregOfRexRM(pfx,modrm);
            imm8 = (Int)getUChar(delta+1);
            assign( dst_vec, getYMMReg( rE ) );
            delta += 1+1;
            DIP( "vdpps $%d,%s,%s,%s\n",
                 imm8, nameYMMReg(rE), nameYMMReg(rV), nameYMMReg(rG) );
         } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 1 );
            imm8 = (Int)getUChar(delta+alen);
            assign( dst_vec, loadLE( Ity_V256, mkexpr(addr) ) );
            delta += alen+1;
            DIP( "vdpps $%d,%s,%s,%s\n", 
                 imm8, dis_buf, nameYMMReg(rV), nameYMMReg(rG) );
         }

         IRTemp src_vec = newTemp(Ity_V256);
         assign(src_vec, getYMMReg( rV ));
         IRTemp s0, s1, d0, d1;
         s0 = s1 = d0 = d1 = IRTemp_INVALID;
         breakupV256toV128s( dst_vec, &d1, &d0 );
         breakupV256toV128s( src_vec, &s1, &s0 );
         putYMMReg( rG, binop( Iop_V128HLtoV256,
                               mkexpr( math_DPPS_128(s1, d1, imm8) ),
                               mkexpr( math_DPPS_128(s0, d0, imm8) ) ) );
         *uses_vvvv = True;
         goto decode_success;
      }
      break;

   case 0x41:
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         UChar  modrm   = getUChar(delta);
         UInt   rG      = gregOfRexRM(pfx, modrm);
         UInt   rV      = getVexNvvvv(pfx);
         IRTemp dst_vec = newTemp(Ity_V128);
         Int    imm8;
         if (epartIsReg( modrm )) {
            UInt rE = eregOfRexRM(pfx,modrm);
            imm8 = (Int)getUChar(delta+1);
            assign( dst_vec, getXMMReg( rE ) );
            delta += 1+1;
            DIP( "vdppd $%d,%s,%s,%s\n",
                 imm8, nameXMMReg(rE), nameXMMReg(rV), nameXMMReg(rG) );
         } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 1 );
            imm8 = (Int)getUChar(delta+alen);
            assign( dst_vec, loadLE( Ity_V128, mkexpr(addr) ) );
            delta += alen+1;
            DIP( "vdppd $%d,%s,%s,%s\n", 
                 imm8, dis_buf, nameXMMReg(rV), nameXMMReg(rG) );
         }

         IRTemp src_vec = newTemp(Ity_V128);
         assign(src_vec, getXMMReg( rV ));
         IRTemp res_vec = math_DPPD_128( src_vec, dst_vec, imm8 );
         putYMMRegLoAndZU( rG, mkexpr(res_vec) );
         *uses_vvvv = True;
         goto decode_success;
      }
      break;

   case 0x42:
      
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         UChar  modrm   = getUChar(delta);
         Int    imm8;
         IRTemp src_vec = newTemp(Ity_V128);
         IRTemp dst_vec = newTemp(Ity_V128);
         UInt   rG      = gregOfRexRM(pfx, modrm);
         UInt   rV      = getVexNvvvv(pfx);

         assign( dst_vec, getXMMReg(rV) );
  
         if ( epartIsReg( modrm ) ) {
            UInt rE = eregOfRexRM(pfx, modrm);

            imm8 = (Int)getUChar(delta+1);
            assign( src_vec, getXMMReg(rE) );
            delta += 1+1;
            DIP( "vmpsadbw $%d, %s,%s,%s\n", imm8,
                 nameXMMReg(rE), nameXMMReg(rV), nameXMMReg(rG) );
         } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 
                             1 );
            assign( src_vec, loadLE( Ity_V128, mkexpr(addr) ) );
            imm8 = (Int)getUChar(delta+alen);
            delta += alen+1;
            DIP( "vmpsadbw $%d, %s,%s,%s\n", imm8,
                 dis_buf, nameXMMReg(rV), nameXMMReg(rG) );
         }

         putYMMRegLoAndZU( rG, mkexpr( math_MPSADBW_128(dst_vec,
                                                        src_vec, imm8) ) );
         *uses_vvvv = True;
         goto decode_success;
      }
      
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         UChar  modrm   = getUChar(delta);
         Int    imm8;
         IRTemp src_vec = newTemp(Ity_V256);
         IRTemp dst_vec = newTemp(Ity_V256);
         UInt   rG      = gregOfRexRM(pfx, modrm);
         UInt   rV      = getVexNvvvv(pfx);
         IRTemp sHi, sLo, dHi, dLo;
         sHi = sLo = dHi = dLo = IRTemp_INVALID;

         assign( dst_vec, getYMMReg(rV) );

         if ( epartIsReg( modrm ) ) {
            UInt rE = eregOfRexRM(pfx, modrm);

            imm8 = (Int)getUChar(delta+1);
            assign( src_vec, getYMMReg(rE) );
            delta += 1+1;
            DIP( "vmpsadbw $%d, %s,%s,%s\n", imm8,
                 nameYMMReg(rE), nameYMMReg(rV), nameYMMReg(rG) );
         } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf,
                             1 );
            assign( src_vec, loadLE( Ity_V256, mkexpr(addr) ) );
            imm8 = (Int)getUChar(delta+alen);
            delta += alen+1;
            DIP( "vmpsadbw $%d, %s,%s,%s\n", imm8,
                 dis_buf, nameYMMReg(rV), nameYMMReg(rG) );
         }

         breakupV256toV128s( dst_vec, &dHi, &dLo );
         breakupV256toV128s( src_vec, &sHi, &sLo );
         putYMMReg( rG, binop( Iop_V128HLtoV256,
                               mkexpr( math_MPSADBW_128(dHi, sHi, imm8 >> 3) ),
                               mkexpr( math_MPSADBW_128(dLo, sLo, imm8) ) ) );
         *uses_vvvv = True;
         goto decode_success;
      }
      break;

   case 0x44:
      
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         UChar  modrm = getUChar(delta);
         Int imm8;
         IRTemp sV    = newTemp(Ity_V128);
         IRTemp dV    = newTemp(Ity_V128);
         UInt   rG    = gregOfRexRM(pfx, modrm);
         UInt   rV    = getVexNvvvv(pfx);

         assign( dV, getXMMReg(rV) );
  
         if ( epartIsReg( modrm ) ) {
            UInt rE = eregOfRexRM(pfx, modrm);
            imm8 = (Int)getUChar(delta+1);
            assign( sV, getXMMReg(rE) );
            delta += 1+1;
            DIP( "vpclmulqdq $%d, %s,%s,%s\n", imm8,
                 nameXMMReg(rE), nameXMMReg(rV), nameXMMReg(rG) );    
         } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 
                             1 );
            assign( sV, loadLE( Ity_V128, mkexpr(addr) ) );
            imm8 = (Int)getUChar(delta+alen);
            delta += alen+1;
            DIP( "vpclmulqdq $%d, %s,%s,%s\n", 
                 imm8, dis_buf, nameXMMReg(rV), nameXMMReg(rG) );
         }

         putYMMRegLoAndZU( rG, mkexpr( math_PCLMULQDQ(dV, sV, imm8) ) );
         *uses_vvvv = True;
         goto decode_success;
      }
      break;

   case 0x46:
      
      if (have66noF2noF3(pfx)
          && 1==getVexL(pfx) && 0==getRexW(pfx)) {
         UChar  modrm = getUChar(delta);
         UInt   imm8  = 0;
         UInt   rG    = gregOfRexRM(pfx, modrm);
         UInt   rV    = getVexNvvvv(pfx);
         IRTemp s00   = newTemp(Ity_V128);
         IRTemp s01   = newTemp(Ity_V128);
         IRTemp s10   = newTemp(Ity_V128);
         IRTemp s11   = newTemp(Ity_V128);
         assign(s00, getYMMRegLane128(rV, 0));
         assign(s01, getYMMRegLane128(rV, 1));
         if (epartIsReg(modrm)) {
            UInt rE = eregOfRexRM(pfx, modrm);
            delta += 1;
            imm8 = getUChar(delta);
            DIP("vperm2i128 $%u,%s,%s,%s\n",
                imm8, nameYMMReg(rE), nameYMMReg(rV), nameYMMReg(rG));
            assign(s10, getYMMRegLane128(rE, 0));
            assign(s11, getYMMRegLane128(rE, 1));
         } else {
            addr = disAMode( &alen, vbi, pfx, delta, dis_buf, 1 );
            delta += alen;
            imm8 = getUChar(delta);
            DIP("vperm2i128 $%u,%s,%s,%s\n",
                imm8, dis_buf, nameYMMReg(rV), nameYMMReg(rG));
            assign(s10, loadLE(Ity_V128, binop(Iop_Add64,
                                               mkexpr(addr), mkU64(0))));
            assign(s11, loadLE(Ity_V128, binop(Iop_Add64,
                                               mkexpr(addr), mkU64(16))));
         }
         delta++;
#        define SEL(_nn) (((_nn)==0) ? s00 : ((_nn)==1) ? s01 \
                                           : ((_nn)==2) ? s10 : s11)
         putYMMRegLane128(rG, 0, mkexpr(SEL((imm8 >> 0) & 3)));
         putYMMRegLane128(rG, 1, mkexpr(SEL((imm8 >> 4) & 3)));
#        undef SEL
         if (imm8 & (1<<3)) putYMMRegLane128(rG, 0, mkV128(0));
         if (imm8 & (1<<7)) putYMMRegLane128(rG, 1, mkV128(0));
         *uses_vvvv = True;
         goto decode_success;
      }
      break;

   case 0x4A:
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_VBLENDV_128 ( vbi, pfx, delta,
                                   "vblendvps", 4, Iop_SarN32x4 );
         *uses_vvvv = True;
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_VBLENDV_256 ( vbi, pfx, delta,
                                   "vblendvps", 4, Iop_SarN32x4 );
         *uses_vvvv = True;
         goto decode_success;
      }
      break;

   case 0x4B:
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_VBLENDV_128 ( vbi, pfx, delta,
                                   "vblendvpd", 8, Iop_SarN64x2 );
         *uses_vvvv = True;
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_VBLENDV_256 ( vbi, pfx, delta,
                                   "vblendvpd", 8, Iop_SarN64x2 );
         *uses_vvvv = True;
         goto decode_success;
      }
      break;

   case 0x4C:
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_VBLENDV_128 ( vbi, pfx, delta,
                                   "vpblendvb", 1, Iop_SarN8x16 );
         *uses_vvvv = True;
         goto decode_success;
      }
      
      if (have66noF2noF3(pfx) && 1==getVexL(pfx)) {
         delta = dis_VBLENDV_256 ( vbi, pfx, delta,
                                   "vpblendvb", 1, Iop_SarN8x16 );
         *uses_vvvv = True;
         goto decode_success;
      }
      break;

   case 0x60:
   case 0x61:
   case 0x62:
   case 0x63:
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         Long delta0 = delta;
         delta = dis_PCMPxSTRx( vbi, pfx, delta, True, opc );
         if (delta > delta0) goto decode_success;
         
      }
      break;

   case 0xDF:
      
      if (have66noF2noF3(pfx) && 0==getVexL(pfx)) {
         delta = dis_AESKEYGENASSIST( vbi, pfx, delta, True );
         goto decode_success;
      }
      break;

   case 0xF0:
      
      
      if (haveF2no66noF3(pfx) && 0==getVexL(pfx) && !haveREX(pfx)) {
         Int     size = getRexW(pfx) ? 8 : 4;
         IRType  ty   = szToITy(size);
         IRTemp  src  = newTemp(ty);
         UChar   rm   = getUChar(delta);
         UChar   imm8;

         if (epartIsReg(rm)) {
            imm8 = getUChar(delta+1);
            assign( src, getIRegE(size,pfx,rm) );
            DIP("rorx %d,%s,%s\n", imm8, nameIRegE(size,pfx,rm),
                                   nameIRegG(size,pfx,rm));
            delta += 2;
         } else {
            addr = disAMode ( &alen, vbi, pfx, delta, dis_buf, 0 );
            imm8 = getUChar(delta+alen);
            assign( src, loadLE(ty, mkexpr(addr)) );
            DIP("rorx %d,%s,%s\n", imm8, dis_buf, nameIRegG(size,pfx,rm));
            delta += alen + 1;
         }
         imm8 &= 8*size-1;

         
         putIRegG( size, pfx, rm,
                   imm8 == 0 ? mkexpr(src)
                   : binop( mkSizedOp(ty,Iop_Or8),
                            binop( mkSizedOp(ty,Iop_Shr8), mkexpr(src),
                                   mkU8(imm8) ),
                            binop( mkSizedOp(ty,Iop_Shl8), mkexpr(src),
                                   mkU8(8*size-imm8) ) ) );
         
         goto decode_success;
      }
      break;

   default:
      break;

   }

  
   return deltaIN;

  decode_success:
   return delta;
}



   
static
DisResult disInstr_AMD64_WRK ( 
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
   IRTemp    t1, t2;
   UChar     pre;
   Int       n, n_prefixes;
   DisResult dres;

   
   Long delta = delta64;

   Long delta_start = delta;

   Int sz = 4;

   
   Prefix pfx = PFX_EMPTY;

   
   Escape esc = ESC_NONE;

   
   dres.whatNext    = Dis_Continue;
   dres.len         = 0;
   dres.continueAt  = 0;
   dres.jk_StopHere = Ijk_INVALID;
   *expect_CAS = False;

   vassert(guest_RIP_next_assumed == 0);
   vassert(guest_RIP_next_mustcheck == False);

   t1 = t2 = IRTemp_INVALID; 

   DIP("\t0x%llx:  ", guest_RIP_bbstart+delta);

   
   {
      const UChar* code = guest_code + delta;
      if (code[ 0] == 0x48 && code[ 1] == 0xC1 && code[ 2] == 0xC7 
                                               && code[ 3] == 0x03 &&
          code[ 4] == 0x48 && code[ 5] == 0xC1 && code[ 6] == 0xC7 
                                               && code[ 7] == 0x0D &&
          code[ 8] == 0x48 && code[ 9] == 0xC1 && code[10] == 0xC7 
                                               && code[11] == 0x3D &&
          code[12] == 0x48 && code[13] == 0xC1 && code[14] == 0xC7 
                                               && code[15] == 0x33) {
         
         if (code[16] == 0x48 && code[17] == 0x87 
                              && code[18] == 0xDB ) {
            
            DIP("%%rdx = client_request ( %%rax )\n");
            delta += 19;
            jmp_lit(&dres, Ijk_ClientReq, guest_RIP_bbstart+delta);
            vassert(dres.whatNext == Dis_StopHere);
            goto decode_success;
         }
         else
         if (code[16] == 0x48 && code[17] == 0x87 
                              && code[18] == 0xC9 ) {
            
            DIP("%%rax = guest_NRADDR\n");
            delta += 19;
            putIRegRAX(8, IRExpr_Get( OFFB_NRADDR, Ity_I64 ));
            goto decode_success;
         }
         else
         if (code[16] == 0x48 && code[17] == 0x87 
                              && code[18] == 0xD2 ) {
            
            DIP("call-noredir *%%rax\n");
            delta += 19;
            t1 = newTemp(Ity_I64);
            assign(t1, getIRegRAX(8));
            t2 = newTemp(Ity_I64);
            assign(t2, binop(Iop_Sub64, getIReg64(R_RSP), mkU64(8)));
            putIReg64(R_RSP, mkexpr(t2));
            storeLE( mkexpr(t2), mkU64(guest_RIP_bbstart+delta));
            jmp_treg(&dres, Ijk_NoRedir, t1);
            vassert(dres.whatNext == Dis_StopHere);
            goto decode_success;
         }
         else
         if (code[16] == 0x48 && code[17] == 0x87
                              && code[18] == 0xff ) {
           
            DIP("IR injection\n");
            vex_inject_ir(irsb, Iend_LE);

            
            
            
            
            stmt(IRStmt_Put(OFFB_CMSTART, mkU64(guest_RIP_curr_instr)));
            stmt(IRStmt_Put(OFFB_CMLEN,   mkU64(19)));
   
            delta += 19;

            stmt( IRStmt_Put( OFFB_RIP, mkU64(guest_RIP_bbstart + delta) ) );
            dres.whatNext    = Dis_StopHere;
            dres.jk_StopHere = Ijk_InvalICache;
            goto decode_success;
         }
         
         goto decode_failure;
         
      }
   }

   n_prefixes = 0;
   while (True) {
      if (n_prefixes > 7) goto decode_failure;
      pre = getUChar(delta);
      switch (pre) {
         case 0x66: pfx |= PFX_66; break;
         case 0x67: pfx |= PFX_ASO; break;
         case 0xF2: pfx |= PFX_F2; break;
         case 0xF3: pfx |= PFX_F3; break;
         case 0xF0: pfx |= PFX_LOCK; *expect_CAS = True; break;
         case 0x2E: pfx |= PFX_CS; break;
         case 0x3E: pfx |= PFX_DS; break;
         case 0x26: pfx |= PFX_ES; break;
         case 0x64: pfx |= PFX_FS; break;
         case 0x65: pfx |= PFX_GS; break;
         case 0x36: pfx |= PFX_SS; break;
         case 0x40 ... 0x4F:
            pfx |= PFX_REX;
            if (pre & (1<<3)) pfx |= PFX_REXW;
            if (pre & (1<<2)) pfx |= PFX_REXR;
            if (pre & (1<<1)) pfx |= PFX_REXX;
            if (pre & (1<<0)) pfx |= PFX_REXB;
            break;
         default: 
            goto not_a_legacy_prefix;
      }
      n_prefixes++;
      delta++;
   }

   not_a_legacy_prefix:
   if (archinfo->hwcaps & VEX_HWCAPS_AMD64_AVX) {
      
      UChar vex0 = getUChar(delta);
      if (vex0 == 0xC4) {
         
         UChar vex1 = getUChar(delta+1);
         UChar vex2 = getUChar(delta+2);
         delta += 3;
         pfx |= PFX_VEX;
         
          pfx |= (vex1 & (1<<7)) ? 0 : PFX_REXR;
          pfx |= (vex1 & (1<<6)) ? 0 : PFX_REXX;
          pfx |= (vex1 & (1<<5)) ? 0 : PFX_REXB;
         
         switch (vex1 & 0x1F) {
            case 1: esc = ESC_0F;   break;
            case 2: esc = ESC_0F38; break;
            case 3: esc = ESC_0F3A; break;
            
            default: goto decode_failure;
         }
         
             pfx |= (vex2 & (1<<7)) ? PFX_REXW : 0;
           pfx |= (vex2 & (1<<6)) ? 0 : PFX_VEXnV3;
           pfx |= (vex2 & (1<<5)) ? 0 : PFX_VEXnV2;
           pfx |= (vex2 & (1<<4)) ? 0 : PFX_VEXnV1;
           pfx |= (vex2 & (1<<3)) ? 0 : PFX_VEXnV0;
             pfx |= (vex2 & (1<<2)) ? PFX_VEXL : 0;
         
         switch (vex2 & 3) {
            case 0: break;
            case 1: pfx |= PFX_66; break;
            case 2: pfx |= PFX_F3; break;
            case 3: pfx |= PFX_F2; break;
            default: vassert(0);
         }
      }
      else if (vex0 == 0xC5) {
         
         UChar vex1 = getUChar(delta+1);
         delta += 2;
         pfx |= PFX_VEX;
         
             pfx |= (vex1 & (1<<7)) ? 0 : PFX_REXR;
           pfx |= (vex1 & (1<<6)) ? 0 : PFX_VEXnV3;
           pfx |= (vex1 & (1<<5)) ? 0 : PFX_VEXnV2;
           pfx |= (vex1 & (1<<4)) ? 0 : PFX_VEXnV1;
           pfx |= (vex1 & (1<<3)) ? 0 : PFX_VEXnV0;
             pfx |= (vex1 & (1<<2)) ? PFX_VEXL : 0;
         
         switch (vex1 & 3) {
            case 0: break;
            case 1: pfx |= PFX_66; break;
            case 2: pfx |= PFX_F3; break;
            case 3: pfx |= PFX_F2; break;
            default: vassert(0);
         }
         
         esc = ESC_0F;
      }
      
      if ((pfx & PFX_VEX) && (pfx & PFX_REX))
         goto decode_failure; 
   }

   
   n = 0;
   if (pfx & PFX_F2) n++;
   if (pfx & PFX_F3) n++;
   if (n > 1) 
      goto decode_failure; 

   n = 0;
   if (pfx & PFX_CS) n++;
   if (pfx & PFX_DS) n++;
   if (pfx & PFX_ES) n++;
   if (pfx & PFX_FS) n++;
   if (pfx & PFX_GS) n++;
   if (pfx & PFX_SS) n++;
   if (n > 1) 
      goto decode_failure; 

   if ((pfx & PFX_FS) && !vbi->guest_amd64_assume_fs_is_const)
      goto decode_failure;

   
   if ((pfx & PFX_GS) && !vbi->guest_amd64_assume_gs_is_const)
      goto decode_failure;

   
   sz = 4;
   if (pfx & PFX_66) sz = 2;
   if ((pfx & PFX_REX) && (pfx & PFX_REXW)) sz = 8;

   if (haveLOCK(pfx)) {
      if (can_be_used_with_LOCK_prefix( &guest_code[delta] )) {
         DIP("lock ");
      } else {
         *expect_CAS = False;
         goto decode_failure;
      }
   }

   if (!(pfx & PFX_VEX)) {
      vassert(esc == ESC_NONE);
      pre = getUChar(delta);
      if (pre == 0x0F) {
         delta++;
         pre = getUChar(delta);
         switch (pre) {
            case 0x38: esc = ESC_0F38; delta++; break;
            case 0x3A: esc = ESC_0F3A; delta++; break;
            default:   esc = ESC_0F; break;
         }
      }
   }

   Long delta_at_primary_opcode = delta;

   if (!(pfx & PFX_VEX)) {
      switch (esc) {
         case ESC_NONE:
            delta = dis_ESC_NONE( &dres, expect_CAS,
                                  resteerOkFn, resteerCisOk, callback_opaque,
                                  archinfo, vbi, pfx, sz, delta );
            break;
         case ESC_0F:
            delta = dis_ESC_0F  ( &dres, expect_CAS,
                                  resteerOkFn, resteerCisOk, callback_opaque,
                                  archinfo, vbi, pfx, sz, delta );
            break;
         case ESC_0F38:
            delta = dis_ESC_0F38( &dres,
                                  resteerOkFn, resteerCisOk, callback_opaque,
                                  archinfo, vbi, pfx, sz, delta );
            break;
         case ESC_0F3A:
            delta = dis_ESC_0F3A( &dres,
                                  resteerOkFn, resteerCisOk, callback_opaque,
                                  archinfo, vbi, pfx, sz, delta );
            break;
         default:
            vassert(0);
      }
   } else {
      
      Bool uses_vvvv = False;
      switch (esc) {
         case ESC_0F:
            delta = dis_ESC_0F__VEX ( &dres, &uses_vvvv,
                                      resteerOkFn, resteerCisOk,
                                      callback_opaque,
                                      archinfo, vbi, pfx, sz, delta );
            break;
         case ESC_0F38:
            delta = dis_ESC_0F38__VEX ( &dres, &uses_vvvv,
                                        resteerOkFn, resteerCisOk,
                                        callback_opaque,
                                        archinfo, vbi, pfx, sz, delta );
            break;
         case ESC_0F3A:
            delta = dis_ESC_0F3A__VEX ( &dres, &uses_vvvv,
                                        resteerOkFn, resteerCisOk,
                                        callback_opaque,
                                        archinfo, vbi, pfx, sz, delta );
            break;
         case ESC_NONE:
            goto decode_failure;
         default:
            vassert(0);
      }
      if (!uses_vvvv) {
         if (getVexNvvvv(pfx) != 0)
            goto decode_failure;
      }
   }

   vassert(delta - delta_at_primary_opcode >= 0);
   vassert(delta - delta_at_primary_opcode < 16);

   if (delta == delta_at_primary_opcode)
      goto decode_failure;
   else
      goto decode_success; 

#if 0 

   
   
   



   insn = &guest_code[delta];


   

   
   
   

   
   
   

   
   
   

   
   
   

   
   
   

   
   
   

   
   
   

   
   
   

   
   
   

   

   
   opc = getUChar(delta); delta++;


   switch (opc) {

   

   

   

   

   case 0xCD: { 
      IRJumpKind jk = Ijk_Boring;
      if (have66orF2orF3(pfx)) goto decode_failure;
      d64 = getUChar(delta); delta++;
      switch (d64) {
         case 32: jk = Ijk_Sys_int32; break;
         default: goto decode_failure;
      }
      guest_RIP_next_mustcheck = True;
      guest_RIP_next_assumed = guest_RIP_bbstart + delta;
      jmp_lit(jk, guest_RIP_next_assumed);
      vassert(dres.whatNext == Dis_StopHere);
      DIP("int $0x%02x\n", (UInt)d64);
      break;
   }

   

   

   

   

   

   

   

   

   

   

   

   

   

   

   
 
   

   

   

   

   

   

   case 0x0F: {
      opc = getUChar(delta); delta++;
      switch (opc) {

      

      

      

      

      

      

      

      

      

      

      

      

      

      

      

      

      

      

      

      default:
         goto decode_failure;
   } 
   goto decode_success;
   } 

   
#endif 
  
     
  decode_failure:
   
   if (sigill_diag) {
      vex_printf("vex amd64->IR: unhandled instruction bytes: "
                 "0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n",
                 (Int)getUChar(delta_start+0),
                 (Int)getUChar(delta_start+1),
                 (Int)getUChar(delta_start+2),
                 (Int)getUChar(delta_start+3),
                 (Int)getUChar(delta_start+4),
                 (Int)getUChar(delta_start+5),
                 (Int)getUChar(delta_start+6),
                 (Int)getUChar(delta_start+7) );
      vex_printf("vex amd64->IR:   REX=%d REX.W=%d REX.R=%d REX.X=%d REX.B=%d\n",
                 haveREX(pfx) ? 1 : 0, getRexW(pfx), getRexR(pfx),
                 getRexX(pfx), getRexB(pfx));
      vex_printf("vex amd64->IR:   VEX=%d VEX.L=%d VEX.nVVVV=0x%x ESC=%s\n",
                 haveVEX(pfx) ? 1 : 0, getVexL(pfx),
                 getVexNvvvv(pfx),
                 esc==ESC_NONE ? "NONE" :
                   esc==ESC_0F ? "0F" :
                   esc==ESC_0F38 ? "0F38" :
                   esc==ESC_0F3A ? "0F3A" : "???");
      vex_printf("vex amd64->IR:   PFX.66=%d PFX.F2=%d PFX.F3=%d\n",
                 have66(pfx) ? 1 : 0, haveF2(pfx) ? 1 : 0,
                 haveF3(pfx) ? 1 : 0);
   }

   stmt( IRStmt_Put( OFFB_RIP, mkU64(guest_RIP_curr_instr) ) );
   jmp_lit(&dres, Ijk_NoDecode, guest_RIP_curr_instr);
   vassert(dres.whatNext == Dis_StopHere);
   dres.len = 0;
   *expect_CAS = False;
   return dres;

   

  decode_success:
   
   switch (dres.whatNext) {
      case Dis_Continue:
         stmt( IRStmt_Put( OFFB_RIP, mkU64(guest_RIP_bbstart + delta) ) );
         break;
      case Dis_ResteerU:
      case Dis_ResteerC:
         stmt( IRStmt_Put( OFFB_RIP, mkU64(dres.continueAt) ) );
         break;
      case Dis_StopHere:
         break;
      default:
         vassert(0);
   }

   DIP("\n");
   dres.len = toUInt(delta - delta_start);
   return dres;
}

#undef DIP
#undef DIS




DisResult disInstr_AMD64 ( IRSB*        irsb_IN,
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

   
   vassert(guest_arch == VexArchAMD64);
   guest_code           = guest_code_IN;
   irsb                 = irsb_IN;
   host_endness         = host_endness_IN;
   guest_RIP_curr_instr = guest_IP;
   guest_RIP_bbstart    = guest_IP - delta;

   
   guest_RIP_next_assumed   = 0;
   guest_RIP_next_mustcheck = False;

   x1 = irsb_IN->stmts_used;
   expect_CAS = False;
   dres = disInstr_AMD64_WRK ( &expect_CAS, resteerOkFn,
                               resteerCisOk,
                               callback_opaque,
                               delta, archinfo, abiinfo, sigill_diag_IN );
   x2 = irsb_IN->stmts_used;
   vassert(x2 >= x1);

   if (guest_RIP_next_mustcheck 
       && guest_RIP_next_assumed != guest_RIP_curr_instr + dres.len) {
      vex_printf("\n");
      vex_printf("assumed next %%rip = 0x%llx\n", 
                 guest_RIP_next_assumed );
      vex_printf(" actual next %%rip = 0x%llx\n", 
                 guest_RIP_curr_instr + dres.len );
      vpanic("disInstr_AMD64: disInstr miscalculated next %rip");
   }

   has_CAS = False;
   for (i = x1; i < x2; i++) {
      if (irsb_IN->stmts[i]->tag == Ist_CAS)
         has_CAS = True;
   }

   if (expect_CAS != has_CAS) {
      vex_traceflags |= VEX_TRACE_FE;
      dres = disInstr_AMD64_WRK ( &expect_CAS, resteerOkFn,
                                  resteerCisOk,
                                  callback_opaque,
                                  delta, archinfo, abiinfo, sigill_diag_IN );
      for (i = x1; i < x2; i++) {
         vex_printf("\t\t");
         ppIRStmt(irsb_IN->stmts[i]);
         vex_printf("\n");
      }
      vpanic("disInstr_AMD64: inconsistency in LOCK prefix handling");
   }

   return dres;
}





