

/*
   This file is part of Valgrind, a dynamic binary instrumentation
   framework.

   Copyright (C) 2004-2013 OpenWorks LLP
      info@open-works.net

   NEON support is
   Copyright (C) 2010-2013 Samsung Electronics
   contributed by Dmitry Zhurikhin <zhur@ispras.ru>
              and Kirill Batuzov <batuzovk@ispras.ru>

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
#include "libvex_guest_arm.h"

#include "main_util.h"
#include "main_globals.h"
#include "guest_generic_bb_to_IR.h"
#include "guest_arm_defs.h"




static VexEndness host_endness;

static Addr32 guest_R15_curr_instr_notENC;

static Bool __curr_is_Thumb;

static IRSB* irsb;


static Bool r15written;

static IRTemp r15guard; 

static IRTemp r15kind;



#define DIP(format, args...)           \
   if (vex_traceflags & VEX_TRACE_FE)  \
      vex_printf(format, ## args)

#define DIS(buf, format, args...)      \
   if (vex_traceflags & VEX_TRACE_FE)  \
      vex_sprintf(buf, format, ## args)

#define ASSERT_IS_THUMB \
   do { vassert(__curr_is_Thumb); } while (0)

#define ASSERT_IS_ARM \
   do { vassert(! __curr_is_Thumb); } while (0)



static inline UInt getUIntLittleEndianly ( const UChar* p )
{
   UInt w = 0;
   w = (w << 8) | p[3];
   w = (w << 8) | p[2];
   w = (w << 8) | p[1];
   w = (w << 8) | p[0];
   return w;
}

static inline UShort getUShortLittleEndianly ( const UChar* p )
{
   UShort w = 0;
   w = (w << 8) | p[1];
   w = (w << 8) | p[0];
   return w;
}

static UInt ROR32 ( UInt x, UInt sh ) {
   vassert(sh >= 0 && sh < 32);
   if (sh == 0)
      return x;
   else
      return (x << (32-sh)) | (x >> sh);
}

static Int popcount32 ( UInt x )
{
   Int res = 0, i;
   for (i = 0; i < 32; i++) {
      res += (x & 1);
      x >>= 1;
   }
   return res;
}

static UInt setbit32 ( UInt x, Int ix, UInt b )
{
   UInt mask = 1 << ix;
   x &= ~mask;
   x |= ((b << ix) & mask);
   return x;
}

#define BITS2(_b1,_b0) \
   (((_b1) << 1) | (_b0))

#define BITS3(_b2,_b1,_b0)                      \
  (((_b2) << 2) | ((_b1) << 1) | (_b0))

#define BITS4(_b3,_b2,_b1,_b0) \
   (((_b3) << 3) | ((_b2) << 2) | ((_b1) << 1) | (_b0))

#define BITS8(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0)  \
   ((BITS4((_b7),(_b6),(_b5),(_b4)) << 4) \
    | BITS4((_b3),(_b2),(_b1),(_b0)))

#define BITS5(_b4,_b3,_b2,_b1,_b0)  \
   (BITS8(0,0,0,(_b4),(_b3),(_b2),(_b1),(_b0)))
#define BITS6(_b5,_b4,_b3,_b2,_b1,_b0)  \
   (BITS8(0,0,(_b5),(_b4),(_b3),(_b2),(_b1),(_b0)))
#define BITS7(_b6,_b5,_b4,_b3,_b2,_b1,_b0)  \
   (BITS8(0,(_b6),(_b5),(_b4),(_b3),(_b2),(_b1),(_b0)))

#define BITS9(_b8,_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0)      \
   (((_b8) << 8) \
    | BITS8((_b7),(_b6),(_b5),(_b4),(_b3),(_b2),(_b1),(_b0)))

#define BITS10(_b9,_b8,_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0)  \
   (((_b9) << 9) | ((_b8) << 8)                                \
    | BITS8((_b7),(_b6),(_b5),(_b4),(_b3),(_b2),(_b1),(_b0)))

#define SLICE_UInt(_uint,_bMax,_bMin) \
   (( ((UInt)(_uint)) >> (_bMin)) \
    & (UInt)((1ULL << ((_bMax) - (_bMin) + 1)) - 1ULL))



static IRExpr* mkU64 ( ULong i )
{
   return IRExpr_Const(IRConst_U64(i));
}

static IRExpr* mkU32 ( UInt i )
{
   return IRExpr_Const(IRConst_U32(i));
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

static void storeGuardedLE ( IRExpr* addr, IRExpr* data, IRTemp guardT )
{
   if (guardT == IRTemp_INVALID) {
      
      storeLE(addr, data);
   } else {
      stmt( IRStmt_StoreG(Iend_LE, addr, data,
                          binop(Iop_CmpNE32, mkexpr(guardT), mkU32(0))) );
   }
}

static void loadGuardedLE ( IRTemp dst, IRLoadGOp cvt,
                            IRExpr* addr, IRExpr* alt, 
                            IRTemp guardT  )
{
   if (guardT == IRTemp_INVALID) {
      
      IRExpr* loaded = NULL;
      switch (cvt) {
         case ILGop_Ident32:
            loaded = loadLE(Ity_I32, addr); break;
         case ILGop_8Uto32:
            loaded = unop(Iop_8Uto32, loadLE(Ity_I8, addr)); break;
         case ILGop_8Sto32:
            loaded = unop(Iop_8Sto32, loadLE(Ity_I8, addr)); break;
         case ILGop_16Uto32:
            loaded = unop(Iop_16Uto32, loadLE(Ity_I16, addr)); break;
         case ILGop_16Sto32:
            loaded = unop(Iop_16Sto32, loadLE(Ity_I16, addr)); break;
         default:
            vassert(0);
      }
      vassert(loaded != NULL);
      assign(dst, loaded);
   } else {
      stmt( IRStmt_LoadG(Iend_LE, cvt, dst, addr, alt,
                         binop(Iop_CmpNE32, mkexpr(guardT), mkU32(0))) );
   }
}

static IRTemp newTemp ( IRType ty )
{
   vassert(isPlausibleIRType(ty));
   return newIRTemp( irsb->tyenv, ty );
}

static IRExpr*  get_FAKE_roundingmode ( void )
{
   return mkU32(Irrm_NEAREST);
}

static IRExpr* genROR32( IRTemp src, Int rot )
{
   vassert(rot >= 0 && rot < 32);
   if (rot == 0)
      return mkexpr(src);
   return
      binop(Iop_Or32,
            binop(Iop_Shl32, mkexpr(src), mkU8(32 - rot)),
            binop(Iop_Shr32, mkexpr(src), mkU8(rot)));
}

static IRExpr* mkU128 ( ULong i )
{
   return binop(Iop_64HLtoV128, mkU64(i), mkU64(i));
}

static IRExpr* align4if ( IRExpr* e, Bool b )
{
   if (b)
      return binop(Iop_And32, e, mkU32(~3));
   else
      return e;
}



#define OFFB_R0       offsetof(VexGuestARMState,guest_R0)
#define OFFB_R1       offsetof(VexGuestARMState,guest_R1)
#define OFFB_R2       offsetof(VexGuestARMState,guest_R2)
#define OFFB_R3       offsetof(VexGuestARMState,guest_R3)
#define OFFB_R4       offsetof(VexGuestARMState,guest_R4)
#define OFFB_R5       offsetof(VexGuestARMState,guest_R5)
#define OFFB_R6       offsetof(VexGuestARMState,guest_R6)
#define OFFB_R7       offsetof(VexGuestARMState,guest_R7)
#define OFFB_R8       offsetof(VexGuestARMState,guest_R8)
#define OFFB_R9       offsetof(VexGuestARMState,guest_R9)
#define OFFB_R10      offsetof(VexGuestARMState,guest_R10)
#define OFFB_R11      offsetof(VexGuestARMState,guest_R11)
#define OFFB_R12      offsetof(VexGuestARMState,guest_R12)
#define OFFB_R13      offsetof(VexGuestARMState,guest_R13)
#define OFFB_R14      offsetof(VexGuestARMState,guest_R14)
#define OFFB_R15T     offsetof(VexGuestARMState,guest_R15T)

#define OFFB_CC_OP    offsetof(VexGuestARMState,guest_CC_OP)
#define OFFB_CC_DEP1  offsetof(VexGuestARMState,guest_CC_DEP1)
#define OFFB_CC_DEP2  offsetof(VexGuestARMState,guest_CC_DEP2)
#define OFFB_CC_NDEP  offsetof(VexGuestARMState,guest_CC_NDEP)
#define OFFB_NRADDR   offsetof(VexGuestARMState,guest_NRADDR)

#define OFFB_D0       offsetof(VexGuestARMState,guest_D0)
#define OFFB_D1       offsetof(VexGuestARMState,guest_D1)
#define OFFB_D2       offsetof(VexGuestARMState,guest_D2)
#define OFFB_D3       offsetof(VexGuestARMState,guest_D3)
#define OFFB_D4       offsetof(VexGuestARMState,guest_D4)
#define OFFB_D5       offsetof(VexGuestARMState,guest_D5)
#define OFFB_D6       offsetof(VexGuestARMState,guest_D6)
#define OFFB_D7       offsetof(VexGuestARMState,guest_D7)
#define OFFB_D8       offsetof(VexGuestARMState,guest_D8)
#define OFFB_D9       offsetof(VexGuestARMState,guest_D9)
#define OFFB_D10      offsetof(VexGuestARMState,guest_D10)
#define OFFB_D11      offsetof(VexGuestARMState,guest_D11)
#define OFFB_D12      offsetof(VexGuestARMState,guest_D12)
#define OFFB_D13      offsetof(VexGuestARMState,guest_D13)
#define OFFB_D14      offsetof(VexGuestARMState,guest_D14)
#define OFFB_D15      offsetof(VexGuestARMState,guest_D15)
#define OFFB_D16      offsetof(VexGuestARMState,guest_D16)
#define OFFB_D17      offsetof(VexGuestARMState,guest_D17)
#define OFFB_D18      offsetof(VexGuestARMState,guest_D18)
#define OFFB_D19      offsetof(VexGuestARMState,guest_D19)
#define OFFB_D20      offsetof(VexGuestARMState,guest_D20)
#define OFFB_D21      offsetof(VexGuestARMState,guest_D21)
#define OFFB_D22      offsetof(VexGuestARMState,guest_D22)
#define OFFB_D23      offsetof(VexGuestARMState,guest_D23)
#define OFFB_D24      offsetof(VexGuestARMState,guest_D24)
#define OFFB_D25      offsetof(VexGuestARMState,guest_D25)
#define OFFB_D26      offsetof(VexGuestARMState,guest_D26)
#define OFFB_D27      offsetof(VexGuestARMState,guest_D27)
#define OFFB_D28      offsetof(VexGuestARMState,guest_D28)
#define OFFB_D29      offsetof(VexGuestARMState,guest_D29)
#define OFFB_D30      offsetof(VexGuestARMState,guest_D30)
#define OFFB_D31      offsetof(VexGuestARMState,guest_D31)

#define OFFB_FPSCR    offsetof(VexGuestARMState,guest_FPSCR)
#define OFFB_TPIDRURO offsetof(VexGuestARMState,guest_TPIDRURO)
#define OFFB_ITSTATE  offsetof(VexGuestARMState,guest_ITSTATE)
#define OFFB_QFLAG32  offsetof(VexGuestARMState,guest_QFLAG32)
#define OFFB_GEFLAG0  offsetof(VexGuestARMState,guest_GEFLAG0)
#define OFFB_GEFLAG1  offsetof(VexGuestARMState,guest_GEFLAG1)
#define OFFB_GEFLAG2  offsetof(VexGuestARMState,guest_GEFLAG2)
#define OFFB_GEFLAG3  offsetof(VexGuestARMState,guest_GEFLAG3)

#define OFFB_CMSTART  offsetof(VexGuestARMState,guest_CMSTART)
#define OFFB_CMLEN    offsetof(VexGuestARMState,guest_CMLEN)



static Int integerGuestRegOffset ( UInt iregNo )
{
   switch (iregNo) {
      case 0:  return OFFB_R0;
      case 1:  return OFFB_R1;
      case 2:  return OFFB_R2;
      case 3:  return OFFB_R3;
      case 4:  return OFFB_R4;
      case 5:  return OFFB_R5;
      case 6:  return OFFB_R6;
      case 7:  return OFFB_R7;
      case 8:  return OFFB_R8;
      case 9:  return OFFB_R9;
      case 10: return OFFB_R10;
      case 11: return OFFB_R11;
      case 12: return OFFB_R12;
      case 13: return OFFB_R13;
      case 14: return OFFB_R14;
      case 15: return OFFB_R15T;
      default: vassert(0);
   }
}

static IRExpr* llGetIReg ( UInt iregNo )
{
   vassert(iregNo < 16);
   return IRExpr_Get( integerGuestRegOffset(iregNo), Ity_I32 );
}

static IRExpr* getIRegA ( UInt iregNo )
{
   IRExpr* e;
   ASSERT_IS_ARM;
   vassert(iregNo < 16);
   if (iregNo == 15) {
      vassert(0 == (guest_R15_curr_instr_notENC & 3));
      e = mkU32(guest_R15_curr_instr_notENC + 8);
   } else {
      e = IRExpr_Get( integerGuestRegOffset(iregNo), Ity_I32 );
   }
   return e;
}

static IRExpr* getIRegT ( UInt iregNo )
{
   IRExpr* e;
   ASSERT_IS_THUMB;
   vassert(iregNo < 16);
   if (iregNo == 15) {
      
      vassert(0 == (guest_R15_curr_instr_notENC & 1));
      e = mkU32(guest_R15_curr_instr_notENC + 4);
   } else {
      e = IRExpr_Get( integerGuestRegOffset(iregNo), Ity_I32 );
   }
   return e;
}

static void llPutIReg ( UInt iregNo, IRExpr* e )
{
   vassert(iregNo < 16);
   vassert(typeOfIRExpr(irsb->tyenv, e) == Ity_I32);
   stmt( IRStmt_Put(integerGuestRegOffset(iregNo), e) );
}

static void putIRegA ( UInt       iregNo,
                       IRExpr*    e,
                       IRTemp     guardT ,
                       IRJumpKind jk  )
{
   
   
   
   
   
   
   ASSERT_IS_ARM;
   if (guardT == IRTemp_INVALID) {
      
      llPutIReg( iregNo, e );
   } else {
      llPutIReg( iregNo,
                 IRExpr_ITE( binop(Iop_CmpNE32, mkexpr(guardT), mkU32(0)),
                             e, llGetIReg(iregNo) ));
   }
   if (iregNo == 15) {
      
      
      
      vassert(r15written == False);
      vassert(r15guard   == IRTemp_INVALID);
      vassert(r15kind    == Ijk_Boring);
      r15written = True;
      r15guard   = guardT;
      r15kind    = jk;
   }
}


static void putIRegT ( UInt       iregNo,
                       IRExpr*    e,
                       IRTemp     guardT  )
{
   ASSERT_IS_THUMB;
   vassert(iregNo >= 0 && iregNo <= 14);
   if (guardT == IRTemp_INVALID) {
      
      llPutIReg( iregNo, e );
   } else {
      llPutIReg( iregNo,
                 IRExpr_ITE( binop(Iop_CmpNE32, mkexpr(guardT), mkU32(0)),
                             e, llGetIReg(iregNo) ));
   }
}


static Bool isBadRegT ( UInt r )
{
   vassert(r <= 15);
   ASSERT_IS_THUMB;
   return r == 13 || r == 15;
}



static Int doubleGuestRegOffset ( UInt dregNo )
{
   switch (dregNo) {
      case 0:  return OFFB_D0;
      case 1:  return OFFB_D1;
      case 2:  return OFFB_D2;
      case 3:  return OFFB_D3;
      case 4:  return OFFB_D4;
      case 5:  return OFFB_D5;
      case 6:  return OFFB_D6;
      case 7:  return OFFB_D7;
      case 8:  return OFFB_D8;
      case 9:  return OFFB_D9;
      case 10: return OFFB_D10;
      case 11: return OFFB_D11;
      case 12: return OFFB_D12;
      case 13: return OFFB_D13;
      case 14: return OFFB_D14;
      case 15: return OFFB_D15;
      case 16: return OFFB_D16;
      case 17: return OFFB_D17;
      case 18: return OFFB_D18;
      case 19: return OFFB_D19;
      case 20: return OFFB_D20;
      case 21: return OFFB_D21;
      case 22: return OFFB_D22;
      case 23: return OFFB_D23;
      case 24: return OFFB_D24;
      case 25: return OFFB_D25;
      case 26: return OFFB_D26;
      case 27: return OFFB_D27;
      case 28: return OFFB_D28;
      case 29: return OFFB_D29;
      case 30: return OFFB_D30;
      case 31: return OFFB_D31;
      default: vassert(0);
   }
}

static IRExpr* llGetDReg ( UInt dregNo )
{
   vassert(dregNo < 32);
   return IRExpr_Get( doubleGuestRegOffset(dregNo), Ity_F64 );
}

static IRExpr* getDReg ( UInt dregNo ) {
   return llGetDReg( dregNo );
}

static void llPutDReg ( UInt dregNo, IRExpr* e )
{
   vassert(dregNo < 32);
   vassert(typeOfIRExpr(irsb->tyenv, e) == Ity_F64);
   stmt( IRStmt_Put(doubleGuestRegOffset(dregNo), e) );
}

static void putDReg ( UInt    dregNo,
                      IRExpr* e,
                      IRTemp  guardT )
{
   if (guardT == IRTemp_INVALID) {
      
      llPutDReg( dregNo, e );
   } else {
      llPutDReg( dregNo,
                 IRExpr_ITE( binop(Iop_CmpNE32, mkexpr(guardT), mkU32(0)),
                             e, llGetDReg(dregNo) ));
   }
}


static IRExpr* llGetDRegI64 ( UInt dregNo )
{
   vassert(dregNo < 32);
   return IRExpr_Get( doubleGuestRegOffset(dregNo), Ity_I64 );
}

static IRExpr* getDRegI64 ( UInt dregNo ) {
   return llGetDRegI64( dregNo );
}

static void llPutDRegI64 ( UInt dregNo, IRExpr* e )
{
   vassert(dregNo < 32);
   vassert(typeOfIRExpr(irsb->tyenv, e) == Ity_I64);
   stmt( IRStmt_Put(doubleGuestRegOffset(dregNo), e) );
}

static void putDRegI64 ( UInt    dregNo,
                         IRExpr* e,
                         IRTemp  guardT )
{
   if (guardT == IRTemp_INVALID) {
      
      llPutDRegI64( dregNo, e );
   } else {
      llPutDRegI64( dregNo,
                    IRExpr_ITE( binop(Iop_CmpNE32, mkexpr(guardT), mkU32(0)),
                                e, llGetDRegI64(dregNo) ));
   }
}


static Int quadGuestRegOffset ( UInt qregNo )
{
   switch (qregNo) {
      case 0:  return OFFB_D0;
      case 1:  return OFFB_D2;
      case 2:  return OFFB_D4;
      case 3:  return OFFB_D6;
      case 4:  return OFFB_D8;
      case 5:  return OFFB_D10;
      case 6:  return OFFB_D12;
      case 7:  return OFFB_D14;
      case 8:  return OFFB_D16;
      case 9:  return OFFB_D18;
      case 10: return OFFB_D20;
      case 11: return OFFB_D22;
      case 12: return OFFB_D24;
      case 13: return OFFB_D26;
      case 14: return OFFB_D28;
      case 15: return OFFB_D30;
      default: vassert(0);
   }
}

static IRExpr* llGetQReg ( UInt qregNo )
{
   vassert(qregNo < 16);
   return IRExpr_Get( quadGuestRegOffset(qregNo), Ity_V128 );
}

static IRExpr* getQReg ( UInt qregNo ) {
   return llGetQReg( qregNo );
}

static void llPutQReg ( UInt qregNo, IRExpr* e )
{
   vassert(qregNo < 16);
   vassert(typeOfIRExpr(irsb->tyenv, e) == Ity_V128);
   stmt( IRStmt_Put(quadGuestRegOffset(qregNo), e) );
}

static void putQReg ( UInt    qregNo,
                      IRExpr* e,
                      IRTemp  guardT )
{
   if (guardT == IRTemp_INVALID) {
      
      llPutQReg( qregNo, e );
   } else {
      llPutQReg( qregNo,
                 IRExpr_ITE( binop(Iop_CmpNE32, mkexpr(guardT), mkU32(0)),
                             e, llGetQReg(qregNo) ));
   }
}



static Int floatGuestRegOffset ( UInt fregNo )
{
   Int off;
   vassert(fregNo < 32);
   off = doubleGuestRegOffset(fregNo >> 1);
   if (host_endness == VexEndnessLE) {
      if (fregNo & 1)
         off += 4;
   } else {
      vassert(0);
   }
   return off;
}

static IRExpr* llGetFReg ( UInt fregNo )
{
   vassert(fregNo < 32);
   return IRExpr_Get( floatGuestRegOffset(fregNo), Ity_F32 );
}

static IRExpr* getFReg ( UInt fregNo ) {
   return llGetFReg( fregNo );
}

static void llPutFReg ( UInt fregNo, IRExpr* e )
{
   vassert(fregNo < 32);
   vassert(typeOfIRExpr(irsb->tyenv, e) == Ity_F32);
   stmt( IRStmt_Put(floatGuestRegOffset(fregNo), e) );
}

static void putFReg ( UInt    fregNo,
                      IRExpr* e,
                      IRTemp  guardT )
{
   if (guardT == IRTemp_INVALID) {
      
      llPutFReg( fregNo, e );
   } else {
      llPutFReg( fregNo,
                 IRExpr_ITE( binop(Iop_CmpNE32, mkexpr(guardT), mkU32(0)),
                             e, llGetFReg(fregNo) ));
   }
}



static void putMiscReg32 ( UInt    gsoffset, 
                           IRExpr* e, 
                           IRTemp  guardT )
{
   switch (gsoffset) {
      case OFFB_FPSCR:   break;
      case OFFB_QFLAG32: break;
      case OFFB_GEFLAG0: break;
      case OFFB_GEFLAG1: break;
      case OFFB_GEFLAG2: break;
      case OFFB_GEFLAG3: break;
      default: vassert(0); 
   }
   vassert(typeOfIRExpr(irsb->tyenv, e) == Ity_I32);

   if (guardT == IRTemp_INVALID) {
      
      stmt(IRStmt_Put(gsoffset, e));
   } else {
      stmt(IRStmt_Put(
         gsoffset,
         IRExpr_ITE( binop(Iop_CmpNE32, mkexpr(guardT), mkU32(0)),
                     e, IRExpr_Get(gsoffset, Ity_I32) )
      ));
   }
}

static IRTemp get_ITSTATE ( void )
{
   ASSERT_IS_THUMB;
   IRTemp t = newTemp(Ity_I32);
   assign(t, IRExpr_Get( OFFB_ITSTATE, Ity_I32));
   return t;
}

static void put_ITSTATE ( IRTemp t )
{
   ASSERT_IS_THUMB;
   stmt( IRStmt_Put( OFFB_ITSTATE, mkexpr(t)) );
}

static IRTemp get_QFLAG32 ( void )
{
   IRTemp t = newTemp(Ity_I32);
   assign(t, IRExpr_Get( OFFB_QFLAG32, Ity_I32));
   return t;
}

static void put_QFLAG32 ( IRTemp t, IRTemp condT )
{
   putMiscReg32( OFFB_QFLAG32, mkexpr(t), condT );
}

static void or_into_QFLAG32 ( IRExpr* e, IRTemp condT )
{
   IRTemp old = get_QFLAG32();
   IRTemp nyu = newTemp(Ity_I32);
   assign(nyu, binop(Iop_Or32, mkexpr(old), e) );
   put_QFLAG32(nyu, condT);
}

static void put_GEFLAG32 ( Int flagNo,            
                           Int lowbits_to_ignore, 
                           IRExpr* e,             
                           IRTemp condT )
{
   vassert( flagNo >= 0 && flagNo <= 3 );
   vassert( lowbits_to_ignore == 0  || 
            lowbits_to_ignore == 8  || 
            lowbits_to_ignore == 16 ||
            lowbits_to_ignore == 31 );
   IRTemp masked = newTemp(Ity_I32);
   assign(masked, binop(Iop_Shr32, e, mkU8(lowbits_to_ignore)));
 
   switch (flagNo) {
      case 0: putMiscReg32(OFFB_GEFLAG0, mkexpr(masked), condT); break;
      case 1: putMiscReg32(OFFB_GEFLAG1, mkexpr(masked), condT); break;
      case 2: putMiscReg32(OFFB_GEFLAG2, mkexpr(masked), condT); break;
      case 3: putMiscReg32(OFFB_GEFLAG3, mkexpr(masked), condT); break;
      default: vassert(0);
   }
}

static IRExpr* get_GEFLAG32( Int flagNo  )
{
   switch (flagNo) {
      case 0: return IRExpr_Get( OFFB_GEFLAG0, Ity_I32 );
      case 1: return IRExpr_Get( OFFB_GEFLAG1, Ity_I32 );
      case 2: return IRExpr_Get( OFFB_GEFLAG2, Ity_I32 );
      case 3: return IRExpr_Get( OFFB_GEFLAG3, Ity_I32 );
      default: vassert(0);
   }
}

static void set_GE_32_10_from_bits_31_15 ( IRTemp t32, IRTemp condT )
{
   IRTemp ge10 = newTemp(Ity_I32);
   IRTemp ge32 = newTemp(Ity_I32);
   assign(ge10, binop(Iop_And32, mkexpr(t32), mkU32(0x00008000)));
   assign(ge32, binop(Iop_And32, mkexpr(t32), mkU32(0x80000000)));
   put_GEFLAG32( 0, 0, mkexpr(ge10), condT );
   put_GEFLAG32( 1, 0, mkexpr(ge10), condT );
   put_GEFLAG32( 2, 0, mkexpr(ge32), condT );
   put_GEFLAG32( 3, 0, mkexpr(ge32), condT );
}


static void set_GE_3_2_1_0_from_bits_31_23_15_7 ( IRTemp t32, IRTemp condT )
{
   IRTemp ge0 = newTemp(Ity_I32);
   IRTemp ge1 = newTemp(Ity_I32);
   IRTemp ge2 = newTemp(Ity_I32);
   IRTemp ge3 = newTemp(Ity_I32);
   assign(ge0, binop(Iop_And32, mkexpr(t32), mkU32(0x00000080)));
   assign(ge1, binop(Iop_And32, mkexpr(t32), mkU32(0x00008000)));
   assign(ge2, binop(Iop_And32, mkexpr(t32), mkU32(0x00800000)));
   assign(ge3, binop(Iop_And32, mkexpr(t32), mkU32(0x80000000)));
   put_GEFLAG32( 0, 0, mkexpr(ge0), condT );
   put_GEFLAG32( 1, 0, mkexpr(ge1), condT );
   put_GEFLAG32( 2, 0, mkexpr(ge2), condT );
   put_GEFLAG32( 3, 0, mkexpr(ge3), condT );
}



static IRTemp  mk_get_IR_rounding_mode ( void )
{
   IRTemp armEncd = newTemp(Ity_I32);
   IRTemp swapped = newTemp(Ity_I32);
   assign(armEncd,
          binop(Iop_Shr32, IRExpr_Get(OFFB_FPSCR, Ity_I32), mkU8(22)));
   
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



static const HChar* name_ARMCondcode ( ARMCondcode cond )
{
   switch (cond) {
      case ARMCondEQ:  return "{eq}";
      case ARMCondNE:  return "{ne}";
      case ARMCondHS:  return "{hs}";  
      case ARMCondLO:  return "{lo}";  
      case ARMCondMI:  return "{mi}";
      case ARMCondPL:  return "{pl}";
      case ARMCondVS:  return "{vs}";
      case ARMCondVC:  return "{vc}";
      case ARMCondHI:  return "{hi}";
      case ARMCondLS:  return "{ls}";
      case ARMCondGE:  return "{ge}";
      case ARMCondLT:  return "{lt}";
      case ARMCondGT:  return "{gt}";
      case ARMCondLE:  return "{le}";
      case ARMCondAL:  return ""; 
      case ARMCondNV:  return "{nv}";
      default: vpanic("name_ARMCondcode");
   }
}
static const HChar* nCC ( ARMCondcode cond ) {
   return name_ARMCondcode(cond);
}


static IRExpr* mk_armg_calculate_condition_dyn ( IRExpr* cond )
{
   vassert(typeOfIRExpr(irsb->tyenv, cond) == Ity_I32);

   IRExpr** args
      = mkIRExprVec_4(
           binop(Iop_Or32, IRExpr_Get(OFFB_CC_OP, Ity_I32), cond),
           IRExpr_Get(OFFB_CC_DEP1, Ity_I32),
           IRExpr_Get(OFFB_CC_DEP2, Ity_I32),
           IRExpr_Get(OFFB_CC_NDEP, Ity_I32)
        );
   IRExpr* call
      = mkIRExprCCall(
           Ity_I32,
           0, 
           "armg_calculate_condition", &armg_calculate_condition,
           args
        );

   call->Iex.CCall.cee->mcx_mask = (1<<0) | (1<<3);
   return call;
}


static IRExpr* mk_armg_calculate_condition ( ARMCondcode cond )
{
   vassert(cond >= 0 && cond <= 15);
   return mk_armg_calculate_condition_dyn( mkU32(cond << 4) );
}


static IRExpr* mk_armg_calculate_flag_c ( void )
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
           "armg_calculate_flag_c", &armg_calculate_flag_c,
           args
        );
   call->Iex.CCall.cee->mcx_mask = (1<<0) | (1<<3);
   return call;
}


static IRExpr* mk_armg_calculate_flag_v ( void )
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
           "armg_calculate_flag_v", &armg_calculate_flag_v,
           args
        );
   call->Iex.CCall.cee->mcx_mask = (1<<0) | (1<<3);
   return call;
}


static IRExpr* mk_armg_calculate_flags_nzcv ( void )
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
           "armg_calculate_flags_nzcv", &armg_calculate_flags_nzcv,
           args
        );
   call->Iex.CCall.cee->mcx_mask = (1<<0) | (1<<3);
   return call;
}

static IRExpr* mk_armg_calculate_flag_qc ( IRExpr* resL, IRExpr* resR, Bool Q )
{
   IRExpr** args1;
   IRExpr** args2;
   IRExpr *call1, *call2, *res;

   if (Q) {
      args1 = mkIRExprVec_4 ( binop(Iop_GetElem32x4, resL, mkU8(0)),
                              binop(Iop_GetElem32x4, resL, mkU8(1)),
                              binop(Iop_GetElem32x4, resR, mkU8(0)),
                              binop(Iop_GetElem32x4, resR, mkU8(1)) );
      args2 = mkIRExprVec_4 ( binop(Iop_GetElem32x4, resL, mkU8(2)),
                              binop(Iop_GetElem32x4, resL, mkU8(3)),
                              binop(Iop_GetElem32x4, resR, mkU8(2)),
                              binop(Iop_GetElem32x4, resR, mkU8(3)) );
   } else {
      args1 = mkIRExprVec_4 ( binop(Iop_GetElem32x2, resL, mkU8(0)),
                              binop(Iop_GetElem32x2, resL, mkU8(1)),
                              binop(Iop_GetElem32x2, resR, mkU8(0)),
                              binop(Iop_GetElem32x2, resR, mkU8(1)) );
   }

   call1 = mkIRExprCCall(
             Ity_I32,
             0, 
             "armg_calculate_flag_qc", &armg_calculate_flag_qc,
             args1
          );
   if (Q) {
      call2 = mkIRExprCCall(
                Ity_I32,
                0, 
                "armg_calculate_flag_qc", &armg_calculate_flag_qc,
                args2
             );
   }
   if (Q) {
      res = binop(Iop_Or32, call1, call2);
   } else {
      res = call1;
   }
   return res;
}

static void setFlag_QC ( IRExpr* resL, IRExpr* resR, Bool Q,
                         IRTemp condT )
{
   putMiscReg32 (OFFB_FPSCR,
                 binop(Iop_Or32,
                       IRExpr_Get(OFFB_FPSCR, Ity_I32),
                       binop(Iop_Shl32,
                             mk_armg_calculate_flag_qc(resL, resR, Q),
                             mkU8(27))),
                 condT);
}

static
void setFlags_D1_D2_ND ( UInt cc_op, IRTemp t_dep1,
                         IRTemp t_dep2, IRTemp t_ndep,
                         IRTemp guardT  )
{
   vassert(typeOfIRTemp(irsb->tyenv, t_dep1 == Ity_I32));
   vassert(typeOfIRTemp(irsb->tyenv, t_dep2 == Ity_I32));
   vassert(typeOfIRTemp(irsb->tyenv, t_ndep == Ity_I32));
   vassert(cc_op >= ARMG_CC_OP_COPY && cc_op < ARMG_CC_OP_NUMBER);
   if (guardT == IRTemp_INVALID) {
      
      stmt( IRStmt_Put( OFFB_CC_OP,   mkU32(cc_op) ));
      stmt( IRStmt_Put( OFFB_CC_DEP1, mkexpr(t_dep1) ));
      stmt( IRStmt_Put( OFFB_CC_DEP2, mkexpr(t_dep2) ));
      stmt( IRStmt_Put( OFFB_CC_NDEP, mkexpr(t_ndep) ));
   } else {
      
      IRTemp c1 = newTemp(Ity_I1);
      assign( c1, binop(Iop_CmpNE32, mkexpr(guardT), mkU32(0)) );
      stmt( IRStmt_Put(
               OFFB_CC_OP,
               IRExpr_ITE( mkexpr(c1),
                           mkU32(cc_op),
                           IRExpr_Get(OFFB_CC_OP, Ity_I32) ) ));
      stmt( IRStmt_Put(
               OFFB_CC_DEP1,
               IRExpr_ITE( mkexpr(c1),
                           mkexpr(t_dep1),
                           IRExpr_Get(OFFB_CC_DEP1, Ity_I32) ) ));
      stmt( IRStmt_Put(
               OFFB_CC_DEP2,
               IRExpr_ITE( mkexpr(c1),
                           mkexpr(t_dep2),
                           IRExpr_Get(OFFB_CC_DEP2, Ity_I32) ) ));
      stmt( IRStmt_Put(
               OFFB_CC_NDEP,
               IRExpr_ITE( mkexpr(c1),
                           mkexpr(t_ndep),
                           IRExpr_Get(OFFB_CC_NDEP, Ity_I32) ) ));
   }
}


static void setFlags_D1_D2 ( UInt cc_op, IRTemp t_dep1,
                             IRTemp t_dep2,
                             IRTemp guardT  )
{
   IRTemp z32 = newTemp(Ity_I32);
   assign( z32, mkU32(0) );
   setFlags_D1_D2_ND( cc_op, t_dep1, t_dep2, z32, guardT );
}


static void setFlags_D1_ND ( UInt cc_op, IRTemp t_dep1,
                             IRTemp t_ndep,
                             IRTemp guardT  )
{
   IRTemp z32 = newTemp(Ity_I32);
   assign( z32, mkU32(0) );
   setFlags_D1_D2_ND( cc_op, t_dep1, z32, t_ndep, guardT );
}


static void setFlags_D1 ( UInt cc_op, IRTemp t_dep1,
                          IRTemp guardT  )
{
   IRTemp z32 = newTemp(Ity_I32);
   assign( z32, mkU32(0) );
   setFlags_D1_D2_ND( cc_op, t_dep1, z32, z32, guardT );
}


static void mk_skip_over_A32_if_cond_is_false ( 
               IRTemp guardT 
            )
{
   ASSERT_IS_ARM;
   vassert(guardT != IRTemp_INVALID);
   vassert(0 == (guest_R15_curr_instr_notENC & 3));
   stmt( IRStmt_Exit(
            unop(Iop_Not1, unop(Iop_32to1, mkexpr(guardT))),
            Ijk_Boring,
            IRConst_U32(toUInt(guest_R15_curr_instr_notENC + 4)),
            OFFB_R15T
       ));
}

static void mk_skip_over_T16_if_cond_is_false ( 
               IRTemp guardT 
            )
{
   ASSERT_IS_THUMB;
   vassert(guardT != IRTemp_INVALID);
   vassert(0 == (guest_R15_curr_instr_notENC & 1));
   stmt( IRStmt_Exit(
            unop(Iop_Not1, unop(Iop_32to1, mkexpr(guardT))),
            Ijk_Boring,
            IRConst_U32(toUInt((guest_R15_curr_instr_notENC + 2) | 1)),
            OFFB_R15T
       ));
}


static void mk_skip_over_T32_if_cond_is_false ( 
               IRTemp guardT 
            )
{
   ASSERT_IS_THUMB;
   vassert(guardT != IRTemp_INVALID);
   vassert(0 == (guest_R15_curr_instr_notENC & 1));
   stmt( IRStmt_Exit(
            unop(Iop_Not1, unop(Iop_32to1, mkexpr(guardT))),
            Ijk_Boring,
            IRConst_U32(toUInt((guest_R15_curr_instr_notENC + 4) | 1)),
            OFFB_R15T
       ));
}


static void gen_SIGILL_T_if_nonzero ( IRTemp t  )
{
   ASSERT_IS_THUMB;
   vassert(t != IRTemp_INVALID);
   vassert(0 == (guest_R15_curr_instr_notENC & 1));
   stmt(
      IRStmt_Exit(
         binop(Iop_CmpNE32, mkexpr(t), mkU32(0)),
         Ijk_NoDecode,
         IRConst_U32(toUInt(guest_R15_curr_instr_notENC | 1)),
         OFFB_R15T
      )
   );
}


static void gen_SIGILL_T_if_in_but_NLI_ITBlock (
               IRTemp old_itstate ,
               IRTemp new_itstate 
            )
{
   ASSERT_IS_THUMB;
   put_ITSTATE(old_itstate); 
   IRTemp guards_for_next3 = newTemp(Ity_I32);
   assign(guards_for_next3,
          binop(Iop_Shr32, mkexpr(old_itstate), mkU8(8)));
   gen_SIGILL_T_if_nonzero(guards_for_next3);
   put_ITSTATE(new_itstate); 
}


static void gen_SIGILL_T_if_in_ITBlock (
               IRTemp old_itstate ,
               IRTemp new_itstate 
            )
{
   put_ITSTATE(old_itstate); 
   gen_SIGILL_T_if_nonzero(old_itstate);
   put_ITSTATE(new_itstate); 
}


static IRTemp synthesise_APSR ( void )
{
   IRTemp res1 = newTemp(Ity_I32);
   
   assign( res1, mk_armg_calculate_flags_nzcv() );
   
   IRTemp res2 = newTemp(Ity_I32);
   assign(
      res2,
      binop(Iop_Or32,
            mkexpr(res1),
            binop(Iop_Shl32,
                  unop(Iop_1Uto32,
                       binop(Iop_CmpNE32,
                             mkexpr(get_QFLAG32()),
                             mkU32(0))),
                  mkU8(ARMG_CC_SHIFT_Q)))
   );
   
   IRExpr* ge0
      = unop(Iop_1Uto32, binop(Iop_CmpNE32, get_GEFLAG32(0), mkU32(0)));
   IRExpr* ge1
      = unop(Iop_1Uto32, binop(Iop_CmpNE32, get_GEFLAG32(1), mkU32(0)));
   IRExpr* ge2
      = unop(Iop_1Uto32, binop(Iop_CmpNE32, get_GEFLAG32(2), mkU32(0)));
   IRExpr* ge3
      = unop(Iop_1Uto32, binop(Iop_CmpNE32, get_GEFLAG32(3), mkU32(0)));
   IRTemp res3 = newTemp(Ity_I32);
   assign(res3,
          binop(Iop_Or32,
                mkexpr(res2),
                binop(Iop_Or32,
                      binop(Iop_Or32,
                            binop(Iop_Shl32, ge0, mkU8(16)),
                            binop(Iop_Shl32, ge1, mkU8(17))),
                      binop(Iop_Or32,
                            binop(Iop_Shl32, ge2, mkU8(18)),
                            binop(Iop_Shl32, ge3, mkU8(19))) )));
   return res3;
}


static void desynthesise_APSR ( Bool write_nzcvq, Bool write_ge,
                                IRTemp apsrT, IRTemp condT )
{
   vassert(write_nzcvq || write_ge);
   if (write_nzcvq) {
      
      IRTemp immT = newTemp(Ity_I32);
      assign(immT, binop(Iop_And32, mkexpr(apsrT), mkU32(0xF0000000)) );
      setFlags_D1(ARMG_CC_OP_COPY, immT, condT);
      
      IRTemp qnewT = newTemp(Ity_I32);
      assign(qnewT, binop(Iop_And32, mkexpr(apsrT), mkU32(ARMG_CC_MASK_Q)));
      put_QFLAG32(qnewT, condT);
   }
   if (write_ge) {
      
      put_GEFLAG32(0, 0, binop(Iop_And32, mkexpr(apsrT), mkU32(1<<16)),
                   condT);
      put_GEFLAG32(1, 0, binop(Iop_And32, mkexpr(apsrT), mkU32(1<<17)),
                   condT);
      put_GEFLAG32(2, 0, binop(Iop_And32, mkexpr(apsrT), mkU32(1<<18)),
                   condT);
      put_GEFLAG32(3, 0, binop(Iop_And32, mkexpr(apsrT), mkU32(1<<19)),
                   condT);
   }
}




static void armUnsignedSatQ( IRTemp* res,  
                             IRTemp* resQ, 
                             IRTemp regT,  
                             UInt imm5 )   
{
   UInt ceil  = (1 << imm5) - 1;    
   UInt floor = 0;

   IRTemp nd0 = newTemp(Ity_I32);
   IRTemp nd1 = newTemp(Ity_I32);
   IRTemp nd2 = newTemp(Ity_I1);
   IRTemp nd3 = newTemp(Ity_I32);
   IRTemp nd4 = newTemp(Ity_I32);
   IRTemp nd5 = newTemp(Ity_I1);
   IRTemp nd6 = newTemp(Ity_I32);

   assign( nd0, mkexpr(regT) );
   assign( nd1, mkU32(ceil) );
   assign( nd2, binop( Iop_CmpLT32S, mkexpr(nd1), mkexpr(nd0) ) );
   assign( nd3, IRExpr_ITE(mkexpr(nd2), mkexpr(nd1), mkexpr(nd0)) );
   assign( nd4, mkU32(floor) );
   assign( nd5, binop( Iop_CmpLT32S, mkexpr(nd3), mkexpr(nd4) ) );
   assign( nd6, IRExpr_ITE(mkexpr(nd5), mkexpr(nd4), mkexpr(nd3)) );
   assign( *res, mkexpr(nd6) );

   if (resQ) {
      assign( *resQ, binop(Iop_Xor32, mkexpr(*res), mkexpr(regT)) );
   }
}


static void armSignedSatQ( IRTemp regT,    
                           UInt imm5,      
                           IRTemp* res,    
                           IRTemp* resQ )  
{
   Int ceil  =  (1 << (imm5-1)) - 1;  
   Int floor = -(1 << (imm5-1));      

   IRTemp nd0 = newTemp(Ity_I32);
   IRTemp nd1 = newTemp(Ity_I32);
   IRTemp nd2 = newTemp(Ity_I1);
   IRTemp nd3 = newTemp(Ity_I32);
   IRTemp nd4 = newTemp(Ity_I32);
   IRTemp nd5 = newTemp(Ity_I1);
   IRTemp nd6 = newTemp(Ity_I32);

   assign( nd0, mkexpr(regT) );
   assign( nd1, mkU32(ceil) );
   assign( nd2, binop( Iop_CmpLT32S, mkexpr(nd1), mkexpr(nd0) ) );
   assign( nd3, IRExpr_ITE( mkexpr(nd2), mkexpr(nd1), mkexpr(nd0) ) );
   assign( nd4, mkU32(floor) );
   assign( nd5, binop( Iop_CmpLT32S, mkexpr(nd3), mkexpr(nd4) ) );
   assign( nd6, IRExpr_ITE( mkexpr(nd5), mkexpr(nd4), mkexpr(nd3) ) );
   assign( *res, mkexpr(nd6) );

   if (resQ) {
     assign( *resQ, binop(Iop_Xor32, mkexpr(*res), mkexpr(regT)) );
   }
}


static
IRExpr* signed_overflow_after_Add32 ( IRExpr* resE,
                                      IRTemp argL, IRTemp argR )
{
   IRTemp res = newTemp(Ity_I32);
   assign(res, resE);
   return
      binop( Iop_Shr32, 
             binop( Iop_And32,
                    binop( Iop_Xor32, mkexpr(res), mkexpr(argL) ),
                    binop( Iop_Xor32, mkexpr(res), mkexpr(argR) )), 
             mkU8(31) );
}

static
IRExpr* signed_overflow_after_Sub32 ( IRExpr* resE,
                                      IRTemp argL, IRTemp argR )
{
   IRTemp res = newTemp(Ity_I32);
   assign(res, resE);
   return
      binop( Iop_Shr32, 
             binop( Iop_And32,
                    binop( Iop_Xor32, mkexpr(argL), mkexpr(argR) ),
                    binop( Iop_Xor32, mkexpr(res),  mkexpr(argL) )), 
             mkU8(31) );
}




static void compute_result_and_C_after_LSL_by_imm5 (
               HChar* buf,
               IRTemp* res,
               IRTemp* newC,
               IRTemp rMt, UInt shift_amt, 
               UInt rM      
            )
{
   if (shift_amt == 0) {
      if (newC) {
         assign( *newC, mk_armg_calculate_flag_c() );
      }
      assign( *res, mkexpr(rMt) );
      DIS(buf, "r%u", rM);
   } else {
      vassert(shift_amt >= 1 && shift_amt <= 31);
      if (newC) {
         assign( *newC,
                 binop(Iop_And32,
                       binop(Iop_Shr32, mkexpr(rMt), 
                                        mkU8(32 - shift_amt)),
                       mkU32(1)));
      }
      assign( *res,
              binop(Iop_Shl32, mkexpr(rMt), mkU8(shift_amt)) );
      DIS(buf, "r%u, LSL #%u", rM, shift_amt);
   }
}


static void compute_result_and_C_after_LSL_by_reg (
               HChar* buf,
               IRTemp* res,
               IRTemp* newC,
               IRTemp rMt, IRTemp rSt,  
               UInt rM,    UInt rS      
            )
{
   
   
   
   
   
   IRTemp amtT = newTemp(Ity_I32);
   assign( amtT, binop(Iop_And32, mkexpr(rSt), mkU32(255)) );
   if (newC) {
      IRTemp oldC = newTemp(Ity_I32);
      assign(oldC, mk_armg_calculate_flag_c() );
      assign(
         *newC,
         IRExpr_ITE(
            binop(Iop_CmpEQ32, mkexpr(amtT), mkU32(0)),
            mkexpr(oldC),
            IRExpr_ITE(
               binop(Iop_CmpLE32U, mkexpr(amtT), mkU32(32)),
               binop(Iop_And32,
                     binop(Iop_Shr32,
                           mkexpr(rMt),
                           unop(Iop_32to8,
                                binop(Iop_And32,
                                      binop(Iop_Sub32,
                                            mkU32(32),
                                            mkexpr(amtT)),
                                      mkU32(31)
                                )
                           )
                     ),
                     mkU32(1)
                     ),
               mkU32(0)
            )
         )
      );
   }
   
   
   
   
   assign(
      *res,
      binop(
         Iop_And32,
         binop(Iop_Shl32,
               mkexpr(rMt),
               unop(Iop_32to8,
                    binop(Iop_And32, mkexpr(rSt), mkU32(31)))),
         binop(Iop_Sar32,
               binop(Iop_Sub32,
                     mkexpr(amtT),
                     mkU32(32)),
               mkU8(31))));
    DIS(buf, "r%u, LSL r%u", rM, rS);
}


static void compute_result_and_C_after_LSR_by_imm5 (
               HChar* buf,
               IRTemp* res,
               IRTemp* newC,
               IRTemp rMt, UInt shift_amt, 
               UInt rM      
            )
{
   if (shift_amt == 0) {
      
      
      
      if (newC) {
         assign( *newC,
                 binop(Iop_And32,
                       binop(Iop_Shr32, mkexpr(rMt), mkU8(31)), 
                       mkU32(1)));
      }
      assign( *res, mkU32(0) );
      DIS(buf, "r%u, LSR #0(a.k.a. 32)", rM);
   } else {
      
      
      
      vassert(shift_amt >= 1 && shift_amt <= 31);
      if (newC) {
         assign( *newC,
                 binop(Iop_And32,
                       binop(Iop_Shr32, mkexpr(rMt), 
                                        mkU8(shift_amt - 1)),
                       mkU32(1)));
      }
      assign( *res,
              binop(Iop_Shr32, mkexpr(rMt), mkU8(shift_amt)) );
      DIS(buf, "r%u, LSR #%u", rM, shift_amt);
   }
}


static void compute_result_and_C_after_LSR_by_reg (
               HChar* buf,
               IRTemp* res,
               IRTemp* newC,
               IRTemp rMt, IRTemp rSt,  
               UInt rM,    UInt rS      
            )
{
   
   
   
   
   
   IRTemp amtT = newTemp(Ity_I32);
   assign( amtT, binop(Iop_And32, mkexpr(rSt), mkU32(255)) );
   if (newC) {
      IRTemp oldC = newTemp(Ity_I32);
      assign(oldC, mk_armg_calculate_flag_c() );
      assign(
         *newC,
         IRExpr_ITE(
            binop(Iop_CmpEQ32, mkexpr(amtT), mkU32(0)), 
            mkexpr(oldC),
            IRExpr_ITE(
               binop(Iop_CmpLE32U, mkexpr(amtT), mkU32(32)),
               binop(Iop_And32,
                     binop(Iop_Shr32,
                           mkexpr(rMt),
                           unop(Iop_32to8,
                                binop(Iop_And32,
                                      binop(Iop_Sub32,
                                            mkexpr(amtT),
                                            mkU32(1)),
                                      mkU32(31)
                                )
                           )
                     ),
                     mkU32(1)
                     ),
               mkU32(0)
            )
         )
      );
   }
   
   
   
   
   assign(
      *res,
      binop(
         Iop_And32,
         binop(Iop_Shr32,
               mkexpr(rMt),
               unop(Iop_32to8,
                    binop(Iop_And32, mkexpr(rSt), mkU32(31)))),
         binop(Iop_Sar32,
               binop(Iop_Sub32,
                     mkexpr(amtT),
                     mkU32(32)),
               mkU8(31))));
    DIS(buf, "r%u, LSR r%u", rM, rS);
}


static void compute_result_and_C_after_ASR_by_imm5 (
               HChar* buf,
               IRTemp* res,
               IRTemp* newC,
               IRTemp rMt, UInt shift_amt, 
               UInt rM      
            )
{
   if (shift_amt == 0) {
      
      
      
      if (newC) {
         assign( *newC,
                 binop(Iop_And32,
                       binop(Iop_Shr32, mkexpr(rMt), mkU8(31)), 
                       mkU32(1)));
      }
      assign( *res, binop(Iop_Sar32, mkexpr(rMt), mkU8(31)) );
      DIS(buf, "r%u, ASR #0(a.k.a. 32)", rM);
   } else {
      
      
      
      vassert(shift_amt >= 1 && shift_amt <= 31);
      if (newC) {
         assign( *newC,
                 binop(Iop_And32,
                       binop(Iop_Shr32, mkexpr(rMt), 
                                        mkU8(shift_amt - 1)),
                       mkU32(1)));
      }
      assign( *res,
              binop(Iop_Sar32, mkexpr(rMt), mkU8(shift_amt)) );
      DIS(buf, "r%u, ASR #%u", rM, shift_amt);
   }
}


static void compute_result_and_C_after_ASR_by_reg (
               HChar* buf,
               IRTemp* res,
               IRTemp* newC,
               IRTemp rMt, IRTemp rSt,  
               UInt rM,    UInt rS      
            )
{
   
   
   
   
   
   IRTemp amtT = newTemp(Ity_I32);
   assign( amtT, binop(Iop_And32, mkexpr(rSt), mkU32(255)) );
   if (newC) {
      IRTemp oldC = newTemp(Ity_I32);
      assign(oldC, mk_armg_calculate_flag_c() );
      assign(
         *newC,
         IRExpr_ITE(
            binop(Iop_CmpEQ32, mkexpr(amtT), mkU32(0)),
            mkexpr(oldC),
            IRExpr_ITE(
               binop(Iop_CmpLE32U, mkexpr(amtT), mkU32(32)),
               binop(Iop_And32,
                     binop(Iop_Shr32,
                           mkexpr(rMt),
                           unop(Iop_32to8,
                                binop(Iop_And32,
                                      binop(Iop_Sub32,
                                            mkexpr(amtT),
                                            mkU32(1)),
                                      mkU32(31)
                                )
                           )
                     ),
                     mkU32(1)
                     ),
               binop(Iop_And32,
                     binop(Iop_Shr32,
                           mkexpr(rMt),
                           mkU8(31)
                     ),
                     mkU32(1)
               )
            )
         )
      );
   }
   
   assign(
      *res,
      binop(
         Iop_Sar32,
         mkexpr(rMt),
         unop(
            Iop_32to8,
            IRExpr_ITE(
               binop(Iop_CmpLT32U, mkexpr(amtT), mkU32(32)),
               mkexpr(amtT),
               mkU32(31)))));
    DIS(buf, "r%u, ASR r%u", rM, rS);
}


static void compute_result_and_C_after_ROR_by_reg (
               HChar* buf,
               IRTemp* res,
               IRTemp* newC,
               IRTemp rMt, IRTemp rSt,  
               UInt rM,    UInt rS      
            )
{
   
   
   
   
   IRTemp amtT = newTemp(Ity_I32);
   assign( amtT, binop(Iop_And32, mkexpr(rSt), mkU32(255)) );
   IRTemp amt5T = newTemp(Ity_I32);
   assign( amt5T, binop(Iop_And32, mkexpr(rSt), mkU32(31)) );
   IRTemp oldC = newTemp(Ity_I32);
   assign(oldC, mk_armg_calculate_flag_c() );
   if (newC) {
      assign(
         *newC,
         IRExpr_ITE(
            binop(Iop_CmpNE32, mkexpr(amtT), mkU32(0)),
            binop(Iop_And32,
                  binop(Iop_Shr32,
                        mkexpr(rMt), 
                        unop(Iop_32to8,
                             binop(Iop_And32,
                                   binop(Iop_Sub32,
                                         mkexpr(amtT), 
                                         mkU32(1)
                                   ),
                                   mkU32(31)
                             )
                        )
                  ),
                  mkU32(1)
            ),
            mkexpr(oldC)
         )
      );
   }
   assign(
      *res,
      IRExpr_ITE(
         binop(Iop_CmpNE32, mkexpr(amt5T), mkU32(0)),
         binop(Iop_Or32,
               binop(Iop_Shr32,
                     mkexpr(rMt), 
                     unop(Iop_32to8, mkexpr(amt5T))
               ),
               binop(Iop_Shl32,
                     mkexpr(rMt),
                     unop(Iop_32to8,
                          binop(Iop_Sub32, mkU32(32), mkexpr(amt5T))
                     )
               )
               ),
         mkexpr(rMt)
      )
   );
   DIS(buf, "r%u, ROR r#%u", rM, rS);
}


static void compute_result_and_C_after_shift_by_imm5 (
               HChar* buf,
               IRTemp* res,
               IRTemp* newC,
               IRTemp  rMt,       
               UInt    how,       
               UInt    shift_amt, 
               UInt    rM         
            )
{
   vassert(shift_amt < 32);
   vassert(how < 4);

   switch (how) {

      case 0:
         compute_result_and_C_after_LSL_by_imm5(
            buf, res, newC, rMt, shift_amt, rM
         );
         break;

      case 1:
         compute_result_and_C_after_LSR_by_imm5(
            buf, res, newC, rMt, shift_amt, rM
         );
         break;

      case 2:
         compute_result_and_C_after_ASR_by_imm5(
            buf, res, newC, rMt, shift_amt, rM
         );
         break;

      case 3:
         if (shift_amt == 0) {
            IRTemp oldcT = newTemp(Ity_I32);
            
            
            
            
            if (newC) {
               assign( *newC,
                       binop(Iop_And32, mkexpr(rMt), mkU32(1)));
            }
            assign( oldcT, mk_armg_calculate_flag_c() );
            assign( *res, 
                    binop(Iop_Or32,
                          binop(Iop_Shl32, mkexpr(oldcT), mkU8(31)),
                          binop(Iop_Shr32, mkexpr(rMt), mkU8(1))) );
            DIS(buf, "r%u, RRX", rM);
         } else {
            
            
            
            vassert(shift_amt >= 1 && shift_amt <= 31);
            if (newC) {
               assign( *newC,
                       binop(Iop_And32,
                             binop(Iop_Shr32, mkexpr(rMt), 
                                              mkU8(shift_amt - 1)),
                             mkU32(1)));
            }
            assign( *res,
                    binop(Iop_Or32,
                          binop(Iop_Shr32, mkexpr(rMt), mkU8(shift_amt)),
                          binop(Iop_Shl32, mkexpr(rMt),
                                           mkU8(32-shift_amt))));
            DIS(buf, "r%u, ROR #%u", rM, shift_amt);
         }
         break;

      default:
         
         vassert(0);
   }
}


static void compute_result_and_C_after_shift_by_reg (
               HChar*  buf,
               IRTemp* res,
               IRTemp* newC,
               IRTemp  rMt,       
               UInt    how,       
               IRTemp  rSt,       
               UInt    rM,        
               UInt    rS         
            )
{
   vassert(how < 4);
   switch (how) {
      case 0: { 
         compute_result_and_C_after_LSL_by_reg(
            buf, res, newC, rMt, rSt, rM, rS
         );
         break;
      }
      case 1: { 
         compute_result_and_C_after_LSR_by_reg(
            buf, res, newC, rMt, rSt, rM, rS
         );
         break;
      }
      case 2: { 
         compute_result_and_C_after_ASR_by_reg(
            buf, res, newC, rMt, rSt, rM, rS
         );
         break;
      }
      case 3: { 
         compute_result_and_C_after_ROR_by_reg(
             buf, res, newC, rMt, rSt, rM, rS
         );
         break;
      }
      default:
         
         vassert(0);
   }
}


static Bool mk_shifter_operand ( UInt insn_25, UInt insn_11_0,
                                 IRTemp* shop,
                                 IRTemp* shco,
                                 HChar* buf )
{
   UInt insn_4 = (insn_11_0 >> 4) & 1;
   UInt insn_7 = (insn_11_0 >> 7) & 1;
   vassert(insn_25 <= 0x1);
   vassert(insn_11_0 <= 0xFFF);

   vassert(shop && *shop == IRTemp_INVALID);
   *shop = newTemp(Ity_I32);

   if (shco) {
      vassert(*shco == IRTemp_INVALID);
      *shco = newTemp(Ity_I32);
   }

   

   if (insn_25 == 1) {
      
      UInt imm = (insn_11_0 >> 0) & 0xFF;
      UInt rot = 2 * ((insn_11_0 >> 8) & 0xF);
      vassert(rot <= 30);
      imm = ROR32(imm, rot);
      if (shco) {
         if (rot == 0) {
            assign( *shco, mk_armg_calculate_flag_c() );
         } else {
            assign( *shco, mkU32( (imm >> 31) & 1 ) );
         }
      }
      DIS(buf, "#0x%x", imm);
      assign( *shop, mkU32(imm) );
      return True;
   }

   

   if (insn_25 == 0 && insn_4 == 0) {
      
      UInt shift_amt = (insn_11_0 >> 7) & 0x1F;
      UInt rM        = (insn_11_0 >> 0) & 0xF;
      UInt how       = (insn_11_0 >> 5) & 3;
      
      IRTemp rMt = newTemp(Ity_I32);
      assign(rMt, getIRegA(rM));

      vassert(shift_amt <= 31);

      compute_result_and_C_after_shift_by_imm5(
         buf, shop, shco, rMt, how, shift_amt, rM
      );
      return True;
   }

   
   if (insn_25 == 0 && insn_4 == 1) {
      
      UInt rM  = (insn_11_0 >> 0) & 0xF;
      UInt rS  = (insn_11_0 >> 8) & 0xF;
      UInt how = (insn_11_0 >> 5) & 3;
      
      IRTemp rMt = newTemp(Ity_I32);
      IRTemp rSt = newTemp(Ity_I32);

      if (insn_7 == 1)
         return False; 

      assign(rMt, getIRegA(rM));
      assign(rSt, getIRegA(rS));

      compute_result_and_C_after_shift_by_reg(
         buf, shop, shco, rMt, how, rSt, rM, rS
      );
      return True;
   }

   vex_printf("mk_shifter_operand(0x%x,0x%x)\n", insn_25, insn_11_0 );
   return False;
}


static 
IRExpr* mk_EA_reg_plusminus_imm12 ( UInt rN, UInt bU, UInt imm12,
                                    HChar* buf )
{
   vassert(rN < 16);
   vassert(bU < 2);
   vassert(imm12 < 0x1000);
   HChar opChar = bU == 1 ? '+' : '-';
   DIS(buf, "[r%u, #%c%u]", rN, opChar, imm12);
   return
      binop( (bU == 1 ? Iop_Add32 : Iop_Sub32),
             getIRegA(rN),
             mkU32(imm12) );
}


static
IRExpr* mk_EA_reg_plusminus_shifted_reg ( UInt rN, UInt bU, UInt rM,
                                          UInt sh2, UInt imm5,
                                          HChar* buf )
{
   vassert(rN < 16);
   vassert(bU < 2);
   vassert(rM < 16);
   vassert(sh2 < 4);
   vassert(imm5 < 32);
   HChar   opChar = bU == 1 ? '+' : '-';
   IRExpr* index  = NULL;
   switch (sh2) {
      case 0: 
         
         index = binop(Iop_Shl32, getIRegA(rM), mkU8(imm5));
         DIS(buf, "[r%u, %c r%u LSL #%u]", rN, opChar, rM, imm5); 
         break;
      case 1: 
         if (imm5 == 0) {
            index = mkU32(0);
            vassert(0); 
         } else {
            index = binop(Iop_Shr32, getIRegA(rM), mkU8(imm5));
         }
         DIS(buf, "[r%u, %cr%u, LSR #%u]",
                  rN, opChar, rM, imm5 == 0 ? 32 : imm5); 
         break;
      case 2: 
         if (imm5 == 0) {
            index = binop(Iop_Sar32, getIRegA(rM), mkU8(31));
            vassert(0); 
         } else {
            index = binop(Iop_Sar32, getIRegA(rM), mkU8(imm5));
         }
         DIS(buf, "[r%u, %cr%u, ASR #%u]",
                  rN, opChar, rM, imm5 == 0 ? 32 : imm5); 
         break;
      case 3: 
         if (imm5 == 0) {
            IRTemp rmT    = newTemp(Ity_I32);
            IRTemp cflagT = newTemp(Ity_I32);
            assign(rmT, getIRegA(rM));
            assign(cflagT, mk_armg_calculate_flag_c());
            index = binop(Iop_Or32, 
                          binop(Iop_Shl32, mkexpr(cflagT), mkU8(31)),
                          binop(Iop_Shr32, mkexpr(rmT), mkU8(1)));
            DIS(buf, "[r%u, %cr%u, RRX]", rN, opChar, rM);
         } else {
            IRTemp rmT = newTemp(Ity_I32);
            assign(rmT, getIRegA(rM));
            vassert(imm5 >= 1 && imm5 <= 31);
            index = binop(Iop_Or32, 
                          binop(Iop_Shl32, mkexpr(rmT), mkU8(32-imm5)),
                          binop(Iop_Shr32, mkexpr(rmT), mkU8(imm5)));
            DIS(buf, "[r%u, %cr%u, ROR #%u]", rN, opChar, rM, imm5); 
         }
         break;
      default:
         vassert(0);
   }
   vassert(index);
   return binop(bU == 1 ? Iop_Add32 : Iop_Sub32,
                getIRegA(rN), index);
}


static 
IRExpr* mk_EA_reg_plusminus_imm8 ( UInt rN, UInt bU, UInt imm8,
                                   HChar* buf )
{
   vassert(rN < 16);
   vassert(bU < 2);
   vassert(imm8 < 0x100);
   HChar opChar = bU == 1 ? '+' : '-';
   DIS(buf, "[r%u, #%c%u]", rN, opChar, imm8);
   return
      binop( (bU == 1 ? Iop_Add32 : Iop_Sub32),
             getIRegA(rN),
             mkU32(imm8) );
}


static
IRExpr* mk_EA_reg_plusminus_reg ( UInt rN, UInt bU, UInt rM,
                                  HChar* buf )
{
   vassert(rN < 16);
   vassert(bU < 2);
   vassert(rM < 16);
   HChar   opChar = bU == 1 ? '+' : '-';
   IRExpr* index  = getIRegA(rM);
   DIS(buf, "[r%u, %c r%u]", rN, opChar, rM); 
   return binop(bU == 1 ? Iop_Add32 : Iop_Sub32,
                getIRegA(rN), index);
}


static
IRTemp mk_convert_IRCmpF64Result_to_NZCV ( IRTemp irRes )
{
   IRTemp ix       = newTemp(Ity_I32);
   IRTemp termL    = newTemp(Ity_I32);
   IRTemp termR    = newTemp(Ity_I32);
   IRTemp nzcv     = newTemp(Ity_I32);

   
   assign(
      ix,
      binop(Iop_Or32,
            binop(Iop_And32,
                  binop(Iop_Shr32, mkexpr(irRes), mkU8(5)),
                  mkU32(3)),
            binop(Iop_And32, mkexpr(irRes), mkU32(1))));

   assign(
      termL,
      binop(Iop_Add32,
            binop(Iop_Shr32,
                  binop(Iop_Sub32,
                        binop(Iop_Shl32,
                              binop(Iop_Xor32, mkexpr(ix), mkU32(1)),
                              mkU8(30)),
                        mkU32(1)),
                  mkU8(29)),
            mkU32(1)));

   assign(
      termR,
      binop(Iop_And32,
            binop(Iop_And32,
                  mkexpr(ix),
                  binop(Iop_Shr32, mkexpr(ix), mkU8(1))),
            mkU32(1)));

   assign(nzcv, binop(Iop_Sub32, mkexpr(termL), mkexpr(termR)));
   return nzcv;
}


/* Thumb32 only.  This is "ThumbExpandImm" in the ARM ARM.  If
   updatesC is non-NULL, a boolean is written to it indicating whether
   or not the C flag is updated, as per ARM ARM "ThumbExpandImm_C".
*/
static UInt thumbExpandImm ( Bool* updatesC,
                             UInt imm1, UInt imm3, UInt imm8 )
{
   vassert(imm1 < (1<<1));
   vassert(imm3 < (1<<3));
   vassert(imm8 < (1<<8));
   UInt i_imm3_a = (imm1 << 4) | (imm3 << 1) | ((imm8 >> 7) & 1);
   UInt abcdefgh = imm8;
   UInt lbcdefgh = imm8 | 0x80;
   if (updatesC) {
      *updatesC = i_imm3_a >= 8;
   }
   switch (i_imm3_a) {
      case 0: case 1:
         return abcdefgh;
      case 2: case 3:
         return (abcdefgh << 16) | abcdefgh;
      case 4: case 5:
         return (abcdefgh << 24) | (abcdefgh << 8);
      case 6: case 7:
         return (abcdefgh << 24) | (abcdefgh << 16)
                | (abcdefgh << 8) | abcdefgh;
      case 8 ... 31:
         return lbcdefgh << (32 - i_imm3_a);
      default:
         break;
   }
   vassert(0);
}


static UInt thumbExpandImm_from_I0_I1 ( Bool* updatesC,
                                        UShort i0s, UShort i1s )
{
   UInt i0    = (UInt)i0s;
   UInt i1    = (UInt)i1s;
   UInt imm1  = SLICE_UInt(i0,10,10);
   UInt imm3  = SLICE_UInt(i1,14,12);
   UInt imm8  = SLICE_UInt(i1,7,0);
   return thumbExpandImm(updatesC, imm1, imm3, imm8);
}


static Bool compute_ITSTATE ( UInt*  itstate,
                              HChar* ch1,
                              HChar* ch2,
                              HChar* ch3,
                              UInt firstcond, UInt mask )
{
   vassert(firstcond <= 0xF);
   vassert(mask <= 0xF);
   *itstate = 0;
   *ch1 = *ch2 = *ch3 = '.';
   if (mask == 0)
      return False; 
   if (firstcond == 0xF)
      return False; 
   if (firstcond == 0xE && popcount32(mask) != 1) 
      return False; 

   UInt m3 = (mask >> 3) & 1;
   UInt m2 = (mask >> 2) & 1;
   UInt m1 = (mask >> 1) & 1;
   UInt m0 = (mask >> 0) & 1;

   UInt fc = (firstcond << 4) | 1;
   UInt ni = (0xE << 4) | 0;

   if (m3 == 1 && (m2|m1|m0) == 0) {
      *itstate = (ni << 24) | (ni << 16) | (ni << 8) | fc;
      *itstate ^= 0xE0E0E0E0;
      return True;
   }

   if (m2 == 1 && (m1|m0) == 0) {
      *itstate = (ni << 24) | (ni << 16) | (setbit32(fc, 4, m3) << 8) | fc;
      *itstate ^= 0xE0E0E0E0;
      *ch1 = m3 == (firstcond & 1) ? 't' : 'e';
      return True;
   }

   if (m1 == 1 && m0 == 0) {
      *itstate = (ni << 24)
                 | (setbit32(fc, 4, m2) << 16)
                 | (setbit32(fc, 4, m3) << 8) | fc;
      *itstate ^= 0xE0E0E0E0;
      *ch1 = m3 == (firstcond & 1) ? 't' : 'e';
      *ch2 = m2 == (firstcond & 1) ? 't' : 'e';
      return True;
   }

   if (m0 == 1) {
      *itstate = (setbit32(fc, 4, m1) << 24)
                 | (setbit32(fc, 4, m2) << 16)
                 | (setbit32(fc, 4, m3) << 8) | fc;
      *itstate ^= 0xE0E0E0E0;
      *ch1 = m3 == (firstcond & 1) ? 't' : 'e';
      *ch2 = m2 == (firstcond & 1) ? 't' : 'e';
      *ch3 = m1 == (firstcond & 1) ? 't' : 'e';
      return True;
   }

   return False;
}


static IRTemp gen_BITREV ( IRTemp x0 )
{
   IRTemp x1 = newTemp(Ity_I32);
   IRTemp x2 = newTemp(Ity_I32);
   IRTemp x3 = newTemp(Ity_I32);
   IRTemp x4 = newTemp(Ity_I32);
   IRTemp x5 = newTemp(Ity_I32);
   UInt   c1 = 0x55555555;
   UInt   c2 = 0x33333333;
   UInt   c3 = 0x0F0F0F0F;
   UInt   c4 = 0x00FF00FF;
   UInt   c5 = 0x0000FFFF;
   assign(x1,
          binop(Iop_Or32,
                binop(Iop_Shl32,
                      binop(Iop_And32, mkexpr(x0), mkU32(c1)),
                      mkU8(1)),
                binop(Iop_Shr32,
                      binop(Iop_And32, mkexpr(x0), mkU32(~c1)),
                      mkU8(1))
   ));
   assign(x2,
          binop(Iop_Or32,
                binop(Iop_Shl32,
                      binop(Iop_And32, mkexpr(x1), mkU32(c2)),
                      mkU8(2)),
                binop(Iop_Shr32,
                      binop(Iop_And32, mkexpr(x1), mkU32(~c2)),
                      mkU8(2))
   ));
   assign(x3,
          binop(Iop_Or32,
                binop(Iop_Shl32,
                      binop(Iop_And32, mkexpr(x2), mkU32(c3)),
                      mkU8(4)),
                binop(Iop_Shr32,
                      binop(Iop_And32, mkexpr(x2), mkU32(~c3)),
                      mkU8(4))
   ));
   assign(x4,
          binop(Iop_Or32,
                binop(Iop_Shl32,
                      binop(Iop_And32, mkexpr(x3), mkU32(c4)),
                      mkU8(8)),
                binop(Iop_Shr32,
                      binop(Iop_And32, mkexpr(x3), mkU32(~c4)),
                      mkU8(8))
   ));
   assign(x5,
          binop(Iop_Or32,
                binop(Iop_Shl32,
                      binop(Iop_And32, mkexpr(x4), mkU32(c5)),
                      mkU8(16)),
                binop(Iop_Shr32,
                      binop(Iop_And32, mkexpr(x4), mkU32(~c5)),
                      mkU8(16))
   ));
   return x5;
}


static IRTemp gen_REV ( IRTemp arg )
{
   IRTemp res = newTemp(Ity_I32);
   assign(res, 
          binop(Iop_Or32,
                binop(Iop_Shl32, mkexpr(arg), mkU8(24)),
          binop(Iop_Or32,
                binop(Iop_And32, binop(Iop_Shl32, mkexpr(arg), mkU8(8)), 
                                 mkU32(0x00FF0000)),
          binop(Iop_Or32,
                binop(Iop_And32, binop(Iop_Shr32, mkexpr(arg), mkU8(8)),
                                       mkU32(0x0000FF00)),
                binop(Iop_And32, binop(Iop_Shr32, mkexpr(arg), mkU8(24)),
                                       mkU32(0x000000FF) )
   ))));
   return res;
}


static IRTemp gen_REV16 ( IRTemp arg )
{
   IRTemp res = newTemp(Ity_I32);
   assign(res,
          binop(Iop_Or32,
                binop(Iop_And32,
                      binop(Iop_Shl32, mkexpr(arg), mkU8(8)),
                      mkU32(0xFF00FF00)),
                binop(Iop_And32,
                      binop(Iop_Shr32, mkexpr(arg), mkU8(8)),
                      mkU32(0x00FF00FF))));
   return res;
}





static
UInt get_neon_d_regno(UInt theInstr)
{
   UInt x = ((theInstr >> 18) & 0x10) | ((theInstr >> 12) & 0xF);
   if (theInstr & 0x40) {
      if (x & 1) {
         x = x + 0x100;
      } else {
         x = x >> 1;
      }
   }
   return x;
}

static
UInt get_neon_n_regno(UInt theInstr)
{
   UInt x = ((theInstr >> 3) & 0x10) | ((theInstr >> 16) & 0xF);
   if (theInstr & 0x40) {
      if (x & 1) {
         x = x + 0x100;
      } else {
         x = x >> 1;
      }
   }
   return x;
}

static
UInt get_neon_m_regno(UInt theInstr)
{
   UInt x = ((theInstr >> 1) & 0x10) | (theInstr & 0xF);
   if (theInstr & 0x40) {
      if (x & 1) {
         x = x + 0x100;
      } else {
         x = x >> 1;
      }
   }
   return x;
}

static
Bool dis_neon_vext ( UInt theInstr, IRTemp condT )
{
   UInt dreg = get_neon_d_regno(theInstr);
   UInt mreg = get_neon_m_regno(theInstr);
   UInt nreg = get_neon_n_regno(theInstr);
   UInt imm4 = (theInstr >> 8) & 0xf;
   UInt Q = (theInstr >> 6) & 1;
   HChar reg_t = Q ? 'q' : 'd';

   if (Q) {
      putQReg(dreg, triop(Iop_SliceV128, getQReg(mreg),
                          getQReg(nreg), mkU8(imm4)), condT);
   } else {
      putDRegI64(dreg, triop(Iop_Slice64, getDRegI64(mreg),
                             getDRegI64(nreg), mkU8(imm4)), condT);
   }
   DIP("vext.8 %c%d, %c%d, %c%d, #%d\n", reg_t, dreg, reg_t, nreg,
                                         reg_t, mreg, imm4);
   return True;
}

static
IRExpr* binop_w_fake_RM ( IROp op, IRExpr* argL, IRExpr* argR )
{
   switch (op) {
      case Iop_Add32Fx4:
      case Iop_Sub32Fx4:
      case Iop_Mul32Fx4:
         return triop(op, get_FAKE_roundingmode(), argL, argR );
      case Iop_Add32x4: case Iop_Add16x8:
      case Iop_Sub32x4: case Iop_Sub16x8:
      case Iop_Mul32x4: case Iop_Mul16x8:
      case Iop_Mul32x2: case Iop_Mul16x4:
      case Iop_Add32Fx2:
      case Iop_Sub32Fx2:
      case Iop_Mul32Fx2:
      case Iop_PwAdd32Fx2:
         return binop(op, argL, argR);
      default:
        ppIROp(op);
        vassert(0);
   }
}

static
Bool dis_neon_vtb ( UInt theInstr, IRTemp condT )
{
   UInt op = (theInstr >> 6) & 1;
   UInt dreg = get_neon_d_regno(theInstr & ~(1 << 6));
   UInt nreg = get_neon_n_regno(theInstr & ~(1 << 6));
   UInt mreg = get_neon_m_regno(theInstr & ~(1 << 6));
   UInt len = (theInstr >> 8) & 3;
   Int i;
   IROp cmp;
   ULong imm;
   IRTemp arg_l;
   IRTemp old_mask, new_mask, cur_mask;
   IRTemp old_res, new_res;
   IRTemp old_arg, new_arg;

   if (dreg >= 0x100 || mreg >= 0x100 || nreg >= 0x100)
      return False;
   if (nreg + len > 31)
      return False;

   cmp = Iop_CmpGT8Ux8;

   old_mask = newTemp(Ity_I64);
   old_res = newTemp(Ity_I64);
   old_arg = newTemp(Ity_I64);
   assign(old_mask, mkU64(0));
   assign(old_res, mkU64(0));
   assign(old_arg, getDRegI64(mreg));
   imm = 8;
   imm = (imm <<  8) | imm;
   imm = (imm << 16) | imm;
   imm = (imm << 32) | imm;

   for (i = 0; i <= len; i++) {
      arg_l = newTemp(Ity_I64);
      new_mask = newTemp(Ity_I64);
      cur_mask = newTemp(Ity_I64);
      new_res = newTemp(Ity_I64);
      new_arg = newTemp(Ity_I64);
      assign(arg_l, getDRegI64(nreg+i));
      assign(new_arg, binop(Iop_Sub8x8, mkexpr(old_arg), mkU64(imm)));
      assign(cur_mask, binop(cmp, mkU64(imm), mkexpr(old_arg)));
      assign(new_mask, binop(Iop_Or64, mkexpr(old_mask), mkexpr(cur_mask)));
      assign(new_res, binop(Iop_Or64,
                            mkexpr(old_res),
                            binop(Iop_And64,
                                  binop(Iop_Perm8x8,
                                        mkexpr(arg_l),
                                        binop(Iop_And64,
                                              mkexpr(old_arg),
                                              mkexpr(cur_mask))),
                                  mkexpr(cur_mask))));

      old_arg = new_arg;
      old_mask = new_mask;
      old_res = new_res;
   }
   if (op) {
      new_res = newTemp(Ity_I64);
      assign(new_res, binop(Iop_Or64,
                            binop(Iop_And64,
                                  getDRegI64(dreg),
                                  unop(Iop_Not64, mkexpr(old_mask))),
                            mkexpr(old_res)));
      old_res = new_res;
   }

   putDRegI64(dreg, mkexpr(old_res), condT);
   DIP("vtb%c.8 d%u, {", op ? 'x' : 'l', dreg);
   if (len > 0) {
      DIP("d%u-d%u", nreg, nreg + len);
   } else {
      DIP("d%u", nreg);
   }
   DIP("}, d%u\n", mreg);
   return True;
}

static
Bool dis_neon_vdup ( UInt theInstr, IRTemp condT )
{
   UInt Q = (theInstr >> 6) & 1;
   UInt dreg = ((theInstr >> 18) & 0x10) | ((theInstr >> 12) & 0xF);
   UInt mreg = ((theInstr >> 1) & 0x10) | (theInstr & 0xF);
   UInt imm4 = (theInstr >> 16) & 0xF;
   UInt index;
   UInt size;
   IRTemp arg_m;
   IRTemp res;
   IROp op, op2;

   if ((imm4 == 0) || (imm4 == 8))
      return False;
   if ((Q == 1) && ((dreg & 1) == 1))
      return False;
   if (Q)
      dreg >>= 1;
   arg_m = newTemp(Ity_I64);
   assign(arg_m, getDRegI64(mreg));
   if (Q)
      res = newTemp(Ity_V128);
   else
      res = newTemp(Ity_I64);
   if ((imm4 & 1) == 1) {
      op = Q ? Iop_Dup8x16 : Iop_Dup8x8;
      op2 = Iop_GetElem8x8;
      index = imm4 >> 1;
      size = 8;
   } else if ((imm4 & 3) == 2) {
      op = Q ? Iop_Dup16x8 : Iop_Dup16x4;
      op2 = Iop_GetElem16x4;
      index = imm4 >> 2;
      size = 16;
   } else if ((imm4 & 7) == 4) {
      op = Q ? Iop_Dup32x4 : Iop_Dup32x2;
      op2 = Iop_GetElem32x2;
      index = imm4 >> 3;
      size = 32;
   } else {
      return False; 
   }
   assign(res, unop(op, binop(op2, mkexpr(arg_m), mkU8(index))));
   if (Q) {
      putQReg(dreg, mkexpr(res), condT);
   } else {
      putDRegI64(dreg, mkexpr(res), condT);
   }
   DIP("vdup.%d %c%d, d%d[%d]\n", size, Q ? 'q' : 'd', dreg, mreg, index);
   return True;
}

static
Bool dis_neon_data_3same ( UInt theInstr, IRTemp condT )
{
   UInt Q = (theInstr >> 6) & 1;
   UInt dreg = get_neon_d_regno(theInstr);
   UInt nreg = get_neon_n_regno(theInstr);
   UInt mreg = get_neon_m_regno(theInstr);
   UInt A = (theInstr >> 8) & 0xF;
   UInt B = (theInstr >> 4) & 1;
   UInt C = (theInstr >> 20) & 0x3;
   UInt U = (theInstr >> 24) & 1;
   UInt size = C;

   IRTemp arg_n;
   IRTemp arg_m;
   IRTemp res;

   if (Q) {
      arg_n = newTemp(Ity_V128);
      arg_m = newTemp(Ity_V128);
      res = newTemp(Ity_V128);
      assign(arg_n, getQReg(nreg));
      assign(arg_m, getQReg(mreg));
   } else {
      arg_n = newTemp(Ity_I64);
      arg_m = newTemp(Ity_I64);
      res = newTemp(Ity_I64);
      assign(arg_n, getDRegI64(nreg));
      assign(arg_m, getDRegI64(mreg));
   }

   switch(A) {
      case 0:
         if (B == 0) {
            
            ULong imm = 0;
            IRExpr *imm_val;
            IROp addOp;
            IROp andOp;
            IROp shOp;
            HChar regType = Q ? 'q' : 'd';

            if (size == 3)
               return False;
            switch(size) {
               case 0: imm = 0x101010101010101LL; break;
               case 1: imm = 0x1000100010001LL; break;
               case 2: imm = 0x100000001LL; break;
               default: vassert(0);
            }
            if (Q) {
               imm_val = binop(Iop_64HLtoV128, mkU64(imm), mkU64(imm));
               andOp = Iop_AndV128;
            } else {
               imm_val = mkU64(imm);
               andOp = Iop_And64;
            }
            if (U) {
               switch(size) {
                  case 0:
                     addOp = Q ? Iop_Add8x16 : Iop_Add8x8;
                     shOp = Q ? Iop_ShrN8x16 : Iop_ShrN8x8;
                     break;
                  case 1:
                     addOp = Q ? Iop_Add16x8 : Iop_Add16x4;
                     shOp = Q ? Iop_ShrN16x8 : Iop_ShrN16x4;
                     break;
                  case 2:
                     addOp = Q ? Iop_Add32x4 : Iop_Add32x2;
                     shOp = Q ? Iop_ShrN32x4 : Iop_ShrN32x2;
                     break;
                  default:
                     vassert(0);
               }
            } else {
               switch(size) {
                  case 0:
                     addOp = Q ? Iop_Add8x16 : Iop_Add8x8;
                     shOp = Q ? Iop_SarN8x16 : Iop_SarN8x8;
                     break;
                  case 1:
                     addOp = Q ? Iop_Add16x8 : Iop_Add16x4;
                     shOp = Q ? Iop_SarN16x8 : Iop_SarN16x4;
                     break;
                  case 2:
                     addOp = Q ? Iop_Add32x4 : Iop_Add32x2;
                     shOp = Q ? Iop_SarN32x4 : Iop_SarN32x2;
                     break;
                  default:
                     vassert(0);
               }
            }
            assign(res,
                   binop(addOp,
                         binop(addOp,
                               binop(shOp, mkexpr(arg_m), mkU8(1)),
                               binop(shOp, mkexpr(arg_n), mkU8(1))),
                         binop(shOp,
                               binop(addOp,
                                     binop(andOp, mkexpr(arg_m), imm_val),
                                     binop(andOp, mkexpr(arg_n), imm_val)),
                               mkU8(1))));
            DIP("vhadd.%c%d %c%d, %c%d, %c%d\n",
                U ? 'u' : 's', 8 << size, regType,
                dreg, regType, nreg, regType, mreg);
         } else {
            
            IROp op, op2;
            IRTemp tmp;
            HChar reg_t = Q ? 'q' : 'd';
            if (Q) {
               switch (size) {
                  case 0:
                     op = U ? Iop_QAdd8Ux16 : Iop_QAdd8Sx16;
                     op2 = Iop_Add8x16;
                     break;
                  case 1:
                     op = U ? Iop_QAdd16Ux8 : Iop_QAdd16Sx8;
                     op2 = Iop_Add16x8;
                     break;
                  case 2:
                     op = U ? Iop_QAdd32Ux4 : Iop_QAdd32Sx4;
                     op2 = Iop_Add32x4;
                     break;
                  case 3:
                     op = U ? Iop_QAdd64Ux2 : Iop_QAdd64Sx2;
                     op2 = Iop_Add64x2;
                     break;
                  default:
                     vassert(0);
               }
            } else {
               switch (size) {
                  case 0:
                     op = U ? Iop_QAdd8Ux8 : Iop_QAdd8Sx8;
                     op2 = Iop_Add8x8;
                     break;
                  case 1:
                     op = U ? Iop_QAdd16Ux4 : Iop_QAdd16Sx4;
                     op2 = Iop_Add16x4;
                     break;
                  case 2:
                     op = U ? Iop_QAdd32Ux2 : Iop_QAdd32Sx2;
                     op2 = Iop_Add32x2;
                     break;
                  case 3:
                     op = U ? Iop_QAdd64Ux1 : Iop_QAdd64Sx1;
                     op2 = Iop_Add64;
                     break;
                  default:
                     vassert(0);
               }
            }
            if (Q) {
               tmp = newTemp(Ity_V128);
            } else {
               tmp = newTemp(Ity_I64);
            }
            assign(res, binop(op, mkexpr(arg_n), mkexpr(arg_m)));
            assign(tmp, binop(op2, mkexpr(arg_n), mkexpr(arg_m)));
            setFlag_QC(mkexpr(res), mkexpr(tmp), Q, condT);
            DIP("vqadd.%c%d %c%d, %c%d, %c%d\n",
                U ? 'u' : 's',
                8 << size, reg_t, dreg, reg_t, nreg, reg_t, mreg);
         }
         break;
      case 1:
         if (B == 0) {
            
            IROp shift_op, add_op;
            IRTemp cc;
            ULong one = 1;
            HChar reg_t = Q ? 'q' : 'd';
            switch (size) {
               case 0: one = (one <<  8) | one; 
               case 1: one = (one << 16) | one; 
               case 2: one = (one << 32) | one; break;
               case 3: return False;
               default: vassert(0);
            }
            if (Q) {
               switch (size) {
                  case 0:
                     shift_op = U ? Iop_ShrN8x16 : Iop_SarN8x16;
                     add_op = Iop_Add8x16;
                     break;
                  case 1:
                     shift_op = U ? Iop_ShrN16x8 : Iop_SarN16x8;
                     add_op = Iop_Add16x8;
                     break;
                  case 2:
                     shift_op = U ? Iop_ShrN32x4 : Iop_SarN32x4;
                     add_op = Iop_Add32x4;
                     break;
                  case 3:
                     return False;
                  default:
                     vassert(0);
               }
            } else {
               switch (size) {
                  case 0:
                     shift_op = U ? Iop_ShrN8x8 : Iop_SarN8x8;
                     add_op = Iop_Add8x8;
                     break;
                  case 1:
                     shift_op = U ? Iop_ShrN16x4 : Iop_SarN16x4;
                     add_op = Iop_Add16x4;
                     break;
                  case 2:
                     shift_op = U ? Iop_ShrN32x2 : Iop_SarN32x2;
                     add_op = Iop_Add32x2;
                     break;
                  case 3:
                     return False;
                  default:
                     vassert(0);
               }
            }
            if (Q) {
               cc = newTemp(Ity_V128);
               assign(cc, binop(shift_op,
                                binop(add_op,
                                      binop(add_op,
                                            binop(Iop_AndV128,
                                                  mkexpr(arg_n),
                                                  binop(Iop_64HLtoV128,
                                                        mkU64(one),
                                                        mkU64(one))),
                                            binop(Iop_AndV128,
                                                  mkexpr(arg_m),
                                                  binop(Iop_64HLtoV128,
                                                        mkU64(one),
                                                        mkU64(one)))),
                                      binop(Iop_64HLtoV128,
                                            mkU64(one),
                                            mkU64(one))),
                                mkU8(1)));
               assign(res, binop(add_op,
                                 binop(add_op,
                                       binop(shift_op,
                                             mkexpr(arg_n),
                                             mkU8(1)),
                                       binop(shift_op,
                                             mkexpr(arg_m),
                                             mkU8(1))),
                                 mkexpr(cc)));
            } else {
               cc = newTemp(Ity_I64);
               assign(cc, binop(shift_op,
                                binop(add_op,
                                      binop(add_op,
                                            binop(Iop_And64,
                                                  mkexpr(arg_n),
                                                  mkU64(one)),
                                            binop(Iop_And64,
                                                  mkexpr(arg_m),
                                                  mkU64(one))),
                                      mkU64(one)),
                                mkU8(1)));
               assign(res, binop(add_op,
                                 binop(add_op,
                                       binop(shift_op,
                                             mkexpr(arg_n),
                                             mkU8(1)),
                                       binop(shift_op,
                                             mkexpr(arg_m),
                                             mkU8(1))),
                                 mkexpr(cc)));
            }
            DIP("vrhadd.%c%d %c%d, %c%d, %c%d\n",
                U ? 'u' : 's',
                8 << size, reg_t, dreg, reg_t, nreg, reg_t, mreg);
         } else {
            if (U == 0)  {
               switch(C) {
                  case 0: {
                     
                     HChar reg_t = Q ? 'q' : 'd';
                     if (Q) {
                        assign(res, binop(Iop_AndV128, mkexpr(arg_n), 
                                                       mkexpr(arg_m)));
                     } else {
                        assign(res, binop(Iop_And64, mkexpr(arg_n),
                                                     mkexpr(arg_m)));
                     }
                     DIP("vand %c%d, %c%d, %c%d\n",
                         reg_t, dreg, reg_t, nreg, reg_t, mreg);
                     break;
                  }
                  case 1: {
                     
                     HChar reg_t = Q ? 'q' : 'd';
                     if (Q) {
                        assign(res, binop(Iop_AndV128,mkexpr(arg_n),
                               unop(Iop_NotV128, mkexpr(arg_m))));
                     } else {
                        assign(res, binop(Iop_And64, mkexpr(arg_n),
                               unop(Iop_Not64, mkexpr(arg_m))));
                     }
                     DIP("vbic %c%d, %c%d, %c%d\n",
                         reg_t, dreg, reg_t, nreg, reg_t, mreg);
                     break;
                  }
                  case 2:
                     if ( nreg != mreg) {
                        
                        HChar reg_t = Q ? 'q' : 'd';
                        if (Q) {
                           assign(res, binop(Iop_OrV128, mkexpr(arg_n),
                                                         mkexpr(arg_m)));
                        } else {
                           assign(res, binop(Iop_Or64, mkexpr(arg_n),
                                                       mkexpr(arg_m)));
                        }
                        DIP("vorr %c%d, %c%d, %c%d\n",
                            reg_t, dreg, reg_t, nreg, reg_t, mreg);
                     } else {
                        
                        HChar reg_t = Q ? 'q' : 'd';
                        assign(res, mkexpr(arg_m));
                        DIP("vmov %c%d, %c%d\n", reg_t, dreg, reg_t, mreg);
                     }
                     break;
                  case 3:{
                     
                     HChar reg_t = Q ? 'q' : 'd';
                     if (Q) {
                        assign(res, binop(Iop_OrV128,mkexpr(arg_n),
                               unop(Iop_NotV128, mkexpr(arg_m))));
                     } else {
                        assign(res, binop(Iop_Or64, mkexpr(arg_n),
                               unop(Iop_Not64, mkexpr(arg_m))));
                     }
                     DIP("vorn %c%d, %c%d, %c%d\n",
                         reg_t, dreg, reg_t, nreg, reg_t, mreg);
                     break;
                  }
               }
            } else {
               switch(C) {
                  case 0:
                     
                     if (Q) {
                        assign(res, binop(Iop_XorV128, mkexpr(arg_n),
                                                       mkexpr(arg_m)));
                     } else {
                        assign(res, binop(Iop_Xor64, mkexpr(arg_n),
                                                     mkexpr(arg_m)));
                     }
                     DIP("veor %c%u, %c%u, %c%u\n", Q ? 'q' : 'd', dreg,
                           Q ? 'q' : 'd', nreg, Q ? 'q' : 'd', mreg);
                     break;
                  case 1:
                     
                     if (Q) {
                        IRTemp reg_d = newTemp(Ity_V128);
                        assign(reg_d, getQReg(dreg));
                        assign(res,
                               binop(Iop_OrV128,
                                     binop(Iop_AndV128, mkexpr(arg_n),
                                                        mkexpr(reg_d)),
                                     binop(Iop_AndV128,
                                           mkexpr(arg_m),
                                           unop(Iop_NotV128,
                                                 mkexpr(reg_d)) ) ) );
                     } else {
                        IRTemp reg_d = newTemp(Ity_I64);
                        assign(reg_d, getDRegI64(dreg));
                        assign(res,
                               binop(Iop_Or64,
                                     binop(Iop_And64, mkexpr(arg_n),
                                                      mkexpr(reg_d)),
                                     binop(Iop_And64,
                                           mkexpr(arg_m),
                                           unop(Iop_Not64, mkexpr(reg_d)))));
                     }
                     DIP("vbsl %c%u, %c%u, %c%u\n",
                         Q ? 'q' : 'd', dreg,
                         Q ? 'q' : 'd', nreg, Q ? 'q' : 'd', mreg);
                     break;
                  case 2:
                     
                     if (Q) {
                        IRTemp reg_d = newTemp(Ity_V128);
                        assign(reg_d, getQReg(dreg));
                        assign(res,
                               binop(Iop_OrV128,
                                     binop(Iop_AndV128, mkexpr(arg_n), 
                                                        mkexpr(arg_m)),
                                     binop(Iop_AndV128,
                                           mkexpr(reg_d),
                                           unop(Iop_NotV128, mkexpr(arg_m)))));
                     } else {
                        IRTemp reg_d = newTemp(Ity_I64);
                        assign(reg_d, getDRegI64(dreg));
                        assign(res,
                               binop(Iop_Or64,
                                     binop(Iop_And64, mkexpr(arg_n),
                                                      mkexpr(arg_m)),
                                     binop(Iop_And64,
                                           mkexpr(reg_d),
                                           unop(Iop_Not64, mkexpr(arg_m)))));
                     }
                     DIP("vbit %c%u, %c%u, %c%u\n",
                         Q ? 'q' : 'd', dreg,
                         Q ? 'q' : 'd', nreg, Q ? 'q' : 'd', mreg);
                     break;
                  case 3:
                     
                     if (Q) {
                        IRTemp reg_d = newTemp(Ity_V128);
                        assign(reg_d, getQReg(dreg));
                        assign(res,
                               binop(Iop_OrV128,
                                     binop(Iop_AndV128, mkexpr(reg_d),
                                                        mkexpr(arg_m)),
                                     binop(Iop_AndV128,
                                           mkexpr(arg_n),
                                           unop(Iop_NotV128, mkexpr(arg_m)))));
                     } else {
                        IRTemp reg_d = newTemp(Ity_I64);
                        assign(reg_d, getDRegI64(dreg));
                        assign(res,
                               binop(Iop_Or64,
                                     binop(Iop_And64, mkexpr(reg_d),
                                                      mkexpr(arg_m)),
                                     binop(Iop_And64,
                                           mkexpr(arg_n),
                                           unop(Iop_Not64, mkexpr(arg_m)))));
                     }
                     DIP("vbif %c%u, %c%u, %c%u\n",
                         Q ? 'q' : 'd', dreg,
                         Q ? 'q' : 'd', nreg, Q ? 'q' : 'd', mreg);
                     break;
               }
            }
         }
         break;
      case 2:
         if (B == 0) {
            
            
            ULong imm = 0;
            IRExpr *imm_val;
            IROp subOp;
            IROp notOp;
            IROp andOp;
            IROp shOp;
            if (size == 3)
               return False;
            switch(size) {
               case 0: imm = 0x101010101010101LL; break;
               case 1: imm = 0x1000100010001LL; break;
               case 2: imm = 0x100000001LL; break;
               default: vassert(0);
            }
            if (Q) {
               imm_val = binop(Iop_64HLtoV128, mkU64(imm), mkU64(imm));
               andOp = Iop_AndV128;
               notOp = Iop_NotV128;
            } else {
               imm_val = mkU64(imm);
               andOp = Iop_And64;
               notOp = Iop_Not64;
            }
            if (U) {
               switch(size) {
                  case 0:
                     subOp = Q ? Iop_Sub8x16 : Iop_Sub8x8;
                     shOp = Q ? Iop_ShrN8x16 : Iop_ShrN8x8;
                     break;
                  case 1:
                     subOp = Q ? Iop_Sub16x8 : Iop_Sub16x4;
                     shOp = Q ? Iop_ShrN16x8 : Iop_ShrN16x4;
                     break;
                  case 2:
                     subOp = Q ? Iop_Sub32x4 : Iop_Sub32x2;
                     shOp = Q ? Iop_ShrN32x4 : Iop_ShrN32x2;
                     break;
                  default:
                     vassert(0);
               }
            } else {
               switch(size) {
                  case 0:
                     subOp = Q ? Iop_Sub8x16 : Iop_Sub8x8;
                     shOp = Q ? Iop_SarN8x16 : Iop_SarN8x8;
                     break;
                  case 1:
                     subOp = Q ? Iop_Sub16x8 : Iop_Sub16x4;
                     shOp = Q ? Iop_SarN16x8 : Iop_SarN16x4;
                     break;
                  case 2:
                     subOp = Q ? Iop_Sub32x4 : Iop_Sub32x2;
                     shOp = Q ? Iop_SarN32x4 : Iop_SarN32x2;
                     break;
                  default:
                     vassert(0);
               }
            }
            assign(res,
                   binop(subOp,
                         binop(subOp,
                               binop(shOp, mkexpr(arg_n), mkU8(1)),
                               binop(shOp, mkexpr(arg_m), mkU8(1))),
                         binop(andOp,
                               binop(andOp,
                                     unop(notOp, mkexpr(arg_n)),
                                     mkexpr(arg_m)),
                               imm_val)));
            DIP("vhsub.%c%u %c%u, %c%u, %c%u\n",
                U ? 'u' : 's', 8 << size,
                Q ? 'q' : 'd', dreg, Q ? 'q' : 'd', nreg, Q ? 'q' : 'd',
                mreg);
         } else {
            
            IROp op, op2;
            IRTemp tmp;
            if (Q) {
               switch (size) {
                  case 0:
                     op = U ? Iop_QSub8Ux16 : Iop_QSub8Sx16;
                     op2 = Iop_Sub8x16;
                     break;
                  case 1:
                     op = U ? Iop_QSub16Ux8 : Iop_QSub16Sx8;
                     op2 = Iop_Sub16x8;
                     break;
                  case 2:
                     op = U ? Iop_QSub32Ux4 : Iop_QSub32Sx4;
                     op2 = Iop_Sub32x4;
                     break;
                  case 3:
                     op = U ? Iop_QSub64Ux2 : Iop_QSub64Sx2;
                     op2 = Iop_Sub64x2;
                     break;
                  default:
                     vassert(0);
               }
            } else {
               switch (size) {
                  case 0:
                     op = U ? Iop_QSub8Ux8 : Iop_QSub8Sx8;
                     op2 = Iop_Sub8x8;
                     break;
                  case 1:
                     op = U ? Iop_QSub16Ux4 : Iop_QSub16Sx4;
                     op2 = Iop_Sub16x4;
                     break;
                  case 2:
                     op = U ? Iop_QSub32Ux2 : Iop_QSub32Sx2;
                     op2 = Iop_Sub32x2;
                     break;
                  case 3:
                     op = U ? Iop_QSub64Ux1 : Iop_QSub64Sx1;
                     op2 = Iop_Sub64;
                     break;
                  default:
                     vassert(0);
               }
            }
            if (Q)
               tmp = newTemp(Ity_V128);
            else
               tmp = newTemp(Ity_I64);
            assign(res, binop(op, mkexpr(arg_n), mkexpr(arg_m)));
            assign(tmp, binop(op2, mkexpr(arg_n), mkexpr(arg_m)));
            setFlag_QC(mkexpr(res), mkexpr(tmp), Q, condT);
            DIP("vqsub.%c%u %c%u, %c%u, %c%u\n",
                U ? 'u' : 's', 8 << size,
                Q ? 'q' : 'd', dreg, Q ? 'q' : 'd', nreg, Q ? 'q' : 'd',
                mreg);
         }
         break;
      case 3: {
            IROp op;
            if (Q) {
               switch (size) {
                  case 0: op = U ? Iop_CmpGT8Ux16 : Iop_CmpGT8Sx16; break;
                  case 1: op = U ? Iop_CmpGT16Ux8 : Iop_CmpGT16Sx8; break;
                  case 2: op = U ? Iop_CmpGT32Ux4 : Iop_CmpGT32Sx4; break;
                  case 3: return False;
                  default: vassert(0);
               }
            } else {
               switch (size) {
                  case 0: op = U ? Iop_CmpGT8Ux8 : Iop_CmpGT8Sx8; break;
                  case 1: op = U ? Iop_CmpGT16Ux4 : Iop_CmpGT16Sx4; break;
                  case 2: op = U ? Iop_CmpGT32Ux2: Iop_CmpGT32Sx2; break;
                  case 3: return False;
                  default: vassert(0);
               }
            }
            if (B == 0) {
               
               assign(res, binop(op, mkexpr(arg_n), mkexpr(arg_m)));
               DIP("vcgt.%c%u %c%u, %c%u, %c%u\n",
                   U ? 'u' : 's', 8 << size,
                   Q ? 'q' : 'd', dreg, Q ? 'q' : 'd', nreg, Q ? 'q' : 'd',
                   mreg);
            } else {
               
               assign(res,
                      unop(Q ? Iop_NotV128 : Iop_Not64,
                           binop(op, mkexpr(arg_m), mkexpr(arg_n))));
               DIP("vcge.%c%u %c%u, %c%u, %c%u\n",
                   U ? 'u' : 's', 8 << size,
                   Q ? 'q' : 'd', dreg, Q ? 'q' : 'd', nreg, Q ? 'q' : 'd',
                   mreg);
            }
         }
         break;
      case 4:
         if (B == 0) {
            
            IROp op = Iop_INVALID, sub_op = Iop_INVALID;
            IRTemp tmp = IRTemp_INVALID;
            if (U) {
               switch (size) {
                  case 0: op = Q ? Iop_Shl8x16 : Iop_Shl8x8; break;
                  case 1: op = Q ? Iop_Shl16x8 : Iop_Shl16x4; break;
                  case 2: op = Q ? Iop_Shl32x4 : Iop_Shl32x2; break;
                  case 3: op = Q ? Iop_Shl64x2 : Iop_Shl64; break;
                  default: vassert(0);
               }
            } else {
               tmp = newTemp(Q ? Ity_V128 : Ity_I64);
               switch (size) {
                  case 0:
                     op = Q ? Iop_Sar8x16 : Iop_Sar8x8;
                     sub_op = Q ? Iop_Sub8x16 : Iop_Sub8x8;
                     break;
                  case 1:
                     op = Q ? Iop_Sar16x8 : Iop_Sar16x4;
                     sub_op = Q ? Iop_Sub16x8 : Iop_Sub16x4;
                     break;
                  case 2:
                     op = Q ? Iop_Sar32x4 : Iop_Sar32x2;
                     sub_op = Q ? Iop_Sub32x4 : Iop_Sub32x2;
                     break;
                  case 3:
                     op = Q ? Iop_Sar64x2 : Iop_Sar64;
                     sub_op = Q ? Iop_Sub64x2 : Iop_Sub64;
                     break;
                  default:
                     vassert(0);
               }
            }
            if (U) {
               if (!Q && (size == 3))
                  assign(res, binop(op, mkexpr(arg_m),
                                        unop(Iop_64to8, mkexpr(arg_n))));
               else
                  assign(res, binop(op, mkexpr(arg_m), mkexpr(arg_n)));
            } else {
               if (Q)
                  assign(tmp, binop(sub_op,
                                    binop(Iop_64HLtoV128, mkU64(0), mkU64(0)),
                                    mkexpr(arg_n)));
               else
                  assign(tmp, binop(sub_op, mkU64(0), mkexpr(arg_n)));
               if (!Q && (size == 3))
                  assign(res, binop(op, mkexpr(arg_m),
                                        unop(Iop_64to8, mkexpr(tmp))));
               else
                  assign(res, binop(op, mkexpr(arg_m), mkexpr(tmp)));
            }
            DIP("vshl.%c%u %c%u, %c%u, %c%u\n",
                U ? 'u' : 's', 8 << size,
                Q ? 'q' : 'd', dreg, Q ? 'q' : 'd', mreg, Q ? 'q' : 'd',
                nreg);
         } else {
            
            IROp op, op_rev, op_shrn, op_shln, cmp_neq, cmp_gt;
            IRTemp tmp, shval, mask, old_shval;
            UInt i;
            ULong esize;
            cmp_neq = Q ? Iop_CmpNEZ8x16 : Iop_CmpNEZ8x8;
            cmp_gt = Q ? Iop_CmpGT8Sx16 : Iop_CmpGT8Sx8;
            if (U) {
               switch (size) {
                  case 0:
                     op = Q ? Iop_QShl8x16 : Iop_QShl8x8;
                     op_rev = Q ? Iop_Shr8x16 : Iop_Shr8x8;
                     op_shrn = Q ? Iop_ShrN8x16 : Iop_ShrN8x8;
                     op_shln = Q ? Iop_ShlN8x16 : Iop_ShlN8x8;
                     break;
                  case 1:
                     op = Q ? Iop_QShl16x8 : Iop_QShl16x4;
                     op_rev = Q ? Iop_Shr16x8 : Iop_Shr16x4;
                     op_shrn = Q ? Iop_ShrN16x8 : Iop_ShrN16x4;
                     op_shln = Q ? Iop_ShlN16x8 : Iop_ShlN16x4;
                     break;
                  case 2:
                     op = Q ? Iop_QShl32x4 : Iop_QShl32x2;
                     op_rev = Q ? Iop_Shr32x4 : Iop_Shr32x2;
                     op_shrn = Q ? Iop_ShrN32x4 : Iop_ShrN32x2;
                     op_shln = Q ? Iop_ShlN32x4 : Iop_ShlN32x2;
                     break;
                  case 3:
                     op = Q ? Iop_QShl64x2 : Iop_QShl64x1;
                     op_rev = Q ? Iop_Shr64x2 : Iop_Shr64;
                     op_shrn = Q ? Iop_ShrN64x2 : Iop_Shr64;
                     op_shln = Q ? Iop_ShlN64x2 : Iop_Shl64;
                     break;
                  default:
                     vassert(0);
               }
            } else {
               switch (size) {
                  case 0:
                     op = Q ? Iop_QSal8x16 : Iop_QSal8x8;
                     op_rev = Q ? Iop_Sar8x16 : Iop_Sar8x8;
                     op_shrn = Q ? Iop_ShrN8x16 : Iop_ShrN8x8;
                     op_shln = Q ? Iop_ShlN8x16 : Iop_ShlN8x8;
                     break;
                  case 1:
                     op = Q ? Iop_QSal16x8 : Iop_QSal16x4;
                     op_rev = Q ? Iop_Sar16x8 : Iop_Sar16x4;
                     op_shrn = Q ? Iop_ShrN16x8 : Iop_ShrN16x4;
                     op_shln = Q ? Iop_ShlN16x8 : Iop_ShlN16x4;
                     break;
                  case 2:
                     op = Q ? Iop_QSal32x4 : Iop_QSal32x2;
                     op_rev = Q ? Iop_Sar32x4 : Iop_Sar32x2;
                     op_shrn = Q ? Iop_ShrN32x4 : Iop_ShrN32x2;
                     op_shln = Q ? Iop_ShlN32x4 : Iop_ShlN32x2;
                     break;
                  case 3:
                     op = Q ? Iop_QSal64x2 : Iop_QSal64x1;
                     op_rev = Q ? Iop_Sar64x2 : Iop_Sar64;
                     op_shrn = Q ? Iop_ShrN64x2 : Iop_Shr64;
                     op_shln = Q ? Iop_ShlN64x2 : Iop_Shl64;
                     break;
                  default:
                     vassert(0);
               }
            }
            if (Q) {
               tmp = newTemp(Ity_V128);
               shval = newTemp(Ity_V128);
               mask = newTemp(Ity_V128);
            } else {
               tmp = newTemp(Ity_I64);
               shval = newTemp(Ity_I64);
               mask = newTemp(Ity_I64);
            }
            assign(res, binop(op, mkexpr(arg_m), mkexpr(arg_n)));
            assign(shval, binop(op_shrn,
                                binop(op_shln,
                                       mkexpr(arg_n),
                                       mkU8((8 << size) - 8)),
                                mkU8((8 << size) - 8)));
            for(i = 0; i < size; i++) {
               old_shval = shval;
               shval = newTemp(Q ? Ity_V128 : Ity_I64);
               assign(shval, binop(Q ? Iop_OrV128 : Iop_Or64,
                                   mkexpr(old_shval),
                                   binop(op_shln,
                                         mkexpr(old_shval),
                                         mkU8(8 << i))));
            }
            esize = (8 << size) - 1;
            esize = (esize <<  8) | esize;
            esize = (esize << 16) | esize;
            esize = (esize << 32) | esize;
            setFlag_QC(binop(Q ? Iop_AndV128 : Iop_And64,
                             binop(cmp_gt, mkexpr(shval),
                                           Q ? mkU128(esize) : mkU64(esize)),
                             unop(cmp_neq, mkexpr(arg_m))),
                       Q ? mkU128(0) : mkU64(0),
                       Q, condT);
            assign(mask, binop(cmp_gt, mkexpr(shval),
                                       Q ? mkU128(0) : mkU64(0)));
            if (!Q && size == 3)
               assign(tmp, binop(op_rev, mkexpr(res),
                                         unop(Iop_64to8, mkexpr(arg_n))));
            else
               assign(tmp, binop(op_rev, mkexpr(res), mkexpr(arg_n)));
            setFlag_QC(binop(Q ? Iop_AndV128 : Iop_And64,
                             mkexpr(tmp), mkexpr(mask)),
                       binop(Q ? Iop_AndV128 : Iop_And64,
                             mkexpr(arg_m), mkexpr(mask)),
                       Q, condT);
            DIP("vqshl.%c%u %c%u, %c%u, %c%u\n",
                U ? 'u' : 's', 8 << size,
                Q ? 'q' : 'd', dreg, Q ? 'q' : 'd', mreg, Q ? 'q' : 'd',
                nreg);
         }
         break;
      case 5:
         if (B == 0) {
            
            IROp op, op_shrn, op_shln, cmp_gt, op_add;
            IRTemp shval, old_shval, imm_val, round;
            UInt i;
            ULong imm;
            cmp_gt = Q ? Iop_CmpGT8Sx16 : Iop_CmpGT8Sx8;
            imm = 1L;
            switch (size) {
               case 0: imm = (imm <<  8) | imm; 
               case 1: imm = (imm << 16) | imm; 
               case 2: imm = (imm << 32) | imm; 
               case 3: break;
               default: vassert(0);
            }
            imm_val = newTemp(Q ? Ity_V128 : Ity_I64);
            round = newTemp(Q ? Ity_V128 : Ity_I64);
            assign(imm_val, Q ? mkU128(imm) : mkU64(imm));
            if (U) {
               switch (size) {
                  case 0:
                     op = Q ? Iop_Shl8x16 : Iop_Shl8x8;
                     op_add = Q ? Iop_Add8x16 : Iop_Add8x8;
                     op_shrn = Q ? Iop_ShrN8x16 : Iop_ShrN8x8;
                     op_shln = Q ? Iop_ShlN8x16 : Iop_ShlN8x8;
                     break;
                  case 1:
                     op = Q ? Iop_Shl16x8 : Iop_Shl16x4;
                     op_add = Q ? Iop_Add16x8 : Iop_Add16x4;
                     op_shrn = Q ? Iop_ShrN16x8 : Iop_ShrN16x4;
                     op_shln = Q ? Iop_ShlN16x8 : Iop_ShlN16x4;
                     break;
                  case 2:
                     op = Q ? Iop_Shl32x4 : Iop_Shl32x2;
                     op_add = Q ? Iop_Add32x4 : Iop_Add32x2;
                     op_shrn = Q ? Iop_ShrN32x4 : Iop_ShrN32x2;
                     op_shln = Q ? Iop_ShlN32x4 : Iop_ShlN32x2;
                     break;
                  case 3:
                     op = Q ? Iop_Shl64x2 : Iop_Shl64;
                     op_add = Q ? Iop_Add64x2 : Iop_Add64;
                     op_shrn = Q ? Iop_ShrN64x2 : Iop_Shr64;
                     op_shln = Q ? Iop_ShlN64x2 : Iop_Shl64;
                     break;
                  default:
                     vassert(0);
               }
            } else {
               switch (size) {
                  case 0:
                     op = Q ? Iop_Sal8x16 : Iop_Sal8x8;
                     op_add = Q ? Iop_Add8x16 : Iop_Add8x8;
                     op_shrn = Q ? Iop_ShrN8x16 : Iop_ShrN8x8;
                     op_shln = Q ? Iop_ShlN8x16 : Iop_ShlN8x8;
                     break;
                  case 1:
                     op = Q ? Iop_Sal16x8 : Iop_Sal16x4;
                     op_add = Q ? Iop_Add16x8 : Iop_Add16x4;
                     op_shrn = Q ? Iop_ShrN16x8 : Iop_ShrN16x4;
                     op_shln = Q ? Iop_ShlN16x8 : Iop_ShlN16x4;
                     break;
                  case 2:
                     op = Q ? Iop_Sal32x4 : Iop_Sal32x2;
                     op_add = Q ? Iop_Add32x4 : Iop_Add32x2;
                     op_shrn = Q ? Iop_ShrN32x4 : Iop_ShrN32x2;
                     op_shln = Q ? Iop_ShlN32x4 : Iop_ShlN32x2;
                     break;
                  case 3:
                     op = Q ? Iop_Sal64x2 : Iop_Sal64x1;
                     op_add = Q ? Iop_Add64x2 : Iop_Add64;
                     op_shrn = Q ? Iop_ShrN64x2 : Iop_Shr64;
                     op_shln = Q ? Iop_ShlN64x2 : Iop_Shl64;
                     break;
                  default:
                     vassert(0);
               }
            }
            if (Q) {
               shval = newTemp(Ity_V128);
            } else {
               shval = newTemp(Ity_I64);
            }
            assign(shval, binop(op_shrn,
                                binop(op_shln,
                                       mkexpr(arg_n),
                                       mkU8((8 << size) - 8)),
                                mkU8((8 << size) - 8)));
            for (i = 0; i < size; i++) {
               old_shval = shval;
               shval = newTemp(Q ? Ity_V128 : Ity_I64);
               assign(shval, binop(Q ? Iop_OrV128 : Iop_Or64,
                                   mkexpr(old_shval),
                                   binop(op_shln,
                                         mkexpr(old_shval),
                                         mkU8(8 << i))));
            }
            
            if (!Q && size == 3 && U) {
               assign(round, binop(Q ? Iop_AndV128 : Iop_And64,
                                   binop(op,
                                         mkexpr(arg_m),
                                         unop(Iop_64to8,
                                              binop(op_add,
                                                    mkexpr(arg_n),
                                                    mkexpr(imm_val)))),
                                   binop(Q ? Iop_AndV128 : Iop_And64,
                                         mkexpr(imm_val),
                                         binop(cmp_gt,
                                               Q ? mkU128(0) : mkU64(0),
                                               mkexpr(arg_n)))));
               assign(res, binop(op_add,
                                 binop(op,
                                       mkexpr(arg_m),
                                       unop(Iop_64to8, mkexpr(arg_n))),
                                 mkexpr(round)));
            } else {
               assign(round, binop(Q ? Iop_AndV128 : Iop_And64,
                                   binop(op,
                                         mkexpr(arg_m),
                                         binop(op_add,
                                               mkexpr(arg_n),
                                               mkexpr(imm_val))),
                                   binop(Q ? Iop_AndV128 : Iop_And64,
                                         mkexpr(imm_val),
                                         binop(cmp_gt,
                                               Q ? mkU128(0) : mkU64(0),
                                               mkexpr(arg_n)))));
               assign(res, binop(op_add,
                                 binop(op, mkexpr(arg_m), mkexpr(arg_n)),
                                 mkexpr(round)));
            }
            DIP("vrshl.%c%u %c%u, %c%u, %c%u\n",
                U ? 'u' : 's', 8 << size,
                Q ? 'q' : 'd', dreg, Q ? 'q' : 'd', mreg, Q ? 'q' : 'd',
                nreg);
         } else {
            
            IROp op, op_rev, op_shrn, op_shln, cmp_neq, cmp_gt, op_add;
            IRTemp tmp, shval, mask, old_shval, imm_val, round;
            UInt i;
            ULong esize, imm;
            cmp_neq = Q ? Iop_CmpNEZ8x16 : Iop_CmpNEZ8x8;
            cmp_gt = Q ? Iop_CmpGT8Sx16 : Iop_CmpGT8Sx8;
            imm = 1L;
            switch (size) {
               case 0: imm = (imm <<  8) | imm; 
               case 1: imm = (imm << 16) | imm; 
               case 2: imm = (imm << 32) | imm; 
               case 3: break;
               default: vassert(0);
            }
            imm_val = newTemp(Q ? Ity_V128 : Ity_I64);
            round = newTemp(Q ? Ity_V128 : Ity_I64);
            assign(imm_val, Q ? mkU128(imm) : mkU64(imm));
            if (U) {
               switch (size) {
                  case 0:
                     op = Q ? Iop_QShl8x16 : Iop_QShl8x8;
                     op_add = Q ? Iop_Add8x16 : Iop_Add8x8;
                     op_rev = Q ? Iop_Shr8x16 : Iop_Shr8x8;
                     op_shrn = Q ? Iop_ShrN8x16 : Iop_ShrN8x8;
                     op_shln = Q ? Iop_ShlN8x16 : Iop_ShlN8x8;
                     break;
                  case 1:
                     op = Q ? Iop_QShl16x8 : Iop_QShl16x4;
                     op_add = Q ? Iop_Add16x8 : Iop_Add16x4;
                     op_rev = Q ? Iop_Shr16x8 : Iop_Shr16x4;
                     op_shrn = Q ? Iop_ShrN16x8 : Iop_ShrN16x4;
                     op_shln = Q ? Iop_ShlN16x8 : Iop_ShlN16x4;
                     break;
                  case 2:
                     op = Q ? Iop_QShl32x4 : Iop_QShl32x2;
                     op_add = Q ? Iop_Add32x4 : Iop_Add32x2;
                     op_rev = Q ? Iop_Shr32x4 : Iop_Shr32x2;
                     op_shrn = Q ? Iop_ShrN32x4 : Iop_ShrN32x2;
                     op_shln = Q ? Iop_ShlN32x4 : Iop_ShlN32x2;
                     break;
                  case 3:
                     op = Q ? Iop_QShl64x2 : Iop_QShl64x1;
                     op_add = Q ? Iop_Add64x2 : Iop_Add64;
                     op_rev = Q ? Iop_Shr64x2 : Iop_Shr64;
                     op_shrn = Q ? Iop_ShrN64x2 : Iop_Shr64;
                     op_shln = Q ? Iop_ShlN64x2 : Iop_Shl64;
                     break;
                  default:
                     vassert(0);
               }
            } else {
               switch (size) {
                  case 0:
                     op = Q ? Iop_QSal8x16 : Iop_QSal8x8;
                     op_add = Q ? Iop_Add8x16 : Iop_Add8x8;
                     op_rev = Q ? Iop_Sar8x16 : Iop_Sar8x8;
                     op_shrn = Q ? Iop_ShrN8x16 : Iop_ShrN8x8;
                     op_shln = Q ? Iop_ShlN8x16 : Iop_ShlN8x8;
                     break;
                  case 1:
                     op = Q ? Iop_QSal16x8 : Iop_QSal16x4;
                     op_add = Q ? Iop_Add16x8 : Iop_Add16x4;
                     op_rev = Q ? Iop_Sar16x8 : Iop_Sar16x4;
                     op_shrn = Q ? Iop_ShrN16x8 : Iop_ShrN16x4;
                     op_shln = Q ? Iop_ShlN16x8 : Iop_ShlN16x4;
                     break;
                  case 2:
                     op = Q ? Iop_QSal32x4 : Iop_QSal32x2;
                     op_add = Q ? Iop_Add32x4 : Iop_Add32x2;
                     op_rev = Q ? Iop_Sar32x4 : Iop_Sar32x2;
                     op_shrn = Q ? Iop_ShrN32x4 : Iop_ShrN32x2;
                     op_shln = Q ? Iop_ShlN32x4 : Iop_ShlN32x2;
                     break;
                  case 3:
                     op = Q ? Iop_QSal64x2 : Iop_QSal64x1;
                     op_add = Q ? Iop_Add64x2 : Iop_Add64;
                     op_rev = Q ? Iop_Sar64x2 : Iop_Sar64;
                     op_shrn = Q ? Iop_ShrN64x2 : Iop_Shr64;
                     op_shln = Q ? Iop_ShlN64x2 : Iop_Shl64;
                     break;
                  default:
                     vassert(0);
               }
            }
            if (Q) {
               tmp = newTemp(Ity_V128);
               shval = newTemp(Ity_V128);
               mask = newTemp(Ity_V128);
            } else {
               tmp = newTemp(Ity_I64);
               shval = newTemp(Ity_I64);
               mask = newTemp(Ity_I64);
            }
            assign(shval, binop(op_shrn,
                                binop(op_shln,
                                       mkexpr(arg_n),
                                       mkU8((8 << size) - 8)),
                                mkU8((8 << size) - 8)));
            for (i = 0; i < size; i++) {
               old_shval = shval;
               shval = newTemp(Q ? Ity_V128 : Ity_I64);
               assign(shval, binop(Q ? Iop_OrV128 : Iop_Or64,
                                   mkexpr(old_shval),
                                   binop(op_shln,
                                         mkexpr(old_shval),
                                         mkU8(8 << i))));
            }
            
            assign(round, binop(Q ? Iop_AndV128 : Iop_And64,
                                binop(op,
                                      mkexpr(arg_m),
                                      binop(op_add,
                                            mkexpr(arg_n),
                                            mkexpr(imm_val))),
                                binop(Q ? Iop_AndV128 : Iop_And64,
                                      mkexpr(imm_val),
                                      binop(cmp_gt,
                                            Q ? mkU128(0) : mkU64(0),
                                            mkexpr(arg_n)))));
            assign(res, binop(op_add,
                              binop(op, mkexpr(arg_m), mkexpr(arg_n)),
                              mkexpr(round)));
            esize = (8 << size) - 1;
            esize = (esize <<  8) | esize;
            esize = (esize << 16) | esize;
            esize = (esize << 32) | esize;
            setFlag_QC(binop(Q ? Iop_AndV128 : Iop_And64,
                             binop(cmp_gt, mkexpr(shval),
                                           Q ? mkU128(esize) : mkU64(esize)),
                             unop(cmp_neq, mkexpr(arg_m))),
                       Q ? mkU128(0) : mkU64(0),
                       Q, condT);
            assign(mask, binop(cmp_gt, mkexpr(shval),
                               Q ? mkU128(0) : mkU64(0)));
            if (!Q && size == 3)
               assign(tmp, binop(op_rev, mkexpr(res),
                                         unop(Iop_64to8, mkexpr(arg_n))));
            else
               assign(tmp, binop(op_rev, mkexpr(res), mkexpr(arg_n)));
            setFlag_QC(binop(Q ? Iop_AndV128 : Iop_And64,
                             mkexpr(tmp), mkexpr(mask)),
                       binop(Q ? Iop_AndV128 : Iop_And64,
                             mkexpr(arg_m), mkexpr(mask)),
                       Q, condT);
            DIP("vqrshl.%c%u %c%u, %c%u, %c%u\n",
                U ? 'u' : 's', 8 << size,
                Q ? 'q' : 'd', dreg, Q ? 'q' : 'd', mreg, Q ? 'q' : 'd',
                nreg);
         }
         break;
      case 6:
         
         if (B == 0) {
            
            IROp op;
            if (U == 0) {
               switch (size) {
                  case 0: op = Q ? Iop_Max8Sx16 : Iop_Max8Sx8; break;
                  case 1: op = Q ? Iop_Max16Sx8 : Iop_Max16Sx4; break;
                  case 2: op = Q ? Iop_Max32Sx4 : Iop_Max32Sx2; break;
                  case 3: return False;
                  default: vassert(0);
               }
            } else {
               switch (size) {
                  case 0: op = Q ? Iop_Max8Ux16 : Iop_Max8Ux8; break;
                  case 1: op = Q ? Iop_Max16Ux8 : Iop_Max16Ux4; break;
                  case 2: op = Q ? Iop_Max32Ux4 : Iop_Max32Ux2; break;
                  case 3: return False;
                  default: vassert(0);
               }
            }
            assign(res, binop(op, mkexpr(arg_n), mkexpr(arg_m)));
            DIP("vmax.%c%u %c%u, %c%u, %c%u\n",
                U ? 'u' : 's', 8 << size,
                Q ? 'q' : 'd', dreg, Q ? 'q' : 'd', nreg, Q ? 'q' : 'd',
                mreg);
         } else {
            
            IROp op;
            if (U == 0) {
               switch (size) {
                  case 0: op = Q ? Iop_Min8Sx16 : Iop_Min8Sx8; break;
                  case 1: op = Q ? Iop_Min16Sx8 : Iop_Min16Sx4; break;
                  case 2: op = Q ? Iop_Min32Sx4 : Iop_Min32Sx2; break;
                  case 3: return False;
                  default: vassert(0);
               }
            } else {
               switch (size) {
                  case 0: op = Q ? Iop_Min8Ux16 : Iop_Min8Ux8; break;
                  case 1: op = Q ? Iop_Min16Ux8 : Iop_Min16Ux4; break;
                  case 2: op = Q ? Iop_Min32Ux4 : Iop_Min32Ux2; break;
                  case 3: return False;
                  default: vassert(0);
               }
            }
            assign(res, binop(op, mkexpr(arg_n), mkexpr(arg_m)));
            DIP("vmin.%c%u %c%u, %c%u, %c%u\n",
                U ? 'u' : 's', 8 << size,
                Q ? 'q' : 'd', dreg, Q ? 'q' : 'd', nreg, Q ? 'q' : 'd',
                mreg);
         }
         break;
      case 7:
         if (B == 0) {
            
            IROp op_cmp, op_sub;
            IRTemp cond;
            if ((theInstr >> 23) & 1) {
               vpanic("VABDL should not be in dis_neon_data_3same\n");
            }
            if (Q) {
               switch (size) {
                  case 0:
                     op_cmp = U ? Iop_CmpGT8Ux16 : Iop_CmpGT8Sx16;
                     op_sub = Iop_Sub8x16;
                     break;
                  case 1:
                     op_cmp = U ? Iop_CmpGT16Ux8 : Iop_CmpGT16Sx8;
                     op_sub = Iop_Sub16x8;
                     break;
                  case 2:
                     op_cmp = U ? Iop_CmpGT32Ux4 : Iop_CmpGT32Sx4;
                     op_sub = Iop_Sub32x4;
                     break;
                  case 3:
                     return False;
                  default:
                     vassert(0);
               }
            } else {
               switch (size) {
                  case 0:
                     op_cmp = U ? Iop_CmpGT8Ux8 : Iop_CmpGT8Sx8;
                     op_sub = Iop_Sub8x8;
                     break;
                  case 1:
                     op_cmp = U ? Iop_CmpGT16Ux4 : Iop_CmpGT16Sx4;
                     op_sub = Iop_Sub16x4;
                     break;
                  case 2:
                     op_cmp = U ? Iop_CmpGT32Ux2 : Iop_CmpGT32Sx2;
                     op_sub = Iop_Sub32x2;
                     break;
                  case 3:
                     return False;
                  default:
                     vassert(0);
               }
            }
            if (Q) {
               cond = newTemp(Ity_V128);
            } else {
               cond = newTemp(Ity_I64);
            }
            assign(cond, binop(op_cmp, mkexpr(arg_n), mkexpr(arg_m)));
            assign(res, binop(Q ? Iop_OrV128 : Iop_Or64,
                              binop(Q ? Iop_AndV128 : Iop_And64,
                                    binop(op_sub, mkexpr(arg_n),
                                                  mkexpr(arg_m)),
                                    mkexpr(cond)),
                              binop(Q ? Iop_AndV128 : Iop_And64,
                                    binop(op_sub, mkexpr(arg_m),
                                                  mkexpr(arg_n)),
                                    unop(Q ? Iop_NotV128 : Iop_Not64,
                                         mkexpr(cond)))));
            DIP("vabd.%c%u %c%u, %c%u, %c%u\n",
                U ? 'u' : 's', 8 << size,
                Q ? 'q' : 'd', dreg, Q ? 'q' : 'd', nreg, Q ? 'q' : 'd',
                mreg);
         } else {
            
            IROp op_cmp, op_sub, op_add;
            IRTemp cond, acc, tmp;
            if ((theInstr >> 23) & 1) {
               vpanic("VABAL should not be in dis_neon_data_3same");
            }
            if (Q) {
               switch (size) {
                  case 0:
                     op_cmp = U ? Iop_CmpGT8Ux16 : Iop_CmpGT8Sx16;
                     op_sub = Iop_Sub8x16;
                     op_add = Iop_Add8x16;
                     break;
                  case 1:
                     op_cmp = U ? Iop_CmpGT16Ux8 : Iop_CmpGT16Sx8;
                     op_sub = Iop_Sub16x8;
                     op_add = Iop_Add16x8;
                     break;
                  case 2:
                     op_cmp = U ? Iop_CmpGT32Ux4 : Iop_CmpGT32Sx4;
                     op_sub = Iop_Sub32x4;
                     op_add = Iop_Add32x4;
                     break;
                  case 3:
                     return False;
                  default:
                     vassert(0);
               }
            } else {
               switch (size) {
                  case 0:
                     op_cmp = U ? Iop_CmpGT8Ux8 : Iop_CmpGT8Sx8;
                     op_sub = Iop_Sub8x8;
                     op_add = Iop_Add8x8;
                     break;
                  case 1:
                     op_cmp = U ? Iop_CmpGT16Ux4 : Iop_CmpGT16Sx4;
                     op_sub = Iop_Sub16x4;
                     op_add = Iop_Add16x4;
                     break;
                  case 2:
                     op_cmp = U ? Iop_CmpGT32Ux2 : Iop_CmpGT32Sx2;
                     op_sub = Iop_Sub32x2;
                     op_add = Iop_Add32x2;
                     break;
                  case 3:
                     return False;
                  default:
                     vassert(0);
               }
            }
            if (Q) {
               cond = newTemp(Ity_V128);
               acc = newTemp(Ity_V128);
               tmp = newTemp(Ity_V128);
               assign(acc, getQReg(dreg));
            } else {
               cond = newTemp(Ity_I64);
               acc = newTemp(Ity_I64);
               tmp = newTemp(Ity_I64);
               assign(acc, getDRegI64(dreg));
            }
            assign(cond, binop(op_cmp, mkexpr(arg_n), mkexpr(arg_m)));
            assign(tmp, binop(Q ? Iop_OrV128 : Iop_Or64,
                              binop(Q ? Iop_AndV128 : Iop_And64,
                                    binop(op_sub, mkexpr(arg_n),
                                                  mkexpr(arg_m)),
                                    mkexpr(cond)),
                              binop(Q ? Iop_AndV128 : Iop_And64,
                                    binop(op_sub, mkexpr(arg_m),
                                                  mkexpr(arg_n)),
                                    unop(Q ? Iop_NotV128 : Iop_Not64,
                                         mkexpr(cond)))));
            assign(res, binop(op_add, mkexpr(acc), mkexpr(tmp)));
            DIP("vaba.%c%u %c%u, %c%u, %c%u\n",
                U ? 'u' : 's', 8 << size,
                Q ? 'q' : 'd', dreg, Q ? 'q' : 'd', nreg, Q ? 'q' : 'd',
                mreg);
         }
         break;
      case 8:
         if (B == 0) {
            IROp op;
            if (U == 0) {
               
               switch (size) {
                  case 0: op = Q ? Iop_Add8x16 : Iop_Add8x8; break;
                  case 1: op = Q ? Iop_Add16x8 : Iop_Add16x4; break;
                  case 2: op = Q ? Iop_Add32x4 : Iop_Add32x2; break;
                  case 3: op = Q ? Iop_Add64x2 : Iop_Add64; break;
                  default: vassert(0);
               }
               DIP("vadd.i%u %c%u, %c%u, %c%u\n",
                   8 << size, Q ? 'q' : 'd',
                   dreg, Q ? 'q' : 'd', nreg, Q ? 'q' : 'd', mreg);
            } else {
               
               switch (size) {
                  case 0: op = Q ? Iop_Sub8x16 : Iop_Sub8x8; break;
                  case 1: op = Q ? Iop_Sub16x8 : Iop_Sub16x4; break;
                  case 2: op = Q ? Iop_Sub32x4 : Iop_Sub32x2; break;
                  case 3: op = Q ? Iop_Sub64x2 : Iop_Sub64; break;
                  default: vassert(0);
               }
               DIP("vsub.i%u %c%u, %c%u, %c%u\n",
                   8 << size, Q ? 'q' : 'd',
                   dreg, Q ? 'q' : 'd', nreg, Q ? 'q' : 'd', mreg);
            }
            assign(res, binop(op, mkexpr(arg_n), mkexpr(arg_m)));
         } else {
            IROp op;
            switch (size) {
               case 0: op = Q ? Iop_CmpNEZ8x16 : Iop_CmpNEZ8x8; break;
               case 1: op = Q ? Iop_CmpNEZ16x8 : Iop_CmpNEZ16x4; break;
               case 2: op = Q ? Iop_CmpNEZ32x4 : Iop_CmpNEZ32x2; break;
               case 3: op = Q ? Iop_CmpNEZ64x2 : Iop_CmpwNEZ64; break;
               default: vassert(0);
            }
            if (U == 0) {
               
               assign(res, unop(op, binop(Q ? Iop_AndV128 : Iop_And64,
                                          mkexpr(arg_n),
                                          mkexpr(arg_m))));
               DIP("vtst.%u %c%u, %c%u, %c%u\n",
                   8 << size, Q ? 'q' : 'd',
                   dreg, Q ? 'q' : 'd', nreg, Q ? 'q' : 'd', mreg);
            } else {
               
               assign(res, unop(Q ? Iop_NotV128 : Iop_Not64,
                                unop(op,
                                     binop(Q ? Iop_XorV128 : Iop_Xor64,
                                           mkexpr(arg_n),
                                           mkexpr(arg_m)))));
               DIP("vceq.i%u %c%u, %c%u, %c%u\n",
                   8 << size, Q ? 'q' : 'd',
                   dreg, Q ? 'q' : 'd', nreg, Q ? 'q' : 'd', mreg);
            }
         }
         break;
      case 9:
         if (B == 0) {
            
            IROp op, op2;
            UInt P = (theInstr >> 24) & 1;
            if (P) {
               switch (size) {
                  case 0:
                     op = Q ? Iop_Mul8x16 : Iop_Mul8x8;
                     op2 = Q ? Iop_Sub8x16 : Iop_Sub8x8;
                     break;
                  case 1:
                     op = Q ? Iop_Mul16x8 : Iop_Mul16x4;
                     op2 = Q ? Iop_Sub16x8 : Iop_Sub16x4;
                     break;
                  case 2:
                     op = Q ? Iop_Mul32x4 : Iop_Mul32x2;
                     op2 = Q ? Iop_Sub32x4 : Iop_Sub32x2;
                     break;
                  case 3:
                     return False;
                  default:
                     vassert(0);
               }
            } else {
               switch (size) {
                  case 0:
                     op = Q ? Iop_Mul8x16 : Iop_Mul8x8;
                     op2 = Q ? Iop_Add8x16 : Iop_Add8x8;
                     break;
                  case 1:
                     op = Q ? Iop_Mul16x8 : Iop_Mul16x4;
                     op2 = Q ? Iop_Add16x8 : Iop_Add16x4;
                     break;
                  case 2:
                     op = Q ? Iop_Mul32x4 : Iop_Mul32x2;
                     op2 = Q ? Iop_Add32x4 : Iop_Add32x2;
                     break;
                  case 3:
                     return False;
                  default:
                     vassert(0);
               }
            }
            assign(res, binop(op2,
                              Q ? getQReg(dreg) : getDRegI64(dreg),
                              binop(op, mkexpr(arg_n), mkexpr(arg_m))));
            DIP("vml%c.i%u %c%u, %c%u, %c%u\n",
                P ? 's' : 'a', 8 << size,
                Q ? 'q' : 'd', dreg, Q ? 'q' : 'd', nreg, Q ? 'q' : 'd',
                mreg);
         } else {
            
            IROp op;
            UInt P = (theInstr >> 24) & 1;
            if (P) {
               switch (size) {
                  case 0:
                     op = Q ? Iop_PolynomialMul8x16 : Iop_PolynomialMul8x8;
                     break;
                  case 1: case 2: case 3: return False;
                  default: vassert(0);
               }
            } else {
               switch (size) {
                  case 0: op = Q ? Iop_Mul8x16 : Iop_Mul8x8; break;
                  case 1: op = Q ? Iop_Mul16x8 : Iop_Mul16x4; break;
                  case 2: op = Q ? Iop_Mul32x4 : Iop_Mul32x2; break;
                  case 3: return False;
                  default: vassert(0);
               }
            }
            assign(res, binop(op, mkexpr(arg_n), mkexpr(arg_m)));
            DIP("vmul.%c%u %c%u, %c%u, %c%u\n",
                P ? 'p' : 'i', 8 << size,
                Q ? 'q' : 'd', dreg, Q ? 'q' : 'd', nreg, Q ? 'q' : 'd',
                mreg);
         }
         break;
      case 10: {
         
         UInt P = (theInstr >> 4) & 1;
         IROp op;
         if (Q)
            return False;
         if (P) {
            switch (size) {
               case 0: op = U ? Iop_PwMin8Ux8  : Iop_PwMin8Sx8; break;
               case 1: op = U ? Iop_PwMin16Ux4 : Iop_PwMin16Sx4; break;
               case 2: op = U ? Iop_PwMin32Ux2 : Iop_PwMin32Sx2; break;
               case 3: return False;
               default: vassert(0);
            }
         } else {
            switch (size) {
               case 0: op = U ? Iop_PwMax8Ux8  : Iop_PwMax8Sx8; break;
               case 1: op = U ? Iop_PwMax16Ux4 : Iop_PwMax16Sx4; break;
               case 2: op = U ? Iop_PwMax32Ux2 : Iop_PwMax32Sx2; break;
               case 3: return False;
               default: vassert(0);
            }
         }
         assign(res, binop(op, mkexpr(arg_n), mkexpr(arg_m)));
         DIP("vp%s.%c%u %c%u, %c%u, %c%u\n",
             P ? "min" : "max", U ? 'u' : 's',
             8 << size, Q ? 'q' : 'd', dreg, Q ? 'q' : 'd', nreg,
             Q ? 'q' : 'd', mreg);
         break;
      }
      case 11:
         if (B == 0) {
            if (U == 0) {
               
               IROp op ,op2;
               ULong imm;
               switch (size) {
                  case 0: case 3:
                     return False;
                  case 1:
                     op = Q ? Iop_QDMulHi16Sx8 : Iop_QDMulHi16Sx4;
                     op2 = Q ? Iop_CmpEQ16x8 : Iop_CmpEQ16x4;
                     imm = 1LL << 15;
                     imm = (imm << 16) | imm;
                     imm = (imm << 32) | imm;
                     break;
                  case 2:
                     op = Q ? Iop_QDMulHi32Sx4 : Iop_QDMulHi32Sx2;
                     op2 = Q ? Iop_CmpEQ32x4 : Iop_CmpEQ32x2;
                     imm = 1LL << 31;
                     imm = (imm << 32) | imm;
                     break;
                  default:
                     vassert(0);
               }
               assign(res, binop(op, mkexpr(arg_n), mkexpr(arg_m)));
               setFlag_QC(binop(Q ? Iop_AndV128 : Iop_And64,
                                binop(op2, mkexpr(arg_n),
                                           Q ? mkU128(imm) : mkU64(imm)),
                                binop(op2, mkexpr(arg_m),
                                           Q ? mkU128(imm) : mkU64(imm))),
                          Q ? mkU128(0) : mkU64(0),
                          Q, condT);
               DIP("vqdmulh.s%u %c%u, %c%u, %c%u\n",
                   8 << size, Q ? 'q' : 'd',
                   dreg, Q ? 'q' : 'd', nreg, Q ? 'q' : 'd', mreg);
            } else {
               
               IROp op ,op2;
               ULong imm;
               switch(size) {
                  case 0: case 3:
                     return False;
                  case 1:
                     imm = 1LL << 15;
                     imm = (imm << 16) | imm;
                     imm = (imm << 32) | imm;
                     op = Q ? Iop_QRDMulHi16Sx8 : Iop_QRDMulHi16Sx4;
                     op2 = Q ? Iop_CmpEQ16x8 : Iop_CmpEQ16x4;
                     break;
                  case 2:
                     imm = 1LL << 31;
                     imm = (imm << 32) | imm;
                     op = Q ? Iop_QRDMulHi32Sx4 : Iop_QRDMulHi32Sx2;
                     op2 = Q ? Iop_CmpEQ32x4 : Iop_CmpEQ32x2;
                     break;
                  default:
                     vassert(0);
               }
               assign(res, binop(op, mkexpr(arg_n), mkexpr(arg_m)));
               setFlag_QC(binop(Q ? Iop_AndV128 : Iop_And64,
                                binop(op2, mkexpr(arg_n),
                                           Q ? mkU128(imm) : mkU64(imm)),
                                binop(op2, mkexpr(arg_m),
                                           Q ? mkU128(imm) : mkU64(imm))),
                          Q ? mkU128(0) : mkU64(0),
                          Q, condT);
               DIP("vqrdmulh.s%u %c%u, %c%u, %c%u\n",
                   8 << size, Q ? 'q' : 'd',
                   dreg, Q ? 'q' : 'd', nreg, Q ? 'q' : 'd', mreg);
            }
         } else {
            if (U == 0) {
               
               IROp op;
               if (Q)
                  return False;
               switch (size) {
                  case 0: op = Q ? Iop_PwAdd8x16 : Iop_PwAdd8x8;  break;
                  case 1: op = Q ? Iop_PwAdd16x8 : Iop_PwAdd16x4; break;
                  case 2: op = Q ? Iop_PwAdd32x4 : Iop_PwAdd32x2; break;
                  case 3: return False;
                  default: vassert(0);
               }
               assign(res, binop(op, mkexpr(arg_n), mkexpr(arg_m)));
               DIP("vpadd.i%d %c%u, %c%u, %c%u\n",
                   8 << size, Q ? 'q' : 'd',
                   dreg, Q ? 'q' : 'd', nreg, Q ? 'q' : 'd', mreg);
            }
         }
         break;
      
      case 13:
         if (B == 0) {
            IROp op;
            if (U == 0) {
               if ((C >> 1) == 0) {
                  
                  op = Q ? Iop_Add32Fx4 : Iop_Add32Fx2 ;
                  DIP("vadd.f32 %c%u, %c%u, %c%u\n",
                      Q ? 'q' : 'd', dreg,
                      Q ? 'q' : 'd', nreg, Q ? 'q' : 'd', mreg);
               } else {
                  
                  op = Q ? Iop_Sub32Fx4 : Iop_Sub32Fx2 ;
                  DIP("vsub.f32 %c%u, %c%u, %c%u\n",
                      Q ? 'q' : 'd', dreg,
                      Q ? 'q' : 'd', nreg, Q ? 'q' : 'd', mreg);
               }
            } else {
               if ((C >> 1) == 0) {
                  
                  if (Q)
                     return False;
                  op = Iop_PwAdd32Fx2;
                  DIP("vpadd.f32 d%u, d%u, d%u\n", dreg, nreg, mreg);
               } else {
                  
                  if (Q) {
                     assign(res, unop(Iop_Abs32Fx4,
                                      triop(Iop_Sub32Fx4,
                                            get_FAKE_roundingmode(),
                                            mkexpr(arg_n),
                                            mkexpr(arg_m))));
                  } else {
                     assign(res, unop(Iop_Abs32Fx2,
                                      binop(Iop_Sub32Fx2,
                                            mkexpr(arg_n),
                                            mkexpr(arg_m))));
                  }
                  DIP("vabd.f32 %c%u, %c%u, %c%u\n",
                      Q ? 'q' : 'd', dreg,
                      Q ? 'q' : 'd', nreg, Q ? 'q' : 'd', mreg);
                  break;
               }
            }
            assign(res, binop_w_fake_RM(op, mkexpr(arg_n), mkexpr(arg_m)));
         } else {
            if (U == 0) {
               
               IROp op, op2;
               UInt P = (theInstr >> 21) & 1;
               if (P) {
                  switch (size & 1) {
                     case 0:
                        op = Q ? Iop_Mul32Fx4 : Iop_Mul32Fx2;
                        op2 = Q ? Iop_Sub32Fx4 : Iop_Sub32Fx2;
                        break;
                     case 1: return False;
                     default: vassert(0);
                  }
               } else {
                  switch (size & 1) {
                     case 0:
                        op = Q ? Iop_Mul32Fx4 : Iop_Mul32Fx2;
                        op2 = Q ? Iop_Add32Fx4 : Iop_Add32Fx2;
                        break;
                     case 1: return False;
                     default: vassert(0);
                  }
               }
               assign(res, binop_w_fake_RM(
                              op2,
                              Q ? getQReg(dreg) : getDRegI64(dreg),
                              binop_w_fake_RM(op, mkexpr(arg_n),
                                                  mkexpr(arg_m))));

               DIP("vml%c.f32 %c%u, %c%u, %c%u\n",
                   P ? 's' : 'a', Q ? 'q' : 'd',
                   dreg, Q ? 'q' : 'd', nreg, Q ? 'q' : 'd', mreg);
            } else {
               
               IROp op;
               if ((C >> 1) != 0)
                  return False;
               op = Q ? Iop_Mul32Fx4 : Iop_Mul32Fx2 ;
               assign(res, binop_w_fake_RM(op, mkexpr(arg_n), mkexpr(arg_m)));
               DIP("vmul.f32 %c%u, %c%u, %c%u\n",
                   Q ? 'q' : 'd', dreg,
                   Q ? 'q' : 'd', nreg, Q ? 'q' : 'd', mreg);
            }
         }
         break;
      case 14:
         if (B == 0) {
            if (U == 0) {
               if ((C >> 1) == 0) {
                  
                  IROp op;
                  if ((theInstr >> 20) & 1)
                     return False;
                  op = Q ? Iop_CmpEQ32Fx4 : Iop_CmpEQ32Fx2;
                  assign(res, binop(op, mkexpr(arg_n), mkexpr(arg_m)));
                  DIP("vceq.f32 %c%u, %c%u, %c%u\n",
                      Q ? 'q' : 'd', dreg,
                      Q ? 'q' : 'd', nreg, Q ? 'q' : 'd', mreg);
               } else {
                  return False;
               }
            } else {
               if ((C >> 1) == 0) {
                  
                  IROp op;
                  if ((theInstr >> 20) & 1)
                     return False;
                  op = Q ? Iop_CmpGE32Fx4 : Iop_CmpGE32Fx2;
                  assign(res, binop(op, mkexpr(arg_n), mkexpr(arg_m)));
                  DIP("vcge.f32 %c%u, %c%u, %c%u\n",
                      Q ? 'q' : 'd', dreg,
                      Q ? 'q' : 'd', nreg, Q ? 'q' : 'd', mreg);
               } else {
                  
                  IROp op;
                  if ((theInstr >> 20) & 1)
                     return False;
                  op = Q ? Iop_CmpGT32Fx4 : Iop_CmpGT32Fx2;
                  assign(res, binop(op, mkexpr(arg_n), mkexpr(arg_m)));
                  DIP("vcgt.f32 %c%u, %c%u, %c%u\n",
                      Q ? 'q' : 'd', dreg,
                      Q ? 'q' : 'd', nreg, Q ? 'q' : 'd', mreg);
               }
            }
         } else {
            if (U == 1) {
               
               UInt op_bit = (theInstr >> 21) & 1;
               IROp op, op2;
               op2 = Q ? Iop_Abs32Fx4 : Iop_Abs32Fx2;
               if (op_bit) {
                  op = Q ? Iop_CmpGT32Fx4 : Iop_CmpGT32Fx2;
                  assign(res, binop(op,
                                    unop(op2, mkexpr(arg_n)),
                                    unop(op2, mkexpr(arg_m))));
               } else {
                  op = Q ? Iop_CmpGE32Fx4 : Iop_CmpGE32Fx2;
                  assign(res, binop(op,
                                    unop(op2, mkexpr(arg_n)),
                                    unop(op2, mkexpr(arg_m))));
               }
               DIP("vacg%c.f32 %c%u, %c%u, %c%u\n", op_bit ? 't' : 'e',
                   Q ? 'q' : 'd', dreg, Q ? 'q' : 'd', nreg,
                   Q ? 'q' : 'd', mreg);
            }
         }
         break;
      case 15:
         if (B == 0) {
            if (U == 0) {
               
               IROp op;
               if ((theInstr >> 20) & 1)
                  return False;
               if ((theInstr >> 21) & 1) {
                  op = Q ? Iop_Min32Fx4 : Iop_Min32Fx2;
                  DIP("vmin.f32 %c%u, %c%u, %c%u\n", Q ? 'q' : 'd', dreg,
                      Q ? 'q' : 'd', nreg, Q ? 'q' : 'd', mreg);
               } else {
                  op = Q ? Iop_Max32Fx4 : Iop_Max32Fx2;
                  DIP("vmax.f32 %c%u, %c%u, %c%u\n", Q ? 'q' : 'd', dreg,
                      Q ? 'q' : 'd', nreg, Q ? 'q' : 'd', mreg);
               }
               assign(res, binop(op, mkexpr(arg_n), mkexpr(arg_m)));
            } else {
               
               IROp op;
               if (Q)
                  return False;
               if ((theInstr >> 20) & 1)
                  return False;
               if ((theInstr >> 21) & 1) {
                  op = Iop_PwMin32Fx2;
                  DIP("vpmin.f32 d%u, d%u, d%u\n", dreg, nreg, mreg);
               } else {
                  op = Iop_PwMax32Fx2;
                  DIP("vpmax.f32 d%u, d%u, d%u\n", dreg, nreg, mreg);
               }
               assign(res, binop(op, mkexpr(arg_n), mkexpr(arg_m)));
            }
         } else {
            if (U == 0) {
               if ((C >> 1) == 0) {
                  
                  if ((theInstr >> 20) & 1)
                     return False;
                  assign(res, binop(Q ? Iop_RecipStep32Fx4
                                      : Iop_RecipStep32Fx2,
                                    mkexpr(arg_n),
                                    mkexpr(arg_m)));
                  DIP("vrecps.f32 %c%u, %c%u, %c%u\n", Q ? 'q' : 'd', dreg,
                      Q ? 'q' : 'd', nreg, Q ? 'q' : 'd', mreg);
               } else {
                  
                  if ((theInstr >> 20) & 1)
                     return False;
                  assign(res, binop(Q ? Iop_RSqrtStep32Fx4
                                      : Iop_RSqrtStep32Fx2,
                                    mkexpr(arg_n),
                                    mkexpr(arg_m)));
                  DIP("vrsqrts.f32 %c%u, %c%u, %c%u\n", Q ? 'q' : 'd', dreg,
                      Q ? 'q' : 'd', nreg, Q ? 'q' : 'd', mreg);
               }
            }
         }
         break;
   }

   if (Q) {
      putQReg(dreg, mkexpr(res), condT);
   } else {
      putDRegI64(dreg, mkexpr(res), condT);
   }

   return True;
}

static
Bool dis_neon_data_3diff ( UInt theInstr, IRTemp condT )
{
   UInt A = (theInstr >> 8) & 0xf;
   UInt B = (theInstr >> 20) & 3;
   UInt U = (theInstr >> 24) & 1;
   UInt P = (theInstr >> 9) & 1;
   UInt mreg = get_neon_m_regno(theInstr);
   UInt nreg = get_neon_n_regno(theInstr);
   UInt dreg = get_neon_d_regno(theInstr);
   UInt size = B;
   ULong imm;
   IRTemp res, arg_m, arg_n, cond, tmp;
   IROp cvt, cvt2, cmp, op, op2, sh, add;
   switch (A) {
      case 0: case 1: case 2: case 3:
         
         if (dreg & 1)
            return False;
         dreg >>= 1;
         size = B;
         switch (size) {
            case 0:
               cvt = U ? Iop_Widen8Uto16x8 : Iop_Widen8Sto16x8;
               op = (A & 2) ? Iop_Sub16x8 : Iop_Add16x8;
               break;
            case 1:
               cvt = U ? Iop_Widen16Uto32x4 : Iop_Widen16Sto32x4;
               op = (A & 2) ? Iop_Sub32x4 : Iop_Add32x4;
               break;
            case 2:
               cvt = U ? Iop_Widen32Uto64x2 : Iop_Widen32Sto64x2;
               op = (A & 2) ? Iop_Sub64x2 : Iop_Add64x2;
               break;
            case 3:
               return False;
            default:
               vassert(0);
         }
         arg_n = newTemp(Ity_V128);
         arg_m = newTemp(Ity_V128);
         if (A & 1) {
            if (nreg & 1)
               return False;
            nreg >>= 1;
            assign(arg_n, getQReg(nreg));
         } else {
            assign(arg_n, unop(cvt, getDRegI64(nreg)));
         }
         assign(arg_m, unop(cvt, getDRegI64(mreg)));
         putQReg(dreg, binop(op, mkexpr(arg_n), mkexpr(arg_m)),
                       condT);
         DIP("v%s%c.%c%u q%u, %c%u, d%u\n", (A & 2) ? "sub" : "add",
             (A & 1) ? 'w' : 'l', U ? 'u' : 's', 8 << size, dreg,
             (A & 1) ? 'q' : 'd', nreg, mreg);
         return True;
      case 4:
         
         if (mreg & 1)
            return False;
         mreg >>= 1;
         if (nreg & 1)
            return False;
         nreg >>= 1;
         size = B;
         switch (size) {
            case 0:
               op = Iop_Add16x8;
               cvt = Iop_NarrowUn16to8x8;
               sh = Iop_ShrN16x8;
               imm = 1U << 7;
               imm = (imm << 16) | imm;
               imm = (imm << 32) | imm;
               break;
            case 1:
               op = Iop_Add32x4;
               cvt = Iop_NarrowUn32to16x4;
               sh = Iop_ShrN32x4;
               imm = 1U << 15;
               imm = (imm << 32) | imm;
               break;
            case 2:
               op = Iop_Add64x2;
               cvt = Iop_NarrowUn64to32x2;
               sh = Iop_ShrN64x2;
               imm = 1U << 31;
               break;
            case 3:
               return False;
            default:
               vassert(0);
         }
         tmp = newTemp(Ity_V128);
         res = newTemp(Ity_V128);
         assign(tmp, binop(op, getQReg(nreg), getQReg(mreg)));
         if (U) {
            
            assign(res, binop(op, mkexpr(tmp),
                     binop(Iop_64HLtoV128, mkU64(imm), mkU64(imm))));
         } else {
            assign(res, mkexpr(tmp));
         }
         putDRegI64(dreg, unop(cvt, binop(sh, mkexpr(res), mkU8(8 << size))),
                    condT);
         DIP("v%saddhn.i%u d%u, q%u, q%u\n", U ? "r" : "", 16 << size, dreg,
             nreg, mreg);
         return True;
      case 5:
         
         if (!((theInstr >> 23) & 1)) {
            vpanic("VABA should not be in dis_neon_data_3diff\n");
         }
         if (dreg & 1)
            return False;
         dreg >>= 1;
         switch (size) {
            case 0:
               cmp = U ? Iop_CmpGT8Ux8 : Iop_CmpGT8Sx8;
               cvt = U ? Iop_Widen8Uto16x8 : Iop_Widen8Sto16x8;
               cvt2 = Iop_Widen8Sto16x8;
               op = Iop_Sub16x8;
               op2 = Iop_Add16x8;
               break;
            case 1:
               cmp = U ? Iop_CmpGT16Ux4 : Iop_CmpGT16Sx4;
               cvt = U ? Iop_Widen16Uto32x4 : Iop_Widen16Sto32x4;
               cvt2 = Iop_Widen16Sto32x4;
               op = Iop_Sub32x4;
               op2 = Iop_Add32x4;
               break;
            case 2:
               cmp = U ? Iop_CmpGT32Ux2 : Iop_CmpGT32Sx2;
               cvt = U ? Iop_Widen32Uto64x2 : Iop_Widen32Sto64x2;
               cvt2 = Iop_Widen32Sto64x2;
               op = Iop_Sub64x2;
               op2 = Iop_Add64x2;
               break;
            case 3:
               return False;
            default:
               vassert(0);
         }
         arg_n = newTemp(Ity_V128);
         arg_m = newTemp(Ity_V128);
         cond = newTemp(Ity_V128);
         res = newTemp(Ity_V128);
         assign(arg_n, unop(cvt, getDRegI64(nreg)));
         assign(arg_m, unop(cvt, getDRegI64(mreg)));
         assign(cond, unop(cvt2, binop(cmp, getDRegI64(nreg),
                                            getDRegI64(mreg))));
         assign(res, binop(op2,
                           binop(Iop_OrV128,
                                 binop(Iop_AndV128,
                                       binop(op, mkexpr(arg_n), mkexpr(arg_m)),
                                       mkexpr(cond)),
                                 binop(Iop_AndV128,
                                       binop(op, mkexpr(arg_m), mkexpr(arg_n)),
                                       unop(Iop_NotV128, mkexpr(cond)))),
                           getQReg(dreg)));
         putQReg(dreg, mkexpr(res), condT);
         DIP("vabal.%c%u q%u, d%u, d%u\n", U ? 'u' : 's', 8 << size, dreg,
             nreg, mreg);
         return True;
      case 6:
         
         if (mreg & 1)
            return False;
         mreg >>= 1;
         if (nreg & 1)
            return False;
         nreg >>= 1;
         size = B;
         switch (size) {
            case 0:
               op = Iop_Sub16x8;
               op2 = Iop_Add16x8;
               cvt = Iop_NarrowUn16to8x8;
               sh = Iop_ShrN16x8;
               imm = 1U << 7;
               imm = (imm << 16) | imm;
               imm = (imm << 32) | imm;
               break;
            case 1:
               op = Iop_Sub32x4;
               op2 = Iop_Add32x4;
               cvt = Iop_NarrowUn32to16x4;
               sh = Iop_ShrN32x4;
               imm = 1U << 15;
               imm = (imm << 32) | imm;
               break;
            case 2:
               op = Iop_Sub64x2;
               op2 = Iop_Add64x2;
               cvt = Iop_NarrowUn64to32x2;
               sh = Iop_ShrN64x2;
               imm = 1U << 31;
               break;
            case 3:
               return False;
            default:
               vassert(0);
         }
         tmp = newTemp(Ity_V128);
         res = newTemp(Ity_V128);
         assign(tmp, binop(op, getQReg(nreg), getQReg(mreg)));
         if (U) {
            
            assign(res, binop(op2, mkexpr(tmp),
                     binop(Iop_64HLtoV128, mkU64(imm), mkU64(imm))));
         } else {
            assign(res, mkexpr(tmp));
         }
         putDRegI64(dreg, unop(cvt, binop(sh, mkexpr(res), mkU8(8 << size))),
                    condT);
         DIP("v%ssubhn.i%u d%u, q%u, q%u\n", U ? "r" : "", 16 << size, dreg,
             nreg, mreg);
         return True;
      case 7:
         
         if (!((theInstr >> 23) & 1)) {
            vpanic("VABL should not be in dis_neon_data_3diff\n");
         }
         if (dreg & 1)
            return False;
         dreg >>= 1;
         switch (size) {
            case 0:
               cmp = U ? Iop_CmpGT8Ux8 : Iop_CmpGT8Sx8;
               cvt = U ? Iop_Widen8Uto16x8 : Iop_Widen8Sto16x8;
               cvt2 = Iop_Widen8Sto16x8;
               op = Iop_Sub16x8;
               break;
            case 1:
               cmp = U ? Iop_CmpGT16Ux4 : Iop_CmpGT16Sx4;
               cvt = U ? Iop_Widen16Uto32x4 : Iop_Widen16Sto32x4;
               cvt2 = Iop_Widen16Sto32x4;
               op = Iop_Sub32x4;
               break;
            case 2:
               cmp = U ? Iop_CmpGT32Ux2 : Iop_CmpGT32Sx2;
               cvt = U ? Iop_Widen32Uto64x2 : Iop_Widen32Sto64x2;
               cvt2 = Iop_Widen32Sto64x2;
               op = Iop_Sub64x2;
               break;
            case 3:
               return False;
            default:
               vassert(0);
         }
         arg_n = newTemp(Ity_V128);
         arg_m = newTemp(Ity_V128);
         cond = newTemp(Ity_V128);
         res = newTemp(Ity_V128);
         assign(arg_n, unop(cvt, getDRegI64(nreg)));
         assign(arg_m, unop(cvt, getDRegI64(mreg)));
         assign(cond, unop(cvt2, binop(cmp, getDRegI64(nreg),
                                            getDRegI64(mreg))));
         assign(res, binop(Iop_OrV128,
                           binop(Iop_AndV128,
                                 binop(op, mkexpr(arg_n), mkexpr(arg_m)),
                                 mkexpr(cond)),
                           binop(Iop_AndV128,
                                 binop(op, mkexpr(arg_m), mkexpr(arg_n)),
                                 unop(Iop_NotV128, mkexpr(cond)))));
         putQReg(dreg, mkexpr(res), condT);
         DIP("vabdl.%c%u q%u, d%u, d%u\n", U ? 'u' : 's', 8 << size, dreg,
             nreg, mreg);
         return True;
      case 8:
      case 10:
         
         if (dreg & 1)
            return False;
         dreg >>= 1;
         size = B;
         switch (size) {
            case 0:
               op = U ? Iop_Mull8Ux8 : Iop_Mull8Sx8;
               op2 = P ? Iop_Sub16x8 : Iop_Add16x8;
               break;
            case 1:
               op = U ? Iop_Mull16Ux4 : Iop_Mull16Sx4;
               op2 = P ? Iop_Sub32x4 : Iop_Add32x4;
               break;
            case 2:
               op = U ? Iop_Mull32Ux2 : Iop_Mull32Sx2;
               op2 = P ? Iop_Sub64x2 : Iop_Add64x2;
               break;
            case 3:
               return False;
            default:
               vassert(0);
         }
         res = newTemp(Ity_V128);
         assign(res, binop(op, getDRegI64(nreg),getDRegI64(mreg)));
         putQReg(dreg, binop(op2, getQReg(dreg), mkexpr(res)), condT);
         DIP("vml%cl.%c%u q%u, d%u, d%u\n", P ? 's' : 'a', U ? 'u' : 's',
             8 << size, dreg, nreg, mreg);
         return True;
      case 9:
      case 11:
         
         if (U)
            return False;
         if (dreg & 1)
            return False;
         dreg >>= 1;
         size = B;
         switch (size) {
            case 0: case 3:
               return False;
            case 1:
               op = Iop_QDMull16Sx4;
               cmp = Iop_CmpEQ16x4;
               add = P ? Iop_QSub32Sx4 : Iop_QAdd32Sx4;
               op2 = P ? Iop_Sub32x4 : Iop_Add32x4;
               imm = 1LL << 15;
               imm = (imm << 16) | imm;
               imm = (imm << 32) | imm;
               break;
            case 2:
               op = Iop_QDMull32Sx2;
               cmp = Iop_CmpEQ32x2;
               add = P ? Iop_QSub64Sx2 : Iop_QAdd64Sx2;
               op2 = P ? Iop_Sub64x2 : Iop_Add64x2;
               imm = 1LL << 31;
               imm = (imm << 32) | imm;
               break;
            default:
               vassert(0);
         }
         res = newTemp(Ity_V128);
         tmp = newTemp(Ity_V128);
         assign(res, binop(op, getDRegI64(nreg), getDRegI64(mreg)));
         assign(tmp, binop(op2, getQReg(dreg), mkexpr(res)));
         setFlag_QC(mkexpr(tmp), binop(add, getQReg(dreg), mkexpr(res)),
                    True, condT);
         setFlag_QC(binop(Iop_And64,
                          binop(cmp, getDRegI64(nreg), mkU64(imm)),
                          binop(cmp, getDRegI64(mreg), mkU64(imm))),
                    mkU64(0),
                    False, condT);
         putQReg(dreg, binop(add, getQReg(dreg), mkexpr(res)), condT);
         DIP("vqdml%cl.s%u q%u, d%u, d%u\n", P ? 's' : 'a', 8 << size, dreg,
             nreg, mreg);
         return True;
      case 12:
      case 14:
         
         if (dreg & 1)
            return False;
         dreg >>= 1;
         size = B;
         switch (size) {
            case 0:
               op = (U) ? Iop_Mull8Ux8 : Iop_Mull8Sx8;
               if (P)
                  op = Iop_PolynomialMull8x8;
               break;
            case 1:
               op = (U) ? Iop_Mull16Ux4 : Iop_Mull16Sx4;
               break;
            case 2:
               op = (U) ? Iop_Mull32Ux2 : Iop_Mull32Sx2;
               break;
            default:
               vassert(0);
         }
         putQReg(dreg, binop(op, getDRegI64(nreg),
                                 getDRegI64(mreg)), condT);
         DIP("vmull.%c%u q%u, d%u, d%u\n", P ? 'p' : (U ? 'u' : 's'),
               8 << size, dreg, nreg, mreg);
         return True;
      case 13:
         
         if (U)
            return False;
         if (dreg & 1)
            return False;
         dreg >>= 1;
         size = B;
         switch (size) {
            case 0:
            case 3:
               return False;
            case 1:
               op = Iop_QDMull16Sx4;
               op2 = Iop_CmpEQ16x4;
               imm = 1LL << 15;
               imm = (imm << 16) | imm;
               imm = (imm << 32) | imm;
               break;
            case 2:
               op = Iop_QDMull32Sx2;
               op2 = Iop_CmpEQ32x2;
               imm = 1LL << 31;
               imm = (imm << 32) | imm;
               break;
            default:
               vassert(0);
         }
         putQReg(dreg, binop(op, getDRegI64(nreg), getDRegI64(mreg)),
               condT);
         setFlag_QC(binop(Iop_And64,
                          binop(op2, getDRegI64(nreg), mkU64(imm)),
                          binop(op2, getDRegI64(mreg), mkU64(imm))),
                    mkU64(0),
                    False, condT);
         DIP("vqdmull.s%u q%u, d%u, d%u\n", 8 << size, dreg, nreg, mreg);
         return True;
      default:
         return False;
   }
   return False;
}

static
Bool dis_neon_data_2reg_and_scalar ( UInt theInstr, IRTemp condT )
{
#  define INSN(_bMax,_bMin)  SLICE_UInt(theInstr, (_bMax), (_bMin))
   UInt U = INSN(24,24);
   UInt dreg = get_neon_d_regno(theInstr & ~(1 << 6));
   UInt nreg = get_neon_n_regno(theInstr & ~(1 << 6));
   UInt mreg = get_neon_m_regno(theInstr & ~(1 << 6));
   UInt size = INSN(21,20);
   UInt index;
   UInt Q = INSN(24,24);

   if (INSN(27,25) != 1 || INSN(23,23) != 1
       || INSN(6,6) != 1 || INSN(4,4) != 0)
      return False;

   
   if ((INSN(11,8) & BITS4(1,0,1,0)) == BITS4(0,0,0,0)) {
      IRTemp res, arg_m, arg_n;
      IROp dup, get, op, op2, add, sub;
      if (Q) {
         if ((dreg & 1) || (nreg & 1))
            return False;
         dreg >>= 1;
         nreg >>= 1;
         res = newTemp(Ity_V128);
         arg_m = newTemp(Ity_V128);
         arg_n = newTemp(Ity_V128);
         assign(arg_n, getQReg(nreg));
         switch(size) {
            case 1:
               dup = Iop_Dup16x8;
               get = Iop_GetElem16x4;
               index = mreg >> 3;
               mreg &= 7;
               break;
            case 2:
               dup = Iop_Dup32x4;
               get = Iop_GetElem32x2;
               index = mreg >> 4;
               mreg &= 0xf;
               break;
            case 0:
            case 3:
               return False;
            default:
               vassert(0);
         }
         assign(arg_m, unop(dup, binop(get, getDRegI64(mreg), mkU8(index))));
      } else {
         res = newTemp(Ity_I64);
         arg_m = newTemp(Ity_I64);
         arg_n = newTemp(Ity_I64);
         assign(arg_n, getDRegI64(nreg));
         switch(size) {
            case 1:
               dup = Iop_Dup16x4;
               get = Iop_GetElem16x4;
               index = mreg >> 3;
               mreg &= 7;
               break;
            case 2:
               dup = Iop_Dup32x2;
               get = Iop_GetElem32x2;
               index = mreg >> 4;
               mreg &= 0xf;
               break;
            case 0:
            case 3:
               return False;
            default:
               vassert(0);
         }
         assign(arg_m, unop(dup, binop(get, getDRegI64(mreg), mkU8(index))));
      }
      if (INSN(8,8)) {
         switch (size) {
            case 2:
               op = Q ? Iop_Mul32Fx4 : Iop_Mul32Fx2;
               add = Q ? Iop_Add32Fx4 : Iop_Add32Fx2;
               sub = Q ? Iop_Sub32Fx4 : Iop_Sub32Fx2;
               break;
            case 0:
            case 1:
            case 3:
               return False;
            default:
               vassert(0);
         }
      } else {
         switch (size) {
            case 1:
               op = Q ? Iop_Mul16x8 : Iop_Mul16x4;
               add = Q ? Iop_Add16x8 : Iop_Add16x4;
               sub = Q ? Iop_Sub16x8 : Iop_Sub16x4;
               break;
            case 2:
               op = Q ? Iop_Mul32x4 : Iop_Mul32x2;
               add = Q ? Iop_Add32x4 : Iop_Add32x2;
               sub = Q ? Iop_Sub32x4 : Iop_Sub32x2;
               break;
            case 0:
            case 3:
               return False;
            default:
               vassert(0);
         }
      }
      op2 = INSN(10,10) ? sub : add;
      assign(res, binop_w_fake_RM(op, mkexpr(arg_n), mkexpr(arg_m)));
      if (Q)
         putQReg(dreg, binop_w_fake_RM(op2, getQReg(dreg), mkexpr(res)),
                 condT);
      else
         putDRegI64(dreg, binop(op2, getDRegI64(dreg), mkexpr(res)),
                    condT);
      DIP("vml%c.%c%u %c%u, %c%u, d%u[%u]\n", INSN(10,10) ? 's' : 'a',
            INSN(8,8) ? 'f' : 'i', 8 << size,
            Q ? 'q' : 'd', dreg, Q ? 'q' : 'd', nreg, mreg, index);
      return True;
   }

   
   if ((INSN(11,8) & BITS4(1,0,1,1)) == BITS4(0,0,1,0)) {
      IRTemp res, arg_m, arg_n;
      IROp dup, get, op, op2, add, sub;
      if (dreg & 1)
         return False;
      dreg >>= 1;
      res = newTemp(Ity_V128);
      arg_m = newTemp(Ity_I64);
      arg_n = newTemp(Ity_I64);
      assign(arg_n, getDRegI64(nreg));
      switch(size) {
         case 1:
            dup = Iop_Dup16x4;
            get = Iop_GetElem16x4;
            index = mreg >> 3;
            mreg &= 7;
            break;
         case 2:
            dup = Iop_Dup32x2;
            get = Iop_GetElem32x2;
            index = mreg >> 4;
            mreg &= 0xf;
            break;
         case 0:
         case 3:
            return False;
         default:
            vassert(0);
      }
      assign(arg_m, unop(dup, binop(get, getDRegI64(mreg), mkU8(index))));
      switch (size) {
         case 1:
            op = U ? Iop_Mull16Ux4 : Iop_Mull16Sx4;
            add = Iop_Add32x4;
            sub = Iop_Sub32x4;
            break;
         case 2:
            op = U ? Iop_Mull32Ux2 : Iop_Mull32Sx2;
            add = Iop_Add64x2;
            sub = Iop_Sub64x2;
            break;
         case 0:
         case 3:
            return False;
         default:
            vassert(0);
      }
      op2 = INSN(10,10) ? sub : add;
      assign(res, binop(op, mkexpr(arg_n), mkexpr(arg_m)));
      putQReg(dreg, binop(op2, getQReg(dreg), mkexpr(res)), condT);
      DIP("vml%cl.%c%u q%u, d%u, d%u[%u]\n",
          INSN(10,10) ? 's' : 'a', U ? 'u' : 's',
          8 << size, dreg, nreg, mreg, index);
      return True;
   }

   
   if ((INSN(11,8) & BITS4(1,0,1,1)) == BITS4(0,0,1,1) && !U) {
      IRTemp res, arg_m, arg_n, tmp;
      IROp dup, get, op, op2, add, cmp;
      UInt P = INSN(10,10);
      ULong imm;
      if (dreg & 1)
         return False;
      dreg >>= 1;
      res = newTemp(Ity_V128);
      arg_m = newTemp(Ity_I64);
      arg_n = newTemp(Ity_I64);
      assign(arg_n, getDRegI64(nreg));
      switch(size) {
         case 1:
            dup = Iop_Dup16x4;
            get = Iop_GetElem16x4;
            index = mreg >> 3;
            mreg &= 7;
            break;
         case 2:
            dup = Iop_Dup32x2;
            get = Iop_GetElem32x2;
            index = mreg >> 4;
            mreg &= 0xf;
            break;
         case 0:
         case 3:
            return False;
         default:
            vassert(0);
      }
      assign(arg_m, unop(dup, binop(get, getDRegI64(mreg), mkU8(index))));
      switch (size) {
         case 0:
         case 3:
            return False;
         case 1:
            op = Iop_QDMull16Sx4;
            cmp = Iop_CmpEQ16x4;
            add = P ? Iop_QSub32Sx4 : Iop_QAdd32Sx4;
            op2 = P ? Iop_Sub32x4 : Iop_Add32x4;
            imm = 1LL << 15;
            imm = (imm << 16) | imm;
            imm = (imm << 32) | imm;
            break;
         case 2:
            op = Iop_QDMull32Sx2;
            cmp = Iop_CmpEQ32x2;
            add = P ? Iop_QSub64Sx2 : Iop_QAdd64Sx2;
            op2 = P ? Iop_Sub64x2 : Iop_Add64x2;
            imm = 1LL << 31;
            imm = (imm << 32) | imm;
            break;
         default:
            vassert(0);
      }
      res = newTemp(Ity_V128);
      tmp = newTemp(Ity_V128);
      assign(res, binop(op, mkexpr(arg_n), mkexpr(arg_m)));
      assign(tmp, binop(op2, getQReg(dreg), mkexpr(res)));
      setFlag_QC(binop(Iop_And64,
                       binop(cmp, mkexpr(arg_n), mkU64(imm)),
                       binop(cmp, mkexpr(arg_m), mkU64(imm))),
                 mkU64(0),
                 False, condT);
      setFlag_QC(mkexpr(tmp), binop(add, getQReg(dreg), mkexpr(res)),
                 True, condT);
      putQReg(dreg, binop(add, getQReg(dreg), mkexpr(res)), condT);
      DIP("vqdml%cl.s%u q%u, d%u, d%u[%u]\n", P ? 's' : 'a', 8 << size,
          dreg, nreg, mreg, index);
      return True;
   }

   
   if ((INSN(11,8) & BITS4(1,1,1,0)) == BITS4(1,0,0,0)) {
      IRTemp res, arg_m, arg_n;
      IROp dup, get, op;
      if (Q) {
         if ((dreg & 1) || (nreg & 1))
            return False;
         dreg >>= 1;
         nreg >>= 1;
         res = newTemp(Ity_V128);
         arg_m = newTemp(Ity_V128);
         arg_n = newTemp(Ity_V128);
         assign(arg_n, getQReg(nreg));
         switch(size) {
            case 1:
               dup = Iop_Dup16x8;
               get = Iop_GetElem16x4;
               index = mreg >> 3;
               mreg &= 7;
               break;
            case 2:
               dup = Iop_Dup32x4;
               get = Iop_GetElem32x2;
               index = mreg >> 4;
               mreg &= 0xf;
               break;
            case 0:
            case 3:
               return False;
            default:
               vassert(0);
         }
         assign(arg_m, unop(dup, binop(get, getDRegI64(mreg), mkU8(index))));
      } else {
         res = newTemp(Ity_I64);
         arg_m = newTemp(Ity_I64);
         arg_n = newTemp(Ity_I64);
         assign(arg_n, getDRegI64(nreg));
         switch(size) {
            case 1:
               dup = Iop_Dup16x4;
               get = Iop_GetElem16x4;
               index = mreg >> 3;
               mreg &= 7;
               break;
            case 2:
               dup = Iop_Dup32x2;
               get = Iop_GetElem32x2;
               index = mreg >> 4;
               mreg &= 0xf;
               break;
            case 0:
            case 3:
               return False;
            default:
               vassert(0);
         }
         assign(arg_m, unop(dup, binop(get, getDRegI64(mreg), mkU8(index))));
      }
      if (INSN(8,8)) {
         switch (size) {
            case 2:
               op = Q ? Iop_Mul32Fx4 : Iop_Mul32Fx2;
               break;
            case 0:
            case 1:
            case 3:
               return False;
            default:
               vassert(0);
         }
      } else {
         switch (size) {
            case 1:
               op = Q ? Iop_Mul16x8 : Iop_Mul16x4;
               break;
            case 2:
               op = Q ? Iop_Mul32x4 : Iop_Mul32x2;
               break;
            case 0:
            case 3:
               return False;
            default:
               vassert(0);
         }
      }
      assign(res, binop_w_fake_RM(op, mkexpr(arg_n), mkexpr(arg_m)));
      if (Q)
         putQReg(dreg, mkexpr(res), condT);
      else
         putDRegI64(dreg, mkexpr(res), condT);
      DIP("vmul.%c%u %c%u, %c%u, d%u[%u]\n", INSN(8,8) ? 'f' : 'i',
          8 << size, Q ? 'q' : 'd', dreg,
          Q ? 'q' : 'd', nreg, mreg, index);
      return True;
   }

   
   if (INSN(11,8) == BITS4(1,0,1,0)) {
      IRTemp res, arg_m, arg_n;
      IROp dup, get, op;
      if (dreg & 1)
         return False;
      dreg >>= 1;
      res = newTemp(Ity_V128);
      arg_m = newTemp(Ity_I64);
      arg_n = newTemp(Ity_I64);
      assign(arg_n, getDRegI64(nreg));
      switch(size) {
         case 1:
            dup = Iop_Dup16x4;
            get = Iop_GetElem16x4;
            index = mreg >> 3;
            mreg &= 7;
            break;
         case 2:
            dup = Iop_Dup32x2;
            get = Iop_GetElem32x2;
            index = mreg >> 4;
            mreg &= 0xf;
            break;
         case 0:
         case 3:
            return False;
         default:
            vassert(0);
      }
      assign(arg_m, unop(dup, binop(get, getDRegI64(mreg), mkU8(index))));
      switch (size) {
         case 1: op = U ? Iop_Mull16Ux4 : Iop_Mull16Sx4; break;
         case 2: op = U ? Iop_Mull32Ux2 : Iop_Mull32Sx2; break;
         case 0: case 3: return False;
         default: vassert(0);
      }
      assign(res, binop(op, mkexpr(arg_n), mkexpr(arg_m)));
      putQReg(dreg, mkexpr(res), condT);
      DIP("vmull.%c%u q%u, d%u, d%u[%u]\n", U ? 'u' : 's', 8 << size, dreg,
          nreg, mreg, index);
      return True;
   }

   
   if (INSN(11,8) == BITS4(1,0,1,1) && !U) {
      IROp op ,op2, dup, get;
      ULong imm;
      IRTemp arg_m, arg_n;
      if (dreg & 1)
         return False;
      dreg >>= 1;
      arg_m = newTemp(Ity_I64);
      arg_n = newTemp(Ity_I64);
      assign(arg_n, getDRegI64(nreg));
      switch(size) {
         case 1:
            dup = Iop_Dup16x4;
            get = Iop_GetElem16x4;
            index = mreg >> 3;
            mreg &= 7;
            break;
         case 2:
            dup = Iop_Dup32x2;
            get = Iop_GetElem32x2;
            index = mreg >> 4;
            mreg &= 0xf;
            break;
         case 0:
         case 3:
            return False;
         default:
            vassert(0);
      }
      assign(arg_m, unop(dup, binop(get, getDRegI64(mreg), mkU8(index))));
      switch (size) {
         case 0:
         case 3:
            return False;
         case 1:
            op = Iop_QDMull16Sx4;
            op2 = Iop_CmpEQ16x4;
            imm = 1LL << 15;
            imm = (imm << 16) | imm;
            imm = (imm << 32) | imm;
            break;
         case 2:
            op = Iop_QDMull32Sx2;
            op2 = Iop_CmpEQ32x2;
            imm = 1LL << 31;
            imm = (imm << 32) | imm;
            break;
         default:
            vassert(0);
      }
      putQReg(dreg, binop(op, mkexpr(arg_n), mkexpr(arg_m)),
            condT);
      setFlag_QC(binop(Iop_And64,
                       binop(op2, mkexpr(arg_n), mkU64(imm)),
                       binop(op2, mkexpr(arg_m), mkU64(imm))),
                 mkU64(0),
                 False, condT);
      DIP("vqdmull.s%u q%u, d%u, d%u[%u]\n", 8 << size, dreg, nreg, mreg,
          index);
      return True;
   }

   
   if (INSN(11,8) == BITS4(1,1,0,0)) {
      IROp op ,op2, dup, get;
      ULong imm;
      IRTemp res, arg_m, arg_n;
      if (Q) {
         if ((dreg & 1) || (nreg & 1))
            return False;
         dreg >>= 1;
         nreg >>= 1;
         res = newTemp(Ity_V128);
         arg_m = newTemp(Ity_V128);
         arg_n = newTemp(Ity_V128);
         assign(arg_n, getQReg(nreg));
         switch(size) {
            case 1:
               dup = Iop_Dup16x8;
               get = Iop_GetElem16x4;
               index = mreg >> 3;
               mreg &= 7;
               break;
            case 2:
               dup = Iop_Dup32x4;
               get = Iop_GetElem32x2;
               index = mreg >> 4;
               mreg &= 0xf;
               break;
            case 0:
            case 3:
               return False;
            default:
               vassert(0);
         }
         assign(arg_m, unop(dup, binop(get, getDRegI64(mreg), mkU8(index))));
      } else {
         res = newTemp(Ity_I64);
         arg_m = newTemp(Ity_I64);
         arg_n = newTemp(Ity_I64);
         assign(arg_n, getDRegI64(nreg));
         switch(size) {
            case 1:
               dup = Iop_Dup16x4;
               get = Iop_GetElem16x4;
               index = mreg >> 3;
               mreg &= 7;
               break;
            case 2:
               dup = Iop_Dup32x2;
               get = Iop_GetElem32x2;
               index = mreg >> 4;
               mreg &= 0xf;
               break;
            case 0:
            case 3:
               return False;
            default:
               vassert(0);
         }
         assign(arg_m, unop(dup, binop(get, getDRegI64(mreg), mkU8(index))));
      }
      switch (size) {
         case 0:
         case 3:
            return False;
         case 1:
            op = Q ? Iop_QDMulHi16Sx8 : Iop_QDMulHi16Sx4;
            op2 = Q ? Iop_CmpEQ16x8 : Iop_CmpEQ16x4;
            imm = 1LL << 15;
            imm = (imm << 16) | imm;
            imm = (imm << 32) | imm;
            break;
         case 2:
            op = Q ? Iop_QDMulHi32Sx4 : Iop_QDMulHi32Sx2;
            op2 = Q ? Iop_CmpEQ32x4 : Iop_CmpEQ32x2;
            imm = 1LL << 31;
            imm = (imm << 32) | imm;
            break;
         default:
            vassert(0);
      }
      assign(res, binop(op, mkexpr(arg_n), mkexpr(arg_m)));
      setFlag_QC(binop(Q ? Iop_AndV128 : Iop_And64,
                       binop(op2, mkexpr(arg_n),
                                  Q ? mkU128(imm) : mkU64(imm)),
                       binop(op2, mkexpr(arg_m),
                             Q ? mkU128(imm) : mkU64(imm))),
                 Q ? mkU128(0) : mkU64(0),
                 Q, condT);
      if (Q)
         putQReg(dreg, mkexpr(res), condT);
      else
         putDRegI64(dreg, mkexpr(res), condT);
      DIP("vqdmulh.s%u %c%u, %c%u, d%u[%u]\n",
          8 << size, Q ? 'q' : 'd', dreg,
          Q ? 'q' : 'd', nreg, mreg, index);
      return True;
   }

   
   if (INSN(11,8) == BITS4(1,1,0,1)) {
      IROp op ,op2, dup, get;
      ULong imm;
      IRTemp res, arg_m, arg_n;
      if (Q) {
         if ((dreg & 1) || (nreg & 1))
            return False;
         dreg >>= 1;
         nreg >>= 1;
         res = newTemp(Ity_V128);
         arg_m = newTemp(Ity_V128);
         arg_n = newTemp(Ity_V128);
         assign(arg_n, getQReg(nreg));
         switch(size) {
            case 1:
               dup = Iop_Dup16x8;
               get = Iop_GetElem16x4;
               index = mreg >> 3;
               mreg &= 7;
               break;
            case 2:
               dup = Iop_Dup32x4;
               get = Iop_GetElem32x2;
               index = mreg >> 4;
               mreg &= 0xf;
               break;
            case 0:
            case 3:
               return False;
            default:
               vassert(0);
         }
         assign(arg_m, unop(dup, binop(get, getDRegI64(mreg), mkU8(index))));
      } else {
         res = newTemp(Ity_I64);
         arg_m = newTemp(Ity_I64);
         arg_n = newTemp(Ity_I64);
         assign(arg_n, getDRegI64(nreg));
         switch(size) {
            case 1:
               dup = Iop_Dup16x4;
               get = Iop_GetElem16x4;
               index = mreg >> 3;
               mreg &= 7;
               break;
            case 2:
               dup = Iop_Dup32x2;
               get = Iop_GetElem32x2;
               index = mreg >> 4;
               mreg &= 0xf;
               break;
            case 0:
            case 3:
               return False;
            default:
               vassert(0);
         }
         assign(arg_m, unop(dup, binop(get, getDRegI64(mreg), mkU8(index))));
      }
      switch (size) {
         case 0:
         case 3:
            return False;
         case 1:
            op = Q ? Iop_QRDMulHi16Sx8 : Iop_QRDMulHi16Sx4;
            op2 = Q ? Iop_CmpEQ16x8 : Iop_CmpEQ16x4;
            imm = 1LL << 15;
            imm = (imm << 16) | imm;
            imm = (imm << 32) | imm;
            break;
         case 2:
            op = Q ? Iop_QRDMulHi32Sx4 : Iop_QRDMulHi32Sx2;
            op2 = Q ? Iop_CmpEQ32x4 : Iop_CmpEQ32x2;
            imm = 1LL << 31;
            imm = (imm << 32) | imm;
            break;
         default:
            vassert(0);
      }
      assign(res, binop(op, mkexpr(arg_n), mkexpr(arg_m)));
      setFlag_QC(binop(Q ? Iop_AndV128 : Iop_And64,
                       binop(op2, mkexpr(arg_n),
                                  Q ? mkU128(imm) : mkU64(imm)),
                       binop(op2, mkexpr(arg_m),
                                  Q ? mkU128(imm) : mkU64(imm))),
                 Q ? mkU128(0) : mkU64(0),
                 Q, condT);
      if (Q)
         putQReg(dreg, mkexpr(res), condT);
      else
         putDRegI64(dreg, mkexpr(res), condT);
      DIP("vqrdmulh.s%u %c%u, %c%u, d%u[%u]\n",
          8 << size, Q ? 'q' : 'd', dreg,
          Q ? 'q' : 'd', nreg, mreg, index);
      return True;
   }

   return False;
#  undef INSN
}

static
Bool dis_neon_data_2reg_and_shift ( UInt theInstr, IRTemp condT )
{
   UInt A = (theInstr >> 8) & 0xf;
   UInt B = (theInstr >> 6) & 1;
   UInt L = (theInstr >> 7) & 1;
   UInt U = (theInstr >> 24) & 1;
   UInt Q = B;
   UInt imm6 = (theInstr >> 16) & 0x3f;
   UInt shift_imm;
   UInt size = 4;
   UInt tmp;
   UInt mreg = get_neon_m_regno(theInstr);
   UInt dreg = get_neon_d_regno(theInstr);
   ULong imm = 0;
   IROp op, cvt, add = Iop_INVALID, cvt2, op_rev;
   IRTemp reg_m, res, mask;

   if (L == 0 && ((theInstr >> 19) & 7) == 0)
      
      return False;

   tmp = (L << 6) | imm6;
   if (tmp & 0x40) {
      size = 3;
      shift_imm = 64 - imm6;
   } else if (tmp & 0x20) {
      size = 2;
      shift_imm = 64 - imm6;
   } else if (tmp & 0x10) {
      size = 1;
      shift_imm = 32 - imm6;
   } else if (tmp & 0x8) {
      size = 0;
      shift_imm = 16 - imm6;
   } else {
      return False;
   }

   switch (A) {
      case 3:
      case 2:
         
         if (shift_imm > 0) {
            IRExpr *imm_val;
            imm = 1L;
            switch (size) {
               case 0:
                  imm = (imm << 8) | imm;
                  
               case 1:
                  imm = (imm << 16) | imm;
                  
               case 2:
                  imm = (imm << 32) | imm;
                  
               case 3:
                  break;
               default:
                  vassert(0);
            }
            if (Q) {
               reg_m = newTemp(Ity_V128);
               res = newTemp(Ity_V128);
               imm_val = binop(Iop_64HLtoV128, mkU64(imm), mkU64(imm));
               assign(reg_m, getQReg(mreg));
               switch (size) {
                  case 0:
                     add = Iop_Add8x16;
                     op = U ? Iop_ShrN8x16 : Iop_SarN8x16;
                     break;
                  case 1:
                     add = Iop_Add16x8;
                     op = U ? Iop_ShrN16x8 : Iop_SarN16x8;
                     break;
                  case 2:
                     add = Iop_Add32x4;
                     op = U ? Iop_ShrN32x4 : Iop_SarN32x4;
                     break;
                  case 3:
                     add = Iop_Add64x2;
                     op = U ? Iop_ShrN64x2 : Iop_SarN64x2;
                     break;
                  default:
                     vassert(0);
               }
            } else {
               reg_m = newTemp(Ity_I64);
               res = newTemp(Ity_I64);
               imm_val = mkU64(imm);
               assign(reg_m, getDRegI64(mreg));
               switch (size) {
                  case 0:
                     add = Iop_Add8x8;
                     op = U ? Iop_ShrN8x8 : Iop_SarN8x8;
                     break;
                  case 1:
                     add = Iop_Add16x4;
                     op = U ? Iop_ShrN16x4 : Iop_SarN16x4;
                     break;
                  case 2:
                     add = Iop_Add32x2;
                     op = U ? Iop_ShrN32x2 : Iop_SarN32x2;
                     break;
                  case 3:
                     add = Iop_Add64;
                     op = U ? Iop_Shr64 : Iop_Sar64;
                     break;
                  default:
                     vassert(0);
               }
            }
            assign(res,
                   binop(add,
                         binop(op,
                               mkexpr(reg_m),
                               mkU8(shift_imm)),
                         binop(Q ? Iop_AndV128 : Iop_And64,
                               binop(op,
                                     mkexpr(reg_m),
                                     mkU8(shift_imm - 1)),
                               imm_val)));
         } else {
            if (Q) {
               res = newTemp(Ity_V128);
               assign(res, getQReg(mreg));
            } else {
               res = newTemp(Ity_I64);
               assign(res, getDRegI64(mreg));
            }
         }
         if (A == 3) {
            if (Q) {
               putQReg(dreg, binop(add, mkexpr(res), getQReg(dreg)),
                             condT);
            } else {
               putDRegI64(dreg, binop(add, mkexpr(res), getDRegI64(dreg)),
                                condT);
            }
            DIP("vrsra.%c%u %c%u, %c%u, #%u\n",
                U ? 'u' : 's', 8 << size,
                Q ? 'q' : 'd', dreg, Q ? 'q' : 'd', mreg, shift_imm);
         } else {
            if (Q) {
               putQReg(dreg, mkexpr(res), condT);
            } else {
               putDRegI64(dreg, mkexpr(res), condT);
            }
            DIP("vrshr.%c%u %c%u, %c%u, #%u\n", U ? 'u' : 's', 8 << size,
                Q ? 'q' : 'd', dreg, Q ? 'q' : 'd', mreg, shift_imm);
         }
         return True;
      case 1:
      case 0:
         
         if (Q) {
            reg_m = newTemp(Ity_V128);
            assign(reg_m, getQReg(mreg));
            res = newTemp(Ity_V128);
         } else {
            reg_m = newTemp(Ity_I64);
            assign(reg_m, getDRegI64(mreg));
            res = newTemp(Ity_I64);
         }
         if (Q) {
            switch (size) {
               case 0:
                  op = U ? Iop_ShrN8x16 : Iop_SarN8x16;
                  add = Iop_Add8x16;
                  break;
               case 1:
                  op = U ? Iop_ShrN16x8 : Iop_SarN16x8;
                  add = Iop_Add16x8;
                  break;
               case 2:
                  op = U ? Iop_ShrN32x4 : Iop_SarN32x4;
                  add = Iop_Add32x4;
                  break;
               case 3:
                  op = U ? Iop_ShrN64x2 : Iop_SarN64x2;
                  add = Iop_Add64x2;
                  break;
               default:
                  vassert(0);
            }
         } else {
            switch (size) {
               case 0:
                  op =  U ? Iop_ShrN8x8 : Iop_SarN8x8;
                  add = Iop_Add8x8;
                  break;
               case 1:
                  op = U ? Iop_ShrN16x4 : Iop_SarN16x4;
                  add = Iop_Add16x4;
                  break;
               case 2:
                  op = U ? Iop_ShrN32x2 : Iop_SarN32x2;
                  add = Iop_Add32x2;
                  break;
               case 3:
                  op = U ? Iop_Shr64 : Iop_Sar64;
                  add = Iop_Add64;
                  break;
               default:
                  vassert(0);
            }
         }
         assign(res, binop(op, mkexpr(reg_m), mkU8(shift_imm)));
         if (A == 1) {
            if (Q) {
               putQReg(dreg, binop(add, mkexpr(res), getQReg(dreg)),
                             condT);
            } else {
               putDRegI64(dreg, binop(add, mkexpr(res), getDRegI64(dreg)),
                                condT);
            }
            DIP("vsra.%c%u %c%u, %c%u, #%u\n", U ? 'u' : 's', 8 << size,
                  Q ? 'q' : 'd', dreg, Q ? 'q' : 'd', mreg, shift_imm);
         } else {
            if (Q) {
               putQReg(dreg, mkexpr(res), condT);
            } else {
               putDRegI64(dreg, mkexpr(res), condT);
            }
            DIP("vshr.%c%u %c%u, %c%u, #%u\n", U ? 'u' : 's', 8 << size,
                  Q ? 'q' : 'd', dreg, Q ? 'q' : 'd', mreg, shift_imm);
         }
         return True;
      case 4:
         
         if (!U)
            return False;
         if (Q) {
            res = newTemp(Ity_V128);
            mask = newTemp(Ity_V128);
         } else {
            res = newTemp(Ity_I64);
            mask = newTemp(Ity_I64);
         }
         switch (size) {
            case 0: op = Q ? Iop_ShrN8x16 : Iop_ShrN8x8; break;
            case 1: op = Q ? Iop_ShrN16x8 : Iop_ShrN16x4; break;
            case 2: op = Q ? Iop_ShrN32x4 : Iop_ShrN32x2; break;
            case 3: op = Q ? Iop_ShrN64x2 : Iop_Shr64; break;
            default: vassert(0);
         }
         if (Q) {
            assign(mask, binop(op, binop(Iop_64HLtoV128,
                                         mkU64(0xFFFFFFFFFFFFFFFFLL),
                                         mkU64(0xFFFFFFFFFFFFFFFFLL)),
                               mkU8(shift_imm)));
            assign(res, binop(Iop_OrV128,
                              binop(Iop_AndV128,
                                    getQReg(dreg),
                                    unop(Iop_NotV128,
                                         mkexpr(mask))),
                              binop(op,
                                    getQReg(mreg),
                                    mkU8(shift_imm))));
            putQReg(dreg, mkexpr(res), condT);
         } else {
            assign(mask, binop(op, mkU64(0xFFFFFFFFFFFFFFFFLL),
                               mkU8(shift_imm)));
            assign(res, binop(Iop_Or64,
                              binop(Iop_And64,
                                    getDRegI64(dreg),
                                    unop(Iop_Not64,
                                         mkexpr(mask))),
                              binop(op,
                                    getDRegI64(mreg),
                                    mkU8(shift_imm))));
            putDRegI64(dreg, mkexpr(res), condT);
         }
         DIP("vsri.%u %c%u, %c%u, #%u\n",
             8 << size, Q ? 'q' : 'd', dreg,
             Q ? 'q' : 'd', mreg, shift_imm);
         return True;
      case 5:
         if (U) {
            
            shift_imm = 8 * (1 << size) - shift_imm;
            if (Q) {
               res = newTemp(Ity_V128);
               mask = newTemp(Ity_V128);
            } else {
               res = newTemp(Ity_I64);
               mask = newTemp(Ity_I64);
            }
            switch (size) {
               case 0: op = Q ? Iop_ShlN8x16 : Iop_ShlN8x8; break;
               case 1: op = Q ? Iop_ShlN16x8 : Iop_ShlN16x4; break;
               case 2: op = Q ? Iop_ShlN32x4 : Iop_ShlN32x2; break;
               case 3: op = Q ? Iop_ShlN64x2 : Iop_Shl64; break;
               default: vassert(0);
            }
            if (Q) {
               assign(mask, binop(op, binop(Iop_64HLtoV128,
                                            mkU64(0xFFFFFFFFFFFFFFFFLL),
                                            mkU64(0xFFFFFFFFFFFFFFFFLL)),
                                  mkU8(shift_imm)));
               assign(res, binop(Iop_OrV128,
                                 binop(Iop_AndV128,
                                       getQReg(dreg),
                                       unop(Iop_NotV128,
                                            mkexpr(mask))),
                                 binop(op,
                                       getQReg(mreg),
                                       mkU8(shift_imm))));
               putQReg(dreg, mkexpr(res), condT);
            } else {
               assign(mask, binop(op, mkU64(0xFFFFFFFFFFFFFFFFLL),
                                  mkU8(shift_imm)));
               assign(res, binop(Iop_Or64,
                                 binop(Iop_And64,
                                       getDRegI64(dreg),
                                       unop(Iop_Not64,
                                            mkexpr(mask))),
                                 binop(op,
                                       getDRegI64(mreg),
                                       mkU8(shift_imm))));
               putDRegI64(dreg, mkexpr(res), condT);
            }
            DIP("vsli.%u %c%u, %c%u, #%u\n",
                8 << size, Q ? 'q' : 'd', dreg,
                Q ? 'q' : 'd', mreg, shift_imm);
            return True;
         } else {
            
            shift_imm = 8 * (1 << size) - shift_imm;
            if (Q) {
               res = newTemp(Ity_V128);
            } else {
               res = newTemp(Ity_I64);
            }
            switch (size) {
               case 0: op = Q ? Iop_ShlN8x16 : Iop_ShlN8x8; break;
               case 1: op = Q ? Iop_ShlN16x8 : Iop_ShlN16x4; break;
               case 2: op = Q ? Iop_ShlN32x4 : Iop_ShlN32x2; break;
               case 3: op = Q ? Iop_ShlN64x2 : Iop_Shl64; break;
               default: vassert(0);
            }
            assign(res, binop(op, Q ? getQReg(mreg) : getDRegI64(mreg),
                     mkU8(shift_imm)));
            if (Q) {
               putQReg(dreg, mkexpr(res), condT);
            } else {
               putDRegI64(dreg, mkexpr(res), condT);
            }
            DIP("vshl.i%u %c%u, %c%u, #%u\n",
                8 << size, Q ? 'q' : 'd', dreg,
                Q ? 'q' : 'd', mreg, shift_imm);
            return True;
         }
         break;
      case 6:
      case 7:
         
         shift_imm = 8 * (1 << size) - shift_imm;
         if (U) {
            if (A & 1) {
               switch (size) {
                  case 0:
                     op = Q ? Iop_QShlNsatUU8x16 : Iop_QShlNsatUU8x8;
                     op_rev = Q ? Iop_ShrN8x16 : Iop_ShrN8x8;
                     break;
                  case 1:
                     op = Q ? Iop_QShlNsatUU16x8 : Iop_QShlNsatUU16x4;
                     op_rev = Q ? Iop_ShrN16x8 : Iop_ShrN16x4;
                     break;
                  case 2:
                     op = Q ? Iop_QShlNsatUU32x4 : Iop_QShlNsatUU32x2;
                     op_rev = Q ? Iop_ShrN32x4 : Iop_ShrN32x2;
                     break;
                  case 3:
                     op = Q ? Iop_QShlNsatUU64x2 : Iop_QShlNsatUU64x1;
                     op_rev = Q ? Iop_ShrN64x2 : Iop_Shr64;
                     break;
                  default:
                     vassert(0);
               }
               DIP("vqshl.u%u %c%u, %c%u, #%u\n",
                   8 << size,
                   Q ? 'q' : 'd', dreg, Q ? 'q' : 'd', mreg, shift_imm);
            } else {
               switch (size) {
                  case 0:
                     op = Q ? Iop_QShlNsatSU8x16 : Iop_QShlNsatSU8x8;
                     op_rev = Q ? Iop_ShrN8x16 : Iop_ShrN8x8;
                     break;
                  case 1:
                     op = Q ? Iop_QShlNsatSU16x8 : Iop_QShlNsatSU16x4;
                     op_rev = Q ? Iop_ShrN16x8 : Iop_ShrN16x4;
                     break;
                  case 2:
                     op = Q ? Iop_QShlNsatSU32x4 : Iop_QShlNsatSU32x2;
                     op_rev = Q ? Iop_ShrN32x4 : Iop_ShrN32x2;
                     break;
                  case 3:
                     op = Q ? Iop_QShlNsatSU64x2 : Iop_QShlNsatSU64x1;
                     op_rev = Q ? Iop_ShrN64x2 : Iop_Shr64;
                     break;
                  default:
                     vassert(0);
               }
               DIP("vqshlu.s%u %c%u, %c%u, #%u\n",
                   8 << size,
                   Q ? 'q' : 'd', dreg, Q ? 'q' : 'd', mreg, shift_imm);
            }
         } else {
            if (!(A & 1))
               return False;
            switch (size) {
               case 0:
                  op = Q ? Iop_QShlNsatSS8x16 : Iop_QShlNsatSS8x8;
                  op_rev = Q ? Iop_SarN8x16 : Iop_SarN8x8;
                  break;
               case 1:
                  op = Q ? Iop_QShlNsatSS16x8 : Iop_QShlNsatSS16x4;
                  op_rev = Q ? Iop_SarN16x8 : Iop_SarN16x4;
                  break;
               case 2:
                  op = Q ? Iop_QShlNsatSS32x4 : Iop_QShlNsatSS32x2;
                  op_rev = Q ? Iop_SarN32x4 : Iop_SarN32x2;
                  break;
               case 3:
                  op = Q ? Iop_QShlNsatSS64x2 : Iop_QShlNsatSS64x1;
                  op_rev = Q ? Iop_SarN64x2 : Iop_Sar64;
                  break;
               default:
                  vassert(0);
            }
            DIP("vqshl.s%u %c%u, %c%u, #%u\n",
                8 << size,
                Q ? 'q' : 'd', dreg, Q ? 'q' : 'd', mreg, shift_imm);
         }
         if (Q) {
            tmp = newTemp(Ity_V128);
            res = newTemp(Ity_V128);
            reg_m = newTemp(Ity_V128);
            assign(reg_m, getQReg(mreg));
         } else {
            tmp = newTemp(Ity_I64);
            res = newTemp(Ity_I64);
            reg_m = newTemp(Ity_I64);
            assign(reg_m, getDRegI64(mreg));
         }
         assign(res, binop(op, mkexpr(reg_m), mkU8(shift_imm)));
         assign(tmp, binop(op_rev, mkexpr(res), mkU8(shift_imm)));
         setFlag_QC(mkexpr(tmp), mkexpr(reg_m), Q, condT);
         if (Q)
            putQReg(dreg, mkexpr(res), condT);
         else
            putDRegI64(dreg, mkexpr(res), condT);
         return True;
      case 8:
         if (!U) {
            if (L == 1)
               return False;
            size++;
            dreg = ((theInstr >> 18) & 0x10) | ((theInstr >> 12) & 0xF);
            mreg = ((theInstr >> 1) & 0x10) | (theInstr & 0xF);
            if (mreg & 1)
               return False;
            mreg >>= 1;
            if (!B) {
               
               IROp narOp;
               reg_m = newTemp(Ity_V128);
               assign(reg_m, getQReg(mreg));
               res = newTemp(Ity_I64);
               switch (size) {
                  case 1:
                     op = Iop_ShrN16x8;
                     narOp = Iop_NarrowUn16to8x8;
                     break;
                  case 2:
                     op = Iop_ShrN32x4;
                     narOp = Iop_NarrowUn32to16x4;
                     break;
                  case 3:
                     op = Iop_ShrN64x2;
                     narOp = Iop_NarrowUn64to32x2;
                     break;
                  default:
                     vassert(0);
               }
               assign(res, unop(narOp,
                                binop(op,
                                      mkexpr(reg_m),
                                      mkU8(shift_imm))));
               putDRegI64(dreg, mkexpr(res), condT);
               DIP("vshrn.i%u d%u, q%u, #%u\n", 8 << size, dreg, mreg,
                   shift_imm);
               return True;
            } else {
               
               IROp addOp, shOp, narOp;
               IRExpr *imm_val;
               reg_m = newTemp(Ity_V128);
               assign(reg_m, getQReg(mreg));
               res = newTemp(Ity_I64);
               imm = 1L;
               switch (size) {
                  case 0: imm = (imm <<  8) | imm; 
                  case 1: imm = (imm << 16) | imm; 
                  case 2: imm = (imm << 32) | imm; 
                  case 3: break;
                  default: vassert(0);
               }
               imm_val = binop(Iop_64HLtoV128, mkU64(imm), mkU64(imm));
               switch (size) {
                  case 1:
                     addOp = Iop_Add16x8;
                     shOp = Iop_ShrN16x8;
                     narOp = Iop_NarrowUn16to8x8;
                     break;
                  case 2:
                     addOp = Iop_Add32x4;
                     shOp = Iop_ShrN32x4;
                     narOp = Iop_NarrowUn32to16x4;
                     break;
                  case 3:
                     addOp = Iop_Add64x2;
                     shOp = Iop_ShrN64x2;
                     narOp = Iop_NarrowUn64to32x2;
                     break;
                  default:
                     vassert(0);
               }
               assign(res, unop(narOp,
                                binop(addOp,
                                      binop(shOp,
                                            mkexpr(reg_m),
                                            mkU8(shift_imm)),
                                      binop(Iop_AndV128,
                                            binop(shOp,
                                                  mkexpr(reg_m),
                                                  mkU8(shift_imm - 1)),
                                            imm_val))));
               putDRegI64(dreg, mkexpr(res), condT);
               if (shift_imm == 0) {
                  DIP("vmov%u d%u, q%u, #%u\n", 8 << size, dreg, mreg,
                      shift_imm);
               } else {
                  DIP("vrshrn.i%u d%u, q%u, #%u\n", 8 << size, dreg, mreg,
                      shift_imm);
               }
               return True;
            }
         } else {
            
         }
      case 9:
         dreg = ((theInstr >> 18) & 0x10) | ((theInstr >> 12) & 0xF);
         mreg = ((theInstr >>  1) & 0x10) | (theInstr & 0xF);
         if (mreg & 1)
            return False;
         mreg >>= 1;
         size++;
         if ((theInstr >> 8) & 1) {
            switch (size) {
               case 1:
                  op = U ? Iop_ShrN16x8 : Iop_SarN16x8;
                  cvt = U ? Iop_QNarrowUn16Uto8Ux8 : Iop_QNarrowUn16Sto8Sx8;
                  cvt2 = U ? Iop_Widen8Uto16x8 : Iop_Widen8Sto16x8;
                  break;
               case 2:
                  op = U ? Iop_ShrN32x4 : Iop_SarN32x4;
                  cvt = U ? Iop_QNarrowUn32Uto16Ux4 : Iop_QNarrowUn32Sto16Sx4;
                  cvt2 = U ? Iop_Widen16Uto32x4 : Iop_Widen16Sto32x4;
                  break;
               case 3:
                  op = U ? Iop_ShrN64x2 : Iop_SarN64x2;
                  cvt = U ? Iop_QNarrowUn64Uto32Ux2 : Iop_QNarrowUn64Sto32Sx2;
                  cvt2 = U ? Iop_Widen32Uto64x2 : Iop_Widen32Sto64x2;
                  break;
               default:
                  vassert(0);
            }
            DIP("vq%sshrn.%c%u d%u, q%u, #%u\n", B ? "r" : "",
                U ? 'u' : 's', 8 << size, dreg, mreg, shift_imm);
         } else {
            vassert(U);
            switch (size) {
               case 1:
                  op = Iop_SarN16x8;
                  cvt = Iop_QNarrowUn16Sto8Ux8;
                  cvt2 = Iop_Widen8Uto16x8;
                  break;
               case 2:
                  op = Iop_SarN32x4;
                  cvt = Iop_QNarrowUn32Sto16Ux4;
                  cvt2 = Iop_Widen16Uto32x4;
                  break;
               case 3:
                  op = Iop_SarN64x2;
                  cvt = Iop_QNarrowUn64Sto32Ux2;
                  cvt2 = Iop_Widen32Uto64x2;
                  break;
               default:
                  vassert(0);
            }
            DIP("vq%sshrun.s%u d%u, q%u, #%u\n", B ? "r" : "",
                8 << size, dreg, mreg, shift_imm);
         }
         if (B) {
            if (shift_imm > 0) {
               imm = 1;
               switch (size) {
                  case 1: imm = (imm << 16) | imm; 
                  case 2: imm = (imm << 32) | imm; 
                  case 3: break;
                  case 0: default: vassert(0);
               }
               switch (size) {
                  case 1: add = Iop_Add16x8; break;
                  case 2: add = Iop_Add32x4; break;
                  case 3: add = Iop_Add64x2; break;
                  case 0: default: vassert(0);
               }
            }
         }
         reg_m = newTemp(Ity_V128);
         res = newTemp(Ity_V128);
         assign(reg_m, getQReg(mreg));
         if (B) {
            
            assign(res, binop(add,
                              binop(op, mkexpr(reg_m), mkU8(shift_imm)),
                              binop(Iop_AndV128,
                                    binop(op,
                                          mkexpr(reg_m),
                                          mkU8(shift_imm - 1)),
                                    mkU128(imm))));
         } else {
            
            assign(res, binop(op, mkexpr(reg_m), mkU8(shift_imm)));
         }
         setFlag_QC(unop(cvt2, unop(cvt, mkexpr(res))), mkexpr(res),
                    True, condT);
         putDRegI64(dreg, unop(cvt, mkexpr(res)), condT);
         return True;
      case 10:
         if (B)
            return False;
         if (dreg & 1)
            return False;
         dreg >>= 1;
         shift_imm = (8 << size) - shift_imm;
         res = newTemp(Ity_V128);
         switch (size) {
            case 0:
               op = Iop_ShlN16x8;
               cvt = U ? Iop_Widen8Uto16x8 : Iop_Widen8Sto16x8;
               break;
            case 1:
               op = Iop_ShlN32x4;
               cvt = U ? Iop_Widen16Uto32x4 : Iop_Widen16Sto32x4;
               break;
            case 2:
               op = Iop_ShlN64x2;
               cvt = U ? Iop_Widen32Uto64x2 : Iop_Widen32Sto64x2;
               break;
            case 3:
               return False;
            default:
               vassert(0);
         }
         assign(res, binop(op, unop(cvt, getDRegI64(mreg)), mkU8(shift_imm)));
         putQReg(dreg, mkexpr(res), condT);
         if (shift_imm == 0) {
            DIP("vmovl.%c%u q%u, d%u\n", U ? 'u' : 's', 8 << size,
                dreg, mreg);
         } else {
            DIP("vshll.%c%u q%u, d%u, #%u\n", U ? 'u' : 's', 8 << size,
                dreg, mreg, shift_imm);
         }
         return True;
      case 14:
      case 15:
         
         if ((theInstr >> 8) & 1) {
            if (U) {
               op = Q ? Iop_F32ToFixed32Ux4_RZ : Iop_F32ToFixed32Ux2_RZ;
            } else {
               op = Q ? Iop_F32ToFixed32Sx4_RZ : Iop_F32ToFixed32Sx2_RZ;
            }
            DIP("vcvt.%c32.f32 %c%u, %c%u, #%u\n", U ? 'u' : 's',
                Q ? 'q' : 'd', dreg, Q ? 'q' : 'd', mreg,
                64 - ((theInstr >> 16) & 0x3f));
         } else {
            if (U) {
               op = Q ? Iop_Fixed32UToF32x4_RN : Iop_Fixed32UToF32x2_RN;
            } else {
               op = Q ? Iop_Fixed32SToF32x4_RN : Iop_Fixed32SToF32x2_RN;
            }
            DIP("vcvt.f32.%c32 %c%u, %c%u, #%u\n", U ? 'u' : 's',
                Q ? 'q' : 'd', dreg, Q ? 'q' : 'd', mreg,
                64 - ((theInstr >> 16) & 0x3f));
         }
         if (((theInstr >> 21) & 1) == 0)
            return False;
         if (Q) {
            putQReg(dreg, binop(op, getQReg(mreg),
                     mkU8(64 - ((theInstr >> 16) & 0x3f))), condT);
         } else {
            putDRegI64(dreg, binop(op, getDRegI64(mreg),
                       mkU8(64 - ((theInstr >> 16) & 0x3f))), condT);
         }
         return True;
      default:
         return False;

   }
   return False;
}

static
Bool dis_neon_data_2reg_misc ( UInt theInstr, IRTemp condT )
{
   UInt A = (theInstr >> 16) & 3;
   UInt B = (theInstr >> 6) & 0x1f;
   UInt Q = (theInstr >> 6) & 1;
   UInt U = (theInstr >> 24) & 1;
   UInt size = (theInstr >> 18) & 3;
   UInt dreg = get_neon_d_regno(theInstr);
   UInt mreg = get_neon_m_regno(theInstr);
   UInt F = (theInstr >> 10) & 1;
   IRTemp arg_d = IRTemp_INVALID;
   IRTemp arg_m = IRTemp_INVALID;
   IRTemp res = IRTemp_INVALID;
   switch (A) {
      case 0:
         if (Q) {
            arg_m = newTemp(Ity_V128);
            res = newTemp(Ity_V128);
            assign(arg_m, getQReg(mreg));
         } else {
            arg_m = newTemp(Ity_I64);
            res = newTemp(Ity_I64);
            assign(arg_m, getDRegI64(mreg));
         }
         switch (B >> 1) {
            case 0: {
               
               IROp op;
               switch (size) {
                  case 0:
                     op = Q ? Iop_Reverse8sIn64_x2 : Iop_Reverse8sIn64_x1;
                     break;
                  case 1:
                     op = Q ? Iop_Reverse16sIn64_x2 : Iop_Reverse16sIn64_x1;
                     break;
                  case 2:
                     op = Q ? Iop_Reverse32sIn64_x2 : Iop_Reverse32sIn64_x1;
                     break;
                  case 3:
                     return False;
                  default:
                     vassert(0);
               }
               assign(res, unop(op, mkexpr(arg_m)));
               DIP("vrev64.%u %c%u, %c%u\n", 8 << size,
                   Q ? 'q' : 'd', dreg, Q ? 'q' : 'd', mreg);
               break;
            }
            case 1: {
               
               IROp op;
               switch (size) {
                  case 0:
                     op = Q ? Iop_Reverse8sIn32_x4 : Iop_Reverse8sIn32_x2;
                     break;
                  case 1:
                     op = Q ? Iop_Reverse16sIn32_x4 : Iop_Reverse16sIn32_x2;
                     break;
                  case 2:
                  case 3:
                     return False;
                  default:
                     vassert(0);
               }
               assign(res, unop(op, mkexpr(arg_m)));
               DIP("vrev32.%u %c%u, %c%u\n", 8 << size,
                   Q ? 'q' : 'd', dreg, Q ? 'q' : 'd', mreg);
               break;
            }
            case 2: {
               
               IROp op;
               switch (size) {
                  case 0:
                     op = Q ? Iop_Reverse8sIn16_x8 : Iop_Reverse8sIn16_x4;
                     break;
                  case 1:
                  case 2:
                  case 3:
                     return False;
                  default:
                     vassert(0);
               }
               assign(res, unop(op, mkexpr(arg_m)));
               DIP("vrev16.%u %c%u, %c%u\n", 8 << size,
                   Q ? 'q' : 'd', dreg, Q ? 'q' : 'd', mreg);
               break;
            }
            case 3:
               return False;
            case 4:
            case 5: {
               
               IROp op;
               U = (theInstr >> 7) & 1;
               if (Q) {
                  switch (size) {
                     case 0: op = U ? Iop_PwAddL8Ux16 : Iop_PwAddL8Sx16; break;
                     case 1: op = U ? Iop_PwAddL16Ux8 : Iop_PwAddL16Sx8; break;
                     case 2: op = U ? Iop_PwAddL32Ux4 : Iop_PwAddL32Sx4; break;
                     case 3: return False;
                     default: vassert(0);
                  }
               } else {
                  switch (size) {
                     case 0: op = U ? Iop_PwAddL8Ux8  : Iop_PwAddL8Sx8;  break;
                     case 1: op = U ? Iop_PwAddL16Ux4 : Iop_PwAddL16Sx4; break;
                     case 2: op = U ? Iop_PwAddL32Ux2 : Iop_PwAddL32Sx2; break;
                     case 3: return False;
                     default: vassert(0);
                  }
               }
               assign(res, unop(op, mkexpr(arg_m)));
               DIP("vpaddl.%c%u %c%u, %c%u\n", U ? 'u' : 's', 8 << size,
                   Q ? 'q' : 'd', dreg, Q ? 'q' : 'd', mreg);
               break;
            }
            case 6:
            case 7:
               return False;
            case 8: {
               
               IROp op;
               switch (size) {
                  case 0: op = Q ? Iop_Cls8x16 : Iop_Cls8x8; break;
                  case 1: op = Q ? Iop_Cls16x8 : Iop_Cls16x4; break;
                  case 2: op = Q ? Iop_Cls32x4 : Iop_Cls32x2; break;
                  case 3: return False;
                  default: vassert(0);
               }
               assign(res, unop(op, mkexpr(arg_m)));
               DIP("vcls.s%u %c%u, %c%u\n", 8 << size, Q ? 'q' : 'd', dreg,
                   Q ? 'q' : 'd', mreg);
               break;
            }
            case 9: {
               
               IROp op;
               switch (size) {
                  case 0: op = Q ? Iop_Clz8x16 : Iop_Clz8x8; break;
                  case 1: op = Q ? Iop_Clz16x8 : Iop_Clz16x4; break;
                  case 2: op = Q ? Iop_Clz32x4 : Iop_Clz32x2; break;
                  case 3: return False;
                  default: vassert(0);
               }
               assign(res, unop(op, mkexpr(arg_m)));
               DIP("vclz.i%u %c%u, %c%u\n", 8 << size, Q ? 'q' : 'd', dreg,
                   Q ? 'q' : 'd', mreg);
               break;
            }
            case 10:
               
               assign(res, unop(Q ? Iop_Cnt8x16 : Iop_Cnt8x8, mkexpr(arg_m)));
               DIP("vcnt.8 %c%u, %c%u\n", Q ? 'q' : 'd', dreg, Q ? 'q' : 'd',
                   mreg);
               break;
            case 11:
               
               if (Q)
                  assign(res, unop(Iop_NotV128, mkexpr(arg_m)));
               else
                  assign(res, unop(Iop_Not64, mkexpr(arg_m)));
               DIP("vmvn %c%u, %c%u\n", Q ? 'q' : 'd', dreg, Q ? 'q' : 'd',
                   mreg);
               break;
            case 12:
            case 13: {
               
               IROp op, add_op;
               U = (theInstr >> 7) & 1;
               if (Q) {
                  switch (size) {
                     case 0:
                        op = U ? Iop_PwAddL8Ux16 : Iop_PwAddL8Sx16;
                        add_op = Iop_Add16x8;
                        break;
                     case 1:
                        op = U ? Iop_PwAddL16Ux8 : Iop_PwAddL16Sx8;
                        add_op = Iop_Add32x4;
                        break;
                     case 2:
                        op = U ? Iop_PwAddL32Ux4 : Iop_PwAddL32Sx4;
                        add_op = Iop_Add64x2;
                        break;
                     case 3:
                        return False;
                     default:
                        vassert(0);
                  }
               } else {
                  switch (size) {
                     case 0:
                        op = U ? Iop_PwAddL8Ux8 : Iop_PwAddL8Sx8;
                        add_op = Iop_Add16x4;
                        break;
                     case 1:
                        op = U ? Iop_PwAddL16Ux4 : Iop_PwAddL16Sx4;
                        add_op = Iop_Add32x2;
                        break;
                     case 2:
                        op = U ? Iop_PwAddL32Ux2 : Iop_PwAddL32Sx2;
                        add_op = Iop_Add64;
                        break;
                     case 3:
                        return False;
                     default:
                        vassert(0);
                  }
               }
               if (Q) {
                  arg_d = newTemp(Ity_V128);
                  assign(arg_d, getQReg(dreg));
               } else {
                  arg_d = newTemp(Ity_I64);
                  assign(arg_d, getDRegI64(dreg));
               }
               assign(res, binop(add_op, unop(op, mkexpr(arg_m)),
                                         mkexpr(arg_d)));
               DIP("vpadal.%c%u %c%u, %c%u\n", U ? 'u' : 's', 8 << size,
                   Q ? 'q' : 'd', dreg, Q ? 'q' : 'd', mreg);
               break;
            }
            case 14: {
               
               IROp op_sub, op_qsub, op_cmp;
               IRTemp mask, tmp;
               IRExpr *zero1, *zero2;
               IRExpr *neg, *neg2;
               if (Q) {
                  zero1 = binop(Iop_64HLtoV128, mkU64(0), mkU64(0));
                  zero2 = binop(Iop_64HLtoV128, mkU64(0), mkU64(0));
                  mask = newTemp(Ity_V128);
                  tmp = newTemp(Ity_V128);
               } else {
                  zero1 = mkU64(0);
                  zero2 = mkU64(0);
                  mask = newTemp(Ity_I64);
                  tmp = newTemp(Ity_I64);
               }
               switch (size) {
                  case 0:
                     op_sub = Q ? Iop_Sub8x16 : Iop_Sub8x8;
                     op_qsub = Q ? Iop_QSub8Sx16 : Iop_QSub8Sx8;
                     op_cmp = Q ? Iop_CmpGT8Sx16 : Iop_CmpGT8Sx8;
                     break;
                  case 1:
                     op_sub = Q ? Iop_Sub16x8 : Iop_Sub16x4;
                     op_qsub = Q ? Iop_QSub16Sx8 : Iop_QSub16Sx4;
                     op_cmp = Q ? Iop_CmpGT16Sx8 : Iop_CmpGT16Sx4;
                     break;
                  case 2:
                     op_sub = Q ? Iop_Sub32x4 : Iop_Sub32x2;
                     op_qsub = Q ? Iop_QSub32Sx4 : Iop_QSub32Sx2;
                     op_cmp = Q ? Iop_CmpGT32Sx4 : Iop_CmpGT32Sx2;
                     break;
                  case 3:
                     return False;
                  default:
                     vassert(0);
               }
               assign(mask, binop(op_cmp, mkexpr(arg_m), zero1));
               neg = binop(op_qsub, zero2, mkexpr(arg_m));
               neg2 = binop(op_sub, zero2, mkexpr(arg_m));
               assign(res, binop(Q ? Iop_OrV128 : Iop_Or64,
                                 binop(Q ? Iop_AndV128 : Iop_And64,
                                       mkexpr(mask),
                                       mkexpr(arg_m)),
                                 binop(Q ? Iop_AndV128 : Iop_And64,
                                       unop(Q ? Iop_NotV128 : Iop_Not64,
                                            mkexpr(mask)),
                                       neg)));
               assign(tmp, binop(Q ? Iop_OrV128 : Iop_Or64,
                                 binop(Q ? Iop_AndV128 : Iop_And64,
                                       mkexpr(mask),
                                       mkexpr(arg_m)),
                                 binop(Q ? Iop_AndV128 : Iop_And64,
                                       unop(Q ? Iop_NotV128 : Iop_Not64,
                                            mkexpr(mask)),
                                       neg2)));
               setFlag_QC(mkexpr(res), mkexpr(tmp), Q, condT);
               DIP("vqabs.s%u %c%u, %c%u\n", 8 << size, Q ? 'q' : 'd', dreg,
                   Q ? 'q' : 'd', mreg);
               break;
            }
            case 15: {
               
               IROp op, op2;
               IRExpr *zero;
               if (Q) {
                  zero = binop(Iop_64HLtoV128, mkU64(0), mkU64(0));
               } else {
                  zero = mkU64(0);
               }
               switch (size) {
                  case 0:
                     op = Q ? Iop_QSub8Sx16 : Iop_QSub8Sx8;
                     op2 = Q ? Iop_Sub8x16 : Iop_Sub8x8;
                     break;
                  case 1:
                     op = Q ? Iop_QSub16Sx8 : Iop_QSub16Sx4;
                     op2 = Q ? Iop_Sub16x8 : Iop_Sub16x4;
                     break;
                  case 2:
                     op = Q ? Iop_QSub32Sx4 : Iop_QSub32Sx2;
                     op2 = Q ? Iop_Sub32x4 : Iop_Sub32x2;
                     break;
                  case 3:
                     return False;
                  default:
                     vassert(0);
               }
               assign(res, binop(op, zero, mkexpr(arg_m)));
               setFlag_QC(mkexpr(res), binop(op2, zero, mkexpr(arg_m)),
                          Q, condT);
               DIP("vqneg.s%u %c%u, %c%u\n", 8 << size, Q ? 'q' : 'd', dreg,
                   Q ? 'q' : 'd', mreg);
               break;
            }
            default:
               vassert(0);
         }
         if (Q) {
            putQReg(dreg, mkexpr(res), condT);
         } else {
            putDRegI64(dreg, mkexpr(res), condT);
         }
         return True;
      case 1:
         if (Q) {
            arg_m = newTemp(Ity_V128);
            res = newTemp(Ity_V128);
            assign(arg_m, getQReg(mreg));
         } else {
            arg_m = newTemp(Ity_I64);
            res = newTemp(Ity_I64);
            assign(arg_m, getDRegI64(mreg));
         }
         switch ((B >> 1) & 0x7) {
            case 0: {
               
               IRExpr *zero;
               IROp op;
               if (Q) {
                  zero = binop(Iop_64HLtoV128, mkU64(0), mkU64(0));
               } else {
                  zero = mkU64(0);
               }
               if (F) {
                  switch (size) {
                     case 0: case 1: case 3: return False;
                     case 2: op = Q ? Iop_CmpGT32Fx4 : Iop_CmpGT32Fx2; break;
                     default: vassert(0);
                  }
               } else {
                  switch (size) {
                     case 0: op = Q ? Iop_CmpGT8Sx16 : Iop_CmpGT8Sx8; break;
                     case 1: op = Q ? Iop_CmpGT16Sx8 : Iop_CmpGT16Sx4; break;
                     case 2: op = Q ? Iop_CmpGT32Sx4 : Iop_CmpGT32Sx2; break;
                     case 3: return False;
                     default: vassert(0);
                  }
               }
               assign(res, binop(op, mkexpr(arg_m), zero));
               DIP("vcgt.%c%u %c%u, %c%u, #0\n", F ? 'f' : 's', 8 << size,
                   Q ? 'q' : 'd', dreg, Q ? 'q' : 'd', mreg);
               break;
            }
            case 1: {
               
               IROp op;
               IRExpr *zero;
               if (Q) {
                  zero = binop(Iop_64HLtoV128, mkU64(0), mkU64(0));
               } else {
                  zero = mkU64(0);
               }
               if (F) {
                  switch (size) {
                     case 0: case 1: case 3: return False;
                     case 2: op = Q ? Iop_CmpGE32Fx4 : Iop_CmpGE32Fx2; break;
                     default: vassert(0);
                  }
                  assign(res, binop(op, mkexpr(arg_m), zero));
               } else {
                  switch (size) {
                     case 0: op = Q ? Iop_CmpGT8Sx16 : Iop_CmpGT8Sx8; break;
                     case 1: op = Q ? Iop_CmpGT16Sx8 : Iop_CmpGT16Sx4; break;
                     case 2: op = Q ? Iop_CmpGT32Sx4 : Iop_CmpGT32Sx2; break;
                     case 3: return False;
                     default: vassert(0);
                  }
                  assign(res, unop(Q ? Iop_NotV128 : Iop_Not64,
                                   binop(op, zero, mkexpr(arg_m))));
               }
               DIP("vcge.%c%u %c%u, %c%u, #0\n", F ? 'f' : 's', 8 << size,
                   Q ? 'q' : 'd', dreg, Q ? 'q' : 'd', mreg);
               break;
            }
            case 2: {
               
               IROp op;
               IRExpr *zero;
               if (F) {
                  if (Q) {
                     zero = binop(Iop_64HLtoV128, mkU64(0), mkU64(0));
                  } else {
                     zero = mkU64(0);
                  }
                  switch (size) {
                     case 0: case 1: case 3: return False;
                     case 2: op = Q ? Iop_CmpEQ32Fx4 : Iop_CmpEQ32Fx2; break;
                     default: vassert(0);
                  }
                  assign(res, binop(op, zero, mkexpr(arg_m)));
               } else {
                  switch (size) {
                     case 0: op = Q ? Iop_CmpNEZ8x16 : Iop_CmpNEZ8x8; break;
                     case 1: op = Q ? Iop_CmpNEZ16x8 : Iop_CmpNEZ16x4; break;
                     case 2: op = Q ? Iop_CmpNEZ32x4 : Iop_CmpNEZ32x2; break;
                     case 3: return False;
                     default: vassert(0);
                  }
                  assign(res, unop(Q ? Iop_NotV128 : Iop_Not64,
                                   unop(op, mkexpr(arg_m))));
               }
               DIP("vceq.%c%u %c%u, %c%u, #0\n", F ? 'f' : 'i', 8 << size,
                   Q ? 'q' : 'd', dreg, Q ? 'q' : 'd', mreg);
               break;
            }
            case 3: {
               
               IRExpr *zero;
               IROp op;
               if (Q) {
                  zero = binop(Iop_64HLtoV128, mkU64(0), mkU64(0));
               } else {
                  zero = mkU64(0);
               }
               if (F) {
                  switch (size) {
                     case 0: case 1: case 3: return False;
                     case 2: op = Q ? Iop_CmpGE32Fx4 : Iop_CmpGE32Fx2; break;
                     default: vassert(0);
                  }
                  assign(res, binop(op, zero, mkexpr(arg_m)));
               } else {
                  switch (size) {
                     case 0: op = Q ? Iop_CmpGT8Sx16 : Iop_CmpGT8Sx8; break;
                     case 1: op = Q ? Iop_CmpGT16Sx8 : Iop_CmpGT16Sx4; break;
                     case 2: op = Q ? Iop_CmpGT32Sx4 : Iop_CmpGT32Sx2; break;
                     case 3: return False;
                     default: vassert(0);
                  }
                  assign(res, unop(Q ? Iop_NotV128 : Iop_Not64,
                                   binop(op, mkexpr(arg_m), zero)));
               }
               DIP("vcle.%c%u %c%u, %c%u, #0\n", F ? 'f' : 's', 8 << size,
                   Q ? 'q' : 'd', dreg, Q ? 'q' : 'd', mreg);
               break;
            }
            case 4: {
               
               IROp op;
               IRExpr *zero;
               if (Q) {
                  zero = binop(Iop_64HLtoV128, mkU64(0), mkU64(0));
               } else {
                  zero = mkU64(0);
               }
               if (F) {
                  switch (size) {
                     case 0: case 1: case 3: return False;
                     case 2: op = Q ? Iop_CmpGT32Fx4 : Iop_CmpGT32Fx2; break;
                     default: vassert(0);
                  }
                  assign(res, binop(op, zero, mkexpr(arg_m)));
               } else {
                  switch (size) {
                     case 0: op = Q ? Iop_CmpGT8Sx16 : Iop_CmpGT8Sx8; break;
                     case 1: op = Q ? Iop_CmpGT16Sx8 : Iop_CmpGT16Sx4; break;
                     case 2: op = Q ? Iop_CmpGT32Sx4 : Iop_CmpGT32Sx2; break;
                     case 3: return False;
                     default: vassert(0);
                  }
                  assign(res, binop(op, zero, mkexpr(arg_m)));
               }
               DIP("vclt.%c%u %c%u, %c%u, #0\n", F ? 'f' : 's', 8 << size,
                   Q ? 'q' : 'd', dreg, Q ? 'q' : 'd', mreg);
               break;
            }
            case 5:
               return False;
            case 6: {
               
               if (!F) {
                  IROp op;
                  switch(size) {
                     case 0: op = Q ? Iop_Abs8x16 : Iop_Abs8x8; break;
                     case 1: op = Q ? Iop_Abs16x8 : Iop_Abs16x4; break;
                     case 2: op = Q ? Iop_Abs32x4 : Iop_Abs32x2; break;
                     case 3: return False;
                     default: vassert(0);
                  }
                  assign(res, unop(op, mkexpr(arg_m)));
               } else {
                  assign(res, unop(Q ? Iop_Abs32Fx4 : Iop_Abs32Fx2,
                                   mkexpr(arg_m)));
               }
               DIP("vabs.%c%u %c%u, %c%u\n",
                   F ? 'f' : 's', 8 << size, Q ? 'q' : 'd', dreg,
                   Q ? 'q' : 'd', mreg);
               break;
            }
            case 7: {
               
               IROp op;
               IRExpr *zero;
               if (F) {
                  switch (size) {
                     case 0: case 1: case 3: return False;
                     case 2: op = Q ? Iop_Neg32Fx4 : Iop_Neg32Fx2; break;
                     default: vassert(0);
                  }
                  assign(res, unop(op, mkexpr(arg_m)));
               } else {
                  if (Q) {
                     zero = binop(Iop_64HLtoV128, mkU64(0), mkU64(0));
                  } else {
                     zero = mkU64(0);
                  }
                  switch (size) {
                     case 0: op = Q ? Iop_Sub8x16 : Iop_Sub8x8; break;
                     case 1: op = Q ? Iop_Sub16x8 : Iop_Sub16x4; break;
                     case 2: op = Q ? Iop_Sub32x4 : Iop_Sub32x2; break;
                     case 3: return False;
                     default: vassert(0);
                  }
                  assign(res, binop(op, zero, mkexpr(arg_m)));
               }
               DIP("vneg.%c%u %c%u, %c%u\n",
                   F ? 'f' : 's', 8 << size, Q ? 'q' : 'd', dreg,
                   Q ? 'q' : 'd', mreg);
               break;
            }
            default:
               vassert(0);
         }
         if (Q) {
            putQReg(dreg, mkexpr(res), condT);
         } else {
            putDRegI64(dreg, mkexpr(res), condT);
         }
         return True;
      case 2:
         if ((B >> 1) == 0) {
            
            if (Q) {
               arg_m = newTemp(Ity_V128);
               assign(arg_m, getQReg(mreg));
               putQReg(mreg, getQReg(dreg), condT);
               putQReg(dreg, mkexpr(arg_m), condT);
            } else {
               arg_m = newTemp(Ity_I64);
               assign(arg_m, getDRegI64(mreg));
               putDRegI64(mreg, getDRegI64(dreg), condT);
               putDRegI64(dreg, mkexpr(arg_m), condT);
            }
            DIP("vswp %c%u, %c%u\n",
                Q ? 'q' : 'd', dreg, Q ? 'q' : 'd', mreg);
            return True;
         } else if ((B >> 1) == 1) {
            
            IROp op_odd = Iop_INVALID, op_even = Iop_INVALID;
            IRTemp old_m, old_d, new_d, new_m;
            if (Q) {
               old_m = newTemp(Ity_V128);
               old_d = newTemp(Ity_V128);
               new_m = newTemp(Ity_V128);
               new_d = newTemp(Ity_V128);
               assign(old_m, getQReg(mreg));
               assign(old_d, getQReg(dreg));
            } else {
               old_m = newTemp(Ity_I64);
               old_d = newTemp(Ity_I64);
               new_m = newTemp(Ity_I64);
               new_d = newTemp(Ity_I64);
               assign(old_m, getDRegI64(mreg));
               assign(old_d, getDRegI64(dreg));
            }
            if (Q) {
               switch (size) {
                  case 0:
                     op_odd  = Iop_InterleaveOddLanes8x16;
                     op_even = Iop_InterleaveEvenLanes8x16;
                     break;
                  case 1:
                     op_odd  = Iop_InterleaveOddLanes16x8;
                     op_even = Iop_InterleaveEvenLanes16x8;
                     break;
                  case 2:
                     op_odd  = Iop_InterleaveOddLanes32x4;
                     op_even = Iop_InterleaveEvenLanes32x4;
                     break;
                  case 3:
                     return False;
                  default:
                     vassert(0);
               }
            } else {
               switch (size) {
                  case 0:
                     op_odd  = Iop_InterleaveOddLanes8x8;
                     op_even = Iop_InterleaveEvenLanes8x8;
                     break;
                  case 1:
                     op_odd  = Iop_InterleaveOddLanes16x4;
                     op_even = Iop_InterleaveEvenLanes16x4;
                     break;
                  case 2:
                     op_odd  = Iop_InterleaveHI32x2;
                     op_even = Iop_InterleaveLO32x2;
                     break;
                  case 3:
                     return False;
                  default:
                     vassert(0);
               }
            }
            assign(new_d, binop(op_even, mkexpr(old_m), mkexpr(old_d)));
            assign(new_m, binop(op_odd, mkexpr(old_m), mkexpr(old_d)));
            if (Q) {
               putQReg(dreg, mkexpr(new_d), condT);
               putQReg(mreg, mkexpr(new_m), condT);
            } else {
               putDRegI64(dreg, mkexpr(new_d), condT);
               putDRegI64(mreg, mkexpr(new_m), condT);
            }
            DIP("vtrn.%u %c%u, %c%u\n",
                8 << size, Q ? 'q' : 'd', dreg, Q ? 'q' : 'd', mreg);
            return True;
         } else if ((B >> 1) == 2) {
            
            IROp op_even, op_odd;
            IRTemp old_m, old_d, new_m, new_d;
            if (!Q && size == 2)
               return False;
            if (Q) {
               old_m = newTemp(Ity_V128);
               old_d = newTemp(Ity_V128);
               new_m = newTemp(Ity_V128);
               new_d = newTemp(Ity_V128);
               assign(old_m, getQReg(mreg));
               assign(old_d, getQReg(dreg));
            } else {
               old_m = newTemp(Ity_I64);
               old_d = newTemp(Ity_I64);
               new_m = newTemp(Ity_I64);
               new_d = newTemp(Ity_I64);
               assign(old_m, getDRegI64(mreg));
               assign(old_d, getDRegI64(dreg));
            }
            switch (size) {
               case 0:
                  op_odd  = Q ? Iop_CatOddLanes8x16 : Iop_CatOddLanes8x8;
                  op_even = Q ? Iop_CatEvenLanes8x16 : Iop_CatEvenLanes8x8;
                  break;
               case 1:
                  op_odd  = Q ? Iop_CatOddLanes16x8 : Iop_CatOddLanes16x4;
                  op_even = Q ? Iop_CatEvenLanes16x8 : Iop_CatEvenLanes16x4;
                  break;
               case 2:
                  op_odd  = Iop_CatOddLanes32x4;
                  op_even = Iop_CatEvenLanes32x4;
                  break;
               case 3:
                  return False;
               default:
                  vassert(0);
            }
            assign(new_d, binop(op_even, mkexpr(old_m), mkexpr(old_d)));
            assign(new_m, binop(op_odd,  mkexpr(old_m), mkexpr(old_d)));
            if (Q) {
               putQReg(dreg, mkexpr(new_d), condT);
               putQReg(mreg, mkexpr(new_m), condT);
            } else {
               putDRegI64(dreg, mkexpr(new_d), condT);
               putDRegI64(mreg, mkexpr(new_m), condT);
            }
            DIP("vuzp.%u %c%u, %c%u\n",
                8 << size, Q ? 'q' : 'd', dreg, Q ? 'q' : 'd', mreg);
            return True;
         } else if ((B >> 1) == 3) {
            
            IROp op_lo, op_hi;
            IRTemp old_m, old_d, new_m, new_d;
            if (!Q && size == 2)
               return False;
            if (Q) {
               old_m = newTemp(Ity_V128);
               old_d = newTemp(Ity_V128);
               new_m = newTemp(Ity_V128);
               new_d = newTemp(Ity_V128);
               assign(old_m, getQReg(mreg));
               assign(old_d, getQReg(dreg));
            } else {
               old_m = newTemp(Ity_I64);
               old_d = newTemp(Ity_I64);
               new_m = newTemp(Ity_I64);
               new_d = newTemp(Ity_I64);
               assign(old_m, getDRegI64(mreg));
               assign(old_d, getDRegI64(dreg));
            }
            switch (size) {
               case 0:
                  op_hi = Q ? Iop_InterleaveHI8x16 : Iop_InterleaveHI8x8;
                  op_lo = Q ? Iop_InterleaveLO8x16 : Iop_InterleaveLO8x8;
                  break;
               case 1:
                  op_hi = Q ? Iop_InterleaveHI16x8 : Iop_InterleaveHI16x4;
                  op_lo = Q ? Iop_InterleaveLO16x8 : Iop_InterleaveLO16x4;
                  break;
               case 2:
                  op_hi = Iop_InterleaveHI32x4;
                  op_lo = Iop_InterleaveLO32x4;
                  break;
               case 3:
                  return False;
               default:
                  vassert(0);
            }
            assign(new_d, binop(op_lo, mkexpr(old_m), mkexpr(old_d)));
            assign(new_m, binop(op_hi, mkexpr(old_m), mkexpr(old_d)));
            if (Q) {
               putQReg(dreg, mkexpr(new_d), condT);
               putQReg(mreg, mkexpr(new_m), condT);
            } else {
               putDRegI64(dreg, mkexpr(new_d), condT);
               putDRegI64(mreg, mkexpr(new_m), condT);
            }
            DIP("vzip.%u %c%u, %c%u\n",
                8 << size, Q ? 'q' : 'd', dreg, Q ? 'q' : 'd', mreg);
            return True;
         } else if (B == 8) {
            
            IROp op;
            mreg >>= 1;
            switch (size) {
               case 0: op = Iop_NarrowUn16to8x8;  break;
               case 1: op = Iop_NarrowUn32to16x4; break;
               case 2: op = Iop_NarrowUn64to32x2; break;
               case 3: return False;
               default: vassert(0);
            }
            putDRegI64(dreg, unop(op, getQReg(mreg)), condT);
            DIP("vmovn.i%u d%u, q%u\n", 16 << size, dreg, mreg);
            return True;
         } else if (B == 9 || (B >> 1) == 5) {
            
            IROp op, op2;
            IRTemp tmp;
            dreg = ((theInstr >> 18) & 0x10) | ((theInstr >> 12) & 0xF);
            mreg = ((theInstr >> 1) & 0x10) | (theInstr & 0xF);
            if (mreg & 1)
               return False;
            mreg >>= 1;
            switch (size) {
               case 0: op2 = Iop_NarrowUn16to8x8;  break;
               case 1: op2 = Iop_NarrowUn32to16x4; break;
               case 2: op2 = Iop_NarrowUn64to32x2; break;
               case 3: return False;
               default: vassert(0);
            }
            switch (B & 3) {
               case 0:
                  vassert(0);
               case 1:
                  switch (size) {
                     case 0: op = Iop_QNarrowUn16Sto8Ux8;  break;
                     case 1: op = Iop_QNarrowUn32Sto16Ux4; break;
                     case 2: op = Iop_QNarrowUn64Sto32Ux2; break;
                     case 3: return False;
                     default: vassert(0);
                  }
                  DIP("vqmovun.s%u d%u, q%u\n", 16 << size, dreg, mreg);
                  break;
               case 2:
                  switch (size) {
                     case 0: op = Iop_QNarrowUn16Sto8Sx8;  break;
                     case 1: op = Iop_QNarrowUn32Sto16Sx4; break;
                     case 2: op = Iop_QNarrowUn64Sto32Sx2; break;
                     case 3: return False;
                     default: vassert(0);
                  }
                  DIP("vqmovn.s%u d%u, q%u\n", 16 << size, dreg, mreg);
                  break;
               case 3:
                  switch (size) {
                     case 0: op = Iop_QNarrowUn16Uto8Ux8;  break;
                     case 1: op = Iop_QNarrowUn32Uto16Ux4; break;
                     case 2: op = Iop_QNarrowUn64Uto32Ux2; break;
                     case 3: return False;
                     default: vassert(0);
                  }
                  DIP("vqmovn.u%u d%u, q%u\n", 16 << size, dreg, mreg);
                  break;
               default:
                  vassert(0);
            }
            res = newTemp(Ity_I64);
            tmp = newTemp(Ity_I64);
            assign(res, unop(op, getQReg(mreg)));
            assign(tmp, unop(op2, getQReg(mreg)));
            setFlag_QC(mkexpr(res), mkexpr(tmp), False, condT);
            putDRegI64(dreg, mkexpr(res), condT);
            return True;
         } else if (B == 12) {
            
            IROp op, cvt;
            UInt shift_imm;
            if (Q)
               return False;
            if (dreg & 1)
               return False;
            dreg >>= 1;
            shift_imm = 8 << size;
            res = newTemp(Ity_V128);
            switch (size) {
               case 0: op = Iop_ShlN16x8; cvt = Iop_Widen8Uto16x8;  break;
               case 1: op = Iop_ShlN32x4; cvt = Iop_Widen16Uto32x4; break;
               case 2: op = Iop_ShlN64x2; cvt = Iop_Widen32Uto64x2; break;
               case 3: return False;
               default: vassert(0);
            }
            assign(res, binop(op, unop(cvt, getDRegI64(mreg)),
                                  mkU8(shift_imm)));
            putQReg(dreg, mkexpr(res), condT);
            DIP("vshll.i%u q%u, d%u, #%u\n", 8 << size, dreg, mreg, 8 << size);
            return True;
         } else if ((B >> 3) == 3 && (B & 3) == 0) {
            
            
            vassert(0); 
            if (((theInstr >> 18) & 3) != 1)
               return False;
            if ((theInstr >> 8) & 1) {
               if (dreg & 1)
                  return False;
               dreg >>= 1;
               putQReg(dreg, unop(Iop_F16toF32x4, getDRegI64(mreg)),
                     condT);
               DIP("vcvt.f32.f16 q%u, d%u\n", dreg, mreg);
            } else {
               if (mreg & 1)
                  return False;
               mreg >>= 1;
               putDRegI64(dreg, unop(Iop_F32toF16x4, getQReg(mreg)),
                                condT);
               DIP("vcvt.f16.f32 d%u, q%u\n", dreg, mreg);
            }
            return True;
         } else {
            return False;
         }
         vassert(0);
         return True;
      case 3:
         if (((B >> 1) & BITS4(1,1,0,1)) == BITS4(1,0,0,0)) {
            
            IROp op;
            F = (theInstr >> 8) & 1;
            if (size != 2)
               return False;
            if (Q) {
               op = F ? Iop_RecipEst32Fx4 : Iop_RecipEst32Ux4;
               putQReg(dreg, unop(op, getQReg(mreg)), condT);
               DIP("vrecpe.%c32 q%u, q%u\n", F ? 'f' : 'u', dreg, mreg);
            } else {
               op = F ? Iop_RecipEst32Fx2 : Iop_RecipEst32Ux2;
               putDRegI64(dreg, unop(op, getDRegI64(mreg)), condT);
               DIP("vrecpe.%c32 d%u, d%u\n", F ? 'f' : 'u', dreg, mreg);
            }
            return True;
         } else if (((B >> 1) & BITS4(1,1,0,1)) == BITS4(1,0,0,1)) {
            
            IROp op;
            F = (B >> 2) & 1;
            if (size != 2)
               return False;
            if (F) {
               
               op = Q ? Iop_RSqrtEst32Fx4 : Iop_RSqrtEst32Fx2;
            } else {
               
               op = Q ? Iop_RSqrtEst32Ux4 : Iop_RSqrtEst32Ux2;
            }
            if (Q) {
               putQReg(dreg, unop(op, getQReg(mreg)), condT);
               DIP("vrsqrte.%c32 q%u, q%u\n", F ? 'f' : 'u', dreg, mreg);
            } else {
               putDRegI64(dreg, unop(op, getDRegI64(mreg)), condT);
               DIP("vrsqrte.%c32 d%u, d%u\n", F ? 'f' : 'u', dreg, mreg);
            }
            return True;
         } else if ((B >> 3) == 3) {
            
            IROp op;
            if (size != 2)
               return False;
            switch ((B >> 1) & 3) {
               case 0:
                  op = Q ? Iop_I32StoFx4 : Iop_I32StoFx2;
                  DIP("vcvt.f32.s32 %c%u, %c%u\n",
                      Q ? 'q' : 'd', dreg, Q ? 'q' : 'd', mreg);
                  break;
               case 1:
                  op = Q ? Iop_I32UtoFx4 : Iop_I32UtoFx2;
                  DIP("vcvt.f32.u32 %c%u, %c%u\n",
                      Q ? 'q' : 'd', dreg, Q ? 'q' : 'd', mreg);
                  break;
               case 2:
                  op = Q ? Iop_FtoI32Sx4_RZ : Iop_FtoI32Sx2_RZ;
                  DIP("vcvt.s32.f32 %c%u, %c%u\n",
                      Q ? 'q' : 'd', dreg, Q ? 'q' : 'd', mreg);
                  break;
               case 3:
                  op = Q ? Iop_FtoI32Ux4_RZ : Iop_FtoI32Ux2_RZ;
                  DIP("vcvt.u32.f32 %c%u, %c%u\n",
                      Q ? 'q' : 'd', dreg, Q ? 'q' : 'd', mreg);
                  break;
               default:
                  vassert(0);
            }
            if (Q) {
               putQReg(dreg, unop(op, getQReg(mreg)), condT);
            } else {
               putDRegI64(dreg, unop(op, getDRegI64(mreg)), condT);
            }
            return True;
         } else {
            return False;
         }
         vassert(0);
         return True;
      default:
         vassert(0);
   }
   return False;
}

static
void ppNeonImm(UInt imm, UInt cmode, UInt op)
{
   int i;
   switch (cmode) {
      case 0: case 1: case 8: case 9:
         vex_printf("0x%x", imm);
         break;
      case 2: case 3: case 10: case 11:
         vex_printf("0x%x00", imm);
         break;
      case 4: case 5:
         vex_printf("0x%x0000", imm);
         break;
      case 6: case 7:
         vex_printf("0x%x000000", imm);
         break;
      case 12:
         vex_printf("0x%xff", imm);
         break;
      case 13:
         vex_printf("0x%xffff", imm);
         break;
      case 14:
         if (op) {
            vex_printf("0x");
            for (i = 7; i >= 0; i--)
               vex_printf("%s", (imm & (1 << i)) ? "ff" : "00");
         } else {
            vex_printf("0x%x", imm);
         }
         break;
      case 15:
         vex_printf("0x%x", imm);
         break;
   }
}

static
const char *ppNeonImmType(UInt cmode, UInt op)
{
   switch (cmode) {
      case 0 ... 7:
      case 12: case 13:
         return "i32";
      case 8 ... 11:
         return "i16";
      case 14:
         if (op)
            return "i64";
         else
            return "i8";
      case 15:
         if (op)
            vassert(0);
         else
            return "f32";
      default:
         vassert(0);
   }
}

static
void DIPimm(UInt imm, UInt cmode, UInt op,
            const char *instr, UInt Q, UInt dreg)
{
   if (vex_traceflags & VEX_TRACE_FE) {
      vex_printf("%s.%s %c%u, #", instr,
                 ppNeonImmType(cmode, op), Q ? 'q' : 'd', dreg);
      ppNeonImm(imm, cmode, op);
      vex_printf("\n");
   }
}

static
Bool dis_neon_data_1reg_and_imm ( UInt theInstr, IRTemp condT )
{
   UInt dreg = get_neon_d_regno(theInstr);
   ULong imm_raw = ((theInstr >> 17) & 0x80) | ((theInstr >> 12) & 0x70) |
                  (theInstr & 0xf);
   ULong imm_raw_pp = imm_raw;
   UInt cmode = (theInstr >> 8) & 0xf;
   UInt op_bit = (theInstr >> 5) & 1;
   ULong imm = 0;
   UInt Q = (theInstr >> 6) & 1;
   int i, j;
   UInt tmp;
   IRExpr *imm_val;
   IRExpr *expr;
   IRTemp tmp_var;
   switch(cmode) {
      case 7: case 6:
         imm_raw = imm_raw << 8;
         
      case 5: case 4:
         imm_raw = imm_raw << 8;
         
      case 3: case 2:
         imm_raw = imm_raw << 8;
         
      case 0: case 1:
         imm = (imm_raw << 32) | imm_raw;
         break;
      case 11: case 10:
         imm_raw = imm_raw << 8;
         
      case 9: case 8:
         imm_raw = (imm_raw << 16) | imm_raw;
         imm = (imm_raw << 32) | imm_raw;
         break;
      case 13:
         imm_raw = (imm_raw << 8) | 0xff;
         
      case 12:
         imm_raw = (imm_raw << 8) | 0xff;
         imm = (imm_raw << 32) | imm_raw;
         break;
      case 14:
         if (! op_bit) {
            for(i = 0; i < 8; i++) {
               imm = (imm << 8) | imm_raw;
            }
         } else {
            for(i = 7; i >= 0; i--) {
               tmp = 0;
               for(j = 0; j < 8; j++) {
                  tmp = (tmp << 1) | ((imm_raw >> i) & 1);
               }
               imm = (imm << 8) | tmp;
            }
         }
         break;
      case 15:
         imm = (imm_raw & 0x80) << 5;
         imm |= ((~imm_raw & 0x40) << 5);
         for(i = 1; i <= 4; i++)
            imm |= (imm_raw & 0x40) << i;
         imm |= (imm_raw & 0x7f);
         imm = imm << 19;
         imm = (imm << 32) | imm;
         break;
      default:
         return False;
   }
   if (Q) {
      imm_val = binop(Iop_64HLtoV128, mkU64(imm), mkU64(imm));
   } else {
      imm_val = mkU64(imm);
   }
   if (((op_bit == 0) &&
      (((cmode & 9) == 0) || ((cmode & 13) == 8) || ((cmode & 12) == 12))) ||
      ((op_bit == 1) && (cmode == 14))) {
      
      if (Q) {
         putQReg(dreg, imm_val, condT);
      } else {
         putDRegI64(dreg, imm_val, condT);
      }
      DIPimm(imm_raw_pp, cmode, op_bit, "vmov", Q, dreg);
      return True;
   }
   if ((op_bit == 1) &&
      (((cmode & 9) == 0) || ((cmode & 13) == 8) || ((cmode & 14) == 12))) {
      
      if (Q) {
         putQReg(dreg, unop(Iop_NotV128, imm_val), condT);
      } else {
         putDRegI64(dreg, unop(Iop_Not64, imm_val), condT);
      }
      DIPimm(imm_raw_pp, cmode, op_bit, "vmvn", Q, dreg);
      return True;
   }
   if (Q) {
      tmp_var = newTemp(Ity_V128);
      assign(tmp_var, getQReg(dreg));
   } else {
      tmp_var = newTemp(Ity_I64);
      assign(tmp_var, getDRegI64(dreg));
   }
   if ((op_bit == 0) && (((cmode & 9) == 1) || ((cmode & 13) == 9))) {
      
      if (Q)
         expr = binop(Iop_OrV128, mkexpr(tmp_var), imm_val);
      else
         expr = binop(Iop_Or64, mkexpr(tmp_var), imm_val);
      DIPimm(imm_raw_pp, cmode, op_bit, "vorr", Q, dreg);
   } else if ((op_bit == 1) && (((cmode & 9) == 1) || ((cmode & 13) == 9))) {
      
      if (Q)
         expr = binop(Iop_AndV128, mkexpr(tmp_var),
                                   unop(Iop_NotV128, imm_val));
      else
         expr = binop(Iop_And64, mkexpr(tmp_var), unop(Iop_Not64, imm_val));
      DIPimm(imm_raw_pp, cmode, op_bit, "vbic", Q, dreg);
   } else {
      return False;
   }
   if (Q)
      putQReg(dreg, expr, condT);
   else
      putDRegI64(dreg, expr, condT);
   return True;
}

static
Bool dis_neon_data_processing ( UInt theInstr, IRTemp condT )
{
   UInt A = (theInstr >> 19) & 0x1F;
   UInt B = (theInstr >>  8) & 0xF;
   UInt C = (theInstr >>  4) & 0xF;
   UInt U = (theInstr >> 24) & 0x1;

   if (! (A & 0x10)) {
      return dis_neon_data_3same(theInstr, condT);
   }
   if (((A & 0x17) == 0x10) && ((C & 0x9) == 0x1)) {
      return dis_neon_data_1reg_and_imm(theInstr, condT);
   }
   if ((C & 1) == 1) {
      return dis_neon_data_2reg_and_shift(theInstr, condT);
   }
   if (((C & 5) == 0) && (((A & 0x14) == 0x10) || ((A & 0x16) == 0x14))) {
      return dis_neon_data_3diff(theInstr, condT);
   }
   if (((C & 5) == 4) && (((A & 0x14) == 0x10) || ((A & 0x16) == 0x14))) {
      return dis_neon_data_2reg_and_scalar(theInstr, condT);
   }
   if ((A & 0x16) == 0x16) {
      if ((U == 0) && ((C & 1) == 0)) {
         return dis_neon_vext(theInstr, condT);
      }
      if ((U != 1) || ((C & 1) == 1))
         return False;
      if ((B & 8) == 0) {
         return dis_neon_data_2reg_misc(theInstr, condT);
      }
      if ((B & 12) == 8) {
         return dis_neon_vtb(theInstr, condT);
      }
      if ((B == 12) && ((C & 9) == 0)) {
         return dis_neon_vdup(theInstr, condT);
      }
   }
   return False;
}




static
void mk_neon_elem_load_to_one_lane( UInt rD, UInt inc, UInt index,
                                    UInt N, UInt size, IRTemp addr )
{
   UInt i;
   switch (size) {
      case 0:
         putDRegI64(rD, triop(Iop_SetElem8x8, getDRegI64(rD), mkU8(index),
                    loadLE(Ity_I8, mkexpr(addr))), IRTemp_INVALID);
         break;
      case 1:
         putDRegI64(rD, triop(Iop_SetElem16x4, getDRegI64(rD), mkU8(index),
                    loadLE(Ity_I16, mkexpr(addr))), IRTemp_INVALID);
         break;
      case 2:
         putDRegI64(rD, triop(Iop_SetElem32x2, getDRegI64(rD), mkU8(index),
                    loadLE(Ity_I32, mkexpr(addr))), IRTemp_INVALID);
         break;
      default:
         vassert(0);
   }
   for (i = 1; i <= N; i++) {
      switch (size) {
         case 0:
            putDRegI64(rD + i * inc,
                       triop(Iop_SetElem8x8,
                             getDRegI64(rD + i * inc),
                             mkU8(index),
                             loadLE(Ity_I8, binop(Iop_Add32,
                                                  mkexpr(addr),
                                                  mkU32(i * 1)))),
                       IRTemp_INVALID);
            break;
         case 1:
            putDRegI64(rD + i * inc,
                       triop(Iop_SetElem16x4,
                             getDRegI64(rD + i * inc),
                             mkU8(index),
                             loadLE(Ity_I16, binop(Iop_Add32,
                                                   mkexpr(addr),
                                                   mkU32(i * 2)))),
                       IRTemp_INVALID);
            break;
         case 2:
            putDRegI64(rD + i * inc,
                       triop(Iop_SetElem32x2,
                             getDRegI64(rD + i * inc),
                             mkU8(index),
                             loadLE(Ity_I32, binop(Iop_Add32,
                                                   mkexpr(addr),
                                                   mkU32(i * 4)))),
                       IRTemp_INVALID);
            break;
         default:
            vassert(0);
      }
   }
}

static
void mk_neon_elem_store_from_one_lane( UInt rD, UInt inc, UInt index,
                                       UInt N, UInt size, IRTemp addr )
{
   UInt i;
   switch (size) {
      case 0:
         storeLE(mkexpr(addr),
                 binop(Iop_GetElem8x8, getDRegI64(rD), mkU8(index)));
         break;
      case 1:
         storeLE(mkexpr(addr),
                 binop(Iop_GetElem16x4, getDRegI64(rD), mkU8(index)));
         break;
      case 2:
         storeLE(mkexpr(addr),
                 binop(Iop_GetElem32x2, getDRegI64(rD), mkU8(index)));
         break;
      default:
         vassert(0);
   }
   for (i = 1; i <= N; i++) {
      switch (size) {
         case 0:
            storeLE(binop(Iop_Add32, mkexpr(addr), mkU32(i * 1)),
                    binop(Iop_GetElem8x8, getDRegI64(rD + i * inc),
                                          mkU8(index)));
            break;
         case 1:
            storeLE(binop(Iop_Add32, mkexpr(addr), mkU32(i * 2)),
                    binop(Iop_GetElem16x4, getDRegI64(rD + i * inc),
                                           mkU8(index)));
            break;
         case 2:
            storeLE(binop(Iop_Add32, mkexpr(addr), mkU32(i * 4)),
                    binop(Iop_GetElem32x2, getDRegI64(rD + i * inc),
                                           mkU8(index)));
            break;
         default:
            vassert(0);
      }
   }
}

static void math_DEINTERLEAVE_2 (IRTemp* u0, IRTemp* u1,
                                 IRTemp i0, IRTemp i1, Int laneszB)
{
   vassert(u0 && u1);
   if (laneszB == 4) {
      
      
      
      assign(*u0, binop(Iop_InterleaveLO32x2, mkexpr(i1), mkexpr(i0)));
      assign(*u1, binop(Iop_InterleaveHI32x2, mkexpr(i1), mkexpr(i0)));
   } else if (laneszB == 2) {
      
      
      
      assign(*u0, binop(Iop_CatEvenLanes16x4, mkexpr(i1), mkexpr(i0)));
      assign(*u1, binop(Iop_CatOddLanes16x4,  mkexpr(i1), mkexpr(i0)));
   } else if (laneszB == 1) {
      
      
      
      assign(*u0, binop(Iop_CatEvenLanes8x8, mkexpr(i1), mkexpr(i0)));
      assign(*u1, binop(Iop_CatOddLanes8x8,  mkexpr(i1), mkexpr(i0)));
   } else {
      
      
      vpanic("math_DEINTERLEAVE_2");
   }
}

static void math_INTERLEAVE_2 (IRTemp* i0, IRTemp* i1,
                               IRTemp u0, IRTemp u1, Int laneszB)
{
   vassert(i0 && i1);
   if (laneszB == 4) {
      
      
      
      assign(*i0, binop(Iop_InterleaveLO32x2, mkexpr(u1), mkexpr(u0)));
      assign(*i1, binop(Iop_InterleaveHI32x2, mkexpr(u1), mkexpr(u0)));
   } else if (laneszB == 2) {
      
      
      
      assign(*i0, binop(Iop_InterleaveLO16x4, mkexpr(u1), mkexpr(u0)));
      assign(*i1, binop(Iop_InterleaveHI16x4, mkexpr(u1), mkexpr(u0)));
   } else if (laneszB == 1) {
      
      
      
      assign(*i0, binop(Iop_InterleaveLO8x8, mkexpr(u1), mkexpr(u0)));
      assign(*i1, binop(Iop_InterleaveHI8x8, mkexpr(u1), mkexpr(u0)));
   } else {
      
      
      vpanic("math_INTERLEAVE_2");
   }
}

static IRExpr* math_PERM_8x8x3(const UChar* desc,
                               IRTemp s0, IRTemp s1, IRTemp s2)
{
   
   
   
   
   
   UInt si;
   for (si = 0; si < 7; si++) {
      vassert(desc[2 * si + 0] <= 2);
      vassert(desc[2 * si + 1] <= 7);
   }
   IRTemp h3 = newTemp(Ity_I64);
   IRTemp h2 = newTemp(Ity_I64);
   IRTemp h1 = newTemp(Ity_I64);
   IRTemp h0 = newTemp(Ity_I64);
   IRTemp srcs[3] = {s0, s1, s2};
#  define SRC_VEC(_lane)   mkexpr(srcs[desc[2 * (7-(_lane)) + 0]])
#  define SRC_SHIFT(_lane) mkU8(56-8*(desc[2 * (7-(_lane)) + 1]))
   assign(h3, binop(Iop_InterleaveHI8x8,
                    binop(Iop_Shl64, SRC_VEC(7), SRC_SHIFT(7)),
                    binop(Iop_Shl64, SRC_VEC(6), SRC_SHIFT(6))));
   assign(h2, binop(Iop_InterleaveHI8x8,
                    binop(Iop_Shl64, SRC_VEC(5), SRC_SHIFT(5)),
                    binop(Iop_Shl64, SRC_VEC(4), SRC_SHIFT(4))));
   assign(h1, binop(Iop_InterleaveHI8x8,
                    binop(Iop_Shl64, SRC_VEC(3), SRC_SHIFT(3)),
                    binop(Iop_Shl64, SRC_VEC(2), SRC_SHIFT(2))));
   assign(h0, binop(Iop_InterleaveHI8x8,
                    binop(Iop_Shl64, SRC_VEC(1), SRC_SHIFT(1)),
                    binop(Iop_Shl64, SRC_VEC(0), SRC_SHIFT(0))));
#  undef SRC_VEC
#  undef SRC_SHIFT
   
   
   
   IRTemp w1 = newTemp(Ity_I64);
   IRTemp w0 = newTemp(Ity_I64);
   assign(w1, binop(Iop_InterleaveHI16x4, mkexpr(h3), mkexpr(h2)));
   assign(w0, binop(Iop_InterleaveHI16x4, mkexpr(h1), mkexpr(h0)));
   return binop(Iop_InterleaveHI32x2, mkexpr(w1), mkexpr(w0));
}

static void math_DEINTERLEAVE_3 (
               IRTemp* u0, IRTemp* u1, IRTemp* u2,
               IRTemp i0, IRTemp i1, IRTemp i2, Int laneszB
            )
{
#  define IHI32x2(_e1, _e2) binop(Iop_InterleaveHI32x2, (_e1), (_e2))
#  define IHI16x4(_e1, _e2) binop(Iop_InterleaveHI16x4, (_e1), (_e2))
#  define SHL64(_tmp, _amt) binop(Iop_Shl64, mkexpr(_tmp), mkU8(_amt))
   vassert(u0 && u1 && u2);
   if (laneszB == 4) {
      
      
      
      assign(*u0, IHI32x2(SHL64(i1,  0), SHL64(i0, 32)));
      assign(*u1, IHI32x2(SHL64(i2, 32), SHL64(i0,  0)));
      assign(*u2, IHI32x2(SHL64(i2,  0), SHL64(i1, 32)));
   } else if (laneszB == 2) {
      
      
      
#     define XXX(_tmp3,_la3,_tmp2,_la2,_tmp1,_la1,_tmp0,_la0) \
                IHI32x2(                                      \
                   IHI16x4(SHL64((_tmp3),48-16*(_la3)),       \
                           SHL64((_tmp2),48-16*(_la2))),      \
                   IHI16x4(SHL64((_tmp1),48-16*(_la1)),       \
                           SHL64((_tmp0),48-16*(_la0))))
      assign(*u0, XXX(i2,1, i1,2, i0,3, i0,0));
      assign(*u1, XXX(i2,2, i1,3, i1,0, i0,1));
      assign(*u2, XXX(i2,3, i2,0, i1,1, i0,2));
#     undef XXX
   } else if (laneszB == 1) {
      
      
      
      static const UChar de0[16] = {2,5, 2,2, 1,7, 1,4, 1,1, 0,6, 0,3, 0,0};
      static const UChar de1[16] = {2,6, 2,3, 2,0, 1,5, 1,2, 0,7, 0,4, 0,1};
      static const UChar de2[16] = {2,7, 2,4, 2,1, 1,6, 1,3, 1,0, 0,5, 0,2};
      assign(*u0, math_PERM_8x8x3(de0, i0, i1, i2));
      assign(*u1, math_PERM_8x8x3(de1, i0, i1, i2));
      assign(*u2, math_PERM_8x8x3(de2, i0, i1, i2));
   } else {
      
      
      vpanic("math_DEINTERLEAVE_3");
   }
#  undef SHL64
#  undef IHI16x4
#  undef IHI32x2
}

static void math_INTERLEAVE_3 (
               IRTemp* i0, IRTemp* i1, IRTemp* i2,
               IRTemp u0, IRTemp u1, IRTemp u2, Int laneszB
            )
{
#  define IHI32x2(_e1, _e2) binop(Iop_InterleaveHI32x2, (_e1), (_e2))
#  define IHI16x4(_e1, _e2) binop(Iop_InterleaveHI16x4, (_e1), (_e2))
#  define SHL64(_tmp, _amt) binop(Iop_Shl64, mkexpr(_tmp), mkU8(_amt))
   vassert(i0 && i1 && i2);
   if (laneszB == 4) {
      
      
      
      assign(*i0, IHI32x2(SHL64(u1, 32), SHL64(u0, 32)));
      assign(*i1, IHI32x2(SHL64(u0,  0), SHL64(u2, 32)));
      assign(*i2, IHI32x2(SHL64(u2,  0), SHL64(u1,  0)));
   } else if (laneszB == 2) {
      
      
      
#     define XXX(_tmp3,_la3,_tmp2,_la2,_tmp1,_la1,_tmp0,_la0) \
                IHI32x2(                                      \
                   IHI16x4(SHL64((_tmp3),48-16*(_la3)),       \
                           SHL64((_tmp2),48-16*(_la2))),      \
                   IHI16x4(SHL64((_tmp1),48-16*(_la1)),       \
                           SHL64((_tmp0),48-16*(_la0))))
      assign(*i0, XXX(u0,1, u2,0, u1,0, u0,0));
      assign(*i1, XXX(u1,2, u0,2, u2,1, u1,1));
      assign(*i2, XXX(u2,3, u1,3, u0,3, u2,2));
#     undef XXX
   } else if (laneszB == 1) {
      
      
      
      static const UChar in0[16] = {1,2, 0,2, 2,1, 1,1, 0,1, 2,0, 1,0, 0,0};
      static const UChar in1[16] = {0,5, 2,4, 1,4, 0,4, 2,3, 1,3, 0,3, 2,2};
      static const UChar in2[16] = {2,7, 1,7, 0,7, 2,6, 1,6, 0,6, 2,5, 1,5};
      assign(*i0, math_PERM_8x8x3(in0, u0, u1, u2));
      assign(*i1, math_PERM_8x8x3(in1, u0, u1, u2));
      assign(*i2, math_PERM_8x8x3(in2, u0, u1, u2));
   } else {
      
      
      vpanic("math_INTERLEAVE_3");
   }
#  undef SHL64
#  undef IHI16x4
#  undef IHI32x2
}

static void math_DEINTERLEAVE_4 (
               IRTemp* u0, IRTemp* u1,
               IRTemp* u2, IRTemp* u3,
               IRTemp i0, IRTemp i1, IRTemp i2, IRTemp i3, Int laneszB
            )
{
#  define IHI32x2(_t1, _t2) \
             binop(Iop_InterleaveHI32x2, mkexpr(_t1), mkexpr(_t2))
#  define ILO32x2(_t1, _t2) \
             binop(Iop_InterleaveLO32x2, mkexpr(_t1), mkexpr(_t2))
#  define IHI16x4(_t1, _t2) \
             binop(Iop_InterleaveHI16x4, mkexpr(_t1), mkexpr(_t2))
#  define ILO16x4(_t1, _t2) \
             binop(Iop_InterleaveLO16x4, mkexpr(_t1), mkexpr(_t2))
#  define IHI8x8(_t1, _e2) \
             binop(Iop_InterleaveHI8x8, mkexpr(_t1), _e2)
#  define SHL64(_tmp, _amt) \
             binop(Iop_Shl64, mkexpr(_tmp), mkU8(_amt))
   vassert(u0 && u1 && u2 && u3);
   if (laneszB == 4) {
      assign(*u0, ILO32x2(i2, i0));
      assign(*u1, IHI32x2(i2, i0));
      assign(*u2, ILO32x2(i3, i1));
      assign(*u3, IHI32x2(i3, i1));
   } else if (laneszB == 2) {
      IRTemp b1b0a1a0 = newTemp(Ity_I64);
      IRTemp b3b2a3a2 = newTemp(Ity_I64);
      IRTemp d1d0c1c0 = newTemp(Ity_I64);
      IRTemp d3d2c3c2 = newTemp(Ity_I64);
      assign(b1b0a1a0, ILO16x4(i1, i0));
      assign(b3b2a3a2, ILO16x4(i3, i2));
      assign(d1d0c1c0, IHI16x4(i1, i0));
      assign(d3d2c3c2, IHI16x4(i3, i2));
      
      assign(*u0, ILO32x2(b3b2a3a2, b1b0a1a0));
      assign(*u1, IHI32x2(b3b2a3a2, b1b0a1a0));
      assign(*u2, ILO32x2(d3d2c3c2, d1d0c1c0));
      assign(*u3, IHI32x2(d3d2c3c2, d1d0c1c0));
   } else if (laneszB == 1) {
      
      IRTemp i0x = newTemp(Ity_I64);
      IRTemp i1x = newTemp(Ity_I64);
      IRTemp i2x = newTemp(Ity_I64);
      IRTemp i3x = newTemp(Ity_I64);
      assign(i0x, IHI8x8(i0, SHL64(i0, 32)));
      assign(i1x, IHI8x8(i1, SHL64(i1, 32)));
      assign(i2x, IHI8x8(i2, SHL64(i2, 32)));
      assign(i3x, IHI8x8(i3, SHL64(i3, 32)));
      
      IRTemp b1b0a1a0 = newTemp(Ity_I64);
      IRTemp b3b2a3a2 = newTemp(Ity_I64);
      IRTemp d1d0c1c0 = newTemp(Ity_I64);
      IRTemp d3d2c3c2 = newTemp(Ity_I64);
      assign(b1b0a1a0, ILO16x4(i1x, i0x));
      assign(b3b2a3a2, ILO16x4(i3x, i2x));
      assign(d1d0c1c0, IHI16x4(i1x, i0x));
      assign(d3d2c3c2, IHI16x4(i3x, i2x));
      
      assign(*u0, ILO32x2(b3b2a3a2, b1b0a1a0));
      assign(*u1, IHI32x2(b3b2a3a2, b1b0a1a0));
      assign(*u2, ILO32x2(d3d2c3c2, d1d0c1c0));
      assign(*u3, IHI32x2(d3d2c3c2, d1d0c1c0));
   } else {
      
      
      vpanic("math_DEINTERLEAVE_4");
   }
#  undef SHL64
#  undef IHI8x8
#  undef ILO16x4
#  undef IHI16x4
#  undef ILO32x2
#  undef IHI32x2
}

static void math_INTERLEAVE_4 (
               IRTemp* i0, IRTemp* i1,
               IRTemp* i2, IRTemp* i3,
               IRTemp u0, IRTemp u1, IRTemp u2, IRTemp u3, Int laneszB
            )
{
#  define IHI32x2(_t1, _t2) \
             binop(Iop_InterleaveHI32x2, mkexpr(_t1), mkexpr(_t2))
#  define ILO32x2(_t1, _t2) \
             binop(Iop_InterleaveLO32x2, mkexpr(_t1), mkexpr(_t2))
#  define CEV16x4(_t1, _t2) \
             binop(Iop_CatEvenLanes16x4, mkexpr(_t1), mkexpr(_t2))
#  define COD16x4(_t1, _t2) \
             binop(Iop_CatOddLanes16x4, mkexpr(_t1), mkexpr(_t2))
#  define COD8x8(_t1, _e2) \
             binop(Iop_CatOddLanes8x8, mkexpr(_t1), _e2)
#  define SHL64(_tmp, _amt) \
             binop(Iop_Shl64, mkexpr(_tmp), mkU8(_amt))
   vassert(u0 && u1 && u2 && u3);
   if (laneszB == 4) {
      assign(*i0, ILO32x2(u1, u0));
      assign(*i1, ILO32x2(u3, u2));
      assign(*i2, IHI32x2(u1, u0));
      assign(*i3, IHI32x2(u3, u2));
   } else if (laneszB == 2) {
      
      IRTemp b1b0a1a0 = newTemp(Ity_I64);
      IRTemp b3b2a3a2 = newTemp(Ity_I64);
      IRTemp d1d0c1c0 = newTemp(Ity_I64);
      IRTemp d3d2c3c2 = newTemp(Ity_I64);
      assign(b1b0a1a0, ILO32x2(u1, u0));
      assign(b3b2a3a2, IHI32x2(u1, u0));
      assign(d1d0c1c0, ILO32x2(u3, u2));
      assign(d3d2c3c2, IHI32x2(u3, u2));
      
      assign(*i0, CEV16x4(d1d0c1c0, b1b0a1a0));
      assign(*i1, COD16x4(d1d0c1c0, b1b0a1a0));
      assign(*i2, CEV16x4(d3d2c3c2, b3b2a3a2));
      assign(*i3, COD16x4(d3d2c3c2, b3b2a3a2));
   } else if (laneszB == 1) {
      
      IRTemp b1b0a1a0 = newTemp(Ity_I64);
      IRTemp b3b2a3a2 = newTemp(Ity_I64);
      IRTemp d1d0c1c0 = newTemp(Ity_I64);
      IRTemp d3d2c3c2 = newTemp(Ity_I64);
      assign(b1b0a1a0, ILO32x2(u1, u0));
      assign(b3b2a3a2, IHI32x2(u1, u0));
      assign(d1d0c1c0, ILO32x2(u3, u2));
      assign(d3d2c3c2, IHI32x2(u3, u2));
      
      IRTemp i0x = newTemp(Ity_I64);
      IRTemp i1x = newTemp(Ity_I64);
      IRTemp i2x = newTemp(Ity_I64);
      IRTemp i3x = newTemp(Ity_I64);
      assign(i0x, CEV16x4(d1d0c1c0, b1b0a1a0));
      assign(i1x, COD16x4(d1d0c1c0, b1b0a1a0));
      assign(i2x, CEV16x4(d3d2c3c2, b3b2a3a2));
      assign(i3x, COD16x4(d3d2c3c2, b3b2a3a2));
      
      assign(*i0, COD8x8(i0x, SHL64(i0x, 8)));
      assign(*i1, COD8x8(i1x, SHL64(i1x, 8)));
      assign(*i2, COD8x8(i2x, SHL64(i2x, 8)));
      assign(*i3, COD8x8(i3x, SHL64(i3x, 8)));
   } else {
      
      
      vpanic("math_DEINTERLEAVE_4");
   }
#  undef SHL64
#  undef COD8x8
#  undef COD16x4
#  undef CEV16x4
#  undef ILO32x2
#  undef IHI32x2
}

static
Bool dis_neon_load_or_store ( UInt theInstr,
                              Bool isT, IRTemp condT )
{
#  define INSN(_bMax,_bMin)  SLICE_UInt(theInstr, (_bMax), (_bMin))
   UInt bA = INSN(23,23);
   UInt fB = INSN(11,8);
   UInt bL = INSN(21,21);
   UInt rD = (INSN(22,22) << 4) | INSN(15,12);
   UInt rN = INSN(19,16);
   UInt rM = INSN(3,0);
   UInt N, size, i, j;
   UInt inc;
   UInt regs = 1;

   if (isT) {
      vassert(condT != IRTemp_INVALID);
   } else {
      vassert(condT == IRTemp_INVALID);
   }

   if (INSN(20,20) != 0)
      return False;

   IRTemp initialRn = newTemp(Ity_I32);
   assign(initialRn, isT ? getIRegT(rN) : getIRegA(rN));

   IRTemp initialRm = newTemp(Ity_I32);
   assign(initialRm, isT ? getIRegT(rM) : getIRegA(rM));

   if (bA) {
      N = fB & 3;
      if ((fB >> 2) < 3) {

         size = fB >> 2;

         switch (size) {
            case 0: i = INSN(7,5); inc = 1; break;
            case 1: i = INSN(7,6); inc = INSN(5,5) ? 2 : 1; break;
            case 2: i = INSN(7,7); inc = INSN(6,6) ? 2 : 1; break;
            case 3: return False;
            default: vassert(0);
         }

         IRTemp addr = newTemp(Ity_I32);
         assign(addr, mkexpr(initialRn));

         
         if (condT != IRTemp_INVALID)
            mk_skip_over_T32_if_cond_is_false(condT);
         

         if (bL)
            mk_neon_elem_load_to_one_lane(rD, inc, i, N, size, addr);
         else
            mk_neon_elem_store_from_one_lane(rD, inc, i, N, size, addr);
         DIP("v%s%u.%u {", bL ? "ld" : "st", N + 1, 8 << size);
         for (j = 0; j <= N; j++) {
            if (j)
               DIP(", ");
            DIP("d%u[%u]", rD + j * inc, i);
         }
         DIP("}, [r%u]", rN);
         if (rM != 13 && rM != 15) {
            DIP(", r%u\n", rM);
         } else {
            DIP("%s\n", (rM != 15) ? "!" : "");
         }
      } else {
         UInt r;
         if (bL == 0)
            return False;

         inc = INSN(5,5) + 1;
         size = INSN(7,6);

         
         if (size == 3 && N == 3 && INSN(4,4) == 1)
            size = 2;

         if (size == 0 && N == 0 && INSN(4,4) == 1)
            return False;
         if (N == 2 && INSN(4,4) == 1)
            return False;
         if (size == 3)
            return False;

         
         if (condT != IRTemp_INVALID)
            mk_skip_over_T32_if_cond_is_false(condT);
         

         IRTemp addr = newTemp(Ity_I32);
         assign(addr, mkexpr(initialRn));

         if (N == 0 && INSN(5,5))
            regs = 2;

         for (r = 0; r < regs; r++) {
            switch (size) {
               case 0:
                  putDRegI64(rD + r, unop(Iop_Dup8x8,
                                          loadLE(Ity_I8, mkexpr(addr))),
                             IRTemp_INVALID);
                  break;
               case 1:
                  putDRegI64(rD + r, unop(Iop_Dup16x4,
                                          loadLE(Ity_I16, mkexpr(addr))),
                             IRTemp_INVALID);
                  break;
               case 2:
                  putDRegI64(rD + r, unop(Iop_Dup32x2,
                                          loadLE(Ity_I32, mkexpr(addr))),
                             IRTemp_INVALID);
                  break;
               default:
                  vassert(0);
            }
            for (i = 1; i <= N; i++) {
               switch (size) {
                  case 0:
                     putDRegI64(rD + r + i * inc,
                                unop(Iop_Dup8x8,
                                     loadLE(Ity_I8, binop(Iop_Add32,
                                                          mkexpr(addr),
                                                          mkU32(i * 1)))),
                                IRTemp_INVALID);
                     break;
                  case 1:
                     putDRegI64(rD + r + i * inc,
                                unop(Iop_Dup16x4,
                                     loadLE(Ity_I16, binop(Iop_Add32,
                                                           mkexpr(addr),
                                                           mkU32(i * 2)))),
                                IRTemp_INVALID);
                     break;
                  case 2:
                     putDRegI64(rD + r + i * inc,
                                unop(Iop_Dup32x2,
                                     loadLE(Ity_I32, binop(Iop_Add32,
                                                           mkexpr(addr),
                                                           mkU32(i * 4)))),
                                IRTemp_INVALID);
                     break;
                  default:
                     vassert(0);
               }
            }
         }
         DIP("vld%u.%u {", N + 1, 8 << size);
         for (r = 0; r < regs; r++) {
            for (i = 0; i <= N; i++) {
               if (i || r)
                  DIP(", ");
               DIP("d%u[]", rD + r + i * inc);
            }
         }
         DIP("}, [r%u]", rN);
         if (rM != 13 && rM != 15) {
            DIP(", r%u\n", rM);
         } else {
            DIP("%s\n", (rM != 15) ? "!" : "");
         }
      }
      
      if (rM != 15) {
         if (rM == 13) {
            IRExpr* e = binop(Iop_Add32,
                              mkexpr(initialRn),
                              mkU32((1 << size) * (N + 1)));
            if (isT)
               putIRegT(rN, e, IRTemp_INVALID);
            else
               putIRegA(rN, e, IRTemp_INVALID, Ijk_Boring);
         } else {
            IRExpr* e = binop(Iop_Add32,
                              mkexpr(initialRn),
                              mkexpr(initialRm));
            if (isT)
               putIRegT(rN, e, IRTemp_INVALID);
            else
               putIRegA(rN, e, IRTemp_INVALID, Ijk_Boring);
         }
      }
      return True;
   } else {
      inc = (fB & 1) + 1;

      if (fB == BITS4(0,0,1,0)       
          || fB == BITS4(0,1,1,0)    
          || fB == BITS4(0,1,1,1)    
          || fB == BITS4(1,0,1,0)) { 
         N = 0; 
                
                
         if (rD + regs > 32) return False;
      } 
      else 
      if (fB == BITS4(0,0,1,1)       
          || fB == BITS4(1,0,0,0)    
          || fB == BITS4(1,0,0,1)) { 
         N = 1; 
         if (regs == 1 && inc == 1 && rD + 1 >= 32) return False;
         if (regs == 1 && inc == 2 && rD + 2 >= 32) return False;
         if (regs == 2 && inc == 2 && rD + 3 >= 32) return False;
      } else if (fB == BITS4(0,1,0,0) || fB == BITS4(0,1,0,1)) {
         N = 2; 
         if (inc == 1 && rD + 2 >= 32) return False;
         if (inc == 2 && rD + 4 >= 32) return False;
      } else if (fB == BITS4(0,0,0,0) || fB == BITS4(0,0,0,1)) {
         N = 3; 
         if (inc == 1 && rD + 3 >= 32) return False;
         if (inc == 2 && rD + 6 >= 32) return False;
      } else {
         return False;
      }

      if (N == 1 && fB == BITS4(0,0,1,1)) {
         regs = 2;
      } else if (N == 0) {
         if (fB == BITS4(1,0,1,0)) {
            regs = 2;
         } else if (fB == BITS4(0,1,1,0)) {
            regs = 3;
         } else if (fB == BITS4(0,0,1,0)) {
            regs = 4;
         }
      }

      size = INSN(7,6);
      if (N == 0 && size == 3)
         size = 2;
      if (size == 3)
         return False;

      
      if (condT != IRTemp_INVALID)
         mk_skip_over_T32_if_cond_is_false(condT);
      

      IRTemp addr = newTemp(Ity_I32);
      assign(addr, mkexpr(initialRn));

      if (N == 0 ) {
         UInt r;
         vassert(regs == 1 || regs == 2 || regs == 3 || regs == 4);
         
         for (r = 0; r < regs; r++) {
            if (bL)
               putDRegI64(rD+r, loadLE(Ity_I64, mkexpr(addr)), IRTemp_INVALID);
            else
               storeLE(mkexpr(addr), getDRegI64(rD+r));
            IRTemp tmp = newTemp(Ity_I32);
            assign(tmp, binop(Iop_Add32, mkexpr(addr), mkU32(8)));
            addr = tmp;
         }
      }
      else
      if (N == 1 ) {
         vassert( (regs == 1 && (inc == 1 || inc == 2))
                   || (regs == 2 && inc == 2) );
         
         
         
         
         
         
         UInt nregs   = 2;
         UInt regstep = 1;
         if (regs == 1 && inc == 1) {
            
         } else if (regs == 1 && inc == 2) {
            regstep = 2;
         } else if (regs == 2 && inc == 2) {
            nregs = 4;
         } else {
            vassert(0);
         }
         
         
         if (nregs == 2) {
            IRExpr* a0  = binop(Iop_Add32, mkexpr(addr), mkU32(0));
            IRExpr* a1  = binop(Iop_Add32, mkexpr(addr), mkU32(8));
            IRTemp  di0 = newTemp(Ity_I64);
            IRTemp  di1 = newTemp(Ity_I64);
            IRTemp  du0 = newTemp(Ity_I64); 
            IRTemp  du1 = newTemp(Ity_I64);
            if (bL) {
               assign(di0, loadLE(Ity_I64, a0));
               assign(di1, loadLE(Ity_I64, a1));
               math_DEINTERLEAVE_2(&du0, &du1, di0, di1, 1 << size);
               putDRegI64(rD + 0 * regstep, mkexpr(du0), IRTemp_INVALID);
               putDRegI64(rD + 1 * regstep, mkexpr(du1), IRTemp_INVALID);
            } else {
               assign(du0, getDRegI64(rD + 0 * regstep));
               assign(du1, getDRegI64(rD + 1 * regstep));
               math_INTERLEAVE_2(&di0, &di1, du0, du1, 1 << size);
               storeLE(a0, mkexpr(di0));
               storeLE(a1, mkexpr(di1));
            }
            IRTemp tmp = newTemp(Ity_I32);
            assign(tmp, binop(Iop_Add32, mkexpr(addr), mkU32(16)));
            addr = tmp;
         } else {
            vassert(nregs == 4);
            vassert(regstep == 1);
            IRExpr* a0  = binop(Iop_Add32, mkexpr(addr), mkU32(0));
            IRExpr* a1  = binop(Iop_Add32, mkexpr(addr), mkU32(8));
            IRExpr* a2  = binop(Iop_Add32, mkexpr(addr), mkU32(16));
            IRExpr* a3  = binop(Iop_Add32, mkexpr(addr), mkU32(24));
            IRTemp  di0 = newTemp(Ity_I64);
            IRTemp  di1 = newTemp(Ity_I64);
            IRTemp  di2 = newTemp(Ity_I64);
            IRTemp  di3 = newTemp(Ity_I64);
            IRTemp  du0 = newTemp(Ity_I64); 
            IRTemp  du1 = newTemp(Ity_I64);
            IRTemp  du2 = newTemp(Ity_I64); 
            IRTemp  du3 = newTemp(Ity_I64);
            if (bL) {
               assign(di0, loadLE(Ity_I64, a0));
               assign(di1, loadLE(Ity_I64, a1));
               assign(di2, loadLE(Ity_I64, a2));
               assign(di3, loadLE(Ity_I64, a3));
               
               math_DEINTERLEAVE_2(&du0, &du2, di0, di1, 1 << size);
               math_DEINTERLEAVE_2(&du1, &du3, di2, di3, 1 << size);
               putDRegI64(rD + 0 * regstep, mkexpr(du0), IRTemp_INVALID);
               putDRegI64(rD + 1 * regstep, mkexpr(du1), IRTemp_INVALID);
               putDRegI64(rD + 2 * regstep, mkexpr(du2), IRTemp_INVALID);
               putDRegI64(rD + 3 * regstep, mkexpr(du3), IRTemp_INVALID);
            } else {
               assign(du0, getDRegI64(rD + 0 * regstep));
               assign(du1, getDRegI64(rD + 1 * regstep));
               assign(du2, getDRegI64(rD + 2 * regstep));
               assign(du3, getDRegI64(rD + 3 * regstep));
               
               math_INTERLEAVE_2(&di0, &di1, du0, du2, 1 << size);
               math_INTERLEAVE_2(&di2, &di3, du1, du3, 1 << size);
               storeLE(a0, mkexpr(di0));
               storeLE(a1, mkexpr(di1));
               storeLE(a2, mkexpr(di2));
               storeLE(a3, mkexpr(di3));
            }

            IRTemp tmp = newTemp(Ity_I32);
            assign(tmp, binop(Iop_Add32, mkexpr(addr), mkU32(32)));
            addr = tmp;
         }
      }
      else
      if (N == 2 ) {
         
         
         vassert(regs == 1 && (inc == 1 || inc == 2));
         IRExpr* a0  = binop(Iop_Add32, mkexpr(addr), mkU32(0));
         IRExpr* a1  = binop(Iop_Add32, mkexpr(addr), mkU32(8));
         IRExpr* a2  = binop(Iop_Add32, mkexpr(addr), mkU32(16));
         IRTemp  di0 = newTemp(Ity_I64);
         IRTemp  di1 = newTemp(Ity_I64);
         IRTemp  di2 = newTemp(Ity_I64);
         IRTemp  du0 = newTemp(Ity_I64); 
         IRTemp  du1 = newTemp(Ity_I64);
         IRTemp  du2 = newTemp(Ity_I64);
         if (bL) {
            assign(di0, loadLE(Ity_I64, a0));
            assign(di1, loadLE(Ity_I64, a1));
            assign(di2, loadLE(Ity_I64, a2));
            math_DEINTERLEAVE_3(&du0, &du1, &du2, di0, di1, di2, 1 << size);
            putDRegI64(rD + 0 * inc, mkexpr(du0), IRTemp_INVALID);
            putDRegI64(rD + 1 * inc, mkexpr(du1), IRTemp_INVALID);
            putDRegI64(rD + 2 * inc, mkexpr(du2), IRTemp_INVALID);
         } else {
            assign(du0, getDRegI64(rD + 0 * inc));
            assign(du1, getDRegI64(rD + 1 * inc));
            assign(du2, getDRegI64(rD + 2 * inc));
            math_INTERLEAVE_3(&di0, &di1, &di2, du0, du1, du2, 1 << size);
            storeLE(a0, mkexpr(di0));
            storeLE(a1, mkexpr(di1));
            storeLE(a2, mkexpr(di2));
         }
         IRTemp tmp = newTemp(Ity_I32);
         assign(tmp, binop(Iop_Add32, mkexpr(addr), mkU32(24)));
         addr = tmp;
      }
      else 
      if (N == 3 ) {
         
         
         vassert(regs == 1 && (inc == 1 || inc == 2));
         IRExpr* a0  = binop(Iop_Add32, mkexpr(addr), mkU32(0));
         IRExpr* a1  = binop(Iop_Add32, mkexpr(addr), mkU32(8));
         IRExpr* a2  = binop(Iop_Add32, mkexpr(addr), mkU32(16));
         IRExpr* a3  = binop(Iop_Add32, mkexpr(addr), mkU32(24));
         IRTemp  di0 = newTemp(Ity_I64);
         IRTemp  di1 = newTemp(Ity_I64);
         IRTemp  di2 = newTemp(Ity_I64);
         IRTemp  di3 = newTemp(Ity_I64);
         IRTemp  du0 = newTemp(Ity_I64); 
         IRTemp  du1 = newTemp(Ity_I64);
         IRTemp  du2 = newTemp(Ity_I64);
         IRTemp  du3 = newTemp(Ity_I64);
         if (bL) {
            assign(di0, loadLE(Ity_I64, a0));
            assign(di1, loadLE(Ity_I64, a1));
            assign(di2, loadLE(Ity_I64, a2));
            assign(di3, loadLE(Ity_I64, a3));
            math_DEINTERLEAVE_4(&du0, &du1, &du2, &du3,
                                di0, di1, di2, di3, 1 << size);
            putDRegI64(rD + 0 * inc, mkexpr(du0), IRTemp_INVALID);
            putDRegI64(rD + 1 * inc, mkexpr(du1), IRTemp_INVALID);
            putDRegI64(rD + 2 * inc, mkexpr(du2), IRTemp_INVALID);
            putDRegI64(rD + 3 * inc, mkexpr(du3), IRTemp_INVALID);
         } else {
            assign(du0, getDRegI64(rD + 0 * inc));
            assign(du1, getDRegI64(rD + 1 * inc));
            assign(du2, getDRegI64(rD + 2 * inc));
            assign(du3, getDRegI64(rD + 3 * inc));
            math_INTERLEAVE_4(&di0, &di1, &di2, &di3,
                              du0, du1, du2, du3, 1 << size);
            storeLE(a0, mkexpr(di0));
            storeLE(a1, mkexpr(di1));
            storeLE(a2, mkexpr(di2));
            storeLE(a3, mkexpr(di3));
         }
         IRTemp tmp = newTemp(Ity_I32);
         assign(tmp, binop(Iop_Add32, mkexpr(addr), mkU32(32)));
         addr = tmp;
      }
      else {
         vassert(0);
      }

      
      if (rM != 15) {
         IRExpr* e;
         if (rM == 13) {
            e = binop(Iop_Add32, mkexpr(initialRn),
                                 mkU32(8 * (N + 1) * regs));
         } else {
            e = binop(Iop_Add32, mkexpr(initialRn),
                                 mkexpr(initialRm));
         }
         if (isT)
            putIRegT(rN, e, IRTemp_INVALID);
         else
            putIRegA(rN, e, IRTemp_INVALID, Ijk_Boring);
      }

      DIP("v%s%u.%u {", bL ? "ld" : "st", N + 1, 8 << INSN(7,6));
      if ((inc == 1 && regs * (N + 1) > 1)
          || (inc == 2 && regs > 1 && N > 0)) {
         DIP("d%u-d%u", rD, rD + regs * (N + 1) - 1);
      } else {
         UInt r;
         for (r = 0; r < regs; r++) {
            for (i = 0; i <= N; i++) {
               if (i || r)
                  DIP(", ");
               DIP("d%u", rD + r + i * inc);
            }
         }
      }
      DIP("}, [r%u]", rN);
      if (rM != 13 && rM != 15) {
         DIP(", r%u\n", rM);
      } else {
         DIP("%s\n", (rM != 15) ? "!" : "");
      }
      return True;
   }
#  undef INSN
}




static Bool decode_NEON_instruction (
               DisResult* dres,
               UInt              insn32,
               IRTemp            condT,
               Bool              isT
            )
{
#  define INSN(_bMax,_bMin)  SLICE_UInt(insn32, (_bMax), (_bMin))


   
   if (!isT)
      vassert(condT == IRTemp_INVALID);

   if (!isT && INSN(31,25) == BITS7(1,1,1,1,0,0,1)) {
      
      return dis_neon_data_processing(INSN(31,0), condT);
   }
   if (isT && INSN(31,29) == BITS3(1,1,1)
       && INSN(27,24) == BITS4(1,1,1,1)) {
      
      UInt reformatted = INSN(23,0);
      reformatted |= (INSN(28,28) << 24); 
      reformatted |= (BITS7(1,1,1,1,0,0,1) << 25);
      return dis_neon_data_processing(reformatted, condT);
   }

   if (!isT && INSN(31,24) == BITS8(1,1,1,1,0,1,0,0)) {
      
      return dis_neon_load_or_store(INSN(31,0), isT, condT);
   }
   if (isT && INSN(31,24) == BITS8(1,1,1,1,1,0,0,1)) {
      UInt reformatted = INSN(23,0);
      reformatted |= (BITS8(1,1,1,1,0,1,0,0) << 24);
      return dis_neon_load_or_store(reformatted, isT, condT);
   }

   
   return False;

#  undef INSN
}




static Bool decode_V6MEDIA_instruction (
               DisResult* dres,
               UInt              insnv6m,
               IRTemp            condT,
               ARMCondcode       conq,
               Bool              isT
            )
{
#  define INSNA(_bMax,_bMin)   SLICE_UInt(insnv6m, (_bMax), (_bMin))
#  define INSNT0(_bMax,_bMin)  SLICE_UInt( ((insnv6m >> 16) & 0xFFFF), \
                                           (_bMax), (_bMin) )
#  define INSNT1(_bMax,_bMin)  SLICE_UInt( ((insnv6m >> 0)  & 0xFFFF), \
                                           (_bMax), (_bMin) )
   HChar dis_buf[128];
   dis_buf[0] = 0;

   if (isT) {
      vassert(conq == ARMCondAL);
   } else {
      vassert(INSNA(31,28) == BITS4(0,0,0,0)); 
      vassert(conq >= ARMCondEQ && conq <= ARMCondAL);
   }

   
   {
     UInt regD = 99, regM = 99, regN = 99, bitM = 0, bitN = 0;
     Bool gate = False;

     if (isT) {
        if (INSNT0(15,4) == 0xFB1 && INSNT1(15,12) == BITS4(1,1,1,1)
            && INSNT1(7,6) == BITS2(0,0)) {
           regD = INSNT1(11,8);
           regM = INSNT1(3,0);
           regN = INSNT0(3,0);
           bitM = INSNT1(4,4);
           bitN = INSNT1(5,5);
           if (!isBadRegT(regD) && !isBadRegT(regN) && !isBadRegT(regM))
              gate = True;
        }
     } else {
        if (BITS8(0,0,0,1,0,1,1,0) == INSNA(27,20) &&
            BITS4(0,0,0,0)         == INSNA(15,12) &&
            BITS4(1,0,0,0)         == (INSNA(7,4) & BITS4(1,0,0,1)) ) {
           regD = INSNA(19,16);
           regM = INSNA(11,8);
           regN = INSNA(3,0);
           bitM = INSNA(6,6);
           bitN = INSNA(5,5);
           if (regD != 15 && regN != 15 && regM != 15)
              gate = True;
        }
     }

     if (gate) {
        IRTemp srcN = newTemp(Ity_I32);
        IRTemp srcM = newTemp(Ity_I32);
        IRTemp res  = newTemp(Ity_I32);

        assign( srcN, binop(Iop_Sar32,
                            binop(Iop_Shl32,
                                  isT ? getIRegT(regN) : getIRegA(regN),
                                  mkU8(bitN ? 0 : 16)), mkU8(16)) );
        assign( srcM, binop(Iop_Sar32,
                            binop(Iop_Shl32,
                                  isT ? getIRegT(regM) : getIRegA(regM),
                                  mkU8(bitM ? 0 : 16)), mkU8(16)) );
        assign( res, binop(Iop_Mul32, mkexpr(srcN), mkexpr(srcM)) );

        if (isT)
           putIRegT( regD, mkexpr(res), condT );
        else
           putIRegA( regD, mkexpr(res), condT, Ijk_Boring );

        DIP( "smul%c%c%s r%u, r%u, r%u\n", bitN ? 't' : 'b', bitM ? 't' : 'b',
             nCC(conq), regD, regN, regM );
        return True;
     }
     
   }

   
   
   {
     UInt regD = 99, regN = 99, regM = 99, bitM = 0;
     Bool gate = False;

     if (isT) {
        if (INSNT0(15,4) == 0xFB3 && INSNT1(15,12) == BITS4(1,1,1,1)
            && INSNT1(7,5) == BITS3(0,0,0)) {
          regN = INSNT0(3,0);
          regD = INSNT1(11,8);
          regM = INSNT1(3,0);
          bitM = INSNT1(4,4);
          if (!isBadRegT(regD) && !isBadRegT(regN) && !isBadRegT(regM))
             gate = True;
        }
     } else {
        if (INSNA(27,20) == BITS8(0,0,0,1,0,0,1,0) && 
            INSNA(15,12) == BITS4(0,0,0,0)         &&
            (INSNA(7,4) & BITS4(1,0,1,1)) == BITS4(1,0,1,0)) {
           regD = INSNA(19,16);
           regN = INSNA(3,0);
           regM = INSNA(11,8);
           bitM = INSNA(6,6);
           if (regD != 15 && regN != 15 && regM != 15)
              gate = True;
        }
     }

     if (gate) {
        IRTemp irt_prod = newTemp(Ity_I64);

        assign( irt_prod, 
                binop(Iop_MullS32,
                      isT ? getIRegT(regN) : getIRegA(regN),
                      binop(Iop_Sar32, 
                            binop(Iop_Shl32,
                                  isT ? getIRegT(regM) : getIRegA(regM),
                                  mkU8(bitM ? 0 : 16)), 
                            mkU8(16))) );

        IRExpr* ire_result = binop(Iop_Or32, 
                                   binop( Iop_Shl32, 
                                          unop(Iop_64HIto32, mkexpr(irt_prod)), 
                                          mkU8(16) ), 
                                   binop( Iop_Shr32, 
                                          unop(Iop_64to32, mkexpr(irt_prod)), 
                                          mkU8(16) ) );

        if (isT)
           putIRegT( regD, ire_result, condT );
        else
           putIRegA( regD, ire_result, condT, Ijk_Boring );

        DIP("smulw%c%s r%u, r%u, r%u\n",
            bitM ? 't' : 'b', nCC(conq),regD,regN,regM);
        return True;
     }
     
   }

   
   
   {
     UInt regD = 99, regN = 99, regM = 99, imm5 = 99, shift_type = 99;
     Bool tbform = False;
     Bool gate = False;

     if (isT) {
        if (INSNT0(15,4) == 0xEAC 
            && INSNT1(15,15) == 0 && INSNT1(4,4) == 0) {
           regN = INSNT0(3,0);
           regD = INSNT1(11,8);
           regM = INSNT1(3,0);
           imm5 = (INSNT1(14,12) << 2) | INSNT1(7,6);
           shift_type = (INSNT1(5,5) << 1) | 0;
           tbform = (INSNT1(5,5) == 0) ? False : True;
           if (!isBadRegT(regD) && !isBadRegT(regN) && !isBadRegT(regM))
              gate = True;
        }
     } else {
        if (INSNA(27,20) == BITS8(0,1,1,0,1,0,0,0) &&
            INSNA(5,4)   == BITS2(0,1)             &&
            (INSNA(6,6)  == 0 || INSNA(6,6) == 1) ) {
           regD = INSNA(15,12);
           regN = INSNA(19,16);
           regM = INSNA(3,0);
           imm5 = INSNA(11,7);
           shift_type = (INSNA(6,6) << 1) | 0;
           tbform = (INSNA(6,6) == 0) ? False : True;
           if (regD != 15 && regN != 15 && regM != 15)
              gate = True;
        }
     }

     if (gate) {
        IRTemp irt_regM       = newTemp(Ity_I32);
        IRTemp irt_regM_shift = newTemp(Ity_I32);
        assign( irt_regM, isT ? getIRegT(regM) : getIRegA(regM) );
        compute_result_and_C_after_shift_by_imm5(
           dis_buf, &irt_regM_shift, NULL, irt_regM, shift_type, imm5, regM );

        UInt mask = (tbform == True) ? 0x0000FFFF : 0xFFFF0000;
        IRExpr* ire_result 
          = binop( Iop_Or32, 
                   binop(Iop_And32, mkexpr(irt_regM_shift), mkU32(mask)), 
                   binop(Iop_And32, isT ? getIRegT(regN) : getIRegA(regN),
                                    unop(Iop_Not32, mkU32(mask))) );

        if (isT)
           putIRegT( regD, ire_result, condT );
        else
           putIRegA( regD, ire_result, condT, Ijk_Boring );

        DIP( "pkh%s%s r%u, r%u, r%u %s\n", tbform ? "tb" : "bt", 
             nCC(conq), regD, regN, regM, dis_buf );

        return True;
     }
     
   }

   
   {
     UInt regD = 99, regN = 99, shift_type = 99, imm5 = 99, sat_imm = 99;
     Bool gate = False;

     if (isT) {
        if (INSNT0(15,6) == BITS10(1,1,1,1,0,0,1,1,1,0)
            && INSNT0(4,4) == 0
            && INSNT1(15,15) == 0 && INSNT1(5,5) == 0) {
           regD       = INSNT1(11,8);
           regN       = INSNT0(3,0);
           shift_type = (INSNT0(5,5) << 1) | 0;
           imm5       = (INSNT1(14,12) << 2) | INSNT1(7,6);
           sat_imm    = INSNT1(4,0);
           if (!isBadRegT(regD) && !isBadRegT(regN))
              gate = True;
           if (shift_type == BITS2(1,0) && imm5 == 0)
              gate = False;
        }
     } else {
        if (INSNA(27,21) == BITS7(0,1,1,0,1,1,1) &&
            INSNA(5,4)   == BITS2(0,1)) {
           regD       = INSNA(15,12);
           regN       = INSNA(3,0);
           shift_type = (INSNA(6,6) << 1) | 0;
           imm5       = INSNA(11,7);
           sat_imm    = INSNA(20,16);
           if (regD != 15 && regN != 15)
              gate = True;
        }
     }

     if (gate) {
        IRTemp irt_regN       = newTemp(Ity_I32);
        IRTemp irt_regN_shift = newTemp(Ity_I32);
        IRTemp irt_sat_Q      = newTemp(Ity_I32);
        IRTemp irt_result     = newTemp(Ity_I32);

        assign( irt_regN, isT ? getIRegT(regN) : getIRegA(regN) );
        compute_result_and_C_after_shift_by_imm5(
                dis_buf, &irt_regN_shift, NULL,
                irt_regN, shift_type, imm5, regN );

        armUnsignedSatQ( &irt_result, &irt_sat_Q, irt_regN_shift, sat_imm );
        or_into_QFLAG32( mkexpr(irt_sat_Q), condT );

        if (isT)
           putIRegT( regD, mkexpr(irt_result), condT );
        else
           putIRegA( regD, mkexpr(irt_result), condT, Ijk_Boring );

        DIP("usat%s r%u, #0x%04x, %s\n",
            nCC(conq), regD, imm5, dis_buf);
        return True;
     }
     
   }

  
   {
     UInt regD = 99, regN = 99, shift_type = 99, imm5 = 99, sat_imm = 99;
     Bool gate = False;

     if (isT) {
        if (INSNT0(15,6) == BITS10(1,1,1,1,0,0,1,1,0,0)
            && INSNT0(4,4) == 0
            && INSNT1(15,15) == 0 && INSNT1(5,5) == 0) {
           regD       = INSNT1(11,8);
           regN       = INSNT0(3,0);
           shift_type = (INSNT0(5,5) << 1) | 0;
           imm5       = (INSNT1(14,12) << 2) | INSNT1(7,6);
           sat_imm    = INSNT1(4,0) + 1;
           if (!isBadRegT(regD) && !isBadRegT(regN))
              gate = True;
           if (shift_type == BITS2(1,0) && imm5 == 0)
              gate = False;
        }
     } else {
        if (INSNA(27,21) == BITS7(0,1,1,0,1,0,1) &&
            INSNA(5,4)   == BITS2(0,1)) {
           regD       = INSNA(15,12);
           regN       = INSNA(3,0);
           shift_type = (INSNA(6,6) << 1) | 0;
           imm5       = INSNA(11,7);
           sat_imm    = INSNA(20,16) + 1;
           if (regD != 15 && regN != 15)
              gate = True;
        }
     }

     if (gate) {
        IRTemp irt_regN       = newTemp(Ity_I32);
        IRTemp irt_regN_shift = newTemp(Ity_I32);
        IRTemp irt_sat_Q      = newTemp(Ity_I32);
        IRTemp irt_result     = newTemp(Ity_I32);

        assign( irt_regN, isT ? getIRegT(regN) : getIRegA(regN) );
        compute_result_and_C_after_shift_by_imm5(
                dis_buf, &irt_regN_shift, NULL,
                irt_regN, shift_type, imm5, regN );

        armSignedSatQ( irt_regN_shift, sat_imm, &irt_result, &irt_sat_Q );
        or_into_QFLAG32( mkexpr(irt_sat_Q), condT );

        if (isT)
           putIRegT( regD, mkexpr(irt_result), condT );
        else
           putIRegA( regD, mkexpr(irt_result), condT, Ijk_Boring );

        DIP( "ssat%s r%u, #0x%04x, %s\n",
             nCC(conq), regD, imm5, dis_buf);
        return True;
    }
    
  }

   
   {
     UInt regD = 99, regN = 99, sat_imm = 99;
     Bool gate = False;

     if (isT) {
        if (INSNT0(15,6) == BITS10(1,1,1,1,0,0,1,1,0,0)
            && INSNT0(5,4) == BITS2(1,0)
            && INSNT1(15,12) == BITS4(0,0,0,0)
            && INSNT1(7,4) == BITS4(0,0,0,0)) {
           regD       = INSNT1(11,8);
           regN       = INSNT0(3,0);
           sat_imm    = INSNT1(3,0) + 1;
           if (!isBadRegT(regD) && !isBadRegT(regN))
              gate = True;
        }
     } else {
        if (INSNA(27,20) == BITS8(0,1,1,0,1,0,1,0) &&
            INSNA(11,4)   == BITS8(1,1,1,1,0,0,1,1)) {
           regD       = INSNA(15,12);
           regN       = INSNA(3,0);
           sat_imm    = INSNA(19,16) + 1;
           if (regD != 15 && regN != 15)
              gate = True;
        }
     }

     if (gate) {
        IRTemp irt_regN    = newTemp(Ity_I32);
        IRTemp irt_regN_lo = newTemp(Ity_I32);
        IRTemp irt_regN_hi = newTemp(Ity_I32);
        IRTemp irt_Q_lo    = newTemp(Ity_I32);
        IRTemp irt_Q_hi    = newTemp(Ity_I32);
        IRTemp irt_res_lo  = newTemp(Ity_I32);
        IRTemp irt_res_hi  = newTemp(Ity_I32);

        assign( irt_regN, isT ? getIRegT(regN) : getIRegA(regN) );
        assign( irt_regN_lo,
                binop( Iop_Sar32,
                       binop(Iop_Shl32, mkexpr(irt_regN), mkU8(16)),
                       mkU8(16)) );
        assign( irt_regN_hi, binop(Iop_Sar32, mkexpr(irt_regN), mkU8(16)) );

        armSignedSatQ( irt_regN_lo, sat_imm, &irt_res_lo, &irt_Q_lo );
        or_into_QFLAG32( mkexpr(irt_Q_lo), condT );

        armSignedSatQ( irt_regN_hi, sat_imm, &irt_res_hi, &irt_Q_hi );
        or_into_QFLAG32( mkexpr(irt_Q_hi), condT );

        IRExpr* ire_result 
           = binop(Iop_Or32, 
                   binop(Iop_And32, mkexpr(irt_res_lo), mkU32(0xFFFF)),
                   binop(Iop_Shl32, mkexpr(irt_res_hi), mkU8(16)));
        if (isT)
           putIRegT( regD, ire_result, condT );
        else
           putIRegA( regD, ire_result, condT, Ijk_Boring );

        DIP( "ssat16%s r%u, #0x%04x, r%u\n", nCC(conq), regD, sat_imm, regN );
        return True;
     }
     
   }

   
   {
     UInt regD = 99, regN = 99, sat_imm = 99;
     Bool gate = False;

     if (isT) {
        if (INSNT0(15,4) == 0xF3A && (INSNT1(15,0) & 0xF0F0) == 0x0000) {
           regN = INSNT0(3,0);
           regD = INSNT1(11,8);
           sat_imm = INSNT1(3,0);
           if (!isBadRegT(regD) && !isBadRegT(regN))
              gate = True;
       }
     } else {
        if (INSNA(27,20) == BITS8(0,1,1,0,1,1,1,0) &&
            INSNA(11,8)  == BITS4(1,1,1,1)         &&
            INSNA(7,4)   == BITS4(0,0,1,1)) {
           regD    = INSNA(15,12);
           regN    = INSNA(3,0);
           sat_imm = INSNA(19,16);
           if (regD != 15 && regN != 15)
              gate = True;
        }
     }

     if (gate) {
        IRTemp irt_regN    = newTemp(Ity_I32);
        IRTemp irt_regN_lo = newTemp(Ity_I32);
        IRTemp irt_regN_hi = newTemp(Ity_I32);
        IRTemp irt_Q_lo    = newTemp(Ity_I32);
        IRTemp irt_Q_hi    = newTemp(Ity_I32);
        IRTemp irt_res_lo  = newTemp(Ity_I32);
        IRTemp irt_res_hi  = newTemp(Ity_I32);

        assign( irt_regN, isT ? getIRegT(regN) : getIRegA(regN) );
        assign( irt_regN_lo, binop( Iop_Sar32, 
                                    binop(Iop_Shl32, mkexpr(irt_regN), mkU8(16)), 
                                    mkU8(16)) );
        assign( irt_regN_hi, binop(Iop_Sar32, mkexpr(irt_regN), mkU8(16)) );

        armUnsignedSatQ( &irt_res_lo, &irt_Q_lo, irt_regN_lo, sat_imm );
        or_into_QFLAG32( mkexpr(irt_Q_lo), condT );

        armUnsignedSatQ( &irt_res_hi, &irt_Q_hi, irt_regN_hi, sat_imm );
        or_into_QFLAG32( mkexpr(irt_Q_hi), condT );

        IRExpr* ire_result = binop( Iop_Or32, 
                                    binop(Iop_Shl32, mkexpr(irt_res_hi), mkU8(16)),
                                    mkexpr(irt_res_lo) );

        if (isT)
           putIRegT( regD, ire_result, condT );
        else
           putIRegA( regD, ire_result, condT, Ijk_Boring );

        DIP( "usat16%s r%u, #0x%04x, r%u\n", nCC(conq), regD, sat_imm, regN );
        return True;
     }
     
   }

   
   {
     UInt regD = 99, regN = 99, regM = 99;
     Bool gate = False;

     if (isT) {
        if (INSNT0(15,4) == 0xFA9 && (INSNT1(15,0) & 0xF0F0) == 0xF040) {
           regN = INSNT0(3,0);
           regD = INSNT1(11,8);
           regM = INSNT1(3,0);
           if (!isBadRegT(regD) && !isBadRegT(regN) && !isBadRegT(regM))
              gate = True;
        }
     } else {
        if (INSNA(27,20) == BITS8(0,1,1,0,0,1,0,1) && 
            INSNA(11,8)  == BITS4(1,1,1,1)         &&
            INSNA(7,4)   == BITS4(0,0,0,1)) {
           regD = INSNA(15,12);
           regN = INSNA(19,16);
           regM = INSNA(3,0);
           if (regD != 15 && regN != 15 && regM != 15)
              gate = True;
        }
     }

     if (gate) {
        IRTemp rNt  = newTemp(Ity_I32);
        IRTemp rMt  = newTemp(Ity_I32);
        IRTemp res  = newTemp(Ity_I32);
        IRTemp reso = newTemp(Ity_I32);

        assign( rNt, isT ? getIRegT(regN) : getIRegA(regN) );
        assign( rMt, isT ? getIRegT(regM) : getIRegA(regM) );

        assign(res, binop(Iop_Add16x2, mkexpr(rNt), mkexpr(rMt)));
        if (isT)
           putIRegT( regD, mkexpr(res), condT );
        else
           putIRegA( regD, mkexpr(res), condT, Ijk_Boring );

        assign(reso, binop(Iop_HAdd16Ux2, mkexpr(rNt), mkexpr(rMt)));
        set_GE_32_10_from_bits_31_15(reso, condT);

        DIP("uadd16%s r%u, r%u, r%u\n", nCC(conq),regD,regN,regM);
        return True;
     }
     
   }

   
   {
     UInt regD = 99, regN = 99, regM = 99;
     Bool gate = False;

     if (isT) {
        if (INSNT0(15,4) == 0xFA9 && (INSNT1(15,0) & 0xF0F0) == 0xF000) {
           regN = INSNT0(3,0);
           regD = INSNT1(11,8);
           regM = INSNT1(3,0);
           if (!isBadRegT(regD) && !isBadRegT(regN) && !isBadRegT(regM))
              gate = True;
        }
     } else {
        if (INSNA(27,20) == BITS8(0,1,1,0,0,0,0,1) && 
            INSNA(11,8)  == BITS4(1,1,1,1)         &&
            INSNA(7,4)   == BITS4(0,0,0,1)) {
           regD = INSNA(15,12);
           regN = INSNA(19,16);
           regM = INSNA(3,0);
           if (regD != 15 && regN != 15 && regM != 15)
              gate = True;
        }
     }

     if (gate) {
        IRTemp rNt  = newTemp(Ity_I32);
        IRTemp rMt  = newTemp(Ity_I32);
        IRTemp res  = newTemp(Ity_I32);
        IRTemp reso = newTemp(Ity_I32);

        assign( rNt, isT ? getIRegT(regN) : getIRegA(regN) );
        assign( rMt, isT ? getIRegT(regM) : getIRegA(regM) );

        assign(res, binop(Iop_Add16x2, mkexpr(rNt), mkexpr(rMt)));
        if (isT)
           putIRegT( regD, mkexpr(res), condT );
        else
           putIRegA( regD, mkexpr(res), condT, Ijk_Boring );

        assign(reso, unop(Iop_Not32,
                          binop(Iop_HAdd16Sx2, mkexpr(rNt), mkexpr(rMt))));
        set_GE_32_10_from_bits_31_15(reso, condT);

        DIP("sadd16%s r%u, r%u, r%u\n", nCC(conq),regD,regN,regM);
        return True;
     }
     
   }

   
   {
     UInt regD = 99, regN = 99, regM = 99;
     Bool gate = False;

     if (isT) {
        if (INSNT0(15,4) == 0xFAD && (INSNT1(15,0) & 0xF0F0) == 0xF040) {
           regN = INSNT0(3,0);
           regD = INSNT1(11,8);
           regM = INSNT1(3,0);
           if (!isBadRegT(regD) && !isBadRegT(regN) && !isBadRegT(regM))
              gate = True;
        }
     } else {
        if (INSNA(27,20) == BITS8(0,1,1,0,0,1,0,1) && 
            INSNA(11,8)  == BITS4(1,1,1,1)         &&
            INSNA(7,4)   == BITS4(0,1,1,1)) {
           regD = INSNA(15,12);
           regN = INSNA(19,16);
           regM = INSNA(3,0);
           if (regD != 15 && regN != 15 && regM != 15)
             gate = True;
        }
     }

     if (gate) {
        IRTemp rNt  = newTemp(Ity_I32);
        IRTemp rMt  = newTemp(Ity_I32);
        IRTemp res  = newTemp(Ity_I32);
        IRTemp reso = newTemp(Ity_I32);

        assign( rNt, isT ? getIRegT(regN) : getIRegA(regN) );
        assign( rMt, isT ? getIRegT(regM) : getIRegA(regM) );

        assign(res, binop(Iop_Sub16x2, mkexpr(rNt), mkexpr(rMt)));
        if (isT)
           putIRegT( regD, mkexpr(res), condT );
        else
           putIRegA( regD, mkexpr(res), condT, Ijk_Boring );

        assign(reso, unop(Iop_Not32,
                          binop(Iop_HSub16Ux2, mkexpr(rNt), mkexpr(rMt))));
        set_GE_32_10_from_bits_31_15(reso, condT);

        DIP("usub16%s r%u, r%u, r%u\n", nCC(conq),regD,regN,regM);
        return True;
     }
     
   }

   
   {
     UInt regD = 99, regN = 99, regM = 99;
     Bool gate = False;

     if (isT) {
        if (INSNT0(15,4) == 0xFAD && (INSNT1(15,0) & 0xF0F0) == 0xF000) {
           regN = INSNT0(3,0);
           regD = INSNT1(11,8);
           regM = INSNT1(3,0);
           if (!isBadRegT(regD) && !isBadRegT(regN) && !isBadRegT(regM))
              gate = True;
        }
     } else {
        if (INSNA(27,20) == BITS8(0,1,1,0,0,0,0,1) && 
            INSNA(11,8)  == BITS4(1,1,1,1)         &&
            INSNA(7,4)   == BITS4(0,1,1,1)) {
           regD = INSNA(15,12);
           regN = INSNA(19,16);
           regM = INSNA(3,0);
           if (regD != 15 && regN != 15 && regM != 15)
              gate = True;
        }
     }

     if (gate) {
        IRTemp rNt  = newTemp(Ity_I32);
        IRTemp rMt  = newTemp(Ity_I32);
        IRTemp res  = newTemp(Ity_I32);
        IRTemp reso = newTemp(Ity_I32);

        assign( rNt, isT ? getIRegT(regN) : getIRegA(regN) );
        assign( rMt, isT ? getIRegT(regM) : getIRegA(regM) );

        assign(res, binop(Iop_Sub16x2, mkexpr(rNt), mkexpr(rMt)));
        if (isT)
           putIRegT( regD, mkexpr(res), condT );
        else
           putIRegA( regD, mkexpr(res), condT, Ijk_Boring );

        assign(reso, unop(Iop_Not32,
                          binop(Iop_HSub16Sx2, mkexpr(rNt), mkexpr(rMt))));
        set_GE_32_10_from_bits_31_15(reso, condT);

        DIP("ssub16%s r%u, r%u, r%u\n", nCC(conq),regD,regN,regM);
        return True;
     }
     
   }

   
   {
     UInt regD = 99, regN = 99, regM = 99;
     Bool gate = False;

     if (isT) {
        if (INSNT0(15,4) == 0xFA8 && (INSNT1(15,0) & 0xF0F0) == 0xF040) {
           regN = INSNT0(3,0);
           regD = INSNT1(11,8);
           regM = INSNT1(3,0);
           if (!isBadRegT(regD) && !isBadRegT(regN) && !isBadRegT(regM))
              gate = True;
        }
     } else {
        if (INSNA(27,20) == BITS8(0,1,1,0,0,1,0,1) &&
            INSNA(11,8)  == BITS4(1,1,1,1)         &&
            (INSNA(7,4)  == BITS4(1,0,0,1))) {
           regD = INSNA(15,12);
           regN = INSNA(19,16);
           regM = INSNA(3,0);
           if (regD != 15 && regN != 15 && regM != 15)
              gate = True;
        }
     }

     if (gate) {
        IRTemp rNt  = newTemp(Ity_I32);
        IRTemp rMt  = newTemp(Ity_I32);
        IRTemp res  = newTemp(Ity_I32);
        IRTemp reso = newTemp(Ity_I32);

        assign( rNt, isT ? getIRegT(regN) : getIRegA(regN) );
        assign( rMt, isT ? getIRegT(regM) : getIRegA(regM) );

        assign(res, binop(Iop_Add8x4, mkexpr(rNt), mkexpr(rMt)));
        if (isT)
           putIRegT( regD, mkexpr(res), condT );
        else
           putIRegA( regD, mkexpr(res), condT, Ijk_Boring );

        assign(reso, binop(Iop_HAdd8Ux4, mkexpr(rNt), mkexpr(rMt)));
        set_GE_3_2_1_0_from_bits_31_23_15_7(reso, condT);

        DIP("uadd8%s r%u, r%u, r%u\n", nCC(conq),regD,regN,regM);
        return True;
     }
     
   }

   
   {
     UInt regD = 99, regN = 99, regM = 99;
     Bool gate = False;

     if (isT) {
        if (INSNT0(15,4) == 0xFA8 && (INSNT1(15,0) & 0xF0F0) == 0xF000) {
           regN = INSNT0(3,0);
           regD = INSNT1(11,8);
           regM = INSNT1(3,0);
           if (!isBadRegT(regD) && !isBadRegT(regN) && !isBadRegT(regM))
              gate = True;
        }
     } else {
        if (INSNA(27,20) == BITS8(0,1,1,0,0,0,0,1) &&
            INSNA(11,8)  == BITS4(1,1,1,1)         &&
            (INSNA(7,4)  == BITS4(1,0,0,1))) {
           regD = INSNA(15,12);
           regN = INSNA(19,16);
           regM = INSNA(3,0);
           if (regD != 15 && regN != 15 && regM != 15)
              gate = True;
        }
     }

     if (gate) {
        IRTemp rNt  = newTemp(Ity_I32);
        IRTemp rMt  = newTemp(Ity_I32);
        IRTemp res  = newTemp(Ity_I32);
        IRTemp reso = newTemp(Ity_I32);

        assign( rNt, isT ? getIRegT(regN) : getIRegA(regN) );
        assign( rMt, isT ? getIRegT(regM) : getIRegA(regM) );

        assign(res, binop(Iop_Add8x4, mkexpr(rNt), mkexpr(rMt)));
        if (isT)
           putIRegT( regD, mkexpr(res), condT );
        else
           putIRegA( regD, mkexpr(res), condT, Ijk_Boring );

        assign(reso, unop(Iop_Not32,
                          binop(Iop_HAdd8Sx4, mkexpr(rNt), mkexpr(rMt))));
        set_GE_3_2_1_0_from_bits_31_23_15_7(reso, condT);

        DIP("sadd8%s r%u, r%u, r%u\n", nCC(conq),regD,regN,regM);
        return True;
     }
     
   }

   
   {
     UInt regD = 99, regN = 99, regM = 99;
     Bool gate = False;

     if (isT) {
        if (INSNT0(15,4) == 0xFAC && (INSNT1(15,0) & 0xF0F0) == 0xF040) {
           regN = INSNT0(3,0);
           regD = INSNT1(11,8);
           regM = INSNT1(3,0);
           if (!isBadRegT(regD) && !isBadRegT(regN) && !isBadRegT(regM))
              gate = True;
        }
     } else {
        if (INSNA(27,20) == BITS8(0,1,1,0,0,1,0,1) &&
            INSNA(11,8)  == BITS4(1,1,1,1)         &&
            (INSNA(7,4)  == BITS4(1,1,1,1))) {
           regD = INSNA(15,12);
           regN = INSNA(19,16);
           regM = INSNA(3,0);
           if (regD != 15 && regN != 15 && regM != 15)
             gate = True;
        }
     }

     if (gate) {
        IRTemp rNt  = newTemp(Ity_I32);
        IRTemp rMt  = newTemp(Ity_I32);
        IRTemp res  = newTemp(Ity_I32);
        IRTemp reso = newTemp(Ity_I32);

        assign( rNt, isT ? getIRegT(regN) : getIRegA(regN) );
        assign( rMt, isT ? getIRegT(regM) : getIRegA(regM) );

        assign(res, binop(Iop_Sub8x4, mkexpr(rNt), mkexpr(rMt)));
        if (isT)
           putIRegT( regD, mkexpr(res), condT );
        else
           putIRegA( regD, mkexpr(res), condT, Ijk_Boring );

        assign(reso, unop(Iop_Not32,
                          binop(Iop_HSub8Ux4, mkexpr(rNt), mkexpr(rMt))));
        set_GE_3_2_1_0_from_bits_31_23_15_7(reso, condT);

        DIP("usub8%s r%u, r%u, r%u\n", nCC(conq),regD,regN,regM);
        return True;
     }
     
   }

   
   {
     UInt regD = 99, regN = 99, regM = 99;
     Bool gate = False;

     if (isT) {
        if (INSNT0(15,4) == 0xFAC && (INSNT1(15,0) & 0xF0F0) == 0xF000) {
           regN = INSNT0(3,0);
           regD = INSNT1(11,8);
           regM = INSNT1(3,0);
           if (!isBadRegT(regD) && !isBadRegT(regN) && !isBadRegT(regM))
              gate = True;
        }
     } else {
        if (INSNA(27,20) == BITS8(0,1,1,0,0,0,0,1) &&
            INSNA(11,8)  == BITS4(1,1,1,1)         &&
            INSNA(7,4)   == BITS4(1,1,1,1)) {
           regD = INSNA(15,12);
           regN = INSNA(19,16);
           regM = INSNA(3,0);
           if (regD != 15 && regN != 15 && regM != 15)
              gate = True;
        }
     }

     if (gate) {
        IRTemp rNt  = newTemp(Ity_I32);
        IRTemp rMt  = newTemp(Ity_I32);
        IRTemp res  = newTemp(Ity_I32);
        IRTemp reso = newTemp(Ity_I32);

        assign( rNt, isT ? getIRegT(regN) : getIRegA(regN) );
        assign( rMt, isT ? getIRegT(regM) : getIRegA(regM) );

        assign(res, binop(Iop_Sub8x4, mkexpr(rNt), mkexpr(rMt)));
        if (isT)
           putIRegT( regD, mkexpr(res), condT );
        else
           putIRegA( regD, mkexpr(res), condT, Ijk_Boring );

        assign(reso, unop(Iop_Not32,
                          binop(Iop_HSub8Sx4, mkexpr(rNt), mkexpr(rMt))));
        set_GE_3_2_1_0_from_bits_31_23_15_7(reso, condT);

        DIP("ssub8%s r%u, r%u, r%u\n", nCC(conq),regD,regN,regM);
        return True;
     }
     
   }

   
   {
     UInt regD = 99, regN = 99, regM = 99;
     Bool gate = False;

     if (isT) {
        if (INSNT0(15,4) == 0xFA8 && (INSNT1(15,0) & 0xF0F0) == 0xF010) {
           regN = INSNT0(3,0);
           regD = INSNT1(11,8);
           regM = INSNT1(3,0);
           if (!isBadRegT(regD) && !isBadRegT(regN) && !isBadRegT(regM))
              gate = True;
        }
     } else {
        if (INSNA(27,20) == BITS8(0,1,1,0,0,0,1,0) &&
            INSNA(11,8)  == BITS4(1,1,1,1)         &&
            INSNA(7,4)   == BITS4(1,0,0,1)) {
           regD = INSNA(15,12);
           regN = INSNA(19,16);
           regM = INSNA(3,0);
           if (regD != 15 && regN != 15 && regM != 15)
              gate = True;
        }
     }

     if (gate) {
        IRTemp rNt   = newTemp(Ity_I32);
        IRTemp rMt   = newTemp(Ity_I32);
        IRTemp res_q = newTemp(Ity_I32);

        assign( rNt, isT ? getIRegT(regN) : getIRegA(regN) );
        assign( rMt, isT ? getIRegT(regM) : getIRegA(regM) );

        assign(res_q, binop(Iop_QAdd8Sx4, mkexpr(rNt), mkexpr(rMt)));
        if (isT)
           putIRegT( regD, mkexpr(res_q), condT );
        else
           putIRegA( regD, mkexpr(res_q), condT, Ijk_Boring );

        DIP("qadd8%s r%u, r%u, r%u\n", nCC(conq),regD,regN,regM);
        return True;
     }
     
   }

   
   {
     UInt regD = 99, regN = 99, regM = 99;
     Bool gate = False;

     if (isT) {
        if (INSNT0(15,4) == 0xFAC && (INSNT1(15,0) & 0xF0F0) == 0xF010) {
           regN = INSNT0(3,0);
           regD = INSNT1(11,8);
           regM = INSNT1(3,0);
           if (!isBadRegT(regD) && !isBadRegT(regN) && !isBadRegT(regM))
              gate = True;
        }
     } else {
        if (INSNA(27,20) == BITS8(0,1,1,0,0,0,1,0) &&
            INSNA(11,8)  == BITS4(1,1,1,1)         &&
            INSNA(7,4)   == BITS4(1,1,1,1)) {
           regD = INSNA(15,12);
           regN = INSNA(19,16);
           regM = INSNA(3,0);
           if (regD != 15 && regN != 15 && regM != 15)
              gate = True;
        }
     }

     if (gate) {
        IRTemp rNt   = newTemp(Ity_I32);
        IRTemp rMt   = newTemp(Ity_I32);
        IRTemp res_q = newTemp(Ity_I32);

        assign( rNt, isT ? getIRegT(regN) : getIRegA(regN) );
        assign( rMt, isT ? getIRegT(regM) : getIRegA(regM) );

        assign(res_q, binop(Iop_QSub8Sx4, mkexpr(rNt), mkexpr(rMt)));
        if (isT)
           putIRegT( regD, mkexpr(res_q), condT );
        else
           putIRegA( regD, mkexpr(res_q), condT, Ijk_Boring );

        DIP("qsub8%s r%u, r%u, r%u\n", nCC(conq),regD,regN,regM);
        return True;
     }
     
   }

   
   {
     UInt regD = 99, regN = 99, regM = 99;
     Bool gate = False;

     if (isT) {
        if (INSNT0(15,4) == 0xFA8 && (INSNT1(15,0) & 0xF0F0) == 0xF050) {
           regN = INSNT0(3,0);
           regD = INSNT1(11,8);
           regM = INSNT1(3,0);
           if (!isBadRegT(regD) && !isBadRegT(regN) && !isBadRegT(regM))
              gate = True;
        }
     } else {
        if (INSNA(27,20) == BITS8(0,1,1,0,0,1,1,0) &&
            INSNA(11,8)  == BITS4(1,1,1,1)         &&
            (INSNA(7,4)  == BITS4(1,0,0,1))) {
           regD = INSNA(15,12);
           regN = INSNA(19,16);
           regM = INSNA(3,0);
           if (regD != 15 && regN != 15 && regM != 15)
              gate = True;
        }
     }

     if (gate) {
        IRTemp rNt   = newTemp(Ity_I32);
        IRTemp rMt   = newTemp(Ity_I32);
        IRTemp res_q = newTemp(Ity_I32);

        assign( rNt, isT ? getIRegT(regN) : getIRegA(regN) );
        assign( rMt, isT ? getIRegT(regM) : getIRegA(regM) );

        assign(res_q, binop(Iop_QAdd8Ux4, mkexpr(rNt), mkexpr(rMt)));
        if (isT)
           putIRegT( regD, mkexpr(res_q), condT );
        else
           putIRegA( regD, mkexpr(res_q), condT, Ijk_Boring );

        DIP("uqadd8%s r%u, r%u, r%u\n", nCC(conq),regD,regN,regM);
        return True;
     }
     
   }

   
   {
     UInt regD = 99, regN = 99, regM = 99;
     Bool gate = False;

     if (isT) {
        if (INSNT0(15,4) == 0xFAC && (INSNT1(15,0) & 0xF0F0) == 0xF050) {
           regN = INSNT0(3,0);
           regD = INSNT1(11,8);
           regM = INSNT1(3,0);
           if (!isBadRegT(regD) && !isBadRegT(regN) && !isBadRegT(regM))
              gate = True;
        }
     } else {
        if (INSNA(27,20) == BITS8(0,1,1,0,0,1,1,0) &&
            INSNA(11,8)  == BITS4(1,1,1,1)         &&
            (INSNA(7,4)  == BITS4(1,1,1,1))) {
           regD = INSNA(15,12);
           regN = INSNA(19,16);
           regM = INSNA(3,0);
           if (regD != 15 && regN != 15 && regM != 15)
             gate = True;
        }
     }

     if (gate) {
        IRTemp rNt   = newTemp(Ity_I32);
        IRTemp rMt   = newTemp(Ity_I32);
        IRTemp res_q = newTemp(Ity_I32);

        assign( rNt, isT ? getIRegT(regN) : getIRegA(regN) );
        assign( rMt, isT ? getIRegT(regM) : getIRegA(regM) );

        assign(res_q, binop(Iop_QSub8Ux4, mkexpr(rNt), mkexpr(rMt)));
        if (isT)
           putIRegT( regD, mkexpr(res_q), condT );
        else
           putIRegA( regD, mkexpr(res_q), condT, Ijk_Boring );

        DIP("uqsub8%s r%u, r%u, r%u\n", nCC(conq),regD,regN,regM);
        return True;
     }
     
   }

   
   {
     UInt regD = 99, regN = 99, regM = 99;
     Bool gate = False;

     if (isT) {
        if (INSNT0(15,4) == 0xFA8 && (INSNT1(15,0) & 0xF0F0) == 0xF060) {
           regN = INSNT0(3,0);
           regD = INSNT1(11,8);
           regM = INSNT1(3,0);
           if (!isBadRegT(regD) && !isBadRegT(regN) && !isBadRegT(regM))
              gate = True;
        }
     } else {
        if (INSNA(27,20) == BITS8(0,1,1,0,0,1,1,1) &&
            INSNA(11,8)  == BITS4(1,1,1,1)         &&
            INSNA(7,4)   == BITS4(1,0,0,1)) {
           regD = INSNA(15,12);
           regN = INSNA(19,16);
           regM = INSNA(3,0);
           if (regD != 15 && regN != 15 && regM != 15)
              gate = True;
        }
     }

     if (gate) {
        IRTemp rNt   = newTemp(Ity_I32);
        IRTemp rMt   = newTemp(Ity_I32);
        IRTemp res_q = newTemp(Ity_I32);

        assign( rNt, isT ? getIRegT(regN) : getIRegA(regN) );
        assign( rMt, isT ? getIRegT(regM) : getIRegA(regM) );

        assign(res_q, binop(Iop_HAdd8Ux4, mkexpr(rNt), mkexpr(rMt)));
        if (isT)
           putIRegT( regD, mkexpr(res_q), condT );
        else
           putIRegA( regD, mkexpr(res_q), condT, Ijk_Boring );

        DIP("uhadd8%s r%u, r%u, r%u\n", nCC(conq),regD,regN,regM);
        return True;
     }
     
   }

   
   {
     UInt regD = 99, regN = 99, regM = 99;
     Bool gate = False;

     if (isT) {
        if (INSNT0(15,4) == 0xFA9 && (INSNT1(15,0) & 0xF0F0) == 0xF060) {
           regN = INSNT0(3,0);
           regD = INSNT1(11,8);
           regM = INSNT1(3,0);
           if (!isBadRegT(regD) && !isBadRegT(regN) && !isBadRegT(regM))
              gate = True;
        }
     } else {
        if (INSNA(27,20) == BITS8(0,1,1,0,0,1,1,1) &&
            INSNA(11,8)  == BITS4(1,1,1,1)         &&
            INSNA(7,4)   == BITS4(0,0,0,1)) {
           regD = INSNA(15,12);
           regN = INSNA(19,16);
           regM = INSNA(3,0);
           if (regD != 15 && regN != 15 && regM != 15)
              gate = True;
        }
     }

     if (gate) {
        IRTemp rNt   = newTemp(Ity_I32);
        IRTemp rMt   = newTemp(Ity_I32);
        IRTemp res_q = newTemp(Ity_I32);

        assign( rNt, isT ? getIRegT(regN) : getIRegA(regN) );
        assign( rMt, isT ? getIRegT(regM) : getIRegA(regM) );

        assign(res_q, binop(Iop_HAdd16Ux2, mkexpr(rNt), mkexpr(rMt)));
        if (isT)
           putIRegT( regD, mkexpr(res_q), condT );
        else
           putIRegA( regD, mkexpr(res_q), condT, Ijk_Boring );

        DIP("uhadd16%s r%u, r%u, r%u\n", nCC(conq),regD,regN,regM);
        return True;
     }
     
   }

   
   {
     UInt regD = 99, regN = 99, regM = 99;
     Bool gate = False;

     if (isT) {
        if (INSNT0(15,4) == 0xFA8 && (INSNT1(15,0) & 0xF0F0) == 0xF020) {
           regN = INSNT0(3,0);
           regD = INSNT1(11,8);
           regM = INSNT1(3,0);
           if (!isBadRegT(regD) && !isBadRegT(regN) && !isBadRegT(regM))
              gate = True;
        }
     } else {
        if (INSNA(27,20) == BITS8(0,1,1,0,0,0,1,1) &&
            INSNA(11,8)  == BITS4(1,1,1,1)         &&
            INSNA(7,4)   == BITS4(1,0,0,1)) {
           regD = INSNA(15,12);
           regN = INSNA(19,16);
           regM = INSNA(3,0);
           if (regD != 15 && regN != 15 && regM != 15)
              gate = True;
        }
     }

     if (gate) {
        IRTemp rNt   = newTemp(Ity_I32);
        IRTemp rMt   = newTemp(Ity_I32);
        IRTemp res_q = newTemp(Ity_I32);

        assign( rNt, isT ? getIRegT(regN) : getIRegA(regN) );
        assign( rMt, isT ? getIRegT(regM) : getIRegA(regM) );

        assign(res_q, binop(Iop_HAdd8Sx4, mkexpr(rNt), mkexpr(rMt)));
        if (isT)
           putIRegT( regD, mkexpr(res_q), condT );
        else
           putIRegA( regD, mkexpr(res_q), condT, Ijk_Boring );

        DIP("shadd8%s r%u, r%u, r%u\n", nCC(conq),regD,regN,regM);
        return True;
     }
     
   }

   
   {
     UInt regD = 99, regN = 99, regM = 99;
     Bool gate = False;

     if (isT) {
        if (INSNT0(15,4) == 0xFA9 && (INSNT1(15,0) & 0xF0F0) == 0xF010) {
           regN = INSNT0(3,0);
           regD = INSNT1(11,8);
           regM = INSNT1(3,0);
           if (!isBadRegT(regD) && !isBadRegT(regN) && !isBadRegT(regM))
              gate = True;
        }
     } else {
        if (INSNA(27,20) == BITS8(0,1,1,0,0,0,1,0) &&
            INSNA(11,8)  == BITS4(1,1,1,1)         &&
            INSNA(7,4)   == BITS4(0,0,0,1)) {
           regD = INSNA(15,12);
           regN = INSNA(19,16);
           regM = INSNA(3,0);
           if (regD != 15 && regN != 15 && regM != 15)
              gate = True;
        }
     }

     if (gate) {
        IRTemp rNt   = newTemp(Ity_I32);
        IRTemp rMt   = newTemp(Ity_I32);
        IRTemp res_q = newTemp(Ity_I32);

        assign( rNt, isT ? getIRegT(regN) : getIRegA(regN) );
        assign( rMt, isT ? getIRegT(regM) : getIRegA(regM) );

        assign(res_q, binop(Iop_QAdd16Sx2, mkexpr(rNt), mkexpr(rMt)));
        if (isT)
           putIRegT( regD, mkexpr(res_q), condT );
        else
           putIRegA( regD, mkexpr(res_q), condT, Ijk_Boring );

        DIP("qadd16%s r%u, r%u, r%u\n", nCC(conq),regD,regN,regM);
        return True;
     }
     
   }

   
   {
     UInt regD = 99, regN = 99, regM = 99;
     Bool gate = False;

      if (isT) {
        if (INSNT0(15,4) == 0xFAD && (INSNT1(15,0) & 0xF0F0) == 0xF010) {
           regN = INSNT0(3,0);
           regD = INSNT1(11,8);
           regM = INSNT1(3,0);
           if (!isBadRegT(regD) && !isBadRegT(regN) && !isBadRegT(regM))
              gate = True;
        }
     } else {
        if (INSNA(27,20) == BITS8(0,1,1,0,0,0,1,0) &&
            INSNA(11,8)  == BITS4(1,1,1,1)         &&
            INSNA(7,4)   == BITS4(0,1,1,1)) {
           regD = INSNA(15,12);
           regN = INSNA(19,16);
           regM = INSNA(3,0);
           if (regD != 15 && regN != 15 && regM != 15)
             gate = True;
        }
     }

     if (gate) {
        IRTemp rNt   = newTemp(Ity_I32);
        IRTemp rMt   = newTemp(Ity_I32);
        IRTemp res_q = newTemp(Ity_I32);

        assign( rNt, isT ? getIRegT(regN) : getIRegA(regN) );
        assign( rMt, isT ? getIRegT(regM) : getIRegA(regM) );

        assign(res_q, binop(Iop_QSub16Sx2, mkexpr(rNt), mkexpr(rMt)));
        if (isT)
           putIRegT( regD, mkexpr(res_q), condT );
        else
           putIRegA( regD, mkexpr(res_q), condT, Ijk_Boring );

        DIP("qsub16%s r%u, r%u, r%u\n", nCC(conq),regD,regN,regM);
        return True;
     }
     
   }

   
   {
     UInt regD = 99, regN = 99, regM = 99;
     Bool gate = False;

     if (isT) {
        if (INSNT0(15,4) == 0xFAE && (INSNT1(15,0) & 0xF0F0) == 0xF010) {
           regN = INSNT0(3,0);
           regD = INSNT1(11,8);
           regM = INSNT1(3,0);
           if (!isBadRegT(regD) && !isBadRegT(regN) && !isBadRegT(regM))
              gate = True;
        }
     } else {
        if (INSNA(27,20) == BITS8(0,1,1,0,0,0,1,0) &&
            INSNA(11,8)  == BITS4(1,1,1,1)         &&
            INSNA(7,4)   == BITS4(0,1,0,1)) {
           regD = INSNA(15,12);
           regN = INSNA(19,16);
           regM = INSNA(3,0);
           if (regD != 15 && regN != 15 && regM != 15)
              gate = True;
        }
     }

     if (gate) {
        IRTemp irt_regN     = newTemp(Ity_I32);
        IRTemp irt_regM     = newTemp(Ity_I32);
        IRTemp irt_sum      = newTemp(Ity_I32);
        IRTemp irt_diff     = newTemp(Ity_I32);
        IRTemp irt_sum_res  = newTemp(Ity_I32);
        IRTemp irt_diff_res = newTemp(Ity_I32);

        assign( irt_regN, isT ? getIRegT(regN) : getIRegA(regN) );
        assign( irt_regM, isT ? getIRegT(regM) : getIRegA(regM) );

        assign( irt_diff, 
                binop( Iop_Sub32, 
                       binop( Iop_Sar32, mkexpr(irt_regN), mkU8(16) ),
                       binop( Iop_Sar32, 
                              binop(Iop_Shl32, mkexpr(irt_regM), mkU8(16)), 
                              mkU8(16) ) ) );
        armSignedSatQ( irt_diff, 0x10, &irt_diff_res, NULL);

        assign( irt_sum, 
                binop( Iop_Add32, 
                       binop( Iop_Sar32, 
                              binop( Iop_Shl32, mkexpr(irt_regN), mkU8(16) ), 
                              mkU8(16) ), 
                       binop( Iop_Sar32, mkexpr(irt_regM), mkU8(16) )) );
        armSignedSatQ( irt_sum, 0x10, &irt_sum_res, NULL );

        IRExpr* ire_result = binop( Iop_Or32, 
                                    binop( Iop_Shl32, mkexpr(irt_diff_res), 
                                           mkU8(16) ), 
                                    binop( Iop_And32, mkexpr(irt_sum_res), 
                                           mkU32(0xFFFF)) );

        if (isT) 
           putIRegT( regD, ire_result, condT );
        else
           putIRegA( regD, ire_result, condT, Ijk_Boring );

        DIP( "qsax%s r%u, r%u, r%u\n", nCC(conq), regD, regN, regM );
        return True;
     }
     
   }

   
   {
     UInt regD = 99, regN = 99, regM = 99;
     Bool gate = False;

     if (isT) {
        if (INSNT0(15,4) == 0xFAA && (INSNT1(15,0) & 0xF0F0) == 0xF010) {
           regN = INSNT0(3,0);
           regD = INSNT1(11,8);
           regM = INSNT1(3,0);
           if (!isBadRegT(regD) && !isBadRegT(regN) && !isBadRegT(regM))
              gate = True;
        }
     } else {
        if (INSNA(27,20) == BITS8(0,1,1,0,0,0,1,0) &&
            INSNA(11,8)  == BITS4(1,1,1,1)         &&
            INSNA(7,4)   == BITS4(0,0,1,1)) {
           regD = INSNA(15,12);
           regN = INSNA(19,16);
           regM = INSNA(3,0);
           if (regD != 15 && regN != 15 && regM != 15)
              gate = True;
        }
     }

     if (gate) {
        IRTemp irt_regN     = newTemp(Ity_I32);
        IRTemp irt_regM     = newTemp(Ity_I32);
        IRTemp irt_sum      = newTemp(Ity_I32);
        IRTemp irt_diff     = newTemp(Ity_I32);
        IRTemp irt_res_sum  = newTemp(Ity_I32);
        IRTemp irt_res_diff = newTemp(Ity_I32);

        assign( irt_regN, isT ? getIRegT(regN) : getIRegA(regN) );
        assign( irt_regM, isT ? getIRegT(regM) : getIRegA(regM) );

        assign( irt_diff,  
                binop( Iop_Sub32, 
                       binop( Iop_Sar32, 
                              binop( Iop_Shl32, mkexpr(irt_regN), mkU8(16) ), 
                              mkU8(16) ), 
                       binop( Iop_Sar32, mkexpr(irt_regM), mkU8(16) ) ) );
        armSignedSatQ( irt_diff, 0x10, &irt_res_diff, NULL );

        assign( irt_sum, 
                binop( Iop_Add32, 
                       binop( Iop_Sar32, mkexpr(irt_regN), mkU8(16) ), 
                       binop( Iop_Sar32, 
                              binop( Iop_Shl32, mkexpr(irt_regM), mkU8(16) ), 
                              mkU8(16) ) ) );
        armSignedSatQ( irt_sum, 0x10, &irt_res_sum, NULL );
       
        IRExpr* ire_result 
          = binop( Iop_Or32, 
                   binop( Iop_Shl32, mkexpr(irt_res_sum), mkU8(16) ), 
                   binop( Iop_And32, mkexpr(irt_res_diff), mkU32(0xFFFF) ) );

        if (isT)
           putIRegT( regD, ire_result, condT );
        else
           putIRegA( regD, ire_result, condT, Ijk_Boring );

        DIP( "qasx%s r%u, r%u, r%u\n", nCC(conq), regD, regN, regM );
        return True;
     }
     
   }

   
   {
     UInt regD = 99, regN = 99, regM = 99;
     Bool gate = False;

     if (isT) {
        if (INSNT0(15,4) == 0xFAA && (INSNT1(15,0) & 0xF0F0) == 0xF000) {
           regN = INSNT0(3,0);
           regD = INSNT1(11,8);
           regM = INSNT1(3,0);
           if (!isBadRegT(regD) && !isBadRegT(regN) && !isBadRegT(regM))
              gate = True;
        }
     } else {
        if (INSNA(27,20) == BITS8(0,1,1,0,0,0,0,1) &&
            INSNA(11,8)  == BITS4(1,1,1,1)         &&
            INSNA(7,4)   == BITS4(0,0,1,1)) {
           regD = INSNA(15,12);
           regN = INSNA(19,16);
           regM = INSNA(3,0);
           if (regD != 15 && regN != 15 && regM != 15)
              gate = True;
        }
     }

     if (gate) {
        IRTemp irt_regN = newTemp(Ity_I32);
        IRTemp irt_regM = newTemp(Ity_I32);
        IRTemp irt_sum  = newTemp(Ity_I32);
        IRTemp irt_diff = newTemp(Ity_I32);

        assign( irt_regN, isT ? getIRegT(regN) : getIRegA(regN) );
        assign( irt_regM, isT ? getIRegT(regM) : getIRegA(regM) );

        assign( irt_diff,  
                binop( Iop_Sub32, 
                       binop( Iop_Sar32, 
                              binop( Iop_Shl32, mkexpr(irt_regN), mkU8(16) ), 
                              mkU8(16) ), 
                       binop( Iop_Sar32, mkexpr(irt_regM), mkU8(16) ) ) );

        assign( irt_sum, 
                binop( Iop_Add32, 
                       binop( Iop_Sar32, mkexpr(irt_regN), mkU8(16) ), 
                       binop( Iop_Sar32, 
                              binop( Iop_Shl32, mkexpr(irt_regM), mkU8(16) ), 
                              mkU8(16) ) ) );
       
        IRExpr* ire_result 
          = binop( Iop_Or32, 
                   binop( Iop_Shl32, mkexpr(irt_sum), mkU8(16) ), 
                   binop( Iop_And32, mkexpr(irt_diff), mkU32(0xFFFF) ) );

        IRTemp ge10 = newTemp(Ity_I32);
        assign(ge10, unop(Iop_Not32, mkexpr(irt_diff)));
        put_GEFLAG32( 0, 31, mkexpr(ge10), condT );
        put_GEFLAG32( 1, 31, mkexpr(ge10), condT );

        IRTemp ge32 = newTemp(Ity_I32);
        assign(ge32, unop(Iop_Not32, mkexpr(irt_sum)));
        put_GEFLAG32( 2, 31, mkexpr(ge32), condT );
        put_GEFLAG32( 3, 31, mkexpr(ge32), condT );

        if (isT)
           putIRegT( regD, ire_result, condT );
        else
           putIRegA( regD, ire_result, condT, Ijk_Boring );

        DIP( "sasx%s r%u, r%u, r%u\n", nCC(conq), regD, regN, regM );
        return True;
     }
     
   }

   
   
   {
     UInt regD = 99, regN = 99, regM = 99, bitM = 99;
     Bool gate = False, isAD = False;

     if (isT) {
        if ((INSNT0(15,4) == 0xFB2 || INSNT0(15,4) == 0xFB4)
            && (INSNT1(15,0) & 0xF0E0) == 0xF000) {
           regN = INSNT0(3,0);
           regD = INSNT1(11,8);
           regM = INSNT1(3,0);
           bitM = INSNT1(4,4);
           isAD = INSNT0(15,4) == 0xFB2;
           if (!isBadRegT(regD) && !isBadRegT(regN) && !isBadRegT(regM))
              gate = True;
        }
     } else {
        if (INSNA(27,20) == BITS8(0,1,1,1,0,0,0,0) &&
            INSNA(15,12) == BITS4(1,1,1,1)         &&
            (INSNA(7,4) & BITS4(1,0,0,1)) == BITS4(0,0,0,1) ) {
           regD = INSNA(19,16);
           regN = INSNA(3,0);
           regM = INSNA(11,8);
           bitM = INSNA(5,5);
           isAD = INSNA(6,6) == 0;
           if (regD != 15 && regN != 15 && regM != 15)
              gate = True;
        }
     }

     if (gate) {
        IRTemp irt_regN    = newTemp(Ity_I32);
        IRTemp irt_regM    = newTemp(Ity_I32);
        IRTemp irt_prod_lo = newTemp(Ity_I32);
        IRTemp irt_prod_hi = newTemp(Ity_I32);
        IRTemp tmpM        = newTemp(Ity_I32);

        assign( irt_regN, isT ? getIRegT(regN) : getIRegA(regN) );

        assign( tmpM, isT ? getIRegT(regM) : getIRegA(regM) );
        assign( irt_regM, genROR32(tmpM, (bitM & 1) ? 16 : 0) );

        assign( irt_prod_lo, 
                binop( Iop_Mul32, 
                       binop( Iop_Sar32, 
                              binop(Iop_Shl32, mkexpr(irt_regN), mkU8(16)), 
                              mkU8(16) ), 
                       binop( Iop_Sar32, 
                              binop(Iop_Shl32, mkexpr(irt_regM), mkU8(16)), 
                              mkU8(16) ) ) );
        assign( irt_prod_hi, binop(Iop_Mul32, 
                                   binop(Iop_Sar32, mkexpr(irt_regN), mkU8(16)), 
                                   binop(Iop_Sar32, mkexpr(irt_regM), mkU8(16))) );
        IRExpr* ire_result 
           = binop( isAD ? Iop_Add32 : Iop_Sub32,
                    mkexpr(irt_prod_lo), mkexpr(irt_prod_hi) );

        if (isT)
           putIRegT( regD, ire_result, condT );
        else
           putIRegA( regD, ire_result, condT, Ijk_Boring );

        if (isAD) {
           or_into_QFLAG32(
              signed_overflow_after_Add32( ire_result,
                                           irt_prod_lo, irt_prod_hi ),
              condT
           );
        }

        DIP("smu%cd%s%s r%u, r%u, r%u\n",
            isAD ? 'a' : 's',
            bitM ? "x" : "", nCC(conq), regD, regN, regM);
        return True;
     }
     
   }

   
   
   {
     UInt regD = 99, regN = 99, regM = 99, regA = 99, bitM = 99;
     Bool gate = False, isAD = False;

     if (isT) {
       if ((INSNT0(15,4) == 0xFB2 || INSNT0(15,4) == 0xFB4)
           && INSNT1(7,5) == BITS3(0,0,0)) {
           regN = INSNT0(3,0);
           regD = INSNT1(11,8);
           regM = INSNT1(3,0);
           regA = INSNT1(15,12);
           bitM = INSNT1(4,4);
           isAD = INSNT0(15,4) == 0xFB2;
           if (!isBadRegT(regD) && !isBadRegT(regN) && !isBadRegT(regM)
               && !isBadRegT(regA))
              gate = True;
        }
     } else {
        if (INSNA(27,20) == BITS8(0,1,1,1,0,0,0,0) &&
            (INSNA(7,4) & BITS4(1,0,0,1)) == BITS4(0,0,0,1)) {
           regD = INSNA(19,16);
           regA = INSNA(15,12);
           regN = INSNA(3,0);
           regM = INSNA(11,8);
           bitM = INSNA(5,5);
           isAD = INSNA(6,6) == 0;
           if (regD != 15 && regN != 15 && regM != 15 && regA != 15)
              gate = True;
        }
     }

     if (gate) {
        IRTemp irt_regN    = newTemp(Ity_I32);
        IRTemp irt_regM    = newTemp(Ity_I32);
        IRTemp irt_regA    = newTemp(Ity_I32);
        IRTemp irt_prod_lo = newTemp(Ity_I32);
        IRTemp irt_prod_hi = newTemp(Ity_I32);
        IRTemp irt_sum     = newTemp(Ity_I32);
        IRTemp tmpM        = newTemp(Ity_I32);

        assign( irt_regN, isT ? getIRegT(regN) : getIRegA(regN) );
        assign( irt_regA, isT ? getIRegT(regA) : getIRegA(regA) );

        assign( tmpM, isT ? getIRegT(regM) : getIRegA(regM) );
        assign( irt_regM, genROR32(tmpM, (bitM & 1) ? 16 : 0) );

        assign( irt_prod_lo, 
                binop(Iop_Mul32, 
                      binop(Iop_Sar32, 
                            binop( Iop_Shl32, mkexpr(irt_regN), mkU8(16) ), 
                            mkU8(16)), 
                      binop(Iop_Sar32, 
                            binop( Iop_Shl32, mkexpr(irt_regM), mkU8(16) ), 
                            mkU8(16))) );
        assign( irt_prod_hi, 
                binop( Iop_Mul32, 
                       binop( Iop_Sar32, mkexpr(irt_regN), mkU8(16) ), 
                       binop( Iop_Sar32, mkexpr(irt_regM), mkU8(16) ) ) );
        assign( irt_sum, binop( isAD ? Iop_Add32 : Iop_Sub32, 
                                mkexpr(irt_prod_lo), mkexpr(irt_prod_hi) ) );

        IRExpr* ire_result = binop(Iop_Add32, mkexpr(irt_sum), mkexpr(irt_regA));

        if (isT)
           putIRegT( regD, ire_result, condT );
        else
           putIRegA( regD, ire_result, condT, Ijk_Boring );

        if (isAD) {
           or_into_QFLAG32(
              signed_overflow_after_Add32( mkexpr(irt_sum),
                                           irt_prod_lo, irt_prod_hi ),
              condT
           );
        }

        or_into_QFLAG32(
           signed_overflow_after_Add32( ire_result, irt_sum, irt_regA ),
           condT
        );

        DIP("sml%cd%s%s r%u, r%u, r%u, r%u\n",
            isAD ? 'a' : 's',
            bitM ? "x" : "", nCC(conq), regD, regN, regM, regA);
        return True;
     }
     
   }

   
   {
     UInt regD = 99, regN = 99, regM = 99, regA = 99, bitM = 99, bitN = 99;
     Bool gate = False;

     if (isT) {
        if (INSNT0(15,4) == 0xFB1 && INSNT1(7,6) == BITS2(0,0)) {
           regN = INSNT0(3,0);
           regD = INSNT1(11,8);
           regM = INSNT1(3,0);
           regA = INSNT1(15,12);
           bitM = INSNT1(4,4);
           bitN = INSNT1(5,5);
           if (!isBadRegT(regD) && !isBadRegT(regN) && !isBadRegT(regM)
               && !isBadRegT(regA))
              gate = True;
        }
     } else {
        if (INSNA(27,20) == BITS8(0,0,0,1,0,0,0,0) &&
            (INSNA(7,4) & BITS4(1,0,0,1)) == BITS4(1,0,0,0)) {
           regD = INSNA(19,16);
           regN = INSNA(3,0);
           regM = INSNA(11,8);
           regA = INSNA(15,12);
           bitM = INSNA(6,6);
           bitN = INSNA(5,5);
           if (regD != 15 && regN != 15 && regM != 15 && regA != 15)
              gate = True;
        }
     }

     if (gate) {
        IRTemp irt_regA = newTemp(Ity_I32);
        IRTemp irt_prod = newTemp(Ity_I32);

        assign( irt_prod, 
                binop(Iop_Mul32, 
                      binop(Iop_Sar32, 
                            binop(Iop_Shl32,
                                  isT ? getIRegT(regN) : getIRegA(regN),
                                  mkU8(bitN ? 0 : 16)),
                            mkU8(16)), 
                      binop(Iop_Sar32, 
                            binop(Iop_Shl32,
                                  isT ? getIRegT(regM) : getIRegA(regM),
                                  mkU8(bitM ? 0 : 16)), 
                            mkU8(16))) );

        assign( irt_regA, isT ? getIRegT(regA) : getIRegA(regA) );

        IRExpr* ire_result = binop(Iop_Add32, mkexpr(irt_prod), mkexpr(irt_regA));

        if (isT)
           putIRegT( regD, ire_result, condT );
        else
           putIRegA( regD, ire_result, condT, Ijk_Boring );

        or_into_QFLAG32(
           signed_overflow_after_Add32( ire_result, irt_prod, irt_regA ),
           condT
        );

        DIP( "smla%c%c%s r%u, r%u, r%u, r%u\n", 
             bitN ? 't' : 'b', bitM ? 't' : 'b', 
             nCC(conq), regD, regN, regM, regA );
        return True;
     }
     
   }

   
   {
     UInt regDHi = 99, regN = 99, regM = 99, regDLo = 99, bitM = 99, bitN = 99;
     Bool gate = False;

     if (isT) {
        if (INSNT0(15,4) == 0xFBC && INSNT1(7,6) == BITS2(1,0)) {
           regN   = INSNT0(3,0);
           regDHi = INSNT1(11,8);
           regM   = INSNT1(3,0);
           regDLo = INSNT1(15,12);
           bitM   = INSNT1(4,4);
           bitN   = INSNT1(5,5);
           if (!isBadRegT(regDHi) && !isBadRegT(regN) && !isBadRegT(regM)
               && !isBadRegT(regDLo) && regDHi != regDLo)
              gate = True;
        }
     } else {
        if (INSNA(27,20) == BITS8(0,0,0,1,0,1,0,0) &&
            (INSNA(7,4) & BITS4(1,0,0,1)) == BITS4(1,0,0,0)) {
           regDHi = INSNA(19,16);
           regN   = INSNA(3,0);
           regM   = INSNA(11,8);
           regDLo = INSNA(15,12);
           bitM   = INSNA(6,6);
           bitN   = INSNA(5,5);
           if (regDHi != 15 && regN != 15 && regM != 15 && regDLo != 15 &&
               regDHi != regDLo)
              gate = True;
        }
     }

     if (gate) {
        IRTemp irt_regD  = newTemp(Ity_I64);
        IRTemp irt_prod  = newTemp(Ity_I64);
        IRTemp irt_res   = newTemp(Ity_I64);
        IRTemp irt_resHi = newTemp(Ity_I32);
        IRTemp irt_resLo = newTemp(Ity_I32);

        assign( irt_prod,
                binop(Iop_MullS32,
                      binop(Iop_Sar32,
                            binop(Iop_Shl32,
                                  isT ? getIRegT(regN) : getIRegA(regN),
                                  mkU8(bitN ? 0 : 16)),
                            mkU8(16)),
                      binop(Iop_Sar32,
                            binop(Iop_Shl32,
                                  isT ? getIRegT(regM) : getIRegA(regM),
                                  mkU8(bitM ? 0 : 16)),
                            mkU8(16))) );

        assign( irt_regD, binop(Iop_32HLto64,
                                isT ? getIRegT(regDHi) : getIRegA(regDHi),
                                isT ? getIRegT(regDLo) : getIRegA(regDLo)) );
        assign( irt_res, binop(Iop_Add64, mkexpr(irt_regD), mkexpr(irt_prod)) );
        assign( irt_resHi, unop(Iop_64HIto32, mkexpr(irt_res)) );
        assign( irt_resLo, unop(Iop_64to32, mkexpr(irt_res)) );

        if (isT) {
           putIRegT( regDHi, mkexpr(irt_resHi), condT );
           putIRegT( regDLo, mkexpr(irt_resLo), condT );
        } else {
           putIRegA( regDHi, mkexpr(irt_resHi), condT, Ijk_Boring );
           putIRegA( regDLo, mkexpr(irt_resLo), condT, Ijk_Boring );
        }

        DIP( "smlal%c%c%s r%u, r%u, r%u, r%u\n",
             bitN ? 't' : 'b', bitM ? 't' : 'b',
             nCC(conq), regDHi, regN, regM, regDLo );
        return True;
     }
     
   }

   
   {
     UInt regD = 99, regN = 99, regM = 99, regA = 99, bitM = 99;
     Bool gate = False;

     if (isT) {
        if (INSNT0(15,4) == 0xFB3 && INSNT1(7,5) == BITS3(0,0,0)) {
           regN = INSNT0(3,0);
           regD = INSNT1(11,8);
           regM = INSNT1(3,0);
           regA = INSNT1(15,12);
           bitM = INSNT1(4,4);
           if (!isBadRegT(regD) && !isBadRegT(regN) && !isBadRegT(regM)
               && !isBadRegT(regA))
              gate = True;
        }
     } else {
        if (INSNA(27,20) == BITS8(0,0,0,1,0,0,1,0) &&
            (INSNA(7,4) & BITS4(1,0,1,1)) == BITS4(1,0,0,0)) {
           regD = INSNA(19,16);
           regN = INSNA(3,0);
           regM = INSNA(11,8);
           regA = INSNA(15,12);
           bitM = INSNA(6,6);
           if (regD != 15 && regN != 15 && regM != 15 && regA != 15)
              gate = True;
        }
     }

     if (gate) {
        IRTemp irt_regA = newTemp(Ity_I32);
        IRTemp irt_prod = newTemp(Ity_I64);

        assign( irt_prod, 
                binop(Iop_MullS32, 
                      isT ? getIRegT(regN) : getIRegA(regN),
                      binop(Iop_Sar32, 
                            binop(Iop_Shl32,
                                  isT ? getIRegT(regM) : getIRegA(regM),
                                  mkU8(bitM ? 0 : 16)), 
                            mkU8(16))) );

        assign( irt_regA, isT ? getIRegT(regA) : getIRegA(regA) );

        IRTemp prod32 = newTemp(Ity_I32);
        assign(prod32,
               binop(Iop_Or32,
                     binop(Iop_Shl32, unop(Iop_64HIto32, mkexpr(irt_prod)), mkU8(16)),
                     binop(Iop_Shr32, unop(Iop_64to32, mkexpr(irt_prod)), mkU8(16))
        ));

        IRExpr* ire_result = binop(Iop_Add32, mkexpr(prod32), mkexpr(irt_regA));

        if (isT)
           putIRegT( regD, ire_result, condT );
        else
           putIRegA( regD, ire_result, condT, Ijk_Boring );

        or_into_QFLAG32(
           signed_overflow_after_Add32( ire_result, prod32, irt_regA ),
           condT
        );

        DIP( "smlaw%c%s r%u, r%u, r%u, r%u\n", 
             bitM ? 't' : 'b', 
             nCC(conq), regD, regN, regM, regA );
        return True;
     }
     
   }

   
   {
     UInt regD = 99, regN = 99, regM = 99;
     Bool gate = False;

     if (isT) {
        if (INSNT0(15,4) == 0xFAA && (INSNT1(15,0) & 0xF0F0) == 0xF080) {
           regN = INSNT0(3,0);
           regD = INSNT1(11,8);
           regM = INSNT1(3,0);
           if (!isBadRegT(regD) && !isBadRegT(regN) && !isBadRegT(regM))
              gate = True;
        }
     } else {
        if (INSNA(27,20) == BITS8(0,1,1,0,1,0,0,0) &&
            INSNA(11,8)  == BITS4(1,1,1,1)         &&
            INSNA(7,4)   == BITS4(1,0,1,1)) {
           regD = INSNA(15,12);
           regN = INSNA(19,16);
           regM = INSNA(3,0);
           if (regD != 15 && regN != 15 && regM != 15)
              gate = True;
        }
     }

     if (gate) {
        IRTemp irt_ge_flag0 = newTemp(Ity_I32);
        IRTemp irt_ge_flag1 = newTemp(Ity_I32);
        IRTemp irt_ge_flag2 = newTemp(Ity_I32);
        IRTemp irt_ge_flag3 = newTemp(Ity_I32);

        assign( irt_ge_flag0, get_GEFLAG32(0) );
        assign( irt_ge_flag1, get_GEFLAG32(1) );
        assign( irt_ge_flag2, get_GEFLAG32(2) );
        assign( irt_ge_flag3, get_GEFLAG32(3) );

        IRExpr* ire_ge_flag0_or 
          = binop(Iop_Or32, mkexpr(irt_ge_flag0), 
                  binop(Iop_Sub32, mkU32(0), mkexpr(irt_ge_flag0)));
        IRExpr* ire_ge_flag1_or 
          = binop(Iop_Or32, mkexpr(irt_ge_flag1), 
                  binop(Iop_Sub32, mkU32(0), mkexpr(irt_ge_flag1)));
        IRExpr* ire_ge_flag2_or 
          = binop(Iop_Or32, mkexpr(irt_ge_flag2), 
                  binop(Iop_Sub32, mkU32(0), mkexpr(irt_ge_flag2)));
        IRExpr* ire_ge_flag3_or 
          = binop(Iop_Or32, mkexpr(irt_ge_flag3), 
                  binop(Iop_Sub32, mkU32(0), mkexpr(irt_ge_flag3)));

        IRExpr* ire_ge_flags 
          = binop( Iop_Or32, 
                   binop(Iop_Or32, 
                         binop(Iop_And32, 
                               binop(Iop_Sar32, ire_ge_flag0_or, mkU8(31)), 
                               mkU32(0x000000ff)), 
                         binop(Iop_And32, 
                               binop(Iop_Sar32, ire_ge_flag1_or, mkU8(31)), 
                               mkU32(0x0000ff00))), 
                   binop(Iop_Or32, 
                         binop(Iop_And32, 
                               binop(Iop_Sar32, ire_ge_flag2_or, mkU8(31)), 
                               mkU32(0x00ff0000)), 
                         binop(Iop_And32, 
                               binop(Iop_Sar32, ire_ge_flag3_or, mkU8(31)), 
                               mkU32(0xff000000))) );

        IRExpr* ire_result 
          = binop(Iop_Or32, 
                  binop(Iop_And32,
                        isT ? getIRegT(regN) : getIRegA(regN),
                        ire_ge_flags ), 
                  binop(Iop_And32,
                        isT ? getIRegT(regM) : getIRegA(regM),
                        unop(Iop_Not32, ire_ge_flags)));

        if (isT)
           putIRegT( regD, ire_result, condT );
        else
           putIRegA( regD, ire_result, condT, Ijk_Boring );

        DIP("sel%s r%u, r%u, r%u\n", nCC(conq), regD, regN, regM );
        return True;
     }
     
   }

   
   {
     UInt regD = 99, regN = 99, regM = 99, rotate = 99;
     Bool gate = False;

     if (isT) {
        if (INSNT0(15,4) == 0xFA3 && (INSNT1(15,0) & 0xF0C0) == 0xF080) {
           regN   = INSNT0(3,0);
           regD   = INSNT1(11,8);
           regM   = INSNT1(3,0);
           rotate = INSNT1(5,4);
           if (!isBadRegT(regD) && !isBadRegT(regN) && !isBadRegT(regM))
              gate = True;
        }
     } else {
        if (INSNA(27,20) == BITS8(0,1,1,0,1,1,0,0) &&
            INSNA(9,4)   == BITS6(0,0,0,1,1,1) ) {
           regD   = INSNA(15,12);
           regN   = INSNA(19,16);
           regM   = INSNA(3,0);
           rotate = INSNA(11,10);
           if (regD != 15 && regN != 15 && regM != 15)
             gate = True;
        }
     }

     if (gate) {
        IRTemp irt_regN = newTemp(Ity_I32);
        assign( irt_regN, isT ? getIRegT(regN) : getIRegA(regN) );

        IRTemp irt_regM = newTemp(Ity_I32);
        assign( irt_regM, isT ? getIRegT(regM) : getIRegA(regM) );

        IRTemp irt_rot = newTemp(Ity_I32);
        assign( irt_rot, binop(Iop_And32,
                               genROR32(irt_regM, 8 * rotate),
                               mkU32(0x00FF00FF)) );

        IRExpr* resLo
           = binop(Iop_And32,
                   binop(Iop_Add32, mkexpr(irt_regN), mkexpr(irt_rot)),
                   mkU32(0x0000FFFF));

        IRExpr* resHi
           = binop(Iop_Add32, 
                   binop(Iop_And32, mkexpr(irt_regN), mkU32(0xFFFF0000)),
                   binop(Iop_And32, mkexpr(irt_rot),  mkU32(0xFFFF0000)));

        IRExpr* ire_result 
           = binop( Iop_Or32, resHi, resLo );

        if (isT)
           putIRegT( regD, ire_result, condT );
        else
           putIRegA( regD, ire_result, condT, Ijk_Boring );

        DIP( "uxtab16%s r%u, r%u, r%u, ROR #%u\n", 
             nCC(conq), regD, regN, regM, 8 * rotate );
        return True;
     }
     
   }

   
   
   {
     UInt rD = 99, rN = 99, rM = 99, rA = 99;
     Bool gate = False;

     if (isT) {
       if (INSNT0(15,4) == 0xFB7 && INSNT1(7,4) == BITS4(0,0,0,0)) {
           rN = INSNT0(3,0);
           rA = INSNT1(15,12);
           rD = INSNT1(11,8);
           rM = INSNT1(3,0);
           if (!isBadRegT(rD) && !isBadRegT(rN) && !isBadRegT(rM) && rA != 13)
              gate = True;
        }
     } else {
        if (INSNA(27,20) == BITS8(0,1,1,1,1,0,0,0) &&
            INSNA(7,4)   == BITS4(0,0,0,1) ) {
           rD = INSNA(19,16);
           rA = INSNA(15,12);
           rM = INSNA(11,8);
           rN = INSNA(3,0);
           if (rD != 15 && rN != 15 && rM != 15 )
              gate = True;
        }
     }
     

     if (gate) {
        IRExpr* rNe = isT ? getIRegT(rN) : getIRegA(rN);
        IRExpr* rMe = isT ? getIRegT(rM) : getIRegA(rM);
        IRExpr* rAe = rA == 15 ? mkU32(0)
                               : (isT ? getIRegT(rA) : getIRegA(rA)); 
        IRExpr* res = binop(Iop_Add32,
                            binop(Iop_Sad8Ux4, rNe, rMe),
                            rAe);
        if (isT)
           putIRegT( rD, res, condT );
        else
           putIRegA( rD, res, condT, Ijk_Boring );

        if (rA == 15) {
           DIP( "usad8%s r%u, r%u, r%u\n", 
                nCC(conq), rD, rN, rM );
        } else {
           DIP( "usada8%s r%u, r%u, r%u, r%u\n", 
                nCC(conq), rD, rN, rM, rA );
        }
        return True;
     }
     
   }

   
   {
     UInt regD = 99, regN = 99, regM = 99;
     Bool gate = False;

     if (isT) {
        if (INSNT0(15,4) == 0xFA8 && (INSNT1(15,0) & 0xF0F0) == 0xF080) {
           regN = INSNT0(3,0);
           regD = INSNT1(11,8);
           regM = INSNT1(3,0);
           if (!isBadRegT(regD) && !isBadRegT(regN) && !isBadRegT(regM))
              gate = True;
        }
     } else {
        if (INSNA(27,20) == BITS8(0,0,0,1,0,0,0,0) &&
            INSNA(11,8)  == BITS4(0,0,0,0)         &&
            INSNA(7,4)   == BITS4(0,1,0,1)) {
           regD = INSNA(15,12);
           regN = INSNA(19,16);
           regM = INSNA(3,0);
           if (regD != 15 && regN != 15 && regM != 15)
              gate = True;
        }
     }

     if (gate) {
        IRTemp rNt   = newTemp(Ity_I32);
        IRTemp rMt   = newTemp(Ity_I32);
        IRTemp res_q = newTemp(Ity_I32);

        assign( rNt, isT ? getIRegT(regN) : getIRegA(regN) );
        assign( rMt, isT ? getIRegT(regM) : getIRegA(regM) );

        assign(res_q, binop(Iop_QAdd32S, mkexpr(rMt), mkexpr(rNt)));
        if (isT)
           putIRegT( regD, mkexpr(res_q), condT );
        else
           putIRegA( regD, mkexpr(res_q), condT, Ijk_Boring );

        or_into_QFLAG32(
           signed_overflow_after_Add32(
              binop(Iop_Add32, mkexpr(rMt), mkexpr(rNt)), rMt, rNt),
           condT
        );

        DIP("qadd%s r%u, r%u, r%u\n", nCC(conq),regD,regM,regN);
        return True;
     }
     
   }

   
   {
     UInt regD = 99, regN = 99, regM = 99;
     Bool gate = False;

     if (isT) {
        if (INSNT0(15,4) == 0xFA8 && (INSNT1(15,0) & 0xF0F0) == 0xF090) {
           regN = INSNT0(3,0);
           regD = INSNT1(11,8);
           regM = INSNT1(3,0);
           if (!isBadRegT(regD) && !isBadRegT(regN) && !isBadRegT(regM))
              gate = True;
        }
     } else {
        if (INSNA(27,20) == BITS8(0,0,0,1,0,1,0,0) &&
            INSNA(11,8)  == BITS4(0,0,0,0)         &&
            INSNA(7,4)   == BITS4(0,1,0,1)) {
           regD = INSNA(15,12);
           regN = INSNA(19,16);
           regM = INSNA(3,0);
           if (regD != 15 && regN != 15 && regM != 15)
              gate = True;
        }
     }

     if (gate) {
        IRTemp rNt   = newTemp(Ity_I32);
        IRTemp rMt   = newTemp(Ity_I32);
        IRTemp rN_d  = newTemp(Ity_I32);
        IRTemp res_q = newTemp(Ity_I32);

        assign( rNt, isT ? getIRegT(regN) : getIRegA(regN) );
        assign( rMt, isT ? getIRegT(regM) : getIRegA(regM) );

        or_into_QFLAG32(
           signed_overflow_after_Add32(
              binop(Iop_Add32, mkexpr(rNt), mkexpr(rNt)), rNt, rNt),
           condT
        );

        assign(rN_d,  binop(Iop_QAdd32S, mkexpr(rNt), mkexpr(rNt)));
        assign(res_q, binop(Iop_QAdd32S, mkexpr(rMt), mkexpr(rN_d)));
        if (isT)
           putIRegT( regD, mkexpr(res_q), condT );
        else
           putIRegA( regD, mkexpr(res_q), condT, Ijk_Boring );

        or_into_QFLAG32(
           signed_overflow_after_Add32(
              binop(Iop_Add32, mkexpr(rMt), mkexpr(rN_d)), rMt, rN_d),
           condT
        );

        DIP("qdadd%s r%u, r%u, r%u\n", nCC(conq),regD,regM,regN);
        return True;
     }
     
   }

   
   {
     UInt regD = 99, regN = 99, regM = 99;
     Bool gate = False;

     if (isT) {
        if (INSNT0(15,4) == 0xFA8 && (INSNT1(15,0) & 0xF0F0) == 0xF0A0) {
           regN = INSNT0(3,0);
           regD = INSNT1(11,8);
           regM = INSNT1(3,0);
           if (!isBadRegT(regD) && !isBadRegT(regN) && !isBadRegT(regM))
              gate = True;
        }
     } else {
        if (INSNA(27,20) == BITS8(0,0,0,1,0,0,1,0) &&
            INSNA(11,8)  == BITS4(0,0,0,0)         &&
            INSNA(7,4)   == BITS4(0,1,0,1)) {
           regD = INSNA(15,12);
           regN = INSNA(19,16);
           regM = INSNA(3,0);
           if (regD != 15 && regN != 15 && regM != 15)
              gate = True;
        }
     }

     if (gate) {
        IRTemp rNt   = newTemp(Ity_I32);
        IRTemp rMt   = newTemp(Ity_I32);
        IRTemp res_q = newTemp(Ity_I32);

        assign( rNt, isT ? getIRegT(regN) : getIRegA(regN) );
        assign( rMt, isT ? getIRegT(regM) : getIRegA(regM) );

        assign(res_q, binop(Iop_QSub32S, mkexpr(rMt), mkexpr(rNt)));
        if (isT)
           putIRegT( regD, mkexpr(res_q), condT );
        else
           putIRegA( regD, mkexpr(res_q), condT, Ijk_Boring );

        or_into_QFLAG32(
           signed_overflow_after_Sub32(
              binop(Iop_Sub32, mkexpr(rMt), mkexpr(rNt)), rMt, rNt),
           condT
        );

        DIP("qsub%s r%u, r%u, r%u\n", nCC(conq),regD,regM,regN);
        return True;
     }
     
   }

   
   {
     UInt regD = 99, regN = 99, regM = 99;
     Bool gate = False;

     if (isT) {
        if (INSNT0(15,4) == 0xFA8 && (INSNT1(15,0) & 0xF0F0) == 0xF0B0) {
           regN = INSNT0(3,0);
           regD = INSNT1(11,8);
           regM = INSNT1(3,0);
           if (!isBadRegT(regD) && !isBadRegT(regN) && !isBadRegT(regM))
              gate = True;
        }
     } else {
        if (INSNA(27,20) == BITS8(0,0,0,1,0,1,1,0) &&
            INSNA(11,8)  == BITS4(0,0,0,0)         &&
            INSNA(7,4)   == BITS4(0,1,0,1)) {
           regD = INSNA(15,12);
           regN = INSNA(19,16);
           regM = INSNA(3,0);
           if (regD != 15 && regN != 15 && regM != 15)
              gate = True;
        }
     }

     if (gate) {
        IRTemp rNt   = newTemp(Ity_I32);
        IRTemp rMt   = newTemp(Ity_I32);
        IRTemp rN_d  = newTemp(Ity_I32);
        IRTemp res_q = newTemp(Ity_I32);

        assign( rNt, isT ? getIRegT(regN) : getIRegA(regN) );
        assign( rMt, isT ? getIRegT(regM) : getIRegA(regM) );

        or_into_QFLAG32(
           signed_overflow_after_Add32(
              binop(Iop_Add32, mkexpr(rNt), mkexpr(rNt)), rNt, rNt),
           condT
        );

        assign(rN_d,  binop(Iop_QAdd32S, mkexpr(rNt), mkexpr(rNt)));
        assign(res_q, binop(Iop_QSub32S, mkexpr(rMt), mkexpr(rN_d)));
        if (isT)
           putIRegT( regD, mkexpr(res_q), condT );
        else
           putIRegA( regD, mkexpr(res_q), condT, Ijk_Boring );

        or_into_QFLAG32(
           signed_overflow_after_Sub32(
              binop(Iop_Sub32, mkexpr(rMt), mkexpr(rN_d)), rMt, rN_d),
           condT
        );

        DIP("qdsub%s r%u, r%u, r%u\n", nCC(conq),regD,regM,regN);
        return True;
     }
     
   }

   
   {
     UInt regD = 99, regN = 99, regM = 99;
     Bool gate = False;

     if (isT) {
        if (INSNT0(15,4) == 0xFAD && (INSNT1(15,0) & 0xF0F0) == 0xF050) {
           regN = INSNT0(3,0);
           regD = INSNT1(11,8);
           regM = INSNT1(3,0);
           if (!isBadRegT(regD) && !isBadRegT(regN) && !isBadRegT(regM))
              gate = True;
        }
     } else {
        if (INSNA(27,20) == BITS8(0,1,1,0,0,1,1,0) &&
            INSNA(11,8)  == BITS4(1,1,1,1)         &&
            INSNA(7,4)   == BITS4(0,1,1,1)) {
           regD = INSNA(15,12);
           regN = INSNA(19,16);
           regM = INSNA(3,0);
           if (regD != 15 && regN != 15 && regM != 15)
             gate = True;
        }
     }

     if (gate) {
        IRTemp rNt   = newTemp(Ity_I32);
        IRTemp rMt   = newTemp(Ity_I32);
        IRTemp res_q = newTemp(Ity_I32);

        assign( rNt, isT ? getIRegT(regN) : getIRegA(regN) );
        assign( rMt, isT ? getIRegT(regM) : getIRegA(regM) );

        assign(res_q, binop(Iop_QSub16Ux2, mkexpr(rNt), mkexpr(rMt)));
        if (isT)
           putIRegT( regD, mkexpr(res_q), condT );
        else
           putIRegA( regD, mkexpr(res_q), condT, Ijk_Boring );

        DIP("uqsub16%s r%u, r%u, r%u\n", nCC(conq),regD,regN,regM);
        return True;
     }
     
   }

   
   {
     UInt regD = 99, regN = 99, regM = 99;
     Bool gate = False;

     if (isT) {
        if (INSNT0(15,4) == 0xFA9 && (INSNT1(15,0) & 0xF0F0) == 0xF020) {
           regN = INSNT0(3,0);
           regD = INSNT1(11,8);
           regM = INSNT1(3,0);
           if (!isBadRegT(regD) && !isBadRegT(regN) && !isBadRegT(regM))
              gate = True;
        }
     } else {
        if (INSNA(27,20) == BITS8(0,1,1,0,0,0,1,1) &&
            INSNA(11,8)  == BITS4(1,1,1,1)         &&
            INSNA(7,4)   == BITS4(0,0,0,1)) {
           regD = INSNA(15,12);
           regN = INSNA(19,16);
           regM = INSNA(3,0);
           if (regD != 15 && regN != 15 && regM != 15)
              gate = True;
        }
     }

     if (gate) {
        IRTemp rNt   = newTemp(Ity_I32);
        IRTemp rMt   = newTemp(Ity_I32);
        IRTemp res_q = newTemp(Ity_I32);

        assign( rNt, isT ? getIRegT(regN) : getIRegA(regN) );
        assign( rMt, isT ? getIRegT(regM) : getIRegA(regM) );

        assign(res_q, binop(Iop_HAdd16Sx2, mkexpr(rNt), mkexpr(rMt)));
        if (isT)
           putIRegT( regD, mkexpr(res_q), condT );
        else
           putIRegA( regD, mkexpr(res_q), condT, Ijk_Boring );

        DIP("shadd16%s r%u, r%u, r%u\n", nCC(conq),regD,regN,regM);
        return True;
     }
     
   }

   
   {
     UInt regD = 99, regN = 99, regM = 99;
     Bool gate = False;

     if (isT) {
        if (INSNT0(15,4) == 0xFAC && (INSNT1(15,0) & 0xF0F0) == 0xF060) {
           regN = INSNT0(3,0);
           regD = INSNT1(11,8);
           regM = INSNT1(3,0);
           if (!isBadRegT(regD) && !isBadRegT(regN) && !isBadRegT(regM))
              gate = True;
        }
     } else {
        if (INSNA(27,20) == BITS8(0,1,1,0,0,1,1,1) &&
            INSNA(11,8)  == BITS4(1,1,1,1)         &&
            INSNA(7,4)   == BITS4(1,1,1,1)) {
           regD = INSNA(15,12);
           regN = INSNA(19,16);
           regM = INSNA(3,0);
           if (regD != 15 && regN != 15 && regM != 15)
              gate = True;
        }
     }

     if (gate) {
        IRTemp rNt   = newTemp(Ity_I32);
        IRTemp rMt   = newTemp(Ity_I32);
        IRTemp res_q = newTemp(Ity_I32);

        assign( rNt, isT ? getIRegT(regN) : getIRegA(regN) );
        assign( rMt, isT ? getIRegT(regM) : getIRegA(regM) );

        assign(res_q, binop(Iop_HSub8Ux4, mkexpr(rNt), mkexpr(rMt)));
        if (isT)
           putIRegT( regD, mkexpr(res_q), condT );
        else
           putIRegA( regD, mkexpr(res_q), condT, Ijk_Boring );

        DIP("uhsub8%s r%u, r%u, r%u\n", nCC(conq),regD,regN,regM);
        return True;
     }
     
   }

   
   {
     UInt regD = 99, regN = 99, regM = 99;
     Bool gate = False;

     if (isT) {
        if (INSNT0(15,4) == 0xFAD && (INSNT1(15,0) & 0xF0F0) == 0xF060) {
           regN = INSNT0(3,0);
           regD = INSNT1(11,8);
           regM = INSNT1(3,0);
           if (!isBadRegT(regD) && !isBadRegT(regN) && !isBadRegT(regM))
              gate = True;
        }
     } else {
        if (INSNA(27,20) == BITS8(0,1,1,0,0,1,1,1) &&
            INSNA(11,8)  == BITS4(1,1,1,1)         &&
            INSNA(7,4)   == BITS4(0,1,1,1)) {
           regD = INSNA(15,12);
           regN = INSNA(19,16);
           regM = INSNA(3,0);
           if (regD != 15 && regN != 15 && regM != 15)
              gate = True;
        }
     }

     if (gate) {
        IRTemp rNt   = newTemp(Ity_I32);
        IRTemp rMt   = newTemp(Ity_I32);
        IRTemp res_q = newTemp(Ity_I32);

        assign( rNt, isT ? getIRegT(regN) : getIRegA(regN) );
        assign( rMt, isT ? getIRegT(regM) : getIRegA(regM) );

        assign(res_q, binop(Iop_HSub16Ux2, mkexpr(rNt), mkexpr(rMt)));
        if (isT)
           putIRegT( regD, mkexpr(res_q), condT );
        else
           putIRegA( regD, mkexpr(res_q), condT, Ijk_Boring );

        DIP("uhsub16%s r%u, r%u, r%u\n", nCC(conq),regD,regN,regM);
        return True;
     }
     
   }

   
   {
     UInt regD = 99, regN = 99, regM = 99;
     Bool gate = False;

     if (isT) {
        if (INSNT0(15,4) == 0xFA9 && (INSNT1(15,0) & 0xF0F0) == 0xF050) {
           regN = INSNT0(3,0);
           regD = INSNT1(11,8);
           regM = INSNT1(3,0);
           if (!isBadRegT(regD) && !isBadRegT(regN) && !isBadRegT(regM))
              gate = True;
        }
     } else {
        if (INSNA(27,20) == BITS8(0,1,1,0,0,1,1,0) &&
            INSNA(11,8)  == BITS4(1,1,1,1)         &&
            INSNA(7,4)   == BITS4(0,0,0,1)) {
           regD = INSNA(15,12);
           regN = INSNA(19,16);
           regM = INSNA(3,0);
           if (regD != 15 && regN != 15 && regM != 15)
              gate = True;
        }
     }

     if (gate) {
        IRTemp rNt   = newTemp(Ity_I32);
        IRTemp rMt   = newTemp(Ity_I32);
        IRTemp res_q = newTemp(Ity_I32);

        assign( rNt, isT ? getIRegT(regN) : getIRegA(regN) );
        assign( rMt, isT ? getIRegT(regM) : getIRegA(regM) );

        assign(res_q, binop(Iop_QAdd16Ux2, mkexpr(rNt), mkexpr(rMt)));
        if (isT)
           putIRegT( regD, mkexpr(res_q), condT );
        else
           putIRegA( regD, mkexpr(res_q), condT, Ijk_Boring );

        DIP("uqadd16%s r%u, r%u, r%u\n", nCC(conq),regD,regN,regM);
        return True;
     }
     
   }

   
   {
     UInt regD = 99, regN = 99, regM = 99;
     Bool gate = False;

     if (isT) {
        if (INSNT0(15,4) == 0xFAE && (INSNT1(15,0) & 0xF0F0) == 0xF050) {
           regN = INSNT0(3,0);
           regD = INSNT1(11,8);
           regM = INSNT1(3,0);
           if (!isBadRegT(regD) && !isBadRegT(regN) && !isBadRegT(regM))
              gate = True;
        }
     } else {
        if (INSNA(27,20) == BITS8(0,1,1,0,0,1,1,0) &&
            INSNA(11,8)  == BITS4(1,1,1,1)         &&
            INSNA(7,4)   == BITS4(0,1,0,1)) {
           regD = INSNA(15,12);
           regN = INSNA(19,16);
           regM = INSNA(3,0);
           if (regD != 15 && regN != 15 && regM != 15)
              gate = True;
        }
     }

     if (gate) {
        IRTemp irt_regN     = newTemp(Ity_I32);
        IRTemp irt_regM     = newTemp(Ity_I32);
        IRTemp irt_sum      = newTemp(Ity_I32);
        IRTemp irt_diff     = newTemp(Ity_I32);
        IRTemp irt_sum_res  = newTemp(Ity_I32);
        IRTemp irt_diff_res = newTemp(Ity_I32);

        assign( irt_regN, isT ? getIRegT(regN) : getIRegA(regN) );
        assign( irt_regM, isT ? getIRegT(regM) : getIRegA(regM) );

        assign( irt_diff, 
                binop( Iop_Sub32, 
                       binop( Iop_Shr32, mkexpr(irt_regN), mkU8(16) ),
                       binop( Iop_Shr32, 
                              binop(Iop_Shl32, mkexpr(irt_regM), mkU8(16)), 
                              mkU8(16) ) ) );
        armUnsignedSatQ( &irt_diff_res, NULL, irt_diff, 0x10);

        assign( irt_sum, 
                binop( Iop_Add32, 
                       binop( Iop_Shr32, 
                              binop( Iop_Shl32, mkexpr(irt_regN), mkU8(16) ), 
                              mkU8(16) ), 
                       binop( Iop_Shr32, mkexpr(irt_regM), mkU8(16) )) );
        armUnsignedSatQ( &irt_sum_res, NULL, irt_sum, 0x10 );

        IRExpr* ire_result = binop( Iop_Or32, 
                                    binop( Iop_Shl32, mkexpr(irt_diff_res), 
                                           mkU8(16) ), 
                                    binop( Iop_And32, mkexpr(irt_sum_res), 
                                           mkU32(0xFFFF)) );

        if (isT) 
           putIRegT( regD, ire_result, condT );
        else
           putIRegA( regD, ire_result, condT, Ijk_Boring );

        DIP( "uqsax%s r%u, r%u, r%u\n", nCC(conq), regD, regN, regM );
        return True;
     }
     
   }

   
   {
     UInt regD = 99, regN = 99, regM = 99;
     Bool gate = False;

     if (isT) {
        if (INSNT0(15,4) == 0xFAA && (INSNT1(15,0) & 0xF0F0) == 0xF050) {
           regN = INSNT0(3,0);
           regD = INSNT1(11,8);
           regM = INSNT1(3,0);
           if (!isBadRegT(regD) && !isBadRegT(regN) && !isBadRegT(regM))
              gate = True;
        }
     } else {
        if (INSNA(27,20) == BITS8(0,1,1,0,0,1,1,0) &&
            INSNA(11,8)  == BITS4(1,1,1,1)         &&
            INSNA(7,4)   == BITS4(0,0,1,1)) {
           regD = INSNA(15,12);
           regN = INSNA(19,16);
           regM = INSNA(3,0);
           if (regD != 15 && regN != 15 && regM != 15)
              gate = True;
        }
     }

     if (gate) {
        IRTemp irt_regN     = newTemp(Ity_I32);
        IRTemp irt_regM     = newTemp(Ity_I32);
        IRTemp irt_sum      = newTemp(Ity_I32);
        IRTemp irt_diff     = newTemp(Ity_I32);
        IRTemp irt_res_sum  = newTemp(Ity_I32);
        IRTemp irt_res_diff = newTemp(Ity_I32);

        assign( irt_regN, isT ? getIRegT(regN) : getIRegA(regN) );
        assign( irt_regM, isT ? getIRegT(regM) : getIRegA(regM) );

        assign( irt_diff,  
                binop( Iop_Sub32,
                       binop( Iop_Shr32, 
                              binop( Iop_Shl32, mkexpr(irt_regN), mkU8(16) ), 
                              mkU8(16) ), 
                       binop( Iop_Shr32, mkexpr(irt_regM), mkU8(16) ) ) );
        armUnsignedSatQ( &irt_res_diff, NULL, irt_diff, 0x10 );

        assign( irt_sum, 
                binop( Iop_Add32, 
                       binop( Iop_Shr32, mkexpr(irt_regN), mkU8(16) ), 
                       binop( Iop_Shr32, 
                              binop( Iop_Shl32, mkexpr(irt_regM), mkU8(16) ), 
                              mkU8(16) ) ) );
        armUnsignedSatQ( &irt_res_sum, NULL, irt_sum, 0x10 );
       
        IRExpr* ire_result 
          = binop( Iop_Or32, 
                   binop( Iop_Shl32, mkexpr(irt_res_sum), mkU8(16) ), 
                   binop( Iop_And32, mkexpr(irt_res_diff), mkU32(0xFFFF) ) );

        if (isT)
           putIRegT( regD, ire_result, condT );
        else
           putIRegA( regD, ire_result, condT, Ijk_Boring );

        DIP( "uqasx%s r%u, r%u, r%u\n", nCC(conq), regD, regN, regM );
        return True;
     }
     
   }

   
   {
     UInt regD = 99, regN = 99, regM = 99;
     Bool gate = False;

     if (isT) {
        if (INSNT0(15,4) == 0xFAE && (INSNT1(15,0) & 0xF0F0) == 0xF040) {
           regN = INSNT0(3,0);
           regD = INSNT1(11,8);
           regM = INSNT1(3,0);
           if (!isBadRegT(regD) && !isBadRegT(regN) && !isBadRegT(regM))
              gate = True;
        }
     } else {
        if (INSNA(27,20) == BITS8(0,1,1,0,0,1,0,1) &&
            INSNA(11,8)  == BITS4(1,1,1,1)         &&
            INSNA(7,4)   == BITS4(0,1,0,1)) {
           regD = INSNA(15,12);
           regN = INSNA(19,16);
           regM = INSNA(3,0);
           if (regD != 15 && regN != 15 && regM != 15)
              gate = True;
        }
     }

     if (gate) {
        IRTemp irt_regN = newTemp(Ity_I32);
        IRTemp irt_regM = newTemp(Ity_I32);
        IRTemp irt_sum  = newTemp(Ity_I32);
        IRTemp irt_diff = newTemp(Ity_I32);

        assign( irt_regN, isT ? getIRegT(regN) : getIRegA(regN) );
        assign( irt_regM, isT ? getIRegT(regM) : getIRegA(regM) );

        assign( irt_sum,  
                binop( Iop_Add32, 
                       unop( Iop_16Uto32,
                             unop( Iop_32to16, mkexpr(irt_regN) )
                       ),  
                       binop( Iop_Shr32, mkexpr(irt_regM), mkU8(16) ) ) );

        assign( irt_diff, 
                binop( Iop_Sub32, 
                       binop( Iop_Shr32, mkexpr(irt_regN), mkU8(16) ), 
                       unop( Iop_16Uto32, 
                             unop( Iop_32to16, mkexpr(irt_regM) )
                       )
                )
        );
       
        IRExpr* ire_result 
          = binop( Iop_Or32, 
                   binop( Iop_Shl32, mkexpr(irt_diff), mkU8(16) ), 
                   binop( Iop_And32, mkexpr(irt_sum), mkU32(0xFFFF) ) );

        IRTemp ge10 = newTemp(Ity_I32);
        assign( ge10, IRExpr_ITE( binop( Iop_CmpLE32U, 
                                         mkU32(0x10000), mkexpr(irt_sum) ),
                                  mkU32(1), mkU32(0) ) );
        put_GEFLAG32( 0, 0, mkexpr(ge10), condT );
        put_GEFLAG32( 1, 0, mkexpr(ge10), condT );

        IRTemp ge32 = newTemp(Ity_I32);
        assign(ge32, unop(Iop_Not32, mkexpr(irt_diff)));
        put_GEFLAG32( 2, 31, mkexpr(ge32), condT );
        put_GEFLAG32( 3, 31, mkexpr(ge32), condT );

        if (isT)
           putIRegT( regD, ire_result, condT );
        else
           putIRegA( regD, ire_result, condT, Ijk_Boring );

        DIP( "usax%s r%u, r%u, r%u\n", nCC(conq), regD, regN, regM );
        return True;
     }
     
   }

   
   {
     UInt regD = 99, regN = 99, regM = 99;
     Bool gate = False;

     if (isT) {
        if (INSNT0(15,4) == 0xFAA && (INSNT1(15,0) & 0xF0F0) == 0xF040) {
           regN = INSNT0(3,0);
           regD = INSNT1(11,8);
           regM = INSNT1(3,0);
           if (!isBadRegT(regD) && !isBadRegT(regN) && !isBadRegT(regM))
              gate = True;
        }
     } else {
        if (INSNA(27,20) == BITS8(0,1,1,0,0,1,0,1) &&
            INSNA(11,8)  == BITS4(1,1,1,1)         &&
            INSNA(7,4)   == BITS4(0,0,1,1)) {
           regD = INSNA(15,12);
           regN = INSNA(19,16);
           regM = INSNA(3,0);
           if (regD != 15 && regN != 15 && regM != 15)
              gate = True;
        }
     }

     if (gate) {
        IRTemp irt_regN = newTemp(Ity_I32);
        IRTemp irt_regM = newTemp(Ity_I32);
        IRTemp irt_sum  = newTemp(Ity_I32);
        IRTemp irt_diff = newTemp(Ity_I32);

        assign( irt_regN, isT ? getIRegT(regN) : getIRegA(regN) );
        assign( irt_regM, isT ? getIRegT(regM) : getIRegA(regM) );

        assign( irt_diff,  
                binop( Iop_Sub32, 
                       unop( Iop_16Uto32, 
                             unop( Iop_32to16, mkexpr(irt_regN) )
                       ), 
                       binop( Iop_Shr32, mkexpr(irt_regM), mkU8(16) ) ) );

        assign( irt_sum, 
                binop( Iop_Add32, 
                       binop( Iop_Shr32, mkexpr(irt_regN), mkU8(16) ), 
                       unop( Iop_16Uto32, 
                             unop( Iop_32to16, mkexpr(irt_regM) )
                       ) ) );
       
        IRExpr* ire_result 
          = binop( Iop_Or32, 
                   binop( Iop_Shl32, mkexpr(irt_sum), mkU8(16) ), 
                   binop( Iop_And32, mkexpr(irt_diff), mkU32(0xFFFF) ) );

        IRTemp ge10 = newTemp(Ity_I32);
        assign(ge10, unop(Iop_Not32, mkexpr(irt_diff)));
        put_GEFLAG32( 0, 31, mkexpr(ge10), condT );
        put_GEFLAG32( 1, 31, mkexpr(ge10), condT );

        IRTemp ge32 = newTemp(Ity_I32);
        assign( ge32, IRExpr_ITE( binop( Iop_CmpLE32U,
                                         mkU32(0x10000), mkexpr(irt_sum) ),
                                  mkU32(1), mkU32(0) ) );
        put_GEFLAG32( 2, 0, mkexpr(ge32), condT );
        put_GEFLAG32( 3, 0, mkexpr(ge32), condT );

        if (isT)
           putIRegT( regD, ire_result, condT );
        else
           putIRegA( regD, ire_result, condT, Ijk_Boring );

        DIP( "uasx%s r%u, r%u, r%u\n", nCC(conq), regD, regN, regM );
        return True;
     }
     
   }

   
   {
     UInt regD = 99, regN = 99, regM = 99;
     Bool gate = False;

     if (isT) {
        if (INSNT0(15,4) == 0xFAE && (INSNT1(15,0) & 0xF0F0) == 0xF000) {
           regN = INSNT0(3,0);
           regD = INSNT1(11,8);
           regM = INSNT1(3,0);
           if (!isBadRegT(regD) && !isBadRegT(regN) && !isBadRegT(regM))
              gate = True;
        }
     } else {
        if (INSNA(27,20) == BITS8(0,1,1,0,0,0,0,1) &&
            INSNA(11,8)  == BITS4(1,1,1,1)         &&
            INSNA(7,4)   == BITS4(0,1,0,1)) {
           regD = INSNA(15,12);
           regN = INSNA(19,16);
           regM = INSNA(3,0);
           if (regD != 15 && regN != 15 && regM != 15)
              gate = True;
        }
     }

     if (gate) {
        IRTemp irt_regN = newTemp(Ity_I32);
        IRTemp irt_regM = newTemp(Ity_I32);
        IRTemp irt_sum  = newTemp(Ity_I32);
        IRTemp irt_diff = newTemp(Ity_I32);

        assign( irt_regN, isT ? getIRegT(regN) : getIRegA(regN) );
        assign( irt_regM, isT ? getIRegT(regM) : getIRegA(regM) );

        assign( irt_sum,  
                binop( Iop_Add32, 
                       binop( Iop_Sar32, 
                              binop( Iop_Shl32, mkexpr(irt_regN), mkU8(16) ), 
                              mkU8(16) ), 
                       binop( Iop_Sar32, mkexpr(irt_regM), mkU8(16) ) ) );

        assign( irt_diff, 
                binop( Iop_Sub32, 
                       binop( Iop_Sar32, mkexpr(irt_regN), mkU8(16) ), 
                       binop( Iop_Sar32, 
                              binop( Iop_Shl32, mkexpr(irt_regM), mkU8(16) ), 
                              mkU8(16) ) ) );
       
        IRExpr* ire_result 
          = binop( Iop_Or32, 
                   binop( Iop_Shl32, mkexpr(irt_diff), mkU8(16) ), 
                   binop( Iop_And32, mkexpr(irt_sum), mkU32(0xFFFF) ) );

        IRTemp ge10 = newTemp(Ity_I32);
        assign(ge10, unop(Iop_Not32, mkexpr(irt_sum)));
        put_GEFLAG32( 0, 31, mkexpr(ge10), condT );
        put_GEFLAG32( 1, 31, mkexpr(ge10), condT );

        IRTemp ge32 = newTemp(Ity_I32);
        assign(ge32, unop(Iop_Not32, mkexpr(irt_diff)));
        put_GEFLAG32( 2, 31, mkexpr(ge32), condT );
        put_GEFLAG32( 3, 31, mkexpr(ge32), condT );

        if (isT)
           putIRegT( regD, ire_result, condT );
        else
           putIRegA( regD, ire_result, condT, Ijk_Boring );

        DIP( "ssax%s r%u, r%u, r%u\n", nCC(conq), regD, regN, regM );
        return True;
     }
     
   }

   
   {
     UInt regD = 99, regN = 99, regM = 99;
     Bool gate = False;

     if (isT) {
        if (INSNT0(15,4) == 0xFAC && (INSNT1(15,0) & 0xF0F0) == 0xF020) {
           regN = INSNT0(3,0);
           regD = INSNT1(11,8);
           regM = INSNT1(3,0);
           if (!isBadRegT(regD) && !isBadRegT(regN) && !isBadRegT(regM))
              gate = True;
        }
     } else {
        if (INSNA(27,20) == BITS8(0,1,1,0,0,0,1,1) &&
            INSNA(11,8)  == BITS4(1,1,1,1)         &&
            INSNA(7,4)   == BITS4(1,1,1,1)) {
           regD = INSNA(15,12);
           regN = INSNA(19,16);
           regM = INSNA(3,0);
           if (regD != 15 && regN != 15 && regM != 15)
              gate = True;
        }
     }

     if (gate) {
        IRTemp rNt   = newTemp(Ity_I32);
        IRTemp rMt   = newTemp(Ity_I32);
        IRTemp res_q = newTemp(Ity_I32);

        assign( rNt, isT ? getIRegT(regN) : getIRegA(regN) );
        assign( rMt, isT ? getIRegT(regM) : getIRegA(regM) );

        assign(res_q, binop(Iop_HSub8Sx4, mkexpr(rNt), mkexpr(rMt)));
        if (isT)
           putIRegT( regD, mkexpr(res_q), condT );
        else
           putIRegA( regD, mkexpr(res_q), condT, Ijk_Boring );

        DIP("shsub8%s r%u, r%u, r%u\n", nCC(conq),regD,regN,regM);
        return True;
     }
     
   }

   
   {
     UInt regD = 99, regN = 99, regM = 99, rotate = 99;
     Bool gate = False;

     if (isT) {
        if (INSNT0(15,4) == 0xFA2 && (INSNT1(15,0) & 0xF0C0) == 0xF080) {
           regN   = INSNT0(3,0);
           regD   = INSNT1(11,8);
           regM   = INSNT1(3,0);
           rotate = INSNT1(5,4);
           if (!isBadRegT(regD) && !isBadRegT(regN) && !isBadRegT(regM))
              gate = True;
        }
     } else {
        if (INSNA(27,20) == BITS8(0,1,1,0,1,0,0,0) &&
            INSNA(9,4)   == BITS6(0,0,0,1,1,1) ) {
           regD   = INSNA(15,12);
           regN   = INSNA(19,16);
           regM   = INSNA(3,0);
           rotate = INSNA(11,10);
           if (regD != 15 && regN != 15 && regM != 15)
             gate = True;
        }
     }

     if (gate) {
        IRTemp irt_regN = newTemp(Ity_I32);
        assign( irt_regN, isT ? getIRegT(regN) : getIRegA(regN) );

        IRTemp irt_regM = newTemp(Ity_I32);
        assign( irt_regM, isT ? getIRegT(regM) : getIRegA(regM) );

        IRTemp irt_rot = newTemp(Ity_I32);
        assign( irt_rot, genROR32(irt_regM, 8 * rotate) );

        
        IRExpr* resLo
           = binop(Iop_And32,
                   binop(Iop_Add32,
                         mkexpr(irt_regN),
                         unop(Iop_16Uto32,
                              unop(Iop_8Sto16,
                                   unop(Iop_32to8, mkexpr(irt_rot))))),
                   mkU32(0x0000FFFF));

        IRExpr* resHi
           = binop(Iop_And32,
                   binop(Iop_Add32,
                         mkexpr(irt_regN),
                         binop(Iop_Shl32,
                               unop(Iop_16Uto32,
                                    unop(Iop_8Sto16,
                                         unop(Iop_32to8,
                                              binop(Iop_Shr32,
                                                    mkexpr(irt_rot),
                                                    mkU8(16))))),
                               mkU8(16))),
                   mkU32(0xFFFF0000));

        IRExpr* ire_result 
           = binop( Iop_Or32, resHi, resLo );

        if (isT)
           putIRegT( regD, ire_result, condT );
        else
           putIRegA( regD, ire_result, condT, Ijk_Boring );

        DIP( "sxtab16%s r%u, r%u, r%u, ROR #%u\n", 
             nCC(conq), regD, regN, regM, 8 * rotate );
        return True;
     }
     
   }

   
   {
     UInt regD = 99, regN = 99, regM = 99;
     Bool gate = False;

     if (isT) {
        if (INSNT0(15,4) == 0xFAA && (INSNT1(15,0) & 0xF0F0) == 0xF020) {
           regN = INSNT0(3,0);
           regD = INSNT1(11,8);
           regM = INSNT1(3,0);
           if (!isBadRegT(regD) && !isBadRegT(regN) && !isBadRegT(regM))
              gate = True;
        }
     } else {
        if (INSNA(27,20) == BITS8(0,1,1,0,0,0,1,1) &&
            INSNA(11,8)  == BITS4(1,1,1,1)         &&
            INSNA(7,4)   == BITS4(0,0,1,1)) {
           regD = INSNA(15,12);
           regN = INSNA(19,16);
           regM = INSNA(3,0);
           if (regD != 15 && regN != 15 && regM != 15)
              gate = True;
        }
     }

     if (gate) {
        IRTemp rNt   = newTemp(Ity_I32);
        IRTemp rMt   = newTemp(Ity_I32);
        IRTemp irt_diff  = newTemp(Ity_I32);
        IRTemp irt_sum   = newTemp(Ity_I32);
        IRTemp res_q = newTemp(Ity_I32);

        assign( rNt, isT ? getIRegT(regN) : getIRegA(regN) );
        assign( rMt, isT ? getIRegT(regM) : getIRegA(regM) );

        assign( irt_diff,
                binop(Iop_Sub32,
                      unop(Iop_16Sto32,
                           unop(Iop_32to16,
                                mkexpr(rNt)
                           )
                      ),
                      unop(Iop_16Sto32,
                           unop(Iop_32to16,
                                binop(Iop_Shr32,
                                      mkexpr(rMt), mkU8(16)
                                )
                           )
                      )
                )
        );

        assign( irt_sum,
                binop(Iop_Add32,
                      unop(Iop_16Sto32,
                           unop(Iop_32to16,
                                binop(Iop_Shr32,
                                      mkexpr(rNt), mkU8(16)
                                )
                           )
                      ),
                      unop(Iop_16Sto32,
                           unop(Iop_32to16, mkexpr(rMt)
                           )
                      )
                )
        );

        assign( res_q,
                binop(Iop_Or32, 
                      unop(Iop_16Uto32,
                           unop(Iop_32to16,
                                binop(Iop_Shr32,
                                      mkexpr(irt_diff), mkU8(1)
                                )
                           )
                      ),
                      binop(Iop_Shl32,
                            binop(Iop_Shr32,
                                  mkexpr(irt_sum), mkU8(1)
                            ),
                            mkU8(16)
                     )
                )
        );

        if (isT)
           putIRegT( regD, mkexpr(res_q), condT );
        else
           putIRegA( regD, mkexpr(res_q), condT, Ijk_Boring );

        DIP("shasx%s r%u, r%u, r%u\n", nCC(conq),regD,regN,regM);
        return True;
     }
     
   }

   
   {
     UInt regD = 99, regN = 99, regM = 99;
     Bool gate = False;

     if (isT) {
        if (INSNT0(15,4) == 0xFAA && (INSNT1(15,0) & 0xF0F0) == 0xF060) {
           regN = INSNT0(3,0);
           regD = INSNT1(11,8);
           regM = INSNT1(3,0);
           if (!isBadRegT(regD) && !isBadRegT(regN) && !isBadRegT(regM))
              gate = True;
        }
     } else {
        if (INSNA(27,20) == BITS8(0,1,1,0,0,1,1,1) &&
            INSNA(11,8)  == BITS4(1,1,1,1)         &&
            INSNA(7,4)   == BITS4(0,0,1,1)) {
           regD = INSNA(15,12);
           regN = INSNA(19,16);
           regM = INSNA(3,0);
           if (regD != 15 && regN != 15 && regM != 15)
              gate = True;
        }
     }

     if (gate) {
        IRTemp rNt   = newTemp(Ity_I32);
        IRTemp rMt   = newTemp(Ity_I32);
        IRTemp irt_diff  = newTemp(Ity_I32);
        IRTemp irt_sum   = newTemp(Ity_I32);
        IRTemp res_q = newTemp(Ity_I32);

        assign( rNt, isT ? getIRegT(regN) : getIRegA(regN) );
        assign( rMt, isT ? getIRegT(regM) : getIRegA(regM) );

        assign( irt_diff,
                binop(Iop_Sub32,
                      unop(Iop_16Uto32,
                           unop(Iop_32to16,
                                mkexpr(rNt)
                           )
                      ),
                      unop(Iop_16Uto32,
                           unop(Iop_32to16,
                                binop(Iop_Shr32,
                                      mkexpr(rMt), mkU8(16)
                                )
                           )
                      )
                )
        );

        assign( irt_sum,
                binop(Iop_Add32,
                      unop(Iop_16Uto32,
                           unop(Iop_32to16,
                                binop(Iop_Shr32,
                                      mkexpr(rNt), mkU8(16)
                                )
                           )
                      ),
                      unop(Iop_16Uto32,
                           unop(Iop_32to16, mkexpr(rMt)
                           )
                      )
                )
        );

        assign( res_q,
                binop(Iop_Or32, 
                      unop(Iop_16Uto32,
                           unop(Iop_32to16,
                                binop(Iop_Shr32,
                                      mkexpr(irt_diff), mkU8(1)
                                )
                           )
                      ),
                      binop(Iop_Shl32,
                            binop(Iop_Shr32,
                                  mkexpr(irt_sum), mkU8(1)
                            ),
                            mkU8(16)
                     )
                )
        );

        if (isT)
           putIRegT( regD, mkexpr(res_q), condT );
        else
           putIRegA( regD, mkexpr(res_q), condT, Ijk_Boring );

        DIP("uhasx%s r%u, r%u, r%u\n", nCC(conq),regD,regN,regM);
        return True;
     }
     
   }

   
   {
     UInt regD = 99, regN = 99, regM = 99;
     Bool gate = False;

     if (isT) {
        if (INSNT0(15,4) == 0xFAE && (INSNT1(15,0) & 0xF0F0) == 0xF020) {
           regN = INSNT0(3,0);
           regD = INSNT1(11,8);
           regM = INSNT1(3,0);
           if (!isBadRegT(regD) && !isBadRegT(regN) && !isBadRegT(regM))
              gate = True;
        }
     } else {
        if (INSNA(27,20) == BITS8(0,1,1,0,0,0,1,1) &&
            INSNA(11,8)  == BITS4(1,1,1,1)         &&
            INSNA(7,4)   == BITS4(0,1,0,1)) {
           regD = INSNA(15,12);
           regN = INSNA(19,16);
           regM = INSNA(3,0);
           if (regD != 15 && regN != 15 && regM != 15)
              gate = True;
        }
     }

     if (gate) {
        IRTemp rNt   = newTemp(Ity_I32);
        IRTemp rMt   = newTemp(Ity_I32);
        IRTemp irt_diff  = newTemp(Ity_I32);
        IRTemp irt_sum   = newTemp(Ity_I32);
        IRTemp res_q = newTemp(Ity_I32);

        assign( rNt, isT ? getIRegT(regN) : getIRegA(regN) );
        assign( rMt, isT ? getIRegT(regM) : getIRegA(regM) );

        assign( irt_sum,
                binop(Iop_Add32,
                      unop(Iop_16Sto32,
                           unop(Iop_32to16,
                                mkexpr(rNt)
                           )
                      ),
                      unop(Iop_16Sto32,
                           unop(Iop_32to16,
                                binop(Iop_Shr32,
                                      mkexpr(rMt), mkU8(16)
                                )
                           )
                      )
                )
        );

        assign( irt_diff,
                binop(Iop_Sub32,
                      unop(Iop_16Sto32,
                           unop(Iop_32to16,
                                binop(Iop_Shr32,
                                      mkexpr(rNt), mkU8(16)
                                )
                           )
                      ),
                      unop(Iop_16Sto32,
                           unop(Iop_32to16, mkexpr(rMt)
                           )
                      )
                )
        );

        assign( res_q,
                binop(Iop_Or32, 
                      unop(Iop_16Uto32,
                           unop(Iop_32to16,
                                binop(Iop_Shr32,
                                      mkexpr(irt_sum), mkU8(1)
                                )
                           )
                      ),
                      binop(Iop_Shl32,
                            binop(Iop_Shr32,
                                  mkexpr(irt_diff), mkU8(1)
                            ),
                            mkU8(16)
                     )
                )
        );

        if (isT)
           putIRegT( regD, mkexpr(res_q), condT );
        else
           putIRegA( regD, mkexpr(res_q), condT, Ijk_Boring );

        DIP("shsax%s r%u, r%u, r%u\n", nCC(conq),regD,regN,regM);
        return True;
     }
     
   }

   
   {
     UInt regD = 99, regN = 99, regM = 99;
     Bool gate = False;

     if (isT) {
        if (INSNT0(15,4) == 0xFAE && (INSNT1(15,0) & 0xF0F0) == 0xF060) {
           regN = INSNT0(3,0);
           regD = INSNT1(11,8);
           regM = INSNT1(3,0);
           if (!isBadRegT(regD) && !isBadRegT(regN) && !isBadRegT(regM))
              gate = True;
        }
     } else {
        if (INSNA(27,20) == BITS8(0,1,1,0,0,1,1,1) &&
            INSNA(11,8)  == BITS4(1,1,1,1)         &&
            INSNA(7,4)   == BITS4(0,1,0,1)) {
           regD = INSNA(15,12);
           regN = INSNA(19,16);
           regM = INSNA(3,0);
           if (regD != 15 && regN != 15 && regM != 15)
              gate = True;
        }
     }

     if (gate) {
        IRTemp rNt   = newTemp(Ity_I32);
        IRTemp rMt   = newTemp(Ity_I32);
        IRTemp irt_diff  = newTemp(Ity_I32);
        IRTemp irt_sum   = newTemp(Ity_I32);
        IRTemp res_q = newTemp(Ity_I32);

        assign( rNt, isT ? getIRegT(regN) : getIRegA(regN) );
        assign( rMt, isT ? getIRegT(regM) : getIRegA(regM) );

        assign( irt_sum,
                binop(Iop_Add32,
                      unop(Iop_16Uto32,
                           unop(Iop_32to16,
                                mkexpr(rNt)
                           )
                      ),
                      unop(Iop_16Uto32,
                           unop(Iop_32to16,
                                binop(Iop_Shr32,
                                      mkexpr(rMt), mkU8(16)
                                )
                           )
                      )
                )
        );

        assign( irt_diff,
                binop(Iop_Sub32,
                      unop(Iop_16Uto32,
                           unop(Iop_32to16,
                                binop(Iop_Shr32,
                                      mkexpr(rNt), mkU8(16)
                                )
                           )
                      ),
                      unop(Iop_16Uto32,
                           unop(Iop_32to16, mkexpr(rMt)
                           )
                      )
                )
        );

        assign( res_q,
                binop(Iop_Or32, 
                      unop(Iop_16Uto32,
                           unop(Iop_32to16,
                                binop(Iop_Shr32,
                                      mkexpr(irt_sum), mkU8(1)
                                )
                           )
                      ),
                      binop(Iop_Shl32,
                            binop(Iop_Shr32,
                                  mkexpr(irt_diff), mkU8(1)
                            ),
                            mkU8(16)
                     )
                )
        );

        if (isT)
           putIRegT( regD, mkexpr(res_q), condT );
        else
           putIRegA( regD, mkexpr(res_q), condT, Ijk_Boring );

        DIP("uhsax%s r%u, r%u, r%u\n", nCC(conq),regD,regN,regM);
        return True;
     }
     
   }

   
   {
     UInt regD = 99, regN = 99, regM = 99;
     Bool gate = False;

     if (isT) {
        if (INSNT0(15,4) == 0xFAD && (INSNT1(15,0) & 0xF0F0) == 0xF020) {
           regN = INSNT0(3,0);
           regD = INSNT1(11,8);
           regM = INSNT1(3,0);
           if (!isBadRegT(regD) && !isBadRegT(regN) && !isBadRegT(regM))
              gate = True;
        }
     } else {
        if (INSNA(27,20) == BITS8(0,1,1,0,0,0,1,1) &&
            INSNA(11,8)  == BITS4(1,1,1,1)         &&
            INSNA(7,4)   == BITS4(0,1,1,1)) {
           regD = INSNA(15,12);
           regN = INSNA(19,16);
           regM = INSNA(3,0);
           if (regD != 15 && regN != 15 && regM != 15)
              gate = True;
        }
     }

     if (gate) {
        IRTemp rNt   = newTemp(Ity_I32);
        IRTemp rMt   = newTemp(Ity_I32);
        IRTemp res_q = newTemp(Ity_I32);

        assign( rNt, isT ? getIRegT(regN) : getIRegA(regN) );
        assign( rMt, isT ? getIRegT(regM) : getIRegA(regM) );

        assign(res_q, binop(Iop_HSub16Sx2, mkexpr(rNt), mkexpr(rMt)));
        if (isT)
           putIRegT( regD, mkexpr(res_q), condT );
        else
           putIRegA( regD, mkexpr(res_q), condT, Ijk_Boring );

        DIP("shsub16%s r%u, r%u, r%u\n", nCC(conq),regD,regN,regM);
        return True;
     }
     
   }

   
   {
     UInt rD = 99, rN = 99, rM = 99, rA = 99;
     Bool round  = False;
     Bool gate   = False;

     if (isT) {
        if (INSNT0(15,7) == BITS9(1,1,1,1,1,0,1,1,0)
            && INSNT0(6,4) == BITS3(1,1,0)
            && INSNT1(7,5) == BITS3(0,0,0)) {
           round = INSNT1(4,4);
           rA    = INSNT1(15,12);
           rD    = INSNT1(11,8);
           rM    = INSNT1(3,0);
           rN    = INSNT0(3,0);
           if (!isBadRegT(rD)
               && !isBadRegT(rN) && !isBadRegT(rM) && !isBadRegT(rA))
              gate = True;
        }
     } else {
        if (INSNA(27,20) == BITS8(0,1,1,1,0,1,0,1)
            && INSNA(15,12) != BITS4(1,1,1,1)
            && (INSNA(7,4) & BITS4(1,1,0,1)) == BITS4(1,1,0,1)) {
           round = INSNA(5,5);
           rD    = INSNA(19,16);
           rA    = INSNA(15,12);
           rM    = INSNA(11,8);
           rN    = INSNA(3,0);
           if (rD != 15 && rM != 15 && rN != 15)
              gate = True;
        }
     }
     if (gate) {
        IRTemp irt_rA   = newTemp(Ity_I32);
        IRTemp irt_rN   = newTemp(Ity_I32);
        IRTemp irt_rM   = newTemp(Ity_I32);
        assign( irt_rA, isT ? getIRegT(rA) : getIRegA(rA) );
        assign( irt_rN, isT ? getIRegT(rN) : getIRegA(rN) );
        assign( irt_rM, isT ? getIRegT(rM) : getIRegA(rM) );
        IRExpr* res
        = unop(Iop_64HIto32,
               binop(Iop_Add64,
                     binop(Iop_Sub64,
                           binop(Iop_32HLto64, mkexpr(irt_rA), mkU32(0)),
                           binop(Iop_MullS32, mkexpr(irt_rN), mkexpr(irt_rM))),
                     mkU64(round ? 0x80000000ULL : 0ULL)));
        if (isT)
           putIRegT( rD, res, condT );
        else
           putIRegA(rD, res, condT, Ijk_Boring);
        DIP("smmls%s%s r%u, r%u, r%u, r%u\n",
            round ? "r" : "", nCC(conq), rD, rN, rM, rA);
        return True;
     }
     
   }

   
   {
     UInt rN = 99, rDlo = 99, rDhi = 99, rM = 99;
     Bool m_swap = False;
     Bool gate   = False;

     if (isT) {
        if (INSNT0(15,4) == 0xFBC &&
            (INSNT1(7,4) & BITS4(1,1,1,0)) == BITS4(1,1,0,0)) {
           rN     = INSNT0(3,0);
           rDlo   = INSNT1(15,12);
           rDhi   = INSNT1(11,8);
           rM     = INSNT1(3,0);
           m_swap = (INSNT1(4,4) & 1) == 1;
           if (!isBadRegT(rDlo) && !isBadRegT(rDhi) && !isBadRegT(rN)
               && !isBadRegT(rM) && rDhi != rDlo)
              gate = True;
        }
     } else {
        if (INSNA(27,20) == BITS8(0,1,1,1,0,1,0,0)
            && (INSNA(7,4) & BITS4(1,1,0,1)) == BITS4(0,0,0,1)) {
           rN     = INSNA(3,0);
           rDlo   = INSNA(15,12);
           rDhi   = INSNA(19,16);
           rM     = INSNA(11,8);
           m_swap = ( INSNA(5,5) & 1 ) == 1;
           if (rDlo != 15 && rDhi != 15
               && rN != 15 && rM != 15 && rDlo != rDhi)
              gate = True;
        }
     }

     if (gate) {
        IRTemp irt_rM   = newTemp(Ity_I32);
        IRTemp irt_rN   = newTemp(Ity_I32);
        IRTemp irt_rDhi = newTemp(Ity_I32);
        IRTemp irt_rDlo = newTemp(Ity_I32);
        IRTemp op_2     = newTemp(Ity_I32);
        IRTemp pr_1     = newTemp(Ity_I64);
        IRTemp pr_2     = newTemp(Ity_I64);
        IRTemp result   = newTemp(Ity_I64);
        IRTemp resHi    = newTemp(Ity_I32);
        IRTemp resLo    = newTemp(Ity_I32);
        assign( irt_rM, isT ? getIRegT(rM) : getIRegA(rM));
        assign( irt_rN, isT ? getIRegT(rN) : getIRegA(rN));
        assign( irt_rDhi, isT ? getIRegT(rDhi) : getIRegA(rDhi));
        assign( irt_rDlo, isT ? getIRegT(rDlo) : getIRegA(rDlo));
        assign( op_2, genROR32(irt_rM, m_swap ? 16 : 0) );
        assign( pr_1, binop(Iop_MullS32,
                            unop(Iop_16Sto32,
                                 unop(Iop_32to16, mkexpr(irt_rN))
                            ),
                            unop(Iop_16Sto32,
                                 unop(Iop_32to16, mkexpr(op_2))
                            )
                      )
        );
        assign( pr_2, binop(Iop_MullS32,
                            binop(Iop_Sar32, mkexpr(irt_rN), mkU8(16)),
                            binop(Iop_Sar32, mkexpr(op_2), mkU8(16))
                      )
        );
        assign( result, binop(Iop_Add64,
                              binop(Iop_Add64,
                                    mkexpr(pr_1),
                                    mkexpr(pr_2)
                              ),
                              binop(Iop_32HLto64,
                                    mkexpr(irt_rDhi),
                                    mkexpr(irt_rDlo)
                              )
                        )
        );
        assign( resHi, unop(Iop_64HIto32, mkexpr(result)) );
        assign( resLo, unop(Iop_64to32, mkexpr(result)) );
        if (isT) {
           putIRegT( rDhi, mkexpr(resHi), condT );
           putIRegT( rDlo, mkexpr(resLo), condT );
        } else {
           putIRegA( rDhi, mkexpr(resHi), condT, Ijk_Boring );
           putIRegA( rDlo, mkexpr(resLo), condT, Ijk_Boring );
        }
        DIP("smlald%c%s r%u, r%u, r%u, r%u\n",
            m_swap ? 'x' : ' ', nCC(conq), rDlo, rDhi, rN, rM);
        return True;
     }
     
   }

   
   {
     UInt rN = 99, rDlo = 99, rDhi = 99, rM = 99;
     Bool m_swap = False;
     Bool gate   = False;

     if (isT) {
        if ((INSNT0(15,4) == 0xFBD &&
            (INSNT1(7,4) & BITS4(1,1,1,0)) == BITS4(1,1,0,0))) {
           rN     = INSNT0(3,0);
           rDlo   = INSNT1(15,12);
           rDhi   = INSNT1(11,8);
           rM     = INSNT1(3,0);
           m_swap = (INSNT1(4,4) & 1) == 1;
           if (!isBadRegT(rDlo) && !isBadRegT(rDhi) && !isBadRegT(rN) &&
               !isBadRegT(rM) && rDhi != rDlo)
              gate = True;
        }
     } else {
        if (INSNA(27,20) == BITS8(0,1,1,1,0,1,0,0) &&
            (INSNA(7,4) & BITS4(1,1,0,1)) == BITS4(0,1,0,1)) {
           rN     = INSNA(3,0);
           rDlo   = INSNA(15,12);
           rDhi   = INSNA(19,16);
           rM     = INSNA(11,8);
           m_swap = (INSNA(5,5) & 1) == 1;
           if (rDlo != 15 && rDhi != 15 &&
               rN != 15 && rM != 15 && rDlo != rDhi)
              gate = True;
        }
     }
     if (gate) {
        IRTemp irt_rM   = newTemp(Ity_I32);
        IRTemp irt_rN   = newTemp(Ity_I32);
        IRTemp irt_rDhi = newTemp(Ity_I32);
        IRTemp irt_rDlo = newTemp(Ity_I32);
        IRTemp op_2     = newTemp(Ity_I32);
        IRTemp pr_1     = newTemp(Ity_I64);
        IRTemp pr_2     = newTemp(Ity_I64);
        IRTemp result   = newTemp(Ity_I64);
        IRTemp resHi    = newTemp(Ity_I32);
        IRTemp resLo    = newTemp(Ity_I32);
        assign( irt_rM, isT ? getIRegT(rM) : getIRegA(rM) );
        assign( irt_rN, isT ? getIRegT(rN) : getIRegA(rN) );
        assign( irt_rDhi, isT ? getIRegT(rDhi) : getIRegA(rDhi) );
        assign( irt_rDlo, isT ? getIRegT(rDlo) : getIRegA(rDlo) );
        assign( op_2, genROR32(irt_rM, m_swap ? 16 : 0) );
        assign( pr_1, binop(Iop_MullS32,
                            unop(Iop_16Sto32,
                                 unop(Iop_32to16, mkexpr(irt_rN))
                            ),
                            unop(Iop_16Sto32,
                                 unop(Iop_32to16, mkexpr(op_2))
                            )
                      )
        );
        assign( pr_2, binop(Iop_MullS32,
                            binop(Iop_Sar32, mkexpr(irt_rN), mkU8(16)),
                            binop(Iop_Sar32, mkexpr(op_2), mkU8(16))
                      )
        );
        assign( result, binop(Iop_Add64,
                              binop(Iop_Sub64,
                                    mkexpr(pr_1),
                                    mkexpr(pr_2)
                              ),
                              binop(Iop_32HLto64,
                                    mkexpr(irt_rDhi),
                                    mkexpr(irt_rDlo)
                              )
                        )
        );
        assign( resHi, unop(Iop_64HIto32, mkexpr(result)) );
        assign( resLo, unop(Iop_64to32, mkexpr(result)) );
        if (isT) {
           putIRegT( rDhi, mkexpr(resHi), condT );
           putIRegT( rDlo, mkexpr(resLo), condT );
        } else {
           putIRegA( rDhi, mkexpr(resHi), condT, Ijk_Boring );
           putIRegA( rDlo, mkexpr(resLo), condT, Ijk_Boring );
        }
        DIP("smlsld%c%s r%u, r%u, r%u, r%u\n",
            m_swap ? 'x' : ' ', nCC(conq), rDlo, rDhi, rN, rM);
        return True;
     }
     
   }

   
   return False;

#  undef INSNA
#  undef INSNT0
#  undef INSNT1
}



static void mk_ldm_stm ( Bool arm,     
                         UInt rN,      
                         UInt bINC,    
                         UInt bBEFORE, 
                         UInt bW,      
                         UInt bL,      
                         UInt regList )
{
   Int i, r, m, nRegs;
   IRTemp jk = Ijk_Boring;

   IRTemp oldRnT = newTemp(Ity_I32);
   assign(oldRnT, arm ? getIRegA(rN) : getIRegT(rN));

   IRTemp anchorT = newTemp(Ity_I32);
   
   
   
   assign(anchorT, mkexpr(oldRnT));

   IROp opADDorSUB = bINC ? Iop_Add32 : Iop_Sub32;
   
   

   
   
   
   
   
   
   
   nRegs = 0;
   for (i = 0; i < 16; i++) {
     if ((regList & (1 << i)) != 0)
         nRegs++;
   }
   if (bW == 1 && !bINC) {
      IRExpr* e = binop(opADDorSUB, mkexpr(oldRnT), mkU32(4*nRegs));
      if (arm)
         putIRegA( rN, e, IRTemp_INVALID, Ijk_Boring );
      else
         putIRegT( rN, e, IRTemp_INVALID );
   }

   
   
   
   UInt xReg[16], xOff[16];
   Int  nX = 0;
   m = 0;
   for (i = 0; i < 16; i++) {
      r = bINC ? i : (15-i);
      if (0 == (regList & (1<<r)))
         continue;
      if (bBEFORE)
         m++;
      if (bW == 1 && bL == 1)
         vassert(r != rN);

      xOff[nX] = 4 * m;
      xReg[nX] = r;
      nX++;

      if (!bBEFORE)
         m++;
   }
   vassert(m == nRegs);
   vassert(nX == nRegs);
   vassert(nX <= 16);

   if (bW == 0 && (regList & (1<<rN)) != 0) {
      if (0) {
         vex_printf("\nREG_LIST_PRE: (rN=%d)\n", rN);
         for (i = 0; i < nX; i++)
            vex_printf("reg %d   off %d\n", xReg[i], xOff[i]);
         vex_printf("\n");
      }

      vassert(nX > 0);
      for (i = 0; i < nX; i++) {
         if (xReg[i] == rN)
             break;
      }
      vassert(i < nX); 
      UInt tReg = xReg[i];
      UInt tOff = xOff[i];
      if (bL == 1) {
         
         if (i < nX-1) {
            for (m = i+1; m < nX; m++) {
               xReg[m-1] = xReg[m];
               xOff[m-1] = xOff[m];
            }
            vassert(m == nX);
            xReg[m-1] = tReg;
            xOff[m-1] = tOff;
         }
      } else {
         
         if (i > 0) {
            for (m = i-1; m >= 0; m--) {
               xReg[m+1] = xReg[m];
               xOff[m+1] = xOff[m];
            }
            vassert(m == -1);
            xReg[0] = tReg;
            xOff[0] = tOff;
         }
      }

      if (0) {
         vex_printf("REG_LIST_POST:\n");
         for (i = 0; i < nX; i++)
            vex_printf("reg %d   off %d\n", xReg[i], xOff[i]);
         vex_printf("\n");
      }
   }

   if (rN == 13 && bL == 1 && bINC && !bBEFORE && bW == 1) {
      jk = Ijk_Ret;
   }

   
   for (i = 0; i < nX; i++) {
      r = xReg[i];
      if (bL == 1) {
         IRExpr* e = loadLE(Ity_I32,
                            binop(opADDorSUB, mkexpr(anchorT),
                                  mkU32(xOff[i])));
         if (arm) {
            putIRegA( r, e, IRTemp_INVALID, jk );
         } else {
            
            
            
            
            llPutIReg( r, e );
         }
      } else {
         storeLE( binop(opADDorSUB, mkexpr(anchorT), mkU32(xOff[i])),
                  r == rN ? mkexpr(oldRnT) 
                          : (arm ? getIRegA(r) : getIRegT(r) ) );
      }
   }

   
   
   if (bW == 1 && bINC) {
      IRExpr* e = binop(opADDorSUB, mkexpr(oldRnT), mkU32(4*nRegs));
      if (arm)
         putIRegA( rN, e, IRTemp_INVALID, Ijk_Boring );
      else
         putIRegT( rN, e, IRTemp_INVALID );
   }
}




static Bool decode_CP10_CP11_instruction (
               DisResult* dres,
               UInt              insn28,
               IRTemp            condT,
               ARMCondcode       conq,
               Bool              isT
            )
{
#  define INSN(_bMax,_bMin)  SLICE_UInt(insn28, (_bMax), (_bMin))

   vassert(INSN(31,28) == BITS4(0,0,0,0)); 

   if (isT) {
      vassert(conq == ARMCondAL);
   } else {
      vassert(conq >= ARMCondEQ && conq <= ARMCondAL);
   }

   
   
   

   
   if (BITS8(1,1,0,0,0,0,0,0) == (INSN(27,20) & BITS8(1,1,1,0,0,0,0,0))
       && INSN(11,8) == BITS4(1,0,1,1)) {
      UInt bP      = (insn28 >> 24) & 1;
      UInt bU      = (insn28 >> 23) & 1;
      UInt bW      = (insn28 >> 21) & 1;
      UInt bL      = (insn28 >> 20) & 1;
      UInt offset  = (insn28 >> 0) & 0xFF;
      UInt rN      = INSN(19,16);
      UInt dD      = (INSN(22,22) << 4) | INSN(15,12);
      UInt nRegs   = (offset - 1) / 2;
      UInt summary = 0;
      Int  i;

       if (bP == 0 && bU == 1 && bW == 0) {
         summary = 1;
      }
      else if (bP == 0 && bU == 1 && bW == 1) {
         summary = 2;
      }
      else if (bP == 1 && bU == 0 && bW == 1) {
         summary = 3;
      }
      else goto after_vfp_fldmx_fstmx;

      
      if (rN == 15 && (summary == 2 || summary == 3 || isT))
         goto after_vfp_fldmx_fstmx;

      
      if (0 == (offset & 1) || offset < 3)
         goto after_vfp_fldmx_fstmx;

      
      if (dD + nRegs - 1 >= 32)
         goto after_vfp_fldmx_fstmx;

      if (condT != IRTemp_INVALID) {
         if (isT)
            mk_skip_over_T32_if_cond_is_false( condT );
         else
            mk_skip_over_A32_if_cond_is_false( condT );
         condT = IRTemp_INVALID;
      }
      

      
      IRTemp rnT = newTemp(Ity_I32);
      assign(rnT, align4if(isT ? getIRegT(rN) : getIRegA(rN),
                           rN == 15));

      
      IRTemp rnTnew = IRTemp_INVALID;
      if (summary == 2 || summary == 3) {
         rnTnew = newTemp(Ity_I32);
         assign(rnTnew, binop(summary == 2 ? Iop_Add32 : Iop_Sub32,
                              mkexpr(rnT),
                              mkU32(4 + 8 * nRegs)));
      }

      
      IRTemp taT = newTemp(Ity_I32);
      assign(taT,  summary == 3 ? mkexpr(rnTnew) : mkexpr(rnT));

      if (summary == 3) {
         if (isT)
            putIRegT(rN, mkexpr(rnTnew), IRTemp_INVALID);
         else
            putIRegA(rN, mkexpr(rnTnew), IRTemp_INVALID, Ijk_Boring);
      }

      
      for (i = 0; i < nRegs; i++) {
         IRExpr* addr = binop(Iop_Add32, mkexpr(taT), mkU32(8*i));
         if (bL) {
            putDReg(dD + i, loadLE(Ity_F64, addr), IRTemp_INVALID);
         } else {
            storeLE(addr, getDReg(dD + i));
         }
      }

      if (summary == 2) {
         if (isT)
            putIRegT(rN, mkexpr(rnTnew), IRTemp_INVALID);
         else
            putIRegA(rN, mkexpr(rnTnew), IRTemp_INVALID, Ijk_Boring);
      }

      const HChar* nm = bL==1 ? "ld" : "st";
      switch (summary) {
         case 1:  DIP("f%smx%s r%u, {d%u-d%u}\n", 
                      nm, nCC(conq), rN, dD, dD + nRegs - 1);
                  break;
         case 2:  DIP("f%smiax%s r%u!, {d%u-d%u}\n", 
                      nm, nCC(conq), rN, dD, dD + nRegs - 1);
                  break;
         case 3:  DIP("f%smdbx%s r%u!, {d%u-d%u}\n", 
                      nm, nCC(conq), rN, dD, dD + nRegs - 1);
                  break;
         default: vassert(0);
      }

      goto decode_success_vfp;
      
   }

  after_vfp_fldmx_fstmx:

   
   if (BITS8(1,1,0,0,0,0,0,0) == (INSN(27,20) & BITS8(1,1,1,0,0,0,0,0))
       && INSN(11,8) == BITS4(1,0,1,1)) {
      UInt bP      = (insn28 >> 24) & 1;
      UInt bU      = (insn28 >> 23) & 1;
      UInt bW      = (insn28 >> 21) & 1;
      UInt bL      = (insn28 >> 20) & 1;
      UInt offset  = (insn28 >> 0) & 0xFF;
      UInt rN      = INSN(19,16);
      UInt dD      = (INSN(22,22) << 4) | INSN(15,12);
      UInt nRegs   = offset / 2;
      UInt summary = 0;
      Int  i;

       if (bP == 0 && bU == 1 && bW == 0) {
         summary = 1;
      }
      else if (bP == 0 && bU == 1 && bW == 1) {
         summary = 2;
      }
      else if (bP == 1 && bU == 0 && bW == 1) {
         summary = 3;
      }
      else goto after_vfp_fldmd_fstmd;

      
      if (rN == 15 && (summary == 2 || summary == 3 || isT))
         goto after_vfp_fldmd_fstmd;

      
      if (1 == (offset & 1) || offset < 2)
         goto after_vfp_fldmd_fstmd;

      
      if (dD + nRegs - 1 >= 32)
         goto after_vfp_fldmd_fstmd;

      if (condT != IRTemp_INVALID) {
         if (isT)
            mk_skip_over_T32_if_cond_is_false( condT );
         else
            mk_skip_over_A32_if_cond_is_false( condT );
         condT = IRTemp_INVALID;
      }
      

      
      IRTemp rnT = newTemp(Ity_I32);
      assign(rnT, align4if(isT ? getIRegT(rN) : getIRegA(rN),
                           rN == 15));

      
      IRTemp rnTnew = IRTemp_INVALID;
      if (summary == 2 || summary == 3) {
         rnTnew = newTemp(Ity_I32);
         assign(rnTnew, binop(summary == 2 ? Iop_Add32 : Iop_Sub32,
                              mkexpr(rnT),
                              mkU32(8 * nRegs)));
      }

      
      IRTemp taT = newTemp(Ity_I32);
      assign(taT, summary == 3 ? mkexpr(rnTnew) : mkexpr(rnT));

      if (summary == 3) {
         if (isT)
            putIRegT(rN, mkexpr(rnTnew), IRTemp_INVALID);
         else
            putIRegA(rN, mkexpr(rnTnew), IRTemp_INVALID, Ijk_Boring);
      }

      
      for (i = 0; i < nRegs; i++) {
         IRExpr* addr = binop(Iop_Add32, mkexpr(taT), mkU32(8*i));
         if (bL) {
            putDReg(dD + i, loadLE(Ity_F64, addr), IRTemp_INVALID);
         } else {
            storeLE(addr, getDReg(dD + i));
         }
      }

      if (summary == 2) {
         if (isT)
            putIRegT(rN, mkexpr(rnTnew), IRTemp_INVALID);
         else
            putIRegA(rN, mkexpr(rnTnew), IRTemp_INVALID, Ijk_Boring);
      }

      const HChar* nm = bL==1 ? "ld" : "st";
      switch (summary) {
         case 1:  DIP("f%smd%s r%u, {d%u-d%u}\n", 
                      nm, nCC(conq), rN, dD, dD + nRegs - 1);
                  break;
         case 2:  DIP("f%smiad%s r%u!, {d%u-d%u}\n", 
                      nm, nCC(conq), rN, dD, dD + nRegs - 1);
                  break;
         case 3:  DIP("f%smdbd%s r%u!, {d%u-d%u}\n", 
                      nm, nCC(conq), rN, dD, dD + nRegs - 1);
                  break;
         default: vassert(0);
      }

      goto decode_success_vfp;
      
   }

  after_vfp_fldmd_fstmd:

   
   if (BITS8(1,1,1,0,1,1,1,1) == INSN(27,20)
       && BITS4(1,0,1,0) == INSN(11,8)
       && BITS8(0,0,0,1,0,0,0,0) == (insn28 & 0xFF)) {
      UInt rD  = INSN(15,12);
      UInt reg = INSN(19,16);
      if (reg == BITS4(0,0,0,1)) {
         if (rD == 15) {
            IRTemp nzcvT = newTemp(Ity_I32);
            assign(nzcvT, binop(Iop_And32,
                                IRExpr_Get(OFFB_FPSCR, Ity_I32),
                                mkU32(0xF0000000)));
            setFlags_D1(ARMG_CC_OP_COPY, nzcvT, condT);
            DIP("fmstat%s\n", nCC(conq));
         } else {
            
            IRExpr* e = IRExpr_Get(OFFB_FPSCR, Ity_I32);
            if (isT)
               putIRegT(rD, e, condT);
            else
               putIRegA(rD, e, condT, Ijk_Boring);
            DIP("fmrx%s r%u, fpscr\n", nCC(conq), rD);
         }
         goto decode_success_vfp;
      }
      
   }

   if (BITS8(1,1,1,0,1,1,1,0) == INSN(27,20)
       && BITS4(1,0,1,0) == INSN(11,8)
       && BITS8(0,0,0,1,0,0,0,0) == (insn28 & 0xFF)) {
      UInt rD  = INSN(15,12);
      UInt reg = INSN(19,16);
      if (reg == BITS4(0,0,0,1)) {
         putMiscReg32(OFFB_FPSCR,
                      isT ? getIRegT(rD) : getIRegA(rD), condT);
         DIP("fmxr%s fpscr, r%u\n", nCC(conq), rD);
         goto decode_success_vfp;
      }
      
   }

   
   
   if (0x0C400B10 == (insn28 & 0x0FF00FD0)) {
      UInt dM = INSN(3,0) | (INSN(5,5) << 4);
      UInt rD = INSN(15,12); 
      UInt rN = INSN(19,16); 
      if (rD == 15 || rN == 15 || (isT && (rD == 13 || rN == 13))) {
         
      } else {
         putDReg(dM,
                 unop(Iop_ReinterpI64asF64,
                      binop(Iop_32HLto64,
                            isT ? getIRegT(rN) : getIRegA(rN),
                            isT ? getIRegT(rD) : getIRegA(rD))),
                 condT);
         DIP("vmov%s d%u, r%u, r%u\n", nCC(conq), dM, rD, rN);
         goto decode_success_vfp;
      }
      
   }

   
   if (0x0C500B10 == (insn28 & 0x0FF00FD0)) {
      UInt dM = INSN(3,0) | (INSN(5,5) << 4);
      UInt rD = INSN(15,12); 
      UInt rN = INSN(19,16); 
      if (rD == 15 || rN == 15 || (isT && (rD == 13 || rN == 13))
          || rD == rN) {
         
      } else {
         IRTemp i64 = newTemp(Ity_I64);
         assign(i64, unop(Iop_ReinterpF64asI64, getDReg(dM)));
         IRExpr* hi32 = unop(Iop_64HIto32, mkexpr(i64));
         IRExpr* lo32 = unop(Iop_64to32,   mkexpr(i64));
         if (isT) {
            putIRegT(rN, hi32, condT);
            putIRegT(rD, lo32, condT);
         } else {
            putIRegA(rN, hi32, condT, Ijk_Boring);
            putIRegA(rD, lo32, condT, Ijk_Boring);
         }
         DIP("vmov%s r%u, r%u, d%u\n", nCC(conq), rD, rN, dM);
         goto decode_success_vfp;
      }
      
   }

   
   if (0x0C400A10 == (insn28 & 0x0FF00FD0)) {
      UInt sD = (INSN(3,0) << 1) | INSN(5,5);
      UInt rN = INSN(15,12);
      UInt rM = INSN(19,16);
      if (rM == 15 || rN == 15 || (isT && (rM == 13 || rN == 13))
          || sD == 31) {
         
      } else {
         putFReg(sD,
                 unop(Iop_ReinterpI32asF32, isT ? getIRegT(rN) : getIRegA(rN)),
                 condT);
         putFReg(sD+1,
                 unop(Iop_ReinterpI32asF32, isT ? getIRegT(rM) : getIRegA(rM)),
                 condT);
         DIP("vmov%s, s%u, s%u, r%u, r%u\n",
              nCC(conq), sD, sD + 1, rN, rM);
         goto decode_success_vfp;
      }
   }

   
   if (0x0C500A10 == (insn28 & 0x0FF00FD0)) {
      UInt sD = (INSN(3,0) << 1) | INSN(5,5);
      UInt rN = INSN(15,12);
      UInt rM = INSN(19,16);
      if (rM == 15 || rN == 15 || (isT && (rM == 13 || rN == 13))
          || sD == 31 || rN == rM) {
         
      } else {
         IRExpr* res0 = unop(Iop_ReinterpF32asI32, getFReg(sD));
         IRExpr* res1 = unop(Iop_ReinterpF32asI32, getFReg(sD+1));
         if (isT) {
            putIRegT(rN, res0, condT);
            putIRegT(rM, res1, condT);
         } else {
            putIRegA(rN, res0, condT, Ijk_Boring);
            putIRegA(rM, res1, condT, Ijk_Boring);
         }
         DIP("vmov%s, r%u, r%u, s%u, s%u\n",
             nCC(conq), rN, rM, sD, sD + 1);
         goto decode_success_vfp;
      }
   }

   
   if (0x0E000B10 == (insn28 & 0x0F900F1F)) {
      UInt rD  = (INSN(7,7) << 4) | INSN(19,16);
      UInt rT  = INSN(15,12);
      UInt opc = (INSN(22,21) << 2) | INSN(6,5);
      UInt index;
      if (rT == 15 || (isT && rT == 13)) {
         
      } else {
         if ((opc & BITS4(1,0,0,0)) == BITS4(1,0,0,0)) {
            index = opc & 7;
            putDRegI64(rD, triop(Iop_SetElem8x8,
                                 getDRegI64(rD),
                                 mkU8(index),
                                 unop(Iop_32to8,
                                      isT ? getIRegT(rT) : getIRegA(rT))),
                           condT);
            DIP("vmov%s.8 d%u[%u], r%u\n", nCC(conq), rD, index, rT);
            goto decode_success_vfp;
         }
         else if ((opc & BITS4(1,0,0,1)) == BITS4(0,0,0,1)) {
            index = (opc >> 1) & 3;
            putDRegI64(rD, triop(Iop_SetElem16x4,
                                 getDRegI64(rD),
                                 mkU8(index),
                                 unop(Iop_32to16,
                                      isT ? getIRegT(rT) : getIRegA(rT))),
                           condT);
            DIP("vmov%s.16 d%u[%u], r%u\n", nCC(conq), rD, index, rT);
            goto decode_success_vfp;
         }
         else if ((opc & BITS4(1,0,1,1)) == BITS4(0,0,0,0)) {
            index = (opc >> 2) & 1;
            putDRegI64(rD, triop(Iop_SetElem32x2,
                                 getDRegI64(rD),
                                 mkU8(index),
                                 isT ? getIRegT(rT) : getIRegA(rT)),
                           condT);
            DIP("vmov%s.32 d%u[%u], r%u\n", nCC(conq), rD, index, rT);
            goto decode_success_vfp;
         } else {
            
         }
      }
   }

   
   
   if (0x0E100B10 == (insn28 & 0x0F100F1F)) {
      UInt rN  = (INSN(7,7) << 4) | INSN(19,16);
      UInt rT  = INSN(15,12);
      UInt U   = INSN(23,23);
      UInt opc = (INSN(22,21) << 2) | INSN(6,5);
      UInt index;
      if (rT == 15 || (isT && rT == 13)) {
         
      } else {
         if ((opc & BITS4(1,0,0,0)) == BITS4(1,0,0,0)) {
            index = opc & 7;
            IRExpr* e = unop(U ? Iop_8Uto32 : Iop_8Sto32,
                             binop(Iop_GetElem8x8,
                                   getDRegI64(rN),
                                   mkU8(index)));
            if (isT)
               putIRegT(rT, e, condT);
            else
               putIRegA(rT, e, condT, Ijk_Boring);
            DIP("vmov%s.%c8 r%u, d%u[%u]\n", nCC(conq), U ? 'u' : 's',
                  rT, rN, index);
            goto decode_success_vfp;
         }
         else if ((opc & BITS4(1,0,0,1)) == BITS4(0,0,0,1)) {
            index = (opc >> 1) & 3;
            IRExpr* e = unop(U ? Iop_16Uto32 : Iop_16Sto32,
                             binop(Iop_GetElem16x4,
                                   getDRegI64(rN),
                                   mkU8(index)));
            if (isT)
               putIRegT(rT, e, condT);
            else
               putIRegA(rT, e, condT, Ijk_Boring);
            DIP("vmov%s.%c16 r%u, d%u[%u]\n", nCC(conq), U ? 'u' : 's',
                  rT, rN, index);
            goto decode_success_vfp;
         }
         else if ((opc & BITS4(1,0,1,1)) == BITS4(0,0,0,0) && U == 0) {
            index = (opc >> 2) & 1;
            IRExpr* e = binop(Iop_GetElem32x2, getDRegI64(rN), mkU8(index));
            if (isT)
               putIRegT(rT, e, condT);
            else
               putIRegA(rT, e, condT, Ijk_Boring);
            DIP("vmov%s.32 r%u, d%u[%u]\n", nCC(conq), rT, rN, index);
            goto decode_success_vfp;
         } else {
            
         }
      }
   }

   
   
   if (BITS8(1,1,1,0,1,0,1,1) == (INSN(27,20) & BITS8(1,1,1,1,1,0,1,1))
       && BITS4(0,0,0,0) == INSN(7,4) && INSN(11,8) == BITS4(1,0,1,0)) {
      UInt rD   = (INSN(15,12) << 1) | INSN(22,22);
      UInt imm8 = (INSN(19,16) << 4) | INSN(3,0);
      UInt b    = (imm8 >> 6) & 1;
      UInt imm;
      imm = (BITS8((imm8 >> 7) & 1,(~b) & 1,b,b,b,b,b,(imm8 >> 5) & 1) << 8)
             | ((imm8 & 0x1f) << 3);
      imm <<= 16;
      putFReg(rD, unop(Iop_ReinterpI32asF32, mkU32(imm)), condT);
      DIP("fconsts%s s%u #%u", nCC(conq), rD, imm8);
      goto decode_success_vfp;
   }

   
   
   if (BITS8(1,1,1,0,1,0,1,1) == (INSN(27,20) & BITS8(1,1,1,1,1,0,1,1))
       && BITS4(0,0,0,0) == INSN(7,4) && INSN(11,8) == BITS4(1,0,1,1)) {
      UInt rD   = INSN(15,12) | (INSN(22,22) << 4);
      UInt imm8 = (INSN(19,16) << 4) | INSN(3,0);
      UInt b    = (imm8 >> 6) & 1;
      ULong imm;
      imm = (BITS8((imm8 >> 7) & 1,(~b) & 1,b,b,b,b,b,b) << 8)
             | BITS8(b,b,0,0,0,0,0,0) | (imm8 & 0x3f);
      imm <<= 48;
      putDReg(rD, unop(Iop_ReinterpI64asF64, mkU64(imm)), condT);
      DIP("fconstd%s d%u #%u", nCC(conq), rD, imm8);
      goto decode_success_vfp;
   }

   
   
   
   if (BITS8(1,1,1,0,1,0,0,0) == (INSN(27,20) & BITS8(1,1,1,1,1,0,0,1))
       && BITS4(1,0,1,1) == INSN(11,8) && INSN(6,6) == 0 && INSN(4,4) == 1) {
      UInt rD   = (INSN(7,7) << 4) | INSN(19,16);
      UInt rT   = INSN(15,12);
      UInt Q    = INSN(21,21);
      UInt size = (INSN(22,22) << 1) | INSN(5,5);
      if (rT == 15 || (isT && rT == 13) || size == 3 || (Q && (rD & 1))) {
         
      } else {
         IRExpr* e = isT ? getIRegT(rT) : getIRegA(rT);
         if (Q) {
            rD >>= 1;
            switch (size) {
               case 0:
                  putQReg(rD, unop(Iop_Dup32x4, e), condT);
                  break;
               case 1:
                  putQReg(rD, unop(Iop_Dup16x8, unop(Iop_32to16, e)),
                              condT);
                  break;
               case 2:
                  putQReg(rD, unop(Iop_Dup8x16, unop(Iop_32to8, e)),
                              condT);
                  break;
               default:
                  vassert(0);
            }
            DIP("vdup.%u q%u, r%u\n", 32 / (1<<size), rD, rT);
         } else {
            switch (size) {
               case 0:
                  putDRegI64(rD, unop(Iop_Dup32x2, e), condT);
                  break;
               case 1:
                  putDRegI64(rD, unop(Iop_Dup16x4, unop(Iop_32to16, e)),
                               condT);
                  break;
               case 2:
                  putDRegI64(rD, unop(Iop_Dup8x8, unop(Iop_32to8, e)),
                               condT);
                  break;
               default:
                  vassert(0);
            }
            DIP("vdup.%u d%u, r%u\n", 32 / (1<<size), rD, rT);
         }
         goto decode_success_vfp;
      }
   }

   
   
   if (BITS8(1,1,0,1,0,0,0,0) == (INSN(27,20) & BITS8(1,1,1,1,0,0,1,0))
       && BITS4(1,0,1,1) == INSN(11,8)) {
      UInt dD     = INSN(15,12) | (INSN(22,22) << 4);
      UInt rN     = INSN(19,16);
      UInt offset = (insn28 & 0xFF) << 2;
      UInt bU     = (insn28 >> 23) & 1; 
      UInt bL     = (insn28 >> 20) & 1; 
      
      if (condT != IRTemp_INVALID) {
         if (isT)
            mk_skip_over_T32_if_cond_is_false( condT );
         else
            mk_skip_over_A32_if_cond_is_false( condT );
         condT = IRTemp_INVALID;
      }
      IRTemp ea = newTemp(Ity_I32);
      assign(ea, binop(bU ? Iop_Add32 : Iop_Sub32,
                       align4if(isT ? getIRegT(rN) : getIRegA(rN),
                                rN == 15),
                       mkU32(offset)));
      if (bL) {
         putDReg(dD, loadLE(Ity_F64,mkexpr(ea)), IRTemp_INVALID);
      } else {
         storeLE(mkexpr(ea), getDReg(dD));
      }
      DIP("f%sd%s d%u, [r%u, %c#%u]\n",
          bL ? "ld" : "st", nCC(conq), dD, rN,
          bU ? '+' : '-', offset);
      goto decode_success_vfp;
   }

   
   if (BITS8(1,1,1,0,0,0,0,0) == (INSN(27,20) & BITS8(1,1,1,1,0,0,0,0))
       && BITS4(1,0,1,1) == INSN(11,8)
       && BITS4(0,0,0,0) == (INSN(7,4) & BITS4(0,0,0,1))) {
      UInt    dM  = INSN(3,0)   | (INSN(5,5) << 4);       
      UInt    dD  = INSN(15,12) | (INSN(22,22) << 4);   
      UInt    dN  = INSN(19,16) | (INSN(7,7) << 4);     
      UInt    bP  = (insn28 >> 23) & 1;
      UInt    bQ  = (insn28 >> 21) & 1;
      UInt    bR  = (insn28 >> 20) & 1;
      UInt    bS  = (insn28 >> 6) & 1;
      UInt    opc = (bP << 3) | (bQ << 2) | (bR << 1) | bS;
      IRExpr* rm  = get_FAKE_roundingmode(); 
      switch (opc) {
         case BITS4(0,0,0,0): 
            putDReg(dD, triop(Iop_AddF64, rm,
                              getDReg(dD),
                              triop(Iop_MulF64, rm, getDReg(dN),
                                                    getDReg(dM))),
                        condT);
            DIP("fmacd%s d%u, d%u, d%u\n", nCC(conq), dD, dN, dM);
            goto decode_success_vfp;
         case BITS4(0,0,0,1): 
            putDReg(dD, triop(Iop_AddF64, rm,
                              getDReg(dD),
                              unop(Iop_NegF64,
                                   triop(Iop_MulF64, rm, getDReg(dN),
                                                         getDReg(dM)))),
                        condT);
            DIP("fnmacd%s d%u, d%u, d%u\n", nCC(conq), dD, dN, dM);
            goto decode_success_vfp;
         case BITS4(0,0,1,0): 
            putDReg(dD, triop(Iop_AddF64, rm,
                              unop(Iop_NegF64, getDReg(dD)),
                              triop(Iop_MulF64, rm, getDReg(dN),
                                                    getDReg(dM))),
                        condT);
            DIP("fmscd%s d%u, d%u, d%u\n", nCC(conq), dD, dN, dM);
            goto decode_success_vfp;
         case BITS4(0,0,1,1): 
            putDReg(dD, triop(Iop_AddF64, rm,
                              unop(Iop_NegF64, getDReg(dD)),
                              unop(Iop_NegF64,
                                   triop(Iop_MulF64, rm, getDReg(dN),
                                                         getDReg(dM)))),
                        condT);
            DIP("fnmscd%s d%u, d%u, d%u\n", nCC(conq), dD, dN, dM);
            goto decode_success_vfp;
         case BITS4(0,1,0,0): 
            putDReg(dD, triop(Iop_MulF64, rm, getDReg(dN), getDReg(dM)),
                        condT);
            DIP("fmuld%s d%u, d%u, d%u\n", nCC(conq), dD, dN, dM);
            goto decode_success_vfp;
         case BITS4(0,1,0,1): 
            putDReg(dD, unop(Iop_NegF64,
                             triop(Iop_MulF64, rm, getDReg(dN),
                                                   getDReg(dM))),
                    condT);
            DIP("fnmuld%s d%u, d%u, d%u\n", nCC(conq), dD, dN, dM);
            goto decode_success_vfp;
         case BITS4(0,1,1,0): 
            putDReg(dD, triop(Iop_AddF64, rm, getDReg(dN), getDReg(dM)),
                        condT);
            DIP("faddd%s d%u, d%u, d%u\n", nCC(conq), dD, dN, dM);
            goto decode_success_vfp;
         case BITS4(0,1,1,1): 
            putDReg(dD, triop(Iop_SubF64, rm, getDReg(dN), getDReg(dM)),
                        condT);
            DIP("fsubd%s d%u, d%u, d%u\n", nCC(conq), dD, dN, dM);
            goto decode_success_vfp;
         case BITS4(1,0,0,0): 
            putDReg(dD, triop(Iop_DivF64, rm, getDReg(dN), getDReg(dM)),
                        condT);
            DIP("fdivd%s d%u, d%u, d%u\n", nCC(conq), dD, dN, dM);
            goto decode_success_vfp;
         case BITS4(1,0,1,0): 
            putDReg(dD, triop(Iop_AddF64, rm,
                              unop(Iop_NegF64, getDReg(dD)),
                              triop(Iop_MulF64, rm,
                                                getDReg(dN),
                                                getDReg(dM))),
                        condT);
            DIP("vfnmsd%s d%u, d%u, d%u\n", nCC(conq), dD, dN, dM);
            goto decode_success_vfp;
         case BITS4(1,0,1,1): 
            putDReg(dD, triop(Iop_AddF64, rm,
                              unop(Iop_NegF64, getDReg(dD)),
                              triop(Iop_MulF64, rm,
                                                unop(Iop_NegF64, getDReg(dN)),
                                                getDReg(dM))),
                        condT);
            DIP("vfnmad%s d%u, d%u, d%u\n", nCC(conq), dD, dN, dM);
            goto decode_success_vfp;
         case BITS4(1,1,0,0): 
            putDReg(dD, triop(Iop_AddF64, rm,
                              getDReg(dD),
                              triop(Iop_MulF64, rm, getDReg(dN),
                                                    getDReg(dM))),
                        condT);
            DIP("vfmad%s d%u, d%u, d%u\n", nCC(conq), dD, dN, dM);
            goto decode_success_vfp;
         case BITS4(1,1,0,1): 
            putDReg(dD, triop(Iop_AddF64, rm,
                              getDReg(dD),
                              triop(Iop_MulF64, rm,
                                    unop(Iop_NegF64, getDReg(dN)),
                                    getDReg(dM))),
                        condT);
            DIP("vfmsd%s d%u, d%u, d%u\n", nCC(conq), dD, dN, dM);
            goto decode_success_vfp;
         default:
            break;
      }
   }

   
   if (BITS8(1,1,1,0,1,0,1,1) == (INSN(27,20) & BITS8(1,1,1,1,1,0,1,1))
       && BITS4(0,1,0,0) == (INSN(19,16) & BITS4(1,1,1,0))
       && BITS4(1,0,1,1) == INSN(11,8)
       && BITS4(0,1,0,0) == (INSN(7,4) & BITS4(0,1,0,1))) {
      UInt bZ = (insn28 >> 16) & 1;
      UInt bN = (insn28 >> 7) & 1;
      UInt dD = INSN(15,12) | (INSN(22,22) << 4);
      UInt dM = INSN(3,0) | (INSN(5,5) << 4);
      if (bZ && INSN(3,0) != 0) {
         
      } else {
         IRTemp argL = newTemp(Ity_F64);
         IRTemp argR = newTemp(Ity_F64);
         IRTemp irRes = newTemp(Ity_I32);
         assign(argL, getDReg(dD));
         assign(argR, bZ ? IRExpr_Const(IRConst_F64i(0)) : getDReg(dM));
         assign(irRes, binop(Iop_CmpF64, mkexpr(argL), mkexpr(argR)));

         IRTemp nzcv     = IRTemp_INVALID;
         IRTemp oldFPSCR = newTemp(Ity_I32);
         IRTemp newFPSCR = newTemp(Ity_I32);

         
         nzcv = mk_convert_IRCmpF64Result_to_NZCV(irRes);

         
         assign(oldFPSCR, IRExpr_Get(OFFB_FPSCR, Ity_I32));
         assign(newFPSCR, 
                binop(Iop_Or32, 
                      binop(Iop_And32, mkexpr(oldFPSCR), mkU32(0x0FFFFFFF)),
                      binop(Iop_Shl32, mkexpr(nzcv), mkU8(28))));

         putMiscReg32(OFFB_FPSCR, mkexpr(newFPSCR), condT);

         if (bZ) {
            DIP("fcmpz%sd%s d%u\n", bN ? "e" : "", nCC(conq), dD);
         } else {
            DIP("fcmp%sd%s d%u, d%u\n", bN ? "e" : "", nCC(conq), dD, dM);
         }
         goto decode_success_vfp;
      }
      
   }  

   
   if (BITS8(1,1,1,0,1,0,1,1) == (INSN(27,20) & BITS8(1,1,1,1,1,0,1,1))
       && BITS4(0,0,0,0) == (INSN(19,16) & BITS4(1,1,1,0))
       && BITS4(1,0,1,1) == INSN(11,8)
       && BITS4(0,1,0,0) == (INSN(7,4) & BITS4(0,1,0,1))) {
      UInt dD  = INSN(15,12) | (INSN(22,22) << 4);
      UInt dM  = INSN(3,0) | (INSN(5,5) << 4);
      UInt b16 = (insn28 >> 16) & 1;
      UInt b7  = (insn28 >> 7) & 1;
       if (b16 == 0 && b7 == 0) {
         
         putDReg(dD, getDReg(dM), condT);
         DIP("fcpyd%s d%u, d%u\n", nCC(conq), dD, dM);
         goto decode_success_vfp;
      }
      else if (b16 == 0 && b7 == 1) {
         
         putDReg(dD, unop(Iop_AbsF64, getDReg(dM)), condT);
         DIP("fabsd%s d%u, d%u\n", nCC(conq), dD, dM);
         goto decode_success_vfp;
      }
      else if (b16 == 1 && b7 == 0) {
         
         putDReg(dD, unop(Iop_NegF64, getDReg(dM)), condT);
         DIP("fnegd%s d%u, d%u\n", nCC(conq), dD, dM);
         goto decode_success_vfp;
      }
      else if (b16 == 1 && b7 == 1) {
         
         IRExpr* rm = get_FAKE_roundingmode(); 
         putDReg(dD, binop(Iop_SqrtF64, rm, getDReg(dM)), condT);
         DIP("fsqrtd%s d%u, d%u\n", nCC(conq), dD, dM);
         goto decode_success_vfp;
      }
      else
         vassert(0);

      
   }

   

   
   if (BITS8(1,1,1,0,1,0,1,1) == (INSN(27,20) & BITS8(1,1,1,1,1,0,1,1))
       && BITS4(1,0,0,0) == (INSN(19,16) & BITS4(1,1,1,1))
       && BITS4(1,0,1,1) == INSN(11,8)
       && BITS4(0,1,0,0) == (INSN(7,4) & BITS4(0,1,0,1))) {
      UInt bM    = (insn28 >> 5) & 1;
      UInt fM    = (INSN(3,0) << 1) | bM;
      UInt dD    = INSN(15,12) | (INSN(22,22) << 4);
      UInt syned = (insn28 >> 7) & 1;
      if (syned) {
         
         putDReg(dD, unop(Iop_I32StoF64,
                          unop(Iop_ReinterpF32asI32, getFReg(fM))),
                 condT);
         DIP("fsitod%s d%u, s%u\n", nCC(conq), dD, fM);
      } else {
         
         putDReg(dD, unop(Iop_I32UtoF64,
                          unop(Iop_ReinterpF32asI32, getFReg(fM))),
                 condT);
         DIP("fuitod%s d%u, s%u\n", nCC(conq), dD, fM);
      }
      goto decode_success_vfp;
   }

   
   if (BITS8(1,1,1,0,1,0,1,1) == (INSN(27,20) & BITS8(1,1,1,1,1,0,1,1))
       && BITS4(1,1,0,0) == (INSN(19,16) & BITS4(1,1,1,0))
       && BITS4(1,0,1,1) == INSN(11,8)
       && BITS4(0,1,0,0) == (INSN(7,4) & BITS4(0,1,0,1))) {
      UInt   bD    = (insn28 >> 22) & 1;
      UInt   fD    = (INSN(15,12) << 1) | bD;
      UInt   dM    = INSN(3,0) | (INSN(5,5) << 4);
      UInt   bZ    = (insn28 >> 7) & 1;
      UInt   syned = (insn28 >> 16) & 1;
      IRTemp rmode = newTemp(Ity_I32);
      assign(rmode, bZ ? mkU32(Irrm_ZERO)
                       : mkexpr(mk_get_IR_rounding_mode()));
      if (syned) {
         
         putFReg(fD, unop(Iop_ReinterpI32asF32,
                          binop(Iop_F64toI32S, mkexpr(rmode),
                                getDReg(dM))),
                 condT);
         DIP("ftosi%sd%s s%u, d%u\n", bZ ? "z" : "",
             nCC(conq), fD, dM);
      } else {
         
         putFReg(fD, unop(Iop_ReinterpI32asF32,
                          binop(Iop_F64toI32U, mkexpr(rmode),
                                getDReg(dM))),
                 condT);
         DIP("ftoui%sd%s s%u, d%u\n", bZ ? "z" : "",
             nCC(conq), fD, dM);
      }
      goto decode_success_vfp;
   }

   
   
   

   
   if (BITS8(1,1,0,0,0,0,0,0) == (INSN(27,20) & BITS8(1,1,1,0,0,0,0,0))
       && INSN(11,8) == BITS4(1,0,1,0)) {
      UInt bP      = (insn28 >> 24) & 1;
      UInt bU      = (insn28 >> 23) & 1;
      UInt bW      = (insn28 >> 21) & 1;
      UInt bL      = (insn28 >> 20) & 1;
      UInt bD      = (insn28 >> 22) & 1;
      UInt offset  = (insn28 >> 0) & 0xFF;
      UInt rN      = INSN(19,16);
      UInt fD      = (INSN(15,12) << 1) | bD;
      UInt nRegs   = offset;
      UInt summary = 0;
      Int  i;

       if (bP == 0 && bU == 1 && bW == 0) {
         summary = 1;
      }
      else if (bP == 0 && bU == 1 && bW == 1) {
         summary = 2;
      }
      else if (bP == 1 && bU == 0 && bW == 1) {
         summary = 3;
      }
      else goto after_vfp_fldms_fstms;

      
      if (rN == 15 && (summary == 2 || summary == 3 || isT))
         goto after_vfp_fldms_fstms;

      
      if (offset < 1)
         goto after_vfp_fldms_fstms;

      
      if (fD + nRegs - 1 >= 32)
         goto after_vfp_fldms_fstms;

      if (condT != IRTemp_INVALID) {
         if (isT)
            mk_skip_over_T32_if_cond_is_false( condT );
         else
            mk_skip_over_A32_if_cond_is_false( condT );
         condT = IRTemp_INVALID;
      }
      

      
      IRTemp rnT = newTemp(Ity_I32);
      assign(rnT, align4if(isT ? getIRegT(rN) : getIRegA(rN),
                           rN == 15));

      
      IRTemp rnTnew = IRTemp_INVALID;
      if (summary == 2 || summary == 3) {
         rnTnew = newTemp(Ity_I32);
         assign(rnTnew, binop(summary == 2 ? Iop_Add32 : Iop_Sub32,
                              mkexpr(rnT),
                              mkU32(4 * nRegs)));
      }

      
      IRTemp taT = newTemp(Ity_I32);
      assign(taT, summary == 3 ? mkexpr(rnTnew) : mkexpr(rnT));

      if (summary == 3) {
         if (isT)
            putIRegT(rN, mkexpr(rnTnew), IRTemp_INVALID);
         else
            putIRegA(rN, mkexpr(rnTnew), IRTemp_INVALID, Ijk_Boring);
      }

      
      for (i = 0; i < nRegs; i++) {
         IRExpr* addr = binop(Iop_Add32, mkexpr(taT), mkU32(4*i));
         if (bL) {
            putFReg(fD + i, loadLE(Ity_F32, addr), IRTemp_INVALID);
         } else {
            storeLE(addr, getFReg(fD + i));
         }
      }

      if (summary == 2) {
         if (isT)
            putIRegT(rN, mkexpr(rnTnew), IRTemp_INVALID);
         else
            putIRegA(rN, mkexpr(rnTnew), IRTemp_INVALID, Ijk_Boring);
      }

      const HChar* nm = bL==1 ? "ld" : "st";
      switch (summary) {
         case 1:  DIP("f%sms%s r%u, {s%u-s%u}\n", 
                      nm, nCC(conq), rN, fD, fD + nRegs - 1);
                  break;
         case 2:  DIP("f%smias%s r%u!, {s%u-s%u}\n", 
                      nm, nCC(conq), rN, fD, fD + nRegs - 1);
                  break;
         case 3:  DIP("f%smdbs%s r%u!, {s%u-s%u}\n", 
                      nm, nCC(conq), rN, fD, fD + nRegs - 1);
                  break;
         default: vassert(0);
      }

      goto decode_success_vfp;
      
   }

  after_vfp_fldms_fstms:

   
   if (BITS8(1,1,1,0,0,0,0,0) == (INSN(27,20) & BITS8(1,1,1,1,1,1,1,0))
       && BITS4(1,0,1,0) == INSN(11,8)
       && BITS4(0,0,0,0) == INSN(3,0)
       && BITS4(0,0,0,1) == (INSN(7,4) & BITS4(0,1,1,1))) {
      UInt rD  = INSN(15,12);
      UInt b7  = (insn28 >> 7) & 1;
      UInt fN  = (INSN(19,16) << 1) | b7;
      UInt b20 = (insn28 >> 20) & 1;
      if (rD == 15) {
         
      } else {
         if (b20) {
            IRExpr* res = unop(Iop_ReinterpF32asI32, getFReg(fN));
            if (isT)
               putIRegT(rD, res, condT);
            else
               putIRegA(rD, res, condT, Ijk_Boring);
            DIP("fmrs%s r%u, s%u\n", nCC(conq), rD, fN);
         } else {
            putFReg(fN, unop(Iop_ReinterpI32asF32,
                             isT ? getIRegT(rD) : getIRegA(rD)),
                        condT);
            DIP("fmsr%s s%u, r%u\n", nCC(conq), fN, rD);
         }
         goto decode_success_vfp;
      }
      
   }

   
   
   if (BITS8(1,1,0,1,0,0,0,0) == (INSN(27,20) & BITS8(1,1,1,1,0,0,1,0))
       && BITS4(1,0,1,0) == INSN(11,8)) {
      UInt bD     = (insn28 >> 22) & 1;
      UInt fD     = (INSN(15,12) << 1) | bD;
      UInt rN     = INSN(19,16);
      UInt offset = (insn28 & 0xFF) << 2;
      UInt bU     = (insn28 >> 23) & 1; 
      UInt bL     = (insn28 >> 20) & 1; 
      
      if (condT != IRTemp_INVALID) {
         if (isT)
            mk_skip_over_T32_if_cond_is_false( condT );
         else
            mk_skip_over_A32_if_cond_is_false( condT );
         condT = IRTemp_INVALID;
      }
      IRTemp ea = newTemp(Ity_I32);
      assign(ea, binop(bU ? Iop_Add32 : Iop_Sub32,
                       align4if(isT ? getIRegT(rN) : getIRegA(rN),
                                rN == 15),
                       mkU32(offset)));
      if (bL) {
         putFReg(fD, loadLE(Ity_F32,mkexpr(ea)), IRTemp_INVALID);
      } else {
         storeLE(mkexpr(ea), getFReg(fD));
      }
      DIP("f%ss%s s%u, [r%u, %c#%u]\n",
          bL ? "ld" : "st", nCC(conq), fD, rN,
          bU ? '+' : '-', offset);
      goto decode_success_vfp;
   }

   
   if (BITS8(1,1,1,0,0,0,0,0) == (INSN(27,20) & BITS8(1,1,1,1,0,0,0,0))
       && BITS4(1,0,1,0) == (INSN(11,8) & BITS4(1,1,1,0))
       && BITS4(0,0,0,0) == (INSN(7,4) & BITS4(0,0,0,1))) {
      UInt    bM  = (insn28 >> 5) & 1;
      UInt    bD  = (insn28 >> 22) & 1;
      UInt    bN  = (insn28 >> 7) & 1;
      UInt    fM  = (INSN(3,0) << 1) | bM;   
      UInt    fD  = (INSN(15,12) << 1) | bD; 
      UInt    fN  = (INSN(19,16) << 1) | bN; 
      UInt    bP  = (insn28 >> 23) & 1;
      UInt    bQ  = (insn28 >> 21) & 1;
      UInt    bR  = (insn28 >> 20) & 1;
      UInt    bS  = (insn28 >> 6) & 1;
      UInt    opc = (bP << 3) | (bQ << 2) | (bR << 1) | bS;
      IRExpr* rm  = get_FAKE_roundingmode(); 
      switch (opc) {
         case BITS4(0,0,0,0): 
            putFReg(fD, triop(Iop_AddF32, rm,
                              getFReg(fD),
                              triop(Iop_MulF32, rm, getFReg(fN), getFReg(fM))),
                        condT);
            DIP("fmacs%s s%u, s%u, s%u\n", nCC(conq), fD, fN, fM);
            goto decode_success_vfp;
         case BITS4(0,0,0,1): 
            putFReg(fD, triop(Iop_AddF32, rm,
                              getFReg(fD),
                              unop(Iop_NegF32,
                                   triop(Iop_MulF32, rm, getFReg(fN),
                                                         getFReg(fM)))),
                        condT);
            DIP("fnmacs%s s%u, s%u, s%u\n", nCC(conq), fD, fN, fM);
            goto decode_success_vfp;
         case BITS4(0,0,1,0): 
            putFReg(fD, triop(Iop_AddF32, rm,
                              unop(Iop_NegF32, getFReg(fD)),
                              triop(Iop_MulF32, rm, getFReg(fN), getFReg(fM))),
                        condT);
            DIP("fmscs%s s%u, s%u, s%u\n", nCC(conq), fD, fN, fM);
            goto decode_success_vfp;
         case BITS4(0,0,1,1): 
            putFReg(fD, triop(Iop_AddF32, rm,
                              unop(Iop_NegF32, getFReg(fD)),
                              unop(Iop_NegF32,
                                   triop(Iop_MulF32, rm,
                                                     getFReg(fN),
                                                    getFReg(fM)))),
                        condT);
            DIP("fnmscs%s s%u, s%u, s%u\n", nCC(conq), fD, fN, fM);
            goto decode_success_vfp;
         case BITS4(0,1,0,0): 
            putFReg(fD, triop(Iop_MulF32, rm, getFReg(fN), getFReg(fM)),
                        condT);
            DIP("fmuls%s s%u, s%u, s%u\n", nCC(conq), fD, fN, fM);
            goto decode_success_vfp;
         case BITS4(0,1,0,1): 
            putFReg(fD, unop(Iop_NegF32,
                             triop(Iop_MulF32, rm, getFReg(fN),
                                                   getFReg(fM))),
                    condT);
            DIP("fnmuls%s s%u, s%u, s%u\n", nCC(conq), fD, fN, fM);
            goto decode_success_vfp;
         case BITS4(0,1,1,0): 
            putFReg(fD, triop(Iop_AddF32, rm, getFReg(fN), getFReg(fM)),
                        condT);
            DIP("fadds%s s%u, s%u, s%u\n", nCC(conq), fD, fN, fM);
            goto decode_success_vfp;
         case BITS4(0,1,1,1): 
            putFReg(fD, triop(Iop_SubF32, rm, getFReg(fN), getFReg(fM)),
                        condT);
            DIP("fsubs%s s%u, s%u, s%u\n", nCC(conq), fD, fN, fM);
            goto decode_success_vfp;
         case BITS4(1,0,0,0): 
            putFReg(fD, triop(Iop_DivF32, rm, getFReg(fN), getFReg(fM)),
                        condT);
            DIP("fdivs%s s%u, s%u, s%u\n", nCC(conq), fD, fN, fM);
            goto decode_success_vfp;
         case BITS4(1,0,1,0): 
            putFReg(fD, triop(Iop_AddF32, rm,
                              unop(Iop_NegF32, getFReg(fD)),
                              triop(Iop_MulF32, rm,
                                                getFReg(fN),
                                                getFReg(fM))),
                        condT);
            DIP("vfnmss%s s%u, s%u, s%u\n", nCC(conq), fD, fN, fM);
            goto decode_success_vfp;
         case BITS4(1,0,1,1): 
            putFReg(fD, triop(Iop_AddF32, rm,
                              unop(Iop_NegF32, getFReg(fD)),
                              triop(Iop_MulF32, rm,
                                                unop(Iop_NegF32, getFReg(fN)),
                                                getFReg(fM))),
                        condT);
            DIP("vfnmas%s s%u, s%u, s%u\n", nCC(conq), fD, fN, fM);
            goto decode_success_vfp;
         case BITS4(1,1,0,0): 
            putFReg(fD, triop(Iop_AddF32, rm,
                              getFReg(fD),
                              triop(Iop_MulF32, rm, getFReg(fN),
                                                    getFReg(fM))),
                        condT);
            DIP("vfmas%s s%u, s%u, s%u\n", nCC(conq), fD, fN, fM);
            goto decode_success_vfp;
         case BITS4(1,1,0,1): 
            putFReg(fD, triop(Iop_AddF32, rm,
                              getFReg(fD),
                              triop(Iop_MulF32, rm,
                                    unop(Iop_NegF32, getFReg(fN)),
                                    getFReg(fM))),
                        condT);
            DIP("vfmss%s s%u, s%u, s%u\n", nCC(conq), fD, fN, fM);
            goto decode_success_vfp;
         default:
            break;
      }
   }

   
   if (BITS8(1,1,1,0,1,0,1,1) == (INSN(27,20) & BITS8(1,1,1,1,1,0,1,1))
       && BITS4(0,1,0,0) == (INSN(19,16) & BITS4(1,1,1,0))
       && BITS4(1,0,1,0) == INSN(11,8)
       && BITS4(0,1,0,0) == (INSN(7,4) & BITS4(0,1,0,1))) {
      UInt bZ = (insn28 >> 16) & 1;
      UInt bN = (insn28 >> 7) & 1;
      UInt bD = (insn28 >> 22) & 1;
      UInt bM = (insn28 >> 5) & 1;
      UInt fD = (INSN(15,12) << 1) | bD;
      UInt fM = (INSN(3,0) << 1) | bM;
      if (bZ && (INSN(3,0) != 0 || (INSN(7,4) & 3) != 0)) {
         
      } else {
         IRTemp argL = newTemp(Ity_F64);
         IRTemp argR = newTemp(Ity_F64);
         IRTemp irRes = newTemp(Ity_I32);

         assign(argL, unop(Iop_F32toF64, getFReg(fD)));
         assign(argR, bZ ? IRExpr_Const(IRConst_F64i(0))
                         : unop(Iop_F32toF64, getFReg(fM)));
         assign(irRes, binop(Iop_CmpF64, mkexpr(argL), mkexpr(argR)));

         IRTemp nzcv     = IRTemp_INVALID;
         IRTemp oldFPSCR = newTemp(Ity_I32);
         IRTemp newFPSCR = newTemp(Ity_I32);

         
         nzcv = mk_convert_IRCmpF64Result_to_NZCV(irRes);

         
         assign(oldFPSCR, IRExpr_Get(OFFB_FPSCR, Ity_I32));
         assign(newFPSCR, 
                binop(Iop_Or32, 
                      binop(Iop_And32, mkexpr(oldFPSCR), mkU32(0x0FFFFFFF)),
                      binop(Iop_Shl32, mkexpr(nzcv), mkU8(28))));

         putMiscReg32(OFFB_FPSCR, mkexpr(newFPSCR), condT);

         if (bZ) {
            DIP("fcmpz%ss%s s%u\n", bN ? "e" : "", nCC(conq), fD);
         } else {
            DIP("fcmp%ss%s s%u, s%u\n", bN ? "e" : "",
                nCC(conq), fD, fM);
         }
         goto decode_success_vfp;
      }
      
   }  

   
   if (BITS8(1,1,1,0,1,0,1,1) == (INSN(27,20) & BITS8(1,1,1,1,1,0,1,1))
       && BITS4(0,0,0,0) == (INSN(19,16) & BITS4(1,1,1,0))
       && BITS4(1,0,1,0) == INSN(11,8)
       && BITS4(0,1,0,0) == (INSN(7,4) & BITS4(0,1,0,1))) {
      UInt bD = (insn28 >> 22) & 1;
      UInt bM = (insn28 >> 5) & 1;
      UInt fD  = (INSN(15,12) << 1) | bD;
      UInt fM  = (INSN(3,0) << 1) | bM;
      UInt b16 = (insn28 >> 16) & 1;
      UInt b7  = (insn28 >> 7) & 1;
       if (b16 == 0 && b7 == 0) {
         
         putFReg(fD, getFReg(fM), condT);
         DIP("fcpys%s s%u, s%u\n", nCC(conq), fD, fM);
         goto decode_success_vfp;
      }
      else if (b16 == 0 && b7 == 1) {
         
         putFReg(fD, unop(Iop_AbsF32, getFReg(fM)), condT);
         DIP("fabss%s s%u, s%u\n", nCC(conq), fD, fM);
         goto decode_success_vfp;
      }
      else if (b16 == 1 && b7 == 0) {
         
         putFReg(fD, unop(Iop_NegF32, getFReg(fM)), condT);
         DIP("fnegs%s s%u, s%u\n", nCC(conq), fD, fM);
         goto decode_success_vfp;
      }
      else if (b16 == 1 && b7 == 1) {
         
         IRExpr* rm = get_FAKE_roundingmode(); 
         putFReg(fD, binop(Iop_SqrtF32, rm, getFReg(fM)), condT);
         DIP("fsqrts%s s%u, s%u\n", nCC(conq), fD, fM);
         goto decode_success_vfp;
      }
      else
         vassert(0);

      
   }

   

   
   if (BITS8(1,1,1,0,1,0,1,1) == (INSN(27,20) & BITS8(1,1,1,1,1,0,1,1))
       && BITS4(1,0,0,0) == INSN(19,16)
       && BITS4(1,0,1,0) == (INSN(11,8) & BITS4(1,1,1,0))
       && BITS4(0,1,0,0) == (INSN(7,4) & BITS4(0,1,0,1))) {
      UInt bM    = (insn28 >> 5) & 1;
      UInt bD    = (insn28 >> 22) & 1;
      UInt fM    = (INSN(3,0) << 1) | bM;
      UInt fD    = (INSN(15,12) << 1) | bD;
      UInt syned = (insn28 >> 7) & 1;
      IRTemp rmode = newTemp(Ity_I32);
      assign(rmode, mkexpr(mk_get_IR_rounding_mode()));
      if (syned) {
         
         putFReg(fD, binop(Iop_F64toF32,
                           mkexpr(rmode),
                           unop(Iop_I32StoF64,
                                unop(Iop_ReinterpF32asI32, getFReg(fM)))),
                 condT);
         DIP("fsitos%s s%u, s%u\n", nCC(conq), fD, fM);
      } else {
         
         putFReg(fD, binop(Iop_F64toF32,
                           mkexpr(rmode),
                           unop(Iop_I32UtoF64,
                                unop(Iop_ReinterpF32asI32, getFReg(fM)))),
                 condT);
         DIP("fuitos%s s%u, s%u\n", nCC(conq), fD, fM);
      }
      goto decode_success_vfp;
   }

   
   if (BITS8(1,1,1,0,1,0,1,1) == (INSN(27,20) & BITS8(1,1,1,1,1,0,1,1))
       && BITS4(1,1,0,0) == (INSN(19,16) & BITS4(1,1,1,0))
       && BITS4(1,0,1,0) == INSN(11,8)
       && BITS4(0,1,0,0) == (INSN(7,4) & BITS4(0,1,0,1))) {
      UInt   bM    = (insn28 >> 5) & 1;
      UInt   bD    = (insn28 >> 22) & 1;
      UInt   fD    = (INSN(15,12) << 1) | bD;
      UInt   fM    = (INSN(3,0) << 1) | bM;
      UInt   bZ    = (insn28 >> 7) & 1;
      UInt   syned = (insn28 >> 16) & 1;
      IRTemp rmode = newTemp(Ity_I32);
      assign(rmode, bZ ? mkU32(Irrm_ZERO)
                       : mkexpr(mk_get_IR_rounding_mode()));
      if (syned) {
         
         putFReg(fD, unop(Iop_ReinterpI32asF32,
                          binop(Iop_F64toI32S, mkexpr(rmode),
                                unop(Iop_F32toF64, getFReg(fM)))),
                 condT);
         DIP("ftosi%ss%s s%u, d%u\n", bZ ? "z" : "",
             nCC(conq), fD, fM);
         goto decode_success_vfp;
      } else {
         
         putFReg(fD, unop(Iop_ReinterpI32asF32,
                          binop(Iop_F64toI32U, mkexpr(rmode),
                                unop(Iop_F32toF64, getFReg(fM)))),
                 condT);
         DIP("ftoui%ss%s s%u, d%u\n", bZ ? "z" : "",
             nCC(conq), fD, fM);
         goto decode_success_vfp;
      }
   }

   

   
   if (BITS8(1,1,1,0,1,0,1,1) == (INSN(27,20) & BITS8(1,1,1,1,1,0,1,1))
       && BITS4(0,1,1,1) == INSN(19,16)
       && BITS4(1,0,1,0) == INSN(11,8)
       && BITS4(1,1,0,0) == (INSN(7,4) & BITS4(1,1,0,1))) {
      UInt dD = INSN(15,12) | (INSN(22,22) << 4);
      UInt bM = (insn28 >> 5) & 1;
      UInt fM = (INSN(3,0) << 1) | bM;
      putDReg(dD, unop(Iop_F32toF64, getFReg(fM)), condT);
      DIP("fcvtds%s d%u, s%u\n", nCC(conq), dD, fM);
      goto decode_success_vfp;
   }

   
   if (BITS8(1,1,1,0,1,0,1,1) == (INSN(27,20) & BITS8(1,1,1,1,1,0,1,1))
       && BITS4(0,1,1,1) == INSN(19,16)
       && BITS4(1,0,1,1) == INSN(11,8)
       && BITS4(1,1,0,0) == (INSN(7,4) & BITS4(1,1,0,1))) {
      UInt   bD    = (insn28 >> 22) & 1;
      UInt   fD    = (INSN(15,12) << 1) | bD;
      UInt   dM    = INSN(3,0) | (INSN(5,5) << 4);
      IRTemp rmode = newTemp(Ity_I32);
      assign(rmode, mkexpr(mk_get_IR_rounding_mode()));
      putFReg(fD, binop(Iop_F64toF32, mkexpr(rmode), getDReg(dM)),
                  condT);
      DIP("fcvtsd%s s%u, d%u\n", nCC(conq), fD, dM);
      goto decode_success_vfp;
   }

   
   if (BITS8(1,1,1,0,1,0,1,1) == (INSN(27,20) & BITS8(1,1,1,1,1,0,1,1))
       && BITS4(1,0,1,0) == (INSN(19,16) & BITS4(1,0,1,0))
       && BITS3(1,0,1) == INSN(11,9)
       && BITS3(1,0,0) == (INSN(6,4) & BITS3(1,0,1))) {
      UInt bD        = INSN(22,22);
      UInt bOP       = INSN(18,18);
      UInt bU        = INSN(16,16);
      UInt Vd        = INSN(15,12);
      UInt bSF       = INSN(8,8);
      UInt bSX       = INSN(7,7);
      UInt bI        = INSN(5,5);
      UInt imm4      = INSN(3,0);
      Bool to_fixed  = bOP == 1;
      Bool dp_op     = bSF == 1;
      Bool unsyned   = bU == 1;
      UInt size      = bSX == 0 ? 16 : 32;
      Int  frac_bits = size - ((imm4 << 1) | bI);
      UInt d         = dp_op  ? ((bD << 4) | Vd)  : ((Vd << 1) | bD);
      if (frac_bits >= 1 && frac_bits <= 32 && !to_fixed && !dp_op
                                            && size == 32) {
         
         IRTemp rmode = newTemp(Ity_I32);
         assign(rmode, mkU32(Irrm_NEAREST)); 
         IRTemp src32 = newTemp(Ity_I32);
         assign(src32,  unop(Iop_ReinterpF32asI32, getFReg(d)));
         IRExpr* as_F64 = unop( unsyned ? Iop_I32UtoF64 : Iop_I32StoF64,
                                mkexpr(src32 ) );
         IRTemp scale = newTemp(Ity_F64);
         assign(scale, unop(Iop_I32UtoF64, mkU32( 1 << (frac_bits-1) )));
         IRExpr* rm     = mkU32(Irrm_NEAREST);
         IRExpr* resF64 = triop(Iop_DivF64,
                                rm, as_F64, 
                                triop(Iop_AddF64, rm, mkexpr(scale),
                                                      mkexpr(scale)));
         IRExpr* resF32 = binop(Iop_F64toF32, mkexpr(rmode), resF64);
         putFReg(d, resF32, condT);
         DIP("vcvt.f32.%c32, s%u, s%u, #%d\n",
             unsyned ? 'u' : 's', d, d, frac_bits);
         goto decode_success_vfp;
      }
      if (frac_bits >= 1 && frac_bits <= 32 && !to_fixed && dp_op
                                            && size == 32) {
         
         IRTemp src32 = newTemp(Ity_I32);
         assign(src32, unop(Iop_64to32, getDRegI64(d)));
         IRExpr* as_F64 = unop( unsyned ? Iop_I32UtoF64 : Iop_I32StoF64,
                                mkexpr(src32 ) );
         IRTemp scale = newTemp(Ity_F64);
         assign(scale, unop(Iop_I32UtoF64, mkU32( 1 << (frac_bits-1) )));
         IRExpr* rm     = mkU32(Irrm_NEAREST);
         IRExpr* resF64 = triop(Iop_DivF64,
                                rm, as_F64, 
                                triop(Iop_AddF64, rm, mkexpr(scale),
                                                      mkexpr(scale)));
         putDReg(d, resF64, condT);
         DIP("vcvt.f64.%c32, d%u, d%u, #%d\n",
             unsyned ? 'u' : 's', d, d, frac_bits);
         goto decode_success_vfp;
      }
      if (frac_bits >= 1 && frac_bits <= 32 && to_fixed && dp_op
                                            && size == 32) {
         
         IRTemp srcF64 = newTemp(Ity_F64);
         assign(srcF64, getDReg(d));
         IRTemp scale = newTemp(Ity_F64);
         assign(scale, unop(Iop_I32UtoF64, mkU32( 1 << (frac_bits-1) )));
         IRTemp scaledF64 = newTemp(Ity_F64);
         IRExpr* rm = mkU32(Irrm_NEAREST);
         assign(scaledF64, triop(Iop_MulF64,
                                 rm, mkexpr(srcF64),
                                 triop(Iop_AddF64, rm, mkexpr(scale),
                                                       mkexpr(scale))));
         IRTemp rmode = newTemp(Ity_I32);
         assign(rmode, mkU32(Irrm_ZERO)); 
         IRTemp asI32 = newTemp(Ity_I32);
         assign(asI32, binop(unsyned ? Iop_F64toI32U : Iop_F64toI32S,
                             mkexpr(rmode), mkexpr(scaledF64)));
         putDRegI64(d, unop(unsyned ? Iop_32Uto64 : Iop_32Sto64,
                            mkexpr(asI32)), condT);
         goto decode_success_vfp;
      }
      
   }

   
   return False;

  decode_success_vfp:
   vassert(INSN(11,9) == BITS3(1,0,1)); 
   return True;  

#  undef INSN
}



static Bool decode_NV_instruction ( DisResult* dres,
                                    const VexArchInfo* archinfo,
                                    UInt insn )
{
#  define INSN(_bMax,_bMin)  SLICE_UInt(insn, (_bMax), (_bMin))
#  define INSN_COND          SLICE_UInt(insn, 31, 28)

   HChar dis_buf[128];

   
   vassert(BITS4(1,1,1,1) == INSN_COND);

   
   if (BITS8(0,1,0,1, 0,0, 0,1) == (INSN(27,20) & BITS8(1,1,1,1, 0,0, 1,1))
       && BITS4(1,1,1,1) == INSN(15,12)) {
      UInt rN    = INSN(19,16);
      UInt imm12 = INSN(11,0);
      UInt bU    = INSN(23,23);
      UInt bR    = INSN(22,22);
      DIP("pld%c [r%u, #%c%u]\n", bR ? ' ' : 'w', rN, bU ? '+' : '-', imm12);
      return True;
   }

   if (BITS8(0,1,1,1, 0,0, 0,1) == (INSN(27,20) & BITS8(1,1,1,1, 0,0, 1,1))
       && BITS4(1,1,1,1) == INSN(15,12)
       && 0 == INSN(4,4)) {
      UInt rN   = INSN(19,16);
      UInt rM   = INSN(3,0);
      UInt imm5 = INSN(11,7);
      UInt sh2  = INSN(6,5);
      UInt bU   = INSN(23,23);
      UInt bR   = INSN(22,22);
      if (rM != 15 && (rN != 15 || bR)) {
         IRExpr* eaE = mk_EA_reg_plusminus_shifted_reg(rN, bU, rM,
                                                       sh2, imm5, dis_buf);
         IRTemp eaT = newTemp(Ity_I32);
         vassert(eaE);
         assign(eaT, eaE);
         DIP("pld%c %s\n", bR ? ' ' : 'w', dis_buf);
         return True;
      }
      
   }

   
   if (BITS8(0,1,0,0, 0, 1,0,1) == (INSN(27,20) & BITS8(1,1,1,1,0,1,1,1))
       && BITS4(1,1,1,1) == INSN(15,12)) {
      UInt rN    = INSN(19,16);
      UInt imm12 = INSN(11,0);
      UInt bU    = INSN(23,23);
      DIP("pli [r%u, #%c%u]\n", rN, bU ? '+' : '-', imm12);
      return True;
   }

   

   
   
   if (INSN(31,25) == BITS7(1,1,1,1,1,0,1)) {
      UInt bitH   = INSN(24,24);
      Int  uimm24 = INSN(23,0);
      Int  simm24 = (((uimm24 << 8) >> 8) << 2) + (bitH << 1);
      UInt dst = guest_R15_curr_instr_notENC + 8 + (simm24 | 1);
      putIRegA( 14, mkU32(guest_R15_curr_instr_notENC + 4),
                    IRTemp_INVALID, Ijk_Boring );
      llPutIReg(15, mkU32(dst));
      dres->jk_StopHere = Ijk_Call;
      dres->whatNext    = Dis_StopHere;
      DIP("blx 0x%x (and switch to Thumb mode)\n", dst - 1);
      return True;
   }

   
   switch (insn) {
      case 0xF57FF06F: 
         stmt( IRStmt_MBE(Imbe_Fence) );
         DIP("ISB\n");
         return True;
      case 0xF57FF04F: 
      case 0xF57FF04E: 
      case 0xF57FF04B: 
      case 0xF57FF04A: 
      case 0xF57FF047: 
      case 0xF57FF046: 
      case 0xF57FF043: 
      case 0xF57FF042: 
         stmt( IRStmt_MBE(Imbe_Fence) );
         DIP("DSB\n");
         return True;
      case 0xF57FF05F: 
      case 0xF57FF05E: 
      case 0xF57FF05B: 
      case 0xF57FF05A: 
      case 0xF57FF057: 
      case 0xF57FF056: 
      case 0xF57FF053: 
      case 0xF57FF052: 
         stmt( IRStmt_MBE(Imbe_Fence) );
         DIP("DMB\n");
         return True;
      default:
         break;
   }

   
   if (insn == 0xF57FF01F) {
      stmt( IRStmt_MBE(Imbe_CancelReservation) );
      DIP("clrex\n");
      return True;
   }

   
   if (archinfo->hwcaps & VEX_HWCAPS_ARM_NEON) {
      Bool ok_neon = decode_NEON_instruction(
                        dres, insn, IRTemp_INVALID, 
                        False
                     );
      if (ok_neon)
         return True;
   }

   
   return False;

#  undef INSN_COND
#  undef INSN
}




static
DisResult disInstr_ARM_WRK (
             Bool         (*resteerOkFn) ( void*, Addr ),
             Bool         resteerCisOk,
             void*        callback_opaque,
             const UChar* guest_instr,
             const VexArchInfo* archinfo,
             const VexAbiInfo*  abiinfo,
             Bool         sigill_diag
          )
{
   
#  define INSN(_bMax,_bMin)  SLICE_UInt(insn, (_bMax), (_bMin))
#  define INSN_COND          SLICE_UInt(insn, 31, 28)

   DisResult dres;
   UInt      insn;
   
   
   IRTemp    condT; 
   UInt      summary;
   HChar     dis_buf[128];  

   
   
   

   
   dres.whatNext    = Dis_Continue;
   dres.len         = 4;
   dres.continueAt  = 0;
   dres.jk_StopHere = Ijk_INVALID;

   r15written = False;
   r15guard   = IRTemp_INVALID; 
   r15kind    = Ijk_Boring;

   insn = getUIntLittleEndianly( guest_instr );

   if (0) vex_printf("insn: 0x%x\n", insn);

   DIP("\t(arm) 0x%x:  ", (UInt)guest_R15_curr_instr_notENC);

   vassert(0 == (guest_R15_curr_instr_notENC & 3));

   

   
   {
      const UChar* code = guest_instr;
      UInt word1 = 0xE1A0C1EC;
      UInt word2 = 0xE1A0C6EC;
      UInt word3 = 0xE1A0CEEC;
      UInt word4 = 0xE1A0C9EC;
      if (getUIntLittleEndianly(code+ 0) == word1 &&
          getUIntLittleEndianly(code+ 4) == word2 &&
          getUIntLittleEndianly(code+ 8) == word3 &&
          getUIntLittleEndianly(code+12) == word4) {
         
         if (getUIntLittleEndianly(code+16) == 0xE18AA00A
                                               ) {
            
            DIP("r3 = client_request ( %%r4 )\n");
            llPutIReg(15, mkU32( guest_R15_curr_instr_notENC + 20 ));
            dres.jk_StopHere = Ijk_ClientReq;
            dres.whatNext    = Dis_StopHere;
            goto decode_success;
         }
         else
         if (getUIntLittleEndianly(code+16) == 0xE18BB00B
                                               ) {
            
            DIP("r3 = guest_NRADDR\n");
            dres.len = 20;
            llPutIReg(3, IRExpr_Get( OFFB_NRADDR, Ity_I32 ));
            goto decode_success;
         }
         else
         if (getUIntLittleEndianly(code+16) == 0xE18CC00C
                                               ) {
            
            DIP("branch-and-link-to-noredir r4\n");
            llPutIReg(14, mkU32( guest_R15_curr_instr_notENC + 20) );
            llPutIReg(15, llGetIReg(4));
            dres.jk_StopHere = Ijk_NoRedir;
            dres.whatNext    = Dis_StopHere;
            goto decode_success;
         }
         else
         if (getUIntLittleEndianly(code+16) == 0xE1899009
                                               ) {
            
            DIP("IR injection\n");
            vex_inject_ir(irsb, Iend_LE);
            
            
            
            
            stmt(IRStmt_Put(OFFB_CMSTART, mkU32(guest_R15_curr_instr_notENC)));
            stmt(IRStmt_Put(OFFB_CMLEN,   mkU32(20)));
            llPutIReg(15, mkU32( guest_R15_curr_instr_notENC + 20 ));
            dres.whatNext    = Dis_StopHere;
            dres.jk_StopHere = Ijk_InvalICache;
            goto decode_success;
         }
         insn = getUIntLittleEndianly(code+16);
         goto decode_failure;
         
      }

   }

   

   

   condT = IRTemp_INVALID;
   switch ( (ARMCondcode)INSN_COND ) {
      case ARMCondNV: {
         
         
         Bool ok = decode_NV_instruction(&dres, archinfo, insn);
         if (ok)
            goto decode_success;
         else
            goto decode_failure;
      }
      case ARMCondAL: 
         break;
      case ARMCondEQ: case ARMCondNE: case ARMCondHS: case ARMCondLO:
      case ARMCondMI: case ARMCondPL: case ARMCondVS: case ARMCondVC:
      case ARMCondHI: case ARMCondLS: case ARMCondGE: case ARMCondLT:
      case ARMCondGT: case ARMCondLE:
         condT = newTemp(Ity_I32);
         assign( condT, mk_armg_calculate_condition( INSN_COND ));
         break;
   }

   
   
   

   

   if (0 == (INSN(27,20) & BITS8(1,1,0,0,0,0,0,0))
       && !(INSN(25,25) == 0 && INSN(7,7) == 1 && INSN(4,4) == 1)) {
      IRTemp  shop = IRTemp_INVALID; 
      IRTemp  shco = IRTemp_INVALID; 
      UInt    rD   = (insn >> 12) & 0xF; 
      UInt    rN   = (insn >> 16) & 0xF; 
      UInt    bitS = (insn >> 20) & 1; 
      IRTemp  rNt  = IRTemp_INVALID;
      IRTemp  res  = IRTemp_INVALID;
      IRTemp  oldV = IRTemp_INVALID;
      IRTemp  oldC = IRTemp_INVALID;
      const HChar*  name = NULL;
      IROp    op   = Iop_INVALID;
      Bool    ok;

      switch (INSN(24,21)) {

         
         case BITS4(0,1,0,0): 
            name = "add"; op = Iop_Add32; goto rd_eq_rn_op_SO;
         case BITS4(0,0,1,0): 
            name = "sub"; op = Iop_Sub32; goto rd_eq_rn_op_SO;
         case BITS4(0,0,1,1): 
            name = "rsb"; op = Iop_Sub32; goto rd_eq_rn_op_SO;
         case BITS4(0,0,0,0): 
            name = "and"; op = Iop_And32; goto rd_eq_rn_op_SO;
         case BITS4(1,1,0,0): 
            name = "orr"; op = Iop_Or32; goto rd_eq_rn_op_SO;
         case BITS4(0,0,0,1): 
            name = "eor"; op = Iop_Xor32; goto rd_eq_rn_op_SO;
         case BITS4(1,1,1,0): 
            name = "bic"; op = Iop_And32; goto rd_eq_rn_op_SO;
         rd_eq_rn_op_SO: {
            Bool isRSB = False;
            Bool isBIC = False;
            switch (INSN(24,21)) {
               case BITS4(0,0,1,1):
                  vassert(op == Iop_Sub32); isRSB = True; break;
               case BITS4(1,1,1,0):
                  vassert(op == Iop_And32); isBIC = True; break;
               default:
                  break;
            }
            rNt = newTemp(Ity_I32);
            assign(rNt, getIRegA(rN));
            ok = mk_shifter_operand(
                    INSN(25,25), INSN(11,0), 
                    &shop, bitS ? &shco : NULL, dis_buf
                 );
            if (!ok)
               break;
            res = newTemp(Ity_I32);
            
            if (isRSB) {
               
               vassert(op == Iop_Sub32);
               assign(res, binop(op, mkexpr(shop), mkexpr(rNt)) );
            } else if (isBIC) {
               
               vassert(op == Iop_And32);
               assign(res, binop(op, mkexpr(rNt),
                                     unop(Iop_Not32, mkexpr(shop))) );
            } else {
               
               assign(res, binop(op, mkexpr(rNt), mkexpr(shop)) );
            }
            
            
            if (bitS
                && (op == Iop_And32 || op == Iop_Or32 || op == Iop_Xor32)) {
               oldV = newTemp(Ity_I32);
               assign( oldV, mk_armg_calculate_flag_v() );
            }
            
            
            putIRegA( rD, mkexpr(res), condT, Ijk_Boring );
            
            
            if (!bitS)
               vassert(shco == IRTemp_INVALID);
            
            if (bitS) {
               vassert(shco != IRTemp_INVALID);
               switch (op) {
                  case Iop_Add32:
                     setFlags_D1_D2( ARMG_CC_OP_ADD, rNt, shop, condT );
                     break;
                  case Iop_Sub32:
                     if (isRSB) {
                        setFlags_D1_D2( ARMG_CC_OP_SUB, shop, rNt, condT );
                     } else {
                        setFlags_D1_D2( ARMG_CC_OP_SUB, rNt, shop, condT );
                     }
                     break;
                  case Iop_And32: 
                  case Iop_Or32:
                  case Iop_Xor32:
                     
                     setFlags_D1_D2_ND( ARMG_CC_OP_LOGIC,
                                        res, shco, oldV, condT );
                     break;
                  default:
                     vassert(0);
               }
            }
            DIP("%s%s%s r%u, r%u, %s\n",
                name, nCC(INSN_COND), bitS ? "s" : "", rD, rN, dis_buf );
            goto decode_success;
         }

         
         case BITS4(1,1,0,1):   
         case BITS4(1,1,1,1): { 
            Bool isMVN = INSN(24,21) == BITS4(1,1,1,1);
            IRTemp jk = Ijk_Boring;
            if (rN != 0)
               break; 
            ok = mk_shifter_operand(
                    INSN(25,25), INSN(11,0), 
                    &shop, bitS ? &shco : NULL, dis_buf
                 );
            if (!ok)
               break;
            res = newTemp(Ity_I32);
            assign( res, isMVN ? unop(Iop_Not32, mkexpr(shop))
                               : mkexpr(shop) );
            if (bitS) {
               vassert(shco != IRTemp_INVALID);
               oldV = newTemp(Ity_I32);
               assign( oldV, mk_armg_calculate_flag_v() );
            } else {
               vassert(shco == IRTemp_INVALID);
            }
            if (!isMVN && INSN(11,0) == 14) {
              jk = Ijk_Ret;
            }
            
            putIRegA( rD, mkexpr(res), condT, jk );
            
            if (bitS) {
               setFlags_D1_D2_ND( ARMG_CC_OP_LOGIC, 
                                  res, shco, oldV, condT );
            }
            DIP("%s%s%s r%u, %s\n",
                isMVN ? "mvn" : "mov",
                nCC(INSN_COND), bitS ? "s" : "", rD, dis_buf );
            goto decode_success;
         }

         
         case BITS4(1,0,1,0):   
         case BITS4(1,0,1,1): { 
            Bool isCMN = INSN(24,21) == BITS4(1,0,1,1);
            if (rD != 0)
               break; 
            if (bitS == 0)
               break; 
            rNt = newTemp(Ity_I32);
            assign(rNt, getIRegA(rN));
            ok = mk_shifter_operand(
                    INSN(25,25), INSN(11,0), 
                    &shop, NULL, dis_buf
                 );
            if (!ok)
               break;
            
            
            setFlags_D1_D2( isCMN ? ARMG_CC_OP_ADD : ARMG_CC_OP_SUB,
                            rNt, shop, condT );
            DIP("%s%s r%u, %s\n",
                isCMN ? "cmn" : "cmp",
                nCC(INSN_COND), rN, dis_buf );
            goto decode_success;
         }

         
         case BITS4(1,0,0,0):   
         case BITS4(1,0,0,1): { 
            Bool isTEQ = INSN(24,21) == BITS4(1,0,0,1);
            if (rD != 0)
               break; 
            if (bitS == 0)
               break; 
            rNt = newTemp(Ity_I32);
            assign(rNt, getIRegA(rN));
            ok = mk_shifter_operand(
                    INSN(25,25), INSN(11,0), 
                    &shop, &shco, dis_buf
                 );
            if (!ok)
               break;
            
            res = newTemp(Ity_I32);
            assign( res, binop(isTEQ ? Iop_Xor32 : Iop_And32, 
                               mkexpr(rNt), mkexpr(shop)) );
            oldV = newTemp(Ity_I32);
            assign( oldV, mk_armg_calculate_flag_v() );
            
            setFlags_D1_D2_ND( ARMG_CC_OP_LOGIC,
                               res, shco, oldV, condT );
            DIP("%s%s r%u, %s\n",
                isTEQ ? "teq" : "tst",
                nCC(INSN_COND), rN, dis_buf );
            goto decode_success;
         }

         
         case BITS4(0,1,0,1): 
            name = "adc"; goto rd_eq_rn_op_SO_op_oldC;
         case BITS4(0,1,1,0): 
            name = "sbc"; goto rd_eq_rn_op_SO_op_oldC;
         case BITS4(0,1,1,1): 
            name = "rsc"; goto rd_eq_rn_op_SO_op_oldC;
         rd_eq_rn_op_SO_op_oldC: {
            
            rNt = newTemp(Ity_I32);
            assign(rNt, getIRegA(rN));
            ok = mk_shifter_operand(
                    INSN(25,25), INSN(11,0), 
                    &shop, bitS ? &shco : NULL, dis_buf
                 );
            if (!ok)
               break;
            oldC = newTemp(Ity_I32);
            assign( oldC, mk_armg_calculate_flag_c() );
            res = newTemp(Ity_I32);
            
            switch (INSN(24,21)) {
               case BITS4(0,1,0,1): 
                  assign(res,
                         binop(Iop_Add32,
                               binop(Iop_Add32, mkexpr(rNt), mkexpr(shop)),
                               mkexpr(oldC) ));
                  break;
               case BITS4(0,1,1,0): 
                  assign(res,
                         binop(Iop_Sub32,
                               binop(Iop_Sub32, mkexpr(rNt), mkexpr(shop)),
                               binop(Iop_Xor32, mkexpr(oldC), mkU32(1)) ));
                  break;
               case BITS4(0,1,1,1): 
                  assign(res,
                         binop(Iop_Sub32,
                               binop(Iop_Sub32, mkexpr(shop), mkexpr(rNt)),
                               binop(Iop_Xor32, mkexpr(oldC), mkU32(1)) ));
                  break;
               default:
                  vassert(0);
            }
            
            
            
            putIRegA( rD, mkexpr(res), condT, Ijk_Boring );
            
            
            if (!bitS)
               vassert(shco == IRTemp_INVALID);
            
            if (bitS) {
               vassert(shco != IRTemp_INVALID);
               switch (INSN(24,21)) {
                  case BITS4(0,1,0,1): 
                     setFlags_D1_D2_ND( ARMG_CC_OP_ADC,
                                        rNt, shop, oldC, condT );
                     break;
                  case BITS4(0,1,1,0): 
                     setFlags_D1_D2_ND( ARMG_CC_OP_SBB,
                                        rNt, shop, oldC, condT );
                     break;
                  case BITS4(0,1,1,1): 
                     setFlags_D1_D2_ND( ARMG_CC_OP_SBB,
                                        shop, rNt, oldC, condT );
                     break;
                  default:
                     vassert(0);
               }
            }
            DIP("%s%s%s r%u, r%u, %s\n",
                name, nCC(INSN_COND), bitS ? "s" : "", rD, rN, dis_buf );
            goto decode_success;
         }

         default:
            vassert(0);
      }
   } 

   
   
   
   if ((INSN(27,24) & BITS4(1,1,0,0)) != BITS4(0,1,0,0))
      goto after_load_store_ubyte_or_word;

   summary = 0;
   
    if (INSN(27,24) == BITS4(0,1,0,1) && INSN(21,21) == 0) {
      summary = 1 | 16;
   }
   else if (INSN(27,24) == BITS4(0,1,1,1) && INSN(21,21) == 0
                                          && INSN(4,4) == 0) {
      summary = 1 | 32;
   }
   else if (INSN(27,24) == BITS4(0,1,0,1) && INSN(21,21) == 1) {
      summary = 2 | 16;
   }
   else if (INSN(27,24) == BITS4(0,1,1,1) && INSN(21,21) == 1
                                          && INSN(4,4) == 0) {
      summary = 2 | 32;
   }
   else if (INSN(27,24) == BITS4(0,1,0,0) && INSN(21,21) == 0) {
      summary = 3 | 16;
   }
   else if (INSN(27,24) == BITS4(0,1,1,0) && INSN(21,21) == 0
                                          && INSN(4,4) == 0) {
      summary = 3 | 32;
   }
   else goto after_load_store_ubyte_or_word;

   { UInt rN = (insn >> 16) & 0xF; 
     UInt rD = (insn >> 12) & 0xF; 
     UInt rM = (insn >> 0)  & 0xF; 
     UInt bU = (insn >> 23) & 1;      
     UInt bB = (insn >> 22) & 1;      
     UInt bL = (insn >> 20) & 1;      
     UInt imm12 = (insn >> 0) & 0xFFF; 
     UInt imm5  = (insn >> 7) & 0x1F;  
     UInt sh2   = (insn >> 5) & 3;     

     switch (summary) {
        case 1 | 16:
           break;
        case 1 | 32: 
           if (rM == 15) goto after_load_store_ubyte_or_word;
           break;
        case 2 | 16: case 3 | 16:
           if (rN == 15) goto after_load_store_ubyte_or_word;
           if (bL == 1 && rN == rD) goto after_load_store_ubyte_or_word;
           break;
        case 2 | 32: case 3 | 32:
           if (rM == 15) goto after_load_store_ubyte_or_word;
           if (rN == 15) goto after_load_store_ubyte_or_word;
           if (rN == rM) goto after_load_store_ubyte_or_word;
           if (bL == 1 && rN == rD) goto after_load_store_ubyte_or_word;
           break;
        default:
           vassert(0);
     }

     IRExpr* eaE = NULL;
     switch (summary & 0xF0) {
        case 16:
           eaE = mk_EA_reg_plusminus_imm12( rN, bU, imm12, dis_buf );
           break;
        case 32:
           eaE = mk_EA_reg_plusminus_shifted_reg( rN, bU, rM, sh2, imm5,
                                                  dis_buf );
           break;
     }
     vassert(eaE);
     IRTemp eaT = newTemp(Ity_I32);
     assign(eaT, eaE);

     
     IRTemp rnT = newTemp(Ity_I32);
     assign(rnT, getIRegA(rN));

     
     IRTemp taT = IRTemp_INVALID;
     switch (summary & 0x0F) {
        case 1: case 2: taT = eaT; break;
        case 3:         taT = rnT; break;
     }
     vassert(taT != IRTemp_INVALID);

     if (bL == 0) {

        
        IRTemp rDt = newTemp(Ity_I32);
        assign(rDt, getIRegA(rD));

        
        switch (summary & 0x0F) {
           case 2: case 3:
              putIRegA( rN, mkexpr(eaT), condT, Ijk_Boring );
              break;
        }

        
        if (bB == 0) { 
           storeGuardedLE( mkexpr(taT), mkexpr(rDt), condT );
        } else { 
           vassert(bB == 1);
           storeGuardedLE( mkexpr(taT), unop(Iop_32to8, mkexpr(rDt)), condT );
        }

     } else {
        
        vassert(bL == 1);

        
        if (bB == 0) { 
           IRTemp jk = Ijk_Boring;
           if (rN == 13 && summary == (3 | 16) && bB == 0) {
              jk = Ijk_Ret;
           }
           IRTemp tD = newTemp(Ity_I32);
           loadGuardedLE( tD, ILGop_Ident32,
                          mkexpr(taT), llGetIReg(rD), condT );
           putIRegA( rD, mkexpr(tD),
                     rD == 15 ? condT : IRTemp_INVALID, jk );
        } else { 
           vassert(bB == 1);
           IRTemp tD = newTemp(Ity_I32);
           loadGuardedLE( tD, ILGop_8Uto32, mkexpr(taT), llGetIReg(rD), condT );
           putIRegA( rD, mkexpr(tD), IRTemp_INVALID, Ijk_Boring );
        }

        
        switch (summary & 0x0F) {
           case 2: case 3:
              
              if (bL == 1)
                 vassert(rD != rN); 
              putIRegA( rN, mkexpr(eaT), condT, Ijk_Boring );
              break;
        }
     }
 
     switch (summary & 0x0F) {
        case 1:  DIP("%sr%s%s r%u, %s\n",
                     bL == 0 ? "st" : "ld",
                     bB == 0 ? "" : "b", nCC(INSN_COND), rD, dis_buf);
                 break;
        case 2:  DIP("%sr%s%s r%u, %s! (at-EA-then-Rn=EA)\n",
                     bL == 0 ? "st" : "ld",
                     bB == 0 ? "" : "b", nCC(INSN_COND), rD, dis_buf);
                 break;
        case 3:  DIP("%sr%s%s r%u, %s! (at-Rn-then-Rn=EA)\n",
                     bL == 0 ? "st" : "ld",
                     bB == 0 ? "" : "b", nCC(INSN_COND), rD, dis_buf);
                 break;
        default: vassert(0);
     }

     

     goto decode_success;

     /* Complications:

        For all loads: if the Amode specifies base register
        writeback, and the same register is specified for Rd and Rn,
        the results are UNPREDICTABLE.

        For all loads and stores: if R15 is written, branch to
        that address afterwards.

        STRB: straightforward
        LDRB: loaded data is zero extended
        STR:  lowest 2 bits of address are ignored
        LDR:  if the lowest 2 bits of the address are nonzero
              then the loaded value is rotated right by 8 * the lowest 2 bits
     */
   }

  after_load_store_ubyte_or_word:

   
   
   
   if ((INSN(27,24) & BITS4(1,1,1,0)) != BITS4(0,0,0,0))
      goto after_load_store_sbyte_or_hword;

   
   if ((INSN(7,4) & BITS4(1,0,0,1)) != BITS4(1,0,0,1))
      goto after_load_store_sbyte_or_hword;

   summary = 0;

    if (INSN(27,24) == BITS4(0,0,0,1) && INSN(22,21) == BITS2(1,0)) {
      summary = 1 | 16;
   }
   else if (INSN(27,24) == BITS4(0,0,0,1) && INSN(22,21) == BITS2(0,0)) {
      summary = 1 | 32;
   }
   else if (INSN(27,24) == BITS4(0,0,0,1) && INSN(22,21) == BITS2(1,1)) {
      summary = 2 | 16;
   }
   else if (INSN(27,24) == BITS4(0,0,0,1) && INSN(22,21) == BITS2(0,1)) {
      summary = 2 | 32;
   }
   else if (INSN(27,24) == BITS4(0,0,0,0) && INSN(22,21) == BITS2(1,0)) {
      summary = 3 | 16;
   }
   else if (INSN(27,24) == BITS4(0,0,0,0) && INSN(22,21) == BITS2(0,0)) {
      summary = 3 | 32;
   }
   else goto after_load_store_sbyte_or_hword;

   { UInt rN   = (insn >> 16) & 0xF; 
     UInt rD   = (insn >> 12) & 0xF; 
     UInt rM   = (insn >> 0)  & 0xF; 
     UInt bU   = (insn >> 23) & 1;   
     UInt bL   = (insn >> 20) & 1;   
     UInt bH   = (insn >> 5) & 1;    
     UInt bS   = (insn >> 6) & 1;    
     UInt imm8 = ((insn >> 4) & 0xF0) | (insn & 0xF); 

     if (bS == 0 && bH == 0) 
        goto after_load_store_sbyte_or_hword;
     if (bS == 1 && bL == 0) 
        goto after_load_store_sbyte_or_hword;

     
     if ((summary & 32) != 0 && (imm8 & 0xF0) != 0)
        goto after_load_store_sbyte_or_hword;

     switch (summary) {
        case 1 | 16:
           break;
        case 1 | 32: 
           if (rM == 15) goto after_load_store_sbyte_or_hword;
           break;
        case 2 | 16: case 3 | 16:
           if (rN == 15) goto after_load_store_sbyte_or_hword;
           if (bL == 1 && rN == rD) goto after_load_store_sbyte_or_hword;
           break;
        case 2 | 32: case 3 | 32:
           if (rM == 15) goto after_load_store_sbyte_or_hword;
           if (rN == 15) goto after_load_store_sbyte_or_hword;
           if (rN == rM) goto after_load_store_sbyte_or_hword;
           if (bL == 1 && rN == rD) goto after_load_store_sbyte_or_hword;
           break;
        default:
           vassert(0);
     }

     if (bL == 1 && rD == 15 && condT != IRTemp_INVALID) {
        
        mk_skip_over_A32_if_cond_is_false( condT );
        condT = IRTemp_INVALID;
        
     }

     IRExpr* eaE = NULL;
     switch (summary & 0xF0) {
        case 16:
           eaE = mk_EA_reg_plusminus_imm8( rN, bU, imm8, dis_buf );
           break;
        case 32:
           eaE = mk_EA_reg_plusminus_reg( rN, bU, rM, dis_buf );
           break;
     }
     vassert(eaE);
     IRTemp eaT = newTemp(Ity_I32);
     assign(eaT, eaE);

     
     IRTemp rnT = newTemp(Ity_I32);
     assign(rnT, getIRegA(rN));

     
     IRTemp taT = IRTemp_INVALID;
     switch (summary & 0x0F) {
        case 1: case 2: taT = eaT; break;
        case 3:         taT = rnT; break;
     }
     vassert(taT != IRTemp_INVALID);

     
     IRTemp llOldRd = newTemp(Ity_I32);
     assign(llOldRd, llGetIReg(rD));

     const HChar* name = NULL;
     
      if (bH == 1 && bL == 0 && bS == 0) { 
        storeGuardedLE( mkexpr(taT),
                        unop(Iop_32to16, getIRegA(rD)), condT );
        name = "strh";
     }
     else if (bH == 1 && bL == 1 && bS == 0) { 
        IRTemp newRd = newTemp(Ity_I32);
        loadGuardedLE( newRd, ILGop_16Uto32, 
                       mkexpr(taT), mkexpr(llOldRd), condT );
        putIRegA( rD, mkexpr(newRd), IRTemp_INVALID, Ijk_Boring );
        name = "ldrh";
     }
     else if (bH == 1 && bL == 1 && bS == 1) { 
        IRTemp newRd = newTemp(Ity_I32);
        loadGuardedLE( newRd, ILGop_16Sto32, 
                       mkexpr(taT), mkexpr(llOldRd), condT );
        putIRegA( rD, mkexpr(newRd), IRTemp_INVALID, Ijk_Boring );
        name = "ldrsh";
     }
     else if (bH == 0 && bL == 1 && bS == 1) { 
        IRTemp newRd = newTemp(Ity_I32);
        loadGuardedLE( newRd, ILGop_8Sto32, 
                       mkexpr(taT), mkexpr(llOldRd), condT );
        putIRegA( rD, mkexpr(newRd), IRTemp_INVALID, Ijk_Boring );
        name = "ldrsb";
     }
     else
        vassert(0); 

     
     switch (summary & 0x0F) {
        case 2: case 3:
           
           if (bL == 1)
              vassert(rD != rN); 
           putIRegA( rN, mkexpr(eaT), condT, Ijk_Boring );
           break;
     }

     switch (summary & 0x0F) {
        case 1:  DIP("%s%s r%u, %s\n", name, nCC(INSN_COND), rD, dis_buf);
                 break;
        case 2:  DIP("%s%s r%u, %s! (at-EA-then-Rn=EA)\n",
                     name, nCC(INSN_COND), rD, dis_buf);
                 break;
        case 3:  DIP("%s%s r%u, %s! (at-Rn-then-Rn=EA)\n",
                     name, nCC(INSN_COND), rD, dis_buf);
                 break;
        default: vassert(0);
     }

     

     goto decode_success;

     /* Complications:

        For all loads: if the Amode specifies base register
        writeback, and the same register is specified for Rd and Rn,
        the results are UNPREDICTABLE.

        For all loads and stores: if R15 is written, branch to
        that address afterwards.

        Misaligned halfword stores => Unpredictable
        Misaligned halfword loads  => Unpredictable
     */
   }

  after_load_store_sbyte_or_hword:

   
   
   
   
   if (BITS8(1,0,0,0,0,0,0,0) == (INSN(27,20) & BITS8(1,1,1,0,0,1,0,0))) {
      
      
      
      
      

      UInt bINC    = (insn >> 23) & 1;
      UInt bBEFORE = (insn >> 24) & 1;

      UInt bL      = (insn >> 20) & 1;  
      UInt bW      = (insn >> 21) & 1;  
      UInt rN      = (insn >> 16) & 0xF;
      UInt regList = insn & 0xFFFF;
      if (rN == 15) goto after_load_store_multiple;
      
      if (regList == 0) goto after_load_store_multiple;
      
      
      
      
      if (bW == 1 && bL == 1 && ((1 << rN) & regList) > 0)
         goto after_load_store_multiple;

      if (condT != IRTemp_INVALID) {
         mk_skip_over_A32_if_cond_is_false( condT );
         condT = IRTemp_INVALID;
      }

      
      mk_ldm_stm( True, rN, bINC, bBEFORE, bW, bL, regList );

      DIP("%sm%c%c%s r%u%s, {0x%04x}\n",
          bL == 1 ? "ld" : "st", bINC ? 'i' : 'd', bBEFORE ? 'b' : 'a',
          nCC(INSN_COND),
          rN, bW ? "!" : "", regList);

      goto decode_success;
   }

  after_load_store_multiple:

   
   
   
   if (BITS8(1,0,1,0,0,0,0,0) == (INSN(27,20) & BITS8(1,1,1,0,0,0,0,0))) {
      UInt link   = (insn >> 24) & 1;
      UInt uimm24 = insn & ((1<<24)-1);
      Int  simm24 = (Int)uimm24;
      UInt dst    = guest_R15_curr_instr_notENC + 8
                    + (((simm24 << 8) >> 8) << 2);
      IRJumpKind jk = link ? Ijk_Call : Ijk_Boring;
      if (link) {
         putIRegA(14, mkU32(guest_R15_curr_instr_notENC + 4),
                      condT, Ijk_Boring);
      }
      if (condT == IRTemp_INVALID) {
         if (resteerOkFn( callback_opaque, dst )) {
            
            dres.whatNext   = Dis_ResteerU;
            dres.continueAt = dst;
         } else {
            
            llPutIReg(15, mkU32(dst));
            dres.jk_StopHere = jk;
            dres.whatNext    = Dis_StopHere;
         }
         DIP("b%s 0x%x\n", link ? "l" : "", dst);
      } else {
         
         const HChar* comment = "";

         if (!link
             && resteerCisOk
             && vex_control.guest_chase_cond
             && dst < guest_R15_curr_instr_notENC
             && resteerOkFn( callback_opaque, dst) ) {
            stmt( IRStmt_Exit( unop(Iop_Not1,
                                    unop(Iop_32to1, mkexpr(condT))),
                               Ijk_Boring,
                               IRConst_U32(guest_R15_curr_instr_notENC+4),
                               OFFB_R15T ));
            dres.whatNext   = Dis_ResteerC;
            dres.continueAt = (Addr32)dst;
            comment = "(assumed taken)";
         }
         else
         if (!link
             && resteerCisOk
             && vex_control.guest_chase_cond
             && dst >= guest_R15_curr_instr_notENC
             && resteerOkFn( callback_opaque, 
                             guest_R15_curr_instr_notENC+4) ) {
            stmt( IRStmt_Exit( unop(Iop_32to1, mkexpr(condT)),
                               Ijk_Boring,
                               IRConst_U32(dst),
                               OFFB_R15T ));
            dres.whatNext   = Dis_ResteerC;
            dres.continueAt = guest_R15_curr_instr_notENC+4;
            comment = "(assumed not taken)";
         }
         else {
            stmt( IRStmt_Exit( unop(Iop_32to1, mkexpr(condT)),
                               jk, IRConst_U32(dst), OFFB_R15T ));
            llPutIReg(15, mkU32(guest_R15_curr_instr_notENC + 4));
            dres.jk_StopHere = Ijk_Boring;
            dres.whatNext    = Dis_StopHere;
         }
         DIP("b%s%s 0x%x %s\n", link ? "l" : "", nCC(INSN_COND),
             dst, comment);
      }
      goto decode_success;
   }

   
   
   if (INSN(27,20) == BITS8(0,0,0,1,0,0,1,0)
       && INSN(19,12) == BITS8(1,1,1,1,1,1,1,1)
       && (INSN(11,4) == BITS8(1,1,1,1,0,0,1,1)
           || INSN(11,4) == BITS8(1,1,1,1,0,0,0,1))) {
      IRTemp  dst = newTemp(Ity_I32);
      UInt    link = (INSN(11,4) >> 1) & 1;
      UInt    rM   = INSN(3,0);
      
      
      if (!(link && rM == 15)) {
         if (condT != IRTemp_INVALID) {
            mk_skip_over_A32_if_cond_is_false( condT );
         }
         
         
         
         assign( dst, getIRegA(rM) );
         if (link) {
            putIRegA( 14, mkU32(guest_R15_curr_instr_notENC + 4),
                      IRTemp_INVALID, Ijk_Boring );
         }
         llPutIReg(15, mkexpr(dst));
         dres.jk_StopHere = link ? Ijk_Call
                                 : (rM == 14 ? Ijk_Ret : Ijk_Boring);
         dres.whatNext    = Dis_StopHere;
         if (condT == IRTemp_INVALID) {
            DIP("b%sx r%u\n", link ? "l" : "", rM);
         } else {
            DIP("b%sx%s r%u\n", link ? "l" : "", nCC(INSN_COND), rM);
         }
         goto decode_success;
      }
      
   }


   
   
   if (INSN(27,20) == BITS8(0,0,0,1,0,1,1,0)
       && INSN(19,16) == BITS4(1,1,1,1)
       && INSN(11,4) == BITS8(1,1,1,1,0,0,0,1)) {
      UInt rD = INSN(15,12);
      UInt rM = INSN(3,0);
      IRTemp arg = newTemp(Ity_I32);
      IRTemp res = newTemp(Ity_I32);
      assign(arg, getIRegA(rM));
      assign(res, IRExpr_ITE(
                     binop(Iop_CmpEQ32, mkexpr(arg), mkU32(0)),
                     mkU32(32),
                     unop(Iop_Clz32, mkexpr(arg))
            ));
      putIRegA(rD, mkexpr(res), condT, Ijk_Boring);
      DIP("clz%s r%u, r%u\n", nCC(INSN_COND), rD, rM);
      goto decode_success;
   }

   
   
   if (BITS8(0,0,0,0,0,0,0,0) == (INSN(27,20) & BITS8(1,1,1,1,1,1,1,0))
       && INSN(15,12) == BITS4(0,0,0,0)
       && INSN(7,4) == BITS4(1,0,0,1)) {
      UInt bitS = (insn >> 20) & 1; 
      UInt rD = INSN(19,16);
      UInt rS = INSN(11,8);
      UInt rM = INSN(3,0);
      if (rD == 15 || rM == 15 || rS == 15) {
         
      } else {
         IRTemp argL = newTemp(Ity_I32);
         IRTemp argR = newTemp(Ity_I32);
         IRTemp res  = newTemp(Ity_I32);
         IRTemp oldC = IRTemp_INVALID;
         IRTemp oldV = IRTemp_INVALID;
         assign( argL, getIRegA(rM));
         assign( argR, getIRegA(rS));
         assign( res, binop(Iop_Mul32, mkexpr(argL), mkexpr(argR)) );
         if (bitS) {
            oldC = newTemp(Ity_I32);
            assign(oldC, mk_armg_calculate_flag_c());
            oldV = newTemp(Ity_I32);
            assign(oldV, mk_armg_calculate_flag_v());
         }
         
         putIRegA( rD, mkexpr(res), condT, Ijk_Boring );
         if (bitS) {
            IRTemp pair = newTemp(Ity_I32);
            assign( pair, binop(Iop_Or32,
                                binop(Iop_Shl32, mkexpr(oldC), mkU8(1)),
                                mkexpr(oldV)) );
            setFlags_D1_ND( ARMG_CC_OP_MUL, res, pair, condT );
         }
         DIP("mul%c%s r%u, r%u, r%u\n",
             bitS ? 's' : ' ', nCC(INSN_COND), rD, rM, rS);
         goto decode_success;
      }
      
   }

   
   
   if (BITS8(0,1,1,1,0,0,0,1) == INSN(27,20)
       && INSN(15,12) == BITS4(1,1,1,1)
       && INSN(7,4) == BITS4(0,0,0,1)) {
      UInt rD = INSN(19,16);
      UInt rM = INSN(11,8);
      UInt rN = INSN(3,0);
      if (rD == 15 || rM == 15 || rN == 15) {
         
      } else {
         IRTemp res  = newTemp(Ity_I32);
         IRTemp argL = newTemp(Ity_I32);
         IRTemp argR = newTemp(Ity_I32);
         assign(argL, getIRegA(rN));
         assign(argR, getIRegA(rM));
         assign(res, binop(Iop_DivS32, mkexpr(argL), mkexpr(argR)));
         putIRegA(rD, mkexpr(res), condT, Ijk_Boring);
         DIP("sdiv r%u, r%u, r%u\n", rD, rN, rM);
         goto decode_success;
      }
    }

   
   if (BITS8(0,1,1,1,0,0,1,1) == INSN(27,20)
       && INSN(15,12) == BITS4(1,1,1,1)
       && INSN(7,4) == BITS4(0,0,0,1)) {
      UInt rD = INSN(19,16);
      UInt rM = INSN(11,8);
      UInt rN = INSN(3,0);
      if (rD == 15 || rM == 15 || rN == 15) {
         
      } else {
         IRTemp res  = newTemp(Ity_I32);
         IRTemp argL = newTemp(Ity_I32);
         IRTemp argR = newTemp(Ity_I32);
         assign(argL, getIRegA(rN));
         assign(argR, getIRegA(rM));
         assign(res, binop(Iop_DivU32, mkexpr(argL), mkexpr(argR)));
         putIRegA(rD, mkexpr(res), condT, Ijk_Boring);
         DIP("udiv r%u, r%u, r%u\n", rD, rN, rM);
         goto decode_success;
      }
   }

   
   if (BITS8(0,0,0,0,0,0,1,0) == (INSN(27,20) & BITS8(1,1,1,1,1,0,1,0))
       && INSN(7,4) == BITS4(1,0,0,1)) {
      UInt bitS  = (insn >> 20) & 1; 
      UInt isMLS = (insn >> 22) & 1; 
      UInt rD = INSN(19,16);
      UInt rN = INSN(15,12);
      UInt rS = INSN(11,8);
      UInt rM = INSN(3,0);
      if (bitS == 1 && isMLS == 1) {
      }
      else
      if (rD == 15 || rM == 15 || rS == 15 || rN == 15) {
         
      } else {
         IRTemp argL = newTemp(Ity_I32);
         IRTemp argR = newTemp(Ity_I32);
         IRTemp argP = newTemp(Ity_I32);
         IRTemp res  = newTemp(Ity_I32);
         IRTemp oldC = IRTemp_INVALID;
         IRTemp oldV = IRTemp_INVALID;
         assign( argL, getIRegA(rM));
         assign( argR, getIRegA(rS));
         assign( argP, getIRegA(rN));
         assign( res, binop(isMLS ? Iop_Sub32 : Iop_Add32,
                            mkexpr(argP),
                            binop(Iop_Mul32, mkexpr(argL), mkexpr(argR)) ));
         if (bitS) {
            vassert(!isMLS); 
            oldC = newTemp(Ity_I32);
            assign(oldC, mk_armg_calculate_flag_c());
            oldV = newTemp(Ity_I32);
            assign(oldV, mk_armg_calculate_flag_v());
         }
         
         putIRegA( rD, mkexpr(res), condT, Ijk_Boring );
         if (bitS) {
            IRTemp pair = newTemp(Ity_I32);
            assign( pair, binop(Iop_Or32,
                                binop(Iop_Shl32, mkexpr(oldC), mkU8(1)),
                                mkexpr(oldV)) );
            setFlags_D1_ND( ARMG_CC_OP_MUL, res, pair, condT );
         }
         DIP("ml%c%c%s r%u, r%u, r%u, r%u\n",
             isMLS ? 's' : 'a', bitS ? 's' : ' ',
             nCC(INSN_COND), rD, rM, rS, rN);
         goto decode_success;
      }
      
   }

   
   if (BITS8(0,0,0,0,1,0,0,0) == (INSN(27,20) & BITS8(1,1,1,1,1,0,1,0))
       && INSN(7,4) == BITS4(1,0,0,1)) {
      UInt bitS = (insn >> 20) & 1; 
      UInt rDhi = INSN(19,16);
      UInt rDlo = INSN(15,12);
      UInt rS   = INSN(11,8);
      UInt rM   = INSN(3,0);
      UInt isS  = (INSN(27,20) >> 2) & 1; 
      if (rDhi == 15 || rDlo == 15 || rM == 15 || rS == 15 || rDhi == rDlo)  {
         
      } else {
         IRTemp argL  = newTemp(Ity_I32);
         IRTemp argR  = newTemp(Ity_I32);
         IRTemp res   = newTemp(Ity_I64);
         IRTemp resHi = newTemp(Ity_I32);
         IRTemp resLo = newTemp(Ity_I32);
         IRTemp oldC  = IRTemp_INVALID;
         IRTemp oldV  = IRTemp_INVALID;
         IROp   mulOp = isS ? Iop_MullS32 : Iop_MullU32;
         assign( argL, getIRegA(rM));
         assign( argR, getIRegA(rS));
         assign( res, binop(mulOp, mkexpr(argL), mkexpr(argR)) );
         assign( resHi, unop(Iop_64HIto32, mkexpr(res)) );
         assign( resLo, unop(Iop_64to32, mkexpr(res)) );
         if (bitS) {
            oldC = newTemp(Ity_I32);
            assign(oldC, mk_armg_calculate_flag_c());
            oldV = newTemp(Ity_I32);
            assign(oldV, mk_armg_calculate_flag_v());
         }
         
         putIRegA( rDhi, mkexpr(resHi), condT, Ijk_Boring );
         putIRegA( rDlo, mkexpr(resLo), condT, Ijk_Boring );
         if (bitS) {
            IRTemp pair = newTemp(Ity_I32);
            assign( pair, binop(Iop_Or32,
                                binop(Iop_Shl32, mkexpr(oldC), mkU8(1)),
                                mkexpr(oldV)) );
            setFlags_D1_D2_ND( ARMG_CC_OP_MULL, resLo, resHi, pair, condT );
         }
         DIP("%cmull%c%s r%u, r%u, r%u, r%u\n",
             isS ? 's' : 'u', bitS ? 's' : ' ',
             nCC(INSN_COND), rDlo, rDhi, rM, rS);
         goto decode_success;
      }
      
   }

   
   if (BITS8(0,0,0,0,1,0,1,0) == (INSN(27,20) & BITS8(1,1,1,1,1,0,1,0))
       && INSN(7,4) == BITS4(1,0,0,1)) {
      UInt bitS = (insn >> 20) & 1; 
      UInt rDhi = INSN(19,16);
      UInt rDlo = INSN(15,12);
      UInt rS   = INSN(11,8);
      UInt rM   = INSN(3,0);
      UInt isS  = (INSN(27,20) >> 2) & 1; 
      if (rDhi == 15 || rDlo == 15 || rM == 15 || rS == 15 || rDhi == rDlo)  {
         
      } else {
         IRTemp argL  = newTemp(Ity_I32);
         IRTemp argR  = newTemp(Ity_I32);
         IRTemp old   = newTemp(Ity_I64);
         IRTemp res   = newTemp(Ity_I64);
         IRTemp resHi = newTemp(Ity_I32);
         IRTemp resLo = newTemp(Ity_I32);
         IRTemp oldC  = IRTemp_INVALID;
         IRTemp oldV  = IRTemp_INVALID;
         IROp   mulOp = isS ? Iop_MullS32 : Iop_MullU32;
         assign( argL, getIRegA(rM));
         assign( argR, getIRegA(rS));
         assign( old, binop(Iop_32HLto64, getIRegA(rDhi), getIRegA(rDlo)) );
         assign( res, binop(Iop_Add64,
                            mkexpr(old),
                            binop(mulOp, mkexpr(argL), mkexpr(argR))) );
         assign( resHi, unop(Iop_64HIto32, mkexpr(res)) );
         assign( resLo, unop(Iop_64to32, mkexpr(res)) );
         if (bitS) {
            oldC = newTemp(Ity_I32);
            assign(oldC, mk_armg_calculate_flag_c());
            oldV = newTemp(Ity_I32);
            assign(oldV, mk_armg_calculate_flag_v());
         }
         
         putIRegA( rDhi, mkexpr(resHi), condT, Ijk_Boring );
         putIRegA( rDlo, mkexpr(resLo), condT, Ijk_Boring );
         if (bitS) {
            IRTemp pair = newTemp(Ity_I32);
            assign( pair, binop(Iop_Or32,
                                binop(Iop_Shl32, mkexpr(oldC), mkU8(1)),
                                mkexpr(oldV)) );
            setFlags_D1_D2_ND( ARMG_CC_OP_MULL, resLo, resHi, pair, condT );
         }
         DIP("%cmlal%c%s r%u, r%u, r%u, r%u\n",
             isS ? 's' : 'u', bitS ? 's' : ' ', nCC(INSN_COND),
             rDlo, rDhi, rM, rS);
         goto decode_success;
      }
      
   }

   
   if (BITS8(0,0,0,0,0,1,0,0) == INSN(27,20) && INSN(7,4) == BITS4(1,0,0,1)) {
      UInt rDhi = INSN(19,16);
      UInt rDlo = INSN(15,12);
      UInt rM   = INSN(11,8);
      UInt rN   = INSN(3,0);
      if (rDlo == 15 || rDhi == 15 || rN == 15 || rM == 15 || rDhi == rDlo)  {
         
      } else {
         IRTemp argN   = newTemp(Ity_I32);
         IRTemp argM   = newTemp(Ity_I32);
         IRTemp argDhi = newTemp(Ity_I32);
         IRTemp argDlo = newTemp(Ity_I32);
         IRTemp res    = newTemp(Ity_I64);
         IRTemp resHi  = newTemp(Ity_I32);
         IRTemp resLo  = newTemp(Ity_I32);
         assign( argN,   getIRegA(rN) );
         assign( argM,   getIRegA(rM) );
         assign( argDhi, getIRegA(rDhi) );
         assign( argDlo, getIRegA(rDlo) );
         assign( res, 
                 binop(Iop_Add64,
                       binop(Iop_Add64,
                             binop(Iop_MullU32, mkexpr(argN), mkexpr(argM)),
                             unop(Iop_32Uto64, mkexpr(argDhi))),
                       unop(Iop_32Uto64, mkexpr(argDlo))) );
         assign( resHi, unop(Iop_64HIto32, mkexpr(res)) );
         assign( resLo, unop(Iop_64to32, mkexpr(res)) );
         
         putIRegA( rDhi, mkexpr(resHi), condT, Ijk_Boring );
         putIRegA( rDlo, mkexpr(resLo), condT, Ijk_Boring );
         DIP("umaal %s r%u, r%u, r%u, r%u\n",
             nCC(INSN_COND), rDlo, rDhi, rN, rM);
         goto decode_success;
      }
      
   }

   

   
   if (INSN(27,20) == BITS8(0,0,1,1,0,0,1,0)
       && INSN(17,12) == BITS6(0,0,1,1,1,1)) {
      UInt write_ge    = INSN(18,18);
      UInt write_nzcvq = INSN(19,19);
      if (write_nzcvq || write_ge) {
         UInt   imm = (INSN(11,0) >> 0) & 0xFF;
         UInt   rot = 2 * ((INSN(11,0) >> 8) & 0xF);
         IRTemp immT = newTemp(Ity_I32);
         vassert(rot <= 30);
         imm = ROR32(imm, rot);
         assign(immT, mkU32(imm));
         desynthesise_APSR( write_nzcvq, write_ge, immT, condT );
         DIP("msr%s cpsr%s%sf, #0x%08x\n", nCC(INSN_COND),
             write_nzcvq ? "f" : "", write_ge ? "g" : "", imm);
         goto decode_success;
      }
      
   }

   
   if (INSN(27,20) == BITS8(0,0,0,1,0,0,1,0) 
       && INSN(17,12) == BITS6(0,0,1,1,1,1)
       && INSN(11,4) == BITS8(0,0,0,0,0,0,0,0)) {
      UInt rN          = INSN(3,0);
      UInt write_ge    = INSN(18,18);
      UInt write_nzcvq = INSN(19,19);
      if (rN != 15 && (write_nzcvq || write_ge)) {
         IRTemp rNt = newTemp(Ity_I32);
         assign(rNt, getIRegA(rN));
         desynthesise_APSR( write_nzcvq, write_ge, rNt, condT );
         DIP("msr%s cpsr_%s%s, r%u\n", nCC(INSN_COND),
             write_nzcvq ? "f" : "", write_ge ? "g" : "", rN);
         goto decode_success;
      }
      
   }

   
   if ((insn & 0x0FFF0FFF) == 0x010F0000) {
      UInt rD   = INSN(15,12);
      if (rD != 15) {
         IRTemp apsr = synthesise_APSR();
         putIRegA( rD, mkexpr(apsr), condT, Ijk_Boring );
         DIP("mrs%s r%u, cpsr\n", nCC(INSN_COND), rD);
         goto decode_success;
      }
      
   }

   
   if (BITS8(1,1,1,1,0,0,0,0) == (INSN(27,20) & BITS8(1,1,1,1,0,0,0,0))) {
      UInt imm24 = (insn >> 0) & 0xFFFFFF;
      if (imm24 == 0) {
         
         if (condT != IRTemp_INVALID) {
            mk_skip_over_A32_if_cond_is_false( condT );
         }
         
         llPutIReg(15, mkU32( guest_R15_curr_instr_notENC + 4 ));
         dres.jk_StopHere = Ijk_Sys_syscall;
         dres.whatNext    = Dis_StopHere;
         DIP("svc%s #0x%08x\n", nCC(INSN_COND), imm24);
         goto decode_success;
      }
      
   }

   

   
   if (BITS8(0,0,0,1,0,0,0,0) == (INSN(27,20) & BITS8(1,1,1,1,1,0,1,1))
       && BITS4(0,0,0,0) == INSN(11,8)
       && BITS4(1,0,0,1) == INSN(7,4)) {
      UInt   rN   = INSN(19,16);
      UInt   rD   = INSN(15,12);
      UInt   rM   = INSN(3,0);
      IRTemp tRn  = newTemp(Ity_I32);
      IRTemp tNew = newTemp(Ity_I32);
      IRTemp tOld = IRTemp_INVALID;
      IRTemp tSC1 = newTemp(Ity_I1);
      UInt   isB  = (insn >> 22) & 1;

      if (rD == 15 || rN == 15 || rM == 15 || rN == rM || rN == rD) {
         
      } else {
         
         if (condT != IRTemp_INVALID) {
            mk_skip_over_A32_if_cond_is_false( condT );
            condT = IRTemp_INVALID;
         }
         
         assign(tRn, getIRegA(rN));
         assign(tNew, getIRegA(rM));
         if (isB) {
            
            tOld = newTemp(Ity_I8);
            stmt( IRStmt_LLSC(Iend_LE, tOld, mkexpr(tRn),
                              NULL) );
            stmt( IRStmt_LLSC(Iend_LE, tSC1, mkexpr(tRn),
                              unop(Iop_32to8, mkexpr(tNew))) );
         } else {
            
            tOld = newTemp(Ity_I32);
            stmt( IRStmt_LLSC(Iend_LE, tOld, mkexpr(tRn),
                              NULL) );
            stmt( IRStmt_LLSC(Iend_LE, tSC1, mkexpr(tRn),
                              mkexpr(tNew)) );
         }
         stmt( IRStmt_Exit(unop(Iop_Not1, mkexpr(tSC1)),
                           Ijk_Boring,
                           IRConst_U32(guest_R15_curr_instr_notENC),
                           OFFB_R15T ));
         putIRegA(rD, isB ? unop(Iop_8Uto32, mkexpr(tOld)) : mkexpr(tOld),
                      IRTemp_INVALID, Ijk_Boring);
         DIP("swp%s%s r%u, r%u, [r%u]\n",
             isB ? "b" : "", nCC(INSN_COND), rD, rM, rN);
         goto decode_success;
      }
      
   }

   
   
   

   

   
   if (0x01900F9F == (insn & 0x0F900FFF)) {
      UInt   rT    = INSN(15,12);
      UInt   rN    = INSN(19,16);
      IRType ty    = Ity_INVALID;
      IROp   widen = Iop_INVALID;
      const HChar* nm = NULL;
      Bool   valid = True;
      switch (INSN(22,21)) {
         case 0: nm = "";  ty = Ity_I32; break;
         case 1: nm = "d"; ty = Ity_I64; break;
         case 2: nm = "b"; ty = Ity_I8;  widen = Iop_8Uto32; break;
         case 3: nm = "h"; ty = Ity_I16; widen = Iop_16Uto32; break;
         default: vassert(0);
      }
      if (ty == Ity_I32 || ty == Ity_I16 || ty == Ity_I8) {
         if (rT == 15 || rN == 15)
            valid = False;
      } else {
         vassert(ty == Ity_I64);
         if ((rT & 1) == 1 || rT == 14 || rN == 15)
            valid = False;
      }
      if (valid) {
         IRTemp res;
         
         if (condT != IRTemp_INVALID) {
           mk_skip_over_A32_if_cond_is_false( condT );
           condT = IRTemp_INVALID;
         }
         
         res = newTemp(ty);
         
         stmt( IRStmt_LLSC(Iend_LE, res, getIRegA(rN),
                           NULL) );
         if (ty == Ity_I64) {
            
            putIRegA(rT+0, unop(Iop_64to32, mkexpr(res)),
                           IRTemp_INVALID, Ijk_Boring);
            putIRegA(rT+1, unop(Iop_64HIto32, mkexpr(res)),
                           IRTemp_INVALID, Ijk_Boring);
            DIP("ldrex%s%s r%u, r%u, [r%u]\n",
                nm, nCC(INSN_COND), rT+0, rT+1, rN);
         } else {
            putIRegA(rT, widen == Iop_INVALID
                            ? mkexpr(res) : unop(widen, mkexpr(res)),
                     IRTemp_INVALID, Ijk_Boring);
            DIP("ldrex%s%s r%u, [r%u]\n", nm, nCC(INSN_COND), rT, rN);
         }
         goto decode_success;
      }
      
   }

   
   if (0x01800F90 == (insn & 0x0F900FF0)) {
      UInt   rT     = INSN(3,0);
      UInt   rN     = INSN(19,16);
      UInt   rD     = INSN(15,12);
      IRType ty     = Ity_INVALID;
      IROp   narrow = Iop_INVALID;
      const HChar* nm = NULL;
      Bool   valid  = True;
      switch (INSN(22,21)) {
         case 0: nm = "";  ty = Ity_I32; break;
         case 1: nm = "d"; ty = Ity_I64; break;
         case 2: nm = "b"; ty = Ity_I8;  narrow = Iop_32to8; break;
         case 3: nm = "h"; ty = Ity_I16; narrow = Iop_32to16; break;
         default: vassert(0);
      }
      if (ty == Ity_I32 || ty == Ity_I16 || ty == Ity_I8) {
         if (rD == 15 || rN == 15 || rT == 15
             || rD == rN || rD == rT)
            valid = False;
      } else {
         vassert(ty == Ity_I64);
         if (rD == 15 || (rT & 1) == 1 || rT == 14 || rN == 15
             || rD == rN || rD == rT || rD == rT+1)
            valid = False;
      }
      if (valid) {
         IRTemp resSC1, resSC32, data;
         
         if (condT != IRTemp_INVALID) {
            mk_skip_over_A32_if_cond_is_false( condT );
            condT = IRTemp_INVALID;
         }
         
         data = newTemp(ty);
         assign(data,
                ty == Ity_I64
                   
                   ? binop(Iop_32HLto64, getIRegA(rT+1), getIRegA(rT+0))
                   : narrow == Iop_INVALID
                      ? getIRegA(rT)
                      : unop(narrow, getIRegA(rT)));
         resSC1 = newTemp(Ity_I1);
         
         stmt( IRStmt_LLSC(Iend_LE, resSC1, getIRegA(rN), mkexpr(data)) );

         resSC32 = newTemp(Ity_I32);
         assign(resSC32,
                unop(Iop_1Uto32, unop(Iop_Not1, mkexpr(resSC1))));

         putIRegA(rD, mkexpr(resSC32),
                      IRTemp_INVALID, Ijk_Boring);
         if (ty == Ity_I64) {
            DIP("strex%s%s r%u, r%u, r%u, [r%u]\n",
                nm, nCC(INSN_COND), rD, rT, rT+1, rN);
         } else {
            DIP("strex%s%s r%u, r%u, [r%u]\n",
                nm, nCC(INSN_COND), rD, rT, rN);
         }
         goto decode_success;
      }
      
   }

   
   if (0x03000000 == (insn & 0x0FF00000)
       || 0x03400000 == (insn & 0x0FF00000))  {
      UInt rD    = INSN(15,12);
      UInt imm16 = (insn & 0xFFF) | ((insn >> 4) & 0x0000F000);
      UInt isT   = (insn >> 22) & 1;
      if (rD == 15) {
         
      } else {
         if (isT) {
            putIRegA(rD,
                     binop(Iop_Or32,
                           binop(Iop_And32, getIRegA(rD), mkU32(0xFFFF)),
                           mkU32(imm16 << 16)),
                     condT, Ijk_Boring);
            DIP("movt%s r%u, #0x%04x\n", nCC(INSN_COND), rD, imm16);
            goto decode_success;
         } else {
            putIRegA(rD, mkU32(imm16), condT, Ijk_Boring);
            DIP("movw%s r%u, #0x%04x\n", nCC(INSN_COND), rD, imm16);
            goto decode_success;
         }
      }
      
   }

   
   if (BITS8(0,1,1,0,1, 0,0,0) == (INSN(27,20) & BITS8(1,1,1,1,1,0,0,0))
       && BITS4(1,1,1,1) == INSN(19,16)
       && BITS4(0,1,1,1) == INSN(7,4)
       && BITS4(0,0, 0,0) == (INSN(11,8) & BITS4(0,0,1,1))) {
      UInt subopc = INSN(27,20) & BITS8(0,0,0,0,0, 1,1,1);
      if (subopc != BITS4(0,0,0,1) && subopc != BITS4(0,1,0,1)) {
         Int    rot  = (INSN(11,8) >> 2) & 3;
         UInt   rM   = INSN(3,0);
         UInt   rD   = INSN(15,12);
         IRTemp srcT = newTemp(Ity_I32);
         IRTemp rotT = newTemp(Ity_I32);
         IRTemp dstT = newTemp(Ity_I32);
         const HChar* nm = "???";
         assign(srcT, getIRegA(rM));
         assign(rotT, genROR32(srcT, 8 * rot)); 
         switch (subopc) {
            case BITS4(0,1,1,0): 
               assign(dstT, unop(Iop_8Uto32, unop(Iop_32to8, mkexpr(rotT))));
               nm = "uxtb";
               break;
            case BITS4(0,0,1,0): 
               assign(dstT, unop(Iop_8Sto32, unop(Iop_32to8, mkexpr(rotT))));
               nm = "sxtb";
               break;
            case BITS4(0,1,1,1): 
               assign(dstT, unop(Iop_16Uto32, unop(Iop_32to16, mkexpr(rotT))));
               nm = "uxth";
               break;
            case BITS4(0,0,1,1): 
               assign(dstT, unop(Iop_16Sto32, unop(Iop_32to16, mkexpr(rotT))));
               nm = "sxth";
               break;
            case BITS4(0,1,0,0): 
               assign(dstT, binop(Iop_And32, mkexpr(rotT), mkU32(0x00FF00FF)));
               nm = "uxtb16";
               break;
            case BITS4(0,0,0,0): { 
               IRTemp lo32 = newTemp(Ity_I32);
               IRTemp hi32 = newTemp(Ity_I32);
               assign(lo32, binop(Iop_And32, mkexpr(rotT), mkU32(0xFF)));
               assign(hi32, binop(Iop_Shr32, mkexpr(rotT), mkU8(16)));
               assign(
                  dstT,
                  binop(Iop_Or32,
                        binop(Iop_And32,
                              unop(Iop_8Sto32,
                                   unop(Iop_32to8, mkexpr(lo32))),
                              mkU32(0xFFFF)),
                        binop(Iop_Shl32,
                              unop(Iop_8Sto32,
                                   unop(Iop_32to8, mkexpr(hi32))),
                              mkU8(16))
               ));
               nm = "sxtb16";
               break;
            }
            default:
               vassert(0); 
         }
         putIRegA(rD, mkexpr(dstT), condT, Ijk_Boring);
         DIP("%s%s r%u, r%u, ROR #%u\n", nm, nCC(INSN_COND), rD, rM, rot);
         goto decode_success;
      }
      
   }

   
   if (BITS8(0,1,1,1,1,1,0, 0) == (INSN(27,20) & BITS8(1,1,1,1,1,1,1,0))
       && BITS4(0, 0,0,1) == (INSN(7,4) & BITS4(0,1,1,1))) {
      UInt rD  = INSN(15,12);
      UInt rN  = INSN(3,0);
      UInt msb = (insn >> 16) & 0x1F; 
      UInt lsb = (insn >> 7) & 0x1F;  
      if (rD == 15 || msb < lsb) {
         
      } else {
         IRTemp src    = newTemp(Ity_I32);
         IRTemp olddst = newTemp(Ity_I32);
         IRTemp newdst = newTemp(Ity_I32);
         UInt   mask = 1 << (msb - lsb);
         mask = (mask - 1) + mask;
         vassert(mask != 0); 
         mask <<= lsb;

         assign(src, rN == 15 ? mkU32(0) : getIRegA(rN));
         assign(olddst, getIRegA(rD));
         assign(newdst,
                binop(Iop_Or32,
                   binop(Iop_And32,
                         binop(Iop_Shl32, mkexpr(src), mkU8(lsb)), 
                         mkU32(mask)),
                   binop(Iop_And32,
                         mkexpr(olddst),
                         mkU32(~mask)))
               );

         putIRegA(rD, mkexpr(newdst), condT, Ijk_Boring);

         if (rN == 15) {
            DIP("bfc%s r%u, #%u, #%u\n",
                nCC(INSN_COND), rD, lsb, msb-lsb+1);
         } else {
            DIP("bfi%s r%u, r%u, #%u, #%u\n",
                nCC(INSN_COND), rD, rN, lsb, msb-lsb+1);
         }
         goto decode_success;
      }
      
   }

   
   if (BITS8(0,1,1,1,1,0,1,0) == (INSN(27,20) & BITS8(1,1,1,1,1,0,1,0))
       && BITS4(0,1,0,1) == (INSN(7,4) & BITS4(0,1,1,1))) {
      UInt rD  = INSN(15,12);
      UInt rN  = INSN(3,0);
      UInt wm1 = (insn >> 16) & 0x1F; 
      UInt lsb = (insn >> 7) & 0x1F;  
      UInt msb = lsb + wm1;
      UInt isU = (insn >> 22) & 1;    
      if (rD == 15 || rN == 15 || msb >= 32) {
         
      } else {
         IRTemp src  = newTemp(Ity_I32);
         IRTemp tmp  = newTemp(Ity_I32);
         IRTemp res  = newTemp(Ity_I32);
         UInt   mask = ((1 << wm1) - 1) + (1 << wm1);
         vassert(msb >= 0 && msb <= 31);
         vassert(mask != 0); 

         assign(src, getIRegA(rN));
         assign(tmp, binop(Iop_And32,
                           binop(Iop_Shr32, mkexpr(src), mkU8(lsb)),
                           mkU32(mask)));
         assign(res, binop(isU ? Iop_Shr32 : Iop_Sar32,
                           binop(Iop_Shl32, mkexpr(tmp), mkU8(31-wm1)),
                           mkU8(31-wm1)));

         putIRegA(rD, mkexpr(res), condT, Ijk_Boring);

         DIP("%s%s r%u, r%u, #%u, #%u\n",
             isU ? "ubfx" : "sbfx",
             nCC(INSN_COND), rD, rN, lsb, wm1 + 1);
         goto decode_success;
      }
      
   }

   
   
   
   if ((INSN(27,24) & BITS4(1,1,1,0)) != BITS4(0,0,0,0))
      goto after_load_store_doubleword;

   
   if ((INSN(7,4) & BITS4(1,1,0,1)) != BITS4(1,1,0,1))
      goto after_load_store_doubleword;

   summary = 0;

    if (INSN(27,24) == BITS4(0,0,0,1) && INSN(22,20) == BITS3(1,0,0)) {
      summary = 1 | 16;
   }
   else if (INSN(27,24) == BITS4(0,0,0,1) && INSN(22,20) == BITS3(0,0,0)) {
      summary = 1 | 32;
   }
   else if (INSN(27,24) == BITS4(0,0,0,1) && INSN(22,20) == BITS3(1,1,0)) {
      summary = 2 | 16;
   }
   else if (INSN(27,24) == BITS4(0,0,0,1) && INSN(22,20) == BITS3(0,1,0)) {
      summary = 2 | 32;
   }
   else if (INSN(27,24) == BITS4(0,0,0,0) && INSN(22,20) == BITS3(1,0,0)) {
      summary = 3 | 16;
   }
   else if (INSN(27,24) == BITS4(0,0,0,0) && INSN(22,20) == BITS3(0,0,0)) {
      summary = 3 | 32;
   }
   else goto after_load_store_doubleword;

   { UInt rN   = (insn >> 16) & 0xF; 
     UInt rD   = (insn >> 12) & 0xF; 
     UInt rM   = (insn >> 0)  & 0xF; 
     UInt bU   = (insn >> 23) & 1;   
     UInt bS   = (insn >> 5) & 1;    
     UInt imm8 = ((insn >> 4) & 0xF0) | (insn & 0xF); 

     
     if ((rD & 1) != 0)
        goto after_load_store_doubleword;

     
     if ((summary & 32) != 0 && (imm8 & 0xF0) != 0)
        goto after_load_store_doubleword;

     switch (summary) {
        case 1 | 16:
           break;
        case 1 | 32: 
           if (rM == 15) goto after_load_store_doubleword;
           break;
        case 2 | 16: case 3 | 16:
           if (rN == 15) goto after_load_store_doubleword;
           if (bS == 0 && (rN == rD || rN == rD+1))
              goto after_load_store_doubleword;
           break;
        case 2 | 32: case 3 | 32:
           if (rM == 15) goto after_load_store_doubleword;
           if (rN == 15) goto after_load_store_doubleword;
           if (rN == rM) goto after_load_store_doubleword;
           if (bS == 0 && (rN == rD || rN == rD+1))
              goto after_load_store_doubleword;
           break;
        default:
           vassert(0);
     }

     vassert((rD & 1) == 0); 
     if (bS == 0 && rD+1 == 15 && condT != IRTemp_INVALID) {
        
        mk_skip_over_A32_if_cond_is_false( condT );
        condT = IRTemp_INVALID;
        
     }

     IRExpr* eaE = NULL;
     switch (summary & 0xF0) {
        case 16:
           eaE = mk_EA_reg_plusminus_imm8( rN, bU, imm8, dis_buf );
           break;
        case 32:
           eaE = mk_EA_reg_plusminus_reg( rN, bU, rM, dis_buf );
           break;
     }
     vassert(eaE);
     IRTemp eaT = newTemp(Ity_I32);
     assign(eaT, eaE);

     
     IRTemp rnT = newTemp(Ity_I32);
     assign(rnT, getIRegA(rN));

     
     IRTemp taT = IRTemp_INVALID;
     switch (summary & 0x0F) {
        case 1: case 2: taT = eaT; break;
        case 3:         taT = rnT; break;
     }
     vassert(taT != IRTemp_INVALID);

     

     Bool writeback_already_done = False;
     if (bS == 1  && summary == (2 | 16)
         && rN == 13 && rN != rD && rN != rD+1
         && bU == 0) {
        putIRegA( rN, mkexpr(eaT), condT, Ijk_Boring );
        writeback_already_done = True;
     }

     const HChar* name = NULL;
     
     if (bS == 1) { 
        storeGuardedLE( binop(Iop_Add32, mkexpr(taT), mkU32(0)),
                        getIRegA(rD+0), condT );
        storeGuardedLE( binop(Iop_Add32, mkexpr(taT), mkU32(4)),
                        getIRegA(rD+1), condT );
        name = "strd";
     } else { 
        IRTemp oldRd0 = newTemp(Ity_I32);
        IRTemp oldRd1 = newTemp(Ity_I32);
        assign(oldRd0, llGetIReg(rD+0));
        assign(oldRd1, llGetIReg(rD+1));
        IRTemp newRd0 = newTemp(Ity_I32);
        IRTemp newRd1 = newTemp(Ity_I32);
        loadGuardedLE( newRd0, ILGop_Ident32,
                       binop(Iop_Add32, mkexpr(taT), mkU32(0)),
                       mkexpr(oldRd0), condT );
        putIRegA( rD+0, mkexpr(newRd0), IRTemp_INVALID, Ijk_Boring );
        loadGuardedLE( newRd1, ILGop_Ident32,
                       binop(Iop_Add32, mkexpr(taT), mkU32(4)),
                       mkexpr(oldRd1), condT );
        putIRegA( rD+1, mkexpr(newRd1), IRTemp_INVALID, Ijk_Boring );
        name = "ldrd";
     }

     
     switch (summary & 0x0F) {
        case 2: case 3:
           
           vassert(rN != 15); 
           if (bS == 0) {
              vassert(rD+0 != rN); 
              vassert(rD+1 != rN); 
           }
           if (!writeback_already_done)
              putIRegA( rN, mkexpr(eaT), condT, Ijk_Boring );
           break;
     }

     switch (summary & 0x0F) {
        case 1:  DIP("%s%s r%u, %s\n", name, nCC(INSN_COND), rD, dis_buf);
                 break;
        case 2:  DIP("%s%s r%u, %s! (at-EA-then-Rn=EA)\n",
                     name, nCC(INSN_COND), rD, dis_buf);
                 break;
        case 3:  DIP("%s%s r%u, %s! (at-Rn-then-Rn=EA)\n",
                     name, nCC(INSN_COND), rD, dis_buf);
                 break;
        default: vassert(0);
     }

     goto decode_success;
   }

  after_load_store_doubleword:

   
   if (BITS8(0,1,1,0,1,0,1,0) == (INSN(27,20) & BITS8(1,1,1,1,1,0,1,1))
       && BITS4(0,0,0,0) == (INSN(11,8) & BITS4(0,0,1,1))
       && BITS4(0,1,1,1) == INSN(7,4)) {
      UInt rN  = INSN(19,16);
      UInt rD  = INSN(15,12);
      UInt rM  = INSN(3,0);
      UInt rot = (insn >> 10) & 3;
      UInt isU = INSN(22,22);
      if (rN == 15 || rD == 15 || rM == 15) {
         
      } else {
         IRTemp srcL = newTemp(Ity_I32);
         IRTemp srcR = newTemp(Ity_I32);
         IRTemp res  = newTemp(Ity_I32);
         assign(srcR, getIRegA(rM));
         assign(srcL, getIRegA(rN));
         assign(res,  binop(Iop_Add32,
                            mkexpr(srcL),
                            unop(isU ? Iop_8Uto32 : Iop_8Sto32,
                                 unop(Iop_32to8, 
                                      genROR32(srcR, 8 * rot)))));
         putIRegA(rD, mkexpr(res), condT, Ijk_Boring);
         DIP("%cxtab%s r%u, r%u, r%u, ror #%u\n",
             isU ? 'u' : 's', nCC(INSN_COND), rD, rN, rM, rot);
         goto decode_success;
      }
      
   }

   
   if (BITS8(0,1,1,0,1,0,1,1) == (INSN(27,20) & BITS8(1,1,1,1,1,0,1,1))
       && BITS4(0,0,0,0) == (INSN(11,8) & BITS4(0,0,1,1))
       && BITS4(0,1,1,1) == INSN(7,4)) {
      UInt rN  = INSN(19,16);
      UInt rD  = INSN(15,12);
      UInt rM  = INSN(3,0);
      UInt rot = (insn >> 10) & 3;
      UInt isU = INSN(22,22);
      if (rN == 15 || rD == 15 || rM == 15) {
         
      } else {
         IRTemp srcL = newTemp(Ity_I32);
         IRTemp srcR = newTemp(Ity_I32);
         IRTemp res  = newTemp(Ity_I32);
         assign(srcR, getIRegA(rM));
         assign(srcL, getIRegA(rN));
         assign(res,  binop(Iop_Add32,
                            mkexpr(srcL),
                            unop(isU ? Iop_16Uto32 : Iop_16Sto32,
                                 unop(Iop_32to16, 
                                      genROR32(srcR, 8 * rot)))));
         putIRegA(rD, mkexpr(res), condT, Ijk_Boring);

         DIP("%cxtah%s r%u, r%u, r%u, ror #%u\n",
             isU ? 'u' : 's', nCC(INSN_COND), rD, rN, rM, rot);
         goto decode_success;
      }
      
   }

   
   if (INSN(27,16) == 0x6BF
       && (INSN(11,4) == 0xFB || INSN(11,4) == 0xF3)) {
      Bool isREV = INSN(11,4) == 0xF3;
      UInt rM    = INSN(3,0);
      UInt rD    = INSN(15,12);
      if (rM != 15 && rD != 15) {
         IRTemp rMt = newTemp(Ity_I32);
         assign(rMt, getIRegA(rM));
         IRTemp res = isREV ? gen_REV(rMt) : gen_REV16(rMt);
         putIRegA(rD, mkexpr(res), condT, Ijk_Boring);
         DIP("rev%s%s r%u, r%u\n", isREV ? "" : "16",
             nCC(INSN_COND), rD, rM);
         goto decode_success;
      }
   }

   
   if (INSN(27,16) == 0x6FF && INSN(11,4) == 0xFB) {
      UInt rM = INSN(3,0);
      UInt rD = INSN(15,12);
      if (rM != 15 && rD != 15) {
         IRTemp irt_rM  = newTemp(Ity_I32);
         IRTemp irt_hi  = newTemp(Ity_I32);
         IRTemp irt_low = newTemp(Ity_I32);
         IRTemp irt_res = newTemp(Ity_I32);
         assign(irt_rM, getIRegA(rM));
         assign(irt_hi,
                binop(Iop_Sar32,
                      binop(Iop_Shl32, mkexpr(irt_rM), mkU8(24)),
                      mkU8(16)
                )
         );
         assign(irt_low,
                binop(Iop_And32,
                      binop(Iop_Shr32, mkexpr(irt_rM), mkU8(8)),
                      mkU32(0xFF)
                )
         );
         assign(irt_res,
                binop(Iop_Or32, mkexpr(irt_hi), mkexpr(irt_low))
         );
         putIRegA(rD, mkexpr(irt_res), condT, Ijk_Boring);
         DIP("revsh%s r%u, r%u\n", nCC(INSN_COND), rD, rM);
         goto decode_success;
      }
   }

   
   if (INSN(27,16) == 0x6FF && INSN(11,4) == 0xF3) {
      UInt rD = INSN(15,12);
      UInt rM = INSN(3,0);
      if (rD != 15 && rM != 15) {
         IRTemp arg = newTemp(Ity_I32);
         assign(arg, getIRegA(rM));
         IRTemp res = gen_BITREV(arg);
         putIRegA(rD, mkexpr(res), condT, Ijk_Boring);
         DIP("rbit r%u, r%u\n", rD, rM);
         goto decode_success;
      }
   }

   
   if (INSN(27,20) == BITS8(0,1,1,1,0,1,0,1)
       && INSN(15,12) == BITS4(1,1,1,1)
       && (INSN(7,4) & BITS4(1,1,0,1)) == BITS4(0,0,0,1)) {
      UInt bitR = INSN(5,5);
      UInt rD = INSN(19,16);
      UInt rM = INSN(11,8);
      UInt rN = INSN(3,0);
      if (rD != 15 && rM != 15 && rN != 15) {
         IRExpr* res
         = unop(Iop_64HIto32,
                binop(Iop_Add64,
                      binop(Iop_MullS32, getIRegA(rN), getIRegA(rM)),
                      mkU64(bitR ? 0x80000000ULL : 0ULL)));
         putIRegA(rD, res, condT, Ijk_Boring);
         DIP("smmul%s%s r%u, r%u, r%u\n",
             nCC(INSN_COND), bitR ? "r" : "", rD, rN, rM);
         goto decode_success;
      }
   }

   
   if (INSN(27,20) == BITS8(0,1,1,1,0,1,0,1)
       && INSN(15,12) != BITS4(1,1,1,1)
       && (INSN(7,4) & BITS4(1,1,0,1)) == BITS4(0,0,0,1)) {
      UInt bitR = INSN(5,5);
      UInt rD = INSN(19,16);
      UInt rA = INSN(15,12);
      UInt rM = INSN(11,8);
      UInt rN = INSN(3,0);
      if (rD != 15 && rM != 15 && rN != 15) {
         IRExpr* res
         = unop(Iop_64HIto32,
                binop(Iop_Add64,
                      binop(Iop_Add64,
                            binop(Iop_32HLto64, getIRegA(rA), mkU32(0)),
                            binop(Iop_MullS32, getIRegA(rN), getIRegA(rM))),
                      mkU64(bitR ? 0x80000000ULL : 0ULL)));
         putIRegA(rD, res, condT, Ijk_Boring);
         DIP("smmla%s%s r%u, r%u, r%u, r%u\n",
             nCC(INSN_COND), bitR ? "r" : "", rD, rN, rM, rA);
         goto decode_success;
      }
   }

   
   if (0x0320F000 == (insn & 0x0FFFFFFF)) {
      DIP("nop%s\n", nCC(INSN_COND));
      goto decode_success;
   }

   
   if ( (INSN(27,20) & BITS8(1,1,1,1,0,1,1,1)) == BITS8(0,1,0,0,0,0,1,1) ) {
      UInt rT     = INSN(15,12);
      UInt rN     = INSN(19,16);
      UInt imm12  = INSN(11,0);
      UInt bU     = INSN(23,23);
      Bool valid  = True;
      if (rT == 15 || rN == 15 || rN == rT) valid = False;
      if (valid) {
         IRTemp newRt = newTemp(Ity_I32);
         loadGuardedLE( newRt,
                        ILGop_Ident32, getIRegA(rN), getIRegA(rT), condT );
         putIRegA(rT, mkexpr(newRt), IRTemp_INVALID, Ijk_Boring);
         IRExpr* erN = binop(bU ? Iop_Add32 : Iop_Sub32,
                             getIRegA(rN), mkU32(imm12));
         putIRegA(rN, erN, condT, Ijk_Boring);
         DIP("ldrt%s r%u, [r%u], #%c%u\n",
             nCC(INSN_COND), rT, rN, bU ? '+' : '-', imm12);
         goto decode_success;
      }
   }

   
   if ( (INSN(27,20) & BITS8(1,1,1,1,0,1,1,1)) == BITS8(0,1,1,0,0,0,1,1)
        && INSN(4,4) == 0 ) {
      UInt rT     = INSN(15,12);
      UInt rN     = INSN(19,16);
      UInt rM     = INSN(3,0);
      UInt imm5   = INSN(11,7);
      UInt bU     = INSN(23,23);
      UInt type   = INSN(6,5);
      Bool valid  = True;
      if (rT == 15 || rN == 15 || rN == rT || rM == 15
          )
         valid = False;
      if (valid) {
         IRTemp newRt = newTemp(Ity_I32);
         loadGuardedLE( newRt,
                        ILGop_Ident32, getIRegA(rN), getIRegA(rT), condT );
         putIRegA(rT, mkexpr(newRt), IRTemp_INVALID, Ijk_Boring);
         
         IRExpr* erN = mk_EA_reg_plusminus_shifted_reg(rN, bU, rM,
                                                       type, imm5, dis_buf);
         putIRegA(rN, erN, condT, Ijk_Boring);
         DIP("ldrt%s r%u, %s\n", nCC(INSN_COND), rT, dis_buf);
         goto decode_success;
      }
   }

   
   if ( (INSN(27,20) & BITS8(1,1,1,1,0,1,1,1)) == BITS8(0,1,0,0,0,1,1,1) ) {
      UInt rT     = INSN(15,12);
      UInt rN     = INSN(19,16);
      UInt imm12  = INSN(11,0);
      UInt bU     = INSN(23,23);
      Bool valid  = True;
      if (rT == 15 || rN == 15 || rN == rT) valid = False;
      if (valid) {
         IRTemp newRt = newTemp(Ity_I32);
         loadGuardedLE( newRt,
                        ILGop_8Uto32, getIRegA(rN), getIRegA(rT), condT );
         putIRegA(rT, mkexpr(newRt), IRTemp_INVALID, Ijk_Boring);
         IRExpr* erN = binop(bU ? Iop_Add32 : Iop_Sub32,
                             getIRegA(rN), mkU32(imm12));
         putIRegA(rN, erN, condT, Ijk_Boring);
         DIP("ldrbt%s r%u, [r%u], #%c%u\n",
             nCC(INSN_COND), rT, rN, bU ? '+' : '-', imm12);
         goto decode_success;
      }
   }

   
   if ( (INSN(27,20) & BITS8(1,1,1,1,0,1,1,1)) == BITS8(0,1,1,0,0,1,1,1)
        && INSN(4,4) == 0 ) {
      UInt rT     = INSN(15,12);
      UInt rN     = INSN(19,16);
      UInt rM     = INSN(3,0);
      UInt imm5   = INSN(11,7);
      UInt bU     = INSN(23,23);
      UInt type   = INSN(6,5);
      Bool valid  = True;
      if (rT == 15 || rN == 15 || rN == rT || rM == 15
          )
         valid = False;
      if (valid) {
         IRTemp newRt = newTemp(Ity_I32);
         loadGuardedLE( newRt,
                        ILGop_8Uto32, getIRegA(rN), getIRegA(rT), condT );
         putIRegA(rT, mkexpr(newRt), IRTemp_INVALID, Ijk_Boring);
         
         IRExpr* erN = mk_EA_reg_plusminus_shifted_reg(rN, bU, rM,
                                                       type, imm5, dis_buf);
         putIRegA(rN, erN, condT, Ijk_Boring);
         DIP("ldrbt%s r%u, %s\n", nCC(INSN_COND), rT, dis_buf);
         goto decode_success;
      }
   }

   
   if ( (INSN(27,20) & BITS8(1,1,1,1,0,1,1,1)) == BITS8(0,0,0,0,0,1,1,1)
       && INSN(7,4) == BITS4(1,0,1,1) ) {
      UInt rT    = INSN(15,12);
      UInt rN    = INSN(19,16);
      UInt bU    = INSN(23,23);
      UInt imm4H = INSN(11,8);
      UInt imm4L = INSN(3,0);
      UInt imm8  = (imm4H << 4) | imm4L;
      Bool valid = True;
      if (rT == 15 || rN == 15 || rN == rT)
         valid = False;
      if (valid) {
         IRTemp newRt = newTemp(Ity_I32);
         loadGuardedLE( newRt,
                        ILGop_16Uto32, getIRegA(rN), getIRegA(rT), condT );
         putIRegA(rT, mkexpr(newRt), IRTemp_INVALID, Ijk_Boring);
         IRExpr* erN = binop(bU ? Iop_Add32 : Iop_Sub32,
                             getIRegA(rN), mkU32(imm8));
         putIRegA(rN, erN, condT, Ijk_Boring);
         DIP("ldrht%s r%u, [r%u], #%c%u\n",
             nCC(INSN_COND), rT, rN, bU ? '+' : '-', imm8);
         goto decode_success;
      }
   }

   
   if ( (INSN(27,20) & BITS8(1,1,1,1,0,1,1,1)) == BITS8(0,0,0,0,0,0,1,1)
       && INSN(11,4) == BITS8(0,0,0,0,1,0,1,1) ) {
      UInt rT    = INSN(15,12);
      UInt rN    = INSN(19,16);
      UInt rM    = INSN(3,0);
      UInt bU    = INSN(23,23);
      Bool valid = True;
      if (rT == 15 || rN == 15 || rN == rT || rM == 15)
         valid = False;
      if (valid) {
         IRTemp newRt = newTemp(Ity_I32);
         loadGuardedLE( newRt,
                        ILGop_16Uto32, getIRegA(rN), getIRegA(rT), condT );
         putIRegA(rT, mkexpr(newRt), IRTemp_INVALID, Ijk_Boring);
         IRExpr* erN = binop(bU ? Iop_Add32 : Iop_Sub32,
                             getIRegA(rN), getIRegA(rM));
         putIRegA(rN, erN, condT, Ijk_Boring);
         DIP("ldrht%s r%u, [r%u], %cr%u\n",
             nCC(INSN_COND), rT, rN, bU ? '+' : '-', rM);
         goto decode_success;
      }
   }

   
   if ( (INSN(27,20) & BITS8(1,1,1,1,0,1,1,1)) == BITS8(0,0,0,0,0,1,1,1)
       && INSN(7,4) == BITS4(1,1,1,1)) {
      UInt rT    = INSN(15,12);
      UInt rN    = INSN(19,16);
      UInt bU    = INSN(23,23);
      UInt imm4H = INSN(11,8);
      UInt imm4L = INSN(3,0);
      UInt imm8  = (imm4H << 4) | imm4L;
      Bool valid = True;
      if (rN == 15 || rT == 15 || rN == rT)
         valid = False;
      if (valid) {
         IRTemp newRt = newTemp(Ity_I32);
         loadGuardedLE( newRt,
                        ILGop_16Sto32, getIRegA(rN), getIRegA(rT), condT );
         putIRegA(rT, mkexpr(newRt), IRTemp_INVALID, Ijk_Boring);
         IRExpr* erN = binop(bU ? Iop_Add32 : Iop_Sub32,
                             getIRegA(rN), mkU32(imm8));
         putIRegA(rN, erN, condT, Ijk_Boring);
         DIP("ldrsht%s r%u, [r%u], #%c%u\n",
             nCC(INSN_COND), rT, rN, bU ? '+' : '-', imm8);
         goto decode_success;
      }
   }

   
   if ( (INSN(27,20) & BITS8(1,1,1,1,0,1,1,1)) == BITS8(0,0,0,0,0,0,1,1)
       && INSN(11,4) == BITS8(0,0,0,0,1,1,1,1)) {
      UInt rT    = INSN(15,12);
      UInt rN    = INSN(19,16);
      UInt rM    = INSN(3,0);
      UInt bU    = INSN(23,23);
      Bool valid = True;
      if (rN == 15 || rT == 15 || rN == rT || rM == 15)
         valid = False;
      if (valid) {
         IRTemp newRt = newTemp(Ity_I32);
         loadGuardedLE( newRt,
                        ILGop_16Sto32, getIRegA(rN), getIRegA(rT), condT );
         putIRegA(rT, mkexpr(newRt), IRTemp_INVALID, Ijk_Boring);
         IRExpr* erN = binop(bU ? Iop_Add32 : Iop_Sub32,
                             getIRegA(rN), getIRegA(rM));
         putIRegA(rN, erN, condT, Ijk_Boring);
         DIP("ldrsht%s r%u, [r%u], %cr%u\n",
             nCC(INSN_COND), rT, rN, bU ? '+' : '-', rM);
         goto decode_success;
      }
   }

   
   if ( (INSN(27,20) & BITS8(1,1,1,1,0,1,1,1)) == BITS8(0,0,0,0,0,1,1,1)
       && INSN(7,4) == BITS4(1,1,0,1)) {
      UInt rT    = INSN(15,12);
      UInt rN    = INSN(19,16);
      UInt bU    = INSN(23,23);
      UInt imm4H = INSN(11,8);
      UInt imm4L = INSN(3,0);
      UInt imm8  = (imm4H << 4) | imm4L;
      Bool valid = True;
      if (rT == 15 || rN == 15 || rN == rT)
         valid = False;
      if (valid) {
         IRTemp newRt = newTemp(Ity_I32);
         loadGuardedLE( newRt,
                        ILGop_8Sto32, getIRegA(rN), getIRegA(rT), condT );
         putIRegA(rT, mkexpr(newRt), IRTemp_INVALID, Ijk_Boring);
         IRExpr* erN = binop(bU ? Iop_Add32 : Iop_Sub32,
                             getIRegA(rN), mkU32(imm8));
         putIRegA(rN, erN, condT, Ijk_Boring);
         DIP("ldrsbt%s r%u, [r%u], #%c%u\n",
             nCC(INSN_COND), rT, rN, bU ? '+' : '-', imm8);
         goto decode_success;
      }
   }

   
   if ( (INSN(27,20) & BITS8(1,1,1,1,0,1,1,1)) == BITS8(0,0,0,0,0,0,1,1)
       && INSN(11,4) == BITS8(0,0,0,0,1,1,0,1)) {
      UInt rT    = INSN(15,12);
      UInt rN    = INSN(19,16);
      UInt bU    = INSN(23,23);
      UInt rM    = INSN(3,0);
      Bool valid = True;
      if (rT == 15 || rN == 15 || rN == rT || rM == 15)
         valid = False;
      if (valid) {
         IRTemp newRt = newTemp(Ity_I32);
         loadGuardedLE( newRt,
                        ILGop_8Sto32, getIRegA(rN), getIRegA(rT), condT );
         putIRegA(rT, mkexpr(newRt), IRTemp_INVALID, Ijk_Boring);
         IRExpr* erN = binop(bU ? Iop_Add32 : Iop_Sub32,
                             getIRegA(rN), getIRegA(rM));
         putIRegA(rN, erN, condT, Ijk_Boring);
         DIP("ldrsbt%s r%u, [r%u], %cr%u\n",
             nCC(INSN_COND), rT, rN, bU ? '+' : '-', rM);
         goto decode_success;
      }
   }

   
   if ( (INSN(27,20) & BITS8(1,1,1,1,0,1,1,1)) == BITS8(0,1,0,0,0,1,1,0) ) {
      UInt rT     = INSN(15,12);
      UInt rN     = INSN(19,16);
      UInt imm12  = INSN(11,0);
      UInt bU     = INSN(23,23);
      Bool valid = True;
      if (rT == 15 || rN == 15 || rN == rT) valid = False;
      if (valid) {
         IRExpr* address = getIRegA(rN);
         IRExpr* data = unop(Iop_32to8, getIRegA(rT));
         storeGuardedLE( address, data, condT);
         IRExpr* newRn = binop(bU ? Iop_Add32 : Iop_Sub32,
                               getIRegA(rN), mkU32(imm12));
         putIRegA(rN, newRn, condT, Ijk_Boring);
         DIP("strbt%s r%u, [r%u], #%c%u\n",
             nCC(INSN_COND), rT, rN, bU ? '+' : '-', imm12);
         goto decode_success;
      }
   }

   
   if ( (INSN(27,20) & BITS8(1,1,1,1,0,1,1,1)) == BITS8(0,1,1,0,0,1,1,0)
       && INSN(4,4) == 0) {
      UInt rT     = INSN(15,12);
      UInt rN     = INSN(19,16);
      UInt imm5   = INSN(11,7);
      UInt type   = INSN(6,5);
      UInt rM     = INSN(3,0);
      UInt bU     = INSN(23,23);
      Bool valid  = True;
      if (rT == 15 || rN == 15 || rN == rT || rM == 15) valid = False;
      if (valid) {
         IRExpr* address = getIRegA(rN);
         IRExpr* data = unop(Iop_32to8, getIRegA(rT));
         storeGuardedLE( address, data, condT);
         
         IRExpr* erN = mk_EA_reg_plusminus_shifted_reg(rN, bU, rM,
                                                       type, imm5, dis_buf);
         putIRegA(rN, erN, condT, Ijk_Boring);
         DIP("strbt%s r%u, %s\n", nCC(INSN_COND), rT, dis_buf);
         goto decode_success;
      }
   }

   
   if ( (INSN(27,20) & BITS8(1,1,1,1,0,1,1,1)) == BITS8(0,0,0,0,0,1,1,0)
       && INSN(7,4) == BITS4(1,0,1,1) ) {
      UInt rT    = INSN(15,12);
      UInt rN    = INSN(19,16);
      UInt imm4H = INSN(11,8);
      UInt imm4L = INSN(3,0);
      UInt imm8  = (imm4H << 4) | imm4L;
      UInt bU    = INSN(23,23);
      Bool valid = True;
      if (rT == 15 || rN == 15 || rN == rT) valid = False;
      if (valid) {
         IRExpr* address = getIRegA(rN);
         IRExpr* data = unop(Iop_32to16, getIRegA(rT));
         storeGuardedLE( address, data, condT);
         IRExpr* newRn = binop(bU ? Iop_Add32 : Iop_Sub32,
                               getIRegA(rN), mkU32(imm8));
         putIRegA(rN, newRn, condT, Ijk_Boring);
         DIP("strht%s r%u, [r%u], #%c%u\n",
             nCC(INSN_COND), rT, rN, bU ? '+' : '-', imm8);
         goto decode_success;
      }
   }

   
   if ( (INSN(27,20) & BITS8(1,1,1,1,0,1,1,1)) == BITS8(0,0,0,0,0,0,1,0)
       && INSN(11,4) == BITS8(0,0,0,0,1,0,1,1) ) {
      UInt rT    = INSN(15,12);
      UInt rN    = INSN(19,16);
      UInt rM    = INSN(3,0);
      UInt bU    = INSN(23,23);
      Bool valid = True;
      if (rT == 15 || rN == 15 || rN == rT || rM == 15) valid = False;
      if (valid) {
         IRExpr* address = getIRegA(rN);
         IRExpr* data = unop(Iop_32to16, getIRegA(rT));
         storeGuardedLE( address, data, condT);
         IRExpr* newRn = binop(bU ? Iop_Add32 : Iop_Sub32,
                               getIRegA(rN), getIRegA(rM));
         putIRegA(rN, newRn, condT, Ijk_Boring);
         DIP("strht%s r%u, [r%u], %cr%u\n",
             nCC(INSN_COND), rT, rN, bU ? '+' : '-', rM);
         goto decode_success;
      }
   }

   
   if ( (INSN(27,20) & BITS8(1,1,1,1,0,1,1,1)) == BITS8(0,1,0,0,0,0,1,0) ) {
      UInt rT    = INSN(15,12);
      UInt rN    = INSN(19,16);
      UInt imm12 = INSN(11,0);
      UInt bU    = INSN(23,23);
      Bool valid = True;
      if (rN == 15 || rN == rT) valid = False;
      if (valid) {
         IRExpr* address = getIRegA(rN);
         storeGuardedLE( address, getIRegA(rT), condT);
         IRExpr* newRn = binop(bU ? Iop_Add32 : Iop_Sub32,
                               getIRegA(rN), mkU32(imm12));
         putIRegA(rN, newRn, condT, Ijk_Boring);
         DIP("strt%s r%u, [r%u], %c%u\n",
             nCC(INSN_COND), rT, rN, bU ? '+' : '-', imm12);
         goto decode_success;
      }
   }

   
   if ( (INSN(27,20) & BITS8(1,1,1,1,0,1,1,1)) == BITS8(0,1,1,0,0,0,1,0)
       && INSN(4,4) == 0 ) {
      UInt rT    = INSN(15,12);
      UInt rN    = INSN(19,16);
      UInt rM    = INSN(3,0);
      UInt type  = INSN(6,5);
      UInt imm5  = INSN(11,7);
      UInt bU    = INSN(23,23);
      Bool valid = True;
      if (rN == 15 || rN == rT || rM == 15) valid = False;
      if (valid) {
         storeGuardedLE( getIRegA(rN), getIRegA(rT), condT);
         
         IRExpr* erN = mk_EA_reg_plusminus_shifted_reg(rN, bU, rM,
                                                       type, imm5, dis_buf);
         putIRegA(rN, erN, condT, Ijk_Boring);
         DIP("strt%s r%u, %s\n", nCC(INSN_COND), rT, dis_buf);
         goto decode_success;
      }
   }

   
   
   

   
   if (0x0E1D0F70 == (insn & 0x0FFF0FFF)) {
      UInt rD = INSN(15,12);
      if (rD <= 14) {
         
         putIRegA(rD, IRExpr_Get(OFFB_TPIDRURO, Ity_I32),
                      condT, Ijk_Boring);
         DIP("mrc%s p15,0, r%u, c13, c0, 3\n", nCC(INSN_COND), rD);
         goto decode_success;
      }
      
   }

    
   if (0xEE070FBA == (insn & 0xFFFF0FFF)) {
      UInt rT = INSN(15,12);
      if (rT <= 14) {
         stmt( IRStmt_MBE(Imbe_Fence) );
         DIP("mcr 15, 0, r%u, c7, c10, 5 (data memory barrier)\n", rT);
         goto decode_success;
      }
      
   }
   
   switch (insn) {
      case 0xEE070F9A: 
         stmt( IRStmt_MBE(Imbe_Fence) );
         DIP("mcr 15, 0, r0, c7, c10, 4 (data synch barrier)\n");
         goto decode_success;
      case 0xEE070F95: 
         stmt( IRStmt_MBE(Imbe_Fence) );
         DIP("mcr 15, 0, r0, c7, c5, 4 (insn synch barrier)\n");
         goto decode_success;
      default:
         break;
   }

   
   
   

   if (INSN_COND != ARMCondNV) {
      Bool ok_vfp = decode_CP10_CP11_instruction (
                       &dres, INSN(27,0), condT, INSN_COND,
                       False
                    );
      if (ok_vfp)
         goto decode_success;
   }

   
   
   


   
   
   

   { Bool ok_v6m = decode_V6MEDIA_instruction(
                       &dres, INSN(27,0), condT, INSN_COND,
                       False
                   );
     if (ok_v6m)
        goto decode_success;
   }

   
   
   

   goto decode_failure;
   

  decode_failure:
   
   if (sigill_diag) {
      vex_printf("disInstr(arm): unhandled instruction: "
                 "0x%x\n", insn);
      vex_printf("                 cond=%d(0x%x) 27:20=%u(0x%02x) "
                                   "4:4=%d "
                                   "3:0=%u(0x%x)\n",
                 (Int)INSN_COND, (UInt)INSN_COND,
                 (Int)INSN(27,20), (UInt)INSN(27,20),
                 (Int)INSN(4,4),
                 (Int)INSN(3,0), (UInt)INSN(3,0) );
   }

   vassert(0 == (guest_R15_curr_instr_notENC & 3));
   llPutIReg( 15, mkU32(guest_R15_curr_instr_notENC) );
   dres.len         = 0;
   dres.whatNext    = Dis_StopHere;
   dres.jk_StopHere = Ijk_NoDecode;
   dres.continueAt  = 0;
   return dres;

  decode_success:
   
   DIP("\n");

   vassert(dres.len == 4 || dres.len == 20);

   
   if (r15written) {
      vassert(dres.whatNext == Dis_Continue);
      vassert(irsb->next == NULL);
      vassert(irsb->jumpkind == Ijk_Boring);
      /* If r15 is unconditionally written, terminate the block by
         jumping to it.  If it's conditionally written, still
         terminate the block (a shame, but we can't do side exits to
         arbitrary destinations), but first jump to the next
         instruction if the condition doesn't hold. */
      if (r15guard == IRTemp_INVALID) {
         
      } else {
         
         stmt( IRStmt_Exit(
                  unop(Iop_32to1,
                       binop(Iop_Xor32,
                             mkexpr(r15guard), mkU32(1))),
                  r15kind,
                  IRConst_U32(guest_R15_curr_instr_notENC + 4),
                  OFFB_R15T
         ));
      }
      llPutIReg(15, llGetIReg(15));
      dres.whatNext    = Dis_StopHere;
      dres.jk_StopHere = r15kind;
   } else {
      
      switch (dres.whatNext) {
         case Dis_Continue:
            llPutIReg(15, mkU32(dres.len + guest_R15_curr_instr_notENC));
            break;
         case Dis_ResteerU:
         case Dis_ResteerC:
            llPutIReg(15, mkU32(dres.continueAt));
            break;
         case Dis_StopHere:
            break;
         default:
            vassert(0);
      }
   }

   return dres;

#  undef INSN_COND
#  undef INSN
}



static const UChar it_length_table[256]; 



static   
DisResult disInstr_THUMB_WRK (
             Bool         (*resteerOkFn) ( void*, Addr ),
             Bool         resteerCisOk,
             void*        callback_opaque,
             const UChar* guest_instr,
             const VexArchInfo* archinfo,
             const VexAbiInfo*  abiinfo,
             Bool         sigill_diag
          )
{
#  define INSN0(_bMax,_bMin)  SLICE_UInt(((UInt)insn0), (_bMax), (_bMin))

   DisResult dres;
   UShort    insn0; 
   UShort    insn1; 
   
   
   HChar     dis_buf[128];  

   Bool guaranteedUnconditional = False;

   
   
   

   
   dres.whatNext    = Dis_Continue;
   dres.len         = 2;
   dres.continueAt  = 0;
   dres.jk_StopHere = Ijk_INVALID;

   r15written = False;
   r15guard   = IRTemp_INVALID; 
   r15kind    = Ijk_Boring;

   insn0 = getUShortLittleEndianly( guest_instr );
   insn1 = 0; 

   
   IRTemp old_itstate = IRTemp_INVALID;

   if (0) vex_printf("insn: 0x%x\n", insn0);

   DIP("\t(thumb) 0x%x:  ", (UInt)guest_R15_curr_instr_notENC);

   vassert(0 == (guest_R15_curr_instr_notENC & 1));

   
   
   {
      const UChar* code = guest_instr;
      UInt word1 = 0x0CFCEA4F;
      UInt word2 = 0x3C7CEA4F;
      UInt word3 = 0x7C7CEA4F;
      UInt word4 = 0x4CFCEA4F;
      if (getUIntLittleEndianly(code+ 0) == word1 &&
          getUIntLittleEndianly(code+ 4) == word2 &&
          getUIntLittleEndianly(code+ 8) == word3 &&
          getUIntLittleEndianly(code+12) == word4) {
         
         
         if (getUIntLittleEndianly(code+16) == 0x0A0AEA4A
                                               ) {
            
            DIP("r3 = client_request ( %%r4 )\n");
            llPutIReg(15, mkU32( (guest_R15_curr_instr_notENC + 20) | 1 ));
            dres.jk_StopHere = Ijk_ClientReq;
            dres.whatNext    = Dis_StopHere;
            goto decode_success;
         }
         else
         
         if (getUIntLittleEndianly(code+16) == 0x0B0BEA4B
                                               ) {
            
            DIP("r3 = guest_NRADDR\n");
            dres.len = 20;
            llPutIReg(3, IRExpr_Get( OFFB_NRADDR, Ity_I32 ));
            goto decode_success;
         }
         else
         
         if (getUIntLittleEndianly(code+16) == 0x0C0CEA4C
                                               ) {
            
            DIP("branch-and-link-to-noredir r4\n");
            llPutIReg(14, mkU32( (guest_R15_curr_instr_notENC + 20) | 1 ));
            llPutIReg(15, getIRegT(4));
            dres.jk_StopHere = Ijk_NoRedir;
            dres.whatNext    = Dis_StopHere;
            goto decode_success;
         }
         else
         
         if (getUIntLittleEndianly(code+16) == 0x0909EA49
                                               ) {
            
            DIP("IR injection\n");
            vex_inject_ir(irsb, Iend_LE);
            
            
            
            
            stmt(IRStmt_Put(OFFB_CMSTART, mkU32(guest_R15_curr_instr_notENC)));
            stmt(IRStmt_Put(OFFB_CMLEN,   mkU32(20)));
            llPutIReg(15, mkU32( (guest_R15_curr_instr_notENC + 20) | 1 ));
            dres.whatNext    = Dis_StopHere;
            dres.jk_StopHere = Ijk_InvalICache;
            goto decode_success;
         }
         insn0 = getUShortLittleEndianly(code+16);
         goto decode_failure;
         
      }

   }

   


   
   {
      vassert(guaranteedUnconditional == False);

      UInt pc = guest_R15_curr_instr_notENC;
      vassert(0 == (pc & 1));

      UInt pageoff = pc & 0xFFF;
      if (pageoff >= 18) {
         guaranteedUnconditional = True; 
         UShort* hwp = (UShort*)(HWord)pc;
         Int i;
         for (i = -1; i >= -9; i--) {
            UShort hwp_i = hwp[i];
            if (UNLIKELY((hwp_i & 0xFF00) == 0xBF00 && (hwp_i & 0xF) != 0)) {
               
               
               Int n_guarded = (Int)it_length_table[hwp_i & 0xFF];
               vassert(n_guarded >= 1 && n_guarded <= 4);
               if (n_guarded * 2 
                   > (-(i+1)))   
                  guaranteedUnconditional = False;
               break;
            }
         }
      }
   }
   



   IRTemp condT              = IRTemp_INVALID;
   IRTemp cond_AND_notInIT_T = IRTemp_INVALID;

   IRTemp new_itstate        = IRTemp_INVALID;
   vassert(old_itstate == IRTemp_INVALID);

   if (guaranteedUnconditional) {
      

      
      IRTemp z32 = newTemp(Ity_I32);
      assign(z32, mkU32(0));
      put_ITSTATE(z32);

      
      
      
      old_itstate = z32; 

      
      
      
      
      
      
      
      new_itstate = z32;

      
      
      

      
      
      
      
      
      
      
      
      
      
      
      
      
      
      

      
      
      
      
      
      
      
      
      
      
      
      
      
      
      condT = newTemp(Ity_I32);
      assign(condT, mkU32(1));

      
      
      
      
      
      
      
      
      
      

      
      
      
      
      
      
      
      cond_AND_notInIT_T = condT; 

      
   } else {
      

      old_itstate = get_ITSTATE();

      new_itstate = newTemp(Ity_I32);
      assign(new_itstate,
             binop(Iop_Shr32, mkexpr(old_itstate), mkU8(8)));

      put_ITSTATE(new_itstate);

      IRTemp condT1 = newTemp(Ity_I32);
      assign(condT1,
             mk_armg_calculate_condition_dyn(
                binop(Iop_Xor32,
                      binop(Iop_And32, mkexpr(old_itstate), mkU32(0xF0)),
                      mkU32(0xE0))
            )
      );

      condT = newTemp(Ity_I32);
      assign(condT, IRExpr_ITE(
                       binop(Iop_CmpNE32, binop(Iop_And32,
                                                mkexpr(old_itstate),
                                                mkU32(0xF0)),
                                          mkU32(0)),
                       mkexpr(condT1),
                       mkU32(1)
            ));

      IRTemp notInITt = newTemp(Ity_I32);
      assign(notInITt,
             binop(Iop_Xor32,
                   binop(Iop_And32, mkexpr(old_itstate), mkU32(1)),
                   mkU32(1)));

      cond_AND_notInIT_T = newTemp(Ity_I32);
      assign(cond_AND_notInIT_T,
             binop(Iop_And32, mkexpr(notInITt), mkexpr(condT)));
      
   }



   
   
   
   
   
   
   
   


   IROp   anOp   = Iop_INVALID;
   const HChar* anOpNm = NULL;

   

   switch (INSN0(15,6)) {

   case 0x10a:   
   case 0x10b: { 
      
      Bool   isCMN = INSN0(15,6) == 0x10b;
      UInt   rN    = INSN0(2,0);
      UInt   rM    = INSN0(5,3);
      IRTemp argL  = newTemp(Ity_I32);
      IRTemp argR  = newTemp(Ity_I32);
      assign( argL, getIRegT(rN) );
      assign( argR, getIRegT(rM) );
      
      setFlags_D1_D2( isCMN ? ARMG_CC_OP_ADD : ARMG_CC_OP_SUB,
                      argL, argR, condT );
      DIP("%s r%u, r%u\n", isCMN ? "cmn" : "cmp", rN, rM);
      goto decode_success;
   }

   case 0x108: {
      
      UInt   rN   = INSN0(2,0);
      UInt   rM   = INSN0(5,3);
      IRTemp oldC = newTemp(Ity_I32);
      IRTemp oldV = newTemp(Ity_I32);
      IRTemp res  = newTemp(Ity_I32);
      assign( oldC, mk_armg_calculate_flag_c() );
      assign( oldV, mk_armg_calculate_flag_v() );
      assign( res,  binop(Iop_And32, getIRegT(rN), getIRegT(rM)) );
      
      setFlags_D1_D2_ND( ARMG_CC_OP_LOGIC, res, oldC, oldV, condT );
      DIP("tst r%u, r%u\n", rN, rM);
      goto decode_success;
   }

   case 0x109: {
      
      
      UInt   rM   = INSN0(5,3);
      UInt   rD   = INSN0(2,0);
      IRTemp arg  = newTemp(Ity_I32);
      IRTemp zero = newTemp(Ity_I32);
      assign(arg, getIRegT(rM));
      assign(zero, mkU32(0));
      
      putIRegT(rD, binop(Iop_Sub32, mkexpr(zero), mkexpr(arg)), condT);
      setFlags_D1_D2( ARMG_CC_OP_SUB, zero, arg, cond_AND_notInIT_T);
      DIP("negs r%u, r%u\n", rD, rM);
      goto decode_success;
   }

   case 0x10F: {
      
      
      UInt   rM   = INSN0(5,3);
      UInt   rD   = INSN0(2,0);
      IRTemp oldV = newTemp(Ity_I32);
      IRTemp oldC = newTemp(Ity_I32);
      IRTemp res  = newTemp(Ity_I32);
      assign( oldV, mk_armg_calculate_flag_v() );
      assign( oldC, mk_armg_calculate_flag_c() );
      assign(res, unop(Iop_Not32, getIRegT(rM)));
      
      putIRegT(rD, mkexpr(res), condT);
      setFlags_D1_D2_ND( ARMG_CC_OP_LOGIC, res, oldC, oldV,
                         cond_AND_notInIT_T );
      DIP("mvns r%u, r%u\n", rD, rM);
      goto decode_success;
   }

   case 0x10C:
      
      anOp = Iop_Or32; anOpNm = "orr"; goto and_orr_eor_mul;
   case 0x100:
      
      anOp = Iop_And32; anOpNm = "and"; goto and_orr_eor_mul;
   case 0x101:
      
      anOp = Iop_Xor32; anOpNm = "eor"; goto and_orr_eor_mul;
   case 0x10d:
      
      anOp = Iop_Mul32; anOpNm = "mul"; goto and_orr_eor_mul;
   and_orr_eor_mul: {
      
      UInt   rM   = INSN0(5,3);
      UInt   rD   = INSN0(2,0);
      IRTemp res  = newTemp(Ity_I32);
      IRTemp oldV = newTemp(Ity_I32);
      IRTemp oldC = newTemp(Ity_I32);
      assign( oldV, mk_armg_calculate_flag_v() );
      assign( oldC, mk_armg_calculate_flag_c() );
      assign( res, binop(anOp, getIRegT(rD), getIRegT(rM) ));
      
      
      putIRegT(rD, mkexpr(res), condT);
      setFlags_D1_D2_ND( ARMG_CC_OP_LOGIC, res, oldC, oldV,
                         cond_AND_notInIT_T );
      DIP("%s r%u, r%u\n", anOpNm, rD, rM);
      goto decode_success;
   }

   case 0x10E: {
      
      
      UInt   rM   = INSN0(5,3);
      UInt   rD   = INSN0(2,0);
      IRTemp res  = newTemp(Ity_I32);
      IRTemp oldV = newTemp(Ity_I32);
      IRTemp oldC = newTemp(Ity_I32);
      assign( oldV, mk_armg_calculate_flag_v() );
      assign( oldC, mk_armg_calculate_flag_c() );
      assign( res, binop(Iop_And32, getIRegT(rD),
                                    unop(Iop_Not32, getIRegT(rM) )));
      
      
      putIRegT(rD, mkexpr(res), condT);
      setFlags_D1_D2_ND( ARMG_CC_OP_LOGIC, res, oldC, oldV,
                         cond_AND_notInIT_T );
      DIP("bics r%u, r%u\n", rD, rM);
      goto decode_success;
   }

   case 0x105: {
      
      
      UInt   rM   = INSN0(5,3);
      UInt   rD   = INSN0(2,0);
      IRTemp argL = newTemp(Ity_I32);
      IRTemp argR = newTemp(Ity_I32);
      IRTemp oldC = newTemp(Ity_I32);
      IRTemp res  = newTemp(Ity_I32);
      assign(argL, getIRegT(rD));
      assign(argR, getIRegT(rM));
      assign(oldC, mk_armg_calculate_flag_c());
      assign(res, binop(Iop_Add32,
                        binop(Iop_Add32, mkexpr(argL), mkexpr(argR)),
                        mkexpr(oldC)));
      
      putIRegT(rD, mkexpr(res), condT);
      setFlags_D1_D2_ND( ARMG_CC_OP_ADC, argL, argR, oldC,
                         cond_AND_notInIT_T );
      DIP("adcs r%u, r%u\n", rD, rM);
      goto decode_success;
   }

   case 0x106: {
      
      
      UInt   rM   = INSN0(5,3);
      UInt   rD   = INSN0(2,0);
      IRTemp argL = newTemp(Ity_I32);
      IRTemp argR = newTemp(Ity_I32);
      IRTemp oldC = newTemp(Ity_I32);
      IRTemp res  = newTemp(Ity_I32);
      assign(argL, getIRegT(rD));
      assign(argR, getIRegT(rM));
      assign(oldC, mk_armg_calculate_flag_c());
      assign(res, binop(Iop_Sub32,
                        binop(Iop_Sub32, mkexpr(argL), mkexpr(argR)),
                        binop(Iop_Xor32, mkexpr(oldC), mkU32(1))));
      
      putIRegT(rD, mkexpr(res), condT);
      setFlags_D1_D2_ND( ARMG_CC_OP_SBB, argL, argR, oldC,
                         cond_AND_notInIT_T );
      DIP("sbcs r%u, r%u\n", rD, rM);
      goto decode_success;
   }

   case 0x2CB: {
      
      
      UInt rM = INSN0(5,3);
      UInt rD = INSN0(2,0);
      putIRegT(rD, binop(Iop_And32, getIRegT(rM), mkU32(0xFF)),
                   condT);
      DIP("uxtb r%u, r%u\n", rD, rM);
      goto decode_success;
   }

   case 0x2C9: {
      
      
      UInt rM = INSN0(5,3);
      UInt rD = INSN0(2,0);
      putIRegT(rD, binop(Iop_Sar32,
                         binop(Iop_Shl32, getIRegT(rM), mkU8(24)),
                         mkU8(24)),
                   condT);
      DIP("sxtb r%u, r%u\n", rD, rM);
      goto decode_success;
   }

   case 0x2CA: {
      
      
      UInt rM = INSN0(5,3);
      UInt rD = INSN0(2,0);
      putIRegT(rD, binop(Iop_And32, getIRegT(rM), mkU32(0xFFFF)),
                   condT);
      DIP("uxth r%u, r%u\n", rD, rM);
      goto decode_success;
   }

   case 0x2C8: {
      
      
      UInt rM = INSN0(5,3);
      UInt rD = INSN0(2,0);
      putIRegT(rD, binop(Iop_Sar32,
                         binop(Iop_Shl32, getIRegT(rM), mkU8(16)),
                         mkU8(16)),
                   condT);
      DIP("sxth r%u, r%u\n", rD, rM);
      goto decode_success;
   }

   case 0x102:   
   case 0x103:   
   case 0x104:   
   case 0x107: { 
      
      
      
      
      
      UInt   rS   = INSN0(5,3);
      UInt   rD   = INSN0(2,0);
      IRTemp oldV = newTemp(Ity_I32);
      IRTemp rDt  = newTemp(Ity_I32);
      IRTemp rSt  = newTemp(Ity_I32);
      IRTemp res  = newTemp(Ity_I32);
      IRTemp resC = newTemp(Ity_I32);
      const HChar* wot  = "???";
      assign(rSt, getIRegT(rS));
      assign(rDt, getIRegT(rD));
      assign(oldV, mk_armg_calculate_flag_v());
      
      switch (INSN0(15,6)) {
         case 0x102:
            compute_result_and_C_after_LSL_by_reg(
               dis_buf, &res, &resC, rDt, rSt, rD, rS
            );
            wot = "lsl";
            break;
         case 0x103:
            compute_result_and_C_after_LSR_by_reg(
               dis_buf, &res, &resC, rDt, rSt, rD, rS
            );
            wot = "lsr";
            break;
         case 0x104:
            compute_result_and_C_after_ASR_by_reg(
               dis_buf, &res, &resC, rDt, rSt, rD, rS
            );
            wot = "asr";
            break;
         case 0x107:
            compute_result_and_C_after_ROR_by_reg(
               dis_buf, &res, &resC, rDt, rSt, rD, rS
            );
            wot = "ror";
            break;
         default:
            vassert(0);
      }
      
      putIRegT(rD, mkexpr(res), condT);
      setFlags_D1_D2_ND( ARMG_CC_OP_LOGIC, res, resC, oldV,
                         cond_AND_notInIT_T );
      DIP("%ss r%u, r%u\n", wot, rS, rD);
      goto decode_success;
   }

   case 0x2E8:   
   case 0x2E9: { 
      
      
      UInt rM = INSN0(5,3);
      UInt rD = INSN0(2,0);
      Bool isREV = INSN0(15,6) == 0x2E8;
      IRTemp arg = newTemp(Ity_I32);
      assign(arg, getIRegT(rM));
      IRTemp res = isREV ? gen_REV(arg) : gen_REV16(arg);
      putIRegT(rD, mkexpr(res), condT);
      DIP("rev%s r%u, r%u\n", isREV ? "" : "16", rD, rM);
      goto decode_success;
   }

   case 0x2EB: { 
      
      UInt rM = INSN0(5,3);
      UInt rD = INSN0(2,0);
      IRTemp irt_rM  = newTemp(Ity_I32);
      IRTemp irt_hi  = newTemp(Ity_I32);
      IRTemp irt_low = newTemp(Ity_I32);
      IRTemp irt_res = newTemp(Ity_I32);
      assign(irt_rM, getIRegT(rM));
      assign(irt_hi,
             binop(Iop_Sar32,
                   binop(Iop_Shl32, mkexpr(irt_rM), mkU8(24)),
                   mkU8(16)
             )
      );
      assign(irt_low,
             binop(Iop_And32,
                   binop(Iop_Shr32, mkexpr(irt_rM), mkU8(8)),
                   mkU32(0xFF)
             )
      );
      assign(irt_res,
             binop(Iop_Or32, mkexpr(irt_hi), mkexpr(irt_low))
      );
      putIRegT(rD, mkexpr(irt_res), condT);
      DIP("revsh r%u, r%u\n", rD, rM);
      goto decode_success;
   }

   default:
      break; 

   }


   

   switch (INSN0(15,7)) {

   case BITS9(1,0,1,1,0,0,0,0,0): {
      
      UInt uimm7 = INSN0(6,0);
      putIRegT(13, binop(Iop_Add32, getIRegT(13), mkU32(uimm7 * 4)),
                   condT);
      DIP("add sp, #%u\n", uimm7 * 4);
      goto decode_success;
   }

   case BITS9(1,0,1,1,0,0,0,0,1): {
      
      UInt uimm7 = INSN0(6,0);
      putIRegT(13, binop(Iop_Sub32, getIRegT(13), mkU32(uimm7 * 4)),
                   condT);
      DIP("sub sp, #%u\n", uimm7 * 4);
      goto decode_success;
   }

   case BITS9(0,1,0,0,0,1,1,1,0): {
      
      UInt rM = (INSN0(6,6) << 3) | INSN0(5,3);
      if (BITS3(0,0,0) == INSN0(2,0)) {
         IRTemp dst = newTemp(Ity_I32);
         gen_SIGILL_T_if_in_but_NLI_ITBlock(old_itstate, new_itstate);
         mk_skip_over_T16_if_cond_is_false(condT);
         condT = IRTemp_INVALID;
         
         if (rM <= 14) {
            assign( dst, getIRegT(rM) );
         } else {
            vassert(rM == 15);
            assign( dst, mkU32(guest_R15_curr_instr_notENC + 4) );
         }
         llPutIReg(15, mkexpr(dst));
         dres.jk_StopHere = rM == 14 ? Ijk_Ret : Ijk_Boring;
         dres.whatNext    = Dis_StopHere;
         DIP("bx r%u (possibly switch to ARM mode)\n", rM);
         goto decode_success;
      }
      break;
   }

   
   
   case BITS9(0,1,0,0,0,1,1,1,1): {
      if (BITS3(0,0,0) == INSN0(2,0)) {
         UInt rM = (INSN0(6,6) << 3) | INSN0(5,3);
         IRTemp dst = newTemp(Ity_I32);
         if (rM <= 14) {
            gen_SIGILL_T_if_in_but_NLI_ITBlock(old_itstate, new_itstate);
            mk_skip_over_T16_if_cond_is_false(condT);
            condT = IRTemp_INVALID;
            
            
            assign( dst, getIRegT(rM) );
            putIRegT( 14, mkU32( (guest_R15_curr_instr_notENC + 2) | 1 ),
                          IRTemp_INVALID );
            llPutIReg(15, mkexpr(dst));
            dres.jk_StopHere = Ijk_Call;
            dres.whatNext    = Dis_StopHere;
            DIP("blx r%u (possibly switch to ARM mode)\n", rM);
            goto decode_success;
         }
         
      }
      break;
   }

   default:
      break; 

   }


   

   switch (INSN0(15,8)) {

   case BITS8(1,1,0,1,1,1,1,1): {
      
      UInt imm8 = INSN0(7,0);
      if (imm8 == 0) {
         
         mk_skip_over_T16_if_cond_is_false( condT );
         
         
         
         
         
         
         llPutIReg(15, mkU32( (guest_R15_curr_instr_notENC + 2) | 1 ));
         dres.jk_StopHere = Ijk_Sys_syscall;
         dres.whatNext    = Dis_StopHere;
         DIP("svc #0x%08x\n", imm8);
         goto decode_success;
      }
      
      break;
   }

   case BITS8(0,1,0,0,0,1,0,0): {
      
      UInt h1 = INSN0(7,7);
      UInt h2 = INSN0(6,6);
      UInt rM = (h2 << 3) | INSN0(5,3);
      UInt rD = (h1 << 3) | INSN0(2,0);
      
      if (rD == 15 && rM == 15) {
         
      } else {
         IRTemp res = newTemp(Ity_I32);
         assign( res, binop(Iop_Add32, getIRegT(rD), getIRegT(rM) ));
         if (rD != 15) {
            putIRegT( rD, mkexpr(res), condT );
         } else {
            
            gen_SIGILL_T_if_in_but_NLI_ITBlock(old_itstate, new_itstate);
            
            mk_skip_over_T16_if_cond_is_false(condT);
            condT = IRTemp_INVALID;
            
            
            llPutIReg(15, binop(Iop_Or32, mkexpr(res), mkU32(1)));
            dres.jk_StopHere = Ijk_Boring;
            dres.whatNext    = Dis_StopHere;
         }
         DIP("add(hi) r%u, r%u\n", rD, rM);
         goto decode_success;
      }
      break;
   }

   case BITS8(0,1,0,0,0,1,0,1): {
      
      UInt h1 = INSN0(7,7);
      UInt h2 = INSN0(6,6);
      UInt rM = (h2 << 3) | INSN0(5,3);
      UInt rN = (h1 << 3) | INSN0(2,0);
      if (h1 != 0 || h2 != 0) {
         IRTemp argL  = newTemp(Ity_I32);
         IRTemp argR  = newTemp(Ity_I32);
         assign( argL, getIRegT(rN) );
         assign( argR, getIRegT(rM) );
         
         setFlags_D1_D2( ARMG_CC_OP_SUB, argL, argR, condT );
         DIP("cmphi r%u, r%u\n", rN, rM);
         goto decode_success;
      }
      break;
   }

   case BITS8(0,1,0,0,0,1,1,0): {
      
      UInt h1 = INSN0(7,7);
      UInt h2 = INSN0(6,6);
      UInt rM = (h2 << 3) | INSN0(5,3);
      UInt rD = (h1 << 3) | INSN0(2,0);
      if (1 ) {
         IRTemp val = newTemp(Ity_I32);
         assign( val, getIRegT(rM) );
         if (rD != 15) {
            putIRegT( rD, mkexpr(val), condT );
         } else {
            
            gen_SIGILL_T_if_in_but_NLI_ITBlock(old_itstate, new_itstate);
            
            mk_skip_over_T16_if_cond_is_false(condT);
            condT = IRTemp_INVALID;
            
            
            llPutIReg(15, binop(Iop_Or32, mkexpr(val), mkU32(1)));
            dres.jk_StopHere = rM == 14 ? Ijk_Ret : Ijk_Boring;
            dres.whatNext    = Dis_StopHere;
         }
         DIP("mov r%u, r%u\n", rD, rM);
         goto decode_success;
      }
      break;
   }

   case BITS8(1,0,1,1,1,1,1,1): {
      
      UInt firstcond = INSN0(7,4);
      UInt mask = INSN0(3,0);
      UInt newITSTATE = 0;
      HChar c1 = '.';
      HChar c2 = '.';
      HChar c3 = '.';
      Bool valid = compute_ITSTATE( &newITSTATE, &c1, &c2, &c3,
                                    firstcond, mask );
      if (valid && firstcond != 0xF) {
         
         gen_SIGILL_T_if_in_ITBlock(old_itstate, new_itstate);

         IRTemp t = newTemp(Ity_I32);
         assign(t, mkU32(newITSTATE));
         put_ITSTATE(t);

         DIP("it%c%c%c %s\n", c1, c2, c3, nCC(firstcond));
         goto decode_success;
      }
      break;
   }

   case BITS8(1,0,1,1,0,0,0,1):
   case BITS8(1,0,1,1,0,0,1,1):
   case BITS8(1,0,1,1,1,0,0,1):
   case BITS8(1,0,1,1,1,0,1,1): {
      
      UInt rN    = INSN0(2,0);
      UInt bOP   = INSN0(11,11);
      UInt imm32 = (INSN0(9,9) << 6) | (INSN0(7,3) << 1);
      gen_SIGILL_T_if_in_ITBlock(old_itstate, new_itstate);
      
      IRTemp kond = newTemp(Ity_I1);
      assign( kond, binop(bOP ? Iop_CmpNE32 : Iop_CmpEQ32,
                          getIRegT(rN), mkU32(0)) );

      vassert(0 == (guest_R15_curr_instr_notENC & 1));
      UInt dst = (guest_R15_curr_instr_notENC + 4 + imm32) | 1;
      stmt(IRStmt_Exit( mkexpr(kond),
                        Ijk_Boring,
                        IRConst_U32(toUInt(dst)),
                        OFFB_R15T ));
      DIP("cb%s r%u, 0x%x\n", bOP ? "nz" : "z", rN, dst - 1);
      goto decode_success;
   }

   default:
      break; 

   }


   

   switch (INSN0(15,9)) {

   case BITS7(1,0,1,1,0,1,0): {
      
      Int  i, nRegs;
      UInt bitR    = INSN0(8,8);
      UInt regList = INSN0(7,0);
      if (bitR) regList |= (1 << 14);
   
      if (regList != 0) {
         mk_skip_over_T16_if_cond_is_false(condT);
         condT = IRTemp_INVALID;
         put_ITSTATE(old_itstate);
         

         nRegs = 0;
         for (i = 0; i < 16; i++) {
            if ((regList & (1 << i)) != 0)
               nRegs++;
         }
         vassert(nRegs >= 1 && nRegs <= 9);

         IRTemp newSP = newTemp(Ity_I32);
         assign(newSP, binop(Iop_Sub32, getIRegT(13), mkU32(4 * nRegs)));
         putIRegT(13, mkexpr(newSP), IRTemp_INVALID);

         IRTemp base = newTemp(Ity_I32);
         assign(base, binop(Iop_And32, mkexpr(newSP), mkU32(~3)));

         
         nRegs = 0;
         for (i = 0; i < 16; i++) {
            if ((regList & (1 << i)) != 0) {
               storeLE( binop(Iop_Add32, mkexpr(base), mkU32(4 * nRegs)),
                        getIRegT(i) );
               nRegs++;
            }
         }

         
         put_ITSTATE(new_itstate);

         DIP("push {%s0x%04x}\n", bitR ? "lr," : "", regList & 0xFF);
         goto decode_success;
      }
      break;
   }

   case BITS7(1,0,1,1,1,1,0): {
      
      Int  i, nRegs;
      UInt bitR    = INSN0(8,8);
      UInt regList = INSN0(7,0);
   
      if (regList != 0 || bitR) {
         mk_skip_over_T16_if_cond_is_false(condT);
         condT = IRTemp_INVALID;
         put_ITSTATE(old_itstate);
         

         nRegs = 0;
         for (i = 0; i < 8; i++) {
            if ((regList & (1 << i)) != 0)
               nRegs++;
         }
         vassert(nRegs >= 0 && nRegs <= 8);
         vassert(bitR == 0 || bitR == 1);

         IRTemp oldSP = newTemp(Ity_I32);
         assign(oldSP, getIRegT(13));

         IRTemp base = newTemp(Ity_I32);
         assign(base, binop(Iop_And32, mkexpr(oldSP), mkU32(~3)));

         IRTemp newSP = newTemp(Ity_I32);
         assign(newSP, binop(Iop_Add32, mkexpr(oldSP),
                                        mkU32(4 * (nRegs + bitR))));

         
         nRegs = 0;
         for (i = 0; i < 8; i++) {
            if ((regList & (1 << i)) != 0) {
               putIRegT(i, loadLE( Ity_I32,
                                   binop(Iop_Add32, mkexpr(base),
                                                    mkU32(4 * nRegs))),
                           IRTemp_INVALID );
               nRegs++;
            }
         }

         IRTemp newPC = IRTemp_INVALID;
         if (bitR) {
            newPC = newTemp(Ity_I32);
            assign( newPC, loadLE( Ity_I32,
                                   binop(Iop_Add32, mkexpr(base),
                                                    mkU32(4 * nRegs))));
         }

         
         putIRegT(13, mkexpr(newSP), IRTemp_INVALID);

         
         put_ITSTATE(new_itstate);

         if (bitR) {
            llPutIReg(15, mkexpr(newPC));
            dres.jk_StopHere = Ijk_Ret;
            dres.whatNext    = Dis_StopHere;
         }

         DIP("pop {%s0x%04x}\n", bitR ? "pc," : "", regList & 0xFF);
         goto decode_success;
      }
      break;
   }

   case BITS7(0,0,0,1,1,1,0):   
   case BITS7(0,0,0,1,1,1,1): { 
      
      
      UInt   uimm3 = INSN0(8,6);
      UInt   rN    = INSN0(5,3);
      UInt   rD    = INSN0(2,0);
      UInt   isSub = INSN0(9,9);
      IRTemp argL  = newTemp(Ity_I32);
      IRTemp argR  = newTemp(Ity_I32);
      assign( argL, getIRegT(rN) );
      assign( argR, mkU32(uimm3) );
      putIRegT(rD, binop(isSub ? Iop_Sub32 : Iop_Add32,
                         mkexpr(argL), mkexpr(argR)),
                   condT);
      setFlags_D1_D2( isSub ? ARMG_CC_OP_SUB : ARMG_CC_OP_ADD,
                      argL, argR, cond_AND_notInIT_T );
      DIP("%s r%u, r%u, #%u\n", isSub ? "subs" : "adds", rD, rN, uimm3);
      goto decode_success;
   }

   case BITS7(0,0,0,1,1,0,0):   
   case BITS7(0,0,0,1,1,0,1): { 
      
      
      UInt   rM    = INSN0(8,6);
      UInt   rN    = INSN0(5,3);
      UInt   rD    = INSN0(2,0);
      UInt   isSub = INSN0(9,9);
      IRTemp argL  = newTemp(Ity_I32);
      IRTemp argR  = newTemp(Ity_I32);
      assign( argL, getIRegT(rN) );
      assign( argR, getIRegT(rM) );
      putIRegT( rD, binop(isSub ? Iop_Sub32 : Iop_Add32,
                          mkexpr(argL), mkexpr(argR)),
                    condT );
      setFlags_D1_D2( isSub ? ARMG_CC_OP_SUB : ARMG_CC_OP_ADD,
                      argL, argR, cond_AND_notInIT_T );
      DIP("%s r%u, r%u, r%u\n", isSub ? "subs" : "adds", rD, rN, rM);
      goto decode_success;
   }

   case BITS7(0,1,0,1,0,0,0):   
   case BITS7(0,1,0,1,1,0,0): { 
      
      
      
      UInt    rD   = INSN0(2,0);
      UInt    rN   = INSN0(5,3);
      UInt    rM   = INSN0(8,6);
      UInt    isLD = INSN0(11,11);

      IRExpr* ea = binop(Iop_Add32, getIRegT(rN), getIRegT(rM));
      put_ITSTATE(old_itstate); 
      if (isLD) {
         IRTemp tD = newTemp(Ity_I32);
         loadGuardedLE( tD, ILGop_Ident32, ea, llGetIReg(rD), condT );
         putIRegT(rD, mkexpr(tD), IRTemp_INVALID);
      } else {
         storeGuardedLE(ea, getIRegT(rD), condT);
      }
      put_ITSTATE(new_itstate); 

      DIP("%s r%u, [r%u, r%u]\n", isLD ? "ldr" : "str", rD, rN, rM);
      goto decode_success;
   }

   case BITS7(0,1,0,1,0,0,1):
   case BITS7(0,1,0,1,1,0,1): {
      
      
      
      UInt    rD   = INSN0(2,0);
      UInt    rN   = INSN0(5,3);
      UInt    rM   = INSN0(8,6);
      UInt    isLD = INSN0(11,11);

      IRExpr* ea = binop(Iop_Add32, getIRegT(rN), getIRegT(rM));
      put_ITSTATE(old_itstate); 
      if (isLD) {
         IRTemp tD = newTemp(Ity_I32);
         loadGuardedLE(tD, ILGop_16Uto32, ea, llGetIReg(rD), condT);
         putIRegT(rD, mkexpr(tD), IRTemp_INVALID);
      } else {
         storeGuardedLE( ea, unop(Iop_32to16, getIRegT(rD)), condT );
      }
      put_ITSTATE(new_itstate); 

      DIP("%sh r%u, [r%u, r%u]\n", isLD ? "ldr" : "str", rD, rN, rM);
      goto decode_success;
   }

   case BITS7(0,1,0,1,1,1,1): {
      
      
      UInt    rD = INSN0(2,0);
      UInt    rN = INSN0(5,3);
      UInt    rM = INSN0(8,6);

      IRExpr* ea = binop(Iop_Add32, getIRegT(rN), getIRegT(rM));
      put_ITSTATE(old_itstate); 
      IRTemp tD = newTemp(Ity_I32);
      loadGuardedLE(tD, ILGop_16Sto32, ea, llGetIReg(rD), condT);
      putIRegT(rD, mkexpr(tD), IRTemp_INVALID);
      put_ITSTATE(new_itstate); 

      DIP("ldrsh r%u, [r%u, r%u]\n", rD, rN, rM);
      goto decode_success;
   }

   case BITS7(0,1,0,1,0,1,1): {
      
      
      UInt    rD = INSN0(2,0);
      UInt    rN = INSN0(5,3);
      UInt    rM = INSN0(8,6);

      IRExpr* ea = binop(Iop_Add32, getIRegT(rN), getIRegT(rM));
      put_ITSTATE(old_itstate); 
      IRTemp tD = newTemp(Ity_I32);
      loadGuardedLE(tD, ILGop_8Sto32, ea, llGetIReg(rD), condT);
      putIRegT(rD, mkexpr(tD), IRTemp_INVALID);
      put_ITSTATE(new_itstate); 

      DIP("ldrsb r%u, [r%u, r%u]\n", rD, rN, rM);
      goto decode_success;
   }

   case BITS7(0,1,0,1,0,1,0):
   case BITS7(0,1,0,1,1,1,0): {
      
      
      
      UInt    rD   = INSN0(2,0);
      UInt    rN   = INSN0(5,3);
      UInt    rM   = INSN0(8,6);
      UInt    isLD = INSN0(11,11);

      IRExpr* ea = binop(Iop_Add32, getIRegT(rN), getIRegT(rM));
      put_ITSTATE(old_itstate); 
      if (isLD) {
         IRTemp tD = newTemp(Ity_I32);
         loadGuardedLE(tD, ILGop_8Uto32, ea, llGetIReg(rD), condT);
         putIRegT(rD, mkexpr(tD), IRTemp_INVALID);
      } else {
         storeGuardedLE( ea, unop(Iop_32to8, getIRegT(rD)), condT );
      }
      put_ITSTATE(new_itstate); 

      DIP("%sb r%u, [r%u, r%u]\n", isLD ? "ldr" : "str", rD, rN, rM);
      goto decode_success;
   }

   default:
      break; 

   }


   

   switch (INSN0(15,11)) {

   case BITS5(0,0,1,1,0):
   case BITS5(0,0,1,1,1): {
      
      
      UInt   isSub = INSN0(11,11);
      UInt   rN    = INSN0(10,8);
      UInt   uimm8 = INSN0(7,0);
      IRTemp argL  = newTemp(Ity_I32);
      IRTemp argR  = newTemp(Ity_I32);
      assign( argL, getIRegT(rN) );
      assign( argR, mkU32(uimm8) );
      putIRegT( rN, binop(isSub ? Iop_Sub32 : Iop_Add32,
                          mkexpr(argL), mkexpr(argR)), condT );
      setFlags_D1_D2( isSub ? ARMG_CC_OP_SUB : ARMG_CC_OP_ADD,
                      argL, argR, cond_AND_notInIT_T );
      DIP("%s r%u, #%u\n", isSub ? "subs" : "adds", rN, uimm8);
      goto decode_success;
   }

   case BITS5(1,0,1,0,0): {
      
      
      
      UInt rD   = INSN0(10,8);
      UInt imm8 = INSN0(7,0);
      putIRegT(rD, binop(Iop_Add32, 
                         binop(Iop_And32, getIRegT(15), mkU32(~3U)),
                         mkU32(imm8 * 4)),
                   condT);
      DIP("add r%u, pc, #%u\n", rD, imm8 * 4);
      goto decode_success;
   }

   case BITS5(1,0,1,0,1): {
      
      UInt rD   = INSN0(10,8);
      UInt imm8 = INSN0(7,0);
      putIRegT(rD, binop(Iop_Add32, getIRegT(13), mkU32(imm8 * 4)),
                   condT);
      DIP("add r%u, r13, #%u\n", rD, imm8 * 4);
      goto decode_success;
   }

   case BITS5(0,0,1,0,1): {
      
      UInt   rN    = INSN0(10,8);
      UInt   uimm8 = INSN0(7,0);
      IRTemp argL  = newTemp(Ity_I32);
      IRTemp argR  = newTemp(Ity_I32);
      assign( argL, getIRegT(rN) );
      assign( argR, mkU32(uimm8) );
      
      setFlags_D1_D2( ARMG_CC_OP_SUB, argL, argR, condT );
      DIP("cmp r%u, #%u\n", rN, uimm8);
      goto decode_success;
   }

   case BITS5(0,0,1,0,0): {
      
      UInt   rD    = INSN0(10,8);
      UInt   uimm8 = INSN0(7,0);
      IRTemp oldV  = newTemp(Ity_I32);
      IRTemp oldC  = newTemp(Ity_I32);
      IRTemp res   = newTemp(Ity_I32);
      assign( oldV, mk_armg_calculate_flag_v() );
      assign( oldC, mk_armg_calculate_flag_c() );
      assign( res, mkU32(uimm8) );
      putIRegT(rD, mkexpr(res), condT);
      setFlags_D1_D2_ND( ARMG_CC_OP_LOGIC, res, oldC, oldV,
                         cond_AND_notInIT_T );
      DIP("movs r%u, #%u\n", rD, uimm8);
      goto decode_success;
   }

   case BITS5(0,1,0,0,1): {
      
      
      UInt   rD   = INSN0(10,8);
      UInt   imm8 = INSN0(7,0);
      IRTemp ea   = newTemp(Ity_I32);

      assign(ea, binop(Iop_Add32, 
                       binop(Iop_And32, getIRegT(15), mkU32(~3U)),
                       mkU32(imm8 * 4)));
      put_ITSTATE(old_itstate); 
      IRTemp tD = newTemp(Ity_I32);
      loadGuardedLE( tD, ILGop_Ident32, mkexpr(ea), llGetIReg(rD), condT );
      putIRegT(rD, mkexpr(tD), IRTemp_INVALID);
      put_ITSTATE(new_itstate); 

      DIP("ldr r%u, [pc, #%u]\n", rD, imm8 * 4);
      goto decode_success;
   }

   case BITS5(0,1,1,0,0):   
   case BITS5(0,1,1,0,1): { 
      
      
      
      UInt    rD   = INSN0(2,0);
      UInt    rN   = INSN0(5,3);
      UInt    imm5 = INSN0(10,6);
      UInt    isLD = INSN0(11,11);

      IRExpr* ea = binop(Iop_Add32, getIRegT(rN), mkU32(imm5 * 4));
      put_ITSTATE(old_itstate); 
      if (isLD) {
         IRTemp tD = newTemp(Ity_I32);
         loadGuardedLE( tD, ILGop_Ident32, ea, llGetIReg(rD), condT );
         putIRegT(rD, mkexpr(tD), IRTemp_INVALID);
      } else {
         storeGuardedLE( ea, getIRegT(rD), condT );
      }
      put_ITSTATE(new_itstate); 

      DIP("%s r%u, [r%u, #%u]\n", isLD ? "ldr" : "str", rD, rN, imm5 * 4);
      goto decode_success;
   }

   case BITS5(1,0,0,0,0):   
   case BITS5(1,0,0,0,1): { 
      
      
      
      UInt    rD   = INSN0(2,0);
      UInt    rN   = INSN0(5,3);
      UInt    imm5 = INSN0(10,6);
      UInt    isLD = INSN0(11,11);

      IRExpr* ea = binop(Iop_Add32, getIRegT(rN), mkU32(imm5 * 2));
      put_ITSTATE(old_itstate); 
      if (isLD) {
         IRTemp tD = newTemp(Ity_I32);
         loadGuardedLE( tD, ILGop_16Uto32, ea, llGetIReg(rD), condT );
         putIRegT(rD, mkexpr(tD), IRTemp_INVALID);
      } else {
         storeGuardedLE( ea, unop(Iop_32to16, getIRegT(rD)), condT );
      }
      put_ITSTATE(new_itstate); 

      DIP("%sh r%u, [r%u, #%u]\n", isLD ? "ldr" : "str", rD, rN, imm5 * 2);
      goto decode_success;
   }

   case BITS5(0,1,1,1,0):   
   case BITS5(0,1,1,1,1): { 
      
      
      
      UInt    rD   = INSN0(2,0);
      UInt    rN   = INSN0(5,3);
      UInt    imm5 = INSN0(10,6);
      UInt    isLD = INSN0(11,11);

      IRExpr* ea = binop(Iop_Add32, getIRegT(rN), mkU32(imm5));
      put_ITSTATE(old_itstate); 
      if (isLD) {
         IRTemp tD = newTemp(Ity_I32);
         loadGuardedLE( tD, ILGop_8Uto32, ea, llGetIReg(rD), condT );
         putIRegT(rD, mkexpr(tD), IRTemp_INVALID);
      } else {
         storeGuardedLE( ea, unop(Iop_32to8, getIRegT(rD)), condT );
      }
      put_ITSTATE(new_itstate); 

      DIP("%sb r%u, [r%u, #%u]\n", isLD ? "ldr" : "str", rD, rN, imm5);
      goto decode_success;
   }

   case BITS5(1,0,0,1,0):   
   case BITS5(1,0,0,1,1): { 
      
      
      
      UInt rD    = INSN0(10,8);
      UInt imm8  = INSN0(7,0);
      UInt isLD  = INSN0(11,11);

      IRExpr* ea = binop(Iop_Add32, getIRegT(13), mkU32(imm8 * 4));
      put_ITSTATE(old_itstate); 
      if (isLD) {
         IRTemp tD = newTemp(Ity_I32);
         loadGuardedLE( tD, ILGop_Ident32, ea, llGetIReg(rD), condT );
         putIRegT(rD, mkexpr(tD), IRTemp_INVALID);
      } else {
         storeGuardedLE(ea, getIRegT(rD), condT);
      }
      put_ITSTATE(new_itstate); 

      DIP("%s r%u, [sp, #%u]\n", isLD ? "ldr" : "str", rD, imm8 * 4);
      goto decode_success;
   }

   case BITS5(1,1,0,0,1): {
      
      Int i, nRegs = 0;
      UInt rN   = INSN0(10,8);
      UInt list = INSN0(7,0);
      
      if (list != 0) {
         mk_skip_over_T16_if_cond_is_false(condT);
         condT = IRTemp_INVALID;
         put_ITSTATE(old_itstate);
         

         IRTemp oldRn = newTemp(Ity_I32);
         IRTemp base  = newTemp(Ity_I32);
         assign(oldRn, getIRegT(rN));
         assign(base, binop(Iop_And32, mkexpr(oldRn), mkU32(~3U)));
         for (i = 0; i < 8; i++) {
            if (0 == (list & (1 << i)))
               continue;
            nRegs++;
            putIRegT(
               i, loadLE(Ity_I32,
                         binop(Iop_Add32, mkexpr(base),
                                          mkU32(nRegs * 4 - 4))),
               IRTemp_INVALID
            );
         }
         if (0 == (list & (1 << rN))) {
            putIRegT(rN,
                     binop(Iop_Add32, mkexpr(oldRn),
                                      mkU32(nRegs * 4)),
                     IRTemp_INVALID
            );
         }

         
         put_ITSTATE(new_itstate);

         DIP("ldmia r%u!, {0x%04x}\n", rN, list);
         goto decode_success;
      }
      break;
   }

   case BITS5(1,1,0,0,0): {
      
      Int i, nRegs = 0;
      UInt rN   = INSN0(10,8);
      UInt list = INSN0(7,0);
      Bool valid = list != 0;
      if (valid && 0 != (list & (1 << rN))) {
         for (i = 0; i < rN; i++) {
            if (0 != (list & (1 << i)))
               valid = False;
         }
      }
      if (valid) {
         mk_skip_over_T16_if_cond_is_false(condT);
         condT = IRTemp_INVALID;
         put_ITSTATE(old_itstate);
         

         IRTemp oldRn = newTemp(Ity_I32);
         IRTemp base = newTemp(Ity_I32);
         assign(oldRn, getIRegT(rN));
         assign(base, binop(Iop_And32, mkexpr(oldRn), mkU32(~3U)));
         for (i = 0; i < 8; i++) {
            if (0 == (list & (1 << i)))
               continue;
            nRegs++;
            storeLE( binop(Iop_Add32, mkexpr(base), mkU32(nRegs * 4 - 4)),
                     getIRegT(i) );
         }
         
         putIRegT(rN,
                  binop(Iop_Add32, mkexpr(oldRn),
                                   mkU32(nRegs * 4)),
                  IRTemp_INVALID);

         
         put_ITSTATE(new_itstate);

         DIP("stmia r%u!, {0x%04x}\n", rN, list);
         goto decode_success;
      }
      break;
   }

   case BITS5(0,0,0,0,0):   
   case BITS5(0,0,0,0,1):   
   case BITS5(0,0,0,1,0): { 
      
      
      
      UInt   rD   = INSN0(2,0);
      UInt   rM   = INSN0(5,3);
      UInt   imm5 = INSN0(10,6);
      IRTemp res  = newTemp(Ity_I32);
      IRTemp resC = newTemp(Ity_I32);
      IRTemp rMt  = newTemp(Ity_I32);
      IRTemp oldV = newTemp(Ity_I32);
      const HChar* wot  = "???";
      assign(rMt, getIRegT(rM));
      assign(oldV, mk_armg_calculate_flag_v());
      switch (INSN0(15,11)) {
         case BITS5(0,0,0,0,0):
            compute_result_and_C_after_LSL_by_imm5(
               dis_buf, &res, &resC, rMt, imm5, rM
            );
            wot = "lsl";
            break;
         case BITS5(0,0,0,0,1):
            compute_result_and_C_after_LSR_by_imm5(
               dis_buf, &res, &resC, rMt, imm5, rM
            );
            wot = "lsr";
            break;
         case BITS5(0,0,0,1,0):
            compute_result_and_C_after_ASR_by_imm5(
               dis_buf, &res, &resC, rMt, imm5, rM
            );
            wot = "asr";
            break;
         default:
            vassert(0);
      }
      
      putIRegT(rD, mkexpr(res), condT);
      setFlags_D1_D2_ND( ARMG_CC_OP_LOGIC, res, resC, oldV,
                         cond_AND_notInIT_T );
      
      DIP("%ss r%u, r%u, #%u\n", wot, rD, rM, imm5);
      goto decode_success;
   }

   case BITS5(1,1,1,0,0): {
      
      Int  simm11 = INSN0(10,0);
           simm11 = (simm11 << 21) >> 20;
      UInt dst    = simm11 + guest_R15_curr_instr_notENC + 4;
      
      gen_SIGILL_T_if_in_but_NLI_ITBlock(old_itstate, new_itstate);
      
      
      mk_skip_over_T16_if_cond_is_false(condT);
      condT = IRTemp_INVALID;
      
      llPutIReg(15, mkU32( dst | 1  ));
      dres.jk_StopHere = Ijk_Boring;
      dres.whatNext    = Dis_StopHere;
      DIP("b 0x%x\n", dst);
      goto decode_success;
   }

   default:
      break; 

   }


   

   switch (INSN0(15,12)) {

   case BITS4(1,1,0,1): {
      
      UInt cond  = INSN0(11,8);
      Int  simm8 = INSN0(7,0);
           simm8 = (simm8 << 24) >> 23;
      UInt dst   = simm8 + guest_R15_curr_instr_notENC + 4;
      if (cond != ARMCondAL && cond != ARMCondNV) {
         
         gen_SIGILL_T_if_in_ITBlock(old_itstate, new_itstate);

         IRTemp kondT = newTemp(Ity_I32);
         assign( kondT, mk_armg_calculate_condition(cond) );
         stmt( IRStmt_Exit( unop(Iop_32to1, mkexpr(kondT)),
                            Ijk_Boring,
                            IRConst_U32(dst | 1),
                            OFFB_R15T ));
         llPutIReg(15, mkU32( (guest_R15_curr_instr_notENC + 2) 
                              | 1  ));
         dres.jk_StopHere = Ijk_Boring;
         dres.whatNext    = Dis_StopHere;
         DIP("b%s 0x%x\n", nCC(cond), dst);
         goto decode_success;
      }
      break;
   }

   default:
      break; 

   }

   

   switch (INSN0(15,0)) {
      case 0xBF00:
         
         DIP("nop\n");
         goto decode_success;
      case 0xBF20:
         
         stmt( IRStmt_Exit( unop(Iop_32to1, mkexpr(condT)),
                            Ijk_Yield,
                            IRConst_U32((guest_R15_curr_instr_notENC + 2) 
                                        | 1 ),
                            OFFB_R15T ));
         DIP("wfe\n");
         goto decode_success;
      case 0xBF40:
         
         DIP("sev\n");
         goto decode_success;
      default:
         break; 
   }

   
   
   
   
   

#  define INSN1(_bMax,_bMin)  SLICE_UInt(((UInt)insn1), (_bMax), (_bMin))

   
   vassert(insn1 == 0);
   insn1 = getUShortLittleEndianly( guest_instr+2 );

   anOp   = Iop_INVALID; 
   anOpNm = NULL;        

   
   vassert(dres.whatNext   == Dis_Continue);
   vassert(dres.len        == 2);
   vassert(dres.continueAt == 0);
   dres.len = 4;

   
   if (BITS5(1,1,1,1,0) == INSN0(15,11) && BITS2(1,1) == INSN1(15,14)) {
      UInt isBL = INSN1(12,12);
      UInt bS   = INSN0(10,10);
      UInt bJ1  = INSN1(13,13);
      UInt bJ2  = INSN1(11,11);
      UInt bI1  = 1 ^ (bJ1 ^ bS);
      UInt bI2  = 1 ^ (bJ2 ^ bS);
      Int simm25
         =   (bS          << (1 + 1 + 10 + 11 + 1))
           | (bI1         << (1 + 10 + 11 + 1))
           | (bI2         << (10 + 11 + 1))
           | (INSN0(9,0)  << (11 + 1))
           | (INSN1(10,0) << 1);
      simm25 = (simm25 << 7) >> 7;

      vassert(0 == (guest_R15_curr_instr_notENC & 1));
      UInt dst = simm25 + guest_R15_curr_instr_notENC + 4;

      Bool valid = True;
      if (isBL == 0 && INSN1(0,0) == 1) valid = False;
      if (valid) {
         
         gen_SIGILL_T_if_in_but_NLI_ITBlock(old_itstate, new_itstate);
         
         
         mk_skip_over_T32_if_cond_is_false(condT);
         condT = IRTemp_INVALID;
         

         
         putIRegT( 14, mkU32( (guest_R15_curr_instr_notENC + 4) | 1 ),
                   IRTemp_INVALID);
         if (isBL) {
            
            
            llPutIReg(15, mkU32( dst | 1 ));
            DIP("bl 0x%x (stay in Thumb mode)\n", dst);
         } else {
            
            llPutIReg(15, mkU32( dst & ~3 ));
            DIP("blx 0x%x (switch to ARM mode)\n", dst & ~3);
         }
         dres.whatNext    = Dis_StopHere;
         dres.jk_StopHere = Ijk_Call;
         goto decode_success;
      }
   }

   
   if (0x3a2 == INSN0(15,6) 
       || 0x3a4 == INSN0(15,6)) { 
      UInt bW      = INSN0(5,5); 
      UInt bL      = INSN0(4,4);
      UInt rN      = INSN0(3,0);
      UInt bP      = INSN1(15,15); 
      UInt bM      = INSN1(14,14); 
      UInt rLmost  = INSN1(12,0);  
      UInt rL13    = INSN1(13,13); 
      UInt regList = 0;
      Bool valid   = True;

      UInt bINC    = 1;
      UInt bBEFORE = 0;
      if (INSN0(15,6) == 0x3a4) {
         bINC    = 0;
         bBEFORE = 1;
      }

      if (rL13 == 1)
         valid = False;

      if (bL == 1) {
         regList = (bP << 15) | (bM << 14) | rLmost;
         if (rN == 15)                       valid = False;
         if (popcount32(regList) < 2)        valid = False;
         if (bP == 1 && bM == 1)             valid = False;
         if (bW == 1 && (regList & (1<<rN))) valid = False;
      } else {
         regList = (bM << 14) | rLmost;
         if (bP == 1)                        valid = False;
         if (rN == 15)                       valid = False;
         if (popcount32(regList) < 2)        valid = False;
         if (bW == 1 && (regList & (1<<rN))) valid = False;
      }

      if (valid) {
         if (bL == 1 && bP == 1) {
            
            
            gen_SIGILL_T_if_in_but_NLI_ITBlock(old_itstate, new_itstate);
         }

         
         mk_skip_over_T32_if_cond_is_false(condT);
         condT = IRTemp_INVALID;
         

         
         mk_ldm_stm(False, rN, bINC, bBEFORE, bW, bL, regList);

         if (bL == 1 && (regList & (1<<15))) {
            
            
            llPutIReg(15, llGetIReg(15));
            dres.jk_StopHere = Ijk_Ret;
            dres.whatNext    = Dis_StopHere;
         }

         DIP("%sm%c%c r%u%s, {0x%04x}\n",
              bL == 1 ? "ld" : "st", bINC ? 'i' : 'd', bBEFORE ? 'b' : 'a',
              rN, bW ? "!" : "", regList);

         goto decode_success;
      }
   }

   
   if (INSN0(15,11) == BITS5(1,1,1,1,0)
       && INSN0(9,5) == BITS5(0,1,0,0,0)
       && INSN1(15,15) == 0) {
      UInt bS = INSN0(4,4);
      UInt rN = INSN0(3,0);
      UInt rD = INSN1(11,8);
      Bool valid = !isBadRegT(rN) && !isBadRegT(rD);
       
      if (!valid && rD <= 14 && rN == 13)
         valid = True;
      if (valid) {
         IRTemp argL  = newTemp(Ity_I32);
         IRTemp argR  = newTemp(Ity_I32);
         IRTemp res   = newTemp(Ity_I32);
         UInt   imm32 = thumbExpandImm_from_I0_I1(NULL, insn0, insn1);
         assign(argL, getIRegT(rN));
         assign(argR, mkU32(imm32));
         assign(res,  binop(Iop_Add32, mkexpr(argL), mkexpr(argR)));
         putIRegT(rD, mkexpr(res), condT);
         if (bS == 1)
            setFlags_D1_D2( ARMG_CC_OP_ADD, argL, argR, condT );
         DIP("add%s.w r%u, r%u, #%u\n",
             bS == 1 ? "s" : "", rD, rN, imm32);
         goto decode_success;
      }
   }

   
   if (INSN0(15,11) == BITS5(1,1,1,1,0)
       && INSN0(9,4) == BITS6(1,0,0,0,0,0)
       && INSN1(15,15) == 0) {
      UInt rN = INSN0(3,0);
      UInt rD = INSN1(11,8);
      Bool valid = !isBadRegT(rN) && !isBadRegT(rD);
      
      if (!valid && rD <= 14 && rN == 13)
         valid = True;
      if (valid) {
         IRTemp argL = newTemp(Ity_I32);
         IRTemp argR = newTemp(Ity_I32);
         IRTemp res  = newTemp(Ity_I32);
         UInt imm12  = (INSN0(10,10) << 11) | (INSN1(14,12) << 8) | INSN1(7,0);
         assign(argL, getIRegT(rN));
         assign(argR, mkU32(imm12));
         assign(res,  binop(Iop_Add32, mkexpr(argL), mkexpr(argR)));
         putIRegT(rD, mkexpr(res), condT);
         DIP("addw r%u, r%u, #%u\n", rD, rN, imm12);
         goto decode_success;
      }
   }

   
   
   if (INSN0(15,11) == BITS5(1,1,1,1,0)
       && (   INSN0(9,4) == BITS6(0,1,1,0,1,1)  
           || INSN0(9,4) == BITS6(0,1,0,0,0,1)) 
       && INSN1(15,15) == 0
       && INSN1(11,8) == BITS4(1,1,1,1)) {
      UInt rN = INSN0(3,0);
      if (rN != 15) {
         IRTemp argL  = newTemp(Ity_I32);
         IRTemp argR  = newTemp(Ity_I32);
         Bool   isCMN = INSN0(9,4) == BITS6(0,1,0,0,0,1);
         UInt   imm32 = thumbExpandImm_from_I0_I1(NULL, insn0, insn1);
         assign(argL, getIRegT(rN));
         assign(argR, mkU32(imm32));
         setFlags_D1_D2( isCMN ? ARMG_CC_OP_ADD : ARMG_CC_OP_SUB,
                         argL, argR, condT );
         DIP("%s.w r%u, #%u\n", isCMN ? "cmn" : "cmp", rN, imm32);
         goto decode_success;
      }
   }

   
   
   if (INSN0(15,11) == BITS5(1,1,1,1,0)
       && (   INSN0(9,4) == BITS6(0,0,0,0,0,1)  
           || INSN0(9,4) == BITS6(0,0,1,0,0,1)) 
       && INSN1(15,15) == 0
       && INSN1(11,8) == BITS4(1,1,1,1)) {
      UInt rN = INSN0(3,0);
      if (!isBadRegT(rN)) { 
         Bool  isTST  = INSN0(9,4) == BITS6(0,0,0,0,0,1);
         IRTemp argL  = newTemp(Ity_I32);
         IRTemp argR  = newTemp(Ity_I32);
         IRTemp res   = newTemp(Ity_I32);
         IRTemp oldV  = newTemp(Ity_I32);
         IRTemp oldC  = newTemp(Ity_I32);
         Bool   updC  = False;
         UInt   imm32 = thumbExpandImm_from_I0_I1(&updC, insn0, insn1);
         assign(argL, getIRegT(rN));
         assign(argR, mkU32(imm32));
         assign(res,  binop(isTST ? Iop_And32 : Iop_Xor32,
                            mkexpr(argL), mkexpr(argR)));
         assign( oldV, mk_armg_calculate_flag_v() );
         assign( oldC, updC 
                       ? mkU32((imm32 >> 31) & 1)
                       : mk_armg_calculate_flag_c() );
         setFlags_D1_D2_ND( ARMG_CC_OP_LOGIC, res, oldC, oldV, condT );
         DIP("%s.w r%u, #%u\n", isTST ? "tst" : "teq", rN, imm32);
         goto decode_success;
      }
   }

   
   
   if (INSN0(15,11) == BITS5(1,1,1,1,0)
       && (INSN0(9,5) == BITS5(0,1,1,0,1) 
           || INSN0(9,5) == BITS5(0,1,1,1,0)) 
       && INSN1(15,15) == 0) {
      Bool isRSB = INSN0(9,5) == BITS5(0,1,1,1,0);
      UInt bS    = INSN0(4,4);
      UInt rN    = INSN0(3,0);
      UInt rD    = INSN1(11,8);
      Bool valid = !isBadRegT(rN) && !isBadRegT(rD);
      if (!valid && !isRSB && rN == 13 && rD != 15)
         valid = True;
      if (valid) {
         IRTemp argL  = newTemp(Ity_I32);
         IRTemp argR  = newTemp(Ity_I32);
         IRTemp res   = newTemp(Ity_I32);
         UInt   imm32 = thumbExpandImm_from_I0_I1(NULL, insn0, insn1);
         assign(argL, getIRegT(rN));
         assign(argR, mkU32(imm32));
         assign(res,  isRSB
                      ? binop(Iop_Sub32, mkexpr(argR), mkexpr(argL))
                      : binop(Iop_Sub32, mkexpr(argL), mkexpr(argR)));
         putIRegT(rD, mkexpr(res), condT);
         if (bS == 1) {
            if (isRSB)
               setFlags_D1_D2( ARMG_CC_OP_SUB, argR, argL, condT );
            else
               setFlags_D1_D2( ARMG_CC_OP_SUB, argL, argR, condT );
         }
         DIP("%s%s.w r%u, r%u, #%u\n",
             isRSB ? "rsb" : "sub", bS == 1 ? "s" : "", rD, rN, imm32);
         goto decode_success;
      }
   }

   
   if (INSN0(15,11) == BITS5(1,1,1,1,0)
       && INSN0(9,4) == BITS6(1,0,1,0,1,0)
       && INSN1(15,15) == 0) {
      UInt rN = INSN0(3,0);
      UInt rD = INSN1(11,8);
      Bool valid = !isBadRegT(rN) && !isBadRegT(rD);
      
      if (!valid && rD == 13 && rN == 13)
         valid = True;
      if (valid) {
         IRTemp argL  = newTemp(Ity_I32);
         IRTemp argR  = newTemp(Ity_I32);
         IRTemp res   = newTemp(Ity_I32);
         UInt imm12   = (INSN0(10,10) << 11) | (INSN1(14,12) << 8) | INSN1(7,0);
         assign(argL, getIRegT(rN));
         assign(argR, mkU32(imm12));
         assign(res,  binop(Iop_Sub32, mkexpr(argL), mkexpr(argR)));
         putIRegT(rD, mkexpr(res), condT);
         DIP("subw r%u, r%u, #%u\n", rD, rN, imm12);
         goto decode_success;
      }
   }

   
   
   if (INSN0(15,11) == BITS5(1,1,1,1,0)
       && (   INSN0(9,5) == BITS5(0,1,0,1,0)  
           || INSN0(9,5) == BITS5(0,1,0,1,1)) 
       && INSN1(15,15) == 0) {
      
      
      UInt bS    = INSN0(4,4);
      UInt rN    = INSN0(3,0);
      UInt rD    = INSN1(11,8);
      if (!isBadRegT(rN) && !isBadRegT(rD)) {
         IRTemp argL  = newTemp(Ity_I32);
         IRTemp argR  = newTemp(Ity_I32);
         IRTemp res   = newTemp(Ity_I32);
         IRTemp oldC  = newTemp(Ity_I32);
         UInt   imm32 = thumbExpandImm_from_I0_I1(NULL, insn0, insn1);
         assign(argL, getIRegT(rN));
         assign(argR, mkU32(imm32));
         assign(oldC, mk_armg_calculate_flag_c() );
         const HChar* nm  = "???";
         switch (INSN0(9,5)) {
            case BITS5(0,1,0,1,0): 
               nm = "adc";
               assign(res,
                      binop(Iop_Add32,
                            binop(Iop_Add32, mkexpr(argL), mkexpr(argR)),
                            mkexpr(oldC) ));
               putIRegT(rD, mkexpr(res), condT);
               if (bS)
                  setFlags_D1_D2_ND( ARMG_CC_OP_ADC,
                                     argL, argR, oldC, condT );
               break;
            case BITS5(0,1,0,1,1): 
               nm = "sbc";
               assign(res,
                      binop(Iop_Sub32,
                            binop(Iop_Sub32, mkexpr(argL), mkexpr(argR)),
                            binop(Iop_Xor32, mkexpr(oldC), mkU32(1)) ));
               putIRegT(rD, mkexpr(res), condT);
               if (bS)
                  setFlags_D1_D2_ND( ARMG_CC_OP_SBB,
                                     argL, argR, oldC, condT );
               break;
            default:
              vassert(0);
         }
         DIP("%s%s.w r%u, r%u, #%u\n",
             nm, bS == 1 ? "s" : "", rD, rN, imm32);
         goto decode_success;
      }
   }

   
   
   
   
   if (INSN0(15,11) == BITS5(1,1,1,1,0)
       && (   INSN0(9,5) == BITS5(0,0,0,1,0)  
           || INSN0(9,5) == BITS5(0,0,0,0,0)  
           || INSN0(9,5) == BITS5(0,0,0,0,1)  
           || INSN0(9,5) == BITS5(0,0,1,0,0)  
           || INSN0(9,5) == BITS5(0,0,0,1,1)) 
       && INSN1(15,15) == 0) {
      UInt bS = INSN0(4,4);
      UInt rN = INSN0(3,0);
      UInt rD = INSN1(11,8);
      if (!isBadRegT(rN) && !isBadRegT(rD)) {
         Bool   notArgR = False;
         IROp   op      = Iop_INVALID;
         const HChar* nm = "???";
         switch (INSN0(9,5)) {
            case BITS5(0,0,0,1,0): op = Iop_Or32;  nm = "orr"; break;
            case BITS5(0,0,0,0,0): op = Iop_And32; nm = "and"; break;
            case BITS5(0,0,0,0,1): op = Iop_And32; nm = "bic";
                                   notArgR = True; break;
            case BITS5(0,0,1,0,0): op = Iop_Xor32; nm = "eor"; break;
            case BITS5(0,0,0,1,1): op = Iop_Or32;  nm = "orn";
                                   notArgR = True; break;
            default: vassert(0);
         }
         IRTemp argL  = newTemp(Ity_I32);
         IRTemp argR  = newTemp(Ity_I32);
         IRTemp res   = newTemp(Ity_I32);
         Bool   updC  = False;
         UInt   imm32 = thumbExpandImm_from_I0_I1(&updC, insn0, insn1);
         assign(argL, getIRegT(rN));
         assign(argR, mkU32(notArgR ? ~imm32 : imm32));
         assign(res,  binop(op, mkexpr(argL), mkexpr(argR)));
         putIRegT(rD, mkexpr(res), condT);
         if (bS) {
            IRTemp oldV = newTemp(Ity_I32);
            IRTemp oldC = newTemp(Ity_I32);
            assign( oldV, mk_armg_calculate_flag_v() );
            assign( oldC, updC 
                          ? mkU32((imm32 >> 31) & 1)
                          : mk_armg_calculate_flag_c() );
            setFlags_D1_D2_ND( ARMG_CC_OP_LOGIC, res, oldC, oldV,
                               condT );
         }
         DIP("%s%s.w r%u, r%u, #%u\n",
             nm, bS == 1 ? "s" : "", rD, rN, imm32);
         goto decode_success;
      }
   }

   
   
   
   if (INSN0(15,9) == BITS7(1,1,1,0,1,0,1)
       && (   INSN0(8,5) == BITS4(1,0,0,0)  
           || INSN0(8,5) == BITS4(1,1,0,1)  
           || INSN0(8,5) == BITS4(1,1,1,0)) 
       && INSN1(15,15) == 0) {
      UInt rN   = INSN0(3,0);
      UInt rD   = INSN1(11,8);
      UInt rM   = INSN1(3,0);
      UInt bS   = INSN0(4,4);
      UInt imm5 = (INSN1(14,12) << 2) | INSN1(7,6);
      UInt how  = INSN1(5,4);

      Bool valid = !isBadRegT(rD) && !isBadRegT(rN) && !isBadRegT(rM);
      if (!valid && INSN0(8,5) == BITS4(1,0,0,0) 
          && rD != 15 && rN == 13 && imm5 <= 3 && how == 0) {
         valid = True;
      }
      if (!valid && INSN0(8,5) == BITS4(1,1,0,1) 
          && rD != 15 && rN == 13 && imm5 == 0 && how == 0) {
         valid = True;
      }
      if (valid) {
         Bool   swap = False;
         IROp   op   = Iop_INVALID;
         const HChar* nm = "???";
         switch (INSN0(8,5)) {
            case BITS4(1,0,0,0): op = Iop_Add32; nm = "add"; break;
            case BITS4(1,1,0,1): op = Iop_Sub32; nm = "sub"; break;
            case BITS4(1,1,1,0): op = Iop_Sub32; nm = "rsb"; 
                                 swap = True; break;
            default: vassert(0);
         }

         IRTemp argL = newTemp(Ity_I32);
         assign(argL, getIRegT(rN));

         IRTemp rMt = newTemp(Ity_I32);
         assign(rMt, getIRegT(rM));

         IRTemp argR = newTemp(Ity_I32);
         compute_result_and_C_after_shift_by_imm5(
            dis_buf, &argR, NULL, rMt, how, imm5, rM
         );

         IRTemp res = newTemp(Ity_I32);
         assign(res, swap 
                     ? binop(op, mkexpr(argR), mkexpr(argL))
                     : binop(op, mkexpr(argL), mkexpr(argR)));

         putIRegT(rD, mkexpr(res), condT);
         if (bS) {
            switch (op) {
               case Iop_Add32:
                  setFlags_D1_D2( ARMG_CC_OP_ADD, argL, argR, condT );
                  break;
               case Iop_Sub32:
                  if (swap)
                     setFlags_D1_D2( ARMG_CC_OP_SUB, argR, argL, condT );
                  else
                     setFlags_D1_D2( ARMG_CC_OP_SUB, argL, argR, condT );
                  break;
               default:
                  vassert(0);
            }
         }

         DIP("%s%s.w r%u, r%u, %s\n",
             nm, bS ? "s" : "", rD, rN, dis_buf);
         goto decode_success;
      }
   }

   
   
   if (INSN0(15,9) == BITS7(1,1,1,0,1,0,1)
       && (   INSN0(8,5) == BITS4(1,0,1,0)   
           || INSN0(8,5) == BITS4(1,0,1,1))  
       && INSN1(15,15) == 0) {
      
      
      UInt rN = INSN0(3,0);
      UInt rD = INSN1(11,8);
      UInt rM = INSN1(3,0);
      if (!isBadRegT(rD) && !isBadRegT(rN) && !isBadRegT(rM)) {
         UInt bS   = INSN0(4,4);
         UInt imm5 = (INSN1(14,12) << 2) | INSN1(7,6);
         UInt how  = INSN1(5,4);

         IRTemp argL = newTemp(Ity_I32);
         assign(argL, getIRegT(rN));

         IRTemp rMt = newTemp(Ity_I32);
         assign(rMt, getIRegT(rM));

         IRTemp oldC = newTemp(Ity_I32);
         assign(oldC, mk_armg_calculate_flag_c());

         IRTemp argR = newTemp(Ity_I32);
         compute_result_and_C_after_shift_by_imm5(
            dis_buf, &argR, NULL, rMt, how, imm5, rM
         );

         const HChar* nm  = "???";
         IRTemp res = newTemp(Ity_I32);
         switch (INSN0(8,5)) {
            case BITS4(1,0,1,0): 
               nm = "adc";
               assign(res,
                      binop(Iop_Add32,
                            binop(Iop_Add32, mkexpr(argL), mkexpr(argR)),
                            mkexpr(oldC) ));
               putIRegT(rD, mkexpr(res), condT);
               if (bS)
                  setFlags_D1_D2_ND( ARMG_CC_OP_ADC,
                                     argL, argR, oldC, condT );
               break;
            case BITS4(1,0,1,1): 
               nm = "sbc";
               assign(res,
                      binop(Iop_Sub32,
                            binop(Iop_Sub32, mkexpr(argL), mkexpr(argR)),
                            binop(Iop_Xor32, mkexpr(oldC), mkU32(1)) ));
               putIRegT(rD, mkexpr(res), condT);
               if (bS)
                  setFlags_D1_D2_ND( ARMG_CC_OP_SBB,
                                     argL, argR, oldC, condT );
               break;
            default:
               vassert(0);
         }

         DIP("%s%s.w r%u, r%u, %s\n",
             nm, bS ? "s" : "", rD, rN, dis_buf);
         goto decode_success;
      }
   }

   
   
   
   
   
   if (INSN0(15,9) == BITS7(1,1,1,0,1,0,1)
       && (   INSN0(8,5) == BITS4(0,0,0,0)  
           || INSN0(8,5) == BITS4(0,0,1,0)  
           || INSN0(8,5) == BITS4(0,1,0,0)  
           || INSN0(8,5) == BITS4(0,0,0,1)  
           || INSN0(8,5) == BITS4(0,0,1,1)) 
       && INSN1(15,15) == 0) {
      UInt rN = INSN0(3,0);
      UInt rD = INSN1(11,8);
      UInt rM = INSN1(3,0);
      if (!isBadRegT(rD) && !isBadRegT(rN) && !isBadRegT(rM)) {
         Bool notArgR = False;
         IROp op      = Iop_INVALID;
         const HChar* nm  = "???";
         switch (INSN0(8,5)) {
            case BITS4(0,0,0,0): op = Iop_And32; nm = "and"; break;
            case BITS4(0,0,1,0): op = Iop_Or32;  nm = "orr"; break;
            case BITS4(0,1,0,0): op = Iop_Xor32; nm = "eor"; break;
            case BITS4(0,0,0,1): op = Iop_And32; nm = "bic";
                                 notArgR = True; break;
            case BITS4(0,0,1,1): op = Iop_Or32; nm = "orn";
                                 notArgR = True; break;
            default: vassert(0);
         }
         UInt bS   = INSN0(4,4);
         UInt imm5 = (INSN1(14,12) << 2) | INSN1(7,6);
         UInt how  = INSN1(5,4);

         IRTemp rNt = newTemp(Ity_I32);
         assign(rNt, getIRegT(rN));

         IRTemp rMt = newTemp(Ity_I32);
         assign(rMt, getIRegT(rM));

         IRTemp argR = newTemp(Ity_I32);
         IRTemp oldC = bS ? newTemp(Ity_I32) : IRTemp_INVALID;

         compute_result_and_C_after_shift_by_imm5(
            dis_buf, &argR, bS ? &oldC : NULL, rMt, how, imm5, rM
         );

         IRTemp res = newTemp(Ity_I32);
         if (notArgR) {
            vassert(op == Iop_And32 || op == Iop_Or32);
            assign(res, binop(op, mkexpr(rNt),
                                  unop(Iop_Not32, mkexpr(argR))));
         } else {
            assign(res, binop(op, mkexpr(rNt), mkexpr(argR)));
         }

         putIRegT(rD, mkexpr(res), condT);
         if (bS) {
            IRTemp oldV = newTemp(Ity_I32);
            assign( oldV, mk_armg_calculate_flag_v() );
            setFlags_D1_D2_ND( ARMG_CC_OP_LOGIC, res, oldC, oldV,
                               condT );
         }

         DIP("%s%s.w r%u, r%u, %s\n",
             nm, bS ? "s" : "", rD, rN, dis_buf);
         goto decode_success;
      }
   }

   
   
   
   
   if (INSN0(15,7) == BITS9(1,1,1,1,1,0,1,0,0)
       && INSN1(15,12) == BITS4(1,1,1,1)
       && INSN1(7,4) == BITS4(0,0,0,0)) {
      UInt how = INSN0(6,5); 
      UInt rN  = INSN0(3,0);
      UInt rD  = INSN1(11,8);
      UInt rM  = INSN1(3,0);
      UInt bS  = INSN0(4,4);
      Bool valid = !isBadRegT(rN) && !isBadRegT(rM) && !isBadRegT(rD);
      if (valid) {
         IRTemp rNt    = newTemp(Ity_I32);
         IRTemp rMt    = newTemp(Ity_I32);
         IRTemp res    = newTemp(Ity_I32);
         IRTemp oldC   = bS ? newTemp(Ity_I32) : IRTemp_INVALID;
         IRTemp oldV   = bS ? newTemp(Ity_I32) : IRTemp_INVALID;
         const HChar* nms[4] = { "lsl", "lsr", "asr", "ror" };
         const HChar* nm     = nms[how];
         assign(rNt, getIRegT(rN));
         assign(rMt, getIRegT(rM));
         compute_result_and_C_after_shift_by_reg(
            dis_buf, &res, bS ? &oldC : NULL,
            rNt, how, rMt, rN, rM
         );
         if (bS)
            assign(oldV, mk_armg_calculate_flag_v());
         putIRegT(rD, mkexpr(res), condT);
         if (bS) {
            setFlags_D1_D2_ND( ARMG_CC_OP_LOGIC, res, oldC, oldV,
                               condT );
         }
         DIP("%s%s.w r%u, r%u, r%u\n",
             nm, bS ? "s" : "", rD, rN, rM);
         goto decode_success;
      }
   }

   
   
   if ((INSN0(15,0) & 0xFFCF) == 0xEA4F
       && INSN1(15,15) == 0) {
      UInt rD = INSN1(11,8);
      UInt rN = INSN1(3,0);
      if (!isBadRegT(rD) && !isBadRegT(rN)) {
         UInt bS    = INSN0(4,4);
         UInt isMVN = INSN0(5,5);
         UInt imm5  = (INSN1(14,12) << 2) | INSN1(7,6);
         UInt how   = INSN1(5,4);

         IRTemp rNt = newTemp(Ity_I32);
         assign(rNt, getIRegT(rN));

         IRTemp oldRn = newTemp(Ity_I32);
         IRTemp oldC  = bS ? newTemp(Ity_I32) : IRTemp_INVALID;
         compute_result_and_C_after_shift_by_imm5(
            dis_buf, &oldRn, bS ? &oldC : NULL, rNt, how, imm5, rN
         );

         IRTemp res = newTemp(Ity_I32);
         assign(res, isMVN ? unop(Iop_Not32, mkexpr(oldRn))
                           : mkexpr(oldRn));

         putIRegT(rD, mkexpr(res), condT);
         if (bS) {
            IRTemp oldV = newTemp(Ity_I32);
            assign( oldV, mk_armg_calculate_flag_v() );
            setFlags_D1_D2_ND( ARMG_CC_OP_LOGIC, res, oldC, oldV, condT);
         }
         DIP("%s%s.w r%u, %s\n",
             isMVN ? "mvn" : "mov", bS ? "s" : "", rD, dis_buf);
         goto decode_success;
      }
   }

   
   
   if (INSN0(15,9) == BITS7(1,1,1,0,1,0,1)
       && (   INSN0(8,4) == BITS5(0,0,0,0,1)  
           || INSN0(8,4) == BITS5(0,1,0,0,1)) 
       && INSN1(15,15) == 0
       && INSN1(11,8) == BITS4(1,1,1,1)) {
      UInt rN = INSN0(3,0);
      UInt rM = INSN1(3,0);
      if (!isBadRegT(rN) && !isBadRegT(rM)) {
         Bool isTST = INSN0(8,4) == BITS5(0,0,0,0,1);

         UInt how  = INSN1(5,4);
         UInt imm5 = (INSN1(14,12) << 2) | INSN1(7,6);

         IRTemp argL = newTemp(Ity_I32);
         assign(argL, getIRegT(rN));

         IRTemp rMt = newTemp(Ity_I32);
         assign(rMt, getIRegT(rM));

         IRTemp argR = newTemp(Ity_I32);
         IRTemp oldC = newTemp(Ity_I32);
         compute_result_and_C_after_shift_by_imm5(
            dis_buf, &argR, &oldC, rMt, how, imm5, rM
         );

         IRTemp oldV = newTemp(Ity_I32);
         assign( oldV, mk_armg_calculate_flag_v() );

         IRTemp res = newTemp(Ity_I32);
         assign(res, binop(isTST ? Iop_And32 : Iop_Xor32,
                           mkexpr(argL), mkexpr(argR)));

         setFlags_D1_D2_ND( ARMG_CC_OP_LOGIC, res, oldC, oldV,
                            condT );
         DIP("%s.w r%u, %s\n", isTST ? "tst" : "teq", rN, dis_buf);
         goto decode_success;
      }
   }

   
   
   if (INSN0(15,9) == BITS7(1,1,1,0,1,0,1)
       && (   INSN0(8,4) == BITS5(1,1,0,1,1)  
           || INSN0(8,4) == BITS5(1,0,0,0,1)) 
       && INSN1(15,15) == 0
       && INSN1(11,8) == BITS4(1,1,1,1)) {
      UInt rN = INSN0(3,0);
      UInt rM = INSN1(3,0);
      if (!isBadRegT(rN) && !isBadRegT(rM)) {
         Bool isCMN = INSN0(8,4) == BITS5(1,0,0,0,1);
         UInt how   = INSN1(5,4);
         UInt imm5  = (INSN1(14,12) << 2) | INSN1(7,6);

         IRTemp argL = newTemp(Ity_I32);
         assign(argL, getIRegT(rN));

         IRTemp rMt = newTemp(Ity_I32);
         assign(rMt, getIRegT(rM));

         IRTemp argR = newTemp(Ity_I32);
         compute_result_and_C_after_shift_by_imm5(
            dis_buf, &argR, NULL, rMt, how, imm5, rM
         );

         setFlags_D1_D2( isCMN ? ARMG_CC_OP_ADD : ARMG_CC_OP_SUB,
                         argL, argR, condT );

         DIP("%s.w r%u, %s\n", isCMN ? "cmn" : "cmp", rN, dis_buf);
         goto decode_success;
      }
   }

   
   
   if (INSN0(15,11) == BITS5(1,1,1,1,0)
       && (   INSN0(9,5) == BITS5(0,0,0,1,0)  
           || INSN0(9,5) == BITS5(0,0,0,1,1)) 
       && INSN0(3,0) == BITS4(1,1,1,1)
       && INSN1(15,15) == 0) {
      UInt rD = INSN1(11,8);
      if (!isBadRegT(rD)) {
         Bool   updC  = False;
         UInt   bS    = INSN0(4,4);
         Bool   isMVN = INSN0(5,5) == 1;
         UInt   imm32 = thumbExpandImm_from_I0_I1(&updC, insn0, insn1);
         IRTemp res   = newTemp(Ity_I32);
         assign(res, mkU32(isMVN ? ~imm32 : imm32));
         putIRegT(rD, mkexpr(res), condT);
         if (bS) {
            IRTemp oldV = newTemp(Ity_I32);
            IRTemp oldC = newTemp(Ity_I32);
            assign( oldV, mk_armg_calculate_flag_v() );
            assign( oldC, updC 
                          ? mkU32((imm32 >> 31) & 1)
                          : mk_armg_calculate_flag_c() );
            setFlags_D1_D2_ND( ARMG_CC_OP_LOGIC, res, oldC, oldV,
                               condT );
         }
         DIP("%s%s.w r%u, #%u\n",
             isMVN ? "mvn" : "mov", bS ? "s" : "", rD, imm32);
         goto decode_success;
      }
   }

   
   if (INSN0(15,11) == BITS5(1,1,1,1,0)
       && INSN0(9,4) == BITS6(1,0,0,1,0,0)
       && INSN1(15,15) == 0) {
      UInt rD = INSN1(11,8);
      if (!isBadRegT(rD)) {
         UInt imm16 = (INSN0(3,0) << 12) | (INSN0(10,10) << 11)
                      | (INSN1(14,12) << 8) | INSN1(7,0);
         putIRegT(rD, mkU32(imm16), condT);
         DIP("movw r%u, #%u\n", rD, imm16);
         goto decode_success;
      }
   }

   
   if (INSN0(15,11) == BITS5(1,1,1,1,0)
       && INSN0(9,4) == BITS6(1,0,1,1,0,0)
       && INSN1(15,15) == 0) {
      UInt rD = INSN1(11,8);
      if (!isBadRegT(rD)) {
         UInt imm16 = (INSN0(3,0) << 12) | (INSN0(10,10) << 11)
                      | (INSN1(14,12) << 8) | INSN1(7,0);
         IRTemp res = newTemp(Ity_I32);
         assign(res,
                binop(Iop_Or32,
                      binop(Iop_And32, getIRegT(rD), mkU32(0xFFFF)),
                      mkU32(imm16 << 16)));
         putIRegT(rD, mkexpr(res), condT);
         DIP("movt r%u, #%u\n", rD, imm16);
         goto decode_success;
      }
   }

   
   if (INSN0(15,9) == BITS7(1,1,1,1,1,0,0) && INSN1(11,11) == 1) {
      Bool   valid  = True;
      Bool   syned  = False;
      Bool   isST   = False;
      IRType ty     = Ity_I8;
      const HChar* nm = "???";

      switch (INSN0(8,4)) {
         case BITS5(0,0,0,0,0):   
            nm = "strb"; isST = True; break;
         case BITS5(0,0,0,0,1):   
            nm = "ldrb"; break;
         case BITS5(1,0,0,0,1):   
            nm = "ldrsb"; syned = True; break;
         case BITS5(0,0,0,1,0):   
            nm = "strh"; ty = Ity_I16; isST = True; break;
         case BITS5(0,0,0,1,1):   
            nm = "ldrh"; ty = Ity_I16; break;
         case BITS5(1,0,0,1,1):   
            nm = "ldrsh"; ty = Ity_I16; syned = True; break;
         case BITS5(0,0,1,0,0):   
            nm = "str"; ty = Ity_I32; isST = True; break;
         case BITS5(0,0,1,0,1):
            nm = "ldr"; ty = Ity_I32; break;  
         default:
            valid = False; break;
      }

      UInt rN      = INSN0(3,0);
      UInt rT      = INSN1(15,12);
      UInt bP      = INSN1(10,10);
      UInt bU      = INSN1(9,9);
      UInt bW      = INSN1(8,8);
      UInt imm8    = INSN1(7,0);
      Bool loadsPC = False;

      if (valid) {
         if (bP == 1 && bU == 1 && bW == 0)
            valid = False;
         if (bP == 0 && bW == 0)
            valid = False;
         if (rN == 15)
            valid = False;
         if (bW == 1 && rN == rT)
            valid = False;
         if (ty == Ity_I8 || ty == Ity_I16) {
            if (isBadRegT(rT))
               valid = False;
         } else {
            
            if (isST && rT == 15)
               valid = False;
            if (!isST && rT == 15)
               loadsPC = True;
         }
      }

      if (valid) {
         
         
         
         if (loadsPC) {
            gen_SIGILL_T_if_in_but_NLI_ITBlock(old_itstate, new_itstate);
            
            mk_skip_over_T32_if_cond_is_false(condT);
            condT = IRTemp_INVALID;
            
         }

         IRTemp preAddr = newTemp(Ity_I32);
         assign(preAddr, getIRegT(rN));

         IRTemp postAddr = newTemp(Ity_I32);
         assign(postAddr, binop(bU == 1 ? Iop_Add32 : Iop_Sub32,
                                mkexpr(preAddr), mkU32(imm8)));

         IRTemp transAddr = bP == 1 ? postAddr : preAddr;

         if (isST) {


            
            IRTemp oldRt = newTemp(Ity_I32);
            assign(oldRt, getIRegT(rT));

            
            if (bW == 1) {
               vassert(rN != rT); 
               putIRegT(rN, mkexpr(postAddr), condT);
            }

            
            IRExpr* data = NULL;
            switch (ty) {
               case Ity_I8:
                  data = unop(Iop_32to8, mkexpr(oldRt));
                  break;
               case Ity_I16:
                  data = unop(Iop_32to16, mkexpr(oldRt));
                  break;
               case Ity_I32:
                  data = mkexpr(oldRt);
                  break;
               default:
                  vassert(0);
            }
            storeGuardedLE(mkexpr(transAddr), data, condT);

         } else {

            
            IRTemp llOldRt = newTemp(Ity_I32);
            assign(llOldRt, llGetIReg(rT));

            
            IRTemp    newRt = newTemp(Ity_I32);
            IRLoadGOp widen = ILGop_INVALID;
            switch (ty) {
               case Ity_I8:
                  widen = syned ? ILGop_8Sto32 : ILGop_8Uto32; break;
               case Ity_I16:
                  widen = syned ? ILGop_16Sto32 : ILGop_16Uto32; break;
               case Ity_I32:
                  widen = ILGop_Ident32; break;
               default:
                  vassert(0);
            }
            loadGuardedLE(newRt, widen,
                          mkexpr(transAddr), mkexpr(llOldRt), condT);
            if (rT == 15) {
               vassert(loadsPC);
               
            } else {
               vassert(!loadsPC);
               putIRegT(rT, mkexpr(newRt), IRTemp_INVALID);
            }

            
            if (bW == 1) {
               vassert(rN != rT); 
               putIRegT(rN, mkexpr(postAddr), condT);
            }

            if (loadsPC) {
               
               vassert(rN != 15); 
               vassert(rT == 15);
               vassert(condT == IRTemp_INVALID); 
               llPutIReg(15, mkexpr(newRt));
               dres.jk_StopHere = Ijk_Boring;  
               dres.whatNext    = Dis_StopHere;
            }
         }

         if (bP == 1 && bW == 0) {
            DIP("%s.w r%u, [r%u, #%c%u]\n",
                nm, rT, rN, bU ? '+' : '-', imm8);
         }
         else if (bP == 1 && bW == 1) {
            DIP("%s.w r%u, [r%u, #%c%u]!\n",
                nm, rT, rN, bU ? '+' : '-', imm8);
         }
         else {
            vassert(bP == 0 && bW == 1);
            DIP("%s.w r%u, [r%u], #%c%u\n",
                nm, rT, rN, bU ? '+' : '-', imm8);
         }

         goto decode_success;
      }
   }

   
   if (INSN0(15,9) == BITS7(1,1,1,1,1,0,0)
       && INSN1(11,6) == BITS6(0,0,0,0,0,0)) {
      Bool   valid  = True;
      Bool   syned  = False;
      Bool   isST   = False;
      IRType ty     = Ity_I8;
      const HChar* nm = "???";

      switch (INSN0(8,4)) {
         case BITS5(0,0,0,0,0):   
            nm = "strb"; isST = True; break;
         case BITS5(0,0,0,0,1):   
            nm = "ldrb"; break;
         case BITS5(1,0,0,0,1):   
            nm = "ldrsb"; syned = True; break;
         case BITS5(0,0,0,1,0):   
            nm = "strh"; ty = Ity_I16; isST = True; break;
         case BITS5(0,0,0,1,1):   
            nm = "ldrh"; ty = Ity_I16; break;
         case BITS5(1,0,0,1,1):   
            nm = "ldrsh"; ty = Ity_I16; syned = True; break;
         case BITS5(0,0,1,0,0):   
            nm = "str"; ty = Ity_I32; isST = True; break;
         case BITS5(0,0,1,0,1):
            nm = "ldr"; ty = Ity_I32; break;  
         default:
            valid = False; break;
      }

      UInt rN      = INSN0(3,0);
      UInt rM      = INSN1(3,0);
      UInt rT      = INSN1(15,12);
      UInt imm2    = INSN1(5,4);
      Bool loadsPC = False;

      if (ty == Ity_I8 || ty == Ity_I16) {
         if (rN == 15 || isBadRegT(rT) || isBadRegT(rM))
            valid = False;
      } else {
         vassert(ty == Ity_I32);
         if (rN == 15 || isBadRegT(rM))
            valid = False;
         if (isST && rT == 15)
            valid = False;
         if (!isST && rT == 15)
            loadsPC = True;
      }

      if (valid) {
         
         
         
         if (loadsPC) {
            gen_SIGILL_T_if_in_but_NLI_ITBlock(old_itstate, new_itstate);
            
            mk_skip_over_T32_if_cond_is_false(condT);
            condT = IRTemp_INVALID;
            
         }

         IRTemp transAddr = newTemp(Ity_I32);
         assign(transAddr,
                binop( Iop_Add32,
                       getIRegT(rN),
                       binop(Iop_Shl32, getIRegT(rM), mkU8(imm2)) ));

         if (isST) {

            
            IRTemp oldRt = newTemp(Ity_I32);
            assign(oldRt, getIRegT(rT));

            
            IRExpr* data = NULL;
            switch (ty) {
               case Ity_I8:
                  data = unop(Iop_32to8, mkexpr(oldRt));
                  break;
               case Ity_I16:
                  data = unop(Iop_32to16, mkexpr(oldRt));
                  break;
              case Ity_I32:
                  data = mkexpr(oldRt);
                  break;
              default:
                 vassert(0);
            }
            storeGuardedLE(mkexpr(transAddr), data, condT);

         } else {

            
            IRTemp llOldRt = newTemp(Ity_I32);
            assign(llOldRt, llGetIReg(rT));

            
            IRTemp    newRt = newTemp(Ity_I32);
            IRLoadGOp widen = ILGop_INVALID;
            switch (ty) {
               case Ity_I8:
                  widen = syned ? ILGop_8Sto32 : ILGop_8Uto32; break;
               case Ity_I16:
                  widen = syned ? ILGop_16Sto32 : ILGop_16Uto32; break;
               case Ity_I32:
                  widen = ILGop_Ident32; break;
               default:
                  vassert(0);
            }
            loadGuardedLE(newRt, widen,
                          mkexpr(transAddr), mkexpr(llOldRt), condT);

            if (rT == 15) {
               vassert(loadsPC);
               
            } else {
               vassert(!loadsPC);
               putIRegT(rT, mkexpr(newRt), IRTemp_INVALID);
            }

            if (loadsPC) {
               
               vassert(rN != 15); 
               vassert(rT == 15);
               vassert(condT == IRTemp_INVALID); 
               llPutIReg(15, mkexpr(newRt));
               dres.jk_StopHere = Ijk_Boring;  
               dres.whatNext    = Dis_StopHere;
            }
         }

         DIP("%s.w r%u, [r%u, r%u, LSL #%u]\n",
             nm, rT, rN, rM, imm2);

         goto decode_success;
      }
   }

   
   if (INSN0(15,9) == BITS7(1,1,1,1,1,0,0)) {
      Bool   valid  = True;
      Bool   syned  = INSN0(8,8) == 1;
      Bool   isST   = False;
      IRType ty     = Ity_I8;
      UInt   bU     = INSN0(7,7); 
                                  
      const HChar* nm = "???";

      switch (INSN0(6,4)) {
         case BITS3(0,0,0):   
            nm = "strb"; isST = True; break;
         case BITS3(0,0,1):   
            nm = syned ? "ldrsb" : "ldrb"; break;
         case BITS3(0,1,0):   
            nm = "strh"; ty = Ity_I16; isST = True; break;
         case BITS3(0,1,1):   
            nm = syned ? "ldrsh" : "ldrh"; ty = Ity_I16; break;
         case BITS3(1,0,0):   
            nm = "str"; ty = Ity_I32; isST = True; break;
         case BITS3(1,0,1):
            nm = "ldr"; ty = Ity_I32; break;  
         default:
            valid = False; break;
      }

      UInt rN      = INSN0(3,0);
      UInt rT      = INSN1(15,12);
      UInt imm12   = INSN1(11,0);
      Bool loadsPC = False;

      if (rN != 15 && bU == 0) {
         
         valid = False;
      }

      if (isST) {
         if (syned) valid = False;
         if (rN == 15 || rT == 15)
            valid = False;
      } else {
         if (rT == 15) {
            if (ty == Ity_I32)
               loadsPC = True;
            else 
               valid = False;
         }
      }

      if (valid) {
         
         
         
         if (loadsPC) {
            gen_SIGILL_T_if_in_but_NLI_ITBlock(old_itstate, new_itstate);
            
            mk_skip_over_T32_if_cond_is_false(condT);
            condT = IRTemp_INVALID;
            
         }

         IRTemp rNt = newTemp(Ity_I32);
         if (rN == 15) {
            vassert(!isST);
            assign(rNt, binop(Iop_And32, getIRegT(15), mkU32(~3)));
         } else {
            assign(rNt, getIRegT(rN));
         }

         IRTemp transAddr = newTemp(Ity_I32);
         assign(transAddr,
                binop(bU == 1 ? Iop_Add32 : Iop_Sub32,
                      mkexpr(rNt), mkU32(imm12)));

         IRTemp oldRt = newTemp(Ity_I32);
         assign(oldRt, getIRegT(rT));

         IRTemp llOldRt = newTemp(Ity_I32);
         assign(llOldRt, llGetIReg(rT));

         if (isST) {
            IRExpr* data = NULL;
            switch (ty) {
               case Ity_I8:
                  data = unop(Iop_32to8, mkexpr(oldRt));
                  break;
               case Ity_I16:
                  data = unop(Iop_32to16, mkexpr(oldRt));
                  break;
              case Ity_I32:
                  data = mkexpr(oldRt);
                  break;
              default:
                 vassert(0);
            }
            storeGuardedLE(mkexpr(transAddr), data, condT);
         } else {
            IRTemp    newRt = newTemp(Ity_I32);
            IRLoadGOp widen = ILGop_INVALID;
            switch (ty) {
               case Ity_I8:
                  widen = syned ? ILGop_8Sto32 : ILGop_8Uto32; break;
               case Ity_I16:
                  widen = syned ? ILGop_16Sto32 : ILGop_16Uto32; break;
               case Ity_I32:
                  widen = ILGop_Ident32; break;
               default:
                  vassert(0);
            }
            loadGuardedLE(newRt, widen,
                          mkexpr(transAddr), mkexpr(llOldRt), condT);
            if (rT == 15) {
               vassert(loadsPC);
               
            } else {
               vassert(!loadsPC);
               putIRegT(rT, mkexpr(newRt), IRTemp_INVALID);
            }

            if (loadsPC) {
               
               vassert(rT == 15);
               vassert(condT == IRTemp_INVALID); 
               llPutIReg(15, mkexpr(newRt));
               dres.jk_StopHere = Ijk_Boring;
               dres.whatNext    = Dis_StopHere;
            }
         }

         DIP("%s.w r%u, [r%u, +#%u]\n", nm, rT, rN, imm12);

         goto decode_success;
      }
   }

   
   if (INSN0(15,9) == BITS7(1,1,1,0,1,0,0) && INSN0(6,6) == 1) {
      UInt bP   = INSN0(8,8);
      UInt bU   = INSN0(7,7);
      UInt bW   = INSN0(5,5);
      UInt bL   = INSN0(4,4);  
      UInt rN   = INSN0(3,0);
      UInt rT   = INSN1(15,12);
      UInt rT2  = INSN1(11,8);
      UInt imm8 = INSN1(7,0);

      Bool valid = True;
      if (bP == 0 && bW == 0)                 valid = False;
      if (bW == 1 && (rN == rT || rN == rT2)) valid = False;
      if (isBadRegT(rT) || isBadRegT(rT2))    valid = False;
      if (bL == 1 && rT == rT2)               valid = False;
      if (rN == 15 && (bL == 0
                       || bW == 1))     valid = False;

      if (valid) {
         IRTemp preAddr = newTemp(Ity_I32);
         assign(preAddr, 15 == rN
                           ? binop(Iop_And32, getIRegT(15), mkU32(~3U))
                           : getIRegT(rN));

         IRTemp postAddr = newTemp(Ity_I32);
         assign(postAddr, binop(bU == 1 ? Iop_Add32 : Iop_Sub32,
                                mkexpr(preAddr), mkU32(imm8 << 2)));

         IRTemp transAddr = bP == 1 ? postAddr : preAddr;

         Bool writeback_already_done = False;
         if (bL == 0 && bW == 1
             && rN == 13 && rN != rT && rN != rT2
             && bU == 0 && (imm8 << 2) == 8) {
            putIRegT(rN, mkexpr(postAddr), condT);
            writeback_already_done = True;
         }

         if (bL == 0) {
            IRTemp oldRt  = newTemp(Ity_I32);
            IRTemp oldRt2 = newTemp(Ity_I32);
            assign(oldRt,  getIRegT(rT));
            assign(oldRt2, getIRegT(rT2));
            storeGuardedLE( mkexpr(transAddr),
                            mkexpr(oldRt), condT );
            storeGuardedLE( binop(Iop_Add32, mkexpr(transAddr), mkU32(4)),
                            mkexpr(oldRt2), condT );
         } else {
            IRTemp oldRt  = newTemp(Ity_I32);
            IRTemp oldRt2 = newTemp(Ity_I32);
            IRTemp newRt  = newTemp(Ity_I32);
            IRTemp newRt2 = newTemp(Ity_I32);
            assign(oldRt,  llGetIReg(rT));
            assign(oldRt2, llGetIReg(rT2));
            loadGuardedLE( newRt, ILGop_Ident32,
                           mkexpr(transAddr),
                           mkexpr(oldRt), condT );
            loadGuardedLE( newRt2, ILGop_Ident32,
                           binop(Iop_Add32, mkexpr(transAddr), mkU32(4)),
                           mkexpr(oldRt2), condT );
 
            putIRegT(rT,  mkexpr(newRt),  IRTemp_INVALID);
            putIRegT(rT2, mkexpr(newRt2), IRTemp_INVALID);
         }

         if (bW == 1 && !writeback_already_done) {
            putIRegT(rN, mkexpr(postAddr), condT);
         }

         const HChar* nm = bL ? "ldrd" : "strd";

         if (bP == 1 && bW == 0) {
            DIP("%s.w r%u, r%u, [r%u, #%c%u]\n",
                nm, rT, rT2, rN, bU ? '+' : '-', imm8 << 2);
         }
         else if (bP == 1 && bW == 1) {
            DIP("%s.w r%u, r%u, [r%u, #%c%u]!\n",
                nm, rT, rT2, rN, bU ? '+' : '-', imm8 << 2);
         }
         else {
            vassert(bP == 0 && bW == 1);
            DIP("%s.w r%u, r%u, [r%u], #%c%u\n",
                nm, rT, rT2, rN, bU ? '+' : '-', imm8 << 2);
         }

         goto decode_success;
      }
   }

   
   if (INSN0(15,11) == BITS5(1,1,1,1,0)
       && INSN1(15,14) == BITS2(1,0)
       && INSN1(12,12) == 0) {
      UInt cond = INSN0(9,6);
      if (cond != ARMCondAL && cond != ARMCondNV) {
         Int simm21
            =   (INSN0(10,10) << (1 + 1 + 6 + 11 + 1))
              | (INSN1(11,11) << (1 + 6 + 11 + 1))
              | (INSN1(13,13) << (6 + 11 + 1))
              | (INSN0(5,0)   << (11 + 1))
              | (INSN1(10,0)  << 1);
         simm21 = (simm21 << 11) >> 11;

         vassert(0 == (guest_R15_curr_instr_notENC & 1));
         UInt dst = simm21 + guest_R15_curr_instr_notENC + 4;

         
         gen_SIGILL_T_if_in_ITBlock(old_itstate, new_itstate);

         IRTemp kondT = newTemp(Ity_I32);
         assign( kondT, mk_armg_calculate_condition(cond) );
         stmt( IRStmt_Exit( unop(Iop_32to1, mkexpr(kondT)),
                            Ijk_Boring,
                            IRConst_U32(dst | 1),
                            OFFB_R15T ));
         llPutIReg(15, mkU32( (guest_R15_curr_instr_notENC + 4) 
                              | 1  ));
         dres.jk_StopHere = Ijk_Boring;
         dres.whatNext    = Dis_StopHere;
         DIP("b%s.w 0x%x\n", nCC(cond), dst);
         goto decode_success;
      }
   }

   
   if (INSN0(15,11) == BITS5(1,1,1,1,0)
       && INSN1(15,14) == BITS2(1,0)
       && INSN1(12,12) == 1) {
      if (1) {
         UInt bS  = INSN0(10,10);
         UInt bJ1 = INSN1(13,13);
         UInt bJ2 = INSN1(11,11);
         UInt bI1 = 1 ^ (bJ1 ^ bS);
         UInt bI2 = 1 ^ (bJ2 ^ bS);
         Int simm25
            =   (bS          << (1 + 1 + 10 + 11 + 1))
              | (bI1         << (1 + 10 + 11 + 1))
              | (bI2         << (10 + 11 + 1))
              | (INSN0(9,0)  << (11 + 1))
              | (INSN1(10,0) << 1);
         simm25 = (simm25 << 7) >> 7;

         vassert(0 == (guest_R15_curr_instr_notENC & 1));
         UInt dst = simm25 + guest_R15_curr_instr_notENC + 4;

         
         gen_SIGILL_T_if_in_but_NLI_ITBlock(old_itstate, new_itstate);

         
         mk_skip_over_T32_if_cond_is_false(condT);
         condT = IRTemp_INVALID;
         

         
         llPutIReg(15, mkU32( dst | 1  ));
         dres.jk_StopHere = Ijk_Boring;
         dres.whatNext    = Dis_StopHere;
         DIP("b.w 0x%x\n", dst);
         goto decode_success;
      }
   }

   
   if (INSN0(15,4) == 0xE8D && INSN1(15,5) == 0x780) {
      UInt rN = INSN0(3,0);
      UInt rM = INSN1(3,0);
      UInt bH = INSN1(4,4);
      if (bH || (rN != 13 && !isBadRegT(rM))) {
         
         gen_SIGILL_T_if_in_but_NLI_ITBlock(old_itstate, new_itstate);
         
         mk_skip_over_T32_if_cond_is_false(condT);
         condT = IRTemp_INVALID;

         IRExpr* ea
             = binop(Iop_Add32,
                     getIRegT(rN),
                     bH ? binop(Iop_Shl32, getIRegT(rM), mkU8(1))
                        : getIRegT(rM));

         IRTemp delta = newTemp(Ity_I32);
         if (bH) {
            assign(delta, unop(Iop_16Uto32, loadLE(Ity_I16, ea)));
         } else {
            assign(delta, unop(Iop_8Uto32, loadLE(Ity_I8, ea)));
         }

         llPutIReg(
            15,
            binop(Iop_Or32,
                  binop(Iop_Add32,
                        getIRegT(15),
                        binop(Iop_Shl32, mkexpr(delta), mkU8(1))
                  ),
                  mkU32(1)
         ));
         dres.jk_StopHere = Ijk_Boring;
         dres.whatNext    = Dis_StopHere;
         DIP("tb%c [r%u, r%u%s]\n",
             bH ? 'h' : 'b', rN, rM, bH ? ", LSL #1" : "");
         goto decode_success;
      }
   }

   
   
   if ((INSN0(15,4) == 0xF3C 
        || INSN0(15,4) == 0xF34) 
       && INSN1(15,15) == 0 && INSN1(5,5) == 0) {
      UInt rN  = INSN0(3,0);
      UInt rD  = INSN1(11,8);
      UInt lsb = (INSN1(14,12) << 2) | INSN1(7,6);
      UInt wm1 = INSN1(4,0);
      UInt msb =  lsb + wm1;
      if (!isBadRegT(rD) && !isBadRegT(rN) && msb <= 31) {
         Bool   isU  = INSN0(15,4) == 0xF3C;
         IRTemp src  = newTemp(Ity_I32);
         IRTemp tmp  = newTemp(Ity_I32);
         IRTemp res  = newTemp(Ity_I32);
         UInt   mask = ((1 << wm1) - 1) + (1 << wm1);
         vassert(msb >= 0 && msb <= 31);
         vassert(mask != 0); 

         assign(src, getIRegT(rN));
         assign(tmp, binop(Iop_And32,
                           binop(Iop_Shr32, mkexpr(src), mkU8(lsb)),
                           mkU32(mask)));
         assign(res, binop(isU ? Iop_Shr32 : Iop_Sar32,
                           binop(Iop_Shl32, mkexpr(tmp), mkU8(31-wm1)),
                           mkU8(31-wm1)));

         putIRegT(rD, mkexpr(res), condT);

         DIP("%s r%u, r%u, #%u, #%u\n",
             isU ? "ubfx" : "sbfx", rD, rN, lsb, wm1 + 1);
         goto decode_success;
      }
   }

   
   
   
   
   
   
   if ((INSN0(15,0) == 0xFA5F     
        || INSN0(15,0) == 0xFA1F  
        || INSN0(15,0) == 0xFA4F  
        || INSN0(15,0) == 0xFA0F  
        || INSN0(15,0) == 0xFA3F  
        || INSN0(15,0) == 0xFA2F) 
       && INSN1(15,12) == BITS4(1,1,1,1)
       && INSN1(7,6) == BITS2(1,0)) {
      UInt rD = INSN1(11,8);
      UInt rM = INSN1(3,0);
      UInt rot = INSN1(5,4);
      if (!isBadRegT(rD) && !isBadRegT(rM)) {
         const HChar* nm = "???";
         IRTemp srcT = newTemp(Ity_I32);
         IRTemp rotT = newTemp(Ity_I32);
         IRTemp dstT = newTemp(Ity_I32);
         assign(srcT, getIRegT(rM));
         assign(rotT, genROR32(srcT, 8 * rot));
         switch (INSN0(15,0)) {
            case 0xFA5F: 
               nm = "uxtb";
               assign(dstT, unop(Iop_8Uto32,
                                 unop(Iop_32to8, mkexpr(rotT))));
               break;
            case 0xFA1F: 
               nm = "uxth";
               assign(dstT, unop(Iop_16Uto32,
                                 unop(Iop_32to16, mkexpr(rotT))));
               break;
            case 0xFA4F: 
               nm = "sxtb";
               assign(dstT, unop(Iop_8Sto32,
                                 unop(Iop_32to8, mkexpr(rotT))));
               break;
            case 0xFA0F: 
               nm = "sxth";
               assign(dstT, unop(Iop_16Sto32,
                                 unop(Iop_32to16, mkexpr(rotT))));
               break;
            case 0xFA3F: 
               nm = "uxtb16";
               assign(dstT, binop(Iop_And32, mkexpr(rotT),
                                             mkU32(0x00FF00FF)));
               break;
            case 0xFA2F: { 
               nm = "sxtb16";
               IRTemp lo32 = newTemp(Ity_I32);
               IRTemp hi32 = newTemp(Ity_I32);
               assign(lo32, binop(Iop_And32, mkexpr(rotT), mkU32(0xFF)));
               assign(hi32, binop(Iop_Shr32, mkexpr(rotT), mkU8(16)));
               assign(
                  dstT,
                  binop(Iop_Or32,
                        binop(Iop_And32,
                              unop(Iop_8Sto32,
                                   unop(Iop_32to8, mkexpr(lo32))),
                              mkU32(0xFFFF)),
                        binop(Iop_Shl32,
                              unop(Iop_8Sto32,
                                   unop(Iop_32to8, mkexpr(hi32))),
                              mkU8(16))
               ));
               break;
            }
            default:
               vassert(0);
         }
         putIRegT(rD, mkexpr(dstT), condT);
         DIP("%s r%u, r%u, ror #%u\n", nm, rD, rM, 8 * rot);
         goto decode_success;
      }
   }

   
   if (INSN0(15,4) == 0xFB0
       && (INSN1(15,0) & 0xF0F0) == 0xF000) {
      UInt rN = INSN0(3,0);
      UInt rD = INSN1(11,8);
      UInt rM = INSN1(3,0);
      if (!isBadRegT(rD) && !isBadRegT(rN) && !isBadRegT(rM)) {
         IRTemp res = newTemp(Ity_I32);
         assign(res, binop(Iop_Mul32, getIRegT(rN), getIRegT(rM)));
         putIRegT(rD, mkexpr(res), condT);
         DIP("mul.w r%u, r%u, r%u\n", rD, rN, rM);
         goto decode_success;
      }
   }

   
   if (INSN0(15,4) == 0xFB9
       && (INSN1(15,0) & 0xF0F0) == 0xF0F0) {
      UInt rN = INSN0(3,0);
      UInt rD = INSN1(11,8);
      UInt rM = INSN1(3,0);
      if (!isBadRegT(rD) && !isBadRegT(rN) && !isBadRegT(rM)) {
         IRTemp res  = newTemp(Ity_I32);
         IRTemp argL = newTemp(Ity_I32);
         IRTemp argR = newTemp(Ity_I32);
         assign(argL, getIRegT(rN));
         assign(argR, getIRegT(rM));
         assign(res, binop(Iop_DivS32, mkexpr(argL), mkexpr(argR)));
         putIRegT(rD, mkexpr(res), condT);
         DIP("sdiv.w r%u, r%u, r%u\n", rD, rN, rM);
         goto decode_success;
      }
   }

   
   if (INSN0(15,4) == 0xFBB
       && (INSN1(15,0) & 0xF0F0) == 0xF0F0) {
      UInt rN = INSN0(3,0);
      UInt rD = INSN1(11,8);
      UInt rM = INSN1(3,0);
      if (!isBadRegT(rD) && !isBadRegT(rN) && !isBadRegT(rM)) {
         IRTemp res  = newTemp(Ity_I32);
         IRTemp argL = newTemp(Ity_I32);
         IRTemp argR = newTemp(Ity_I32);
         assign(argL, getIRegT(rN));
         assign(argR, getIRegT(rM));
         assign(res, binop(Iop_DivU32, mkexpr(argL), mkexpr(argR)));
         putIRegT(rD, mkexpr(res), condT);
         DIP("udiv.w r%u, r%u, r%u\n", rD, rN, rM);
         goto decode_success;
      }
   }

   
   if ((INSN0(15,4) == 0xFB8 || INSN0(15,4) == 0xFBA)
       && INSN1(7,4) == BITS4(0,0,0,0)) {
      UInt isU  = INSN0(5,5);
      UInt rN   = INSN0(3,0);
      UInt rDlo = INSN1(15,12);
      UInt rDhi = INSN1(11,8);
      UInt rM   = INSN1(3,0);
      if (!isBadRegT(rDhi) && !isBadRegT(rDlo)
          && !isBadRegT(rN) && !isBadRegT(rM) && rDlo != rDhi) {
         IRTemp res   = newTemp(Ity_I64);
         assign(res, binop(isU ? Iop_MullU32 : Iop_MullS32,
                           getIRegT(rN), getIRegT(rM)));
         putIRegT( rDhi, unop(Iop_64HIto32, mkexpr(res)), condT );
         putIRegT( rDlo, unop(Iop_64to32, mkexpr(res)), condT );
         DIP("%cmull r%u, r%u, r%u, r%u\n",
             isU ? 'u' : 's', rDlo, rDhi, rN, rM);
         goto decode_success;
      }
   }

   
   if (INSN0(15,4) == 0xFB0
       && (   INSN1(7,4) == BITS4(0,0,0,0)    
           || INSN1(7,4) == BITS4(0,0,0,1))) { 
      UInt rN = INSN0(3,0);
      UInt rA = INSN1(15,12);
      UInt rD = INSN1(11,8);
      UInt rM = INSN1(3,0);
      if (!isBadRegT(rD) && !isBadRegT(rN)
          && !isBadRegT(rM) && !isBadRegT(rA)) {
         Bool   isMLA = INSN1(7,4) == BITS4(0,0,0,0);
         IRTemp res   = newTemp(Ity_I32);
         assign(res,
                binop(isMLA ? Iop_Add32 : Iop_Sub32,
                      getIRegT(rA),
                      binop(Iop_Mul32, getIRegT(rN), getIRegT(rM))));
         putIRegT(rD, mkexpr(res), condT);
         DIP("%s r%u, r%u, r%u, r%u\n",
             isMLA ? "mla" : "mls", rD, rN, rM, rA);
         goto decode_success;
      }
   }

   
   if ((INSN0(15,0) == 0xF20F || INSN0(15,0) == 0xF60F)
       && INSN1(15,15) == 0) {
      
      UInt rD = INSN1(11,8);
      if (!isBadRegT(rD)) {
         UInt imm32 = (INSN0(10,10) << 11)
                      | (INSN1(14,12) << 8) | INSN1(7,0);
         putIRegT(rD, binop(Iop_Add32, 
                            binop(Iop_And32, getIRegT(15), mkU32(~3U)),
                            mkU32(imm32)),
                      condT);
         DIP("add r%u, pc, #%u\n", rD, imm32);
         goto decode_success;
      }
   }

   
   
   if ((INSN0(15,4) == 0xFBE 
        || INSN0(15,4) == 0xFBC) 
       && INSN1(7,4) == BITS4(0,0,0,0)) {
      UInt rN   = INSN0(3,0);
      UInt rDlo = INSN1(15,12);
      UInt rDhi = INSN1(11,8);
      UInt rM   = INSN1(3,0);
      if (!isBadRegT(rDlo) && !isBadRegT(rDhi) && !isBadRegT(rN)
          && !isBadRegT(rM) && rDhi != rDlo) {
         Bool   isS   = INSN0(15,4) == 0xFBC;
         IRTemp argL  = newTemp(Ity_I32);
         IRTemp argR  = newTemp(Ity_I32);
         IRTemp old   = newTemp(Ity_I64);
         IRTemp res   = newTemp(Ity_I64);
         IRTemp resHi = newTemp(Ity_I32);
         IRTemp resLo = newTemp(Ity_I32);
         IROp   mulOp = isS ? Iop_MullS32 : Iop_MullU32;
         assign( argL, getIRegT(rM));
         assign( argR, getIRegT(rN));
         assign( old, binop(Iop_32HLto64, getIRegT(rDhi), getIRegT(rDlo)) );
         assign( res, binop(Iop_Add64,
                            mkexpr(old),
                            binop(mulOp, mkexpr(argL), mkexpr(argR))) );
         assign( resHi, unop(Iop_64HIto32, mkexpr(res)) );
         assign( resLo, unop(Iop_64to32, mkexpr(res)) );
         putIRegT( rDhi, mkexpr(resHi), condT );
         putIRegT( rDlo, mkexpr(resLo), condT );
         DIP("%cmlal r%u, r%u, r%u, r%u\n",
             isS ? 's' : 'u', rDlo, rDhi, rN, rM);
         goto decode_success;
      }
   }

   
   if (INSN0(15,4) == 0xFBE && INSN1(7,4) == BITS4(0,1,1,0)) {
      UInt rN   = INSN0(3,0);
      UInt rDlo = INSN1(15,12);
      UInt rDhi = INSN1(11,8);
      UInt rM   = INSN1(3,0);
      if (!isBadRegT(rDlo) && !isBadRegT(rDhi) && !isBadRegT(rN)
          && !isBadRegT(rM) && rDhi != rDlo) {
         IRTemp argN   = newTemp(Ity_I32);
         IRTemp argM   = newTemp(Ity_I32);
         IRTemp argDhi = newTemp(Ity_I32);
         IRTemp argDlo = newTemp(Ity_I32);
         IRTemp res    = newTemp(Ity_I64);
         IRTemp resHi  = newTemp(Ity_I32);
         IRTemp resLo  = newTemp(Ity_I32);
         assign( argN,   getIRegT(rN) );
         assign( argM,   getIRegT(rM) );
         assign( argDhi, getIRegT(rDhi) );
         assign( argDlo, getIRegT(rDlo) );
         assign( res, 
                 binop(Iop_Add64,
                       binop(Iop_Add64,
                             binop(Iop_MullU32, mkexpr(argN), mkexpr(argM)),
                             unop(Iop_32Uto64, mkexpr(argDhi))),
                       unop(Iop_32Uto64, mkexpr(argDlo))) );
         assign( resHi, unop(Iop_64HIto32, mkexpr(res)) );
         assign( resLo, unop(Iop_64to32, mkexpr(res)) );
         putIRegT( rDhi, mkexpr(resHi), condT );
         putIRegT( rDlo, mkexpr(resLo), condT );
         DIP("umaal r%u, r%u, r%u, r%u\n", rDlo, rDhi, rN, rM);
         goto decode_success;
      }
   }

   
   if (INSN0(15,7) == BITS9(1,1,1,1,1,0,1,1,0)
       && INSN0(6,4) == BITS3(1,0,1)
       && INSN1(15,12) == BITS4(1,1,1,1)
       && INSN1(7,5) == BITS3(0,0,0)) {
      UInt bitR = INSN1(4,4);
      UInt rD = INSN1(11,8);
      UInt rM = INSN1(3,0);
      UInt rN = INSN0(3,0);
      if (!isBadRegT(rD) && !isBadRegT(rN) && !isBadRegT(rM)) {
         IRExpr* res
         = unop(Iop_64HIto32,
                binop(Iop_Add64,
                      binop(Iop_MullS32, getIRegT(rN), getIRegT(rM)),
                      mkU64(bitR ? 0x80000000ULL : 0ULL)));
         putIRegT(rD, res, condT);
         DIP("smmul%s r%u, r%u, r%u\n",
             bitR ? "r" : "", rD, rN, rM);
         goto decode_success;
      }
   }

   
   if (INSN0(15,7) == BITS9(1,1,1,1,1,0,1,1,0)
       && INSN0(6,4) == BITS3(1,0,1)
       && INSN1(7,5) == BITS3(0,0,0)) {
      UInt bitR = INSN1(4,4);
      UInt rA = INSN1(15,12);
      UInt rD = INSN1(11,8);
      UInt rM = INSN1(3,0);
      UInt rN = INSN0(3,0);
      if (!isBadRegT(rD) && !isBadRegT(rN) && !isBadRegT(rM) && (rA != 13)) {
         IRExpr* res
         = unop(Iop_64HIto32,
                binop(Iop_Add64,
                      binop(Iop_Add64,
                            binop(Iop_32HLto64, getIRegT(rA), mkU32(0)),
                            binop(Iop_MullS32, getIRegT(rN), getIRegT(rM))),
                      mkU64(bitR ? 0x80000000ULL : 0ULL)));
         putIRegT(rD, res, condT);
         DIP("smmla%s r%u, r%u, r%u, r%u\n",
             bitR ? "r" : "", rD, rN, rM, rA);
         goto decode_success;
      }
   }

   
   if ((INSN0(15,0) == 0xF2AF || INSN0(15,0) == 0xF6AF)
       && INSN1(15,15) == 0) {
      
      UInt rD = INSN1(11,8);
      if (!isBadRegT(rD)) {
         UInt imm32 = (INSN0(10,10) << 11)
                      | (INSN1(14,12) << 8) | INSN1(7,0);
         putIRegT(rD, binop(Iop_Sub32, 
                            binop(Iop_And32, getIRegT(15), mkU32(~3U)),
                            mkU32(imm32)),
                      condT);
         DIP("sub r%u, pc, #%u\n", rD, imm32);
         goto decode_success;
      }
   }

   
   
   if (INSN0(15,4) == 0xF36 && INSN1(15,15) == 0 && INSN1(5,5) == 0) {
      UInt rD  = INSN1(11,8);
      UInt rN  = INSN0(3,0);
      UInt msb = INSN1(4,0);
      UInt lsb = (INSN1(14,12) << 2) | INSN1(7,6);
      if (isBadRegT(rD) || rN == 13 || msb < lsb) {
         
      } else {
         IRTemp src    = newTemp(Ity_I32);
         IRTemp olddst = newTemp(Ity_I32);
         IRTemp newdst = newTemp(Ity_I32);
         UInt   mask = 1 << (msb - lsb);
         mask = (mask - 1) + mask;
         vassert(mask != 0); 
         mask <<= lsb;

         assign(src, rN == 15 ? mkU32(0) : getIRegT(rN));
         assign(olddst, getIRegT(rD));
         assign(newdst,
                binop(Iop_Or32,
                   binop(Iop_And32,
                         binop(Iop_Shl32, mkexpr(src), mkU8(lsb)), 
                         mkU32(mask)),
                   binop(Iop_And32,
                         mkexpr(olddst),
                         mkU32(~mask)))
               );

         putIRegT(rD, mkexpr(newdst), condT);

         if (rN == 15) {
            DIP("bfc r%u, #%u, #%u\n",
                rD, lsb, msb-lsb+1);
         } else {
            DIP("bfi r%u, r%u, #%u, #%u\n",
                rD, rN, lsb, msb-lsb+1);
         }
         goto decode_success;
      }
   }

   
   
   if ((INSN0(15,4) == 0xFA1      
        || INSN0(15,4) == 0xFA0)  
       && INSN1(15,12) == BITS4(1,1,1,1)
       && INSN1(7,6) == BITS2(1,0)) {
      Bool isU = INSN0(15,4) == 0xFA1;
      UInt rN  = INSN0(3,0);
      UInt rD  = INSN1(11,8);
      UInt rM  = INSN1(3,0);
      UInt rot = INSN1(5,4);
      if (!isBadRegT(rD) && !isBadRegT(rN) && !isBadRegT(rM)) {
         IRTemp srcL = newTemp(Ity_I32);
         IRTemp srcR = newTemp(Ity_I32);
         IRTemp res  = newTemp(Ity_I32);
         assign(srcR, getIRegT(rM));
         assign(srcL, getIRegT(rN));
         assign(res,  binop(Iop_Add32,
                            mkexpr(srcL),
                            unop(isU ? Iop_16Uto32 : Iop_16Sto32,
                                 unop(Iop_32to16, 
                                      genROR32(srcR, 8 * rot)))));
         putIRegT(rD, mkexpr(res), condT);
         DIP("%cxtah r%u, r%u, r%u, ror #%u\n",
             isU ? 'u' : 's', rD, rN, rM, rot);
         goto decode_success;
      }
   }

   
   
   if ((INSN0(15,4) == 0xFA5      
        || INSN0(15,4) == 0xFA4)  
       && INSN1(15,12) == BITS4(1,1,1,1)
       && INSN1(7,6) == BITS2(1,0)) {
      Bool isU = INSN0(15,4) == 0xFA5;
      UInt rN  = INSN0(3,0);
      UInt rD  = INSN1(11,8);
      UInt rM  = INSN1(3,0);
      UInt rot = INSN1(5,4);
      if (!isBadRegT(rD) && !isBadRegT(rN) && !isBadRegT(rM)) {
         IRTemp srcL = newTemp(Ity_I32);
         IRTemp srcR = newTemp(Ity_I32);
         IRTemp res  = newTemp(Ity_I32);
         assign(srcR, getIRegT(rM));
         assign(srcL, getIRegT(rN));
         assign(res,  binop(Iop_Add32,
                            mkexpr(srcL),
                            unop(isU ? Iop_8Uto32 : Iop_8Sto32,
                                 unop(Iop_32to8, 
                                      genROR32(srcR, 8 * rot)))));
         putIRegT(rD, mkexpr(res), condT);
         DIP("%cxtab r%u, r%u, r%u, ror #%u\n",
             isU ? 'u' : 's', rD, rN, rM, rot);
         goto decode_success;
      }
   }

   
   if (INSN0(15,4) == 0xFAB
       && INSN1(15,12) == BITS4(1,1,1,1)
       && INSN1(7,4) == BITS4(1,0,0,0)) {
      UInt rM1 = INSN0(3,0);
      UInt rD  = INSN1(11,8);
      UInt rM2 = INSN1(3,0);
      if (!isBadRegT(rD) && !isBadRegT(rM1) && rM1 == rM2) {
         IRTemp arg = newTemp(Ity_I32);
         IRTemp res = newTemp(Ity_I32);
         assign(arg, getIRegT(rM1));
         assign(res, IRExpr_ITE(
                        binop(Iop_CmpEQ32, mkexpr(arg), mkU32(0)),
                        mkU32(32),
                        unop(Iop_Clz32, mkexpr(arg))
         ));
         putIRegT(rD, mkexpr(res), condT);
         DIP("clz r%u, r%u\n", rD, rM1);
         goto decode_success;
      }
   }

   
   if (INSN0(15,4) == 0xFA9
       && INSN1(15,12) == BITS4(1,1,1,1)
       && INSN1(7,4) == BITS4(1,0,1,0)) {
      UInt rM1 = INSN0(3,0);
      UInt rD  = INSN1(11,8);
      UInt rM2 = INSN1(3,0);
      if (!isBadRegT(rD) && !isBadRegT(rM1) && rM1 == rM2) {
         IRTemp arg = newTemp(Ity_I32);
         assign(arg, getIRegT(rM1));
         IRTemp res = gen_BITREV(arg);
         putIRegT(rD, mkexpr(res), condT);
         DIP("rbit r%u, r%u\n", rD, rM1);
         goto decode_success;
      }
   }

   
   
   if (INSN0(15,4) == 0xFA9
       && INSN1(15,12) == BITS4(1,1,1,1)
       && (   INSN1(7,4) == BITS4(1,0,0,0)     
           || INSN1(7,4) == BITS4(1,0,0,1))) { 
      UInt rM1   = INSN0(3,0);
      UInt rD    = INSN1(11,8);
      UInt rM2   = INSN1(3,0);
      Bool isREV = INSN1(7,4) == BITS4(1,0,0,0);
      if (!isBadRegT(rD) && !isBadRegT(rM1) && rM1 == rM2) {
         IRTemp arg = newTemp(Ity_I32);
         assign(arg, getIRegT(rM1));
         IRTemp res = isREV ? gen_REV(arg) : gen_REV16(arg);
         putIRegT(rD, mkexpr(res), condT);
         DIP("rev%s r%u, r%u\n", isREV ? "" : "16", rD, rM1);
         goto decode_success;
      }
   }

   
   if (INSN0(15,4) == 0xFA9
       && INSN1(15,12) == BITS4(1,1,1,1)
       && INSN1(7,4) == BITS4(1,0,1,1)) {
      UInt rM1 = INSN0(3,0);
      UInt rM2 = INSN1(3,0);
      UInt rD  = INSN1(11,8);
      if (!isBadRegT(rD) && !isBadRegT(rM1) && rM1 == rM2) {
         IRTemp irt_rM  = newTemp(Ity_I32);
         IRTemp irt_hi  = newTemp(Ity_I32);
         IRTemp irt_low = newTemp(Ity_I32);
         IRTemp irt_res = newTemp(Ity_I32);
         assign(irt_rM, getIRegT(rM1));
         assign(irt_hi,
                binop(Iop_Sar32,
                      binop(Iop_Shl32, mkexpr(irt_rM), mkU8(24)),
                      mkU8(16)
                )
         );
         assign(irt_low,
                binop(Iop_And32,
                      binop(Iop_Shr32, mkexpr(irt_rM), mkU8(8)),
                      mkU32(0xFF)
                )
         );
         assign(irt_res,
                binop(Iop_Or32, mkexpr(irt_hi), mkexpr(irt_low))
         );
         putIRegT(rD, mkexpr(irt_res), condT);
         DIP("revsh r%u, r%u\n", rD, rM1);
         goto decode_success;
      }
   }

   
   if (INSN0(15,4) == 0xF38 
       && INSN1(15,12) == BITS4(1,0,0,0) && INSN1(9,0) == 0x000) {
      UInt rN          = INSN0(3,0);
      UInt write_ge    = INSN1(10,10);
      UInt write_nzcvq = INSN1(11,11);
      if (!isBadRegT(rN) && (write_nzcvq || write_ge)) {
         IRTemp rNt = newTemp(Ity_I32);
         assign(rNt, getIRegT(rN));
         desynthesise_APSR( write_nzcvq, write_ge, rNt, condT );
         DIP("msr cpsr_%s%s, r%u\n",
             write_nzcvq ? "f" : "", write_ge ? "g" : "", rN);
         goto decode_success;
      }
   }

   
   if (INSN0(15,0) == 0xF3EF
       && INSN1(15,12) == BITS4(1,0,0,0) && INSN1(7,0) == 0x00) {
      UInt rD = INSN1(11,8);
      if (!isBadRegT(rD)) {
         IRTemp apsr = synthesise_APSR();
         putIRegT( rD, mkexpr(apsr), condT );
         DIP("mrs r%u, cpsr\n", rD);
         goto decode_success;
      }
   }

   
   if (INSN0(15,4) == 0xE85 && INSN1(11,8) == BITS4(1,1,1,1)) {
      UInt rN   = INSN0(3,0);
      UInt rT   = INSN1(15,12);
      UInt imm8 = INSN1(7,0);
      if (!isBadRegT(rT) && rN != 15) {
         IRTemp res;
         
         mk_skip_over_T32_if_cond_is_false( condT );
         
         res = newTemp(Ity_I32);
         stmt( IRStmt_LLSC(Iend_LE,
                           res,
                           binop(Iop_Add32, getIRegT(rN), mkU32(imm8 * 4)),
                           NULL ));
         putIRegT(rT, mkexpr(res), IRTemp_INVALID);
         DIP("ldrex r%u, [r%u, #+%u]\n", rT, rN, imm8 * 4);
         goto decode_success;
      }
   }

   
   if (INSN0(15,4) == 0xE8D
       && (INSN1(11,0) == 0xF4F || INSN1(11,0) == 0xF5F)) {
      UInt rN  = INSN0(3,0);
      UInt rT  = INSN1(15,12);
      Bool isH = INSN1(11,0) == 0xF5F;
      if (!isBadRegT(rT) && rN != 15) {
         IRTemp res;
         
         mk_skip_over_T32_if_cond_is_false( condT );
         
         res = newTemp(isH ? Ity_I16 : Ity_I8);
         stmt( IRStmt_LLSC(Iend_LE, res, getIRegT(rN),
                           NULL ));
         putIRegT(rT, unop(isH ? Iop_16Uto32 : Iop_8Uto32, mkexpr(res)),
                      IRTemp_INVALID);
         DIP("ldrex%c r%u, [r%u]\n", isH ? 'h' : 'b', rT, rN);
         goto decode_success;
      }
   }

   
   if (INSN0(15,4) == 0xE8D && INSN1(7,0) == 0x7F) {
      UInt rN  = INSN0(3,0);
      UInt rT  = INSN1(15,12);
      UInt rT2 = INSN1(11,8);
      if (!isBadRegT(rT) && !isBadRegT(rT2) && rT != rT2 && rN != 15) {
         IRTemp res;
         
         mk_skip_over_T32_if_cond_is_false( condT );
         
         res = newTemp(Ity_I64);
         
         stmt( IRStmt_LLSC(Iend_LE, res, getIRegT(rN),
                           NULL ));
         
         putIRegT(rT,  unop(Iop_64to32,   mkexpr(res)), IRTemp_INVALID);
         putIRegT(rT2, unop(Iop_64HIto32, mkexpr(res)), IRTemp_INVALID);
         DIP("ldrexd r%u, r%u, [r%u]\n", rT, rT2, rN);
         goto decode_success;
      }
   }

   
   if (INSN0(15,4) == 0xE84) {
      UInt rN   = INSN0(3,0);
      UInt rT   = INSN1(15,12);
      UInt rD   = INSN1(11,8);
      UInt imm8 = INSN1(7,0);
      if (!isBadRegT(rD) && !isBadRegT(rT) && rN != 15 
          && rD != rN && rD != rT) {
         IRTemp resSC1, resSC32;
         
         mk_skip_over_T32_if_cond_is_false( condT );
         
         
         resSC1 = newTemp(Ity_I1);
         stmt( IRStmt_LLSC(Iend_LE,
                           resSC1,
                           binop(Iop_Add32, getIRegT(rN), mkU32(imm8 * 4)),
                           getIRegT(rT)) );
         resSC32 = newTemp(Ity_I32);
         assign(resSC32,
                unop(Iop_1Uto32, unop(Iop_Not1, mkexpr(resSC1))));
         putIRegT(rD, mkexpr(resSC32), IRTemp_INVALID);
         DIP("strex r%u, r%u, [r%u, #+%u]\n", rD, rT, rN, imm8 * 4);
         goto decode_success;
      }
   }

   
   if (INSN0(15,4) == 0xE8C
       && (INSN1(11,4) == 0xF4 || INSN1(11,4) == 0xF5)) {
      UInt rN  = INSN0(3,0);
      UInt rT  = INSN1(15,12);
      UInt rD  = INSN1(3,0);
      Bool isH = INSN1(11,4) == 0xF5;
      if (!isBadRegT(rD) && !isBadRegT(rT) && rN != 15 
          && rD != rN && rD != rT) {
         IRTemp resSC1, resSC32;
         
         mk_skip_over_T32_if_cond_is_false( condT );
         
         
         resSC1 = newTemp(Ity_I1);
         stmt( IRStmt_LLSC(Iend_LE, resSC1, getIRegT(rN),
                           unop(isH ? Iop_32to16 : Iop_32to8,
                                getIRegT(rT))) );
         resSC32 = newTemp(Ity_I32);
         assign(resSC32,
                unop(Iop_1Uto32, unop(Iop_Not1, mkexpr(resSC1))));
         putIRegT(rD, mkexpr(resSC32), IRTemp_INVALID);
         DIP("strex%c r%u, r%u, [r%u]\n", isH ? 'h' : 'b', rD, rT, rN);
         goto decode_success;
      }
   }

   
   if (INSN0(15,4) == 0xE8C && INSN1(7,4) == BITS4(0,1,1,1)) {
      UInt rN  = INSN0(3,0);
      UInt rT  = INSN1(15,12);
      UInt rT2 = INSN1(11,8);
      UInt rD  = INSN1(3,0);
      if (!isBadRegT(rD) && !isBadRegT(rT) && !isBadRegT(rT2)
          && rN != 15 && rD != rN && rD != rT && rD != rT) {
         IRTemp resSC1, resSC32, data;
         
         mk_skip_over_T32_if_cond_is_false( condT );
         
         
         resSC1 = newTemp(Ity_I1);
         data = newTemp(Ity_I64);
         
         assign(data, binop(Iop_32HLto64, getIRegT(rT2), getIRegT(rT)));
         
         stmt( IRStmt_LLSC(Iend_LE, resSC1, getIRegT(rN), mkexpr(data)));
         resSC32 = newTemp(Ity_I32);
         assign(resSC32,
                unop(Iop_1Uto32, unop(Iop_Not1, mkexpr(resSC1))));
         putIRegT(rD, mkexpr(resSC32), IRTemp_INVALID);
         DIP("strexd r%u, r%u, r%u, [r%u]\n", rD, rT, rT2, rN);
         goto decode_success;
      }
   }

   
   if (INSN0(15,0) == 0xF3BF && (INSN1(15,0) & 0xFF00) == 0x8F00) {
      
      switch (INSN1(7,0)) {
         case 0x4F: 
         case 0x4E: 
         case 0x4B: 
         case 0x4A: 
         case 0x47: 
         case 0x46: 
         case 0x43: 
         case 0x42: 
            stmt( IRStmt_MBE(Imbe_Fence) );
            DIP("DSB\n");
            goto decode_success;
         case 0x5F: 
         case 0x5E: 
         case 0x5B: 
         case 0x5A: 
         case 0x57: 
         case 0x56: 
         case 0x53: 
         case 0x52: 
            stmt( IRStmt_MBE(Imbe_Fence) );
            DIP("DMB\n");
            goto decode_success;
         case 0x6F: 
            stmt( IRStmt_MBE(Imbe_Fence) );
            DIP("ISB\n");
            goto decode_success;
         default:
            break;
      }
   }

   
   if ((INSN0(15,4) & 0xFFD) == 0xF89 && INSN1(15,12) == 0xF) {
      
      
      UInt rN    = INSN0(3,0);
      UInt bW    = INSN0(5,5);
      UInt imm12 = INSN1(11,0);
      DIP("pld%s [r%u, #%u]\n", bW ? "w" : "",  rN, imm12);
      goto decode_success;
   }

   if ((INSN0(15,4) & 0xFFD) == 0xF81 && INSN1(15,8) == 0xFC) {
      
      
      UInt rN    = INSN0(3,0);
      UInt bW    = INSN0(5,5);
      UInt imm8  = INSN1(7,0);
      DIP("pld%s [r%u, #-%u]\n", bW ? "w" : "",  rN, imm8);
      goto decode_success;
   }

   if ((INSN0(15,4) & 0xFFD) == 0xF81 && INSN1(15,6) == 0x3C0) {
      
      
      UInt rN   = INSN0(3,0);
      UInt rM   = INSN1(3,0);
      UInt bW   = INSN0(5,5);
      UInt imm2 = INSN1(5,4);
      if (!isBadRegT(rM)) {
         DIP("pld%s [r%u, r%u, lsl %d]\n", bW ? "w" : "", rN, rM, imm2);
         goto decode_success;
      }
      
   }

   
   if ((INSN0(15,0) == 0xEE1D) && (INSN1(11,0) == 0x0F70)) {
      
      UInt rD = INSN1(15,12);
      if (!isBadRegT(rD)) {
         putIRegT(rD, IRExpr_Get(OFFB_TPIDRURO, Ity_I32), IRTemp_INVALID);
         DIP("mrc p15,0, r%u, c13, c0, 3\n", rD);
         goto decode_success;
      }
      
   }

   
   if (INSN0(15,0) == 0xF3BF && INSN1(15,0) == 0x8F2F) {
      mk_skip_over_T32_if_cond_is_false( condT );
      stmt( IRStmt_MBE(Imbe_CancelReservation) );
      DIP("clrex\n");
      goto decode_success;
   }

   
   if (INSN0(15,0) == 0xF3AF && INSN1(15,0) == 0x8000) {
      DIP("nop\n");
      goto decode_success;
   }

   
   if (INSN0(15,6) == BITS10(1,1,1,1,1,0,0,0,0,1) && INSN0(5,4) == BITS2(0,1)
       && INSN1(11,8) == BITS4(1,1,1,0)) {
      UInt rT    = INSN1(15,12);
      UInt rN    = INSN0(3,0);
      UInt imm8  = INSN1(7,0);
      Bool valid = True;
      if (rN == 15 || isBadRegT(rT)) valid = False;
      if (valid) {
         put_ITSTATE(old_itstate);
         IRExpr* ea = binop(Iop_Add32, getIRegT(rN), mkU32(imm8));
         IRTemp newRt = newTemp(Ity_I32);
         loadGuardedLE( newRt, ILGop_Ident32, ea, llGetIReg(rT), condT );
         putIRegT(rT, mkexpr(newRt), IRTemp_INVALID);
         put_ITSTATE(new_itstate);
         DIP("ldrt r%u, [r%u, #%u]\n", rT, rN, imm8);
         goto decode_success;
      }
   }

   
   if (INSN0(15,6) == BITS10(1,1,1,1,1,0,0,0,0,1) && INSN0(5,4) == BITS2(0,0)
       && INSN1(11,8) == BITS4(1,1,1,0)) {
      UInt rT    = INSN1(15,12);
      UInt rN    = INSN0(3,0);
      UInt imm8  = INSN1(7,0);
      Bool valid = True;
      if (rN == 15 || isBadRegT(rT)) valid = False;
      if (valid) {
         put_ITSTATE(old_itstate);
         IRExpr* address = binop(Iop_Add32, getIRegT(rN), mkU32(imm8));
         storeGuardedLE( address, llGetIReg(rT), condT );
         put_ITSTATE(new_itstate);
         DIP("strt r%u, [r%u, #%u]\n", rT, rN, imm8);
         goto decode_success;
      }
   }

   
   if (INSN0(15,6) == BITS10(1,1,1,1,1,0,0,0,0,0) && INSN0(5,4) == BITS2(0,0)
       && INSN1(11,8) == BITS4(1,1,1,0)) {
      UInt rT    = INSN1(15,12);
      UInt rN    = INSN0(3,0);
      UInt imm8  = INSN1(7,0);
      Bool valid = True;
      if (rN == 15 || isBadRegT(rT)) valid = False;
      if (valid) {
         put_ITSTATE(old_itstate);
         IRExpr* address = binop(Iop_Add32, getIRegT(rN), mkU32(imm8));
         IRExpr* data = unop(Iop_32to8, llGetIReg(rT));
         storeGuardedLE( address, data, condT );
         put_ITSTATE(new_itstate);
         DIP("strbt r%u, [r%u, #%u]\n", rT, rN, imm8);
         goto decode_success;
      }
   }

   
   if (INSN0(15,6) == BITS10(1,1,1,1,1,0,0,0,0,0) && INSN0(5,4) == BITS2(1,1)
       && INSN1(11,8) == BITS4(1,1,1,0)) {
      UInt rN    = INSN0(3,0);
      Bool valid = True;
      if (rN == 15) {
         valid = False;
      }
      UInt rT    = INSN1(15,12);
      UInt imm8  = INSN1(7,0);
      if (isBadRegT(rT)) valid = False;
      if (valid) {
         put_ITSTATE(old_itstate);
         IRExpr* ea = binop(Iop_Add32, getIRegT(rN), mkU32(imm8));
         IRTemp newRt = newTemp(Ity_I32);
         loadGuardedLE( newRt, ILGop_16Uto32, ea, llGetIReg(rT), condT );
         putIRegT(rT, mkexpr(newRt), IRTemp_INVALID);
         put_ITSTATE(new_itstate);
         DIP("ldrht r%u, [r%u, #%u]\n", rT, rN, imm8);
         goto decode_success;
      }
   }

   
   if (INSN0(15,6) == BITS10(1,1,1,1,1,0,0,1,0,0) && INSN0(5,4) == BITS2(1,1)
       && INSN1(11,8) == BITS4(1,1,1,0)) {
      UInt rN    = INSN0(3,0);
      Bool valid = True;
      if (rN == 15) {
         valid = False;
      }
      UInt rT    = INSN1(15,12);
      UInt imm8  = INSN1(7,0);
      if (isBadRegT(rT)) valid = False;
      if (valid) {
         put_ITSTATE(old_itstate);
         IRExpr* ea = binop(Iop_Add32, getIRegT(rN), mkU32(imm8));
         IRTemp newRt = newTemp(Ity_I32);
         loadGuardedLE( newRt, ILGop_16Sto32, ea, llGetIReg(rT), condT );
         putIRegT(rT, mkexpr(newRt), IRTemp_INVALID);
         put_ITSTATE(new_itstate);
         DIP("ldrsht r%u, [r%u, #%u]\n", rT, rN, imm8);
         goto decode_success;
      }
   }

   
   if (INSN0(15,6) == BITS10(1,1,1,1,1,0,0,0,0,0) && INSN0(5,4) == BITS2(1,0)
       && INSN1(11,8) == BITS4(1,1,1,0)) {
      UInt rT    = INSN1(15,12);
      UInt rN    = INSN0(3,0);
      UInt imm8  = INSN1(7,0);
      Bool valid = True;
      if (rN == 15 || isBadRegT(rT)) valid = False;
      if (valid) {
         put_ITSTATE(old_itstate);
         IRExpr* address = binop(Iop_Add32, getIRegT(rN), mkU32(imm8));
         IRExpr* data = unop(Iop_32to16, llGetIReg(rT));
         storeGuardedLE( address, data, condT );
         put_ITSTATE(new_itstate);
         DIP("strht r%u, [r%u, #%u]\n", rT, rN, imm8);
         goto decode_success;
      }
   }

   
   if (INSN0(15,6) == BITS10(1,1,1,1,1,0,0,0,0,0) && INSN0(5,4) == BITS2(0,1)
       && INSN1(11,8) == BITS4(1,1,1,0)) {
      UInt rN    = INSN0(3,0);
      UInt rT    = INSN1(15,12);
      UInt imm8  = INSN1(7,0);
      Bool valid = True;
      if (rN == 15 ) valid = False;
      if (isBadRegT(rT)) valid = False;
      if (valid) {
         put_ITSTATE(old_itstate);
         IRExpr* ea = binop(Iop_Add32, getIRegT(rN), mkU32(imm8));
         IRTemp newRt = newTemp(Ity_I32);
         loadGuardedLE( newRt, ILGop_8Uto32, ea, llGetIReg(rT), condT );
         putIRegT(rT, mkexpr(newRt), IRTemp_INVALID);
         put_ITSTATE(new_itstate);
         DIP("ldrbt r%u, [r%u, #%u]\n", rT, rN, imm8);
         goto decode_success;
      }
   }

   
   if (INSN0(15,6) == BITS10(1,1,1,1,1,0,0,1,0,0) && INSN0(5,4) == BITS2(0,1)
       && INSN1(11,8) == BITS4(1,1,1,0)) {
      UInt rN    = INSN0(3,0);
      Bool valid = True;
      UInt rT    = INSN1(15,12);
      UInt imm8  = INSN1(7,0);
      if (rN == 15 ) valid = False;
      if (isBadRegT(rT)) valid = False;
      if (valid) {
         put_ITSTATE(old_itstate);
         IRExpr* ea = binop(Iop_Add32, getIRegT(rN), mkU32(imm8));
         IRTemp newRt = newTemp(Ity_I32);
         loadGuardedLE( newRt, ILGop_8Sto32, ea, llGetIReg(rT), condT );
         putIRegT(rT, mkexpr(newRt), IRTemp_INVALID);
         put_ITSTATE(new_itstate);
         DIP("ldrsbt r%u, [r%u, #%u]\n", rT, rN, imm8);
         goto decode_success;
      }
   }

   
   if (INSN0(15,6) == BITS10(1,1,1,1,1,0,0,1,1,0) && INSN0(5,4) == BITS2(0,1)
       && INSN1(15,12) == BITS4(1,1,1,1)) {
      UInt rN    = INSN0(3,0);
      UInt imm12 = INSN1(11,0);
      if (rN != 15) {
         DIP("pli [r%u, #%u]\n", rN, imm12);
         goto decode_success;
      }
   }

   
   if (INSN0(15,6) == BITS10(1,1,1,1,1,0,0,1,0,0) && INSN0(5,4) == BITS2(0,1)
       && INSN1(15,8) == BITS8(1,1,1,1,1,1,0,0)) {
      UInt rN   = INSN0(3,0);
      UInt imm8 = INSN1(7,0);
      if (rN != 15) {
         DIP("pli [r%u, #-%u]\n", rN, imm8);
         goto decode_success;
      }
   }

   
   if (INSN0(15,8) == BITS8(1,1,1,1,1,0,0,1)
       && INSN0(6,0) == BITS7(0,0,1,1,1,1,1)
       && INSN1(15,12) == BITS4(1,1,1,1)) {
      UInt imm12 = INSN1(11,0);
      UInt bU    = INSN0(7,7);
      DIP("pli [pc, #%c%u]\n", bU == 1 ? '+' : '-', imm12);
      goto decode_success;
   }

   
   
   

   if (INSN0(15,12) == BITS4(1,1,1,0)) {
      UInt insn28 = (INSN0(11,0) << 16) | INSN1(15,0);
      Bool ok_vfp = decode_CP10_CP11_instruction (
                       &dres, insn28, condT, ARMCondAL,
                       True
                    );
      if (ok_vfp)
         goto decode_success;
   }

   
   
   

   if (archinfo->hwcaps & VEX_HWCAPS_ARM_NEON) {
      UInt insn32 = (INSN0(15,0) << 16) | INSN1(15,0);
      Bool ok_neon = decode_NEON_instruction(
                        &dres, insn32, condT, True
                     );
      if (ok_neon)
         goto decode_success;
   }

   
   
   

   { UInt insn32 = (INSN0(15,0) << 16) | INSN1(15,0);
     Bool ok_v6m = decode_V6MEDIA_instruction(
                      &dres, insn32, condT, ARMCondAL,
                      True
                   );
     if (ok_v6m)
        goto decode_success;
   }

   
   
   

   goto decode_failure;
   

  decode_failure:
   
   if (sigill_diag)
      vex_printf("disInstr(thumb): unhandled instruction: "
                 "0x%04x 0x%04x\n", (UInt)insn0, (UInt)insn1);

   if (old_itstate != IRTemp_INVALID)
      put_ITSTATE(old_itstate);

   vassert(0 == (guest_R15_curr_instr_notENC & 1));
   llPutIReg( 15, mkU32(guest_R15_curr_instr_notENC | 1) );
   dres.len         = 0;
   dres.whatNext    = Dis_StopHere;
   dres.jk_StopHere = Ijk_NoDecode;
   dres.continueAt  = 0;
   return dres;

  decode_success:
   
   vassert(dres.len == 4 || dres.len == 2 || dres.len == 20);
   switch (dres.whatNext) {
      case Dis_Continue:
         llPutIReg(15, mkU32(dres.len + (guest_R15_curr_instr_notENC | 1)));
         break;
      case Dis_ResteerU:
      case Dis_ResteerC:
         llPutIReg(15, mkU32(dres.continueAt));
         break;
      case Dis_StopHere:
         break;
      default:
         vassert(0);
   }

   DIP("\n");

   return dres;

#  undef INSN0
#  undef INSN1
}

#undef DIP
#undef DIS


static const UChar it_length_table[256]
   = { 0, 4, 3, 4, 2, 4, 3, 4, 1, 4, 3, 4, 2, 4, 3, 4,
       0, 4, 3, 4, 2, 4, 3, 4, 1, 4, 3, 4, 2, 4, 3, 4,
       0, 4, 3, 4, 2, 4, 3, 4, 1, 4, 3, 4, 2, 4, 3, 4,
       0, 4, 3, 4, 2, 4, 3, 4, 1, 4, 3, 4, 2, 4, 3, 4,
       0, 4, 3, 4, 2, 4, 3, 4, 1, 4, 3, 4, 2, 4, 3, 4,
       0, 4, 3, 4, 2, 4, 3, 4, 1, 4, 3, 4, 2, 4, 3, 4,
       0, 4, 3, 4, 2, 4, 3, 4, 1, 4, 3, 4, 2, 4, 3, 4,
       0, 4, 3, 4, 2, 4, 3, 4, 1, 4, 3, 4, 2, 4, 3, 4, 
       0, 4, 3, 4, 2, 4, 3, 4, 1, 4, 3, 4, 2, 4, 3, 4,
       0, 4, 3, 4, 2, 4, 3, 4, 1, 4, 3, 4, 2, 4, 3, 4,
       0, 4, 3, 4, 2, 4, 3, 4, 1, 4, 3, 4, 2, 4, 3, 4,
       0, 4, 3, 4, 2, 4, 3, 4, 1, 4, 3, 4, 2, 4, 3, 4,
       0, 4, 3, 4, 2, 4, 3, 4, 1, 4, 3, 4, 2, 4, 3, 4,
       0, 4, 3, 4, 2, 4, 3, 4, 1, 4, 3, 4, 2, 4, 3, 4,
       0, 4, 3, 4, 2, 4, 4, 4, 1, 4, 4, 4, 4, 4, 4, 4,
       0, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4
     };




DisResult disInstr_ARM ( IRSB*        irsb_IN,
                         Bool         (*resteerOkFn) ( void*, Addr ),
                         Bool         resteerCisOk,
                         void*        callback_opaque,
                         const UChar* guest_code_IN,
                         Long         delta_ENCODED,
                         Addr         guest_IP_ENCODED,
                         VexArch      guest_arch,
                         const VexArchInfo* archinfo,
                         const VexAbiInfo*  abiinfo,
                         VexEndness   host_endness_IN,
                         Bool         sigill_diag_IN )
{
   DisResult dres;
   Bool isThumb = (Bool)(guest_IP_ENCODED & 1);

   
   vassert(guest_arch == VexArchARM);

   irsb            = irsb_IN;
   host_endness    = host_endness_IN;
   __curr_is_Thumb = isThumb;

   if (isThumb) {
      guest_R15_curr_instr_notENC = (Addr32)guest_IP_ENCODED - 1;
   } else {
      guest_R15_curr_instr_notENC = (Addr32)guest_IP_ENCODED;
   }

   if (isThumb) {
      dres = disInstr_THUMB_WRK ( resteerOkFn,
                                  resteerCisOk, callback_opaque,
                                  &guest_code_IN[delta_ENCODED - 1],
                                  archinfo, abiinfo, sigill_diag_IN );
   } else {
      dres = disInstr_ARM_WRK ( resteerOkFn,
                                resteerCisOk, callback_opaque,
                                &guest_code_IN[delta_ENCODED],
                                archinfo, abiinfo, sigill_diag_IN );
   }

   return dres;
}



