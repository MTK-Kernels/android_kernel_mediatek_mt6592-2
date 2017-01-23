

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
#include "libvex_ir.h"
#include "libvex.h"
#include "libvex_guest_arm64.h"

#include "main_util.h"
#include "main_globals.h"
#include "guest_generic_bb_to_IR.h"
#include "guest_arm64_defs.h"




static VexEndness host_endness;

static Addr64 guest_PC_curr_instr;

static IRSB* irsb;



#define DIP(format, args...)           \
   if (vex_traceflags & VEX_TRACE_FE)  \
      vex_printf(format, ## args)

#define DIS(buf, format, args...)      \
   if (vex_traceflags & VEX_TRACE_FE)  \
      vex_sprintf(buf, format, ## args)



static inline UInt getUIntLittleEndianly ( const UChar* p )
{
   UInt w = 0;
   w = (w << 8) | p[3];
   w = (w << 8) | p[2];
   w = (w << 8) | p[1];
   w = (w << 8) | p[0];
   return w;
}

static ULong sx_to_64 ( ULong x, UInt n )
{
   vassert(n > 1 && n < 64);
   Long r = (Long)x;
   r = (r << (64-n)) >> (64-n);
   return (ULong)r;
}


#define BITS2(_b1,_b0)  \
   (((_b1) << 1) | (_b0))

#define BITS3(_b2,_b1,_b0)  \
  (((_b2) << 2) | ((_b1) << 1) | (_b0))

#define BITS4(_b3,_b2,_b1,_b0)  \
   (((_b3) << 3) | ((_b2) << 2) | ((_b1) << 1) | (_b0))

#define BITS8(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0)  \
   ((BITS4((_b7),(_b6),(_b5),(_b4)) << 4)  \
    | BITS4((_b3),(_b2),(_b1),(_b0)))

#define BITS5(_b4,_b3,_b2,_b1,_b0)  \
   (BITS8(0,0,0,(_b4),(_b3),(_b2),(_b1),(_b0)))
#define BITS6(_b5,_b4,_b3,_b2,_b1,_b0)  \
   (BITS8(0,0,(_b5),(_b4),(_b3),(_b2),(_b1),(_b0)))
#define BITS7(_b6,_b5,_b4,_b3,_b2,_b1,_b0)  \
   (BITS8(0,(_b6),(_b5),(_b4),(_b3),(_b2),(_b1),(_b0)))

#define BITS9(_b8,_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0)  \
   (((_b8) << 8)  \
    | BITS8((_b7),(_b6),(_b5),(_b4),(_b3),(_b2),(_b1),(_b0)))

#define BITS10(_b9,_b8,_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0)  \
   (((_b9) << 9) | ((_b8) << 8)  \
    | BITS8((_b7),(_b6),(_b5),(_b4),(_b3),(_b2),(_b1),(_b0)))

#define BITS11(_b10,_b9,_b8,_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0)  \
   (((_b10) << 10)  \
    | BITS10(_b9,_b8,_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0))

#define BITS12(_b11, _b10,_b9,_b8,_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0) \
   (((_b11) << 11)  \
    | BITS11(_b10,_b9,_b8,_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0))

#define X00 BITS2(0,0)
#define X01 BITS2(0,1)
#define X10 BITS2(1,0)
#define X11 BITS2(1,1)

#define SLICE_UInt(_uint,_bMax,_bMin)  \
   (( ((UInt)(_uint)) >> (_bMin))  \
    & (UInt)((1ULL << ((_bMax) - (_bMin) + 1)) - 1ULL))



static IRExpr* mkV128 ( UShort w )
{
   return IRExpr_Const(IRConst_V128(w));
}

static IRExpr* mkU64 ( ULong i )
{
   return IRExpr_Const(IRConst_U64(i));
}

static IRExpr* mkU32 ( UInt i )
{
   return IRExpr_Const(IRConst_U32(i));
}

static IRExpr* mkU16 ( UInt i )
{
   vassert(i < 65536);
   return IRExpr_Const(IRConst_U16(i));
}

static IRExpr* mkU8 ( UInt i )
{
   vassert(i < 256);
   return IRExpr_Const(IRConst_U8( (UChar)i ));
}

static IRExpr* mkexpr ( IRTemp tmp )
{
   return IRExpr_RdTmp(tmp);
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

static IRExpr* loadLE ( IRType ty, IRExpr* addr )
{
   return IRExpr_Load(Iend_LE, ty, addr);
}

static void stmt ( IRStmt* st )
{
   addStmtToIRSB( irsb, st );
}

static void assign ( IRTemp dst, IRExpr* e )
{
   stmt( IRStmt_WrTmp(dst, e) );
}

static void storeLE ( IRExpr* addr, IRExpr* data )
{
   stmt( IRStmt_Store(Iend_LE, addr, data) );
}


static IRTemp newTemp ( IRType ty )
{
   vassert(isPlausibleIRType(ty));
   return newIRTemp( irsb->tyenv, ty );
}

static IRTemp newTempV128(void)
{
   return newTemp(Ity_V128);
}

static
void newTempsV128_2(IRTemp* t1, IRTemp* t2)
{
   vassert(t1 && *t1 == IRTemp_INVALID);
   vassert(t2 && *t2 == IRTemp_INVALID);
   *t1 = newTempV128();
   *t2 = newTempV128();
}

static
void newTempsV128_3(IRTemp* t1, IRTemp* t2, IRTemp* t3)
{
   vassert(t1 && *t1 == IRTemp_INVALID);
   vassert(t2 && *t2 == IRTemp_INVALID);
   vassert(t3 && *t3 == IRTemp_INVALID);
   *t1 = newTempV128();
   *t2 = newTempV128();
   *t3 = newTempV128();
}

static
void newTempsV128_4(IRTemp* t1, IRTemp* t2, IRTemp* t3, IRTemp* t4)
{
   vassert(t1 && *t1 == IRTemp_INVALID);
   vassert(t2 && *t2 == IRTemp_INVALID);
   vassert(t3 && *t3 == IRTemp_INVALID);
   vassert(t4 && *t4 == IRTemp_INVALID);
   *t1 = newTempV128();
   *t2 = newTempV128();
   *t3 = newTempV128();
   *t4 = newTempV128();
}

static
void newTempsV128_7(IRTemp* t1, IRTemp* t2, IRTemp* t3,
                    IRTemp* t4, IRTemp* t5, IRTemp* t6, IRTemp* t7)
{
   vassert(t1 && *t1 == IRTemp_INVALID);
   vassert(t2 && *t2 == IRTemp_INVALID);
   vassert(t3 && *t3 == IRTemp_INVALID);
   vassert(t4 && *t4 == IRTemp_INVALID);
   vassert(t5 && *t5 == IRTemp_INVALID);
   vassert(t6 && *t6 == IRTemp_INVALID);
   vassert(t7 && *t7 == IRTemp_INVALID);
   *t1 = newTempV128();
   *t2 = newTempV128();
   *t3 = newTempV128();
   *t4 = newTempV128();
   *t5 = newTempV128();
   *t6 = newTempV128();
   *t7 = newTempV128();
}


static IROp mkAND ( IRType ty ) {
   switch (ty) {
      case Ity_I32: return Iop_And32;
      case Ity_I64: return Iop_And64;
      default: vpanic("mkAND");
   }
}

static IROp mkOR ( IRType ty ) {
   switch (ty) {
      case Ity_I32: return Iop_Or32;
      case Ity_I64: return Iop_Or64;
      default: vpanic("mkOR");
   }
}

static IROp mkXOR ( IRType ty ) {
   switch (ty) {
      case Ity_I32: return Iop_Xor32;
      case Ity_I64: return Iop_Xor64;
      default: vpanic("mkXOR");
   }
}

static IROp mkSHL ( IRType ty ) {
   switch (ty) {
      case Ity_I32: return Iop_Shl32;
      case Ity_I64: return Iop_Shl64;
      default: vpanic("mkSHL");
   }
}

static IROp mkSHR ( IRType ty ) {
   switch (ty) {
      case Ity_I32: return Iop_Shr32;
      case Ity_I64: return Iop_Shr64;
      default: vpanic("mkSHR");
   }
}

static IROp mkSAR ( IRType ty ) {
   switch (ty) {
      case Ity_I32: return Iop_Sar32;
      case Ity_I64: return Iop_Sar64;
      default: vpanic("mkSAR");
   }
}

static IROp mkNOT ( IRType ty ) {
   switch (ty) {
      case Ity_I32: return Iop_Not32;
      case Ity_I64: return Iop_Not64;
      default: vpanic("mkNOT");
   }
}

static IROp mkADD ( IRType ty ) {
   switch (ty) {
      case Ity_I32: return Iop_Add32;
      case Ity_I64: return Iop_Add64;
      default: vpanic("mkADD");
   }
}

static IROp mkSUB ( IRType ty ) {
   switch (ty) {
      case Ity_I32: return Iop_Sub32;
      case Ity_I64: return Iop_Sub64;
      default: vpanic("mkSUB");
   }
}

static IROp mkADDF ( IRType ty ) {
   switch (ty) {
      case Ity_F32: return Iop_AddF32;
      case Ity_F64: return Iop_AddF64;
      default: vpanic("mkADDF");
   }
}

static IROp mkSUBF ( IRType ty ) {
   switch (ty) {
      case Ity_F32: return Iop_SubF32;
      case Ity_F64: return Iop_SubF64;
      default: vpanic("mkSUBF");
   }
}

static IROp mkMULF ( IRType ty ) {
   switch (ty) {
      case Ity_F32: return Iop_MulF32;
      case Ity_F64: return Iop_MulF64;
      default: vpanic("mkMULF");
   }
}

static IROp mkDIVF ( IRType ty ) {
   switch (ty) {
      case Ity_F32: return Iop_DivF32;
      case Ity_F64: return Iop_DivF64;
      default: vpanic("mkMULF");
   }
}

static IROp mkNEGF ( IRType ty ) {
   switch (ty) {
      case Ity_F32: return Iop_NegF32;
      case Ity_F64: return Iop_NegF64;
      default: vpanic("mkNEGF");
   }
}

static IROp mkABSF ( IRType ty ) {
   switch (ty) {
      case Ity_F32: return Iop_AbsF32;
      case Ity_F64: return Iop_AbsF64;
      default: vpanic("mkNEGF");
   }
}

static IROp mkSQRTF ( IRType ty ) {
   switch (ty) {
      case Ity_F32: return Iop_SqrtF32;
      case Ity_F64: return Iop_SqrtF64;
      default: vpanic("mkNEGF");
   }
}

static IROp mkVecADD ( UInt size ) {
   const IROp ops[4]
      = { Iop_Add8x16, Iop_Add16x8, Iop_Add32x4, Iop_Add64x2 };
   vassert(size < 4);
   return ops[size];
}

static IROp mkVecQADDU ( UInt size ) {
   const IROp ops[4]
      = { Iop_QAdd8Ux16, Iop_QAdd16Ux8, Iop_QAdd32Ux4, Iop_QAdd64Ux2 };
   vassert(size < 4);
   return ops[size];
}

static IROp mkVecQADDS ( UInt size ) {
   const IROp ops[4]
      = { Iop_QAdd8Sx16, Iop_QAdd16Sx8, Iop_QAdd32Sx4, Iop_QAdd64Sx2 };
   vassert(size < 4);
   return ops[size];
}

static IROp mkVecQADDEXTSUSATUU ( UInt size ) {
   const IROp ops[4]
      = { Iop_QAddExtSUsatUU8x16, Iop_QAddExtSUsatUU16x8,
          Iop_QAddExtSUsatUU32x4, Iop_QAddExtSUsatUU64x2 };
   vassert(size < 4);
   return ops[size];
}

static IROp mkVecQADDEXTUSSATSS ( UInt size ) {
   const IROp ops[4]
      = { Iop_QAddExtUSsatSS8x16, Iop_QAddExtUSsatSS16x8,
          Iop_QAddExtUSsatSS32x4, Iop_QAddExtUSsatSS64x2 };
   vassert(size < 4);
   return ops[size];
}

static IROp mkVecSUB ( UInt size ) {
   const IROp ops[4]
      = { Iop_Sub8x16, Iop_Sub16x8, Iop_Sub32x4, Iop_Sub64x2 };
   vassert(size < 4);
   return ops[size];
}

static IROp mkVecQSUBU ( UInt size ) {
   const IROp ops[4]
      = { Iop_QSub8Ux16, Iop_QSub16Ux8, Iop_QSub32Ux4, Iop_QSub64Ux2 };
   vassert(size < 4);
   return ops[size];
}

static IROp mkVecQSUBS ( UInt size ) {
   const IROp ops[4]
      = { Iop_QSub8Sx16, Iop_QSub16Sx8, Iop_QSub32Sx4, Iop_QSub64Sx2 };
   vassert(size < 4);
   return ops[size];
}

static IROp mkVecSARN ( UInt size ) {
   const IROp ops[4]
      = { Iop_SarN8x16, Iop_SarN16x8, Iop_SarN32x4, Iop_SarN64x2 };
   vassert(size < 4);
   return ops[size];
}

static IROp mkVecSHRN ( UInt size ) {
   const IROp ops[4]
      = { Iop_ShrN8x16, Iop_ShrN16x8, Iop_ShrN32x4, Iop_ShrN64x2 };
   vassert(size < 4);
   return ops[size];
}

static IROp mkVecSHLN ( UInt size ) {
   const IROp ops[4]
      = { Iop_ShlN8x16, Iop_ShlN16x8, Iop_ShlN32x4, Iop_ShlN64x2 };
   vassert(size < 4);
   return ops[size];
}

static IROp mkVecCATEVENLANES ( UInt size ) {
   const IROp ops[4]
      = { Iop_CatEvenLanes8x16, Iop_CatEvenLanes16x8,
          Iop_CatEvenLanes32x4, Iop_InterleaveLO64x2 };
   vassert(size < 4);
   return ops[size];
}

static IROp mkVecCATODDLANES ( UInt size ) {
   const IROp ops[4]
      = { Iop_CatOddLanes8x16, Iop_CatOddLanes16x8,
          Iop_CatOddLanes32x4, Iop_InterleaveHI64x2 };
   vassert(size < 4);
   return ops[size];
}

static IROp mkVecINTERLEAVELO ( UInt size ) {
   const IROp ops[4]
      = { Iop_InterleaveLO8x16, Iop_InterleaveLO16x8,
          Iop_InterleaveLO32x4, Iop_InterleaveLO64x2 };
   vassert(size < 4);
   return ops[size];
}

static IROp mkVecINTERLEAVEHI ( UInt size ) {
   const IROp ops[4]
      = { Iop_InterleaveHI8x16, Iop_InterleaveHI16x8,
          Iop_InterleaveHI32x4, Iop_InterleaveHI64x2 };
   vassert(size < 4);
   return ops[size];
}

static IROp mkVecMAXU ( UInt size ) {
   const IROp ops[4]
      = { Iop_Max8Ux16, Iop_Max16Ux8, Iop_Max32Ux4, Iop_Max64Ux2 };
   vassert(size < 4);
   return ops[size];
}

static IROp mkVecMAXS ( UInt size ) {
   const IROp ops[4]
      = { Iop_Max8Sx16, Iop_Max16Sx8, Iop_Max32Sx4, Iop_Max64Sx2 };
   vassert(size < 4);
   return ops[size];
}

static IROp mkVecMINU ( UInt size ) {
   const IROp ops[4]
      = { Iop_Min8Ux16, Iop_Min16Ux8, Iop_Min32Ux4, Iop_Min64Ux2 };
   vassert(size < 4);
   return ops[size];
}

static IROp mkVecMINS ( UInt size ) {
   const IROp ops[4]
      = { Iop_Min8Sx16, Iop_Min16Sx8, Iop_Min32Sx4, Iop_Min64Sx2 };
   vassert(size < 4);
   return ops[size];
}

static IROp mkVecMUL ( UInt size ) {
   const IROp ops[4]
      = { Iop_Mul8x16, Iop_Mul16x8, Iop_Mul32x4, Iop_INVALID };
   vassert(size < 3);
   return ops[size];
}

static IROp mkVecMULLU ( UInt sizeNarrow ) {
   const IROp ops[4]
      = { Iop_Mull8Ux8, Iop_Mull16Ux4, Iop_Mull32Ux2, Iop_INVALID };
   vassert(sizeNarrow < 3);
   return ops[sizeNarrow];
}

static IROp mkVecMULLS ( UInt sizeNarrow ) {
   const IROp ops[4]
      = { Iop_Mull8Sx8, Iop_Mull16Sx4, Iop_Mull32Sx2, Iop_INVALID };
   vassert(sizeNarrow < 3);
   return ops[sizeNarrow];
}

static IROp mkVecQDMULLS ( UInt sizeNarrow ) {
   const IROp ops[4]
      = { Iop_INVALID, Iop_QDMull16Sx4, Iop_QDMull32Sx2, Iop_INVALID };
   vassert(sizeNarrow < 3);
   return ops[sizeNarrow];
}

static IROp mkVecCMPEQ ( UInt size ) {
   const IROp ops[4]
      = { Iop_CmpEQ8x16, Iop_CmpEQ16x8, Iop_CmpEQ32x4, Iop_CmpEQ64x2 };
   vassert(size < 4);
   return ops[size];
}

static IROp mkVecCMPGTU ( UInt size ) {
   const IROp ops[4]
      = { Iop_CmpGT8Ux16, Iop_CmpGT16Ux8, Iop_CmpGT32Ux4, Iop_CmpGT64Ux2 };
   vassert(size < 4);
   return ops[size];
}

static IROp mkVecCMPGTS ( UInt size ) {
   const IROp ops[4]
      = { Iop_CmpGT8Sx16, Iop_CmpGT16Sx8, Iop_CmpGT32Sx4, Iop_CmpGT64Sx2 };
   vassert(size < 4);
   return ops[size];
}

static IROp mkVecABS ( UInt size ) {
   const IROp ops[4]
      = { Iop_Abs8x16, Iop_Abs16x8, Iop_Abs32x4, Iop_Abs64x2 };
   vassert(size < 4);
   return ops[size];
}

static IROp mkVecZEROHIxxOFV128 ( UInt size ) {
   const IROp ops[4]
      = { Iop_ZeroHI120ofV128, Iop_ZeroHI112ofV128,
          Iop_ZeroHI96ofV128,  Iop_ZeroHI64ofV128 };
   vassert(size < 4);
   return ops[size];
}

static IRExpr* mkU ( IRType ty, ULong imm ) {
   switch (ty) {
      case Ity_I32: return mkU32((UInt)(imm & 0xFFFFFFFFULL));
      case Ity_I64: return mkU64(imm);
      default: vpanic("mkU");
   }
}

static IROp mkVecQDMULHIS ( UInt size ) {
   const IROp ops[4]
      = { Iop_INVALID, Iop_QDMulHi16Sx8, Iop_QDMulHi32Sx4, Iop_INVALID };
   vassert(size < 4);
   return ops[size];
}

static IROp mkVecQRDMULHIS ( UInt size ) {
   const IROp ops[4]
      = { Iop_INVALID, Iop_QRDMulHi16Sx8, Iop_QRDMulHi32Sx4, Iop_INVALID };
   vassert(size < 4);
   return ops[size];
}

static IROp mkVecQANDUQSH ( UInt size ) {
   const IROp ops[4]
      = { Iop_QandUQsh8x16, Iop_QandUQsh16x8,
          Iop_QandUQsh32x4, Iop_QandUQsh64x2 };
   vassert(size < 4);
   return ops[size];
}

static IROp mkVecQANDSQSH ( UInt size ) {
   const IROp ops[4]
      = { Iop_QandSQsh8x16, Iop_QandSQsh16x8,
          Iop_QandSQsh32x4, Iop_QandSQsh64x2 };
   vassert(size < 4);
   return ops[size];
}

static IROp mkVecQANDUQRSH ( UInt size ) {
   const IROp ops[4]
      = { Iop_QandUQRsh8x16, Iop_QandUQRsh16x8,
          Iop_QandUQRsh32x4, Iop_QandUQRsh64x2 };
   vassert(size < 4);
   return ops[size];
}

static IROp mkVecQANDSQRSH ( UInt size ) {
   const IROp ops[4]
      = { Iop_QandSQRsh8x16, Iop_QandSQRsh16x8,
          Iop_QandSQRsh32x4, Iop_QandSQRsh64x2 };
   vassert(size < 4);
   return ops[size];
}

static IROp mkVecSHU ( UInt size ) {
   const IROp ops[4]
      = { Iop_Sh8Ux16, Iop_Sh16Ux8, Iop_Sh32Ux4, Iop_Sh64Ux2 };
   vassert(size < 4);
   return ops[size];
}

static IROp mkVecSHS ( UInt size ) {
   const IROp ops[4]
      = { Iop_Sh8Sx16, Iop_Sh16Sx8, Iop_Sh32Sx4, Iop_Sh64Sx2 };
   vassert(size < 4);
   return ops[size];
}

static IROp mkVecRSHU ( UInt size ) {
   const IROp ops[4]
      = { Iop_Rsh8Ux16, Iop_Rsh16Ux8, Iop_Rsh32Ux4, Iop_Rsh64Ux2 };
   vassert(size < 4);
   return ops[size];
}

static IROp mkVecRSHS ( UInt size ) {
   const IROp ops[4]
      = { Iop_Rsh8Sx16, Iop_Rsh16Sx8, Iop_Rsh32Sx4, Iop_Rsh64Sx2 };
   vassert(size < 4);
   return ops[size];
}

static IROp mkVecNARROWUN ( UInt sizeNarrow ) {
   const IROp ops[4]
      = { Iop_NarrowUn16to8x8, Iop_NarrowUn32to16x4,
          Iop_NarrowUn64to32x2, Iop_INVALID };
   vassert(sizeNarrow < 4);
   return ops[sizeNarrow];
}

static IROp mkVecQNARROWUNSU ( UInt sizeNarrow ) {
   const IROp ops[4]
      = { Iop_QNarrowUn16Sto8Ux8,  Iop_QNarrowUn32Sto16Ux4,
          Iop_QNarrowUn64Sto32Ux2, Iop_INVALID };
   vassert(sizeNarrow < 4);
   return ops[sizeNarrow];
}

static IROp mkVecQNARROWUNSS ( UInt sizeNarrow ) {
   const IROp ops[4]
      = { Iop_QNarrowUn16Sto8Sx8,  Iop_QNarrowUn32Sto16Sx4,
          Iop_QNarrowUn64Sto32Sx2, Iop_INVALID };
   vassert(sizeNarrow < 4);
   return ops[sizeNarrow];
}

static IROp mkVecQNARROWUNUU ( UInt sizeNarrow ) {
   const IROp ops[4]
      = { Iop_QNarrowUn16Uto8Ux8,  Iop_QNarrowUn32Uto16Ux4,
          Iop_QNarrowUn64Uto32Ux2, Iop_INVALID };
   vassert(sizeNarrow < 4);
   return ops[sizeNarrow];
}

static IROp mkVecQANDqshrNNARROWUU ( UInt sizeNarrow ) {
   const IROp ops[4]
      = { Iop_QandQShrNnarrow16Uto8Ux8, Iop_QandQShrNnarrow32Uto16Ux4,
          Iop_QandQShrNnarrow64Uto32Ux2, Iop_INVALID };
   vassert(sizeNarrow < 4);
   return ops[sizeNarrow];
}

static IROp mkVecQANDqsarNNARROWSS ( UInt sizeNarrow ) {
   const IROp ops[4]
      = { Iop_QandQSarNnarrow16Sto8Sx8,  Iop_QandQSarNnarrow32Sto16Sx4,
          Iop_QandQSarNnarrow64Sto32Sx2, Iop_INVALID };
   vassert(sizeNarrow < 4);
   return ops[sizeNarrow];
}

static IROp mkVecQANDqsarNNARROWSU ( UInt sizeNarrow ) {
   const IROp ops[4]
      = { Iop_QandQSarNnarrow16Sto8Ux8,  Iop_QandQSarNnarrow32Sto16Ux4,
          Iop_QandQSarNnarrow64Sto32Ux2, Iop_INVALID };
   vassert(sizeNarrow < 4);
   return ops[sizeNarrow];
}

static IROp mkVecQANDqrshrNNARROWUU ( UInt sizeNarrow ) {
   const IROp ops[4]
      = { Iop_QandQRShrNnarrow16Uto8Ux8,  Iop_QandQRShrNnarrow32Uto16Ux4,
          Iop_QandQRShrNnarrow64Uto32Ux2, Iop_INVALID };
   vassert(sizeNarrow < 4);
   return ops[sizeNarrow];
}

static IROp mkVecQANDqrsarNNARROWSS ( UInt sizeNarrow ) {
   const IROp ops[4]
      = { Iop_QandQRSarNnarrow16Sto8Sx8,  Iop_QandQRSarNnarrow32Sto16Sx4,
          Iop_QandQRSarNnarrow64Sto32Sx2, Iop_INVALID };
   vassert(sizeNarrow < 4);
   return ops[sizeNarrow];
}

static IROp mkVecQANDqrsarNNARROWSU ( UInt sizeNarrow ) {
   const IROp ops[4]
      = { Iop_QandQRSarNnarrow16Sto8Ux8,  Iop_QandQRSarNnarrow32Sto16Ux4,
          Iop_QandQRSarNnarrow64Sto32Ux2, Iop_INVALID };
   vassert(sizeNarrow < 4);
   return ops[sizeNarrow];
}

static IROp mkVecQSHLNSATUU ( UInt size ) {
   const IROp ops[4]
      = { Iop_QShlNsatUU8x16, Iop_QShlNsatUU16x8,
          Iop_QShlNsatUU32x4, Iop_QShlNsatUU64x2 };
   vassert(size < 4);
   return ops[size];
}

static IROp mkVecQSHLNSATSS ( UInt size ) {
   const IROp ops[4]
      = { Iop_QShlNsatSS8x16, Iop_QShlNsatSS16x8,
          Iop_QShlNsatSS32x4, Iop_QShlNsatSS64x2 };
   vassert(size < 4);
   return ops[size];
}

static IROp mkVecQSHLNSATSU ( UInt size ) {
   const IROp ops[4]
      = { Iop_QShlNsatSU8x16, Iop_QShlNsatSU16x8,
          Iop_QShlNsatSU32x4, Iop_QShlNsatSU64x2 };
   vassert(size < 4);
   return ops[size];
}

static IROp mkVecADDF ( UInt size ) {
   const IROp ops[4]
      = { Iop_INVALID, Iop_INVALID, Iop_Add32Fx4, Iop_Add64Fx2 };
   vassert(size < 4);
   return ops[size];
}

static IROp mkVecMAXF ( UInt size ) {
   const IROp ops[4]
      = { Iop_INVALID, Iop_INVALID, Iop_Max32Fx4, Iop_Max64Fx2 };
   vassert(size < 4);
   return ops[size];
}

static IROp mkVecMINF ( UInt size ) {
   const IROp ops[4]
      = { Iop_INVALID, Iop_INVALID, Iop_Min32Fx4, Iop_Min64Fx2 };
   vassert(size < 4);
   return ops[size];
}

static IRTemp mathROR ( IRType ty, IRTemp arg, UInt imm )
{
   UInt w = 0;
   if (ty == Ity_I64) {
      w = 64;
   } else {
      vassert(ty == Ity_I32);
      w = 32;
   }
   vassert(w != 0);
   vassert(imm < w);
   if (imm == 0) {
      return arg;
   }
   IRTemp res = newTemp(ty);
   assign(res, binop(mkOR(ty),
                     binop(mkSHL(ty), mkexpr(arg), mkU8(w - imm)),
                     binop(mkSHR(ty), mkexpr(arg), mkU8(imm)) ));
   return res;
}

static IRTemp mathREPLICATE ( IRType ty, IRTemp arg, UInt imm )
{
   UInt w = 0;
   if (ty == Ity_I64) {
      w = 64;
   } else {
      vassert(ty == Ity_I32);
      w = 32;
   }
   vassert(w != 0);
   vassert(imm < w);
   IRTemp res = newTemp(ty);
   assign(res, binop(mkSAR(ty),
                     binop(mkSHL(ty), mkexpr(arg), mkU8(w - 1 - imm)),
                     mkU8(w - 1)));
   return res;
}

static IRExpr* widenUto64 ( IRType srcTy, IRExpr* e )
{
   switch (srcTy) {
      case Ity_I64: return e;
      case Ity_I32: return unop(Iop_32Uto64, e);
      case Ity_I16: return unop(Iop_16Uto64, e);
      case Ity_I8:  return unop(Iop_8Uto64, e);
      default: vpanic("widenUto64(arm64)");
   }
}

static IRExpr* narrowFrom64 ( IRType dstTy, IRExpr* e )
{
   switch (dstTy) {
      case Ity_I64: return e;
      case Ity_I32: return unop(Iop_64to32, e);
      case Ity_I16: return unop(Iop_64to16, e);
      case Ity_I8:  return unop(Iop_64to8, e);
      default: vpanic("narrowFrom64(arm64)");
   }
}



#define OFFB_X0       offsetof(VexGuestARM64State,guest_X0)
#define OFFB_X1       offsetof(VexGuestARM64State,guest_X1)
#define OFFB_X2       offsetof(VexGuestARM64State,guest_X2)
#define OFFB_X3       offsetof(VexGuestARM64State,guest_X3)
#define OFFB_X4       offsetof(VexGuestARM64State,guest_X4)
#define OFFB_X5       offsetof(VexGuestARM64State,guest_X5)
#define OFFB_X6       offsetof(VexGuestARM64State,guest_X6)
#define OFFB_X7       offsetof(VexGuestARM64State,guest_X7)
#define OFFB_X8       offsetof(VexGuestARM64State,guest_X8)
#define OFFB_X9       offsetof(VexGuestARM64State,guest_X9)
#define OFFB_X10      offsetof(VexGuestARM64State,guest_X10)
#define OFFB_X11      offsetof(VexGuestARM64State,guest_X11)
#define OFFB_X12      offsetof(VexGuestARM64State,guest_X12)
#define OFFB_X13      offsetof(VexGuestARM64State,guest_X13)
#define OFFB_X14      offsetof(VexGuestARM64State,guest_X14)
#define OFFB_X15      offsetof(VexGuestARM64State,guest_X15)
#define OFFB_X16      offsetof(VexGuestARM64State,guest_X16)
#define OFFB_X17      offsetof(VexGuestARM64State,guest_X17)
#define OFFB_X18      offsetof(VexGuestARM64State,guest_X18)
#define OFFB_X19      offsetof(VexGuestARM64State,guest_X19)
#define OFFB_X20      offsetof(VexGuestARM64State,guest_X20)
#define OFFB_X21      offsetof(VexGuestARM64State,guest_X21)
#define OFFB_X22      offsetof(VexGuestARM64State,guest_X22)
#define OFFB_X23      offsetof(VexGuestARM64State,guest_X23)
#define OFFB_X24      offsetof(VexGuestARM64State,guest_X24)
#define OFFB_X25      offsetof(VexGuestARM64State,guest_X25)
#define OFFB_X26      offsetof(VexGuestARM64State,guest_X26)
#define OFFB_X27      offsetof(VexGuestARM64State,guest_X27)
#define OFFB_X28      offsetof(VexGuestARM64State,guest_X28)
#define OFFB_X29      offsetof(VexGuestARM64State,guest_X29)
#define OFFB_X30      offsetof(VexGuestARM64State,guest_X30)

#define OFFB_XSP      offsetof(VexGuestARM64State,guest_XSP)
#define OFFB_PC       offsetof(VexGuestARM64State,guest_PC)

#define OFFB_CC_OP    offsetof(VexGuestARM64State,guest_CC_OP)
#define OFFB_CC_DEP1  offsetof(VexGuestARM64State,guest_CC_DEP1)
#define OFFB_CC_DEP2  offsetof(VexGuestARM64State,guest_CC_DEP2)
#define OFFB_CC_NDEP  offsetof(VexGuestARM64State,guest_CC_NDEP)

#define OFFB_TPIDR_EL0 offsetof(VexGuestARM64State,guest_TPIDR_EL0)
#define OFFB_NRADDR   offsetof(VexGuestARM64State,guest_NRADDR)

#define OFFB_Q0       offsetof(VexGuestARM64State,guest_Q0)
#define OFFB_Q1       offsetof(VexGuestARM64State,guest_Q1)
#define OFFB_Q2       offsetof(VexGuestARM64State,guest_Q2)
#define OFFB_Q3       offsetof(VexGuestARM64State,guest_Q3)
#define OFFB_Q4       offsetof(VexGuestARM64State,guest_Q4)
#define OFFB_Q5       offsetof(VexGuestARM64State,guest_Q5)
#define OFFB_Q6       offsetof(VexGuestARM64State,guest_Q6)
#define OFFB_Q7       offsetof(VexGuestARM64State,guest_Q7)
#define OFFB_Q8       offsetof(VexGuestARM64State,guest_Q8)
#define OFFB_Q9       offsetof(VexGuestARM64State,guest_Q9)
#define OFFB_Q10      offsetof(VexGuestARM64State,guest_Q10)
#define OFFB_Q11      offsetof(VexGuestARM64State,guest_Q11)
#define OFFB_Q12      offsetof(VexGuestARM64State,guest_Q12)
#define OFFB_Q13      offsetof(VexGuestARM64State,guest_Q13)
#define OFFB_Q14      offsetof(VexGuestARM64State,guest_Q14)
#define OFFB_Q15      offsetof(VexGuestARM64State,guest_Q15)
#define OFFB_Q16      offsetof(VexGuestARM64State,guest_Q16)
#define OFFB_Q17      offsetof(VexGuestARM64State,guest_Q17)
#define OFFB_Q18      offsetof(VexGuestARM64State,guest_Q18)
#define OFFB_Q19      offsetof(VexGuestARM64State,guest_Q19)
#define OFFB_Q20      offsetof(VexGuestARM64State,guest_Q20)
#define OFFB_Q21      offsetof(VexGuestARM64State,guest_Q21)
#define OFFB_Q22      offsetof(VexGuestARM64State,guest_Q22)
#define OFFB_Q23      offsetof(VexGuestARM64State,guest_Q23)
#define OFFB_Q24      offsetof(VexGuestARM64State,guest_Q24)
#define OFFB_Q25      offsetof(VexGuestARM64State,guest_Q25)
#define OFFB_Q26      offsetof(VexGuestARM64State,guest_Q26)
#define OFFB_Q27      offsetof(VexGuestARM64State,guest_Q27)
#define OFFB_Q28      offsetof(VexGuestARM64State,guest_Q28)
#define OFFB_Q29      offsetof(VexGuestARM64State,guest_Q29)
#define OFFB_Q30      offsetof(VexGuestARM64State,guest_Q30)
#define OFFB_Q31      offsetof(VexGuestARM64State,guest_Q31)

#define OFFB_FPCR     offsetof(VexGuestARM64State,guest_FPCR)
#define OFFB_QCFLAG   offsetof(VexGuestARM64State,guest_QCFLAG)

#define OFFB_CMSTART  offsetof(VexGuestARM64State,guest_CMSTART)
#define OFFB_CMLEN    offsetof(VexGuestARM64State,guest_CMLEN)



static Int offsetIReg64 ( UInt iregNo )
{
   switch (iregNo) {
      case 0:  return OFFB_X0;
      case 1:  return OFFB_X1;
      case 2:  return OFFB_X2;
      case 3:  return OFFB_X3;
      case 4:  return OFFB_X4;
      case 5:  return OFFB_X5;
      case 6:  return OFFB_X6;
      case 7:  return OFFB_X7;
      case 8:  return OFFB_X8;
      case 9:  return OFFB_X9;
      case 10: return OFFB_X10;
      case 11: return OFFB_X11;
      case 12: return OFFB_X12;
      case 13: return OFFB_X13;
      case 14: return OFFB_X14;
      case 15: return OFFB_X15;
      case 16: return OFFB_X16;
      case 17: return OFFB_X17;
      case 18: return OFFB_X18;
      case 19: return OFFB_X19;
      case 20: return OFFB_X20;
      case 21: return OFFB_X21;
      case 22: return OFFB_X22;
      case 23: return OFFB_X23;
      case 24: return OFFB_X24;
      case 25: return OFFB_X25;
      case 26: return OFFB_X26;
      case 27: return OFFB_X27;
      case 28: return OFFB_X28;
      case 29: return OFFB_X29;
      case 30: return OFFB_X30;
      
      default: vassert(0);
   }
}

static Int offsetIReg64orSP ( UInt iregNo )
{
   return iregNo == 31  ? OFFB_XSP  : offsetIReg64(iregNo);
}

static const HChar* nameIReg64orZR ( UInt iregNo )
{
   vassert(iregNo < 32);
   static const HChar* names[32]
      = { "x0",  "x1",  "x2",  "x3",  "x4",  "x5",  "x6",  "x7", 
          "x8",  "x9",  "x10", "x11", "x12", "x13", "x14", "x15", 
          "x16", "x17", "x18", "x19", "x20", "x21", "x22", "x23", 
          "x24", "x25", "x26", "x27", "x28", "x29", "x30", "xzr" };
   return names[iregNo];
}

static const HChar* nameIReg64orSP ( UInt iregNo )
{
   if (iregNo == 31) {
      return "sp";
   }
   vassert(iregNo < 31);
   return nameIReg64orZR(iregNo);
}

static IRExpr* getIReg64orSP ( UInt iregNo )
{
   vassert(iregNo < 32);
   return IRExpr_Get( offsetIReg64orSP(iregNo), Ity_I64 );
}

static IRExpr* getIReg64orZR ( UInt iregNo )
{
   if (iregNo == 31) {
      return mkU64(0);
   }
   vassert(iregNo < 31);
   return IRExpr_Get( offsetIReg64orSP(iregNo), Ity_I64 );
}

static void putIReg64orSP ( UInt iregNo, IRExpr* e ) 
{
   vassert(typeOfIRExpr(irsb->tyenv, e) == Ity_I64);
   stmt( IRStmt_Put(offsetIReg64orSP(iregNo), e) );
}

static void putIReg64orZR ( UInt iregNo, IRExpr* e ) 
{
   vassert(typeOfIRExpr(irsb->tyenv, e) == Ity_I64);
   if (iregNo == 31) {
      return;
   }
   vassert(iregNo < 31);
   stmt( IRStmt_Put(offsetIReg64orSP(iregNo), e) );
}

static const HChar* nameIReg32orZR ( UInt iregNo )
{
   vassert(iregNo < 32);
   static const HChar* names[32]
      = { "w0",  "w1",  "w2",  "w3",  "w4",  "w5",  "w6",  "w7", 
          "w8",  "w9",  "w10", "w11", "w12", "w13", "w14", "w15", 
          "w16", "w17", "w18", "w19", "w20", "w21", "w22", "w23", 
          "w24", "w25", "w26", "w27", "w28", "w29", "w30", "wzr" };
   return names[iregNo];
}

static const HChar* nameIReg32orSP ( UInt iregNo )
{
   if (iregNo == 31) {
      return "wsp";
   }
   vassert(iregNo < 31);
   return nameIReg32orZR(iregNo);
}

static IRExpr* getIReg32orSP ( UInt iregNo )
{
   vassert(iregNo < 32);
   return unop(Iop_64to32,
               IRExpr_Get( offsetIReg64orSP(iregNo), Ity_I64 ));
}

static IRExpr* getIReg32orZR ( UInt iregNo )
{
   if (iregNo == 31) {
      return mkU32(0);
   }
   vassert(iregNo < 31);
   return unop(Iop_64to32,
               IRExpr_Get( offsetIReg64orSP(iregNo), Ity_I64 ));
}

static void putIReg32orSP ( UInt iregNo, IRExpr* e ) 
{
   vassert(typeOfIRExpr(irsb->tyenv, e) == Ity_I32);
   stmt( IRStmt_Put(offsetIReg64orSP(iregNo), unop(Iop_32Uto64, e)) );
}

static void putIReg32orZR ( UInt iregNo, IRExpr* e ) 
{
   vassert(typeOfIRExpr(irsb->tyenv, e) == Ity_I32);
   if (iregNo == 31) {
      return;
   }
   vassert(iregNo < 31);
   stmt( IRStmt_Put(offsetIReg64orSP(iregNo), unop(Iop_32Uto64, e)) );
}

static const HChar* nameIRegOrSP ( Bool is64, UInt iregNo )
{
   vassert(is64 == True || is64 == False);
   return is64 ? nameIReg64orSP(iregNo) : nameIReg32orSP(iregNo);
}

static const HChar* nameIRegOrZR ( Bool is64, UInt iregNo )
{
   vassert(is64 == True || is64 == False);
   return is64 ? nameIReg64orZR(iregNo) : nameIReg32orZR(iregNo);
}

static IRExpr* getIRegOrZR ( Bool is64, UInt iregNo )
{
   vassert(is64 == True || is64 == False);
   return is64 ? getIReg64orZR(iregNo) : getIReg32orZR(iregNo);
}

static void putIRegOrZR ( Bool is64, UInt iregNo, IRExpr* e )
{
   vassert(is64 == True || is64 == False);
   if (is64) putIReg64orZR(iregNo, e); else putIReg32orZR(iregNo, e);
}

static void putPC ( IRExpr* e )
{
   vassert(typeOfIRExpr(irsb->tyenv, e) == Ity_I64);
   stmt( IRStmt_Put(OFFB_PC, e) );
}



static Int offsetQReg128 ( UInt qregNo )
{
   switch (qregNo) {
      case 0:  return OFFB_Q0;
      case 1:  return OFFB_Q1;
      case 2:  return OFFB_Q2;
      case 3:  return OFFB_Q3;
      case 4:  return OFFB_Q4;
      case 5:  return OFFB_Q5;
      case 6:  return OFFB_Q6;
      case 7:  return OFFB_Q7;
      case 8:  return OFFB_Q8;
      case 9:  return OFFB_Q9;
      case 10: return OFFB_Q10;
      case 11: return OFFB_Q11;
      case 12: return OFFB_Q12;
      case 13: return OFFB_Q13;
      case 14: return OFFB_Q14;
      case 15: return OFFB_Q15;
      case 16: return OFFB_Q16;
      case 17: return OFFB_Q17;
      case 18: return OFFB_Q18;
      case 19: return OFFB_Q19;
      case 20: return OFFB_Q20;
      case 21: return OFFB_Q21;
      case 22: return OFFB_Q22;
      case 23: return OFFB_Q23;
      case 24: return OFFB_Q24;
      case 25: return OFFB_Q25;
      case 26: return OFFB_Q26;
      case 27: return OFFB_Q27;
      case 28: return OFFB_Q28;
      case 29: return OFFB_Q29;
      case 30: return OFFB_Q30;
      case 31: return OFFB_Q31;
      default: vassert(0);
   }
}

static void putQReg128 ( UInt qregNo, IRExpr* e )
{
   vassert(qregNo < 32);
   vassert(typeOfIRExpr(irsb->tyenv, e) == Ity_V128);
   stmt( IRStmt_Put(offsetQReg128(qregNo), e) );
}

static IRExpr* getQReg128 ( UInt qregNo )
{
   vassert(qregNo < 32);
   return IRExpr_Get(offsetQReg128(qregNo), Ity_V128);
}

static IRType preferredVectorSubTypeFromSize ( UInt szB )
{
   switch (szB) {
      case 1:  return Ity_I8;
      case 2:  return Ity_I16;
      case 4:  return Ity_I32; 
      case 8:  return Ity_F64;
      case 16: return Ity_V128;
      default: vassert(0);
   }
}

static Int offsetQRegLane ( UInt qregNo, IRType laneTy, UInt laneNo )
{
   vassert(host_endness == VexEndnessLE);
   Int base = offsetQReg128(qregNo);
   UInt laneSzB = 0;
   switch (laneTy) {
      case Ity_I8:                 laneSzB = 1;  break;
      case Ity_F16: case Ity_I16:  laneSzB = 2;  break;
      case Ity_F32: case Ity_I32:  laneSzB = 4;  break;
      case Ity_F64: case Ity_I64:  laneSzB = 8;  break;
      case Ity_V128:               laneSzB = 16; break;
      default: break;
   }
   vassert(laneSzB > 0);
   UInt minOff = laneNo * laneSzB;
   UInt maxOff = minOff + laneSzB - 1;
   vassert(maxOff < 16);
   return base + minOff;
}

static void putQRegLO ( UInt qregNo, IRExpr* e )
{
   IRType ty  = typeOfIRExpr(irsb->tyenv, e);
   Int    off = offsetQRegLane(qregNo, ty, 0);
   switch (ty) {
      case Ity_I8:  case Ity_I16: case Ity_I32: case Ity_I64:
      case Ity_F16: case Ity_F32: case Ity_F64: case Ity_V128:
         break;
      default:
         vassert(0); 
   }
   stmt(IRStmt_Put(off, e));
}

static IRExpr* getQRegLO ( UInt qregNo, IRType ty )
{
   Int off = offsetQRegLane(qregNo, ty, 0);
   switch (ty) {
      case Ity_I8:
      case Ity_F16: case Ity_I16:
      case Ity_I32: case Ity_I64:
      case Ity_F32: case Ity_F64: case Ity_V128:
         break;
      default:
         vassert(0); 
   }
   return IRExpr_Get(off, ty);
}

static const HChar* nameQRegLO ( UInt qregNo, IRType laneTy )
{
   static const HChar* namesQ[32]
      = { "q0",  "q1",  "q2",  "q3",  "q4",  "q5",  "q6",  "q7", 
          "q8",  "q9",  "q10", "q11", "q12", "q13", "q14", "q15", 
          "q16", "q17", "q18", "q19", "q20", "q21", "q22", "q23", 
          "q24", "q25", "q26", "q27", "q28", "q29", "q30", "q31" };
   static const HChar* namesD[32]
      = { "d0",  "d1",  "d2",  "d3",  "d4",  "d5",  "d6",  "d7", 
          "d8",  "d9",  "d10", "d11", "d12", "d13", "d14", "d15", 
          "d16", "d17", "d18", "d19", "d20", "d21", "d22", "d23", 
          "d24", "d25", "d26", "d27", "d28", "d29", "d30", "d31" };
   static const HChar* namesS[32]
      = { "s0",  "s1",  "s2",  "s3",  "s4",  "s5",  "s6",  "s7", 
          "s8",  "s9",  "s10", "s11", "s12", "s13", "s14", "s15", 
          "s16", "s17", "s18", "s19", "s20", "s21", "s22", "s23", 
          "s24", "s25", "s26", "s27", "s28", "s29", "s30", "s31" };
   static const HChar* namesH[32]
      = { "h0",  "h1",  "h2",  "h3",  "h4",  "h5",  "h6",  "h7", 
          "h8",  "h9",  "h10", "h11", "h12", "h13", "h14", "h15", 
          "h16", "h17", "h18", "h19", "h20", "h21", "h22", "h23", 
          "h24", "h25", "h26", "h27", "h28", "h29", "h30", "h31" };
   static const HChar* namesB[32]
      = { "b0",  "b1",  "b2",  "b3",  "b4",  "b5",  "b6",  "b7", 
          "b8",  "b9",  "b10", "b11", "b12", "b13", "b14", "b15", 
          "b16", "b17", "b18", "b19", "b20", "b21", "b22", "b23", 
          "b24", "b25", "b26", "b27", "b28", "b29", "b30", "b31" };
   vassert(qregNo < 32);
   switch (sizeofIRType(laneTy)) {
      case 1:  return namesB[qregNo];
      case 2:  return namesH[qregNo];
      case 4:  return namesS[qregNo];
      case 8:  return namesD[qregNo];
      case 16: return namesQ[qregNo];
      default: vassert(0);
   }
   
}

static const HChar* nameQReg128 ( UInt qregNo )
{
   return nameQRegLO(qregNo, Ity_V128);
}

static Int offsetQRegHI64 ( UInt qregNo )
{
   return offsetQRegLane(qregNo, Ity_I64, 1);
}

static IRExpr* getQRegHI64 ( UInt qregNo )
{
   return IRExpr_Get(offsetQRegHI64(qregNo), Ity_I64);
}

static void putQRegHI64 ( UInt qregNo, IRExpr* e )
{
   IRType ty  = typeOfIRExpr(irsb->tyenv, e);
   Int    off = offsetQRegHI64(qregNo);
   switch (ty) {
      case Ity_I64: case Ity_F64:
         break;
      default:
         vassert(0); 
   }
   stmt(IRStmt_Put(off, e));
}

static void putQRegLane ( UInt qregNo, UInt laneNo, IRExpr* e )
{
   IRType laneTy  = typeOfIRExpr(irsb->tyenv, e);
   Int    off     = offsetQRegLane(qregNo, laneTy, laneNo);
   switch (laneTy) {
      case Ity_F64: case Ity_I64:
      case Ity_I32: case Ity_F32:
      case Ity_I16: case Ity_F16:
      case Ity_I8:
         break;
      default:
         vassert(0); 
   }
   stmt(IRStmt_Put(off, e));
}

static IRExpr* getQRegLane ( UInt qregNo, UInt laneNo, IRType laneTy )
{
   Int off = offsetQRegLane(qregNo, laneTy, laneNo);
   switch (laneTy) {
      case Ity_I64: case Ity_I32: case Ity_I16: case Ity_I8:
      case Ity_F64: case Ity_F32: case Ity_F16:
         break;
      default:
         vassert(0); 
   }
   return IRExpr_Get(off, laneTy);
}





static IRTemp  mk_get_IR_rounding_mode ( void )
{
   IRTemp armEncd = newTemp(Ity_I32);
   IRTemp swapped = newTemp(Ity_I32);
   assign(armEncd,
          binop(Iop_Shr32, IRExpr_Get(OFFB_FPCR, Ity_I32), mkU8(22)));
   
   assign(swapped,
          binop(Iop_Or32,
                binop(Iop_And32,
                      binop(Iop_Shl32, mkexpr(armEncd), mkU8(1)),
                      mkU32(2)),
                binop(Iop_And32,
                      binop(Iop_Shr32, mkexpr(armEncd), mkU8(1)),
                      mkU32(1))
         ));
   return swapped;
}



static const HChar* nameARM64Condcode ( ARM64Condcode cond )
{
   switch (cond) {
      case ARM64CondEQ:  return "eq";
      case ARM64CondNE:  return "ne";
      case ARM64CondCS:  return "cs";  
      case ARM64CondCC:  return "cc";  
      case ARM64CondMI:  return "mi";
      case ARM64CondPL:  return "pl";
      case ARM64CondVS:  return "vs";
      case ARM64CondVC:  return "vc";
      case ARM64CondHI:  return "hi";
      case ARM64CondLS:  return "ls";
      case ARM64CondGE:  return "ge";
      case ARM64CondLT:  return "lt";
      case ARM64CondGT:  return "gt";
      case ARM64CondLE:  return "le";
      case ARM64CondAL:  return "al";
      case ARM64CondNV:  return "nv";
      default: vpanic("name_ARM64Condcode");
   }
}

static const HChar* nameCC ( ARM64Condcode cond ) {
   return nameARM64Condcode(cond);
}


static IRExpr* mk_arm64g_calculate_condition_dyn ( IRExpr* cond )
{
   vassert(typeOfIRExpr(irsb->tyenv, cond) == Ity_I64);

   IRExpr** args
      = mkIRExprVec_4(
           binop(Iop_Or64, IRExpr_Get(OFFB_CC_OP, Ity_I64), cond),
           IRExpr_Get(OFFB_CC_DEP1, Ity_I64),
           IRExpr_Get(OFFB_CC_DEP2, Ity_I64),
           IRExpr_Get(OFFB_CC_NDEP, Ity_I64)
        );
   IRExpr* call
      = mkIRExprCCall(
           Ity_I64,
           0, 
           "arm64g_calculate_condition", &arm64g_calculate_condition,
           args
        );

   call->Iex.CCall.cee->mcx_mask = (1<<0) | (1<<3);
   return call;
}


static IRExpr* mk_arm64g_calculate_condition ( ARM64Condcode cond )
{
   vassert(cond >= 0 && cond <= 15);
   return mk_arm64g_calculate_condition_dyn( mkU64(cond << 4) );
}


static IRExpr* mk_arm64g_calculate_flag_c ( void )
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
           "arm64g_calculate_flag_c", &arm64g_calculate_flag_c,
           args
        );
   call->Iex.CCall.cee->mcx_mask = (1<<0) | (1<<3);
   return call;
}




static IRExpr* mk_arm64g_calculate_flags_nzcv ( void )
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
           "arm64g_calculate_flags_nzcv", &arm64g_calculate_flags_nzcv,
           args
        );
   call->Iex.CCall.cee->mcx_mask = (1<<0) | (1<<3);
   return call;
}


static
void setFlags_D1_D2_ND ( UInt cc_op,
                         IRTemp t_dep1, IRTemp t_dep2, IRTemp t_ndep )
{
   vassert(typeOfIRTemp(irsb->tyenv, t_dep1 == Ity_I64));
   vassert(typeOfIRTemp(irsb->tyenv, t_dep2 == Ity_I64));
   vassert(typeOfIRTemp(irsb->tyenv, t_ndep == Ity_I64));
   vassert(cc_op >= ARM64G_CC_OP_COPY && cc_op < ARM64G_CC_OP_NUMBER);
   stmt( IRStmt_Put( OFFB_CC_OP,   mkU64(cc_op) ));
   stmt( IRStmt_Put( OFFB_CC_DEP1, mkexpr(t_dep1) ));
   stmt( IRStmt_Put( OFFB_CC_DEP2, mkexpr(t_dep2) ));
   stmt( IRStmt_Put( OFFB_CC_NDEP, mkexpr(t_ndep) ));
}

static
void setFlags_ADD_SUB ( Bool is64, Bool isSUB, IRTemp argL, IRTemp argR )
{
   IRTemp argL64 = IRTemp_INVALID;
   IRTemp argR64 = IRTemp_INVALID;
   IRTemp z64    = newTemp(Ity_I64);
   if (is64) {
      argL64 = argL;
      argR64 = argR;
   } else {
      argL64 = newTemp(Ity_I64);
      argR64 = newTemp(Ity_I64);
      assign(argL64, unop(Iop_32Uto64, mkexpr(argL)));
      assign(argR64, unop(Iop_32Uto64, mkexpr(argR)));
   }
   assign(z64, mkU64(0));
   UInt cc_op = ARM64G_CC_OP_NUMBER;
    if ( isSUB &&  is64) { cc_op = ARM64G_CC_OP_SUB64; }
   else if ( isSUB && !is64) { cc_op = ARM64G_CC_OP_SUB32; }
   else if (!isSUB &&  is64) { cc_op = ARM64G_CC_OP_ADD64; }
   else if (!isSUB && !is64) { cc_op = ARM64G_CC_OP_ADD32; }
   else                      { vassert(0); }
   setFlags_D1_D2_ND(cc_op, argL64, argR64, z64);
}

static
void setFlags_ADC_SBC ( Bool is64, Bool isSBC,
                        IRTemp argL, IRTemp argR, IRTemp oldC )
{
   IRTemp argL64 = IRTemp_INVALID;
   IRTemp argR64 = IRTemp_INVALID;
   IRTemp oldC64 = IRTemp_INVALID;
   if (is64) {
      argL64 = argL;
      argR64 = argR;
      oldC64 = oldC;
   } else {
      argL64 = newTemp(Ity_I64);
      argR64 = newTemp(Ity_I64);
      oldC64 = newTemp(Ity_I64);
      assign(argL64, unop(Iop_32Uto64, mkexpr(argL)));
      assign(argR64, unop(Iop_32Uto64, mkexpr(argR)));
      assign(oldC64, unop(Iop_32Uto64, mkexpr(oldC)));
   }
   UInt cc_op = ARM64G_CC_OP_NUMBER;
    if ( isSBC &&  is64) { cc_op = ARM64G_CC_OP_SBC64; }
   else if ( isSBC && !is64) { cc_op = ARM64G_CC_OP_SBC32; }
   else if (!isSBC &&  is64) { cc_op = ARM64G_CC_OP_ADC64; }
   else if (!isSBC && !is64) { cc_op = ARM64G_CC_OP_ADC32; }
   else                      { vassert(0); }
   setFlags_D1_D2_ND(cc_op, argL64, argR64, oldC64);
}

static
void setFlags_ADD_SUB_conditionally (
        Bool is64, Bool isSUB,
        IRTemp cond, IRTemp argL, IRTemp argR, UInt nzcv
     )
{

   IRTemp z64 = newTemp(Ity_I64);
   assign(z64, mkU64(0));

   
   IRTemp t_dep1 = IRTemp_INVALID;
   IRTemp t_dep2 = IRTemp_INVALID;
   UInt   t_op   = ARM64G_CC_OP_NUMBER;
    if ( isSUB &&  is64) { t_op = ARM64G_CC_OP_SUB64; }
   else if ( isSUB && !is64) { t_op = ARM64G_CC_OP_SUB32; }
   else if (!isSUB &&  is64) { t_op = ARM64G_CC_OP_ADD64; }
   else if (!isSUB && !is64) { t_op = ARM64G_CC_OP_ADD32; }
   else                      { vassert(0); }
   
   if (is64) {
      t_dep1 = argL;
      t_dep2 = argR;
   } else {
      t_dep1 = newTemp(Ity_I64);
      t_dep2 = newTemp(Ity_I64);
      assign(t_dep1, unop(Iop_32Uto64, mkexpr(argL)));
      assign(t_dep2, unop(Iop_32Uto64, mkexpr(argR)));
   }

   
   IRTemp f_dep1 = newTemp(Ity_I64);
   IRTemp f_dep2 = z64;
   UInt   f_op   = ARM64G_CC_OP_COPY;
   assign(f_dep1, mkU64(nzcv << 28));

   
   IRTemp dep1 = newTemp(Ity_I64);
   IRTemp dep2 = newTemp(Ity_I64);
   IRTemp op   = newTemp(Ity_I64);

   assign(op,   IRExpr_ITE(mkexpr(cond), mkU64(t_op), mkU64(f_op)));
   assign(dep1, IRExpr_ITE(mkexpr(cond), mkexpr(t_dep1), mkexpr(f_dep1)));
   assign(dep2, IRExpr_ITE(mkexpr(cond), mkexpr(t_dep2), mkexpr(f_dep2)));

   
   stmt( IRStmt_Put( OFFB_CC_OP,   mkexpr(op) ));
   stmt( IRStmt_Put( OFFB_CC_DEP1, mkexpr(dep1) ));
   stmt( IRStmt_Put( OFFB_CC_DEP2, mkexpr(dep2) ));
   stmt( IRStmt_Put( OFFB_CC_NDEP, mkexpr(z64) ));
}

static
void setFlags_LOGIC ( Bool is64, IRTemp res )
{
   IRTemp res64 = IRTemp_INVALID;
   IRTemp z64   = newTemp(Ity_I64);
   UInt   cc_op = ARM64G_CC_OP_NUMBER;
   if (is64) {
      res64 = res;
      cc_op = ARM64G_CC_OP_LOGIC64;
   } else {
      res64 = newTemp(Ity_I64);
      assign(res64, unop(Iop_32Uto64, mkexpr(res)));
      cc_op = ARM64G_CC_OP_LOGIC32;
   }
   assign(z64, mkU64(0));
   setFlags_D1_D2_ND(cc_op, res64, z64, z64);
}

static
void setFlags_COPY ( IRTemp nzcv_28x0 )
{
   IRTemp z64 = newTemp(Ity_I64);
   assign(z64, mkU64(0));
   setFlags_D1_D2_ND(ARM64G_CC_OP_COPY, nzcv_28x0, z64, z64);
}





static IRTemp math_SWAPHELPER ( IRTemp x, ULong mask, Int sh )
{
   IRTemp maskT = newTemp(Ity_I64);
   IRTemp res   = newTemp(Ity_I64);
   vassert(sh >= 1 && sh <= 63);
   assign(maskT, mkU64(mask));
   assign( res,
           binop(Iop_Or64,
                 binop(Iop_Shr64,
                       binop(Iop_And64,mkexpr(x),mkexpr(maskT)),
                       mkU8(sh)),
                 binop(Iop_And64,
                       binop(Iop_Shl64,mkexpr(x),mkU8(sh)),
                       mkexpr(maskT))
                 ) 
           );
   return res;
}

static IRTemp math_UINTSWAP64 ( IRTemp src )
{
   IRTemp res;
   res = math_SWAPHELPER(src, 0xFF00FF00FF00FF00ULL, 8);
   res = math_SWAPHELPER(res, 0xFFFF0000FFFF0000ULL, 16);
   return res;
}

static IRTemp math_USHORTSWAP64 ( IRTemp src )
{
   IRTemp res;
   res = math_SWAPHELPER(src, 0xFF00FF00FF00FF00ULL, 8);
   return res;
}

static IRTemp math_BYTESWAP64 ( IRTemp src )
{
   IRTemp res;
   res = math_SWAPHELPER(src, 0xFF00FF00FF00FF00ULL, 8);
   res = math_SWAPHELPER(res, 0xFFFF0000FFFF0000ULL, 16);
   res = math_SWAPHELPER(res, 0xFFFFFFFF00000000ULL, 32);
   return res;
}

static IRTemp math_BITSWAP64 ( IRTemp src )
{
   IRTemp res;
   res = math_SWAPHELPER(src, 0xAAAAAAAAAAAAAAAAULL, 1);
   res = math_SWAPHELPER(res, 0xCCCCCCCCCCCCCCCCULL, 2);
   res = math_SWAPHELPER(res, 0xF0F0F0F0F0F0F0F0ULL, 4);
   return math_BYTESWAP64(res);
}

static IRTemp math_DUP_TO_64 ( IRTemp src, IRType srcTy )
{
   if (srcTy == Ity_I8) {
      IRTemp t16 = newTemp(Ity_I64);
      assign(t16, binop(Iop_Or64, mkexpr(src),
                                  binop(Iop_Shl64, mkexpr(src), mkU8(8))));
      IRTemp t32 = newTemp(Ity_I64);
      assign(t32, binop(Iop_Or64, mkexpr(t16),
                                  binop(Iop_Shl64, mkexpr(t16), mkU8(16))));
      IRTemp t64 = newTemp(Ity_I64);
      assign(t64, binop(Iop_Or64, mkexpr(t32),
                                  binop(Iop_Shl64, mkexpr(t32), mkU8(32))));
      return t64;
   }
   if (srcTy == Ity_I16) {
      IRTemp t32 = newTemp(Ity_I64);
      assign(t32, binop(Iop_Or64, mkexpr(src),
                                  binop(Iop_Shl64, mkexpr(src), mkU8(16))));
      IRTemp t64 = newTemp(Ity_I64);
      assign(t64, binop(Iop_Or64, mkexpr(t32),
                                  binop(Iop_Shl64, mkexpr(t32), mkU8(32))));
      return t64;
   }
   if (srcTy == Ity_I32) {
      IRTemp t64 = newTemp(Ity_I64);
      assign(t64, binop(Iop_Or64, mkexpr(src),
                                  binop(Iop_Shl64, mkexpr(src), mkU8(32))));
      return t64;
   }
   if (srcTy == Ity_I64) {
      return src;
   }
   vassert(0);
}


static IRTemp math_DUP_TO_V128 ( IRTemp src, IRType srcTy )
{
   IRTemp res = newTempV128();
   if (srcTy == Ity_F64) {
      IRTemp i64 = newTemp(Ity_I64);
      assign(i64, unop(Iop_ReinterpF64asI64, mkexpr(src)));
      assign(res, binop(Iop_64HLtoV128, mkexpr(i64), mkexpr(i64)));
      return res;
   }
   if (srcTy == Ity_F32) {
      IRTemp i64a = newTemp(Ity_I64);
      assign(i64a, unop(Iop_32Uto64, unop(Iop_ReinterpF32asI32, mkexpr(src))));
      IRTemp i64b = newTemp(Ity_I64);
      assign(i64b, binop(Iop_Or64, binop(Iop_Shl64, mkexpr(i64a), mkU8(32)),
                                   mkexpr(i64a)));
      assign(res, binop(Iop_64HLtoV128, mkexpr(i64b), mkexpr(i64b)));
      return res;
   }
   if (srcTy == Ity_I64) {
      assign(res, binop(Iop_64HLtoV128, mkexpr(src), mkexpr(src)));
      return res;
   }
   if (srcTy == Ity_I32 || srcTy == Ity_I16 || srcTy == Ity_I8) {
      IRTemp t1 = newTemp(Ity_I64);
      assign(t1, widenUto64(srcTy, mkexpr(src)));
      IRTemp t2 = math_DUP_TO_64(t1, srcTy);
      assign(res, binop(Iop_64HLtoV128, mkexpr(t2), mkexpr(t2)));
      return res;
   }
   vassert(0);
}


static IRExpr* math_MAYBE_ZERO_HI64 ( UInt bitQ, IRTemp fullWidth )
{
   if (bitQ == 1) return mkexpr(fullWidth);
   if (bitQ == 0) return unop(Iop_ZeroHI64ofV128, mkexpr(fullWidth));
   vassert(0);
}

static IRExpr* math_MAYBE_ZERO_HI64_fromE ( UInt bitQ, IRExpr* fullWidth )
{
   IRTemp fullWidthT = newTempV128();
   assign(fullWidthT, fullWidth);
   return math_MAYBE_ZERO_HI64(bitQ, fullWidthT);
}



static
IRTemp mk_convert_IRCmpF64Result_to_NZCV ( IRTemp irRes32 )
{
   IRTemp ix       = newTemp(Ity_I64);
   IRTemp termL    = newTemp(Ity_I64);
   IRTemp termR    = newTemp(Ity_I64);
   IRTemp nzcv     = newTemp(Ity_I64);
   IRTemp irRes    = newTemp(Ity_I64);

   
   assign(irRes, unop(Iop_32Uto64, mkexpr(irRes32)));

   assign(
      ix,
      binop(Iop_Or64,
            binop(Iop_And64,
                  binop(Iop_Shr64, mkexpr(irRes), mkU8(5)),
                  mkU64(3)),
            binop(Iop_And64, mkexpr(irRes), mkU64(1))));

   assign(
      termL,
      binop(Iop_Add64,
            binop(Iop_Shr64,
                  binop(Iop_Sub64,
                        binop(Iop_Shl64,
                              binop(Iop_Xor64, mkexpr(ix), mkU64(1)),
                              mkU8(62)),
                        mkU64(1)),
                  mkU8(61)),
            mkU64(1)));

   assign(
      termR,
      binop(Iop_And64,
            binop(Iop_And64,
                  mkexpr(ix),
                  binop(Iop_Shr64, mkexpr(ix), mkU8(1))),
            mkU64(1)));

   assign(nzcv, binop(Iop_Sub64, mkexpr(termL), mkexpr(termR)));
   return nzcv;
}




static ULong dbm_ROR ( Int width, ULong x, Int rot )
{
   vassert(width > 0 && width <= 64);
   vassert(rot >= 0 && rot < width);
   if (rot == 0) return x;
   ULong res = x >> rot;
   res |= (x << (width - rot));
   if (width < 64)
     res &= ((1ULL << width) - 1);
   return res;
}

static ULong dbm_RepTo64( Int esize, ULong x )
{
   switch (esize) {
      case 64:
         return x;
      case 32:
         x &= 0xFFFFFFFF; x |= (x << 32);
         return x;
      case 16:
         x &= 0xFFFF; x |= (x << 16); x |= (x << 32);
         return x;
      case 8:
         x &= 0xFF; x |= (x << 8); x |= (x << 16); x |= (x << 32);
         return x;
      case 4:
         x &= 0xF; x |= (x << 4); x |= (x << 8);
         x |= (x << 16); x |= (x << 32);
         return x;
      case 2:
         x &= 0x3; x |= (x << 2); x |= (x << 4); x |= (x << 8);
         x |= (x << 16); x |= (x << 32);
         return x;
      default:
         break;
   }
   vpanic("dbm_RepTo64");
   
   return 0;
}

static Int dbm_highestSetBit ( ULong x )
{
   Int i;
   for (i = 63; i >= 0; i--) {
      if (x & (1ULL << i))
         return i;
   }
   vassert(x == 0);
   return -1;
}

static
Bool dbm_DecodeBitMasks ( ULong* wmask, ULong* tmask, 
                          ULong immN, ULong imms, ULong immr, Bool immediate,
                          UInt M )
{
   vassert(immN < (1ULL << 1));
   vassert(imms < (1ULL << 6));
   vassert(immr < (1ULL << 6));
   vassert(immediate == False || immediate == True);
   vassert(M == 32 || M == 64);

   Int len = dbm_highestSetBit( ((immN << 6) & 64) | ((~imms) & 63) );
   if (len < 1) {  return False; }
   vassert(len <= 6);
   vassert(M >= (1 << len));

   vassert(len >= 1 && len <= 6);
   ULong levels = 
                  (1 << len) - 1;
   vassert(levels >= 1 && levels <= 63);

   if (immediate && ((imms & levels) == levels)) { 
      
      return False;
   }

   ULong S = imms & levels;
   ULong R = immr & levels;
   Int   diff = S - R;
   diff &= 63;
   Int esize = 1 << len;
   vassert(2 <= esize && esize <= 64);

   vassert(S >= 0 && S <= 63);
   vassert(esize >= (S+1));
   ULong elem_s = 
                  
                  ((1ULL << S) - 1) + (1ULL << S);

   Int d = 
           diff & ((1 << len)-1);
   vassert(esize >= (d+1));
   vassert(d >= 0 && d <= 63);

   ULong elem_d = 
                  
                  ((1ULL << d) - 1) + (1ULL << d);

   if (esize != 64) vassert(elem_s < (1ULL << esize));
   if (esize != 64) vassert(elem_d < (1ULL << esize));

   if (wmask) *wmask = dbm_RepTo64(esize, dbm_ROR(esize, elem_s, R));
   if (tmask) *tmask = dbm_RepTo64(esize, elem_d);

   return True;
}


static
Bool dis_ARM64_data_processing_immediate(DisResult* dres,
                                         UInt insn)
{
#  define INSN(_bMax,_bMin)  SLICE_UInt(insn, (_bMax), (_bMin))


   
   if (INSN(28,24) == BITS5(1,0,0,0,1)) {
      Bool is64   = INSN(31,31) == 1;
      Bool isSub  = INSN(30,30) == 1;
      Bool setCC  = INSN(29,29) == 1;
      UInt sh     = INSN(23,22);
      UInt uimm12 = INSN(21,10);
      UInt nn     = INSN(9,5);
      UInt dd     = INSN(4,0);
      const HChar* nm = isSub ? "sub" : "add";
      if (sh >= 2) {
         
      } else {
         vassert(sh <= 1);
         uimm12 <<= (12 * sh);
         if (is64) {
            IRTemp argL  = newTemp(Ity_I64);
            IRTemp argR  = newTemp(Ity_I64);
            IRTemp res   = newTemp(Ity_I64);
            assign(argL, getIReg64orSP(nn));
            assign(argR, mkU64(uimm12));
            assign(res,  binop(isSub ? Iop_Sub64 : Iop_Add64,
                               mkexpr(argL), mkexpr(argR)));
            if (setCC) {
               putIReg64orZR(dd, mkexpr(res));
               setFlags_ADD_SUB(True, isSub, argL, argR);
               DIP("%ss %s, %s, 0x%x\n",
                   nm, nameIReg64orZR(dd), nameIReg64orSP(nn), uimm12);
            } else {
               putIReg64orSP(dd, mkexpr(res));
               DIP("%s %s, %s, 0x%x\n",
                   nm, nameIReg64orSP(dd), nameIReg64orSP(nn), uimm12);
            }
         } else {
            IRTemp argL  = newTemp(Ity_I32);
            IRTemp argR  = newTemp(Ity_I32);
            IRTemp res   = newTemp(Ity_I32);
            assign(argL, getIReg32orSP(nn));
            assign(argR, mkU32(uimm12));
            assign(res,  binop(isSub ? Iop_Sub32 : Iop_Add32,
                               mkexpr(argL), mkexpr(argR)));
            if (setCC) {
               putIReg32orZR(dd, mkexpr(res));
               setFlags_ADD_SUB(False, isSub, argL, argR);
               DIP("%ss %s, %s, 0x%x\n",
                   nm, nameIReg32orZR(dd), nameIReg32orSP(nn), uimm12);
            } else {
               putIReg32orSP(dd, mkexpr(res));
               DIP("%s %s, %s, 0x%x\n",
                   nm, nameIReg32orSP(dd), nameIReg32orSP(nn), uimm12);
            }
         }
         return True;
      }
   }

   
   if (INSN(28,24) == BITS5(1,0,0,0,0)) {
      UInt  bP    = INSN(31,31);
      UInt  immLo = INSN(30,29);
      UInt  immHi = INSN(23,5);
      UInt  rD    = INSN(4,0);
      ULong uimm  = (immHi << 2) | immLo;
      ULong simm  = sx_to_64(uimm, 21);
      ULong val;
      if (bP) {
         val = (guest_PC_curr_instr & 0xFFFFFFFFFFFFF000ULL) + (simm << 12);
      } else {
         val = guest_PC_curr_instr + simm;
      }
      putIReg64orZR(rD, mkU64(val));
      DIP("adr%s %s, 0x%llx\n", bP ? "p" : "", nameIReg64orZR(rD), val);
      return True;
   }

   
   if (INSN(28,23) == BITS6(1,0,0,1,0,0)) {
      Bool  is64 = INSN(31,31) == 1;
      UInt  op   = INSN(30,29);
      UInt  N    = INSN(22,22);
      UInt  immR = INSN(21,16);
      UInt  immS = INSN(15,10);
      UInt  nn   = INSN(9,5);
      UInt  dd   = INSN(4,0);
      ULong imm  = 0;
      Bool  ok;
      if (N == 1 && !is64) 
         goto after_logic_imm; 
      ok = dbm_DecodeBitMasks(&imm, NULL,
                              N, immS, immR, True, is64 ? 64 : 32);
      if (!ok)
         goto after_logic_imm;

      const HChar* names[4] = { "and", "orr", "eor", "ands" };
      const IROp   ops64[4] = { Iop_And64, Iop_Or64, Iop_Xor64, Iop_And64 };
      const IROp   ops32[4] = { Iop_And32, Iop_Or32, Iop_Xor32, Iop_And32 };

      vassert(op < 4);
      if (is64) {
         IRExpr* argL = getIReg64orZR(nn);
         IRExpr* argR = mkU64(imm);
         IRTemp  res  = newTemp(Ity_I64);
         assign(res, binop(ops64[op], argL, argR));
         if (op < 3) {
            putIReg64orSP(dd, mkexpr(res));
            DIP("%s %s, %s, 0x%llx\n", names[op],
                nameIReg64orSP(dd), nameIReg64orZR(nn), imm);
         } else {
            putIReg64orZR(dd, mkexpr(res));
            setFlags_LOGIC(True, res);
            DIP("%s %s, %s, 0x%llx\n", names[op],
                nameIReg64orZR(dd), nameIReg64orZR(nn), imm);
         }
      } else {
         IRExpr* argL = getIReg32orZR(nn);
         IRExpr* argR = mkU32((UInt)imm);
         IRTemp  res  = newTemp(Ity_I32);
         assign(res, binop(ops32[op], argL, argR));
         if (op < 3) {
            putIReg32orSP(dd, mkexpr(res));
            DIP("%s %s, %s, 0x%x\n", names[op],
                nameIReg32orSP(dd), nameIReg32orZR(nn), (UInt)imm);
         } else {
            putIReg32orZR(dd, mkexpr(res));
            setFlags_LOGIC(False, res);
            DIP("%s %s, %s, 0x%x\n", names[op],
                nameIReg32orZR(dd), nameIReg32orZR(nn), (UInt)imm);
         }
      }
      return True;
   }
   after_logic_imm:

   
   if (INSN(28,23) == BITS6(1,0,0,1,0,1)) {
      Bool is64   = INSN(31,31) == 1;
      UInt subopc = INSN(30,29);
      UInt hw     = INSN(22,21);
      UInt imm16  = INSN(20,5);
      UInt dd     = INSN(4,0);
      if (subopc == BITS2(0,1) || (!is64 && hw >= 2)) {
         
      } else {
         ULong imm64 = ((ULong)imm16) << (16 * hw);
         if (!is64)
            vassert(imm64 < 0x100000000ULL);
         switch (subopc) {
            case BITS2(1,0): 
               putIRegOrZR(is64, dd, is64 ? mkU64(imm64) : mkU32((UInt)imm64));
               DIP("movz %s, 0x%llx\n", nameIRegOrZR(is64, dd), imm64);
               break;
            case BITS2(0,0): 
               imm64 = ~imm64;
               if (!is64)
                  imm64 &= 0xFFFFFFFFULL;
               putIRegOrZR(is64, dd, is64 ? mkU64(imm64) : mkU32((UInt)imm64));
               DIP("movn %s, 0x%llx\n", nameIRegOrZR(is64, dd), imm64);
               break;
            case BITS2(1,1): 
               if (is64) {
                  IRTemp old = newTemp(Ity_I64);
                  assign(old, getIReg64orZR(dd));
                  ULong mask = 0xFFFFULL << (16 * hw);
                  IRExpr* res
                     = binop(Iop_Or64, 
                             binop(Iop_And64, mkexpr(old), mkU64(~mask)),
                             mkU64(imm64));
                  putIReg64orZR(dd, res);
                  DIP("movk %s, 0x%x, lsl %u\n",
                      nameIReg64orZR(dd), imm16, 16*hw);
               } else {
                  IRTemp old = newTemp(Ity_I32);
                  assign(old, getIReg32orZR(dd));
                  vassert(hw <= 1);
                  UInt mask = 0xFFFF << (16 * hw);
                  IRExpr* res
                     = binop(Iop_Or32, 
                             binop(Iop_And32, mkexpr(old), mkU32(~mask)),
                             mkU32((UInt)imm64));
                  putIReg32orZR(dd, res);
                  DIP("movk %s, 0x%x, lsl %u\n",
                      nameIReg32orZR(dd), imm16, 16*hw);
               }
               break;
            default:
               vassert(0);
         }
         return True;
      }
   }

   
   if (INSN(28,23) == BITS6(1,0,0,1,1,0)) {
      UInt sf     = INSN(31,31);
      UInt opc    = INSN(30,29);
      UInt N      = INSN(22,22);
      UInt immR   = INSN(21,16);
      UInt immS   = INSN(15,10);
      UInt nn     = INSN(9,5);
      UInt dd     = INSN(4,0);
      Bool inZero = False;
      Bool extend = False;
      const HChar* nm = "???";
      
      switch (opc) {
         case BITS2(0,0):
            inZero = True; extend = True; nm = "sbfm"; break;
         case BITS2(0,1):
            inZero = False; extend = False; nm = "bfm"; break;
         case BITS2(1,0):
            inZero = True; extend = False; nm = "ubfm"; break;
         case BITS2(1,1):
            goto after_bfm; 
         default:
            vassert(0);
      }
      if (sf == 1 && N != 1) goto after_bfm;
      if (sf == 0 && (N != 0 || ((immR >> 5) & 1) != 0
                             || ((immS >> 5) & 1) != 0)) goto after_bfm;
      ULong wmask = 0, tmask = 0;
      Bool ok = dbm_DecodeBitMasks(&wmask, &tmask,
                                   N, immS, immR, False, sf == 1 ? 64 : 32);
      if (!ok) goto after_bfm; 

      Bool   is64 = sf == 1;
      IRType ty   = is64 ? Ity_I64 : Ity_I32;

      IRTemp dst = newTemp(ty);
      IRTemp src = newTemp(ty);
      IRTemp bot = newTemp(ty);
      IRTemp top = newTemp(ty);
      IRTemp res = newTemp(ty);
      assign(dst, inZero ? mkU(ty,0) : getIRegOrZR(is64, dd));
      assign(src, getIRegOrZR(is64, nn));
      
      assign(bot, binop(mkOR(ty),
                        binop(mkAND(ty), mkexpr(dst), mkU(ty, ~wmask)),
                        binop(mkAND(ty), mkexpr(mathROR(ty, src, immR)),
                                         mkU(ty, wmask))));
      
      assign(top, mkexpr(extend ? mathREPLICATE(ty, src, immS) : dst));
      
      assign(res, binop(mkOR(ty),
                        binop(mkAND(ty), mkexpr(top), mkU(ty, ~tmask)),
                        binop(mkAND(ty), mkexpr(bot), mkU(ty, tmask))));
      putIRegOrZR(is64, dd, mkexpr(res));
      DIP("%s %s, %s, immR=%u, immS=%u\n",
          nm, nameIRegOrZR(is64, dd), nameIRegOrZR(is64, nn), immR, immS);
      return True;
   }
   after_bfm:

   
   if (INSN(30,23) == BITS8(0,0,1,0,0,1,1,1) && INSN(21,21) == 0) {
      Bool is64  = INSN(31,31) == 1;
      UInt mm    = INSN(20,16);
      UInt imm6  = INSN(15,10);
      UInt nn    = INSN(9,5);
      UInt dd    = INSN(4,0);
      Bool valid = True;
      if (INSN(31,31) != INSN(22,22))
        valid = False;
      if (!is64 && imm6 >= 32)
        valid = False;
      if (!valid) goto after_extr;
      IRType ty    = is64 ? Ity_I64 : Ity_I32;
      IRTemp srcHi = newTemp(ty);
      IRTemp srcLo = newTemp(ty);
      IRTemp res   = newTemp(ty);
      assign(srcHi, getIRegOrZR(is64, nn));
      assign(srcLo, getIRegOrZR(is64, mm));
      if (imm6 == 0) {
        assign(res, mkexpr(srcLo));
      } else {
        UInt szBits = 8 * sizeofIRType(ty);
        vassert(imm6 > 0 && imm6 < szBits);
        assign(res, binop(mkOR(ty),
                          binop(mkSHL(ty), mkexpr(srcHi), mkU8(szBits-imm6)),
                          binop(mkSHR(ty), mkexpr(srcLo), mkU8(imm6))));
      }
      putIRegOrZR(is64, dd, mkexpr(res));
      DIP("extr %s, %s, %s, #%u\n",
          nameIRegOrZR(is64,dd),
          nameIRegOrZR(is64,nn), nameIRegOrZR(is64,mm), imm6);
      return True;
   }
  after_extr:

   vex_printf("ARM64 front end: data_processing_immediate\n");
   return False;
#  undef INSN
}



static const HChar* nameSH ( UInt sh ) {
   switch (sh) {
      case 0: return "lsl";
      case 1: return "lsr";
      case 2: return "asr";
      case 3: return "ror";
      default: vassert(0);
   }
}

static IRTemp getShiftedIRegOrZR ( Bool is64,
                                   UInt sh_how, UInt sh_amt, UInt regNo,
                                   Bool invert )
{
   vassert(sh_how < 4);
   vassert(sh_amt < (is64 ? 64 : 32));
   IRType ty = is64 ? Ity_I64 : Ity_I32;
   IRTemp t0 = newTemp(ty);
   assign(t0, getIRegOrZR(is64, regNo));
   IRTemp t1 = newTemp(ty);
   switch (sh_how) {
      case BITS2(0,0):
         assign(t1, binop(mkSHL(ty), mkexpr(t0), mkU8(sh_amt)));
         break;
      case BITS2(0,1):
         assign(t1, binop(mkSHR(ty), mkexpr(t0), mkU8(sh_amt)));
         break;
      case BITS2(1,0):
         assign(t1, binop(mkSAR(ty), mkexpr(t0), mkU8(sh_amt)));
         break;
      case BITS2(1,1):
         assign(t1, mkexpr(mathROR(ty, t0, sh_amt)));
         break;
      default:
         vassert(0);
   }
   if (invert) {
      IRTemp t2 = newTemp(ty);
      assign(t2, unop(mkNOT(ty), mkexpr(t1)));
      return t2;
   } else {
      return t1;
   }
}


static
Bool dis_ARM64_data_processing_register(DisResult* dres,
                                        UInt insn)
{
#  define INSN(_bMax,_bMin)  SLICE_UInt(insn, (_bMax), (_bMin))

   
   if (INSN(28,24) == BITS5(0,1,0,1,1) && INSN(21,21) == 0) {
      UInt   bX    = INSN(31,31);
      UInt   bOP   = INSN(30,30); 
      UInt   bS    = INSN(29, 29); 
      UInt   sh    = INSN(23,22);
      UInt   rM    = INSN(20,16);
      UInt   imm6  = INSN(15,10);
      UInt   rN    = INSN(9,5);
      UInt   rD    = INSN(4,0);
      Bool   isSUB = bOP == 1;
      Bool   is64  = bX == 1;
      IRType ty    = is64 ? Ity_I64 : Ity_I32;
      if ((!is64 && imm6 > 31) || sh == BITS2(1,1)) {
         
      } else {
         IRTemp argL = newTemp(ty);
         assign(argL, getIRegOrZR(is64, rN));
         IRTemp argR = getShiftedIRegOrZR(is64, sh, imm6, rM, False);
         IROp   op   = isSUB ? mkSUB(ty) : mkADD(ty);
         IRTemp res  = newTemp(ty);
         assign(res, binop(op, mkexpr(argL), mkexpr(argR)));
         if (rD != 31) putIRegOrZR(is64, rD, mkexpr(res));
         if (bS) {
            setFlags_ADD_SUB(is64, isSUB, argL, argR);
         }
         DIP("%s%s %s, %s, %s, %s #%u\n",
             bOP ? "sub" : "add", bS ? "s" : "",
             nameIRegOrZR(is64, rD), nameIRegOrZR(is64, rN),
             nameIRegOrZR(is64, rM), nameSH(sh), imm6);
         return True;
      }
   }

   

   if (INSN(28,21) == BITS8(1,1,0,1,0,0,0,0) && INSN(15,10) == 0 ) {
      UInt   bX    = INSN(31,31);
      UInt   bOP   = INSN(30,30); 
      UInt   bS    = INSN(29,29); 
      UInt   rM    = INSN(20,16);
      UInt   rN    = INSN(9,5);
      UInt   rD    = INSN(4,0);

      Bool   isSUB = bOP == 1;
      Bool   is64  = bX == 1;
      IRType ty    = is64 ? Ity_I64 : Ity_I32;

      IRTemp oldC = newTemp(ty);
      assign(oldC,
             is64 ? mk_arm64g_calculate_flag_c()
                  : unop(Iop_64to32, mk_arm64g_calculate_flag_c()) );

      IRTemp argL = newTemp(ty);
      assign(argL, getIRegOrZR(is64, rN));
      IRTemp argR = newTemp(ty);
      assign(argR, getIRegOrZR(is64, rM));

      IROp   op   = isSUB ? mkSUB(ty) : mkADD(ty);
      IRTemp res  = newTemp(ty);
      if (isSUB) {
         IRExpr* one = is64 ? mkU64(1) : mkU32(1);
         IROp xorOp = is64 ? Iop_Xor64 : Iop_Xor32;
         assign(res,
                binop(op,
                      binop(op, mkexpr(argL), mkexpr(argR)),
                      binop(xorOp, mkexpr(oldC), one)));
      } else {
         assign(res,
                binop(op,
                      binop(op, mkexpr(argL), mkexpr(argR)),
                      mkexpr(oldC)));
      }

      if (rD != 31) putIRegOrZR(is64, rD, mkexpr(res));

      if (bS) {
         setFlags_ADC_SBC(is64, isSUB, argL, argR, oldC);
      }

      DIP("%s%s %s, %s, %s\n",
          bOP ? "sbc" : "adc", bS ? "s" : "",
          nameIRegOrZR(is64, rD), nameIRegOrZR(is64, rN),
          nameIRegOrZR(is64, rM));
      return True;
   }

      
   if (INSN(28,24) == BITS5(0,1,0,1,0)) {
      UInt   bX   = INSN(31,31);
      UInt   sh   = INSN(23,22);
      UInt   bN   = INSN(21,21);
      UInt   rM   = INSN(20,16);
      UInt   imm6 = INSN(15,10);
      UInt   rN   = INSN(9,5);
      UInt   rD   = INSN(4,0);
      Bool   is64 = bX == 1;
      IRType ty   = is64 ? Ity_I64 : Ity_I32;
      if (!is64 && imm6 > 31) {
         
      } else {
         IRTemp argL = newTemp(ty);
         assign(argL, getIRegOrZR(is64, rN));
         IRTemp argR = getShiftedIRegOrZR(is64, sh, imm6, rM, bN == 1);
         IROp   op   = Iop_INVALID;
         switch (INSN(30,29)) {
            case BITS2(0,0): case BITS2(1,1): op = mkAND(ty); break;
            case BITS2(0,1):                  op = mkOR(ty);  break;
            case BITS2(1,0):                  op = mkXOR(ty); break;
            default: vassert(0);
         }
         IRTemp res = newTemp(ty);
         assign(res, binop(op, mkexpr(argL), mkexpr(argR)));
         if (INSN(30,29) == BITS2(1,1)) {
            setFlags_LOGIC(is64, res);
         }
         putIRegOrZR(is64, rD, mkexpr(res));

         static const HChar* names_op[8]
            = { "and", "orr", "eor", "ands", "bic", "orn", "eon", "bics" };
         vassert(((bN << 2) | INSN(30,29)) < 8);
         const HChar* nm_op = names_op[(bN << 2) | INSN(30,29)];
         
         if (rN == 31 && sh == 0 && imm6 == 0 && bN == 0) {
            DIP("mov %s, %s\n", nameIRegOrZR(is64, rD),
                                nameIRegOrZR(is64, rM));
         } else {
            DIP("%s %s, %s, %s, %s #%u\n", nm_op,
                nameIRegOrZR(is64, rD), nameIRegOrZR(is64, rN),
                nameIRegOrZR(is64, rM), nameSH(sh), imm6);
         }
         return True;
      }
   }

      
   if (INSN(31,24) == BITS8(1,0,0,1,1,0,1,1)
       && INSN(22,21) == BITS2(1,0) && INSN(15,10) == BITS6(0,1,1,1,1,1)) {
      Bool isU = INSN(23,23) == 1;
      UInt mm  = INSN(20,16);
      UInt nn  = INSN(9,5);
      UInt dd  = INSN(4,0);
      putIReg64orZR(dd, unop(Iop_128HIto64,
                             binop(isU ? Iop_MullU64 : Iop_MullS64,
                                   getIReg64orZR(nn), getIReg64orZR(mm))));
      DIP("%cmulh %s, %s, %s\n", 
          isU ? 'u' : 's',
          nameIReg64orZR(dd), nameIReg64orZR(nn), nameIReg64orZR(mm));
      return True;
   }

      
   if (INSN(30,21) == BITS10(0,0,1,1,0,1,1,0,0,0)) {
      Bool is64  = INSN(31,31) == 1;
      UInt mm    = INSN(20,16);
      Bool isAdd = INSN(15,15) == 0;
      UInt aa    = INSN(14,10);
      UInt nn    = INSN(9,5);
      UInt dd    = INSN(4,0);
      if (is64) {
         putIReg64orZR(
            dd,
            binop(isAdd ? Iop_Add64 : Iop_Sub64,
                  getIReg64orZR(aa),
                  binop(Iop_Mul64, getIReg64orZR(mm), getIReg64orZR(nn))));
      } else {
         putIReg32orZR(
            dd,
            binop(isAdd ? Iop_Add32 : Iop_Sub32,
                  getIReg32orZR(aa),
                  binop(Iop_Mul32, getIReg32orZR(mm), getIReg32orZR(nn))));
      }
      DIP("%s %s, %s, %s, %s\n",
          isAdd ? "madd" : "msub",
          nameIRegOrZR(is64, dd), nameIRegOrZR(is64, nn),
          nameIRegOrZR(is64, mm), nameIRegOrZR(is64, aa));
      return True;
   }

      
   if (INSN(29,21) == BITS9(0, 1,1,0,1, 0,1,0,0) && INSN(11,11) == 0) {
      Bool    is64 = INSN(31,31) == 1;
      UInt    b30  = INSN(30,30);
      UInt    mm   = INSN(20,16);
      UInt    cond = INSN(15,12);
      UInt    b10  = INSN(10,10);
      UInt    nn   = INSN(9,5);
      UInt    dd   = INSN(4,0);
      UInt    op   = (b30 << 1) | b10; 
      IRType  ty   = is64 ? Ity_I64 : Ity_I32;
      IRExpr* argL = getIRegOrZR(is64, nn);
      IRExpr* argR = getIRegOrZR(is64, mm);
      switch (op) {
         case BITS2(0,0):
            break;
         case BITS2(0,1):
            argR = binop(mkADD(ty), argR, mkU(ty,1));
            break;
         case BITS2(1,0):
            argR = unop(mkNOT(ty), argR);
            break;
         case BITS2(1,1):
            argR = binop(mkSUB(ty), mkU(ty,0), argR);
            break;
         default:
            vassert(0);
      }
      putIRegOrZR(
         is64, dd,
         IRExpr_ITE(unop(Iop_64to1, mk_arm64g_calculate_condition(cond)),
                    argL, argR)
      );
      const HChar* op_nm[4] = { "csel", "csinc", "csinv", "csneg" };
      DIP("%s %s, %s, %s, %s\n", op_nm[op],
          nameIRegOrZR(is64, dd), nameIRegOrZR(is64, nn),
          nameIRegOrZR(is64, mm), nameCC(cond));
      return True;
   }

      
   if (INSN(28,21) == BITS8(0,1,0,1,1,0,0,1) && INSN(12,10) <= 4) {
      Bool is64  = INSN(31,31) == 1;
      Bool isSub = INSN(30,30) == 1;
      Bool setCC = INSN(29,29) == 1;
      UInt mm    = INSN(20,16);
      UInt opt   = INSN(15,13);
      UInt imm3  = INSN(12,10);
      UInt nn    = INSN(9,5);
      UInt dd    = INSN(4,0);
      const HChar* nameExt[8] = { "uxtb", "uxth", "uxtw", "uxtx",
                                  "sxtb", "sxth", "sxtw", "sxtx" };
      
      IRTemp xN = newTemp(Ity_I64);
      IRTemp xM = newTemp(Ity_I64);
      assign(xN, getIReg64orSP(nn));
      assign(xM, getIReg64orZR(mm));
      IRExpr* xMw  = mkexpr(xM); 
      Int     shSX = 0;
      
      switch (opt) {
         case BITS3(0,0,0): 
            xMw = binop(Iop_And64, xMw, mkU64(0xFF)); break;
         case BITS3(0,0,1): 
            xMw = binop(Iop_And64, xMw, mkU64(0xFFFF)); break;
         case BITS3(0,1,0): 
            if (is64) {
               xMw = unop(Iop_32Uto64, unop(Iop_64to32, xMw));
            }
            break;
         case BITS3(0,1,1): 
            break;
         case BITS3(1,0,0): 
            shSX = 56; goto sxTo64;
         case BITS3(1,0,1): 
            shSX = 48; goto sxTo64;
         case BITS3(1,1,0): 
            if (is64) {
               shSX = 32; goto sxTo64;
            }
            break;
         case BITS3(1,1,1): 
            break;
         sxTo64:
            vassert(shSX >= 32);
            xMw = binop(Iop_Sar64, binop(Iop_Shl64, xMw, mkU8(shSX)),
                        mkU8(shSX));
            break;
         default:
            vassert(0);
      }
      
      IRTemp argL = xN;
      IRTemp argR = newTemp(Ity_I64);
      assign(argR, binop(Iop_Shl64, xMw, mkU8(imm3)));
      IRTemp res = newTemp(Ity_I64);
      assign(res, binop(isSub ? Iop_Sub64 : Iop_Add64,
                        mkexpr(argL), mkexpr(argR)));
      if (is64) {
         if (setCC) {
            putIReg64orZR(dd, mkexpr(res));
            setFlags_ADD_SUB(True, isSub, argL, argR);
         } else {
            putIReg64orSP(dd, mkexpr(res));
         }
      } else {
         if (setCC) {
            IRTemp argL32 = newTemp(Ity_I32);
            IRTemp argR32 = newTemp(Ity_I32);
            putIReg32orZR(dd, unop(Iop_64to32, mkexpr(res)));
            assign(argL32, unop(Iop_64to32, mkexpr(argL)));
            assign(argR32, unop(Iop_64to32, mkexpr(argR)));
            setFlags_ADD_SUB(False, isSub, argL32, argR32);
         } else {
            putIReg32orSP(dd, unop(Iop_64to32, mkexpr(res)));
         }
      }
      DIP("%s%s %s, %s, %s %s lsl %u\n",
          isSub ? "sub" : "add", setCC ? "s" : "",
          setCC ? nameIRegOrZR(is64, dd) : nameIRegOrSP(is64, dd),
          nameIRegOrSP(is64, nn), nameIRegOrSP(is64, mm),
          nameExt[opt], imm3);
      return True;
   }

   
   if (INSN(29,21) == BITS9(1,1,1,0,1,0,0,1,0)
       && INSN(11,10) == BITS2(1,0) && INSN(4,4) == 0) {
      Bool is64  = INSN(31,31) == 1;
      Bool isSUB = INSN(30,30) == 1;
      UInt imm5  = INSN(20,16);
      UInt cond  = INSN(15,12);
      UInt nn    = INSN(9,5);
      UInt nzcv  = INSN(3,0);

      IRTemp condT = newTemp(Ity_I1);
      assign(condT, unop(Iop_64to1, mk_arm64g_calculate_condition(cond)));

      IRType ty   = is64 ? Ity_I64 : Ity_I32;
      IRTemp argL = newTemp(ty);
      IRTemp argR = newTemp(ty);

      if (is64) {
         assign(argL, getIReg64orZR(nn));
         assign(argR, mkU64(imm5));
      } else {
         assign(argL, getIReg32orZR(nn));
         assign(argR, mkU32(imm5));
      }
      setFlags_ADD_SUB_conditionally(is64, isSUB, condT, argL, argR, nzcv);

      DIP("ccm%c %s, #%u, #%u, %s\n",
          isSUB ? 'p' : 'n', nameIRegOrZR(is64, nn),
          imm5, nzcv, nameCC(cond));
      return True;
   }

   
   if (INSN(29,21) == BITS9(1,1,1,0,1,0,0,1,0)
       && INSN(11,10) == BITS2(0,0) && INSN(4,4) == 0) {
      Bool is64  = INSN(31,31) == 1;
      Bool isSUB = INSN(30,30) == 1;
      UInt mm    = INSN(20,16);
      UInt cond  = INSN(15,12);
      UInt nn    = INSN(9,5);
      UInt nzcv  = INSN(3,0);

      IRTemp condT = newTemp(Ity_I1);
      assign(condT, unop(Iop_64to1, mk_arm64g_calculate_condition(cond)));

      IRType ty   = is64 ? Ity_I64 : Ity_I32;
      IRTemp argL = newTemp(ty);
      IRTemp argR = newTemp(ty);

      if (is64) {
         assign(argL, getIReg64orZR(nn));
         assign(argR, getIReg64orZR(mm));
      } else {
         assign(argL, getIReg32orZR(nn));
         assign(argR, getIReg32orZR(mm));
      }
      setFlags_ADD_SUB_conditionally(is64, isSUB, condT, argL, argR, nzcv);

      DIP("ccm%c %s, %s, #%u, %s\n",
          isSUB ? 'p' : 'n', nameIRegOrZR(is64, nn),
          nameIRegOrZR(is64, mm), nzcv, nameCC(cond));
      return True;
   }


      
   if (INSN(30,21) == BITS10(1,0,1,1,0,1,0,1,1,0)
       && INSN(20,12) == BITS9(0,0,0,0,0,0,0,0,0)) {
      UInt b31 = INSN(31,31);
      UInt opc = INSN(11,10);

      UInt ix = 0;
       if (b31 == 1 && opc == BITS2(1,1)) ix = 1; 
      else if (b31 == 0 && opc == BITS2(1,0)) ix = 2; 
      else if (b31 == 1 && opc == BITS2(0,0)) ix = 3; 
      else if (b31 == 0 && opc == BITS2(0,0)) ix = 4; 
      else if (b31 == 1 && opc == BITS2(0,1)) ix = 5; 
      else if (b31 == 0 && opc == BITS2(0,1)) ix = 6; 
      else if (b31 == 1 && opc == BITS2(1,0)) ix = 7; 
      if (ix >= 1 && ix <= 7) {
         Bool   is64  = ix == 1 || ix == 3 || ix == 5 || ix == 7;
         UInt   nn    = INSN(9,5);
         UInt   dd    = INSN(4,0);
         IRTemp src   = newTemp(Ity_I64);
         IRTemp dst   = IRTemp_INVALID;
         IRTemp (*math)(IRTemp) = NULL;
         switch (ix) {
            case 1: case 2: math = math_BYTESWAP64;   break;
            case 3: case 4: math = math_BITSWAP64;    break;
            case 5: case 6: math = math_USHORTSWAP64; break;
            case 7:         math = math_UINTSWAP64;   break;
            default: vassert(0);
         }
         const HChar* names[7]
           = { "rev", "rev", "rbit", "rbit", "rev16", "rev16", "rev32" };
         const HChar* nm = names[ix-1];
         vassert(math);
         if (ix == 6) {
            assign(src, getIReg64orZR(nn));
            dst = math(src);
            putIReg64orZR(dd,
                          unop(Iop_32Uto64, unop(Iop_64to32, mkexpr(dst))));
         } else if (is64) {
            assign(src, getIReg64orZR(nn));
            dst = math(src);
            putIReg64orZR(dd, mkexpr(dst));
         } else {
            assign(src, binop(Iop_Shl64, getIReg64orZR(nn), mkU8(32)));
            dst = math(src);
            putIReg32orZR(dd, unop(Iop_64to32, mkexpr(dst)));
         }
         DIP("%s %s, %s\n", nm,
             nameIRegOrZR(is64,dd), nameIRegOrZR(is64,nn));
         return True;
      }
      
   }

      
   if (INSN(30,21) == BITS10(1,0,1,1,0,1,0,1,1,0)
       && INSN(20,11) == BITS10(0,0,0,0,0,0,0,0,1,0)) {
      Bool   is64  = INSN(31,31) == 1;
      Bool   isCLS = INSN(10,10) == 1;
      UInt   nn    = INSN(9,5);
      UInt   dd    = INSN(4,0);
      IRTemp src   = newTemp(Ity_I64);
      IRTemp srcZ  = newTemp(Ity_I64);
      IRTemp dst   = newTemp(Ity_I64);
      
      if (is64) {
         assign(src, getIReg64orZR(nn));
      } else {
         assign(src, binop(Iop_Shl64,
                           unop(Iop_32Uto64, getIReg32orZR(nn)), mkU8(32)));
      }
      
      if (isCLS) {
         IRExpr* one = mkU8(1);
         assign(srcZ,
         binop(Iop_Xor64,
               binop(Iop_Shl64, mkexpr(src), one),
               binop(Iop_Shl64, binop(Iop_Shr64, mkexpr(src), one), one)));
      } else {
         assign(srcZ, mkexpr(src));
      }
      
      if (is64) {
         assign(dst, IRExpr_ITE(binop(Iop_CmpEQ64, mkexpr(srcZ), mkU64(0)),
                                mkU64(isCLS ? 63 : 64),
                                unop(Iop_Clz64, mkexpr(srcZ))));
         putIReg64orZR(dd, mkexpr(dst));
      } else {
         assign(dst, IRExpr_ITE(binop(Iop_CmpEQ64, mkexpr(srcZ), mkU64(0)),
                                mkU64(isCLS ? 31 : 32),
                                unop(Iop_Clz64, mkexpr(srcZ))));
         putIReg32orZR(dd, unop(Iop_64to32, mkexpr(dst)));
      }
      DIP("cl%c %s, %s\n", isCLS ? 's' : 'z',
          nameIRegOrZR(is64, dd), nameIRegOrZR(is64, nn));
      return True;
   }

      
   if (INSN(30,21) == BITS10(0,0,1,1,0,1,0,1,1,0)
       && INSN(15,12) == BITS4(0,0,1,0)) {
      Bool   is64 = INSN(31,31) == 1;
      UInt   mm   = INSN(20,16);
      UInt   op   = INSN(11,10);
      UInt   nn   = INSN(9,5);
      UInt   dd   = INSN(4,0);
      IRType ty   = is64 ? Ity_I64 : Ity_I32;
      IRTemp srcL = newTemp(ty);
      IRTemp srcR = newTemp(Ity_I64);
      IRTemp res  = newTemp(ty);
      IROp   iop  = Iop_INVALID;
      assign(srcL, getIRegOrZR(is64, nn));
      assign(srcR, binop(Iop_And64, getIReg64orZR(mm),
                                    mkU64(is64 ? 63 : 31)));
      if (op < 3) {
         
         switch (op) {
            case BITS2(0,0): iop = mkSHL(ty); break;
            case BITS2(0,1): iop = mkSHR(ty); break;
            case BITS2(1,0): iop = mkSAR(ty); break;
            default: vassert(0);
         }
         assign(res, binop(iop, mkexpr(srcL),
                                unop(Iop_64to8, mkexpr(srcR))));
      } else {
         
         IROp opSHL = mkSHL(ty);
         IROp opSHR = mkSHR(ty);
         IROp opOR  = mkOR(ty);
         IRExpr* width = mkU64(is64 ? 64: 32);
         assign(
            res,
            IRExpr_ITE(
               binop(Iop_CmpEQ64, mkexpr(srcR), mkU64(0)),
               mkexpr(srcL),
               binop(opOR,
                     binop(opSHL,
                           mkexpr(srcL),
                           unop(Iop_64to8, binop(Iop_Sub64, width,
                                                            mkexpr(srcR)))),
                     binop(opSHR,
                           mkexpr(srcL), unop(Iop_64to8, mkexpr(srcR))))
         ));
      }
      putIRegOrZR(is64, dd, mkexpr(res));
      vassert(op < 4);
      const HChar* names[4] = { "lslv", "lsrv", "asrv", "rorv" };
      DIP("%s %s, %s, %s\n",
          names[op], nameIRegOrZR(is64,dd),
                     nameIRegOrZR(is64,nn), nameIRegOrZR(is64,mm));
      return True;
   }

      
   if (INSN(30,21) == BITS10(0,0,1,1,0,1,0,1,1,0)
       && INSN(15,11) == BITS5(0,0,0,0,1)) {
      Bool is64 = INSN(31,31) == 1;
      UInt mm   = INSN(20,16);
      Bool isS  = INSN(10,10) == 1;
      UInt nn   = INSN(9,5);
      UInt dd   = INSN(4,0);
      if (isS) {
         putIRegOrZR(is64, dd, binop(is64 ? Iop_DivS64 : Iop_DivS32,
                                     getIRegOrZR(is64, nn),
                                     getIRegOrZR(is64, mm)));
      } else {
         putIRegOrZR(is64, dd, binop(is64 ? Iop_DivU64 : Iop_DivU32,
                                     getIRegOrZR(is64, nn),
                                     getIRegOrZR(is64, mm)));
      }
      DIP("%cdiv %s, %s, %s\n", isS ? 's' : 'u',
          nameIRegOrZR(is64, dd),
          nameIRegOrZR(is64, nn), nameIRegOrZR(is64, mm));
      return True;
   }

      
   if (INSN(31,24) == BITS8(1,0,0,1,1,0,1,1) && INSN(22,21) == BITS2(0,1)) {
      Bool   isU   = INSN(23,23) == 1;
      UInt   mm    = INSN(20,16);
      Bool   isAdd = INSN(15,15) == 0;
      UInt   aa    = INSN(14,10);
      UInt   nn    = INSN(9,5);
      UInt   dd    = INSN(4,0);
      IRTemp wN    = newTemp(Ity_I32);
      IRTemp wM    = newTemp(Ity_I32);
      IRTemp xA    = newTemp(Ity_I64);
      IRTemp muld  = newTemp(Ity_I64);
      IRTemp res   = newTemp(Ity_I64);
      assign(wN, getIReg32orZR(nn));
      assign(wM, getIReg32orZR(mm));
      assign(xA, getIReg64orZR(aa));
      assign(muld, binop(isU ? Iop_MullU32 : Iop_MullS32,
                         mkexpr(wN), mkexpr(wM)));
      assign(res, binop(isAdd ? Iop_Add64 : Iop_Sub64,
                        mkexpr(xA), mkexpr(muld)));
      putIReg64orZR(dd, mkexpr(res));
      DIP("%cm%sl %s, %s, %s, %s\n", isU ? 'u' : 's', isAdd ? "add" : "sub",
          nameIReg64orZR(dd), nameIReg32orZR(nn),
          nameIReg32orZR(mm), nameIReg64orZR(aa));
      return True;
   }
   vex_printf("ARM64 front end: data_processing_register\n");
   return False;
#  undef INSN
}



#define EX(_tmp) \
           mkexpr(_tmp)
#define SL(_hi128,_lo128,_nbytes) \
           ( (_nbytes) == 0 \
                ? (_lo128) \
                : triop(Iop_SliceV128,(_hi128),(_lo128),mkU8(_nbytes)) )
#define ROR(_v128,_nbytes) \
           SL((_v128),(_v128),(_nbytes))
#define ROL(_v128,_nbytes) \
           SL((_v128),(_v128),16-(_nbytes))
#define SHR(_v128,_nbytes) \
           binop(Iop_ShrV128,(_v128),mkU8(8*(_nbytes)))
#define SHL(_v128,_nbytes) \
           binop(Iop_ShlV128,(_v128),mkU8(8*(_nbytes)))
#define ILO64x2(_argL,_argR) \
           binop(Iop_InterleaveLO64x2,(_argL),(_argR))
#define IHI64x2(_argL,_argR) \
           binop(Iop_InterleaveHI64x2,(_argL),(_argR))
#define ILO32x4(_argL,_argR) \
           binop(Iop_InterleaveLO32x4,(_argL),(_argR))
#define IHI32x4(_argL,_argR) \
           binop(Iop_InterleaveHI32x4,(_argL),(_argR))
#define ILO16x8(_argL,_argR) \
           binop(Iop_InterleaveLO16x8,(_argL),(_argR))
#define IHI16x8(_argL,_argR) \
           binop(Iop_InterleaveHI16x8,(_argL),(_argR))
#define ILO8x16(_argL,_argR) \
           binop(Iop_InterleaveLO8x16,(_argL),(_argR))
#define IHI8x16(_argL,_argR) \
           binop(Iop_InterleaveHI8x16,(_argL),(_argR))
#define CEV32x4(_argL,_argR) \
           binop(Iop_CatEvenLanes32x4,(_argL),(_argR))
#define COD32x4(_argL,_argR) \
           binop(Iop_CatOddLanes32x4,(_argL),(_argR))
#define COD16x8(_argL,_argR) \
           binop(Iop_CatOddLanes16x8,(_argL),(_argR))
#define COD8x16(_argL,_argR) \
           binop(Iop_CatOddLanes8x16,(_argL),(_argR))
#define CEV8x16(_argL,_argR) \
           binop(Iop_CatEvenLanes8x16,(_argL),(_argR))
#define AND(_arg1,_arg2) \
           binop(Iop_AndV128,(_arg1),(_arg2))
#define OR2(_arg1,_arg2) \
           binop(Iop_OrV128,(_arg1),(_arg2))
#define OR3(_arg1,_arg2,_arg3) \
           binop(Iop_OrV128,(_arg1),binop(Iop_OrV128,(_arg2),(_arg3)))
#define OR4(_arg1,_arg2,_arg3,_arg4) \
           binop(Iop_OrV128, \
                 binop(Iop_OrV128,(_arg1),(_arg2)), \
                 binop(Iop_OrV128,(_arg3),(_arg4)))


static
void math_INTERLEAVE1_128(  IRTemp* i0,
                           UInt laneSzBlg2, IRTemp u0 )
{
   assign(*i0, mkexpr(u0));
}


static
void math_INTERLEAVE2_128(  IRTemp* i0, IRTemp* i1,
                           UInt laneSzBlg2, IRTemp u0, IRTemp u1 )
{
   if (laneSzBlg2 == 3) {
      
      
      
      assign(*i0, binop(Iop_InterleaveLO64x2, mkexpr(u1), mkexpr(u0)));
      assign(*i1, binop(Iop_InterleaveHI64x2, mkexpr(u1), mkexpr(u0)));
      return;
   }
   if (laneSzBlg2 == 2) {
      
      
      
      assign(*i0, binop(Iop_InterleaveLO32x4, mkexpr(u1), mkexpr(u0)));
      assign(*i1, binop(Iop_InterleaveHI32x4, mkexpr(u1), mkexpr(u0)));
      return;
   } 
   if (laneSzBlg2 == 1) {
      
      
      
      
      assign(*i0, binop(Iop_InterleaveLO16x8, mkexpr(u1), mkexpr(u0)));
      assign(*i1, binop(Iop_InterleaveHI16x8, mkexpr(u1), mkexpr(u0)));
      return;
   }
   if (laneSzBlg2 == 0) {
      
      
      
      
      assign(*i0, binop(Iop_InterleaveLO8x16, mkexpr(u1), mkexpr(u0)));
      assign(*i1, binop(Iop_InterleaveHI8x16, mkexpr(u1), mkexpr(u0)));
      return;
   }
   
   vassert(0);
}


static
void math_INTERLEAVE3_128( 
         IRTemp* i0, IRTemp* i1, IRTemp* i2,
        UInt laneSzBlg2,
        IRTemp u0, IRTemp u1, IRTemp u2 )
{
   if (laneSzBlg2 == 3) {
      
      
      
      assign(*i2, IHI64x2( EX(u2), EX(u1) ));
      assign(*i1, ILO64x2( ROR(EX(u0),8), EX(u2) ));
      assign(*i0, ILO64x2( EX(u1), EX(u0) ));
      return;
   }

   if (laneSzBlg2 == 2) {
      
      
      
      
      IRTemp p0    = newTempV128();
      IRTemp p1    = newTempV128();
      IRTemp p2    = newTempV128();
      IRTemp c1100 = newTempV128();
      IRTemp c0011 = newTempV128();
      IRTemp c0110 = newTempV128();
      assign(c1100, mkV128(0xFF00));
      assign(c0011, mkV128(0x00FF));
      assign(c0110, mkV128(0x0FF0));
      
      
      math_INTERLEAVE3_128(&p0, &p1, &p2, 3, u0, u1, u2);
      
      assign(*i2, OR2( AND( IHI32x4(EX(p2), ROL(EX(p2),8)), EX(c1100) ),
                       AND( IHI32x4(ROR(EX(p1),4), EX(p2)), EX(c0011) ) ));
      assign(*i1, OR3( SHL(EX(p2),12),
                       AND(EX(p1),EX(c0110)),
                       SHR(EX(p0),12) ));
      assign(*i0, OR2( AND( ILO32x4(EX(p0),ROL(EX(p1),4)), EX(c1100) ),
                       AND( ILO32x4(ROR(EX(p0),8),EX(p0)), EX(c0011) ) ));
      return;
   }

   if (laneSzBlg2 == 1) {
      
      
      
      
      
      
      
      
      
      
      
      
      IRTemp p0    = newTempV128();
      IRTemp p1    = newTempV128();
      IRTemp p2    = newTempV128();
      IRTemp c1000 = newTempV128();
      IRTemp c0100 = newTempV128();
      IRTemp c0010 = newTempV128();
      IRTemp c0001 = newTempV128();
      assign(c1000, mkV128(0xF000));
      assign(c0100, mkV128(0x0F00));
      assign(c0010, mkV128(0x00F0));
      assign(c0001, mkV128(0x000F));
      
      
      math_INTERLEAVE3_128(&p0, &p1, &p2, 2, u0, u1, u2);
      
      assign(*i2,
             OR4( AND( IHI16x8( EX(p2),        ROL(EX(p2),4) ), EX(c1000) ),
                  AND( IHI16x8( ROL(EX(p2),6), EX(p2)        ), EX(c0100) ),
                  AND( IHI16x8( ROL(EX(p2),2), ROL(EX(p2),6) ), EX(c0010) ),
                  AND( ILO16x8( ROR(EX(p2),2), ROL(EX(p1),2) ), EX(c0001) )
      ));
      assign(*i1,
             OR4( AND( IHI16x8( ROL(EX(p1),4), ROR(EX(p2),2) ), EX(c1000) ),
                  AND( IHI16x8( EX(p1),        ROL(EX(p1),4) ), EX(c0100) ),
                  AND( IHI16x8( ROL(EX(p1),4), ROL(EX(p1),8) ), EX(c0010) ),
                  AND( IHI16x8( ROR(EX(p0),6), ROL(EX(p1),4) ), EX(c0001) )
      ));
      assign(*i0,
             OR4( AND( IHI16x8( ROR(EX(p1),2), ROL(EX(p0),2) ), EX(c1000) ),
                  AND( IHI16x8( ROL(EX(p0),2), ROL(EX(p0),6) ), EX(c0100) ),
                  AND( IHI16x8( ROL(EX(p0),8), ROL(EX(p0),2) ), EX(c0010) ),
                  AND( IHI16x8( ROL(EX(p0),4), ROL(EX(p0),8) ), EX(c0001) )
      ));
      return;
   }

   if (laneSzBlg2 == 0) {
      
      
      
      
      
      
      

      IRTemp i2_FEDC = newTempV128(); IRTemp i2_BA98 = newTempV128();
      IRTemp i2_7654 = newTempV128(); IRTemp i2_3210 = newTempV128();
      IRTemp i1_FEDC = newTempV128(); IRTemp i1_BA98 = newTempV128();
      IRTemp i1_7654 = newTempV128(); IRTemp i1_3210 = newTempV128();
      IRTemp i0_FEDC = newTempV128(); IRTemp i0_BA98 = newTempV128();
      IRTemp i0_7654 = newTempV128(); IRTemp i0_3210 = newTempV128();
      IRTemp i2_hi64 = newTempV128(); IRTemp i2_lo64 = newTempV128();
      IRTemp i1_hi64 = newTempV128(); IRTemp i1_lo64 = newTempV128();
      IRTemp i0_hi64 = newTempV128(); IRTemp i0_lo64 = newTempV128();

      
      
      
#     define XXXX(_tempName,_srcVec1,_srcShift1,_srcVec2,_srcShift2) \
         IRTemp t_##_tempName = newTempV128(); \
         assign(t_##_tempName, \
                ILO8x16( ROR(EX(_srcVec1),(_srcShift1)), \
                         ROR(EX(_srcVec2),(_srcShift2)) ) )

      
      IRTemp CC = u2; IRTemp BB = u1; IRTemp AA = u0;

      
      
      

      XXXX(CfBf, CC, 0xf, BB, 0xf); 
      XXXX(AfCe, AA, 0xf, CC, 0xe);
      assign(i2_FEDC, ILO16x8(EX(t_CfBf), EX(t_AfCe)));

      XXXX(BeAe, BB, 0xe, AA, 0xe);
      XXXX(CdBd, CC, 0xd, BB, 0xd);
      assign(i2_BA98, ILO16x8(EX(t_BeAe), EX(t_CdBd)));
      assign(i2_hi64, ILO32x4(EX(i2_FEDC), EX(i2_BA98)));

      XXXX(AdCc, AA, 0xd, CC, 0xc);
      XXXX(BcAc, BB, 0xc, AA, 0xc);
      assign(i2_7654, ILO16x8(EX(t_AdCc), EX(t_BcAc)));

      XXXX(CbBb, CC, 0xb, BB, 0xb);
      XXXX(AbCa, AA, 0xb, CC, 0xa); 
      assign(i2_3210, ILO16x8(EX(t_CbBb), EX(t_AbCa)));
      assign(i2_lo64, ILO32x4(EX(i2_7654), EX(i2_3210)));
      assign(*i2, ILO64x2(EX(i2_hi64), EX(i2_lo64)));

      XXXX(BaAa, BB, 0xa, AA, 0xa); 
      XXXX(C9B9, CC, 0x9, BB, 0x9);
      assign(i1_FEDC, ILO16x8(EX(t_BaAa), EX(t_C9B9)));

      XXXX(A9C8, AA, 0x9, CC, 0x8);
      XXXX(B8A8, BB, 0x8, AA, 0x8);
      assign(i1_BA98, ILO16x8(EX(t_A9C8), EX(t_B8A8)));
      assign(i1_hi64, ILO32x4(EX(i1_FEDC), EX(i1_BA98)));

      XXXX(C7B7, CC, 0x7, BB, 0x7);
      XXXX(A7C6, AA, 0x7, CC, 0x6);
      assign(i1_7654, ILO16x8(EX(t_C7B7), EX(t_A7C6)));

      XXXX(B6A6, BB, 0x6, AA, 0x6);
      XXXX(C5B5, CC, 0x5, BB, 0x5); 
      assign(i1_3210, ILO16x8(EX(t_B6A6), EX(t_C5B5)));
      assign(i1_lo64, ILO32x4(EX(i1_7654), EX(i1_3210)));
      assign(*i1, ILO64x2(EX(i1_hi64), EX(i1_lo64)));

      XXXX(A5C4, AA, 0x5, CC, 0x4); 
      XXXX(B4A4, BB, 0x4, AA, 0x4);
      assign(i0_FEDC, ILO16x8(EX(t_A5C4), EX(t_B4A4)));

      XXXX(C3B3, CC, 0x3, BB, 0x3);
      XXXX(A3C2, AA, 0x3, CC, 0x2);
      assign(i0_BA98, ILO16x8(EX(t_C3B3), EX(t_A3C2)));
      assign(i0_hi64, ILO32x4(EX(i0_FEDC), EX(i0_BA98)));

      XXXX(B2A2, BB, 0x2, AA, 0x2);
      XXXX(C1B1, CC, 0x1, BB, 0x1);
      assign(i0_7654, ILO16x8(EX(t_B2A2), EX(t_C1B1)));

      XXXX(A1C0, AA, 0x1, CC, 0x0);
      XXXX(B0A0, BB, 0x0, AA, 0x0); 
      assign(i0_3210, ILO16x8(EX(t_A1C0), EX(t_B0A0)));
      assign(i0_lo64, ILO32x4(EX(i0_7654), EX(i0_3210)));
      assign(*i0, ILO64x2(EX(i0_hi64), EX(i0_lo64)));

#     undef XXXX
      return;
   }

   
   vassert(0);
}


static
void math_INTERLEAVE4_128( 
         IRTemp* i0, IRTemp* i1, IRTemp* i2, IRTemp* i3,
        UInt laneSzBlg2,
        IRTemp u0, IRTemp u1, IRTemp u2, IRTemp u3 )
{
   if (laneSzBlg2 == 3) {
      
      assign(*i0, ILO64x2(EX(u1), EX(u0)));
      assign(*i1, ILO64x2(EX(u3), EX(u2)));
      assign(*i2, IHI64x2(EX(u1), EX(u0)));
      assign(*i3, IHI64x2(EX(u3), EX(u2)));
      return;
   }
   if (laneSzBlg2 == 2) {
      
      
      IRTemp p0 = newTempV128();
      IRTemp p1 = newTempV128();
      IRTemp p2 = newTempV128();
      IRTemp p3 = newTempV128();
      math_INTERLEAVE4_128(&p0, &p1, &p2, &p3, 3, u0, u1, u2, u3);
      
      assign(*i0, CEV32x4(EX(p1), EX(p0)));
      assign(*i1, COD32x4(EX(p1), EX(p0)));
      assign(*i2, CEV32x4(EX(p3), EX(p2)));
      assign(*i3, COD32x4(EX(p3), EX(p2)));
      return;
   }
   if (laneSzBlg2 == 1) {
      
      
      IRTemp p0 = newTempV128();
      IRTemp p1 = newTempV128();
      IRTemp p2 = newTempV128();
      IRTemp p3 = newTempV128();
      math_INTERLEAVE4_128(&p0, &p1, &p2, &p3, 2, u0, u1, u2, u3);
      
      assign(*i0, COD16x8(EX(p0), SHL(EX(p0), 2)));
      assign(*i1, COD16x8(EX(p1), SHL(EX(p1), 2)));
      assign(*i2, COD16x8(EX(p2), SHL(EX(p2), 2)));
      assign(*i3, COD16x8(EX(p3), SHL(EX(p3), 2)));
      return;
   }
   if (laneSzBlg2 == 0) {
      
      
      IRTemp p0 = newTempV128();
      IRTemp p1 = newTempV128();
      IRTemp p2 = newTempV128();
      IRTemp p3 = newTempV128();
      math_INTERLEAVE4_128(&p0, &p1, &p2, &p3, 1, u0, u1, u2, u3);
      
      assign(*i0, IHI32x4(COD8x16(EX(p0),EX(p0)), CEV8x16(EX(p0),EX(p0))));
      assign(*i1, IHI32x4(COD8x16(EX(p1),EX(p1)), CEV8x16(EX(p1),EX(p1))));
      assign(*i2, IHI32x4(COD8x16(EX(p2),EX(p2)), CEV8x16(EX(p2),EX(p2))));
      assign(*i3, IHI32x4(COD8x16(EX(p3),EX(p3)), CEV8x16(EX(p3),EX(p3))));
      return;
   }
   
   vassert(0);
}


static
void math_DEINTERLEAVE1_128(  IRTemp* u0,
                             UInt laneSzBlg2, IRTemp i0 )
{
   assign(*u0, mkexpr(i0));
}


static
void math_DEINTERLEAVE2_128(  IRTemp* u0, IRTemp* u1,
                             UInt laneSzBlg2, IRTemp i0, IRTemp i1 )
{
   if (laneSzBlg2 == 3) {
      
      
      
      assign(*u0, binop(Iop_InterleaveLO64x2, mkexpr(i1), mkexpr(i0)));
      assign(*u1, binop(Iop_InterleaveHI64x2, mkexpr(i1), mkexpr(i0)));
      return;
   }
   if (laneSzBlg2 == 2) {
      
      
      
      assign(*u0, binop(Iop_CatEvenLanes32x4, mkexpr(i1), mkexpr(i0)));
      assign(*u1, binop(Iop_CatOddLanes32x4, mkexpr(i1), mkexpr(i0)));
      return;
   }
   if (laneSzBlg2 == 1) {
      
      
      
      
      assign(*u0, binop(Iop_CatEvenLanes16x8, mkexpr(i1), mkexpr(i0)));
      assign(*u1, binop(Iop_CatOddLanes16x8,  mkexpr(i1), mkexpr(i0)));
      return;
   }
   if (laneSzBlg2 == 0) {
      
      
      
      
      assign(*u0, binop(Iop_CatEvenLanes8x16, mkexpr(i1), mkexpr(i0)));
      assign(*u1, binop(Iop_CatOddLanes8x16,  mkexpr(i1), mkexpr(i0)));
      return;
   }
   
   vassert(0);
}


static
void math_DEINTERLEAVE3_128( 
         IRTemp* u0, IRTemp* u1, IRTemp* u2,
        UInt laneSzBlg2,
        IRTemp i0, IRTemp i1, IRTemp i2 )
{
   if (laneSzBlg2 == 3) {
      
      
      
      assign(*u2, ILO64x2( ROL(EX(i2),8), EX(i1)        ));
      assign(*u1, ILO64x2( EX(i2),        ROL(EX(i0),8) ));
      assign(*u0, ILO64x2( ROL(EX(i1),8), EX(i0)        ));
      return;
   }

   if (laneSzBlg2 == 2) {
      
      
      
      
      IRTemp t_a1c0b0a0 = newTempV128();
      IRTemp t_a2c1b1a1 = newTempV128();
      IRTemp t_a3c2b2a2 = newTempV128();
      IRTemp t_a0c3b3a3 = newTempV128();
      IRTemp p0 = newTempV128();
      IRTemp p1 = newTempV128();
      IRTemp p2 = newTempV128();
      
      assign(t_a1c0b0a0, EX(i0));
      assign(t_a2c1b1a1, SL(EX(i1),EX(i0),3*4));
      assign(t_a3c2b2a2, SL(EX(i2),EX(i1),2*4));
      assign(t_a0c3b3a3, SL(EX(i0),EX(i2),1*4));
      
      assign(p0, ILO32x4(EX(t_a2c1b1a1),EX(t_a1c0b0a0)));
      assign(p1, ILO64x2(ILO32x4(EX(t_a0c3b3a3), EX(t_a3c2b2a2)),
                         IHI32x4(EX(t_a2c1b1a1), EX(t_a1c0b0a0))));
      assign(p2, ILO32x4(ROR(EX(t_a0c3b3a3),1*4), ROR(EX(t_a3c2b2a2),1*4)));
      
      math_DEINTERLEAVE3_128(u0, u1, u2, 3, p0, p1, p2);
      return;
   }

   if (laneSzBlg2 == 1) {
      
      
      
      
      
      
      
      
      
      
      
      

      IRTemp s0, s1, s2, s3, t0, t1, t2, t3, p0, p1, p2, c00111111;
      s0 = s1 = s2 = s3
         = t0 = t1 = t2 = t3 = p0 = p1 = p2 = c00111111 = IRTemp_INVALID;
      newTempsV128_4(&s0, &s1, &s2, &s3);
      newTempsV128_4(&t0, &t1, &t2, &t3);
      newTempsV128_4(&p0, &p1, &p2, &c00111111);

      
      
      
      
      assign(s0, EX(i0));
      assign(s1, SL(EX(i1),EX(i0),6*2));
      assign(s2, SL(EX(i2),EX(i1),4*2));
      assign(s3, SL(EX(i0),EX(i2),2*2));

      
      
      
      
      assign(c00111111, mkV128(0x0FFF));
      assign(t0, AND( ILO16x8( ROR(EX(s0),3*2), EX(s0)), EX(c00111111)));
      assign(t1, AND( ILO16x8( ROR(EX(s1),3*2), EX(s1)), EX(c00111111)));
      assign(t2, AND( ILO16x8( ROR(EX(s2),3*2), EX(s2)), EX(c00111111)));
      assign(t3, AND( ILO16x8( ROR(EX(s3),3*2), EX(s3)), EX(c00111111)));

      assign(p0, OR2(EX(t0),          SHL(EX(t1),6*2)));
      assign(p1, OR2(SHL(EX(t2),4*2), SHR(EX(t1),2*2)));
      assign(p2, OR2(SHL(EX(t3),2*2), SHR(EX(t2),4*2)));

      
      math_DEINTERLEAVE3_128(u0, u1, u2, 2, p0, p1, p2);
      return;
   }

   if (laneSzBlg2 == 0) {
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      IRTemp s0, s1, s2, s3, s4, s5, s6, s7,
             t0, t1, t2, t3, t4, t5, t6, t7, p0, p1, p2, cMASK;
      s0 = s1 = s2 = s3 = s4 = s5 = s6 = s7
         = t0 = t1 = t2 = t3 = t4 = t5 = t6 = t7 = p0 = p1 = p2 = cMASK
         = IRTemp_INVALID;
      newTempsV128_4(&s0, &s1, &s2, &s3);
      newTempsV128_4(&s4, &s5, &s6, &s7);
      newTempsV128_4(&t0, &t1, &t2, &t3);
      newTempsV128_4(&t4, &t5, &t6, &t7);
      newTempsV128_4(&p0, &p1, &p2, &cMASK);

      
      
      
      
      
      
      
      
      assign(s0, SL(EX(i1),EX(i0), 0));
      assign(s1, SL(EX(i1),EX(i0), 6));
      assign(s2, SL(EX(i1),EX(i0),12));
      assign(s3, SL(EX(i2),EX(i1), 2));
      assign(s4, SL(EX(i2),EX(i1), 8));
      assign(s5, SL(EX(i2),EX(i1),14));
      assign(s6, SL(EX(i0),EX(i2), 4));
      assign(s7, SL(EX(i0),EX(i2),10));

      
      
      
      
      
      
      
      
      assign(cMASK, mkV128(0x003F));
      assign(t0, AND( ILO8x16( ROR(EX(s0),3), EX(s0)), EX(cMASK)));
      assign(t1, AND( ILO8x16( ROR(EX(s1),3), EX(s1)), EX(cMASK)));
      assign(t2, AND( ILO8x16( ROR(EX(s2),3), EX(s2)), EX(cMASK)));
      assign(t3, AND( ILO8x16( ROR(EX(s3),3), EX(s3)), EX(cMASK)));
      assign(t4, AND( ILO8x16( ROR(EX(s4),3), EX(s4)), EX(cMASK)));
      assign(t5, AND( ILO8x16( ROR(EX(s5),3), EX(s5)), EX(cMASK)));
      assign(t6, AND( ILO8x16( ROR(EX(s6),3), EX(s6)), EX(cMASK)));
      assign(t7, AND( ILO8x16( ROR(EX(s7),3), EX(s7)), EX(cMASK)));

      assign(p0, OR3( SHL(EX(t2),12), SHL(EX(t1),6), EX(t0) ));
      assign(p1, OR4( SHL(EX(t5),14), SHL(EX(t4),8),
                 SHL(EX(t3),2), SHR(EX(t2),4) ));
      assign(p2, OR3( SHL(EX(t7),10), SHL(EX(t6),4), SHR(EX(t5),2) ));

      
      math_DEINTERLEAVE3_128(u0, u1, u2, 1, p0, p1, p2);
      return;
   }

   
   vassert(0);
}


static
void math_DEINTERLEAVE4_128( 
         IRTemp* u0, IRTemp* u1, IRTemp* u2, IRTemp* u3,
        UInt laneSzBlg2,
        IRTemp i0, IRTemp i1, IRTemp i2, IRTemp i3 )
{
   if (laneSzBlg2 == 3) {
      
      assign(*u0, ILO64x2(EX(i2), EX(i0)));
      assign(*u1, IHI64x2(EX(i2), EX(i0)));
      assign(*u2, ILO64x2(EX(i3), EX(i1)));
      assign(*u3, IHI64x2(EX(i3), EX(i1)));
      return;
   }
   if (laneSzBlg2 == 2) {
      
      IRTemp p0 = newTempV128();
      IRTemp p2 = newTempV128();
      IRTemp p1 = newTempV128();
      IRTemp p3 = newTempV128();
      assign(p0, ILO32x4(EX(i1), EX(i0)));
      assign(p1, IHI32x4(EX(i1), EX(i0)));
      assign(p2, ILO32x4(EX(i3), EX(i2)));
      assign(p3, IHI32x4(EX(i3), EX(i2)));
      
      math_DEINTERLEAVE4_128(u0, u1, u2, u3, 3, p0, p1, p2, p3);
      return;
   }
   if (laneSzBlg2 == 1) {
      
      
      IRTemp p0 = newTempV128();
      IRTemp p1 = newTempV128();
      IRTemp p2 = newTempV128();
      IRTemp p3 = newTempV128();
      assign(p0, IHI16x8(EX(i0), SHL(EX(i0), 8)));
      assign(p1, IHI16x8(EX(i1), SHL(EX(i1), 8)));
      assign(p2, IHI16x8(EX(i2), SHL(EX(i2), 8)));
      assign(p3, IHI16x8(EX(i3), SHL(EX(i3), 8)));
      
      math_DEINTERLEAVE4_128(u0, u1, u2, u3, 2, p0, p1, p2, p3);
      return;
   }
   if (laneSzBlg2 == 0) {
      
      
      IRTemp p0 = newTempV128();
      IRTemp p1 = newTempV128();
      IRTemp p2 = newTempV128();
      IRTemp p3 = newTempV128();
      assign(p0, IHI64x2( IHI8x16(EX(i0),ROL(EX(i0),4)),
                          ILO8x16(EX(i0),ROL(EX(i0),4)) ));
      assign(p1, IHI64x2( IHI8x16(EX(i1),ROL(EX(i1),4)),
                          ILO8x16(EX(i1),ROL(EX(i1),4)) ));
      assign(p2, IHI64x2( IHI8x16(EX(i2),ROL(EX(i2),4)),
                          ILO8x16(EX(i2),ROL(EX(i2),4)) ));
      assign(p3, IHI64x2( IHI8x16(EX(i3),ROL(EX(i3),4)),
                          ILO8x16(EX(i3),ROL(EX(i3),4)) ));
      
      math_DEINTERLEAVE4_128(u0, u1, u2, u3, 1, p0, p1, p2, p3);
      return;
   }
   
   vassert(0);
}



static
void math_get_doubler_and_halver ( IROp* doubler,
                                   IROp* halver,
                                   UInt laneSzBlg2 )
{
   switch (laneSzBlg2) {
      case 2:
         *doubler = Iop_InterleaveLO32x4; *halver = Iop_CatEvenLanes32x4;
         break;
      case 1:
         *doubler = Iop_InterleaveLO16x8; *halver = Iop_CatEvenLanes16x8;
         break;
      case 0:
         *doubler = Iop_InterleaveLO8x16; *halver = Iop_CatEvenLanes8x16;
         break;
      default:
         vassert(0);
   }
}

static
void math_INTERLEAVE1_64(  IRTemp* i0,
                          UInt laneSzBlg2, IRTemp u0 )
{
   assign(*i0, mkexpr(u0));
}


static
void math_INTERLEAVE2_64(  IRTemp* i0, IRTemp* i1,
                          UInt laneSzBlg2, IRTemp u0, IRTemp u1 )
{
   if (laneSzBlg2 == 3) {
      
      assign(*i0, EX(u0));
      assign(*i1, EX(u1));
      return;
   }

   vassert(laneSzBlg2 >= 0 && laneSzBlg2 <= 2);
   IROp doubler = Iop_INVALID, halver = Iop_INVALID;
   math_get_doubler_and_halver(&doubler, &halver, laneSzBlg2);

   IRTemp du0 = newTempV128();
   IRTemp du1 = newTempV128();
   assign(du0, binop(doubler, EX(u0), EX(u0)));
   assign(du1, binop(doubler, EX(u1), EX(u1)));
   IRTemp di0 = newTempV128();
   IRTemp di1 = newTempV128();
   math_INTERLEAVE2_128(&di0, &di1, laneSzBlg2 + 1, du0, du1);
   assign(*i0, binop(halver, EX(di0), EX(di0)));
   assign(*i1, binop(halver, EX(di1), EX(di1)));
}


static
void math_INTERLEAVE3_64( 
         IRTemp* i0, IRTemp* i1, IRTemp* i2,
        UInt laneSzBlg2,
        IRTemp u0, IRTemp u1, IRTemp u2 )
{
   if (laneSzBlg2 == 3) {
      
      assign(*i0, EX(u0));
      assign(*i1, EX(u1));
      assign(*i2, EX(u2));
      return;
   }

   vassert(laneSzBlg2 >= 0 && laneSzBlg2 <= 2);
   IROp doubler = Iop_INVALID, halver = Iop_INVALID;
   math_get_doubler_and_halver(&doubler, &halver, laneSzBlg2);

   IRTemp du0 = newTempV128();
   IRTemp du1 = newTempV128();
   IRTemp du2 = newTempV128();
   assign(du0, binop(doubler, EX(u0), EX(u0)));
   assign(du1, binop(doubler, EX(u1), EX(u1)));
   assign(du2, binop(doubler, EX(u2), EX(u2)));
   IRTemp di0 = newTempV128();
   IRTemp di1 = newTempV128();
   IRTemp di2 = newTempV128();
   math_INTERLEAVE3_128(&di0, &di1, &di2, laneSzBlg2 + 1, du0, du1, du2);
   assign(*i0, binop(halver, EX(di0), EX(di0)));
   assign(*i1, binop(halver, EX(di1), EX(di1)));
   assign(*i2, binop(halver, EX(di2), EX(di2)));
}


static
void math_INTERLEAVE4_64( 
         IRTemp* i0, IRTemp* i1, IRTemp* i2, IRTemp* i3,
        UInt laneSzBlg2,
        IRTemp u0, IRTemp u1, IRTemp u2, IRTemp u3 )
{
   if (laneSzBlg2 == 3) {
      
      assign(*i0, EX(u0));
      assign(*i1, EX(u1));
      assign(*i2, EX(u2));
      assign(*i3, EX(u3));
      return;
   }

   vassert(laneSzBlg2 >= 0 && laneSzBlg2 <= 2);
   IROp doubler = Iop_INVALID, halver = Iop_INVALID;
   math_get_doubler_and_halver(&doubler, &halver, laneSzBlg2);

   IRTemp du0 = newTempV128();
   IRTemp du1 = newTempV128();
   IRTemp du2 = newTempV128();
   IRTemp du3 = newTempV128();
   assign(du0, binop(doubler, EX(u0), EX(u0)));
   assign(du1, binop(doubler, EX(u1), EX(u1)));
   assign(du2, binop(doubler, EX(u2), EX(u2)));
   assign(du3, binop(doubler, EX(u3), EX(u3)));
   IRTemp di0 = newTempV128();
   IRTemp di1 = newTempV128();
   IRTemp di2 = newTempV128();
   IRTemp di3 = newTempV128();
   math_INTERLEAVE4_128(&di0, &di1, &di2, &di3,
                        laneSzBlg2 + 1, du0, du1, du2, du3);
   assign(*i0, binop(halver, EX(di0), EX(di0)));
   assign(*i1, binop(halver, EX(di1), EX(di1)));
   assign(*i2, binop(halver, EX(di2), EX(di2)));
   assign(*i3, binop(halver, EX(di3), EX(di3)));
}


static
void math_DEINTERLEAVE1_64(  IRTemp* u0,
                            UInt laneSzBlg2, IRTemp i0 )
{
   assign(*u0, mkexpr(i0));
}


static
void math_DEINTERLEAVE2_64(  IRTemp* u0, IRTemp* u1,
                            UInt laneSzBlg2, IRTemp i0, IRTemp i1 )
{
   if (laneSzBlg2 == 3) {
      
      assign(*u0, EX(i0));
      assign(*u1, EX(i1));
      return;
   }

   vassert(laneSzBlg2 >= 0 && laneSzBlg2 <= 2);
   IROp doubler = Iop_INVALID, halver = Iop_INVALID;
   math_get_doubler_and_halver(&doubler, &halver, laneSzBlg2);

   IRTemp di0 = newTempV128();
   IRTemp di1 = newTempV128();
   assign(di0, binop(doubler, EX(i0), EX(i0)));
   assign(di1, binop(doubler, EX(i1), EX(i1)));

   IRTemp du0 = newTempV128();
   IRTemp du1 = newTempV128();
   math_DEINTERLEAVE2_128(&du0, &du1, laneSzBlg2 + 1, di0, di1);
   assign(*u0, binop(halver, EX(du0), EX(du0)));
   assign(*u1, binop(halver, EX(du1), EX(du1)));
}


static
void math_DEINTERLEAVE3_64( 
         IRTemp* u0, IRTemp* u1, IRTemp* u2,
        UInt laneSzBlg2,
        IRTemp i0, IRTemp i1, IRTemp i2 )
{
   if (laneSzBlg2 == 3) {
      
      assign(*u0, EX(i0));
      assign(*u1, EX(i1));
      assign(*u2, EX(i2));
      return;
   }

   vassert(laneSzBlg2 >= 0 && laneSzBlg2 <= 2);
   IROp doubler = Iop_INVALID, halver = Iop_INVALID;
   math_get_doubler_and_halver(&doubler, &halver, laneSzBlg2);

   IRTemp di0 = newTempV128();
   IRTemp di1 = newTempV128();
   IRTemp di2 = newTempV128();
   assign(di0, binop(doubler, EX(i0), EX(i0)));
   assign(di1, binop(doubler, EX(i1), EX(i1)));
   assign(di2, binop(doubler, EX(i2), EX(i2)));
   IRTemp du0 = newTempV128();
   IRTemp du1 = newTempV128();
   IRTemp du2 = newTempV128();
   math_DEINTERLEAVE3_128(&du0, &du1, &du2, laneSzBlg2 + 1, di0, di1, di2);
   assign(*u0, binop(halver, EX(du0), EX(du0)));
   assign(*u1, binop(halver, EX(du1), EX(du1)));
   assign(*u2, binop(halver, EX(du2), EX(du2)));
}


static
void math_DEINTERLEAVE4_64( 
         IRTemp* u0, IRTemp* u1, IRTemp* u2, IRTemp* u3,
        UInt laneSzBlg2,
        IRTemp i0, IRTemp i1, IRTemp i2, IRTemp i3 )
{
   if (laneSzBlg2 == 3) {
      
      assign(*u0, EX(i0));
      assign(*u1, EX(i1));
      assign(*u2, EX(i2));
      assign(*u3, EX(i3));
      return;
   }

   vassert(laneSzBlg2 >= 0 && laneSzBlg2 <= 2);
   IROp doubler = Iop_INVALID, halver = Iop_INVALID;
   math_get_doubler_and_halver(&doubler, &halver, laneSzBlg2);

   IRTemp di0 = newTempV128();
   IRTemp di1 = newTempV128();
   IRTemp di2 = newTempV128();
   IRTemp di3 = newTempV128();
   assign(di0, binop(doubler, EX(i0), EX(i0)));
   assign(di1, binop(doubler, EX(i1), EX(i1)));
   assign(di2, binop(doubler, EX(i2), EX(i2)));
   assign(di3, binop(doubler, EX(i3), EX(i3)));
   IRTemp du0 = newTempV128();
   IRTemp du1 = newTempV128();
   IRTemp du2 = newTempV128();
   IRTemp du3 = newTempV128();
   math_DEINTERLEAVE4_128(&du0, &du1, &du2, &du3,
                          laneSzBlg2 + 1, di0, di1, di2, di3);
   assign(*u0, binop(halver, EX(du0), EX(du0)));
   assign(*u1, binop(halver, EX(du1), EX(du1)));
   assign(*u2, binop(halver, EX(du2), EX(du2)));
   assign(*u3, binop(halver, EX(du3), EX(du3)));
}


#undef EX
#undef SL
#undef ROR
#undef ROL
#undef SHR
#undef SHL
#undef ILO64x2
#undef IHI64x2
#undef ILO32x4
#undef IHI32x4
#undef ILO16x8
#undef IHI16x8
#undef ILO16x8
#undef IHI16x8
#undef CEV32x4
#undef COD32x4
#undef COD16x8
#undef COD8x16
#undef CEV8x16
#undef AND
#undef OR2
#undef OR3
#undef OR4



static IRTemp gen_indexed_EA ( HChar* buf, UInt insn, Bool isInt )
{
   UInt    optS  = SLICE_UInt(insn, 15, 12);
   UInt    mm    = SLICE_UInt(insn, 20, 16);
   UInt    nn    = SLICE_UInt(insn, 9, 5);
   UInt    szLg2 = (isInt ? 0 : (SLICE_UInt(insn, 23, 23) << 2))
                   | SLICE_UInt(insn, 31, 30); 

   buf[0] = 0;

   
   if (SLICE_UInt(insn, 11, 10) != BITS2(1,0))
      goto fail;

   if (isInt
       && SLICE_UInt(insn, 29, 21) != BITS9(1,1,1,0,0,0,0,1,1)
       && SLICE_UInt(insn, 29, 21) != BITS9(1,1,1,0,0,0,0,0,1)
       && SLICE_UInt(insn, 29, 21) != BITS9(1,1,1,0,0,0,1,0,1)
       && SLICE_UInt(insn, 29, 21) != BITS9(1,1,1,0,0,0,1,1,1))
      goto fail;

   if (!isInt
       && SLICE_UInt(insn, 29, 24) != BITS6(1,1,1,1,0,0)) 
      goto fail;

   
   switch (szLg2) {
      case BITS3(0,0,0): break; 
      case BITS3(0,0,1): break; 
      case BITS3(0,1,0): break; 
      case BITS3(0,1,1): break; 
      case BITS3(1,0,0): 
                         if (isInt) goto fail; else break;
      case BITS3(1,0,1): 
      case BITS3(1,1,0):
      case BITS3(1,1,1): goto fail;

      default: vassert(0);
   }

   IRExpr* rhs  = NULL;
   switch (optS) {
      case BITS4(1,1,1,0): goto fail; 
      case BITS4(0,1,1,0):
         rhs = getIReg64orZR(mm);
         vex_sprintf(buf, "[%s, %s]",
                     nameIReg64orZR(nn), nameIReg64orZR(mm));
         break;
      case BITS4(1,1,1,1): goto fail; 
      case BITS4(0,1,1,1):
         rhs = binop(Iop_Shl64, getIReg64orZR(mm), mkU8(szLg2));
         vex_sprintf(buf, "[%s, %s lsl %u]",
                     nameIReg64orZR(nn), nameIReg64orZR(mm), szLg2);
         break;
      case BITS4(0,1,0,0):
         rhs = unop(Iop_32Uto64, getIReg32orZR(mm));
         vex_sprintf(buf, "[%s, %s uxtx]",
                     nameIReg64orZR(nn), nameIReg32orZR(mm));
         break;
      case BITS4(0,1,0,1):
         rhs = binop(Iop_Shl64,
                     unop(Iop_32Uto64, getIReg32orZR(mm)), mkU8(szLg2));
         vex_sprintf(buf, "[%s, %s uxtx, lsl %u]",
                     nameIReg64orZR(nn), nameIReg32orZR(mm), szLg2);
         break;
      case BITS4(1,1,0,0):
         rhs = unop(Iop_32Sto64, getIReg32orZR(mm));
         vex_sprintf(buf, "[%s, %s sxtx]",
                     nameIReg64orZR(nn), nameIReg32orZR(mm));
         break;
      case BITS4(1,1,0,1):
         rhs = binop(Iop_Shl64,
                     unop(Iop_32Sto64, getIReg32orZR(mm)), mkU8(szLg2));
         vex_sprintf(buf, "[%s, %s sxtx, lsl %u]",
                     nameIReg64orZR(nn), nameIReg32orZR(mm), szLg2);
         break;
      default:
         
         goto fail;
   }

   vassert(rhs);
   IRTemp res = newTemp(Ity_I64);
   assign(res, binop(Iop_Add64, getIReg64orSP(nn), rhs));
   return res;

  fail:
   vex_printf("gen_indexed_EA: unhandled case optS == 0x%x\n", optS);
   return IRTemp_INVALID;
}


static void gen_narrowing_store ( UInt szB, IRTemp addr, IRExpr* dataE )
{
   IRExpr* addrE = mkexpr(addr);
   switch (szB) {
      case 8:
         storeLE(addrE, dataE);
         break;
      case 4:
         storeLE(addrE, unop(Iop_64to32, dataE));
         break;
      case 2:
         storeLE(addrE, unop(Iop_64to16, dataE));
         break;
      case 1:
         storeLE(addrE, unop(Iop_64to8, dataE));
         break;
      default:
         vassert(0);
   }
}


static IRTemp gen_zwidening_load ( UInt szB, IRTemp addr )
{
   IRTemp  res   = newTemp(Ity_I64);
   IRExpr* addrE = mkexpr(addr);
   switch (szB) {
      case 8:
         assign(res, loadLE(Ity_I64,addrE));
         break;
      case 4:
         assign(res, unop(Iop_32Uto64, loadLE(Ity_I32,addrE)));
         break;
      case 2:
         assign(res, unop(Iop_16Uto64, loadLE(Ity_I16,addrE)));
         break;
      case 1:
         assign(res, unop(Iop_8Uto64, loadLE(Ity_I8,addrE)));
         break;
      default:
         vassert(0);
   }
   return res;
}


static
const HChar* nameArr_Q_SZ ( UInt bitQ, UInt size )
{
   vassert(bitQ <= 1 && size <= 3);
   const HChar* nms[8]
      = { "8b", "4h", "2s", "1d", "16b", "8h", "4s", "2d" };
   UInt ix = (bitQ << 2) | size;
   vassert(ix < 8);
   return nms[ix];
}


static
Bool dis_ARM64_load_store(DisResult* dres, UInt insn)
{
#  define INSN(_bMax,_bMin)  SLICE_UInt(insn, (_bMax), (_bMin))

   
   if (INSN(29,23) == BITS7(1,1,1,0,0,1,0)) {
      UInt   szLg2 = INSN(31,30);
      UInt   szB   = 1 << szLg2;
      Bool   isLD  = INSN(22,22) == 1;
      UInt   offs  = INSN(21,10) * szB;
      UInt   nn    = INSN(9,5);
      UInt   tt    = INSN(4,0);
      IRTemp ta    = newTemp(Ity_I64);
      assign(ta, binop(Iop_Add64, getIReg64orSP(nn), mkU64(offs)));
      if (nn == 31) {  }
      vassert(szLg2 < 4);
      if (isLD) {
         putIReg64orZR(tt, mkexpr(gen_zwidening_load(szB, ta)));
      } else {
         gen_narrowing_store(szB, ta, getIReg64orZR(tt));
      }
      const HChar* ld_name[4] = { "ldrb", "ldrh", "ldr", "ldr" };
      const HChar* st_name[4] = { "strb", "strh", "str", "str" };
      DIP("%s %s, [%s, #%u]\n", 
          (isLD ? ld_name : st_name)[szLg2], nameIRegOrZR(szB == 8, tt),
          nameIReg64orSP(nn), offs);
      return True;
   }

   
   if ((INSN(29,21) & BITS9(1,1,1, 1,1,1,1,0, 1))
       == BITS9(1,1,1, 0,0,0,0,0, 0)) {
      UInt szLg2  = INSN(31,30);
      UInt szB    = 1 << szLg2;
      Bool isLoad = INSN(22,22) == 1;
      UInt imm9   = INSN(20,12);
      UInt nn     = INSN(9,5);
      UInt tt     = INSN(4,0);
      Bool wBack  = INSN(10,10) == 1;
      UInt how    = INSN(11,10);
      if (how == BITS2(1,0) || (wBack && nn == tt && tt != 31)) {
         
      } else {
         if (nn == 31) {  }

         
         IRTemp tRN = newTemp(Ity_I64);
         assign(tRN, getIReg64orSP(nn));
         IRTemp tEA = newTemp(Ity_I64);
         Long simm9 = (Long)sx_to_64(imm9, 9);
         assign(tEA, binop(Iop_Add64, mkexpr(tRN), mkU64(simm9)));

         IRTemp tTA = newTemp(Ity_I64);
         IRTemp tWA = newTemp(Ity_I64);
         switch (how) {
            case BITS2(0,1):
               assign(tTA, mkexpr(tRN)); assign(tWA, mkexpr(tEA)); break;
            case BITS2(1,1):
               assign(tTA, mkexpr(tEA)); assign(tWA, mkexpr(tEA)); break;
            case BITS2(0,0):
               assign(tTA, mkexpr(tEA));  break;
            default:
               vassert(0); 
         }

         Bool earlyWBack
           = wBack && simm9 < 0 && szB == 8
             && how == BITS2(1,1) && nn == 31 && !isLoad && tt != nn;

         if (wBack && earlyWBack)
            putIReg64orSP(nn, mkexpr(tEA));

         if (isLoad) {
            putIReg64orZR(tt, mkexpr(gen_zwidening_load(szB, tTA)));
         } else {
            gen_narrowing_store(szB, tTA, getIReg64orZR(tt));
         }

         if (wBack && !earlyWBack)
            putIReg64orSP(nn, mkexpr(tEA));

         const HChar* ld_name[4] = { "ldurb", "ldurh", "ldur", "ldur" };
         const HChar* st_name[4] = { "sturb", "sturh", "stur", "stur" };
         const HChar* fmt_str = NULL;
         switch (how) {
            case BITS2(0,1):
               fmt_str = "%s %s, [%s], #%lld (at-Rn-then-Rn=EA)\n";
               break;
            case BITS2(1,1):
               fmt_str = "%s %s, [%s, #%lld]! (at-EA-then-Rn=EA)\n";
               break;
            case BITS2(0,0):
               fmt_str = "%s %s, [%s, #%lld] (at-Rn)\n";
               break;
            default:
               vassert(0);
         }
         DIP(fmt_str, (isLoad ? ld_name : st_name)[szLg2],
                      nameIRegOrZR(szB == 8, tt),
                      nameIReg64orSP(nn), simm9);
         return True;
      }
   }

   

   UInt insn_30_23 = INSN(30,23);
   if (insn_30_23 == BITS8(0,1,0,1,0,0,0,1) 
       || insn_30_23 == BITS8(0,1,0,1,0,0,1,1)
       || insn_30_23 == BITS8(0,1,0,1,0,0,1,0)) {
      UInt bL     = INSN(22,22);
      UInt bX     = INSN(31,31);
      UInt bWBack = INSN(23,23);
      UInt rT1    = INSN(4,0);
      UInt rN     = INSN(9,5);
      UInt rT2    = INSN(14,10);
      Long simm7  = (Long)sx_to_64(INSN(21,15), 7);
      if ((bWBack && (rT1 == rN || rT2 == rN) && rN != 31)
          || (bL && rT1 == rT2)) {
         
      } else {
         if (rN == 31) {  }

         
         IRTemp tRN = newTemp(Ity_I64);
         assign(tRN, getIReg64orSP(rN));
         IRTemp tEA = newTemp(Ity_I64);
         simm7 = (bX ? 8 : 4) * simm7;
         assign(tEA, binop(Iop_Add64, mkexpr(tRN), mkU64(simm7)));

         IRTemp tTA = newTemp(Ity_I64);
         IRTemp tWA = newTemp(Ity_I64);
         switch (INSN(24,23)) {
            case BITS2(0,1):
               assign(tTA, mkexpr(tRN)); assign(tWA, mkexpr(tEA)); break;
            case BITS2(1,1):
               assign(tTA, mkexpr(tEA)); assign(tWA, mkexpr(tEA)); break;
            case BITS2(1,0):
               assign(tTA, mkexpr(tEA));  break;
            default:
               vassert(0); 
         }

         Bool earlyWBack
           = bWBack && simm7 < 0
             && INSN(24,23) == BITS2(1,1) && rN == 31 && bL == 0;

         if (bWBack && earlyWBack)
            putIReg64orSP(rN, mkexpr(tEA));

          if (bL == 1 && bX == 1) {
            
            putIReg64orZR(rT1, loadLE(Ity_I64,
                                      binop(Iop_Add64,mkexpr(tTA),mkU64(0))));
            putIReg64orZR(rT2, loadLE(Ity_I64, 
                                      binop(Iop_Add64,mkexpr(tTA),mkU64(8))));
         } else if (bL == 1 && bX == 0) {
            
            putIReg32orZR(rT1, loadLE(Ity_I32,
                                      binop(Iop_Add64,mkexpr(tTA),mkU64(0))));
            putIReg32orZR(rT2, loadLE(Ity_I32, 
                                      binop(Iop_Add64,mkexpr(tTA),mkU64(4))));
         } else if (bL == 0 && bX == 1) {
            
            storeLE(binop(Iop_Add64,mkexpr(tTA),mkU64(0)),
                    getIReg64orZR(rT1));
            storeLE(binop(Iop_Add64,mkexpr(tTA),mkU64(8)),
                    getIReg64orZR(rT2));
         } else {
            vassert(bL == 0 && bX == 0);
            
            storeLE(binop(Iop_Add64,mkexpr(tTA),mkU64(0)),
                    getIReg32orZR(rT1));
            storeLE(binop(Iop_Add64,mkexpr(tTA),mkU64(4)),
                    getIReg32orZR(rT2));
         }

         if (bWBack && !earlyWBack)
            putIReg64orSP(rN, mkexpr(tEA));

         const HChar* fmt_str = NULL;
         switch (INSN(24,23)) {
            case BITS2(0,1):
               fmt_str = "%sp %s, %s, [%s], #%lld (at-Rn-then-Rn=EA)\n";
               break;
            case BITS2(1,1):
               fmt_str = "%sp %s, %s, [%s, #%lld]! (at-EA-then-Rn=EA)\n";
               break;
            case BITS2(1,0):
               fmt_str = "%sp %s, %s, [%s, #%lld] (at-Rn)\n";
               break;
            default:
               vassert(0);
         }
         DIP(fmt_str, bL == 0 ? "st" : "ld",
                      nameIRegOrZR(bX == 1, rT1),
                      nameIRegOrZR(bX == 1, rT2),
                      nameIReg64orSP(rN), simm7);
         return True;
      }
   }

   
   if (INSN(29,24) == BITS6(0,1,1,0,0,0) && INSN(31,31) == 0) {
      UInt  imm19 = INSN(23,5);
      UInt  rT    = INSN(4,0);
      UInt  bX    = INSN(30,30);
      ULong ea    = guest_PC_curr_instr + sx_to_64(imm19 << 2, 21);
      if (bX) {
         putIReg64orZR(rT, loadLE(Ity_I64, mkU64(ea)));
      } else {
         putIReg32orZR(rT, loadLE(Ity_I32, mkU64(ea)));
      }
      DIP("ldr %s, 0x%llx (literal)\n", nameIRegOrZR(bX == 1, rT), ea);
      return True;
   }

   
   if (INSN(29,23) == BITS7(1,1,1,0,0,0,0)
       && INSN(21,21) == 1 && INSN(11,10) == BITS2(1,0)) {
      HChar  dis_buf[64];
      UInt   szLg2 = INSN(31,30);
      Bool   isLD  = INSN(22,22) == 1;
      UInt   tt    = INSN(4,0);
      IRTemp ea    = gen_indexed_EA(dis_buf, insn, True);
      if (ea != IRTemp_INVALID) {
         switch (szLg2) {
            case 3: 
               if (isLD) {
                  putIReg64orZR(tt, loadLE(Ity_I64, mkexpr(ea)));
                  DIP("ldr %s, %s\n", nameIReg64orZR(tt), dis_buf);
               } else {
                  storeLE(mkexpr(ea), getIReg64orZR(tt));
                  DIP("str %s, %s\n", nameIReg64orZR(tt), dis_buf);
               }
               break;
            case 2: 
               if (isLD) {
                  putIReg32orZR(tt, loadLE(Ity_I32, mkexpr(ea)));
                  DIP("ldr %s, %s\n", nameIReg32orZR(tt), dis_buf);
               } else {
                  storeLE(mkexpr(ea), getIReg32orZR(tt));
                  DIP("str %s, %s\n", nameIReg32orZR(tt), dis_buf);
               }
               break;
            case 1: 
               if (isLD) {
                  putIReg64orZR(tt, unop(Iop_16Uto64,
                                         loadLE(Ity_I16, mkexpr(ea))));
                  DIP("ldruh %s, %s\n", nameIReg32orZR(tt), dis_buf);
               } else {
                  storeLE(mkexpr(ea), unop(Iop_64to16, getIReg64orZR(tt)));
                  DIP("strh %s, %s\n", nameIReg32orZR(tt), dis_buf);
               }
               break;
            case 0: 
               if (isLD) {
                  putIReg64orZR(tt, unop(Iop_8Uto64,
                                         loadLE(Ity_I8, mkexpr(ea))));
                  DIP("ldrub %s, %s\n", nameIReg32orZR(tt), dis_buf);
               } else {
                  storeLE(mkexpr(ea), unop(Iop_64to8, getIReg64orZR(tt)));
                  DIP("strb %s, %s\n", nameIReg32orZR(tt), dis_buf);
               }
               break;
            default:
               vassert(0);
         }
         return True;
      }
   }

   
   if (INSN(29,23) == BITS7(1,1,1,0,0,1,1)) {
      
      Bool valid = False;
      switch ((INSN(31,30) << 1) | INSN(22,22)) {
         case BITS3(1,0,0):
         case BITS3(0,1,0): case BITS3(0,1,1):
         case BITS3(0,0,0): case BITS3(0,0,1):
            valid = True;
            break;
      }
      if (valid) {
         UInt    szLg2 = INSN(31,30);
         UInt    bitX  = INSN(22,22);
         UInt    imm12 = INSN(21,10);
         UInt    nn    = INSN(9,5);
         UInt    tt    = INSN(4,0);
         UInt    szB   = 1 << szLg2;
         IRExpr* ea    = binop(Iop_Add64,
                               getIReg64orSP(nn), mkU64(imm12 * szB));
         switch (szB) {
            case 4:
               vassert(bitX == 0);
               putIReg64orZR(tt, unop(Iop_32Sto64, loadLE(Ity_I32, ea)));
               DIP("ldrsw %s, [%s, #%u]\n", nameIReg64orZR(tt),
                   nameIReg64orSP(nn), imm12 * szB);
               break;
            case 2:
               if (bitX == 1) {
                  putIReg32orZR(tt, unop(Iop_16Sto32, loadLE(Ity_I16, ea)));
               } else {
                  putIReg64orZR(tt, unop(Iop_16Sto64, loadLE(Ity_I16, ea)));
               }
               DIP("ldrsh %s, [%s, #%u]\n",
                   nameIRegOrZR(bitX == 0, tt),
                   nameIReg64orSP(nn), imm12 * szB);
               break;
            case 1:
               if (bitX == 1) {
                  putIReg32orZR(tt, unop(Iop_8Sto32, loadLE(Ity_I8, ea)));
               } else {
                  putIReg64orZR(tt, unop(Iop_8Sto64, loadLE(Ity_I8, ea)));
               }
               DIP("ldrsb %s, [%s, #%u]\n",
                   nameIRegOrZR(bitX == 0, tt),
                   nameIReg64orSP(nn), imm12 * szB);
               break;
            default:
               vassert(0);
         }
         return True;
      }
      
   }

   
   if (INSN(29,23) == BITS7(1,1,1,0,0,0,1)
       && INSN(21,21) == 0 && INSN(10,10) == 1) {
      
      Bool valid = False;
      switch ((INSN(31,30) << 1) | INSN(22,22)) {
         case BITS3(1,0,0):                    
         case BITS3(0,1,0): case BITS3(0,1,1): 
         case BITS3(0,0,0): case BITS3(0,0,1): 
            valid = True;
            break;
      }
      if (valid) {
         UInt   szLg2 = INSN(31,30);
         UInt   imm9  = INSN(20,12);
         Bool   atRN  = INSN(11,11) == 0;
         UInt   nn    = INSN(9,5);
         UInt   tt    = INSN(4,0);
         IRTemp tRN   = newTemp(Ity_I64);
         IRTemp tEA   = newTemp(Ity_I64);
         IRTemp tTA   = IRTemp_INVALID;
         ULong  simm9 = sx_to_64(imm9, 9);
         Bool   is64  = INSN(22,22) == 0;
         assign(tRN, getIReg64orSP(nn));
         assign(tEA, binop(Iop_Add64, mkexpr(tRN), mkU64(simm9)));
         tTA = atRN ? tRN : tEA;
         HChar ch = '?';
         if (szLg2 == 0) {
            ch = 'b';
            if (is64) {
               putIReg64orZR(tt, unop(Iop_8Sto64,
                                      loadLE(Ity_I8, mkexpr(tTA))));
            } else {
               putIReg32orZR(tt, unop(Iop_8Sto32,
                                      loadLE(Ity_I8, mkexpr(tTA))));
            }
         }
         else if (szLg2 == 1) {
            ch = 'h';
            if (is64) {
               putIReg64orZR(tt, unop(Iop_16Sto64,
                                      loadLE(Ity_I16, mkexpr(tTA))));
            } else {
               putIReg32orZR(tt, unop(Iop_16Sto32,
                                      loadLE(Ity_I16, mkexpr(tTA))));
            }
         }
         else if (szLg2 == 2 && is64) {
            ch = 'w';
            putIReg64orZR(tt, unop(Iop_32Sto64,
                                   loadLE(Ity_I32, mkexpr(tTA))));
         }
         else {
            vassert(0);
         }
         putIReg64orSP(nn, mkexpr(tEA));
         DIP(atRN ? "ldrs%c %s, [%s], #%lld\n" : "ldrs%c %s, [%s, #%lld]!",
             ch, nameIRegOrZR(is64, tt), nameIReg64orSP(nn), simm9);
         return True;
      }
      
   }

   
   if (INSN(29,23) == BITS7(1,1,1,0,0,0,1)
       && INSN(21,21) == 0 && INSN(11,10) == BITS2(0,0)) {
      
      Bool valid = False;
      switch ((INSN(31,30) << 1) | INSN(22,22)) {
         case BITS3(1,0,0):                    
         case BITS3(0,1,0): case BITS3(0,1,1): 
         case BITS3(0,0,0): case BITS3(0,0,1): 
            valid = True;
            break;
      }
      if (valid) {
         UInt   szLg2 = INSN(31,30);
         UInt   imm9  = INSN(20,12);
         UInt   nn    = INSN(9,5);
         UInt   tt    = INSN(4,0);
         IRTemp tRN   = newTemp(Ity_I64);
         IRTemp tEA   = newTemp(Ity_I64);
         ULong  simm9 = sx_to_64(imm9, 9);
         Bool   is64  = INSN(22,22) == 0;
         assign(tRN, getIReg64orSP(nn));
         assign(tEA, binop(Iop_Add64, mkexpr(tRN), mkU64(simm9)));
         HChar ch = '?';
         if (szLg2 == 0) {
            ch = 'b';
            if (is64) {
               putIReg64orZR(tt, unop(Iop_8Sto64,
                                      loadLE(Ity_I8, mkexpr(tEA))));
            } else {
               putIReg32orZR(tt, unop(Iop_8Sto32,
                                      loadLE(Ity_I8, mkexpr(tEA))));
            }
         }
         else if (szLg2 == 1) {
            ch = 'h';
            if (is64) {
               putIReg64orZR(tt, unop(Iop_16Sto64,
                                      loadLE(Ity_I16, mkexpr(tEA))));
            } else {
               putIReg32orZR(tt, unop(Iop_16Sto32,
                                      loadLE(Ity_I16, mkexpr(tEA))));
            }
         }
         else if (szLg2 == 2 && is64) {
            ch = 'w';
            putIReg64orZR(tt, unop(Iop_32Sto64,
                                   loadLE(Ity_I32, mkexpr(tEA))));
         }
         else {
            vassert(0);
         }
         DIP("ldurs%c %s, [%s, #%lld]",
             ch, nameIRegOrZR(is64, tt), nameIReg64orSP(nn), simm9);
         return True;
      }
      
   }

   
   if (INSN(29,25) == BITS5(1,0,1,1,0)) {
      UInt szSlg2 = INSN(31,30); 
      Bool isLD   = INSN(22,22) == 1;
      Bool wBack  = INSN(23,23) == 1;
      Long simm7  = (Long)sx_to_64(INSN(21,15), 7);
      UInt tt2    = INSN(14,10);
      UInt nn     = INSN(9,5);
      UInt tt1    = INSN(4,0);
      if (szSlg2 == BITS2(1,1) || (isLD && tt1 == tt2)) {
         
      } else {
         if (nn == 31) {  }

         
         UInt   szB = 4 << szSlg2; 
         IRTemp tRN = newTemp(Ity_I64);
         assign(tRN, getIReg64orSP(nn));
         IRTemp tEA = newTemp(Ity_I64);
         simm7 = szB * simm7;
         assign(tEA, binop(Iop_Add64, mkexpr(tRN), mkU64(simm7)));

         IRTemp tTA = newTemp(Ity_I64);
         IRTemp tWA = newTemp(Ity_I64);
         switch (INSN(24,23)) {
            case BITS2(0,1):
               assign(tTA, mkexpr(tRN)); assign(tWA, mkexpr(tEA)); break;
            case BITS2(1,1):
               assign(tTA, mkexpr(tEA)); assign(tWA, mkexpr(tEA)); break;
            case BITS2(1,0):
            case BITS2(0,0):
               assign(tTA, mkexpr(tEA));  break;
            default:
               vassert(0); 
         }

         IRType ty = Ity_INVALID;
         switch (szB) {
            case 4:  ty = Ity_F32;  break;
            case 8:  ty = Ity_F64;  break;
            case 16: ty = Ity_V128; break;
            default: vassert(0);
         }

         Bool earlyWBack
           = wBack && simm7 < 0
             && INSN(24,23) == BITS2(1,1) && nn == 31 && !isLD;

         if (wBack && earlyWBack)
            putIReg64orSP(nn, mkexpr(tEA));

         if (isLD) {
            if (szB < 16) {
               putQReg128(tt1, mkV128(0x0000));
            }
            putQRegLO(tt1,
                      loadLE(ty, binop(Iop_Add64, mkexpr(tTA), mkU64(0))));
            if (szB < 16) {
               putQReg128(tt2, mkV128(0x0000));
            }
            putQRegLO(tt2,
                      loadLE(ty, binop(Iop_Add64, mkexpr(tTA), mkU64(szB))));
         } else {
            storeLE(binop(Iop_Add64, mkexpr(tTA), mkU64(0)),
                    getQRegLO(tt1, ty));
            storeLE(binop(Iop_Add64, mkexpr(tTA), mkU64(szB)),
                    getQRegLO(tt2, ty));
         }

         if (wBack && !earlyWBack)
            putIReg64orSP(nn, mkexpr(tEA));

         const HChar* fmt_str = NULL;
         switch (INSN(24,23)) {
            case BITS2(0,1):
               fmt_str = "%sp %s, %s, [%s], #%lld (at-Rn-then-Rn=EA)\n";
               break;
            case BITS2(1,1):
               fmt_str = "%sp %s, %s, [%s, #%lld]! (at-EA-then-Rn=EA)\n";
               break;
            case BITS2(1,0):
               fmt_str = "%sp %s, %s, [%s, #%lld] (at-Rn)\n";
               break;
            case BITS2(0,0):
               fmt_str = "%snp %s, %s, [%s, #%lld] (at-Rn)\n";
               break;
            default:
               vassert(0);
         }
         DIP(fmt_str, isLD ? "ld" : "st",
                      nameQRegLO(tt1, ty), nameQRegLO(tt2, ty),
                      nameIReg64orSP(nn), simm7);
         return True;
      }
   }

   
   if (INSN(29,24) == BITS6(1,1,1,1,0,0)
       && INSN(21,21) == 1 && INSN(11,10) == BITS2(1,0)) {
      HChar  dis_buf[64];
      UInt   szLg2 = (INSN(23,23) << 2) | INSN(31,30);
      Bool   isLD  = INSN(22,22) == 1;
      UInt   tt    = INSN(4,0);
      if (szLg2 > 4) goto after_LDR_STR_vector_register;
      IRTemp ea    = gen_indexed_EA(dis_buf, insn, False);
      if (ea == IRTemp_INVALID) goto after_LDR_STR_vector_register;
      switch (szLg2) {
         case 0: 
            if (isLD) {
               putQReg128(tt, mkV128(0x0000));
               putQRegLO(tt, loadLE(Ity_I8, mkexpr(ea)));
               DIP("ldr %s, %s\n", nameQRegLO(tt, Ity_I8), dis_buf);
            } else {
               storeLE(mkexpr(ea), getQRegLO(tt, Ity_I8));
               DIP("str %s, %s\n", nameQRegLO(tt, Ity_I8), dis_buf);
            }
            break;
         case 1:
            if (isLD) {
               putQReg128(tt, mkV128(0x0000));
               putQRegLO(tt, loadLE(Ity_I16, mkexpr(ea)));
               DIP("ldr %s, %s\n", nameQRegLO(tt, Ity_I16), dis_buf);
            } else {
               storeLE(mkexpr(ea), getQRegLO(tt, Ity_I16));
               DIP("str %s, %s\n", nameQRegLO(tt, Ity_I16), dis_buf);
            }
            break;
         case 2: 
            if (isLD) {
               putQReg128(tt, mkV128(0x0000));
               putQRegLO(tt, loadLE(Ity_I32, mkexpr(ea)));
               DIP("ldr %s, %s\n", nameQRegLO(tt, Ity_I32), dis_buf);
            } else {
               storeLE(mkexpr(ea), getQRegLO(tt, Ity_I32));
               DIP("str %s, %s\n", nameQRegLO(tt, Ity_I32), dis_buf);
            }
            break;
         case 3: 
            if (isLD) {
               putQReg128(tt, mkV128(0x0000));
               putQRegLO(tt, loadLE(Ity_I64, mkexpr(ea)));
               DIP("ldr %s, %s\n", nameQRegLO(tt, Ity_I64), dis_buf);
            } else {
               storeLE(mkexpr(ea), getQRegLO(tt, Ity_I64));
               DIP("str %s, %s\n", nameQRegLO(tt, Ity_I64), dis_buf);
            }
            break;
         case 4:
            if (isLD) {
               putQReg128(tt, loadLE(Ity_V128, mkexpr(ea)));
               DIP("ldr %s, %s\n", nameQReg128(tt), dis_buf);
            } else {
               storeLE(mkexpr(ea), getQReg128(tt));
               DIP("str %s, %s\n", nameQReg128(tt), dis_buf);
            }
            break;
         default:
            vassert(0);
      }
      return True;
   }
  after_LDR_STR_vector_register:

   
   if (INSN(29,23) == BITS7(1,1,1,0,0,0,1)
       && INSN(21,21) == 1 && INSN(11,10) == BITS2(1,0)) {
      HChar  dis_buf[64];
      UInt   szLg2  = INSN(31,30);
      Bool   sxTo64 = INSN(22,22) == 0; 
      UInt   tt     = INSN(4,0);
      if (szLg2 == 3) goto after_LDRS_integer_register;
      IRTemp ea     = gen_indexed_EA(dis_buf, insn, True);
      if (ea == IRTemp_INVALID) goto after_LDRS_integer_register;
      
      if (szLg2 == 2 && sxTo64) {
         putIReg64orZR(tt, unop(Iop_32Sto64, loadLE(Ity_I32, mkexpr(ea))));
         DIP("ldrsw %s, %s\n", nameIReg64orZR(tt), dis_buf);
         return True;
      }
      else
      if (szLg2 == 1) {
         if (sxTo64) {
            putIReg64orZR(tt, unop(Iop_16Sto64, loadLE(Ity_I16, mkexpr(ea))));
            DIP("ldrsh %s, %s\n", nameIReg64orZR(tt), dis_buf);
         } else {
            putIReg32orZR(tt, unop(Iop_16Sto32, loadLE(Ity_I16, mkexpr(ea))));
            DIP("ldrsh %s, %s\n", nameIReg32orZR(tt), dis_buf);
         }
         return True;
      }
      else
      if (szLg2 == 0) {
         if (sxTo64) {
            putIReg64orZR(tt, unop(Iop_8Sto64, loadLE(Ity_I8, mkexpr(ea))));
            DIP("ldrsb %s, %s\n", nameIReg64orZR(tt), dis_buf);
         } else {
            putIReg32orZR(tt, unop(Iop_8Sto32, loadLE(Ity_I8, mkexpr(ea))));
            DIP("ldrsb %s, %s\n", nameIReg32orZR(tt), dis_buf);
         }
         return True;
      }
      
   }
  after_LDRS_integer_register:

   
   if (INSN(29,24) == BITS6(1,1,1,1,0,1)
       && ((INSN(23,23) << 2) | INSN(31,30)) <= 4) {
      UInt   szLg2  = (INSN(23,23) << 2) | INSN(31,30);
      Bool   isLD   = INSN(22,22) == 1;
      UInt   pimm12 = INSN(21,10) << szLg2;
      UInt   nn     = INSN(9,5);
      UInt   tt     = INSN(4,0);
      IRTemp tEA    = newTemp(Ity_I64);
      IRType ty     = preferredVectorSubTypeFromSize(1 << szLg2);
      assign(tEA, binop(Iop_Add64, getIReg64orSP(nn), mkU64(pimm12)));
      if (isLD) {
         if (szLg2 < 4) {
            putQReg128(tt, mkV128(0x0000));
         }
         putQRegLO(tt, loadLE(ty, mkexpr(tEA)));
      } else {
         storeLE(mkexpr(tEA), getQRegLO(tt, ty));
      }
      DIP("%s %s, [%s, #%u]\n",
          isLD ? "ldr" : "str",
          nameQRegLO(tt, ty), nameIReg64orSP(nn), pimm12);
      return True;
   }

   
   if (INSN(29,24) == BITS6(1,1,1,1,0,0)
       && ((INSN(23,23) << 2) | INSN(31,30)) <= 4
       && INSN(21,21) == 0 && INSN(10,10) == 1) {
      UInt   szLg2  = (INSN(23,23) << 2) | INSN(31,30);
      Bool   isLD   = INSN(22,22) == 1;
      UInt   imm9   = INSN(20,12);
      Bool   atRN   = INSN(11,11) == 0;
      UInt   nn     = INSN(9,5);
      UInt   tt     = INSN(4,0);
      IRTemp tRN    = newTemp(Ity_I64);
      IRTemp tEA    = newTemp(Ity_I64);
      IRTemp tTA    = IRTemp_INVALID;
      IRType ty     = preferredVectorSubTypeFromSize(1 << szLg2);
      ULong  simm9  = sx_to_64(imm9, 9);
      assign(tRN, getIReg64orSP(nn));
      assign(tEA, binop(Iop_Add64, mkexpr(tRN), mkU64(simm9)));
      tTA = atRN ? tRN : tEA;
      if (isLD) {
         if (szLg2 < 4) {
            putQReg128(tt, mkV128(0x0000));
         }
         putQRegLO(tt, loadLE(ty, mkexpr(tTA)));
      } else {
         storeLE(mkexpr(tTA), getQRegLO(tt, ty));
      }
      putIReg64orSP(nn, mkexpr(tEA));
      DIP(atRN ? "%s %s, [%s], #%lld\n" : "%s %s, [%s, #%lld]!\n",
          isLD ? "ldr" : "str",
          nameQRegLO(tt, ty), nameIReg64orSP(nn), simm9);
      return True;
   }

   
   if (INSN(29,24) == BITS6(1,1,1,1,0,0)
       && ((INSN(23,23) << 2) | INSN(31,30)) <= 4
       && INSN(21,21) == 0 && INSN(11,10) == BITS2(0,0)) {
      UInt   szLg2  = (INSN(23,23) << 2) | INSN(31,30);
      Bool   isLD   = INSN(22,22) == 1;
      UInt   imm9   = INSN(20,12);
      UInt   nn     = INSN(9,5);
      UInt   tt     = INSN(4,0);
      ULong  simm9  = sx_to_64(imm9, 9);
      IRTemp tEA    = newTemp(Ity_I64);
      IRType ty     = preferredVectorSubTypeFromSize(1 << szLg2);
      assign(tEA, binop(Iop_Add64, getIReg64orSP(nn), mkU64(simm9)));
      if (isLD) {
         if (szLg2 < 4) {
            putQReg128(tt, mkV128(0x0000));
         }
         putQRegLO(tt, loadLE(ty, mkexpr(tEA)));
      } else {
         storeLE(mkexpr(tEA), getQRegLO(tt, ty));
      }
      DIP("%s %s, [%s, #%lld]\n",
          isLD ? "ldur" : "stur",
          nameQRegLO(tt, ty), nameIReg64orSP(nn), (Long)simm9);
      return True;
   }

   
   if (INSN(29,24) == BITS6(0,1,1,1,0,0) && INSN(31,30) < BITS2(1,1)) {
      UInt   szB   = 4 << INSN(31,30);
      UInt   imm19 = INSN(23,5);
      UInt   tt    = INSN(4,0);
      ULong  ea    = guest_PC_curr_instr + sx_to_64(imm19 << 2, 21);
      IRType ty    = preferredVectorSubTypeFromSize(szB);
      putQReg128(tt, mkV128(0x0000));
      putQRegLO(tt, loadLE(ty, mkU64(ea)));
      DIP("ldr %s, 0x%llx (literal)\n", nameQRegLO(tt, ty), ea);
      return True;
   }

   
   
   
   
   if (INSN(31,31) == 0 && INSN(29,24) == BITS6(0,0,1,1,0,0)
       && INSN(21,21) == 0) {
      Bool bitQ  = INSN(30,30);
      Bool isPX  = INSN(23,23) == 1;
      Bool isLD  = INSN(22,22) == 1;
      UInt mm    = INSN(20,16);
      UInt opc   = INSN(15,12);
      UInt sz    = INSN(11,10);
      UInt nn    = INSN(9,5);
      UInt tt    = INSN(4,0);
      Bool isQ   = bitQ == 1;
      Bool is1d  = sz == BITS2(1,1) && !isQ;
      UInt nRegs = 0;
      switch (opc) {
         case BITS4(0,0,0,0): nRegs = 4; break;
         case BITS4(0,1,0,0): nRegs = 3; break;
         case BITS4(1,0,0,0): nRegs = 2; break;
         case BITS4(0,1,1,1): nRegs = 1; break;
         default: break;
      }

      if (!isPX && mm != 0)
         nRegs = 0;
      
      if (nRegs == 1                             
          || (nRegs >= 2 && nRegs <= 4 && !is1d) ) {

         UInt xferSzB = (isQ ? 16 : 8) * nRegs;

         IRTemp tTA = newTemp(Ity_I64);
         assign(tTA, getIReg64orSP(nn));
         if (nn == 31) {  }
         IRTemp tWB = IRTemp_INVALID;
         if (isPX) {
            tWB = newTemp(Ity_I64);
            assign(tWB, binop(Iop_Add64,
                              mkexpr(tTA), 
                              mm == BITS5(1,1,1,1,1) ? mkU64(xferSzB)
                                                     : getIReg64orZR(mm)));
         }

         

         IRTemp u0, u1, u2, u3, i0, i1, i2, i3;
         u0 = u1 = u2 = u3 = i0 = i1 = i2 = i3 = IRTemp_INVALID;
         switch (nRegs) {
            case 4: u3 = newTempV128(); i3 = newTempV128(); 
            case 3: u2 = newTempV128(); i2 = newTempV128(); 
            case 2: u1 = newTempV128(); i1 = newTempV128(); 
            case 1: u0 = newTempV128(); i0 = newTempV128(); break;
            default: vassert(0);
         }

         
         if (!isLD) {
            switch (nRegs) {
               case 4: assign(u3, getQReg128((tt+3) % 32)); 
               case 3: assign(u2, getQReg128((tt+2) % 32)); 
               case 2: assign(u1, getQReg128((tt+1) % 32)); 
               case 1: assign(u0, getQReg128((tt+0) % 32)); break;
               default: vassert(0);
            }
            switch (nRegs) {
               case 4:  (isQ ? math_INTERLEAVE4_128 : math_INTERLEAVE4_64)
                           (&i0, &i1, &i2, &i3, sz, u0, u1, u2, u3);
                        break;
               case 3:  (isQ ? math_INTERLEAVE3_128 : math_INTERLEAVE3_64)
                           (&i0, &i1, &i2, sz, u0, u1, u2);
                        break;
               case 2:  (isQ ? math_INTERLEAVE2_128 : math_INTERLEAVE2_64)
                           (&i0, &i1, sz, u0, u1);
                        break;
               case 1:  (isQ ? math_INTERLEAVE1_128 : math_INTERLEAVE1_64)
                           (&i0, sz, u0);
                        break;
               default: vassert(0);
            }
#           define MAYBE_NARROW_TO_64(_expr) \
                      (isQ ? (_expr) : unop(Iop_V128to64,(_expr)))
            UInt step = isQ ? 16 : 8;
            switch (nRegs) {
               case 4:  storeLE( binop(Iop_Add64, mkexpr(tTA), mkU64(3*step)),
                                 MAYBE_NARROW_TO_64(mkexpr(i3)) );
                        
               case 3:  storeLE( binop(Iop_Add64, mkexpr(tTA), mkU64(2*step)),
                                 MAYBE_NARROW_TO_64(mkexpr(i2)) );
                        
               case 2:  storeLE( binop(Iop_Add64, mkexpr(tTA), mkU64(1*step)),
                                 MAYBE_NARROW_TO_64(mkexpr(i1)) );
                        
               case 1:  storeLE( binop(Iop_Add64, mkexpr(tTA), mkU64(0*step)),
                                 MAYBE_NARROW_TO_64(mkexpr(i0)) );
                        break;
               default: vassert(0);
            }
#           undef MAYBE_NARROW_TO_64
         }

         
         else  {
            UInt   step   = isQ ? 16 : 8;
            IRType loadTy = isQ ? Ity_V128 : Ity_I64;
#           define MAYBE_WIDEN_FROM_64(_expr) \
                      (isQ ? (_expr) : unop(Iop_64UtoV128,(_expr)))
            switch (nRegs) {
               case 4:
                  assign(i3, MAYBE_WIDEN_FROM_64(
                                loadLE(loadTy,
                                       binop(Iop_Add64, mkexpr(tTA),
                                                        mkU64(3 * step)))));
                  
               case 3:
                  assign(i2, MAYBE_WIDEN_FROM_64(
                                loadLE(loadTy,
                                       binop(Iop_Add64, mkexpr(tTA),
                                                        mkU64(2 * step)))));
                  
               case 2:
                  assign(i1, MAYBE_WIDEN_FROM_64(
                                loadLE(loadTy,
                                       binop(Iop_Add64, mkexpr(tTA),
                                                        mkU64(1 * step)))));
                  
               case 1:
                  assign(i0, MAYBE_WIDEN_FROM_64(
                                loadLE(loadTy,
                                       binop(Iop_Add64, mkexpr(tTA),
                                                        mkU64(0 * step)))));
                  break;
               default:
                  vassert(0);
            }
#           undef MAYBE_WIDEN_FROM_64
            switch (nRegs) {
               case 4:  (isQ ? math_DEINTERLEAVE4_128 : math_DEINTERLEAVE4_64)
                           (&u0, &u1, &u2, &u3, sz, i0,i1,i2,i3);
                        break;
               case 3:  (isQ ? math_DEINTERLEAVE3_128 : math_DEINTERLEAVE3_64)
                           (&u0, &u1, &u2, sz, i0, i1, i2);
                        break;
               case 2:  (isQ ? math_DEINTERLEAVE2_128 : math_DEINTERLEAVE2_64)
                           (&u0, &u1, sz, i0, i1);
                        break;
               case 1:  (isQ ? math_DEINTERLEAVE1_128 : math_DEINTERLEAVE1_64)
                           (&u0, sz, i0);
                        break;
               default: vassert(0);
            }
            switch (nRegs) {
               case 4:  putQReg128( (tt+3) % 32,
                                    math_MAYBE_ZERO_HI64(bitQ, u3));
                        
               case 3:  putQReg128( (tt+2) % 32,
                                    math_MAYBE_ZERO_HI64(bitQ, u2));
                        
               case 2:  putQReg128( (tt+1) % 32,
                                    math_MAYBE_ZERO_HI64(bitQ, u1));
                        
               case 1:  putQReg128( (tt+0) % 32,
                                    math_MAYBE_ZERO_HI64(bitQ, u0));
                        break;
               default: vassert(0);
            }
         }

         

         
         if (isPX) {
            putIReg64orSP(nn, mkexpr(tWB));
         }            

         HChar pxStr[20];
         pxStr[0] = pxStr[sizeof(pxStr)-1] = 0;
         if (isPX) {
            if (mm == BITS5(1,1,1,1,1))
               vex_sprintf(pxStr, ", #%u", xferSzB);
            else
               vex_sprintf(pxStr, ", %s", nameIReg64orZR(mm));
         }
         const HChar* arr = nameArr_Q_SZ(bitQ, sz);
         DIP("%s%u {v%u.%s .. v%u.%s}, [%s]%s\n",
             isLD ? "ld" : "st", nRegs,
             (tt+0) % 32, arr, (tt+nRegs-1) % 32, arr, nameIReg64orSP(nn),
             pxStr);

         return True;
      }
      
   }

   
   
   
   if (INSN(31,31) == 0 && INSN(29,24) == BITS6(0,0,1,1,0,0)
       && INSN(21,21) == 0) {
      Bool bitQ  = INSN(30,30);
      Bool isPX  = INSN(23,23) == 1;
      Bool isLD  = INSN(22,22) == 1;
      UInt mm    = INSN(20,16);
      UInt opc   = INSN(15,12);
      UInt sz    = INSN(11,10);
      UInt nn    = INSN(9,5);
      UInt tt    = INSN(4,0);
      Bool isQ   = bitQ == 1;
      UInt nRegs = 0;
      switch (opc) {
         case BITS4(0,0,1,0): nRegs = 4; break;
         case BITS4(0,1,1,0): nRegs = 3; break;
         case BITS4(1,0,1,0): nRegs = 2; break;
         default: break;
      }
      
      if (!isPX && mm != 0)
         nRegs = 0;
      
      if (nRegs >= 2 && nRegs <= 4) {

         UInt xferSzB = (isQ ? 16 : 8) * nRegs;

         IRTemp tTA = newTemp(Ity_I64);
         assign(tTA, getIReg64orSP(nn));
         if (nn == 31) {  }
         IRTemp tWB = IRTemp_INVALID;
         if (isPX) {
            tWB = newTemp(Ity_I64);
            assign(tWB, binop(Iop_Add64,
                              mkexpr(tTA), 
                              mm == BITS5(1,1,1,1,1) ? mkU64(xferSzB)
                                                     : getIReg64orZR(mm)));
         }

         

         IRTemp u0, u1, u2, u3;
         u0 = u1 = u2 = u3 = IRTemp_INVALID;
         switch (nRegs) {
            case 4: u3 = newTempV128(); 
            case 3: u2 = newTempV128(); 
            case 2: u1 = newTempV128();
                    u0 = newTempV128(); break;
            default: vassert(0);
         }

         
         if (!isLD) {
            switch (nRegs) {
               case 4: assign(u3, getQReg128((tt+3) % 32)); 
               case 3: assign(u2, getQReg128((tt+2) % 32)); 
               case 2: assign(u1, getQReg128((tt+1) % 32));
                       assign(u0, getQReg128((tt+0) % 32)); break;
               default: vassert(0);
            }
#           define MAYBE_NARROW_TO_64(_expr) \
                      (isQ ? (_expr) : unop(Iop_V128to64,(_expr)))
            UInt step = isQ ? 16 : 8;
            switch (nRegs) {
               case 4:  storeLE( binop(Iop_Add64, mkexpr(tTA), mkU64(3*step)),
                                 MAYBE_NARROW_TO_64(mkexpr(u3)) );
                        
               case 3:  storeLE( binop(Iop_Add64, mkexpr(tTA), mkU64(2*step)),
                                 MAYBE_NARROW_TO_64(mkexpr(u2)) );
                        
               case 2:  storeLE( binop(Iop_Add64, mkexpr(tTA), mkU64(1*step)),
                                 MAYBE_NARROW_TO_64(mkexpr(u1)) );
                        storeLE( binop(Iop_Add64, mkexpr(tTA), mkU64(0*step)),
                                 MAYBE_NARROW_TO_64(mkexpr(u0)) );
                        break;
               default: vassert(0);
            }
#           undef MAYBE_NARROW_TO_64
         }

         
         else  {
            UInt   step   = isQ ? 16 : 8;
            IRType loadTy = isQ ? Ity_V128 : Ity_I64;
#           define MAYBE_WIDEN_FROM_64(_expr) \
                      (isQ ? (_expr) : unop(Iop_64UtoV128,(_expr)))
            switch (nRegs) {
               case 4:
                  assign(u3, MAYBE_WIDEN_FROM_64(
                                loadLE(loadTy,
                                       binop(Iop_Add64, mkexpr(tTA),
                                                        mkU64(3 * step)))));
                  
               case 3:
                  assign(u2, MAYBE_WIDEN_FROM_64(
                                loadLE(loadTy,
                                       binop(Iop_Add64, mkexpr(tTA),
                                                        mkU64(2 * step)))));
                  
               case 2:
                  assign(u1, MAYBE_WIDEN_FROM_64(
                                loadLE(loadTy,
                                       binop(Iop_Add64, mkexpr(tTA),
                                                        mkU64(1 * step)))));
                  assign(u0, MAYBE_WIDEN_FROM_64(
                                loadLE(loadTy,
                                       binop(Iop_Add64, mkexpr(tTA),
                                                        mkU64(0 * step)))));
                  break;
               default:
                  vassert(0);
            }
#           undef MAYBE_WIDEN_FROM_64
            switch (nRegs) {
               case 4:  putQReg128( (tt+3) % 32,
                                    math_MAYBE_ZERO_HI64(bitQ, u3));
                        
               case 3:  putQReg128( (tt+2) % 32,
                                    math_MAYBE_ZERO_HI64(bitQ, u2));
                        
               case 2:  putQReg128( (tt+1) % 32,
                                    math_MAYBE_ZERO_HI64(bitQ, u1));
                        putQReg128( (tt+0) % 32,
                                    math_MAYBE_ZERO_HI64(bitQ, u0));
                        break;
               default: vassert(0);
            }
         }

         

         
         if (isPX) {
            putIReg64orSP(nn, mkexpr(tWB));
         }            

         HChar pxStr[20];
         pxStr[0] = pxStr[sizeof(pxStr)-1] = 0;
         if (isPX) {
            if (mm == BITS5(1,1,1,1,1))
               vex_sprintf(pxStr, ", #%u", xferSzB);
            else
               vex_sprintf(pxStr, ", %s", nameIReg64orZR(mm));
         }
         const HChar* arr = nameArr_Q_SZ(bitQ, sz);
         DIP("%s1 {v%u.%s .. v%u.%s}, [%s]%s\n",
             isLD ? "ld" : "st",
             (tt+0) % 32, arr, (tt+nRegs-1) % 32, arr, nameIReg64orSP(nn),
             pxStr);

         return True;
      }
      
   }

   
   
   
   
   if (INSN(31,31) == 0 && INSN(29,24) == BITS6(0,0,1,1,0,1)
       && INSN(22,22) == 1 && INSN(15,14) == BITS2(1,1)
       && INSN(12,12) == 0) {
      UInt   bitQ  = INSN(30,30);
      Bool   isPX  = INSN(23,23) == 1;
      UInt   nRegs = ((INSN(13,13) << 1) | INSN(21,21)) + 1;
      UInt   mm    = INSN(20,16);
      UInt   sz    = INSN(11,10);
      UInt   nn    = INSN(9,5);
      UInt   tt    = INSN(4,0);

      
      if (isPX || mm == 0) {

         IRType ty    = integerIRTypeOfSize(1 << sz);

         UInt laneSzB = 1 << sz;
         UInt xferSzB = laneSzB * nRegs;

         IRTemp tTA = newTemp(Ity_I64);
         assign(tTA, getIReg64orSP(nn));
         if (nn == 31) {  }
         IRTemp tWB = IRTemp_INVALID;
         if (isPX) {
            tWB = newTemp(Ity_I64);
            assign(tWB, binop(Iop_Add64,
                              mkexpr(tTA), 
                              mm == BITS5(1,1,1,1,1) ? mkU64(xferSzB)
                                                     : getIReg64orZR(mm)));
         }

         
         if (isPX) {
            putIReg64orSP(nn, mkexpr(tWB));
         }            

         IRTemp e0, e1, e2, e3, v0, v1, v2, v3;
         e0 = e1 = e2 = e3 = v0 = v1 = v2 = v3 = IRTemp_INVALID;
         switch (nRegs) {
            case 4:
               e3 = newTemp(ty);
               assign(e3, loadLE(ty, binop(Iop_Add64, mkexpr(tTA),
                                                      mkU64(3 * laneSzB))));
               v3 = math_DUP_TO_V128(e3, ty);
               putQReg128((tt+3) % 32, math_MAYBE_ZERO_HI64(bitQ, v3));
               
            case 3:
               e2 = newTemp(ty);
               assign(e2, loadLE(ty, binop(Iop_Add64, mkexpr(tTA),
                                                      mkU64(2 * laneSzB))));
               v2 = math_DUP_TO_V128(e2, ty);
               putQReg128((tt+2) % 32, math_MAYBE_ZERO_HI64(bitQ, v2));
               
            case 2:
               e1 = newTemp(ty);
               assign(e1, loadLE(ty, binop(Iop_Add64, mkexpr(tTA),
                                                      mkU64(1 * laneSzB))));
               v1 = math_DUP_TO_V128(e1, ty);
               putQReg128((tt+1) % 32, math_MAYBE_ZERO_HI64(bitQ, v1));
               
            case 1:
               e0 = newTemp(ty);
               assign(e0, loadLE(ty, binop(Iop_Add64, mkexpr(tTA),
                                                      mkU64(0 * laneSzB))));
               v0 = math_DUP_TO_V128(e0, ty);
               putQReg128((tt+0) % 32, math_MAYBE_ZERO_HI64(bitQ, v0));
               break;
            default:
               vassert(0);
         }

         HChar pxStr[20];
         pxStr[0] = pxStr[sizeof(pxStr)-1] = 0;
         if (isPX) {
            if (mm == BITS5(1,1,1,1,1))
               vex_sprintf(pxStr, ", #%u", xferSzB);
            else
               vex_sprintf(pxStr, ", %s", nameIReg64orZR(mm));
         }
         const HChar* arr = nameArr_Q_SZ(bitQ, sz);
         DIP("ld%ur {v%u.%s .. v%u.%s}, [%s]%s\n",
             nRegs,
             (tt+0) % 32, arr, (tt+nRegs-1) % 32, arr, nameIReg64orSP(nn),
             pxStr);

         return True;
      }
      
   }

   
   
   
   
   if (INSN(31,31) == 0 && INSN(29,24) == BITS6(0,0,1,1,0,1)) {
      UInt   bitQ  = INSN(30,30);
      Bool   isPX  = INSN(23,23) == 1;
      Bool   isLD  = INSN(22,22) == 1;
      UInt   nRegs = ((INSN(13,13) << 1) | INSN(21,21)) + 1;
      UInt   mm    = INSN(20,16);
      UInt   xx    = INSN(15,14);
      UInt   bitS  = INSN(12,12);
      UInt   sz    = INSN(11,10);
      UInt   nn    = INSN(9,5);
      UInt   tt    = INSN(4,0);

      Bool valid = True;

      
      if (!isPX && mm != 0)
         valid = False;

      UInt laneSzB = 0;  
      UInt ix      = 16; 

      UInt xx_q_S_sz = (xx << 4) | (bitQ << 3) | (bitS << 2) | sz;
      switch (xx_q_S_sz) {
         case 0x00: case 0x01: case 0x02: case 0x03:
         case 0x04: case 0x05: case 0x06: case 0x07:
         case 0x08: case 0x09: case 0x0A: case 0x0B:
         case 0x0C: case 0x0D: case 0x0E: case 0x0F:
            laneSzB = 1; ix = xx_q_S_sz & 0xF;
            break;
         case 0x10: case 0x12: case 0x14: case 0x16:
         case 0x18: case 0x1A: case 0x1C: case 0x1E:
            laneSzB = 2; ix = (xx_q_S_sz >> 1) & 7;
            break;
         case 0x20: case 0x24: case 0x28: case 0x2C:
            laneSzB = 4; ix = (xx_q_S_sz >> 2) & 3;
            break;
         case 0x21: case 0x29:
            laneSzB = 8; ix = (xx_q_S_sz >> 3) & 1;
            break;
         default:
            break;
      }

      if (valid && laneSzB != 0) {

         IRType ty      = integerIRTypeOfSize(laneSzB);
         UInt   xferSzB = laneSzB * nRegs;

         IRTemp tTA = newTemp(Ity_I64);
         assign(tTA, getIReg64orSP(nn));
         if (nn == 31) {  }
         IRTemp tWB = IRTemp_INVALID;
         if (isPX) {
            tWB = newTemp(Ity_I64);
            assign(tWB, binop(Iop_Add64,
                              mkexpr(tTA), 
                              mm == BITS5(1,1,1,1,1) ? mkU64(xferSzB)
                                                     : getIReg64orZR(mm)));
         }

         
         if (isPX) {
            putIReg64orSP(nn, mkexpr(tWB));
         }            

         switch (nRegs) {
            case 4: {
               IRExpr* addr
                  = binop(Iop_Add64, mkexpr(tTA), mkU64(3 * laneSzB));
               if (isLD) {
                  putQRegLane((tt+3) % 32, ix, loadLE(ty, addr));
               } else {
                  storeLE(addr, getQRegLane((tt+3) % 32, ix, ty));
               }
               
            }
            case 3: {
               IRExpr* addr
                  = binop(Iop_Add64, mkexpr(tTA), mkU64(2 * laneSzB));
               if (isLD) {
                  putQRegLane((tt+2) % 32, ix, loadLE(ty, addr));
               } else {
                  storeLE(addr, getQRegLane((tt+2) % 32, ix, ty));
               }
               
            }
            case 2: {
               IRExpr* addr
                  = binop(Iop_Add64, mkexpr(tTA), mkU64(1 * laneSzB));
               if (isLD) {
                  putQRegLane((tt+1) % 32, ix, loadLE(ty, addr));
               } else {
                  storeLE(addr, getQRegLane((tt+1) % 32, ix, ty));
               }
               
            }
            case 1: {
               IRExpr* addr
                  = binop(Iop_Add64, mkexpr(tTA), mkU64(0 * laneSzB));
               if (isLD) {
                  putQRegLane((tt+0) % 32, ix, loadLE(ty, addr));
               } else {
                  storeLE(addr, getQRegLane((tt+0) % 32, ix, ty));
               }
               break;
            }
            default:
               vassert(0);
         }

         HChar pxStr[20];
         pxStr[0] = pxStr[sizeof(pxStr)-1] = 0;
         if (isPX) {
            if (mm == BITS5(1,1,1,1,1))
               vex_sprintf(pxStr, ", #%u", xferSzB);
            else
               vex_sprintf(pxStr, ", %s", nameIReg64orZR(mm));
         }
         const HChar* arr = nameArr_Q_SZ(bitQ, sz);
         DIP("%s%u {v%u.%s .. v%u.%s}[%u], [%s]%s\n",
             isLD ? "ld" : "st", nRegs,
             (tt+0) % 32, arr, (tt+nRegs-1) % 32, arr, 
             ix, nameIReg64orSP(nn), pxStr);

         return True;
      }
      
   }

   
   
   if (INSN(29,23) == BITS7(0,0,1,0,0,0,0)
       && (INSN(23,21) & BITS3(1,0,1)) == BITS3(0,0,0)
       && INSN(14,10) == BITS5(1,1,1,1,1)) {
      UInt szBlg2     = INSN(31,30);
      Bool isLD       = INSN(22,22) == 1;
      Bool isAcqOrRel = INSN(15,15) == 1;
      UInt ss         = INSN(20,16);
      UInt nn         = INSN(9,5);
      UInt tt         = INSN(4,0);

      vassert(szBlg2 < 4);
      UInt   szB = 1 << szBlg2; 
      IRType ty  = integerIRTypeOfSize(szB);
      const HChar* suffix[4] = { "rb", "rh", "r", "r" };

      IRTemp ea = newTemp(Ity_I64);
      assign(ea, getIReg64orSP(nn));
      

      if (isLD && ss == BITS5(1,1,1,1,1)) {
         IRTemp res = newTemp(ty);
         stmt(IRStmt_LLSC(Iend_LE, res, mkexpr(ea), NULL));
         putIReg64orZR(tt, widenUto64(ty, mkexpr(res)));
         if (isAcqOrRel) {
            stmt(IRStmt_MBE(Imbe_Fence));
         }
         DIP("ld%sx%s %s, [%s]\n", isAcqOrRel ? "a" : "", suffix[szBlg2],
             nameIRegOrZR(szB == 8, tt), nameIReg64orSP(nn));
         return True;
      }
      if (!isLD) {
         if (isAcqOrRel) {
            stmt(IRStmt_MBE(Imbe_Fence));
         }
         IRTemp  res  = newTemp(Ity_I1);
         IRExpr* data = narrowFrom64(ty, getIReg64orZR(tt));
         stmt(IRStmt_LLSC(Iend_LE, res, mkexpr(ea), data));
         putIReg64orZR(ss, binop(Iop_Xor64, unop(Iop_1Uto64, mkexpr(res)),
                                            mkU64(1)));
         DIP("st%sx%s %s, %s, [%s]\n", isAcqOrRel ? "a" : "", suffix[szBlg2],
             nameIRegOrZR(False, ss),
             nameIRegOrZR(szB == 8, tt), nameIReg64orSP(nn));
         return True;
      }
      
   }

   
   
   if (INSN(29,23) == BITS7(0,0,1,0,0,0,1)
       && INSN(21,10) == BITS12(0,1,1,1,1,1,1,1,1,1,1,1)) {
      UInt szBlg2 = INSN(31,30);
      Bool isLD   = INSN(22,22) == 1;
      UInt nn     = INSN(9,5);
      UInt tt     = INSN(4,0);

      vassert(szBlg2 < 4);
      UInt   szB = 1 << szBlg2; 
      IRType ty  = integerIRTypeOfSize(szB);
      const HChar* suffix[4] = { "rb", "rh", "r", "r" };

      IRTemp ea = newTemp(Ity_I64);
      assign(ea, getIReg64orSP(nn));
      

      if (isLD) {
         IRTemp res = newTemp(ty);
         assign(res, loadLE(ty, mkexpr(ea)));
         putIReg64orZR(tt, widenUto64(ty, mkexpr(res)));
         stmt(IRStmt_MBE(Imbe_Fence));
         DIP("lda%s %s, [%s]\n", suffix[szBlg2],
             nameIRegOrZR(szB == 8, tt), nameIReg64orSP(nn));
      } else {
         stmt(IRStmt_MBE(Imbe_Fence));
         IRExpr* data = narrowFrom64(ty, getIReg64orZR(tt));
         storeLE(mkexpr(ea), data);
         DIP("stl%s %s, [%s]\n", suffix[szBlg2],
             nameIRegOrZR(szB == 8, tt), nameIReg64orSP(nn));
      }
      return True;
   }

   
   if (INSN(31,22) == BITS10(1,1,1,1,1,0,0,1,1,0)) {
      UInt imm12 = INSN(21,10);
      UInt nn    = INSN(9,5);
      UInt tt    = INSN(4,0);
      IRTemp ea = newTemp(Ity_I64);
      assign(ea, binop(Iop_Add64, getIReg64orSP(nn), mkU64(imm12 * 8)));
      DIP("prfm prfop=%u, [%s, #%u]\n", tt, nameIReg64orSP(nn), imm12 * 8);
      return True;
   }

   vex_printf("ARM64 front end: load_store\n");
   return False;
#  undef INSN
}



static
Bool dis_ARM64_branch_etc(DisResult* dres, UInt insn,
                          const VexArchInfo* archinfo)
{
#  define INSN(_bMax,_bMin)  SLICE_UInt(insn, (_bMax), (_bMin))

   
   if (INSN(31,24) == BITS8(0,1,0,1,0,1,0,0) && INSN(4,4) == 0) {
      UInt  cond   = INSN(3,0);
      ULong uimm64 = INSN(23,5) << 2;
      Long  simm64 = (Long)sx_to_64(uimm64, 21);
      vassert(dres->whatNext    == Dis_Continue);
      vassert(dres->len         == 4);
      vassert(dres->continueAt  == 0);
      vassert(dres->jk_StopHere == Ijk_INVALID);
      stmt( IRStmt_Exit(unop(Iop_64to1, mk_arm64g_calculate_condition(cond)),
                        Ijk_Boring,
                        IRConst_U64(guest_PC_curr_instr + simm64),
                        OFFB_PC) );
      putPC(mkU64(guest_PC_curr_instr + 4));
      dres->whatNext    = Dis_StopHere;
      dres->jk_StopHere = Ijk_Boring;
      DIP("b.%s 0x%llx\n", nameCC(cond), guest_PC_curr_instr + simm64);
      return True;
   }

   
   if (INSN(30,26) == BITS5(0,0,1,0,1)) {
      UInt  bLink  = INSN(31,31);
      ULong uimm64 = INSN(25,0) << 2;
      Long  simm64 = (Long)sx_to_64(uimm64, 28);
      if (bLink) {
         putIReg64orSP(30, mkU64(guest_PC_curr_instr + 4));
      }
      putPC(mkU64(guest_PC_curr_instr + simm64));
      dres->whatNext = Dis_StopHere;
      dres->jk_StopHere = Ijk_Call;
      DIP("b%s 0x%llx\n", bLink == 1 ? "l" : "",
                          guest_PC_curr_instr + simm64);
      return True;
   }

   
   if (INSN(31,23) == BITS9(1,1,0,1,0,1,1,0,0)
       && INSN(20,16) == BITS5(1,1,1,1,1)
       && INSN(15,10) == BITS6(0,0,0,0,0,0)
       && INSN(4,0) == BITS5(0,0,0,0,0)) {
      UInt branch_type = INSN(22,21);
      UInt nn          = INSN(9,5);
      if (branch_type == BITS2(1,0) ) {
         putPC(getIReg64orZR(nn));
         dres->whatNext = Dis_StopHere;
         dres->jk_StopHere = Ijk_Ret;
         DIP("ret %s\n", nameIReg64orZR(nn));
         return True;
      }
      if (branch_type == BITS2(0,1) ) {
         IRTemp dst = newTemp(Ity_I64);
         assign(dst, getIReg64orZR(nn));
         putIReg64orSP(30, mkU64(guest_PC_curr_instr + 4));
         putPC(mkexpr(dst));
         dres->whatNext = Dis_StopHere;
         dres->jk_StopHere = Ijk_Call;
         DIP("blr %s\n", nameIReg64orZR(nn));
         return True;
      }
      if (branch_type == BITS2(0,0) ) {
         putPC(getIReg64orZR(nn));
         dres->whatNext = Dis_StopHere;
         dres->jk_StopHere = Ijk_Boring;
         DIP("jmp %s\n", nameIReg64orZR(nn));
         return True;
      }
   }

   
   if (INSN(30,25) == BITS6(0,1,1,0,1,0)) {
      Bool    is64   = INSN(31,31) == 1;
      Bool    bIfZ   = INSN(24,24) == 0;
      ULong   uimm64 = INSN(23,5) << 2;
      UInt    rT     = INSN(4,0);
      Long    simm64 = (Long)sx_to_64(uimm64, 21);
      IRExpr* cond   = NULL;
      if (is64) {
         cond = binop(bIfZ ? Iop_CmpEQ64 : Iop_CmpNE64,
                      getIReg64orZR(rT), mkU64(0));
      } else {
         cond = binop(bIfZ ? Iop_CmpEQ32 : Iop_CmpNE32,
                      getIReg32orZR(rT), mkU32(0));
      }
      stmt( IRStmt_Exit(cond,
                        Ijk_Boring,
                        IRConst_U64(guest_PC_curr_instr + simm64),
                        OFFB_PC) );
      putPC(mkU64(guest_PC_curr_instr + 4));
      dres->whatNext    = Dis_StopHere;
      dres->jk_StopHere = Ijk_Boring;
      DIP("cb%sz %s, 0x%llx\n",
          bIfZ ? "" : "n", nameIRegOrZR(is64, rT),
          guest_PC_curr_instr + simm64);
      return True;
   }

   
   if (INSN(30,25) == BITS6(0,1,1,0,1,1)) {
      UInt    b5     = INSN(31,31);
      Bool    bIfZ   = INSN(24,24) == 0;
      UInt    b40    = INSN(23,19);
      UInt    imm14  = INSN(18,5);
      UInt    tt     = INSN(4,0);
      UInt    bitNo  = (b5 << 5) | b40;
      ULong   uimm64 = imm14 << 2;
      Long    simm64 = sx_to_64(uimm64, 16);
      IRExpr* cond 
         = binop(bIfZ ? Iop_CmpEQ64 : Iop_CmpNE64,
                 binop(Iop_And64,
                       binop(Iop_Shr64, getIReg64orZR(tt), mkU8(bitNo)),
                       mkU64(1)),
                 mkU64(0));
      stmt( IRStmt_Exit(cond,
                        Ijk_Boring,
                        IRConst_U64(guest_PC_curr_instr + simm64),
                        OFFB_PC) );
      putPC(mkU64(guest_PC_curr_instr + 4));
      dres->whatNext    = Dis_StopHere;
      dres->jk_StopHere = Ijk_Boring;
      DIP("tb%sz %s, #%u, 0x%llx\n",
          bIfZ ? "" : "n", nameIReg64orZR(tt), bitNo,
          guest_PC_curr_instr + simm64);
      return True;
   }

   
   if (INSN(31,0) == 0xD4000001) {
      putPC(mkU64(guest_PC_curr_instr + 4));
      dres->whatNext    = Dis_StopHere;
      dres->jk_StopHere = Ijk_Sys_syscall;
      DIP("svc #0\n");
      return True;
   }

   
   if (   (INSN(31,0) & 0xFFFFFFE0) == 0xD51BD040 
       || (INSN(31,0) & 0xFFFFFFE0) == 0xD53BD040 ) {
      Bool toSys = INSN(21,21) == 0;
      UInt tt    = INSN(4,0);
      if (toSys) {
         stmt( IRStmt_Put( OFFB_TPIDR_EL0, getIReg64orZR(tt)) );
         DIP("msr tpidr_el0, %s\n", nameIReg64orZR(tt));
      } else {
         putIReg64orZR(tt, IRExpr_Get( OFFB_TPIDR_EL0, Ity_I64 ));
         DIP("mrs %s, tpidr_el0\n", nameIReg64orZR(tt));
      }
      return True;
   }
   if (   (INSN(31,0) & 0xFFFFFFE0) == 0xD51B4400 
       || (INSN(31,0) & 0xFFFFFFE0) == 0xD53B4400 ) {
      Bool toSys = INSN(21,21) == 0;
      UInt tt    = INSN(4,0);
      if (toSys) {
         stmt( IRStmt_Put( OFFB_FPCR, getIReg32orZR(tt)) );
         DIP("msr fpcr, %s\n", nameIReg64orZR(tt));
      } else {
         putIReg32orZR(tt, IRExpr_Get(OFFB_FPCR, Ity_I32));
         DIP("mrs %s, fpcr\n", nameIReg64orZR(tt));
      }
      return True;
   }
   if (   (INSN(31,0) & 0xFFFFFFE0) == 0xD51B4420 
       || (INSN(31,0) & 0xFFFFFFE0) == 0xD53B4420 ) {
      Bool toSys = INSN(21,21) == 0;
      UInt tt    = INSN(4,0);
      if (toSys) {
         IRTemp qc64 = newTemp(Ity_I64);
         assign(qc64, binop(Iop_And64,
                            binop(Iop_Shr64, getIReg64orZR(tt), mkU8(27)),
                            mkU64(1)));
         IRExpr* qcV128 = binop(Iop_64HLtoV128, mkexpr(qc64), mkexpr(qc64));
         stmt( IRStmt_Put( OFFB_QCFLAG, qcV128 ) );
         DIP("msr fpsr, %s\n", nameIReg64orZR(tt));
      } else {
         IRTemp qcV128 = newTempV128();
         assign(qcV128, IRExpr_Get( OFFB_QCFLAG, Ity_V128 ));
         IRTemp qc64 = newTemp(Ity_I64);
         assign(qc64, binop(Iop_Or64, unop(Iop_V128HIto64, mkexpr(qcV128)),
                                      unop(Iop_V128to64,   mkexpr(qcV128))));
         IRExpr* res = binop(Iop_Shl64, 
                             unop(Iop_1Uto64,
                                  binop(Iop_CmpNE64, mkexpr(qc64), mkU64(0))),
                             mkU8(27));
         putIReg64orZR(tt, res);
         DIP("mrs %s, fpsr\n", nameIReg64orZR(tt));
      }
      return True;
   }
   if (   (INSN(31,0) & 0xFFFFFFE0) == 0xD51B4200 
       || (INSN(31,0) & 0xFFFFFFE0) == 0xD53B4200 ) {
      Bool  toSys = INSN(21,21) == 0;
      UInt  tt    = INSN(4,0);
      if (toSys) {
         IRTemp t = newTemp(Ity_I64);
         assign(t, binop(Iop_And64, getIReg64orZR(tt), mkU64(0xF0000000ULL)));
         setFlags_COPY(t);
         DIP("msr %s, nzcv\n", nameIReg32orZR(tt));
      } else {
         IRTemp res = newTemp(Ity_I64);
         assign(res, mk_arm64g_calculate_flags_nzcv());
         putIReg32orZR(tt, unop(Iop_64to32, mkexpr(res)));
         DIP("mrs %s, nzcv\n", nameIReg64orZR(tt));
      }
      return True;
   }
   if ((INSN(31,0) & 0xFFFFFFE0) == 0xD53B00E0) {
      UInt tt = INSN(4,0);
      putIReg64orZR(tt, mkU64(1<<4));
      DIP("mrs %s, dczid_el0 (FAKED)\n", nameIReg64orZR(tt));
      return True;
   }
   if ((INSN(31,0) & 0xFFFFFFE0) == 0xD53B0020) {
      UInt tt = INSN(4,0);
      vassert(archinfo->arm64_dMinLine_lg2_szB >= 2
              && archinfo->arm64_dMinLine_lg2_szB <= 17
              && archinfo->arm64_iMinLine_lg2_szB >= 2
              && archinfo->arm64_iMinLine_lg2_szB <= 17);
      UInt val
         = 0x8440c000 | ((0xF & (archinfo->arm64_dMinLine_lg2_szB - 2)) << 16)
                      | ((0xF & (archinfo->arm64_iMinLine_lg2_szB - 2)) << 0);
      putIReg64orZR(tt, mkU64(val));
      DIP("mrs %s, ctr_el0\n", nameIReg64orZR(tt));
      return True;
   }
   if ((INSN(31,0) & 0xFFFFFFE0) == 0xD53BE040) {
      UInt     tt   = INSN(4,0);
      IRTemp   val  = newTemp(Ity_I64);
      IRExpr** args = mkIRExprVec_0();
      IRDirty* d    = unsafeIRDirty_1_N ( 
                         val, 
                         0, 
                         "arm64g_dirtyhelper_MRS_CNTVCT_EL0",
                         &arm64g_dirtyhelper_MRS_CNTVCT_EL0,
                         args 
                      );
      
      stmt( IRStmt_Dirty(d) );
      putIReg64orZR(tt, mkexpr(val));
      DIP("mrs %s, cntvct_el0\n", nameIReg64orZR(tt));
      return True;
   }   

   
   if ((INSN(31,0) & 0xFFFFFFE0) == 0xD50B7520) {
      
      vassert(archinfo->arm64_iMinLine_lg2_szB >= 2
              && archinfo->arm64_iMinLine_lg2_szB <= 17);
      UInt   tt      = INSN(4,0);
      ULong  lineszB = 1ULL << archinfo->arm64_iMinLine_lg2_szB;
      IRTemp addr    = newTemp(Ity_I64);
      assign( addr, binop( Iop_And64,
                           getIReg64orZR(tt),
                           mkU64(~(lineszB - 1))) );
      stmt(IRStmt_Put(OFFB_CMSTART, mkexpr(addr)));
      stmt(IRStmt_Put(OFFB_CMLEN,   mkU64(lineszB)));
      
      stmt( IRStmt_MBE(Imbe_Fence) );
      putPC(mkU64( guest_PC_curr_instr + 4 ));
      dres->whatNext    = Dis_StopHere;
      dres->jk_StopHere = Ijk_InvalICache;
      DIP("ic ivau, %s\n", nameIReg64orZR(tt));
      return True;
   }

   
   if ((INSN(31,0) & 0xFFFFFFE0) == 0xD50B7B20) {
      
      vassert(archinfo->arm64_dMinLine_lg2_szB >= 2
              && archinfo->arm64_dMinLine_lg2_szB <= 17);
      UInt   tt      = INSN(4,0);
      ULong  lineszB = 1ULL << archinfo->arm64_dMinLine_lg2_szB;
      IRTemp addr    = newTemp(Ity_I64);
      assign( addr, binop( Iop_And64,
                           getIReg64orZR(tt),
                           mkU64(~(lineszB - 1))) );
      stmt(IRStmt_Put(OFFB_CMSTART, mkexpr(addr)));
      stmt(IRStmt_Put(OFFB_CMLEN,   mkU64(lineszB)));
      
      stmt( IRStmt_MBE(Imbe_Fence) );
      putPC(mkU64( guest_PC_curr_instr + 4 ));
      dres->whatNext    = Dis_StopHere;
      dres->jk_StopHere = Ijk_FlushDCache;
      DIP("dc cvau, %s\n", nameIReg64orZR(tt));
      return True;
   }

   
   if (INSN(31,22) == BITS10(1,1,0,1,0,1,0,1,0,0)
       && INSN(21,12) == BITS10(0,0,0,0,1,1,0,0,1,1)
       && INSN(7,7) == 1
       && INSN(6,5) <= BITS2(1,0) && INSN(4,0) == BITS5(1,1,1,1,1)) {
      UInt opc = INSN(6,5);
      UInt CRm = INSN(11,8);
      vassert(opc <= 2 && CRm <= 15);
      stmt(IRStmt_MBE(Imbe_Fence));
      const HChar* opNames[3] 
         = { "dsb", "dmb", "isb" };
      const HChar* howNames[16]
         = { "#0", "oshld", "oshst", "osh", "#4", "nshld", "nshst", "nsh",
             "#8", "ishld", "ishst", "ish", "#12", "ld", "st", "sy" };
      DIP("%s %s\n", opNames[opc], howNames[CRm]);
      return True;
   }

   
   if (INSN(31,0) == 0xD503201F) {
      DIP("nop\n");
      return True;
   }

   
   if (INSN(31,24) == BITS8(1,1,0,1,0,1,0,0)
       && INSN(23,21) == BITS3(0,0,1) && INSN(4,0) == BITS5(0,0,0,0,0)) {
      UInt imm16 = INSN(20,5);
      
      putPC(mkU64(guest_PC_curr_instr + 0));
      dres->whatNext    = Dis_StopHere;
      dres->jk_StopHere = Ijk_SigTRAP;
      DIP("brk #%u\n", imm16);
      return True;
   }

  
   vex_printf("ARM64 front end: branch_etc\n");
   return False;
#  undef INSN
}




static IRExpr* mk_CatEvenLanes64x2 ( IRTemp a10, IRTemp b10 ) {
   
   return binop(Iop_InterleaveLO64x2, mkexpr(a10), mkexpr(b10));
}

static IRExpr* mk_CatOddLanes64x2 ( IRTemp a10, IRTemp b10 ) {
   
   return binop(Iop_InterleaveHI64x2, mkexpr(a10), mkexpr(b10));
}

static IRExpr* mk_CatEvenLanes32x4 ( IRTemp a3210, IRTemp b3210 ) {
   
   return binop(Iop_CatEvenLanes32x4, mkexpr(a3210), mkexpr(b3210));
}

static IRExpr* mk_CatOddLanes32x4 ( IRTemp a3210, IRTemp b3210 ) {
   
   return binop(Iop_CatOddLanes32x4, mkexpr(a3210), mkexpr(b3210));
}

static IRExpr* mk_InterleaveLO32x4 ( IRTemp a3210, IRTemp b3210 ) {
   
   return binop(Iop_InterleaveLO32x4, mkexpr(a3210), mkexpr(b3210));
}

static IRExpr* mk_InterleaveHI32x4 ( IRTemp a3210, IRTemp b3210 ) {
   
   return binop(Iop_InterleaveHI32x4, mkexpr(a3210), mkexpr(b3210));
}

static IRExpr* mk_CatEvenLanes16x8 ( IRTemp a76543210, IRTemp b76543210 ) {
   
   return binop(Iop_CatEvenLanes16x8, mkexpr(a76543210), mkexpr(b76543210));
}

static IRExpr* mk_CatOddLanes16x8 ( IRTemp a76543210, IRTemp b76543210 ) {
   
   return binop(Iop_CatOddLanes16x8, mkexpr(a76543210), mkexpr(b76543210));
}

static IRExpr* mk_InterleaveLO16x8 ( IRTemp a76543210, IRTemp b76543210 ) {
   
   return binop(Iop_InterleaveLO16x8, mkexpr(a76543210), mkexpr(b76543210));
}

static IRExpr* mk_InterleaveHI16x8 ( IRTemp a76543210, IRTemp b76543210 ) {
   
   return binop(Iop_InterleaveHI16x8, mkexpr(a76543210), mkexpr(b76543210));
}

static IRExpr* mk_CatEvenLanes8x16 ( IRTemp aFEDCBA9876543210,
                                     IRTemp bFEDCBA9876543210 ) {
   
   return binop(Iop_CatEvenLanes8x16, mkexpr(aFEDCBA9876543210),
                                      mkexpr(bFEDCBA9876543210));
}

static IRExpr* mk_CatOddLanes8x16 ( IRTemp aFEDCBA9876543210,
                                    IRTemp bFEDCBA9876543210 ) {
   
   return binop(Iop_CatOddLanes8x16, mkexpr(aFEDCBA9876543210),
                                     mkexpr(bFEDCBA9876543210));
}

static IRExpr* mk_InterleaveLO8x16 ( IRTemp aFEDCBA9876543210,
                                     IRTemp bFEDCBA9876543210 ) {
   
   return binop(Iop_InterleaveLO8x16, mkexpr(aFEDCBA9876543210),
                                      mkexpr(bFEDCBA9876543210));
}

static IRExpr* mk_InterleaveHI8x16 ( IRTemp aFEDCBA9876543210,
                                     IRTemp bFEDCBA9876543210 ) {
   
   return binop(Iop_InterleaveHI8x16, mkexpr(aFEDCBA9876543210),
                                      mkexpr(bFEDCBA9876543210));
}

static ULong Replicate ( ULong bit, Int N )
{
   vassert(bit <= 1 && N >= 1 && N < 64);
   if (bit == 0) {
      return 0;
    } else {
      
      return (1ULL << N) - 1;
   }
}

static ULong Replicate32x2 ( ULong bits32 )
{
   vassert(0 == (bits32 & ~0xFFFFFFFFULL));
   return (bits32 << 32) | bits32;
}

static ULong Replicate16x4 ( ULong bits16 )
{
   vassert(0 == (bits16 & ~0xFFFFULL));
   return Replicate32x2((bits16 << 16) | bits16);
}

static ULong Replicate8x8 ( ULong bits8 )
{
   vassert(0 == (bits8 & ~0xFFULL));
   return Replicate16x4((bits8 << 8) | bits8);
}

static ULong VFPExpandImm ( ULong imm8, Int N )
{
   vassert(imm8 <= 0xFF);
   vassert(N == 32 || N == 64);
   Int E = ((N == 32) ? 8 : 11) - 2; 
   Int F = N - E - 1;
   ULong imm8_6 = (imm8 >> 6) & 1;
   
   
   
   ULong sign = (imm8 >> 7) & 1;
   ULong exp  = ((imm8_6 ^ 1) << (E-1)) | Replicate(imm8_6, E-1);
   ULong frac = ((imm8 & 63) << (F-6)) | Replicate(0, F-6);
   vassert(sign < (1ULL << 1));
   vassert(exp  < (1ULL << E));
   vassert(frac < (1ULL << F));
   vassert(1 + E + F == N);
   ULong res = (sign << (E+F)) | (exp << F) | frac;
   return res;
}

static Bool AdvSIMDExpandImm ( ULong* res,
                               UInt op, UInt cmode, UInt imm8 )
{
   vassert(op <= 1);
   vassert(cmode <= 15);
   vassert(imm8 <= 255);

   *res = 0; 

   ULong imm64    = 0;
   Bool  testimm8 = False;

   switch (cmode >> 1) {
      case 0:
         testimm8 = False; imm64 = Replicate32x2(imm8); break;
      case 1:
         testimm8 = True; imm64 = Replicate32x2(imm8 << 8); break;
      case 2:
         testimm8 = True; imm64 = Replicate32x2(imm8 << 16); break;
      case 3:
         testimm8 = True; imm64 = Replicate32x2(imm8 << 24); break;
      case 4:
          testimm8 = False; imm64 = Replicate16x4(imm8); break;
      case 5:
          testimm8 = True; imm64 = Replicate16x4(imm8 << 8); break;
      case 6:
          testimm8 = True;
          if ((cmode & 1) == 0)
              imm64 = Replicate32x2((imm8 << 8) | 0xFF);
          else
              imm64 = Replicate32x2((imm8 << 16) | 0xFFFF);
          break;
      case 7:
         testimm8 = False;
         if ((cmode & 1) == 0 && op == 0)
             imm64 = Replicate8x8(imm8);
         if ((cmode & 1) == 0 && op == 1) {
             imm64 = 0;   imm64 |= (imm8 & 0x80) ? 0xFF : 0x00;
             imm64 <<= 8; imm64 |= (imm8 & 0x40) ? 0xFF : 0x00;
             imm64 <<= 8; imm64 |= (imm8 & 0x20) ? 0xFF : 0x00;
             imm64 <<= 8; imm64 |= (imm8 & 0x10) ? 0xFF : 0x00;
             imm64 <<= 8; imm64 |= (imm8 & 0x08) ? 0xFF : 0x00;
             imm64 <<= 8; imm64 |= (imm8 & 0x04) ? 0xFF : 0x00;
             imm64 <<= 8; imm64 |= (imm8 & 0x02) ? 0xFF : 0x00;
             imm64 <<= 8; imm64 |= (imm8 & 0x01) ? 0xFF : 0x00;
         }
         if ((cmode & 1) == 1 && op == 0) {
            ULong imm8_7  = (imm8 >> 7) & 1;
            ULong imm8_6  = (imm8 >> 6) & 1;
            ULong imm8_50 = imm8 & 63;
            ULong imm32 = (imm8_7                 << (1 + 5 + 6 + 19))
                          | ((imm8_6 ^ 1)         << (5 + 6 + 19))
                          | (Replicate(imm8_6, 5) << (6 + 19))
                          | (imm8_50              << 19);
            imm64 = Replicate32x2(imm32);
         }
         if ((cmode & 1) == 1 && op == 1) {
            
            
            ULong imm8_7  = (imm8 >> 7) & 1;
            ULong imm8_6  = (imm8 >> 6) & 1;
            ULong imm8_50 = imm8 & 63;
            imm64 = (imm8_7 << 63) | ((imm8_6 ^ 1) << 62)
                    | (Replicate(imm8_6, 8) << 54)
                    | (imm8_50 << 48);
         }
         break;
      default:
        vassert(0);
   }

   if (testimm8 && imm8 == 0)
      return False;

   *res = imm64;
   return True;
}

static Bool getLaneInfo_Q_SZ ( IRType* tyI,  IRType* tyF,
                               UInt* nLanes, Bool* zeroUpper,
                               const HChar** arrSpec,
                               Bool bitQ, Bool bitSZ )
{
   vassert(bitQ == True || bitQ == False);
   vassert(bitSZ == True || bitSZ == False);
   if (bitQ && bitSZ) { 
      if (tyI)       *tyI       = Ity_I64;
      if (tyF)       *tyF       = Ity_F64;
      if (nLanes)    *nLanes    = 2;
      if (zeroUpper) *zeroUpper = False;
      if (arrSpec)   *arrSpec   = "2d";
      return True;
   }
   if (bitQ && !bitSZ) { 
      if (tyI)       *tyI       = Ity_I32;
      if (tyF)       *tyF       = Ity_F32;
      if (nLanes)    *nLanes    = 4;
      if (zeroUpper) *zeroUpper = False;
      if (arrSpec)   *arrSpec   = "4s";
      return True;
   }
   if (!bitQ && !bitSZ) { 
      if (tyI)       *tyI       = Ity_I32;
      if (tyF)       *tyF       = Ity_F32;
      if (nLanes)    *nLanes    = 2;
      if (zeroUpper) *zeroUpper = True;
      if (arrSpec)   *arrSpec   = "2s";
      return True;
   }
   
   return False;
}

static Bool getLaneInfo_IMMH_IMMB ( UInt* shift, UInt* szBlg2,
                                    UInt immh, UInt immb )
{
   vassert(immh < (1<<4));
   vassert(immb < (1<<3));
   UInt immhb = (immh << 3) | immb;
   if (immh & 8) {
      if (shift)  *shift  = 128 - immhb;
      if (szBlg2) *szBlg2 = 3;
      return True;
   }
   if (immh & 4) {
      if (shift)  *shift  = 64 - immhb;
      if (szBlg2) *szBlg2 = 2;
      return True;
   }
   if (immh & 2) {
      if (shift)  *shift  = 32 - immhb;
      if (szBlg2) *szBlg2 = 1;
      return True;
   }
   if (immh & 1) {
      if (shift)  *shift  = 16 - immhb;
      if (szBlg2) *szBlg2 = 0;
      return True;
   }
   return False;
}

static IRTemp math_FOLDV ( IRTemp src, IROp op )
{
   switch (op) {
      case Iop_Min8Sx16: case Iop_Min8Ux16:
      case Iop_Max8Sx16: case Iop_Max8Ux16: case Iop_Add8x16: {
         IRTemp x76543210 = src;
         IRTemp x76547654 = newTempV128();
         IRTemp x32103210 = newTempV128();
         assign(x76547654, mk_CatOddLanes64x2 (x76543210, x76543210));
         assign(x32103210, mk_CatEvenLanes64x2(x76543210, x76543210));
         IRTemp x76767676 = newTempV128();
         IRTemp x54545454 = newTempV128();
         IRTemp x32323232 = newTempV128();
         IRTemp x10101010 = newTempV128();
         assign(x76767676, mk_CatOddLanes32x4 (x76547654, x76547654));
         assign(x54545454, mk_CatEvenLanes32x4(x76547654, x76547654));
         assign(x32323232, mk_CatOddLanes32x4 (x32103210, x32103210));
         assign(x10101010, mk_CatEvenLanes32x4(x32103210, x32103210));
         IRTemp x77777777 = newTempV128();
         IRTemp x66666666 = newTempV128();
         IRTemp x55555555 = newTempV128();
         IRTemp x44444444 = newTempV128();
         IRTemp x33333333 = newTempV128();
         IRTemp x22222222 = newTempV128();
         IRTemp x11111111 = newTempV128();
         IRTemp x00000000 = newTempV128();
         assign(x77777777, mk_CatOddLanes16x8 (x76767676, x76767676));
         assign(x66666666, mk_CatEvenLanes16x8(x76767676, x76767676));
         assign(x55555555, mk_CatOddLanes16x8 (x54545454, x54545454));
         assign(x44444444, mk_CatEvenLanes16x8(x54545454, x54545454));
         assign(x33333333, mk_CatOddLanes16x8 (x32323232, x32323232));
         assign(x22222222, mk_CatEvenLanes16x8(x32323232, x32323232));
         assign(x11111111, mk_CatOddLanes16x8 (x10101010, x10101010));
         assign(x00000000, mk_CatEvenLanes16x8(x10101010, x10101010));
         
         IRTemp xAllF = newTempV128();
         IRTemp xAllE = newTempV128();
         IRTemp xAllD = newTempV128();
         IRTemp xAllC = newTempV128();
         IRTemp xAllB = newTempV128();
         IRTemp xAllA = newTempV128();
         IRTemp xAll9 = newTempV128();
         IRTemp xAll8 = newTempV128();
         IRTemp xAll7 = newTempV128();
         IRTemp xAll6 = newTempV128();
         IRTemp xAll5 = newTempV128();
         IRTemp xAll4 = newTempV128();
         IRTemp xAll3 = newTempV128();
         IRTemp xAll2 = newTempV128();
         IRTemp xAll1 = newTempV128();
         IRTemp xAll0 = newTempV128();
         assign(xAllF, mk_CatOddLanes8x16 (x77777777, x77777777));
         assign(xAllE, mk_CatEvenLanes8x16(x77777777, x77777777));
         assign(xAllD, mk_CatOddLanes8x16 (x66666666, x66666666));
         assign(xAllC, mk_CatEvenLanes8x16(x66666666, x66666666));
         assign(xAllB, mk_CatOddLanes8x16 (x55555555, x55555555));
         assign(xAllA, mk_CatEvenLanes8x16(x55555555, x55555555));
         assign(xAll9, mk_CatOddLanes8x16 (x44444444, x44444444));
         assign(xAll8, mk_CatEvenLanes8x16(x44444444, x44444444));
         assign(xAll7, mk_CatOddLanes8x16 (x33333333, x33333333));
         assign(xAll6, mk_CatEvenLanes8x16(x33333333, x33333333));
         assign(xAll5, mk_CatOddLanes8x16 (x22222222, x22222222));
         assign(xAll4, mk_CatEvenLanes8x16(x22222222, x22222222));
         assign(xAll3, mk_CatOddLanes8x16 (x11111111, x11111111));
         assign(xAll2, mk_CatEvenLanes8x16(x11111111, x11111111));
         assign(xAll1, mk_CatOddLanes8x16 (x00000000, x00000000));
         assign(xAll0, mk_CatEvenLanes8x16(x00000000, x00000000));
         IRTemp maxFE = newTempV128();
         IRTemp maxDC = newTempV128();
         IRTemp maxBA = newTempV128();
         IRTemp max98 = newTempV128();
         IRTemp max76 = newTempV128();
         IRTemp max54 = newTempV128();
         IRTemp max32 = newTempV128();
         IRTemp max10 = newTempV128();
         assign(maxFE, binop(op, mkexpr(xAllF), mkexpr(xAllE)));
         assign(maxDC, binop(op, mkexpr(xAllD), mkexpr(xAllC)));
         assign(maxBA, binop(op, mkexpr(xAllB), mkexpr(xAllA)));
         assign(max98, binop(op, mkexpr(xAll9), mkexpr(xAll8)));
         assign(max76, binop(op, mkexpr(xAll7), mkexpr(xAll6)));
         assign(max54, binop(op, mkexpr(xAll5), mkexpr(xAll4)));
         assign(max32, binop(op, mkexpr(xAll3), mkexpr(xAll2)));
         assign(max10, binop(op, mkexpr(xAll1), mkexpr(xAll0)));
         IRTemp maxFEDC = newTempV128();
         IRTemp maxBA98 = newTempV128();
         IRTemp max7654 = newTempV128();
         IRTemp max3210 = newTempV128();
         assign(maxFEDC, binop(op, mkexpr(maxFE), mkexpr(maxDC)));
         assign(maxBA98, binop(op, mkexpr(maxBA), mkexpr(max98)));
         assign(max7654, binop(op, mkexpr(max76), mkexpr(max54)));
         assign(max3210, binop(op, mkexpr(max32), mkexpr(max10)));
         IRTemp maxFEDCBA98 = newTempV128();
         IRTemp max76543210 = newTempV128();
         assign(maxFEDCBA98, binop(op, mkexpr(maxFEDC), mkexpr(maxBA98)));
         assign(max76543210, binop(op, mkexpr(max7654), mkexpr(max3210)));
         IRTemp maxAllLanes = newTempV128();
         assign(maxAllLanes, binop(op, mkexpr(maxFEDCBA98),
                                       mkexpr(max76543210)));
         IRTemp res = newTempV128();
         assign(res, unop(Iop_ZeroHI120ofV128, mkexpr(maxAllLanes)));
         return res;
      }
      case Iop_Min16Sx8: case Iop_Min16Ux8:
      case Iop_Max16Sx8: case Iop_Max16Ux8: case Iop_Add16x8: {
         IRTemp x76543210 = src;
         IRTemp x76547654 = newTempV128();
         IRTemp x32103210 = newTempV128();
         assign(x76547654, mk_CatOddLanes64x2 (x76543210, x76543210));
         assign(x32103210, mk_CatEvenLanes64x2(x76543210, x76543210));
         IRTemp x76767676 = newTempV128();
         IRTemp x54545454 = newTempV128();
         IRTemp x32323232 = newTempV128();
         IRTemp x10101010 = newTempV128();
         assign(x76767676, mk_CatOddLanes32x4 (x76547654, x76547654));
         assign(x54545454, mk_CatEvenLanes32x4(x76547654, x76547654));
         assign(x32323232, mk_CatOddLanes32x4 (x32103210, x32103210));
         assign(x10101010, mk_CatEvenLanes32x4(x32103210, x32103210));
         IRTemp x77777777 = newTempV128();
         IRTemp x66666666 = newTempV128();
         IRTemp x55555555 = newTempV128();
         IRTemp x44444444 = newTempV128();
         IRTemp x33333333 = newTempV128();
         IRTemp x22222222 = newTempV128();
         IRTemp x11111111 = newTempV128();
         IRTemp x00000000 = newTempV128();
         assign(x77777777, mk_CatOddLanes16x8 (x76767676, x76767676));
         assign(x66666666, mk_CatEvenLanes16x8(x76767676, x76767676));
         assign(x55555555, mk_CatOddLanes16x8 (x54545454, x54545454));
         assign(x44444444, mk_CatEvenLanes16x8(x54545454, x54545454));
         assign(x33333333, mk_CatOddLanes16x8 (x32323232, x32323232));
         assign(x22222222, mk_CatEvenLanes16x8(x32323232, x32323232));
         assign(x11111111, mk_CatOddLanes16x8 (x10101010, x10101010));
         assign(x00000000, mk_CatEvenLanes16x8(x10101010, x10101010));
         IRTemp max76 = newTempV128();
         IRTemp max54 = newTempV128();
         IRTemp max32 = newTempV128();
         IRTemp max10 = newTempV128();
         assign(max76, binop(op, mkexpr(x77777777), mkexpr(x66666666)));
         assign(max54, binop(op, mkexpr(x55555555), mkexpr(x44444444)));
         assign(max32, binop(op, mkexpr(x33333333), mkexpr(x22222222)));
         assign(max10, binop(op, mkexpr(x11111111), mkexpr(x00000000)));
         IRTemp max7654 = newTempV128();
         IRTemp max3210 = newTempV128();
         assign(max7654, binop(op, mkexpr(max76), mkexpr(max54)));
         assign(max3210, binop(op, mkexpr(max32), mkexpr(max10)));
         IRTemp max76543210 = newTempV128();
         assign(max76543210, binop(op, mkexpr(max7654), mkexpr(max3210)));
         IRTemp res = newTempV128();
         assign(res, unop(Iop_ZeroHI112ofV128, mkexpr(max76543210)));
         return res;
      }
      case Iop_Max32Fx4: case Iop_Min32Fx4:
      case Iop_Min32Sx4: case Iop_Min32Ux4:
      case Iop_Max32Sx4: case Iop_Max32Ux4: case Iop_Add32x4: {
         IRTemp x3210 = src;
         IRTemp x3232 = newTempV128();
         IRTemp x1010 = newTempV128();
         assign(x3232, mk_CatOddLanes64x2 (x3210, x3210));
         assign(x1010, mk_CatEvenLanes64x2(x3210, x3210));
         IRTemp x3333 = newTempV128();
         IRTemp x2222 = newTempV128();
         IRTemp x1111 = newTempV128();
         IRTemp x0000 = newTempV128();
         assign(x3333, mk_CatOddLanes32x4 (x3232, x3232));
         assign(x2222, mk_CatEvenLanes32x4(x3232, x3232));
         assign(x1111, mk_CatOddLanes32x4 (x1010, x1010));
         assign(x0000, mk_CatEvenLanes32x4(x1010, x1010));
         IRTemp max32 = newTempV128();
         IRTemp max10 = newTempV128();
         assign(max32, binop(op, mkexpr(x3333), mkexpr(x2222)));
         assign(max10, binop(op, mkexpr(x1111), mkexpr(x0000)));
         IRTemp max3210 = newTempV128();
         assign(max3210, binop(op, mkexpr(max32), mkexpr(max10)));
         IRTemp res = newTempV128();
         assign(res, unop(Iop_ZeroHI96ofV128, mkexpr(max3210)));
         return res;
      }
      case Iop_Add64x2: {
         IRTemp x10 = src;
         IRTemp x00 = newTempV128();
         IRTemp x11 = newTempV128();
         assign(x11, binop(Iop_InterleaveHI64x2, mkexpr(x10), mkexpr(x10)));
         assign(x00, binop(Iop_InterleaveLO64x2, mkexpr(x10), mkexpr(x10)));
         IRTemp max10 = newTempV128();
         assign(max10, binop(op, mkexpr(x11), mkexpr(x00)));
         IRTemp res = newTempV128();
         assign(res, unop(Iop_ZeroHI64ofV128, mkexpr(max10)));
         return res;
      }
      default:
         vassert(0);
   }
}


static IRTemp math_TBL_TBX ( IRTemp tab[4], UInt len, IRTemp src,
                             IRTemp oor_values )
{
   vassert(len >= 0 && len <= 3);

   
   IRTemp half15 = newTemp(Ity_I64);
   assign(half15, mkU64(0x0F0F0F0F0F0F0F0FULL));
   IRTemp half16 = newTemp(Ity_I64);
   assign(half16, mkU64(0x1010101010101010ULL));

   
   IRTemp allZero = newTempV128();
   assign(allZero, mkV128(0x0000));
   
   IRTemp all15 = newTempV128();
   assign(all15, binop(Iop_64HLtoV128, mkexpr(half15), mkexpr(half15)));
   
   IRTemp all16 = newTempV128();
   assign(all16, binop(Iop_64HLtoV128, mkexpr(half16), mkexpr(half16)));
   
   IRTemp all32 = newTempV128();
   assign(all32, binop(Iop_Add8x16, mkexpr(all16), mkexpr(all16)));
   
   IRTemp all48 = newTempV128();
   assign(all48, binop(Iop_Add8x16, mkexpr(all16), mkexpr(all32)));
   
   IRTemp all64 = newTempV128();
   assign(all64, binop(Iop_Add8x16, mkexpr(all32), mkexpr(all32)));

   
   IRTemp allXX[4] = { all16, all32, all48, all64 };

   IRTemp running_result = newTempV128();
   assign(running_result, mkV128(0));

   UInt tabent;
   for (tabent = 0; tabent <= len; tabent++) {
      vassert(tabent >= 0 && tabent < 4);
      IRTemp bias = newTempV128();
      assign(bias,
             mkexpr(tabent == 0 ? allZero : allXX[tabent-1]));
      IRTemp biased_indices = newTempV128();
      assign(biased_indices,
             binop(Iop_Sub8x16, mkexpr(src), mkexpr(bias)));
      IRTemp valid_mask = newTempV128();
      assign(valid_mask,
             binop(Iop_CmpGT8Ux16, mkexpr(all16), mkexpr(biased_indices)));
      IRTemp safe_biased_indices = newTempV128();
      assign(safe_biased_indices,
             binop(Iop_AndV128, mkexpr(biased_indices), mkexpr(all15)));
      IRTemp results_or_junk = newTempV128();
      assign(results_or_junk,
             binop(Iop_Perm8x16, mkexpr(tab[tabent]),
                                 mkexpr(safe_biased_indices)));
      IRTemp results_or_zero = newTempV128();
      assign(results_or_zero,
             binop(Iop_AndV128, mkexpr(results_or_junk), mkexpr(valid_mask)));
      
      IRTemp tmp = newTempV128();
      assign(tmp, binop(Iop_OrV128, mkexpr(results_or_zero),
                        mkexpr(running_result)));
      running_result = tmp;
   }

   IRTemp overall_valid_mask = newTempV128();
   assign(overall_valid_mask,
          binop(Iop_CmpGT8Ux16, mkexpr(allXX[len]), mkexpr(src)));
   IRTemp result = newTempV128();
   assign(result,
          binop(Iop_OrV128,
                mkexpr(running_result),
                binop(Iop_AndV128,
                      mkexpr(oor_values),
                      unop(Iop_NotV128, mkexpr(overall_valid_mask)))));
   return result;      
}


static
IRTemp math_BINARY_WIDENING_V128 ( Bool is2, IROp opI64x2toV128,
                                   IRExpr* argL, IRExpr* argR )
{
   IRTemp res   = newTempV128();
   IROp   slice = is2 ? Iop_V128HIto64 : Iop_V128to64;
   assign(res, binop(opI64x2toV128, unop(slice, argL),
                                    unop(slice, argR)));
   return res;
}


static
IRTemp math_ABD ( Bool isU, UInt size, IRExpr* argLE, IRExpr* argRE )
{
   vassert(size <= 3);
   IRTemp argL = newTempV128();
   IRTemp argR = newTempV128();
   IRTemp msk  = newTempV128();
   IRTemp res  = newTempV128();
   assign(argL, argLE);
   assign(argR, argRE);
   assign(msk, binop(isU ? mkVecCMPGTU(size) : mkVecCMPGTS(size),
                     mkexpr(argL), mkexpr(argR)));
   assign(res,
          binop(Iop_OrV128,
                binop(Iop_AndV128,
                      binop(mkVecSUB(size), mkexpr(argL), mkexpr(argR)),
                      mkexpr(msk)),
                binop(Iop_AndV128,
                      binop(mkVecSUB(size), mkexpr(argR), mkexpr(argL)),
                      unop(Iop_NotV128, mkexpr(msk)))));
   return res;
}


static
IRTemp math_WIDEN_LO_OR_HI_LANES ( Bool zWiden, Bool fromUpperHalf,
                                   UInt sizeNarrow, IRExpr* srcE )
{
   IRTemp src = newTempV128();
   IRTemp res = newTempV128();
   assign(src, srcE);
   switch (sizeNarrow) {
      case X10:
         assign(res,
                binop(zWiden ? Iop_ShrN64x2 : Iop_SarN64x2,
                      binop(fromUpperHalf ? Iop_InterleaveHI32x4
                                          : Iop_InterleaveLO32x4,
                            mkexpr(src),
                            mkexpr(src)),
                      mkU8(32)));
         break;
      case X01:
         assign(res,
                binop(zWiden ? Iop_ShrN32x4 : Iop_SarN32x4,
                      binop(fromUpperHalf ? Iop_InterleaveHI16x8
                                          : Iop_InterleaveLO16x8,
                            mkexpr(src),
                            mkexpr(src)),
                      mkU8(16)));
         break;
      case X00:
         assign(res,
                binop(zWiden ? Iop_ShrN16x8 : Iop_SarN16x8,
                      binop(fromUpperHalf ? Iop_InterleaveHI8x16
                                          : Iop_InterleaveLO8x16,
                            mkexpr(src),
                            mkexpr(src)),
                      mkU8(8)));
         break;
      default:
         vassert(0);
   }
   return res;
}


static
IRTemp math_WIDEN_EVEN_OR_ODD_LANES ( Bool zWiden, Bool fromOdd,
                                      UInt sizeNarrow, IRExpr* srcE )
{
   IRTemp src   = newTempV128();
   IRTemp res   = newTempV128();
   IROp   opSAR = mkVecSARN(sizeNarrow+1);
   IROp   opSHR = mkVecSHRN(sizeNarrow+1);
   IROp   opSHL = mkVecSHLN(sizeNarrow+1);
   IROp   opSxR = zWiden ? opSHR : opSAR;
   UInt   amt   = 0;
   switch (sizeNarrow) {
      case X10: amt = 32; break;
      case X01: amt = 16; break;
      case X00: amt = 8;  break;
      default: vassert(0);
   }
   assign(src, srcE);
   if (fromOdd) {
      assign(res, binop(opSxR, mkexpr(src), mkU8(amt)));
   } else {
      assign(res, binop(opSxR, binop(opSHL, mkexpr(src), mkU8(amt)),
                               mkU8(amt)));
   }
   return res;
}


static
IRTemp math_NARROW_LANES ( IRTemp argHi, IRTemp argLo, UInt sizeNarrow )
{
   IRTemp res = newTempV128();
   assign(res, binop(mkVecCATEVENLANES(sizeNarrow),
                     mkexpr(argHi), mkexpr(argLo)));
   return res;
}


static
IRTemp math_DUP_VEC_ELEM ( IRExpr* src, UInt size, UInt laneNo )
{
   vassert(size <= 3);
   UInt ix = laneNo << size;
   vassert(ix <= 15);
   IROp ops[4] = { Iop_INVALID, Iop_INVALID, Iop_INVALID, Iop_INVALID };
   switch (size) {
      case 0: 
         ops[0] = (ix & 1) ? Iop_CatOddLanes8x16 : Iop_CatEvenLanes8x16;
         
      case 1: 
         ops[1] = (ix & 2) ? Iop_CatOddLanes16x8 : Iop_CatEvenLanes16x8;
         
      case 2: 
         ops[2] = (ix & 4) ? Iop_CatOddLanes32x4 : Iop_CatEvenLanes32x4;
         
      case 3: 
         ops[3] = (ix & 8) ? Iop_InterleaveHI64x2 : Iop_InterleaveLO64x2;
         break;
      default:
         vassert(0);
   }
   IRTemp res = newTempV128();
   assign(res, src);
   Int i;
   for (i = 3; i >= 0; i--) {
      if (ops[i] == Iop_INVALID)
         break;
      IRTemp tmp = newTempV128();
      assign(tmp, binop(ops[i], mkexpr(res), mkexpr(res)));
      res = tmp;
   }
   return res;
}


static
IRTemp handle_DUP_VEC_ELEM ( UInt* laneNo,
                             UInt* laneSzLg2, HChar* laneCh,
                             IRExpr* srcV, UInt imm5 )
{
   *laneNo    = 0;
   *laneSzLg2 = 0;
   *laneCh    = '?';

   if (imm5 & 1) {
      *laneNo    = (imm5 >> 1) & 15;
      *laneSzLg2 = 0;
      *laneCh    = 'b';
   }
   else if (imm5 & 2) {
      *laneNo    = (imm5 >> 2) & 7;
      *laneSzLg2 = 1;
      *laneCh    = 'h';
   }
   else if (imm5 & 4) {
      *laneNo    = (imm5 >> 3) & 3;
      *laneSzLg2 = 2;
      *laneCh    = 's';
   }
   else if (imm5 & 8) {
      *laneNo    = (imm5 >> 4) & 1;
      *laneSzLg2 = 3;
      *laneCh    = 'd';
   }
   else {
      
      return IRTemp_INVALID;
   }

   return math_DUP_VEC_ELEM(srcV, *laneSzLg2, *laneNo);
}


static
IRTemp math_VEC_DUP_IMM ( UInt size, ULong imm )
{
   IRType ty  = Ity_INVALID;
   IRTemp rcS = IRTemp_INVALID;
   switch (size) {
      case X01:
         vassert(imm <= 0xFFFFULL);
         ty  = Ity_I16;
         rcS = newTemp(ty); assign(rcS, mkU16( (UShort)imm ));
         break;
      case X10:
         vassert(imm <= 0xFFFFFFFFULL);
         ty  = Ity_I32;
         rcS = newTemp(ty); assign(rcS, mkU32( (UInt)imm ));
         break;
      case X11:
         ty  = Ity_I64;
         rcS = newTemp(ty); assign(rcS, mkU64(imm)); break;
      default:
         vassert(0);
   }
   IRTemp rcV = math_DUP_TO_V128(rcS, ty);
   return rcV;
}


static
void putLO64andZUorPutHI64 ( Bool is2, UInt dd, IRTemp new64 )
{
   if (is2) {
      IRTemp t_zero_oldLO = newTempV128();
      assign(t_zero_oldLO, unop(Iop_ZeroHI64ofV128, getQReg128(dd)));
      IRTemp t_newHI_zero = newTempV128();
      assign(t_newHI_zero, binop(Iop_InterleaveLO64x2, mkexpr(new64),
                                                       mkV128(0x0000)));
      IRTemp res = newTempV128();
      assign(res, binop(Iop_OrV128, mkexpr(t_zero_oldLO),
                                    mkexpr(t_newHI_zero)));
      putQReg128(dd, mkexpr(res));
   } else {
      
      putQReg128(dd, unop(Iop_ZeroHI64ofV128, mkexpr(new64)));
   }
}


static
void math_SQABS ( IRTemp* qabs, IRTemp* nabs,
                  IRExpr* srcE, UInt size )
{
      IRTemp src, mask, maskn, nsub, qsub;
      src = mask = maskn = nsub = qsub = IRTemp_INVALID;
      newTempsV128_7(&src, &mask, &maskn, &nsub, &qsub, nabs, qabs);
      assign(src,   srcE);
      assign(mask,  binop(mkVecCMPGTS(size),  mkV128(0x0000), mkexpr(src)));
      assign(maskn, unop(Iop_NotV128, mkexpr(mask)));
      assign(nsub,  binop(mkVecSUB(size),   mkV128(0x0000), mkexpr(src)));
      assign(qsub,  binop(mkVecQSUBS(size), mkV128(0x0000), mkexpr(src)));
      assign(*nabs, binop(Iop_OrV128,
                          binop(Iop_AndV128, mkexpr(nsub), mkexpr(mask)),
                          binop(Iop_AndV128, mkexpr(src),  mkexpr(maskn))));
      assign(*qabs, binop(Iop_OrV128,
                          binop(Iop_AndV128, mkexpr(qsub), mkexpr(mask)),
                          binop(Iop_AndV128, mkexpr(src),  mkexpr(maskn))));
}


static
void math_SQNEG ( IRTemp* qneg, IRTemp* nneg,
                  IRExpr* srcE, UInt size )
{
      IRTemp src = IRTemp_INVALID;
      newTempsV128_3(&src, nneg, qneg);
      assign(src,   srcE);
      assign(*nneg, binop(mkVecSUB(size),   mkV128(0x0000), mkexpr(src)));
      assign(*qneg, binop(mkVecQSUBS(size), mkV128(0x0000), mkexpr(src)));
}


static IRTemp math_ZERO_ALL_EXCEPT_LOWEST_LANE ( UInt size, IRExpr* srcE )
{
   vassert(size < 4);
   IRTemp t = newTempV128();
   assign(t, unop(mkVecZEROHIxxOFV128(size), srcE));
   return t;
}


static
void math_MULL_ACC ( IRTemp* res,
                     Bool is2, Bool isU, UInt size, HChar mas,
                     IRTemp vecN, IRTemp vecM, IRTemp vecD )
{
   vassert(res && *res == IRTemp_INVALID);
   vassert(size <= 2);
   vassert(mas == 'm' || mas == 'a' || mas == 's');
   if (mas == 'm') vassert(vecD == IRTemp_INVALID);
   IROp   mulOp = isU ? mkVecMULLU(size) : mkVecMULLS(size);
   IROp   accOp = (mas == 'a') ? mkVecADD(size+1) 
                  : (mas == 's' ? mkVecSUB(size+1)
                  : Iop_INVALID);
   IRTemp mul   = math_BINARY_WIDENING_V128(is2, mulOp, 
                                            mkexpr(vecN), mkexpr(vecM));
   *res = newTempV128();
   assign(*res, mas == 'm' ? mkexpr(mul) 
                           : binop(accOp, mkexpr(vecD), mkexpr(mul)));
}


static
void math_SQDMULL_ACC ( IRTemp* res,
                        IRTemp* sat1q, IRTemp* sat1n,
                        IRTemp* sat2q, IRTemp* sat2n,
                        Bool is2, UInt size, HChar mas,
                        IRTemp vecN, IRTemp vecM, IRTemp vecD )
{
   vassert(size <= 2);
   vassert(mas == 'm' || mas == 'a' || mas == 's');
   vassert(sat2q && *sat2q == IRTemp_INVALID);
   vassert(sat2n && *sat2n == IRTemp_INVALID);
   newTempsV128_3(sat1q, sat1n, res);
   IRTemp tq = math_BINARY_WIDENING_V128(is2, mkVecQDMULLS(size),
                                         mkexpr(vecN), mkexpr(vecM));
   IRTemp tn = math_BINARY_WIDENING_V128(is2, mkVecMULLS(size),
                                         mkexpr(vecN), mkexpr(vecM));
   assign(*sat1q, mkexpr(tq));
   assign(*sat1n, binop(mkVecADD(size+1), mkexpr(tn), mkexpr(tn)));

   if (mas == 'm') {
      assign(*res, mkexpr(*sat1q));
      return;
   }

   newTempsV128_2(sat2q, sat2n);
   assign(*sat2q, binop(mas == 'a' ? mkVecQADDS(size+1) : mkVecQSUBS(size+1),
                        mkexpr(vecD), mkexpr(*sat1q)));
   assign(*sat2n, binop(mas == 'a' ? mkVecADD(size+1) : mkVecSUB(size+1),
                        mkexpr(vecD), mkexpr(*sat1n)));
   assign(*res, mkexpr(*sat2q));
}


static
void math_MULLS ( IRTemp* resHI, IRTemp* resLO,
                  UInt sizeNarrow, IRTemp argL, IRTemp argR )
{
   vassert(sizeNarrow <= 2);
   newTempsV128_2(resHI, resLO);
   IRTemp argLhi = newTemp(Ity_I64);
   IRTemp argLlo = newTemp(Ity_I64);
   IRTemp argRhi = newTemp(Ity_I64);
   IRTemp argRlo = newTemp(Ity_I64);
   assign(argLhi, unop(Iop_V128HIto64, mkexpr(argL)));
   assign(argLlo, unop(Iop_V128to64,   mkexpr(argL)));
   assign(argRhi, unop(Iop_V128HIto64, mkexpr(argR)));
   assign(argRlo, unop(Iop_V128to64,   mkexpr(argR)));
   IROp opMulls = mkVecMULLS(sizeNarrow);
   assign(*resHI, binop(opMulls, mkexpr(argLhi), mkexpr(argRhi)));
   assign(*resLO, binop(opMulls, mkexpr(argLlo), mkexpr(argRlo)));
}


static
void math_SQDMULH ( IRTemp* res,
                    IRTemp* sat1q, IRTemp* sat1n,
                    Bool isR, UInt size, IRTemp vN, IRTemp vM )
{
   vassert(size == X01 || size == X10); 

   newTempsV128_3(res, sat1q, sat1n);

   IRTemp mullsHI = IRTemp_INVALID, mullsLO = IRTemp_INVALID;
   math_MULLS(&mullsHI, &mullsLO, size, vN, vM);

   IRTemp addWide = mkVecADD(size+1);

   if (isR) {
      assign(*sat1q, binop(mkVecQRDMULHIS(size), mkexpr(vN), mkexpr(vM)));

      Int    rcShift    = size == X01 ? 15 : 31;
      IRTemp roundConst = math_VEC_DUP_IMM(size+1, 1ULL << rcShift);
      assign(*sat1n,
             binop(mkVecCATODDLANES(size),
                   binop(addWide,
                         binop(addWide, mkexpr(mullsHI), mkexpr(mullsHI)),
                         mkexpr(roundConst)),
                   binop(addWide,
                         binop(addWide, mkexpr(mullsLO), mkexpr(mullsLO)),
                         mkexpr(roundConst))));
   } else {
      assign(*sat1q, binop(mkVecQDMULHIS(size), mkexpr(vN), mkexpr(vM)));

      assign(*sat1n,
             binop(mkVecCATODDLANES(size),
                   binop(addWide, mkexpr(mullsHI), mkexpr(mullsHI)),
                   binop(addWide, mkexpr(mullsLO), mkexpr(mullsLO))));
   }

   assign(*res, mkexpr(*sat1q));
}


static
void math_QSHL_IMM ( IRTemp* res,
                     IRTemp* qDiff1, IRTemp* qDiff2, 
                     IRTemp src, UInt size, UInt shift, const HChar* nm )
{
   vassert(size <= 3);
   UInt laneBits = 8 << size;
   vassert(shift < laneBits);
   newTempsV128_3(res, qDiff1, qDiff2);
   IRTemp z128 = newTempV128();
   assign(z128, mkV128(0x0000));

   
   if (vex_streq(nm, "uqshl")) {
      IROp qop = mkVecQSHLNSATUU(size);
      assign(*res, binop(qop, mkexpr(src), mkU8(shift)));
      if (shift == 0) {
         
         assign(*qDiff1, mkexpr(z128));
         assign(*qDiff2, mkexpr(z128));
      } else {
         UInt rshift = laneBits - shift;
         vassert(rshift >= 1 && rshift < laneBits);
         assign(*qDiff1, binop(mkVecSHRN(size), mkexpr(src), mkU8(rshift)));
         assign(*qDiff2, mkexpr(z128));
      }
      return;
   }

   
   if (vex_streq(nm, "sqshl")) {
      IROp qop = mkVecQSHLNSATSS(size);
      assign(*res, binop(qop, mkexpr(src), mkU8(shift)));
      if (shift == 0) {
         
         assign(*qDiff1, mkexpr(z128));
         assign(*qDiff2, mkexpr(z128));
      } else {
         UInt rshift = laneBits - 1 - shift;
         vassert(rshift >= 0 && rshift < laneBits-1);
         assign(*qDiff1, binop(mkVecSHRN(size), mkexpr(src), mkU8(rshift)));
         assign(*qDiff2, binop(mkVecSHRN(size),
                               binop(mkVecSARN(size), mkexpr(src),
                                                      mkU8(laneBits-1)),
                               mkU8(rshift)));
      }
      return;
   }

   
   if (vex_streq(nm, "sqshlu")) {
      IROp qop = mkVecQSHLNSATSU(size);
      assign(*res, binop(qop, mkexpr(src), mkU8(shift)));
      if (shift == 0) {
         assign(*qDiff1, binop(mkVecSHRN(size), mkexpr(src), mkU8(laneBits-1)));
         assign(*qDiff2, mkexpr(z128));
      } else {
         UInt rshift = laneBits - shift;
         vassert(rshift >= 1 && rshift < laneBits);
         assign(*qDiff1, binop(mkVecSHRN(size), mkexpr(src), mkU8(rshift)));
         assign(*qDiff2, mkexpr(z128));
      }
      return;
   }

   vassert(0);
}


static
IRTemp math_RHADD ( UInt size, Bool isU, IRTemp aa, IRTemp bb )
{
   vassert(size <= 3);
   IROp opSHR = isU ? mkVecSHRN(size) : mkVecSARN(size);
   IROp opADD = mkVecADD(size);
   
   const ULong ones64[4]
      = { 0x0101010101010101ULL, 0x0001000100010001ULL,
          0x0000000100000001ULL, 0x0000000000000001ULL };
   IRTemp imm64 = newTemp(Ity_I64);
   assign(imm64, mkU64(ones64[size]));
   IRTemp vecOne = newTempV128();
   assign(vecOne, binop(Iop_64HLtoV128, mkexpr(imm64), mkexpr(imm64)));
   IRTemp scaOne = newTemp(Ity_I8);
   assign(scaOne, mkU8(1));
   IRTemp res = newTempV128();
   assign(res,
          binop(opADD,
                binop(opSHR, mkexpr(aa), mkexpr(scaOne)),
                binop(opADD,
                      binop(opSHR, mkexpr(bb), mkexpr(scaOne)),
                      binop(opSHR,
                            binop(opADD,
                                  binop(opADD,
                                        binop(Iop_AndV128, mkexpr(aa),
                                                           mkexpr(vecOne)),
                                        binop(Iop_AndV128, mkexpr(bb),
                                                           mkexpr(vecOne))
                                  ),
                                  mkexpr(vecOne)
                            ),
                            mkexpr(scaOne)
                      )
                )
          )
   );
   return res;
}


static
void updateQCFLAGwithDifferenceZHI ( IRTemp qres, IRTemp nres, IROp opZHI )
{
   IRTemp diff      = newTempV128();
   IRTemp oldQCFLAG = newTempV128();
   IRTemp newQCFLAG = newTempV128();
   if (opZHI == Iop_INVALID) {
      assign(diff, binop(Iop_XorV128, mkexpr(qres), mkexpr(nres)));
   } else {
      vassert(opZHI == Iop_ZeroHI64ofV128
              || opZHI == Iop_ZeroHI96ofV128 || opZHI == Iop_ZeroHI112ofV128);
      assign(diff, unop(opZHI, binop(Iop_XorV128, mkexpr(qres), mkexpr(nres))));
   }
   assign(oldQCFLAG, IRExpr_Get(OFFB_QCFLAG, Ity_V128));
   assign(newQCFLAG, binop(Iop_OrV128, mkexpr(oldQCFLAG), mkexpr(diff)));
   stmt(IRStmt_Put(OFFB_QCFLAG, mkexpr(newQCFLAG)));
}


static
void updateQCFLAGwithDifference ( IRTemp qres, IRTemp nres )
{
   updateQCFLAGwithDifferenceZHI(qres, nres, Iop_INVALID);
}


static
void math_REARRANGE_FOR_FLOATING_PAIRWISE (
        IRTemp* rearrL, IRTemp* rearrR,
        IRTemp vecM, IRTemp vecN, Bool isD, UInt bitQ
     )
{
   vassert(rearrL && *rearrL == IRTemp_INVALID);
   vassert(rearrR && *rearrR == IRTemp_INVALID);
   *rearrL = newTempV128();
   *rearrR = newTempV128();
   if (isD) {
      
      vassert(bitQ == 1);
      assign(*rearrL, binop(Iop_InterleaveHI64x2, mkexpr(vecM), mkexpr(vecN)));
      assign(*rearrR, binop(Iop_InterleaveLO64x2, mkexpr(vecM), mkexpr(vecN)));
   }
   else if (!isD && bitQ == 1) {
      
      assign(*rearrL, binop(Iop_CatOddLanes32x4,  mkexpr(vecM), mkexpr(vecN)));
      assign(*rearrR, binop(Iop_CatEvenLanes32x4, mkexpr(vecM), mkexpr(vecN)));
   } else {
      
      vassert(!isD && bitQ == 0);
      IRTemp m1n1m0n0 = newTempV128();
      IRTemp m0n0m1n1 = newTempV128();
      assign(m1n1m0n0, binop(Iop_InterleaveLO32x4,
                             mkexpr(vecM), mkexpr(vecN)));
      assign(m0n0m1n1, triop(Iop_SliceV128,
                             mkexpr(m1n1m0n0), mkexpr(m1n1m0n0), mkU8(8)));
      assign(*rearrL, unop(Iop_ZeroHI64ofV128, mkexpr(m1n1m0n0)));
      assign(*rearrR, unop(Iop_ZeroHI64ofV128, mkexpr(m0n0m1n1)));
   }
}


static Double two_to_the_minus ( Int n )
{
   if (n == 1) return 0.5;
   vassert(n >= 2 && n <= 64);
   Int half = n / 2;
   return two_to_the_minus(half) * two_to_the_minus(n - half);
}


static Double two_to_the_plus ( Int n )
{
   if (n == 1) return 2.0;
   vassert(n >= 2 && n <= 64);
   Int half = n / 2;
   return two_to_the_plus(half) * two_to_the_plus(n - half);
}



static
Bool dis_AdvSIMD_EXT(DisResult* dres, UInt insn)
{
#  define INSN(_bMax,_bMin)  SLICE_UInt(insn, (_bMax), (_bMin))
   if (INSN(31,31) != 0
       || INSN(29,24) != BITS6(1,0,1,1,1,0)
       || INSN(21,21) != 0 || INSN(15,15) != 0 || INSN(10,10) != 0) {
      return False;
   }
   UInt bitQ = INSN(30,30);
   UInt op2  = INSN(23,22);
   UInt mm   = INSN(20,16);
   UInt imm4 = INSN(14,11);
   UInt nn   = INSN(9,5);
   UInt dd   = INSN(4,0);

   if (op2 == BITS2(0,0)) {
      
      IRTemp sHi = newTempV128();
      IRTemp sLo = newTempV128();
      IRTemp res = newTempV128();
      assign(sHi, getQReg128(mm));
      assign(sLo, getQReg128(nn));
      if (bitQ == 1) {
         if (imm4 == 0) {
            assign(res, mkexpr(sLo));
         } else {
            vassert(imm4 >= 1 && imm4 <= 15);
            assign(res, triop(Iop_SliceV128,
                              mkexpr(sHi), mkexpr(sLo), mkU8(imm4)));
         }
         putQReg128(dd, mkexpr(res));
         DIP("ext v%u.16b, v%u.16b, v%u.16b, #%u\n", dd, nn, mm, imm4);
      } else {
         if (imm4 >= 8) return False;
         if (imm4 == 0) {
            assign(res, mkexpr(sLo));
         } else {
            vassert(imm4 >= 1 && imm4 <= 7);
            IRTemp hi64lo64 = newTempV128();
            assign(hi64lo64, binop(Iop_InterleaveLO64x2,
                                   mkexpr(sHi), mkexpr(sLo)));
            assign(res, triop(Iop_SliceV128,
                              mkexpr(hi64lo64), mkexpr(hi64lo64), mkU8(imm4)));
         }
         putQReg128(dd, unop(Iop_ZeroHI64ofV128, mkexpr(res)));
         DIP("ext v%u.8b, v%u.8b, v%u.8b, #%u\n", dd, nn, mm, imm4);
      }
      return True;
   }

   return False;
#  undef INSN
}


static
Bool dis_AdvSIMD_TBL_TBX(DisResult* dres, UInt insn)
{
#  define INSN(_bMax,_bMin)  SLICE_UInt(insn, (_bMax), (_bMin))
   if (INSN(31,31) != 0
       || INSN(29,24) != BITS6(0,0,1,1,1,0)
       || INSN(21,21) != 0
       || INSN(15,15) != 0
       || INSN(11,10) != BITS2(0,0)) {
      return False;
   }
   UInt bitQ  = INSN(30,30);
   UInt op2   = INSN(23,22);
   UInt mm    = INSN(20,16);
   UInt len   = INSN(14,13);
   UInt bitOP = INSN(12,12);
   UInt nn    = INSN(9,5);
   UInt dd    = INSN(4,0);

   if (op2 == X00) {
      
      
      Bool isTBX = bitOP == 1;
      
      IRTemp oor_values = newTempV128();
      assign(oor_values, isTBX ? getQReg128(dd) : mkV128(0));
      
      IRTemp src = newTempV128();
      assign(src, getQReg128(mm));
      
      IRTemp tab[4];
      UInt   i;
      for (i = 0; i <= len; i++) {
         vassert(i < 4);
         tab[i] = newTempV128();
         assign(tab[i], getQReg128((nn + i) % 32));
      }
      IRTemp res = math_TBL_TBX(tab, len, src, oor_values);
      putQReg128(dd, math_MAYBE_ZERO_HI64(bitQ, res));
      const HChar* Ta = bitQ ==1 ? "16b" : "8b";
      const HChar* nm = isTBX ? "tbx" : "tbl";
      DIP("%s %s.%s, {v%d.16b .. v%d.16b}, %s.%s\n",
          nm, nameQReg128(dd), Ta, nn, (nn + len) % 32, nameQReg128(mm), Ta);
      return True;
   }

#  define INSN(_bMax,_bMin)  SLICE_UInt(insn, (_bMax), (_bMin))
   return False;
#  undef INSN
}


static
Bool dis_AdvSIMD_ZIP_UZP_TRN(DisResult* dres, UInt insn)
{
#  define INSN(_bMax,_bMin)  SLICE_UInt(insn, (_bMax), (_bMin))
   if (INSN(31,31) != 0
       || INSN(29,24) != BITS6(0,0,1,1,1,0)
       || INSN(21,21) != 0 || INSN(15,15) != 0 || INSN(11,10) != BITS2(1,0)) {
      return False;
   }
   UInt bitQ   = INSN(30,30);
   UInt size   = INSN(23,22);
   UInt mm     = INSN(20,16);
   UInt opcode = INSN(14,12);
   UInt nn     = INSN(9,5);
   UInt dd     = INSN(4,0);

   if (opcode == BITS3(0,0,1) || opcode == BITS3(1,0,1)) {
      
      
      if (bitQ == 0 && size == X11) return False; 
      Bool   isUZP1 = opcode == BITS3(0,0,1);
      IROp   op     = isUZP1 ? mkVecCATEVENLANES(size)
                             : mkVecCATODDLANES(size);
      IRTemp preL = newTempV128();
      IRTemp preR = newTempV128();
      IRTemp res  = newTempV128();
      if (bitQ == 0) {
         assign(preL, binop(Iop_InterleaveLO64x2, getQReg128(mm),
                                                  getQReg128(nn)));
         assign(preR, mkexpr(preL));
      } else {
         assign(preL, getQReg128(mm));
         assign(preR, getQReg128(nn));
      }
      assign(res, binop(op, mkexpr(preL), mkexpr(preR)));
      putQReg128(dd, math_MAYBE_ZERO_HI64(bitQ, res));
      const HChar* nm  = isUZP1 ? "uzp1" : "uzp2";
      const HChar* arr = nameArr_Q_SZ(bitQ, size);
      DIP("%s %s.%s, %s.%s, %s.%s\n", nm,
          nameQReg128(dd), arr, nameQReg128(nn), arr, nameQReg128(mm), arr);
      return True;
   }

   if (opcode == BITS3(0,1,0) || opcode == BITS3(1,1,0)) {
      
      
      if (bitQ == 0 && size == X11) return False; 
      Bool   isTRN1 = opcode == BITS3(0,1,0);
      IROp   op1    = isTRN1 ? mkVecCATEVENLANES(size)
                             : mkVecCATODDLANES(size);
      IROp op2 = mkVecINTERLEAVEHI(size);
      IRTemp srcM = newTempV128();
      IRTemp srcN = newTempV128();
      IRTemp res  = newTempV128();
      assign(srcM, getQReg128(mm));
      assign(srcN, getQReg128(nn));
      assign(res, binop(op2, binop(op1, mkexpr(srcM), mkexpr(srcM)),
                             binop(op1, mkexpr(srcN), mkexpr(srcN))));
      putQReg128(dd, math_MAYBE_ZERO_HI64(bitQ, res));
      const HChar* nm  = isTRN1 ? "trn1" : "trn2";
      const HChar* arr = nameArr_Q_SZ(bitQ, size);
      DIP("%s %s.%s, %s.%s, %s.%s\n", nm,
          nameQReg128(dd), arr, nameQReg128(nn), arr, nameQReg128(mm), arr);
      return True;
   }

   if (opcode == BITS3(0,1,1) || opcode == BITS3(1,1,1)) {
      
      
      if (bitQ == 0 && size == X11) return False; 
      Bool   isZIP1 = opcode == BITS3(0,1,1);
      IROp   op     = isZIP1 ? mkVecINTERLEAVELO(size)
                             : mkVecINTERLEAVEHI(size);
      IRTemp preL = newTempV128();
      IRTemp preR = newTempV128();
      IRTemp res  = newTempV128();
      if (bitQ == 0 && !isZIP1) {
         IRTemp z128 = newTempV128();
         assign(z128, mkV128(0x0000));
         
         
         assign(preL, triop(Iop_SliceV128,
                            getQReg128(mm), mkexpr(z128), mkU8(12)));
         assign(preR, triop(Iop_SliceV128,
                            getQReg128(nn), mkexpr(z128), mkU8(12)));

      } else {
         assign(preL, getQReg128(mm));
         assign(preR, getQReg128(nn));
      }
      assign(res, binop(op, mkexpr(preL), mkexpr(preR)));
      putQReg128(dd, math_MAYBE_ZERO_HI64(bitQ, res));
      const HChar* nm  = isZIP1 ? "zip1" : "zip2";
      const HChar* arr = nameArr_Q_SZ(bitQ, size);
      DIP("%s %s.%s, %s.%s, %s.%s\n", nm,
          nameQReg128(dd), arr, nameQReg128(nn), arr, nameQReg128(mm), arr);
      return True;
   }

   return False;
#  undef INSN
}


static
Bool dis_AdvSIMD_across_lanes(DisResult* dres, UInt insn)
{
#  define INSN(_bMax,_bMin)  SLICE_UInt(insn, (_bMax), (_bMin))
   if (INSN(31,31) != 0
       || INSN(28,24) != BITS5(0,1,1,1,0)
       || INSN(21,17) != BITS5(1,1,0,0,0) || INSN(11,10) != BITS2(1,0)) {
      return False;
   }
   UInt bitQ   = INSN(30,30);
   UInt bitU   = INSN(29,29);
   UInt size   = INSN(23,22);
   UInt opcode = INSN(16,12);
   UInt nn     = INSN(9,5);
   UInt dd     = INSN(4,0);

   if (opcode == BITS5(0,0,0,1,1)) {
      
      
      
      if (size == X11 || (size == X10 && bitQ == 0)) return False;
      Bool   isU = bitU == 1;
      IRTemp src = newTempV128();
      assign(src, getQReg128(nn));
      IRExpr* widened
         = mkexpr(math_WIDEN_LO_OR_HI_LANES(
                     isU, False, size, mkexpr(src)));
      if (bitQ == 1) {
         widened
            = binop(mkVecADD(size+1),
                    widened,
                    mkexpr(math_WIDEN_LO_OR_HI_LANES(
                              isU, True, size, mkexpr(src)))
              );
      }
      
      IRTemp tWi = newTempV128();
      assign(tWi, widened);
      IRTemp res = math_FOLDV(tWi, mkVecADD(size+1));
      putQReg128(dd, mkexpr(res));
      const HChar* arr = nameArr_Q_SZ(bitQ, size);
      const HChar  ch  = "bhsd"[size];
      DIP("%s %s.%c, %s.%s\n", isU ? "uaddlv" : "saddlv",
          nameQReg128(dd), ch, nameQReg128(nn), arr);
      return True;
   }

   UInt ix = 0;
    if (opcode == BITS5(0,1,0,1,0)) { ix = bitU == 0 ? 1 : 2; }
   else if (opcode == BITS5(1,1,0,1,0)) { ix = bitU == 0 ? 3 : 4; }
   else if (opcode == BITS5(1,1,0,1,1) && bitU == 0) { ix = 5; }
      
   if (ix != 0) {
      
      
      
      
      
      vassert(ix >= 1 && ix <= 5);
      if (size == X11) return False; 
      if (size == X10 && bitQ == 0) return False; 
      const IROp opMAXS[3]
         = { Iop_Max8Sx16, Iop_Max16Sx8, Iop_Max32Sx4 };
      const IROp opMAXU[3]
         = { Iop_Max8Ux16, Iop_Max16Ux8, Iop_Max32Ux4 };
      const IROp opMINS[3]
         = { Iop_Min8Sx16, Iop_Min16Sx8, Iop_Min32Sx4 };
      const IROp opMINU[3]
         = { Iop_Min8Ux16, Iop_Min16Ux8, Iop_Min32Ux4 };
      const IROp opADD[3]
         = { Iop_Add8x16,  Iop_Add16x8,  Iop_Add32x4 };
      vassert(size < 3);
      IROp op = Iop_INVALID;
      const HChar* nm = NULL;
      switch (ix) {
         case 1: op = opMAXS[size]; nm = "smaxv"; break;
         case 2: op = opMAXU[size]; nm = "umaxv"; break;
         case 3: op = opMINS[size]; nm = "sminv"; break;
         case 4: op = opMINU[size]; nm = "uminv"; break;
         case 5: op = opADD[size];  nm = "addv";  break;
         default: vassert(0);
      }
      vassert(op != Iop_INVALID && nm != NULL);
      IRTemp tN1 = newTempV128();
      assign(tN1, getQReg128(nn));
      IRTemp tN2 = newTempV128();
      assign(tN2, bitQ == 0 
                     ? (ix == 5 ? unop(Iop_ZeroHI64ofV128, mkexpr(tN1))
                                : mk_CatEvenLanes64x2(tN1,tN1))
                     : mkexpr(tN1));
      IRTemp res = math_FOLDV(tN2, op);
      if (res == IRTemp_INVALID)
         return False; 
      putQReg128(dd, mkexpr(res));
      const IRType tys[3] = { Ity_I8, Ity_I16, Ity_I32 };
      IRType laneTy = tys[size];
      const HChar* arr = nameArr_Q_SZ(bitQ, size);
      DIP("%s %s, %s.%s\n", nm,
          nameQRegLO(dd, laneTy), nameQReg128(nn), arr);
      return True;
   }

   if ((size == X00 || size == X10)
       && (opcode == BITS5(0,1,1,0,0) || opcode == BITS5(0,1,1,1,1))) {
      
      
      
      
      
      if (bitQ == 0) return False; 
      Bool   isMIN = (size & 2) == 2;
      Bool   isNM  = opcode == BITS5(0,1,1,0,0);
      IROp   opMXX = (isMIN ? mkVecMINF : mkVecMAXF)(2);
      IRTemp src = newTempV128();
      assign(src, getQReg128(nn));
      IRTemp res = math_FOLDV(src, opMXX);
      putQReg128(dd, mkexpr(res));
      DIP("%s%sv s%u, %u.4s\n",
          isMIN ? "fmin" : "fmax", isNM ? "nm" : "", dd, nn);
      return True;
   }

#  define INSN(_bMax,_bMin)  SLICE_UInt(insn, (_bMax), (_bMin))
   return False;
#  undef INSN
}


static
Bool dis_AdvSIMD_copy(DisResult* dres, UInt insn)
{
#  define INSN(_bMax,_bMin)  SLICE_UInt(insn, (_bMax), (_bMin))
   if (INSN(31,31) != 0
       || INSN(28,21) != BITS8(0,1,1,1,0,0,0,0)
       || INSN(15,15) != 0 || INSN(10,10) != 1) {
      return False;
   }
   UInt bitQ  = INSN(30,30);
   UInt bitOP = INSN(29,29);
   UInt imm5  = INSN(20,16);
   UInt imm4  = INSN(14,11);
   UInt nn    = INSN(9,5);
   UInt dd    = INSN(4,0);

   
   if (bitOP == 0 && imm4 == BITS4(0,0,0,0)) {
      UInt   laneNo    = 0;
      UInt   laneSzLg2 = 0;
      HChar  laneCh    = '?';
      IRTemp res       = handle_DUP_VEC_ELEM(&laneNo, &laneSzLg2, &laneCh,
                                             getQReg128(nn), imm5);
      if (res == IRTemp_INVALID)
         return False;
      if (bitQ == 0 && laneSzLg2 == X11)
         return False; 
      putQReg128(dd, math_MAYBE_ZERO_HI64(bitQ, res));
      const HChar* arT = nameArr_Q_SZ(bitQ, laneSzLg2);
      DIP("dup %s.%s, %s.%c[%u]\n",
           nameQReg128(dd), arT, nameQReg128(nn), laneCh, laneNo);
      return True;
   }

   
   if (bitOP == 0 && imm4 == BITS4(0,0,0,1)) {
      Bool   isQ = bitQ == 1;
      IRTemp w0  = newTemp(Ity_I64);
      const HChar* arT = "??";
      IRType laneTy = Ity_INVALID;
      if (imm5 & 1) {
         arT    = isQ ? "16b" : "8b";
         laneTy = Ity_I8;
         assign(w0, unop(Iop_8Uto64, unop(Iop_64to8, getIReg64orZR(nn))));
      }
      else if (imm5 & 2) {
         arT    = isQ ? "8h" : "4h";
         laneTy = Ity_I16;
         assign(w0, unop(Iop_16Uto64, unop(Iop_64to16, getIReg64orZR(nn))));
      }
      else if (imm5 & 4) {
         arT    = isQ ? "4s" : "2s";
         laneTy = Ity_I32;
         assign(w0, unop(Iop_32Uto64, unop(Iop_64to32, getIReg64orZR(nn))));
      }
      else if ((imm5 & 8) && isQ) {
         arT    = "2d";
         laneTy = Ity_I64;
         assign(w0, getIReg64orZR(nn));
      }
      else {
         
      }
      
      if (laneTy != Ity_INVALID) {
         IRTemp w1 = math_DUP_TO_64(w0, laneTy);
         putQReg128(dd, binop(Iop_64HLtoV128,
                              isQ ? mkexpr(w1) : mkU64(0), mkexpr(w1)));
         DIP("dup %s.%s, %s\n",
             nameQReg128(dd), arT, nameIRegOrZR(laneTy == Ity_I64, nn));
         return True;
      }
      
      return False;
   }

   
   if (bitQ == 1 && bitOP == 0 && imm4 == BITS4(0,0,1,1)) {
      HChar   ts     = '?';
      UInt    laneNo = 16;
      IRExpr* src    = NULL;
      if (imm5 & 1) {
         src    = unop(Iop_64to8, getIReg64orZR(nn));
         laneNo = (imm5 >> 1) & 15;
         ts     = 'b';
      }
      else if (imm5 & 2) {
         src    = unop(Iop_64to16, getIReg64orZR(nn));
         laneNo = (imm5 >> 2) & 7;
         ts     = 'h';
      }
      else if (imm5 & 4) {
         src    = unop(Iop_64to32, getIReg64orZR(nn));
         laneNo = (imm5 >> 3) & 3;
         ts     = 's';
      }
      else if (imm5 & 8) {
         src    = getIReg64orZR(nn);
         laneNo = (imm5 >> 4) & 1;
         ts     = 'd';
      }
      
      if (src) {
         vassert(laneNo < 16);
         putQRegLane(dd, laneNo, src);
         DIP("ins %s.%c[%u], %s\n",
             nameQReg128(dd), ts, laneNo, nameIReg64orZR(nn));
         return True;
      }
      
      return False;
   }

   
   
   if (bitOP == 0 && (imm4 == BITS4(0,1,0,1) || imm4 == BITS4(0,1,1,1))) {
      Bool isU  = (imm4 & 2) == 2;
      const HChar* arTs = "??";
      UInt    laneNo = 16; 
      
      IRExpr* res    = NULL;
      if (!bitQ && (imm5 & 1)) { 
         laneNo = (imm5 >> 1) & 15;
         IRExpr* lane = getQRegLane(nn, laneNo, Ity_I8);
         res = isU ? unop(Iop_8Uto64, lane)
                   : unop(Iop_32Uto64, unop(Iop_8Sto32, lane));
         arTs = "b";
      }
      else if (bitQ && (imm5 & 1)) { 
         laneNo = (imm5 >> 1) & 15;
         IRExpr* lane = getQRegLane(nn, laneNo, Ity_I8);
         res = isU ? NULL
                   : unop(Iop_8Sto64, lane);
         arTs = "b";
      }
      else if (!bitQ && (imm5 & 2)) { 
         laneNo = (imm5 >> 2) & 7;
         IRExpr* lane = getQRegLane(nn, laneNo, Ity_I16);
         res = isU ? unop(Iop_16Uto64, lane)
                   : unop(Iop_32Uto64, unop(Iop_16Sto32, lane));
         arTs = "h";
      }
      else if (bitQ && (imm5 & 2)) { 
         laneNo = (imm5 >> 2) & 7;
         IRExpr* lane = getQRegLane(nn, laneNo, Ity_I16);
         res = isU ? NULL
                   : unop(Iop_16Sto64, lane);
         arTs = "h";
      }
      else if (!bitQ && (imm5 & 4)) { 
         laneNo = (imm5 >> 3) & 3;
         IRExpr* lane = getQRegLane(nn, laneNo, Ity_I32);
         res = isU ? unop(Iop_32Uto64, lane)
                   : NULL;
         arTs = "s";
      }
      else if (bitQ && (imm5 & 4)) { 
         laneNo = (imm5 >> 3) & 3;
         IRExpr* lane = getQRegLane(nn, laneNo, Ity_I32);
         res = isU ? NULL
                   : unop(Iop_32Sto64, lane);
         arTs = "s";
      }
      else if (bitQ && (imm5 & 8)) { 
         laneNo = (imm5 >> 4) & 1;
         IRExpr* lane = getQRegLane(nn, laneNo, Ity_I64);
         res = isU ? lane
                   : NULL;
         arTs = "d";
      }
      
      if (res) {
         vassert(laneNo < 16);
         putIReg64orZR(dd, res);
         DIP("%cmov %s, %s.%s[%u]\n", isU ? 'u' : 's',
             nameIRegOrZR(bitQ == 1, dd),
             nameQReg128(nn), arTs, laneNo);
         return True;
      }
      
      return False;
   }

   
   if (bitQ == 1 && bitOP == 1) {
      HChar   ts  = '?';
      IRType  ity = Ity_INVALID;
      UInt    ix1 = 16;
      UInt    ix2 = 16;
      if (imm5 & 1) {
         ts  = 'b';
         ity = Ity_I8;
         ix1 = (imm5 >> 1) & 15;
         ix2 = (imm4 >> 0) & 15;
      }
      else if (imm5 & 2) {
         ts  = 'h';
         ity = Ity_I16;
         ix1 = (imm5 >> 2) & 7;
         ix2 = (imm4 >> 1) & 7;
      }
      else if (imm5 & 4) {
         ts  = 's';
         ity = Ity_I32;
         ix1 = (imm5 >> 3) & 3;
         ix2 = (imm4 >> 2) & 3;
      }
      else if (imm5 & 8) {
         ts  = 'd';
         ity = Ity_I64;
         ix1 = (imm5 >> 4) & 1;
         ix2 = (imm4 >> 3) & 1;
      }
      
      if (ity != Ity_INVALID) {
         vassert(ix1 < 16);
         vassert(ix2 < 16);
         putQRegLane(dd, ix1, getQRegLane(nn, ix2, ity));
         DIP("ins %s.%c[%u], %s.%c[%u]\n",
             nameQReg128(dd), ts, ix1, nameQReg128(nn), ts, ix2);
         return True;
      }
      
      return False;
   }

   return False;
#  undef INSN
}


static
Bool dis_AdvSIMD_modified_immediate(DisResult* dres, UInt insn)
{
#  define INSN(_bMax,_bMin)  SLICE_UInt(insn, (_bMax), (_bMin))
   if (INSN(31,31) != 0
       || INSN(28,19) != BITS10(0,1,1,1,1,0,0,0,0,0)
       || INSN(11,10) != BITS2(0,1)) {
      return False;
   }
   UInt bitQ     = INSN(30,30);
   UInt bitOP    = INSN(29,29);
   UInt cmode    = INSN(15,12);
   UInt abcdefgh = (INSN(18,16) << 5) | INSN(9,5);
   UInt dd       = INSN(4,0);

   ULong imm64lo  = 0;
   UInt  op_cmode = (bitOP << 4) | cmode;
   Bool  ok       = False;
   Bool  isORR    = False;
   Bool  isBIC    = False;
   Bool  isMOV    = False;
   Bool  isMVN    = False;
   Bool  isFMOV   = False;
   switch (op_cmode) {
      
      
      
      
      case BITS5(0,0,0,0,0): case BITS5(0,0,0,1,0):
      case BITS5(0,0,1,0,0): case BITS5(0,0,1,1,0): 
         ok = True; isMOV = True; break;

      
      
      
      
      case BITS5(0,0,0,0,1): case BITS5(0,0,0,1,1):
      case BITS5(0,0,1,0,1): case BITS5(0,0,1,1,1): 
         ok = True; isORR = True; break;

      
      
      case BITS5(0,1,0,0,0): case BITS5(0,1,0,1,0): 
         ok = True; isMOV = True; break;

      
      
      case BITS5(0,1,0,0,1): case BITS5(0,1,0,1,1): 
         ok = True; isORR = True; break;

      
      
      case BITS5(0,1,1,0,0): case BITS5(0,1,1,0,1): 
         ok = True; isMOV = True; break;

      
      case BITS5(0,1,1,1,0):
         ok = True; isMOV = True; break;

      
      case BITS5(0,1,1,1,1): 
         ok = True; isFMOV = True; break;

      
      
      
      
      case BITS5(1,0,0,0,0): case BITS5(1,0,0,1,0):
      case BITS5(1,0,1,0,0): case BITS5(1,0,1,1,0): 
         ok = True; isMVN = True; break;

      
      
      
      
      case BITS5(1,0,0,0,1): case BITS5(1,0,0,1,1):
      case BITS5(1,0,1,0,1): case BITS5(1,0,1,1,1): 
         ok = True; isBIC = True; break;

      
      
      case BITS5(1,1,0,0,0): case BITS5(1,1,0,1,0): 
         ok = True; isMVN = True; break;

      
      
      case BITS5(1,1,0,0,1): case BITS5(1,1,0,1,1): 
         ok = True; isBIC = True; break;

      
      
      case BITS5(1,1,1,0,0): case BITS5(1,1,1,0,1): 
         ok = True; isMVN = True; break;

      
      
      case BITS5(1,1,1,1,0):
         ok = True; isMOV = True; break;

      
      case BITS5(1,1,1,1,1): 
         ok = bitQ == 1; isFMOV = True; break;

      default:
        break;
   }
   if (ok) {
      vassert(1 == (isMOV ? 1 : 0) + (isMVN ? 1 : 0)
                   + (isORR ? 1 : 0) + (isBIC ? 1 : 0) + (isFMOV ? 1 : 0));
      ok = AdvSIMDExpandImm(&imm64lo, bitOP, cmode, abcdefgh);
   }
   if (ok) {
      if (isORR || isBIC) {
         ULong inv
            = isORR ? 0ULL : ~0ULL;
         IRExpr* immV128
            = binop(Iop_64HLtoV128, mkU64(inv ^ imm64lo), mkU64(inv ^ imm64lo));
         IRExpr* res
            = binop(isORR ? Iop_OrV128 : Iop_AndV128, getQReg128(dd), immV128);
         const HChar* nm = isORR ? "orr" : "bic";
         if (bitQ == 0) {
            putQReg128(dd, unop(Iop_ZeroHI64ofV128, res));
            DIP("%s %s.1d, %016llx\n", nm, nameQReg128(dd), imm64lo);
         } else {
            putQReg128(dd, res);
            DIP("%s %s.2d, #0x%016llx'%016llx\n", nm,
                nameQReg128(dd), imm64lo, imm64lo);
         }
      } 
      else if (isMOV || isMVN || isFMOV) {
         if (isMVN) imm64lo = ~imm64lo;
         ULong   imm64hi = bitQ == 0  ? 0  :  imm64lo;
         IRExpr* immV128 = binop(Iop_64HLtoV128, mkU64(imm64hi),
                                                 mkU64(imm64lo));
         putQReg128(dd, immV128);
         DIP("mov %s, #0x%016llx'%016llx\n", nameQReg128(dd), imm64hi, imm64lo);
      }
      return True;
   }
   

   return False;
#  undef INSN
}


static
Bool dis_AdvSIMD_scalar_copy(DisResult* dres, UInt insn)
{
#  define INSN(_bMax,_bMin)  SLICE_UInt(insn, (_bMax), (_bMin))
   if (INSN(31,30) != BITS2(0,1)
       || INSN(28,21) != BITS8(1,1,1,1,0,0,0,0)
       || INSN(15,15) != 0 || INSN(10,10) != 1) {
      return False;
   }
   UInt bitOP = INSN(29,29);
   UInt imm5  = INSN(20,16);
   UInt imm4  = INSN(14,11);
   UInt nn    = INSN(9,5);
   UInt dd    = INSN(4,0);

   if (bitOP == 0 && imm4 == BITS4(0,0,0,0)) {
      
      IRTemp w0     = newTemp(Ity_I64);
      const HChar* arTs = "??";
      IRType laneTy = Ity_INVALID;
      UInt   laneNo = 16; 
      if (imm5 & 1) {
         arTs   = "b";
         laneNo = (imm5 >> 1) & 15;
         laneTy = Ity_I8;
         assign(w0, unop(Iop_8Uto64, getQRegLane(nn, laneNo, laneTy)));
      }
      else if (imm5 & 2) {
         arTs   = "h";
         laneNo = (imm5 >> 2) & 7;
         laneTy = Ity_I16;
         assign(w0, unop(Iop_16Uto64, getQRegLane(nn, laneNo, laneTy)));
      }
      else if (imm5 & 4) {
         arTs   = "s";
         laneNo = (imm5 >> 3) & 3;
         laneTy = Ity_I32;
         assign(w0, unop(Iop_32Uto64, getQRegLane(nn, laneNo, laneTy)));
      }
      else if (imm5 & 8) {
         arTs   = "d";
         laneNo = (imm5 >> 4) & 1;
         laneTy = Ity_I64;
         assign(w0, getQRegLane(nn, laneNo, laneTy));
      }
      else {
         
      }
      
      if (laneTy != Ity_INVALID) {
         vassert(laneNo < 16);
         putQReg128(dd, binop(Iop_64HLtoV128, mkU64(0), mkexpr(w0)));
         DIP("dup %s, %s.%s[%u]\n",
             nameQRegLO(dd, laneTy), nameQReg128(nn), arTs, laneNo);
         return True;
      }
      
   }

   return False;
#  undef INSN
}


static
Bool dis_AdvSIMD_scalar_pairwise(DisResult* dres, UInt insn)
{
#  define INSN(_bMax,_bMin)  SLICE_UInt(insn, (_bMax), (_bMin))
   if (INSN(31,30) != BITS2(0,1)
       || INSN(28,24) != BITS5(1,1,1,1,0)
       || INSN(21,17) != BITS5(1,1,0,0,0)
       || INSN(11,10) != BITS2(1,0)) {
      return False;
   }
   UInt bitU   = INSN(29,29);
   UInt sz     = INSN(23,22);
   UInt opcode = INSN(16,12);
   UInt nn     = INSN(9,5);
   UInt dd     = INSN(4,0);

   if (bitU == 0 && sz == X11 && opcode == BITS5(1,1,0,1,1)) {
      
      IRTemp xy = newTempV128();
      IRTemp xx = newTempV128();
      assign(xy, getQReg128(nn));
      assign(xx, binop(Iop_InterleaveHI64x2, mkexpr(xy), mkexpr(xy)));
      putQReg128(dd, unop(Iop_ZeroHI64ofV128,
                          binop(Iop_Add64x2, mkexpr(xy), mkexpr(xx))));
      DIP("addp d%u, %s.2d\n", dd, nameQReg128(nn));
      return True;
   }

   if (bitU == 1 && sz <= X01 && opcode == BITS5(0,1,1,0,1)) {
      
      
      Bool   isD   = sz == X01;
      IROp   opZHI = mkVecZEROHIxxOFV128(isD ? 3 : 2);
      IROp   opADD = mkVecADDF(isD ? 3 : 2);
      IRTemp src   = newTempV128();
      IRTemp argL  = newTempV128();
      IRTemp argR  = newTempV128();
      assign(src, getQReg128(nn));
      assign(argL, unop(opZHI, mkexpr(src)));
      assign(argR, unop(opZHI, triop(Iop_SliceV128, mkexpr(src), mkexpr(src), 
                                                    mkU8(isD ? 8 : 4))));
      putQReg128(dd, unop(opZHI,
                          triop(opADD, mkexpr(mk_get_IR_rounding_mode()),
                                              mkexpr(argL), mkexpr(argR))));
      DIP(isD ? "faddp d%u, v%u.2d\n" : "faddp s%u, v%u.2s\n", dd, nn);
      return True;
   }

   if (bitU == 1
       && (opcode == BITS5(0,1,1,0,0) || opcode == BITS5(0,1,1,1,1))) {
      
      
      
      
      
      Bool   isD   = (sz & 1) == 1;
      Bool   isMIN = (sz & 2) == 2;
      Bool   isNM  = opcode == BITS5(0,1,1,0,0);
      IROp   opZHI = mkVecZEROHIxxOFV128(isD ? 3 : 2);
      IROp   opMXX = (isMIN ? mkVecMINF : mkVecMAXF)(isD ? 3 : 2);
      IRTemp src   = newTempV128();
      IRTemp argL  = newTempV128();
      IRTemp argR  = newTempV128();
      assign(src, getQReg128(nn));
      assign(argL, unop(opZHI, mkexpr(src)));
      assign(argR, unop(opZHI, triop(Iop_SliceV128, mkexpr(src), mkexpr(src), 
                                                    mkU8(isD ? 8 : 4))));
      putQReg128(dd, unop(opZHI,
                          binop(opMXX, mkexpr(argL), mkexpr(argR))));
      HChar c = isD ? 'd' : 's';
      DIP("%s%sp %c%u, v%u.2%c\n",
           isMIN ? "fmin" : "fmax", isNM ? "nm" : "", c, dd, nn, c);
      return True;
   }

   return False;
#  undef INSN
}


static
Bool dis_AdvSIMD_scalar_shift_by_imm(DisResult* dres, UInt insn)
{
#  define INSN(_bMax,_bMin)  SLICE_UInt(insn, (_bMax), (_bMin))
   if (INSN(31,30) != BITS2(0,1)
       || INSN(28,23) != BITS6(1,1,1,1,1,0) || INSN(10,10) != 1) {
      return False;
   }
   UInt bitU   = INSN(29,29);
   UInt immh   = INSN(22,19);
   UInt immb   = INSN(18,16);
   UInt opcode = INSN(15,11);
   UInt nn     = INSN(9,5);
   UInt dd     = INSN(4,0);
   UInt immhb  = (immh << 3) | immb;

   if ((immh & 8) == 8
       && (opcode == BITS5(0,0,0,0,0) || opcode == BITS5(0,0,0,1,0))) {
      
      
      
      
      Bool isU   = bitU == 1;
      Bool isAcc = opcode == BITS5(0,0,0,1,0);
      UInt sh    = 128 - immhb;
      vassert(sh >= 1 && sh <= 64);
      IROp    op  = isU ? Iop_ShrN64x2 : Iop_SarN64x2;
      IRExpr* src = getQReg128(nn);
      IRTemp  shf = newTempV128();
      IRTemp  res = newTempV128();
      if (sh == 64 && isU) {
         assign(shf, mkV128(0x0000));
      } else {
         UInt nudge = 0;
         if (sh == 64) {
            vassert(!isU);
            nudge = 1;
         }
         assign(shf, binop(op, src, mkU8(sh - nudge)));
      }
      assign(res, isAcc ? binop(Iop_Add64x2, getQReg128(dd), mkexpr(shf))
                        : mkexpr(shf));
      putQReg128(dd, unop(Iop_ZeroHI64ofV128, mkexpr(res)));
      const HChar* nm = isAcc ? (isU ? "usra" : "ssra")
                              : (isU ? "ushr" : "sshr");
      DIP("%s d%u, d%u, #%u\n", nm, dd, nn, sh);
      return True;
   }

   if ((immh & 8) == 8
       && (opcode == BITS5(0,0,1,0,0) || opcode == BITS5(0,0,1,1,0))) {
      
      
      
      
      Bool isU   = bitU == 1;
      Bool isAcc = opcode == BITS5(0,0,1,1,0);
      UInt sh    = 128 - immhb;
      vassert(sh >= 1 && sh <= 64);
      IROp    op  = isU ? Iop_Rsh64Ux2 : Iop_Rsh64Sx2;
      vassert(sh >= 1 && sh <= 64);
      IRExpr* src  = getQReg128(nn);
      IRTemp  imm8 = newTemp(Ity_I8);
      assign(imm8, mkU8((UChar)(-sh)));
      IRExpr* amt  = mkexpr(math_DUP_TO_V128(imm8, Ity_I8));
      IRTemp  shf  = newTempV128();
      IRTemp  res  = newTempV128();
      assign(shf, binop(op, src, amt));
      assign(res, isAcc ? binop(Iop_Add64x2, getQReg128(dd), mkexpr(shf))
                        : mkexpr(shf));
      putQReg128(dd, unop(Iop_ZeroHI64ofV128, mkexpr(res)));
      const HChar* nm = isAcc ? (isU ? "ursra" : "srsra")
                              : (isU ? "urshr" : "srshr");
      DIP("%s d%u, d%u, #%u\n", nm, dd, nn, sh);
      return True;
   }

   if (bitU == 1 && (immh & 8) == 8 && opcode == BITS5(0,1,0,0,0)) {
      
      UInt sh = 128 - immhb;
      vassert(sh >= 1 && sh <= 64);
      if (sh == 64) {
         putQReg128(dd, unop(Iop_ZeroHI64ofV128, getQReg128(dd)));
      } else {
         
         ULong   nmask  = (ULong)(((Long)0x8000000000000000ULL) >> (sh-1));
         IRExpr* nmaskV = binop(Iop_64HLtoV128, mkU64(nmask), mkU64(nmask));
         IRTemp  res    = newTempV128();
         assign(res, binop(Iop_OrV128,
                           binop(Iop_AndV128, getQReg128(dd), nmaskV),
                           binop(Iop_ShrN64x2, getQReg128(nn), mkU8(sh))));
         putQReg128(dd, unop(Iop_ZeroHI64ofV128, mkexpr(res)));
      }
      DIP("sri d%u, d%u, #%u\n", dd, nn, sh);
      return True;
   }

   if (bitU == 0 && (immh & 8) == 8 && opcode == BITS5(0,1,0,1,0)) {
      
      UInt sh = immhb - 64;
      vassert(sh >= 0 && sh < 64);
      putQReg128(dd,
                 unop(Iop_ZeroHI64ofV128,
                      sh == 0 ? getQReg128(nn)
                              : binop(Iop_ShlN64x2, getQReg128(nn), mkU8(sh))));
      DIP("shl d%u, d%u, #%u\n", dd, nn, sh);
      return True;
   }

   if (bitU == 1 && (immh & 8) == 8 && opcode == BITS5(0,1,0,1,0)) {
      
      UInt sh = immhb - 64;
      vassert(sh >= 0 && sh < 64);
      if (sh == 0) {
         putQReg128(dd, unop(Iop_ZeroHI64ofV128, getQReg128(nn)));
      } else {
         
         ULong   nmask  = (1ULL << sh) - 1;
         IRExpr* nmaskV = binop(Iop_64HLtoV128, mkU64(nmask), mkU64(nmask));
         IRTemp  res    = newTempV128();
         assign(res, binop(Iop_OrV128,
                           binop(Iop_AndV128, getQReg128(dd), nmaskV),
                           binop(Iop_ShlN64x2, getQReg128(nn), mkU8(sh))));
         putQReg128(dd, unop(Iop_ZeroHI64ofV128, mkexpr(res)));
      }
      DIP("sli d%u, d%u, #%u\n", dd, nn, sh);
      return True;
   }

   if (opcode == BITS5(0,1,1,1,0)
       || (bitU == 1 && opcode == BITS5(0,1,1,0,0))) {
      
      
      
      UInt size  = 0;
      UInt shift = 0;
      Bool ok    = getLaneInfo_IMMH_IMMB(&shift, &size, immh, immb);
      if (!ok) return False;
      vassert(size >= 0 && size <= 3);
      UInt lanebits = 8 << size;
      shift = lanebits - shift;
      vassert(shift >= 0 && shift < lanebits);
      const HChar* nm = NULL;
       if (bitU == 0 && opcode == BITS5(0,1,1,1,0)) nm = "sqshl";
      else if (bitU == 1 && opcode == BITS5(0,1,1,1,0)) nm = "uqshl";
      else if (bitU == 1 && opcode == BITS5(0,1,1,0,0)) nm = "sqshlu";
      else vassert(0);
      IRTemp qDiff1 = IRTemp_INVALID;
      IRTemp qDiff2 = IRTemp_INVALID;
      IRTemp res = IRTemp_INVALID;
      IRTemp src = math_ZERO_ALL_EXCEPT_LOWEST_LANE(size, getQReg128(nn));
      math_QSHL_IMM(&res, &qDiff1, &qDiff2, src, size, shift, nm);
      putQReg128(dd, mkexpr(res));
      updateQCFLAGwithDifference(qDiff1, qDiff2);
      const HChar arr = "bhsd"[size];
      DIP("%s %c%u, %c%u, #%u\n", nm, arr, dd, arr, nn, shift);
      return True;
   }

   if (opcode == BITS5(1,0,0,1,0) || opcode == BITS5(1,0,0,1,1)
       || (bitU == 1
           && (opcode == BITS5(1,0,0,0,0) || opcode == BITS5(1,0,0,0,1)))) {
      
      
      
      
      
      
      UInt size  = 0;
      UInt shift = 0;
      Bool ok    = getLaneInfo_IMMH_IMMB(&shift, &size, immh, immb);
      if (!ok || size == X11) return False;
      vassert(size >= X00 && size <= X10);
      vassert(shift >= 1 && shift <= (8 << size));
      const HChar* nm = "??";
      IROp op = Iop_INVALID;
      
       if (bitU == 0 && opcode == BITS5(1,0,0,1,0)) {
         nm = "sqshrn"; op = mkVecQANDqsarNNARROWSS(size);
      }
      else if (bitU == 1 && opcode == BITS5(1,0,0,1,0)) {
         nm = "uqshrn"; op = mkVecQANDqshrNNARROWUU(size);
      }
      else if (bitU == 0 && opcode == BITS5(1,0,0,1,1)) {
         nm = "sqrshrn"; op = mkVecQANDqrsarNNARROWSS(size);
      }
      else if (bitU == 1 && opcode == BITS5(1,0,0,1,1)) {
         nm = "uqrshrn"; op = mkVecQANDqrshrNNARROWUU(size);
      }
      else if (bitU == 1 && opcode == BITS5(1,0,0,0,0)) {
         nm = "sqshrun"; op = mkVecQANDqsarNNARROWSU(size);
      }
      else if (bitU == 1 && opcode == BITS5(1,0,0,0,1)) {
         nm = "sqrshrun"; op = mkVecQANDqrsarNNARROWSU(size);
      }
      else vassert(0);
      
      IRTemp src128 = math_ZERO_ALL_EXCEPT_LOWEST_LANE(size+1, getQReg128(nn));
      IRTemp pair   = newTempV128();
      assign(pair, binop(op, mkexpr(src128), mkU8(shift)));
      
      IRTemp res64in128 = newTempV128();
      assign(res64in128, unop(Iop_ZeroHI64ofV128, mkexpr(pair)));
      putQReg128(dd, mkexpr(res64in128));
      
      IRTemp q64q64 = newTempV128();
      assign(q64q64, binop(Iop_InterleaveHI64x2, mkexpr(pair), mkexpr(pair)));
      IRTemp z128 = newTempV128();
      assign(z128, mkV128(0x0000));
      updateQCFLAGwithDifference(q64q64, z128);
      
      const HChar arrNarrow = "bhsd"[size];
      const HChar arrWide   = "bhsd"[size+1];
      DIP("%s %c%u, %c%u, #%u\n", nm, arrNarrow, dd, arrWide, nn, shift);
      return True;
   }

   if (immh >= BITS4(0,1,0,0) && opcode == BITS5(1,1,1,0,0)) {
      
      
      UInt size  = 0;
      UInt fbits = 0;
      Bool ok    = getLaneInfo_IMMH_IMMB(&fbits, &size, immh, immb);
      
      vassert(ok);
      
      vassert(size == X10 || size == X11);
      Bool isD = size == X11;
      Bool isU = bitU == 1;
      vassert(fbits >= 1 && fbits <= (isD ? 64 : 32));
      Double  scale  = two_to_the_minus(fbits);
      IRExpr* scaleE = isD ? IRExpr_Const(IRConst_F64(scale))
                             : IRExpr_Const(IRConst_F32( (Float)scale ));
      IROp    opMUL  = isD ? Iop_MulF64 : Iop_MulF32;
      IROp    opCVT  = isU ? (isD ? Iop_I64UtoF64 : Iop_I32UtoF32)
                           : (isD ? Iop_I64StoF64 : Iop_I32StoF32);
      IRType tyF = isD ? Ity_F64 : Ity_F32;
      IRType tyI = isD ? Ity_I64 : Ity_I32;
      IRTemp src = newTemp(tyI);
      IRTemp res = newTemp(tyF);
      IRTemp rm  = mk_get_IR_rounding_mode();
      assign(src, getQRegLane(nn, 0, tyI));
      assign(res, triop(opMUL, mkexpr(rm),
                               binop(opCVT, mkexpr(rm), mkexpr(src)), scaleE));
      putQRegLane(dd, 0, mkexpr(res));
      if (!isD) {
         putQRegLane(dd, 1, mkU32(0));
      }
      putQRegLane(dd, 1, mkU64(0));
      const HChar ch = isD ? 'd' : 's';
      DIP("%s %c%u, %c%u, #%u\n", isU ? "ucvtf" : "scvtf",
          ch, dd, ch, nn, fbits);
      return True;
   }

   if (immh >= BITS4(0,1,0,0) && opcode == BITS5(1,1,1,1,1)) {
      
      
      UInt size  = 0;
      UInt fbits = 0;
      Bool ok    = getLaneInfo_IMMH_IMMB(&fbits, &size, immh, immb);
      
      vassert(ok);
      
      vassert(size == X10 || size == X11);
      Bool isD = size == X11;
      Bool isU = bitU == 1;
      vassert(fbits >= 1 && fbits <= (isD ? 64 : 32));
      Double  scale  = two_to_the_plus(fbits);
      IRExpr* scaleE = isD ? IRExpr_Const(IRConst_F64(scale))
                           : IRExpr_Const(IRConst_F32( (Float)scale ));
      IROp    opMUL  = isD ? Iop_MulF64 : Iop_MulF32;
      IROp    opCVT  = isU ? (isD ? Iop_F64toI64U : Iop_F32toI32U)
                           : (isD ? Iop_F64toI64S : Iop_F32toI32S);
      IRType tyF = isD ? Ity_F64 : Ity_F32;
      IRType tyI = isD ? Ity_I64 : Ity_I32;
      IRTemp src = newTemp(tyF);
      IRTemp res = newTemp(tyI);
      IRTemp rm  = newTemp(Ity_I32);
      assign(src, getQRegLane(nn, 0, tyF));
      assign(rm,  mkU32(Irrm_ZERO));
      assign(res, binop(opCVT, mkexpr(rm), 
                               triop(opMUL, mkexpr(rm), mkexpr(src), scaleE)));
      putQRegLane(dd, 0, mkexpr(res));
      if (!isD) {
         putQRegLane(dd, 1, mkU32(0));
      }
      putQRegLane(dd, 1, mkU64(0));
      const HChar ch = isD ? 'd' : 's';
      DIP("%s %c%u, %c%u, #%u\n", isU ? "fcvtzu" : "fcvtzs",
          ch, dd, ch, nn, fbits);
      return True;
   }

#  define INSN(_bMax,_bMin)  SLICE_UInt(insn, (_bMax), (_bMin))
   return False;
#  undef INSN
}


static
Bool dis_AdvSIMD_scalar_three_different(DisResult* dres, UInt insn)
{
#  define INSN(_bMax,_bMin)  SLICE_UInt(insn, (_bMax), (_bMin))
   if (INSN(31,30) != BITS2(0,1)
       || INSN(28,24) != BITS5(1,1,1,1,0)
       || INSN(21,21) != 1
       || INSN(11,10) != BITS2(0,0)) {
      return False;
   }
   UInt bitU   = INSN(29,29);
   UInt size   = INSN(23,22);
   UInt mm     = INSN(20,16);
   UInt opcode = INSN(15,12);
   UInt nn     = INSN(9,5);
   UInt dd     = INSN(4,0);
   vassert(size < 4);

   if (bitU == 0
       && (opcode == BITS4(1,1,0,1)
           || opcode == BITS4(1,0,0,1) || opcode == BITS4(1,0,1,1))) {
       
       
       
      
      UInt ks = 3;
      switch (opcode) {
         case BITS4(1,1,0,1): ks = 0; break;
         case BITS4(1,0,0,1): ks = 1; break;
         case BITS4(1,0,1,1): ks = 2; break;
         default: vassert(0);
      }
      vassert(ks >= 0 && ks <= 2);
      if (size == X00 || size == X11) return False;
      vassert(size <= 2);
      IRTemp vecN, vecM, vecD, res, sat1q, sat1n, sat2q, sat2n;
      vecN = vecM = vecD = res = sat1q = sat1n = sat2q = sat2n = IRTemp_INVALID;
      newTempsV128_3(&vecN, &vecM, &vecD);
      assign(vecN, getQReg128(nn));
      assign(vecM, getQReg128(mm));
      assign(vecD, getQReg128(dd));
      math_SQDMULL_ACC(&res, &sat1q, &sat1n, &sat2q, &sat2n,
                       False, size, "mas"[ks],
                       vecN, vecM, ks == 0 ? IRTemp_INVALID : vecD);
      IROp opZHI = mkVecZEROHIxxOFV128(size+1);
      putQReg128(dd, unop(opZHI, mkexpr(res)));
      vassert(sat1q != IRTemp_INVALID && sat1n != IRTemp_INVALID);
      updateQCFLAGwithDifferenceZHI(sat1q, sat1n, opZHI);
      if (sat2q != IRTemp_INVALID || sat2n != IRTemp_INVALID) {
         updateQCFLAGwithDifferenceZHI(sat2q, sat2n, opZHI);
      }
      const HChar* nm        = ks == 0 ? "sqdmull"
                                       : (ks == 1 ? "sqdmlal" : "sqdmlsl");
      const HChar  arrNarrow = "bhsd"[size];
      const HChar  arrWide   = "bhsd"[size+1];
      DIP("%s %c%d, %c%d, %c%d\n",
          nm, arrWide, dd, arrNarrow, nn, arrNarrow, mm);
      return True;
   }

   return False;
#  undef INSN
}


static
Bool dis_AdvSIMD_scalar_three_same(DisResult* dres, UInt insn)
{
#  define INSN(_bMax,_bMin)  SLICE_UInt(insn, (_bMax), (_bMin))
   if (INSN(31,30) != BITS2(0,1)
       || INSN(28,24) != BITS5(1,1,1,1,0)
       || INSN(21,21) != 1
       || INSN(10,10) != 1) {
      return False;
   }
   UInt bitU   = INSN(29,29);
   UInt size   = INSN(23,22);
   UInt mm     = INSN(20,16);
   UInt opcode = INSN(15,11);
   UInt nn     = INSN(9,5);
   UInt dd     = INSN(4,0);
   vassert(size < 4);

   if (opcode == BITS5(0,0,0,0,1) || opcode == BITS5(0,0,1,0,1)) {
      
      
      
      
      Bool isADD = opcode == BITS5(0,0,0,0,1);
      Bool isU   = bitU == 1;
      IROp qop   = Iop_INVALID;
      IROp nop   = Iop_INVALID;
      if (isADD) {
         qop = isU ? mkVecQADDU(size) : mkVecQADDS(size);
         nop = mkVecADD(size);
      } else {
         qop = isU ? mkVecQSUBU(size) : mkVecQSUBS(size);
         nop = mkVecSUB(size);
      }
      IRTemp argL = newTempV128();
      IRTemp argR = newTempV128();
      IRTemp qres = newTempV128();
      IRTemp nres = newTempV128();
      assign(argL, getQReg128(nn));
      assign(argR, getQReg128(mm));
      assign(qres, mkexpr(math_ZERO_ALL_EXCEPT_LOWEST_LANE(
                             size, binop(qop, mkexpr(argL), mkexpr(argR)))));
      assign(nres, mkexpr(math_ZERO_ALL_EXCEPT_LOWEST_LANE(
                             size, binop(nop, mkexpr(argL), mkexpr(argR)))));
      putQReg128(dd, mkexpr(qres));
      updateQCFLAGwithDifference(qres, nres);
      const HChar* nm  = isADD ? (isU ? "uqadd" : "sqadd") 
                               : (isU ? "uqsub" : "sqsub");
      const HChar  arr = "bhsd"[size];
      DIP("%s %c%u, %c%u, %c%u\n", nm, arr, dd, arr, nn, arr, mm);
      return True;
   }

   if (size == X11 && opcode == BITS5(0,0,1,1,0)) {
       
       
      Bool    isGT = bitU == 0;
      IRExpr* argL = getQReg128(nn);
      IRExpr* argR = getQReg128(mm);
      IRTemp  res  = newTempV128();
      assign(res,
             isGT ? binop(Iop_CmpGT64Sx2, argL, argR)
                  : binop(Iop_CmpGT64Ux2, argL, argR));
      putQReg128(dd, unop(Iop_ZeroHI64ofV128, mkexpr(res)));
      DIP("%s %s, %s, %s\n",isGT ? "cmgt" : "cmhi",
          nameQRegLO(dd, Ity_I64),
          nameQRegLO(nn, Ity_I64), nameQRegLO(mm, Ity_I64));
      return True;
   }

   if (size == X11 && opcode == BITS5(0,0,1,1,1)) {
       
       
      Bool    isGE = bitU == 0;
      IRExpr* argL = getQReg128(nn);
      IRExpr* argR = getQReg128(mm);
      IRTemp  res  = newTempV128();
      assign(res,
             isGE ? unop(Iop_NotV128, binop(Iop_CmpGT64Sx2, argR, argL))
                  : unop(Iop_NotV128, binop(Iop_CmpGT64Ux2, argR, argL)));
      putQReg128(dd, unop(Iop_ZeroHI64ofV128, mkexpr(res)));
      DIP("%s %s, %s, %s\n", isGE ? "cmge" : "cmhs",
          nameQRegLO(dd, Ity_I64),
          nameQRegLO(nn, Ity_I64), nameQRegLO(mm, Ity_I64));
      return True;
   }

   if (size == X11 && (opcode == BITS5(0,1,0,0,0)
                       || opcode == BITS5(0,1,0,1,0))) {
      
      
      
      
      Bool isU = bitU == 1;
      Bool isR = opcode == BITS5(0,1,0,1,0);
      IROp op  = isR ? (isU ? mkVecRSHU(size) : mkVecRSHS(size))
                     : (isU ? mkVecSHU(size)  : mkVecSHS(size));
      IRTemp res = newTempV128();
      assign(res, binop(op, getQReg128(nn), getQReg128(mm)));
      putQReg128(dd, unop(Iop_ZeroHI64ofV128, mkexpr(res)));
      const HChar* nm  = isR ? (isU ? "urshl" : "srshl")
                             : (isU ? "ushl"  : "sshl");
      DIP("%s %s, %s, %s\n", nm,
          nameQRegLO(dd, Ity_I64),
          nameQRegLO(nn, Ity_I64), nameQRegLO(mm, Ity_I64));
      return True;
   }

   if (opcode == BITS5(0,1,0,0,1) || opcode == BITS5(0,1,0,1,1)) {
      
      
      
      
      Bool isU = bitU == 1;
      Bool isR = opcode == BITS5(0,1,0,1,1);
      IROp op  = isR ? (isU ? mkVecQANDUQRSH(size) : mkVecQANDSQRSH(size))
                     : (isU ? mkVecQANDUQSH(size)  : mkVecQANDSQSH(size));
      IRTemp res256 = newTemp(Ity_V256);
      IRTemp resSH  = newTempV128();
      IRTemp resQ   = newTempV128();
      IRTemp zero   = newTempV128();
      assign(
         res256,
         binop(op, 
               mkexpr(math_ZERO_ALL_EXCEPT_LOWEST_LANE(size, getQReg128(nn))),
               mkexpr(math_ZERO_ALL_EXCEPT_LOWEST_LANE(size, getQReg128(mm)))));
      assign(resSH, unop(Iop_V256toV128_0, mkexpr(res256)));
      assign(resQ,  unop(Iop_V256toV128_1, mkexpr(res256)));
      assign(zero,  mkV128(0x0000));      
      putQReg128(dd, mkexpr(resSH));
      updateQCFLAGwithDifference(resQ, zero);
      const HChar* nm  = isR ? (isU ? "uqrshl" : "sqrshl")
                             : (isU ? "uqshl"  : "sqshl");
      const HChar  arr = "bhsd"[size];
      DIP("%s %c%u, %c%u, %c%u\n", nm, arr, dd, arr, nn, arr, mm);
      return True;
   }

   if (size == X11 && opcode == BITS5(1,0,0,0,0)) {
      
      
      Bool   isSUB = bitU == 1;
      IRTemp res   = newTemp(Ity_I64);
      assign(res, binop(isSUB ? Iop_Sub64 : Iop_Add64,
                        getQRegLane(nn, 0, Ity_I64),
                        getQRegLane(mm, 0, Ity_I64)));
      putQRegLane(dd, 0, mkexpr(res));
      putQRegLane(dd, 1, mkU64(0));
      DIP("%s %s, %s, %s\n", isSUB ? "sub" : "add",
          nameQRegLO(dd, Ity_I64),
          nameQRegLO(nn, Ity_I64), nameQRegLO(mm, Ity_I64));
      return True;
   }

   if (size == X11 && opcode == BITS5(1,0,0,0,1)) {
       
       
      Bool    isEQ = bitU == 1;
      IRExpr* argL = getQReg128(nn);
      IRExpr* argR = getQReg128(mm);
      IRTemp  res  = newTempV128();
      assign(res,
             isEQ ? binop(Iop_CmpEQ64x2, argL, argR)
                  : unop(Iop_NotV128, binop(Iop_CmpEQ64x2,
                                            binop(Iop_AndV128, argL, argR), 
                                            mkV128(0x0000))));
      putQReg128(dd, unop(Iop_ZeroHI64ofV128, mkexpr(res)));
      DIP("%s %s, %s, %s\n", isEQ ? "cmeq" : "cmtst",
          nameQRegLO(dd, Ity_I64),
          nameQRegLO(nn, Ity_I64), nameQRegLO(mm, Ity_I64));
      return True;
   }

   if (opcode == BITS5(1,0,1,1,0)) {
      
      
      if (size == X00 || size == X11) return False;
      Bool isR = bitU == 1;
      IRTemp res, sat1q, sat1n, vN, vM;
      res = sat1q = sat1n = vN = vM = IRTemp_INVALID;
      newTempsV128_2(&vN, &vM);
      assign(vN, getQReg128(nn));
      assign(vM, getQReg128(mm));
      math_SQDMULH(&res, &sat1q, &sat1n, isR, size, vN, vM);
      putQReg128(dd,
                 mkexpr(math_ZERO_ALL_EXCEPT_LOWEST_LANE(size, mkexpr(res))));
      updateQCFLAGwithDifference(
         math_ZERO_ALL_EXCEPT_LOWEST_LANE(size, mkexpr(sat1q)),
         math_ZERO_ALL_EXCEPT_LOWEST_LANE(size, mkexpr(sat1n)));
      const HChar  arr = "bhsd"[size];
      const HChar* nm  = isR ? "sqrdmulh" : "sqdmulh";
      DIP("%s %c%d, %c%d, %c%d\n", nm, arr, dd, arr, nn, arr, mm);
      return True;
   }

   if (bitU == 1 && size >= X10 && opcode == BITS5(1,1,0,1,0)) {
      
      IRType ity = size == X11 ? Ity_F64 : Ity_F32;
      IRTemp res = newTemp(ity);
      assign(res, unop(mkABSF(ity),
                       triop(mkSUBF(ity),
                             mkexpr(mk_get_IR_rounding_mode()),
                             getQRegLO(nn,ity), getQRegLO(mm,ity))));
      putQReg128(dd, mkV128(0x0000));
      putQRegLO(dd, mkexpr(res));
      DIP("fabd %s, %s, %s\n",
          nameQRegLO(dd, ity), nameQRegLO(nn, ity), nameQRegLO(mm, ity));
      return True;
   }

   if (bitU == 0 && size <= X01 && opcode == BITS5(1,1,0,1,1)) {
      
      
      IRType ity = size == X01 ? Ity_F64 : Ity_F32;
      IRTemp res = newTemp(ity);
      assign(res, triop(mkMULF(ity),
                        mkexpr(mk_get_IR_rounding_mode()),
                        getQRegLO(nn,ity), getQRegLO(mm,ity)));
      putQReg128(dd, mkV128(0x0000));
      putQRegLO(dd, mkexpr(res));
      DIP("fmulx %s, %s, %s\n",
          nameQRegLO(dd, ity), nameQRegLO(nn, ity), nameQRegLO(mm, ity));
      return True;
   }

   if (size <= X01 && opcode == BITS5(1,1,1,0,0)) {
      
      
      Bool   isD   = size == X01;
      IRType ity   = isD ? Ity_F64 : Ity_F32;
      Bool   isGE  = bitU == 1;
      IROp   opCMP = isGE ? (isD ? Iop_CmpLE64Fx2 : Iop_CmpLE32Fx4)
                          : (isD ? Iop_CmpEQ64Fx2 : Iop_CmpEQ32Fx4);
      IRTemp res   = newTempV128();
      assign(res, isGE ? binop(opCMP, getQReg128(mm), getQReg128(nn)) 
                       : binop(opCMP, getQReg128(nn), getQReg128(mm)));
      putQReg128(dd, mkexpr(math_ZERO_ALL_EXCEPT_LOWEST_LANE(isD ? X11 : X10,
                                                             mkexpr(res))));
      DIP("%s %s, %s, %s\n", isGE ? "fcmge" : "fcmeq",
          nameQRegLO(dd, ity), nameQRegLO(nn, ity), nameQRegLO(mm, ity));
      return True;
   }

   if (bitU == 1 && size >= X10 && opcode == BITS5(1,1,1,0,0)) {
      
      Bool   isD   = size == X11;
      IRType ity   = isD ? Ity_F64 : Ity_F32;
      IROp   opCMP = isD ? Iop_CmpLT64Fx2 : Iop_CmpLT32Fx4;
      IRTemp res   = newTempV128();
      assign(res, binop(opCMP, getQReg128(mm), getQReg128(nn))); 
      putQReg128(dd, mkexpr(math_ZERO_ALL_EXCEPT_LOWEST_LANE(isD ? X11 : X10,
                                                             mkexpr(res))));
      DIP("%s %s, %s, %s\n", "fcmgt",
          nameQRegLO(dd, ity), nameQRegLO(nn, ity), nameQRegLO(mm, ity));
      return True;
   }

   if (bitU == 1 && opcode == BITS5(1,1,1,0,1)) {
      
      
      Bool   isD   = (size & 1) == 1;
      IRType ity   = isD ? Ity_F64 : Ity_F32;
      Bool   isGT  = (size & 2) == 2;
      IROp   opCMP = isGT ? (isD ? Iop_CmpLT64Fx2 : Iop_CmpLT32Fx4)
                          : (isD ? Iop_CmpLE64Fx2 : Iop_CmpLE32Fx4);
      IROp   opABS = isD ? Iop_Abs64Fx2 : Iop_Abs32Fx4;
      IRTemp res   = newTempV128();
      assign(res, binop(opCMP, unop(opABS, getQReg128(mm)),
                               unop(opABS, getQReg128(nn)))); 
      putQReg128(dd, mkexpr(math_ZERO_ALL_EXCEPT_LOWEST_LANE(isD ? X11 : X10,
                                                             mkexpr(res))));
      DIP("%s %s, %s, %s\n", isGT ? "facgt" : "facge",
          nameQRegLO(dd, ity), nameQRegLO(nn, ity), nameQRegLO(mm, ity));
      return True;
   }

   if (bitU == 0 && opcode == BITS5(1,1,1,1,1)) {
      
      
      Bool isSQRT = (size & 2) == 2;
      Bool isD    = (size & 1) == 1;
      IROp op     = isSQRT ? (isD ? Iop_RSqrtStep64Fx2 : Iop_RSqrtStep32Fx4)
                           : (isD ? Iop_RecipStep64Fx2 : Iop_RecipStep32Fx4);
      IRTemp res = newTempV128();
      assign(res, binop(op, getQReg128(nn), getQReg128(mm)));
      putQReg128(dd, mkexpr(math_ZERO_ALL_EXCEPT_LOWEST_LANE(isD ? X11 : X10,
                                                             mkexpr(res))));
      HChar c = isD ? 'd' : 's';
      DIP("%s %c%u, %c%u, %c%u\n", isSQRT ? "frsqrts" : "frecps",
          c, dd, c, nn, c, mm);
      return True;
   }

   return False;
#  undef INSN
}


static
Bool dis_AdvSIMD_scalar_two_reg_misc(DisResult* dres, UInt insn)
{
#  define INSN(_bMax,_bMin)  SLICE_UInt(insn, (_bMax), (_bMin))
   if (INSN(31,30) != BITS2(0,1)
       || INSN(28,24) != BITS5(1,1,1,1,0)
       || INSN(21,17) != BITS5(1,0,0,0,0)
       || INSN(11,10) != BITS2(1,0)) {
      return False;
   }
   UInt bitU   = INSN(29,29);
   UInt size   = INSN(23,22);
   UInt opcode = INSN(16,12);
   UInt nn     = INSN(9,5);
   UInt dd     = INSN(4,0);
   vassert(size < 4);

   if (opcode == BITS5(0,0,0,1,1)) {
      
      
      Bool   isUSQADD = bitU == 1;
      IROp   qop  = isUSQADD ? mkVecQADDEXTSUSATUU(size)
                             : mkVecQADDEXTUSSATSS(size);
      IROp   nop  = mkVecADD(size);
      IRTemp argL = newTempV128();
      IRTemp argR = newTempV128();
      assign(argL, getQReg128(nn));
      assign(argR, getQReg128(dd));
      IRTemp qres = math_ZERO_ALL_EXCEPT_LOWEST_LANE(
                       size, binop(qop, mkexpr(argL), mkexpr(argR)));
      IRTemp nres = math_ZERO_ALL_EXCEPT_LOWEST_LANE(
                       size, binop(nop, mkexpr(argL), mkexpr(argR)));
      putQReg128(dd, mkexpr(qres));
      updateQCFLAGwithDifference(qres, nres);
      const HChar arr = "bhsd"[size];
      DIP("%s %c%u, %c%u\n", isUSQADD ? "usqadd" : "suqadd", arr, dd, arr, nn);
      return True;
   }

   if (opcode == BITS5(0,0,1,1,1)) {
      
      
      Bool isNEG = bitU == 1;
      IRTemp qresFW = IRTemp_INVALID, nresFW = IRTemp_INVALID;
      (isNEG ? math_SQNEG : math_SQABS)( &qresFW, &nresFW,
                                         getQReg128(nn), size );
      IRTemp qres = math_ZERO_ALL_EXCEPT_LOWEST_LANE(size, mkexpr(qresFW));
      IRTemp nres = math_ZERO_ALL_EXCEPT_LOWEST_LANE(size, mkexpr(nresFW));
      putQReg128(dd, mkexpr(qres));
      updateQCFLAGwithDifference(qres, nres);
      const HChar arr = "bhsd"[size];
      DIP("%s %c%u, %c%u\n", isNEG ? "sqneg" : "sqabs", arr, dd, arr, nn);
      return True;
   }

   if (size == X11 && opcode == BITS5(0,1,0,0,0)) {
       
       
      Bool    isGT = bitU == 0;
      IRExpr* argL = getQReg128(nn);
      IRExpr* argR = mkV128(0x0000);
      IRTemp  res  = newTempV128();
      assign(res, isGT ? binop(Iop_CmpGT64Sx2, argL, argR)
                       : unop(Iop_NotV128, binop(Iop_CmpGT64Sx2, argR, argL)));
      putQReg128(dd, unop(Iop_ZeroHI64ofV128, mkexpr(res)));
      DIP("cm%s d%u, d%u, #0\n", isGT ? "gt" : "ge", dd, nn);
      return True;
   }

   if (size == X11 && opcode == BITS5(0,1,0,0,1)) {
       
       
      Bool    isEQ = bitU == 0;
      IRExpr* argL = getQReg128(nn);
      IRExpr* argR = mkV128(0x0000);
      IRTemp  res  = newTempV128();
      assign(res, isEQ ? binop(Iop_CmpEQ64x2, argL, argR)
                       : unop(Iop_NotV128,
                              binop(Iop_CmpGT64Sx2, argL, argR)));
      putQReg128(dd, unop(Iop_ZeroHI64ofV128, mkexpr(res)));
      DIP("cm%s d%u, d%u, #0\n", isEQ ? "eq" : "le", dd, nn);
      return True;
   }

   if (bitU == 0 && size == X11 && opcode == BITS5(0,1,0,1,0)) {
       
      putQReg128(dd, unop(Iop_ZeroHI64ofV128,
                          binop(Iop_CmpGT64Sx2, mkV128(0x0000),
                                                getQReg128(nn))));
      DIP("cm%s d%u, d%u, #0\n", "lt", dd, nn);
      return True;
   }

   if (bitU == 0 && size == X11 && opcode == BITS5(0,1,0,1,1)) {
      
      putQReg128(dd, unop(Iop_ZeroHI64ofV128,
                          unop(Iop_Abs64x2, getQReg128(nn))));
      DIP("abs d%u, d%u\n", dd, nn);
      return True;
   }

   if (bitU == 1 && size == X11 && opcode == BITS5(0,1,0,1,1)) {
      
      putQReg128(dd, unop(Iop_ZeroHI64ofV128,
                          binop(Iop_Sub64x2, mkV128(0x0000), getQReg128(nn))));
      DIP("neg d%u, d%u\n", dd, nn);
      return True;
   }

   UInt ix = 0; 
   if (size >= X10) {
      switch (opcode) {
         case BITS5(0,1,1,0,0): ix = (bitU == 1) ? 4 : 1; break;
         case BITS5(0,1,1,0,1): ix = (bitU == 1) ? 5 : 2; break;
         case BITS5(0,1,1,1,0): if (bitU == 0) ix = 3; break;
         default: break;
      }
   }
   if (ix > 0) {
      
      
      
      
      
      Bool   isD     = size == X11;
      IRType ity     = isD ? Ity_F64 : Ity_F32;
      IROp   opCmpEQ = isD ? Iop_CmpEQ64Fx2 : Iop_CmpEQ32Fx4;
      IROp   opCmpLE = isD ? Iop_CmpLE64Fx2 : Iop_CmpLE32Fx4;
      IROp   opCmpLT = isD ? Iop_CmpLT64Fx2 : Iop_CmpLT32Fx4;
      IROp   opCmp   = Iop_INVALID;
      Bool   swap    = False;
      const HChar* nm = "??";
      switch (ix) {
         case 1: nm = "fcmgt"; opCmp = opCmpLT; swap = True; break;
         case 2: nm = "fcmeq"; opCmp = opCmpEQ; break;
         case 3: nm = "fcmlt"; opCmp = opCmpLT; break;
         case 4: nm = "fcmge"; opCmp = opCmpLE; swap = True; break;
         case 5: nm = "fcmle"; opCmp = opCmpLE; break;
         default: vassert(0);
      }
      IRExpr* zero = mkV128(0x0000);
      IRTemp res = newTempV128();
      assign(res, swap ? binop(opCmp, zero, getQReg128(nn))
                       : binop(opCmp, getQReg128(nn), zero));
      putQReg128(dd, mkexpr(math_ZERO_ALL_EXCEPT_LOWEST_LANE(isD ? X11 : X10,
                                                             mkexpr(res))));

      DIP("%s %s, %s, #0.0\n", nm, nameQRegLO(dd, ity), nameQRegLO(nn, ity));
      return True;
   }

   if (opcode == BITS5(1,0,1,0,0)
       || (bitU == 1 && opcode == BITS5(1,0,0,1,0))) {
      
      
      
      if (size == X11) return False;
      vassert(size < 3);
      IROp  opN    = Iop_INVALID;
      Bool  zWiden = True;
      const HChar* nm = "??";
       if (bitU == 0 && opcode == BITS5(1,0,1,0,0)) {
         opN = mkVecQNARROWUNSS(size); nm = "sqxtn"; zWiden = False;
      }
      else if (bitU == 1 && opcode == BITS5(1,0,1,0,0)) {
         opN = mkVecQNARROWUNUU(size); nm = "uqxtn";
      }
      else if (bitU == 1 && opcode == BITS5(1,0,0,1,0)) {
         opN = mkVecQNARROWUNSU(size); nm = "sqxtun";
      }
      else vassert(0);
      IRTemp src  = math_ZERO_ALL_EXCEPT_LOWEST_LANE(
                       size+1, getQReg128(nn));
      IRTemp resN = math_ZERO_ALL_EXCEPT_LOWEST_LANE(
                       size, unop(Iop_64UtoV128, unop(opN, mkexpr(src))));
      putQReg128(dd, mkexpr(resN));
      IRTemp resW = math_WIDEN_LO_OR_HI_LANES(zWiden, False,
                                              size, mkexpr(resN));
      updateQCFLAGwithDifference(src, resW);
      const HChar arrNarrow = "bhsd"[size];
      const HChar arrWide   = "bhsd"[size+1];
      DIP("%s %c%u, %c%u\n", nm, arrNarrow, dd, arrWide, nn);
      return True;
   }

   if (opcode == BITS5(1,0,1,1,0) && bitU == 1 && size == X01) {
      
      putQRegLO(dd,
                binop(Iop_F64toF32, mkU32(Irrm_NEAREST),
                                    getQRegLO(nn, Ity_F64)));
      putQRegLane(dd, 1, mkU32(0));
      putQRegLane(dd, 1, mkU64(0));
      DIP("fcvtxn s%u, d%u\n", dd, nn);
      return True;
   }

   ix = 0; 
   switch (opcode) {
      case BITS5(1,1,0,1,0): ix = ((size & 2) == 2) ? 4 : 1; break;
      case BITS5(1,1,0,1,1): ix = ((size & 2) == 2) ? 5 : 2; break;
      case BITS5(1,1,1,0,0): if ((size & 2) == 0) ix = 3; break;
      default: break;
   }
   if (ix > 0) {
      
      
      
      
      
      
      
      
      
      
      Bool           isD  = (size & 1) == 1;
      IRType         tyF  = isD ? Ity_F64 : Ity_F32;
      IRType         tyI  = isD ? Ity_I64 : Ity_I32;
      IRRoundingMode irrm = 8; 
      HChar          ch   = '?';
      switch (ix) {
         case 1: ch = 'n'; irrm = Irrm_NEAREST; break;
         case 2: ch = 'm'; irrm = Irrm_NegINF;  break;
         case 3: ch = 'a'; irrm = Irrm_NEAREST; break; 
         case 4: ch = 'p'; irrm = Irrm_PosINF;  break;
         case 5: ch = 'z'; irrm = Irrm_ZERO;    break;
         default: vassert(0);
      }
      IROp cvt = Iop_INVALID;
      if (bitU == 1) {
         cvt = isD ? Iop_F64toI64U : Iop_F32toI32U;
      } else {
         cvt = isD ? Iop_F64toI64S : Iop_F32toI32S;
      }
      IRTemp src = newTemp(tyF);
      IRTemp res = newTemp(tyI);
      assign(src, getQRegLane(nn, 0, tyF));
      assign(res, binop(cvt, mkU32(irrm), mkexpr(src)));
      putQRegLane(dd, 0, mkexpr(res)); 
      if (!isD) {
         putQRegLane(dd, 1, mkU32(0)); 
      }
      putQRegLane(dd, 1, mkU64(0));    
      HChar sOrD = isD ? 'd' : 's';
      DIP("fcvt%c%c %c%u, %c%u\n", ch, bitU == 1 ? 'u' : 's',
          sOrD, dd, sOrD, nn);
      return True;
   }

   if (size <= X01 && opcode == BITS5(1,1,1,0,1)) {
      
      
      Bool   isU = bitU == 1;
      Bool   isD = (size & 1) == 1;
      IRType tyI = isD ? Ity_I64 : Ity_I32;
      IROp   iop = isU ? (isD ? Iop_I64UtoF64 : Iop_I32UtoF32)
                       : (isD ? Iop_I64StoF64 : Iop_I32StoF32);
      IRTemp rm  = mk_get_IR_rounding_mode();
      putQRegLO(dd, binop(iop, mkexpr(rm), getQRegLO(nn, tyI)));
      if (!isD) {
         putQRegLane(dd, 1, mkU32(0)); 
      }
      putQRegLane(dd, 1, mkU64(0));    
      HChar c = isD ? 'd' : 's';
      DIP("%ccvtf %c%u, %c%u\n", isU ? 'u' : 's', c, dd, c, nn);
      return True;
   }

   if (size >= X10 && opcode == BITS5(1,1,1,0,1)) {
      
      
      Bool isSQRT = bitU == 1;
      Bool isD    = (size & 1) == 1;
      IROp op     = isSQRT ? (isD ? Iop_RSqrtEst64Fx2 : Iop_RSqrtEst32Fx4)
                           : (isD ? Iop_RecipEst64Fx2 : Iop_RecipEst32Fx4);
      IRTemp resV = newTempV128();
      assign(resV, unop(op, getQReg128(nn)));
      putQReg128(dd, mkexpr(math_ZERO_ALL_EXCEPT_LOWEST_LANE(isD ? X11 : X10,
                                                             mkexpr(resV))));
      HChar c = isD ? 'd' : 's';
      DIP("%s %c%u, %c%u\n", isSQRT ? "frsqrte" : "frecpe", c, dd, c, nn);
      return True;
   }

   if (bitU == 0 && size >= X10 && opcode == BITS5(1,1,1,1,1)) {
      
      Bool   isD = (size & 1) == 1;
      IRType ty  = isD ? Ity_F64 : Ity_F32;
      IROp   op  = isD ? Iop_RecpExpF64 : Iop_RecpExpF32;
      IRTemp res = newTemp(ty);
      IRTemp rm  = mk_get_IR_rounding_mode();
      assign(res, binop(op, mkexpr(rm), getQRegLane(nn, 0, ty)));
      putQReg128(dd, mkV128(0x0000));
      putQRegLane(dd, 0, mkexpr(res));
      HChar c = isD ? 'd' : 's';
      DIP("%s %c%u, %c%u\n", "frecpx", c, dd, c, nn);
      return True;
   }

   return False;
#  undef INSN
}


static
Bool dis_AdvSIMD_scalar_x_indexed_element(DisResult* dres, UInt insn)
{
#  define INSN(_bMax,_bMin)  SLICE_UInt(insn, (_bMax), (_bMin))
   if (INSN(31,30) != BITS2(0,1)
       || INSN(28,24) != BITS5(1,1,1,1,1) || INSN(10,10) !=0) {
      return False;
   }
   UInt bitU   = INSN(29,29);
   UInt size   = INSN(23,22);
   UInt bitL   = INSN(21,21);
   UInt bitM   = INSN(20,20);
   UInt mmLO4  = INSN(19,16);
   UInt opcode = INSN(15,12);
   UInt bitH   = INSN(11,11);
   UInt nn     = INSN(9,5);
   UInt dd     = INSN(4,0);
   vassert(size < 4);
   vassert(bitH < 2 && bitM < 2 && bitL < 2);

   if (bitU == 0 && size >= X10
       && (opcode == BITS4(0,0,0,1) || opcode == BITS4(0,1,0,1))) {
      
      
      Bool isD   = (size & 1) == 1;
      Bool isSUB = opcode == BITS4(0,1,0,1);
      UInt index;
      if      (!isD)             index = (bitH << 1) | bitL;
      else if (isD && bitL == 0) index = bitH;
      else return False; 
      vassert(index < (isD ? 2 : 4));
      IRType ity   = isD ? Ity_F64 : Ity_F32;
      IRTemp elem  = newTemp(ity);
      UInt   mm    = (bitM << 4) | mmLO4;
      assign(elem, getQRegLane(mm, index, ity));
      IRTemp dupd  = math_DUP_TO_V128(elem, ity);
      IROp   opADD = isD ? Iop_Add64Fx2 : Iop_Add32Fx4;
      IROp   opSUB = isD ? Iop_Sub64Fx2 : Iop_Sub32Fx4;
      IROp   opMUL = isD ? Iop_Mul64Fx2 : Iop_Mul32Fx4;
      IRTemp rm    = mk_get_IR_rounding_mode();
      IRTemp t1    = newTempV128();
      IRTemp t2    = newTempV128();
      
      assign(t1, triop(opMUL, mkexpr(rm), getQReg128(nn), mkexpr(dupd)));
      assign(t2, triop(isSUB ? opSUB : opADD,
                       mkexpr(rm), getQReg128(dd), mkexpr(t1)));
      putQReg128(dd,
                 mkexpr(math_ZERO_ALL_EXCEPT_LOWEST_LANE(isD ? 3 : 2,
                                                         mkexpr(t2))));
      const HChar c = isD ? 'd' : 's';
      DIP("%s %c%u, %c%u, %s.%c[%u]\n", isSUB ? "fmls" : "fmla",
          c, dd, c, nn, nameQReg128(mm), c, index);
      return True;
   }

   if (size >= X10 && opcode == BITS4(1,0,0,1)) {
      
      
      Bool isD    = (size & 1) == 1;
      Bool isMULX = bitU == 1;
      UInt index;
      if      (!isD)             index = (bitH << 1) | bitL;
      else if (isD && bitL == 0) index = bitH;
      else return False; 
      vassert(index < (isD ? 2 : 4));
      IRType ity   = isD ? Ity_F64 : Ity_F32;
      IRTemp elem  = newTemp(ity);
      UInt   mm    = (bitM << 4) | mmLO4;
      assign(elem, getQRegLane(mm, index, ity));
      IRTemp dupd  = math_DUP_TO_V128(elem, ity);
      IROp   opMUL = isD ? Iop_Mul64Fx2 : Iop_Mul32Fx4;
      IRTemp rm    = mk_get_IR_rounding_mode();
      IRTemp t1    = newTempV128();
      
      assign(t1, triop(opMUL, mkexpr(rm), getQReg128(nn), mkexpr(dupd)));
      putQReg128(dd,
                 mkexpr(math_ZERO_ALL_EXCEPT_LOWEST_LANE(isD ? 3 : 2,
                                                         mkexpr(t1))));
      const HChar c = isD ? 'd' : 's';
      DIP("%s %c%u, %c%u, %s.%c[%u]\n", isMULX ? "fmulx" : "fmul",
          c, dd, c, nn, nameQReg128(mm), c, index);
      return True;
   }

   if (bitU == 0 
       && (opcode == BITS4(1,0,1,1)
           || opcode == BITS4(0,0,1,1) || opcode == BITS4(0,1,1,1))) {
       
       
       
      
      UInt ks = 3;
      switch (opcode) {
         case BITS4(1,0,1,1): ks = 0; break;
         case BITS4(0,0,1,1): ks = 1; break;
         case BITS4(0,1,1,1): ks = 2; break;
         default: vassert(0);
      }
      vassert(ks >= 0 && ks <= 2);
      UInt mm  = 32; 
      UInt ix  = 16; 
      switch (size) {
         case X00:
            return False; 
         case X01:
            mm = mmLO4; ix = (bitH << 2) | (bitL << 1) | (bitM << 0); break;
         case X10:
            mm = (bitM << 4) | mmLO4; ix = (bitH << 1) | (bitL << 0); break;
         case X11:
            return False; 
         default:
            vassert(0);
      }
      vassert(mm < 32 && ix < 16);
      IRTemp vecN, vecD, res, sat1q, sat1n, sat2q, sat2n;
      vecN = vecD = res = sat1q = sat1n = sat2q = sat2n = IRTemp_INVALID;
      newTempsV128_2(&vecN, &vecD);
      assign(vecN, getQReg128(nn));
      IRTemp vecM  = math_DUP_VEC_ELEM(getQReg128(mm), size, ix);
      assign(vecD, getQReg128(dd));
      math_SQDMULL_ACC(&res, &sat1q, &sat1n, &sat2q, &sat2n,
                       False, size, "mas"[ks],
                       vecN, vecM, ks == 0 ? IRTemp_INVALID : vecD);
      IROp opZHI = mkVecZEROHIxxOFV128(size+1);
      putQReg128(dd, unop(opZHI, mkexpr(res)));
      vassert(sat1q != IRTemp_INVALID && sat1n != IRTemp_INVALID);
      updateQCFLAGwithDifferenceZHI(sat1q, sat1n, opZHI);
      if (sat2q != IRTemp_INVALID || sat2n != IRTemp_INVALID) {
         updateQCFLAGwithDifferenceZHI(sat2q, sat2n, opZHI);
      }
      const HChar* nm        = ks == 0 ? "sqmull"
                                       : (ks == 1 ? "sqdmlal" : "sqdmlsl");
      const HChar  arrNarrow = "bhsd"[size];
      const HChar  arrWide   = "bhsd"[size+1];
      DIP("%s %c%d, %c%d, v%d.%c[%u]\n",
          nm, arrWide, dd, arrNarrow, nn, dd, arrNarrow, ix);
      return True;
   }

   if (opcode == BITS4(1,1,0,0) || opcode == BITS4(1,1,0,1)) {
      
      
      UInt mm  = 32; 
      UInt ix  = 16; 
      switch (size) {
         case X00:
            return False; 
         case X01:
            mm = mmLO4; ix = (bitH << 2) | (bitL << 1) | (bitM << 0); break;
         case X10:
            mm = (bitM << 4) | mmLO4; ix = (bitH << 1) | (bitL << 0); break;
         case X11:
            return False; 
         default:
            vassert(0);
      }
      vassert(mm < 32 && ix < 16);
      Bool isR = opcode == BITS4(1,1,0,1);
      IRTemp res, sat1q, sat1n, vN, vM;
      res = sat1q = sat1n = vN = vM = IRTemp_INVALID;
      vN = newTempV128();
      assign(vN, getQReg128(nn));
      vM = math_DUP_VEC_ELEM(getQReg128(mm), size, ix);
      math_SQDMULH(&res, &sat1q, &sat1n, isR, size, vN, vM);
      IROp opZHI = mkVecZEROHIxxOFV128(size);
      putQReg128(dd, unop(opZHI, mkexpr(res)));
      updateQCFLAGwithDifferenceZHI(sat1q, sat1n, opZHI);
      const HChar* nm  = isR ? "sqrdmulh" : "sqdmulh";
      HChar ch         = size == X01 ? 'h' : 's';
      DIP("%s %c%d, %c%d, v%d.%c[%u]\n", nm, ch, dd, ch, nn, ch, dd, ix);
      return True;
   }

   return False;
#  undef INSN
}


static
Bool dis_AdvSIMD_shift_by_immediate(DisResult* dres, UInt insn)
{
#  define INSN(_bMax,_bMin)  SLICE_UInt(insn, (_bMax), (_bMin))
   if (INSN(31,31) != 0
       || INSN(28,23) != BITS6(0,1,1,1,1,0) || INSN(10,10) != 1) {
      return False;
   }
   UInt bitQ   = INSN(30,30);
   UInt bitU   = INSN(29,29);
   UInt immh   = INSN(22,19);
   UInt immb   = INSN(18,16);
   UInt opcode = INSN(15,11);
   UInt nn     = INSN(9,5);
   UInt dd     = INSN(4,0);

   if (opcode == BITS5(0,0,0,0,0) || opcode == BITS5(0,0,0,1,0)) {
      
      
      
      
      UInt size  = 0;
      UInt shift = 0;
      Bool isQ   = bitQ == 1;
      Bool isU   = bitU == 1;
      Bool isAcc = opcode == BITS5(0,0,0,1,0);
      Bool ok    = getLaneInfo_IMMH_IMMB(&shift, &size, immh, immb);
      if (!ok || (bitQ == 0 && size == X11)) return False;
      vassert(size >= 0 && size <= 3);
      UInt lanebits = 8 << size;
      vassert(shift >= 1 && shift <= lanebits);
      IROp    op  = isU ? mkVecSHRN(size) : mkVecSARN(size);
      IRExpr* src = getQReg128(nn);
      IRTemp  shf = newTempV128();
      IRTemp  res = newTempV128();
      if (shift == lanebits && isU) {
         assign(shf, mkV128(0x0000));
      } else {
         UInt nudge = 0;
         if (shift == lanebits) {
            vassert(!isU);
            nudge = 1;
         }
         assign(shf, binop(op, src, mkU8(shift - nudge)));
      }
      assign(res, isAcc ? binop(mkVecADD(size), getQReg128(dd), mkexpr(shf))
                        : mkexpr(shf));
      putQReg128(dd, math_MAYBE_ZERO_HI64(bitQ, res));
      HChar laneCh = "bhsd"[size];
      UInt  nLanes = (isQ ? 128 : 64) / lanebits;
      const HChar* nm = isAcc ? (isU ? "usra" : "ssra")
                              : (isU ? "ushr" : "sshr");
      DIP("%s %s.%u%c, %s.%u%c, #%u\n", nm,
          nameQReg128(dd), nLanes, laneCh,
          nameQReg128(nn), nLanes, laneCh, shift);
      return True;
   }

   if (opcode == BITS5(0,0,1,0,0) || opcode == BITS5(0,0,1,1,0)) {
      
      
      
      
      UInt size  = 0;
      UInt shift = 0;
      Bool isQ   = bitQ == 1;
      Bool isU   = bitU == 1;
      Bool isAcc = opcode == BITS5(0,0,1,1,0);
      Bool ok    = getLaneInfo_IMMH_IMMB(&shift, &size, immh, immb);
      if (!ok || (bitQ == 0 && size == X11)) return False;
      vassert(size >= 0 && size <= 3);
      UInt lanebits = 8 << size;
      vassert(shift >= 1 && shift <= lanebits);
      IROp    op   = isU ? mkVecRSHU(size) : mkVecRSHS(size);
      IRExpr* src  = getQReg128(nn);
      IRTemp  imm8 = newTemp(Ity_I8);
      assign(imm8, mkU8((UChar)(-shift)));
      IRExpr* amt  = mkexpr(math_DUP_TO_V128(imm8, Ity_I8));
      IRTemp  shf  = newTempV128();
      IRTemp  res  = newTempV128();
      assign(shf, binop(op, src, amt));
      assign(res, isAcc ? binop(mkVecADD(size), getQReg128(dd), mkexpr(shf))
                        : mkexpr(shf));
      putQReg128(dd, math_MAYBE_ZERO_HI64(bitQ, res));
      HChar laneCh = "bhsd"[size];
      UInt  nLanes = (isQ ? 128 : 64) / lanebits;
      const HChar* nm = isAcc ? (isU ? "ursra" : "srsra")
                              : (isU ? "urshr" : "srshr");
      DIP("%s %s.%u%c, %s.%u%c, #%u\n", nm,
          nameQReg128(dd), nLanes, laneCh,
          nameQReg128(nn), nLanes, laneCh, shift);
      return True;
   }

   if (bitU == 1 && opcode == BITS5(0,1,0,0,0)) {
      
      UInt size  = 0;
      UInt shift = 0;
      Bool isQ   = bitQ == 1;
      Bool ok    = getLaneInfo_IMMH_IMMB(&shift, &size, immh, immb);
      if (!ok || (bitQ == 0 && size == X11)) return False;
      vassert(size >= 0 && size <= 3);
      UInt lanebits = 8 << size;
      vassert(shift >= 1 && shift <= lanebits);
      IRExpr* src = getQReg128(nn);
      IRTemp  res = newTempV128();
      if (shift == lanebits) {
         assign(res, getQReg128(dd));
      } else {
         assign(res, binop(mkVecSHRN(size), src, mkU8(shift)));
         IRExpr* nmask = binop(mkVecSHLN(size),
                               mkV128(0xFFFF), mkU8(lanebits - shift));
         IRTemp  tmp   = newTempV128();
         assign(tmp, binop(Iop_OrV128,
                           mkexpr(res),
                           binop(Iop_AndV128, getQReg128(dd), nmask)));
         res = tmp;
      }
      putQReg128(dd, math_MAYBE_ZERO_HI64(bitQ, res));
      HChar laneCh = "bhsd"[size];
      UInt  nLanes = (isQ ? 128 : 64) / lanebits;
      DIP("%s %s.%u%c, %s.%u%c, #%u\n", "sri",
          nameQReg128(dd), nLanes, laneCh,
          nameQReg128(nn), nLanes, laneCh, shift);
      return True;
   }

   if (opcode == BITS5(0,1,0,1,0)) {
      
      
      UInt size  = 0;
      UInt shift = 0;
      Bool isSLI = bitU == 1;
      Bool isQ   = bitQ == 1;
      Bool ok    = getLaneInfo_IMMH_IMMB(&shift, &size, immh, immb);
      if (!ok || (bitQ == 0 && size == X11)) return False;
      vassert(size >= 0 && size <= 3);
      UInt lanebits = 8 << size;
      shift = lanebits - shift;
      vassert(shift >= 0 && shift < lanebits);
      IROp    op  = mkVecSHLN(size);
      IRExpr* src = getQReg128(nn);
      IRTemp  res = newTempV128();
      if (shift == 0) {
         assign(res, src);
      } else {
         assign(res, binop(op, src, mkU8(shift)));
         if (isSLI) {
            IRExpr* nmask = binop(mkVecSHRN(size),
                                  mkV128(0xFFFF), mkU8(lanebits - shift));
            IRTemp  tmp   = newTempV128();
            assign(tmp, binop(Iop_OrV128,
                              mkexpr(res),
                              binop(Iop_AndV128, getQReg128(dd), nmask)));
            res = tmp;
         }
      }
      putQReg128(dd, math_MAYBE_ZERO_HI64(bitQ, res));
      HChar laneCh = "bhsd"[size];
      UInt  nLanes = (isQ ? 128 : 64) / lanebits;
      const HChar* nm = isSLI ? "sli" : "shl";
      DIP("%s %s.%u%c, %s.%u%c, #%u\n", nm,
          nameQReg128(dd), nLanes, laneCh,
          nameQReg128(nn), nLanes, laneCh, shift);
      return True;
   }

   if (opcode == BITS5(0,1,1,1,0)
       || (bitU == 1 && opcode == BITS5(0,1,1,0,0))) {
      
      
      
      UInt size  = 0;
      UInt shift = 0;
      Bool isQ   = bitQ == 1;
      Bool ok    = getLaneInfo_IMMH_IMMB(&shift, &size, immh, immb);
      if (!ok || (bitQ == 0 && size == X11)) return False;
      vassert(size >= 0 && size <= 3);
      UInt lanebits = 8 << size;
      shift = lanebits - shift;
      vassert(shift >= 0 && shift < lanebits);
      const HChar* nm = NULL;
       if (bitU == 0 && opcode == BITS5(0,1,1,1,0)) nm = "sqshl";
      else if (bitU == 1 && opcode == BITS5(0,1,1,1,0)) nm = "uqshl";
      else if (bitU == 1 && opcode == BITS5(0,1,1,0,0)) nm = "sqshlu";
      else vassert(0);
      IRTemp qDiff1 = IRTemp_INVALID;
      IRTemp qDiff2 = IRTemp_INVALID;
      IRTemp res = IRTemp_INVALID;
      IRTemp src = newTempV128();
      assign(src, getQReg128(nn));
      math_QSHL_IMM(&res, &qDiff1, &qDiff2, src, size, shift, nm);
      putQReg128(dd, math_MAYBE_ZERO_HI64(bitQ, res));
      updateQCFLAGwithDifferenceZHI(qDiff1, qDiff2,
                                    isQ ? Iop_INVALID : Iop_ZeroHI64ofV128);
      const HChar* arr = nameArr_Q_SZ(bitQ, size);
      DIP("%s %s.%s, %s.%s, #%u\n", nm,
          nameQReg128(dd), arr, nameQReg128(nn), arr, shift);
      return True;
   }

   if (bitU == 0
       && (opcode == BITS5(1,0,0,0,0) || opcode == BITS5(1,0,0,0,1))) {
      
      
      
      UInt size  = 0;
      UInt shift = 0;
      Bool is2   = bitQ == 1;
      Bool isR   = opcode == BITS5(1,0,0,0,1);
      Bool ok    = getLaneInfo_IMMH_IMMB(&shift, &size, immh, immb);
      if (!ok || size == X11) return False;
      vassert(shift >= 1);
      IRTemp t1 = newTempV128();
      IRTemp t2 = newTempV128();
      IRTemp t3 = newTempV128();
      assign(t1, getQReg128(nn));
      assign(t2, isR ? binop(mkVecADD(size+1),
                             mkexpr(t1),
                             mkexpr(math_VEC_DUP_IMM(size+1, 1ULL<<(shift-1))))
                     : mkexpr(t1));
      assign(t3, binop(mkVecSHRN(size+1), mkexpr(t2), mkU8(shift)));
      IRTemp t4 = math_NARROW_LANES(t3, t3, size);
      putLO64andZUorPutHI64(is2, dd, t4);
      const HChar* arrNarrow = nameArr_Q_SZ(bitQ, size);
      const HChar* arrWide   = nameArr_Q_SZ(1,    size+1);
      DIP("%s %s.%s, %s.%s, #%u\n", isR ? "rshrn" : "shrn",
          nameQReg128(dd), arrNarrow, nameQReg128(nn), arrWide, shift);
      return True;
   }

   if (opcode == BITS5(1,0,0,1,0) || opcode == BITS5(1,0,0,1,1)
       || (bitU == 1
           && (opcode == BITS5(1,0,0,0,0) || opcode == BITS5(1,0,0,0,1)))) {
      
      
      
      
      
      
      UInt size  = 0;
      UInt shift = 0;
      Bool is2   = bitQ == 1;
      Bool ok    = getLaneInfo_IMMH_IMMB(&shift, &size, immh, immb);
      if (!ok || size == X11) return False;
      vassert(shift >= 1 && shift <= (8 << size));
      const HChar* nm = "??";
      IROp op = Iop_INVALID;
      
       if (bitU == 0 && opcode == BITS5(1,0,0,1,0)) {
         nm = "sqshrn"; op = mkVecQANDqsarNNARROWSS(size);
      }
      else if (bitU == 1 && opcode == BITS5(1,0,0,1,0)) {
         nm = "uqshrn"; op = mkVecQANDqshrNNARROWUU(size);
      }
      else if (bitU == 0 && opcode == BITS5(1,0,0,1,1)) {
         nm = "sqrshrn"; op = mkVecQANDqrsarNNARROWSS(size);
      }
      else if (bitU == 1 && opcode == BITS5(1,0,0,1,1)) {
         nm = "uqrshrn"; op = mkVecQANDqrshrNNARROWUU(size);
      }
      else if (bitU == 1 && opcode == BITS5(1,0,0,0,0)) {
         nm = "sqshrun"; op = mkVecQANDqsarNNARROWSU(size);
      }
      else if (bitU == 1 && opcode == BITS5(1,0,0,0,1)) {
         nm = "sqrshrun"; op = mkVecQANDqrsarNNARROWSU(size);
      }
      else vassert(0);
      
      IRTemp src128 = newTempV128();
      assign(src128, getQReg128(nn));
      IRTemp pair = newTempV128();
      assign(pair, binop(op, mkexpr(src128), mkU8(shift)));
      
      IRTemp res64in128 = newTempV128();
      assign(res64in128, unop(Iop_ZeroHI64ofV128, mkexpr(pair)));
      putLO64andZUorPutHI64(is2, dd, res64in128);
      
      IRTemp q64q64 = newTempV128();
      assign(q64q64, binop(Iop_InterleaveHI64x2, mkexpr(pair), mkexpr(pair)));
      IRTemp z128 = newTempV128();
      assign(z128, mkV128(0x0000));
      updateQCFLAGwithDifference(q64q64, z128);
      
      const HChar* arrNarrow = nameArr_Q_SZ(bitQ, size);
      const HChar* arrWide   = nameArr_Q_SZ(1,    size+1);
      DIP("%s %s.%s, %s.%s, #%u\n", nm,
          nameQReg128(dd), arrNarrow, nameQReg128(nn), arrWide, shift);
      return True;
   }

   if (opcode == BITS5(1,0,1,0,0)) {
      
      
      Bool    isQ   = bitQ == 1;
      Bool    isU   = bitU == 1;
      UInt    immhb = (immh << 3) | immb;
      IRTemp  src   = newTempV128();
      IRTemp  zero  = newTempV128();
      IRExpr* res   = NULL;
      UInt    sh    = 0;
      const HChar* ta = "??";
      const HChar* tb = "??";
      assign(src, getQReg128(nn));
      assign(zero, mkV128(0x0000));
      if (immh & 8) {
         
      }
      else if (immh & 4) {
         sh = immhb - 32;
         vassert(sh < 32); 
         ta = "2d";
         tb = isQ ? "4s" : "2s";
         IRExpr* tmp = isQ ? mk_InterleaveHI32x4(src, zero) 
                           : mk_InterleaveLO32x4(src, zero);
         res = binop(isU ? Iop_ShrN64x2 : Iop_SarN64x2, tmp, mkU8(32-sh));
      }
      else if (immh & 2) {
         sh = immhb - 16;
         vassert(sh < 16); 
         ta = "4s";
         tb = isQ ? "8h" : "4h";
         IRExpr* tmp = isQ ? mk_InterleaveHI16x8(src, zero) 
                           : mk_InterleaveLO16x8(src, zero);
         res = binop(isU ? Iop_ShrN32x4 : Iop_SarN32x4, tmp, mkU8(16-sh));
      }
      else if (immh & 1) {
         sh = immhb - 8;
         vassert(sh < 8); 
         ta = "8h";
         tb = isQ ? "16b" : "8b";
         IRExpr* tmp = isQ ? mk_InterleaveHI8x16(src, zero) 
                           : mk_InterleaveLO8x16(src, zero);
         res = binop(isU ? Iop_ShrN16x8 : Iop_SarN16x8, tmp, mkU8(8-sh));
      } else {
         vassert(immh == 0);
         
      }
      
      if (res) {
         putQReg128(dd, res);
         DIP("%cshll%s %s.%s, %s.%s, #%d\n",
             isU ? 'u' : 's', isQ ? "2" : "",
             nameQReg128(dd), ta, nameQReg128(nn), tb, sh);
         return True;
      }
      return False;
   }

   if (opcode == BITS5(1,1,1,0,0)) {
      
      
      
      if (immh < BITS4(0,1,0,0)) return False;
      UInt size  = 0;
      UInt fbits = 0;
      Bool ok    = getLaneInfo_IMMH_IMMB(&fbits, &size, immh, immb);
      
      vassert(ok);
      
      vassert(size == X10 || size == X11);
      Bool isD = size == X11;
      Bool isU = bitU == 1;
      Bool isQ = bitQ == 1;
      if (isD && !isQ) return False; 
      vassert(fbits >= 1 && fbits <= (isD ? 64 : 32));
      Double  scale  = two_to_the_minus(fbits);
      IRExpr* scaleE = isD ? IRExpr_Const(IRConst_F64(scale))
                           : IRExpr_Const(IRConst_F32( (Float)scale ));
      IROp    opMUL  = isD ? Iop_MulF64 : Iop_MulF32;
      IROp    opCVT  = isU ? (isD ? Iop_I64UtoF64 : Iop_I32UtoF32)
                           : (isD ? Iop_I64StoF64 : Iop_I32StoF32);
      IRType tyF = isD ? Ity_F64 : Ity_F32;
      IRType tyI = isD ? Ity_I64 : Ity_I32;
      UInt nLanes = (isQ ? 2 : 1) * (isD ? 1 : 2);
      vassert(nLanes == 2 || nLanes == 4);
      for (UInt i = 0; i < nLanes; i++) {
         IRTemp src = newTemp(tyI);
         IRTemp res = newTemp(tyF);
         IRTemp rm  = mk_get_IR_rounding_mode();
         assign(src, getQRegLane(nn, i, tyI));
         assign(res, triop(opMUL, mkexpr(rm),
                                  binop(opCVT, mkexpr(rm), mkexpr(src)),
                                  scaleE));
         putQRegLane(dd, i, mkexpr(res));
      }
      if (!isQ) {
         putQRegLane(dd, 1, mkU64(0));
      }
      const HChar* arr = nameArr_Q_SZ(bitQ, size);
      DIP("%s %s.%s, %s.%s, #%u\n", isU ? "ucvtf" : "scvtf",
          nameQReg128(dd), arr, nameQReg128(nn), arr, fbits);
      return True;
   }

   if (opcode == BITS5(1,1,1,1,1)) {
      
      
      
      if (immh < BITS4(0,1,0,0)) return False;
      UInt size  = 0;
      UInt fbits = 0;
      Bool ok    = getLaneInfo_IMMH_IMMB(&fbits, &size, immh, immb);
      
      vassert(ok);
      
      vassert(size == X10 || size == X11);
      Bool isD = size == X11;
      Bool isU = bitU == 1;
      Bool isQ = bitQ == 1;
      if (isD && !isQ) return False; 
      vassert(fbits >= 1 && fbits <= (isD ? 64 : 32));
      Double  scale  = two_to_the_plus(fbits);
      IRExpr* scaleE = isD ? IRExpr_Const(IRConst_F64(scale))
                           : IRExpr_Const(IRConst_F32( (Float)scale ));
      IROp    opMUL  = isD ? Iop_MulF64 : Iop_MulF32;
      IROp    opCVT  = isU ? (isD ? Iop_F64toI64U : Iop_F32toI32U)
                           : (isD ? Iop_F64toI64S : Iop_F32toI32S);
      IRType tyF = isD ? Ity_F64 : Ity_F32;
      IRType tyI = isD ? Ity_I64 : Ity_I32;
      UInt nLanes = (isQ ? 2 : 1) * (isD ? 1 : 2);
      vassert(nLanes == 2 || nLanes == 4);
      for (UInt i = 0; i < nLanes; i++) {
         IRTemp src = newTemp(tyF);
         IRTemp res = newTemp(tyI);
         IRTemp rm  = newTemp(Ity_I32);
         assign(src, getQRegLane(nn, i, tyF));
         assign(rm,  mkU32(Irrm_ZERO));
         assign(res, binop(opCVT, mkexpr(rm), 
                                  triop(opMUL, mkexpr(rm),
                                               mkexpr(src), scaleE)));
         putQRegLane(dd, i, mkexpr(res));
      }
      if (!isQ) {
         putQRegLane(dd, 1, mkU64(0));
      }
      const HChar* arr = nameArr_Q_SZ(bitQ, size);
      DIP("%s %s.%s, %s.%s, #%u\n", isU ? "fcvtzu" : "fcvtzs",
          nameQReg128(dd), arr, nameQReg128(nn), arr, fbits);
      return True;
   }

#  define INSN(_bMax,_bMin)  SLICE_UInt(insn, (_bMax), (_bMin))
   return False;
#  undef INSN
}


static
Bool dis_AdvSIMD_three_different(DisResult* dres, UInt insn)
{
#  define INSN(_bMax,_bMin)  SLICE_UInt(insn, (_bMax), (_bMin))
   if (INSN(31,31) != 0
       || INSN(28,24) != BITS5(0,1,1,1,0)
       || INSN(21,21) != 1
       || INSN(11,10) != BITS2(0,0)) {
      return False;
   }
   UInt bitQ   = INSN(30,30);
   UInt bitU   = INSN(29,29);
   UInt size   = INSN(23,22);
   UInt mm     = INSN(20,16);
   UInt opcode = INSN(15,12);
   UInt nn     = INSN(9,5);
   UInt dd     = INSN(4,0);
   vassert(size < 4);
   Bool is2    = bitQ == 1;

   if (opcode == BITS4(0,0,0,0) || opcode == BITS4(0,0,1,0)) {
      
      
      
      
      
      if (size == X11) return False;
      vassert(size <= 2);
      Bool   isU   = bitU == 1;
      Bool   isADD = opcode == BITS4(0,0,0,0);
      IRTemp argL  = math_WIDEN_LO_OR_HI_LANES(isU, is2, size, getQReg128(nn));
      IRTemp argR  = math_WIDEN_LO_OR_HI_LANES(isU, is2, size, getQReg128(mm));
      IRTemp res   = newTempV128();
      assign(res, binop(isADD ? mkVecADD(size+1) : mkVecSUB(size+1),
                        mkexpr(argL), mkexpr(argR)));
      putQReg128(dd, mkexpr(res));
      const HChar* arrNarrow = nameArr_Q_SZ(bitQ, size);
      const HChar* arrWide   = nameArr_Q_SZ(1,    size+1);
      const HChar* nm        = isADD ? (isU ? "uaddl" : "saddl")
                                     : (isU ? "usubl" : "ssubl");
      DIP("%s%s %s.%s, %s.%s, %s.%s\n", nm, is2 ? "2" : "",
          nameQReg128(dd), arrWide,
          nameQReg128(nn), arrNarrow, nameQReg128(mm), arrNarrow);
      return True;
   }

   if (opcode == BITS4(0,0,0,1) || opcode == BITS4(0,0,1,1)) {
      
      
      
      
      
      if (size == X11) return False;
      vassert(size <= 2);
      Bool   isU   = bitU == 1;
      Bool   isADD = opcode == BITS4(0,0,0,1);
      IRTemp argR  = math_WIDEN_LO_OR_HI_LANES(isU, is2, size, getQReg128(mm));
      IRTemp res   = newTempV128();
      assign(res, binop(isADD ? mkVecADD(size+1) : mkVecSUB(size+1),
                        getQReg128(nn), mkexpr(argR)));
      putQReg128(dd, mkexpr(res));
      const HChar* arrNarrow = nameArr_Q_SZ(bitQ, size);
      const HChar* arrWide   = nameArr_Q_SZ(1,    size+1);
      const HChar* nm        = isADD ? (isU ? "uaddw" : "saddw")
                                     : (isU ? "usubw" : "ssubw");
      DIP("%s%s %s.%s, %s.%s, %s.%s\n", nm, is2 ? "2" : "",
          nameQReg128(dd), arrWide,
          nameQReg128(nn), arrWide, nameQReg128(mm), arrNarrow);
      return True;
   }

   if (opcode == BITS4(0,1,0,0) || opcode == BITS4(0,1,1,0)) {
      
      
      
      
      
      if (size == X11) return False;
      vassert(size <= 2);
      const UInt shift[3] = { 8, 16, 32 };
      Bool isADD = opcode == BITS4(0,1,0,0);
      Bool isR   = bitU == 1;
      
      IRTemp  wide  = newTempV128();
      IRExpr* wideE = binop(isADD ? mkVecADD(size+1) : mkVecSUB(size+1),
                            getQReg128(nn), getQReg128(mm));
      if (isR) {
         wideE = binop(mkVecADD(size+1),
                       wideE,
                       mkexpr(math_VEC_DUP_IMM(size+1,
                                               1ULL << (shift[size]-1))));
      }
      assign(wide, wideE);
      
      IRTemp shrd = newTempV128();
      assign(shrd, binop(mkVecSHRN(size+1), mkexpr(wide), mkU8(shift[size])));
      
      IRTemp new64 = newTempV128();
      assign(new64, binop(mkVecCATEVENLANES(size), mkexpr(shrd), mkexpr(shrd)));
      putLO64andZUorPutHI64(is2, dd, new64);
      const HChar* arrNarrow = nameArr_Q_SZ(bitQ, size);
      const HChar* arrWide   = nameArr_Q_SZ(1,    size+1);
      const HChar* nm = isADD ? (isR ? "raddhn" : "addhn")
                              : (isR ? "rsubhn" : "subhn");
      DIP("%s%s %s.%s, %s.%s, %s.%s\n", nm, is2 ? "2" : "",
          nameQReg128(dd), arrNarrow,
          nameQReg128(nn), arrWide, nameQReg128(mm), arrWide);
      return True;
   }

   if (opcode == BITS4(0,1,0,1) || opcode == BITS4(0,1,1,1)) {
      
      
      
      
      
      if (size == X11) return False;
      vassert(size <= 2);
      Bool   isU   = bitU == 1;
      Bool   isACC = opcode == BITS4(0,1,0,1);
      IRTemp argL  = math_WIDEN_LO_OR_HI_LANES(isU, is2, size, getQReg128(nn));
      IRTemp argR  = math_WIDEN_LO_OR_HI_LANES(isU, is2, size, getQReg128(mm));
      IRTemp abd   = math_ABD(isU, size+1, mkexpr(argL), mkexpr(argR));
      IRTemp res   = newTempV128();
      assign(res, isACC ? binop(mkVecADD(size+1), mkexpr(abd), getQReg128(dd))
                        : mkexpr(abd));
      putQReg128(dd, mkexpr(res));
      const HChar* arrNarrow = nameArr_Q_SZ(bitQ, size);
      const HChar* arrWide   = nameArr_Q_SZ(1,    size+1);
      const HChar* nm        = isACC ? (isU ? "uabal" : "sabal")
                                     : (isU ? "uabdl" : "sabdl");
      DIP("%s%s %s.%s, %s.%s, %s.%s\n", nm, is2 ? "2" : "",
          nameQReg128(dd), arrWide,
          nameQReg128(nn), arrNarrow, nameQReg128(mm), arrNarrow);
      return True;
   }

   if (opcode == BITS4(1,1,0,0)
       || opcode == BITS4(1,0,0,0) || opcode == BITS4(1,0,1,0)) {
       
       
       
       
       
       
      
      UInt ks = 3;
      switch (opcode) {
         case BITS4(1,1,0,0): ks = 0; break;
         case BITS4(1,0,0,0): ks = 1; break;
         case BITS4(1,0,1,0): ks = 2; break;
         default: vassert(0);
      }
      vassert(ks >= 0 && ks <= 2);
      if (size == X11) return False;
      vassert(size <= 2);
      Bool   isU  = bitU == 1;
      IRTemp vecN = newTempV128();
      IRTemp vecM = newTempV128();
      IRTemp vecD = newTempV128();
      assign(vecN, getQReg128(nn));
      assign(vecM, getQReg128(mm));
      assign(vecD, getQReg128(dd));
      IRTemp res = IRTemp_INVALID;
      math_MULL_ACC(&res, is2, isU, size, "mas"[ks],
                    vecN, vecM, ks == 0 ? IRTemp_INVALID : vecD);
      putQReg128(dd, mkexpr(res));
      const HChar* arrNarrow = nameArr_Q_SZ(bitQ, size);
      const HChar* arrWide   = nameArr_Q_SZ(1,    size+1);
      const HChar* nm        = ks == 0 ? "mull" : (ks == 1 ? "mlal" : "mlsl");
      DIP("%c%s%s %s.%s, %s.%s, %s.%s\n", isU ? 'u' : 's', nm, is2 ? "2" : "",
          nameQReg128(dd), arrWide,
          nameQReg128(nn), arrNarrow, nameQReg128(mm), arrNarrow);
      return True;
   }

   if (bitU == 0
       && (opcode == BITS4(1,1,0,1)
           || opcode == BITS4(1,0,0,1) || opcode == BITS4(1,0,1,1))) {
       
       
       
      
      UInt ks = 3;
      switch (opcode) {
         case BITS4(1,1,0,1): ks = 0; break;
         case BITS4(1,0,0,1): ks = 1; break;
         case BITS4(1,0,1,1): ks = 2; break;
         default: vassert(0);
      }
      vassert(ks >= 0 && ks <= 2);
      if (size == X00 || size == X11) return False;
      vassert(size <= 2);
      IRTemp vecN, vecM, vecD, res, sat1q, sat1n, sat2q, sat2n;
      vecN = vecM = vecD = res = sat1q = sat1n = sat2q = sat2n = IRTemp_INVALID;
      newTempsV128_3(&vecN, &vecM, &vecD);
      assign(vecN, getQReg128(nn));
      assign(vecM, getQReg128(mm));
      assign(vecD, getQReg128(dd));
      math_SQDMULL_ACC(&res, &sat1q, &sat1n, &sat2q, &sat2n,
                       is2, size, "mas"[ks],
                       vecN, vecM, ks == 0 ? IRTemp_INVALID : vecD);
      putQReg128(dd, mkexpr(res));
      vassert(sat1q != IRTemp_INVALID && sat1n != IRTemp_INVALID);
      updateQCFLAGwithDifference(sat1q, sat1n);
      if (sat2q != IRTemp_INVALID || sat2n != IRTemp_INVALID) {
         updateQCFLAGwithDifference(sat2q, sat2n);
      }
      const HChar* arrNarrow = nameArr_Q_SZ(bitQ, size);
      const HChar* arrWide   = nameArr_Q_SZ(1,    size+1);
      const HChar* nm        = ks == 0 ? "sqdmull"
                                       : (ks == 1 ? "sqdmlal" : "sqdmlsl");
      DIP("%s%s %s.%s, %s.%s, %s.%s\n", nm, is2 ? "2" : "",
          nameQReg128(dd), arrWide,
          nameQReg128(nn), arrNarrow, nameQReg128(mm), arrNarrow);
      return True;
   }

   if (bitU == 0 && opcode == BITS4(1,1,1,0)) {
      
      
      if (size != X00) return False;
      IRTemp res
         = math_BINARY_WIDENING_V128(is2, Iop_PolynomialMull8x8,
                                     getQReg128(nn), getQReg128(mm));
      putQReg128(dd, mkexpr(res));
      const HChar* arrNarrow = nameArr_Q_SZ(bitQ, size);
      const HChar* arrWide   = nameArr_Q_SZ(1,    size+1);
      DIP("%s%s %s.%s, %s.%s, %s.%s\n", "pmull", is2 ? "2" : "",
          nameQReg128(dd), arrNarrow,
          nameQReg128(nn), arrWide, nameQReg128(mm), arrWide);
      return True;
   }

   return False;
#  undef INSN
}


static
Bool dis_AdvSIMD_three_same(DisResult* dres, UInt insn)
{
#  define INSN(_bMax,_bMin)  SLICE_UInt(insn, (_bMax), (_bMin))
   if (INSN(31,31) != 0
       || INSN(28,24) != BITS5(0,1,1,1,0)
       || INSN(21,21) != 1
       || INSN(10,10) != 1) {
      return False;
   }
   UInt bitQ   = INSN(30,30);
   UInt bitU   = INSN(29,29);
   UInt size   = INSN(23,22);
   UInt mm     = INSN(20,16);
   UInt opcode = INSN(15,11);
   UInt nn     = INSN(9,5);
   UInt dd     = INSN(4,0);
   vassert(size < 4);

   if (opcode == BITS5(0,0,0,0,0) || opcode == BITS5(0,0,1,0,0)) {
      
      
      
      
      if (size == X11) return False;
      Bool isADD = opcode == BITS5(0,0,0,0,0);
      Bool isU   = bitU == 1;
      
      IRTemp argL   = newTempV128();
      IRTemp argLhi = IRTemp_INVALID;
      IRTemp argLlo = IRTemp_INVALID;
      IRTemp argR   = newTempV128();
      IRTemp argRhi = IRTemp_INVALID;
      IRTemp argRlo = IRTemp_INVALID;
      IRTemp resHi  = newTempV128();
      IRTemp resLo  = newTempV128();
      IRTemp res    = IRTemp_INVALID;
      assign(argL, getQReg128(nn));
      argLlo = math_WIDEN_LO_OR_HI_LANES(isU, False, size, mkexpr(argL));
      argLhi = math_WIDEN_LO_OR_HI_LANES(isU, True,  size, mkexpr(argL));
      assign(argR, getQReg128(mm));
      argRlo = math_WIDEN_LO_OR_HI_LANES(isU, False, size, mkexpr(argR));
      argRhi = math_WIDEN_LO_OR_HI_LANES(isU, True,  size, mkexpr(argR));
      IROp opADDSUB = isADD ? mkVecADD(size+1) : mkVecSUB(size+1);
      IROp opSxR = isU ? mkVecSHRN(size+1) : mkVecSARN(size+1);
      assign(resHi, binop(opSxR,
                          binop(opADDSUB, mkexpr(argLhi), mkexpr(argRhi)),
                          mkU8(1)));
      assign(resLo, binop(opSxR,
                          binop(opADDSUB, mkexpr(argLlo), mkexpr(argRlo)),
                          mkU8(1)));
      res = math_NARROW_LANES ( resHi, resLo, size );
      putQReg128(dd, math_MAYBE_ZERO_HI64(bitQ, res));
      const HChar* nm  = isADD ? (isU ? "uhadd" : "shadd") 
                               : (isU ? "uhsub" : "shsub");
      const HChar* arr = nameArr_Q_SZ(bitQ, size);
      DIP("%s %s.%s, %s.%s, %s.%s\n", nm,
          nameQReg128(dd), arr, nameQReg128(nn), arr, nameQReg128(mm), arr);
      return True;
   }

   if (opcode == BITS5(0,0,0,1,0)) {
      
      
      if (bitQ == 0 && size == X11) return False; 
      Bool   isU  = bitU == 1;
      IRTemp argL = newTempV128();
      IRTemp argR = newTempV128();
      assign(argL, getQReg128(nn));
      assign(argR, getQReg128(mm));
      IRTemp res = math_RHADD(size, isU, argL, argR);
      putQReg128(dd, math_MAYBE_ZERO_HI64(bitQ, res));
      const HChar* arr = nameArr_Q_SZ(bitQ, size);
      DIP("%s %s.%s, %s.%s, %s.%s\n", isU ? "urhadd" : "srhadd",
          nameQReg128(dd), arr, nameQReg128(nn), arr, nameQReg128(mm), arr);
      return True;
   }

   if (opcode == BITS5(0,0,0,0,1) || opcode == BITS5(0,0,1,0,1)) {
      
      
      
      
      if (bitQ == 0 && size == X11) return False; 
      Bool isADD = opcode == BITS5(0,0,0,0,1);
      Bool isU   = bitU == 1;
      IROp qop   = Iop_INVALID;
      IROp nop   = Iop_INVALID;
      if (isADD) {
         qop = isU ? mkVecQADDU(size) : mkVecQADDS(size);
         nop = mkVecADD(size);
      } else {
         qop = isU ? mkVecQSUBU(size) : mkVecQSUBS(size);
         nop = mkVecSUB(size);
      }
      IRTemp argL = newTempV128();
      IRTemp argR = newTempV128();
      IRTemp qres = newTempV128();
      IRTemp nres = newTempV128();
      assign(argL, getQReg128(nn));
      assign(argR, getQReg128(mm));
      assign(qres, math_MAYBE_ZERO_HI64_fromE(
                      bitQ, binop(qop, mkexpr(argL), mkexpr(argR))));
      assign(nres, math_MAYBE_ZERO_HI64_fromE(
                      bitQ, binop(nop, mkexpr(argL), mkexpr(argR))));
      putQReg128(dd, mkexpr(qres));
      updateQCFLAGwithDifference(qres, nres);
      const HChar* nm  = isADD ? (isU ? "uqadd" : "sqadd") 
                               : (isU ? "uqsub" : "sqsub");
      const HChar* arr = nameArr_Q_SZ(bitQ, size);
      DIP("%s %s.%s, %s.%s, %s.%s\n", nm,
          nameQReg128(dd), arr, nameQReg128(nn), arr, nameQReg128(mm), arr);
      return True;
   }

   if (bitU == 0 && opcode == BITS5(0,0,0,1,1)) {
      
      
      
      
      Bool   isORx  = (size & 2) == 2;
      Bool   invert = (size & 1) == 1;
      IRTemp res    = newTempV128();
      assign(res, binop(isORx ? Iop_OrV128 : Iop_AndV128,
                        getQReg128(nn),
                        invert ? unop(Iop_NotV128, getQReg128(mm))
                               : getQReg128(mm)));
      putQReg128(dd, math_MAYBE_ZERO_HI64(bitQ, res));
      const HChar* names[4] = { "and", "bic", "orr", "orn" };
      const HChar* ar = bitQ == 1 ? "16b" : "8b";
      DIP("%s %s.%s, %s.%s, %s.%s\n", names[INSN(23,22)],
          nameQReg128(dd), ar, nameQReg128(nn), ar, nameQReg128(mm), ar);
      return True;
   }

   if (bitU == 1 && opcode == BITS5(0,0,0,1,1)) {
      
      
      
      
      IRTemp argD = newTempV128();
      IRTemp argN = newTempV128();
      IRTemp argM = newTempV128();
      assign(argD, getQReg128(dd));
      assign(argN, getQReg128(nn));
      assign(argM, getQReg128(mm));
      const IROp opXOR = Iop_XorV128;
      const IROp opAND = Iop_AndV128;
      const IROp opNOT = Iop_NotV128;
      IRTemp res = newTempV128();
      switch (size) {
         case BITS2(0,0): 
            assign(res, binop(opXOR, mkexpr(argM), mkexpr(argN)));
            break;
         case BITS2(0,1): 
            assign(res, binop(opXOR, mkexpr(argM),
                              binop(opAND,
                                    binop(opXOR, mkexpr(argM), mkexpr(argN)),
                                          mkexpr(argD))));
            break;
         case BITS2(1,0): 
            assign(res, binop(opXOR, mkexpr(argD),
                              binop(opAND,
                                    binop(opXOR, mkexpr(argD), mkexpr(argN)),
                                    mkexpr(argM))));
            break;
         case BITS2(1,1): 
            assign(res, binop(opXOR, mkexpr(argD),
                              binop(opAND,
                                    binop(opXOR, mkexpr(argD), mkexpr(argN)),
                                    unop(opNOT, mkexpr(argM)))));
            break;
         default:
            vassert(0);
      }
      putQReg128(dd, math_MAYBE_ZERO_HI64(bitQ, res));
      const HChar* nms[4] = { "eor", "bsl", "bit", "bif" };
      const HChar* arr = bitQ == 1 ? "16b" : "8b";
      DIP("%s %s.%s, %s.%s, %s.%s\n", nms[size],
          nameQReg128(dd), arr, nameQReg128(nn), arr, nameQReg128(mm), arr);
      return True;
   }

   if (opcode == BITS5(0,0,1,1,0)) {
       
       
      if (bitQ == 0 && size == X11) return False; 
      Bool   isGT  = bitU == 0;
      IRExpr* argL = getQReg128(nn);
      IRExpr* argR = getQReg128(mm);
      IRTemp  res  = newTempV128();
      assign(res,
             isGT ? binop(mkVecCMPGTS(size), argL, argR)
                  : binop(mkVecCMPGTU(size), argL, argR));
      putQReg128(dd, math_MAYBE_ZERO_HI64(bitQ, res));
      const HChar* nm  = isGT ? "cmgt" : "cmhi";
      const HChar* arr = nameArr_Q_SZ(bitQ, size);
      DIP("%s %s.%s, %s.%s, %s.%s\n", nm,
          nameQReg128(dd), arr, nameQReg128(nn), arr, nameQReg128(mm), arr);
      return True;
   }

   if (opcode == BITS5(0,0,1,1,1)) {
       
       
      if (bitQ == 0 && size == X11) return False; 
      Bool    isGE = bitU == 0;
      IRExpr* argL = getQReg128(nn);
      IRExpr* argR = getQReg128(mm);
      IRTemp  res  = newTempV128();
      assign(res,
             isGE ? unop(Iop_NotV128, binop(mkVecCMPGTS(size), argR, argL))
                  : unop(Iop_NotV128, binop(mkVecCMPGTU(size), argR, argL)));
      putQReg128(dd, math_MAYBE_ZERO_HI64(bitQ, res));
      const HChar* nm  = isGE ? "cmge" : "cmhs";
      const HChar* arr = nameArr_Q_SZ(bitQ, size);
      DIP("%s %s.%s, %s.%s, %s.%s\n", nm,
          nameQReg128(dd), arr, nameQReg128(nn), arr, nameQReg128(mm), arr);
      return True;
   }

   if (opcode == BITS5(0,1,0,0,0) || opcode == BITS5(0,1,0,1,0)) {
      
      
      
      
      if (bitQ == 0 && size == X11) return False; 
      Bool isU = bitU == 1;
      Bool isR = opcode == BITS5(0,1,0,1,0);
      IROp op  = isR ? (isU ? mkVecRSHU(size) : mkVecRSHS(size))
                     : (isU ? mkVecSHU(size)  : mkVecSHS(size));
      IRTemp res = newTempV128();
      assign(res, binop(op, getQReg128(nn), getQReg128(mm)));
      putQReg128(dd, math_MAYBE_ZERO_HI64(bitQ, res));
      const HChar* nm  = isR ? (isU ? "urshl" : "srshl")
                             : (isU ? "ushl"  : "sshl");
      const HChar* arr = nameArr_Q_SZ(bitQ, size);
      DIP("%s %s.%s, %s.%s, %s.%s\n", nm,
          nameQReg128(dd), arr, nameQReg128(nn), arr, nameQReg128(mm), arr);
      return True;
   }

   if (opcode == BITS5(0,1,0,0,1) || opcode == BITS5(0,1,0,1,1)) {
      
      
      
      
      if (bitQ == 0 && size == X11) return False; 
      Bool isU = bitU == 1;
      Bool isR = opcode == BITS5(0,1,0,1,1);
      IROp op  = isR ? (isU ? mkVecQANDUQRSH(size) : mkVecQANDSQRSH(size))
                     : (isU ? mkVecQANDUQSH(size)  : mkVecQANDSQSH(size));
      IRTemp res256 = newTemp(Ity_V256);
      IRTemp resSH  = newTempV128();
      IRTemp resQ   = newTempV128();
      IRTemp zero   = newTempV128();
      assign(res256, binop(op, 
                           math_MAYBE_ZERO_HI64_fromE(bitQ, getQReg128(nn)),
                           math_MAYBE_ZERO_HI64_fromE(bitQ, getQReg128(mm))));
      assign(resSH, unop(Iop_V256toV128_0, mkexpr(res256)));
      assign(resQ,  unop(Iop_V256toV128_1, mkexpr(res256)));
      assign(zero,  mkV128(0x0000));      
      putQReg128(dd, mkexpr(resSH));
      updateQCFLAGwithDifference(resQ, zero);
      const HChar* nm  = isR ? (isU ? "uqrshl" : "sqrshl")
                             : (isU ? "uqshl"  : "sqshl");
      const HChar* arr = nameArr_Q_SZ(bitQ, size);
      DIP("%s %s.%s, %s.%s, %s.%s\n", nm,
          nameQReg128(dd), arr, nameQReg128(nn), arr, nameQReg128(mm), arr);
      return True;
   }

   if (opcode == BITS5(0,1,1,0,0) || opcode == BITS5(0,1,1,0,1)) {
      
      
      
      
      if (bitQ == 0 && size == X11) return False; 
      Bool isU   = bitU == 1;
      Bool isMAX = (opcode & 1) == 0;
      IROp op    = isMAX ? (isU ? mkVecMAXU(size) : mkVecMAXS(size))
                         : (isU ? mkVecMINU(size) : mkVecMINS(size));
      IRTemp t   = newTempV128();
      assign(t, binop(op, getQReg128(nn), getQReg128(mm)));
      putQReg128(dd, math_MAYBE_ZERO_HI64(bitQ, t));
      const HChar* nm = isMAX ? (isU ? "umax" : "smax")
                              : (isU ? "umin" : "smin");
      const HChar* arr = nameArr_Q_SZ(bitQ, size);
      DIP("%s %s.%s, %s.%s, %s.%s\n", nm,
          nameQReg128(dd), arr, nameQReg128(nn), arr, nameQReg128(mm), arr);
      return True;
   }

   if (opcode == BITS5(0,1,1,1,0) || opcode == BITS5(0,1,1,1,1)) {
      
      
      
      
      if (size == X11) return False; 
      Bool isU   = bitU == 1;
      Bool isACC = opcode == BITS5(0,1,1,1,1);
      vassert(size <= 2);      
      IRTemp t1 = math_ABD(isU, size, getQReg128(nn), getQReg128(mm));
      IRTemp t2 = newTempV128();
      assign(t2, isACC ? binop(mkVecADD(size), mkexpr(t1), getQReg128(dd))
                       : mkexpr(t1));
      putQReg128(dd, math_MAYBE_ZERO_HI64(bitQ, t2));
      const HChar* nm  = isACC ? (isU ? "uaba" : "saba")
                               : (isU ? "uabd" : "sabd");
      const HChar* arr = nameArr_Q_SZ(bitQ, size);
      DIP("%s %s.%s, %s.%s, %s.%s\n", nm,
          nameQReg128(dd), arr, nameQReg128(nn), arr, nameQReg128(mm), arr);
      return True;
   }

   if (opcode == BITS5(1,0,0,0,0)) {
      
      
      if (bitQ == 0 && size == X11) return False; 
      Bool   isSUB = bitU == 1;
      IROp   op    = isSUB ? mkVecSUB(size) : mkVecADD(size);
      IRTemp t     = newTempV128();
      assign(t, binop(op, getQReg128(nn), getQReg128(mm)));
      putQReg128(dd, math_MAYBE_ZERO_HI64(bitQ, t));
      const HChar* nm  = isSUB ? "sub" : "add";
      const HChar* arr = nameArr_Q_SZ(bitQ, size);
      DIP("%s %s.%s, %s.%s, %s.%s\n", nm,
          nameQReg128(dd), arr, nameQReg128(nn), arr, nameQReg128(mm), arr);
      return True;
   }

   if (opcode == BITS5(1,0,0,0,1)) {
       
       
      if (bitQ == 0 && size == X11) return False; 
      Bool    isEQ = bitU == 1;
      IRExpr* argL = getQReg128(nn);
      IRExpr* argR = getQReg128(mm);
      IRTemp  res  = newTempV128();
      assign(res,
             isEQ ? binop(mkVecCMPEQ(size), argL, argR)
                  : unop(Iop_NotV128, binop(mkVecCMPEQ(size),
                                            binop(Iop_AndV128, argL, argR), 
                                            mkV128(0x0000))));
      putQReg128(dd, math_MAYBE_ZERO_HI64(bitQ, res));
      const HChar* nm  = isEQ ? "cmeq" : "cmtst";
      const HChar* arr = nameArr_Q_SZ(bitQ, size);
      DIP("%s %s.%s, %s.%s, %s.%s\n", nm,
          nameQReg128(dd), arr, nameQReg128(nn), arr, nameQReg128(mm), arr);
      return True;
   }

   if (opcode == BITS5(1,0,0,1,0)) {
      
      
      if (bitQ == 0 && size == X11) return False; 
      Bool isMLS = bitU == 1;
      IROp   opMUL    = mkVecMUL(size);
      IROp   opADDSUB = isMLS ? mkVecSUB(size) : mkVecADD(size);
      IRTemp res      = newTempV128();
      if (opMUL != Iop_INVALID && opADDSUB != Iop_INVALID) {
         assign(res, binop(opADDSUB,
                           getQReg128(dd),
                           binop(opMUL, getQReg128(nn), getQReg128(mm))));
         putQReg128(dd, math_MAYBE_ZERO_HI64(bitQ, res));
         const HChar* arr = nameArr_Q_SZ(bitQ, size);
         DIP("%s %s.%s, %s.%s, %s.%s\n", isMLS ? "mls" : "mla",
             nameQReg128(dd), arr, nameQReg128(nn), arr, nameQReg128(mm), arr);
         return True;
      }
      return False;
   }

   if (opcode == BITS5(1,0,0,1,1)) {
      
      
      if (bitQ == 0 && size == X11) return False; 
      Bool isPMUL = bitU == 1;
      const IROp opsPMUL[4]
         = { Iop_PolynomialMul8x16, Iop_INVALID, Iop_INVALID, Iop_INVALID };
      IROp   opMUL = isPMUL ? opsPMUL[size] : mkVecMUL(size);
      IRTemp res   = newTempV128();
      if (opMUL != Iop_INVALID) {
         assign(res, binop(opMUL, getQReg128(nn), getQReg128(mm)));
         putQReg128(dd, math_MAYBE_ZERO_HI64(bitQ, res));
         const HChar* arr = nameArr_Q_SZ(bitQ, size);
         DIP("%s %s.%s, %s.%s, %s.%s\n", isPMUL ? "pmul" : "mul",
             nameQReg128(dd), arr, nameQReg128(nn), arr, nameQReg128(mm), arr);
         return True;
      }
      return False;
   }

   if (opcode == BITS5(1,0,1,0,0) || opcode == BITS5(1,0,1,0,1)) {
      
      
      
      
      if (size == X11) return False;
      Bool isU   = bitU == 1;
      Bool isMAX = opcode == BITS5(1,0,1,0,0);
      IRTemp vN  = newTempV128();
      IRTemp vM  = newTempV128();
      IROp op = isMAX ? (isU ? mkVecMAXU(size) : mkVecMAXS(size))
                      : (isU ? mkVecMINU(size) : mkVecMINS(size));
      assign(vN, getQReg128(nn));
      assign(vM, getQReg128(mm));
      IRTemp res128 = newTempV128();
      assign(res128,
             binop(op,
                   binop(mkVecCATEVENLANES(size), mkexpr(vM), mkexpr(vN)),
                   binop(mkVecCATODDLANES(size),  mkexpr(vM), mkexpr(vN))));
      IRExpr* res
         = bitQ == 0 ? unop(Iop_ZeroHI64ofV128,
                            binop(Iop_CatEvenLanes32x4, mkexpr(res128),
                                                        mkexpr(res128)))
                     : mkexpr(res128);
      putQReg128(dd, res);
      const HChar* arr = nameArr_Q_SZ(bitQ, size);
      const HChar* nm  = isMAX ? (isU ? "umaxp" : "smaxp")
                               : (isU ? "uminp" : "sminp");
      DIP("%s %s.%s, %s.%s, %s.%s\n", nm,
          nameQReg128(dd), arr, nameQReg128(nn), arr, nameQReg128(mm), arr);
      return True;
   }

   if (opcode == BITS5(1,0,1,1,0)) {
      
      
      if (size == X00 || size == X11) return False;
      Bool isR = bitU == 1;
      IRTemp res, sat1q, sat1n, vN, vM;
      res = sat1q = sat1n = vN = vM = IRTemp_INVALID;
      newTempsV128_2(&vN, &vM);
      assign(vN, getQReg128(nn));
      assign(vM, getQReg128(mm));
      math_SQDMULH(&res, &sat1q, &sat1n, isR, size, vN, vM);
      putQReg128(dd, math_MAYBE_ZERO_HI64(bitQ, res));
      IROp opZHI = bitQ == 0 ? Iop_ZeroHI64ofV128 : Iop_INVALID;
      updateQCFLAGwithDifferenceZHI(sat1q, sat1n, opZHI);
      const HChar* arr = nameArr_Q_SZ(bitQ, size);
      const HChar* nm  = isR ? "sqrdmulh" : "sqdmulh";
      DIP("%s %s.%s, %s.%s, %s.%s\n", nm,
          nameQReg128(dd), arr, nameQReg128(nn), arr, nameQReg128(mm), arr);
      return True;
   }

   if (bitU == 0 && opcode == BITS5(1,0,1,1,1)) {
      
      if (bitQ == 0 && size == X11) return False; 
      IRTemp vN = newTempV128();
      IRTemp vM = newTempV128();
      assign(vN, getQReg128(nn));
      assign(vM, getQReg128(mm));
      IRTemp res128 = newTempV128();
      assign(res128,
             binop(mkVecADD(size),
                   binop(mkVecCATEVENLANES(size), mkexpr(vM), mkexpr(vN)),
                   binop(mkVecCATODDLANES(size),  mkexpr(vM), mkexpr(vN))));
      IRExpr* res
         = bitQ == 0 ? unop(Iop_ZeroHI64ofV128,
                            binop(Iop_CatEvenLanes32x4, mkexpr(res128),
                                                        mkexpr(res128)))
                     : mkexpr(res128);
      putQReg128(dd, res);
      const HChar* arr = nameArr_Q_SZ(bitQ, size);
      DIP("addp %s.%s, %s.%s, %s.%s\n",
          nameQReg128(dd), arr, nameQReg128(nn), arr, nameQReg128(mm), arr);
      return True;
   }

   if (bitU == 0
       && (opcode == BITS5(1,1,0,0,0) || opcode == BITS5(1,1,1,1,0))) {
      
      
      
      
      
      Bool   isD   = (size & 1) == 1;
      if (bitQ == 0 && isD) return False; 
      Bool   isMIN = (size & 2) == 2;
      Bool   isNM  = opcode == BITS5(1,1,0,0,0);
      IROp   opMXX = (isMIN ? mkVecMINF : mkVecMAXF)(isD ? X11 : X10);
      IRTemp res   = newTempV128();
      assign(res, binop(opMXX, getQReg128(nn), getQReg128(mm)));
      putQReg128(dd, math_MAYBE_ZERO_HI64(bitQ, res));
      const HChar* arr = bitQ == 0 ? "2s" : (isD ? "2d" : "4s");
      DIP("%s%s %s.%s, %s.%s, %s.%s\n",
          isMIN ? "fmin" : "fmax", isNM ? "nm" : "",
          nameQReg128(dd), arr, nameQReg128(nn), arr, nameQReg128(mm), arr);
      return True;
   }

   if (bitU == 0 && opcode == BITS5(1,1,0,0,1)) {
      
      
      Bool isD   = (size & 1) == 1;
      Bool isSUB = (size & 2) == 2;
      if (bitQ == 0 && isD) return False; 
      IROp opADD = isD ? Iop_Add64Fx2 : Iop_Add32Fx4;
      IROp opSUB = isD ? Iop_Sub64Fx2 : Iop_Sub32Fx4;
      IROp opMUL = isD ? Iop_Mul64Fx2 : Iop_Mul32Fx4;
      IRTemp rm = mk_get_IR_rounding_mode();
      IRTemp t1 = newTempV128();
      IRTemp t2 = newTempV128();
      
      assign(t1, triop(opMUL,
                       mkexpr(rm), getQReg128(nn), getQReg128(mm)));
      assign(t2, triop(isSUB ? opSUB : opADD,
                       mkexpr(rm), getQReg128(dd), mkexpr(t1)));
      putQReg128(dd, math_MAYBE_ZERO_HI64(bitQ, t2));
      const HChar* arr = bitQ == 0 ? "2s" : (isD ? "2d" : "4s");
      DIP("%s %s.%s, %s.%s, %s.%s\n", isSUB ? "fmls" : "fmla",
          nameQReg128(dd), arr, nameQReg128(nn), arr, nameQReg128(mm), arr);
      return True;
   }

   if (bitU == 0 && opcode == BITS5(1,1,0,1,0)) {
      
      
      Bool isD   = (size & 1) == 1;
      Bool isSUB = (size & 2) == 2;
      if (bitQ == 0 && isD) return False; 
      const IROp ops[4]
         = { Iop_Add32Fx4, Iop_Add64Fx2, Iop_Sub32Fx4, Iop_Sub64Fx2 };
      IROp   op = ops[size];
      IRTemp rm = mk_get_IR_rounding_mode();
      IRTemp t1 = newTempV128();
      IRTemp t2 = newTempV128();
      assign(t1, triop(op, mkexpr(rm), getQReg128(nn), getQReg128(mm)));
      assign(t2, math_MAYBE_ZERO_HI64(bitQ, t1));
      putQReg128(dd, mkexpr(t2));
      const HChar* arr = bitQ == 0 ? "2s" : (isD ? "2d" : "4s");
      DIP("%s %s.%s, %s.%s, %s.%s\n", isSUB ? "fsub" : "fadd",
          nameQReg128(dd), arr, nameQReg128(nn), arr, nameQReg128(mm), arr);
      return True;
   }

   if (bitU == 1 && size >= X10 && opcode == BITS5(1,1,0,1,0)) {
      
      Bool isD = (size & 1) == 1;
      if (bitQ == 0 && isD) return False; 
      IROp   opSUB = isD ? Iop_Sub64Fx2 : Iop_Sub32Fx4;
      IROp   opABS = isD ? Iop_Abs64Fx2 : Iop_Abs32Fx4;
      IRTemp rm    = mk_get_IR_rounding_mode();
      IRTemp t1    = newTempV128();
      IRTemp t2    = newTempV128();
      
      assign(t1, triop(opSUB, mkexpr(rm), getQReg128(nn), getQReg128(mm)));
      assign(t2, unop(opABS, mkexpr(t1)));
      putQReg128(dd, math_MAYBE_ZERO_HI64(bitQ, t2));
      const HChar* arr = bitQ == 0 ? "2s" : (isD ? "2d" : "4s");
      DIP("fabd %s.%s, %s.%s, %s.%s\n",
          nameQReg128(dd), arr, nameQReg128(nn), arr, nameQReg128(mm), arr);
      return True;
   }

   if (size <= X01 && opcode == BITS5(1,1,0,1,1)) {
      
      
      
      Bool isD    = (size & 1) == 1;
      Bool isMULX = bitU == 0;
      if (bitQ == 0 && isD) return False; 
      IRTemp rm = mk_get_IR_rounding_mode();
      IRTemp t1 = newTempV128();
      assign(t1, triop(isD ? Iop_Mul64Fx2 : Iop_Mul32Fx4,
                       mkexpr(rm), getQReg128(nn), getQReg128(mm)));
      putQReg128(dd, math_MAYBE_ZERO_HI64(bitQ, t1));
      const HChar* arr = bitQ == 0 ? "2s" : (isD ? "2d" : "4s");
      DIP("%s %s.%s, %s.%s, %s.%s\n", isMULX ? "fmulx" : "fmul",
          nameQReg128(dd), arr, nameQReg128(nn), arr, nameQReg128(mm), arr);
      return True;
   }

   if (size <= X01 && opcode == BITS5(1,1,1,0,0)) {
      
      
      Bool isD = (size & 1) == 1;
      if (bitQ == 0 && isD) return False; 
      Bool   isGE  = bitU == 1;
      IROp   opCMP = isGE ? (isD ? Iop_CmpLE64Fx2 : Iop_CmpLE32Fx4)
                          : (isD ? Iop_CmpEQ64Fx2 : Iop_CmpEQ32Fx4);
      IRTemp t1    = newTempV128();
      assign(t1, isGE ? binop(opCMP, getQReg128(mm), getQReg128(nn)) 
                      : binop(opCMP, getQReg128(nn), getQReg128(mm)));
      putQReg128(dd, math_MAYBE_ZERO_HI64(bitQ, t1));
      const HChar* arr = bitQ == 0 ? "2s" : (isD ? "2d" : "4s");
      DIP("%s %s.%s, %s.%s, %s.%s\n", isGE ? "fcmge" : "fcmeq",
          nameQReg128(dd), arr, nameQReg128(nn), arr, nameQReg128(mm), arr);
      return True;
   }

   if (bitU == 1 && size >= X10 && opcode == BITS5(1,1,1,0,0)) {
      
      Bool isD = (size & 1) == 1;
      if (bitQ == 0 && isD) return False; 
      IROp   opCMP = isD ? Iop_CmpLT64Fx2 : Iop_CmpLT32Fx4;
      IRTemp t1    = newTempV128();
      assign(t1, binop(opCMP, getQReg128(mm), getQReg128(nn))); 
      putQReg128(dd, math_MAYBE_ZERO_HI64(bitQ, t1));
      const HChar* arr = bitQ == 0 ? "2s" : (isD ? "2d" : "4s");
      DIP("%s %s.%s, %s.%s, %s.%s\n", "fcmgt",
          nameQReg128(dd), arr, nameQReg128(nn), arr, nameQReg128(mm), arr);
      return True;
   }

   if (bitU == 1 && opcode == BITS5(1,1,1,0,1)) {
      
      
      Bool isD  = (size & 1) == 1;
      Bool isGT = (size & 2) == 2;
      if (bitQ == 0 && isD) return False; 
      IROp   opCMP = isGT ? (isD ? Iop_CmpLT64Fx2 : Iop_CmpLT32Fx4)
                          : (isD ? Iop_CmpLE64Fx2 : Iop_CmpLE32Fx4);
      IROp   opABS = isD ? Iop_Abs64Fx2 : Iop_Abs32Fx4;
      IRTemp t1    = newTempV128();
      assign(t1, binop(opCMP, unop(opABS, getQReg128(mm)),
                              unop(opABS, getQReg128(nn)))); 
      putQReg128(dd, math_MAYBE_ZERO_HI64(bitQ, t1));
      const HChar* arr = bitQ == 0 ? "2s" : (isD ? "2d" : "4s");
      DIP("%s %s.%s, %s.%s, %s.%s\n", isGT ? "facgt" : "facge",
          nameQReg128(dd), arr, nameQReg128(nn), arr, nameQReg128(mm), arr);
      return True;
   }

   if (bitU == 1
       && (opcode == BITS5(1,1,0,0,0) || opcode == BITS5(1,1,1,1,0))) {
      
      
      
      
      
      Bool isD = (size & 1) == 1;
      if (bitQ == 0 && isD) return False; 
      Bool   isMIN = (size & 2) == 2;
      Bool   isNM  = opcode == BITS5(1,1,0,0,0);
      IROp   opMXX = (isMIN ? mkVecMINF : mkVecMAXF)(isD ? 3 : 2);
      IRTemp srcN  = newTempV128();
      IRTemp srcM  = newTempV128();
      IRTemp preL  = IRTemp_INVALID;
      IRTemp preR  = IRTemp_INVALID;
      assign(srcN, getQReg128(nn));
      assign(srcM, getQReg128(mm));
      math_REARRANGE_FOR_FLOATING_PAIRWISE(&preL, &preR,
                                           srcM, srcN, isD, bitQ);
      putQReg128(
         dd, math_MAYBE_ZERO_HI64_fromE(
                bitQ,
                binop(opMXX, mkexpr(preL), mkexpr(preR))));
      const HChar* arr = bitQ == 0 ? "2s" : (isD ? "2d" : "4s");
      DIP("%s%sp %s.%s, %s.%s, %s.%s\n", 
          isMIN ? "fmin" : "fmax", isNM ? "nm" : "",
          nameQReg128(dd), arr, nameQReg128(nn), arr, nameQReg128(mm), arr);
      return True;
   }

   if (bitU == 1 && size <= X01 && opcode == BITS5(1,1,0,1,0)) {
      
      Bool isD = size == X01;
      if (bitQ == 0 && isD) return False; 
      IRTemp srcN = newTempV128();
      IRTemp srcM = newTempV128();
      IRTemp preL = IRTemp_INVALID;
      IRTemp preR = IRTemp_INVALID;
      assign(srcN, getQReg128(nn));
      assign(srcM, getQReg128(mm));
      math_REARRANGE_FOR_FLOATING_PAIRWISE(&preL, &preR,
                                           srcM, srcN, isD, bitQ);
      putQReg128(
         dd, math_MAYBE_ZERO_HI64_fromE(
                bitQ,
                triop(mkVecADDF(isD ? 3 : 2),
                      mkexpr(mk_get_IR_rounding_mode()),
                      mkexpr(preL), mkexpr(preR))));
      const HChar* arr = bitQ == 0 ? "2s" : (isD ? "2d" : "4s");
      DIP("%s %s.%s, %s.%s, %s.%s\n", "faddp",
          nameQReg128(dd), arr, nameQReg128(nn), arr, nameQReg128(mm), arr);
      return True;
   }

   if (bitU == 1 && size <= X01 && opcode == BITS5(1,1,1,1,1)) {
      
      Bool isD = (size & 1) == 1;
      if (bitQ == 0 && isD) return False; 
      vassert(size <= 1);
      const IROp ops[2] = { Iop_Div32Fx4, Iop_Div64Fx2 };
      IROp   op = ops[size];
      IRTemp rm = mk_get_IR_rounding_mode();
      IRTemp t1 = newTempV128();
      IRTemp t2 = newTempV128();
      assign(t1, triop(op, mkexpr(rm), getQReg128(nn), getQReg128(mm)));
      assign(t2, math_MAYBE_ZERO_HI64(bitQ, t1));
      putQReg128(dd, mkexpr(t2));
      const HChar* arr = bitQ == 0 ? "2s" : (isD ? "2d" : "4s");
      DIP("%s %s.%s, %s.%s, %s.%s\n", "fdiv",
          nameQReg128(dd), arr, nameQReg128(nn), arr, nameQReg128(mm), arr);
      return True;
   }

   if (bitU == 0 && opcode == BITS5(1,1,1,1,1)) {
      
      
      Bool isSQRT = (size & 2) == 2;
      Bool isD    = (size & 1) == 1;
      if (bitQ == 0 && isD) return False; 
      IROp op     = isSQRT ? (isD ? Iop_RSqrtStep64Fx2 : Iop_RSqrtStep32Fx4)
                           : (isD ? Iop_RecipStep64Fx2 : Iop_RecipStep32Fx4);
      IRTemp res = newTempV128();
      assign(res, binop(op, getQReg128(nn), getQReg128(mm)));
      putQReg128(dd, math_MAYBE_ZERO_HI64(bitQ, res));
      const HChar* arr = bitQ == 0 ? "2s" : (isD ? "2d" : "4s");
      DIP("%s %s.%s, %s.%s, %s.%s\n", isSQRT ? "frsqrts" : "frecps",
          nameQReg128(dd), arr, nameQReg128(nn), arr, nameQReg128(mm), arr);
      return True;
   }

   return False;
#  undef INSN
}


static
Bool dis_AdvSIMD_two_reg_misc(DisResult* dres, UInt insn)
{
#  define INSN(_bMax,_bMin)  SLICE_UInt(insn, (_bMax), (_bMin))
   if (INSN(31,31) != 0
       || INSN(28,24) != BITS5(0,1,1,1,0)
       || INSN(21,17) != BITS5(1,0,0,0,0)
       || INSN(11,10) != BITS2(1,0)) {
      return False;
   }
   UInt bitQ   = INSN(30,30);
   UInt bitU   = INSN(29,29);
   UInt size   = INSN(23,22);
   UInt opcode = INSN(16,12);
   UInt nn     = INSN(9,5);
   UInt dd     = INSN(4,0);
   vassert(size < 4);

   if (bitU == 0 && size <= X10 && opcode == BITS5(0,0,0,0,0)) {
      
      
      
      const IROp iops[3] = { Iop_Reverse8sIn64_x2,
                             Iop_Reverse16sIn64_x2, Iop_Reverse32sIn64_x2 };
      vassert(size <= 2);
      IRTemp res = newTempV128();
      assign(res, unop(iops[size], getQReg128(nn)));
      putQReg128(dd, math_MAYBE_ZERO_HI64(bitQ, res));
      const HChar* arr = nameArr_Q_SZ(bitQ, size);
      DIP("%s %s.%s, %s.%s\n", "rev64",
          nameQReg128(dd), arr, nameQReg128(nn), arr);
      return True;
   }

   if (bitU == 1 && size <= X01 && opcode == BITS5(0,0,0,0,0)) {
      
      
      Bool   isH = size == X01;
      IRTemp res = newTempV128();
      IROp   iop = isH ? Iop_Reverse16sIn32_x4 : Iop_Reverse8sIn32_x4;
      assign(res, unop(iop, getQReg128(nn)));
      putQReg128(dd, math_MAYBE_ZERO_HI64(bitQ, res));
      const HChar* arr = nameArr_Q_SZ(bitQ, size);
      DIP("%s %s.%s, %s.%s\n", "rev32",
          nameQReg128(dd), arr, nameQReg128(nn), arr);
      return True;
   }

   if (bitU == 0 && size == X00 && opcode == BITS5(0,0,0,0,1)) {
      
      IRTemp res = newTempV128();
      assign(res, unop(Iop_Reverse8sIn16_x8, getQReg128(nn)));
      putQReg128(dd, math_MAYBE_ZERO_HI64(bitQ, res));
      const HChar* arr = nameArr_Q_SZ(bitQ, size);
      DIP("%s %s.%s, %s.%s\n", "rev16",
          nameQReg128(dd), arr, nameQReg128(nn), arr);
      return True;
   }

   if (opcode == BITS5(0,0,0,1,0) || opcode == BITS5(0,0,1,1,0)) {
      
      
      
      
      
      if (size == X11) return False; 
      Bool   isU   = bitU == 1;
      Bool   isACC = opcode == BITS5(0,0,1,1,0);
      IRTemp src   = newTempV128();
      IRTemp sum   = newTempV128();
      IRTemp res   = newTempV128();
      assign(src, getQReg128(nn));
      assign(sum,
             binop(mkVecADD(size+1),
                   mkexpr(math_WIDEN_EVEN_OR_ODD_LANES(
                             isU, True, size, mkexpr(src))),
                   mkexpr(math_WIDEN_EVEN_OR_ODD_LANES(
                             isU, False, size, mkexpr(src)))));
      assign(res, isACC ? binop(mkVecADD(size+1), mkexpr(sum), getQReg128(dd))
                        : mkexpr(sum));
      putQReg128(dd, math_MAYBE_ZERO_HI64(bitQ, res));
      const HChar* arrNarrow = nameArr_Q_SZ(bitQ, size);
      const HChar* arrWide   = nameArr_Q_SZ(bitQ, size+1);
      DIP("%s %s.%s, %s.%s\n", isACC ? (isU ? "uadalp" : "sadalp")
                                     : (isU ? "uaddlp" : "saddlp"),
          nameQReg128(dd), arrWide, nameQReg128(nn), arrNarrow);
      return True;
   }

   if (opcode == BITS5(0,0,0,1,1)) {
      
      
      if (bitQ == 0 && size == X11) return False; 
      Bool isUSQADD = bitU == 1;
      IROp   qop  = isUSQADD ? mkVecQADDEXTSUSATUU(size)
                             : mkVecQADDEXTUSSATSS(size);
      IROp   nop  = mkVecADD(size);
      IRTemp argL = newTempV128();
      IRTemp argR = newTempV128();
      IRTemp qres = newTempV128();
      IRTemp nres = newTempV128();
      assign(argL, getQReg128(nn));
      assign(argR, getQReg128(dd));
      assign(qres, math_MAYBE_ZERO_HI64_fromE(
                      bitQ, binop(qop, mkexpr(argL), mkexpr(argR))));
      assign(nres, math_MAYBE_ZERO_HI64_fromE(
                      bitQ, binop(nop, mkexpr(argL), mkexpr(argR))));
      putQReg128(dd, mkexpr(qres));
      updateQCFLAGwithDifference(qres, nres);
      const HChar* arr = nameArr_Q_SZ(bitQ, size);
      DIP("%s %s.%s, %s.%s\n", isUSQADD ? "usqadd" : "suqadd",
          nameQReg128(dd), arr, nameQReg128(nn), arr);
      return True;
   }

   if (opcode == BITS5(0,0,1,0,0)) {
      
      
      if (size == X11) return False; 
      const IROp opsCLS[3] = { Iop_Cls8x16, Iop_Cls16x8, Iop_Cls32x4 };
      const IROp opsCLZ[3] = { Iop_Clz8x16, Iop_Clz16x8, Iop_Clz32x4 };
      Bool   isCLZ = bitU == 1;
      IRTemp res   = newTempV128();
      vassert(size <= 2);
      assign(res, unop(isCLZ ? opsCLZ[size] : opsCLS[size], getQReg128(nn)));
      putQReg128(dd, math_MAYBE_ZERO_HI64(bitQ, res));
      const HChar* arr = nameArr_Q_SZ(bitQ, size);
      DIP("%s %s.%s, %s.%s\n", isCLZ ? "clz" : "cls",
          nameQReg128(dd), arr, nameQReg128(nn), arr);
      return True;
   }

   if (size == X00 && opcode == BITS5(0,0,1,0,1)) {
      
      
      IRTemp res = newTempV128();
      assign(res, unop(bitU == 0 ? Iop_Cnt8x16 : Iop_NotV128, getQReg128(nn)));
      putQReg128(dd, math_MAYBE_ZERO_HI64(bitQ, res));
      const HChar* arr = nameArr_Q_SZ(bitQ, 0);
      DIP("%s %s.%s, %s.%s\n", bitU == 0 ? "cnt" : "not",
          nameQReg128(dd), arr, nameQReg128(nn), arr);
      return True;
   }

   if (bitU == 1 && size == X01 && opcode == BITS5(0,0,1,0,1)) {
      
      IRTemp res = newTempV128();
      assign(res, unop(Iop_Reverse1sIn8_x16, getQReg128(nn)));
      putQReg128(dd, math_MAYBE_ZERO_HI64(bitQ, res));
      const HChar* arr = nameArr_Q_SZ(bitQ, 0);
      DIP("%s %s.%s, %s.%s\n", "rbit",
          nameQReg128(dd), arr, nameQReg128(nn), arr);
      return True;
   }

   if (opcode == BITS5(0,0,1,1,1)) {
      
      
      if (bitQ == 0 && size == X11) return False; 
      Bool   isNEG  = bitU == 1;
      IRTemp qresFW = IRTemp_INVALID, nresFW = IRTemp_INVALID;
      (isNEG ? math_SQNEG : math_SQABS)( &qresFW, &nresFW,
                                         getQReg128(nn), size );
      IRTemp qres = newTempV128(), nres = newTempV128();
      assign(qres, math_MAYBE_ZERO_HI64(bitQ, qresFW));
      assign(nres, math_MAYBE_ZERO_HI64(bitQ, nresFW));
      putQReg128(dd, mkexpr(qres));
      updateQCFLAGwithDifference(qres, nres);
      const HChar* arr = nameArr_Q_SZ(bitQ, size);
      DIP("%s %s.%s, %s.%s\n", isNEG ? "sqneg" : "sqabs",
          nameQReg128(dd), arr, nameQReg128(nn), arr);
      return True;
   }

   if (opcode == BITS5(0,1,0,0,0)) {
       
       
      if (bitQ == 0 && size == X11) return False; 
      Bool    isGT  = bitU == 0;
      IRExpr* argL  = getQReg128(nn);
      IRExpr* argR  = mkV128(0x0000);
      IRTemp  res   = newTempV128();
      IROp    opGTS = mkVecCMPGTS(size);
      assign(res, isGT ? binop(opGTS, argL, argR)
                       : unop(Iop_NotV128, binop(opGTS, argR, argL)));
      putQReg128(dd, math_MAYBE_ZERO_HI64(bitQ, res));
      const HChar* arr = nameArr_Q_SZ(bitQ, size);
      DIP("cm%s %s.%s, %s.%s, #0\n", isGT ? "gt" : "ge",
          nameQReg128(dd), arr, nameQReg128(nn), arr);
      return True;
   }

   if (opcode == BITS5(0,1,0,0,1)) {
       
       
      if (bitQ == 0 && size == X11) return False; 
      Bool    isEQ = bitU == 0;
      IRExpr* argL = getQReg128(nn);
      IRExpr* argR = mkV128(0x0000);
      IRTemp  res  = newTempV128();
      assign(res, isEQ ? binop(mkVecCMPEQ(size), argL, argR)
                       : unop(Iop_NotV128,
                              binop(mkVecCMPGTS(size), argL, argR)));
      putQReg128(dd, math_MAYBE_ZERO_HI64(bitQ, res));
      const HChar* arr = nameArr_Q_SZ(bitQ, size);
      DIP("cm%s %s.%s, %s.%s, #0\n", isEQ ? "eq" : "le",
          nameQReg128(dd), arr, nameQReg128(nn), arr);
      return True;
   }

   if (bitU == 0 && opcode == BITS5(0,1,0,1,0)) {
       
      if (bitQ == 0 && size == X11) return False; 
      IRExpr* argL = getQReg128(nn);
      IRExpr* argR = mkV128(0x0000);
      IRTemp  res  = newTempV128();
      assign(res, binop(mkVecCMPGTS(size), argR, argL));
      putQReg128(dd, math_MAYBE_ZERO_HI64(bitQ, res));
      const HChar* arr = nameArr_Q_SZ(bitQ, size);
      DIP("cm%s %s.%s, %s.%s, #0\n", "lt",
          nameQReg128(dd), arr, nameQReg128(nn), arr);
      return True;
   }

   if (bitU == 0 && opcode == BITS5(0,1,0,1,1)) {
      
      if (bitQ == 0 && size == X11) return False; 
      IRTemp res = newTempV128();
      assign(res, unop(mkVecABS(size), getQReg128(nn)));
      putQReg128(dd, math_MAYBE_ZERO_HI64(bitQ, res));
      const HChar* arr = nameArr_Q_SZ(bitQ, size);
      DIP("abs %s.%s, %s.%s\n", nameQReg128(dd), arr, nameQReg128(nn), arr);
      return True;
   }

   if (bitU == 1 && opcode == BITS5(0,1,0,1,1)) {
      
      if (bitQ == 0 && size == X11) return False; 
      IRTemp res = newTempV128();
      assign(res, binop(mkVecSUB(size), mkV128(0x0000), getQReg128(nn)));
      putQReg128(dd, math_MAYBE_ZERO_HI64(bitQ, res));
      const HChar* arr = nameArr_Q_SZ(bitQ, size);
      DIP("neg %s.%s, %s.%s\n", nameQReg128(dd), arr, nameQReg128(nn), arr);
      return True;
   }

   UInt ix = 0; 
   if (size >= X10) {
      switch (opcode) {
         case BITS5(0,1,1,0,0): ix = (bitU == 1) ? 4 : 1; break;
         case BITS5(0,1,1,0,1): ix = (bitU == 1) ? 5 : 2; break;
         case BITS5(0,1,1,1,0): if (bitU == 0) ix = 3; break;
         default: break;
      }
   }
   if (ix > 0) {
      
      
      
      
      
      if (bitQ == 0 && size == X11) return False; 
      Bool   isD     = size == X11;
      IROp   opCmpEQ = isD ? Iop_CmpEQ64Fx2 : Iop_CmpEQ32Fx4;
      IROp   opCmpLE = isD ? Iop_CmpLE64Fx2 : Iop_CmpLE32Fx4;
      IROp   opCmpLT = isD ? Iop_CmpLT64Fx2 : Iop_CmpLT32Fx4;
      IROp   opCmp   = Iop_INVALID;
      Bool   swap    = False;
      const HChar* nm = "??";
      switch (ix) {
         case 1: nm = "fcmgt"; opCmp = opCmpLT; swap = True; break;
         case 2: nm = "fcmeq"; opCmp = opCmpEQ; break;
         case 3: nm = "fcmlt"; opCmp = opCmpLT; break;
         case 4: nm = "fcmge"; opCmp = opCmpLE; swap = True; break;
         case 5: nm = "fcmle"; opCmp = opCmpLE; break;
         default: vassert(0);
      }
      IRExpr* zero = mkV128(0x0000);
      IRTemp res = newTempV128();
      assign(res, swap ? binop(opCmp, zero, getQReg128(nn))
                       : binop(opCmp, getQReg128(nn), zero));
      putQReg128(dd, math_MAYBE_ZERO_HI64(bitQ, res));
      const HChar* arr = bitQ == 0 ? "2s" : (size == X11 ? "2d" : "4s");
      DIP("%s %s.%s, %s.%s, #0.0\n", nm,
          nameQReg128(dd), arr, nameQReg128(nn), arr);
      return True;
   }

   if (size >= X10 && opcode == BITS5(0,1,1,1,1)) {
      
      
      if (bitQ == 0 && size == X11) return False; 
      Bool   isFNEG = bitU == 1;
      IROp   op     = isFNEG ? (size == X10 ? Iop_Neg32Fx4 : Iop_Neg64Fx2)
                             : (size == X10 ? Iop_Abs32Fx4 : Iop_Abs64Fx2);
      IRTemp res = newTempV128();
      assign(res, unop(op, getQReg128(nn)));
      putQReg128(dd, math_MAYBE_ZERO_HI64(bitQ, res));
      const HChar* arr = bitQ == 0 ? "2s" : (size == X11 ? "2d" : "4s");
      DIP("%s %s.%s, %s.%s\n", isFNEG ? "fneg" : "fabs",
          nameQReg128(dd), arr, nameQReg128(nn), arr);
      return True;
   }

   if (bitU == 0 && opcode == BITS5(1,0,0,1,0)) {
      
      if (size == X11) return False;
      vassert(size < 3);
      Bool   is2  = bitQ == 1;
      IROp   opN  = mkVecNARROWUN(size);
      IRTemp resN = newTempV128();
      assign(resN, unop(Iop_64UtoV128, unop(opN, getQReg128(nn))));
      putLO64andZUorPutHI64(is2, dd, resN);
      const HChar* nm        = "xtn";
      const HChar* arrNarrow = nameArr_Q_SZ(bitQ, size);
      const HChar* arrWide   = nameArr_Q_SZ(1,    size+1);
      DIP("%s%s %s.%s, %s.%s\n", is2 ? "2" : "", nm,
          nameQReg128(dd), arrNarrow, nameQReg128(nn), arrWide);
      return True;
   }

   if (opcode == BITS5(1,0,1,0,0)
       || (bitU == 1 && opcode == BITS5(1,0,0,1,0))) {
      
      
      
      if (size == X11) return False;
      vassert(size < 3);
      Bool  is2    = bitQ == 1;
      IROp  opN    = Iop_INVALID;
      Bool  zWiden = True;
      const HChar* nm = "??";
       if (bitU == 0 && opcode == BITS5(1,0,1,0,0)) {
         opN = mkVecQNARROWUNSS(size); nm = "sqxtn"; zWiden = False;
      }
      else if (bitU == 1 && opcode == BITS5(1,0,1,0,0)) {
         opN = mkVecQNARROWUNUU(size); nm = "uqxtn";
      }
      else if (bitU == 1 && opcode == BITS5(1,0,0,1,0)) {
         opN = mkVecQNARROWUNSU(size); nm = "sqxtun";
      }
      else vassert(0);
      IRTemp src  = newTempV128();
      assign(src, getQReg128(nn));
      IRTemp resN = newTempV128();
      assign(resN, unop(Iop_64UtoV128, unop(opN, mkexpr(src))));
      putLO64andZUorPutHI64(is2, dd, resN);
      IRTemp resW = math_WIDEN_LO_OR_HI_LANES(zWiden, False,
                                              size, mkexpr(resN));
      updateQCFLAGwithDifference(src, resW);
      const HChar* arrNarrow = nameArr_Q_SZ(bitQ, size);
      const HChar* arrWide   = nameArr_Q_SZ(1,    size+1);
      DIP("%s%s %s.%s, %s.%s\n", is2 ? "2" : "", nm,
          nameQReg128(dd), arrNarrow, nameQReg128(nn), arrWide);
      return True;
   }

   if (bitU == 1 && opcode == BITS5(1,0,0,1,1)) {
      
      
      if (size == X11) return False;
      Bool is2   = bitQ == 1;
      IROp opINT = is2 ? mkVecINTERLEAVEHI(size) : mkVecINTERLEAVELO(size);
      IROp opSHL = mkVecSHLN(size+1);
      IRTemp src = newTempV128();
      IRTemp res = newTempV128();
      assign(src, getQReg128(nn));
      assign(res, binop(opSHL, binop(opINT, mkexpr(src), mkexpr(src)),
                               mkU8(8 << size)));
      putQReg128(dd, mkexpr(res));
      const HChar* arrNarrow = nameArr_Q_SZ(bitQ, size);
      const HChar* arrWide   = nameArr_Q_SZ(1,    size+1);
      DIP("shll%s %s.%s, %s.%s, #%u\n", is2 ? "2" : "", 
          nameQReg128(dd), arrWide, nameQReg128(nn), arrNarrow, 8 << size);
      return True;
   }

   if (bitU == 0 && size <= X01 && opcode == BITS5(1,0,1,1,0)) {
      
      UInt   nLanes = size == X00 ? 4 : 2;
      IRType srcTy  = size == X00 ? Ity_F32 : Ity_F64;
      IROp   opCvt  = size == X00 ? Iop_F32toF16 : Iop_F64toF32;
      IRTemp rm     = mk_get_IR_rounding_mode();
      IRTemp src[nLanes];
      for (UInt i = 0; i < nLanes; i++) {
         src[i] = newTemp(srcTy);
         assign(src[i], getQRegLane(nn, i, srcTy));
      }
      for (UInt i = 0; i < nLanes; i++) {
         putQRegLane(dd, nLanes * bitQ + i,
                         binop(opCvt, mkexpr(rm), mkexpr(src[i])));
      }
      if (bitQ == 0) {
         putQRegLane(dd, 1, mkU64(0));
      }
      const HChar* arrNarrow = nameArr_Q_SZ(bitQ, 1+size);
      const HChar* arrWide   = nameArr_Q_SZ(1,    1+size+1);
      DIP("fcvtn%s %s.%s, %s.%s\n", bitQ ? "2" : "",
          nameQReg128(dd), arrNarrow, nameQReg128(nn), arrWide);
      return True;
   }

   if (bitU == 1 && size == X01 && opcode == BITS5(1,0,1,1,0)) {
      
      IRType srcTy = Ity_F64;
      IROp   opCvt = Iop_F64toF32;
      IRTemp src[2];
      for (UInt i = 0; i < 2; i++) {
         src[i] = newTemp(srcTy);
         assign(src[i], getQRegLane(nn, i, srcTy));
      }
      for (UInt i = 0; i < 2; i++) {
         putQRegLane(dd, 2 * bitQ + i,
                         binop(opCvt, mkU32(Irrm_NEAREST), mkexpr(src[i])));
      }
      if (bitQ == 0) {
         putQRegLane(dd, 1, mkU64(0));
      }
      const HChar* arrNarrow = nameArr_Q_SZ(bitQ, 1+size);
      const HChar* arrWide   = nameArr_Q_SZ(1,    1+size+1);
      DIP("fcvtxn%s %s.%s, %s.%s\n", bitQ ? "2" : "",
          nameQReg128(dd), arrNarrow, nameQReg128(nn), arrWide);
      return True;
   }

   if (bitU == 0 && size <= X01 && opcode == BITS5(1,0,1,1,1)) {
      
      UInt   nLanes = size == X00 ? 4 : 2;
      IRType srcTy  = size == X00 ? Ity_F16 : Ity_F32;
      IROp   opCvt  = size == X00 ? Iop_F16toF32 : Iop_F32toF64;
      IRTemp src[nLanes];
      for (UInt i = 0; i < nLanes; i++) {
         src[i] = newTemp(srcTy);
         assign(src[i], getQRegLane(nn, nLanes * bitQ + i, srcTy));
      }
      for (UInt i = 0; i < nLanes; i++) {
         putQRegLane(dd, i, unop(opCvt, mkexpr(src[i])));
      }
      const HChar* arrNarrow = nameArr_Q_SZ(bitQ, 1+size);
      const HChar* arrWide   = nameArr_Q_SZ(1,    1+size+1);
      DIP("fcvtl%s %s.%s, %s.%s\n", bitQ ? "2" : "",
          nameQReg128(dd), arrWide, nameQReg128(nn), arrNarrow);
      return True;
   }

   ix = 0;
   if (opcode == BITS5(1,1,0,0,0) || opcode == BITS5(1,1,0,0,1)) {
      ix = 1 + ((((bitU & 1) << 2) | ((size & 2) << 0)) | ((opcode & 1) << 0));
      
      vassert(ix >= 1 && ix <= 8);
      if (ix == 7) ix = 0;
   }
   if (ix > 0) {
      
      
      
      
      
      
      
      
      Bool isD = (size & 1) == 1;
      if (bitQ == 0 && isD) return False; 

      IRTemp irrmRM = mk_get_IR_rounding_mode();

      UChar ch = '?';
      IRTemp irrm = newTemp(Ity_I32);
      switch (ix) {
         case 1: ch = 'n'; assign(irrm, mkU32(Irrm_NEAREST)); break;
         case 2: ch = 'm'; assign(irrm, mkU32(Irrm_NegINF)); break;
         case 3: ch = 'p'; assign(irrm, mkU32(Irrm_PosINF)); break;
         case 4: ch = 'z'; assign(irrm, mkU32(Irrm_ZERO)); break; 
         
         case 5: ch = 'a'; assign(irrm, mkU32(Irrm_NEAREST)); break;
         
         
         case 6: ch = 'x'; assign(irrm, mkexpr(irrmRM)); break;
         case 8: ch = 'i'; assign(irrm, mkexpr(irrmRM)); break; 
         default: vassert(0);
      }

      IROp opRND = isD ? Iop_RoundF64toInt : Iop_RoundF32toInt;
      if (isD) {
         for (UInt i = 0; i < 2; i++) {
            putQRegLane(dd, i, binop(opRND, mkexpr(irrm),
                                            getQRegLane(nn, i, Ity_F64)));
         }
      } else {
         UInt n = bitQ==1 ? 4 : 2;
         for (UInt i = 0; i < n; i++) {
            putQRegLane(dd, i, binop(opRND, mkexpr(irrm),
                                            getQRegLane(nn, i, Ity_F32)));
         }
         if (bitQ == 0)
            putQRegLane(dd, 1, mkU64(0)); 
      }
      const HChar* arr = nameArr_Q_SZ(bitQ, size);
      DIP("frint%c %s.%s, %s.%s\n", ch,
          nameQReg128(dd), arr, nameQReg128(nn), arr);
      return True;
   }

   ix = 0; 
   switch (opcode) {
      case BITS5(1,1,0,1,0): ix = ((size & 2) == 2) ? 4 : 1; break;
      case BITS5(1,1,0,1,1): ix = ((size & 2) == 2) ? 5 : 2; break;
      case BITS5(1,1,1,0,0): if ((size & 2) == 0) ix = 3; break;
      default: break;
   }
   if (ix > 0) {
      
      
      
      
      
      
      
      
      
      
      Bool isD = (size & 1) == 1;
      if (bitQ == 0 && isD) return False; 

      IRRoundingMode irrm = 8; 
      HChar          ch   = '?';
      switch (ix) {
         case 1: ch = 'n'; irrm = Irrm_NEAREST; break;
         case 2: ch = 'm'; irrm = Irrm_NegINF;  break;
         case 3: ch = 'a'; irrm = Irrm_NEAREST; break; 
         case 4: ch = 'p'; irrm = Irrm_PosINF;  break;
         case 5: ch = 'z'; irrm = Irrm_ZERO;    break;
         default: vassert(0);
      }
      IROp cvt = Iop_INVALID;
      if (bitU == 1) {
         cvt = isD ? Iop_F64toI64U : Iop_F32toI32U;
      } else {
         cvt = isD ? Iop_F64toI64S : Iop_F32toI32S;
      }
      if (isD) {
         for (UInt i = 0; i < 2; i++) {
            putQRegLane(dd, i, binop(cvt, mkU32(irrm),
                                            getQRegLane(nn, i, Ity_F64)));
         }
      } else {
         UInt n = bitQ==1 ? 4 : 2;
         for (UInt i = 0; i < n; i++) {
            putQRegLane(dd, i, binop(cvt, mkU32(irrm),
                                            getQRegLane(nn, i, Ity_F32)));
         }
         if (bitQ == 0)
            putQRegLane(dd, 1, mkU64(0)); 
      }
      const HChar* arr = nameArr_Q_SZ(bitQ, size);
      DIP("fcvt%c%c %s.%s, %s.%s\n", ch, bitU == 1 ? 'u' : 's',
          nameQReg128(dd), arr, nameQReg128(nn), arr);
      return True;
   }

   if (size == X10 && opcode == BITS5(1,1,1,0,0)) {
      
      
      Bool isREC = bitU == 0;
      IROp op    = isREC ? Iop_RecipEst32Ux4 : Iop_RSqrtEst32Ux4;
      IRTemp res = newTempV128();
      assign(res, unop(op, getQReg128(nn)));
      putQReg128(dd, math_MAYBE_ZERO_HI64(bitQ, res));
      const HChar* nm  = isREC ? "urecpe" : "ursqrte";
      const HChar* arr = nameArr_Q_SZ(bitQ, size);
      DIP("%s %s.%s, %s.%s\n", nm,
          nameQReg128(dd), arr, nameQReg128(nn), arr);
      return True;
   }

   if (size <= X01 && opcode == BITS5(1,1,1,0,1)) {
      
      
      Bool isQ   = bitQ == 1;
      Bool isU   = bitU == 1;
      Bool isF64 = (size & 1) == 1;
      if (isQ || !isF64) {
         IRType tyF = Ity_INVALID, tyI = Ity_INVALID;
         UInt   nLanes = 0;
         Bool   zeroHI = False;
         const HChar* arrSpec = NULL;
         Bool   ok  = getLaneInfo_Q_SZ(&tyI, &tyF, &nLanes, &zeroHI, &arrSpec,
                                       isQ, isF64 );
         IROp   iop = isU ? (isF64 ? Iop_I64UtoF64 : Iop_I32UtoF32)
                          : (isF64 ? Iop_I64StoF64 : Iop_I32StoF32);
         IRTemp rm  = mk_get_IR_rounding_mode();
         UInt   i;
         vassert(ok); 
         for (i = 0; i < nLanes; i++) {
            putQRegLane(dd, i,
                        binop(iop, mkexpr(rm), getQRegLane(nn, i, tyI)));
         }
         if (zeroHI) {
            putQRegLane(dd, 1, mkU64(0));
         }
         DIP("%ccvtf %s.%s, %s.%s\n", isU ? 'u' : 's',
             nameQReg128(dd), arrSpec, nameQReg128(nn), arrSpec);
         return True;
      }
      
   }

   if (size >= X10 && opcode == BITS5(1,1,1,0,1)) {
      
      
      Bool isSQRT = bitU == 1;
      Bool isD    = (size & 1) == 1;
      IROp op     = isSQRT ? (isD ? Iop_RSqrtEst64Fx2 : Iop_RSqrtEst32Fx4)
                           : (isD ? Iop_RecipEst64Fx2 : Iop_RecipEst32Fx4);
      if (bitQ == 0 && isD) return False; 
      IRTemp resV = newTempV128();
      assign(resV, unop(op, getQReg128(nn)));
      putQReg128(dd, math_MAYBE_ZERO_HI64(bitQ, resV));
      const HChar* arr = bitQ == 0 ? "2s" : (size == X11 ? "2d" : "4s");
      DIP("%s %s.%s, %s.%s\n", isSQRT ? "frsqrte" : "frecpe",
          nameQReg128(dd), arr, nameQReg128(nn), arr);
      return True;
   }

   if (bitU == 1 && size >= X10 && opcode == BITS5(1,1,1,1,1)) {
      
      Bool isD = (size & 1) == 1;
      IROp op  = isD ? Iop_Sqrt64Fx2 : Iop_Sqrt32Fx4;
      if (bitQ == 0 && isD) return False; 
      IRTemp resV = newTempV128();
      assign(resV, binop(op, mkexpr(mk_get_IR_rounding_mode()),
                             getQReg128(nn)));
      putQReg128(dd, math_MAYBE_ZERO_HI64(bitQ, resV));
      const HChar* arr = bitQ == 0 ? "2s" : (size == X11 ? "2d" : "4s");
      DIP("%s %s.%s, %s.%s\n", "fsqrt",
          nameQReg128(dd), arr, nameQReg128(nn), arr);
      return True;
   }

   return False;
#  undef INSN
}


static
Bool dis_AdvSIMD_vector_x_indexed_elem(DisResult* dres, UInt insn)
{
#  define INSN(_bMax,_bMin)  SLICE_UInt(insn, (_bMax), (_bMin))
   if (INSN(31,31) != 0
       || INSN(28,24) != BITS5(0,1,1,1,1) || INSN(10,10) !=0) {
      return False;
   }
   UInt bitQ   = INSN(30,30);
   UInt bitU   = INSN(29,29);
   UInt size   = INSN(23,22);
   UInt bitL   = INSN(21,21);
   UInt bitM   = INSN(20,20);
   UInt mmLO4  = INSN(19,16);
   UInt opcode = INSN(15,12);
   UInt bitH   = INSN(11,11);
   UInt nn     = INSN(9,5);
   UInt dd     = INSN(4,0);
   vassert(size < 4);
   vassert(bitH < 2 && bitM < 2 && bitL < 2);

   if (bitU == 0 && size >= X10
       && (opcode == BITS4(0,0,0,1) || opcode == BITS4(0,1,0,1))) {
      
      
      if (bitQ == 0 && size == X11) return False; 
      Bool isD   = (size & 1) == 1;
      Bool isSUB = opcode == BITS4(0,1,0,1);
      UInt index;
      if      (!isD)             index = (bitH << 1) | bitL;
      else if (isD && bitL == 0) index = bitH;
      else return False; 
      vassert(index < (isD ? 2 : 4));
      IRType ity   = isD ? Ity_F64 : Ity_F32;
      IRTemp elem  = newTemp(ity);
      UInt   mm    = (bitM << 4) | mmLO4;
      assign(elem, getQRegLane(mm, index, ity));
      IRTemp dupd  = math_DUP_TO_V128(elem, ity);
      IROp   opADD = isD ? Iop_Add64Fx2 : Iop_Add32Fx4;
      IROp   opSUB = isD ? Iop_Sub64Fx2 : Iop_Sub32Fx4;
      IROp   opMUL = isD ? Iop_Mul64Fx2 : Iop_Mul32Fx4;
      IRTemp rm    = mk_get_IR_rounding_mode();
      IRTemp t1    = newTempV128();
      IRTemp t2    = newTempV128();
      
      assign(t1, triop(opMUL, mkexpr(rm), getQReg128(nn), mkexpr(dupd)));
      assign(t2, triop(isSUB ? opSUB : opADD,
                       mkexpr(rm), getQReg128(dd), mkexpr(t1)));
      putQReg128(dd, math_MAYBE_ZERO_HI64(bitQ, t2));
      const HChar* arr = bitQ == 0 ? "2s" : (isD ? "2d" : "4s");
      DIP("%s %s.%s, %s.%s, %s.%c[%u]\n", isSUB ? "fmls" : "fmla",
          nameQReg128(dd), arr, nameQReg128(nn), arr, nameQReg128(mm),
          isD ? 'd' : 's', index);
      return True;
   }

   if (size >= X10 && opcode == BITS4(1,0,0,1)) {
      
      
      if (bitQ == 0 && size == X11) return False; 
      Bool isD    = (size & 1) == 1;
      Bool isMULX = bitU == 1;
      UInt index;
      if      (!isD)             index = (bitH << 1) | bitL;
      else if (isD && bitL == 0) index = bitH;
      else return False; 
      vassert(index < (isD ? 2 : 4));
      IRType ity  = isD ? Ity_F64 : Ity_F32;
      IRTemp elem = newTemp(ity);
      UInt   mm   = (bitM << 4) | mmLO4;
      assign(elem, getQRegLane(mm, index, ity));
      IRTemp dupd = math_DUP_TO_V128(elem, ity);
      
      IRTemp res  = newTempV128();
      assign(res, triop(isD ? Iop_Mul64Fx2 : Iop_Mul32Fx4,
                        mkexpr(mk_get_IR_rounding_mode()),
                        getQReg128(nn), mkexpr(dupd)));
      putQReg128(dd, math_MAYBE_ZERO_HI64(bitQ, res));
      const HChar* arr = bitQ == 0 ? "2s" : (isD ? "2d" : "4s");
      DIP("%s %s.%s, %s.%s, %s.%c[%u]\n", 
          isMULX ? "fmulx" : "fmul", nameQReg128(dd), arr,
          nameQReg128(nn), arr, nameQReg128(mm), isD ? 'd' : 's', index);
      return True;
   }

   if ((bitU == 1 && (opcode == BITS4(0,0,0,0) || opcode == BITS4(0,1,0,0)))
       || (bitU == 0 && opcode == BITS4(1,0,0,0))) {
      
      
      
      Bool isMLA = opcode == BITS4(0,0,0,0);
      Bool isMLS = opcode == BITS4(0,1,0,0);
      UInt mm    = 32; 
      UInt ix    = 16; 
      switch (size) {
         case X00:
            return False; 
         case X01:
            mm = mmLO4; ix = (bitH << 2) | (bitL << 1) | (bitM << 0); break;
         case X10:
            mm = (bitM << 4) | mmLO4; ix = (bitH << 1) | (bitL << 0); break;
         case X11:
            return False; 
         default:
            vassert(0);
      }
      vassert(mm < 32 && ix < 16);
      IROp   opMUL = mkVecMUL(size);
      IROp   opADD = mkVecADD(size);
      IROp   opSUB = mkVecSUB(size);
      HChar  ch    = size == X01 ? 'h' : 's';
      IRTemp vecM  = math_DUP_VEC_ELEM(getQReg128(mm), size, ix);
      IRTemp vecD  = newTempV128();
      IRTemp vecN  = newTempV128();
      IRTemp res   = newTempV128();
      assign(vecD, getQReg128(dd));
      assign(vecN, getQReg128(nn));
      IRExpr* prod = binop(opMUL, mkexpr(vecN), mkexpr(vecM));
      if (isMLA || isMLS) {
         assign(res, binop(isMLA ? opADD : opSUB, mkexpr(vecD), prod));
      } else {
         assign(res, prod);
      }
      putQReg128(dd, math_MAYBE_ZERO_HI64(bitQ, res));
      const HChar* arr = nameArr_Q_SZ(bitQ, size);
      DIP("%s %s.%s, %s.%s, %s.%c[%u]\n", isMLA ? "mla"
                                                : (isMLS ? "mls" : "mul"),
          nameQReg128(dd), arr,
          nameQReg128(nn), arr, nameQReg128(dd), ch, ix);
      return True;
   }

   if (opcode == BITS4(1,0,1,0)
       || opcode == BITS4(0,0,1,0) || opcode == BITS4(0,1,1,0)) {
       
       
       
       
       
       
      
      UInt ks = 3;
      switch (opcode) {
         case BITS4(1,0,1,0): ks = 0; break;
         case BITS4(0,0,1,0): ks = 1; break;
         case BITS4(0,1,1,0): ks = 2; break;
         default: vassert(0);
      }
      vassert(ks >= 0 && ks <= 2);
      Bool isU = bitU == 1;
      Bool is2 = bitQ == 1;
      UInt mm  = 32; 
      UInt ix  = 16; 
      switch (size) {
         case X00:
            return False; 
         case X01:
            mm = mmLO4; ix = (bitH << 2) | (bitL << 1) | (bitM << 0); break;
         case X10:
            mm = (bitM << 4) | mmLO4; ix = (bitH << 1) | (bitL << 0); break;
         case X11:
            return False; 
         default:
            vassert(0);
      }
      vassert(mm < 32 && ix < 16);
      IRTemp vecN  = newTempV128();
      IRTemp vecM  = math_DUP_VEC_ELEM(getQReg128(mm), size, ix);
      IRTemp vecD  = newTempV128();
      assign(vecN, getQReg128(nn));
      assign(vecD, getQReg128(dd));
      IRTemp res = IRTemp_INVALID;
      math_MULL_ACC(&res, is2, isU, size, "mas"[ks],
                    vecN, vecM, ks == 0 ? IRTemp_INVALID : vecD);
      putQReg128(dd, mkexpr(res));
      const HChar* nm        = ks == 0 ? "mull" : (ks == 1 ? "mlal" : "mlsl");
      const HChar* arrNarrow = nameArr_Q_SZ(bitQ, size);
      const HChar* arrWide   = nameArr_Q_SZ(1,    size+1);
      HChar ch               = size == X01 ? 'h' : 's';
      DIP("%c%s%s %s.%s, %s.%s, %s.%c[%u]\n",
          isU ? 'u' : 's', nm, is2 ? "2" : "",
          nameQReg128(dd), arrWide,
          nameQReg128(nn), arrNarrow, nameQReg128(dd), ch, ix);
      return True;
   }

   if (bitU == 0 
       && (opcode == BITS4(1,0,1,1)
           || opcode == BITS4(0,0,1,1) || opcode == BITS4(0,1,1,1))) {
       
       
       
      
      UInt ks = 3;
      switch (opcode) {
         case BITS4(1,0,1,1): ks = 0; break;
         case BITS4(0,0,1,1): ks = 1; break;
         case BITS4(0,1,1,1): ks = 2; break;
         default: vassert(0);
      }
      vassert(ks >= 0 && ks <= 2);
      Bool is2 = bitQ == 1;
      UInt mm  = 32; 
      UInt ix  = 16; 
      switch (size) {
         case X00:
            return False; 
         case X01:
            mm = mmLO4; ix = (bitH << 2) | (bitL << 1) | (bitM << 0); break;
         case X10:
            mm = (bitM << 4) | mmLO4; ix = (bitH << 1) | (bitL << 0); break;
         case X11:
            return False; 
         default:
            vassert(0);
      }
      vassert(mm < 32 && ix < 16);
      IRTemp vecN, vecD, res, sat1q, sat1n, sat2q, sat2n;
      vecN = vecD = res = sat1q = sat1n = sat2q = sat2n = IRTemp_INVALID;
      newTempsV128_2(&vecN, &vecD);
      assign(vecN, getQReg128(nn));
      IRTemp vecM  = math_DUP_VEC_ELEM(getQReg128(mm), size, ix);
      assign(vecD, getQReg128(dd));
      math_SQDMULL_ACC(&res, &sat1q, &sat1n, &sat2q, &sat2n,
                       is2, size, "mas"[ks],
                       vecN, vecM, ks == 0 ? IRTemp_INVALID : vecD);
      putQReg128(dd, mkexpr(res));
      vassert(sat1q != IRTemp_INVALID && sat1n != IRTemp_INVALID);
      updateQCFLAGwithDifference(sat1q, sat1n);
      if (sat2q != IRTemp_INVALID || sat2n != IRTemp_INVALID) {
         updateQCFLAGwithDifference(sat2q, sat2n);
      }
      const HChar* nm        = ks == 0 ? "sqdmull"
                                       : (ks == 1 ? "sqdmlal" : "sqdmlsl");
      const HChar* arrNarrow = nameArr_Q_SZ(bitQ, size);
      const HChar* arrWide   = nameArr_Q_SZ(1,    size+1);
      HChar ch               = size == X01 ? 'h' : 's';
      DIP("%s%s %s.%s, %s.%s, %s.%c[%u]\n",
          nm, is2 ? "2" : "",
          nameQReg128(dd), arrWide,
          nameQReg128(nn), arrNarrow, nameQReg128(dd), ch, ix);
      return True;
   }

   if (opcode == BITS4(1,1,0,0) || opcode == BITS4(1,1,0,1)) {
      
      
      UInt mm  = 32; 
      UInt ix  = 16; 
      switch (size) {
         case X00:
            return False; 
         case X01:
            mm = mmLO4; ix = (bitH << 2) | (bitL << 1) | (bitM << 0); break;
         case X10:
            mm = (bitM << 4) | mmLO4; ix = (bitH << 1) | (bitL << 0); break;
         case X11:
            return False; 
         default:
            vassert(0);
      }
      vassert(mm < 32 && ix < 16);
      Bool isR = opcode == BITS4(1,1,0,1);
      IRTemp res, sat1q, sat1n, vN, vM;
      res = sat1q = sat1n = vN = vM = IRTemp_INVALID;
      vN = newTempV128();
      assign(vN, getQReg128(nn));
      vM = math_DUP_VEC_ELEM(getQReg128(mm), size, ix);
      math_SQDMULH(&res, &sat1q, &sat1n, isR, size, vN, vM);
      putQReg128(dd, math_MAYBE_ZERO_HI64(bitQ, res));
      IROp opZHI = bitQ == 0 ? Iop_ZeroHI64ofV128 : Iop_INVALID;
      updateQCFLAGwithDifferenceZHI(sat1q, sat1n, opZHI);
      const HChar* nm  = isR ? "sqrdmulh" : "sqdmulh";
      const HChar* arr = nameArr_Q_SZ(bitQ, size);
      HChar ch         = size == X01 ? 'h' : 's';
      DIP("%s %s.%s, %s.%s, %s.%c[%u]\n", nm,
          nameQReg128(dd), arr, nameQReg128(nn), arr, nameQReg128(dd), ch, ix);
      return True;
   }

   return False;
#  undef INSN
}


static
Bool dis_AdvSIMD_crypto_aes(DisResult* dres, UInt insn)
{
#  define INSN(_bMax,_bMin)  SLICE_UInt(insn, (_bMax), (_bMin))
   return False;
#  undef INSN
}


static
Bool dis_AdvSIMD_crypto_three_reg_sha(DisResult* dres, UInt insn)
{
#  define INSN(_bMax,_bMin)  SLICE_UInt(insn, (_bMax), (_bMin))
   return False;
#  undef INSN
}


static
Bool dis_AdvSIMD_crypto_two_reg_sha(DisResult* dres, UInt insn)
{
#  define INSN(_bMax,_bMin)  SLICE_UInt(insn, (_bMax), (_bMin))
   return False;
#  undef INSN
}


static
Bool dis_AdvSIMD_fp_compare(DisResult* dres, UInt insn)
{
#  define INSN(_bMax,_bMin)  SLICE_UInt(insn, (_bMax), (_bMin))
   if (INSN(31,24) != BITS8(0,0,0,1,1,1,1,0)
       || INSN(21,21) != 1 || INSN(13,10) != BITS4(1,0,0,0)) {
      return False;
   }
   UInt ty      = INSN(23,22);
   UInt mm      = INSN(20,16);
   UInt op      = INSN(15,14);
   UInt nn      = INSN(9,5);
   UInt opcode2 = INSN(4,0);
   vassert(ty < 4);

   if (ty <= X01 && op == X00
       && (opcode2 & BITS5(0,0,1,1,1)) == BITS5(0,0,0,0,0)) {
      
      
      
      
      Bool   isD     = (ty & 1) == 1;
      Bool   isCMPE  = (opcode2 & 16) == 16;
      Bool   cmpZero = (opcode2 & 8) == 8;
      IRType ity     = isD ? Ity_F64 : Ity_F32;
      Bool   valid   = True;
      if (cmpZero && mm != 0) valid = False;
      if (valid) {
         IRTemp argL  = newTemp(ity);
         IRTemp argR  = newTemp(ity);
         IRTemp irRes = newTemp(Ity_I32);
         assign(argL, getQRegLO(nn, ity));
         assign(argR,
                cmpZero 
                   ? (IRExpr_Const(isD ? IRConst_F64i(0) : IRConst_F32i(0)))
                   : getQRegLO(mm, ity));
         assign(irRes, binop(isD ? Iop_CmpF64 : Iop_CmpF32,
                             mkexpr(argL), mkexpr(argR)));
         IRTemp nzcv = mk_convert_IRCmpF64Result_to_NZCV(irRes);
         IRTemp nzcv_28x0 = newTemp(Ity_I64);
         assign(nzcv_28x0, binop(Iop_Shl64, mkexpr(nzcv), mkU8(28)));
         setFlags_COPY(nzcv_28x0);
         DIP("fcmp%s %s, %s\n", isCMPE ? "e" : "", nameQRegLO(nn, ity),
             cmpZero ? "#0.0" : nameQRegLO(mm, ity));
         return True;
      }
      return False;
   }

   return False;
#  undef INSN
}


static
Bool dis_AdvSIMD_fp_conditional_compare(DisResult* dres, UInt insn)
{
#  define INSN(_bMax,_bMin)  SLICE_UInt(insn, (_bMax), (_bMin))
   if (INSN(31,24) != BITS8(0,0,0,1,1,1,1,0)
       || INSN(21,21) != 1 || INSN(11,10) != BITS2(0,1)) {
      return False;
   }
   UInt ty   = INSN(23,22);
   UInt mm   = INSN(20,16);
   UInt cond = INSN(15,12);
   UInt nn   = INSN(9,5);
   UInt op   = INSN(4,4);
   UInt nzcv = INSN(3,0);
   vassert(ty < 4 && op <= 1);

   if (ty <= BITS2(0,1)) {
      
      
      
      

      Bool   isD    = (ty & 1) == 1;
      Bool   isCMPE = op == 1;
      IRType ity    = isD ? Ity_F64 : Ity_F32;
      IRTemp argL   = newTemp(ity);
      IRTemp argR   = newTemp(ity);
      IRTemp irRes  = newTemp(Ity_I32);
      assign(argL,  getQRegLO(nn, ity));
      assign(argR,  getQRegLO(mm, ity));
      assign(irRes, binop(isD ? Iop_CmpF64 : Iop_CmpF32,
                          mkexpr(argL), mkexpr(argR)));
      IRTemp condT = newTemp(Ity_I1);
      assign(condT, unop(Iop_64to1, mk_arm64g_calculate_condition(cond)));
      IRTemp nzcvT = mk_convert_IRCmpF64Result_to_NZCV(irRes);

      IRTemp nzcvT_28x0 = newTemp(Ity_I64);
      assign(nzcvT_28x0, binop(Iop_Shl64, mkexpr(nzcvT), mkU8(28)));

      IRExpr* nzcvF_28x0 = mkU64(((ULong)nzcv) << 28);

      IRTemp nzcv_28x0 = newTemp(Ity_I64);
      assign(nzcv_28x0, IRExpr_ITE(mkexpr(condT),
                                   mkexpr(nzcvT_28x0), nzcvF_28x0));
      setFlags_COPY(nzcv_28x0);
      DIP("fccmp%s %s, %s, #%u, %s\n", isCMPE ? "e" : "",
          nameQRegLO(nn, ity), nameQRegLO(mm, ity), nzcv, nameCC(cond));
      return True;
   }

   return False;
#  undef INSN
}


static
Bool dis_AdvSIMD_fp_conditional_select(DisResult* dres, UInt insn)
{
#  define INSN(_bMax,_bMin)  SLICE_UInt(insn, (_bMax), (_bMin))
   if (INSN(31,24) != BITS8(0,0,0,1,1,1,1,0) || INSN(21,21) != 1
       || INSN(11,10) != BITS2(1,1)) {
      return False;
   }
   UInt ty   = INSN(23,22);
   UInt mm   = INSN(20,16);
   UInt cond = INSN(15,12);
   UInt nn   = INSN(9,5);
   UInt dd   = INSN(4,0);
   if (ty <= X01) {
      
      
      IRType ity = ty == X01 ? Ity_F64 : Ity_F32;
      IRTemp srcT = newTemp(ity);
      IRTemp srcF = newTemp(ity);
      IRTemp res  = newTemp(ity);
      assign(srcT, getQRegLO(nn, ity));
      assign(srcF, getQRegLO(mm, ity));
      assign(res, IRExpr_ITE(
                     unop(Iop_64to1, mk_arm64g_calculate_condition(cond)),
                     mkexpr(srcT), mkexpr(srcF)));
      putQReg128(dd, mkV128(0x0000));
      putQRegLO(dd, mkexpr(res));
      DIP("fcsel %s, %s, %s, %s\n",
          nameQRegLO(dd, ity), nameQRegLO(nn, ity), nameQRegLO(mm, ity),
          nameCC(cond));
      return True;
   }
   return False;
#  undef INSN
}


static
Bool dis_AdvSIMD_fp_data_proc_1_source(DisResult* dres, UInt insn)
{
#  define INSN(_bMax,_bMin)  SLICE_UInt(insn, (_bMax), (_bMin))
   if (INSN(31,24) != BITS8(0,0,0,1,1,1,1,0)
       || INSN(21,21) != 1 || INSN(14,10) != BITS5(1,0,0,0,0)) {
      return False;
   }
   UInt ty     = INSN(23,22);
   UInt opcode = INSN(20,15);
   UInt nn     = INSN(9,5);
   UInt dd     = INSN(4,0);

   if (ty <= X01 && opcode <= BITS6(0,0,0,0,1,1)) {
      
      
      
      
      IRType ity = ty == X01 ? Ity_F64 : Ity_F32;
      IRTemp src = newTemp(ity);
      IRTemp res = newTemp(ity);
      const HChar* nm = "??";
      assign(src, getQRegLO(nn, ity));
      switch (opcode) {
         case BITS6(0,0,0,0,0,0):
            nm = "fmov"; assign(res, mkexpr(src)); break;
         case BITS6(0,0,0,0,0,1):
            nm = "fabs"; assign(res, unop(mkABSF(ity), mkexpr(src))); break;
         case BITS6(0,0,0,0,1,0):
            nm = "fabs"; assign(res, unop(mkNEGF(ity), mkexpr(src))); break;
         case BITS6(0,0,0,0,1,1):
            nm = "fsqrt";
            assign(res, binop(mkSQRTF(ity), 
                              mkexpr(mk_get_IR_rounding_mode()),
                              mkexpr(src))); break;
         default:
            vassert(0);
      }
      putQReg128(dd, mkV128(0x0000));
      putQRegLO(dd, mkexpr(res));
      DIP("%s %s, %s\n", nm, nameQRegLO(dd, ity), nameQRegLO(nn, ity));
      return True;
   }

   if (   (ty == X11 && (opcode == BITS6(0,0,0,1,0,0) 
                         || opcode == BITS6(0,0,0,1,0,1)))
       || (ty == X00 && (opcode == BITS6(0,0,0,1,1,1) 
                         || opcode == BITS6(0,0,0,1,0,1)))
       || (ty == X01 && (opcode == BITS6(0,0,0,1,1,1) 
                         || opcode == BITS6(0,0,0,1,0,0)))) {
      
      
      
      
      
      
      UInt b2322 = ty;
      UInt b1615 = opcode & BITS2(1,1);
      switch ((b2322 << 2) | b1615) {
         case BITS4(0,0,0,1):   
         case BITS4(1,1,0,1): { 
            Bool   srcIsH = b2322 == BITS2(1,1);
            IRType srcTy  = srcIsH ? Ity_F16 : Ity_F32;
            IRTemp res    = newTemp(Ity_F64);
            assign(res, unop(srcIsH ? Iop_F16toF64 : Iop_F32toF64,
                             getQRegLO(nn, srcTy)));
            putQReg128(dd, mkV128(0x0000));
            putQRegLO(dd, mkexpr(res));
            DIP("fcvt %s, %s\n",
                nameQRegLO(dd, Ity_F64), nameQRegLO(nn, srcTy));
            return True;
         }
         case BITS4(0,1,0,0):   
         case BITS4(0,1,1,1): { 
            Bool   dstIsH = b1615 == BITS2(1,1);
            IRType dstTy  = dstIsH ? Ity_F16 : Ity_F32;
            IRTemp res    = newTemp(dstTy);
            assign(res, binop(dstIsH ? Iop_F64toF16 : Iop_F64toF32,
                              mkexpr(mk_get_IR_rounding_mode()),
                              getQRegLO(nn, Ity_F64)));
            putQReg128(dd, mkV128(0x0000));
            putQRegLO(dd, mkexpr(res));
            DIP("fcvt %s, %s\n",
                nameQRegLO(dd, dstTy), nameQRegLO(nn, Ity_F64));
            return True;
         }
         case BITS4(0,0,1,1):   
         case BITS4(1,1,0,0): { 
            Bool   toH   = b1615 == BITS2(1,1);
            IRType srcTy = toH ? Ity_F32 : Ity_F16;
            IRType dstTy = toH ? Ity_F16 : Ity_F32;
            IRTemp res = newTemp(dstTy);
            if (toH) {
               assign(res, binop(Iop_F32toF16,
                                 mkexpr(mk_get_IR_rounding_mode()),
                                 getQRegLO(nn, srcTy)));

            } else {
               assign(res, unop(Iop_F16toF32,
                                getQRegLO(nn, srcTy)));
            }
            putQReg128(dd, mkV128(0x0000));
            putQRegLO(dd, mkexpr(res));
            DIP("fcvt %s, %s\n",
                nameQRegLO(dd, dstTy), nameQRegLO(nn, srcTy));
            return True;
         }
         default:
            break;
      }
      
      return False;
   }

   if (ty <= X01
       && opcode >= BITS6(0,0,1,0,0,0) && opcode <= BITS6(0,0,1,1,1,1)
       && opcode != BITS6(0,0,1,1,0,1)) {
      
      
      
      
      
      
      
      Bool    isD   = (ty & 1) == 1;
      UInt    rm    = opcode & BITS6(0,0,0,1,1,1);
      IRType  ity   = isD ? Ity_F64 : Ity_F32;
      IRExpr* irrmE = NULL;
      UChar   ch    = '?';
      switch (rm) {
         case BITS3(0,1,1): ch = 'z'; irrmE = mkU32(Irrm_ZERO); break;
         case BITS3(0,1,0): ch = 'm'; irrmE = mkU32(Irrm_NegINF); break;
         case BITS3(0,0,1): ch = 'p'; irrmE = mkU32(Irrm_PosINF); break;
         
         case BITS3(1,0,0): ch = 'a'; irrmE = mkU32(Irrm_NEAREST); break;
         
         
         case BITS3(1,1,0):
            ch = 'x'; irrmE = mkexpr(mk_get_IR_rounding_mode()); break;
         case BITS3(1,1,1):
            ch = 'i'; irrmE = mkexpr(mk_get_IR_rounding_mode()); break;
         
         
         case BITS3(0,0,0): ch = 'n'; irrmE = mkU32(Irrm_NEAREST); break;
         default: break;
      }
      if (irrmE) {
         IRTemp src = newTemp(ity);
         IRTemp dst = newTemp(ity);
         assign(src, getQRegLO(nn, ity));
         assign(dst, binop(isD ? Iop_RoundF64toInt : Iop_RoundF32toInt,
                           irrmE, mkexpr(src)));
         putQReg128(dd, mkV128(0x0000));
         putQRegLO(dd, mkexpr(dst));
         DIP("frint%c %s, %s\n",
             ch, nameQRegLO(dd, ity), nameQRegLO(nn, ity));
         return True;
      }
      return False;
   }

   return False;
#  undef INSN
}


static
Bool dis_AdvSIMD_fp_data_proc_2_source(DisResult* dres, UInt insn)
{
#  define INSN(_bMax,_bMin)  SLICE_UInt(insn, (_bMax), (_bMin))
   if (INSN(31,24) != BITS8(0,0,0,1,1,1,1,0)
       || INSN(21,21) != 1 || INSN(11,10) != BITS2(1,0)) {
      return False;
   }
   UInt ty     = INSN(23,22);
   UInt mm     = INSN(20,16);
   UInt opcode = INSN(15,12);
   UInt nn     = INSN(9,5);
   UInt dd     = INSN(4,0);

   if (ty <= X01 && opcode <= BITS4(0,1,1,1)) {
      
      
      
      
      
      
      
      
      IRType ity = ty == X00 ? Ity_F32 : Ity_F64;
      IROp   iop = Iop_INVALID;
      const HChar* nm = "???";
      switch (opcode) {
         case BITS4(0,0,0,0): nm = "fmul"; iop = mkMULF(ity); break;
         case BITS4(0,0,0,1): nm = "fdiv"; iop = mkDIVF(ity); break;
         case BITS4(0,0,1,0): nm = "fadd"; iop = mkADDF(ity); break;
         case BITS4(0,0,1,1): nm = "fsub"; iop = mkSUBF(ity); break;
         case BITS4(0,1,0,0): nm = "fmax"; iop = mkVecMAXF(ty+2); break;
         case BITS4(0,1,0,1): nm = "fmin"; iop = mkVecMINF(ty+2); break;
         case BITS4(0,1,1,0): nm = "fmaxnm"; iop = mkVecMAXF(ty+2); break; 
         case BITS4(0,1,1,1): nm = "fminnm"; iop = mkVecMINF(ty+2); break; 
         default: vassert(0);
      }
      if (opcode <= BITS4(0,0,1,1)) {
         
         IRTemp res = newTemp(ity);
         assign(res, triop(iop, mkexpr(mk_get_IR_rounding_mode()),
                                getQRegLO(nn, ity), getQRegLO(mm, ity)));
         putQReg128(dd, mkV128(0));
         putQRegLO(dd, mkexpr(res));
      } else {
         putQReg128(dd, unop(mkVecZEROHIxxOFV128(ty+2),
                             binop(iop, getQReg128(nn), getQReg128(mm))));
      }
      DIP("%s %s, %s, %s\n",
          nm, nameQRegLO(dd, ity), nameQRegLO(nn, ity), nameQRegLO(mm, ity));
      return True;
   }

   if (ty <= X01 && opcode == BITS4(1,0,0,0)) {
      
      IRType ity  = ty == X00 ? Ity_F32 : Ity_F64;
      IROp   iop  = mkMULF(ity);
      IROp   iopn = mkNEGF(ity);
      const HChar* nm = "fnmul";
      IRExpr* resE = unop(iopn,
                          triop(iop, mkexpr(mk_get_IR_rounding_mode()),
                                getQRegLO(nn, ity), getQRegLO(mm, ity)));
      IRTemp  res  = newTemp(ity);
      assign(res, resE);
      putQReg128(dd, mkV128(0));
      putQRegLO(dd, mkexpr(res));
      DIP("%s %s, %s, %s\n",
          nm, nameQRegLO(dd, ity), nameQRegLO(nn, ity), nameQRegLO(mm, ity));
      return True;
   }

   return False;
#  undef INSN
}


static
Bool dis_AdvSIMD_fp_data_proc_3_source(DisResult* dres, UInt insn)
{
#  define INSN(_bMax,_bMin)  SLICE_UInt(insn, (_bMax), (_bMin))
   if (INSN(31,24) != BITS8(0,0,0,1,1,1,1,1)) {
      return False;
   }
   UInt ty    = INSN(23,22);
   UInt bitO1 = INSN(21,21);
   UInt mm    = INSN(20,16);
   UInt bitO0 = INSN(15,15);
   UInt aa    = INSN(14,10);
   UInt nn    = INSN(9,5);
   UInt dd    = INSN(4,0);
   vassert(ty < 4);

   if (ty <= X01) {
      
      
      
      
      
      Bool    isD   = (ty & 1) == 1;
      UInt    ix    = (bitO1 << 1) | bitO0;
      IRType  ity   = isD ? Ity_F64 : Ity_F32;
      IROp    opADD = mkADDF(ity);
      IROp    opSUB = mkSUBF(ity);
      IROp    opMUL = mkMULF(ity);
      IROp    opNEG = mkNEGF(ity);
      IRTemp  res   = newTemp(ity);
      IRExpr* eA    = getQRegLO(aa, ity);
      IRExpr* eN    = getQRegLO(nn, ity);
      IRExpr* eM    = getQRegLO(mm, ity);
      IRExpr* rm    = mkexpr(mk_get_IR_rounding_mode());
      IRExpr* eNxM  = triop(opMUL, rm, eN, eM);
      switch (ix) {
         case 0:  assign(res, triop(opADD, rm, eA, eNxM)); break;
         case 1:  assign(res, triop(opSUB, rm, eA, eNxM)); break;
         case 2:  assign(res, unop(opNEG, triop(opADD, rm, eA, eNxM))); break;
         case 3:  assign(res, unop(opNEG, triop(opSUB, rm, eA, eNxM))); break;
         default: vassert(0);
      }
      putQReg128(dd, mkV128(0x0000));
      putQRegLO(dd, mkexpr(res));
      const HChar* names[4] = { "fmadd", "fmsub", "fnmadd", "fnmsub" };
      DIP("%s %s, %s, %s, %s\n",
          names[ix], nameQRegLO(dd, ity), nameQRegLO(nn, ity),
                     nameQRegLO(mm, ity), nameQRegLO(aa, ity));
      return True;
   }

   return False;
#  undef INSN
}


static
Bool dis_AdvSIMD_fp_immediate(DisResult* dres, UInt insn)
{
#  define INSN(_bMax,_bMin)  SLICE_UInt(insn, (_bMax), (_bMin))
   if (INSN(31,24) != BITS8(0,0,0,1,1,1,1,0)
       || INSN(21,21) != 1 || INSN(12,10) != BITS3(1,0,0)) {
      return False;
   }
   UInt ty     = INSN(23,22);
   UInt imm8   = INSN(20,13);
   UInt imm5   = INSN(9,5);
   UInt dd     = INSN(4,0);

   
   
   if (ty <= X01 && imm5 == BITS5(0,0,0,0,0)) {
      Bool  isD  = (ty & 1) == 1;
      ULong imm  = VFPExpandImm(imm8, isD ? 64 : 32);
      if (!isD) {
         vassert(0 == (imm & 0xFFFFFFFF00000000ULL));
      }
      putQReg128(dd, mkV128(0));
      putQRegLO(dd, isD ? mkU64(imm) : mkU32(imm & 0xFFFFFFFFULL));
      DIP("fmov %s, #0x%llx\n",
          nameQRegLO(dd, isD ? Ity_F64 : Ity_F32), imm);
      return True;
   }

   return False;
#  undef INSN
}


static
Bool dis_AdvSIMD_fp_to_from_fixedp_conv(DisResult* dres, UInt insn)
{
#  define INSN(_bMax,_bMin)  SLICE_UInt(insn, (_bMax), (_bMin))
#  define INSN(_bMax,_bMin)  SLICE_UInt(insn, (_bMax), (_bMin))
   if (INSN(30,29) != BITS2(0,0)
       || INSN(28,24) != BITS5(1,1,1,1,0)
       || INSN(21,21) != 0) {
      return False;
   }
   UInt bitSF = INSN(31,31);
   UInt ty    = INSN(23,22); 
   UInt rm    = INSN(20,19); 
   UInt op    = INSN(18,16); 
   UInt sc    = INSN(15,10); 
   UInt nn    = INSN(9,5);
   UInt dd    = INSN(4,0);

   if (ty <= X01 && rm == X11 
       && (op == BITS3(0,0,0) || op == BITS3(0,0,1))) {
      
      
      
      
      

      
      
      
      
      Bool isI64 = bitSF == 1;
      Bool isF64 = (ty & 1) == 1;
      Bool isU   = (op & 1) == 1;
      UInt ix    = (isU ? 4 : 0) | (isI64 ? 2 : 0) | (isF64 ? 1 : 0);

      Int fbits = 64 - sc;
      vassert(fbits >= 1 && fbits <= (isI64 ? 64 : 32));      

      Double  scale  = two_to_the_plus(fbits);
      IRExpr* scaleE = isF64 ? IRExpr_Const(IRConst_F64(scale))
                             : IRExpr_Const(IRConst_F32( (Float)scale ));
      IROp    opMUL  = isF64 ? Iop_MulF64 : Iop_MulF32;

      const IROp ops[8]
        = { Iop_F32toI32S, Iop_F64toI32S, Iop_F32toI64S, Iop_F64toI64S,
            Iop_F32toI32U, Iop_F64toI32U, Iop_F32toI64U, Iop_F64toI64U };
      IRTemp irrm = newTemp(Ity_I32);
      assign(irrm, mkU32(Irrm_ZERO));

      IRExpr* src = getQRegLO(nn, isF64 ? Ity_F64 : Ity_F32);
      IRExpr* res = binop(ops[ix], mkexpr(irrm),
                                   triop(opMUL, mkexpr(irrm), src, scaleE));
      putIRegOrZR(isI64, dd, res);

      DIP("fcvtz%c %s, %s, #%d\n",
          isU ? 'u' : 's', nameIRegOrZR(isI64, dd),
          nameQRegLO(nn, isF64 ? Ity_F64 : Ity_F32), fbits);
      return True;
   }

   
   
   
   if (ty <= X01 && rm == X00 
       && (op == BITS3(0,1,0) || op == BITS3(0,1,1))
       && (bitSF == 1 || ((sc >> 5) & 1) == 1)) {
      Bool isI64 = bitSF == 1;
      Bool isF64 = (ty & 1) == 1;
      Bool isU   = (op & 1) == 1;
      UInt ix    = (isU ? 4 : 0) | (isI64 ? 2 : 0) | (isF64 ? 1 : 0);

      Int fbits = 64 - sc;
      vassert(fbits >= 1 && fbits <= (isI64 ? 64 : 32));      

      Double  scale  = two_to_the_minus(fbits);
      IRExpr* scaleE = isF64 ? IRExpr_Const(IRConst_F64(scale))
                             : IRExpr_Const(IRConst_F32( (Float)scale ));
      IROp    opMUL  = isF64 ? Iop_MulF64 : Iop_MulF32;

      const IROp ops[8]
        = { Iop_I32StoF32, Iop_I32StoF64, Iop_I64StoF32, Iop_I64StoF64,
            Iop_I32UtoF32, Iop_I32UtoF64, Iop_I64UtoF32, Iop_I64UtoF64 };
      IRExpr* src = getIRegOrZR(isI64, nn);
      IRExpr* res = (isF64 && !isI64) 
                       ? unop(ops[ix], src)
                       : binop(ops[ix],
                               mkexpr(mk_get_IR_rounding_mode()), src);
      putQReg128(dd, mkV128(0));
      putQRegLO(dd, triop(opMUL, mkU32(Irrm_NEAREST), res, scaleE));

      DIP("%ccvtf %s, %s, #%d\n",
          isU ? 'u' : 's', nameQRegLO(dd, isF64 ? Ity_F64 : Ity_F32), 
          nameIRegOrZR(isI64, nn), fbits);
      return True;
   }

   return False;
#  undef INSN
}


static
Bool dis_AdvSIMD_fp_to_from_int_conv(DisResult* dres, UInt insn)
{
#  define INSN(_bMax,_bMin)  SLICE_UInt(insn, (_bMax), (_bMin))
   if (INSN(30,29) != BITS2(0,0)
       || INSN(28,24) != BITS5(1,1,1,1,0)
       || INSN(21,21) != 1
       || INSN(15,10) != BITS6(0,0,0,0,0,0)) {
      return False;
   }
   UInt bitSF = INSN(31,31);
   UInt ty    = INSN(23,22); 
   UInt rm    = INSN(20,19); 
   UInt op    = INSN(18,16); 
   UInt nn    = INSN(9,5);
   UInt dd    = INSN(4,0);

   
   
   if (ty <= X01
       && (   ((op == BITS3(0,0,0) || op == BITS3(0,0,1)) && True)
           || ((op == BITS3(1,0,0) || op == BITS3(1,0,1)) && rm == BITS2(0,0))
          )
      ) {
      Bool isI64 = bitSF == 1;
      Bool isF64 = (ty & 1) == 1;
      Bool isU   = (op & 1) == 1;
      
      IRRoundingMode irrm = 8; 
      HChar ch = '?';
      if (op == BITS3(0,0,0) || op == BITS3(0,0,1)) {
         switch (rm) {
            case BITS2(0,0): ch = 'n'; irrm = Irrm_NEAREST; break;
            case BITS2(0,1): ch = 'p'; irrm = Irrm_PosINF; break;
            case BITS2(1,0): ch = 'm'; irrm = Irrm_NegINF; break;
            case BITS2(1,1): ch = 'z'; irrm = Irrm_ZERO; break;
            default: vassert(0);
         }
      } else {
         vassert(op == BITS3(1,0,0) || op == BITS3(1,0,1));
         switch (rm) {
            case BITS2(0,0): ch = 'a'; irrm = Irrm_NEAREST; break;
            default: vassert(0);
         }
      }
      vassert(irrm != 8);
      UInt ix = (isF64 ? 4 : 0) | (isI64 ? 2 : 0) | (isU ? 1 : 0);
      vassert(ix < 8);
      const IROp iops[8] 
         = { Iop_F32toI32S, Iop_F32toI32U, Iop_F32toI64S, Iop_F32toI64U,
             Iop_F64toI32S, Iop_F64toI32U, Iop_F64toI64S, Iop_F64toI64U };
      IROp iop = iops[ix];
      
      if (
             (iop == Iop_F32toI32S && irrm == Irrm_ZERO)   
          || (iop == Iop_F32toI32S && irrm == Irrm_NegINF) 
          || (iop == Iop_F32toI32S && irrm == Irrm_PosINF) 
          || (iop == Iop_F32toI32S && irrm == Irrm_NEAREST)
          
          || (iop == Iop_F32toI32U && irrm == Irrm_ZERO)   
          || (iop == Iop_F32toI32U && irrm == Irrm_NegINF) 
          || (iop == Iop_F32toI32U && irrm == Irrm_PosINF) 
          || (iop == Iop_F32toI32U && irrm == Irrm_NEAREST)
          
          || (iop == Iop_F32toI64S && irrm == Irrm_ZERO)   
          || (iop == Iop_F32toI64S && irrm == Irrm_NegINF) 
          || (iop == Iop_F32toI64S && irrm == Irrm_PosINF) 
          || (iop == Iop_F32toI64S && irrm == Irrm_NEAREST)
          
          || (iop == Iop_F32toI64U && irrm == Irrm_ZERO)   
          || (iop == Iop_F32toI64U && irrm == Irrm_NegINF) 
          || (iop == Iop_F32toI64U && irrm == Irrm_PosINF) 
          || (iop == Iop_F32toI64U && irrm == Irrm_NEAREST)
          
          || (iop == Iop_F64toI32S && irrm == Irrm_ZERO)   
          || (iop == Iop_F64toI32S && irrm == Irrm_NegINF) 
          || (iop == Iop_F64toI32S && irrm == Irrm_PosINF) 
          || (iop == Iop_F64toI32S && irrm == Irrm_NEAREST)
          
          || (iop == Iop_F64toI32U && irrm == Irrm_ZERO)   
          || (iop == Iop_F64toI32U && irrm == Irrm_NegINF) 
          || (iop == Iop_F64toI32U && irrm == Irrm_PosINF) 
          || (iop == Iop_F64toI32U && irrm == Irrm_NEAREST)
          
          || (iop == Iop_F64toI64S && irrm == Irrm_ZERO)   
          || (iop == Iop_F64toI64S && irrm == Irrm_NegINF) 
          || (iop == Iop_F64toI64S && irrm == Irrm_PosINF) 
          || (iop == Iop_F64toI64S && irrm == Irrm_NEAREST)
          
          || (iop == Iop_F64toI64U && irrm == Irrm_ZERO)   
          || (iop == Iop_F64toI64U && irrm == Irrm_NegINF) 
          || (iop == Iop_F64toI64U && irrm == Irrm_PosINF) 
          || (iop == Iop_F64toI64U && irrm == Irrm_NEAREST)
         ) {
        
      } else {
        return False;
      }
      IRType srcTy  = isF64 ? Ity_F64 : Ity_F32;
      IRType dstTy  = isI64 ? Ity_I64 : Ity_I32;
      IRTemp src    = newTemp(srcTy);
      IRTemp dst    = newTemp(dstTy);
      assign(src, getQRegLO(nn, srcTy));
      assign(dst, binop(iop, mkU32(irrm), mkexpr(src)));
      putIRegOrZR(isI64, dd, mkexpr(dst));
      DIP("fcvt%c%c %s, %s\n", ch, isU ? 'u' : 's',
          nameIRegOrZR(isI64, dd), nameQRegLO(nn, srcTy));
      return True;
   }

   
   
   if (ty <= X01 && rm == X00 && (op == BITS3(0,1,0) || op == BITS3(0,1,1))) {
      Bool isI64 = bitSF == 1;
      Bool isF64 = (ty & 1) == 1;
      Bool isU   = (op & 1) == 1;
      UInt ix    = (isU ? 4 : 0) | (isI64 ? 2 : 0) | (isF64 ? 1 : 0);
      const IROp ops[8]
        = { Iop_I32StoF32, Iop_I32StoF64, Iop_I64StoF32, Iop_I64StoF64,
            Iop_I32UtoF32, Iop_I32UtoF64, Iop_I64UtoF32, Iop_I64UtoF64 };
      IRExpr* src = getIRegOrZR(isI64, nn);
      IRExpr* res = (isF64 && !isI64) 
                       ? unop(ops[ix], src)
                       : binop(ops[ix],
                               mkexpr(mk_get_IR_rounding_mode()), src);
      putQReg128(dd, mkV128(0));
      putQRegLO(dd, res);
      DIP("%ccvtf %s, %s\n",
          isU ? 'u' : 's', nameQRegLO(dd, isF64 ? Ity_F64 : Ity_F32), 
          nameIRegOrZR(isI64, nn));
      return True;
   }

   
   
   if (1) {
      UInt ix = 0; 
      if (bitSF == 0) {
         if (ty == BITS2(0,0) && rm == BITS2(0,0) && op == BITS3(1,1,1))
            ix = 1;
         else
         if (ty == BITS2(0,0) && rm == BITS2(0,0) && op == BITS3(1,1,0))
            ix = 4;
      } else {
         vassert(bitSF == 1);
         if (ty == BITS2(0,1) && rm == BITS2(0,0) && op == BITS3(1,1,1))
            ix = 2;
         else
         if (ty == BITS2(0,1) && rm == BITS2(0,0) && op == BITS3(1,1,0))
            ix = 5;
         else
         if (ty == BITS2(1,0) && rm == BITS2(0,1) && op == BITS3(1,1,1))
            ix = 3;
         else
         if (ty == BITS2(1,0) && rm == BITS2(0,1) && op == BITS3(1,1,0))
            ix = 6;
      }
      if (ix > 0) {
         switch (ix) {
            case 1:
               putQReg128(dd, mkV128(0));
               putQRegLO(dd, getIReg32orZR(nn));
               DIP("fmov s%u, w%u\n", dd, nn);
               break;
            case 2:
               putQReg128(dd, mkV128(0));
               putQRegLO(dd, getIReg64orZR(nn));
               DIP("fmov d%u, x%u\n", dd, nn);
               break;
            case 3:
               putQRegHI64(dd, getIReg64orZR(nn));
               DIP("fmov v%u.d[1], x%u\n", dd, nn);
               break;
            case 4:
               putIReg32orZR(dd, getQRegLO(nn, Ity_I32));
               DIP("fmov w%u, s%u\n", dd, nn);
               break;
            case 5:
               putIReg64orZR(dd, getQRegLO(nn, Ity_I64));
               DIP("fmov x%u, d%u\n", dd, nn);
               break;
            case 6:
               putIReg64orZR(dd, getQRegHI64(nn));
               DIP("fmov x%u, v%u.d[1]\n", dd, nn);
               break;
            default:
               vassert(0);
         }
         return True;
      }
      
   }

   return False;
#  undef INSN
}


static
Bool dis_ARM64_simd_and_fp(DisResult* dres, UInt insn)
{
   Bool ok;
   ok = dis_AdvSIMD_EXT(dres, insn);
   if (UNLIKELY(ok)) return True;
   ok = dis_AdvSIMD_TBL_TBX(dres, insn);
   if (UNLIKELY(ok)) return True;
   ok = dis_AdvSIMD_ZIP_UZP_TRN(dres, insn);
   if (UNLIKELY(ok)) return True;
   ok = dis_AdvSIMD_across_lanes(dres, insn);
   if (UNLIKELY(ok)) return True;
   ok = dis_AdvSIMD_copy(dres, insn);
   if (UNLIKELY(ok)) return True;
   ok = dis_AdvSIMD_modified_immediate(dres, insn);
   if (UNLIKELY(ok)) return True;
   ok = dis_AdvSIMD_scalar_copy(dres, insn);
   if (UNLIKELY(ok)) return True;
   ok = dis_AdvSIMD_scalar_pairwise(dres, insn);
   if (UNLIKELY(ok)) return True;
   ok = dis_AdvSIMD_scalar_shift_by_imm(dres, insn);
   if (UNLIKELY(ok)) return True;
   ok = dis_AdvSIMD_scalar_three_different(dres, insn);
   if (UNLIKELY(ok)) return True;
   ok = dis_AdvSIMD_scalar_three_same(dres, insn);
   if (UNLIKELY(ok)) return True;
   ok = dis_AdvSIMD_scalar_two_reg_misc(dres, insn);
   if (UNLIKELY(ok)) return True;
   ok = dis_AdvSIMD_scalar_x_indexed_element(dres, insn);
   if (UNLIKELY(ok)) return True;
   ok = dis_AdvSIMD_shift_by_immediate(dres, insn);
   if (UNLIKELY(ok)) return True;
   ok = dis_AdvSIMD_three_different(dres, insn);
   if (UNLIKELY(ok)) return True;
   ok = dis_AdvSIMD_three_same(dres, insn);
   if (UNLIKELY(ok)) return True;
   ok = dis_AdvSIMD_two_reg_misc(dres, insn);
   if (UNLIKELY(ok)) return True;
   ok = dis_AdvSIMD_vector_x_indexed_elem(dres, insn);
   if (UNLIKELY(ok)) return True;
   ok = dis_AdvSIMD_crypto_aes(dres, insn);
   if (UNLIKELY(ok)) return True;
   ok = dis_AdvSIMD_crypto_three_reg_sha(dres, insn);
   if (UNLIKELY(ok)) return True;
   ok = dis_AdvSIMD_crypto_two_reg_sha(dres, insn);
   if (UNLIKELY(ok)) return True;
   ok = dis_AdvSIMD_fp_compare(dres, insn);
   if (UNLIKELY(ok)) return True;
   ok = dis_AdvSIMD_fp_conditional_compare(dres, insn);
   if (UNLIKELY(ok)) return True;
   ok = dis_AdvSIMD_fp_conditional_select(dres, insn);
   if (UNLIKELY(ok)) return True;
   ok = dis_AdvSIMD_fp_data_proc_1_source(dres, insn);
   if (UNLIKELY(ok)) return True;
   ok = dis_AdvSIMD_fp_data_proc_2_source(dres, insn);
   if (UNLIKELY(ok)) return True;
   ok = dis_AdvSIMD_fp_data_proc_3_source(dres, insn);
   if (UNLIKELY(ok)) return True;
   ok = dis_AdvSIMD_fp_immediate(dres, insn);
   if (UNLIKELY(ok)) return True;
   ok = dis_AdvSIMD_fp_to_from_fixedp_conv(dres, insn);
   if (UNLIKELY(ok)) return True;
   ok = dis_AdvSIMD_fp_to_from_int_conv(dres, insn);
   if (UNLIKELY(ok)) return True;
   return False;
}




static
Bool disInstr_ARM64_WRK (
        DisResult* dres,
        Bool         (*resteerOkFn) ( void*, Addr ),
        Bool         resteerCisOk,
        void*        callback_opaque,
        const UChar* guest_instr,
        const VexArchInfo* archinfo,
        const VexAbiInfo*  abiinfo
     )
{
   
#  define INSN(_bMax,_bMin)  SLICE_UInt(insn, (_bMax), (_bMin))


   
   dres->whatNext    = Dis_Continue;
   dres->len         = 4;
   dres->continueAt  = 0;
   dres->jk_StopHere = Ijk_INVALID;

   UInt insn = getUIntLittleEndianly( guest_instr );

   if (0) vex_printf("insn: 0x%x\n", insn);

   DIP("\t(arm64) 0x%llx:  ", (ULong)guest_PC_curr_instr);

   vassert(0 == (guest_PC_curr_instr & 3ULL));

   

   
   {
      const UChar* code = guest_instr;
      UInt word1 = 0x93CC0D8C;
      UInt word2 = 0x93CC358C;
      UInt word3 = 0x93CCCD8C;
      UInt word4 = 0x93CCF58C;
      if (getUIntLittleEndianly(code+ 0) == word1 &&
          getUIntLittleEndianly(code+ 4) == word2 &&
          getUIntLittleEndianly(code+ 8) == word3 &&
          getUIntLittleEndianly(code+12) == word4) {
         
         if (getUIntLittleEndianly(code+16) == 0xAA0A014A
                                               ) {
            
            DIP("x3 = client_request ( x4 )\n");
            putPC(mkU64( guest_PC_curr_instr + 20 ));
            dres->jk_StopHere = Ijk_ClientReq;
            dres->whatNext    = Dis_StopHere;
            return True;
         }
         else
         if (getUIntLittleEndianly(code+16) == 0xAA0B016B
                                               ) {
            
            DIP("x3 = guest_NRADDR\n");
            dres->len = 20;
            putIReg64orZR(3, IRExpr_Get( OFFB_NRADDR, Ity_I64 ));
            return True;
         }
         else
         if (getUIntLittleEndianly(code+16) == 0xAA0C018C
                                               ) {
            
            DIP("branch-and-link-to-noredir x8\n");
            putIReg64orZR(30, mkU64(guest_PC_curr_instr + 20));
            putPC(getIReg64orZR(8));
            dres->jk_StopHere = Ijk_NoRedir;
            dres->whatNext    = Dis_StopHere;
            return True;
         }
         else
         if (getUIntLittleEndianly(code+16) == 0xAA090129
                                               ) {
            
            DIP("IR injection\n");
            vex_inject_ir(irsb, Iend_LE);
            
            
            
            
            stmt(IRStmt_Put(OFFB_CMSTART, mkU64(guest_PC_curr_instr)));
            stmt(IRStmt_Put(OFFB_CMLEN,   mkU64(20)));
            putPC(mkU64( guest_PC_curr_instr + 20 ));
            dres->whatNext    = Dis_StopHere;
            dres->jk_StopHere = Ijk_InvalICache;
            return True;
         }
         
         return False;
         
      }
   }

   

   

   Bool ok = False;

   switch (INSN(28,25)) {
      case BITS4(1,0,0,0): case BITS4(1,0,0,1):
         
         ok = dis_ARM64_data_processing_immediate(dres, insn);
         break;
      case BITS4(1,0,1,0): case BITS4(1,0,1,1):
         
         ok = dis_ARM64_branch_etc(dres, insn, archinfo);
         break;
      case BITS4(0,1,0,0): case BITS4(0,1,1,0):
      case BITS4(1,1,0,0): case BITS4(1,1,1,0):
         
         ok = dis_ARM64_load_store(dres, insn);
         break;
      case BITS4(0,1,0,1): case BITS4(1,1,0,1):
         
         ok = dis_ARM64_data_processing_register(dres, insn);
         break;
      case BITS4(0,1,1,1): case BITS4(1,1,1,1): 
         
         ok = dis_ARM64_simd_and_fp(dres, insn);
         break;
      case BITS4(0,0,0,0): case BITS4(0,0,0,1):
      case BITS4(0,0,1,0): case BITS4(0,0,1,1):
         
         break;
      default:
         vassert(0); 
   }

   if (!ok) {
      vassert(dres->whatNext    == Dis_Continue);
      vassert(dres->len         == 4);
      vassert(dres->continueAt  == 0);
      vassert(dres->jk_StopHere == Ijk_INVALID);
   }

   return ok;

#  undef INSN
}




DisResult disInstr_ARM64 ( IRSB*        irsb_IN,
                           Bool         (*resteerOkFn) ( void*, Addr ),
                           Bool         resteerCisOk,
                           void*        callback_opaque,
                           const UChar* guest_code_IN,
                           Long         delta_IN,
                           Addr         guest_IP,
                           VexArch      guest_arch,
                           const VexArchInfo* archinfo,
                           const VexAbiInfo*  abiinfo,
                           VexEndness   host_endness_IN,
                           Bool         sigill_diag_IN )
{
   DisResult dres;
   vex_bzero(&dres, sizeof(dres));

   
   vassert(guest_arch == VexArchARM64);

   irsb                = irsb_IN;
   host_endness        = host_endness_IN;
   guest_PC_curr_instr = (Addr64)guest_IP;

   
   
   vassert((archinfo->arm64_dMinLine_lg2_szB - 2) <= 15);
   vassert((archinfo->arm64_iMinLine_lg2_szB - 2) <= 15);

   
   Bool ok = disInstr_ARM64_WRK( &dres,
                                 resteerOkFn, resteerCisOk, callback_opaque,
                                 &guest_code_IN[delta_IN],
                                 archinfo, abiinfo );
   if (ok) {
      
      vassert(dres.len == 4 || dres.len == 20);
      switch (dres.whatNext) {
         case Dis_Continue:
            putPC( mkU64(dres.len + guest_PC_curr_instr) );
            break;
         case Dis_ResteerU:
         case Dis_ResteerC:
            putPC(mkU64(dres.continueAt));
            break;
         case Dis_StopHere:
            break;
         default:
            vassert(0);
      }
      DIP("\n");
   } else {
      
      if (sigill_diag_IN) {
         Int   i, j;
         UChar buf[64];
         UInt  insn
                  = getUIntLittleEndianly( &guest_code_IN[delta_IN] );
         vex_bzero(buf, sizeof(buf));
         for (i = j = 0; i < 32; i++) {
            if (i > 0) {
              if ((i & 7) == 0) buf[j++] = ' ';
              else if ((i & 3) == 0) buf[j++] = '\'';
            }
            buf[j++] = (insn & (1<<(31-i))) ? '1' : '0';
         }
         vex_printf("disInstr(arm64): unhandled instruction 0x%08x\n", insn);
         vex_printf("disInstr(arm64): %s\n", buf);
      }

      putPC( mkU64(guest_PC_curr_instr) );
      dres.len         = 0;
      dres.whatNext    = Dis_StopHere;
      dres.jk_StopHere = Ijk_NoDecode;
      dres.continueAt  = 0;
   }
   return dres;
}


